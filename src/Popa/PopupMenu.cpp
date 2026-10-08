#include "Popa/PopupMenu.h"

#include "PopaCore/Localization.h"

#include <shellscalingapi.h>

#include <algorithm>
#include <cwctype>

namespace popa {

namespace {

// Layout in pixels at 96 DPI (100 % scaling); Scale() converts them.
constexpr int kSidePadding = 10;     // between the menu edge and the icon
constexpr int kIconTextGap = 10;
constexpr int kVerticalPadding = 4;  // above and below the icon
constexpr int kArrowArea = 26;       // right part, for the submenu arrow
constexpr int kSeparatorHeight = 9;
constexpr int kHighlightInsetX = 4;
constexpr int kHighlightInsetY = 1;
constexpr int kHighlightRadius = 4;
constexpr int kMaxTextWidth = 420;

// DPI of the monitor that contains a point. With per-monitor DPI awareness
// each monitor can have its own scaling, and the menu should be sized for
// the monitor where it opens.
int DpiAt(POINT point)
{
    HMONITOR monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
        return 96;
    return static_cast<int>(dpiX);
}

} // namespace

PopupMenu::~PopupMenu()
{
    ReleaseResources();
}

int PopupMenu::Scale(int value) const
{
    // MulDiv computes value * dpi / 96 with rounding and without overflow.
    return MulDiv(value, dpi_, 96);
}

void PopupMenu::ReleaseResources()
{
    if (font_)
        DeleteObject(font_);
    if (backgroundBrush_)
        DeleteObject(backgroundBrush_);
    font_ = nullptr;
    backgroundBrush_ = nullptr;
    rows_.clear();
    commands_.clear();
    icons_ = nullptr;
}

const MenuItem* PopupMenu::Show(HWND owner, const std::vector<MenuItem>& items, POINT at, int iconSize, bool dark,
                                IconCache& icons)
{
    if (open_)
        return nullptr; // already open (for example a second hotkey press)

    icons_ = &icons;
    dpi_ = DpiAt(at);
    iconPixels_ = Scale(iconSize);
    colors_ = MenuColorsFor(dark);
    ApplyMenuTheme(dark);

    // The font Windows uses for menus, at the monitor's DPI.
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi_);
    font_ = CreateFontIndirectW(&metrics.lfMenuFont);
    backgroundBrush_ = CreateSolidBrush(colors_.background);

    HMENU root = Build(items);

    // Paint the menu background (the space around the rows) with our color
    // too, in the root menu and every submenu.
    MENUINFO info{};
    info.cbSize = sizeof(info);
    info.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    info.hbrBack = backgroundBrush_;
    SetMenuInfo(root, &info);

    // Same foreground rule as the tray menu: without it the menu would not
    // close when the user clicks somewhere else (see Tray.cpp).
    SetForegroundWindow(owner);

    // TrackPopupMenuEx runs its own message loop and returns when the menu
    // closes. TPM_RETURNCMD makes it return the chosen command ID (0 for
    // none) instead of posting WM_COMMAND. Meanwhile the owner window keeps
    // receiving WM_MEASUREITEM / WM_DRAWITEM, which land in HandleMessage.
    open_ = true;
    const UINT flags = TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON;
    const auto command = static_cast<size_t>(TrackPopupMenuEx(root, flags, at.x, at.y, owner, nullptr));
    open_ = false;
    PostMessageW(owner, WM_NULL, 0, 0);

    // Destroying the root menu also destroys every submenu attached to it.
    DestroyMenu(root);
    const MenuItem* chosen = (command >= 1 && command <= commands_.size()) ? commands_[command - 1] : nullptr;
    ReleaseResources();
    return chosen;
}

HMENU PopupMenu::Build(const std::vector<MenuItem>& items)
{
    HMENU menu = CreatePopupMenu();
    if (items.empty()) {
        AddPlaceholder(menu);
        return menu;
    }

    UINT position = 0;
    for (const MenuItem& item : items) {
        rows_.push_back(Row{&item});

        // MENUITEMINFOW describes one row; fMask says which fields are set.
        // dwItemData is free for the program: it holds a pointer to our Row
        // and comes back in WM_MEASUREITEM and WM_DRAWITEM.
        MENUITEMINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_ID;
        info.fType = MFT_OWNERDRAW;
        info.dwItemData = reinterpret_cast<ULONG_PTR>(&rows_.back());

        if (item.type == ItemType::Separator) {
            info.fType |= MFT_SEPARATOR;
        } else if (item.type == ItemType::Menu) {
            info.fMask |= MIIM_SUBMENU;
            info.hSubMenu = Build(item.children); // recursion builds the whole tree
        } else {
            commands_.push_back(&item);
            info.wID = static_cast<UINT>(commands_.size()); // 1-based
        }
        InsertMenuItemW(menu, position++, TRUE, &info);
    }
    return menu;
}

