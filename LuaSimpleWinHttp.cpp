#include "LuaSimpleWinHttp.h"

extern "C"
{
	#include <lauxlib.h>
}

#include "Request.h"





static int lswh_delete(lua_State * aState)
{
	LuaSimpleWinHttp::Request req(aState, "DELETE");
	try
	{
		req.readUrl(1);
		req.readParamsTable(2);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static int lswh_get(lua_State * aState)
{
	LuaSimpleWinHttp::Request req(aState, "GET");
	try
	{
		req.readUrl(1);
		req.readParamsTable(2);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static int lswh_head(lua_State * aState)
{
	LuaSimpleWinHttp::Request req(aState, "HEAD");
	try
	{
		req.readUrl(1);
		req.readParamsTable(2);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static int lswh_post(lua_State * aState)
{
	LuaSimpleWinHttp::Request req(aState, "POST");
	try
	{
		req.readUrl(1);
		req.readBody(2);
		req.readContentType(3, "application/x-www-form-urlencoded");
		req.readParamsTable(4);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static int lswh_put(lua_State * aState)
{
	LuaSimpleWinHttp::Request req(aState, "PUT");
	try
	{
		req.readUrl(1);
		req.readBody(2);
		req.readContentType(3, "application/x-www-form-urlencoded");
		req.readParamsTable(4);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static int lswh_request(lua_State * aState)
{
	// Read the method name:
	size_t len;
	auto s = lua_tolstring(aState, 1, &len);
	if (s == nullptr)
	{
		lua_pushnil(aState);
		lua_pushstring(aState, "Expected a http verb (string) in the first parameter.");
		return 2;
	}


	LuaSimpleWinHttp::Request req(aState, {s, len});
	try
	{
		req.readUrl(2);
		req.readBody(3);
		req.readContentType(4, "application/x-www-form-urlencoded");
		req.readParamsTable(5);
		return req.make();
	}
	catch (const LuaSimpleWinHttp::Exception & exc)
	{
		return exc.pushTo(aState);
	}
}





static const struct luaL_Reg lswhlib[] =
{
	{"delete",  &lswh_delete},
	{"get",     &lswh_get},
	{"head",    &lswh_head},
	{"post",    &lswh_post},
	{"put",     &lswh_put},
	{"request", &lswh_request},
	{nullptr, nullptr},
};





int luaopen_LuaSimpleWinHttp(lua_State * aState)
{
	luaL_openlib(aState, "LuaSimpleWinHttp", lswhlib, 0);
	return 1;
}
