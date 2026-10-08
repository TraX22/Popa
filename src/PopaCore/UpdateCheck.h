// Understanding GitHub's answer about the latest release, and the safety
// rules for downloading it. No network code here: the application does the
// HTTP requests (Updater, with WinHTTP) and uses these functions to decide.
#pragma once

#include "PopaCore/Version.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace popa {

struct ReleaseInfo {
    Version version;
    std::wstring tag;         // as published, e.g. "v1.2.0"
    std::wstring notes;       // release description (Markdown text)
    std::wstring pageUrl;     // the release page on github.com
    std::wstring downloadUrl; // Popa.exe of the release; empty when missing
    std::wstring sha256;      // expected SHA-256 of Popa.exe in lowercase hex; empty when GitHub gives none
    std::uint64_t size = 0;   // expected size in bytes; 0 when unknown
};

// Reads the JSON of https://api.github.com/repos/<owner>/<repo>/releases/latest.
// No value when it is not a usable release: invalid JSON, a tag that is not
// a version number, or a draft / pre-release.
std::optional<ReleaseInfo> ParseLatestRelease(std::string_view json);

// True when the release is newer than the running version.
bool IsNewer(const ReleaseInfo& release, const Version& current);

// Downloads are only accepted over HTTPS from github.com and the domains
// GitHub uses to serve release files. Checked for the first address and
// for every redirect.
bool IsAllowedDownloadUrl(std::wstring_view url);

// The only header Popa sends: "Popa/1.2.0" (GitHub requires a User-Agent).
std::wstring UserAgent(const Version& current);

// Checks that downloaded bytes are a 64-bit Windows program: the "MZ" and
// "PE" signatures and the x64 machine type. Guards against saving an HTML
// error page or a truncated file as Popa.exe.
bool LooksLikeExecutable(std::string_view bytes);

// SHA-256 of the bytes in lowercase hexadecimal (with Windows' BCrypt).
std::wstring Sha256Hex(std::string_view bytes);

} // namespace popa
