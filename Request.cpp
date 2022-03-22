#include "Request.h"

#include <cassert>
#include <tuple>

#include <fmt/format.h>

extern "C"
{
	#include <lua.h>
}




namespace LuaSimpleWinHttp
{





/** Converts the string from utf8 to ucs2. */
static std::wstring widen(const std::string & aUtf8)
{
	int count = MultiByteToWideChar(CP_UTF8, 0, aUtf8.c_str(), static_cast<int>(aUtf8.length()), nullptr, 0);
	std::wstring wstr(count, 0);
	MultiByteToWideChar(CP_UTF8, 0, aUtf8.c_str(), static_cast<int>(aUtf8.length()), &wstr[0], count);
	return wstr;
}




/** Converts the string from ucs2 to utf8. */
static std::string narrow(const std::vector<wchar_t> & aUcs2)
{
	int count = WideCharToMultiByte(CP_UTF8, 0, aUcs2.data(), static_cast<int>(aUcs2.size()), nullptr, 0, nullptr, nullptr);
	std::string str(count, 0);
	WideCharToMultiByte(CP_UTF8, 0, aUcs2.data(), static_cast<int>(aUcs2.size()), &str[0], count, nullptr, nullptr);
	return str;
}




/** Parses the URL into the server name, port and path parts, converting them from utf8 to ucs2 in the process.
Throws an Exception if the URL is malformed or not recognized. */
static std::tuple<bool /* IsSecure*/, std::wstring /* ServerName */ , std::uint16_t /* ServerPort */, std::wstring /* UrlPath */>
parseUrl(const std::string & aUrl)
{
	std::uint16_t port;
	size_t serverNameStart;
	bool isSecure;
	if (aUrl.compare(0, 8, "https://") == 0)
	{
		isSecure = true;
		port = 443;
		serverNameStart = 8;
	}
	else if (aUrl.compare(0, 7, "http://") == 0)
	{
		isSecure = false;
		port = 80;
		serverNameStart = 7;
	}
	else
	{
		throw Exception("The URL is malformed, expected http:// or https:// at the beginning.");
	}
	auto serverNameEnd = aUrl.find_first_of("/:", serverNameStart);
	if (serverNameEnd == std::string::npos)
	{
		if (aUrl.size() > serverNameStart)
		{
			// This is a plain URL in the form of "https://servername"
			return {isSecure, widen(aUrl.substr(serverNameStart)), port, L"/"};
		}
		throw Exception("The URL is malformed, expected a server name to follow the protocol specification.");
	}
	auto serverName = widen(aUrl.substr(serverNameStart, serverNameEnd - serverNameStart));
	if (aUrl[serverNameEnd] == ':')
	{
		auto portEnd = aUrl.find('/', serverNameEnd + 1);
		auto portInt = std::stoi(aUrl.substr(serverNameEnd + 1, (portEnd == std::string::npos) ? portEnd : (portEnd - serverNameEnd - 1)));
		if ((portInt < 0) || (portInt > 65535))
		{
			throw Exception(fmt::format("Invalid port specified in the URL, must be between 0 and 65535, got {}", portInt));
		}
		port = static_cast<std::uint16_t>(portInt);
		if (portEnd == std::string::npos)
		{
			// The URL is in the form "https://servername:port"
			return {isSecure, serverName, port, L"/"};
		}
		serverNameEnd = portEnd;
	}
	if (aUrl.length() == serverNameEnd)
	{
		return {isSecure, serverName, port, L"/"};
	}
	return {isSecure, serverName, port, widen(aUrl.substr(serverNameEnd))};
}





#ifdef _DEBUG
struct TestParseUrl
{
	TestParseUrl()
	{
		testParse("http://localhost:88/",          false, L"localhost",  88, L"/");
		testParse("https://localhost",             true,  L"localhost", 443, L"/");
		testParse("http://localhost",              false, L"localhost",  80, L"/");
		testParse("https://localhost:442",         true,  L"localhost", 442, L"/");
		testParse("http://localhost/path",         false, L"localhost",  80, L"/path");
		testParse("http://localhost/path/to/file", false, L"localhost",  80, L"/path/to/file");
	}

