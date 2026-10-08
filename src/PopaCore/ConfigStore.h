// Reading and writing of menu.json and settings.json.
//
// The store works on a folder given by the caller. The application passes
// the folder chosen by ChooseDirectory (next to the .exe, or %APPDATA%\Popa);
// the tests pass a temporary folder. Keeping Windows-specific lookups (such
// as finding %APPDATA%) out of this class makes it easy to test.
#pragma once

#include "PopaCore/Model.h"

#include <filesystem>
#include <vector>

namespace popa {

enum class LoadStatus {
    Ok,         // file read and parsed
    Missing,    // file does not exist yet (first run)
    Corrupt,    // file exists but is not valid; a .bak copy was made
    Unreadable, // file exists but could not be opened (locked, no permission)
};

struct MenuLoadResult {
    std::vector<MenuItem> items;
    LoadStatus status = LoadStatus::Missing;
    // Where the damaged file was copied, when status is Corrupt and the
    // copy succeeded. Empty otherwise.
    std::filesystem::path backupPath;
};

class ConfigStore {
public:
    static constexpr const wchar_t* kMenuFileName = L"menu.json";
    static constexpr const wchar_t* kSettingsFileName = L"settings.json";
    static constexpr const wchar_t* kBackupSuffix = L".bak";

    explicit ConfigStore(std::filesystem::path directory);

    const std::filesystem::path& Directory() const { return directory_; }
    std::filesystem::path MenuPath() const { return directory_ / kMenuFileName; }
    std::filesystem::path SettingsPath() const { return directory_ / kSettingsFileName; }

    // Note on naming: "LoadMenu" would be a bad name here. <windows.h>
    // defines LoadMenu as a macro (LoadMenuW), so in any file that includes
    // it before this header the method would be silently renamed.
    //
    // Loads menu.json. A damaged file is copied to menu.json.bak and an empty
    // menu is returned, so the program can start and tell the user.
    // With Unreadable the caller should not save, to avoid overwriting a
    // file that may be perfectly fine but temporarily locked.
    MenuLoadResult LoadMenuItems() const;

    // Loads settings.json. Missing or damaged files give the defaults.
    Settings LoadSettings() const;

    // Both save functions write to a temporary file first and then rename it
    // over the real one. If the program dies halfway, the old file is intact.
    // They create the folder if needed and return false on any failure.
    bool SaveMenuItems(const std::vector<MenuItem>& items) const;
    bool SaveSettings(const Settings& settings) const;

    // Picks where configuration lives: the .exe folder when it is writable
    // (portable mode), otherwise the fallback folder, created if needed.
    static std::filesystem::path ChooseDirectory(const std::filesystem::path& exeDirectory,
                                                 const std::filesystem::path& fallbackDirectory);

    // True when a file can be created in the folder.
    static bool IsWritable(const std::filesystem::path& directory);

private:
    std::filesystem::path directory_;
};

} // namespace popa
