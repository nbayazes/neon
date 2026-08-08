#pragma once

#include <string_view>
#include "neon-strings.h"
#include "unordered_dense.h"

// Comparator for equality of strings ignoring case
struct StringEqualsIgnoreCase {
    using is_transparent = int;

    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const {
        return neon::String::EqualsIgnoreCase(a, b);
    }
};

struct StringEquals {
    using is_transparent = int;

    [[nodiscard]] bool operator()(std::string_view a, std::string_view b) const {
        return a == b;
    }
};

// hash that works with any string compatible with string_view
struct StringHash {
    using is_transparent = void; // enable heterogeneous overloads
    using is_avalanching = void; // mark class as high quality avalanching hash

    [[nodiscard]] uint64_t operator()(std::string_view str) const noexcept {
        return ankerl::unordered_dense::hash<std::string_view>{}(str);
    }
};

struct StringHashIgnoreCase {
    using is_transparent = void; // enable heterogeneous overloads
    using is_avalanching = void; // mark class as high quality avalanching hash

    [[nodiscard]] uint64_t operator()(std::string_view str) const noexcept {
        return ankerl::unordered_dense::hash<std::string_view>{}(neon::String::ToLower(str));
    }
};

namespace neon {
// <T, U, class _Hasher = hash<_Kty>, class _Keyeq = equal_to<_Kty>, class _Alloc = allocator<pair<const _Kty, _Ty>>
//template <class T, class U, class THasher = std::hash<T>>
//using Dictionary = ankerl::unordered_dense::map<T, U, THasher>;

template <class Key,
          class T,
          class Hash = ankerl::unordered_dense::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class AllocatorOrContainer = std::allocator<std::pair<Key, T>>,
          class Bucket = ankerl::unordered_dense::bucket_type::standard>
using Dictionary = ankerl::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, false>;

template <class T>
using CaseInsensitiveDictionary = ankerl::unordered_dense::detail::table<std::string, T, StringHashIgnoreCase, StringEqualsIgnoreCase, std::allocator<std::pair<std::string, T>>, ankerl::unordered_dense::bucket_type::standard, false>;

}
