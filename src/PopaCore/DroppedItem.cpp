#include "PopaCore/DroppedItem.h"

#include <cwctype>

namespace popa {

namespace {

// "C:\Apps\Tool.exe" -> "Tool.exe". Trailing backslashes are ignored, and
// a drive root ("D:\") has no last part, so the path itself is used.
std::wstring FileName(std::wstring_view path)
{
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/'))
        path.remove_suffix(1);
    const size_t slash = path.find_last_of(L"\\/");
    std::wstring_view name = slash == std::wstring_view::npos ? path : path.substr(slash + 1);
    return std::wstring(name.empty() ? path : name);
}

std::wstring WithoutExtension(const std::wstring& name)
{
    const size_t dot = name.rfind(L'.');
    return (dot == std::wstring::npos || dot == 0) ? name : name.substr(0, dot);
}

bool HasExtension(const std::wstring& name, std::wstring_view extension)
{
    if (name.size() < extension.size())
        return false;
    for (size_t i = 0; i < extension.size(); ++i) {
        if (std::towlower(name[name.size() - extension.size() + i]) != extension[i])
            return false;
    }
    return true;
}

} // namespace

std::wstring QuoteIfNeeded(std::wstring_view path)
{
    if (path.find_first_of(L" \t") == std::wstring_view::npos)
        return std::wstring(path);
    return L"\"" + std::wstring(path) + L"\"";
}

MenuItem ItemForDroppedPath(std::wstring_view path, PathKind kind, const ShortcutInfo* shortcut)
{
    const std::wstring name = FileName(path);

    if (shortcut) {
        // A shortcut to something that is not a file (a Control Panel item,
        // a Store app) has no target path; running the .lnk itself works.
        std::wstring command = shortcut->target.empty() ? QuoteIfNeeded(path) : QuoteIfNeeded(shortcut->target);
        if (!shortcut->target.empty() && !shortcut->arguments.empty())
            command += L" " + shortcut->arguments;
        return MenuItem::MakeItem(WithoutExtension(name), std::move(command), shortcut->icon);
    }

    if (kind == PathKind::Directory)
        return MenuItem::MakeItem(name, QuoteIfNeeded(path));

    if (HasExtension(name, L".exe"))
        return MenuItem::MakeItem(WithoutExtension(name), QuoteIfNeeded(path));

    return MenuItem::MakeItem(name, QuoteIfNeeded(path));
}

} // namespace popa
