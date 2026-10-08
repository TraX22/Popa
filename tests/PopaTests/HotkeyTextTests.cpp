// Tests for converting hotkey text such as "Win+Z" to keys (HotkeyText.h).

#include "TestUtil.h"

#include "PopaCore/HotkeyText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

TEST_CLASS(HotkeyTextTests)
{
public:
    TEST_METHOD(DefaultHotkey)
    {
        const auto hotkey = ParseHotkey(L"Win+Z");
        Assert::IsTrue(hotkey.has_value());
        Assert::IsTrue(hotkey->win);
        Assert::IsFalse(hotkey->ctrl || hotkey->alt || hotkey->shift);
        Assert::AreEqual(static_cast<unsigned>('Z'), hotkey->key);
    }

    TEST_METHOD(ModifiersKeysAndSpacing)
    {
        const auto hotkey = ParseHotkey(L" ctrl + ALT + space ");
        Assert::IsTrue(hotkey.has_value());
        Assert::IsTrue(hotkey->ctrl && hotkey->alt);
        Assert::IsFalse(hotkey->win || hotkey->shift);
        Assert::AreEqual(static_cast<unsigned>(VK_SPACE), hotkey->key);

        Assert::AreEqual(static_cast<unsigned>(VK_F12), ParseHotkey(L"Control+Shift+F12")->key);
        Assert::AreEqual(static_cast<unsigned>('7'), ParseHotkey(L"Alt+7")->key);
        Assert::AreEqual(static_cast<unsigned>(VK_NEXT), ParseHotkey(L"Win+PageDown")->key);
    }

    TEST_METHOD(FunctionKeysMayStandAlone)
    {
        Assert::AreEqual(static_cast<unsigned>(VK_F1), ParseHotkey(L"F1")->key);
        Assert::AreEqual(static_cast<unsigned>(VK_F24), ParseHotkey(L"f24")->key);
    }

    TEST_METHOD(InvalidTextIsRejected)
    {
        for (const wchar_t* text : {L"", L"Z", L"Win", L"Win+", L"+Z", L"Win++Z", L"Win+Win+Z", L"Win+Z+X",
                                    L"Z+Win", L"Win+Hyper", L"Win+F25", L"Win+F0", L"Win+Fx", L"Win+ZZ",
                                    L"Space", L"Shift+A", L"Win+Ñ"}) {
            Assert::IsFalse(ParseHotkey(text).has_value(), text);
        }
    }

    TEST_METHOD(FormatIsCanonical)
    {
        Assert::AreEqual(std::wstring(L"Win+Z"), FormatHotkey(*ParseHotkey(L"win+z")));
        Assert::AreEqual(std::wstring(L"Win+Ctrl+Alt+Shift+Space"),
                         FormatHotkey(*ParseHotkey(L"Shift+Alt+Ctrl+Win+Space")));
        Assert::AreEqual(std::wstring(L"Ctrl+F5"), FormatHotkey(*ParseHotkey(L"control+f5")));
    }

    TEST_METHOD(EveryListedKeyIsAccepted)
    {
        // The options dialog offers exactly this list; each one must work
        // and come back with the same name.
        const auto names = HotkeyKeyNames();
        Assert::AreEqual<size_t>(26 + 10 + 24 + 16, names.size());
        for (const std::wstring& name : names) {
            const auto hotkey = ParseHotkey(L"Ctrl+" + name);
            Assert::IsTrue(hotkey.has_value(), name.c_str());
            Assert::AreEqual(L"Ctrl+" + name, FormatHotkey(*hotkey));
        }
    }

    TEST_METHOD(FormatThenParseGivesTheSameHotkey)
    {
        for (const wchar_t* text : {L"Win+Z", L"Ctrl+Alt+Delete", L"Alt+Enter", L"Win+Shift+Left", L"F9",
                                    L"Ctrl+Pause", L"Alt+Esc", L"Ctrl+Backspace", L"Win+Tab"}) {
            const auto hotkey = ParseHotkey(text);
            Assert::IsTrue(hotkey.has_value(), text);
            Assert::IsTrue(hotkey == ParseHotkey(FormatHotkey(*hotkey)), text);
        }
    }
};

} // namespace popa::tests
