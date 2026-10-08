// Tests for the entries created from dropped files (DroppedItem.h).

#include "TestUtil.h"

#include "PopaCore/DroppedItem.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

void CheckItem(const MenuItem& item, std::wstring_view name, std::wstring_view command)
{
    Assert::AreEqual(ItemType::Item, item.type);
    Assert::AreEqual(std::wstring(name), item.name);
    Assert::AreEqual(std::wstring(command), item.command);
}

} // namespace

TEST_CLASS(DroppedItemTests)
{
public:
    TEST_METHOD(Quoting)
    {
        Assert::AreEqual(std::wstring(L"C:\\Apps\\a.exe"), QuoteIfNeeded(L"C:\\Apps\\a.exe"));
        Assert::AreEqual(std::wstring(L"\"C:\\Program Files\\a.exe\""), QuoteIfNeeded(L"C:\\Program Files\\a.exe"));
    }

    TEST_METHOD(ShortcutUsesItsTargetArgumentsAndIcon)
    {
        ShortcutInfo link{L"C:\\Program Files\\App\\app.exe", L"--fast", IconSpec{L"C:\\icons\\app.ico", 2}};
        const MenuItem item = ItemForDroppedPath(L"C:\\Users\\Public\\Desktop\\Mi App.lnk", PathKind::File, &link);
        CheckItem(item, L"Mi App", L"\"C:\\Program Files\\App\\app.exe\" --fast");
        Assert::IsTrue(item.icon == IconSpec{L"C:\\icons\\app.ico", 2});
    }

    TEST_METHOD(ShortcutWithoutIconUsesTheTargetIcon)
    {
        ShortcutInfo link{L"C:\\Apps\\tool.exe", L"", std::nullopt};
        const MenuItem item = ItemForDroppedPath(L"D:\\Tool.lnk", PathKind::File, &link);
        CheckItem(item, L"Tool", L"C:\\Apps\\tool.exe");
        Assert::IsFalse(item.icon.has_value());
    }

    TEST_METHOD(ShortcutWithoutFileTargetRunsTheShortcut)
    {
        ShortcutInfo link{L"", L"ignored", std::nullopt};
        const MenuItem item = ItemForDroppedPath(L"C:\\Links\\Panel de control.lnk", PathKind::File, &link);
        CheckItem(item, L"Panel de control", L"\"C:\\Links\\Panel de control.lnk\"");
    }

    TEST_METHOD(ExecutableDropsItsExtension)
    {
        CheckItem(ItemForDroppedPath(L"C:\\Program Files\\Mozilla Firefox\\firefox.exe", PathKind::File, nullptr),
                  L"firefox", L"\"C:\\Program Files\\Mozilla Firefox\\firefox.exe\"");
        CheckItem(ItemForDroppedPath(L"E:\\Games\\GAME.EXE", PathKind::File, nullptr), L"GAME", L"E:\\Games\\GAME.EXE");
    }

    TEST_METHOD(FolderUsesItsName)
    {
        CheckItem(ItemForDroppedPath(L"D:\\Mis Proyectos", PathKind::Directory, nullptr), L"Mis Proyectos",
                  L"\"D:\\Mis Proyectos\"");
        CheckItem(ItemForDroppedPath(L"D:\\Music\\", PathKind::Directory, nullptr), L"Music", L"D:\\Music\\");
        CheckItem(ItemForDroppedPath(L"D:\\", PathKind::Directory, nullptr), L"D:\\", L"D:\\");
    }

    TEST_METHOD(OtherFilesKeepTheirExtension)
    {
        CheckItem(ItemForDroppedPath(L"C:\\Docs\\Lista de compras.txt", PathKind::File, nullptr),
                  L"Lista de compras.txt", L"\"C:\\Docs\\Lista de compras.txt\"");
        CheckItem(ItemForDroppedPath(L"C:\\Tools\\run.bat", PathKind::File, nullptr), L"run.bat", L"C:\\Tools\\run.bat");
    }
};

} // namespace popa::tests
