// Tests for the JSON conversion of the menu and the settings (Model.h).

#include "TestUtil.h"

#include "PopaCore/Model.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

// A menu that uses every feature: nested submenus, separators, icons with
// and without index, null icons and non-ASCII text.
std::vector<MenuItem> SampleMenu()
{
    return {
        MenuItem::MakeItem(L"Discord", L"C:\\Apps\\Discord\\Update.exe --processStart Discord.exe"),
        MenuItem::MakeSeparator(),
        MenuItem::MakeMenu(L"Internet", {}, IconSpec{L"shell32.dll", 13}),
        MenuItem::MakeMenu(
            L"Diseño ✏️",
            {
                MenuItem::MakeItem(L"Photoshop", L"\"C:\\Program Files\\Adobe\\Photoshop.exe\""),
                MenuItem::MakeMenu(L"Más", {MenuItem::MakeItem(L"Paint", L"mspaint.exe",
                                                               IconSpec{L"%SystemRoot%\\system32\\mspaint.exe", -2})}),
            },
            IconSpec{L"icons\\adobe.ico", 0}),
    };
}

} // namespace

TEST_CLASS(ModelTests)
{
public:
    TEST_METHOD(MenuRoundTripKeepsEverything)
    {
        const auto original = SampleMenu();
        const auto parsed = ParseMenu(SerializeMenu(original));

        Assert::IsTrue(parsed.has_value());
        Assert::IsTrue(original == *parsed);
    }

    TEST_METHOD(ParsesTheExampleFromTheSpecification)
    {
        // The menu.json example of section 6 of the design document.
        const std::string json = R"({
  "version": 1,
  "items": [
    { "type": "item", "name": "Discord",
      "command": "C:\\Users\\yo\\AppData\\Local\\Discord\\Update.exe --processStart Discord.exe",
      "icon": null },
    { "type": "separator" },
    { "type": "menu", "name": "Internet",
      "icon": { "path": "shell32.dll", "index": 13 }, "items": [] },
    { "type": "menu", "name": "Adobe",
      "icon": { "path": "icons\\adobe.ico", "index": 0 },
      "items": [
        { "type": "item", "name": "Photoshop",
          "command": "\"C:\\Program Files\\Adobe\\Photoshop\\Photoshop.exe\"",
          "icon": null }
      ] }
  ]
})";
        const auto items = ParseMenu(json);
        Assert::IsTrue(items.has_value());
        Assert::AreEqual<size_t>(4, items->size());

        const MenuItem& discord = (*items)[0];
        Assert::AreEqual(ItemType::Item, discord.type);
        Assert::AreEqual(std::wstring(L"Discord"), discord.name);
        Assert::AreEqual(std::wstring(L"C:\\Users\\yo\\AppData\\Local\\Discord\\Update.exe --processStart Discord.exe"),
                         discord.command);
        Assert::IsFalse(discord.icon.has_value());

        Assert::AreEqual(ItemType::Separator, (*items)[1].type);

        const MenuItem& internet = (*items)[2];
        Assert::AreEqual(ItemType::Menu, internet.type);
        Assert::IsTrue(internet.icon == IconSpec{L"shell32.dll", 13});
        Assert::IsTrue(internet.children.empty());

        const MenuItem& adobe = (*items)[3];
        Assert::AreEqual<size_t>(1, adobe.children.size());
        Assert::AreEqual(std::wstring(L"\"C:\\Program Files\\Adobe\\Photoshop\\Photoshop.exe\""),
                         adobe.children[0].command);
    }

    TEST_METHOD(SerializedMenuIsIndentedWithTypeFirst)
    {
        const std::string json = SerializeMenu({MenuItem::MakeItem(L"A", L"a.exe")});

        // Indented with two spaces, keys in insertion order, null icon.
        Assert::IsTrue(json.starts_with("{\n  \"version\": 1,\n  \"items\": ["));
        Assert::IsTrue(json.find("\"type\": \"item\",\n      \"name\": \"A\"") != std::string::npos);
        Assert::IsTrue(json.find("\"icon\": null") != std::string::npos);
        Assert::IsTrue(json.ends_with("\n"));
    }

    TEST_METHOD(NonAsciiTextIsWrittenAsUtf8)
    {
        const std::string json = SerializeMenu({MenuItem::MakeItem(L"Año", L"x")});
        // "ñ" is the two bytes C3 B1 in UTF-8, written as-is (not \u00f1).
        Assert::IsTrue(json.find("A\xC3\xB1o") != std::string::npos);
    }

    TEST_METHOD(InvalidMenuJsonIsRejected)
    {
        Assert::IsFalse(ParseMenu("").has_value());
        Assert::IsFalse(ParseMenu("{ not json").has_value());
        Assert::IsFalse(ParseMenu("[1, 2, 3]").has_value());
        Assert::IsFalse(ParseMenu(R"({"version": 1})").has_value());
        Assert::IsFalse(ParseMenu(R"({"items": "nope"})").has_value());
    }

    TEST_METHOD(UnknownOrBrokenEntriesAreSkipped)
    {
        const auto items = ParseMenu(R"({"items": [
            {"type": "clip", "name": "From a future version"},
            42,
            {"name": "No type"},
            {"type": "item", "name": "Kept", "command": 7}
        ]})");
        Assert::IsTrue(items.has_value());
        Assert::AreEqual<size_t>(1, items->size());
        Assert::AreEqual(std::wstring(L"Kept"), (*items)[0].name);
        Assert::AreEqual(std::wstring(), (*items)[0].command); // wrong type -> empty
    }

    TEST_METHOD(IconWithoutPathMeansNoIcon)
    {
        const auto items = ParseMenu(R"({"items": [
            {"type": "item", "name": "A", "icon": {"path": "", "index": 3}},
            {"type": "item", "name": "B", "icon": {"path": "b.ico"}},
            {"type": "item", "name": "C", "icon": "shell32.dll:4"}
        ]})");
        Assert::IsTrue(items.has_value());
        Assert::IsFalse((*items)[0].icon.has_value());
        Assert::IsTrue((*items)[1].icon == IconSpec{L"b.ico", 0}); // missing index -> 0
        Assert::IsFalse((*items)[2].icon.has_value());           // not an object
    }

    TEST_METHOD(Utf8ByteOrderMarkIsAccepted)
    {
        // Notepad used to add a BOM (EF BB BF) when saving as UTF-8.
        const auto items = ParseMenu("\xEF\xBB\xBF{\"items\": []}");
        Assert::IsTrue(items.has_value());
    }

    TEST_METHOD(SettingsRoundTrip)
    {
        Settings settings;
        settings.hotkey = L"Ctrl+Alt+Space";
        settings.hotkeysEnabled = false;
        settings.iconSize = 48;
        settings.theme = Theme::Dark;
        settings.language = LanguageSetting::Spanish;
        settings.runAtStartup = true;
        settings.checkUpdatesAtStartup = false;

        Assert::IsTrue(settings == ParseSettings(SerializeSettings(settings)));
    }

    TEST_METHOD(SettingsDefaults)
    {
        const Settings settings;
        Assert::AreEqual(std::wstring(L"Win+Z"), settings.hotkey);
        Assert::IsTrue(settings.hotkeysEnabled);
        Assert::AreEqual(32, settings.iconSize);
        Assert::AreEqual(Theme::System, settings.theme);
        Assert::IsTrue(LanguageSetting::System == settings.language);
        Assert::IsFalse(settings.runAtStartup);
        Assert::IsTrue(settings.checkUpdatesAtStartup);
    }

    TEST_METHOD(InvalidSettingsJsonGivesDefaults)
    {
        Assert::IsTrue(Settings{} == ParseSettings(""));
        Assert::IsTrue(Settings{} == ParseSettings("{{{"));
        Assert::IsTrue(Settings{} == ParseSettings("[true]"));
    }

    TEST_METHOD(InvalidSettingsFieldsKeepTheirDefaults)
    {
        const Settings settings = ParseSettings(R"({
            "hotkey": "",
            "hotkeysEnabled": "yes",
            "iconSize": 20,
            "theme": "purple",
            "language": "fr",
            "runAtStartup": 1,
            "checkUpdatesAtStartup": false
        })");
        Assert::AreEqual(std::wstring(L"Win+Z"), settings.hotkey);
        Assert::IsTrue(settings.hotkeysEnabled);
        Assert::AreEqual(32, settings.iconSize);
        Assert::AreEqual(Theme::System, settings.theme);
        Assert::IsTrue(LanguageSetting::System == settings.language);
        Assert::IsFalse(settings.runAtStartup);
        Assert::IsFalse(settings.checkUpdatesAtStartup); // the one valid field
    }

    TEST_METHOD(LanguageCodes)
    {
        Assert::IsTrue(LanguageSetting::English == ParseSettings(R"({"language": "en"})").language);
        Assert::IsTrue(LanguageSetting::Spanish == ParseSettings(R"({"language": "es"})").language);
        Settings settings;
        settings.language = LanguageSetting::English;
        Assert::IsTrue(SerializeSettings(settings).find("\"language\": \"en\"") != std::string::npos);
    }

    TEST_METHOD(HugeIconSizeDoesNotWrapAround)
    {
        // 2^32 + 16 would become 16 if cast to int before checking the range.
        const Settings settings = ParseSettings(R"({"iconSize": 4294967312})");
        Assert::AreEqual(32, settings.iconSize);
    }

    TEST_METHOD(ValidIconSizes)
    {
        for (int size : {16, 24, 32, 48})
            Assert::IsTrue(IsValidIconSize(size));
        for (int size : {0, -16, 20, 64})
            Assert::IsFalse(IsValidIconSize(size));
    }
};

} // namespace popa::tests
