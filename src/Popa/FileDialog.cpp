#include "Popa/FileDialog.h"

#include <shlobj.h>
#include <shobjidl.h>

namespace popa {

std::optional<std::wstring> BrowseForFile(HWND owner, const std::wstring& title,
                                          const std::vector<FileFilter>& filters, const std::wstring& initialPath)
{
    // IFileOpenDialog is the modern dialog (the one with the navigation
    // pane and the search box). It is a COM object, like the shortcut
    // object in ShortcutResolver.cpp.
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
        return std::nullopt;

    std::vector<COMDLG_FILTERSPEC> specs;
    for (const FileFilter& filter : filters)
        specs.push_back({filter.name.c_str(), filter.pattern.c_str()});
    if (!specs.empty())
        dialog->SetFileTypes(static_cast<UINT>(specs.size()), specs.data());
    dialog->SetTitle(title.c_str());

    // FOS_NODEREFERENCELINKS: picking a .lnk returns the .lnk itself, not
    // its target, so the user decides what goes in the menu.
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_NODEREFERENCELINKS);

    // Start in the folder of the current file, when there is one.
    if (!initialPath.empty()) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialPath.c_str(), nullptr, IID_PPV_ARGS(&item)))) {
            IShellItem* parent = nullptr;
            if (SUCCEEDED(item->GetParent(&parent))) {
                dialog->SetFolder(parent);
                parent->Release();
            }
            item->Release();
        }
    }

    std::optional<std::wstring> result;
    // Show() is modal: it returns when the dialog closes. Cancel gives an
    // error HRESULT (ERROR_CANCELLED), handled like any failure here.
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = path;
                CoTaskMemFree(path); // the dialog allocated it with the COM allocator
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

} // namespace popa
