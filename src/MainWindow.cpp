// demoHelper - screen drawing and presentation tool

// Copyright (C) 2007-2008, 2012-2013, 2015, 2020-2021 - Stefan Kueng

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

#include "stdafx.h"
#include "MainWindow.h"
#include "DPIAware.h"
#include "DebugOutput.h"
#include "IniSettings.h"
#include "RectSelectionWnd.h"
#include "MemDC.h"

#include <algorithm>

#define TRAY_WM_MESSAGE (WM_APP + 1)

extern HINSTANCE g_hInstance; // current instance
extern HINSTANCE g_hResource; // the resource dll

CMagnifierWindow CMainWindow::m_magnifierWindow = CMagnifierWindow();
bool             CMainWindow::m_bLensMode       = false;
HWND             CMainWindow::m_mainWnd         = nullptr;

bool CMainWindow::RegisterAndCreateWindow()
{
    WNDCLASSEX wcx = {0};

    // Fill in the window class structure with default parameters
    wcx.cbSize      = sizeof(WNDCLASSEX);
    wcx.style       = CS_HREDRAW | CS_VREDRAW;
    wcx.lpfnWndProc = CWindow::stWinMsgHandler;
    wcx.cbClsExtra  = 0;
    wcx.cbWndExtra  = 0;
    wcx.hInstance   = hResource;
    //wcx.hCursor     = LoadCursor(nullptr, IDC_ARROW);
    ResString clsName(hResource, IDS_APP_TITLE);
    wcx.lpszClassName = clsName;
    wcx.hIcon         = LoadIcon(hResource, MAKEINTRESOURCE(IDI_DEMOHELPER));
    wcx.hbrBackground = nullptr;
    wcx.lpszMenuName  = nullptr;
    wcx.hIconSm       = LoadIcon(wcx.hInstance, MAKEINTRESOURCE(IDI_DEMOHELPER));
    if (RegisterWindow(&wcx))
    {
        if (CreateEx(NULL, WS_POPUP, nullptr))
        {
            // since our main window is hidden most of the time
            // we have to add an auxiliary window to the system tray

            const auto iconSizeX = CDPIAware::Instance().Scale(*this, GetSystemMetrics(SM_CXSMICON));
            const auto iconSizeY = CDPIAware::Instance().Scale(*this, GetSystemMetrics(SM_CYSMICON));
            SecureZeroMemory(&niData, sizeof(NOTIFYICONDATA));

            niData.uID    = IDI_DEMOHELPER;
            niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_INFO;

            niData.hIcon            = static_cast<HICON>(LoadImage(hResource, MAKEINTRESOURCE(IDI_DEMOHELPER),
                                                        IMAGE_ICON, iconSizeX, iconSizeY, LR_DEFAULTCOLOR));
            niData.hWnd             = *this;
            niData.uCallbackMessage = TRAY_WM_MESSAGE;

            Shell_NotifyIcon(NIM_ADD, &niData);
            DestroyIcon(niData.hIcon);
            m_mainWnd = *this;
            return true;
        }
    }
    return false;
}

