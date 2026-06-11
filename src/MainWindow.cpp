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
#include "PathUtils.h"
#include "MemDC.h"

#include <shlobj.h>
#include <algorithm>
#include <string>

#define TRAY_WM_MESSAGE (WM_APP + 1)

extern HINSTANCE g_hInstance; // current instance
extern HINSTANCE g_hResource; // the resource dll

HWND CMainWindow::m_mainWnd = nullptr;

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
            // Defaults applied at every launch (configurable in Options > Draw);
            // not carried over from the previous session.
            m_colorIndex      = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"defaultcolor", 1));
            m_currentPenWidth = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"defaultpenwidth", 6));
            ApplyTheme();
        }
        break;
        case WM_COMMAND:
            return DoCommand(LOWORD(wParam));
        case WM_HOTKEY:
        {
            WORD key  = MAKEWORD(HIWORD(lParam), LOWORD(lParam));
            key       = HotKey2HotKeyControl(key);
            WORD draw = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"draw", 0x232));
            if (key == draw)
            {
                if (IsWindowVisible(*this))
                    EndPresentationMode();
                else
                    StartPresentationMode();
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
                    RenderAnnotations(graphics);
                    RenderTextCaret(graphics);
                }
            }
            EndPaint(*this, &ps);
        }
        break;
        case WM_RBUTTONDOWN:
        case WM_LBUTTONDOWN:
        {
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
                KillTimer(*this, TIMER_ID_CARET);
                InvalidateRect(*this, nullptr, FALSE);
                break;
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
            m_bDrawing              = false;
            m_lineStartShiftPoint.x = -1;
            m_lineStartShiftPoint.y = -1;
            break;
        case WM_MOUSEMOVE:
        {
            if (m_bTextMode)
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
                    CheckMenuItem(hPopMenu, ID_TRAYCONTEXT_AUTOSTART,
                                  MF_BYCOMMAND | (IsAutostartEnabled() ? MF_CHECKED : MF_UNCHECKED));
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
            InvalidateRect(*this, nullptr, false);
        }
        break;
        case WM_TIMER:
            if (wParam == TIMER_ID_DRAW)
            {
                KillTimer(*this, TIMER_ID_DRAW);
                StartPresentationMode();
            }
            else if (wParam == TIMER_ID_CARET)
            {
                if (m_bTextMode)
                {
                    m_caretVisible = !m_caretVisible;
                    InvalidateRect(*this, nullptr, FALSE);
                }
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
    m_theme      = Theme::Transparent;
    m_boardStyle = BoardStyle::None;
    ApplyTheme();
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

    // Keep a pristine snapshot of the desktop so we can restore it when
    // cycling back to the Transparent theme. We can't recapture from the
    // live desktop later because our own window would be visible.
    m_desktopSnapshotDC     = CreateCompatibleDC(hDesktopDC);
    m_desktopSnapshotBitmap = CreateCompatibleBitmap(hDesktopDC, nScreenWidth, nScreenHeight);
    m_desktopSnapshotOldBmp = static_cast<HBITMAP>(SelectObject(m_desktopSnapshotDC, m_desktopSnapshotBitmap));
    BitBlt(m_desktopSnapshotDC, 0, 0, nScreenWidth, nScreenHeight,
           hDesktopCompatibleDC, 0, 0, SRCCOPY);

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
    if (m_hCursor)
        DestroyCursor(m_hCursor);
    m_hCursor         = CreateDrawCursor(m_colors[m_colorIndex], std::max(2, m_currentPenWidth));
    m_hPreviousCursor = SetCursor(m_hCursor);

    m_fadeSeconds = static_cast<int>(CIniSettings::Instance().GetInt64(L"Draw", L"fadeseconds", 0));
    if (m_fadeSeconds > 0)
    {
        ::SetTimer(*this, TIMER_ID_FADE, 100, nullptr);
    }

    return true;
}

bool CMainWindow::EndPresentationMode()
{
    // Auto-save the annotated screen (no-op if nothing was drawn) before the
    // backing DCs are freed. Covers both exit paths: Esc and the draw hotkey.
    SaveScreenshot();
    SetWindowPos(*this, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_HIDEWINDOW | SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOSIZE);
    SelectObject(hDesktopCompatibleDC, hOldBmp);
    DeleteObject(hDesktopCompatibleBitmap);
    DeleteDC(hDesktopCompatibleDC);
    if (m_desktopSnapshotDC)
    {
        SelectObject(m_desktopSnapshotDC, m_desktopSnapshotOldBmp);
        DeleteObject(m_desktopSnapshotBitmap);
        DeleteDC(m_desktopSnapshotDC);
        m_desktopSnapshotDC     = nullptr;
        m_desktopSnapshotBitmap = nullptr;
        m_desktopSnapshotOldBmp = nullptr;
    }
    m_drawLines.clear();
    m_bDrawing  = false;
    m_bTextMode = false;
    KillTimer(*this, TIMER_ID_CARET);
    SetCursor(m_hPreviousCursor);
    if (m_hCursor)
    {
        DestroyCursor(m_hCursor);
        m_hCursor = nullptr;
    }
    return true;
}

void CMainWindow::RegisterHotKeys()
{
    WORD draw = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"draw", 0x232));
    draw      = HotKeyControl2HotKey(draw);

    RegisterHotKey(*this, DRAW_HOTKEY, HIBYTE(draw), LOBYTE(draw));
}

