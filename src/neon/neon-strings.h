#pragma once

#include <string>
#include <neon-types.h>

namespace neon {
// Helper to allow appending string views to each other. Allocates.
inline string operator+(string_view a, string_view b) {
    string dest(a.size() + b.size(), '\0');
    a.copy(dest.data(), a.size());
    b.copy(dest.data() + a.size(), b.size());
    return dest;
}

namespace String {
    constexpr bool Contains(string_view str, string_view value) {
        return str.find(value) != string::npos;
    }

    constexpr Option<size_t> IndexOf(string_view str, string_view value) {
        auto p = str.find(value);
        if (p == string::npos) return {};
        return p;
    }

    //inline string OfBytes(span<ubyte> data) {
    //    return { data.begin(), data.end() };
    //}

    template <class T>
    constexpr bool TryParse(string_view str, T& result) {
        return std::from_chars(str.data(), str.data() + str.size(), result).ec == std::errc{};
    }

    // Returns true if two strings are equal ignoring capitalization
    inline bool EqualsIgnoreCase(string_view s1, string_view s2) {
        if (s1.size() != s2.size()) return false;
        return _strnicmp(s1.data(), s2.data(), std::min(s1.size(), s2.size())) == 0; // NOLINT(bugprone-suspicious-stringview-data-usage)
    }

    // Returns true if two strings are equal ignoring capitalization, up to a number of characters
    inline bool EqualsIgnoreCase(string_view s1, string_view s2, size_t maxCount) {
        //if (s1.size() < maxCount || s2.size() < maxCount) return false;
        return _strnicmp(s1.data(), s2.data(), std::min({ maxCount, s1.size(), s2.size() })) == 0; // NOLINT(bugprone-suspicious-stringview-data-usage)
    }

    // Returns true if two strings are equal ignoring capitalization
    inline bool EqualsIgnoreCase(wstring_view s1, wstring_view s2) {
        if (s1.size() != s2.size()) return false;
        return _wcsnicmp(s1.data(), s2.data(), std::min(s1.size(), s2.size())) == 0; // NOLINT(bugprone-suspicious-stringview-data-usage)
    }

    // Returns < 0 when s1 < s2, 0 if equal, and > 0 when s2 > s1
    inline int CompareIgnoreCase(string_view s1, string_view s2) {
        if (s1.size() != s2.size()) return false;
        return _strnicmp(s1.data(), s2.data(), std::min(s1.size(), s2.size())); // NOLINT(bugprone-suspicious-stringview-data-usage)
    }

    // Returns the file name without the extension. Returns original string if no extension.
    inline string NameWithoutExtension(string_view str) {
        auto i = str.find('.');
        if (i == string::npos) return string(str);
        return string(str.substr(0, i));
    }

    // Returns the extension with the dot. Returns empty if no extension.
    constexpr string_view Extension(string_view str) {
        auto i = str.find('.');
        if (i == string::npos) return {};
        return str.substr(i);
    }

    // Returns the extension with the dot. Returns empty if no extension.
    constexpr wstring_view Extension(wstring_view str) {
        auto i = str.find('.');
        if (i == wstring::npos) return {};
        return str.substr(i);
    }

    constexpr bool HasExtension(string_view str) { return !Extension(str).empty(); }

    inline bool HasExtension(string_view str, string_view ext) {
        return EqualsIgnoreCase(Extension(str), ext);
    }

    constexpr string_view Whitespace = " \n\r\t\f\v";

    // Remove whitespace from the beginning
    inline string TrimStart(string_view s, string_view token = Whitespace) {
        auto start = s.find_first_not_of(token);
        return start == string::npos ? "" : string(s.substr(start));
    }

    // Remove whitespace from the end
    inline string TrimEnd(string_view s, string_view token = Whitespace) {
        auto end = s.find_last_not_of(token);
        return end == string::npos ? "" : string(s.substr(0, end + 1));
    }

    // Remove whitespace from both ends
    inline string Trim(string_view s, string_view token = Whitespace) {
        return TrimStart(TrimEnd(s, token), token);
    }

    // Returns an uppercase copy of the string
    [[nodiscard]] auto ToUpper(const auto str) {
        string buffer{ str };
        CharUpperA(buffer.data());
        return buffer;
    };

    // Returns a lowercase copy of the string.
    [[nodiscard]] auto ToLower(const auto str) {
        string buffer{ str };
        CharLowerA(buffer.data());
        return buffer;
    }

    // Splits a string into a vector. Returns the original string if no separator is found.
    inline List<string> Split(const string& str, const char separator = '\n', bool trim = false) {
        List<string> items;
        std::stringstream ss(str);
        string item;
        while (std::getline(ss, item, separator)) {
            std::erase(item, '\r'); // getline() returns \r even on windows
            items.push_back(trim ? String::Trim(item) : item);
        }

        return items;
    }

    string Join(auto&& strings, const string& delimiter = ", ") {
        string result;
        int index = 0;

        for (auto f : strings) {
            if (index++ > 0) result += delimiter;
            result += f;
        }

        return result;
    }

    // Splits a string into lines
    inline List<string> ToLines(const string& source) {
        List<string> lines;
        std::stringstream stream(source);

        string line;
        while (std::getline(stream, line)) {
            std::erase(line, '\r'); // getline() returns \r even on windows
            lines.push_back(line);
        }

        return lines;
    }

    // djb2 hash algorithm by Dan Bernstein.
    // Prefer using std::hash when compile time values aren't necessary.
    constexpr auto Hash(string_view s) noexcept {
        uint32 hash = 5381;

        for (auto& c : s)
            hash = ((hash << 5) + hash) + (uint)c;

        return hash;
    }
}
}
