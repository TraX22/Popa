#include "PopaCore/Model.h"

#include "PopaCore/Text.h"

#include <nlohmann/json.hpp>

#include <climits>

namespace popa {

// ordered_json keeps object keys in insertion order. The default json type
// sorts them alphabetically, which would put "command" before "type" and
// make the files harder to read and edit by hand.
using Json = nlohmann::ordered_json;

// --- Factory helpers ---------------------------------------------------------

MenuItem MenuItem::MakeItem(std::wstring name, std::wstring command,
                            std::optional<IconSpec> icon)
{
    MenuItem item;
    item.type = ItemType::Item;
    item.name = std::move(name);
    item.command = std::move(command);
    item.icon = std::move(icon);
    return item;
}

MenuItem MenuItem::MakeMenu(std::wstring name, std::vector<MenuItem> children,
                            std::optional<IconSpec> icon)
{
    MenuItem item;
    item.type = ItemType::Menu;
    item.name = std::move(name);
    item.children = std::move(children);
    item.icon = std::move(icon);
    return item;
}

MenuItem MenuItem::MakeSeparator()
{
    MenuItem item;
    item.type = ItemType::Separator;
    return item;
}

bool IsValidIconSize(int size)
{
    return size == 16 || size == 24 || size == 32 || size == 48;
}

// --- Small readers -------------------------------------------------------------
//
// These helpers read one field of a JSON object and fall back to a default
// when the field is missing or has the wrong type. nlohmann's get<T>() would
// throw an exception on a type mismatch instead; checking first keeps the
// reading code free of try/catch.

namespace {

std::wstring ReadString(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
        return {};
    return Utf8ToWide(it->get_ref<const std::string&>());
}

std::optional<long long> ReadInteger(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer())
        return std::nullopt;
    return it->get<long long>();
}

std::optional<bool> ReadBool(const Json& object, const char* key)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_boolean())
        return std::nullopt;
    return it->get<bool>();
}

// Text written to disk always ends with a newline, as most editors expect.
// error_handler_t::replace turns invalid UTF-8 into U+FFFD instead of
// throwing; it cannot happen with strings that came from WideToUtf8, but
// saving the user's menu must never crash the program.
std::string Dump(const Json& value)
{
    return value.dump(2, ' ', false, Json::error_handler_t::replace) + "\n";
}

} // namespace

// --- menu.json -----------------------------------------------------------------

namespace {

Json IconToJson(const std::optional<IconSpec>& icon)
{
    if (!icon)
        return nullptr; // JSON null: "use the command's icon"
    return Json{{"path", WideToUtf8(icon->path)}, {"index", icon->index}};
}

std::optional<IconSpec> IconFromJson(const Json& object)
{
    const auto it = object.find("icon");
    if (it == object.end() || !it->is_object())
        return std::nullopt;

    IconSpec icon;
    icon.path = ReadString(*it, "path");
    if (icon.path.empty())
        return std::nullopt;

    const auto index = ReadInteger(*it, "index").value_or(0);
    // Icon indexes fit in an int; negative values are resource IDs in the
    // Windows convention and are allowed. Anything absurd becomes 0.
    icon.index = (index >= INT_MIN && index <= INT_MAX) ? static_cast<int>(index) : 0;
    return icon;
}

Json ItemsToJson(const std::vector<MenuItem>& items); // used recursively below

Json ItemToJson(const MenuItem& item)
{
    switch (item.type) {
    case ItemType::Separator:
        return Json{{"type", "separator"}};
    case ItemType::Menu:
        return Json{{"type", "menu"},
                    {"name", WideToUtf8(item.name)},
                    {"icon", IconToJson(item.icon)},
                    {"items", ItemsToJson(item.children)}};
    case ItemType::Item:
    default:
        return Json{{"type", "item"},
                    {"name", WideToUtf8(item.name)},
                    {"command", WideToUtf8(item.command)},
                    {"icon", IconToJson(item.icon)}};
    }
}

Json ItemsToJson(const std::vector<MenuItem>& items)
{
    Json array = Json::array();
    for (const MenuItem& item : items)
        array.push_back(ItemToJson(item));
    return array;
}

// Reads an "items" array. Entries that are not objects or have an unknown
// "type" are skipped: a newer Popa might add kinds this one does not know.
std::vector<MenuItem> ItemsFromJson(const Json& array)
{
    std::vector<MenuItem> items;
    if (!array.is_array())
        return items;

    for (const Json& entry : array) {
        if (!entry.is_object())
            continue;

        const auto typeIt = entry.find("type");
        if (typeIt == entry.end() || !typeIt->is_string())
            continue;
        const std::string& type = typeIt->get_ref<const std::string&>();

        if (type == "separator") {
            items.push_back(MenuItem::MakeSeparator());
        } else if (type == "menu") {
            const auto childrenIt = entry.find("items");
            std::vector<MenuItem> children =
                childrenIt != entry.end() ? ItemsFromJson(*childrenIt) : std::vector<MenuItem>{};
            items.push_back(MenuItem::MakeMenu(ReadString(entry, "name"), std::move(children),
                                               IconFromJson(entry)));
        } else if (type == "item") {
            items.push_back(MenuItem::MakeItem(ReadString(entry, "name"),
                                               ReadString(entry, "command"),
                                               IconFromJson(entry)));
        }
    }
    return items;
}

} // namespace

