#include "PopaCore/CommandLine.h"

#include <windows.h>

#include <cwctype>

namespace popa {

namespace {

bool IsSpace(wchar_t c)
{
    return c == L' ' || c == L'\t';
}

std::wstring_view Trim(std::wstring_view text)
{
    while (!text.empty() && IsSpace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back()))
        text.remove_suffix(1);
    return text;
}

} // namespace

PathKind ProbeFileSystem(const std::wstring& path)
{
    // GetFileAttributesW answers "does it exist, and is it a folder?" in one
    // cheap call without opening the file.
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return PathKind::None;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) ? PathKind::Directory : PathKind::File;
}

std::wstring ExpandEnvironment(std::wstring_view text)
{
    if (text.find(L'%') == std::wstring_view::npos)
        return std::wstring(text);

    // ExpandEnvironmentStringsW needs a null-terminated input, which a
    // string_view does not guarantee, so copy it into a std::wstring first.
    const std::wstring input(text);

    // Two-call pattern: the first call returns the size needed, including
    // the terminating null character.
    const DWORD needed = ExpandEnvironmentStringsW(input.c_str(), nullptr, 0);
    if (needed == 0)
        return input;

    std::wstring result(needed, L'\0');
    const DWORD written = ExpandEnvironmentStringsW(input.c_str(), result.data(), needed);
    if (written == 0 || written > needed)
        return input;
    result.resize(written - 1); // drop the null terminator the API wrote
    return result;
}

SplitCommand Split(std::wstring_view command, const PathProbe& probe)
{
    const std::wstring_view text = Trim(command);
    if (text.empty())
        return {};

    // Rule 1: quoted target.
    if (text.front() == L'"') {
        const size_t closing = text.find(L'"', 1);
        if (closing == std::wstring_view::npos) // unbalanced quote: take it all
            return {std::wstring(Trim(text.substr(1))), {}};
        return {std::wstring(text.substr(1, closing - 1)),
                std::wstring(Trim(text.substr(closing + 1)))};
    }

    const size_t firstSpace = text.find_first_of(L" \t");
    if (firstSpace == std::wstring_view::npos)
        return {std::wstring(text), {}};

    // Rule 2: the whole text names something that exists.
    if (probe(std::wstring(text)) != PathKind::None)
        return {std::wstring(text), {}};

    // Rule 3: shortest existing file before a space. Only files count: a
    // folder such as "D:\My" must not cut "D:\My Apps\app.exe" in two.
    for (size_t i = firstSpace; i != std::wstring_view::npos; i = text.find_first_of(L" \t", i + 1)) {
        const std::wstring prefix(text.substr(0, i));
        if (probe(prefix) == PathKind::File)
            return {prefix, std::wstring(Trim(text.substr(i)))};
    }

    // Rule 4: first space.
    return {std::wstring(text.substr(0, firstSpace)), std::wstring(Trim(text.substr(firstSpace)))};
}

SplitCommand ParseCommand(std::wstring_view command, const PathProbe& probe)
{
    return Split(ExpandEnvironment(command), probe);
}

bool IsAbsolutePath(std::wstring_view path)
{
    if (path.size() >= 3 && std::iswalpha(path[0]) && path[1] == L':' &&
        (path[2] == L'\\' || path[2] == L'/'))
        return true;
    return path.size() >= 3 && path[0] == L'\\' && path[1] == L'\\';
}

std::wstring CompleteExtension(std::wstring_view target, std::wstring_view pathExt,
                               const PathProbe& probe)
{
    std::wstring original(target);
    if (!IsAbsolutePath(target) || probe(original) != PathKind::None)
        return original;

    // A dot after the last backslash means the name already has an extension.
    const size_t lastSlash = target.find_last_of(L"\\/");
    const size_t lastDot = target.rfind(L'.');
    if (lastDot != std::wstring_view::npos && lastDot > lastSlash)
        return original;

    while (!pathExt.empty()) {
        const size_t separator = pathExt.find(L';');
        const std::wstring_view extension = Trim(pathExt.substr(0, separator));
        pathExt = separator == std::wstring_view::npos ? std::wstring_view{} : pathExt.substr(separator + 1);
        if (extension.empty())
            continue;
        std::wstring candidate = original + std::wstring(extension);
        if (probe(candidate) == PathKind::File)
            return candidate;
    }
    return original;
}

std::wstring CompleteExtension(std::wstring_view target)
{
    std::wstring pathExt = ExpandEnvironment(L"%PATHEXT%");
    if (pathExt == L"%PATHEXT%") // not set
        pathExt = L".COM;.EXE;.BAT;.CMD";
    return CompleteExtension(target, pathExt, ProbeFileSystem);
}

std::wstring WorkingDirectoryFor(std::wstring_view target)
{
    if (!IsAbsolutePath(target))
        return {};

    const size_t lastSlash = target.find_last_of(L"\\/");
    if (lastSlash < 2) // "\\server" alone: no folder to speak of
        return {};
    // "C:\app.exe": the folder is the drive root, keep its backslash ("C:\").
    if (lastSlash == 2 && target[1] == L':')
        return std::wstring(target.substr(0, 3));
    return std::wstring(target.substr(0, lastSlash));
}

} // namespace popa
