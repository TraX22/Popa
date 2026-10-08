#include "PopaCore/HotkeyText.h"

#include <windows.h>

#include <cwctype>
#include <vector>

namespace popa {

namespace {

struct NamedKey {
    const wchar_t* name;
    unsigned vk;
};

// Keys with a name; letters, digits and F-keys are handled by rules below.
constexpr NamedKey kNamedKeys[] = {
    {L"Space", VK_SPACE},    {L"Tab", VK_TAB},          {L"Enter", VK_RETURN},  {L"Esc", VK_ESCAPE},
    {L"Backspace", VK_BACK}, {L"Insert", VK_INSERT},    {L"Delete", VK_DELETE}, {L"Home", VK_HOME},
    {L"End", VK_END},        {L"PageUp", VK_PRIOR},     {L"PageDown", VK_NEXT}, {L"Up", VK_UP},
    {L"Down", VK_DOWN},      {L"Left", VK_LEFT},        {L"Right", VK_RIGHT},   {L"Pause", VK_PAUSE},
};

std::wstring Lower(std::wstring_view text)
{
    std::wstring result(text);
    for (wchar_t& c : result)
        c = static_cast<wchar_t>(std::towlower(c));
    return result;
}

std::wstring_view Trim(std::wstring_view text)
{
    while (!text.empty() && std::iswspace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && std::iswspace(text.back()))
        text.remove_suffix(1);
    return text;
}

bool IsFunctionKey(unsigned vk)
{
    return vk >= VK_F1 && vk <= VK_F24;
}

// Virtual-key code for a key name, or 0 when unknown.
unsigned KeyFromName(std::wstring_view name)
{
    // Letters and digits: their virtual-key codes are the ASCII codes of
    // the upper-case letter or the digit ('A' = 0x41, '0' = 0x30).
    if (name.size() == 1) {
        const wchar_t c = static_cast<wchar_t>(std::towupper(name[0]));
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9'))
            return c;
        return 0;
    }

    const std::wstring lower = Lower(name);
    // F1..F24 have consecutive codes starting at VK_F1.
    if (lower.size() >= 2 && lower.size() <= 3 && lower[0] == L'f') {
        int number = 0;
        for (size_t i = 1; i < lower.size(); ++i) {
            if (!std::iswdigit(lower[i]))
                return 0;
            number = number * 10 + (lower[i] - L'0');
        }
        if (number >= 1 && number <= 24)
            return VK_F1 + static_cast<unsigned>(number - 1);
        return 0;
    }

    for (const NamedKey& key : kNamedKeys) {
        if (lower == Lower(key.name))
            return key.vk;
    }
    return 0;
}

std::wstring NameFromKey(unsigned vk)
{
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9'))
        return std::wstring(1, static_cast<wchar_t>(vk));
    if (IsFunctionKey(vk))
        return L"F" + std::to_wstring(vk - VK_F1 + 1);
    for (const NamedKey& key : kNamedKeys) {
        if (key.vk == vk)
            return key.name;
    }
    return {};
}

} // namespace

std::optional<Hotkey> ParseHotkey(std::wstring_view text)
{
    Hotkey hotkey;
    bool hasKey = false;

    // Split at "+" and look at each part.
    while (true) {
        const size_t plus = text.find(L'+');
        const std::wstring_view part = Trim(text.substr(0, plus));
        if (part.empty())
            return std::nullopt; // "Win+", "+Z", "Win++Z"

        const std::wstring lower = Lower(part);
        bool* modifier = nullptr;
        if (lower == L"win") modifier = &hotkey.win;
        else if (lower == L"ctrl" || lower == L"control") modifier = &hotkey.ctrl;
        else if (lower == L"alt") modifier = &hotkey.alt;
        else if (lower == L"shift") modifier = &hotkey.shift;

        if (modifier) {
            if (*modifier || hasKey)
                return std::nullopt; // repeated modifier, or modifier after the key
            *modifier = true;
        } else {
            if (hasKey)
                return std::nullopt; // two keys
            hotkey.key = KeyFromName(part);
            if (hotkey.key == 0)
                return std::nullopt;
            hasKey = true;
        }

        if (plus == std::wstring_view::npos)
            break;
        text.remove_prefix(plus + 1);
    }

    if (!hasKey)
        return std::nullopt;
    const bool anyModifier = hotkey.win || hotkey.ctrl || hotkey.alt || hotkey.shift;
    const bool onlyShift = hotkey.shift && !hotkey.win && !hotkey.ctrl && !hotkey.alt;
    if (!IsFunctionKey(hotkey.key) && (!anyModifier || onlyShift))
        return std::nullopt;
    return hotkey;
}

std::vector<std::wstring> HotkeyKeyNames()
{
    std::vector<std::wstring> names;
    for (wchar_t c = L'A'; c <= L'Z'; ++c)
        names.emplace_back(1, c);
    for (wchar_t c = L'0'; c <= L'9'; ++c)
        names.emplace_back(1, c);
    for (int f = 1; f <= 24; ++f)
        names.push_back(L"F" + std::to_wstring(f));
    for (const NamedKey& key : kNamedKeys)
        names.push_back(key.name);
    return names;
}

std::wstring FormatHotkey(const Hotkey& hotkey)
{
    std::wstring text;
    if (hotkey.win) text += L"Win+";
    if (hotkey.ctrl) text += L"Ctrl+";
    if (hotkey.alt) text += L"Alt+";
    if (hotkey.shift) text += L"Shift+";
    return text + NameFromKey(hotkey.key);
}

} // namespace popa
