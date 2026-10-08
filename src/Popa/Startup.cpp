#include "Popa/Startup.h"

#include <windows.h>

#include <cwctype>

namespace popa {

namespace {

// Every program listed in this key runs when the user signs in. HKCU
// ("current user") is writable without administrator rights and only
// affects this user, unlike the same key under HKLM.
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Popa";

std::wstring Lower(std::wstring text)
{
    for (wchar_t& c : text)
        c = static_cast<wchar_t>(std::towlower(c));
    return text;
}

// The value written: the path in quotes, because paths with spaces in the
// Run key are otherwise split at the first space.
std::wstring CommandFor(const std::wstring& exePath)
{
    return L"\"" + exePath + L"\"";
}

} // namespace

std::optional<std::wstring> RunAtStartupCommand()
{
    // Two-call pattern again: first the size in bytes, then the data.
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
        return std::nullopt;
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValueName, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) !=
        ERROR_SUCCESS)
        return std::nullopt;
    value.resize(wcslen(value.c_str())); // drop the terminating null characters
    return value;
}

bool ApplyRunAtStartup(bool enabled, const std::wstring& exePath)
{
    const std::wstring command = CommandFor(exePath);
    const auto current = RunAtStartupCommand();

    if (enabled) {
        // Rewritten when it differs, for example after Popa.exe was moved
        // to another folder: the setting follows the program.
        if (current && Lower(*current) == Lower(command))
            return true;
        const auto size = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));
        return RegSetKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName, REG_SZ, command.c_str(), size) ==
               ERROR_SUCCESS;
    }

    if (!current || Lower(*current) != Lower(command))
        return true; // nothing of ours to remove
    const LSTATUS status = RegDeleteKeyValueW(HKEY_CURRENT_USER, kRunKey, kValueName);
    return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
}

} // namespace popa
