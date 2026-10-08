#include "PopaCore/Version.h"

namespace popa {

std::wstring Version::ToString() const
{
    std::wstring text = std::to_wstring(parts[0]) + L"." + std::to_wstring(parts[1]) + L"." +
                        std::to_wstring(parts[2]);
    if (parts[3] != 0)
        text += L"." + std::to_wstring(parts[3]);
    return text;
}

std::optional<Version> ParseVersion(std::wstring_view text)
{
    if (!text.empty() && (text.front() == L'v' || text.front() == L'V'))
        text.remove_prefix(1);
    if (text.empty())
        return std::nullopt;

    Version version;
    size_t part = 0;
    size_t digits = 0; // digits seen in the current part
    unsigned value = 0;

    // A small hand-written scanner: digits accumulate into the current part,
    // a dot closes it. Any other character, an empty part ("1..2"), a fifth
    // part or a value over 65535 rejects the whole text.
    for (wchar_t c : text) {
        if (c >= L'0' && c <= L'9') {
            value = value * 10 + static_cast<unsigned>(c - L'0');
            if (value > 65535)
                return std::nullopt;
            ++digits;
        } else if (c == L'.') {
            if (digits == 0 || part == 3)
                return std::nullopt;
            version.parts[part++] = value;
            value = 0;
            digits = 0;
        } else {
            return std::nullopt;
        }
    }
    if (digits == 0) // trailing dot: "1.2."
        return std::nullopt;
    version.parts[part] = value;
    return version;
}

} // namespace popa
