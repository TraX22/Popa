// The global hotkey (Win+Z by default), with a low-level keyboard hook.
//
// Why a hook instead of RegisterHotKey? RegisterHotKey is the normal API
// for global hotkeys, but Windows 11 reserves most Win+letter combinations
// for itself (Win+Z opens the Snap Layouts flyout) and refuses to register
// them. A low-level keyboard hook (WH_KEYBOARD_LL) sees every key press in
// the session before any program does, so Popa can recognize the
// combination and "consume" it so nobody else reacts to it.
#pragma once

#include "PopaCore/HotkeyText.h"

#include <windows.h>

#include <thread>

namespace popa {

class HotkeyListener {
public:
    HotkeyListener() = default;
    ~HotkeyListener();
    HotkeyListener(const HotkeyListener&) = delete;
    HotkeyListener& operator=(const HotkeyListener&) = delete;

    // Starts watching for `hotkey`; each press posts `message` to `target`.
    // Returns false when Windows refused to install the hook.
    bool Start(const Hotkey& hotkey, HWND target, UINT message);

    // Stops watching. Safe to call when not running.
    void Stop();

    bool IsRunning() const { return thread_.joinable(); }

private:
    static LRESULT CALLBACK HookProc(int code, WPARAM wParam, LPARAM lParam);
    bool OnKey(WPARAM message, const KBDLLHOOKSTRUCT& key);
    void ThreadMain(bool* installed, HANDLE ready);

    std::thread thread_;
    DWORD threadId_ = 0;

    // Read by the hook thread only after Start() set them.
    Hotkey hotkey_;
    HWND target_ = nullptr;
    UINT message_ = 0;
    bool comboDown_ = false; // between the hotkey's key-down and key-up
};

} // namespace popa
