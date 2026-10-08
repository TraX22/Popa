// Data model: the tree of menu entries and the user settings, plus their
// conversion to and from the JSON text stored in menu.json and settings.json.
//
// The JSON library (nlohmann/json) is only used inside Model.cpp. The public
// functions take and return plain std::string, so code that includes this
// header does not pay the compile time of the 25,000-line json.hpp.
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace popa {

// An icon is identified by a file plus the index of the icon inside it,
// as in Windows icon locations: "shell32.dll" + 13 is icon number 13 of shell32.
struct IconSpec {
    std::wstring path;
    int index = 0;

    bool operator==(const IconSpec&) const = default;
};

enum class ItemType {
    Item,      // runs a command
    Menu,      // submenu with children
    Separator, // thin line between entries
};

// One entry of the popup menu. A single struct covers the three kinds;
// fields that do not apply to a kind stay empty (a separator has no name,
// only a Menu has children, only an Item has a command).
struct MenuItem {
    ItemType type = ItemType::Item;
    std::wstring name;
    std::wstring command;
    // No value means "use the icon of the command's file".
    std::optional<IconSpec> icon;
    std::vector<MenuItem> children;

    // C++20 generates == member by member, recursing into children.
    bool operator==(const MenuItem&) const = default;

    static MenuItem MakeItem(std::wstring name, std::wstring command,
                             std::optional<IconSpec> icon = std::nullopt);
    static MenuItem MakeMenu(std::wstring name, std::vector<MenuItem> children = {},
                             std::optional<IconSpec> icon = std::nullopt);
    static MenuItem MakeSeparator();
};

enum class Theme {
    System, // follow the Windows light/dark setting
    Light,
    Dark,
};

// Language of the interface: System follows the Windows display language.
enum class LanguageSetting {
    System,
    English,
    Spanish,
};

struct Settings {
    std::wstring hotkey = L"Win+Z";
    bool hotkeysEnabled = true;
    int iconSize = 32; // one of 16, 24, 32, 48
    Theme theme = Theme::System;
    LanguageSetting language = LanguageSetting::System;
    bool runAtStartup = false;
    bool checkUpdatesAtStartup = true;

    bool operator==(const Settings&) const = default;
};

// Icon sizes the popup menu can draw.
bool IsValidIconSize(int size);

// --- menu.json --------------------------------------------------------------

// Current format version written to the "version" field.
inline constexpr int kMenuFormatVersion = 1;

// Converts the top-level menu entries to indented UTF-8 JSON.
std::string SerializeMenu(const std::vector<MenuItem>& items);

// Parses menu.json text. Returns no value when the text is not valid JSON
// or lacks the "items" array, which the caller treats as a damaged file.
// Individual entries are read leniently: unknown types are skipped and
// missing fields take empty values, so one odd entry does not lose the rest.
std::optional<std::vector<MenuItem>> ParseMenu(std::string_view json);

// --- settings.json ----------------------------------------------------------

std::string SerializeSettings(const Settings& settings);

// Parses settings.json text. Never fails: invalid JSON gives the defaults,
// and each missing or invalid field keeps its default value.
Settings ParseSettings(std::string_view json);

} // namespace popa
