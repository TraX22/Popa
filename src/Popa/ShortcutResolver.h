// Reading Windows shortcuts (.lnk files).
#pragma once

#include "PopaCore/DroppedItem.h"

#include <optional>
#include <string>

namespace popa {

// True when the path ends in ".lnk".
bool IsShortcutFile(const std::wstring& path);

// Reads the target, arguments and icon of a .lnk file. No value when the
// file cannot be read as a shortcut. COM must be initialized on the thread.
std::optional<ShortcutInfo> ReadShortcut(const std::wstring& path);

} // namespace popa
