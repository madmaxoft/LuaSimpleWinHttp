# Simple CMakeLists.txt to build LuaSimpleWinHttp as a static library (assuming Lua is a static library as well)
cmake_minimum_required(VERSION 3.8)
project (LuaSimpleWinHttp CXX)

# We need C++17 features:
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
	add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
endif()

add_library(LuaSimpleWinHttp STATIC
	LuaSimpleWinHttp.cpp
	LuaSimpleWinHttp.h
	Request.cpp
	Request.h
)

target_link_libraries(LuaSimpleWinHttp
	fmt::fmt-header-only
	lua
	winhttp
)

target_include_directories(LuaSimpleWinHttp
	SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)
