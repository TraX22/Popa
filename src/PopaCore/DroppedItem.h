// Turning a file dropped on the editor into a menu entry (section 5.6 of
// the design document). Reading the .lnk itself needs COM and lives in the
// application (ShortcutResolver); this part is only the rules, so it can be
// tested without real files.
#pragma once

#include "PopaCore/CommandLine.h"
#include "PopaCore/Model.h"

#include <optional>
#include <string>
#include <string_view>

namespace popa {

// What a shortcut (.lnk) points to.
struct ShortcutInfo {
    std::wstring target;    // empty for targets that are not files (e.g. Control Panel items)
    std::wstring arguments;
    std::optional<IconSpec> icon; // the shortcut's own icon, if it has one
};

// Wraps a path in double quotes when it contains spaces, so the command
// splits correctly later: C:\Program Files\a.exe -> "C:\Program Files\a.exe".
std::wstring QuoteIfNeeded(std::wstring_view path);

// Builds the entry for a dropped path:
//   shortcut  -> name of the .lnk, its target and arguments, its icon
//   .exe      -> file name without extension, the .exe
//   folder    -> folder name, the folder
//   other     -> file name, the file
// `shortcut` is the content of the .lnk when the path is one, else null.
MenuItem ItemForDroppedPath(std::wstring_view path, PathKind kind, const ShortcutInfo* shortcut);

} // namespace popa
