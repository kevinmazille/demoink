// demoHelper - screen drawing and presentation tool

// Copyright (C) 2007-2008, 2012, 2020-2021 - Stefan Kueng

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#pragma once

#include "BaseWindow.h"
#include "resource.h"
#include "hyperlink.h"
#include "ResString.h"
#include <shellapi.h>
#include <vector>
#include <deque>
#include <string>

#define DRAW_HOTKEY 100

#define TIMER_ID_DRAW 101
#define TIMER_ID_FADE 103
#define TIMER_ID_CARET 104

#define LINE_ALPHA 100

enum class LineType
{
    Hand,
    Straight,
    Arrow,
    Rectangle,
    Ellipse,
    Text
};

enum class Theme
{
    Transparent = 0,
    Light       = 1,
    Dark        = 2
};

// Decorative "board" frame painted on top of the theme background.
// Toggled with the N key, independently of the plain B themes.
enum class BoardStyle
{
    None   = 0, // no frame (plain B themes)
    FrameA = 1, // light whiteboard: clay liseré, mat, corner ticks
    FrameB = 2, // dark slate: bevelled frame, clay baseline
    Image  = 3  // user-provided background image (Background/image), letterboxed
};

class DrawLine
{
public:
    DrawLine()
    {
    }

    LineType                    lineType  = LineType::Hand;
    int                         lineIndex = 0;
    std::vector<Gdiplus::Point> points;
    BYTE                        alpha = 100;

    Gdiplus::Point lineStartPoint = {-1, -1};
    Gdiplus::Point lineEndPoint   = {-1, -1};

    int penWidth   = 1;
    int colorIndex = 0;

    std::wstring text;
    int          fontSize = 24;
    std::wstring fontName = L"Segoe Print";
};

class CMainWindow : public CWindow
{
public:
    explicit CMainWindow(HINSTANCE hInst, const WNDCLASSEX* wcx = nullptr)
        : CWindow(hInst, wcx)
        , niData({0})
        , hDesktopCompatibleDC(nullptr)
        , hDesktopCompatibleBitmap(nullptr)
        , hOldBmp(nullptr)
        , m_bDrawing(false)
        , m_colorIndex(1)
        , m_currentPenWidth(6)
        , m_currentAlpha(LINE_ALPHA)
        , m_fadeSeconds(0)
        , m_lineStartShiftPoint({})
        , m_hCursor(nullptr)
        , m_hPreviousCursor(nullptr)
        , m_rcScreen({0})
    {
        SetWindowTitle(static_cast<LPCTSTR>(ResString(hResource, IDS_APP_TITLE)));
        ApplyTheme();
    };
    ~CMainWindow(){};

    bool RegisterAndCreateWindow();

    bool IsInTextMode() const { return m_bTextMode; }

protected:
    /// the message handler for this window
    LRESULT CALLBACK WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;

    /// Handles all the WM_COMMAND window messages (e.g. menu commands)
    LRESULT DoCommand(int id);

    bool    StartPresentationMode();
    bool    EndPresentationMode();
    HCURSOR CreateDrawCursor(COLORREF color, int penwidth);
    void    ApplyTheme();
    void    PaintThemeBackground();
    void    PaintBoardFrame();
    void    RenderAnnotations(Gdiplus::Graphics& graphics);
    void    RenderTextCaret(Gdiplus::Graphics& graphics);
    void    SaveScreenshot();

    // Options is a tabbed PropertySheet; one property-page proc per tab.
    static void             ShowOptionsSheet(HWND hParent);
    static INT_PTR CALLBACK GeneralPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK DrawPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK TextPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ColorsPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ScreenshotPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK ShortcutsPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static INT_PTR CALLBACK BackgroundPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static WORD             HotKeyControl2HotKey(WORD hk);
    static WORD             HotKey2HotKeyControl(WORD hk);

    // Solid-fill background colors for the Q themes. Defaults are white (light)
    // and black (dark); each is overridable via [Background] light/dark in the
    // INI. BackgroundColor() resolves the value for the current theme.
    static constexpr COLORREF DEFAULT_BG_LIGHT = RGB(255, 255, 255);
    static constexpr COLORREF DEFAULT_BG_DARK  = RGB(0, 0, 0);
    static COLORREF           BackgroundColor(bool dark);
    // Path to the optional board-mode background image (Background/image).
    // Empty when none is set; when set, it adds a third step to the Z cycle.
    static std::wstring BackgroundImagePath();

