// The icon in the notification area (next to the clock) and its right-click menu.
#pragma once

#include <windows.h>

namespace popa {

// Commands of the tray icon's context menu. The values are arbitrary but
// must be non-zero: TrackPopupMenuEx returns 0 when the user picks nothing.
enum TrayCommand : UINT {
    kTrayOpenEditor = 1,
    kTrayOptions,
    kTrayCheckUpdates,
    kTrayToggleHotkeys,
    kTrayExit,
};

class TrayIcon {
public:
    TrayIcon() = default;
    ~TrayIcon();
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Adds the icon. Mouse and keyboard events on it arrive at `window` as
    // `callbackMessage` (see App::OnTrayEvent for how to read them).
    bool Add(HWND window, UINT callbackMessage, HINSTANCE instance, int iconId);

    // Adds it again after Explorer restarted (the "TaskbarCreated" message).
    bool Recreate();

    void Remove();

    // Shows a notification ("balloon" / toast) coming from the icon.
    void ShowNotification(const wchar_t* title, const wchar_t* text, bool warning);

    // Shows the right-click menu at `anchor` and returns the chosen
    // TrayCommand, or 0 when the menu was dismissed.
    UINT ShowContextMenu(POINT anchor, bool hotkeysEnabled);

private:
    bool AddToShell();
    void ReloadIcon(); // not "LoadIcon": <windows.h> has a macro with that name

    HWND window_ = nullptr;
    UINT callbackMessage_ = 0;
    HINSTANCE instance_ = nullptr;
    int iconId_ = 0;
    HICON icon_ = nullptr;
    bool added_ = false;
};

} // namespace popa