LRESULT CALLBACK CMainWindow::WinMsgHandler(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            m_hwnd = hwnd;
            RegisterHotKeys();
            m_magnifierWindow.Create(g_hInstance, *this, FALSE);
            m_colorIndex      = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"colorindex", 1));
            m_currentPenWidth = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"currentpenwidth", 6));
        }
        break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        case WM_HOTKEY:
        {
            WORD key  = MAKEWORD(HIWORD(lParam), LOWORD(lParam));
            key       = HotKey2HotKeyControl(key);
            WORD zoom = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"zoom", 0x231));
            WORD draw = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"draw", 0x232));
            WORD lens = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"lens", 0x233));
            if (key == zoom)
            {
                m_bZooming = true;
                StartZoomingMode();
            }
            else if (key == draw)
            {
                if (IsWindowVisible(*this))
                    EndPresentationMode();
                else
                    StartPresentationMode();
            }
            else if (key == lens)
            {
                SetWindowLong(*this, GWL_EXSTYLE, 0);
                m_magnifierWindow.Reset();
                ShowWindow(m_magnifierWindow, SW_HIDE);
                EndPresentationMode();

                if (m_bLensMode)
                {
                    m_bLensMode = false;
                }
                else
                {
                    m_bLensMode      = true;
                    auto allMonitors = CIniSettings::Instance().GetInt64(L"Misc", L"allmonitors", 0) != 0;
                    if (allMonitors)
                    {
                        m_rcScreen.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
                        m_rcScreen.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
                        m_rcScreen.right  = m_rcScreen.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
                        m_rcScreen.bottom = m_rcScreen.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
                    }
                    else
                    {
                        POINT pt;
                        GetCursorPos(&pt);
                        auto          hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
                        MONITORINFOEX mi       = {};
                        mi.cbSize              = sizeof(MONITORINFOEX);
                        GetMonitorInfo(hMonitor, &mi);
                        m_rcScreen = mi.rcMonitor;
                    }

                    CRectSelectionWnd selWnd(g_hInstance);
                    auto              zoomRect = selWnd.Show(*this, m_rcScreen, static_cast<float>(m_rcScreen.right - m_rcScreen.left) / static_cast<float>(m_rcScreen.bottom - m_rcScreen.top));
                    CloseWindow(selWnd);
                    DestroyWindow(selWnd);
                    if (!IsRectEmpty(&zoomRect))
                    {
                        StartPresentationMode();
                        m_bLensMode = true;
                        SetWindowLong(*this, GWL_EXSTYLE, WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT);
                        SetLayeredWindowAttributes(*this, 0, 255, LWA_ALPHA);
                        SetWindowPos(m_magnifierWindow, HWND_TOP, m_rcScreen.left, m_rcScreen.top, m_rcScreen.right - m_rcScreen.left, m_rcScreen.bottom - m_rcScreen.top,
                                     SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE);
                        m_magnifierWindow.SetSourceRect(zoomRect);
                        ::SetTimer(*this, TIMER_ID_LENS, 20, nullptr);
                        SetWindowPos(*this, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
                    }
                    else
                    {
                        SetWindowLong(*this, GWL_EXSTYLE, 0);
                        m_magnifierWindow.Reset();
                        ShowWindow(m_magnifierWindow, SW_HIDE);
                        EndPresentationMode();
                        m_bLensMode = false;
                    }
                }
            }
        }
        break;
        case WM_ERASEBKGND:
            return 1; // don't erase the background!
        case WM_CHAR:
        {
            if (m_bTextMode && !m_drawLines.empty() && m_drawLines.back().lineType == LineType::Text)
            {
                auto&   line = m_drawLines.back();
                wchar_t ch   = static_cast<wchar_t>(wParam);
                if (ch == VK_BACK)
                {
                    if (!line.text.empty())
                        line.text.pop_back();
                }
                else if (ch == VK_ESCAPE || ch == VK_RETURN)
                {
                    // ignore — handled elsewhere
                }
                else if (ch >= 0x20)
                {
                    line.text.push_back(ch);
                }
                InvalidateRect(*this, nullptr, FALSE);
            }
        }
        break;
        case WM_PAINT:
        {
            ProfileTimer profiler(L"WM_PAINT");
            PAINTSTRUCT  ps;
            HDC          hDc = BeginPaint(*this, &ps);
            {
                CMemDC memDC(hDc, ps.rcPaint);
                if (m_bZooming)
                {
                    // we're zooming,
                    // just stretch the part of the original window around the mouse pointer
                    POINT pt;
                    auto  msgPos = GetMessagePos();
                    pt.x         = GET_X_LPARAM(msgPos);
                    pt.y         = GET_Y_LPARAM(msgPos);
                    DrawZoom(memDC, pt);
                }
                else
                {
                    BitBlt(memDC,
                           ps.rcPaint.left,
                           ps.rcPaint.top,
                           ps.rcPaint.right - ps.rcPaint.left,
                           ps.rcPaint.bottom - ps.rcPaint.top,
                           hDesktopCompatibleDC,
                           ps.rcPaint.left,
                           ps.rcPaint.top,
                           SRCCOPY);
                    Gdiplus::Graphics graphics(memDC);
                    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

                    for (const auto& line : m_drawLines)
                    {
                        Gdiplus::Color color;
                        color.SetValue(Gdiplus::Color::MakeARGB(line.alpha, GetRValue(m_colors[line.colorIndex]), GetGValue(m_colors[line.colorIndex]), GetBValue(m_colors[line.colorIndex])));
                        Gdiplus::Pen pen(color, static_cast<Gdiplus::REAL>(line.penWidth));
                        pen.SetLineCap(Gdiplus::LineCap::LineCapRound, Gdiplus::LineCap::LineCapRound, Gdiplus::DashCap::DashCapRound);

                        if (line.lineType == LineType::Hand)
                        {
                            if (line.points.size() == 1)
                            {
                                auto                halfPenWidth = line.penWidth / 2;
                                Gdiplus::SolidBrush brush(color);
                                graphics.FillEllipse(&brush, line.points[0].X - halfPenWidth, line.points[0].Y - halfPenWidth, line.penWidth, line.penWidth);
                            }
                            else
                                graphics.DrawLines(&pen, line.points.data(), static_cast<int>(line.points.size()));
                        }
                        else
                        {
                            switch (line.lineType)
                            {
                                case LineType::Arrow:
                                    pen.SetEndCap(Gdiplus::LineCap::LineCapArrowAnchor);
                                    [[fallthrough]];
                                case LineType::Hand:
                                case LineType::Straight:
                                    if ((line.lineStartPoint.X >= 0) && (line.lineStartPoint.Y >= 0) && (line.lineEndPoint.X >= 0) && (line.lineEndPoint.Y >= 0))
                                    {
                                        graphics.DrawLine(&pen, line.lineStartPoint, line.lineEndPoint);
                                    }
                                    break;
                                case LineType::Rectangle:
                                    if ((line.lineStartPoint.X >= 0) && (line.lineStartPoint.Y >= 0) && (line.lineEndPoint.X >= 0) && (line.lineEndPoint.Y >= 0))
                                    {
                                        Gdiplus::Point startPt;
                                        startPt.X   = std::min(line.lineStartPoint.X, line.lineEndPoint.X);
                                        startPt.Y   = std::min(line.lineStartPoint.Y, line.lineEndPoint.Y);
                                        auto width  = std::abs(line.lineEndPoint.X - line.lineStartPoint.X);
                                        auto height = std::abs(line.lineEndPoint.Y - line.lineStartPoint.Y);
                                        graphics.DrawRectangle(&pen, startPt.X, startPt.Y, width, height);
                                    }
                                    break;
                                case LineType::Ellipse:
                                    if ((line.lineStartPoint.X >= 0) && (line.lineStartPoint.Y >= 0) && (line.lineEndPoint.X >= 0) && (line.lineEndPoint.Y >= 0))
                                    {
                                        Gdiplus::Point startPt;
                                        startPt.X   = std::min(line.lineStartPoint.X, line.lineEndPoint.X);
                                        startPt.Y   = std::min(line.lineStartPoint.Y, line.lineEndPoint.Y);
                                        auto width  = std::abs(line.lineEndPoint.X - line.lineStartPoint.X);
                                        auto height = std::abs(line.lineEndPoint.Y - line.lineStartPoint.Y);
                                        graphics.DrawEllipse(&pen, startPt.X, startPt.Y, width, height);
                                    }
                                    break;
                                case LineType::Text:
                                    if (!line.text.empty() && (line.lineStartPoint.X >= 0) && (line.lineStartPoint.Y >= 0))
                                    {
                                        Gdiplus::FontFamily fontFamily(L"Segoe UI");
                                        Gdiplus::Font       font(&fontFamily, static_cast<Gdiplus::REAL>(line.fontSize), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                                        Gdiplus::SolidBrush brush(color);
                                        Gdiplus::PointF     origin(static_cast<Gdiplus::REAL>(line.lineStartPoint.X), static_cast<Gdiplus::REAL>(line.lineStartPoint.Y));
                                        graphics.DrawString(line.text.c_str(), static_cast<INT>(line.text.size()), &font, origin, &brush);
                                    }
                                    break;
                            }
                        }
                    }
                }
            }
            EndPaint(*this, &ps);
        }
        break;
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDOWN:
        {
            if (m_bLensMode)
                break;
            if (m_bTextMode && uMsg == WM_LBUTTONDOWN)
            {
                if (!m_drawLines.empty() && m_drawLines.back().lineType == LineType::Text)
                {
                    auto& line            = m_drawLines.back();
                    line.lineStartPoint.X = GET_X_LPARAM(lParam);
                    line.lineStartPoint.Y = GET_Y_LPARAM(lParam);
                    if (line.text.empty())
                        m_drawLines.pop_back();
                }
                m_bTextMode = false;
                InvalidateRect(*this, nullptr, FALSE);
                break;
            }
            if (m_bInlineZoom)
            {
                m_ptInlineZoomStartPoint.x = GET_X_LPARAM(lParam);
                m_ptInlineZoomStartPoint.y = GET_Y_LPARAM(lParam);
                m_ptInlineZoomEndPoint.x   = GET_X_LPARAM(lParam);
                m_ptInlineZoomEndPoint.y   = GET_Y_LPARAM(lParam);
            }
            if (m_bZooming)
            {
                // now make the zoomed window the 'default'
                HDC   hdc = GetDC(*this);
                POINT pt;
                auto  msgPos = GetMessagePos();
                pt.x         = GET_X_LPARAM(msgPos);
                pt.y         = GET_Y_LPARAM(msgPos);
                DrawZoom(hdc, pt);
                m_bZooming         = false;
                auto nScreenWidth  = m_rcScreen.right - m_rcScreen.left;
                auto nScreenHeight = m_rcScreen.bottom - m_rcScreen.top;

                BitBlt(hDesktopCompatibleDC, 0, 0, nScreenWidth, nScreenHeight,
                       hdc, 0, 0, SRCCOPY);
                DeleteDC(hdc);
                InvalidateRect(*this, nullptr, false);
            }
            m_bDrawing = true;
            DrawLine drawLine;
            drawLine.lineStartPoint.X = GET_X_LPARAM(lParam);
            drawLine.lineStartPoint.Y = GET_Y_LPARAM(lParam);
            drawLine.alpha            = m_currentAlpha;
            drawLine.points.push_back(drawLine.lineStartPoint);
            drawLine.colorIndex = m_colorIndex;
            drawLine.penWidth   = m_currentPenWidth;
            m_drawLines.push_back(std::move(drawLine));

            RECT invalidRect = {drawLine.lineStartPoint.X, drawLine.lineStartPoint.Y, drawLine.lineStartPoint.X, drawLine.lineStartPoint.Y};
            InflateRect(&invalidRect, 2 * m_currentPenWidth, 2 * m_currentPenWidth);
            InvalidateRect(*this, &invalidRect, FALSE);
        }
        break;
        case WM_RBUTTONUP:
        case WM_LBUTTONUP:
            if (m_bInlineZoom)
            {
                m_bInlineZoom      = false;
                auto nScreenWidth  = m_rcScreen.right - m_rcScreen.left;
                auto nScreenHeight = m_rcScreen.bottom - m_rcScreen.top;

                StretchBlt(hDesktopCompatibleDC,
                           m_rcScreen.left, m_rcScreen.top,
                           nScreenWidth, nScreenHeight,
                           hDesktopCompatibleDC,
                           m_ptInlineZoomStartPoint.x, m_ptInlineZoomStartPoint.y,
                           abs(m_ptInlineZoomStartPoint.x - m_ptInlineZoomEndPoint.x), abs(m_ptInlineZoomStartPoint.y - m_ptInlineZoomEndPoint.y),
                           SRCCOPY);
                UpdateCursor();
                InvalidateRect(*this, nullptr, false);
            }
            else
            {
                m_bDrawing              = false;
                m_lineStartShiftPoint.x = -1;
                m_lineStartShiftPoint.y = -1;
            }
            break;
        case WM_MOUSEMOVE:
        {
            if (m_bInlineZoom)
            {
                if ((m_ptInlineZoomStartPoint.x >= 0) && (m_ptInlineZoomStartPoint.y >= 0) &&
                    (m_ptInlineZoomEndPoint.x >= 0) && (m_ptInlineZoomEndPoint.y >= 0))
                {
                    HDC hDC = GetDC(*this);
                    SetROP2(hDC, R2_NOT);
                    SelectObject(hDC, GetStockObject(NULL_BRUSH));
                    Rectangle(hDC, m_ptInlineZoomStartPoint.x, m_ptInlineZoomStartPoint.y, m_ptInlineZoomEndPoint.x, m_ptInlineZoomEndPoint.y);
                    m_ptInlineZoomEndPoint.x = GET_X_LPARAM(lParam);
                    m_ptInlineZoomEndPoint.y = GET_Y_LPARAM(lParam);
                    Rectangle(hDC, m_ptInlineZoomStartPoint.x, m_ptInlineZoomStartPoint.y, m_ptInlineZoomEndPoint.x, m_ptInlineZoomEndPoint.y);
                    ReleaseDC(*this, hDC);
                }
            }
            else if (m_bZooming)
            {
                InvalidateRect(*this, nullptr, false);
            }
            else if (m_bTextMode)
            {
                if (!m_drawLines.empty() && m_drawLines.back().lineType == LineType::Text)
                {
                    auto& line            = m_drawLines.back();
                    line.lineStartPoint.X = GET_X_LPARAM(lParam);
                    line.lineStartPoint.Y = GET_Y_LPARAM(lParam);
                    InvalidateRect(*this, nullptr, FALSE);
                }
            }
            else if (m_bDrawing)
            {
                int xPos = GET_X_LPARAM(lParam);
                int yPos = GET_Y_LPARAM(lParam);
                if (wParam & MK_LBUTTON)
                {
                    m_lineStartShiftPoint.x = -1;
                    m_lineStartShiftPoint.y = -1;
                    if ((wParam & MK_CONTROL) || (wParam & MK_SHIFT))
                    {
                        auto& line = m_drawLines.back();
                        if (wParam & MK_SHIFT)
                        {
                            if (wParam & MK_CONTROL)
                                line.lineType = LineType::Ellipse;
                            else
                                line.lineType = LineType::Rectangle;
                        }
                        else
                            line.lineType = LineType::Straight;

                        RECT invalidRect;
                        invalidRect.left   = std::min(line.lineStartPoint.X, xPos);
                        invalidRect.top    = std::min(line.lineStartPoint.Y, yPos);
                        invalidRect.right  = std::max(line.lineStartPoint.X, xPos);
                        invalidRect.bottom = std::max(line.lineStartPoint.Y, yPos);

                        invalidRect.left   = std::min(line.lineStartPoint.X, line.lineEndPoint.X);
                        invalidRect.top    = std::min(line.lineStartPoint.Y, line.lineEndPoint.Y);
                        invalidRect.right  = std::max(line.lineStartPoint.X, line.lineEndPoint.X);
                        invalidRect.bottom = std::max(line.lineStartPoint.Y, line.lineEndPoint.Y);

                        InflateRect(&invalidRect, 10 * m_currentPenWidth, 10 * m_currentPenWidth);
                        invalidRect.left = std::max(0L, invalidRect.left);
                        invalidRect.top  = std::max(0L, invalidRect.top);
                        InvalidateRect(*this, &invalidRect, FALSE);
                        line.lineEndPoint.X = xPos;
                        line.lineEndPoint.Y = yPos;
                    }
                    else
                    {
                        auto& line = m_drawLines.back();

                        RECT           invalidRect = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
                        Gdiplus::Point pt          = {xPos, yPos};
                        auto           prevPt      = line.points.back();
                        if (sqrt((pt.X - prevPt.X) * (pt.X - prevPt.X) + (pt.Y - prevPt.Y) * (pt.Y - prevPt.Y)) > 1)
                        {
                            line.points.push_back(pt);
                            line.lineType = LineType::Hand;
                            InvalidateRect(*this, &invalidRect, FALSE);
                        }
                    }
                }
                else if (wParam & MK_RBUTTON)
                {
                    auto& line = m_drawLines.back();
                    if (wParam & MK_SHIFT)
                    {
                        // if shift is pressed, the user want's to draw straight lines
                        if (abs(xPos - line.lineStartPoint.X) > abs(yPos - line.lineStartPoint.Y))
                        {
                            // straight line horizontally
                            yPos = line.lineStartPoint.Y;
                        }
                        else
                        {
                            // straight line vertically
                            xPos = line.lineStartPoint.X;
                        }
                    }
                    if (wParam & MK_CONTROL)
                    {
                        // control pressed means normal lines, not arrows
                        line.lineType = LineType::Straight;
                    }
                    else
                    {
                        line.lineType = LineType::Arrow;
                    }
                    RECT invalidRect;
                    invalidRect.left   = std::min(line.lineStartPoint.X, xPos);
                    invalidRect.top    = std::min(line.lineStartPoint.Y, yPos);
                    invalidRect.right  = std::max(line.lineStartPoint.X, xPos);
                    invalidRect.bottom = std::max(line.lineStartPoint.Y, yPos);

                    invalidRect.left   = std::min(line.lineStartPoint.X, line.lineEndPoint.X);
                    invalidRect.top    = std::min(line.lineStartPoint.Y, line.lineEndPoint.Y);
                    invalidRect.right  = std::max(line.lineStartPoint.X, line.lineEndPoint.X);
                    invalidRect.bottom = std::max(line.lineStartPoint.Y, line.lineEndPoint.Y);

                    InflateRect(&invalidRect, 10 * m_currentPenWidth, 10 * m_currentPenWidth);
                    invalidRect.left = std::max(0L, invalidRect.left);
                    invalidRect.top  = std::max(0L, invalidRect.top);
                    InvalidateRect(*this, &invalidRect, FALSE);
                    line.lineEndPoint.X = xPos;
                    line.lineEndPoint.Y = yPos;
                }
            }
        }
        break;
        case TRAY_WM_MESSAGE:
        {
            switch (lParam)
            {
                case WM_LBUTTONDBLCLK:
                    StartPresentationMode();
                    break;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                {
                    POINT pt;
                    GetCursorPos(&pt);
                    HMENU hMenu    = LoadMenu(hResource, MAKEINTRESOURCE(IDC_DEMOHELPER));
                    HMENU hPopMenu = GetSubMenu(hMenu, 0);
                    TrackPopupMenu(hPopMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, *this, nullptr);
                    DestroyMenu(hMenu);
                }
                break;
            }
        }
        break;
        case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (m_bTextMode && !m_drawLines.empty() && m_drawLines.back().lineType == LineType::Text)
            {
                auto& line = m_drawLines.back();
                if (zDelta > 0)
                    line.fontSize = std::min(line.fontSize + 4, 256);
                else
                    line.fontSize = std::max(line.fontSize - 4, 8);
                InvalidateRect(*this, nullptr, FALSE);
                break;
            }
            if (m_bZooming)
            {
                if (zDelta > 0)
                {
                    m_zoomFactor += 0.2f;
                    if (m_zoomFactor > 4.0f)
                        m_zoomFactor = 4.0f;
                }
                else
                {
                    m_zoomFactor -= 0.2f;
                    if (m_zoomFactor < 1.0f)
                        m_zoomFactor = 1.0f;
                }
                auto transZoom  = Animator::Instance().CreateLinearTransition(m_animVarZoom, 0.3, m_zoomFactor);
                auto storyBoard = Animator::Instance().CreateStoryBoard();
                storyBoard->AddTransition(m_animVarZoom.m_animVar, transZoom);
                Animator::Instance().RunStoryBoard(storyBoard, [this]() {
                    InvalidateRect(*this, nullptr, false);
                });
            }
            else
            {
                if (wParam & MK_CONTROL)
                {
                    if (zDelta > 0)
                        DoCommand(ID_CMD_DECREASE);
                    else
                        DoCommand(ID_CMD_INCREASE);
                }
                else
                {
                    if (zDelta > 0)
                        DoCommand(ID_CMD_PREVCOLOR);
                    else
                        DoCommand(ID_CMD_NEXTCOLOR);
                }
            }
            InvalidateRect(*this, nullptr, false);
        }
        break;
        case WM_TIMER:
            if (wParam == TIMER_ID_DRAW)
            {
                KillTimer(*this, TIMER_ID_DRAW);
                StartPresentationMode();
            }
            else if (wParam == TIMER_ID_ZOOM)
            {
                KillTimer(*this, TIMER_ID_ZOOM);
                StartZoomingMode();
            }
            else if (wParam == TIMER_ID_FADE)
            {
                // go through all lines and reduce the fade-count value
                auto dec = std::max(static_cast<BYTE>(LINE_ALPHA / (m_fadeSeconds * 10)), static_cast<BYTE>(1));
                for (auto& line : m_drawLines)
                {
                    if (line.alpha > dec)
                        line.alpha -= dec;
                    else
                        line.alpha = 0;
                }
                // go through all lines again, and remove all lines with a pen width
                // of zero
                bool doRedraw = !m_drawLines.empty();
                for (auto it = m_drawLines.begin(); it != m_drawLines.end();)
                {
                    if (it->alpha == 0)
                    {
                        it       = m_drawLines.erase(it);
                        doRedraw = true;
                    }
                    else
                        ++it;
                }
                if (doRedraw)
                    InvalidateRect(*this, nullptr, false);
            }
            else if (wParam == TIMER_ID_LENS)
            {
                m_magnifierWindow.UpdateMagnifier();
                SetWindowPos(*this, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
            }
            break;
        case WM_DESTROY:
            Shell_NotifyIcon(NIM_DELETE, &niData);
            bWindowClosed = TRUE;
            PostQuitMessage(0);
            break;
        case WM_CLOSE:
            ::DestroyWindow(m_hwnd);
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
};

