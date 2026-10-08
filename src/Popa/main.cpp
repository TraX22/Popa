// Entry point of Popa: single-instance check, then the App object runs the
// message loop until the user picks "Exit".

#include "Popa/App.h"
#include "Popa/Updater.h"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#include <cwchar>

namespace {

// Name of the mutex that marks a running Popa. "Local\" scopes it to the
// current Windows session, so two users logged in at the same time (fast
// user switching) can each run their own Popa.
constexpr wchar_t kInstanceMutexName[] = L"Local\\Popa";

// Asks the already running Popa to open its editor. Its hidden window may
// not exist yet if it is still starting, so the search is retried briefly.
void SignalFirstInstance()
{
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (HWND existing = FindWindowW(popa::kMainWindowClass, nullptr)) {
            // Windows only lets the program the user is interacting with
            // bring a window to the front. This process has that right
            // (the user just launched it), so it hands it to the first one;
            // otherwise the editor would open behind other windows.
            DWORD processId = 0;
            GetWindowThreadProcessId(existing, &processId);
            AllowSetForegroundWindow(processId);
            PostMessageW(existing, popa::kMsgOpenEditor, 0, 0);
            return;
        }
        Sleep(100);
    }
}

} // namespace

// wWinMain is the entry point for a Windows-subsystem program that uses
// wide (UTF-16) strings. The C runtime calls it after initializing itself.
int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE /*unused*/,
                    _In_ PWSTR /*commandLine*/, _In_ int /*showCommand*/)
{
    // Started by an update ("--after-update=<pid>"): wait until the old
    // Popa has exited, so its mutex and tray icon are gone and its
    // Popa.old.exe can be deleted.
    if (const wchar_t* afterUpdate = std::wcsstr(GetCommandLineW(), popa::kAfterUpdateSwitch)) {
        const DWORD oldProcessId = std::wcstoul(afterUpdate + std::wcslen(popa::kAfterUpdateSwitch), nullptr, 10);
        if (HANDLE oldProcess = OpenProcess(SYNCHRONIZE, FALSE, oldProcessId)) {
            WaitForSingleObject(oldProcess, 15000);
            CloseHandle(oldProcess);
        }
    }

    // Single instance. CreateMutexW creates the named mutex or, if another
    // process already created it, opens the existing one and reports
    // ERROR_ALREADY_EXISTS. The mutex is never locked; it only serves as a
    // system-wide flag that lives as long as the first Popa keeps its handle.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kInstanceMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        SignalFirstInstance();
        CloseHandle(mutex);
        return 0;
    }

    // COM is Windows' component system. Shell functions used later
    // (ShellExecuteExW, IShellLinkW for .lnk files, file dialogs) need it
    // initialized on the thread. "Apartment threaded" (STA) is the model
    // required by anything that has windows.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    // Registers the classes of the common controls (version 6, selected by
    // the manifest) used later by the editor: tree view, toolbar, etc.
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);

    int exitCode = 1;
    {
        // The App lives inside this block so it is destroyed (tray icon
        // removed, resources freed) before COM is shut down.
        popa::App app;
        if (app.Initialize(instance))
            exitCode = app.Run();
    }

    if (SUCCEEDED(comResult))
        CoUninitialize();
    if (mutex)
        CloseHandle(mutex);
    return exitCode;
}
