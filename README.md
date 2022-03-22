# LuaSimpleWinHttp
A library to add simple synchronous WinHttp client to Lua scripting. Does HTTP and HTTPS requests, is very easy to compile on Windows (no OpenSSL dependencies), works on all Windows version since XP SP1.

The WinHttp client automatically handles HTTP redirects (`301`, `302`) transparently. It also handles cookies set by the servers within the same session (script run).

## Supported operations
- `delete(url, options)`
- `get(url, options)`
- `head(url, options)`
- `post(url, body, contentType, options)`
- `put(url, body, contentType, options)`
- `request(verb, url, body, contentType, options)`

All functions return 4 values: The response body, the status code number, the status code text and an array-table of all response headers (`{"Name: Value", ...}`). If an error occurs, the functions return `nil` and an error description.

The `options` parameter is an optional table which can specify the additional request headers to use (`{headers = {"Name: Value", ...}}`)

## Example
```lua
local lswh = require("LuaSimpleWinHttp")

-- Get a webpage:
local resp, statusCode, statusText, headers = assert(
	lswh.get("https://github.com", {headers = {"Referrer: hidden"}})
)

-- Post to a web form:
resp, statusCode, _, headers = assert(lswh.post(
	"https://example.com",
	"username=example&password=secret",
	"application/x-www-form-urlencoded"
))

-- Call a web API:
local json = '{"title":"foo", "body":"bar", "userId":1}'
resp, statusCode, _, headers = assert(lswh.post(
	"https://jsonplaceholder.typicode.com/posts",
	json,
	"application/json; charset=UTF-8"
))
```

## Compiling
The library currently supports only static linking - that is, if you're embedding a Lua interpreter to an application, you can add this library. It is currently not possible to make a DLL for generic Lua interpreter.

The compilation uses standard CMake process and requires a C++17 compiler (tested only with VS 2019). It requires that the Lua library is already present in the build system as a `lua` target. It also requires the [{fmt}](https://github.com/fmtlib/fmt) library present in the build system, providing the `fmt::fmt-header-only` target

```
# Application's CMakeLists.txt
add_directory(lib/lua)
add_directory(lib/fmt)
add_directory(lib/LuaSimpleWinHttp)

target_link_libraries(App LuaSimpleWinHttp)
```
