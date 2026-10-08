#include "PopaCore/ConfigStore.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>

namespace popa {

namespace fs = std::filesystem;

namespace {

// Reads a whole file as bytes. Returns no value when it cannot be opened.
// Binary mode matters: in text mode the C++ library would translate line
// endings, and the JSON text must reach the parser untouched.
std::optional<std::string> ReadFileBytes(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return std::nullopt;
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (file.bad())
        return std::nullopt;
    return bytes;
}

// Writes the text to "<path>.tmp" and then renames it to <path>.
// A rename within one folder is atomic on NTFS: readers see either the old
// file or the new one, never a half-written mix.
bool WriteFileAtomically(const fs::path& path, const std::string& bytes)
{
    // The std::filesystem functions used here take a std::error_code so they
    // report failures through it instead of throwing exceptions.
    std::error_code error;
    fs::create_directories(path.parent_path(), error);

    fs::path tempPath = path;
    tempPath += L".tmp";

    {
        std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        file.flush();
        if (!file) {
            file.close();
            fs::remove(tempPath, error);
            return false;
        }
    } // the ofstream destructor closes the file here, before the rename

    // std::filesystem::rename replaces an existing destination file.
    fs::rename(tempPath, path, error);
    if (error) {
        fs::remove(tempPath, error);
        return false;
    }
    return true;
}

} // namespace

ConfigStore::ConfigStore(fs::path directory)
    : directory_(std::move(directory))
{
}

MenuLoadResult ConfigStore::LoadMenuItems() const
{
    MenuLoadResult result;
    const fs::path path = MenuPath();

    std::error_code error;
    if (!fs::exists(path, error)) {
        result.status = LoadStatus::Missing;
        return result;
    }

    const auto bytes = ReadFileBytes(path);
    if (!bytes) {
        result.status = LoadStatus::Unreadable;
        return result;
    }

    if (auto items = ParseMenu(*bytes)) {
        result.items = std::move(*items);
        result.status = LoadStatus::Ok;
        return result;
    }

    // Damaged file: keep a copy so the user can recover it by hand. The
    // copy replaces an older .bak; the newest damaged file is the useful one.
    result.status = LoadStatus::Corrupt;
    fs::path backupPath = path;
    backupPath += kBackupSuffix;
    if (fs::copy_file(path, backupPath, fs::copy_options::overwrite_existing, error))
        result.backupPath = std::move(backupPath);
    return result;
}

Settings ConfigStore::LoadSettings() const
{
    const auto bytes = ReadFileBytes(SettingsPath());
    if (!bytes)
        return Settings{};
    return ParseSettings(*bytes);
}

bool ConfigStore::SaveMenuItems(const std::vector<MenuItem>& items) const
{
    return WriteFileAtomically(MenuPath(), SerializeMenu(items));
}

bool ConfigStore::SaveSettings(const Settings& settings) const
{
    return WriteFileAtomically(SettingsPath(), SerializeSettings(settings));
}

bool ConfigStore::IsWritable(const fs::path& directory)
{
    // The only reliable test is to try: folder permissions on Windows are
    // ACLs that are hard to evaluate by hand, and the folder could also be
    // on read-only media.
    const fs::path probe = directory / L".popa-write-test";
    {
        std::ofstream file(probe, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
    }
    std::error_code error;
    fs::remove(probe, error);
    return true;
}

fs::path ConfigStore::ChooseDirectory(const fs::path& exeDirectory,
                                      const fs::path& fallbackDirectory)
{
    if (IsWritable(exeDirectory))
        return exeDirectory;

    std::error_code error;
    fs::create_directories(fallbackDirectory, error);
    return fallbackDirectory;
}

} // namespace popa
