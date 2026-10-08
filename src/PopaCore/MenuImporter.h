// Importing a menu from an XML configuration file.
//
// The importer works on a small, already-parsed element tree (XmlElement)
// instead of reading the file itself. Reading XML is done in the
// application with XmlLite, a Windows component; keeping it out of
// PopaCore lets the conversion rules be tested with trees built in code.
#pragma once

#include "PopaCore/Model.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace popa {

// One XML element: its name, attributes, the text directly inside it and
// its child elements. Comments and processing instructions are not kept.
struct XmlElement {
    std::wstring name;
    std::vector<std::pair<std::wstring, std::wstring>> attributes;
    std::wstring text;
    std::vector<XmlElement> children;

    // Value of an attribute, or null when absent.
    const std::wstring* Attribute(std::wstring_view attributeName) const;
    // First child element with that name, or null.
    const XmlElement* Child(std::wstring_view childName) const;
};

// Converts the root element of the configuration (or its <menu> element).
// Rules (section 5.9 of the design document):
//   <item issep="1">                    -> separator (its decorative name is dropped)
//   <item> with <item> children         -> submenu; its <command> is a placeholder, ignored
//   <item> with <command>command</...>  -> empty submenu (the format's placeholder)
//     and no children
//   other <item>                        -> entry; <command> becomes its command
//   icon="path:N"                       -> icon (quotes around the path removed)
//   no icon attribute                   -> automatic icon
//   ID, lastID, <settings>, <wins>      -> not imported
// No value when there is no <menu> element: not a menu configuration.
std::optional<std::vector<MenuItem>> ImportMenuXml(const XmlElement& root);

// Number of entries in a menu, counting the contents of submenus.
size_t CountEntries(const std::vector<MenuItem>& items);

} // namespace popa
