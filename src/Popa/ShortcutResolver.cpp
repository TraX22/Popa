#include "Popa/ShortcutResolver.h"

#include <windows.h>
#include <shlobj.h>

#include <cwctype>
#include <vector>

namespace popa {

bool IsShortcutFile(const std::wstring& path)
{
    if (path.size() < 4)
        return false;
    std::wstring extension = path.substr(path.size() - 4);
    for (wchar_t& c : extension)
        c = static_cast<wchar_t>(std::towlower(c));
    return extension == L".lnk";
}

// A .lnk file is not plain text: it is a binary format that Windows reads
// through COM. COM objects are created by class ID (here CLSID_ShellLink,
// "the shortcut object") and used through interfaces, tables of functions
// identified by an interface ID:
//   - IShellLinkW reads and writes the shortcut's fields;
//   - IPersistFile loads and saves it from a file.
// The same object offers both interfaces; QueryInterface asks it for the
// second one. Every interface pointer obtained must be released with
// Release() when no longer needed: COM objects count their users and
// delete themselves when the count reaches zero.
std::optional<ShortcutInfo> ReadShortcut(const std::wstring& path)
{
    IShellLinkW* link = nullptr;
    IPersistFile* file = nullptr;
    std::optional<ShortcutInfo> result;

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link));
    if (SUCCEEDED(hr))
        hr = link->QueryInterface(IID_PPV_ARGS(&file));
    if (SUCCEEDED(hr))
        hr = file->Load(path.c_str(), STGM_READ);

    if (SUCCEEDED(hr)) {
        ShortcutInfo info;
        // Paths can be long; arguments too (up to 32K characters).
        std::vector<wchar_t> buffer(32768, L'\0');
        const int size = static_cast<int>(buffer.size());

        // SLGP_RAWPATH keeps environment variables unexpanded
        // ("%windir%\notepad.exe"), so the entry keeps working if they
        // point elsewhere on another PC. GetPath fails (or returns S_FALSE
        // and an empty string) when the target is not a file.
        if (link->GetPath(buffer.data(), size, nullptr, SLGP_RAWPATH) == S_OK)
            info.target = buffer.data();

        buffer[0] = L'\0';
        if (SUCCEEDED(link->GetArguments(buffer.data(), size)))
            info.arguments = buffer.data();

        buffer[0] = L'\0';
        int iconIndex = 0;
        if (SUCCEEDED(link->GetIconLocation(buffer.data(), size, &iconIndex)) && buffer[0] != L'\0')
            info.icon = IconSpec{buffer.data(), iconIndex};

        result = std::move(info);
    }

    if (file)
        file->Release();
    if (link)
        link->Release();
    return result;
}

} // namespace popa
