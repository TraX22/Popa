// Tests for the English/Spanish text table (Localization.h).

#include "TestUtil.h"

#include "PopaCore/Localization.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

TEST_CLASS(LocalizationTests)
{
public:
    TEST_METHOD(EveryTextHasBothLanguages)
    {
        for (size_t i = 0; i < static_cast<size_t>(TextId::Count); ++i) {
            const auto id = static_cast<TextId>(i);
            const std::wstring label = L"TextId #" + std::to_wstring(i);
            Assert::IsTrue(*Tr(id, Language::English) != L'\0', label.c_str());
            Assert::IsTrue(*Tr(id, Language::Spanish) != L'\0', label.c_str());
        }
    }

    TEST_METHOD(RowsFollowTheEnumOrder)
    {
        // Spot checks: a row shifted by one would break these.
        Assert::AreEqual(L"Popa", Tr(TextId::AppName, Language::English));
        Assert::AreEqual(L"Abrir editor", Tr(TextId::TrayOpenEditor, Language::Spanish));
        Assert::AreEqual(L"Exit", Tr(TextId::TrayExit, Language::English));
        Assert::AreEqual(L"(vacío)", Tr(TextId::MenuEmpty, Language::Spanish));
    }

    TEST_METHOD(PlaceholdersMatchBetweenLanguages)
    {
        // A translation that forgets "{0}" would silently drop information.
        for (size_t i = 0; i < static_cast<size_t>(TextId::Count); ++i) {
            const auto id = static_cast<TextId>(i);
            const std::wstring english = Tr(id, Language::English);
            const std::wstring spanish = Tr(id, Language::Spanish);
            for (const wchar_t* placeholder : {L"{0}", L"{1}", L"{2}"}) {
                const bool inEnglish = english.find(placeholder) != std::wstring::npos;
                const bool inSpanish = spanish.find(placeholder) != std::wstring::npos;
                Assert::AreEqual(inEnglish, inSpanish, english.c_str());
            }
        }
    }

    TEST_METHOD(SpanishVariantsAreSpanish)
    {
        Assert::IsTrue(Language::Spanish == LanguageFromLangId(0x0C0A)); // es-ES
        Assert::IsTrue(Language::Spanish == LanguageFromLangId(0x2C0A)); // es-AR
        Assert::IsTrue(Language::Spanish == LanguageFromLangId(0x080A)); // es-MX
        Assert::IsTrue(Language::English == LanguageFromLangId(0x0409)); // en-US
        Assert::IsTrue(Language::English == LanguageFromLangId(0x0407)); // de-DE
    }

    TEST_METHOD(LanguageOption)
    {
        // A fixed choice ignores the Windows language; "system" follows it.
        Assert::IsTrue(Language::English == ResolveLanguage(LanguageSetting::English, 0x2C0A));
        Assert::IsTrue(Language::Spanish == ResolveLanguage(LanguageSetting::Spanish, 0x0409));
        Assert::IsTrue(Language::Spanish == ResolveLanguage(LanguageSetting::System, 0x2C0A));
        Assert::IsTrue(Language::English == ResolveLanguage(LanguageSetting::System, 0x0409));
    }

    TEST_METHOD(CurrentLanguageIsUsed)
    {
        const Language saved = CurrentLanguage();
        SetLanguage(Language::Spanish);
        Assert::AreEqual(L"Salir", Tr(TextId::TrayExit));
        SetLanguage(Language::English);
        Assert::AreEqual(L"Exit", Tr(TextId::TrayExit));
        SetLanguage(saved);
    }

    TEST_METHOD(FormatReplacesPlaceholders)
    {
        Assert::AreEqual(std::wstring(L"a=1, b=2, a=1"), Format(L"a={0}, b={1}, a={0}", {L"1", L"2"}));
        Assert::AreEqual(std::wstring(L"missing {3} stays"), Format(L"missing {3} stays", {L"x"}));
        Assert::AreEqual(std::wstring(L"{x} {"), Format(L"{x} {", {L"y"}));
        Assert::AreEqual(std::wstring(L"C:\\Popa\\menu.json.bak"), Format(L"{0}", {L"C:\\Popa\\menu.json.bak"}));
    }
};

} // namespace popa::tests
