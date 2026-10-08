// Icons of menu entries: extraction from files and an in-memory cache.
//
// Every icon is loaded once at the exact pixel size requested and kept
// until the cache is cleared, so opening the menu again is instant.
#pragma once

#include "PopaCore/Model.h"

#include <windows.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace popa {

// Loads icon `index` of a file (.ico, .exe, .dll; .png ignores the index)
// at `size` pixels, without caching. The caller destroys it. Null on failure.
HICON LoadIconFromFile(const std::wstring& path, int index, int size);

// Every icon of a file at `size` pixels, for the icon picker. `ids`
// receives each icon's resource ID (used by negative indexes). The caller
// destroys the icons.
std::vector<HICON> ExtractAllIcons(const std::wstring& path, int size, std::vector<UINT>& ids);

class IconCache {
public:
    explicit IconCache(std::wstring exeDirectory);
    ~IconCache();
    IconCache(const IconCache&) = delete;
    IconCache& operator=(const IconCache&) = delete;

    // Icon for a menu entry at `size` x `size` pixels. Never null: when
    // nothing can be loaded, a generic icon is returned. The cache owns the
    // icon; callers must not destroy it.
    HICON ForItem(const MenuItem& item, int size);

    // An icon given as file + index ("shell32.dll", 13). Null on failure.
    HICON ForSpec(const IconSpec& spec, int size);

    // Releases every icon (for example after the menu file changed).
    void Clear();

private:
    HICON Remember(const std::wstring& key, HICON icon);
    HICON ForCommand(const std::wstring& command, int size);
    HICON Generic(int size);
    HICON Folder(int size);

    std::wstring exeDirectory_;
    std::unordered_map<std::wstring, HICON> icons_;
};

} // namespace popa
