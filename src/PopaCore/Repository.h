// Where Popa is published. The GitHub user and repository name appear only
// here (section 10 of the design document); everything else builds on them.
#pragma once

#include <string>

namespace popa {

inline constexpr wchar_t kRepositoryOwner[] = L"TraX22";
inline constexpr wchar_t kRepositoryName[] = L"Popa";

// File name of the program in each GitHub release.
inline constexpr wchar_t kReleaseAssetName[] = L"Popa.exe";

// https://github.com/TraX22/Popa
inline std::wstring RepositoryUrl()
{
    return std::wstring(L"https://github.com/") + kRepositoryOwner + L"/" + kRepositoryName;
}

// The only address Popa ever contacts by itself (the update check).
inline std::wstring LatestReleaseApiUrl()
{
    return std::wstring(L"https://api.github.com/repos/") + kRepositoryOwner + L"/" + kRepositoryName +
           L"/releases/latest";
}

} // namespace popa
