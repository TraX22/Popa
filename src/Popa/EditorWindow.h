// The menu editor: a tree with the entries, a row of buttons and, below,
// the details of the selected entry. Changes are saved automatically.
#pragma once

#include "Popa/IconCache.h"
#include "PopaCore/Model.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace popa {

class EditorWindow {
public:
    // Called with the whole menu after every change (a short moment after
    // typing stops, so saving does not happen on every key press).
    using ChangedCallback = std::function<void(std::vector<MenuItem>)>;
    // Called when the window has been destroyed.
    using ClosedCallback = std::function<void()>;

    EditorWindow(IconCache& icons, std::wstring exeDirectory, ChangedCallback onChanged, ClosedCallback onClosed);
    ~EditorWindow();
    EditorWindow(const EditorWindow&) = delete;
    EditorWindow& operator=(const EditorWindow&) = delete;

    // Creates and shows the window with a copy of the menu.
    bool Create(const std::vector<MenuItem>& items, bool darkTitleBar);

    // Brings the window to the front (restoring it when minimized).
    void Activate();

    // Dark or light title bar, following the theme.
    void SetDarkTitleBar(bool dark);

    // Gives the window its dialog keyboard behavior (Tab between
    // controls). Must be called from the main message loop for every
    // message; returns true when the message was handled.
    bool PreTranslateMessage(MSG& message);

    // Shows the texts again in the current language.
    void ApplyLanguage();

    // Saves pending changes now (before the program exits).
    void FlushChanges();

private:
    // The editor works on its own tree of nodes. unique_ptr keeps every
    // node at a fixed address, so the TreeView can store a pointer to it.
    struct Node {
        ItemType type = ItemType::Item;
        std::wstring name;
        std::wstring command;
        std::optional<IconSpec> icon;
        std::vector<std::unique_ptr<Node>> children;
        Node* parent = nullptr;
        HTREEITEM handle = nullptr; // its row in the TreeView
        int image = -1;             // index in the image list; -1 = not loaded
        bool missing = false;       // the command's target was not found
    };

    // Where a new or moved entry goes.
    struct Position {
        Node* parent = nullptr;
        size_t index = 0;
    };

    // Where a drag would drop, and how to show it in the tree: a line
    // before/after `mark`, or `mark` highlighted when dropping inside it.
    struct DropTarget {
        Position position;
        HTREEITEM mark = nullptr;
        bool after = false;
        bool inside = false;
    };

    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // Window setup
    void CreateControls();
    void ApplyDpi(int dpi);
    void Layout();
    int Scale(int value) const;

    // Model <-> nodes
    std::unique_ptr<Node> NodeFromItem(const MenuItem& item, Node* parent) const;
    MenuItem ItemFromNode(const Node& node) const;
    static MenuItem ShallowItem(const Node& node); // without children, for icons and launching
    std::vector<MenuItem> CurrentItems() const;

    // Tree view
    void InsertIntoTree(Node* node);
    void RemoveFromTree(Node* node);
    void RefreshRow(Node* node);
    int ImageFor(Node* node);
    void ReloadIcon(Node* node);
    void FlushIconRefresh();
    void ResetImages();
    Node* NodeFromHandle(HTREEITEM handle) const;
    Node* SelectedNode() const;
    void Select(Node* node);
    LRESULT OnTreeNotify(NMHDR* header);
    LRESULT OnCustomDraw(NMTVCUSTOMDRAW* draw);

    // Editing operations
    Position PositionForNewEntry() const;
    void InsertNode(std::unique_ptr<Node> node, Position position);
    std::unique_ptr<Node> DetachNode(Node* node);
    void MoveNode(Node* node, Position position);
    void AddEntry(ItemType type);
    void MoveSelected(int direction);
    void DeleteSelected();
    bool IsInside(const Node* node, const Node* ancestor) const;

    // Detail panel
    void ShowDetails(Node* node);
    void OnNameChanged();
    void OnCommandChanged();
    void BrowseCommand();
    void ChangeIcon();
    void ResetIcon();
    void TestEntry();
    void UpdateIconPreview(Node* node);

    // Dragging inside the tree, and files dropped from Windows
    void BeginDrag(Node* node);
    void UpdateDrag(POINT treePoint);
    void EndDrag(bool drop);
    std::optional<DropTarget> DropTargetAt(POINT treePoint) const;
    void ClearDropMarks();
    void OnDropFiles(HDROP drop);

    // Import a configuration
    void ImportConfiguration();

    // Saving
    void ScheduleSave();
    void SaveNow();

    IconCache& icons_;
    std::wstring exeDirectory_;
    ChangedCallback onChanged_;
    ClosedCallback onClosed_;

    HWND window_ = nullptr;
    HWND tree_ = nullptr;
    std::vector<HWND> buttons_;
    HWND nameLabel_ = nullptr;
    HWND name_ = nullptr;
    HWND commandLabel_ = nullptr;
    HWND command_ = nullptr;
    HWND browse_ = nullptr;
    HWND iconLabel_ = nullptr;
    HWND iconPreview_ = nullptr;
    HWND changeIcon_ = nullptr;
    HWND resetIcon_ = nullptr;
    HWND test_ = nullptr;
    HWND missingLabel_ = nullptr;
    HFONT font_ = nullptr;
    HIMAGELIST images_ = nullptr;
    int dpi_ = 96;
    int treeIconSize_ = 16;

    Node root_; // invisible root; its children are the top-level entries
    bool fillingDetails_ = false; // ignore edit notifications while filling the panel
    bool savePending_ = false;

    Node* dragged_ = nullptr;          // node being dragged inside the tree
    Node* pendingIconNode_ = nullptr;  // command edited; icon refresh waiting on a timer
};

} // namespace popa