std::string SerializeMenu(const std::vector<MenuItem>& items)
{
    const Json root{{"version", kMenuFormatVersion}, {"items", ItemsToJson(items)}};
    return Dump(root);
}

std::optional<std::vector<MenuItem>> ParseMenu(std::string_view json)
{
    // The third argument (false) asks the parser to return a "discarded"
    // value instead of throwing an exception when the text is not valid JSON.
    const Json root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object())
        return std::nullopt;

    const auto itemsIt = root.find("items");
    if (itemsIt == root.end() || !itemsIt->is_array())
        return std::nullopt;

    return ItemsFromJson(*itemsIt);
}

// --- settings.json ---------------------------------------------------------------

namespace {

const char* ThemeToString(Theme theme)
{
    switch (theme) {
    case Theme::Light: return "light";
    case Theme::Dark: return "dark";
    case Theme::System:
    default: return "system";
    }
}

// Stored as language codes ("en", "es") so the file reads the same in
// any language.
const char* LanguageToString(LanguageSetting language)
{
    switch (language) {
    case LanguageSetting::English: return "en";
    case LanguageSetting::Spanish: return "es";
    case LanguageSetting::System:
    default: return "system";
    }
}

std::optional<LanguageSetting> LanguageFromString(const std::wstring& text)
{
    if (text == L"system") return LanguageSetting::System;
    if (text == L"en") return LanguageSetting::English;
    if (text == L"es") return LanguageSetting::Spanish;
    return std::nullopt;
}

std::optional<Theme> ThemeFromString(const std::wstring& text)
{
    if (text == L"system") return Theme::System;
    if (text == L"light") return Theme::Light;
    if (text == L"dark") return Theme::Dark;
    return std::nullopt;
}

} // namespace

std::string SerializeSettings(const Settings& settings)
{
    const Json root{{"hotkey", WideToUtf8(settings.hotkey)},
                    {"hotkeysEnabled", settings.hotkeysEnabled},
                    {"iconSize", settings.iconSize},
                    {"theme", ThemeToString(settings.theme)},
                    {"language", LanguageToString(settings.language)},
                    {"runAtStartup", settings.runAtStartup},
                    {"checkUpdatesAtStartup", settings.checkUpdatesAtStartup}};
    return Dump(root);
}

Settings ParseSettings(std::string_view json)
{
    Settings settings; // starts with every default value

    const Json root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object())
        return settings;

    // Each field replaces its default only when present and valid.
    if (std::wstring hotkey = ReadString(root, "hotkey"); !hotkey.empty())
        settings.hotkey = std::move(hotkey);
    if (const auto value = ReadBool(root, "hotkeysEnabled"))
        settings.hotkeysEnabled = *value;
    // The range check comes before the cast so a huge number cannot wrap
    // around into a valid size.
    if (const auto value = ReadInteger(root, "iconSize");
        value && *value > 0 && *value <= 48 && IsValidIconSize(static_cast<int>(*value)))
        settings.iconSize = static_cast<int>(*value);
    if (const auto value = ThemeFromString(ReadString(root, "theme")))
        settings.theme = *value;
    if (const auto value = LanguageFromString(ReadString(root, "language")))
        settings.language = *value;
    if (const auto value = ReadBool(root, "runAtStartup"))
        settings.runAtStartup = *value;
    if (const auto value = ReadBool(root, "checkUpdatesAtStartup"))
        settings.checkUpdatesAtStartup = *value;

    return settings;
}

} // namespace popa
