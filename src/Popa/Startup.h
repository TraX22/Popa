// "Start with Windows": an entry in the current user's Run key.
#pragma once

#include <optional>
#include <string>

namespace popa {

// Makes Windows start `exePath` at sign-in, or stops it. Writes or removes
// the value "Popa" in HKCU\Software\Microsoft\Windows\CurrentVersion\Run,
// which needs no administrator rights. When disabling, a value that points
// to another Popa.exe (another portable copy) is left alone.
// Returns false when the registry could not be changed.
bool ApplyRunAtStartup(bool enabled, const std::wstring& exePath);

// The command currently registered, if any.
std::optional<std::wstring> RunAtStartupCommand();

} // namespace popa
