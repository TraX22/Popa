// User interface texts in English and Spanish.
//
// Every visible text has an identifier (TextId) and one row in a table with
// both translations side by side, so adding a text means writing both
// languages in the same place. The language follows the Windows display
// language: Spanish for any Spanish variant, English for everything else.
#pragma once

#include "PopaCore/Model.h"

#include <initializer_list>
#include <string>
#include <string_view>

namespace popa {

enum class Language { English, Spanish };

enum class TextId {
    AppName,

    // Tray icon menu
    TrayOpenEditor,
    TrayOptions,
    TrayCheckUpdates,
    TrayEnableHotkeys,
    TrayDisableHotkeys,
    TrayExit,

    // Startup problems with menu.json
    MenuCorruptTitle,
    MenuCorruptBody,      // {0} = path of the backup copy
    MenuUnreadableTitle,
    MenuUnreadableBody,   // {0} = path of menu.json

    // Popup menu and launching
    MenuEmpty,
    LaunchFailedBody,     // {0} = entry name, {1} = target, {2} = reason

    // Hotkey
    HotkeyFailedTitle,
    HotkeyFailedBody,     // {0} = hotkey text

    // Editor
    EditorTitle,
    EditorAddItem,
    EditorAddMenu,
    EditorAddSeparator,
    EditorMoveUp,
    EditorMoveDown,
    EditorDelete,
    EditorImport,
    EditorName,
    EditorCommand,
    EditorBrowse,
    EditorIcon,
    EditorChangeIcon,
    EditorResetIcon,
    EditorTest,
    EditorNewItem,
    EditorNewMenu,
    EditorNoName,
    EditorDeleteConfirm,  // {0} = submenu name, {1} = number of entries inside
    EditorTargetMissing,
    EditorSaveFailedTitle,
    EditorSaveFailedBody, // {0} = path of menu.json

    // Import a configuration
    ImportFilter,
    ImportFailed,         // {0} = file path
    ImportQuestion,
    ImportReplace,
    ImportAppend,
    ImportDone,           // {0} = number of entries

    // Options
    SettingsTitle,
    SettingsHotkey,
    SettingsHotkeyInvalid,
    SettingsIconSize,
    SettingsPixels,       // {0} = number
    SettingsTheme,
    SettingsLanguage,
    LanguageSystem,
    ThemeSystem,
    ThemeLight,
    ThemeDark,
    SettingsRunAtStartup,
    SettingsCheckUpdates,
    StartupFailedTitle,
    StartupFailedBody,

    // Updates
    UpdatesTitle,
    UpdateAvailableTitle, // {0} = new version
    UpdateAvailableBody,  // {0} = current version
    UpdateShowNotes,
    UpdateHideNotes,
    UpdateNow,
    UpdateLater,
    UpdateUpToDate,       // {0} = current version
    UpdateNoReleases,
    UpdateCheckFailed,
    UpdateDownloading,    // {0} = new version
    UpdateDownloadFailed, // {0} = reason
    UpdateErrorConnection,
    UpdateErrorAddress,
    UpdateErrorHttp,      // {0} = HTTP status code
    UpdateErrorInvalid,
    UpdateErrorHash,
    UpdateErrorWrite,     // {0} = file path
    UpdateInstallFailed,  // {0} = reason
    UpdateRestartFailed,

    // File dialogs
    ProgramsFilter,
    IconFilesFilter,
    AllFilesFilter,

    // Icon picker
    IconPickerTitle,
    IconPickerFile,
    IconPickerNoIcons,

    // Common buttons
    CommonOk,
    CommonCancel,

    // Temporary: features that arrive in later stages
    NotYetAvailable,

    Count // number of texts; must stay last
};

// Picks the language for a Windows language identifier (LANGID), such as
// the one returned by GetUserDefaultUILanguage().
Language LanguageFromLangId(unsigned short langId);

// The language to use for the "language" option: a fixed one, or the
// Windows display language (`systemLangId`) for LanguageSetting::System.
Language ResolveLanguage(LanguageSetting setting, unsigned short systemLangId);

void SetLanguage(Language language);
Language CurrentLanguage();

// Text in the current language.
const wchar_t* Tr(TextId id);

// Text in a given language (used by the tests).
const wchar_t* Tr(TextId id, Language language);

// Replaces {0}, {1}, ... in the pattern with the given arguments.
std::wstring Format(std::wstring_view pattern, std::initializer_list<std::wstring_view> arguments);

} // namespace popa
