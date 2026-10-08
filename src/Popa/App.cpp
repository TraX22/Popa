#include "Popa/App.h"

#include "Popa/Launcher.h"
#include "Popa/SettingsDialog.h"
#include "Popa/Startup.h"
#include "Popa/Theme.h"
#include "Popa/resource.h"
#include "PopaCore/HotkeyText.h"
#include "PopaCore/Repository.h"
#include "PopaCore/Localization.h"

#include <shellapi.h>
#include <shlobj.h>
#include <windowsx.h>

#include <filesystem>
#include <string>

namespace popa {

namespace {

// Timer that delays the single-click action, see OnTrayEvent.
constexpr UINT_PTR kClickTimer = 1;
// Timer for the automatic update check, a few seconds after startup so it
// does not compete with signing in to Windows.
constexpr UINT_PTR kUpdateTimer = 2;
constexpr UINT kUpdateDelayMs = 5000;

// Full path of the running Popa.exe. GetModuleFileNameW truncates silently
// when the buffer is too small, so the buffer grows until the path fits.
std::filesystem::path ExecutablePath()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
            return {};
        if (length < buffer.size()) {
            buffer.resize(length);
            return buffer;
        }
        buffer.resize(buffer.size() * 2);
    }
}

// %APPDATA%\Popa, the fallback configuration folder. SHGetKnownFolderPath
// allocates the string with the COM allocator, so it is freed with
// CoTaskMemFree.
std::filesystem::path AppDataFolder()
{
    PWSTR path = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
        result = std::filesystem::path(path) / L"Popa";
    CoTaskMemFree(path);
    return result;
}

} // namespace

bool App::Initialize(HINSTANCE instance)
{
    instance_ = instance;
    LoadConfiguration();
    UpdateTheme();

    // Explorer broadcasts "TaskbarCreated" when it starts, for example after
    // a crash or a restart from Task Manager. Every program with a tray icon
    // must add it again then. RegisterWindowMessageW turns the agreed name
    // into a message number that is the same in every process.
    taskbarCreatedMessage_ = RegisterWindowMessageW(L"TaskbarCreated");

    // A window class is a template for windows: mainly, which function
    // (the "window procedure") handles their messages.
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &App::WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kMainWindowClass;
    if (!RegisterClassExW(&windowClass))
        return false;

    // The window is never shown: it exists only to receive messages.
    // It is a normal top-level window and not a "message-only" window
    // (HWND_MESSAGE parent) because message-only windows do not receive
    // broadcasts, and TaskbarCreated is a broadcast.
    // `this` travels in the last parameter so WindowProc can find the App.
    window_ = CreateWindowExW(0, kMainWindowClass, Tr(TextId::AppName), WS_OVERLAPPED, 0, 0, 0, 0,
                              nullptr, nullptr, instance, this);
    if (!window_)
        return false;

    tray_.Add(window_, kMsgTrayEvent, instance_, IDI_POPA);
    ReportMenuLoadProblem(pendingMenuProblem_);
    ApplyHotkeySettings();
    ApplyStartupSetting(); // also fixes the entry when Popa.exe was moved
    DeleteOldExecutable(exePath_); // Popa.old.exe left by an update
    if (settings_.checkUpdatesAtStartup)
        SetTimer(window_, kUpdateTimer, kUpdateDelayMs, nullptr);
    return true;
}

