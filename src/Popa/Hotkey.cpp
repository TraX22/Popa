#include "Popa/Hotkey.h"

namespace popa {

namespace {

// A virtual-key code that no keyboard has ("unassigned" in the Windows
// list). Popa sends it as a harmless extra key press, see SendNeutralKey.
constexpr WORD kNeutralKey = 0xE8;

// The hook procedure is a plain function with no "context" parameter, so
// it reaches the listener through this pointer. Only one listener exists
// at a time, and only its hook thread uses the pointer.
HotkeyListener* g_listener = nullptr;

bool IsDown(int vk)
{
    // The high bit of GetAsyncKeyState tells whether the key is down now.
    return (GetAsyncKeyState(vk) & 0x8000) != 0;
}

// Why this is needed: Windows opens the Start menu when the Win key is
// pressed and released with no other key in between. Popa swallows the Z,
// so from Windows' point of view the user pressed Win alone, and Start
// would open on release. Likewise a lone Alt focuses the menu bar of the
// active window. Sending a key press that means nothing (0xE8) while the
// modifier is still down tells Windows "another key was used", and
// nothing opens. It is the same trick AutoHotkey and similar tools use.
void SendNeutralKey()
{
    INPUT inputs[2]{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = kNeutralKey;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

} // namespace

HotkeyListener::~HotkeyListener()
{
    Stop();
}

bool HotkeyListener::Start(const Hotkey& hotkey, HWND target, UINT message)
{
    Stop();
    hotkey_ = hotkey;
    target_ = target;
    message_ = message;
    comboDown_ = false;

    // The new thread reports through `installed` whether the hook worked;
    // the event ("ready") makes this thread wait until it knows.
    bool installed = false;
    HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ready)
        return false;
    thread_ = std::thread(&HotkeyListener::ThreadMain, this, &installed, ready);
    WaitForSingleObject(ready, INFINITE);
    CloseHandle(ready);

    if (!installed) {
        thread_.join();
        return false;
    }
    return true;
}

void HotkeyListener::Stop()
{
    if (!thread_.joinable())
        return;
    // WM_QUIT ends the hook thread's message loop; then wait for it to end.
    PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
    thread_.join();
    threadId_ = 0;
}

void HotkeyListener::ThreadMain(bool* installed, HANDLE ready)
{
    threadId_ = GetCurrentThreadId();

    // A thread only gets a message queue when it first touches one. This
    // PeekMessageW creates it, so Stop()'s PostThreadMessageW cannot fail.
    MSG message;
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    // Windows calls a low-level hook *on the thread that installed it*,
    // by sending it a message, and waits for the answer before the key
    // reaches any program. So this thread must always be free to answer:
    // that is why the hook lives here and not on the user interface
    // thread, which can be busy (loading icons, a dialog box...). If a hook
    // takes too long, Windows skips it, and after repeated timeouts it
    // removes it silently.
    g_listener = this;
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, &HotkeyListener::HookProc, GetModuleHandleW(nullptr), 0);
    *installed = hook != nullptr;
    SetEvent(ready); // after this line `installed` and `ready` must not be touched
    if (!hook) {
        g_listener = nullptr;
        return;
    }

    // The thread's message loop: it does nothing but keep the thread
    // available for the hook calls, until Stop() posts WM_QUIT.
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    UnhookWindowsHookEx(hook);
    g_listener = nullptr;
}

LRESULT CALLBACK HotkeyListener::HookProc(int code, WPARAM wParam, LPARAM lParam)
{
    // code < 0 means "do not process, just pass it on" (documented rule).
    if (code == HC_ACTION && g_listener) {
        const auto& key = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (g_listener->OnKey(wParam, key))
            return 1; // non-zero: the key is consumed, no program sees it
    }
    // Let the next hook in the chain, and finally the programs, see the key.
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool HotkeyListener::OnKey(WPARAM message, const KBDLLHOOKSTRUCT& key)
{
    // Our own neutral key passes untouched. Other injected input (from
    // AutoHotkey, remote desktop, on-screen keyboards) is treated like real
    // typing, so the hotkey works with those too.
    if (key.vkCode == kNeutralKey)
        return false;
    if (key.vkCode != hotkey_.key)
        return false;

    const bool keyDown = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    if (!keyDown) {
        // Swallow the release of a key whose press was swallowed, so the
        // active program does not get a stray key-up.
        if (comboDown_) {
            comboDown_ = false;
            return true;
        }
        return false;
    }

    // The modifiers must match exactly: Win+Shift+Z is not Win+Z.
    const bool win = IsDown(VK_LWIN) || IsDown(VK_RWIN);
    const bool ctrl = IsDown(VK_CONTROL);
    const bool alt = IsDown(VK_MENU);
    const bool shift = IsDown(VK_SHIFT);
    if (win != hotkey_.win || ctrl != hotkey_.ctrl || alt != hotkey_.alt || shift != hotkey_.shift)
        return false;

    // Holding the keys produces repeated key-downs; only the first counts.
    if (!comboDown_) {
        comboDown_ = true;
        SendNeutralKey();
        // Post, never send: PostMessageW returns immediately, so the hook
        // answers Windows right away and the menu opens on the UI thread.
        PostMessageW(target_, message_, 0, 0);
    }
    return true;
}

} // namespace popa
