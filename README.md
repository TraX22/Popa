# Popa

**A small, private application launcher for Windows 11.** Press **Win+Z**
and a menu with your programs opens where the mouse pointer is.

[Leer en español](README.es.md)

- **One file, nothing to install.** `Popa.exe` is about 0.6 MB and needs no
  runtime. The configuration lives next to it.
- **Only what you put in.** Popa never scans your disk or detects installed
  programs.
- **Private.** No telemetry, no statistics. The only connection Popa makes
  is the update check described below.
- **Looks like Windows 11.** Native menu with rounded corners, icons from
  16 to 48 px, light and dark themes, sharp on high-DPI screens.
- **Open source** (MIT), written in C++ with the plain Windows API. The
  code is commented so it can be used to learn C++ and Win32.

## Download

Get `Popa.exe` from the [latest release](https://github.com/TraX22/Popa/releases/latest),
put it in any folder you like and run it. Popa shows an icon next to the clock.

Windows SmartScreen may warn about an unknown publisher the first time,
because the program is not digitally signed. Choose **More info → Run anyway**.
Each release lists the SHA-256 fingerprint of `Popa.exe` if you want to
verify the download.

## Using Popa

| Action | Result |
|---|---|
| **Win+Z** (configurable) | Opens the menu at the mouse pointer |
| Click the tray icon | Opens the menu |
| Double-click the tray icon | Opens the editor |
| Right-click the tray icon | Editor, Options, Check for updates, enable/disable the hotkey, Exit |

In the menu, use the mouse or the keyboard: arrows, Enter, Esc, and the
first letter of an entry to jump to it.

### The editor

- **Add item / Add submenu / Add separator**, **Move up / Move down**,
  **Delete** (also with the Delete key).
- Drag entries inside the tree to reorder them or to move them into a
  submenu.
- Drag files, folders or shortcuts from Explorer or the desktop onto the
  tree to add them.
- For each entry: name, command line (with **Browse…**), icon (**Change…**
  picks any icon of an `.exe`, `.dll`, `.ico` or `.png`; **Reset** goes back
  to the program's own icon) and **Test**.
- Entries whose program cannot be found are shown in red.
- Changes are saved automatically.

### Importing a configuration

In the editor, **Import configuration…** and choose an XML menu
configuration (a file with `<menu>` and `<item>` elements). Submenus,
separators, commands and icons are imported; the original file is only
read, never changed.

### Options

Hotkey, icon size (16, 24, 32 or 48 px), theme (same as Windows, light or
dark), language (same as Windows, English or Spanish), start with Windows,
and check for updates at startup.

## Configuration files

Popa keeps two UTF-8 files next to `Popa.exe`, indented so they can be
edited by hand: `menu.json` (the menu) and `settings.json` (the options).
If that folder is not writable (for example inside `Program Files`), they
go to `%APPDATA%\Popa` instead.

```json
{
  "version": 1,
  "items": [
    { "type": "item", "name": "Notepad", "command": "notepad.exe", "icon": null },
    { "type": "separator" },
    { "type": "menu", "name": "Tools", "icon": { "path": "shell32.dll", "index": 21 },
      "items": [
        { "type": "item", "name": "Calculator", "command": "calc.exe", "icon": null }
      ] }
  ]
}
```

- `icon: null` uses the icon of the command's file.
- Icon paths may contain environment variables (`%SystemRoot%`), be relative
  to `Popa.exe`'s folder, or be bare names such as `shell32.dll`.
- If `menu.json` gets damaged, Popa keeps a copy as `menu.json.bak` and
  starts with an empty menu.

## Privacy and network

Popa contacts **only**
`https://api.github.com/repos/TraX22/Popa/releases/latest`, to find out
whether there is a newer version: once a few seconds after starting (can be
turned off in Options) and when you choose **Check for updates**. The
request carries only the header `User-Agent: Popa/<version>`. Without
internet the automatic check stays silent.

When you accept an update, Popa downloads `Popa.exe` from the release, only
over HTTPS from GitHub's own addresses, checks that it is a valid program
and that it matches the SHA-256 fingerprint GitHub publishes, and replaces
itself. Your `menu.json` and `settings.json` are never touched.

## Known limitations

- **Antivirus software** may distrust the low-level keyboard hook Popa uses
  for Win+Z, because the program is not digitally signed. The hook is
  needed because Windows 11 reserves Win+Z and refuses to register it in the
  normal way.
- The hotkey does not work while a program **running as administrator** is
  in the foreground: Windows does not let normal programs see the keyboard
  of elevated ones. Click the tray icon instead.
- The editor uses the light theme; with dark mode only its title bar is dark.

## Building

Requirements: **Visual Studio 2026** with the "Desktop development with C++"
workload (MSVC toolset v145, Windows SDK).

```powershell
.\build.ps1                         # Debug and Release, plus all unit tests
.\build.ps1 -Configuration Release  # Release only
.\publish.ps1                       # Release build ready to publish in dist\
```

Or open `Popa.slnx` in Visual Studio.

Project layout:

| Folder | Contents |
|---|---|
| `src/PopaCore` | Static library with all the logic without windows (model, JSON, command lines, icons, versions, import, languages, update rules) |
| `src/Popa` | The Win32 application (tray, hotkey, menu, editor, dialogs, updater) |
| `tests/PopaTests` | Unit tests of PopaCore (Microsoft CppUnitTest) |
| `third_party/nlohmann` | [nlohmann/json](https://github.com/nlohmann/json) single header (MIT) |
| `tools` | Script that draws the icon |
| `docs` | Design document |

Release builds use the static C++ runtime (`/MT`) and are optimized for
size.

## License

[MIT](LICENSE). nlohmann/json is also MIT licensed
([third_party/nlohmann/LICENSE.MIT](third_party/nlohmann/LICENSE.MIT)).
