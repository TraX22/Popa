#include "Popa/IconPickerDialog.h"

#include "Popa/FileDialog.h"
#include "Popa/IconCache.h"
#include "Popa/UiUtil.h"
#include "PopaCore/IconRef.h"
#include "PopaCore/Localization.h"

#include <commctrl.h>
#include <uxtheme.h>

#include <vector>

namespace popa {

namespace {

constexpr wchar_t kWindowClass[] = L"Popa.IconPicker";

enum ControlId : int {
    kIdPathLabel = 100,
    kIdPath,
    kIdBrowse,
    kIdList,
    kIdStatus,
};

// Sizes at 96 DPI.
constexpr int kMargin = 10;
constexpr int kRowHeight = 24;
constexpr int kButtonWidth = 96;
constexpr int kButtonHeight = 28;
constexpr int kIconSize = 32;

std::wstring Trimmed(std::wstring text)
{
    while (!text.empty() && (text.back() == L' ' || text.back() == L'"'))
        text.pop_back();
    size_t start = 0;
    while (start < text.size() && (text[start] == L' ' || text[start] == L'"'))
        ++start;
    return text.substr(start);
}

// All the state of one open dialog. It lives on the stack of PickIcon().
class IconPicker {
public:
    IconPicker(const IconSpec& initial, std::wstring exeDirectory)
        : initial_(initial), exeDirectory_(std::move(exeDirectory))
    {
    }

    std::optional<IconSpec> Run(HWND owner);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void Layout();
    void LoadIcons(int selectIndex);
    void Accept();
    void Browse();
    int Scale(int value) const { return ScaleForDpi(value, dpi_); }

    IconSpec initial_;
    std::wstring exeDirectory_;
    HWND window_ = nullptr;
    HWND pathLabel_ = nullptr;
    HWND path_ = nullptr;
    HWND browse_ = nullptr;
    HWND list_ = nullptr;
    HWND status_ = nullptr;
    HWND ok_ = nullptr;
    HWND cancel_ = nullptr;
    HIMAGELIST images_ = nullptr;
    HFONT font_ = nullptr;
    int dpi_ = 96;
    std::wstring loadedPath_;   // the path text whose icons are shown
    std::vector<UINT> iconIds_; // resource ID of each shown icon
    bool done_ = false;
    std::optional<IconSpec> result_;
};

std::optional<IconSpec> IconPicker::Run(HWND owner)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &IconPicker::WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass); // fails harmlessly when already registered

    dpi_ = owner ? static_cast<int>(GetDpiForWindow(owner)) : 96;

    // Window size from the wanted client size: AdjustWindowRectExForDpi adds
    // the title bar and borders at this DPI.
    const DWORD style = WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN;
    RECT rect{0, 0, Scale(580), Scale(460)};
    AdjustWindowRectExForDpi(&rect, style, FALSE, WS_EX_DLGMODALFRAME, static_cast<UINT>(dpi_));
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    // Centered over the owner.
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int x = ownerRect.left + (ownerRect.right - ownerRect.left - width) / 2;
    const int y = ownerRect.top + (ownerRect.bottom - ownerRect.top - height) / 2;

    // Modal: see RunModalLoop in UiUtil.h for the steps.
    EnableWindow(owner, FALSE);
    window_ = CreateWindowExW(WS_EX_DLGMODALFRAME, kWindowClass, Tr(TextId::IconPickerTitle), style, x, y, width,
                              height, owner, nullptr, GetModuleHandleW(nullptr), this);
    if (!window_) {
        EnableWindow(owner, TRUE);
        return std::nullopt;
    }
    ShowWindow(window_, SW_SHOW);
    LoadIcons(initial_.index);
    SetFocus(list_);

    RunModalLoop(window_, done_);

    EnableWindow(owner, TRUE);
    DestroyWindow(window_);
    SetForegroundWindow(owner);
    return result_;
}

