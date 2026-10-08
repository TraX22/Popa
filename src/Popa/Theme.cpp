#include "Popa/Theme.h"

#include <cwchar>

namespace popa {

bool SystemUsesDarkTheme()
{
    // Windows stores the app mode in the registry. 1 (or missing) = light,
    // 0 = dark. RegGetValueW opens the key, reads and closes in one call.
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER,
                                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    return status == ERROR_SUCCESS && value == 0;
}

bool IsDarkTheme(Theme setting)
{
    switch (setting) {
    case Theme::Light: return false;
    case Theme::Dark: return true;
    case Theme::System:
    default: return SystemUsesDarkTheme();
    }
}

MenuColors MenuColorsFor(bool dark)
{
    // Close to the colors of Windows 11's own menus, so Popa's rows blend
    // with the border and shadow the system draws around them.
    if (dark)
        return {RGB(44, 44, 44), RGB(255, 255, 255), RGB(120, 120, 120), RGB(61, 61, 61), RGB(70, 70, 70)};
    return {RGB(249, 249, 249), RGB(26, 26, 26), RGB(160, 160, 160), RGB(234, 234, 234), RGB(224, 224, 224)};
}

namespace {

// The undocumented function, as reverse-engineered by the community and
// used by many programs (Notepad++, Explorer Patcher, ...). Windows itself
// uses it to make Explorer's menus dark.
enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight };
using SetPreferredAppModeFn = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFn = void(WINAPI*)();

// Windows version without the compatibility "lies" of GetVersionExW:
// RtlGetVersion in ntdll.dll always reports the real build number.
DWORD WindowsBuildNumber()
{
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    const auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
    OSVERSIONINFOW info{};
    info.dwOSVersionInfoSize = sizeof(info);
    if (!rtlGetVersion || rtlGetVersion(&info) != 0)
        return 0;
    return info.dwBuildNumber;
}

} // namespace

void ApplyMenuTheme(bool dark)
{
    // The functions have no name in the DLL's export table, only a number
    // ("ordinal"): 135 is SetPreferredAppMode and 136 FlushMenuThemes.
    // Before Windows 10 1903 (build 18362) ordinal 135 was a different
    // function with another signature, so older builds are skipped: calling
    // it with the wrong signature could crash.
    static const DWORD build = WindowsBuildNumber();
    if (build < 18362)
        return;

    HMODULE uxtheme = GetModuleHandleW(L"uxtheme.dll");
    if (!uxtheme)
        uxtheme = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!uxtheme)
        return;

    // GetProcAddress returns a generic function pointer; the cast through
    // void* tells the compiler the type change is intentional.
    const auto setPreferredAppMode = reinterpret_cast<SetPreferredAppModeFn>(
        reinterpret_cast<void*>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135))));
    const auto flushMenuThemes = reinterpret_cast<FlushMenuThemesFn>(
        reinterpret_cast<void*>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136))));
    if (!setPreferredAppMode)
        return;

    setPreferredAppMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
    if (flushMenuThemes)
        flushMenuThemes(); // apply to menus already themed in this process
}

bool IsThemeChangeMessage(LPARAM lParam)
{
    // Windows sends WM_SETTINGCHANGE with the text "ImmersiveColorSet" in
    // lParam when the light/dark mode or the accent color changes.
    const auto* area = reinterpret_cast<const wchar_t*>(lParam);
    return area && std::wcscmp(area, L"ImmersiveColorSet") == 0;
}

} // namespace popa
