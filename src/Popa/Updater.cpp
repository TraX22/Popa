#include "Popa/Updater.h"

#include "Popa/UiUtil.h"
#include "Popa/version.h"
#include "PopaCore/Localization.h"
#include "PopaCore/Repository.h"

#include <winhttp.h>

#include <fstream>
#include <thread>
#include <vector>

namespace popa {

namespace {

constexpr int kMaxRedirects = 5;
constexpr size_t kMaxApiBytes = 1024 * 1024;         // the JSON answer is a few KB
constexpr size_t kMaxDownloadBytes = 50 * 1024 * 1024; // Popa.exe is well under 5 MB

// --- WinHTTP ----------------------------------------------------------------------------------
//
// WinHTTP works with three nested handles: a *session* (settings shared by
// all requests, such as the User-Agent and timeouts), a *connection* to one
// server, and a *request* on that connection. Each must be closed with
// WinHttpCloseHandle; this small class does it automatically (RAII).
class Handle {
public:
    explicit Handle(HINTERNET handle = nullptr) : handle_(handle) {}
    ~Handle()
    {
        if (handle_)
            WinHttpCloseHandle(handle_);
    }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    HINTERNET get() const { return handle_; }
    explicit operator bool() const { return handle_ != nullptr; }

private:
    HINTERNET handle_;
};

struct HttpResponse {
    bool ok = false; // a response arrived (any status)
    DWORD status = 0;
    std::string body;
    std::wstring location; // target of a redirect (3xx)
    bool tooLarge = false;
};

// One HTTPS GET request, without following redirects.
HttpResponse Request(HINTERNET session, const std::wstring& url, size_t maxBytes)
{
    HttpResponse response;

    // WinHttpCrackUrl splits a URL into its parts. Setting the lengths to
    // -1 asks it to point into the original string instead of copying.
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return response;
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    const std::wstring path = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
                              std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength);

    Handle connection(WinHttpConnect(session, host.c_str(), parts.nPort, 0));
    if (!connection)
        return response;
    // WINHTTP_FLAG_SECURE: HTTPS. WinHTTP checks the server certificate
    // with Windows' certificate store and fails on any problem.
    Handle request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                      WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request)
        return response;

    // Redirects are followed by hand (see Get), so every address can be
    // checked before connecting. No cookies are kept or sent.
    DWORD option = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &option, sizeof(option));
    option = WINHTTP_DISABLE_COOKIES;
    WinHttpSetOption(request.get(), WINHTTP_OPTION_DISABLE_FEATURE, &option, sizeof(option));

    // No extra headers: the only one of ours is the session's User-Agent.
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr))
        return response;

    DWORD size = sizeof(response.status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &response.status, &size, WINHTTP_NO_HEADER_INDEX);
    response.ok = true;

    if (response.status >= 300 && response.status < 400) {
        // First call: get the size; second: read the header.
        size = 0;
        WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &size,
                            WINHTTP_NO_HEADER_INDEX);
        if (size > 0) {
            std::wstring location(size / sizeof(wchar_t), L'\0');
            if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                    location.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
                location.resize(size / sizeof(wchar_t));
                response.location = location;
            }
        }
        return response;
    }

    // The body arrives in pieces: ask how much is available, read it,
    // repeat until 0. A size limit protects against endless answers.
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request.get(), &available)) {
            response.ok = false;
            return response;
        }
        if (available == 0)
            break;
        if (response.body.size() + available > maxBytes) {
            response.tooLarge = true;
            response.ok = false;
            return response;
        }
        const size_t offset = response.body.size();
        response.body.resize(offset + available);
        DWORD read = 0;
        if (!WinHttpReadData(request.get(), response.body.data() + offset, available, &read)) {
            response.ok = false;
            return response;
        }
        response.body.resize(offset + read);
    }
    return response;
}

// GET with redirects followed by hand. `allowed` is checked for the first
// address and for each address GitHub redirects to.
template <typename Allowed>
HttpResponse Get(std::wstring url, size_t maxBytes, DWORD receiveTimeoutMs, Allowed allowed, bool& refused)
{
    refused = false;
    // WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY uses the system proxy settings,
    // like the browser does, when there is a proxy.
    Handle session(WinHttpOpen(UserAgent(CurrentVersion()).c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session)
        return {};
    // Resolve, connect, send, receive (milliseconds): without internet the
    // check gives up after a few seconds instead of hanging.
    WinHttpSetTimeouts(session.get(), 10000, 10000, 15000, static_cast<int>(receiveTimeoutMs));

    for (int hop = 0; hop <= kMaxRedirects; ++hop) {
        if (!allowed(url)) {
            refused = true;
            return {};
        }
        HttpResponse response = Request(session.get(), url, maxBytes);
        if (!response.ok || response.status < 300 || response.status >= 400)
            return response;
        if (response.location.empty())
            return response;
        url = response.location;
    }
    return {};
}

bool IsGitHubApi(const std::wstring& url)
{
    return url.starts_with(L"https://api.github.com/");
}

// Hands the result to the UI thread. When the window no longer exists
// (Popa is closing) the message cannot be delivered: the unique_ptr keeps
// ownership and frees the result.
template <typename T>
void Post(HWND window, UINT message, std::unique_ptr<T>& result)
{
    if (PostMessageW(window, message, 0, reinterpret_cast<LPARAM>(result.get())))
        result.release(); // now owned by the message (see TakeResult)
}

} // namespace

