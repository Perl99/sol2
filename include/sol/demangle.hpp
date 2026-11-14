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

#ifndef SOL_DEMANGLE_HPP
#define SOL_DEMANGLE_HPP

#include <sol/string_view.hpp>
#include <array>

namespace sol { namespace detail {
	inline constexpr std::array<string_view, 9> removals { { "{anonymous}",
		"(anonymous namespace)",
		"public:",
		"private:",
		"protected:",
		"struct ",
		"class ",
		"`anonymous-namespace'",
		"`anonymous namespace'" } };

	// Only these characters can begin a token in `removals`. The constexpr evaluator interprets
	// the filter loop, so it pays a full call per token compare; rejecting a position with one
	// table lookup is what speeds up filtering a long signature in the compiler frontend.
	inline constexpr std::array<bool, 256> removal_head = [] {
		std::array<bool, 256> table { };
		for (std::size_t j = 0; j < removals.size(); ++j) {
			table[static_cast<unsigned char>(removals[j].front())] = true;
		}
		return table;
	}();

	// This isn't the most complete, but it'll do for now...?
	inline constexpr std::array<string_view, 11> ops { { "operator<",
		"operator<<",
		"operator<<=",
		"operator<=",
		"operator>",
		"operator>>",
		"operator>>=",
		"operator>=",
		"operator->",
		"operator->*",
		"operator<=>" } };

	inline constexpr string_view whitespace = " \t\n\r\f\v";

	constexpr string_view trim_name(string_view s) {
		const std::size_t first = s.find_first_not_of(whitespace);
		if (first == string_view::npos) {
			return { };
		}
		const std::size_t last = s.find_last_not_of(whitespace);
		return s.substr(first, last - first + 1);
	}

	// Returns length of the first token in `removals` that starts at `pos`, or 0 when none does.
	// `head` is the already-widened byte at `pos`.
	constexpr std::size_t matched_removal(string_view in, std::size_t pos, unsigned char head) {
		const std::size_t total = in.size();
		for (auto& to_remove : removals) {
			if (static_cast<unsigned char>(to_remove.front()) != head || pos + to_remove.size() > total) {
				continue;
			}
			std::size_t matching_length = 1;
			while (matching_length < to_remove.size() && in[pos + matching_length] == to_remove[matching_length]) {
				++matching_length;
			}
			if (matching_length == to_remove.size()) {
				return to_remove.size();
			}
		}
		return 0;
	}

	// Drops every occurrence of every token in `removals`, in list order, NUL-terminating and
	// returning the exact final length
	constexpr void filter_name(string_view in, char* out, std::size_t cap) {
		std::size_t out_pos = 0;
		const std::size_t total = in.size();
		for (std::size_t i = 0; i < total; ++i) {
			const auto head = static_cast<unsigned char>(in[i]);
			const std::size_t skip = removal_head[head] ? matched_removal(in, i, head) : 0;
			if (skip != 0) {
				i += skip - 1;
				continue;
			}
			if (out_pos < cap) {
				out[out_pos] = in[i];
			}
			++out_pos;
		}

		out[out_pos < cap ? out_pos : cap] = '\0';
	}

	// Returns length of the longest token in `ops` that ends exactly at `pos`, or 0 when none does.
	constexpr std::size_t longest_operator_at(string_view name, std::size_t pos) {
		std::size_t longest = 0;
		for (auto& op : ops) {
			if (op.size() <= pos + 1 && op.size() > longest && name.compare(pos - op.size() + 1, op.size(), op) == 0) {
				longest = op.size();
			}
		}
		return longest;
	}

