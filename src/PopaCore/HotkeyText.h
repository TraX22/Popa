// Hotkeys written as text, such as "Win+Z" or "Ctrl+Alt+Space", as stored
// in settings.json and shown in the options dialog.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace popa {

struct Hotkey {
    bool win = false;
    bool ctrl = false;
    bool alt = false;
    bool shift = false;
    unsigned key = 0; // Windows virtual-key code (VK_...), never a modifier

    bool operator==(const Hotkey&) const = default;
};

// Parses "Modifier+...+Key". Names are case-insensitive and spaces around
// "+" are allowed. Modifiers: Win, Ctrl (or Control), Alt, Shift.
// Keys: A-Z, 0-9, F1-F24, Space, Tab, Enter, Esc, Backspace, Insert,
// Delete, Home, End, PageUp, PageDown, Up, Down, Left, Right, Pause.
// Rejected, because they would get in the way of normal typing:
//   - a key without modifiers, except F1-F24;
//   - Shift as the only modifier ("Shift+A" would block typing "A").
std::optional<Hotkey> ParseHotkey(std::wstring_view text);

// The canonical text: modifiers in the order Win, Ctrl, Alt, Shift.
std::wstring FormatHotkey(const Hotkey& hotkey);

// Names of every key a hotkey can use, in the order shown to the user:
// A-Z, 0-9, F1-F24, then the named keys (Space, Tab, ...).
std::vector<std::wstring> HotkeyKeyNames();

} // namespace popa
