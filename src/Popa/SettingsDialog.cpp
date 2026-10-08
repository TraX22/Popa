#include "Popa/SettingsDialog.h"

#include "Popa/UiUtil.h"
#include "PopaCore/HotkeyText.h"
#include "PopaCore/Localization.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace popa {

namespace {

constexpr wchar_t kWindowClass[] = L"Popa.Settings";
constexpr COLORREF kErrorColor = RGB(196, 43, 28);

enum ControlId : int {
    kIdWin = 300,
    kIdCtrl,
    kIdAlt,
    kIdShift,
    kIdKey,
    kIdHotkeyError,
    kIdIconSize,
    kIdTheme,
    kIdLanguage,
    kIdRunAtStartup,
    kIdCheckUpdates,
    kIdLabel,
};

constexpr std::array<int, 4> kIconSizes = {16, 24, 32, 48};
constexpr std::array<Theme, 3> kThemes = {Theme::System, Theme::Light, Theme::Dark};
constexpr std::array<LanguageSetting, 3> kLanguages = {LanguageSetting::System, LanguageSetting::English,
                                                       LanguageSetting::Spanish};

// Sizes at 96 DPI.
constexpr int kMargin = 12;
constexpr int kGap = 10;
constexpr int kRowHeight = 24;
constexpr int kButtonWidth = 96;
constexpr int kButtonHeight = 28;
constexpr int kCheckboxExtra = 28; // the box and the space before the text

HWND g_openDialog = nullptr; // so a second "Options" click can find it

class SettingsDialog {
public:
    SettingsDialog(const Settings& current, bool dark) : settings_(current), dark_(dark) {}
    std::optional<Settings> Run(HWND owner);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void CreateControls();
    void LoadValues();
    SIZE Layout(bool apply); // computes (and optionally applies) the layout; returns the client size
    std::optional<Hotkey> ChosenHotkey() const;
    void Validate();
    void Accept();
    int Scale(int value) const { return ScaleForDpi(value, dpi_); }

    Settings settings_;
    bool dark_ = false;
    HWND window_ = nullptr;
    HWND hotkeyLabel_ = nullptr;
    std::array<HWND, 4> modifiers_{}; // Win, Ctrl, Alt, Shift
    HWND plus_ = nullptr;
    HWND key_ = nullptr;
    HWND hotkeyError_ = nullptr;
    HWND iconSizeLabel_ = nullptr;
    HWND iconSize_ = nullptr;
    HWND themeLabel_ = nullptr;
    HWND theme_ = nullptr;
    HWND languageLabel_ = nullptr;
    HWND language_ = nullptr;
    HWND runAtStartup_ = nullptr;
    HWND checkUpdates_ = nullptr;
    HWND ok_ = nullptr;
    HWND cancel_ = nullptr;
    HFONT font_ = nullptr;
    int dpi_ = 96;
    std::vector<std::wstring> keyNames_ = HotkeyKeyNames();
    bool done_ = false;
    std::optional<Settings> result_;
};

std::optional<Settings> SettingsDialog::Run(HWND owner)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = &SettingsDialog::WindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClass;
    RegisterClassExW(&windowClass);

