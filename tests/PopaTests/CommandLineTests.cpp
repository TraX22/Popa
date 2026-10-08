// Tests for splitting commands into target and arguments (CommandLine.h).

#include "TestUtil.h"

#include "PopaCore/CommandLine.h"

#include <map>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

// A fake file system: only the listed paths exist.
PathProbe FakeFileSystem(std::map<std::wstring, PathKind> entries)
{
    return [entries = std::move(entries)](const std::wstring& path) {
        const auto it = entries.find(path);
        return it == entries.end() ? PathKind::None : it->second;
    };
}

const PathProbe kNothingExists = FakeFileSystem({});

void CheckSplit(std::wstring_view command, std::wstring_view target, std::wstring_view arguments,
                const PathProbe& probe = kNothingExists)
{
    const SplitCommand result = Split(command, probe);
    Assert::AreEqual(std::wstring(target), result.target);
    Assert::AreEqual(std::wstring(arguments), result.arguments);
}

} // namespace

TEST_CLASS(CommandLineTests)
{
public:
    TEST_METHOD(QuotedTargetWithSpaces)
    {
        CheckSplit(L"\"C:\\Program Files\\Adobe\\Photoshop.exe\"", L"C:\\Program Files\\Adobe\\Photoshop.exe", L"");
        CheckSplit(L"\"C:\\Program Files\\App\\app.exe\"  --flag \"x y\"", L"C:\\Program Files\\App\\app.exe",
                   L"--flag \"x y\"");
    }

    TEST_METHOD(UnquotedTargetWithArguments)
    {
        CheckSplit(L"C:\\Apps\\Discord\\Update.exe --processStart Discord.exe", L"C:\\Apps\\Discord\\Update.exe",
                   L"--processStart Discord.exe");
        CheckSplit(L"C:\\Windows\\System32\\wscript.exe //B //Nologo \"C:\\a b\\x.vbs\"",
                   L"C:\\Windows\\System32\\wscript.exe", L"//B //Nologo \"C:\\a b\\x.vbs\"");
    }

    TEST_METHOD(SingleWordAndSurroundingSpaces)
    {
        CheckSplit(L"notepad.exe", L"notepad.exe", L"");
        CheckSplit(L"   calc.exe   ", L"calc.exe", L"");
        CheckSplit(L"https://github.com/TraX22/Popa", L"https://github.com/TraX22/Popa", L"");
        CheckSplit(L"", L"", L"");
        CheckSplit(L"   ", L"", L"");
    }

    TEST_METHOD(UnbalancedQuoteTakesEverything)
    {
        CheckSplit(L"\"C:\\My Apps\\app.exe --x", L"C:\\My Apps\\app.exe --x", L"");
    }

    TEST_METHOD(UnquotedPathWithSpacesIsFoundOnDisk)
    {
        const auto probe = FakeFileSystem({
            {L"C:\\Program", PathKind::Directory}, // a folder must not cut the path
            {L"C:\\Program Files\\App\\app.exe", PathKind::File},
        });
        CheckSplit(L"C:\\Program Files\\App\\app.exe --flag value", L"C:\\Program Files\\App\\app.exe",
                   L"--flag value", probe);
    }

    TEST_METHOD(WholeTextThatExistsIsTheTarget)
    {
        const auto probe = FakeFileSystem({{L"D:\\My Games", PathKind::Directory}});
        CheckSplit(L"D:\\My Games", L"D:\\My Games", L"", probe);
    }

    TEST_METHOD(UnknownUnquotedPathSplitsAtFirstSpace)
    {
        CheckSplit(L"C:\\No Such\\app.exe -a", L"C:\\No", L"Such\\app.exe -a");
    }

    TEST_METHOD(EnvironmentVariablesAreExpanded)
    {
        SetEnvironmentVariableW(L"POPA_TEST_DIR", L"C:\\Popa Test");
        Assert::AreEqual(std::wstring(L"C:\\Popa Test\\a.exe"), ExpandEnvironment(L"%POPA_TEST_DIR%\\a.exe"));
        Assert::AreEqual(std::wstring(L"%POPA_NOT_DEFINED%\\a.exe"), ExpandEnvironment(L"%POPA_NOT_DEFINED%\\a.exe"));
        Assert::AreEqual(std::wstring(L"no variables"), ExpandEnvironment(L"no variables"));

        // Expansion happens before splitting, so a quoted variable whose
        // value has spaces still yields the right target.
        const SplitCommand result = ParseCommand(L"\"%POPA_TEST_DIR%\\a.exe\" -x", kNothingExists);
        Assert::AreEqual(std::wstring(L"C:\\Popa Test\\a.exe"), result.target);
        Assert::AreEqual(std::wstring(L"-x"), result.arguments);
        SetEnvironmentVariableW(L"POPA_TEST_DIR", nullptr);
    }

    TEST_METHOD(WorkingDirectoryIsTheTargetFolder)
    {
        Assert::AreEqual(std::wstring(L"C:\\Program Files\\App"), WorkingDirectoryFor(L"C:\\Program Files\\App\\app.exe"));
        Assert::AreEqual(std::wstring(L"C:\\"), WorkingDirectoryFor(L"C:\\app.exe"));
        Assert::AreEqual(std::wstring(L"\\\\server\\share"), WorkingDirectoryFor(L"\\\\server\\share\\app.exe"));
        Assert::AreEqual(std::wstring(), WorkingDirectoryFor(L"notepad.exe"));
        Assert::AreEqual(std::wstring(), WorkingDirectoryFor(L"https://example.com/a/b"));
        Assert::AreEqual(std::wstring(), WorkingDirectoryFor(L"tools\\a.exe"));
    }

    TEST_METHOD(MissingExtensionIsCompletedFromPathExt)
    {
        const auto probe = FakeFileSystem({{L"D:\\Comfy\\run_gpu.bat", PathKind::File},
                                           {L"D:\\Tools\\both.exe", PathKind::File},
                                           {L"D:\\Tools\\both.bat", PathKind::File}});
        // Lowercase here because the fake file system, unlike NTFS, is case-sensitive.
        const std::wstring_view pathExt = L".com;.exe;.bat;.cmd";

        Assert::AreEqual(std::wstring(L"D:\\Comfy\\run_gpu.bat"), CompleteExtension(L"D:\\Comfy\\run_gpu", pathExt, probe));
        // PATHEXT order decides: .exe comes before .bat.
        Assert::AreEqual(std::wstring(L"D:\\Tools\\both.exe"), CompleteExtension(L"D:\\Tools\\both", pathExt, probe));
        // Nothing found: unchanged.
        Assert::AreEqual(std::wstring(L"D:\\Tools\\none"), CompleteExtension(L"D:\\Tools\\none", pathExt, probe));
    }

    TEST_METHOD(ExtensionIsNotCompletedWhenNotNeeded)
    {
        const auto probe = FakeFileSystem({{L"D:\\Apps", PathKind::Directory},
                                           {L"D:\\Apps.exe", PathKind::File},
                                           {L"D:\\x.v2\\tool.exe", PathKind::File}});
        const std::wstring_view pathExt = L".exe";
        // Exists already (a folder): a sibling "Apps.exe" must not win.
        Assert::AreEqual(std::wstring(L"D:\\Apps"), CompleteExtension(L"D:\\Apps", pathExt, probe));
        // Has an extension.
        Assert::AreEqual(std::wstring(L"D:\\a.txt"), CompleteExtension(L"D:\\a.txt", pathExt, probe));
        // A dot in a folder name is not an extension of the file.
        Assert::AreEqual(std::wstring(L"D:\\x.v2\\tool.exe"), CompleteExtension(L"D:\\x.v2\\tool", pathExt, probe));
        // Not absolute: left to ShellExecuteExW, which searches the PATH.
        Assert::AreEqual(std::wstring(L"notepad"), CompleteExtension(L"notepad", pathExt, probe));
        Assert::AreEqual(std::wstring(L"https://x.org/a"), CompleteExtension(L"https://x.org/a", pathExt, probe));
    }

    TEST_METHOD(RealFileSystemProbe)
    {
        TempDir dir;
        const std::filesystem::path file = dir.Path() / L"a file.txt";
        WriteBytes(file, "x");
        Assert::IsTrue(PathKind::File == ProbeFileSystem(file.wstring()));
        Assert::IsTrue(PathKind::Directory == ProbeFileSystem(dir.Path().wstring()));
        Assert::IsTrue(PathKind::None == ProbeFileSystem((dir.Path() / L"missing").wstring()));

        // End to end: an unquoted path with a space, found on disk.
        const SplitCommand result = Split(file.wstring() + L" --open");
        Assert::AreEqual(file.wstring(), result.target);
        Assert::AreEqual(std::wstring(L"--open"), result.arguments);
    }
};

} // namespace popa::tests