namespace
{
    constexpr wchar_t AUTOSTART_KEY[]   = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t AUTOSTART_VALUE[] = L"DemoInk";
} // namespace

bool CMainWindow::IsAutostartEnabled()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_QUERY_VALUE, &hKey) != ERROR_SUCCESS)
        return false;
    bool present = (RegQueryValueEx(hKey, AUTOSTART_VALUE, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    RegCloseKey(hKey);
    return present;
}

void CMainWindow::SetAutostart(bool enable)
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, AUTOSTART_KEY, 0, KEY_SET_VALUE, &hKey) != ERROR_SUCCESS)
        return;
    if (enable)
    {
        // Quote the path so a space in it can't break the command line.
        std::wstring quoted = L"\"" + CPathUtils::GetModulePath() + L"\"";
        RegSetValueEx(hKey, AUTOSTART_VALUE, 0, REG_SZ,
                      reinterpret_cast<const BYTE*>(quoted.c_str()),
                      static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        RegDeleteValue(hKey, AUTOSTART_VALUE);
    }
    RegCloseKey(hKey);
}

bool CMainWindow::UpdateCursor()
{
    DestroyCursor(m_hCursor);
    m_hCursor = CreateDrawCursor(m_colors[m_colorIndex], std::max(2, m_currentPenWidth));
    if (m_hCursor)
    {
        SetCursor(m_hCursor);
        return true;
    }
    return false;
}

void CMainWindow::ApplyTheme()
{
    // Dark uses its own palette tuned for a black background and opaque
    // strokes; Light and Transparent share one palette and the alpha blend
    // (only the background fill differs, handled by the caller). Each color
    // can be overridden via [Colors] in the INI, falling back to the
    // built-in defaults.
    const bool      dark     = (m_theme == Theme::Dark);
    const COLORREF* defaults = dark ? DEFAULT_COLORS_DARK : DEFAULT_COLORS_LIGHT;
    const wchar_t*  prefix   = dark ? L"dark" : L"light";
    for (int i = 0; i < 10; ++i)
    {
        wchar_t key[16] = {0};
        swprintf_s(key, _countof(key), L"%s%d", prefix, i);
        m_colors[i] = static_cast<COLORREF>(CIniSettings::Instance().GetInt64(L"Colors", key, defaults[i]));
    }
    m_currentAlpha = dark ? 255 : LINE_ALPHA;
}

