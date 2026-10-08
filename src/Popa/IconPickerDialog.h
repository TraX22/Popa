// Dialog that shows every icon of a file in a grid to pick one.
#pragma once

#include "PopaCore/Model.h"

#include <windows.h>

#include <optional>
#include <string>

namespace popa {

// Shows the dialog, modal to `owner`, starting with `initial` (the path as
// the user wrote it, possibly with variables, and the selected index).
// `exeDirectory` resolves relative paths. Returns the chosen icon, or no
// value when the user cancels.
std::optional<IconSpec> PickIcon(HWND owner, const IconSpec& initial, const std::wstring& exeDirectory);

} // namespace popa
