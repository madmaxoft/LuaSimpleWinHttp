#pragma once

#include <string>
#include <stdexcept>
#include <vector>

#define NOMINMAX
#include <Windows.h>
#include <winhttp.h>





// fwd: lua.h
struct lua_State;





namespace LuaSimpleWinHttp
{





/** Exception that is thrown on LSWH errors.
Supports pushing its information to a Lua state for ease of use. */
class Exception:
	public std::runtime_error
{
	using Super = std::runtime_error;


public:

	Exception(std::string && aDescription);

	/** Pushes the exception details onto the Lua state and returns the number of items pushed.
	Used to provide a return value from a function call.
	The first value pushed is always a nil, to signalize an error to the Lua script. */
	int pushTo(lua_State * aState) const;
};





/** Represents a single HTTP request being made.
Usage:
- Create an instance
- Read the parameters from Lua VM to the instance (readX() methods)
- Call make() to actually send the request and receive the response
- Delete the instance
One instance can only make one request. The make() function cannot be called multiple times. */
class Request
{
	/** The Lua VM that is tied to this request. */
	lua_State * mState;

	/** The HTTP verb ("GET", "POST" etc.) for the request. */
	std::string mHttpVerb;

	/** The entire url for this requests.
	Includes the protocol, server, path, query - everything. */
	std::string mUrl;

	/** The body of the request to send. */
	std::string mBody;

	/** The content type of the body to send. */
	std::string mContentType;

	/** The HTTP connection used for this request, as returned by WinHttpConnect(). */
	HINTERNET mConnection;

	/** The HTTP request representation in WinHttp, as returned by WinHttpOpenRequest(). */
	HINTERNET mRequest;

	/** The additional headers to add to the request.
	Each item is a single header in the "Name: Value" form. */
	std::vector<std::string> mAdditionalHeaders;


	/** Returns the string at the specified Lua stack position.
	Throws a general Exception if there's no string there. */
	std::string readString(int aStackPos);

	/** Returns true if there is an "Accept:" header in mAdditionalHeaders.
	Used to detect whether a synthetic Accept header should be appended to the request. */
	bool hasAcceptHeader() const;

	/** Parses the returned headers and pushes them onto the Lua stack as an array-table of "Name: Value" strings.
	The first "header" is the status code and text, those are skipped. */
	void pushHeaders(const std::string & aAllHeaders);


public:

	/** Creates a new Request object tied to the specified state that will use the specified HTTP verb. */
	Request(lua_State * aState, std::string && aHttpVerb);

	~Request();

	/** Reads the URL to be requested from the Lua stack at the specified position.
	Throws an Exception on error. */
	void readUrl(int aStackPos);

	/** Reads the body to be sent from the Lua stack at the specified position.
	Throws an Exception on error. */
	void readBody(int aStackPos);

	/** Reads the content type to be sent from the Lua stack at the specified position.
	If the position is not valid, uses aDefault instead.
	Throws an Exception on error. */
	void readContentType(int aStackPos, const std::string & aDefault);

	/** Reads the parameter table further configuring the request, such as additional headers to add,
	from the Lua stack at the specified position.
	Throws an Exception on error (there's a non-table value). */
	void readParamsTable(int aStackPos);

	/** Reads the optional headers from the table at the the specified position of the Lua stack. */
	void readParamsHeaders(int aParamsStackPos);

	/** Makes the request.
	Connects to the server, sends the request, receives the response and pushes it onto mState's stack.
	Throws an Exception on error.
	Returns the number of values pushed onto the Lua stack. */
	int make();
};

}