LRESULT CALLBACK IconPicker::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Same `this` pointer technique as App::WindowProc.
    if (message == WM_NCCREATE) {
        auto* picker = static_cast<IconPicker*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        picker->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(picker));
    }
    auto* picker = reinterpret_cast<IconPicker*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return picker ? picker->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT IconPicker::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateControls();
        return 0;

    case WM_SIZE:
        Layout();
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {Scale(380), Scale(300)};
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        // Labels ask their parent which background to use; without this
        // they would be gray on the white window.
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            // Enter in the path box means "load this file", not "accept".
            if (GetFocus() == path_)
                LoadIcons(0);
            else
                Accept();
            return 0;
        case IDCANCEL:
            done_ = true;
            return 0;
        case kIdBrowse:
            Browse();
            return 0;
        }
        break;

    case WM_NOTIFY: {
        const auto* header = reinterpret_cast<const NMHDR*>(lParam);
        if (header->idFrom == kIdList && header->code == NM_DBLCLK)
            Accept();
        else if (header->idFrom == kIdList && header->code == LVN_ITEMCHANGED)
            EnableWindow(ok_, ListView_GetNextItem(list_, -1, LVNI_SELECTED) >= 0);
        return 0;
    }

    case WM_CLOSE:
        done_ = true; // the window is destroyed by Run() after the loop
        return 0;

    case WM_DESTROY:
        if (font_)
            DeleteObject(font_);
        font_ = nullptr;
        return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void IconPicker::CreateControls()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    auto create = [&](DWORD exStyle, const wchar_t* className, const wchar_t* text, DWORD style, int id) {
        return CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, window_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    };

    pathLabel_ = create(0, WC_STATICW, Tr(TextId::IconPickerFile), SS_LEFT | SS_CENTERIMAGE, kIdPathLabel);
    path_ = create(WS_EX_CLIENTEDGE, WC_EDITW, initial_.path.c_str(), WS_TABSTOP | ES_AUTOHSCROLL, kIdPath);
    browse_ = create(0, WC_BUTTONW, Tr(TextId::EditorBrowse), WS_TABSTOP | BS_PUSHBUTTON, kIdBrowse);
    list_ = create(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                   WS_TABSTOP | LVS_ICON | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_AUTOARRANGE, kIdList);
    status_ = create(0, WC_STATICW, L"", SS_LEFT | SS_CENTERIMAGE, kIdStatus);
    ok_ = create(0, WC_BUTTONW, Tr(TextId::CommonOk), WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK);
    cancel_ = create(0, WC_BUTTONW, Tr(TextId::CommonCancel), WS_TABSTOP | BS_PUSHBUTTON, IDCANCEL);

    // "Explorer" theme: the list view gets the Windows 11 selection look.
    SetWindowTheme(list_, L"Explorer", nullptr);
    ListView_SetExtendedListViewStyle(list_, LVS_EX_DOUBLEBUFFER);

    // ILC_COLOR32: keep the alpha channel (soft edges). The list view
    // destroys its image list when it is destroyed.
    images_ = ImageList_Create(Scale(kIconSize), Scale(kIconSize), ILC_COLOR32, 64, 64);
    ListView_SetImageList(list_, images_, LVSIL_NORMAL);
    ListView_SetIconSpacing(list_, Scale(kIconSize + 28), Scale(kIconSize + 30));

    font_ = CreateMessageFont(dpi_);
    SetFontOnChildren(window_, font_);
}