bool CMainWindow::StartPresentationMode()
{
    int          nScreenWidth  = 0;
    int          nScreenHeight = 0;
    std::wstring devName;
    auto         allMonitors = CIniSettings::Instance().GetInt64(L"Misc", L"allmonitors", 0) != 0;
    if (allMonitors)
    {
        m_rcScreen.left   = GetSystemMetrics(SM_XVIRTUALSCREEN);
        m_rcScreen.top    = GetSystemMetrics(SM_YVIRTUALSCREEN);
        nScreenWidth      = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        nScreenHeight     = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        m_rcScreen.right  = m_rcScreen.left + nScreenWidth;
        m_rcScreen.bottom = m_rcScreen.top + nScreenHeight;
    }
    else
    {
        POINT pt;
        GetCursorPos(&pt);
        auto          hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEX mi       = {};
        mi.cbSize              = sizeof(MONITORINFOEX);
        GetMonitorInfo(hMonitor, &mi);
        m_rcScreen    = mi.rcMonitor;
        nScreenWidth  = m_rcScreen.right - m_rcScreen.left;
        nScreenHeight = m_rcScreen.bottom - m_rcScreen.top;
        devName       = mi.szDevice;
    }
    HDC hDesktopDC = nullptr;
    if (allMonitors)
        hDesktopDC = GetDC(nullptr);
    else
        hDesktopDC = CreateDC(nullptr, devName.c_str(), nullptr, nullptr);
    hDesktopCompatibleDC     = CreateCompatibleDC(hDesktopDC);
    hDesktopCompatibleBitmap = CreateCompatibleBitmap(hDesktopDC, nScreenWidth, nScreenHeight);
    hOldBmp                  = static_cast<HBITMAP>(SelectObject(hDesktopCompatibleDC, hDesktopCompatibleBitmap));
    BitBlt(hDesktopCompatibleDC, 0, 0, nScreenWidth, nScreenHeight,
           hDesktopDC, allMonitors ? m_rcScreen.left : 0, allMonitors ? m_rcScreen.top : 0,
           SRCCOPY | CAPTUREBLT);

#ifdef _DEBUG
    auto topWnd = HWND_TOP;
#else
    auto topWnd = HWND_TOPMOST;
#endif
    SetWindowPos(*this, topWnd, m_rcScreen.left, m_rcScreen.top, nScreenWidth, nScreenHeight, SWP_SHOWWINDOW | SWP_DRAWFRAME);

    if (devName.empty())
        ReleaseDC(nullptr, hDesktopDC);
    else
        DeleteDC(hDesktopDC);
    if (m_bLensMode)
    {
        SetWindowLong(*this, GWL_EXSTYLE, 0);
        m_magnifierWindow.Reset();
        ShowWindow(m_magnifierWindow, SW_HIDE);
        m_bLensMode = false;
    }
    if (!m_bZooming)
    {
        if (m_hCursor)
            DestroyCursor(m_hCursor);
        m_hCursor         = CreateDrawCursor(m_colors[m_colorIndex], std::max(2, m_currentPenWidth));
        m_hPreviousCursor = SetCursor(m_hCursor);
    }
    else
    {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    m_bInlineZoom = false;

    m_fadeSeconds = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"fadeseconds", 0));
    if (m_fadeSeconds > 0)
    {
        ::SetTimer(*this, TIMER_ID_FADE, 100, nullptr);
    }

    return true;
}

