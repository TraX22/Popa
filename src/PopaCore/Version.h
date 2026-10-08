// Version numbers such as "v1.2.0" (a GitHub release tag) or "1.2.0.0" (the
// version resource of Popa.exe), and their comparison.
#pragma once

#include <array>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace popa {

struct Version {
    // major, minor, patch, build. Missing parts are 0, so "1.2" equals "1.2.0.0".
    std::array<unsigned, 4> parts{};

    // C++20 "spaceship" operator: the compiler derives <, >, <=, >= and ==
    // from it. std::array compares element by element, left to right, which
    // is exactly how version numbers compare.
    auto operator<=>(const Version&) const = default;

    // "1.2.0", with ".build" added only when the build number is not 0.
    std::wstring ToString() const;
};

// Parses "1", "1.2", "1.2.3" or "1.2.3.4", optionally prefixed by "v" or "V".
// Each part is 0..65535, the range a Windows version resource can hold.
// Anything else ("1.2-beta", "latest", "1..2") gives no value: an unknown
// format must never be mistaken for a newer version.
std::optional<Version> ParseVersion(std::wstring_view text);

} // namespace popa