    // Rebindable single-letter shortcuts for the main draw actions. Each entry
    // maps a command id to its [Shortcuts] INI key, built-in default letter and
    // the edit control on the Shortcuts page. Everything else (digits, arrows,
    // Esc, Backspace, Delete, F1) stays fixed. See BuildAcceleratorTable.
    struct ShortcutDef
    {
        int            cmd;
        const wchar_t* iniKey;
        wchar_t        defaultLetter;
        int            editId;
    };
    static constexpr ShortcutDef SHORTCUTS[] = {
        {ID_CMD_TEXTMODE, L"textmode", L'A', IDC_KEY_TEXTMODE},
        {ID_CMD_CLEARLINES, L"clearlines", L'W', IDC_KEY_CLEARLINES},
        {ID_CMD_TOGGLETHEME, L"toggletheme", L'Q', IDC_KEY_TOGGLETHEME},
        {ID_CMD_CYCLEBOARD, L"cycleboard", L'Z', IDC_KEY_CYCLEBOARD},
        {ID_CMD_TOGGLEROP, L"togglerop", L'X', IDC_KEY_TOGGLEROP},
    };
    // Reads the configured letter for an action from the INI, uppercased and
    // validated A-Z, falling back to its built-in default.
    static wchar_t ResolveShortcutLetter(const ShortcutDef& def);

    // Builds the keyboard accelerator table at runtime from the [Shortcuts]
    // INI section plus the fixed keys. Caller owns the returned HACCEL.
    static HACCEL BuildAcceleratorTable();
    // Destroys the current table and rebuilds it from the INI (after Options).
    void RebuildAcceleratorTable();

public:
    HACCEL AcceleratorTable() const { return m_hAccel; }

protected:

    // Built-in palettes (10 colors each). Used as the fallback when no custom
    // color is stored in the INI, and as the target of "Reset to defaults".
    // Light is shared with the Transparent theme; Dark is tuned for black.
    static constexpr COLORREF DEFAULT_COLORS_LIGHT[10] = {
        RGB(255, 255, 0), RGB(255, 0, 0), RGB(0, 80, 220), RGB(0, 170, 0), RGB(150, 0, 0),
        RGB(0, 0, 150), RGB(0, 100, 0), RGB(0, 0, 0), RGB(120, 120, 120), RGB(200, 0, 200)};
    static constexpr COLORREF DEFAULT_COLORS_DARK[10] = {
        RGB(255, 255, 0), RGB(255, 140, 0), RGB(255, 90, 90), RGB(0, 220, 255), RGB(220, 150, 80),
        RGB(255, 120, 150), RGB(140, 180, 255), RGB(255, 255, 255), RGB(180, 180, 180), RGB(120, 255, 120)};

    // Text-annotation font: the picker offers this fixed list, and the
    // configured choice falls back to TEXT_FONTS[0] when not installed.
    static constexpr const wchar_t* TEXT_FONTS[] = {L"Segoe Print", L"Segoe UI", L"Ink Free", L"Consolas"};
    // Reads Text/font from the INI and returns an installed family name,
    // falling back to TEXT_FONTS[0] if the stored one is unavailable.
    static std::wstring ResolveTextFont();

    // Autostart at logon, backed by the HKCU ...\CurrentVersion\Run key.
    static bool IsAutostartEnabled();
    static void SetAutostart(bool enable);

    void RegisterHotKeys();
    bool UpdateCursor();

protected:
    NOTIFYICONDATA niData;
    HDC            hDesktopCompatibleDC;
    HBITMAP        hDesktopCompatibleBitmap;
    HBITMAP        hOldBmp;
    HDC            m_desktopSnapshotDC     = nullptr;
    HBITMAP        m_desktopSnapshotBitmap = nullptr;
    HBITMAP        m_desktopSnapshotOldBmp = nullptr;

    bool m_bDrawing;

    int  m_colorIndex;
    int  m_currentPenWidth;
    BYTE m_currentAlpha;
    int  m_fadeSeconds;

    POINT m_lineStartShiftPoint;

    COLORREF m_colors[10];

    HCURSOR m_hCursor;
    HCURSOR m_hPreviousCursor;

    bool       m_bTextMode    = false;
    bool       m_caretVisible = true; // blink state for the text-mode caret
    Theme      m_theme        = Theme::Light;
    BoardStyle m_boardStyle   = BoardStyle::None;

    RECT                 m_rcScreen;
    std::deque<DrawLine> m_drawLines;

    HACCEL m_hAccel = nullptr; // runtime accelerator table (see BuildAcceleratorTable)

    static HWND m_mainWnd;
};
