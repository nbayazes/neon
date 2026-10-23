#pragma once

#include <algorithm>
#include <concepts>
#include <future>
#include <set>
#include <EASTL/vector.h>
#include "neon-types.h"
#include "neon-strings.h"

namespace neon {
// Creates a four character code to identify file formats
consteval uint32 MakeFourCC(const char cc[4]) {
    // this is the same as assigning the characters backwards to an int
    // int i = 'dcba';
    return cc[0] | cc[1] << 8 | cc[2] << 16 | cc[3] << 24;
}

//constexpr std::array<char, 4> DecodeFourCC(uint32 value) {
//    std::array<char, 4> cc{};
//    cc[0] = char(value & 0x000000ff);
//    cc[1] = char((value & 0x0000ff00) >> 8);
//    cc[2] = char((value & 0x00ff0000) >> 16);
//    cc[3] = char((value & 0xff000000) >> 24);
//    return cc;
//}

// Get the total size of a vector in bytes
template <typename T>
constexpr size_t GetVectorSizeInBytes(const eastl::vector<T>& v) {
    return v.size() * sizeof(T);
}

// Get the total size of a vector in bytes
template <typename T>
constexpr size_t GetVectorSizeInBytes(const std::vector<T>& v) {
    return v.size() * sizeof(T);
}

// Equivalent to hlsl smoothstep
constexpr float SmoothStep(float a, float b, float x) {
    x = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

// Polynomial smooth min between two values.
// https://iquilezles.org/articles/smin/
constexpr float SmoothMin(float a, float b, float k) {
    float h = std::clamp(0.5f + 0.5f * (a - b) / k, 0.0f, 1.0f);
    return SmoothStep(a, b, h) - k * h * (1.0f - h);
}

constexpr bool IsPowerOfTwo(std::unsigned_integral auto v) {
    return v != 0 && (v & (v - 1)) == 0;
}

// Aligns an offset to a power of two
constexpr uint32 AlignTo(uint32 offset, uint32 alignment) {
    ASSERT(IsPowerOfTwo(alignment));
    //return (offset + alignment - 1) / alignment * alignment;
    return (offset + (alignment - 1)) & ~(alignment - 1);
}

// Aligns an offset to a power of two
constexpr uint64 AlignTo(uint64 offset, uint64 alignment) {
    ASSERT(IsPowerOfTwo(alignment));
    //return (offset + alignment - 1) / alignment * alignment;
    return (offset + (alignment - 1)) & ~(alignment - 1);
}


// Returns 1 for positive numbers and 0, -1 for negative numbers
template <typename T>
constexpr int Sign(T val) {
    if (val == 0) return 1;
    return (T(0) < val) - (val < T(0));
}

// Combines two unsigned ints into an optimized value
constexpr uint32 SzudzikPairing(uint16 a, uint16 b) {
    return a >= b ? a * a + a + b : a + b * b;
}

// Executes a function on a new thread asynchronously
void StartAsync(auto&& fun) {
    auto future = std::make_shared<std::future<void>>();
    // ReSharper disable once CppLambdaCaptureNeverUsed
    *future = std::async(std::launch::async, [future, fun] {
        fun();
    }); // future disposes itself on exit
}

namespace Seq {
    // Converts a std::set to a std::vector
    template <class T>
    constexpr auto ofSet(const std::set<T>& set) {
        return eastl::vector<T>(set.begin(), set.end());
    }

    // Converts a span to a std::vector
    template <class T>
    constexpr auto toList(const std::span<T> xs) {
        return eastl::vector<T>(xs.begin(), xs.end());
    }

    // Returns true if the index is valid for a container
    constexpr bool inRange(auto&& xs, size_t index) {
        return index < xs.size();
    }

    // Inserts a container into a set
    template <class T>
    constexpr void insert(std::set<T>& dest, auto&& src) {
        dest.insert(src.begin(), src.end());
    }

    // Generates a new list by mapping a function to each element. Causes heap allocation.
    template <class T, class Fn>
    [[nodiscard]] auto map(T&& xs, Fn&& fn) {
        // dereference the first element in a collection to determine the type
        using TElement = std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;
        List<std::invoke_result_t<Fn, TElement>> r;
        r.reserve(std::size(xs));
        for (auto& x : xs)
            r.push_back(fn(x));
        return r;
    }

    // Generates a new list by mapping a function to each element along with an index. 
    // Lambda parameters are (i, elem). Causes heap allocation.
    template <class T, class Fn>
    [[nodiscard]] auto mapi(T&& xs, Fn&& fn) {
        // dereference the first element in a collection to determine the type
        using TElement = std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;
        List<std::invoke_result_t<Fn, TElement>> r;
        r.reserve(std::size(xs));

        int i = 0;
        for (auto& x : xs)
            r.push_back(fn(i++, x));

        return r;
    }

    // Executes a function on each element.
    constexpr void iter(auto&& xs, auto&& fn) {
        for (auto& x : xs)
            fn(x);
    }

    // Executes a function on each element with the parameters (i, element).
    constexpr void iteri(auto&& xs, auto&& fn) {
        for (size_t i = 0; i < std::size(xs); i++)
            fn(i, xs[i]);
    }

    // Moves the contents of src to the end of dest
    constexpr void move(auto& dest, auto&& src) {
        std::move(src.begin(), src.end(), std::back_inserter(dest));
    }

    // Copies the contents of src to the end of dest
    constexpr void append(auto& dest, const auto& src) {
        std::copy(src.begin(), src.end(), std::back_inserter(dest));
    }

    // Returns a pointer to an element in the collection. Null if not found.
    auto find(auto& xs, auto&& predicate) {
        auto iter = std::find_if(std::begin(xs), std::end(xs), predicate);
        return iter == std::end(xs) ? nullptr : &(*iter);
    }

    // Returns true if an element is found in the collection.
    constexpr bool contains(auto&& xs, auto&& element) {
        auto iter = std::find(std::begin(xs), std::end(xs), element);
        return iter != std::end(xs);
    }

    // Returns a true if any element satisfies the predicate.
    auto exists(auto& xs, auto&& predicate) {
        return std::find_if(std::begin(xs), std::end(xs), predicate) != std::end(xs);
    }

    // Sorts a range in ascending order by a function (a, b) -> bool
    constexpr void sortBy(auto&& xs, auto&& fn) {
        std::ranges::sort(xs, fn);
    }

    // Sorts a range in ascending order
    constexpr void sort(auto&& xs) {
        std::ranges::sort(xs);
    }

    // Sorts a range in descending order
    constexpr void sortDescending(auto&& xs) {
        std::ranges::sort(xs, ranges::greater());
    }

    // Tries to retrieve an element at index. Returns nullptr if not in range.
    constexpr auto* tryItem(auto&& xs, auto index) {
        return inRange(xs, index) ? &xs[index] : nullptr;
    }

    // Returns the index of an element
    constexpr std::optional<size_t> indexOf(auto&& xs, auto&& element) {
        auto iter = std::find(std::begin(xs), std::end(xs), element);
        if (iter == xs.end()) return {};
        return std::distance(std::begin(xs), iter);
    }

    // Returns the index of an element
    constexpr std::optional<size_t> findIndex(auto&& xs, auto&& predicate) {
        auto iter = std::find_if(std::begin(xs), std::end(xs), predicate);
        if (iter == xs.end()) return {};
        return std::distance(std::begin(xs), iter);
    }

    constexpr bool remove(auto&& xs, auto&& element) {
        auto iter = std::find(std::begin(xs), std::end(xs), element);
        if (iter == xs.end()) return false;
        xs.erase(iter);
        return true;
    }

    // Removes an element at index.
    constexpr bool removeAt(auto&& xs, size_t index) {
        if (!inRange(xs, index)) return false;
        xs.erase(xs.begin() + index);
        return true;
    }

    // Filters a collection. Causes heap allocation.
    template <class T>
    [[nodiscard]] auto filter(const T& xs, auto&& predicate) {
        using TElement = std::remove_reference_t<decltype(*std::begin(std::declval<T&>()))>;
        List<TElement> result(std::size(xs));
        auto iter = std::copy_if(std::begin(xs), std::end(xs), result.begin(), predicate);
        result.resize(std::distance(result.begin(), iter));
        return result;
    }

    // Specialization to filter a collection of strings by a value. Causes heap allocation.
    [[nodiscard]] List<string> filter(const auto& xs, string value, bool invariant) {
        if (invariant) {
            value = String::ToLower(value);
            return filter(xs, [&](const string& e) {
                return String::ToLower(e).find(value) != string::npos;
            });
        }
        else {
            return filter(xs, [&](const string& e) { return e.find(value) != string::npos; });
        }
    }
}

// Converts a file name to 8.3 format
inline string FormatShortFileName(string_view fileName) {
    auto i = (int)fileName.find('.', 0);
    auto name = i == -1 ? fileName.substr(0, 8) : fileName.substr(0, std::min(8, i));
    auto ext = i == -1 ? "" : fileName.substr(i, 4); // extension is optional

    // todo: discard spaces, convert invalid characters to underscores

    return String::TrimEnd(string(name)) + String::TrimEnd(string(ext));
}

inline bool ExtensionEquals(const std::filesystem::path& path, string ext) {
    if (!path.has_extension()) return false;
    if (!ext.starts_with('.')) ext.insert(0, ".");

    return String::EqualsIgnoreCase(path.extension().string(), ext);
}
}
