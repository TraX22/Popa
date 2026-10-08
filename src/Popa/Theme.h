// Light and dark theme: detection, colors of the popup menu, and dark mode
// for the menus Windows draws (borders, scroll arrows, tray menu).
#pragma once

#include "PopaCore/Model.h"

#include <windows.h>

namespace popa {

struct MenuColors {
    COLORREF background;
    COLORREF text;
    COLORREF disabledText;
    COLORREF highlight; // background of the row under the mouse
    COLORREF separator;
};

// True when Windows apps are set to dark ("Choose your app mode" in
// Settings > Personalization > Colors).
bool SystemUsesDarkTheme();

// Resolves the "theme" option: System follows Windows, Light/Dark force it.
bool IsDarkTheme(Theme setting);

MenuColors MenuColorsFor(bool dark);

// Tells Windows to draw this process's menus dark or light. Uses an
// undocumented uxtheme.dll function; does nothing where it is missing.
void ApplyMenuTheme(bool dark);

// True when a WM_SETTINGCHANGE message reports a light/dark theme change.
bool IsThemeChangeMessage(LPARAM lParam);

} // namespace popa