    // The owner is Popa's invisible window, so the dialog is centered on
    // the monitor where the mouse is instead of over the owner.
    POINT cursor{};
    GetCursorPos(&cursor);
    HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    GetMonitorInfoW(monitor, &monitorInfo);
    UINT dpiX = 96;
    UINT dpiY = 96;
    if (FAILED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
        dpiX = 96;
    dpi_ = static_cast<int>(dpiX);

    const DWORD style = WS_CAPTION | WS_SYSMENU;
    EnableWindow(owner, FALSE);
    window_ = CreateWindowExW(WS_EX_DLGMODALFRAME, kWindowClass, Tr(TextId::SettingsTitle), style,
                              monitorInfo.rcWork.left, monitorInfo.rcWork.top, Scale(400), Scale(300), owner, nullptr,
                              GetModuleHandleW(nullptr), this);
    if (!window_) {
        EnableWindow(owner, TRUE);
        return std::nullopt;
    }
    dpi_ = static_cast<int>(GetDpiForWindow(window_));

    // Size the window around the controls, then center it.
    const SIZE client = Layout(false);
    RECT rect{0, 0, client.cx, client.cy};
    AdjustWindowRectExForDpi(&rect, style, FALSE, WS_EX_DLGMODALFRAME, static_cast<UINT>(dpi_));
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    const RECT& work = monitorInfo.rcWork;
    SetWindowPos(window_, nullptr, work.left + (work.right - work.left - width) / 2,
                 work.top + (work.bottom - work.top - height) / 2, width, height, SWP_NOZORDER);
    Layout(true);

    const BOOL darkValue = dark_ ? TRUE : FALSE;
    DwmSetWindowAttribute(window_, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkValue, sizeof(darkValue));

    g_openDialog = window_;
    ShowWindow(window_, SW_SHOW);
    SetForegroundWindow(window_);
    SetFocus(modifiers_[0]);

    RunModalLoop(window_, done_); // see UiUtil.h

    g_openDialog = nullptr;
    EnableWindow(owner, TRUE);
    DestroyWindow(window_);
    return result_;
}

LRESULT CALLBACK SettingsDialog::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE) {
        auto* dialog = static_cast<SettingsDialog*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        dialog->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
    }
    auto* dialog = reinterpret_cast<SettingsDialog*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    return dialog ? dialog->HandleMessage(message, wParam, lParam) : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT SettingsDialog::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE:
        CreateControls();
        LoadValues();
        Validate();
        return 0;

    case WM_CTLCOLORSTATIC: {
        // Labels and check boxes ask their parent for colors. White
        // background like the window; the hotkey error in red.
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, GetSysColor(COLOR_WINDOW));
        SetTextColor(dc, reinterpret_cast<HWND>(lParam) == hotkeyError_ ? kErrorColor
                                                                          : GetSysColor(COLOR_WINDOWTEXT));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_COMMAND: {
        const int id = LOWORD(wParam);
        const int code = HIWORD(wParam);
        if (id == IDOK) {
            Accept();
        } else if (id == IDCANCEL) {
            done_ = true;
        } else if ((id >= kIdWin && id <= kIdShift && code == BN_CLICKED) ||
                   (id == kIdKey && code == CBN_SELCHANGE)) {
            Validate();
        }
        return 0;
    }

    case WM_DPICHANGED: {
        // Moved to a monitor with other scaling: new font and layout.
        dpi_ = HIWORD(wParam);
        if (font_)
            DeleteObject(font_);
        font_ = CreateMessageFont(dpi_);
        SetFontOnChildren(window_, font_);
        const auto* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(window_, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        Layout(true);
        return 0;
    }

    case WM_CLOSE:
        done_ = true;
        return 0;

    case WM_DESTROY:
        if (font_)
            DeleteObject(font_);
        font_ = nullptr;
        return 0;
    }
    return DefWindowProcW(window_, message, wParam, lParam);
}

