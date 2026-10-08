#include "Popa/XmlFile.h"

#include <windows.h>
#include <shlwapi.h>
#include <xmllite.h>

#include <vector>

namespace popa {

// XmlLite is a "pull" parser: instead of building a document in memory,
// it walks the file one node at a time. Each Read() call moves to the next
// node (start of an element, text, end of an element...), and the program
// builds whatever it needs. Here: a tree of XmlElement, kept with a stack
// of the elements that are open at the current point of the file.
std::optional<XmlElement> ReadXmlFile(const std::wstring& path)
{
    IStream* stream = nullptr;
    IXmlReader* reader = nullptr;
    std::optional<XmlElement> result;

    // XmlLite reads from an IStream, the COM interface for a sequence of
    // bytes. SHCreateStreamOnFileEx opens a file as one.
    HRESULT hr = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_WRITE, FILE_ATTRIBUTE_NORMAL,
                                        FALSE, nullptr, &stream);
    if (SUCCEEDED(hr))
        hr = CreateXmlReader(IID_PPV_ARGS(&reader), nullptr);
    // Document type definitions are refused (the default, set explicitly
    // here): they are not needed and are a classic attack vector of XML
    // parsers ("billion laughs", external entities).
    if (SUCCEEDED(hr))
        hr = reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
    if (SUCCEEDED(hr))
        hr = reader->SetInput(stream);

    std::vector<XmlElement> open; // open[0] is the root while reading
    XmlElement root;
    bool hasRoot = false;
    XmlNodeType nodeType = XmlNodeType_None;

    // Read returns S_OK for each node, S_FALSE at the end of the file and
    // an error code when the XML is broken.
    while (SUCCEEDED(hr) && (hr = reader->Read(&nodeType)) == S_OK) {
        switch (nodeType) {
        case XmlNodeType_Element: {
            XmlElement element;
            const wchar_t* text = nullptr;
            reader->GetLocalName(&text, nullptr);
            element.name = text;

            // Attributes are visited by moving the reader onto each one.
            for (HRESULT a = reader->MoveToFirstAttribute(); a == S_OK; a = reader->MoveToNextAttribute()) {
                const wchar_t* name = nullptr;
                const wchar_t* value = nullptr;
                reader->GetLocalName(&name, nullptr);
                reader->GetValue(&value, nullptr); // entities such as &quot; are already decoded
                element.attributes.emplace_back(name, value);
            }
            reader->MoveToElement();

            // <item .../> has no end tag, so it is complete right away.
            if (reader->IsEmptyElement()) {
                if (open.empty()) {
                    root = std::move(element);
                    hasRoot = true;
                } else {
                    open.back().children.push_back(std::move(element));
                }
            } else {
                open.push_back(std::move(element));
            }
            break;
        }

        case XmlNodeType_Text:
        case XmlNodeType_CDATA:
            if (!open.empty()) {
                const wchar_t* value = nullptr;
                reader->GetValue(&value, nullptr);
                open.back().text += value;
            }
            break;

        case XmlNodeType_EndElement:
            if (!open.empty()) {
                XmlElement finished = std::move(open.back());
                open.pop_back();
                if (open.empty()) {
                    root = std::move(finished);
                    hasRoot = true;
                } else {
                    open.back().children.push_back(std::move(finished));
                }
            }
            break;

        default:
            break; // declaration, comments, whitespace between elements
        }
    }

    if (SUCCEEDED(hr) && hasRoot && open.empty())
        result = std::move(root);

    if (reader)
        reader->Release();
    if (stream)
        stream->Release();
    return result;
}

} // namespace popa
