#include "Popa/Tray.h"

#include "PopaCore/Localization.h"

#include <commctrl.h>
#include <shellapi.h>

namespace popa {

namespace {

// Several icons of one program are told apart by this number. Popa has one.
constexpr UINT kIconUid = 1;

// NOTIFYICONDATAW is the single structure behind every Shell_NotifyIconW
// call. cbSize tells the shell which version of the structure we compiled
// against; hWnd + uID identify our icon.
NOTIFYICONDATAW MakeData(HWND window)
{
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = kIconUid;
    return data;
}

} // namespace

TrayIcon::~TrayIcon()
{
    Remove();
    if (icon_)
        DestroyIcon(icon_);
}

void TrayIcon::ReloadIcon()
{
    if (icon_)
        DestroyIcon(icon_);
    icon_ = nullptr;
    // LoadIconMetric picks the image of the right size for the current DPI
    // (16 px at 100 %, 24 px at 150 %...) from the multi-size .ico, instead
    // of stretching one size. LIM_SMALL is the size used in the tray.
    if (FAILED(LoadIconMetric(instance_, MAKEINTRESOURCEW(iconId_), LIM_SMALL, &icon_)))
        icon_ = ::LoadIconW(nullptr, IDI_APPLICATION);
}

bool TrayIcon::Add(HWND window, UINT callbackMessage, HINSTANCE instance, int iconId)
{
    window_ = window;
    callbackMessage_ = callbackMessage;
    instance_ = instance;
    iconId_ = iconId;
    ReloadIcon();
    return AddToShell();
}

bool TrayIcon::AddToShell()
{
    NOTIFYICONDATAW data = MakeData(window_);
    // NIF_SHOWTIP: with version 4 (below) the shell only shows the standard
    // tooltip when asked to.
    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    data.uCallbackMessage = callbackMessage_;
    data.hIcon = icon_;
    wcsncpy_s(data.szTip, Tr(TextId::AppName), _TRUNCATE);

    added_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
    if (!added_)
        return false;

    // Version 4 is the modern behavior: keyboard selection, NIN_SELECT,
    // and WM_CONTEXTMENU with the anchor point packed in wParam.
    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    return true;
}

bool TrayIcon::Recreate()
{
    // After an Explorer restart the old icon is gone, but the shell may
    // also have kept nothing of ours, so this is a plain add. The icon is
    // reloaded in case the DPI changed meanwhile.
    added_ = false;
    ReloadIcon();
    return AddToShell();
}

void TrayIcon::Remove()
{
    if (!added_)
        return;
    NOTIFYICONDATAW data = MakeData(window_);
    Shell_NotifyIconW(NIM_DELETE, &data);
    added_ = false;
}

void TrayIcon::ShowNotification(const wchar_t* title, const wchar_t* text, bool warning)
{
    if (!added_)
        return;
    NOTIFYICONDATAW data = MakeData(window_);
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = warning ? NIIF_WARNING : NIIF_INFO;
    wcsncpy_s(data.szInfoTitle, title, _TRUNCATE);
    wcsncpy_s(data.szInfo, text, _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

UINT TrayIcon::ShowContextMenu(POINT anchor, bool hotkeysEnabled)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return 0;

    AppendMenuW(menu, MF_STRING, kTrayOpenEditor, Tr(TextId::TrayOpenEditor));
    AppendMenuW(menu, MF_STRING, kTrayOptions, Tr(TextId::TrayOptions));
    AppendMenuW(menu, MF_STRING, kTrayCheckUpdates, Tr(TextId::TrayCheckUpdates));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayToggleHotkeys,
                Tr(hotkeysEnabled ? TextId::TrayDisableHotkeys : TextId::TrayEnableHotkeys));
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayExit, Tr(TextId::TrayExit));
    // The default item is drawn in bold; by Windows convention it is what
    // a double-click on the icon does.
    SetMenuDefaultItem(menu, kTrayOpenEditor, FALSE);

    // A well-known quirk: a popup menu only closes when the user clicks
    // elsewhere if its owner window is in the foreground. Our window is
    // invisible, so it is brought forward first, and a dummy WM_NULL is
    // posted afterwards so the menu does not reappear on the next click
    // (documented in the TrackPopupMenu remarks).
    SetForegroundWindow(window_);

    // TPM_RETURNCMD: return the chosen command instead of sending WM_COMMAND.
    // The menu is placed by the system so it never goes off screen; the
    // alignment flags follow the user's left/right-handed setting.
    UINT flags = TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON;
    flags |= GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN;
    const UINT command = static_cast<UINT>(
        TrackPopupMenuEx(menu, flags, anchor.x, anchor.y, window_, nullptr));

    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return command;
}

} // namespace popa
