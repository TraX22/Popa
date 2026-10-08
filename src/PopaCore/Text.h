// Text conversion between UTF-8 and UTF-16.
//
// Why two encodings? The Windows API works with UTF-16 strings (wchar_t,
// std::wstring): every "W" function such as CreateFileW or ShellExecuteExW
// expects them. Files on disk, on the other hand, are UTF-8 (menu.json,
// settings.json, the GitHub API). The program keeps text as std::wstring in
// memory and converts only at the edges, when reading or writing.
#pragma once

#include <string>
#include <string_view>

namespace popa {

// Converts UTF-8 bytes to a UTF-16 string. Invalid sequences become U+FFFD.
std::wstring Utf8ToWide(std::string_view utf8);

// Converts a UTF-16 string to UTF-8 bytes. Unpaired surrogates become U+FFFD.
std::string WideToUtf8(std::wstring_view wide);

} // namespace popa
