#pragma once

namespace neon {
// defined in C++23
template <class T>
inline constexpr bool is_scoped_enum_v = std::conjunction_v<std::is_enum<T>, std::negation<std::is_convertible<T, int>>>;

template <class T>
struct is_scoped_enum : std::bool_constant<is_scoped_enum_v<T>> {};

// Templates to enable bitwise operators on all enums. Might be a bad idea.

template <class T> requires is_scoped_enum_v<T>
constexpr T operator |(T lhs, T rhs) {
    return T((std::underlying_type_t<T>)lhs | (std::underlying_type_t<T>)rhs);
}

template <class T> requires is_scoped_enum_v<T>
T& operator |=(T& lhs, T rhs) {
    return lhs = lhs | rhs;
}

template <class T> requires is_scoped_enum_v<T>
constexpr T operator &(T lhs, T rhs) {
    return T((std::underlying_type_t<T>)lhs & (std::underlying_type_t<T>)rhs);
}

template <class T> requires is_scoped_enum_v<T>
T& operator &=(T& lhs, T rhs) {
    return lhs = lhs & rhs;
}

template <class T> requires is_scoped_enum_v<T>
T& operator ~(T& value) {
    return value = T(~(int)value);
}

template <class T>
constexpr void SetFlag(T& flags, T flag) { flags |= flag; }

template <class T>
constexpr bool HasFlag(const T& flags, T flag) { return bool(flags & flag); }

template <class T>
constexpr void ClearFlag(T& flags, T flag) { flags &= ~flag; }

template <class T>
constexpr void SetFlag(T& flags, T flag, bool value) {
    if (value) flags |= flag;
    else flags &= ~flag;
}

template <class T>
concept IsEnum = is_scoped_enum_v<T>;

//// Converts an enum to the underlying type
//constexpr auto ToUnderlying(IsEnum auto e) {
//    return static_cast<std::underlying_type<declspec(e)>::type>(e);
//};

//inline _ENUM_FLAG_CONSTEXPR ENUMTYPE operator | (ENUMTYPE a, ENUMTYPE b) WIN_NOEXCEPT { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) | ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
    //    inline ENUMTYPE& operator |= (ENUMTYPE& a, ENUMTYPE b) WIN_NOEXCEPT { return (ENUMTYPE&)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type&)a) |= ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
    //    inline _ENUM_FLAG_CONSTEXPR ENUMTYPE operator & (ENUMTYPE a, ENUMTYPE b) WIN_NOEXCEPT { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) & ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
    //    inline ENUMTYPE& operator &= (ENUMTYPE& a, ENUMTYPE b) WIN_NOEXCEPT { return (ENUMTYPE&)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type&)a) &= ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
    //    inline _ENUM_FLAG_CONSTEXPR ENUMTYPE operator ~ (ENUMTYPE a) WIN_NOEXCEPT { return ENUMTYPE(~((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a)); } \
    //    inline _ENUM_FLAG_CONSTEXPR ENUMTYPE operator ^ (ENUMTYPE a, ENUMTYPE b) WIN_NOEXCEPT { return ENUMTYPE(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)a) ^ ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \
    //    inline ENUMTYPE& operator ^= (ENUMTYPE& a, ENUMTYPE b) WIN_NOEXCEPT { return (ENUMTYPE&)(((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type&)a) ^= ((_ENUM_FLAG_SIZED_INTEGER<ENUMTYPE>::type)b)); } \

}
