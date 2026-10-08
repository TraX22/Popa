// The options dialog: hotkey, icon size, theme, start with Windows and
// automatic update check.
#pragma once

#include "PopaCore/Model.h"

#include <windows.h>

#include <optional>

namespace popa {

// Shows the dialog, modal to `owner`, with `current` as starting values.
// Returns the new settings when the user presses OK, no value on Cancel.
// Settings the dialog does not show (hotkeysEnabled) are kept as they were.
std::optional<Settings> ShowSettingsDialog(HWND owner, const Settings& current, bool darkTitleBar);

// When the dialog is already open, brings it to the front and returns true.
bool ActivateSettingsDialog();

} // namespace popa