	// Returns number of leading characters short_demangle() drops: everything up to and
	// including the last scope separator that sits outside every <>, () pair.
	constexpr std::size_t short_name_cut(string_view name) {
		if (name.size() <= 1) {
			return 0;
		}
		int level = 0;
		for (std::size_t idx = name.size() - 1; idx > 0; --idx) {
			const char ch = name[idx];
			if (level == 0 && ch == ':') {
				return name.size() < idx ? name.size() : idx + 1;
			}
			const bool is_left = ch == '<' || ch == '(';
			const bool is_right = ch == '>' || ch == ')';
			if (!is_left && !is_right) {
				continue;
			}
			// An operator name ending exactly here is a token, not a template bracket: rewind
			// over the whole token so its brackets never affect `level`. No token in `ops` ends
			// in '(' or ')', so only '<' and '>' can ever close one.
			const std::size_t longest = (ch == '<' || ch == '>') ? longest_operator_at(name, idx) : 0;
			if (longest != 0) {
				idx = idx - longest + 1;
				continue;
			}
			level += is_left ? -1 : 1;
		}
		return 0;
	}

	// `seperator_mark` exists only for GCC/Clang, whose __PRETTY_FUNCTION__ needs a second
	// template parameter to be printed at all. MSVC's __FUNCSIG__ spells out every defaulted
	// parameter, so adding it there would append ",int" to every name.
#if SOL_IS_ON(SOL_COMPILER_GCC) || SOL_IS_ON(SOL_COMPILER_CLANG)
	template <typename T, class seperator_mark = int>
	constexpr string_view ctti_get_type_name() {
		string_view f = __PRETTY_FUNCTION__;
		std::size_t start = f.find('[');
		start = f.find('=', start);
		std::size_t end = f.rfind(']');
		if (end == string_view::npos) {
			end = f.size();
		}
		if (start == string_view::npos) {
			start = 0;
		}
		else if (start + 1 < f.size()) {
			start += 1;
		}
		if (end < start) {
			end = start;
		}
		string_view name = trim_name(f.substr(start, end - start));
		const std::size_t sep = name.rfind("seperator_mark");
		if (sep != string_view::npos) {
			name = name.substr(0, sep > 2 ? sep - 2 : 0);
		}
		return trim_name(name);
	}
#elif SOL_IS_ON(SOL_COMPILER_VCXX)
	template <typename T>
	constexpr string_view ctti_get_type_name() {
		string_view f = __FUNCSIG__;
		constexpr string_view anchor = "ctti_get_type_name";
		std::size_t start = f.find(anchor);
		if (start == string_view::npos) {
			start = 0;
		}
		else {
			start += anchor.size();
		}
		if (start + 1 < f.size()) {
			start += 1;
		}
		std::size_t end = f.rfind('>');
		if (end == string_view::npos) {
			end = f.size();
		}
		if (end < start) {
			end = start;
		}
		return trim_name(f.substr(start, end - start));
	}
#else
#error Compiler not supported for demangling
#endif // compilers

	// filter_name() writes no terminator of its own, so the first NUL in a filled buffer sits
	// exactly one past the name.
	template <std::size_t N>
	constexpr std::size_t terminated_length(const std::array<char, N>& a) {
		std::size_t i = 0;
		while (i < N && a[i] != '\0') {
			++i;
		}
		return i;
	}

	// The one instantiation point per type. The signature is a static array, so nothing here
	// copies or allocates; short_demangle() is a suffix of the same buffer, so a type that
	// needs both names still costs one parse and one object. The buffer is sized by the raw
	// signature rather than the filtered name, because filtering only ever deletes.
	template <typename T>
	struct demangle_storage {
		static constexpr string_view signature = ctti_get_type_name<T>();
		static constexpr std::array<char, signature.size() + 1> fill() {
			std::array<char, signature.size() + 1> result { };
			filter_name(signature, result.data(), signature.size());
			return result;
		}
		static constexpr std::array<char, signature.size() + 1> filtered = fill();
		static constexpr std::size_t length = terminated_length(filtered);
		static constexpr string_view qualified { filtered.data(), length };
		static constexpr std::size_t cut = short_name_cut(qualified);
		static constexpr string_view short_name { filtered.data() + cut, length - cut };
	};

	template <typename T>
	constexpr string_view demangle() {
		return demangle_storage<T>::qualified;
	}

	template <typename T>
	constexpr string_view short_demangle() {
		return demangle_storage<T>::short_name;
	}
}} // namespace sol::detail

#endif // SOL_DEMANGLE_HPP