Version CurrentVersion()
{
    // The version resource is read from the file of the running program.
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
    if (size > 0) {
        std::vector<BYTE> data(size);
        VS_FIXEDFILEINFO* info = nullptr;
        UINT length = 0;
        // "\\" asks for the fixed (numeric) part of the version information.
        if (GetFileVersionInfoW(path, 0, size, data.data()) &&
            VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) && info) {
            Version version;
            version.parts = {HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                             HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS)};
            return version;
        }
    }
    Version fallback;
    fallback.parts = {POPA_VERSION_MAJOR, POPA_VERSION_MINOR, POPA_VERSION_PATCH, 0};
    return fallback;
}

void StartUpdateCheck(HWND window, UINT message, bool manual)
{
    // A detached thread: it owns copies of everything it uses and talks to
    // the UI only through PostMessage, so it never touches the App object.
    std::thread([window, message, manual] {
        auto result = std::make_unique<CheckResult>();
        result->manual = manual;

        bool refused = false;
        const HttpResponse response = Get(LatestReleaseApiUrl(), kMaxApiBytes, 15000, IsGitHubApi, refused);
        if (response.ok && response.status == 200) {
            if (const auto release = ParseLatestRelease(response.body)) {
                result->release = *release;
                result->outcome = IsNewer(*release, CurrentVersion()) ? CheckOutcome::NewVersion
                                                                       : CheckOutcome::UpToDate;
            }
        } else if (response.ok && response.status == 404) {
            result->outcome = CheckOutcome::NoReleases; // repository without releases
        }
        Post(window, message, result);
    }).detach();
}

void StartDownload(HWND window, UINT message, const ReleaseInfo& release, const std::wstring& exePath)
{
    std::thread([window, message, release, exePath] {
        auto result = std::make_unique<DownloadResult>();
        bool refused = false;
        const HttpResponse response =
            Get(release.downloadUrl, kMaxDownloadBytes, 60000,
                [](const std::wstring& url) { return IsAllowedDownloadUrl(url); }, refused);

        if (refused) {
            result->error = Tr(TextId::UpdateErrorAddress);
        } else if (!response.ok) {
            result->error = Tr(TextId::UpdateErrorConnection);
        } else if (response.status != 200) {
            result->error = Format(Tr(TextId::UpdateErrorHttp), {std::to_wstring(response.status)});
        } else if ((release.size != 0 && response.body.size() != release.size) ||
                   !LooksLikeExecutable(response.body)) {
            result->error = Tr(TextId::UpdateErrorInvalid);
        } else if (!release.sha256.empty() && Sha256Hex(response.body) != release.sha256) {
            result->error = Tr(TextId::UpdateErrorHash);
        } else {
            // Saved next to Popa.exe: the later rename stays on the same
            // drive, which makes it instant and atomic.
            const size_t slash = exePath.find_last_of(L"\\/");
            const std::wstring path = exePath.substr(0, slash + 1) + L"Popa.new.exe";
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            file.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
            file.close();
            if (file) {
                result->ok = true;
                result->filePath = path;
            } else {
                DeleteFileW(path.c_str());
                result->error = Format(Tr(TextId::UpdateErrorWrite), {path});
            }
        }
        Post(window, message, result);
    }).detach();
}

namespace {

// "D:\Apps\Popa.exe" -> "D:\Apps\Popa.old.exe"
std::wstring OldPathFor(const std::wstring& exePath)
{
    const size_t dot = exePath.rfind(L'.');
    return (dot == std::wstring::npos ? exePath : exePath.substr(0, dot)) + L".old.exe";
}

} // namespace

bool InstallUpdate(const std::wstring& newFile, const std::wstring& exePath, std::wstring& error)
{
    const std::wstring oldPath = OldPathFor(exePath);
    DeleteFileW(oldPath.c_str()); // leftover of an earlier update, if any

    // Step 1: move the running program aside. Windows keeps the file open
    // while the program runs, which blocks deleting or overwriting it, but
    // renaming within the same folder is allowed.
    if (!MoveFileExW(exePath.c_str(), oldPath.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        error = SystemErrorText(GetLastError());
        DeleteFileW(newFile.c_str());
        return false;
    }
    // Step 2: the new file takes the original name.
    if (!MoveFileExW(newFile.c_str(), exePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = SystemErrorText(GetLastError());
        MoveFileExW(oldPath.c_str(), exePath.c_str(), MOVEFILE_REPLACE_EXISTING); // put the original back
        DeleteFileW(newFile.c_str());
        return false;
    }
    return true;
}

bool RestartAfterUpdate(const std::wstring& exePath)
{
    // CreateProcessW may modify the command line buffer, so it must be a
    // writable copy, not a string literal.
    std::wstring commandLine =
        L"\"" + exePath + L"\" " + kAfterUpdateSwitch + std::to_wstring(GetCurrentProcessId());
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                        &process))
        return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

void DeleteOldExecutable(const std::wstring& exePath)
{
    DeleteFileW(OldPathFor(exePath).c_str());
}

} // namespace popa