	/** Asserts that parsing the URL results in the expected values. */
	void testParse(const std::string & aUrl, bool aExpIsSecure, const std::wstring & aExpServerName, std::uint16_t aExpPort, const std::wstring & aExpPath)
	{
		auto [isSecure, serverName, port, path] = parseUrl(aUrl);
		assert(isSecure == aExpIsSecure);
		assert(serverName == aExpServerName);
		assert(port == aExpPort);
		assert(path == aExpPath);
	}
} gTest;
#endif  // _DEBUG





/** A singleton that opens the global HINTERNET handle for internet access, used by all the WinHttp functions. */
class Internet
{
	HINTERNET mInternet;

	Internet():
		mInternet(WinHttpOpen(L"LuaSimpleWinHttp/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0))
	{
	}

	~Internet()
	{
		WinHttpCloseHandle(mInternet);
	}


public:

	static Internet & instance()
	{
		static Internet inst;
		return inst;
	}

	HINTERNET handle() const { return mInternet; }
};





/** A simple RAII class that pops one value off the Lua stack upon leaving the scope. */
class LuaPopper
{
	/** The Lua state from which the value shall be popped. */
	lua_State * mState;


public:
	LuaPopper(lua_State * aState):
		mState(aState)
	{
	}

	~LuaPopper()
	{
		lua_pop(mState, 1);
	}
};





////////////////////////////////////////////////////////////////////////////////
// Exception:

Exception::Exception(std::string && aMessage):
	Super(std::move(aMessage))
{
}





int Exception::pushTo(lua_State * aState) const
{
	assert(aState != nullptr);
	lua_pushnil(aState);
	lua_pushstring(aState, what());
	return 2;
}





////////////////////////////////////////////////////////////////////////////////
// Request:

Request::Request(lua_State * aState, std::string && aHttpVerb):
	mState(aState),
	mHttpVerb(aHttpVerb),
	mConnection(nullptr),
	mRequest(nullptr)
{
}





Request::~Request()
{
	if (mRequest != nullptr)
	{
		WinHttpCloseHandle(mRequest);
	}
	if (mConnection != nullptr)
	{
		WinHttpCloseHandle(mConnection);
	}
}





std::string Request::readString(int aStackPos)
{
	size_t len;
	auto s = lua_tolstring(mState, aStackPos, &len);
	if (s == nullptr)
	{
		throw Exception(fmt::format("Expected a string at stack position {}.", aStackPos));
	}
	return std::string(s, len);
}





bool Request::hasAcceptHeader() const
{
	for (const auto & hdr: mAdditionalHeaders)
	{
		if (hdr.compare(0, 7, "Accept:") == 0)
		{
			return true;
		}
	}
	return false;
}





void Request::pushHeaders(const std::string & aAllHeaders)
{
	lua_newtable(mState);
	auto len = aAllHeaders.size();
	size_t idxStart = 0;
	lua_Integer num = 0;
	bool isFirst = true;
	for (size_t i = 0; i < len; ++i)
	{
		if (aAllHeaders[i] == '\r')
		{
			if (isFirst)
			{
				isFirst = false;
			}
			else
			{
				if (i > idxStart)  // Do not push empty headers
				{
					lua_pushlstring(mState, aAllHeaders.data() + idxStart, i - idxStart);
					lua_seti(mState, -2, ++num);
				}
			}
			idxStart = i + 2;
		}
	}
}





void Request::readUrl(int aStackPos)
{
	try
	{
		mUrl = readString(aStackPos);
	}
	catch (const Exception & exc)
	{
		throw Exception(fmt::format("Expected an URL string in parameter {} ({})", aStackPos, exc.what()));
	}
}





void Request::readBody(int aStackPos)
{
	try
	{
		mBody = readString(aStackPos);
	}
	catch (const Exception & exc)
	{
		throw Exception(fmt::format("Expected a request body string in parameter {} ({})", aStackPos, exc.what()));
	}
}





void Request::readContentType(int aStackPos, const std::string & aDefault)
{
	// If there is no param, use the default:
	if (lua_isnil(mState, aStackPos) || lua_isnone(mState, aStackPos))
	{
		mContentType = aDefault;
		return;
	}

	// Otherwise read the param:
	try
	{
		mContentType = readString(aStackPos);
	}
	catch (const Exception & exc)
	{
		throw Exception(fmt::format("Expected a request body content type string in parameter {} ({})", aStackPos, exc.what()));
	}
}





void Request::readParamsTable(int aStackPos)
{
	switch (lua_type(mState, aStackPos))
	{
		case LUA_TNIL:
		case LUA_TNONE:
		{
			return;
		}
		case LUA_TTABLE:
		{
			break;
		}
		default:
		{
			throw Exception(fmt::format("Expected a table of additional parameters in parameter {}, got a {}.", 
				aStackPos, lua_typename(mState, lua_type(mState, aStackPos)))
			);
		}
	}
	readParamsHeaders(aStackPos);
	// TODO: Other params
}





void Request::readParamsHeaders(int aParamsStackPos)
{
	lua_getfield(mState, aParamsStackPos, "headers");
	LuaPopper pop(mState);
	if (lua_isnil(mState, -1))
	{
		return;
	}
	if (!lua_istable(mState, -1))
	{
		throw Exception(fmt::format("Expected a table for the \"headers\" in additional parameters in parameter {}, got a {}.",
			aParamsStackPos, lua_typename(mState, lua_type(mState, -1)))
		);
	}
	for (int i = 1;;++i)
	{
		auto type = lua_geti(mState, -1, i);
		LuaPopper popV(mState);
		switch (type)
		{
			case LUA_TNIL:
			{
				return;
			}
			case LUA_TSTRING:
			{
				size_t len = 0;
				auto str = lua_tolstring(mState, -1, &len);
				mAdditionalHeaders.emplace_back(str, len);
				break;
			}
			default:
			{
				throw Exception(fmt::format("Expected a string header in the \"headers\" additional param, got a {} instead.",
					lua_typename(mState, type))
				);
			}
		}  // switch (type)
	}  // for i
}





int Request::make()
{
	assert(mConnection == nullptr);
	assert(mRequest == nullptr);

	auto [isSecure, serverName, port, path] = parseUrl(mUrl);
	mConnection = WinHttpConnect(Internet::instance().handle(), serverName.c_str(), port, 0);
	if (mConnection == nullptr)
	{
		throw Exception(fmt::format("Failed to start connecting to the server, WinHttpConnect() failed with error code 0x{:x}.", GetLastError()));
	}

	mRequest = WinHttpOpenRequest(mConnection, widen(mHttpVerb).c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, nullptr, WINHTTP_FLAG_ESCAPE_PERCENT | (isSecure ? WINHTTP_FLAG_SECURE : 0));
	if (mRequest == nullptr)
	{
		throw Exception(fmt::format("Failed to create request, WinHttpOpenRequest() failed with error code 0x{:x}.", GetLastError()));
	}

	if (!mBody.empty())
	{
		std::wstring headers = widen(fmt::format("Content-Type: {}", mContentType));
		if (!WinHttpAddRequestHeaders(mRequest, headers.data(), static_cast<DWORD>(headers.length()), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
		{
			throw Exception(fmt::format("Failed to set the Content-Type header, WinHttpAddRequestHeaders() failed with error code 0x{:x}", GetLastError()));
		}
	}

	std::wstring headers;
	for (const auto & hdr: mAdditionalHeaders)
	{
		if (!headers.empty())
		{
			headers.append(L"\r\n");
		}
		headers.append(widen(hdr));
	}
	if (!hasAcceptHeader())
	{
		headers.append(L"\r\nAccept: */*");
	}
	if (!WinHttpAddRequestHeaders(mRequest, headers.data(), static_cast<DWORD>(headers.length()), WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE))
	{
		throw Exception(fmt::format("Failed to set the additional headers, WinHttpAddRequestHeaders() failed with error code 0x{:x}", GetLastError()));
	}

	if (!WinHttpSendRequest(
		mRequest,
		WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		(mBody.empty() ? nullptr : mBody.data()), static_cast<DWORD>(mBody.size()),
		static_cast<DWORD>(mBody.size()),
		0
	))
	{
		throw Exception(fmt::format("Failed to send request, WinHttpSendRequest() failed with error code 0x{:x}.", GetLastError()));
	}

	if (!WinHttpReceiveResponse(mRequest, nullptr))
	{
		throw Exception(fmt::format("Failed to receive response, WinHttpReceiveResponse() failed with error code 0x{:x}.", GetLastError()));
	}

	// Retrieve the status code:
	DWORD statusCode;
	DWORD sizeStatusCode = sizeof(statusCode);
	if (!WinHttpQueryHeaders(
		mRequest,
		WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
		WINHTTP_HEADER_NAME_BY_INDEX,
		&statusCode, &sizeStatusCode,
		WINHTTP_NO_HEADER_INDEX
	))
	{
		throw Exception(fmt::format("Failed to retrieve response status code, WinHttpQueryHeaders() failed with error code 0x{:x}.", GetLastError()));
	}

	// Retrieve the status text:
	DWORD sizeStatusText;
	WinHttpQueryHeaders(
		mRequest,
		WINHTTP_QUERY_STATUS_TEXT,
		WINHTTP_HEADER_NAME_BY_INDEX,
		nullptr, &sizeStatusText,
		WINHTTP_NO_HEADER_INDEX
	);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		throw Exception(fmt::format("Failed to retrieve response status text size, WinHttpQueryHeaders() failed with error code 0x{:x}.", GetLastError()));
	}
	std::vector<wchar_t> statusTextW;
	statusTextW.resize(sizeStatusText);
	if (!WinHttpQueryHeaders(
		mRequest,
		WINHTTP_QUERY_STATUS_TEXT,
		WINHTTP_HEADER_NAME_BY_INDEX,
		statusTextW.data(), &sizeStatusText,
		WINHTTP_NO_HEADER_INDEX
	))
	{
		throw Exception(fmt::format("Failed to retrieve response status text, WinHttpQueryHeaders() failed with error code 0x{:x}.", GetLastError()));
	}
	auto statusText = narrow(statusTextW);

	// Retrieve all the headers:
	DWORD sizeAllHeaders;
	WinHttpQueryHeaders(
		mRequest,
		WINHTTP_QUERY_RAW_HEADERS_CRLF,
		WINHTTP_HEADER_NAME_BY_INDEX,
		nullptr, &sizeAllHeaders,
		WINHTTP_NO_HEADER_INDEX
	);
	if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
	{
		throw Exception(fmt::format("Failed to retrieve response headers size, WinHttpQueryHeaders() failed with error code 0x{:x}.", GetLastError()));
	}
	std::vector<wchar_t> allHeadersW;
	allHeadersW.resize(sizeAllHeaders);
	if (!WinHttpQueryHeaders(
		mRequest,
		WINHTTP_QUERY_RAW_HEADERS_CRLF,
		WINHTTP_HEADER_NAME_BY_INDEX,
		allHeadersW.data(), &sizeAllHeaders,
		WINHTTP_NO_HEADER_INDEX
	))
	{
		throw Exception(fmt::format("Failed to retrieve response headers, WinHttpQueryHeaders() failed with error code 0x{:x}.", GetLastError()));
	}
	auto allHeaders = narrow(allHeadersW);

	// Read the response body:
	std::string response;
	while (true)
	{
		char buf[8192];  // WinHttp docs say that this buffer should be *at least* 8 KiB
		DWORD bytesRead = 0;
		if (!WinHttpReadData(mRequest, buf, sizeof(buf), &bytesRead))
		{
			throw Exception(fmt::format("Failed to read HTTP response data, WinHttpReadData() failed with error code 0x{:x}.", GetLastError()));
		}
		if (bytesRead == 0)
		{
			break;
		}
		response.append(std::string(buf, bytesRead));
	}

	lua_pushlstring(mState, response.c_str(), response.size());
	lua_pushnumber(mState, statusCode);
	lua_pushlstring(mState, statusText.data(), statusText.size());
	pushHeaders(allHeaders);

	return 4;
}





}  // namespace LuaSimpleWinHttp
