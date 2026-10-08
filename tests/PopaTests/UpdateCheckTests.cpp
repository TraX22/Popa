// Tests for reading GitHub's release answer and the download safety rules
// (UpdateCheck.h, Repository.h).

#include "TestUtil.h"

#include "PopaCore/Repository.h"
#include "PopaCore/UpdateCheck.h"

#include <cstring>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace popa::tests {

namespace {

// Shortened version of a real answer of the GitHub API.
const char* kRelease = R"({
  "url": "https://api.github.com/repos/TraX22/Popa/releases/1",
  "html_url": "https://github.com/TraX22/Popa/releases/tag/v1.2.0",
  "tag_name": "v1.2.0",
  "name": "Popa 1.2.0",
  "draft": false,
  "prerelease": false,
  "body": "- Faster menu\r\n- Fixes ñ",
  "assets": [
    {
      "name": "Popa-1.2.0-source.zip",
      "size": 100,
      "browser_download_url": "https://github.com/TraX22/Popa/releases/download/v1.2.0/source.zip"
    },
    {
      "name": "Popa.exe",
      "size": 612352,
      "digest": "sha256:0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef",
      "browser_download_url": "https://github.com/TraX22/Popa/releases/download/v1.2.0/Popa.exe"
    }
  ]
})";

Version V(const wchar_t* text)
{
    return *ParseVersion(text);
}

// The smallest byte sequence LooksLikeExecutable accepts.
std::string MinimalPe(std::uint16_t machine)
{
    std::string bytes(0x80, '\0');
    bytes[0] = 'M';
    bytes[1] = 'Z';
    const std::uint32_t peOffset = 0x40;
    std::memcpy(bytes.data() + 0x3C, &peOffset, 4);
    std::memcpy(bytes.data() + peOffset, "PE\0\0", 4);
    std::memcpy(bytes.data() + peOffset + 4, &machine, 2);
    return bytes;
}

} // namespace

TEST_CLASS(UpdateCheckTests)
{
public:
    TEST_METHOD(RepositoryAddresses)
    {
        Assert::AreEqual(std::wstring(L"https://api.github.com/repos/TraX22/Popa/releases/latest"),
                         LatestReleaseApiUrl());
        Assert::AreEqual(std::wstring(L"https://github.com/TraX22/Popa"), RepositoryUrl());
    }

    TEST_METHOD(ReadsTheRelease)
    {
        const auto release = ParseLatestRelease(kRelease);
        Assert::IsTrue(release.has_value());
        Assert::AreEqual(std::wstring(L"v1.2.0"), release->tag);
        Assert::IsTrue(V(L"1.2.0") == release->version);
        Assert::AreEqual(std::wstring(L"- Faster menu\r\n- Fixes ñ"), release->notes);
        Assert::AreEqual(std::wstring(L"https://github.com/TraX22/Popa/releases/tag/v1.2.0"), release->pageUrl);
        Assert::AreEqual(std::wstring(L"https://github.com/TraX22/Popa/releases/download/v1.2.0/Popa.exe"),
                         release->downloadUrl);
        Assert::AreEqual(std::wstring(L"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"),
                         release->sha256); // lower-cased
        Assert::AreEqual<std::uint64_t>(612352, release->size);
    }

    TEST_METHOD(ReleaseWithoutTheExecutableHasNoDownload)
    {
        const auto release = ParseLatestRelease(R"({"tag_name": "1.3", "assets": [{"name": "notes.txt"}]})");
        Assert::IsTrue(release.has_value());
        Assert::IsTrue(release->downloadUrl.empty());
        Assert::IsTrue(release->sha256.empty());
    }

    TEST_METHOD(BadDigestIsIgnored)
    {
        const auto release = ParseLatestRelease(
            R"({"tag_name": "v2.0.0", "assets": [{"name": "popa.EXE", "digest": "sha256:xyz",
                "browser_download_url": "https://github.com/a"}]})");
        Assert::IsTrue(release.has_value());
        Assert::AreEqual(std::wstring(L"https://github.com/a"), release->downloadUrl); // name case ignored
        Assert::IsTrue(release->sha256.empty());
    }

    TEST_METHOD(UnusableAnswersAreRejected)
    {
        Assert::IsFalse(ParseLatestRelease("").has_value());
        Assert::IsFalse(ParseLatestRelease("<html>rate limited</html>").has_value());
        Assert::IsFalse(ParseLatestRelease(R"({"message": "Not Found"})").has_value());
        Assert::IsFalse(ParseLatestRelease(R"({"tag_name": "nightly"})").has_value());
        Assert::IsFalse(ParseLatestRelease(R"({"tag_name": "v9.0.0", "prerelease": true})").has_value());
        Assert::IsFalse(ParseLatestRelease(R"({"tag_name": "v9.0.0", "draft": true})").has_value());
    }

    TEST_METHOD(NewerComparison)
    {
        const auto release = ParseLatestRelease(kRelease);
        Assert::IsTrue(IsNewer(*release, V(L"1.1.9")));
        Assert::IsTrue(IsNewer(*release, V(L"0.9.0")));
        Assert::IsFalse(IsNewer(*release, V(L"1.2.0.0")));
        Assert::IsFalse(IsNewer(*release, V(L"1.10.0")));
    }

    TEST_METHOD(AllowedDownloadAddresses)
    {
        for (const wchar_t* url : {L"https://github.com/TraX22/Popa/releases/download/v1.2.0/Popa.exe",
                                   L"HTTPS://GitHub.com/x",
                                   L"https://objects.githubusercontent.com/github-production-release-asset/1?x=y",
                                   L"https://release-assets.githubusercontent.com/github-production-release-asset/1",
                                   L"https://github.com:443/x"}) {
            Assert::IsTrue(IsAllowedDownloadUrl(url), url);
        }
    }

    TEST_METHOD(RefusedDownloadAddresses)
    {
        for (const wchar_t* url : {L"http://github.com/x",                // not encrypted
                                   L"https://github.com.evil.example/x", // look-alike host
                                   L"https://evilgithub.com/x",
                                   L"https://github.com@evil.example/x", // user-info trick
                                   L"https://github.com:8443/x",         // other port
                                   L"https://raw.githubusercontent.com/x", L"https://example.com/github.com/x",
                                   L"ftp://github.com/x", L"https://", L"github.com/x"}) {
            Assert::IsFalse(IsAllowedDownloadUrl(url), url);
        }
    }

    TEST_METHOD(UserAgentHasTheVersion)
    {
        Assert::AreEqual(std::wstring(L"Popa/1.2.0"), UserAgent(V(L"1.2")));
    }

    TEST_METHOD(ExecutableCheck)
    {
        Assert::IsTrue(LooksLikeExecutable(MinimalPe(0x8664)));
        Assert::IsFalse(LooksLikeExecutable(MinimalPe(0x014C))); // 32-bit x86
        Assert::IsFalse(LooksLikeExecutable("<!DOCTYPE html><html>Not Found</html>"));
        Assert::IsFalse(LooksLikeExecutable("MZ"));
        std::string truncated = MinimalPe(0x8664);
        const std::uint32_t farAway = 0x7FFFFFFF;
        std::memcpy(truncated.data() + 0x3C, &farAway, 4);
        Assert::IsFalse(LooksLikeExecutable(truncated));
    }

    TEST_METHOD(Sha256KnownValues)
    {
        // Standard test vectors (FIPS 180-2).
        Assert::AreEqual(std::wstring(L"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),
                         Sha256Hex("abc"));
        Assert::AreEqual(std::wstring(L"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),
                         Sha256Hex(""));
    }
};

} // namespace popa::tests
