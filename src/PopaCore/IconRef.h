// Icon references in the "file:index" text form, and
// resolution of icon paths to real files.
//
// Examples of the text form:
//   shell32.dll:13                        icon 13 of shell32.dll
//   %SystemRoot%\System32\SHELL32.dll:130 with an environment variable
//   "C:\Program Files\App\app.exe":0      quotes around the path
//   C:\icons\app.ico                      no index means 0
#pragma once

#include "PopaCore/CommandLine.h"
#include "PopaCore/Model.h"

#include <string>
#include <string_view>

namespace popa {

// Reads the "file:index" form. Quotes around the path are removed; the path
// is not expanded or resolved. Empty text gives an empty path.
IconSpec ParseIconRef(std::wstring_view text);

// Writes the "file:index" form, for showing an icon reference to the user.
std::wstring FormatIconRef(const IconSpec& icon);

// Turns an icon path from menu.json into a full path:
// - environment variables are expanded (%SystemRoot%);
// - absolute paths stay as they are;
// - bare names (pifmgr.dll) are looked up next to Popa.exe first, then in
//   the Windows system folder (System32);
// - other relative paths (icons\adobe.ico) are relative to Popa.exe's folder.
std::wstring ResolveIconPath(std::wstring_view path, std::wstring_view exeDirectory,
                             std::wstring_view systemDirectory,
                             const PathProbe& probe = ProbeFileSystem);

// Same, with the real System32 folder (GetSystemDirectoryW).
std::wstring ResolveIconPath(std::wstring_view path, std::wstring_view exeDirectory);

} // namespace popa