void CMainWindow::RenderAnnotations(Gdiplus::Graphics& graphics)
{
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
                // Cardinal spline through the captured points: smooths the
                // polyline so fast strokes don't show angular segments. Low
                // tension keeps sharp corners from ballooning.
                graphics.DrawCurve(&pen, line.points.data(), static_cast<int>(line.points.size()), 0.5f);
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
                        Gdiplus::FontFamily fontFamily(line.fontName.c_str());
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

void CMainWindow::RenderTextCaret(Gdiplus::Graphics& graphics)
{
    // Blinking caret shown only while typing. Drawn as editing chrome here in
    // WM_PAINT (not in RenderAnnotations) so it never ends up in screenshots.
    if (!m_bTextMode || !m_caretVisible || m_drawLines.empty())
        return;
    const auto& line = m_drawLines.back();
    if (line.lineType != LineType::Text)
        return;

    Gdiplus::FontFamily fontFamily(line.fontName.c_str());
    Gdiplus::Font       font(&fontFamily, static_cast<Gdiplus::REAL>(line.fontSize), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    // Caret sits at the end of the text already typed (or at the origin if empty).
    Gdiplus::REAL caretX = static_cast<Gdiplus::REAL>(line.lineStartPoint.X);
    if (!line.text.empty())
    {
        Gdiplus::RectF  bounds;
        Gdiplus::PointF origin(0, 0);
        graphics.MeasureString(line.text.c_str(), static_cast<INT>(line.text.size()), &font, origin, &bounds);
        caretX += bounds.Width;
    }
    // Anchor the caret's bottom on the text baseline so the text sits at the
    // bottom of the caret, not the top. DrawString places the top of the em
    // box at lineStartPoint.Y and the baseline one ascent below it; a caret of
    // full fontSize height would otherwise run past the baseline into the
    // descent space and leave the text floating at the top.
    const Gdiplus::REAL emHeight  = fontFamily.GetEmHeight(Gdiplus::FontStyleRegular);
    const Gdiplus::REAL ascent    = fontFamily.GetCellAscent(Gdiplus::FontStyleRegular);
    const Gdiplus::REAL scale     = static_cast<Gdiplus::REAL>(line.fontSize) / emHeight;
    const Gdiplus::REAL baselineY = static_cast<Gdiplus::REAL>(line.lineStartPoint.Y) + ascent * scale;

    Gdiplus::REAL caretHeight = ascent * scale;
    Gdiplus::REAL caretY      = baselineY - caretHeight;

    Gdiplus::Color color;
    color.SetValue(Gdiplus::Color::MakeARGB(255, GetRValue(m_colors[line.colorIndex]), GetGValue(m_colors[line.colorIndex]), GetBValue(m_colors[line.colorIndex])));
    Gdiplus::Pen caretPen(color, std::max(2.0f, line.fontSize / 16.0f));
    graphics.DrawLine(&caretPen, caretX, caretY, caretX, caretY + caretHeight);
}

void CMainWindow::PaintThemeBackground()
{
    int cx = m_rcScreen.right - m_rcScreen.left;
    int cy = m_rcScreen.bottom - m_rcScreen.top;

    if (m_theme == Theme::Transparent && m_desktopSnapshotDC)
    {
        // Restore from the snapshot taken at draw start; recapturing now
        // would include our own painted background.
        BitBlt(hDesktopCompatibleDC, 0, 0, cx, cy, m_desktopSnapshotDC, 0, 0, SRCCOPY);
    }
    else
    {
        COLORREF bg     = (m_theme == Theme::Dark) ? RGB(0, 0, 0) : RGB(255, 255, 255);
        RECT     rect   = {0, 0, cx, cy};
        HBRUSH   hBrush = CreateSolidBrush(bg);
        FillRect(hDesktopCompatibleDC, &rect, hBrush);
        DeleteObject(hBrush);
    }

    if (m_boardStyle != BoardStyle::None)
        PaintBoardFrame();
}

static void DrawRoundedRect(Gdiplus::Graphics& graphics, Gdiplus::REAL x, Gdiplus::REAL y,
                            Gdiplus::REAL w, Gdiplus::REAL h, Gdiplus::REAL r, const Gdiplus::Pen& pen)
{
    Gdiplus::REAL d = r * 2;
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, d, d, 180, 90);
    path.AddArc(x + w - d, y, d, d, 270, 90);
    path.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    path.AddArc(x, y + h - d, d, d, 90, 90);
    path.CloseFigure();
    graphics.DrawPath(&pen, &path);
}

void CMainWindow::PaintBoardFrame()
{
    // Decorative frame drawn on top of the theme background. Geometry is
    // expressed in the original 1920x1080 design space and scaled to the
    // live screen so it stays crisp at any resolution.
    int cx = m_rcScreen.right - m_rcScreen.left;
    int cy = m_rcScreen.bottom - m_rcScreen.top;
    if (cx <= 0 || cy <= 0)
        return;

    const double sx = cx / 1920.0;
    const double sy = cy / 1080.0;
    const double s  = sy; // uniform scale for stroke widths / tick sizes

    auto X = [&](double v) { return static_cast<Gdiplus::REAL>(v * sx); };
    auto Y = [&](double v) { return static_cast<Gdiplus::REAL>(v * sy); };
    auto W = [&](double v) { return static_cast<Gdiplus::REAL>(std::max(1.0, v * s)); };

    Gdiplus::Graphics graphics(hDesktopCompatibleDC);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const Gdiplus::Color clay(0xFF, 0xD9, 0x77, 0x57);

    if (m_boardStyle == BoardStyle::FrameA)
    {
        // Light whiteboard: paper gradient, grey mat border, thin clay
        // liseré framing the ~95% canvas, plus corner registration ticks.
        Gdiplus::RectF full(0.0f, 0.0f, static_cast<Gdiplus::REAL>(cx), static_cast<Gdiplus::REAL>(cy));
        Gdiplus::LinearGradientBrush paper(full,
                                           Gdiplus::Color(0xFF, 0xFD, 0xFD, 0xFC),
                                           Gdiplus::Color(0xFF, 0xF4, 0xF2, 0xEE),
                                           Gdiplus::LinearGradientModeVertical);
        graphics.FillRectangle(&paper, full);

        // Grey mat: a thick border stroke around the outer edge.
        Gdiplus::Pen matPen(Gdiplus::Color(0xFF, 0xE7, 0xE3, 0xDC), W(28));
        graphics.DrawRectangle(&matPen, X(14), Y(14), X(1920) - X(28), Y(1080) - Y(28));

        // Thin clay liseré, rounded, framing the drawing zone.
        DrawRoundedRect(graphics, X(34), Y(34), X(1852), Y(1012), W(8), Gdiplus::Pen(clay, W(3)));

        // Corner registration ticks.
        Gdiplus::Pen tickPen(clay, W(4));
        tickPen.SetStartCap(Gdiplus::LineCapRound);
        tickPen.SetEndCap(Gdiplus::LineCapRound);
        // top-left
        graphics.DrawLine(&tickPen, X(60), Y(34), X(60), Y(70));
        graphics.DrawLine(&tickPen, X(34), Y(60), X(70), Y(60));
        // top-right
        graphics.DrawLine(&tickPen, X(1860), Y(34), X(1860), Y(70));
        graphics.DrawLine(&tickPen, X(1886), Y(60), X(1850), Y(60));
        // bottom-left
        graphics.DrawLine(&tickPen, X(60), Y(1046), X(60), Y(1010));
        graphics.DrawLine(&tickPen, X(34), Y(1020), X(70), Y(1020));
        // bottom-right
        graphics.DrawLine(&tickPen, X(1860), Y(1046), X(1860), Y(1010));
        graphics.DrawLine(&tickPen, X(1886), Y(1020), X(1850), Y(1020));
    }
    else if (m_boardStyle == BoardStyle::FrameB)
    {
        // Dark slate board: lighter bevelled frame, radial slate canvas,
        // clay "tray" baseline. The plain Dark theme already filled the
        // background black, so we paint the full decor here.
        Gdiplus::RectF full(0.0f, 0.0f, static_cast<Gdiplus::REAL>(cx), static_cast<Gdiplus::REAL>(cy));

        // Outer frame, vertical gradient.
        Gdiplus::LinearGradientBrush frame(full,
                                           Gdiplus::Color(0xFF, 0x3A, 0x3D, 0x42),
                                           Gdiplus::Color(0xFF, 0x2C, 0x2E, 0x33),
                                           Gdiplus::LinearGradientModeVertical);
        graphics.FillRectangle(&frame, full);

        // Inner bevel highlight.
        Gdiplus::Pen bevelPen(Gdiplus::Color(0xFF, 0x4A, 0x4D, 0x53), W(2));
        graphics.DrawRectangle(&bevelPen, X(22), Y(22), X(1920) - X(44), Y(1080) - Y(44));

        // Slate canvas (~95%), radial gradient centre-biased upward.
        Gdiplus::RectF canvas(X(40), Y(40), X(1840), Y(1000));
        Gdiplus::GraphicsPath canvasPath;
        canvasPath.AddRectangle(canvas);
        Gdiplus::PathGradientBrush slate(&canvasPath);
        slate.SetCenterPoint(Gdiplus::PointF(X(960), Y(454)));
        slate.SetCenterColor(Gdiplus::Color(0xFF, 0x2A, 0x2D, 0x31));
        Gdiplus::Color surround(0xFF, 0x20, 0x22, 0x25);
        int surroundCount = 1;
        slate.SetSurroundColors(&surround, &surroundCount);
        graphics.FillRectangle(&slate, canvas);

        // Clay tray baseline at the bottom of the board.
        Gdiplus::SolidBrush tray(Gdiplus::Color(0xD9, 0xD9, 0x77, 0x57));
        graphics.FillRectangle(&tray, X(40), Y(1028), X(1840), Y(12));
    }
}

static int GetPngEncoderClsid(CLSID* pClsid)
{
    UINT num  = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0)
        return -1;
    auto* pImageCodecInfo = static_cast<Gdiplus::ImageCodecInfo*>(malloc(size));
    if (!pImageCodecInfo)
        return -1;
    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
    int result = -1;
    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(pImageCodecInfo[i].MimeType, L"image/png") == 0)
        {
            *pClsid = pImageCodecInfo[i].Clsid;
            result  = static_cast<int>(i);
            break;
        }
    }
    free(pImageCodecInfo);
    return result;
}

