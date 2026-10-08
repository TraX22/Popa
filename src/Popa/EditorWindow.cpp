#include "Popa/EditorWindow.h"

#include "Popa/FileDialog.h"
#include "Popa/IconPickerDialog.h"
#include "Popa/Launcher.h"
#include "Popa/XmlFile.h"
#include "Popa/ShortcutResolver.h"
#include "Popa/UiUtil.h"
#include "Popa/resource.h"
#include "PopaCore/CommandLine.h"
#include "PopaCore/DroppedItem.h"
#include "PopaCore/Localization.h"
#include "PopaCore/MenuImporter.h"

#include <dwmapi.h>
#include <shellscalingapi.h>
#include <shellapi.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <cwctype>
#include <vector>

namespace popa {

namespace {

constexpr wchar_t kWindowClass[] = L"Popa.Editor";

enum ControlId : int {
    kIdTree = 200,
    kIdAddItem,
    kIdAddMenu,
    kIdAddSeparator,
    kIdMoveUp,
    kIdMoveDown,
    kIdDelete,
    kIdImport,
    kIdName,
    kIdCommand,
    kIdBrowse,
    kIdChangeIcon,
    kIdResetIcon,
    kIdTest,
    kIdLabel, // all labels share one ID: nothing refers to them
};

enum TimerId : UINT_PTR {
    kSaveTimer = 1, // saves a moment after the last change
    kIconTimer,     // reloads the icon a moment after the command stops changing
};

constexpr UINT kSaveDelayMs = 600;
constexpr UINT kIconDelayMs = 400;

// Layout sizes at 96 DPI.
constexpr int kMargin = 10;
constexpr int kGap = 8;
constexpr int kButtonHeight = 28;
constexpr int kButtonPadding = 20;
constexpr int kRowHeight = 24;
constexpr int kPreviewSize = 32;

constexpr COLORREF kMissingColor = RGB(196, 43, 28);
constexpr wchar_t kSeparatorText[] = L"──────────────";

} // namespace

// --- Construction --------------------------------------------------------------------

EditorWindow::EditorWindow(IconCache& icons, std::wstring exeDirectory, ChangedCallback onChanged,
                           ClosedCallback onClosed)
    : icons_(icons), exeDirectory_(std::move(exeDirectory)), onChanged_(std::move(onChanged)),
      onClosed_(std::move(onClosed))
{
    root_.type = ItemType::Menu;
    root_.handle = TVI_ROOT; // the TreeView's own "parent of the top level"
}

EditorWindow::~EditorWindow()
{
    onClosed_ = nullptr; // the owner is destroying us; do not call it back
    if (window_)
        DestroyWindow(window_);
}

bool EditorWindow::Create(const std::vector<MenuItem>& items, bool darkTitleBar)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &EditorWindow::WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_POPA));
    RegisterClassExW(&windowClass); // fails harmlessly when already registered

    for (const MenuItem& item : items)
        root_.children.push_back(NodeFromItem(item, &root_));

    // Open on the monitor where the mouse is, sized for that monitor's DPI.
    // Creating the window directly at its final place lets Windows give it
    // the right DPI from the start.
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    GetMonitorInfoW(monitor, &monitorInfo);
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
        dpiX = 96;
    const RECT& work = monitorInfo.rcWork;
    const int width = std::min(ScaleForDpi(760, static_cast<int>(dpiX)), static_cast<int>(work.right - work.left));
    const int height = std::min(ScaleForDpi(720, static_cast<int>(dpiX)), static_cast<int>(work.bottom - work.top));
    const int x = work.left + (work.right - work.left - width) / 2;
    const int y = work.top + (work.bottom - work.top - height) / 2;

    window_ = CreateWindowExW(0, kWindowClass, Tr(TextId::EditorTitle), WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y,
                              width, height, nullptr, nullptr, instance, this);
    if (!window_)
        return false;

    SetDarkTitleBar(darkTitleBar);
    for (auto& node : root_.children)
        InsertIntoTree(node.get());
    if (!root_.children.empty())
        Select(root_.children.front().get());
    else
        ShowDetails(nullptr);

    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
    SetFocus(tree_);
    return true;
}

void EditorWindow::Activate()
{
    if (!window_)
        return;
    if (IsIconic(window_))
        ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}

void EditorWindow::SetDarkTitleBar(bool dark)
{
    if (!window_)
        return;
    // The Desktop Window Manager draws the title bar. Attribute 20
    // (DWMWA_USE_IMMERSIVE_DARK_MODE) asks it for the dark version; the
    // window's content stays light, since the standard Win32 controls have
    // no complete dark mode (see section 5.5 of the design document).
    const BOOL value = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}

bool EditorWindow::PreTranslateMessage(MSG& message)
{
    if (!window_)
        return false;
    // During a drag the mouse is captured but the keyboard still goes to
    // the focused control, so Esc is caught here to cancel the drag.
    if (dragged_ && message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
        EndDrag(false);
        return true;
    }
    // IsDialogMessageW gives a normal window the keyboard behavior of a
    // dialog box: Tab and Shift+Tab move between controls (WS_TABSTOP),
    // Alt+letter shortcuts, Enter/Esc as IDOK/IDCANCEL commands.
    return IsDialogMessageW(window_, &message) != FALSE;
}

