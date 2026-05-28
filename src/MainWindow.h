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
#include "AnimationManager.h"
#include "MagnifierWindow.h"
#include <shellapi.h>
#include <vector>
#include <deque>
#include <string>

#define DRAW_HOTKEY 100
#define ZOOM_HOTKEY 101
#define LENS_HOTKEY 102

#define TIMER_ID_DRAW 101
#define TIMER_ID_ZOOM 102
#define TIMER_ID_FADE 103
#define TIMER_ID_LENS 104

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
        , m_zoomFactor(1.2f)
        , m_bZooming(false)
        , m_colorIndex(1)
        , m_currentPenWidth(6)
        , m_currentAlpha(LINE_ALPHA)
        , m_fadeSeconds(0)
        , m_lineStartShiftPoint({})
        , m_hCursor(nullptr)
        , m_hPreviousCursor(nullptr)
        , m_bMarker(false)
        , m_oldPenWidth(6)
        , m_oldColorIndex(0)
        , m_oldAlpha(0)
        , m_bInlineZoom(false)
        , m_ptInlineZoomStartPoint({})
        , m_ptInlineZoomEndPoint({})
        , m_rcScreen({0})
    {
        SetWindowTitle(static_cast<LPCTSTR>(ResString(hResource, IDS_APP_TITLE)));
        m_colors[0]   = RGB(255, 255, 0);
        m_colors[1]   = RGB(255, 0, 0);
        m_colors[2]   = RGB(150, 0, 0);
        m_colors[3]   = RGB(0, 255, 0);
        m_colors[4]   = RGB(0, 150, 0);
        m_colors[5]   = RGB(0, 0, 255);
        m_colors[6]   = RGB(0, 0, 150);
        m_colors[7]   = RGB(0, 0, 0);
        m_colors[8]   = RGB(150, 150, 150);
        m_colors[9]   = RGB(0, 255, 255);
        m_animVarZoom = Animator::Instance().CreateAnimationVariable(1.2, 1.2);
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
    bool    StartInlineZoom();
    bool    StartZoomingMode();
    bool    EndZoomingMode();
    bool    DrawZoom(HDC hdc, POINT pt);
    HCURSOR CreateDrawCursor(COLORREF color, int penwidth);

    static BOOL CALLBACK OptionsDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
    static WORD          HotKeyControl2HotKey(WORD hk);
    static WORD          HotKey2HotKeyControl(WORD hk);

    void RegisterHotKeys();
    bool UpdateCursor();

protected:
    NOTIFYICONDATA niData;
    HDC            hDesktopCompatibleDC;
    HBITMAP        hDesktopCompatibleBitmap;
    HBITMAP        hOldBmp;

    bool  m_bDrawing;
    float m_zoomFactor;
    bool  m_bZooming;

    int  m_colorIndex;
    int  m_currentPenWidth;
    BYTE m_currentAlpha;
    int  m_fadeSeconds;

    POINT m_lineStartShiftPoint;

    COLORREF m_colors[10];

    HCURSOR m_hCursor;
    HCURSOR m_hPreviousCursor;

    bool m_bMarker;
    int  m_oldPenWidth;
    int  m_oldColorIndex;
    BYTE m_oldAlpha;

    bool  m_bInlineZoom;
    POINT m_ptInlineZoomStartPoint;
    POINT m_ptInlineZoomEndPoint;

    bool m_bTextMode = false;

    RECT                 m_rcScreen;
    AnimationVariable    m_animVarZoom;
    std::deque<DrawLine> m_drawLines;

    static CMagnifierWindow m_magnifierWindow;
    static bool             m_bLensMode;
    static HWND             m_mainWnd;
};
