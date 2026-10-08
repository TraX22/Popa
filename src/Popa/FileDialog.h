// The standard "Open" dialog of Windows, to pick one file.
#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

namespace popa {

struct FileFilter {
    std::wstring name;    // shown to the user: "Programs"
    std::wstring pattern; // "*.exe;*.lnk"
};

// Shows the dialog and returns the chosen file's full path, or no value
// when the user cancels. `initialPath` (optional) selects a starting file.
std::optional<std::wstring> BrowseForFile(HWND owner, const std::wstring& title,
                                          const std::vector<FileFilter>& filters,
                                          const std::wstring& initialPath = {});

} // namespace popa