void EditorWindow::ApplyLanguage()
{
    if (!window_)
        return;
    SetWindowTextW(window_, Tr(TextId::EditorTitle));
    const TextId buttonTexts[] = {TextId::EditorAddItem,  TextId::EditorAddMenu,  TextId::EditorAddSeparator,
                                  TextId::EditorMoveUp,   TextId::EditorMoveDown, TextId::EditorDelete,
                                  TextId::EditorImport};
    for (size_t i = 0; i < buttons_.size(); ++i)
        SetWindowTextW(buttons_[i], Tr(buttonTexts[i]));
    SetWindowTextW(nameLabel_, Tr(TextId::EditorName));
    SetWindowTextW(commandLabel_, Tr(TextId::EditorCommand));
    SetWindowTextW(browse_, Tr(TextId::EditorBrowse));
    SetWindowTextW(iconLabel_, Tr(TextId::EditorIcon));
    SetWindowTextW(changeIcon_, Tr(TextId::EditorChangeIcon));
    SetWindowTextW(resetIcon_, Tr(TextId::EditorResetIcon));
    SetWindowTextW(test_, Tr(TextId::EditorTest));
    // Rows without a name show "(no name)" through the text callback: a
    // repaint asks for it again in the new language.
    InvalidateRect(tree_, nullptr, TRUE);
    ShowDetails(SelectedNode()); // the "not found" message
    Layout();                    // buttons change width with their text
}

void EditorWindow::FlushChanges()
{
    FlushIconRefresh();
    SaveNow();
}

int EditorWindow::Scale(int value) const
{
    return ScaleForDpi(value, dpi_);
}

// --- Window procedure --------------------------------------------------------------------

LRESULT CALLBACK EditorWindow::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Same `this` pointer technique as App::WindowProc.
    if (message == WM_NCCREATE) {
        auto* editor = static_cast<EditorWindow*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        editor->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(editor));
    }
    auto* editor = reinterpret_cast<EditorWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return editor ? editor->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT EditorWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        dpi_ = static_cast<int>(GetDpiForWindow(window_));
        CreateControls();
        ApplyDpi(dpi_);
        // Accept files dragged from Explorer or the desktop (WM_DROPFILES).
        DragAcceptFiles(window_, TRUE);
        return 0;

    case WM_SIZE:
        Layout();
        return 0;

    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {Scale(480), Scale(460)};
        return 0;
    }

    case WM_DPICHANGED: {
        // The window moved to a monitor with another scaling. Windows
        // suggests a new rectangle that keeps the same physical size;
        // fonts, icons and the layout are rebuilt for the new DPI.
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        ApplyDpi(HIWORD(wParam));
        return 0;
    }

    case WM_CTLCOLORSTATIC: {
        // Labels paint their background with what the parent returns here.
        // The window is white (COLOR_WINDOW), so the labels are too; the
        // "not found" label also gets its red text color.
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == missingLabel_ ? kMissingColor
                                                                           : GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == kIdName && code == EN_CHANGE) OnNameChanged();
        else if (id == kIdCommand && code == EN_CHANGE) OnCommandChanged();
        else if (code == BN_CLICKED) {
            switch (id) {
            case kIdAddItem: AddEntry(ItemType::Item); break;
            case kIdAddMenu: AddEntry(ItemType::Menu); break;
            case kIdAddSeparator: AddEntry(ItemType::Separator); break;
            case kIdMoveUp: MoveSelected(-1); break;
            case kIdMoveDown: MoveSelected(+1); break;
            case kIdDelete: DeleteSelected(); break;
            case kIdImport: ImportConfiguration(); break;
            case kIdBrowse: BrowseCommand(); break;
            case kIdChangeIcon: ChangeIcon(); break;
            case kIdResetIcon: ResetIcon(); break;
            case kIdTest: TestEntry(); break;
            }
        }
        // IDOK / IDCANCEL from Enter / Esc (IsDialogMessageW) are ignored:
        // the editor has no "accept" action and closing on Esc would be
        // too easy by accident.
        return 0;
    }

    case WM_NOTIFY: {
        auto* header = reinterpret_cast<NMHDR*>(lParam);
        if (header->hwndFrom == tree_)
            return OnTreeNotify(header);
        return 0;
    }

    case WM_TIMER:
        if (wParam == kSaveTimer)
            SaveNow();
        else if (wParam == kIconTimer)
            FlushIconRefresh();
        return 0;

    case WM_MOUSEMOVE:
        if (dragged_) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            MapWindowPoints(window_, tree_, &point, 1); // editor -> tree coordinates
            UpdateDrag(point);
        }
        return 0;

    case WM_LBUTTONUP:
        if (dragged_)
            EndDrag(true);
        return 0;

    case WM_CAPTURECHANGED:
        // Another window took the mouse (for example Alt+Tab): cancel.
        if (dragged_)
            EndDrag(false);
        return 0;

    case WM_DROPFILES:
        OnDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;

    case WM_CLOSE:
        FlushChanges();
        DestroyWindow(window_);
        return 0;

    case WM_DESTROY:
        KillTimer(window_, kSaveTimer);
        KillTimer(window_, kIconTimer);
        return 0;

    case WM_NCDESTROY: {
        // The last message a window receives, after its children are gone.
        // The tree no longer uses the image list, so it can be destroyed.
        if (images_)
            ImageList_Destroy(images_);
        if (font_)
            DeleteObject(font_);
        images_ = nullptr;
        font_ = nullptr;
        window_ = nullptr;
        tree_ = nullptr;
        if (onClosed_)
            onClosed_();
        return 0;
    }
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

// --- Controls and layout --------------------------------------------------------------------