int App::Run()
{
    // The message loop. Windows puts input and other events for the
    // windows of this thread in a queue. GetMessageW waits (without using
    // CPU) until a message arrives, TranslateMessage turns key presses into
    // character messages, and DispatchMessageW calls the window procedure
    // of the target window. GetMessageW returns 0 for WM_QUIT, which ends
    // the loop, and -1 on error.
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        // The editor needs to see keyboard messages first (Tab key).
        if (editor_ && editor_->PreTranslateMessage(message))
            continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK App::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Windows calls this static function, which knows nothing about our
    // App object. The usual trick: CreateWindowExW passed `this`, which
    // arrives in WM_NCCREATE (the first message a window gets) inside
    // CREATESTRUCT. It is stored in the window's "user data" slot and read
    // back for every later message.
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* app = static_cast<App*>(create->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }

    auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (app)
        return app->HandleMessage(message, wParam, lParam);
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT App::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    // TaskbarCreated has no fixed number, so it cannot be a switch case.
    if (message == taskbarCreatedMessage_ && message != 0) {
        tray_.Recreate();
        return 0;
    }

    // While the popup menu is open, Windows sends its owner-draw messages
    // to this window; the PopupMenu object handles them.
    if (LRESULT result = 0; popup_.HandleMessage(message, wParam, lParam, result))
        return result;

    switch (message) {
    case kMsgTrayEvent:
        OnTrayEvent(wParam, lParam);
        return 0;

    case kMsgHotkey:
        ShowPopupMenu();
        return 0;

    case kMsgEditorClosed:
        // Destroyed here and not inside the editor's own callback: an
        // object must not delete itself while one of its functions runs.
        editor_.reset();
        return 0;

    case kMsgOpenEditor:
        OpenEditor();
        return 0;

    case WM_TIMER:
        if (wParam == kClickTimer) {
            KillTimer(window_, kClickTimer);
            ShowPopupMenu();
        } else if (wParam == kUpdateTimer) {
            KillTimer(window_, kUpdateTimer);
            StartCheck(false);
        }
        return 0;

    case kMsgUpdateChecked:
        OnUpdateChecked(TakeResult<CheckResult>(lParam));
        return 0;

    case kMsgUpdateDownloaded:
        OnUpdateDownloaded(TakeResult<DownloadResult>(lParam));
        return 0;

    case WM_SETTINGCHANGE:
        if (IsThemeChangeMessage(lParam))
            UpdateTheme();
        return 0;

    case WM_DESTROY:
        if (editor_) {
            editor_->FlushChanges();
            editor_.reset();
        }
        hotkey_.Stop();
        tray_.Remove();
        PostQuitMessage(0); // puts WM_QUIT in the queue: Run() returns
        return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void App::LoadConfiguration()
{
    const std::filesystem::path exe = ExecutablePath();
    exePath_ = exe.wstring();
    const std::filesystem::path exeFolder = exe.parent_path();
    store_ = std::make_unique<ConfigStore>(ConfigStore::ChooseDirectory(exeFolder, AppDataFolder()));
    exeFolder_ = exeFolder.wstring();
    icons_ = std::make_unique<IconCache>(exeFolder_);

    settings_ = store_->LoadSettings();
    ApplyLanguage(); // before any window or text is created

    MenuLoadResult result = store_->LoadMenuItems();
    menu_ = std::move(result.items);
    canSaveMenu_ = result.status != LoadStatus::Unreadable;
    pendingMenuProblem_ = std::move(result);
}

void App::ReportMenuLoadProblem(const MenuLoadResult& result)
{
    // A notification from the tray instead of a dialog box: the program
    // keeps starting, and the user reads it when convenient.
    if (result.status == LoadStatus::Corrupt) {
        const std::wstring backup = result.backupPath.empty() ? std::wstring(ConfigStore::kBackupSuffix)
                                                              : result.backupPath.wstring();
        tray_.ShowNotification(Tr(TextId::MenuCorruptTitle),
                               Format(Tr(TextId::MenuCorruptBody), {backup}).c_str(), true);
    } else if (result.status == LoadStatus::Unreadable) {
        tray_.ShowNotification(Tr(TextId::MenuUnreadableTitle),
                               Format(Tr(TextId::MenuUnreadableBody), {store_->MenuPath().wstring()}).c_str(),
                               true);
    }
}

void App::UpdateTheme()
{
    dark_ = IsDarkTheme(settings_.theme);
    // Also affects the tray icon's menu, which Windows draws by itself.
    ApplyMenuTheme(dark_);
    if (editor_)
        editor_->SetDarkTitleBar(dark_);
}

void App::ApplyLanguage()
{
    SetLanguage(ResolveLanguage(settings_.language, GetUserDefaultUILanguage()));
    // Menus and dialogs read their texts when they open; the editor is the
    // only window that stays open, so it is told to refresh its texts.
    if (editor_)
        editor_->ApplyLanguage();
}

void App::ShowPopupMenu()
{
    // The menu opens where the mouse pointer is.
    POINT cursor{};
    GetCursorPos(&cursor);
    if (const MenuItem* chosen = popup_.Show(window_, menu_, cursor, settings_.iconSize, dark_, *icons_))
        Launch(*chosen, window_);
    if (pendingMenu_) {
        ApplyMenu(std::move(*pendingMenu_));
        pendingMenu_.reset();
    }
}

void App::OpenEditor()
{
    if (editor_) {
        editor_->Activate();
        return;
    }
    editor_ = std::make_unique<EditorWindow>(
        *icons_, exeFolder_, [this](std::vector<MenuItem> items) { OnMenuEdited(std::move(items)); },
        [this] { PostMessageW(window_, kMsgEditorClosed, 0, 0); });
    if (!editor_->Create(menu_, dark_))
        editor_.reset();
}

void App::OnMenuEdited(std::vector<MenuItem> items)
{
    // The editor saves while the user works; the popup menu may be open at
    // that moment (opened with the hotkey) and holds pointers into menu_.
    if (popup_.IsOpen())
        pendingMenu_ = std::move(items);
    else
        ApplyMenu(std::move(items));
}

void App::ApplyMenu(std::vector<MenuItem> items)
{
    menu_ = std::move(items);
    if (!canSaveMenu_)
        return; // menu.json was locked at startup; do not overwrite it
    if (store_->SaveMenuItems(menu_)) {
        saveFailureShown_ = false;
    } else if (!saveFailureShown_) {
        saveFailureShown_ = true;
        tray_.ShowNotification(Tr(TextId::EditorSaveFailedTitle),
                               Format(Tr(TextId::EditorSaveFailedBody), {store_->MenuPath().wstring()}).c_str(), true);
    }
}

void App::SaveSettings()
{
    store_->SaveSettings(settings_);
}

void App::OnTrayEvent(WPARAM wParam, LPARAM lParam)
{
    // With NOTIFYICON_VERSION_4 the event is in the low word of lParam and
    // the anchor point (in screen coordinates) is packed in wParam.
    const UINT event = LOWORD(lParam);
    const POINT anchor{GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam)};

    switch (event) {
    case WM_LBUTTONUP:
        // A single click opens the popup menu and a double-click opens the
        // editor. Both begin with the same button-up, so the click action
        // waits for the double-click time (500 ms by default) in a timer.
        // If a double-click arrives first, the timer is cancelled.
        if (ignoreNextButtonUp_) {
            ignoreNextButtonUp_ = false; // the button-up that ends a double-click
            break;
        }
        SetTimer(window_, kClickTimer, GetDoubleClickTime(), nullptr);
        break;

    case WM_LBUTTONDBLCLK:
        KillTimer(window_, kClickTimer);
        ignoreNextButtonUp_ = true;
        OpenEditor();
        break;

    case NIN_KEYSELECT: // Enter or Space with the icon focused (Win+B, arrows)
        ShowPopupMenu();
        break;

    case WM_CONTEXTMENU: // right click, or Shift+F10 / menu key
        ShowTrayMenu(anchor);
        break;
    }
}

void App::ShowTrayMenu(POINT anchor)
{
    switch (tray_.ShowContextMenu(anchor, settings_.hotkeysEnabled)) {
    case kTrayOpenEditor: OpenEditor(); break;
    case kTrayOptions: OpenSettings(); break;
    case kTrayCheckUpdates: CheckForUpdates(); break;
    case kTrayToggleHotkeys: ToggleHotkeys(); break;
    case kTrayExit: DestroyWindow(window_); break;
    }
}

void App::ToggleHotkeys()
{
    settings_.hotkeysEnabled = !settings_.hotkeysEnabled;
    SaveSettings();
    ApplyHotkeySettings();
}

void App::ApplyHotkeySettings()
{
    hotkey_.Stop();
    if (!settings_.hotkeysEnabled)
        return;

    // A hotkey text that cannot be used (edited by hand, for example)
    // falls back to the default instead of leaving Popa without a hotkey.
    const Hotkey hotkey = ParseHotkey(settings_.hotkey).value_or(*ParseHotkey(Settings{}.hotkey));
    if (!hotkey_.Start(hotkey, window_, kMsgHotkey)) {
        tray_.ShowNotification(Tr(TextId::HotkeyFailedTitle),
                               Format(Tr(TextId::HotkeyFailedBody), {FormatHotkey(hotkey)}).c_str(), true);
    }
}

// --- Features that arrive in later stages --------------------------------------

void App::OpenSettings()
{
    // A second "Options" click while the dialog is open just shows it.
    if (ActivateSettingsDialog())
        return;
    const auto result = ShowSettingsDialog(window_, settings_, dark_);
    if (!result)
        return;

    settings_ = *result;
    SaveSettings();
    ApplyLanguage();
    ApplyHotkeySettings();
    UpdateTheme();
    ApplyStartupSetting();
}

void App::ApplyStartupSetting()
{
    if (!ApplyRunAtStartup(settings_.runAtStartup, exePath_))
        tray_.ShowNotification(Tr(TextId::StartupFailedTitle), Tr(TextId::StartupFailedBody), true);
}

void App::CheckForUpdates()
{
    if (downloadingUpdate_) {
        tray_.ShowNotification(Tr(TextId::UpdatesTitle),
                               Format(Tr(TextId::UpdateDownloading), {pendingRelease_.version.ToString()}).c_str(),
                               false);
        return;
    }
    StartCheck(true);
}

// --- Updates ------------------------------------------------------------------------------------

void App::StartCheck(bool manual)
{
    if (checkingUpdates_ || downloadingUpdate_)
        return;
    checkingUpdates_ = true;
    StartUpdateCheck(window_, kMsgUpdateChecked, manual);
}

void App::ShowUpdateMessage(const std::wstring& text, bool warning)
{
    MessageBoxW(window_, text.c_str(), Tr(TextId::UpdatesTitle),
                MB_OK | MB_SETFOREGROUND | (warning ? MB_ICONWARNING : MB_ICONINFORMATION));
}

void App::OnUpdateChecked(std::unique_ptr<CheckResult> result)
{
    checkingUpdates_ = false;
    if (!result)
        return;
    switch (result->outcome) {
    case CheckOutcome::NewVersion:
        PromptForUpdate(result->release);
        break;
    // The automatic check stays silent unless there is something new
    // (section 5.11): no message without internet, nothing when up to date.
    case CheckOutcome::UpToDate:
        if (result->manual)
            ShowUpdateMessage(Format(Tr(TextId::UpdateUpToDate), {CurrentVersion().ToString()}), false);
        break;
    case CheckOutcome::NoReleases:
        if (result->manual)
            ShowUpdateMessage(Tr(TextId::UpdateNoReleases), false);
        break;
    case CheckOutcome::Failed:
        if (result->manual)
            ShowUpdateMessage(Tr(TextId::UpdateCheckFailed), true);
        break;
    }
}

void App::PromptForUpdate(const ReleaseInfo& release)
{
    // Release notes can be long; the dialog shows the beginning.
    std::wstring notes = release.notes;
    if (notes.size() > 1500)
        notes = notes.substr(0, 1500) + L"…";
    const std::wstring title = Format(Tr(TextId::UpdateAvailableTitle), {release.version.ToString()});
    const std::wstring body = Format(Tr(TextId::UpdateAvailableBody), {CurrentVersion().ToString()});

    // A TaskDialog with the notes in its collapsible "expanded" area.
    const TASKDIALOG_BUTTON buttons[] = {{IDOK, Tr(TextId::UpdateNow)}, {IDCANCEL, Tr(TextId::UpdateLater)}};
    TASKDIALOGCONFIG config{};
    config.cbSize = sizeof(config);
    config.hwndParent = window_;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.pszWindowTitle = Tr(TextId::UpdatesTitle);
    config.pszMainIcon = TD_INFORMATION_ICON;
    config.pszMainInstruction = title.c_str();
    config.pszContent = body.c_str();
    if (!notes.empty()) {
        config.pszExpandedInformation = notes.c_str();
        config.pszExpandedControlText = Tr(TextId::UpdateHideNotes);
        config.pszCollapsedControlText = Tr(TextId::UpdateShowNotes);
    }
    config.cButtons = ARRAYSIZE(buttons);
    config.pButtons = buttons;
    config.nDefaultButton = IDOK;

    SetForegroundWindow(window_);
    int chosen = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&config, &chosen, nullptr, nullptr)) || chosen != IDOK)
        return; // "Not now": asked again at the next start

    // A release without Popa.exe attached: open its page instead.
    if (release.downloadUrl.empty()) {
        const std::wstring page = release.pageUrl.empty() ? RepositoryUrl() : release.pageUrl;
        ShellExecuteW(nullptr, L"open", page.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    downloadingUpdate_ = true;
    pendingRelease_ = release;
    tray_.ShowNotification(Tr(TextId::UpdatesTitle),
                           Format(Tr(TextId::UpdateDownloading), {release.version.ToString()}).c_str(), false);
    StartDownload(window_, kMsgUpdateDownloaded, release, exePath_);
}

void App::OnUpdateDownloaded(std::unique_ptr<DownloadResult> result)
{
    downloadingUpdate_ = false;
    if (!result || !result->ok) {
        ShowUpdateMessage(Format(Tr(TextId::UpdateDownloadFailed), {result ? result->error : std::wstring()}), true);
        return;
    }

    std::wstring error;
    if (!InstallUpdate(result->filePath, exePath_, error)) {
        ShowUpdateMessage(Format(Tr(TextId::UpdateInstallFailed), {error}), true);
        return;
    }
    // menu.json and settings.json are separate files: an update never
    // touches them. Pending editor changes are saved when the window is
    // destroyed; the new Popa waits for this process to end.
    if (!RestartAfterUpdate(exePath_))
        ShowUpdateMessage(Tr(TextId::UpdateRestartFailed), false);
    DestroyWindow(window_);
}

void App::ShowNotYetAvailable(const wchar_t* feature)
{
    // MB_SETFOREGROUND: the owner window is invisible, so without it the
    // box could open behind other windows.
    MessageBoxW(window_, Tr(TextId::NotYetAvailable), feature, MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
}

} // namespace popa