void PopupMenu::AddPlaceholder(HMENU menu)
{
    rows_.push_back(Row{nullptr});
    MENUITEMINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STATE;
    info.fType = MFT_OWNERDRAW;
    info.fState = MFS_DISABLED;
    info.dwItemData = reinterpret_cast<ULONG_PTR>(&rows_.back());
    InsertMenuItemW(menu, 0, TRUE, &info);
}

bool PopupMenu::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam, LRESULT& result)
{
    if (!open_)
        return false;

    switch (message) {
    case WM_MEASUREITEM: {
        auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (measure->CtlType != ODT_MENU)
            return false;
        Measure(*measure);
        result = TRUE;
        return true;
    }
    case WM_DRAWITEM: {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw->CtlType != ODT_MENU)
            return false;
        Draw(*draw);
        result = TRUE;
        return true;
    }
    case WM_MENUCHAR:
        result = OnMenuChar(static_cast<wchar_t>(LOWORD(wParam)), reinterpret_cast<HMENU>(lParam));
        return true;
    }
    return false;
}

void PopupMenu::Measure(MEASUREITEMSTRUCT& measure)
{
    const auto* row = reinterpret_cast<const Row*>(measure.itemData);
    if (row && row->item && row->item->type == ItemType::Separator) {
        measure.itemWidth = static_cast<UINT>(Scale(60));
        measure.itemHeight = static_cast<UINT>(Scale(kSeparatorHeight));
        return;
    }

    const std::wstring_view text = (row && row->item) ? std::wstring_view(row->item->name)
                                                       : std::wstring_view(Tr(TextId::MenuEmpty));

    // Measure the text with the menu font. A device context (DC) is GDI's
    // drawing surface; the screen's DC is enough for measuring.
    HDC dc = GetDC(nullptr);
    HGDIOBJ oldFont = SelectObject(dc, font_);
    SIZE textSize{};
    GetTextExtentPoint32W(dc, text.data(), static_cast<int>(text.size()), &textSize);
    TEXTMETRICW textMetrics{};
    GetTextMetricsW(dc, &textMetrics);
    SelectObject(dc, oldFont); // always put back what was there before
    ReleaseDC(nullptr, dc);

    const int textWidth = std::min<int>(textSize.cx, Scale(kMaxTextWidth));
    const int contentHeight = std::max<int>(iconPixels_, textMetrics.tmHeight);

    // Note: Windows adds the width of a check mark to this value on its
    // own, so the row ends up somewhat wider than what is computed here.
    measure.itemWidth = static_cast<UINT>(Scale(kSidePadding) + iconPixels_ + Scale(kIconTextGap) + textWidth +
                                          Scale(kArrowArea));
    measure.itemHeight = static_cast<UINT>(contentHeight + 2 * Scale(kVerticalPadding));
}

