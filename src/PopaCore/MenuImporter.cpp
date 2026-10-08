#include "PopaCore/MenuImporter.h"

#include "PopaCore/IconRef.h"

namespace popa {

namespace {

// The format writes this word as the command of every submenu.
constexpr std::wstring_view kPlaceholderCommand = L"command";

std::wstring Trim(std::wstring_view text)
{
    const size_t first = text.find_first_not_of(L" \t\r\n");
    if (first == std::wstring_view::npos)
        return {};
    const size_t last = text.find_last_not_of(L" \t\r\n");
    return std::wstring(text.substr(first, last - first + 1));
}

std::optional<IconSpec> IconOf(const XmlElement& element)
{
    const std::wstring* icon = element.Attribute(L"icon");
    if (!icon)
        return std::nullopt;
    IconSpec spec = ParseIconRef(*icon);
    if (spec.path.empty())
        return std::nullopt;
    return spec;
}

std::vector<MenuItem> ConvertItems(const XmlElement& parent)
{
    std::vector<MenuItem> items;
    for (const XmlElement& element : parent.children) {
        if (element.name != L"item")
            continue; // <command> of a submenu, or anything unknown

        const std::wstring* isSeparator = element.Attribute(L"issep");
        if (isSeparator && *isSeparator == L"1") {
            items.push_back(MenuItem::MakeSeparator());
            continue;
        }

        const std::wstring* nameAttribute = element.Attribute(L"name");
        std::wstring name = nameAttribute ? *nameAttribute : std::wstring();
        const XmlElement* commandElement = element.Child(L"command");
        std::wstring command = commandElement ? Trim(commandElement->text) : std::wstring();

        const bool hasChildren = element.Child(L"item") != nullptr;
        if (hasChildren || command == kPlaceholderCommand) {
            items.push_back(MenuItem::MakeMenu(std::move(name), ConvertItems(element), IconOf(element)));
        } else {
            items.push_back(MenuItem::MakeItem(std::move(name), std::move(command), IconOf(element)));
        }
    }
    return items;
}

} // namespace

const std::wstring* XmlElement::Attribute(std::wstring_view attributeName) const
{
    for (const auto& [key, value] : attributes) {
        if (key == attributeName)
            return &value;
    }
    return nullptr;
}

const XmlElement* XmlElement::Child(std::wstring_view childName) const
{
    for (const XmlElement& child : children) {
        if (child.name == childName)
            return &child;
    }
    return nullptr;
}

std::optional<std::vector<MenuItem>> ImportMenuXml(const XmlElement& root)
{
    const XmlElement* menu = root.name == L"menu" ? &root : root.Child(L"menu");
    if (!menu)
        return std::nullopt;
    return ConvertItems(*menu);
}

size_t CountEntries(const std::vector<MenuItem>& items)
{
    size_t count = items.size();
    for (const MenuItem& item : items)
        count += CountEntries(item.children);
    return count;
}

} // namespace popa
