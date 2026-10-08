// Small helpers shared by Popa's windows.
#pragma once

#include <windows.h>

#include <string>

namespace popa {

// Converts a size designed at 96 DPI (100 % scaling) to the given DPI.
inline int ScaleForDpi(int value, int dpi)
{
    return MulDiv(value, dpi, 96);
}

// The font Windows uses for dialog text ("message font", Segoe UI on
// Windows 11) at the given DPI. The caller deletes it with DeleteObject.
inline HFONT CreateMessageFont(int dpi)
{
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, static_cast<UINT>(dpi));
    return CreateFontIndirectW(&metrics.lfMessageFont);
}

// Text of a window or control.
inline std::wstring GetText(HWND window)
{
    std::wstring text(static_cast<size_t>(GetWindowTextLengthW(window)), L'\0');
    if (!text.empty())
        GetWindowTextW(window, text.data(), static_cast<int>(text.size()) + 1);
    return text;
}

// The system's text for an error code, in the Windows display language
// ("The system cannot find the file specified.").
inline std::wstring SystemErrorText(DWORD error)
{
    wchar_t* buffer = nullptr;
    // FORMAT_MESSAGE_ALLOCATE_BUFFER: Windows allocates the text, which is
    // then freed with LocalFree. The cast is required by that flag: the
    // function writes the pointer into the "buffer" parameter.
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error,
        0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring text = length ? std::wstring(buffer, length) : L"#" + std::to_wstring(error);
    LocalFree(buffer);
    while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L' '))
        text.pop_back();
    return text;
}

// Width in pixels of a text drawn with a font.
inline int TextWidth(HFONT font, const std::wstring& text)
{
    HDC dc = GetDC(nullptr);
    HGDIOBJ old = SelectObject(dc, font);
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    return size.cx;
}

// Sends WM_SETFONT to every child window.
inline void SetFontOnChildren(HWND parent, HFONT font)
{
    EnumChildWindows(
        parent,
        [](HWND child, LPARAM font) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(font));
}

// Runs a modal message loop for `dialog` until `done` becomes true.
//
// How a modal window works without a dialog template (CreateDialog):
// 1. The caller disables the owner (EnableWindow(owner, FALSE)) so clicks
//    on it are ignored, and creates the dialog owned by it.
// 2. This loop runs until the dialog sets `done`. Messages go through
//    IsDialogMessageW, which gives the dialog keyboard behavior: Tab
//    between controls, Enter = IDOK, Esc = IDCANCEL.
// 3. Afterwards the caller enables the owner *before* destroying the
//    dialog, so Windows gives the activation back to the owner and not to
//    some other program.
// WM_QUIT belongs to the main loop: when it shows up here it is posted
// again and the loop ends.
inline void RunModalLoop(HWND dialog, const bool& done)
{
    MSG message;
    while (!done) {
        const BOOL got = GetMessageW(&message, nullptr, 0, 0);
        if (got <= 0) {
            PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

} // namespace popa