// Recursively create every directory along the path. Existing dirs are fine.
static void EnsureDirectory(const std::wstring& path)
{
    if (path.empty())
        return;
    for (size_t i = 0; i < path.size(); ++i)
    {
        if (path[i] == L'\\' || path[i] == L'/')
        {
            if (i > 0)
                CreateDirectoryW(path.substr(0, i).c_str(), nullptr);
        }
    }
    CreateDirectoryW(path.c_str(), nullptr);
}

// Strip characters Windows forbids in file/folder names, plus trim.
static std::wstring SanitizeForPath(std::wstring s)
{
    const std::wstring forbidden = L"\\/:*?\"<>|";
    for (auto& ch : s)
    {
        if (forbidden.find(ch) != std::wstring::npos || ch < 0x20)
            ch = L'_';
    }
    // Trim leading/trailing spaces and dots (illegal as trailing on Windows).
    size_t start = s.find_first_not_of(L" .");
    size_t end   = s.find_last_not_of(L" .");
    if (start == std::wstring::npos)
        return L"";
    return s.substr(start, end - start + 1);
}

// Find the active Google Meet tab and return the meeting name, or empty.
// Chrome window titles follow "Meet - <name> - Google Chrome".
static BOOL CALLBACK FindMeetProc(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;
    int len = GetWindowTextLengthW(hwnd);
    if (len <= 0)
        return TRUE;
    std::wstring title(len + 1, L'\0');
    GetWindowTextW(hwnd, &title[0], len + 1);
    title.resize(len);

    const std::wstring prefix = L"Meet - ";
    const std::wstring suffix = L" - Google Chrome";
    if (title.size() > prefix.size() + suffix.size() &&
        title.compare(0, prefix.size(), prefix) == 0 &&
        title.compare(title.size() - suffix.size(), suffix.size(), suffix) == 0)
    {
        std::wstring name = title.substr(prefix.size(), title.size() - prefix.size() - suffix.size());
        *reinterpret_cast<std::wstring*>(lParam) = name;
        return FALSE; // stop enumeration
    }
    return TRUE;
}

