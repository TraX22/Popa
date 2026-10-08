// Helpers shared by the test files.
#pragma once

#include "CppUnitTest.h"
#include "PopaCore/ConfigStore.h"

#include <windows.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

namespace popa::tests {

// A temporary folder that exists for the lifetime of the object and is
// deleted with everything inside when it goes out of scope (RAII: the
// destructor does the cleanup even when an assertion fails and throws).
class TempDir {
public:
    TempDir()
    {
        static std::atomic<unsigned> counter{0};
        path_ = std::filesystem::temp_directory_path() /
                (L"PopaTests-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                 std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(counter++));
        std::filesystem::create_directories(path_);
    }

    ~TempDir()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

inline void WriteBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

inline std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

} // namespace popa::tests

// Assert::AreEqual needs to print values when a comparison fails. The
// framework knows built-in types and std::wstring; for our own types we
// teach it with a ToString specialization in this exact namespace.
namespace Microsoft::VisualStudio::CppUnitTestFramework {

template <>
inline std::wstring ToString<popa::LoadStatus>(const popa::LoadStatus& status)
{
    switch (status) {
    case popa::LoadStatus::Ok: return L"Ok";
    case popa::LoadStatus::Missing: return L"Missing";
    case popa::LoadStatus::Corrupt: return L"Corrupt";
    case popa::LoadStatus::Unreadable: return L"Unreadable";
    }
    return L"?";
}

template <>
inline std::wstring ToString<popa::Theme>(const popa::Theme& theme)
{
    switch (theme) {
    case popa::Theme::System: return L"System";
    case popa::Theme::Light: return L"Light";
    case popa::Theme::Dark: return L"Dark";
    }
    return L"?";
}

template <>
inline std::wstring ToString<popa::ItemType>(const popa::ItemType& type)
{
    switch (type) {
    case popa::ItemType::Item: return L"Item";
    case popa::ItemType::Menu: return L"Menu";
    case popa::ItemType::Separator: return L"Separator";
    }
    return L"?";
}

} // namespace Microsoft::VisualStudio::CppUnitTestFramework