bool CMainWindow::EndPresentationMode()
{
    SetWindowPos(*this, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE);
    SelectObject(hDesktopCompatibleDC, hOldBmp);
    DeleteObject(hDesktopCompatibleBitmap);
    DeleteDC(hDesktopCompatibleDC);
    m_drawLines.clear();
    m_bDrawing  = false;
    m_bZooming  = false;
    m_bTextMode = false;
    SetCursor(m_hPreviousCursor);
    if (m_hCursor)
    {
        DestroyCursor(m_hCursor);
        m_hCursor = nullptr;
    }
    m_bInlineZoom = false;
    m_zoomFactor  = 1.2f;
    CIniSettings::Instance().SetInt64(L"Draw", L"colorindex", m_colorIndex);
    CIniSettings::Instance().SetInt64(L"Draw", L"currentpenwidth", m_currentPenWidth);
    return true;
}

void CMainWindow::RegisterHotKeys()
{
    WORD zoom = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"zoom", 0x231));
    WORD draw = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"draw", 0x232));
    WORD lens = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"lens", 0x233));
    zoom      = HotKeyControl2HotKey(zoom);
    draw      = HotKeyControl2HotKey(draw);
    lens      = HotKeyControl2HotKey(lens);

    RegisterHotKey(*this, DRAW_HOTKEY, HIBYTE(draw), LOBYTE(draw));
    RegisterHotKey(*this, ZOOM_HOTKEY, HIBYTE(zoom), LOBYTE(zoom));
    RegisterHotKey(*this, LENS_HOTKEY, HIBYTE(lens), LOBYTE(lens));
}