static std::wstring GetMeetName()
{
    std::wstring name;
    EnumWindows(FindMeetProc, reinterpret_cast<LPARAM>(&name));
    return SanitizeForPath(name);
}

void CMainWindow::SaveScreenshot()
{
    if (m_drawLines.empty())
        return;

    // Auto-capture is opt-out (Options > Screenshot).
    if (CIniSettings::Instance().GetInt64(L"Screenshot", L"enabled", 1) == 0)
        return;

    int cx = m_rcScreen.right - m_rcScreen.left;
    int cy = m_rcScreen.bottom - m_rcScreen.top;
    if (cx <= 0 || cy <= 0)
        return;

    // Compose the theme background (already in hDesktopCompatibleDC) with the
    // annotations on top, into a fresh DIB we can encode to PNG.
    HDC     hScreenDC = GetDC(nullptr);
    HDC     hMemDC    = CreateCompatibleDC(hScreenDC);
    HBITMAP hBmp      = CreateCompatibleBitmap(hScreenDC, cx, cy);
    auto    hOld      = static_cast<HBITMAP>(SelectObject(hMemDC, hBmp));

    BitBlt(hMemDC, 0, 0, cx, cy, hDesktopCompatibleDC, 0, 0, SRCCOPY);
    {
        Gdiplus::Graphics graphics(hMemDC);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        RenderAnnotations(graphics);
    }

    // Root folder: the configured one, else %USERPROFILE%\Pictures\DemoInk.
    std::wstring baseDir = CIniSettings::Instance().GetString(L"Screenshot", L"folder", L"");
    if (baseDir.empty())
    {
        PWSTR picturesPath = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Pictures, 0, nullptr, &picturesPath)))
        {
            baseDir = picturesPath;
            CoTaskMemFree(picturesPath);
        }
        if (!baseDir.empty())
            baseDir += L"\\DemoInk";
    }
    if (baseDir.empty())
    {
        SelectObject(hMemDC, hOld);
        DeleteObject(hBmp);
        DeleteDC(hMemDC);
        ReleaseDC(nullptr, hScreenDC);
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t ymdDash[16], hms[16];
    swprintf_s(ymdDash, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    swprintf_s(hms, L"%02d-%02d-%02d.png", st.wHour, st.wMinute, st.wSecond);

    // Client name comes from the active Google Meet tab, unless detection is
    // turned off in Options (then captures are filed by date only).
    std::wstring client;
    if (CIniSettings::Instance().GetInt64(L"Screenshot", L"meetdetect", 1) != 0)
        client = GetMeetName();

    // Encode once into a reusable Gdiplus::Bitmap, then save to each tree.
    CLSID pngClsid;
    if (GetPngEncoderClsid(&pngClsid) >= 0)
    {
        Gdiplus::Bitmap bitmap(hBmp, nullptr);

        // Tree 2 — by date: By date\YYYY-MM-DD\[<client>\]HH-MM-SS.png
        // Always written (client optional). This is the fallback when no
        // Meet name is detected or detection is off.
        std::wstring byDate = baseDir + L"\\By date\\" + ymdDash;
        if (!client.empty())
            byDate += L"\\" + client;
        EnsureDirectory(byDate);
        bitmap.Save((byDate + L"\\" + hms).c_str(), &pngClsid, nullptr);

        // Tree 1 — by client: By client\<client>\YYYY-MM-DD\HH-MM-SS.png
        // Only written when a client name was detected.
        if (!client.empty())
        {
            std::wstring byClient = baseDir + L"\\By client\\" + client + L"\\" + ymdDash;
            EnsureDirectory(byClient);
            bitmap.Save((byClient + L"\\" + hms).c_str(), &pngClsid, nullptr);
        }
    }

    SelectObject(hMemDC, hOld);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
    ReleaseDC(nullptr, hScreenDC);
}
