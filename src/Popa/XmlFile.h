// Reading an XML file into the XmlElement tree used by the menu importer,
// with XmlLite (the small XML reader built into Windows).
#pragma once

#include "PopaCore/MenuImporter.h"

#include <optional>
#include <string>

namespace popa {

// Reads the whole file. The encoding (UTF-16, UTF-8...) is
// detected by XmlLite from the byte order mark and the XML declaration.
// No value when the file cannot be opened or is not well-formed XML.
// COM must be initialized on the thread.
std::optional<XmlElement> ReadXmlFile(const std::wstring& path);

} // namespace popa
