#include "PopaCore/IconRef.h"

#include <windows.h>

#include <charconv>

namespace popa {

namespace {

std::wstring_view Trim(std::wstring_view text)
{
    while (!text.empty() && (text.front() == L' ' || text.front() == L'\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == L' ' || text.back() == L'\t'))
        text.remove_suffix(1);
    return text;
}

std::wstring_view StripQuotes(std::wstring_view text)
{
    text = Trim(text);
    if (text.size() >= 2 && text.front() == L'"' && text.back() == L'"')
        text = Trim(text.substr(1, text.size() - 2));
    return text;
}

// Parses an optionally negative decimal integer that fills the whole text.
// std::from_chars works on char, so the (ASCII) digits are narrowed first.
bool ParseIndex(std::wstring_view text, int& value)
{
    text = Trim(text);
    if (text.empty() || text.size() > 11)
        return false;
    std::string narrow;
    for (wchar_t c : text) {
        if (!(c == L'-' || (c >= L'0' && c <= L'9')))
            return false;
        narrow.push_back(static_cast<char>(c));
    }
    const char* end = narrow.data() + narrow.size();
    const auto [ptr, error] = std::from_chars(narrow.data(), end, value);
    return error == std::errc() && ptr == end;
}

std::wstring Join(std::wstring_view folder, std::wstring_view name)
{
    std::wstring result(folder);
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/')
        result += L'\\';
    result += name;
    return result;
}

} // namespace

IconSpec ParseIconRef(std::wstring_view text)
{
    // Quotes around everything ("C:\app.exe:0") are removed first; quotes
    // around the path only ("C:\app.exe":0) are handled further down.
    text = StripQuotes(text);

    // The index follows the LAST colon. A colon right after the drive letter
    // ("C:\...") is followed by a backslash, not digits, so it never parses
    // as an index and the whole text stays the path.
    const size_t colon = text.rfind(L':');
    if (colon != std::wstring_view::npos) {
        int index = 0;
        if (ParseIndex(text.substr(colon + 1), index))
            return {std::wstring(StripQuotes(text.substr(0, colon))), index};
    }
    return {std::wstring(StripQuotes(text)), 0};
}

std::wstring FormatIconRef(const IconSpec& icon)
{
    return icon.path + L":" + std::to_wstring(icon.index);
}

std::wstring ResolveIconPath(std::wstring_view path, std::wstring_view exeDirectory,
                             std::wstring_view systemDirectory, const PathProbe& probe)
{
    const std::wstring expanded = ExpandEnvironment(StripQuotes(path));
    if (expanded.empty() || IsAbsolutePath(expanded))
        return expanded;

    const bool bareName = expanded.find_first_of(L"\\/") == std::wstring::npos;
    if (bareName) {
        // An icon file shipped next to Popa.exe wins over a system file.
        std::wstring local = Join(exeDirectory, expanded);
        if (probe(local) == PathKind::File)
            return local;
        return Join(systemDirectory, expanded);
    }
    return Join(exeDirectory, expanded);
}

std::wstring ResolveIconPath(std::wstring_view path, std::wstring_view exeDirectory)
{
    wchar_t systemDirectory[MAX_PATH] = {};
    GetSystemDirectoryW(systemDirectory, MAX_PATH);
    return ResolveIconPath(path, exeDirectory, systemDirectory);
}

} // namespace popa
