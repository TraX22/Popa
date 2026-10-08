// The popup menu with the user's programs.
//
// It is a native Windows menu (CreatePopupMenu + TrackPopupMenuEx), so
// Windows provides cascading submenus, keyboard navigation, closing on an
// outside click, keeping it on screen, scroll arrows for tall menus and,
// on Windows 11, the rounded corners, shadow and animation. Every row is
// "owner-drawn" (MFT_OWNERDRAW): Windows asks the owner window for each
// row's size (WM_MEASUREITEM) and lets it paint the row (WM_DRAWITEM),
// which is how the icons at 16-48 px and the custom highlight are drawn.
#pragma once

#include "Popa/IconCache.h"
#include "Popa/Theme.h"
#include "PopaCore/Model.h"

#include <windows.h>

#include <deque>
#include <vector>

namespace popa {

class PopupMenu {
public:
    PopupMenu() = default;
    ~PopupMenu();
    PopupMenu(const PopupMenu&) = delete;
    PopupMenu& operator=(const PopupMenu&) = delete;

    // Shows the menu at `at` (screen coordinates) and waits until it closes.
    // Returns the chosen entry, or null when the menu was dismissed.
    // `owner` receives the owner-draw messages and must forward them to
    // HandleMessage while the menu is open.
    const MenuItem* Show(HWND owner, const std::vector<MenuItem>& items, POINT at, int iconSize, bool dark,
                         IconCache& icons);

    bool IsOpen() const { return open_; }

    // Handles WM_MEASUREITEM, WM_DRAWITEM and WM_MENUCHAR for the open menu.
    // Returns true (and sets `result`) when the message was ours.
    bool HandleMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result);

private:
    // What each row of the menu shows; the menu stores a pointer to it.
    struct Row {
        const MenuItem* item = nullptr; // null for the "(empty)" placeholder
    };

    HMENU Build(const std::vector<MenuItem>& items);
    void AddPlaceholder(HMENU menu);
    void Measure(MEASUREITEMSTRUCT& measure);
    void Draw(const DRAWITEMSTRUCT& draw);
    LRESULT OnMenuChar(wchar_t character, HMENU menu);
    int Scale(int value) const;
    void ReleaseResources();

    // Valid only while Show() runs.
    std::deque<Row> rows_; // a deque never moves its elements, so pointers stay valid
    std::vector<const MenuItem*> commands_; // command ID - 1 -> entry
    IconCache* icons_ = nullptr;
    MenuColors colors_{};
    HFONT font_ = nullptr;
    HBRUSH backgroundBrush_ = nullptr;
    int dpi_ = 96;
    int iconPixels_ = 32;
    bool open_ = false;
};

} // namespace popa
