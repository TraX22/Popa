// Tests for version parsing and comparison (Version.h).

#include "TestUtil.h"

#include "PopaCore/Version.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

Version V(std::wstring_view text)
{
    const auto version = ParseVersion(text);
    Assert::IsTrue(version.has_value(), std::wstring(text).c_str());
    return *version;
}

} // namespace

TEST_CLASS(VersionTests)
{
public:
    TEST_METHOD(ParsesTagsAndResourceVersions)
    {
        Assert::IsTrue((std::array<unsigned, 4>{1, 2, 0, 0}) == V(L"v1.2.0").parts);
        Assert::IsTrue((std::array<unsigned, 4>{1, 2, 3, 4}) == V(L"1.2.3.4").parts);
        Assert::IsTrue((std::array<unsigned, 4>{2, 0, 0, 0}) == V(L"V2").parts);
        Assert::IsTrue((std::array<unsigned, 4>{0, 10, 0, 0}) == V(L"0.10").parts);
        Assert::IsTrue((std::array<unsigned, 4>{65535, 0, 0, 0}) == V(L"65535").parts);
    }

    TEST_METHOD(RejectsUnknownFormats)
    {
        for (const wchar_t* text : {L"", L"v", L"latest", L"1..2", L".1", L"1.", L"1.2.3.4.5",
                                    L"1.2-beta", L"1.2.0+build", L" 1.2", L"1.2 ", L"65536", L"v-1",
                                    L"99999999999999999999"}) {
            Assert::IsFalse(ParseVersion(text).has_value(), text);
        }
    }

    TEST_METHOD(ComparesPartByPart)
    {
        Assert::IsTrue(V(L"1.2.0") < V(L"1.10.0")); // numeric, not alphabetical
        Assert::IsTrue(V(L"1.9.9") < V(L"2.0.0"));
        Assert::IsTrue(V(L"v1.2.1") > V(L"1.2.0.0"));
        Assert::IsTrue(V(L"1.2.0.1") > V(L"1.2.0"));
    }

    TEST_METHOD(MissingPartsAreZero)
    {
        Assert::IsTrue(V(L"1.2") == V(L"1.2.0.0"));
        Assert::IsTrue(V(L"v1") == V(L"1.0.0"));
    }

    TEST_METHOD(ToStringShowsBuildOnlyWhenNeeded)
    {
        Assert::AreEqual(std::wstring(L"1.2.0"), V(L"v1.2").ToString());
        Assert::AreEqual(std::wstring(L"1.2.3.4"), V(L"1.2.3.4").ToString());
    }
};

} // namespace popa::tests
