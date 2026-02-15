#pragma once

#include <string>

// Widens a string to a wstring
std::wstring Widen(const std::string& str);

// Widens a string_view to a wstring. Allocates.
std::wstring Widen(std::string_view str);

// Narrows a wstring to a string
std::string Narrow(const std::wstring& str);
