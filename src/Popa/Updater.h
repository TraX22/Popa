// Update check and installation, with WinHTTP.
//
// Network work happens on background threads so the program never freezes
// while waiting for GitHub. Each thread reports back by posting a message to
// Popa's window with a heap-allocated result in lParam; the receiver takes
// ownership with TakeResult().
#pragma once

#include "PopaCore/UpdateCheck.h"
#include "PopaCore/Version.h"

#include <windows.h>

#include <memory>
#include <string>

namespace popa {

enum class CheckOutcome {
    NewVersion, // a newer release exists
    UpToDate,
    NoReleases, // the repository has no published release yet
    Failed,     // no connection, GitHub error, unreadable answer
};

struct CheckResult {
    CheckOutcome outcome = CheckOutcome::Failed;
    ReleaseInfo release;
    bool manual = false; // started from the tray menu (report every outcome)
};

struct DownloadResult {
    bool ok = false;
    std::wstring error;    // reason, when !ok
    std::wstring filePath; // the downloaded Popa.new.exe, when ok
};

// The version in Popa.exe's version resource.
Version CurrentVersion();

// Asks GitHub for the latest release on a background thread. When done,
// posts `message` to `window` with a CheckResult* in lParam.
void StartUpdateCheck(HWND window, UINT message, bool manual);

// Downloads the release's Popa.exe next to the running one (as
// Popa.new.exe) on a background thread, verifying address, size, program
// signature and SHA-256. Posts `message` with a DownloadResult* in lParam.
void StartDownload(HWND window, UINT message, const ReleaseInfo& release, const std::wstring& exePath);

// Takes ownership of the result carried by one of the messages above.
template <typename T>
std::unique_ptr<T> TakeResult(LPARAM lParam)
{
    return std::unique_ptr<T>(reinterpret_cast<T*>(lParam));
}

// Puts the downloaded file in place of the running Popa.exe:
//   Popa.exe -> Popa.old.exe, Popa.new.exe -> Popa.exe.
// Windows does not allow deleting or overwriting a running .exe, but it
// does allow renaming it. On failure the original is restored.
bool InstallUpdate(const std::wstring& newFile, const std::wstring& exePath, std::wstring& error);

// Starts the new Popa.exe, which waits for this process to exit.
bool RestartAfterUpdate(const std::wstring& exePath);

// Deletes Popa.old.exe left by a previous update (called at startup).
void DeleteOldExecutable(const std::wstring& exePath);

// Command line switch used by RestartAfterUpdate: "--after-update=<pid>".
inline constexpr wchar_t kAfterUpdateSwitch[] = L"--after-update=";

} // namespace popa
