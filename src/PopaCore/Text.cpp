#include "PopaCore/Text.h"

#include <windows.h>

namespace popa {

// Both functions follow the usual two-call pattern of the Windows API:
// the first call, with no output buffer, returns the required length; the
// second call fills a buffer of exactly that size.

std::wstring Utf8ToWide(std::string_view utf8)
{
    if (utf8.empty())
        return {};

    const int inputLength = static_cast<int>(utf8.size());
    const int length = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), inputLength, nullptr, 0);
    if (length <= 0)
        return {};

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), inputLength, result.data(), length);
    return result;
}

std::string WideToUtf8(std::wstring_view wide)
{
    if (wide.empty())
        return {};

    const int inputLength = static_cast<int>(wide.size());
    const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), inputLength,
                                           nullptr, 0, nullptr, nullptr);
    if (length <= 0)
        return {};

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), inputLength, result.data(), length,
                        nullptr, nullptr);
    return result;
}

} // namespace popa
