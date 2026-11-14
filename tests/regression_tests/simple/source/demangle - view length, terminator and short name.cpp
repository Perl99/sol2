// sol2

// The MIT License (MIT)

// Copyright (c) 2013-2022 Rapptz, ThePhD, and contributors

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

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sol2_regression_test_demangle {
	struct plain {
		int field;
		int method() {
			return field;
		}
		static void smethod() {
		}
	};

	enum plain_enum { pe_a, pe_b };
	enum class scoped_enum { se_b };

	namespace outer {
		struct nested {
			struct deep {
				int x;
			};
		};
	} // namespace outer

	namespace {
		struct anonymous_local {
			int x;
		};
	} // namespace

	template <typename T>
	struct templated {
		T value;
	};

	struct has_field {
		int counter;
		std::string name;
		std::vector<int> items;
	};

	struct operators {
		int field;
		int operator+(const operators&) const {
			return 0;
		}
		bool operator==(const operators&) const {
			return true;
		}
		operators* operator->() {
			return this;
		}
		operators& operator*() {
			return *this;
		}
		int& operator[](int) {
			return field;
		}
		operators operator()(int) {
			return *this;
		}
		int operator<<(int) const {
			return 0;
		}
		bool operator<=(const operators&) const {
			return true;
		}
		int operator>>(int) const {
			return 0;
		}
	};

	int free_function(int a) {
		return a;
	}

	struct point {
		double x;
		double y;
	};
} // namespace sol2_regression_test_demangle

namespace {
	// Index of the first NUL within the first `limit` bytes, or `limit` when there is
	// none. A bounded scan rather than std::strlen on purpose: short_demangle() is an
	// interior pointer into the shared name buffer, and GCC's -Wstringop-overread
	// (enabled by -Wall, and the test build adds -Werror) cannot prove short_name_cut()
	// stays inside the array, so it rejects an unbounded strlen on it. Callers pass
	// size() + 1, which is the stronger assertion anyway: the terminator must sit at
	// exactly the byte right after the counted characters.
	constexpr std::size_t first_nul(const char* p, std::size_t limit) {
		std::size_t n = 0;
		while (n < limit && p[n] != '\0') {
			++n;
		}
		return n;
	}

	// Invariants that hold for every supported compiler, so the test constrains the
	// contract of demangle()/short_demangle() instead of pinning a spelling.
	template <typename T>
	void check_name_invariants() {
		const std::string_view q = sol::detail::demangle<T>();
		const std::string_view s = sol::detail::short_demangle<T>();

		// A string_view length is a character count. Counting the terminator as a
		// character inflates every consumer, including lua_pushlstring in usertype_core.
		REQUIRE(first_nul(q.data(), q.size() + 1) == q.size());
		REQUIRE(first_nul(s.data(), s.size() + 1) == s.size());
		REQUIRE(!q.empty());
		REQUIRE(!s.empty());
		CHECK(q.back() != '\0');
		CHECK(s.back() != '\0');

		// Names are trimmed, so they stay usable as metatable keys.
		CHECK(q.front() != ' ');
		CHECK(q.back() != ' ');

		// No removal token may survive filtering.
		for (std::string_view r : sol::detail::removals) {
			CHECK(q.find(r) == std::string_view::npos);
		}

		// short_demangle() must be a suffix of demangle() of the same buffer.
		CHECK(q.size() >= s.size());
		CHECK(q.substr(q.size() - s.size()) == s);

		// The metatable key is derived from exactly the qualified name.
		CHECK(sol::usertype_traits<T>::metatable() == ("sol." + std::string(q)));
		CHECK(sol::usertype_traits<T>::qualified_name() == q);
		CHECK(sol::usertype_traits<T>::name() == s);

		// The name is a static object, so repeated calls hand back the same storage.
		CHECK(sol::detail::demangle<T>().data() == q.data());
	}
} // namespace

