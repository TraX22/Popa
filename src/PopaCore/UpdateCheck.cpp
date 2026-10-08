#include "PopaCore/UpdateCheck.h"

#include "PopaCore/Repository.h"
#include "PopaCore/Text.h"

#include <windows.h>
#include <bcrypt.h>

#include <nlohmann/json.hpp>

#include <cstring>
#include <cwctype>

// BCrypt lives in bcrypt.dll. This line asks the linker to add its import
// library to any program that uses this file (the app and the tests).
#pragma comment(lib, "bcrypt.lib")

namespace popa {

namespace {

using Json = nlohmann::json;

std::wstring Lower(std::wstring_view text)
{
    std::wstring result(text);
    for (wchar_t& c : result)
        c = static_cast<wchar_t>(std::towlower(c));
    return result;
}

std::wstring StringField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    return (it != object.end() && it->is_string()) ? Utf8ToWide(it->get_ref<const std::string&>()) : std::wstring();
}

bool BoolField(const Json& object, const char* key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() && it->get<bool>();
}

bool IsHexDigest(const std::wstring& text)
{
    if (text.size() != 64)
        return false;
    for (wchar_t c : text) {
        if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f')))
            return false;
    }
    return true;
}

} // namespace

std::optional<ReleaseInfo> ParseLatestRelease(std::string_view json)
{
    const Json root = Json::parse(json, nullptr, false);
    if (root.is_discarded() || !root.is_object())
        return std::nullopt;

    // /releases/latest never returns drafts or pre-releases, but checking
    // costs nothing and protects against a change on GitHub's side.
    if (BoolField(root, "draft") || BoolField(root, "prerelease"))
        return std::nullopt;

    ReleaseInfo release;
    release.tag = StringField(root, "tag_name");
    const auto version = ParseVersion(release.tag);
    if (!version)
        return std::nullopt;
    release.version = *version;
    release.notes = StringField(root, "body");
    release.pageUrl = StringField(root, "html_url");

    const auto assets = root.find("assets");
    if (assets != root.end() && assets->is_array()) {
        for (const Json& asset : *assets) {
            if (!asset.is_object() || Lower(StringField(asset, "name")) != Lower(kReleaseAssetName))
                continue;
            release.downloadUrl = StringField(asset, "browser_download_url");
            // Newer API answers include "digest": "sha256:<64 hex digits>".
            const std::wstring digest = Lower(StringField(asset, "digest"));
            if (digest.starts_with(L"sha256:") && IsHexDigest(digest.substr(7)))
                release.sha256 = digest.substr(7);
            const auto size = asset.find("size");
            if (size != asset.end() && size->is_number_unsigned())
                release.size = size->get<std::uint64_t>();
            break;
        }
    }
    return release;
}

bool IsNewer(const ReleaseInfo& release, const Version& current)
{
    return release.version > current;
}

bool IsAllowedDownloadUrl(std::wstring_view url)
{
    // Only https, compared without regard to case.
    constexpr std::wstring_view scheme = L"https://";
    if (url.size() <= scheme.size() || Lower(url.substr(0, scheme.size())) != scheme)
        return false;

    // The "authority" is what follows the scheme up to the first / ? or #.
    const std::wstring_view rest = url.substr(scheme.size());
    const std::wstring_view authority = rest.substr(0, rest.find_first_of(L"/?#"));

    // "https://github.com@evil.example/..." really goes to evil.example:
    // everything before an "@" is a user name. Such URLs are refused.
    if (authority.find(L'@') != std::wstring_view::npos)
        return false;

    std::wstring host = Lower(authority);
    const size_t colon = host.find(L':');
    if (colon != std::wstring::npos) {
        if (host.substr(colon) != L":443")
            return false;
        host.resize(colon);
    }

    // Exact names only: "github.com.evil.example" or "evilgithub.com" fail.
    for (const wchar_t* allowed : {L"github.com", L"objects.githubusercontent.com",
                                   L"release-assets.githubusercontent.com",
                                   L"github-releases.githubusercontent.com"}) {
        if (host == allowed)
            return true;
    }
    return false;
}

std::wstring UserAgent(const Version& current)
{
    return L"Popa/" + current.ToString();
}

bool LooksLikeExecutable(std::string_view bytes)
{
    // Every Windows program starts with the old DOS header ("MZ"). At
    // offset 0x3C it holds the position of the real "PE" header, which is
    // followed by the machine type (0x8664 = x64).
    if (bytes.size() < 0x40 || bytes[0] != 'M' || bytes[1] != 'Z')
        return false;
    std::uint32_t peOffset = 0;
    std::memcpy(&peOffset, bytes.data() + 0x3C, sizeof(peOffset));
    if (peOffset > bytes.size() - 6)
        return false;
    if (std::memcmp(bytes.data() + peOffset, "PE\0\0", 4) != 0)
        return false;
    std::uint16_t machine = 0;
    std::memcpy(&machine, bytes.data() + peOffset + 4, sizeof(machine));
    return machine == 0x8664;
}

std::wstring Sha256Hex(std::string_view bytes)
{
    // BCrypt is the Windows cryptography API (CNG). BCryptHash (Windows 10)
    // computes a hash in one call with an algorithm "pseudo-handle".
    unsigned char digest[32] = {};
    const NTSTATUS status =
        BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
                   reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data())), static_cast<ULONG>(bytes.size()),
                   digest, sizeof(digest));
    if (status != 0)
        return {};

    constexpr wchar_t kHex[] = L"0123456789abcdef";
    std::wstring text;
    for (unsigned char byte : digest) {
        text += kHex[byte >> 4];
        text += kHex[byte & 0x0F];
    }
    return text;
}

} // namespace popa
