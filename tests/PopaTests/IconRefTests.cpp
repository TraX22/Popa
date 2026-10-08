// Tests for "file:index" icon references and icon path resolution (IconRef.h).

#include "TestUtil.h"

#include "PopaCore/IconRef.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

void CheckParse(std::wstring_view text, std::wstring_view path, int index)
{
    const IconSpec icon = ParseIconRef(text);
    Assert::AreEqual(std::wstring(path), icon.path);
    Assert::AreEqual(index, icon.index);
}

PathProbe OnlyFile(std::wstring existing)
{
    return [existing = std::move(existing)](const std::wstring& path) {
        return path == existing ? PathKind::File : PathKind::None;
    };
}

const PathProbe kNothingExists = [](const std::wstring&) { return PathKind::None; };

} // namespace

TEST_CLASS(IconRefTests)
{
public:
    TEST_METHOD(FileAndIndex)
    {
        CheckParse(L"shell32.dll:13", L"shell32.dll", 13);
        CheckParse(L"%SystemRoot%\\System32\\SHELL32.dll:130", L"%SystemRoot%\\System32\\SHELL32.dll", 130);
        CheckParse(L"C:\\Program Files\\App\\app.exe:0", L"C:\\Program Files\\App\\app.exe", 0);
        CheckParse(L"imageres.dll:-102", L"imageres.dll", -102); // negative: resource ID
    }

    TEST_METHOD(QuotesAreRemoved)
    {
        // As some XML configurations write it: quotes around the path only.
        CheckParse(L"\"C:\\Program Files (x86)\\Steam\\steam.exe\":0", L"C:\\Program Files (x86)\\Steam\\steam.exe", 0);
        CheckParse(L"\"C:\\Apps\\a.exe\":5", L"C:\\Apps\\a.exe", 5);
        // Quotes around everything.
        CheckParse(L"\"C:\\My Apps\\a.exe:3\"", L"C:\\My Apps\\a.exe", 3);
        CheckParse(L"\"C:\\My Apps\\a.ico\"", L"C:\\My Apps\\a.ico", 0);
    }

    TEST_METHOD(MissingIndexMeansZero)
    {
        CheckParse(L"C:\\icons\\app.ico", L"C:\\icons\\app.ico", 0);
        CheckParse(L"app.ico", L"app.ico", 0);
        CheckParse(L"", L"", 0);
    }

    TEST_METHOD(InvalidIndexStaysInThePath)
    {
        CheckParse(L"weird:name.ico", L"weird:name.ico", 0);
        CheckParse(L"a.dll:99999999999", L"a.dll:99999999999", 0); // does not fit an int
    }

    TEST_METHOD(FormatIsTheInverseOfParse)
    {
        const IconSpec icon{L"C:\\Program Files\\App\\app.exe", 7};
        Assert::AreEqual(std::wstring(L"C:\\Program Files\\App\\app.exe:7"), FormatIconRef(icon));
        Assert::IsTrue(icon == ParseIconRef(FormatIconRef(icon)));
    }

    TEST_METHOD(AbsolutePathsStayAsTheyAre)
    {
        Assert::AreEqual(std::wstring(L"C:\\icons\\a.ico"),
                         ResolveIconPath(L"C:\\icons\\a.ico", L"D:\\Popa", L"C:\\Windows\\System32", kNothingExists));
        Assert::AreEqual(std::wstring(L"\\\\nas\\icons\\a.ico"),
                         ResolveIconPath(L"\\\\nas\\icons\\a.ico", L"D:\\Popa", L"C:\\Windows\\System32", kNothingExists));
    }

    TEST_METHOD(VariablesAreExpanded)
    {
        SetEnvironmentVariableW(L"POPA_TEST_ROOT", L"C:\\Win");
        Assert::AreEqual(std::wstring(L"C:\\Win\\System32\\shell32.dll"),
                         ResolveIconPath(L"%POPA_TEST_ROOT%\\System32\\shell32.dll", L"D:\\Popa",
                                         L"C:\\Windows\\System32", kNothingExists));
        SetEnvironmentVariableW(L"POPA_TEST_ROOT", nullptr);
    }

    TEST_METHOD(RelativePathsAreNextToTheExe)
    {
        Assert::AreEqual(std::wstring(L"D:\\Popa\\icons\\adobe.ico"),
                         ResolveIconPath(L"icons\\adobe.ico", L"D:\\Popa", L"C:\\Windows\\System32", kNothingExists));
        Assert::AreEqual(std::wstring(L"D:\\Popa\\icons\\adobe.ico"),
                         ResolveIconPath(L"icons\\adobe.ico", L"D:\\Popa\\", L"C:\\Windows\\System32", kNothingExists));
    }

    TEST_METHOD(BareNamesUseSystem32UnlessNextToTheExe)
    {
        Assert::AreEqual(std::wstring(L"C:\\Windows\\System32\\pifmgr.dll"),
                         ResolveIconPath(L"pifmgr.dll", L"D:\\Popa", L"C:\\Windows\\System32", kNothingExists));
        Assert::AreEqual(std::wstring(L"D:\\Popa\\mine.ico"),
                         ResolveIconPath(L"mine.ico", L"D:\\Popa", L"C:\\Windows\\System32",
                                         OnlyFile(L"D:\\Popa\\mine.ico")));
    }

    TEST_METHOD(RealSystemFolder)
    {
        // The overload without a system folder asks Windows for System32.
        const std::wstring resolved = ResolveIconPath(L"shell32.dll", L"Z:\\nowhere");
        Assert::IsTrue(PathKind::File == ProbeFileSystem(resolved), resolved.c_str());
    }
};

} // namespace popa::tests