TEST_CASE("demangle - view length, terminator and suffix contract", "[sol2][regression][demangle]") {
	using namespace sol2_regression_test_demangle;

	check_name_invariants<plain>();
	check_name_invariants<plain*>();
	check_name_invariants<plain&>();
	check_name_invariants<const plain>();
	check_name_invariants<plain_enum>();
	check_name_invariants<scoped_enum>();
	check_name_invariants<outer::nested>();
	check_name_invariants<outer::nested::deep>();
	check_name_invariants<anonymous_local>();
	check_name_invariants<templated<int>>();
	check_name_invariants<templated<outer::nested>>();
	check_name_invariants<templated<std::vector<int>>>();
	check_name_invariants<int has_field::*>();
	check_name_invariants<decltype(&has_field::counter)>();
	check_name_invariants<decltype(&has_field::items)>();
	check_name_invariants<void (plain::*)()>();
	check_name_invariants<void (plain::*)() const>();
	check_name_invariants<int (plain::*)()>();
	check_name_invariants<int (*)(int)>();
	check_name_invariants<decltype(&free_function)>();
	check_name_invariants<decltype(&plain::method)>();
	check_name_invariants<decltype(&plain::smethod)>();
	check_name_invariants<decltype(&operators::operator+)>();
	check_name_invariants<decltype(&operators::operator==)>();
	check_name_invariants<decltype(&operators::operator->)>();
	check_name_invariants<decltype(&operators::operator*)>();
	check_name_invariants<decltype(&operators::operator[])>();
	check_name_invariants<decltype(&operators::operator())>();
	check_name_invariants<decltype(&operators::operator<<)>();
	check_name_invariants<decltype(&operators::operator<=)>();
	check_name_invariants<decltype(&operators::operator>>)>();
	check_name_invariants<std::function<int(int)>>();
	check_name_invariants<std::unique_ptr<plain>>();
	check_name_invariants<std::shared_ptr<plain>>();
	check_name_invariants<std::vector<int>>();
	check_name_invariants<std::string>();
	check_name_invariants<int>();
	check_name_invariants<double>();
	check_name_invariants<void>();
	check_name_invariants<sol::state*>();

	auto lambda = [](int a, int b) { return a + b; };
	check_name_invariants<decltype(lambda)>();
	check_name_invariants<decltype([=](int a) { return a; })>();
}

TEST_CASE("demangle - distinct types get distinct names", "[sol2][regression][demangle]") {
	using namespace sol2_regression_test_demangle;

	CHECK(sol::detail::demangle<plain>() != sol::detail::demangle<plain*>());
	CHECK(sol::detail::demangle<plain>() != sol::detail::demangle<const plain>());
	CHECK(sol::detail::demangle<plain_enum>() != sol::detail::demangle<scoped_enum>());
	CHECK(sol::detail::demangle<outer::nested>() != sol::detail::demangle<outer::nested::deep>());
	CHECK(sol::detail::demangle<templated<int>>() != sol::detail::demangle<templated<outer::nested>>());
	CHECK(sol::detail::demangle<void (plain::*)()>() != sol::detail::demangle<void (plain::*)() const>());
	CHECK(sol::detail::demangle<int (plain::*)()>() != sol::detail::demangle<int (*)(int)>());
	CHECK(sol::detail::demangle<decltype(&plain::method)>() != sol::detail::demangle<decltype(&plain::smethod)>());
	CHECK(sol::detail::demangle<int has_field::*>() != sol::detail::demangle<decltype(&has_field::items)>());
	CHECK(sol::detail::demangle<plain_enum>() != sol::detail::demangle<anonymous_local>());
}

TEST_CASE("demangle - __type.name in Lua carries no embedded terminator", "[sol2][regression][demangle]") {
	using namespace sol2_regression_test_demangle;

	const std::string expected = std::string(sol::detail::demangle<point>());

	sol::state lua;
	lua.open_libraries(sol::lib::base, sol::lib::string);
	lua.new_usertype<point>("point", sol::constructors<point()>(), "x", &point::x);

	point p { 1.0, 2.0 };
	lua["p"] = &p;
	lua["expected_len"] = static_cast<int>(expected.size());
	lua["expected_name"] = expected;

	auto result = lua.safe_script(R"lua(
		local mt = getmetatable(p)
		assert(mt ~= nil, "no metatable on the userdata")
		local tt = rawget(mt, "__type")
		assert(tt ~= nil and tt.name ~= nil, "no __type.name")
		local n = tt.name
		assert(#n == expected_len, string.format("length %d, expected %d", #n, expected_len))
		assert(n == expected_name, "__type.name is not the demangled name")
		assert(not n:find(string.char(0), 1, true), "__type.name contains an embedded NUL")
		return n
	)lua",
	     sol::script_pass_on_error);
	REQUIRE(result.valid());
	REQUIRE(result.get<std::string>() == expected);
}