void PopupMenu::Draw(const DRAWITEMSTRUCT& draw)
{
    const auto* row = reinterpret_cast<const Row*>(draw.itemData);
    const MenuItem* item = row ? row->item : nullptr;
    HDC dc = draw.hDC;
    RECT rect = draw.rcItem;

    // 1. Background of the whole row.
    FillRect(dc, &rect, backgroundBrush_);

    // 2. Separator: a thin horizontal line in the middle.
    if (item && item->type == ItemType::Separator) {
        const int y = (rect.top + rect.bottom) / 2;
        RECT line{rect.left + Scale(kSidePadding), y, rect.right - Scale(kSidePadding), y + std::max(1, Scale(1))};
        HBRUSH brush = CreateSolidBrush(colors_.separator);
        FillRect(dc, &line, brush);
        DeleteObject(brush);
        return;
    }

    const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
    const bool selected = (draw.itemState & ODS_SELECTED) != 0 && !disabled;

    // 3. Highlight: a rounded rectangle under the mouse / keyboard selection.
    // RoundRect draws with the current pen (outline) and brush (fill); the
    // NULL_PEN stock object means "no outline".
    if (selected) {
        HBRUSH brush = CreateSolidBrush(colors_.highlight);
        HGDIOBJ oldBrush = SelectObject(dc, brush);
        HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        const int radius = Scale(kHighlightRadius) * 2; // RoundRect wants the ellipse size
        RoundRect(dc, rect.left + Scale(kHighlightInsetX), rect.top + Scale(kHighlightInsetY),
                  rect.right - Scale(kHighlightInsetX) + 1, rect.bottom - Scale(kHighlightInsetY) + 1, radius,
                  radius);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(brush);
    }

    // 4. Icon, vertically centered. DrawIconEx draws it at the given size
    // with its alpha channel (soft edges).
    const int iconX = rect.left + Scale(kSidePadding);
    if (item) {
        const int iconY = rect.top + (rect.bottom - rect.top - iconPixels_) / 2;
        DrawIconEx(dc, iconX, iconY, icons_->ForItem(*item, iconPixels_), iconPixels_, iconPixels_, 0, nullptr,
                   DI_NORMAL);
    }

    // 5. Name. DT_NOPREFIX: an "&" in a name is shown as is instead of
    // underlining the next letter; DT_END_ELLIPSIS: "..." when too long.
    RECT textRect = rect;
    textRect.left = iconX + iconPixels_ + Scale(kIconTextGap);
    textRect.right = rect.right - Scale(kArrowArea);
    const std::wstring_view text = item ? std::wstring_view(item->name) : std::wstring_view(Tr(TextId::MenuEmpty));
    HGDIOBJ oldFont = SelectObject(dc, font_);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled || !item ? colors_.disabledText : colors_.text);
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &textRect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(dc, oldFont);

    // 6. Submenu arrow. After WM_DRAWITEM Windows paints its own arrow,
    // which ignores our colors (black on a dark background). Two steps:
    // draw a chevron ourselves, then remove the arrow area from the DC's
    // clipping region so the system's arrow paints nothing.
    if (item && item->type == ItemType::Menu) {
        const int size = Scale(4);
        const int cx = rect.right - Scale(kArrowArea) / 2;
        const int cy = (rect.top + rect.bottom) / 2;
        HPEN pen = CreatePen(PS_SOLID, std::max(1, Scale(1)), colors_.text);
        HGDIOBJ oldPen = SelectObject(dc, pen);
        MoveToEx(dc, cx - size / 2, cy - size, nullptr);
        LineTo(dc, cx + size / 2, cy);
        LineTo(dc, cx - size / 2, cy + size + 1);
        SelectObject(dc, oldPen);
        DeleteObject(pen);
        ExcludeClipRect(dc, rect.right - Scale(kArrowArea), rect.top, rect.right + Scale(kArrowArea), rect.bottom);
    }
}

LRESULT PopupMenu::OnMenuChar(wchar_t character, HMENU menu)
{
    // Owner-drawn rows have no "&" mnemonics, so Windows asks the owner
    // what a typed letter means (WM_MENUCHAR). Popa jumps to the next entry
    // whose name starts with that letter; when only one entry matches, it
    // is opened right away, as with normal menu mnemonics.
    const int count = GetMenuItemCount(menu);
    const wchar_t wanted = static_cast<wchar_t>(std::towlower(character));

    int selected = -1;
    std::vector<int> matches;
    for (int i = 0; i < count; ++i) {
        MENUITEMINFOW info{};
        info.cbSize = sizeof(info);
        info.fMask = MIIM_DATA | MIIM_STATE;
        if (!GetMenuItemInfoW(menu, static_cast<UINT>(i), TRUE, &info))
            continue;
        if (info.fState & MFS_HILITE)
            selected = i;
        const auto* row = reinterpret_cast<const Row*>(info.dwItemData);
        if (row && row->item && row->item->type != ItemType::Separator && !row->item->name.empty() &&
            std::towlower(row->item->name.front()) == wanted)
            matches.push_back(i);
    }

    if (matches.empty())
        return MAKELRESULT(0, MNC_IGNORE);
    if (matches.size() == 1)
        return MAKELRESULT(matches.front(), MNC_EXECUTE);
    // Several matches: select the first one after the current selection,
    // wrapping around to the top.
    const auto next = std::find_if(matches.begin(), matches.end(), [&](int i) { return i > selected; });
    return MAKELRESULT(next != matches.end() ? *next : matches.front(), MNC_SELECT);
}

} // namespace popa
