#include "Popa/IconCache.h"

#include "PopaCore/CommandLine.h"
#include "PopaCore/IconRef.h"

#include <commctrl.h>
#include <commoncontrols.h>
#include <shellapi.h>
#include <wincodec.h>

#include <cwctype>
#include <vector>

namespace popa {

namespace {

std::wstring LowerExtension(const std::wstring& path)
{
    const size_t dot = path.rfind(L'.');
    const size_t slash = path.find_last_of(L"\\/");
    if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
        return {};
    std::wstring extension = path.substr(dot);
    for (wchar_t& c : extension)
        c = static_cast<wchar_t>(std::towlower(c));
    return extension;
}

// --- Icons stored inside .exe, .dll and .ico files ------------------------------
//
// PrivateExtractIconsW reads icon number `index` of a file and builds it at
// exactly `size` pixels, choosing the best image of the multi-size icon.
// (ExtractIconEx only gives the two system sizes, 16 and 32 at 100 %.)
// A negative index means "the icon whose resource ID is -index", the same
// convention Windows uses in "file,-123" icon locations.
HICON ExtractFromFile(const std::wstring& path, int index, int size)
{
    HICON icon = nullptr;
    UINT iconId = 0;
    const UINT count = PrivateExtractIconsW(path.c_str(), index, size, size, &icon, &iconId, 1, LR_DEFAULTCOLOR);
    if (count == 0 || count == 0xFFFFFFFF)
        return nullptr;
    return icon;
}

// --- PNG files, through WIC --------------------------------------------------------
//
// WIC (Windows Imaging Component) decodes image formats; it is a COM API.
// Smart pointers are not used here to keep the COM steps visible: every
// interface obtained is released at the end with Release().
HICON IconFromPng(const std::wstring& path, int size)
{
    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    HICON icon = nullptr;

    // CoCreateInstance creates a COM object by class ID and returns the
    // requested interface; IID_PPV_ARGS fills in the interface ID for us.
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(hr))
        hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr))
        hr = decoder->GetFrame(0, &frame);
    if (SUCCEEDED(hr))
        hr = factory->CreateBitmapScaler(&scaler);
    if (SUCCEEDED(hr))
        hr = scaler->Initialize(frame, size, size, WICBitmapInterpolationModeHighQualityCubic);
    if (SUCCEEDED(hr))
        hr = factory->CreateFormatConverter(&converter);
    // Icons want 32-bit BGRA with straight (not premultiplied) alpha.
    if (SUCCEEDED(hr))
        hr = converter->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);

    if (SUCCEEDED(hr)) {
        // A DIB section is a bitmap whose pixels live in memory we can write.
        // A negative height makes it top-down, matching WIC's row order.
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = size;
        info.bmiHeader.biHeight = -size;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP color = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (color && SUCCEEDED(converter->CopyPixels(nullptr, size * 4, size * size * 4, static_cast<BYTE*>(bits)))) {
            // An icon is a color bitmap plus a 1-bit mask. With 32-bit color
            // the alpha channel does the job and the mask can stay empty.
            HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
            ICONINFO iconInfo{TRUE, 0, 0, mask, color};
            icon = CreateIconIndirect(&iconInfo); // copies both bitmaps
            DeleteObject(mask);
        }
        if (color)
            DeleteObject(color);
    }

    if (converter) converter->Release();
    if (scaler) scaler->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (factory) factory->Release();
    return icon;
}

// --- Icons from the system image list -------------------------------------------------
//
// Explorer keeps every icon it shows (folders, document types, shortcuts)
// in "system image lists", one per size. SHGetFileInfoW gives the position
// of a file's icon in those lists; SHGetImageList gives the list itself.
HICON FromSystemImageList(int imageIndex, int size)
{
    // Pick the smallest list whose icons are at least `size` pixels, so the
    // icon is only ever scaled down when drawn, never blown up.
    const int lists[] = {SHIL_SMALL, SHIL_LARGE, SHIL_EXTRALARGE, SHIL_JUMBO};
    for (int list : lists) {
        IImageList* imageList = nullptr;
        if (FAILED(SHGetImageList(list, IID_PPV_ARGS(&imageList))))
            continue;
        int width = 0;
        int height = 0;
        imageList->GetIconSize(&width, &height);
        HICON icon = nullptr;
        if (width >= size || list == SHIL_JUMBO)
            imageList->GetIcon(imageIndex, ILD_TRANSPARENT, &icon);
        imageList->Release();
        if (icon)
            return icon;
    }
    return nullptr;
}

// Icon that Explorer shows for a path. With `pretend` the file does not
// need to exist: Windows uses only the extension and the attributes, which
// gives "the icon of a .txt file" or "the icon of a folder".
HICON ShellIcon(const std::wstring& path, int size, bool pretend, DWORD attributes = FILE_ATTRIBUTE_NORMAL)
{
    SHFILEINFOW info{};
    UINT flags = SHGFI_SYSICONINDEX;
    if (pretend)
        flags |= SHGFI_USEFILEATTRIBUTES;
    if (!SHGetFileInfoW(path.c_str(), pretend ? attributes : 0, &info, sizeof(info), flags))
        return nullptr;
    return FromSystemImageList(info.iIcon, size);
}

