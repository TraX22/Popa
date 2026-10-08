// Splitting a menu entry's command into the program to run and its arguments.
//
// A command is what the user typed in the editor, for example:
//   "C:\Program Files\App\app.exe" --flag
//   C:\Tools\tool.exe -x
//   %SystemRoot%\notepad.exe
//   https://example.com
// ShellExecuteExW wants the target (file, folder or URL) and the arguments
// as two separate strings, so the launcher needs them apart.
#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace popa {

struct SplitCommand {
    std::wstring target;    // program, document, folder or URL
    std::wstring arguments; // the rest, passed through untouched

    bool operator==(const SplitCommand&) const = default;
};

enum class PathKind { None, File, Directory };

// Tells what exists at a path. The real one asks the file system; tests pass
// a fake so the results do not depend on the machine running them.
using PathProbe = std::function<PathKind(const std::wstring&)>;

// The PathProbe that asks the file system (GetFileAttributesW).
PathKind ProbeFileSystem(const std::wstring& path);

// Replaces %NAME% with the value of the environment variable NAME
// (ExpandEnvironmentStringsW). Unknown variables are left as they are.
std::wstring ExpandEnvironment(std::wstring_view text);

// Splits a command without expanding variables. Rules:
// 1. A leading double quote: the target is everything up to the closing
//    quote, the arguments are what follows.
// 2. Unquoted and the whole text exists (a file or folder whose name has
//    spaces): the whole text is the target.
// 3. Unquoted with spaces: the shortest prefix ending before a space that is
//    an existing file is the target. This rescues paths like
//    C:\Program Files\App\app.exe --flag typed without quotes.
// 4. Otherwise the target ends at the first space.
SplitCommand Split(std::wstring_view command, const PathProbe& probe = ProbeFileSystem);

// Expands environment variables and then splits: what the launcher uses.
SplitCommand ParseCommand(std::wstring_view command, const PathProbe& probe = ProbeFileSystem);

// Completes an absolute target typed without its extension, the way the
// command prompt does: when "D:\Tools\run" does not exist, tries
// "D:\Tools\run.COM", ".EXE", ".BAT"... in the order of `pathExt` (the
// PATHEXT environment variable, ";"-separated). Targets that exist, have an
// extension, or are not absolute paths are returned unchanged.
std::wstring CompleteExtension(std::wstring_view target, std::wstring_view pathExt,
                               const PathProbe& probe = ProbeFileSystem);

// Same, with the real PATHEXT (or a standard list when it is not set).
std::wstring CompleteExtension(std::wstring_view target);

// Folder to start the program in: the folder that contains the target when
// it is an absolute file path (C:\... or \\server\...). Empty for URLs,
// bare names such as notepad.exe, and anything else.
std::wstring WorkingDirectoryFor(std::wstring_view target);

// True for absolute paths: "X:\..." with a drive letter, or "\\server\...".
bool IsAbsolutePath(std::wstring_view path);

} // namespace popa
