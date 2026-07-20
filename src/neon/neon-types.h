#pragma once
#include <memory>
#include <ranges>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <string_view>
#include <span>

namespace neon {
    // Typed concept that allows iterating over a range
    // Usage: Function(IEnumerable<Type> auto items)
    template <class Rng, class T>
    concept IEnumerable =
        std::ranges::range<Rng> && // is a range
        std::input_iterator<std::ranges::iterator_t<Rng>> && // can forward iterate
        std::is_same_v<T, std::ranges::range_value_t<Rng>>; // Check that element type matches T

    // Scoped unique pointer.
    template<typename T, typename Deleter = std::default_delete<T>>
    using Ptr = std::unique_ptr<T, Deleter>;

    template<typename T>
    constexpr Ptr<T> MakePtr(auto&& ...args) {
        return std::make_unique<T>(std::forward<decltype(args)>(args)...);
    }

    // Reference counted shared pointer.
    template<typename T>
    using Ref = std::shared_ptr<T>;

    template<typename T>
    constexpr Ref<T> MakeRef(auto&& ...args) {
        return std::make_shared<T>(std::forward<decltype(args)>(args)...);
    }

    using std::make_unique;
    using std::string;
    using std::wstring;
    using std::string_view;
    using std::wstring_view;
    using std::span;

    using namespace std::string_view_literals;

    namespace views = std::views;
    namespace ranges = std::ranges;
    namespace filesystem = std::filesystem;


    using Exception = std::runtime_error;

    //struct Exception : public std::exception {
    //    template<class...TArgs>
    //    Exception(const string_view format, TArgs&&...args) :
    //        std::runtime_error(fmt::format(format, std::forward<TArgs>(args)...)) {}
    //};
    using ArgumentException = std::invalid_argument;
    struct IndexOutOfRangeException final : std::exception {
        const char* what() const override { return "Index out of range"; }
    };

    struct NotImplementedException final : std::exception {
        const char* what() const override { return "Not Implemented"; }
    };

    struct NotSupportedException final : std::exception {
        const char* what() const override { return "Operation not supported"; }
    };

    // ensure types are the expected size
    static_assert(sizeof(char) == 1);
    static_assert(sizeof(short) == 2);
    static_assert(sizeof(int) == 4);
    static_assert(sizeof(long long) == 8);

    using sbyte = char;
    using ubyte = unsigned char; // 'byte' in C#
    using int8 = int8_t;
    using uint8 = uint8_t;
    using int16 = int16_t;
    using uint16 = uint16_t;
    using int32 = int32_t;
    using uint32 = uint32_t;
    using int64 = int64_t;
    using uint64 = uint64_t;

    using uchar = unsigned char;
    using ushort = unsigned short;
    using uint = unsigned int;

    template<class T, class TAlloc = std::allocator<T>>
    using List = std::vector<T, TAlloc>;

    template<class T, size_t TSize>
    using Array = std::array<T, TSize>;

    template<class T>
    using Option = std::optional<T>;

    //template <class T, class TPr = std::less<T>, class TAlloc = std::allocator<T>>
    //using Set = std::set<T, TPr, TAlloc>;

    //template<class T, class U>
    //using Tuple = std::pair<T, U>;

    //template<class T>
    //using Stack = std::stack<T>;

    // <T, U, class _Hasher = hash<_Kty>, class _Keyeq = equal_to<_Kty>, class _Alloc = allocator<pair<const _Kty, _Ty>>
    template<class T, class U, class THasher = std::hash<T>>
    using Dictionary = std::unordered_map<T, U, THasher>;

    //template <class T, class TContainer = std::deque<T>>
    //using Queue = std::queue<T, TContainer>;

    using fix64 = int64; //64 bits int, for timers
    using fix = int32; //16 bits int, 16 bits frac
    using fixang = int16; //angles


    // Flags a type as non-copyable
    struct NonCopyable {
        NonCopyable(NonCopyable const&) = delete;
        NonCopyable& operator=(NonCopyable const&) = delete;
        NonCopyable(NonCopyable&&) = default;
        NonCopyable& operator=(NonCopyable&&) = default;
        ~NonCopyable() = default; // Must be public for move constructor to be implicitly defined

    protected:
        constexpr NonCopyable() = default;
    };

    // Flags a type as non-movable
    struct NonMovable {
        NonMovable(NonMovable const&) = delete;
        NonMovable& operator=(NonMovable const&) = delete;
        NonMovable(NonMovable&&) = delete;
        NonMovable& operator=(NonMovable&&) = delete;

    protected:
        constexpr NonMovable() = default;
        ~NonMovable() = default;
    };

}
