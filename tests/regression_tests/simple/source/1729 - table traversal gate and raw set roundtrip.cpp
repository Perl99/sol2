// sol2

// The MIT License (MIT)

// Copyright (c) 2013-2022 Rapptz, ThePhD and contributors

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <catch2/catch_all.hpp>

#include <sol/sol.hpp>

#include <string>

TEST_CASE("PR #1729 regression - global table traversal must not go tableless on a non-string first key", "[sol2][regression][table-traversal]") {
	const std::string magic_value = "test_value";

	sol::state lua;
	lua.open_libraries(sol::lib::base);

	// A string anywhere in the pack is not enough to raise the global flag:
	// global=true makes push_pop skip pushing the globals table and report table
	// index -1, so a non-string first key would index whatever sits on the stack.
	REQUIRE_NOTHROW(lua.globals().set(1, magic_value));
	REQUIRE(lua.globals()[1].get<std::string>() == magic_value);

	REQUIRE_NOTHROW(lua.globals().set("k", magic_value));
	REQUIRE(lua.globals()["k"].get<std::string>() == magic_value);

	// A later string key must not raise it either.
	lua.globals()[1] = lua.create_table();
	lua.globals()[1]["field"] = magic_value;
	REQUIRE(lua.globals().traverse_get<std::string>(1, "field") == magic_value);

	// tuple_set reuses one table_index for every pair, so all keys must qualify.
	REQUIRE_NOTHROW(lua.globals().set("flagged", 1, 2, magic_value));
	REQUIRE(lua.globals()["flagged"].get<int>() == 1);
	REQUIRE(lua.globals()[2].get<std::string>() == magic_value);

	// count_even_for_pack feeds create_named its array/hash size split.
	sol::table named = lua.globals().create_named("named", 1, "a", 2, "b");
	REQUIRE(named.get<std::string>(1) == "a");
	REQUIRE(named.get<std::string>(2) == "b");
}

TEST_CASE("PR #1729 - traverse_raw_set and traverse_raw_get roundtrip", "[sol2][regression][raw-table]") {
	const std::string magic_value = "test_value";

	sol::state lua;
	lua.open_libraries(sol::lib::base);

	// traverse_raw_set used to fail to compile: it called traverse_set_deep
	// without the leading table_index argument.
	REQUIRE_NOTHROW(lua.globals().traverse_raw_set(magic_value, magic_value));
	REQUIRE(lua.globals().traverse_raw_get<std::string>(magic_value) == magic_value);
	REQUIRE(lua.globals().raw_get<std::string>(magic_value) == magic_value);

	SECTION("on a non-global table") {
		sol::table t = lua.globals().create_named("multi");
		t.traverse_raw_set("a", 1);
		REQUIRE(t.traverse_raw_get<int>("a") == 1);
		REQUIRE(t.raw_get<int>("a") == 1);
	}

	SECTION("raw access ignores the metatable") {
		sol::table fallback = lua.create_table();
		fallback["missing"] = magic_value;
		sol::table t = lua.create_table();
		sol::table mt = lua.create_table();
		mt["__index"] = fallback;
		t[sol::metatable_key] = mt;

		REQUIRE(t.get<std::string>("missing") == magic_value);
		REQUIRE_FALSE(t.raw_get<sol::optional<std::string>>("missing"));
		t.traverse_raw_set("missing", magic_value);
		REQUIRE(t.raw_get<std::string>("missing") == magic_value);
	}
}