void EditorWindow::CreateControls()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    auto create = [&](DWORD exStyle, const wchar_t* className, const wchar_t* text, DWORD style, int id) {
        return CreateWindowExW(exStyle, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, window_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    };
    auto button = [&](TextId text, int id) {
        return create(0, WC_BUTTONW, Tr(text), WS_TABSTOP | BS_PUSHBUTTON, id);
    };

    // Creation order is also the Tab order.
    buttons_ = {button(TextId::EditorAddItem, kIdAddItem),     button(TextId::EditorAddMenu, kIdAddMenu),
                button(TextId::EditorAddSeparator, kIdAddSeparator), button(TextId::EditorMoveUp, kIdMoveUp),
                button(TextId::EditorMoveDown, kIdMoveDown),   button(TextId::EditorDelete, kIdDelete),
                button(TextId::EditorImport, kIdImport)};

    tree_ = create(WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                   WS_TABSTOP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS | TVS_FULLROWSELECT, kIdTree);
    // The "Explorer" visual style gives the tree the look of File
    // Explorer's navigation pane (hover and selection highlight, chevrons).
    SetWindowTheme(tree_, L"Explorer", nullptr);
    TreeView_SetExtendedStyle(tree_, TVS_EX_DOUBLEBUFFER, TVS_EX_DOUBLEBUFFER);

    nameLabel_ = create(0, WC_STATICW, Tr(TextId::EditorName), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    name_ = create(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, kIdName);
    commandLabel_ = create(0, WC_STATICW, Tr(TextId::EditorCommand), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    command_ = create(WS_EX_CLIENTEDGE, WC_EDITW, L"", WS_TABSTOP | ES_AUTOHSCROLL, kIdCommand);
    browse_ = button(TextId::EditorBrowse, kIdBrowse);
    iconLabel_ = create(0, WC_STATICW, Tr(TextId::EditorIcon), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    // SS_REALSIZECONTROL draws the icon at the control's size.
    iconPreview_ = create(0, WC_STATICW, L"", SS_ICON | SS_REALSIZECONTROL, kIdLabel);
    changeIcon_ = button(TextId::EditorChangeIcon, kIdChangeIcon);
    resetIcon_ = button(TextId::EditorResetIcon, kIdResetIcon);
    test_ = button(TextId::EditorTest, kIdTest);
    missingLabel_ = create(0, WC_STATICW, L"", SS_LEFT | SS_CENTERIMAGE, kIdLabel);
}

void EditorWindow::ApplyDpi(int dpi)
{
    dpi_ = dpi;
    if (font_)
        DeleteObject(font_);
    font_ = CreateMessageFont(dpi_);
    SetFontOnChildren(window_, font_);
    ResetImages();
    UpdateIconPreview(SelectedNode());
    Layout();
}

void EditorWindow::Layout()
{
    if (!tree_)
        return;
    RECT client{};
    GetClientRect(window_, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int m = Scale(kMargin);
    const int gap = Scale(kGap);
    const int buttonHeight = Scale(kButtonHeight);
    const int row = Scale(kRowHeight);
    const int preview = Scale(kPreviewSize);

    HDWP batch = BeginDeferWindowPos(24);
    auto place = [&](HWND control, int x, int y, int w, int h) {
        batch = DeferWindowPos(batch, control, nullptr, x, y, std::max(w, 0), std::max(h, 0),
                               SWP_NOZORDER | SWP_NOACTIVATE);
    };
    auto buttonWidth = [&](HWND control) { return TextWidth(font_, GetText(control)) + Scale(kButtonPadding); };

    // 1. Button bar, wrapping to more rows when the window is narrow.
    int x = m;
    int y = m;
    for (HWND control : buttons_) {
        const int w = buttonWidth(control);
        if (x > m && x + w > width - m) {
            x = m;
            y += buttonHeight + Scale(4);
        }
        place(control, x, y, w, buttonHeight);
        x += w + Scale(6);
    }
    const int treeTop = y + buttonHeight + gap;

    // 2. Detail panel at the bottom: four rows (name, command, icon, test).
    const int iconRowHeight = std::max(preview, buttonHeight);
    const int detailHeight = row + gap + row + gap + iconRowHeight + gap + buttonHeight;
    const int detailTop = height - m - detailHeight;
    const int labelWidth = std::max({TextWidth(font_, GetText(nameLabel_)), TextWidth(font_, GetText(commandLabel_)),
                                     TextWidth(font_, GetText(iconLabel_))}) +
                           Scale(12);
    const int fieldX = m + labelWidth;
    const int fieldWidth = width - m - fieldX;

    // 3. The tree takes the space in between.
    place(tree_, m, treeTop, width - 2 * m, detailTop - gap - treeTop);

    y = detailTop;
    place(nameLabel_, m, y, labelWidth, row);
    place(name_, fieldX, y, fieldWidth, row);
    y += row + gap;

    const int browseWidth = buttonWidth(browse_);
    place(commandLabel_, m, y, labelWidth, row);
    place(command_, fieldX, y, fieldWidth - gap - browseWidth, row);
    place(browse_, width - m - browseWidth, y - (buttonHeight - row) / 2, browseWidth, buttonHeight);
    y += row + gap;

    place(iconLabel_, m, y, labelWidth, iconRowHeight);
    place(iconPreview_, fieldX, y + (iconRowHeight - preview) / 2, preview, preview);
    const int changeWidth = buttonWidth(changeIcon_);
    place(changeIcon_, fieldX + preview + gap, y + (iconRowHeight - buttonHeight) / 2, changeWidth, buttonHeight);
    place(resetIcon_, fieldX + preview + 2 * gap + changeWidth, y + (iconRowHeight - buttonHeight) / 2,
          buttonWidth(resetIcon_), buttonHeight);
    y += iconRowHeight + gap;

    const int testWidth = buttonWidth(test_);
    place(test_, fieldX, y, testWidth, buttonHeight);
    place(missingLabel_, fieldX + testWidth + gap, y, width - m - (fieldX + testWidth + gap), buttonHeight);
    EndDeferWindowPos(batch);
}

// --- Nodes and items -----------------------------------------------------------------------

std::unique_ptr<EditorWindow::Node> EditorWindow::NodeFromItem(const MenuItem& item, Node* parent) const
{
    auto node = std::make_unique<Node>();
    node->type = item.type;
    node->name = item.name;
    node->command = item.command;
    node->icon = item.icon;
    node->parent = parent;
    node->missing = item.type == ItemType::Item && !item.command.empty() && !TargetExists(item.command);
    for (const MenuItem& child : item.children)
        node->children.push_back(NodeFromItem(child, node.get()));
    return node;
}

MenuItem EditorWindow::ShallowItem(const Node& node)
{
    MenuItem item;
    item.type = node.type;
    item.name = node.name;
    item.command = node.type == ItemType::Item ? node.command : std::wstring();
    item.icon = node.type == ItemType::Separator ? std::nullopt : node.icon;
    return item;
}

MenuItem EditorWindow::ItemFromNode(const Node& node) const
{
    MenuItem item = ShallowItem(node);
    for (const auto& child : node.children)
        item.children.push_back(ItemFromNode(*child));
    return item;
}

std::vector<MenuItem> EditorWindow::CurrentItems() const
{
    std::vector<MenuItem> items;
    for (const auto& child : root_.children)
        items.push_back(ItemFromNode(*child));
    return items;
}

// --- Tree view -----------------------------------------------------------------------------

void EditorWindow::InsertIntoTree(Node* node)
{
    // The row goes right after its previous sibling (or first).
    const auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
    const size_t index = static_cast<size_t>(it - siblings.begin());

    TVINSERTSTRUCTW insert{};
    insert.hParent = node->parent->handle;
    insert.hInsertAfter = index == 0 ? TVI_FIRST : siblings[index - 1]->handle;
    // Text, icon and "has children" are "callbacks": the tree asks for them
    // with TVN_GETDISPINFO when it needs to paint the row. So renaming an
    // entry only needs a repaint, and icons load only for visible rows.
    insert.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    insert.item.pszText = LPSTR_TEXTCALLBACKW;
    insert.item.iImage = I_IMAGECALLBACK;
    insert.item.iSelectedImage = I_IMAGECALLBACK;
    insert.item.cChildren = I_CHILDRENCALLBACK;
    insert.item.lParam = reinterpret_cast<LPARAM>(node); // our pointer, back in every notification
    node->handle = TreeView_InsertItem(tree_, &insert);

    for (auto& child : node->children)
        InsertIntoTree(child.get());
}

void EditorWindow::RemoveFromTree(Node* node)
{
    // Deleting a row deletes all the rows under it.
    TreeView_DeleteItem(tree_, node->handle);
    auto clear = [](auto& self, Node* n) -> void {
        n->handle = nullptr;
        for (auto& child : n->children)
            self(self, child.get());
    };
    clear(clear, node);
}

void EditorWindow::RefreshRow(Node* node)
{
    if (!node || !node->handle || node == &root_)
        return;
    // Setting the callback values again makes the tree ask for them anew.
    TVITEMW item{};
    item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN;
    item.hItem = node->handle;
    item.pszText = LPSTR_TEXTCALLBACKW;
    item.iImage = I_IMAGECALLBACK;
    item.iSelectedImage = I_IMAGECALLBACK;
    item.cChildren = I_CHILDRENCALLBACK;
    TreeView_SetItem(tree_, &item);
}

int EditorWindow::ImageFor(Node* node)
{
    if (node->type == ItemType::Separator)
        return 0; // the blank image
    if (node->image < 0)
        node->image = ImageList_AddIcon(images_, icons_.ForItem(ShallowItem(*node), treeIconSize_));
    return node->image;
}

void EditorWindow::ReloadIcon(Node* node)
{
    if (!node || node->type == ItemType::Separator)
        return;
    node->missing = node->type == ItemType::Item && !node->command.empty() && !TargetExists(node->command);
    HICON icon = icons_.ForItem(ShallowItem(*node), treeIconSize_);
    // Reuse the node's slot in the image list instead of growing it.
    if (node->image >= 0)
        ImageList_ReplaceIcon(images_, node->image, icon);
    else
        node->image = ImageList_AddIcon(images_, icon);
    RefreshRow(node);
    if (node == SelectedNode())
        UpdateIconPreview(node);
}

void EditorWindow::FlushIconRefresh()
{
    if (!window_)
        return;
    KillTimer(window_, kIconTimer);
    Node* node = pendingIconNode_;
    pendingIconNode_ = nullptr;
    ReloadIcon(node);
}

void EditorWindow::ResetImages()
{
    // A new image list at the size for the current DPI. Every node forgets
    // its image index and loads again when painted.
    treeIconSize_ = Scale(16);
    HIMAGELIST old = images_;
    // ILC_MASK keeps the transparent mask of the blank image; with comctl32 v6
    // the alpha channel of the other icons is still used.
    images_ = ImageList_Create(treeIconSize_, treeIconSize_, ILC_COLOR32 | ILC_MASK, 32, 32);

    // Image 0 is fully transparent, for separators. A 32-bit bitmap whose
    // alpha is all zero would not do: the image list takes "no alpha at
    // all" as "this bitmap has no alpha channel" and draws it opaque black.
    // An icon whose AND mask is all ones is transparent by definition (the
    // mask says "keep the background" for every pixel).
    const int maskBytes = ((treeIconSize_ + 15) / 16) * 2 * treeIconSize_; // 1-bit rows, WORD aligned
    std::vector<BYTE> ones(static_cast<size_t>(maskBytes), 0xFF);
    std::vector<BYTE> zeros(static_cast<size_t>(maskBytes), 0x00);
    HICON blank = CreateIcon(GetModuleHandleW(nullptr), treeIconSize_, treeIconSize_, 1, 1, ones.data(), zeros.data());
    ImageList_AddIcon(images_, blank);
    DestroyIcon(blank);

    auto forget = [](auto& self, Node* n) -> void {
        n->image = -1;
        for (auto& child : n->children)
            self(self, child.get());
    };
    forget(forget, &root_);

    TreeView_SetImageList(tree_, images_, TVSIL_NORMAL);
    if (old)
        ImageList_Destroy(old);
    InvalidateRect(tree_, nullptr, TRUE);
}

EditorWindow::Node* EditorWindow::NodeFromHandle(HTREEITEM handle) const
{
    if (!handle)
        return nullptr;
    TVITEMW item{};
    item.mask = TVIF_PARAM;
    item.hItem = handle;
    if (!TreeView_GetItem(tree_, &item))
        return nullptr;
    return reinterpret_cast<Node*>(item.lParam);
}

EditorWindow::Node* EditorWindow::SelectedNode() const
{
    return tree_ ? NodeFromHandle(TreeView_GetSelection(tree_)) : nullptr;
}

void EditorWindow::Select(Node* node)
{
    if (!node || !node->handle)
        return;
    TreeView_SelectItem(tree_, node->handle);
    TreeView_EnsureVisible(tree_, node->handle);
}

LRESULT EditorWindow::OnTreeNotify(NMHDR* header)
{
    switch (header->code) {
    case TVN_GETDISPINFOW: {
        auto* info = reinterpret_cast<NMTVDISPINFOW*>(header);
        auto* node = reinterpret_cast<Node*>(info->item.lParam);
        if (!node)
            return 0;
        if (info->item.mask & TVIF_TEXT) {
            const wchar_t* text = node->type == ItemType::Separator ? kSeparatorText
                                  : node->name.empty()              ? Tr(TextId::EditorNoName)
                                                                    : node->name.c_str();
            wcsncpy_s(info->item.pszText, static_cast<size_t>(info->item.cchTextMax), text, _TRUNCATE);
        }
        if (info->item.mask & (TVIF_IMAGE | TVIF_SELECTEDIMAGE)) {
            info->item.iImage = ImageFor(node);
            info->item.iSelectedImage = info->item.iImage;
        }
        if (info->item.mask & TVIF_CHILDREN)
            info->item.cChildren = node->children.empty() ? 0 : 1;
        return 0;
    }

    case TVN_SELCHANGEDW: {
        FlushIconRefresh(); // finish the previous entry's icon before switching
        const auto* change = reinterpret_cast<NMTREEVIEWW*>(header);
        ShowDetails(reinterpret_cast<Node*>(change->itemNew.lParam));
        return 0;
    }

    case TVN_KEYDOWN: {
        const auto* key = reinterpret_cast<NMTVKEYDOWN*>(header);
        if (key->wVKey == VK_DELETE)
            DeleteSelected();
        return 0;
    }

    case TVN_BEGINDRAGW: {
        const auto* drag = reinterpret_cast<NMTREEVIEWW*>(header);
        BeginDrag(reinterpret_cast<Node*>(drag->itemNew.lParam));
        return 0;
    }

    case NM_CUSTOMDRAW:
        return OnCustomDraw(reinterpret_cast<NMTVCUSTOMDRAW*>(header));
    }
    return 0;
}

LRESULT EditorWindow::OnCustomDraw(NMTVCUSTOMDRAW* draw)
{
    // Custom draw lets the tree paint normally but change details per row.
    // Stage 1 (PREPAINT): ask to be notified for each item. Stage 2
    // (ITEMPREPAINT): entries whose program was not found get red text.
    switch (draw->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT: {
        const auto* node = reinterpret_cast<const Node*>(draw->nmcd.lItemlParam);
        if (node && node->missing && !(draw->nmcd.uItemState & CDIS_SELECTED))
            draw->clrText = kMissingColor;
        return CDRF_DODEFAULT;
    }
    }
    return CDRF_DODEFAULT;
}

// --- Editing operations --------------------------------------------------------------------

EditorWindow::Position EditorWindow::PositionForNewEntry() const
{
    Node* selected = SelectedNode();
    if (!selected)
        return {const_cast<Node*>(&root_), root_.children.size()};
    // A selected submenu receives the new entry at its end; otherwise the
    // entry goes right after the selection.
    if (selected->type == ItemType::Menu)
        return {selected, selected->children.size()};
    const auto& siblings = selected->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == selected; });
    return {selected->parent, static_cast<size_t>(it - siblings.begin()) + 1};
}

void EditorWindow::InsertNode(std::unique_ptr<Node> node, Position position)
{
    Node* raw = node.get();
    raw->parent = position.parent;
    auto& siblings = position.parent->children;
    position.index = std::min(position.index, siblings.size());
    siblings.insert(siblings.begin() + static_cast<std::ptrdiff_t>(position.index), std::move(node));
    InsertIntoTree(raw);
    if (position.parent != &root_) {
        RefreshRow(position.parent); // it may have just got its first child
        TreeView_Expand(tree_, position.parent->handle, TVE_EXPAND);
    }
}

std::unique_ptr<EditorWindow::Node> EditorWindow::DetachNode(Node* node)
{
    if (pendingIconNode_ && IsInside(pendingIconNode_, node))
        FlushIconRefresh();
    RemoveFromTree(node);
    auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
    std::unique_ptr<Node> owned = std::move(*it);
    siblings.erase(it);
    Node* oldParent = node->parent;
    node->parent = nullptr;
    RefreshRow(oldParent); // it may have lost its last child
    return owned;
}

void EditorWindow::MoveNode(Node* node, Position position)
{
    // Moving down within the same parent: removing the node first shifts
    // the later positions up by one.
    if (position.parent == node->parent) {
        const auto& siblings = node->parent->children;
        const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
        if (static_cast<size_t>(it - siblings.begin()) < position.index)
            --position.index;
    }
    InsertNode(DetachNode(node), position);
    Select(node);
    ScheduleSave();
}

bool EditorWindow::IsInside(const Node* node, const Node* ancestor) const
{
    for (const Node* n = node; n; n = n->parent) {
        if (n == ancestor)
            return true;
    }
    return false;
}

void EditorWindow::AddEntry(ItemType type)
{
    auto node = std::make_unique<Node>();
    node->type = type;
    if (type == ItemType::Item)
        node->name = Tr(TextId::EditorNewItem);
    else if (type == ItemType::Menu)
        node->name = Tr(TextId::EditorNewMenu);

    Node* raw = node.get();
    InsertNode(std::move(node), PositionForNewEntry());
    Select(raw);
    ScheduleSave();

    // Ready to type the name.
    if (type != ItemType::Separator) {
        SetFocus(name_);
        Edit_SetSel(name_, 0, -1);
    }
}

void EditorWindow::MoveSelected(int direction)
{
    Node* node = SelectedNode();
    if (!node)
        return;
    const auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
    const size_t index = static_cast<size_t>(it - siblings.begin());
    if (direction < 0 && index > 0)
        MoveNode(node, {node->parent, index - 1});
    else if (direction > 0 && index + 1 < siblings.size())
        MoveNode(node, {node->parent, index + 2}); // after the next sibling
    SetFocus(tree_);
}

void EditorWindow::DeleteSelected()
{
    Node* node = SelectedNode();
    if (!node)
        return;

    // Ask first when a submenu with entries would be lost.
    auto count = [](auto& self, const Node* n) -> size_t {
        size_t total = n->children.size();
        for (const auto& child : n->children)
            total += self(self, child.get());
        return total;
    };
    const size_t inside = count(count, node);
    if (inside > 0) {
        const std::wstring question =
            Format(Tr(TextId::EditorDeleteConfirm), {node->name, std::to_wstring(inside)});
        if (MessageBoxW(window_, question.c_str(), Tr(TextId::AppName), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) !=
            IDYES)
            return;
    }

    // Select a neighbor afterwards: the next entry, else the previous one,
    // else the parent submenu.
    const auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
    Node* next = (it + 1) != siblings.end() ? (it + 1)->get()
                 : it != siblings.begin()   ? (it - 1)->get()
                 : node->parent != &root_   ? node->parent
                                            : nullptr;

    DetachNode(node); // the returned unique_ptr is dropped: the node is deleted here
    if (next)
        Select(next);
    else
        ShowDetails(nullptr);
    ScheduleSave();
}

// --- Detail panel --------------------------------------------------------------------------

void EditorWindow::ShowDetails(Node* node)
{
    fillingDetails_ = true;
    const bool isItem = node && node->type == ItemType::Item;
    const bool hasIcon = node && node->type != ItemType::Separator;
    SetWindowTextW(name_, node && node->type != ItemType::Separator ? node->name.c_str() : L"");
    SetWindowTextW(command_, isItem ? node->command.c_str() : L"");
    fillingDetails_ = false;

    EnableWindow(name_, hasIcon);
    EnableWindow(command_, isItem);
    EnableWindow(browse_, isItem);
    EnableWindow(changeIcon_, hasIcon);
    EnableWindow(resetIcon_, hasIcon && node->icon.has_value());
    EnableWindow(test_, isItem);
    for (HWND button : {buttons_[3], buttons_[4], buttons_[5]}) // up, down, delete
        EnableWindow(button, node != nullptr);
    SetWindowTextW(missingLabel_, isItem && node->missing ? Tr(TextId::EditorTargetMissing) : L"");
    UpdateIconPreview(node);
}

void EditorWindow::UpdateIconPreview(Node* node)
{
    HICON icon = nullptr;
    if (node && node->type != ItemType::Separator)
        icon = icons_.ForItem(ShallowItem(*node), Scale(kPreviewSize));
    // The cache owns the icon; the static control only displays it.
    SendMessageW(iconPreview_, STM_SETICON, reinterpret_cast<WPARAM>(icon), 0);
}

void EditorWindow::OnNameChanged()
{
    Node* node = SelectedNode();
    if (fillingDetails_ || !node)
        return;
    node->name = GetText(name_);
    RefreshRow(node);
    ScheduleSave();
}

void EditorWindow::OnCommandChanged()
{
    Node* node = SelectedNode();
    if (fillingDetails_ || !node || node->type != ItemType::Item)
        return;
    node->command = GetText(command_);
    // Reloading the icon on every key press would be wasteful; wait until
    // typing pauses.
    pendingIconNode_ = node;
    SetTimer(window_, kIconTimer, kIconDelayMs, nullptr);
    ScheduleSave();
}

void EditorWindow::BrowseCommand()
{
    Node* node = SelectedNode();
    if (!node || node->type != ItemType::Item)
        return;
    const std::wstring current = CompleteExtension(ParseCommand(node->command).target);
    const auto file = BrowseForFile(window_, Tr(TextId::EditorBrowse),
                                    {{Tr(TextId::ProgramsFilter), L"*.exe;*.lnk;*.bat;*.cmd;*.com"},
                                     {Tr(TextId::AllFilesFilter), L"*.*"}},
                                    current);
    if (!file)
        return;

    // The same rules as dropping the file on the tree.
    std::optional<ShortcutInfo> shortcut;
    if (IsShortcutFile(*file))
        shortcut = ReadShortcut(*file);
    const MenuItem picked = ItemForDroppedPath(*file, ProbeFileSystem(*file), shortcut ? &*shortcut : nullptr);

    if (!node->icon && picked.icon)
        node->icon = picked.icon;
    SetWindowTextW(command_, picked.command.c_str()); // EN_CHANGE updates the node
    if (node->name.empty() || node->name == Tr(TextId::EditorNewItem))
        SetWindowTextW(name_, picked.name.c_str());
    FlushIconRefresh();
    ShowDetails(node);
}

void EditorWindow::ChangeIcon()
{
    Node* node = SelectedNode();
    if (!node || node->type == ItemType::Separator)
        return;

    // Start with the current icon, or the program's own file, or shell32.
    IconSpec initial{L"shell32.dll", 0};
    if (node->icon) {
        initial = *node->icon;
    } else if (node->type == ItemType::Item) {
        const std::wstring target = CompleteExtension(ParseCommand(node->command).target);
        std::wstring lower = target;
        for (wchar_t& c : lower)
            c = static_cast<wchar_t>(std::towlower(c));
        if (lower.ends_with(L".exe") || lower.ends_with(L".dll") || lower.ends_with(L".ico"))
            initial = {target, 0};
    }

    if (const auto picked = PickIcon(window_, initial, exeDirectory_)) {
        node->icon = *picked;
        ReloadIcon(node);
        ShowDetails(node);
        ScheduleSave();
    }
}

void EditorWindow::ResetIcon()
{
    Node* node = SelectedNode();
    if (!node || !node->icon)
        return;
    node->icon.reset();
    ReloadIcon(node);
    ShowDetails(node);
    ScheduleSave();
}

void EditorWindow::TestEntry()
{
    Node* node = SelectedNode();
    if (node && node->type == ItemType::Item)
        Launch(ShallowItem(*node), window_);
}

// --- Drag and drop -------------------------------------------------------------------------

void EditorWindow::BeginDrag(Node* node)
{
    if (!node)
        return;
    FlushIconRefresh();
    dragged_ = node;
    // Capturing the mouse sends every mouse message to this window until
    // released, even when the pointer leaves it.
    SetCapture(window_);
}

std::optional<EditorWindow::DropTarget> EditorWindow::DropTargetAt(POINT point) const
{
    TVHITTESTINFO hit{};
    hit.pt = point;
    TreeView_HitTest(tree_, &hit);

    DropTarget target;
    if (!hit.hItem) {
        // Below the last row: at the end of the top level.
        RECT client{};
        GetClientRect(tree_, &client);
        if (!PtInRect(&client, point))
            return std::nullopt;
        target.position = {const_cast<Node*>(&root_), root_.children.size()};
        if (!root_.children.empty()) {
            target.mark = root_.children.back()->handle;
            target.after = true;
        }
        return target;
    }

    Node* node = NodeFromHandle(hit.hItem);
    RECT row{};
    TreeView_GetItemRect(tree_, hit.hItem, &row, FALSE);
    const int height = std::max<int>(row.bottom - row.top, 1);
    const int offset = point.y - row.top;
    const auto& siblings = node->parent->children;
    const auto it = std::find_if(siblings.begin(), siblings.end(), [&](const auto& n) { return n.get() == node; });
    const size_t index = static_cast<size_t>(it - siblings.begin());
    target.mark = hit.hItem;

    if (node->type == ItemType::Menu) {
        // Submenu rows have three zones: top quarter = before it, bottom
        // quarter = after it, middle = inside it. An open submenu with
        // children shows them right below, so its bottom zone means
        // "first inside" rather than "after".
        const bool expanded = (TreeView_GetItemState(tree_, hit.hItem, TVIS_EXPANDED) & TVIS_EXPANDED) != 0;
        if (offset < height / 4) {
            target.position = {node->parent, index};
        } else if (offset >= height * 3 / 4 && !(expanded && !node->children.empty())) {
            target.position = {node->parent, index + 1};
            target.after = true;
        } else {
            target.position = {node, expanded ? 0 : node->children.size()};
            target.inside = true;
        }
    } else if (offset < height / 2) {
        target.position = {node->parent, index};
    } else {
        target.position = {node->parent, index + 1};
        target.after = true;
    }
    return target;
}

void EditorWindow::ClearDropMarks()
{
    TreeView_SetInsertMark(tree_, nullptr, FALSE);
    TreeView_SelectDropTarget(tree_, nullptr);
}

void EditorWindow::UpdateDrag(POINT point)
{
    // Scroll when the pointer is near the top or bottom edge.
    RECT client{};
    GetClientRect(tree_, &client);
    if (point.y < Scale(16))
        SendMessageW(tree_, WM_VSCROLL, SB_LINEUP, 0);
    else if (point.y > client.bottom - Scale(16))
        SendMessageW(tree_, WM_VSCROLL, SB_LINEDOWN, 0);

    const auto target = DropTargetAt(point);
    // A submenu cannot be moved into itself or into one of its children.
    const bool valid = target && !IsInside(target->position.parent, dragged_);
    if (!valid) {
        ClearDropMarks();
        SetCursor(LoadCursorW(nullptr, IDC_NO));
        return;
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    if (target->inside) {
        TreeView_SetInsertMark(tree_, nullptr, FALSE);
        TreeView_SelectDropTarget(tree_, target->mark); // highlight the submenu
    } else {
        TreeView_SelectDropTarget(tree_, nullptr);
        TreeView_SetInsertMark(tree_, target->mark, target->after); // a line between rows
    }
}

void EditorWindow::EndDrag(bool drop)
{
    Node* node = dragged_;
    dragged_ = nullptr; // first, because ReleaseCapture sends WM_CAPTURECHANGED
    ReleaseCapture();
    ClearDropMarks();
    if (!drop || !node)
        return;

    POINT point{};
    GetCursorPos(&point);
    ScreenToClient(tree_, &point);
    const auto target = DropTargetAt(point);
    if (target && !IsInside(target->position.parent, node))
        MoveNode(node, target->position);
}

void EditorWindow::OnDropFiles(HDROP drop)
{
    // Files dragged from Explorer. HDROP is a handle to the list of paths;
    // DragQueryFileW with index 0xFFFFFFFF returns how many there are.
    POINT point{};
    DragQueryPoint(drop, &point); // where they were dropped, in our client coordinates
    MapWindowPoints(window_, tree_, &point, 1);

    Position position = PositionForNewEntry();
    RECT treeRect{};
    GetClientRect(tree_, &treeRect);
    if (PtInRect(&treeRect, point)) {
        if (const auto target = DropTargetAt(point))
            position = target->position;
    }

    Node* last = nullptr;
    const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        const UINT length = DragQueryFileW(drop, i, nullptr, 0);
        std::wstring path(length, L'\0');
        DragQueryFileW(drop, i, path.data(), length + 1);

        const PathKind kind = ProbeFileSystem(path);
        std::optional<ShortcutInfo> shortcut;
        if (kind == PathKind::File && IsShortcutFile(path))
            shortcut = ReadShortcut(path);

        auto node = NodeFromItem(ItemForDroppedPath(path, kind, shortcut ? &*shortcut : nullptr), nullptr);
        last = node.get();
        InsertNode(std::move(node), position);
        ++position.index; // several files keep their order
    }
    DragFinish(drop); // frees the list

    if (last) {
        Select(last);
        SetForegroundWindow(window_);
        ScheduleSave();
    }
}

// --- Import a configuration ------------------------------------------------------------

void EditorWindow::ImportConfiguration()
{
    const auto file = BrowseForFile(window_, Tr(TextId::EditorImport),
                                    {{Tr(TextId::ImportFilter), L"*.xml"},
                                     {Tr(TextId::AllFilesFilter), L"*.*"}});
    if (!file)
        return;

    // The file is only read: the original configuration is never modified.
    const auto root = ReadXmlFile(*file);
    const auto items = root ? ImportMenuXml(*root) : std::nullopt;
    if (!items) {
        MessageBoxW(window_, Format(Tr(TextId::ImportFailed), {*file}).c_str(), Tr(TextId::AppName),
                    MB_OK | MB_ICONWARNING);
        return;
    }

    bool replace = false;
    if (!root_.children.empty()) {
        // TaskDialogIndirect is the modern message box: it allows buttons
        // with any text ("command links" here, the big buttons with an
        // arrow), not just Yes/No/Cancel.
        constexpr int kReplace = 1001;
        constexpr int kAppend = 1002;
        const TASKDIALOG_BUTTON buttons[] = {{kAppend, Tr(TextId::ImportAppend)},
                                             {kReplace, Tr(TextId::ImportReplace)}};
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config);
        config.hwndParent = window_;
        config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
        config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        config.pszWindowTitle = Tr(TextId::AppName);
        config.pszMainInstruction = Tr(TextId::EditorImport);
        config.pszContent = Tr(TextId::ImportQuestion);
        config.cButtons = ARRAYSIZE(buttons);
        config.pButtons = buttons;
        config.nDefaultButton = kAppend;
        int chosen = IDCANCEL;
        if (FAILED(TaskDialogIndirect(&config, &chosen, nullptr, nullptr)) || chosen == IDCANCEL)
            return;
        replace = chosen == kReplace;
    }

    FlushIconRefresh();
    if (replace) {
        TreeView_DeleteAllItems(tree_);
        root_.children.clear();
    }

    Position position{&root_, root_.children.size()};
    Node* first = nullptr;
    for (const MenuItem& item : *items) {
        auto node = NodeFromItem(item, nullptr);
        if (!first)
            first = node.get();
        InsertNode(std::move(node), position);
        ++position.index;
    }
    if (first)
        Select(first);
    else
        ShowDetails(nullptr);
    ScheduleSave();

    MessageBoxW(window_, Format(Tr(TextId::ImportDone), {std::to_wstring(CountEntries(*items))}).c_str(),
                Tr(TextId::AppName), MB_OK | MB_ICONINFORMATION);
}

// --- Saving -----------------------------------------------------------------------------------

void EditorWindow::ScheduleSave()
{
    savePending_ = true;
    // SetTimer with an ID that already runs restarts it: the save happens
    // kSaveDelayMs after the *last* change.
    SetTimer(window_, kSaveTimer, kSaveDelayMs, nullptr);
}

void EditorWindow::SaveNow()
{
    if (window_)
        KillTimer(window_, kSaveTimer);
    if (!savePending_)
        return;
    savePending_ = false;
    if (onChanged_)
        onChanged_(CurrentItems());
}

} // namespace popa