HICON StockIcon(SHSTOCKICONID id, int size)
{
    SHSTOCKICONINFO info{};
    info.cbSize = sizeof(info);
    if (FAILED(SHGetStockIconInfo(id, SHGSI_SYSICONINDEX, &info)))
        return nullptr;
    return FromSystemImageList(info.iSysImageIndex, size);
}

} // namespace

HICON LoadIconFromFile(const std::wstring& path, int index, int size)
{
    return LowerExtension(path) == L".png" ? IconFromPng(path, size) : ExtractFromFile(path, index, size);
}

std::vector<HICON> ExtractAllIcons(const std::wstring& path, int size, std::vector<UINT>& ids)
{
    ids.clear();
    if (LowerExtension(path) == L".png") {
        HICON icon = IconFromPng(path, size);
        if (!icon)
            return {};
        ids.push_back(0);
        return {icon};
    }

    // With no output arrays, PrivateExtractIconsW only counts the icons.
    const UINT count = PrivateExtractIconsW(path.c_str(), 0, 0, 0, nullptr, nullptr, 0, 0);
    if (count == 0 || count == 0xFFFFFFFF)
        return {};

    // Then one call extracts them all, much faster than one call per icon
    // because the file is opened and parsed only once.
    std::vector<HICON> icons(count, nullptr);
    ids.assign(count, 0);
    const UINT extracted = PrivateExtractIconsW(path.c_str(), 0, size, size, icons.data(), ids.data(), count, 0);
    if (extracted == 0xFFFFFFFF)
        return {};
    icons.resize(extracted);
    ids.resize(extracted);
    return icons;
}

IconCache::IconCache(std::wstring exeDirectory)
    : exeDirectory_(std::move(exeDirectory))
{
}

IconCache::~IconCache()
{
    Clear();
}

void IconCache::Clear()
{
    // Icons are system objects (GDI handles). Each one must be destroyed,
    // or the process slowly runs out of them.
    for (auto& [key, icon] : icons_) {
        if (icon)
            DestroyIcon(icon);
    }
    icons_.clear();
}

HICON IconCache::Remember(const std::wstring& key, HICON icon)
{
    icons_[key] = icon; // a failure is remembered too (null), not retried
    return icon;
}

HICON IconCache::ForItem(const MenuItem& item, int size)
{
    HICON icon = nullptr;
    if (item.icon)
        icon = ForSpec(*item.icon, size);
    if (!icon)
        icon = item.type == ItemType::Menu ? Folder(size) : ForCommand(item.command, size);
    return icon ? icon : Generic(size);
}

HICON IconCache::ForSpec(const IconSpec& spec, int size)
{
    const std::wstring path = ResolveIconPath(spec.path, exeDirectory_);
    const std::wstring key = L"spec|" + std::to_wstring(size) + L"|" + std::to_wstring(spec.index) + L"|" + path;
    if (const auto it = icons_.find(key); it != icons_.end())
        return it->second;

    return Remember(key, LoadIconFromFile(path, spec.index, size));
}

HICON IconCache::ForCommand(const std::wstring& command, int size)
{
    std::wstring target = CompleteExtension(ParseCommand(command).target);
    const std::wstring key = L"cmd|" + std::to_wstring(size) + L"|" + target;
    if (const auto it = icons_.find(key); it != icons_.end())
        return it->second;

    if (target.empty())
        return Remember(key, nullptr);

    // Web addresses and other URLs: the icon of an Internet shortcut.
    if (target.find(L"://") != std::wstring::npos)
        return Remember(key, ShellIcon(L".url", size, true));

    // A bare name such as "notepad.exe": find it in the PATH like Windows does.
    if (!IsAbsolutePath(target)) {
        wchar_t found[MAX_PATH] = {};
        if (SearchPathW(nullptr, target.c_str(), nullptr, MAX_PATH, found, nullptr))
            target = found;
    }

    HICON icon = nullptr;
    const PathKind kind = ProbeFileSystem(target);
    const std::wstring extension = LowerExtension(target);
    if (kind == PathKind::File && (extension == L".exe" || extension == L".dll" || extension == L".ico"))
        icon = ExtractFromFile(target, 0, size); // the program's own icon, at the exact size
    else if (kind != PathKind::None)
        icon = ShellIcon(target, size, false); // folders, documents, shortcuts
    else
        icon = ShellIcon(target, size, true); // missing: the icon of its file type
    return Remember(key, icon);
}

HICON IconCache::Generic(int size)
{
    const std::wstring key = L"generic|" + std::to_wstring(size);
    if (const auto it = icons_.find(key); it != icons_.end())
        return it->second;
    HICON icon = StockIcon(SIID_APPLICATION, size);
    if (!icon) // last resort, shared system icon: must not be destroyed, so copy it
        icon = CopyIcon(LoadIconW(nullptr, IDI_APPLICATION));
    return Remember(key, icon);
}

HICON IconCache::Folder(int size)
{
    const std::wstring key = L"folder|" + std::to_wstring(size);
    if (const auto it = icons_.find(key); it != icons_.end())
        return it->second;
    return Remember(key, StockIcon(SIID_FOLDER, size));
}

} // namespace popa
