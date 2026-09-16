#include "pch.h"
#include "Widechar.h"
#include <EASTL/vector.h>
#include <Windows.h>

std::wstring Widen(const std::string& str) {
    int size = MultiByteToWideChar(CP_UTF8, 0, str.data(), (int)str.size(), nullptr, 0);
    if (size < 0) return {}; // failure...
    eastl::vector<WCHAR> buffer((size_t)size);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), -1, buffer.data(), size);
    return { buffer.begin(), buffer.end() };
}

std::wstring Widen(std::string_view str) {
    return Widen(std::string(str));
}

std::string Narrow(const std::wstring& str) {
    int size = WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0, nullptr, nullptr);
    if (size < 0) return {}; // failure...
    eastl::vector<char> buffer((size_t)size);
    WideCharToMultiByte(CP_UTF8, 0, str.c_str(), (int)str.size(), buffer.data(), size, nullptr, nullptr);
    return { buffer.begin(), buffer.end() };
}
