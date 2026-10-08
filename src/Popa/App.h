// The application object: owns the hidden window that receives messages
// from the tray icon (and later from the hotkey and the updater), the loaded
// configuration and the tray icon itself.
#pragma once

#include "Popa/EditorWindow.h"
#include "Popa/Hotkey.h"
#include "Popa/IconCache.h"
#include "Popa/PopupMenu.h"
#include "Popa/Tray.h"
#include "Popa/Updater.h"
#include "PopaCore/ConfigStore.h"
#include "PopaCore/Model.h"

#include <windows.h>

#include <memory>
#include <optional>
#include <vector>

namespace popa {

// Window class of the hidden window. A second instance of Popa finds the
// first one by this name (FindWindowW).
inline constexpr wchar_t kMainWindowClass[] = L"Popa.MainWindow";

// Private messages. WM_APP..0xBFFF is the range Windows reserves for
// application-defined messages inside one program.
inline constexpr UINT kMsgTrayEvent = WM_APP + 1;  // from the tray icon
inline constexpr UINT kMsgOpenEditor = WM_APP + 2; // from a second instance
inline constexpr UINT kMsgHotkey = WM_APP + 3;     // from the keyboard hook thread
inline constexpr UINT kMsgEditorClosed = WM_APP + 4; // the editor window was destroyed
inline constexpr UINT kMsgUpdateChecked = WM_APP + 5;    // update check finished (CheckResult*)
inline constexpr UINT kMsgUpdateDownloaded = WM_APP + 6; // download finished (DownloadResult*)

class App {
public:
    App() = default;
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Loads the configuration, creates the hidden window and the tray icon.
    bool Initialize(HINSTANCE instance);

    // Runs the message loop until the program exits; returns the exit code.
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void LoadConfiguration();
    void UpdateTheme();
    void ApplyLanguage();
    void ReportMenuLoadProblem(const MenuLoadResult& result);
    void SaveSettings();

    void OnTrayEvent(WPARAM wParam, LPARAM lParam);
    void ShowTrayMenu(POINT anchor);

    void ShowPopupMenu();
    void OpenEditor();
    void OpenSettings();
    void CheckForUpdates();
    void ToggleHotkeys();
    void ApplyHotkeySettings();
    void ApplyStartupSetting();
    void StartCheck(bool manual);
    void OnUpdateChecked(std::unique_ptr<CheckResult> result);
    void PromptForUpdate(const ReleaseInfo& release);
    void OnUpdateDownloaded(std::unique_ptr<DownloadResult> result);
    void ShowUpdateMessage(const std::wstring& text, bool warning);
    void ShowNotYetAvailable(const wchar_t* feature);
    void OnMenuEdited(std::vector<MenuItem> items);
    void ApplyMenu(std::vector<MenuItem> items);

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    UINT taskbarCreatedMessage_ = 0;
    TrayIcon tray_;
    PopupMenu popup_;
    HotkeyListener hotkey_;
    std::unique_ptr<IconCache> icons_;
    std::unique_ptr<EditorWindow> editor_;
    std::wstring exeFolder_;
    std::wstring exePath_;
    bool dark_ = false;

    std::unique_ptr<ConfigStore> store_;
    Settings settings_;
    std::vector<MenuItem> menu_;
    MenuLoadResult pendingMenuProblem_; // shown once the tray icon exists
    bool canSaveMenu_ = true;           // false when menu.json was unreadable
    bool saveFailureShown_ = false;
    bool checkingUpdates_ = false;
    bool downloadingUpdate_ = false;
    ReleaseInfo pendingRelease_; // being downloaded
    // Changes from the editor that arrived while the popup menu was open;
    // applied when it closes (the open menu points into menu_).
    std::optional<std::vector<MenuItem>> pendingMenu_;

    // Telling a click from a double-click on the tray icon, see OnTrayEvent.
    bool ignoreNextButtonUp_ = false;
};

} // namespace popa
