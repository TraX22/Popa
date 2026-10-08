// Running the command of a menu entry.
#pragma once

#include "PopaCore/Model.h"

#include <windows.h>

namespace popa {

// True when the command's target can be found: an existing file or
// folder, a program in the PATH, or a URL / protocol ("https://", "ms-settings:").
bool TargetExists(const std::wstring& command);

// Runs the entry's command. On failure shows a short message with the
// reason, owned by `owner`, and returns false.
bool Launch(const MenuItem& item, HWND owner);

} // namespace popa
