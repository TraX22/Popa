#include "Popa/Launcher.h"

#include "Popa/UiUtil.h"
#include "PopaCore/CommandLine.h"
#include "PopaCore/Localization.h"

#include <shellapi.h>

#include <string>

namespace popa {

bool TargetExists(const std::wstring& command)
{
    const std::wstring target = CompleteExtension(ParseCommand(command).target);
    if (target.empty())
        return false;
    if (IsAbsolutePath(target))
        return ProbeFileSystem(target) != PathKind::None;
    // "scheme:..." (two or more letters before the colon, so not "C:").
    const size_t colon = target.find(L':');
    if (colon != std::wstring::npos && colon >= 2)
        return true;
    // A bare name: look in the folders of the PATH variable, as Windows does.
    wchar_t found[MAX_PATH] = {};
    if (SearchPathW(nullptr, target.c_str(), L".exe", MAX_PATH, found, nullptr) != 0)
        return true;
    // ShellExecuteExW also finds programs registered under "App Paths"
    // (chrome.exe, winword.exe...), which need not be in the PATH.
    const std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + target;
    for (HKEY root : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
        if (RegGetValueW(root, key.c_str(), nullptr, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, nullptr,
                         nullptr) == ERROR_SUCCESS)
            return true;
    }
    return false;
}

bool Launch(const MenuItem& item, HWND owner)
{
    const SplitCommand command = ParseCommand(item.command);
    const std::wstring target = CompleteExtension(command.target);
    const std::wstring folder = WorkingDirectoryFor(target);

    // ShellExecuteExW is "what a double-click in Explorer does": it runs
    // programs, opens documents with their app, folders in Explorer, .lnk
    // shortcuts and web addresses in the browser.
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    // FLAG_NO_UI: no system error box; Popa shows its own, shorter one.
    // NOASYNC: finish the launch before returning (no background thread
    // that could outlive a quick exit of Popa).
    info.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    info.hwnd = owner;
    info.lpFile = target.c_str();
    info.lpParameters = command.arguments.empty() ? nullptr : command.arguments.c_str();
    info.lpDirectory = folder.empty() ? nullptr : folder.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!target.empty() && ShellExecuteExW(&info))
        return true;

    const DWORD error = target.empty() ? ERROR_FILE_NOT_FOUND : GetLastError();
    // The user cancelled something on purpose, such as the UAC prompt of a
    // program that needs administrator rights: not an error to report.
    if (error == ERROR_CANCELLED)
        return false;

    const std::wstring message = Format(Tr(TextId::LaunchFailedBody), {item.name, target, SystemErrorText(error)});
    MessageBoxW(owner, message.c_str(), Tr(TextId::AppName), MB_OK | MB_ICONWARNING | MB_SETFOREGROUND);
    return false;
}

} // namespace popa