void SettingsDialog::CreateControls()
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    auto create = [&](const wchar_t* className, const wchar_t* text, DWORD style, int id) {
        return CreateWindowExW(0, className, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, window_,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    };
    auto checkbox = [&](const wchar_t* text, int id) {
        return create(WC_BUTTONW, text, WS_TABSTOP | BS_AUTOCHECKBOX, id);
    };
    // CBS_DROPDOWNLIST: a list to pick from, without free typing.
    auto combo = [&](int id) { return create(WC_COMBOBOXW, L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, id); };

    hotkeyLabel_ = create(WC_STATICW, Tr(TextId::SettingsHotkey), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    // The modifier names are the same in both languages (as on keyboards).
    modifiers_ = {checkbox(L"Win", kIdWin), checkbox(L"Ctrl", kIdCtrl), checkbox(L"Alt", kIdAlt),
                  checkbox(L"Shift", kIdShift)};
    plus_ = create(WC_STATICW, L"+", SS_CENTER | SS_CENTERIMAGE, kIdLabel);
    key_ = combo(kIdKey);
    hotkeyError_ = create(WC_STATICW, L"", SS_LEFT, kIdHotkeyError);

    iconSizeLabel_ = create(WC_STATICW, Tr(TextId::SettingsIconSize), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    iconSize_ = combo(kIdIconSize);
    themeLabel_ = create(WC_STATICW, Tr(TextId::SettingsTheme), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    theme_ = combo(kIdTheme);
    languageLabel_ = create(WC_STATICW, Tr(TextId::SettingsLanguage), SS_LEFT | SS_CENTERIMAGE, kIdLabel);
    language_ = combo(kIdLanguage);
    runAtStartup_ = checkbox(Tr(TextId::SettingsRunAtStartup), kIdRunAtStartup);
    checkUpdates_ = checkbox(Tr(TextId::SettingsCheckUpdates), kIdCheckUpdates);
    ok_ = create(WC_BUTTONW, Tr(TextId::CommonOk), WS_TABSTOP | BS_DEFPUSHBUTTON, IDOK);
    cancel_ = create(WC_BUTTONW, Tr(TextId::CommonCancel), WS_TABSTOP | BS_PUSHBUTTON, IDCANCEL);

    for (const std::wstring& name : keyNames_)
        ComboBox_AddString(key_, name.c_str());
    for (int size : kIconSizes)
        ComboBox_AddString(iconSize_, Format(Tr(TextId::SettingsPixels), {std::to_wstring(size)}).c_str());
    ComboBox_AddString(theme_, Tr(TextId::ThemeSystem));
    ComboBox_AddString(theme_, Tr(TextId::ThemeLight));
    ComboBox_AddString(theme_, Tr(TextId::ThemeDark));
    // Each language is named in itself, so it can be found whatever the
    // current interface language is.
    ComboBox_AddString(language_, Tr(TextId::LanguageSystem));
    ComboBox_AddString(language_, L"English");
    ComboBox_AddString(language_, L"Español");

    font_ = CreateMessageFont(dpi_);
    SetFontOnChildren(window_, font_);
}

void SettingsDialog::LoadValues()
{
    const Hotkey hotkey = ParseHotkey(settings_.hotkey).value_or(*ParseHotkey(Settings{}.hotkey));
    const bool states[] = {hotkey.win, hotkey.ctrl, hotkey.alt, hotkey.shift};
    for (size_t i = 0; i < modifiers_.size(); ++i)
        Button_SetCheck(modifiers_[i], states[i] ? BST_CHECKED : BST_UNCHECKED);

    // The key: find its name in the list ("Ctrl+Z" -> "Z").
    const std::wstring text = FormatHotkey(hotkey);
    const std::wstring keyName = text.substr(text.rfind(L'+') == std::wstring::npos ? 0 : text.rfind(L'+') + 1);
    const auto it = std::find(keyNames_.begin(), keyNames_.end(), keyName);
    ComboBox_SetCurSel(key_, it == keyNames_.end() ? 0 : static_cast<int>(it - keyNames_.begin()));

    const auto size = std::find(kIconSizes.begin(), kIconSizes.end(), settings_.iconSize);
    ComboBox_SetCurSel(iconSize_, size == kIconSizes.end() ? 2 : static_cast<int>(size - kIconSizes.begin()));
    const auto theme = std::find(kThemes.begin(), kThemes.end(), settings_.theme);
    ComboBox_SetCurSel(theme_, static_cast<int>(theme - kThemes.begin()));
    const auto language = std::find(kLanguages.begin(), kLanguages.end(), settings_.language);
    ComboBox_SetCurSel(language_, static_cast<int>(language - kLanguages.begin()));

    Button_SetCheck(runAtStartup_, settings_.runAtStartup ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(checkUpdates_, settings_.checkUpdatesAtStartup ? BST_CHECKED : BST_UNCHECKED);
}

SIZE SettingsDialog::Layout(bool apply)
{
    const int m = Scale(kMargin);
    const int gap = Scale(kGap);
    const int row = Scale(kRowHeight);
    const int buttonWidth = Scale(kButtonWidth);
    const int buttonHeight = Scale(kButtonHeight);
    auto textWidth = [&](HWND control) { return TextWidth(font_, GetText(control)); };
    auto checkboxWidth = [&](HWND control) { return textWidth(control) + Scale(kCheckboxExtra); };

    const int labelWidth =
        std::max({textWidth(hotkeyLabel_), textWidth(iconSizeLabel_), textWidth(themeLabel_),
                  textWidth(languageLabel_)}) +
        Scale(14);
    const int fieldX = m + labelWidth;

    HDWP batch = apply ? BeginDeferWindowPos(18) : nullptr;
    auto place = [&](HWND control, int x, int y, int w, int h) {
        if (apply)
            batch = DeferWindowPos(batch, control, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    };

    // Row 1: hotkey = four check boxes, "+", key list.
    int y = m;
    int x = fieldX;
    place(hotkeyLabel_, m, y, labelWidth, row);
    for (HWND modifier : modifiers_) {
        const int w = checkboxWidth(modifier);
        place(modifier, x, y, w, row);
        x += w + Scale(4);
    }
    place(plus_, x, y, Scale(16), row);
    x += Scale(16) + Scale(6);
    const int keyWidth = Scale(110);
    // A combo box's height includes its open list: 12 rows of choices.
    place(key_, x, y, keyWidth, row * 12);
    int right = x + keyWidth;
    y += row + Scale(4);

    // Row 2: the reason when the combination cannot be used.
    place(hotkeyError_, fieldX, y, right - fieldX, Scale(36));
    y += Scale(36) + gap;

    place(iconSizeLabel_, m, y, labelWidth, row);
    place(iconSize_, fieldX, y, Scale(120), row * 6);
    y += row + gap;

    const int themeWidth = std::max(Scale(160), TextWidth(font_, Tr(TextId::ThemeSystem)) + Scale(40));
    place(themeLabel_, m, y, labelWidth, row);
    place(theme_, fieldX, y, themeWidth, row * 6);
    right = std::max(right, fieldX + themeWidth);
    y += row + gap;

    place(languageLabel_, m, y, labelWidth, row);
    place(language_, fieldX, y, themeWidth, row * 6);
    y += row + gap + Scale(4);

    const int startupWidth = checkboxWidth(runAtStartup_);
    const int updatesWidth = checkboxWidth(checkUpdates_);
    place(runAtStartup_, m, y, startupWidth, row);
    y += row + Scale(4);
    place(checkUpdates_, m, y, updatesWidth, row);
    right = std::max({right, m + startupWidth, m + updatesWidth, m + 2 * buttonWidth + gap});
    y += row + gap + Scale(8);

    const int width = right + m;
    place(cancel_, width - m - buttonWidth, y, buttonWidth, buttonHeight);
    place(ok_, width - m - 2 * buttonWidth - Scale(8), y, buttonWidth, buttonHeight);
    y += buttonHeight + m;

    if (apply)
        EndDeferWindowPos(batch);
    return SIZE{width, y};
}

std::optional<Hotkey> SettingsDialog::ChosenHotkey() const
{
    std::wstring text;
    const wchar_t* names[] = {L"Win+", L"Ctrl+", L"Alt+", L"Shift+"};
    for (size_t i = 0; i < modifiers_.size(); ++i) {
        if (Button_GetCheck(modifiers_[i]) == BST_CHECKED)
            text += names[i];
    }
    const int key = ComboBox_GetCurSel(key_);
    if (key < 0 || static_cast<size_t>(key) >= keyNames_.size())
        return std::nullopt;
    return ParseHotkey(text + keyNames_[static_cast<size_t>(key)]);
}

void SettingsDialog::Validate()
{
    // The same rules as settings.json (HotkeyText): the dialog never lets
    // through a combination that would get in the way of typing.
    const bool valid = ChosenHotkey().has_value();
    SetWindowTextW(hotkeyError_, valid ? L"" : Tr(TextId::SettingsHotkeyInvalid));
    EnableWindow(ok_, valid);
}

void SettingsDialog::Accept()
{
    const auto hotkey = ChosenHotkey();
    if (!hotkey)
        return;
    Settings result = settings_; // keeps hotkeysEnabled
    result.hotkey = FormatHotkey(*hotkey);
    result.iconSize = kIconSizes[static_cast<size_t>(std::max(ComboBox_GetCurSel(iconSize_), 0))];
    result.theme = kThemes[static_cast<size_t>(std::max(ComboBox_GetCurSel(theme_), 0))];
    result.language = kLanguages[static_cast<size_t>(std::max(ComboBox_GetCurSel(language_), 0))];
    result.runAtStartup = Button_GetCheck(runAtStartup_) == BST_CHECKED;
    result.checkUpdatesAtStartup = Button_GetCheck(checkUpdates_) == BST_CHECKED;
    result_ = result;
    done_ = true;
}

} // namespace

std::optional<Settings> ShowSettingsDialog(HWND owner, const Settings& current, bool darkTitleBar)
{
    SettingsDialog dialog(current, darkTitleBar);
    return dialog.Run(owner);
}

bool ActivateSettingsDialog()
{
    if (!g_openDialog)
        return false;
    SetForegroundWindow(g_openDialog);
    return true;
}

} // namespace popa
