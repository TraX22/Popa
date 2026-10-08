// Tests for loading and saving the configuration files (ConfigStore.h).

#include "TestUtil.h"

#include "PopaCore/ConfigStore.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;

namespace popa::tests {

TEST_CLASS(ConfigStoreTests)
{
public:
    TEST_METHOD(MissingMenuGivesEmptyMenu)
    {
        TempDir dir;
        const auto result = ConfigStore(dir.Path()).LoadMenuItems();
        Assert::AreEqual(LoadStatus::Missing, result.status);
        Assert::IsTrue(result.items.empty());
    }

    TEST_METHOD(SavedMenuLoadsBack)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        const std::vector<MenuItem> items{
            MenuItem::MakeItem(L"Notas", L"notepad.exe"),
            MenuItem::MakeSeparator(),
            MenuItem::MakeMenu(L"Sub", {MenuItem::MakeItem(L"Calc", L"calc.exe")},
                               IconSpec{L"shell32.dll", 13}),
        };

        Assert::IsTrue(store.SaveMenuItems(items));
        const auto result = store.LoadMenuItems();

        Assert::AreEqual(LoadStatus::Ok, result.status);
        Assert::IsTrue(items == result.items);
    }

    TEST_METHOD(SavingAgainReplacesTheFileAndLeavesNoTemporary)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        Assert::IsTrue(store.SaveMenuItems({MenuItem::MakeItem(L"Old", L"old.exe")}));
        Assert::IsTrue(store.SaveMenuItems({MenuItem::MakeItem(L"New", L"new.exe")}));

        const auto result = store.LoadMenuItems();
        Assert::AreEqual<size_t>(1, result.items.size());
        Assert::AreEqual(std::wstring(L"New"), result.items[0].name);
        Assert::IsFalse(fs::exists(dir.Path() / L"menu.json.tmp"));
    }

    TEST_METHOD(SaveCreatesTheFolder)
    {
        TempDir dir;
        ConfigStore store(dir.Path() / L"nested" / L"Popa");
        Assert::IsTrue(store.SaveSettings(Settings{}));
        Assert::IsTrue(fs::exists(store.SettingsPath()));
    }

    TEST_METHOD(CorruptMenuIsBackedUpAndGivesEmptyMenu)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        const std::string damaged = "{\"items\": [ {\"type\": \"item\", ";
        WriteBytes(store.MenuPath(), damaged);

        const auto result = store.LoadMenuItems();

        Assert::AreEqual(LoadStatus::Corrupt, result.status);
        Assert::IsTrue(result.items.empty());
        Assert::IsTrue(result.backupPath == dir.Path() / L"menu.json.bak");
        Assert::AreEqual(damaged, ReadBytes(result.backupPath));
        // The original stays where it was until the program saves again.
        Assert::AreEqual(damaged, ReadBytes(store.MenuPath()));
    }

    TEST_METHOD(LockedMenuIsReportedAsUnreadable)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        Assert::IsTrue(store.SaveMenuItems({}));

        // Open the file with share mode 0: nobody else may open it while
        // this handle lives, the same as another program holding it locked.
        HANDLE lock = CreateFileW(store.MenuPath().c_str(), GENERIC_READ, 0, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Assert::IsTrue(lock != INVALID_HANDLE_VALUE);
        const auto result = store.LoadMenuItems();
        CloseHandle(lock);

        Assert::AreEqual(LoadStatus::Unreadable, result.status);
        Assert::IsFalse(fs::exists(dir.Path() / L"menu.json.bak"));
    }

    TEST_METHOD(MissingSettingsGiveDefaults)
    {
        TempDir dir;
        Assert::IsTrue(Settings{} == ConfigStore(dir.Path()).LoadSettings());
    }

    TEST_METHOD(CorruptSettingsGiveDefaults)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        WriteBytes(store.SettingsPath(), "\x01\x02 not json");
        Assert::IsTrue(Settings{} == store.LoadSettings());
    }

    TEST_METHOD(SavedSettingsLoadBack)
    {
        TempDir dir;
        ConfigStore store(dir.Path());
        Settings settings;
        settings.iconSize = 16;
        settings.theme = Theme::Light;
        settings.runAtStartup = true;

        Assert::IsTrue(store.SaveSettings(settings));
        Assert::IsTrue(settings == store.LoadSettings());
    }

    TEST_METHOD(WritableExeFolderIsChosen)
    {
        TempDir exeDir;
        TempDir fallback;
        Assert::IsTrue(ConfigStore::ChooseDirectory(exeDir.Path(), fallback.Path()) == exeDir.Path());
        // The write probe must not leave anything behind.
        Assert::IsTrue(fs::is_empty(exeDir.Path()));
    }

    TEST_METHOD(UnwritableExeFolderFallsBack)
    {
        TempDir dir;
        // A folder that does not exist cannot receive files.
        const fs::path missing = dir.Path() / L"does-not-exist";
        const fs::path fallback = dir.Path() / L"AppData" / L"Popa";

        Assert::IsFalse(ConfigStore::IsWritable(missing));
        Assert::IsTrue(ConfigStore::ChooseDirectory(missing, fallback) == fallback);
        Assert::IsTrue(fs::is_directory(fallback));
    }
};

} // namespace popa::tests