bool CMainWindow::StartZoomingMode()
{
    m_bZooming      = true;
    m_zoomFactor    = 1.2f;
    auto transZoom  = Animator::Instance().CreateLinearTransition(m_animVarZoom, 0.5, m_zoomFactor);
    auto storyBoard = Animator::Instance().CreateStoryBoard();
    storyBoard->AddTransition(m_animVarZoom.m_animVar, transZoom);
    Animator::Instance().RunStoryBoard(storyBoard, [this]() {
        InvalidateRect(*this, nullptr, false);
    });
    StartPresentationMode();
    return true;
}

bool CMainWindow::EndZoomingMode()
{
    m_bZooming = false;
    EndPresentationMode();
    return true;
}

bool CMainWindow::DrawZoom(HDC hdc, POINT pt)
{
    // cursor pos is in screen coordinates - just what we need since our window covers the whole screen.
    // to zoom, we need to stretch the part around the cursor to the full screen
    // zoomfactor 1 = whole screen
    // zoomfactor 2 = quarter screen to fullscreen
    auto cx          = m_rcScreen.right - m_rcScreen.left;
    auto cy          = m_rcScreen.bottom - m_rcScreen.top;
    auto zoomFactor  = Animator::GetValue(m_animVarZoom);
    auto zoomWindowX = static_cast<long>(static_cast<double>(cx) / zoomFactor);
    auto zoomWindowY = static_cast<long>(static_cast<double>(cy) / zoomFactor);

    // adjust the cursor position to the zoom factor
    ScreenToClient(*this, &pt);
    POINT resPt;
    resPt.x = pt.x * (cx - zoomWindowX) / cx;
    resPt.y = pt.y * (cy - zoomWindowY) / cy;

    return !!StretchBlt(hdc, 0, 0, cx, cy, hDesktopCompatibleDC, resPt.x, resPt.y, zoomWindowX, zoomWindowY, SRCCOPY);
}

bool CMainWindow::UpdateCursor()
{
    if (m_bZooming)
    {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    DestroyCursor(m_hCursor);
    m_hCursor = CreateDrawCursor(m_colors[m_colorIndex], std::max(2, m_currentPenWidth));
    if (m_hCursor)
    {
        SetCursor(m_hCursor);
        return true;
    }
    return false;
}

bool CMainWindow::StartInlineZoom()
{
    m_bInlineZoom = true;
    HCURSOR hCur  = LoadCursor(nullptr, IDC_CROSS);
    SetCursor(hCur);
    m_ptInlineZoomStartPoint.x = -1;
    m_ptInlineZoomStartPoint.y = -1;
    m_ptInlineZoomEndPoint.x   = -1;
    m_ptInlineZoomEndPoint.y   = -1;
    return true;
}