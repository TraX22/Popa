// Tests for the menu importer (MenuImporter.h) and for reading XML files
// with XmlLite (src/Popa/XmlFile.cpp, compiled into the tests too).

#include "TestUtil.h"

#include "Popa/XmlFile.h"
#include "PopaCore/MenuImporter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
namespace fs = std::filesystem;

namespace popa::tests {

namespace {

// __FILE__ is this file's full path (UseFullPaths in the project), so the
// test data is found next to it no matter where the tests run from.
fs::path ThisFolder()
{
    return fs::path(__FILE__).parent_path();
}

XmlElement Element(std::wstring name, std::vector<std::pair<std::wstring, std::wstring>> attributes = {},
                   std::vector<XmlElement> children = {}, std::wstring text = {})
{
    return XmlElement{std::move(name), std::move(attributes), std::move(text), std::move(children)};
}

XmlElement Command(std::wstring text)
{
    return Element(L"command", {}, {}, std::move(text));
}

// Counts the <item> elements under an element, at any depth.
size_t CountItemElements(const XmlElement& element)
{
    size_t count = 0;
    for (const XmlElement& child : element.children) {
        if (child.name == L"item")
            count += 1 + CountItemElements(child);
    }
    return count;
}

void CheckNoPlaceholderCommands(const std::vector<MenuItem>& items)
{
    for (const MenuItem& item : items) {
        Assert::AreNotEqual(std::wstring(L"command"), item.command, item.name.c_str());
        CheckNoPlaceholderCommands(item.children);
    }
}

} // namespace

TEST_CLASS(MenuImporterTests)
{
public:
    TEST_METHOD(ConvertsEveryKindOfEntry)
    {
        const XmlElement root = Element(
            L"config", {},
            {Element(L"lastID", {}, {}, L"9"),
             Element(L"menu", {{L"ID", L"0"}, {L"name", L"main"}},
                     {Element(L"item", {{L"ID", L"1"}, {L"name", L"App"}, {L"icon", L"\"C:\\A b\\app.exe\":3"}},
                              {Command(L"  \"C:\\A b\\app.exe\" -x  ")}),
                      Element(L"item", {{L"ID", L"2"}, {L"issep", L"1"}, {L"name", L"----<"}}),
                      Element(L"item", {{L"ID", L"3"}, {L"name", L"Sub"}, {L"icon", L"shell32.dll:4"}},
                              {Command(L"command"),
                               Element(L"item", {{L"name", L"Inner"}}, {Command(L"notepad.exe")})}),
                      Element(L"item", {{L"name", L"Empty"}}, {Command(L"command")})}),
             Element(L"settings", {}, {Element(L"opt", {{L"name", L"x"}, {L"value", L"1"}})})});

        const auto items = ImportMenuXml(root);
        Assert::IsTrue(items.has_value());
        Assert::AreEqual<size_t>(4, items->size());

        const MenuItem& app = (*items)[0];
        Assert::AreEqual(ItemType::Item, app.type);
        Assert::AreEqual(std::wstring(L"App"), app.name);
        Assert::AreEqual(std::wstring(L"\"C:\\A b\\app.exe\" -x"), app.command); // trimmed
        Assert::IsTrue(app.icon == IconSpec{L"C:\\A b\\app.exe", 3});         // quotes removed

        Assert::AreEqual(ItemType::Separator, (*items)[1].type);
        Assert::AreEqual(std::wstring(), (*items)[1].name); // decorative name dropped

        const MenuItem& sub = (*items)[2];
        Assert::AreEqual(ItemType::Menu, sub.type);
        Assert::IsTrue(sub.icon == IconSpec{L"shell32.dll", 4});
        Assert::AreEqual(std::wstring(), sub.command); // placeholder ignored
        Assert::AreEqual<size_t>(1, sub.children.size());
        Assert::IsFalse(sub.children[0].icon.has_value()); // no icon attribute: automatic

        const MenuItem& empty = (*items)[3];
        Assert::AreEqual(ItemType::Menu, empty.type);
        Assert::IsTrue(empty.children.empty());
    }

    TEST_METHOD(MenuElementCanBeTheRoot)
    {
        const auto items = ImportMenuXml(Element(L"menu", {}, {Element(L"item", {{L"issep", L"1"}})}));
        Assert::IsTrue(items.has_value());
        Assert::AreEqual<size_t>(1, items->size());
    }

    TEST_METHOD(OtherXmlIsRejected)
    {
        Assert::IsFalse(ImportMenuXml(Element(L"html", {}, {Element(L"body")})).has_value());
    }

    TEST_METHOD(ReadsTheSampleWithXmlLite)
    {
        const auto root = ReadXmlFile((ThisFolder() / L"data" / L"menu_sample.xml").wstring());
        Assert::IsTrue(root.has_value());
        Assert::AreEqual(std::wstring(L"config"), root->name);

        const auto items = ImportMenuXml(*root);
        Assert::IsTrue(items.has_value());
        Assert::AreEqual<size_t>(6, items->size());

        // Icon written with &quot; entities around the path.
        Assert::IsTrue((*items)[0].icon == IconSpec{L"C:\\Program Files\\Example Browser\\browser.exe", 0});
        Assert::AreEqual(std::wstring(L"\"C:\\Program Files\\Example Browser\\browser.exe\""), (*items)[0].command);

        // Submenu without its own command, with a separator and a variable icon inside.
        const MenuItem& tools = (*items)[1];
        Assert::AreEqual(ItemType::Menu, tools.type);
        Assert::AreEqual<size_t>(3, tools.children.size());
        Assert::AreEqual(ItemType::Separator, tools.children[1].type);
        Assert::IsTrue(tools.children[2].icon == IconSpec{L"%SystemRoot%\\System32\\SHELL32.dll", 130});
        Assert::AreEqual(std::wstring(L"C:\\Windows\\System32\\wscript.exe //B //Nologo \"C:\\Tools\\My Scripts\\start.vbs\""),
                         tools.children[2].command);

        Assert::AreEqual(ItemType::Separator, (*items)[2].type);

        // Entities in names (&amp;) and non-ASCII text (UTF-16 file).
        const MenuItem& games = (*items)[3];
        Assert::AreEqual(ItemType::Menu, games.type);
        Assert::AreEqual(std::wstring(L"Lanzador & tienda"), games.children[0].name);
        Assert::AreEqual(std::wstring(L"Vacío"), (*items)[4].name);
        Assert::AreEqual(ItemType::Menu, (*items)[4].type);
        Assert::IsFalse((*items)[5].icon.has_value());
    }

    TEST_METHOD(BrokenXmlIsReported)
    {
        TempDir dir;
        const fs::path file = dir.Path() / L"broken.xml";
        WriteBytes(file, "<config><menu><item name=\"a\"></menu>");
        Assert::IsFalse(ReadXmlFile(file.wstring()).has_value());
        Assert::IsFalse(ReadXmlFile((dir.Path() / L"missing.xml").wstring()).has_value());
    }

    TEST_METHOD(DtdIsRefused)
    {
        TempDir dir;
        const fs::path file = dir.Path() / L"dtd.xml";
        WriteBytes(file, "<?xml version=\"1.0\"?><!DOCTYPE config [<!ENTITY x \"y\">]><config><menu/></config>");
        Assert::IsFalse(ReadXmlFile(file.wstring()).has_value());
    }

    TEST_METHOD(ReadsPrivateConfigurationsWhenPresent)
    {
        // docs/private/ may hold real configurations with personal paths; the
        // folder is not in the repository (.gitignore). Every .xml found
        // there must import completely: one entry per <item>, no
        // placeholder commands. Without the folder the test does nothing.
        const fs::path folder = ThisFolder().parent_path().parent_path() / L"docs" / L"private";
        std::error_code error;
        if (!fs::is_directory(folder, error)) {
            Logger::WriteMessage(L"docs/private not found; test skipped.");
            return;
        }
        for (const auto& entry : fs::directory_iterator(folder, error)) {
            if (entry.path().extension() != L".xml")
                continue;
            const auto root = ReadXmlFile(entry.path().wstring());
            Assert::IsTrue(root.has_value(), entry.path().c_str());
            const auto items = ImportMenuXml(*root);
            Assert::IsTrue(items.has_value(), entry.path().c_str());
            Assert::AreEqual(CountItemElements(*root->Child(L"menu")), CountEntries(*items));
            CheckNoPlaceholderCommands(*items);
        }
    }
};

} // namespace popa::tests