void IconPicker::Layout()
{
    RECT client{};
    GetClientRect(window_, &client);
    const int m = Scale(kMargin);
    const int row = Scale(kRowHeight);
    const int buttonWidth = Scale(kButtonWidth);
    const int buttonHeight = Scale(kButtonHeight);
    const int labelWidth = TextWidth(font_, Tr(TextId::IconPickerFile)) + Scale(8);
    const int width = client.right;
    const int height = client.bottom;

    // DeferWindowPos moves all controls in one go, avoiding flicker.
    HDWP batch = BeginDeferWindowPos(7);
    auto place = [&](HWND control, int x, int y, int w, int h) {
        batch = DeferWindowPos(batch, control, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };
    place(pathLabel_, m, m, labelWidth, row);
    place(path_, m + labelWidth, m, width - 3 * m - labelWidth - buttonWidth, row);
    place(browse_, width - m - buttonWidth, m - Scale(2), buttonWidth, buttonHeight);
    const int listTop = m + row + m;
    const int bottomRow = height - m - buttonHeight;
    place(list_, m, listTop, width - 2 * m, bottomRow - m - listTop);
    place(status_, m, bottomRow, width - 4 * m - 2 * buttonWidth, buttonHeight);
    place(ok_, width - 2 * m - 2 * buttonWidth + m, bottomRow, buttonWidth, buttonHeight);
    place(cancel_, width - m - buttonWidth, bottomRow, buttonWidth, buttonHeight);
    EndDeferWindowPos(batch);
}

void IconPicker::LoadIcons(int selectIndex)
{
    loadedPath_ = Trimmed(GetText(path_));
    ListView_DeleteAllItems(list_);
    ImageList_RemoveAll(images_);

    const std::wstring file = ResolveIconPath(loadedPath_, exeDirectory_);
    std::vector<HICON> icons = ExtractAllIcons(file, Scale(kIconSize), iconIds_);

    // Hide the list while filling it: hundreds of inserts with repaints in
    // between would be slow.
    SendMessageW(list_, WM_SETREDRAW, FALSE, 0);
    for (size_t i = 0; i < icons.size(); ++i) {
        // ImageList_AddIcon copies the image, so the icon is destroyed after.
        const int image = ImageList_AddIcon(images_, icons[i]);
        DestroyIcon(icons[i]);

        const std::wstring label = std::to_wstring(i);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_IMAGE;
        item.iItem = static_cast<int>(i);
        item.pszText = const_cast<wchar_t*>(label.c_str());
        item.iImage = image;
        ListView_InsertItem(list_, &item);
    }
    SendMessageW(list_, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(list_, nullptr, TRUE);

    // A negative index is a resource ID: find the icon that has it.
    int select = selectIndex;
    if (selectIndex < 0) {
        select = 0;
        for (size_t i = 0; i < iconIds_.size(); ++i) {
            if (static_cast<int>(iconIds_[i]) == -selectIndex)
                select = static_cast<int>(i);
        }
    }
    if (!icons.empty()) {
        select = std::min(std::max(select, 0), static_cast<int>(icons.size()) - 1);
        ListView_SetItemState(list_, select, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        ListView_EnsureVisible(list_, select, FALSE);
    }
    SetWindowTextW(status_, icons.empty() ? Tr(TextId::IconPickerNoIcons) : L"");
    EnableWindow(ok_, !icons.empty());
}

void IconPicker::Accept()
{
    // The path box was edited but not loaded yet: load it first.
    if (Trimmed(GetText(path_)) != loadedPath_) {
        LoadIcons(0);
        return;
    }
    const int selected = ListView_GetNextItem(list_, -1, LVNI_SELECTED);
    if (selected < 0)
        return;
    result_ = IconSpec{loadedPath_, selected};
    done_ = true;
}

void IconPicker::Browse()
{
    const std::wstring current = ResolveIconPath(Trimmed(GetText(path_)), exeDirectory_);
    const auto file = BrowseForFile(window_, Tr(TextId::IconPickerTitle),
                                    {{Tr(TextId::IconFilesFilter), L"*.ico;*.exe;*.dll;*.png"},
                                     {Tr(TextId::AllFilesFilter), L"*.*"}},
                                    current);
    if (!file)
        return;
    SetWindowTextW(path_, file->c_str());
    LoadIcons(0);
    SetFocus(list_);
}

} // namespace

std::optional<IconSpec> PickIcon(HWND owner, const IconSpec& initial, const std::wstring& exeDirectory)
{
    IconPicker picker(initial, exeDirectory);
    return picker.Run(owner);
}

} // namespace popa
