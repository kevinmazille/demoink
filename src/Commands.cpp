// demoHelper - screen drawing and presentation tool

// Copyright (C) 2007-2008, 2020-2021 - Stefan Kueng

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
#include "IniSettings.h"
#include "InfoRtfDialog.h"

extern HINSTANCE g_hInstance; // current instance
extern HINSTANCE g_hResource; // the resource dll

LRESULT CMainWindow::DoCommand(int id)
{
    switch (id)
    {
        case ID_CMD_TOGGLEROP:
            if (m_currentAlpha == 255)
                m_currentAlpha = LINE_ALPHA;
            else
                m_currentAlpha = 255;
            break;
        case ID_CMD_QUITMODE:
            if (m_bTextMode)
            {
                m_bTextMode = false;
                KillTimer(*this, TIMER_ID_CARET);
                if (!m_drawLines.empty() && m_drawLines.back().lineType == LineType::Text)
                    m_drawLines.pop_back();
                RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
                break;
            }
            EndPresentationMode();
            UpdateCursor();
            break;
        case ID_CMD_UNDOLINE:
            m_bDrawing = false;
            if (!m_drawLines.empty())
                m_drawLines.pop_back();
            RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
            break;
        case ID_CMD_REMOVEFIRST:
        {
            m_bDrawing = false;
            if (!m_drawLines.empty())
                m_drawLines.pop_front();
            RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
        }
        break;
        case ID_CMD_INCREASE:
            if (!m_drawLines.empty())
            {
                if (m_currentPenWidth < 32)
                {
                    m_currentPenWidth++;
                    RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
                }
            }
            UpdateCursor();
            break;
        case ID_CMD_DECREASE:
            if (!m_drawLines.empty())
            {
                if (m_currentPenWidth > 1)
                {
                    m_currentPenWidth--;
                    RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
                }
            }
            UpdateCursor();
            break;
        case ID_CMD_NEXTCOLOR:
            // cycle through colors
            if (!m_drawLines.empty())
            {
                if (m_colorIndex < 9)
                    m_colorIndex++;
                else
                    m_colorIndex = 0;
                RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
            }
            UpdateCursor();
            break;
        case ID_CMD_PREVCOLOR:
            // cycle through colors
            if (!m_drawLines.empty())
            {
                if (m_colorIndex > 0)
                    m_colorIndex--;
                else
                    m_colorIndex = 9;
                RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
            }
            UpdateCursor();
            break;
        case ID_CMD_COLOR0:
            m_colorIndex = 0;
            UpdateCursor();
            break;
        case ID_CMD_COLOR1:
            m_colorIndex = 1;
            UpdateCursor();
            break;
        case ID_CMD_COLOR2:
            m_colorIndex = 2;
            UpdateCursor();
            break;
        case ID_CMD_COLOR3:
            m_colorIndex = 3;
            UpdateCursor();
            break;
        case ID_CMD_COLOR4:
            m_colorIndex = 4;
            UpdateCursor();
            break;
        case ID_CMD_COLOR5:
            m_colorIndex = 5;
            UpdateCursor();
            break;
        case ID_CMD_COLOR6:
            m_colorIndex = 6;
            UpdateCursor();
            break;
        case ID_CMD_COLOR7:
            m_colorIndex = 7;
            UpdateCursor();
            break;
        case ID_CMD_COLOR8:
            m_colorIndex = 8;
            UpdateCursor();
            break;
        case ID_CMD_COLOR9:
            m_colorIndex = 9;
            UpdateCursor();
            break;
        case ID_CMD_CLEARLINES:
            m_bDrawing = false;
            m_drawLines.clear();
            RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
            break;
        case IDM_EXIT:
            Shell_NotifyIcon(NIM_DELETE, &niData);
            ::PostQuitMessage(0);
            return 0;
        case ID_TRAYCONTEXT_OPTIONS:
        {
            UnregisterHotKey(*this, DRAW_HOTKEY);
            DialogBox(hResource, MAKEINTRESOURCE(IDD_OPTIONS), *this, reinterpret_cast<DLGPROC>(OptionsDlgProc));
            RegisterHotKeys();
        }
        break;
        case ID_TRAYCONTEXT_DRAW:
            SetTimer(*this, TIMER_ID_DRAW, 300, nullptr);
            break;
        case ID_TRAYCONTEXT_AUTOSTART:
            SetAutostart(!IsAutostartEnabled());   // toggle; checkmark refreshes on next menu open
            break;
        case ID_CMD_TEXTMODE:
            if (!m_bTextMode)
            {
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(*this, &pt);

                DrawLine line;
                line.lineType       = LineType::Text;
                line.colorIndex     = m_colorIndex;
                line.penWidth       = m_currentPenWidth;
                line.alpha          = m_currentAlpha; // follow theme like strokes: alpha on Transparent/Light, opaque on Dark
                line.fontSize       = m_currentPenWidth * 5;
                line.lineStartPoint = Gdiplus::Point(pt.x, pt.y);
                m_drawLines.push_back(line);

                m_bTextMode    = true;
                m_bDrawing     = false;
                m_caretVisible = true;
                ::SetTimer(*this, TIMER_ID_CARET, 500, nullptr);
                RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
            }
            break;
        case ID_CMD_TOGGLETHEME:
        {
            if (m_theme == Theme::Transparent)
            {
                // First B press: leave the desktop snapshot, wipe annotations,
                // start fresh on a Light canvas. After that B only toggles
                // Light ↔ Dark.
                m_theme    = Theme::Light;
                m_bDrawing = false;
                m_drawLines.clear();
                ApplyTheme();
            }
            else
            {
                m_theme = (m_theme == Theme::Light) ? Theme::Dark : Theme::Light;
                ApplyTheme();
                for (auto& line : m_drawLines)
                    line.alpha = m_currentAlpha;
            }
            m_boardStyle = BoardStyle::None;
            PaintThemeBackground();
            UpdateCursor();
            RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
        }
        break;
        case ID_CMD_CYCLEBOARD:
        {
            // Leaving the pristine Transparent state (via B or N) wipes the
            // annotations for a fresh canvas; any later switch — B↔N, N↔N,
            // B↔B — preserves them. The frame is painted into the background
            // DC, so existing drawings stay rendered on top in WM_PAINT.
            bool leavingTransparent = (m_theme == Theme::Transparent);
            if (m_boardStyle == BoardStyle::None)
                m_boardStyle = BoardStyle::FrameA;
            else
                m_boardStyle = (m_boardStyle == BoardStyle::FrameA) ? BoardStyle::FrameB : BoardStyle::FrameA;
            m_theme = (m_boardStyle == BoardStyle::FrameA) ? Theme::Light : Theme::Dark;
            ApplyTheme();
            if (leavingTransparent)
            {
                m_bDrawing = false;
                m_drawLines.clear();
            }
            else
            {
                for (auto& line : m_drawLines)
                    line.alpha = m_currentAlpha;
            }
            PaintThemeBackground();
            UpdateCursor();
            RedrawWindow(*this, nullptr, nullptr, RDW_INTERNALPAINT | RDW_INVALIDATE);
        }
        break;
        case IDHELP:
        {
            CInfoRtfDialog dlg;
            RECT           rcOwner;
            auto           hwndOwner = GetDesktopWindow();
            GetWindowRect(hwndOwner, &rcOwner);
            const int width  = 470;
            const int height = 430;
            dlg.DoModal(hResource, hwndOwner, "DemoInk help", IDR_HELP, L"RTF", IDI_DEMOHELPER,
                        rcOwner.right - width, rcOwner.bottom - height, width, height);
        }
        break;
        default:
            break;
    };
    return 1;
}
