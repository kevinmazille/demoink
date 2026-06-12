// demoHelper - screen drawing and presentation tool

// Copyright (C) 2007-2008, 2012, 2015, 2020-2021, 2023 - Stefan Kueng

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
#include "DemoHelper.h"
#include "MainWindow.h"
#include "IniSettings.h"
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>

// Builds the tabbed Options dialog as a PropertySheet. Each tab is a child
// property page; every page reads its settings on WM_INITDIALOG and writes
// them back on PSN_APPLY (i.e. when the user clicks OK), then they are saved
// to the INI once here.
void CMainWindow::ShowOptionsSheet(HWND hParent)
{
    PROPSHEETPAGE psp[7] = {0};

    psp[0].dwSize      = sizeof(PROPSHEETPAGE);
    psp[0].dwFlags     = PSP_DEFAULT;
    psp[0].hInstance   = g_hInstance;
    psp[0].pszTemplate = MAKEINTRESOURCE(IDD_OPT_GENERAL);
    psp[0].pfnDlgProc  = GeneralPageProc;

    psp[1].dwSize      = sizeof(PROPSHEETPAGE);
    psp[1].dwFlags     = PSP_DEFAULT;
    psp[1].hInstance   = g_hInstance;
    psp[1].pszTemplate = MAKEINTRESOURCE(IDD_OPT_DRAW);
    psp[1].pfnDlgProc  = DrawPageProc;

    psp[2].dwSize      = sizeof(PROPSHEETPAGE);
    psp[2].dwFlags     = PSP_DEFAULT;
    psp[2].hInstance   = g_hInstance;
    psp[2].pszTemplate = MAKEINTRESOURCE(IDD_OPT_TEXT);
    psp[2].pfnDlgProc  = TextPageProc;

    psp[3].dwSize      = sizeof(PROPSHEETPAGE);
    psp[3].dwFlags     = PSP_DEFAULT;
    psp[3].hInstance   = g_hInstance;
    psp[3].pszTemplate = MAKEINTRESOURCE(IDD_OPT_COLORS);
    psp[3].pfnDlgProc  = ColorsPageProc;

    psp[4].dwSize      = sizeof(PROPSHEETPAGE);
    psp[4].dwFlags     = PSP_DEFAULT;
    psp[4].hInstance   = g_hInstance;
    psp[4].pszTemplate = MAKEINTRESOURCE(IDD_OPT_SCREENSHOT);
    psp[4].pfnDlgProc  = ScreenshotPageProc;

    psp[5].dwSize      = sizeof(PROPSHEETPAGE);
    psp[5].dwFlags     = PSP_DEFAULT;
    psp[5].hInstance   = g_hInstance;
    psp[5].pszTemplate = MAKEINTRESOURCE(IDD_OPT_SHORTCUTS);
    psp[5].pfnDlgProc  = ShortcutsPageProc;

    psp[6].dwSize      = sizeof(PROPSHEETPAGE);
    psp[6].dwFlags     = PSP_DEFAULT;
    psp[6].hInstance   = g_hInstance;
    psp[6].pszTemplate = MAKEINTRESOURCE(IDD_OPT_BACKGROUND);
    psp[6].pfnDlgProc  = BackgroundPageProc;

    PROPSHEETHEADER psh = {0};
    psh.dwSize          = sizeof(PROPSHEETHEADER);
    psh.dwFlags         = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW | PSH_USEHICON;
    psh.hwndParent      = hParent;
    psh.hInstance       = g_hInstance;
    psh.hIcon           = LoadIcon(g_hResource, MAKEINTRESOURCE(IDI_DEMOHELPER));
    psh.pszCaption      = L"DemoInk Options";
    psh.nPages          = _countof(psp);
    psh.nStartPage      = 0;
    psh.ppsp            = psp;

    if (PropertySheet(&psh) > 0)
        CIniSettings::Instance().Save();
}

INT_PTR CALLBACK CMainWindow::GeneralPageProc(HWND hwndDlg, UINT message, WPARAM /*wParam*/, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            auto allMonitors = CIniSettings::Instance().GetInt64(L"Misc", L"allmonitors", 0);
            CheckRadioButton(hwndDlg, IDC_CURRENTMONITOR, IDC_ALLMONITORS, allMonitors ? IDC_ALLMONITORS : IDC_CURRENTMONITOR);
        }
        break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            switch (pnmh->code)
            {
                case PSN_APPLY:
                    CIniSettings::Instance().SetInt64(L"Misc", L"allmonitors", IsDlgButtonChecked(hwndDlg, IDC_ALLMONITORS) ? 1 : 0);
                    SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                    return TRUE;
                case NM_CLICK:
                case NM_RETURN:
                {
                    auto pNMLink = reinterpret_cast<PNMLINK>(lParam);
                    if (pnmh->hwndFrom == GetDlgItem(hwndDlg, IDC_SYSLINK1) && pNMLink->item.iLink == 0)
                        ShellExecute(nullptr, L"open", pNMLink->item.szUrl, nullptr, nullptr, SW_SHOW);
                    break;
                }
            }
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK CMainWindow::DrawPageProc(HWND hwndDlg, UINT message, WPARAM /*wParam*/, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            auto  draw        = static_cast<WORD>(CIniSettings::Instance().GetInt64(L"HotKeys", L"draw", 0x232));
            auto  fadeSeconds = CIniSettings::Instance().GetInt64(L"Draw", L"fadeseconds", 0);
            SendMessage(GetDlgItem(hwndDlg, IDC_HOTKEY_DRAWMODE), HKM_SETHOTKEY, static_cast<WPARAM>(draw), 0);
            TCHAR buffer[128] = {0};
            _stprintf_s(buffer, _countof(buffer), _T("%ld"), static_cast<DWORD>(fadeSeconds));
            SetWindowText(GetDlgItem(hwndDlg, IDC_FADESECONDS), buffer);

            auto defaultColor = CIniSettings::Instance().GetInt64(L"Draw", L"defaultcolor", 1);
            _stprintf_s(buffer, _countof(buffer), _T("%ld"), static_cast<DWORD>(defaultColor));
            SetWindowText(GetDlgItem(hwndDlg, IDC_DEFAULTCOLOR), buffer);
            auto defaultPen = CIniSettings::Instance().GetInt64(L"Draw", L"defaultpenwidth", 6);
            _stprintf_s(buffer, _countof(buffer), _T("%ld"), static_cast<DWORD>(defaultPen));
            SetWindowText(GetDlgItem(hwndDlg, IDC_DEFAULTPENWIDTH), buffer);

            CheckDlgButton(hwndDlg, IDC_DEFAULTOPAQUE, CIniSettings::Instance().GetInt64(L"Draw", L"startopaque", 0) ? BST_CHECKED : BST_UNCHECKED);
        }
        break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY)
            {
                LRESULT res = SendMessage(GetDlgItem(hwndDlg, IDC_HOTKEY_DRAWMODE), HKM_GETHOTKEY, 0, 0);
                CIniSettings::Instance().SetInt64(L"HotKeys", L"draw", res);
                TCHAR buffer[128] = {0};
                GetWindowText(GetDlgItem(hwndDlg, IDC_FADESECONDS), buffer, _countof(buffer));
                CIniSettings::Instance().SetString(L"Draw", L"fadeseconds", buffer);

                GetWindowText(GetDlgItem(hwndDlg, IDC_DEFAULTCOLOR), buffer, _countof(buffer));
                int color = _wtoi(buffer);
                if (color < 0)
                    color = 0;
                if (color > 9)
                    color = 9;
                CIniSettings::Instance().SetInt64(L"Draw", L"defaultcolor", color);

                GetWindowText(GetDlgItem(hwndDlg, IDC_DEFAULTPENWIDTH), buffer, _countof(buffer));
                int pen = _wtoi(buffer);
                if (pen < 1)
                    pen = 1;
                if (pen > 32)
                    pen = 32;
                CIniSettings::Instance().SetInt64(L"Draw", L"defaultpenwidth", pen);

                CIniSettings::Instance().SetInt64(L"Draw", L"startopaque", IsDlgButtonChecked(hwndDlg, IDC_DEFAULTOPAQUE) ? 1 : 0);
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

std::wstring CMainWindow::ResolveTextFont()
{
    std::wstring        font = CIniSettings::Instance().GetString(L"Text", L"font", TEXT_FONTS[0]);
    Gdiplus::FontFamily family(font.c_str());
    if (family.IsAvailable())
        return font;
    return TEXT_FONTS[0]; // stored font not installed -> safe default
}

INT_PTR CALLBACK CMainWindow::TextPageProc(HWND hwndDlg, UINT message, WPARAM /*wParam*/, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            HWND hCombo = GetDlgItem(hwndDlg, IDC_TEXTFONT);
            for (auto* name : TEXT_FONTS)
                SendMessage(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name));
            std::wstring font = CIniSettings::Instance().GetString(L"Text", L"font", TEXT_FONTS[0]);
            if (SendMessage(hCombo, CB_SELECTSTRING, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(font.c_str())) == CB_ERR)
                SendMessage(hCombo, CB_SETCURSEL, 0, 0);

            auto  size        = CIniSettings::Instance().GetInt64(L"Text", L"defaultsize", 30);
            TCHAR buffer[32]  = {0};
            _stprintf_s(buffer, _countof(buffer), _T("%ld"), static_cast<DWORD>(size));
            SetWindowText(GetDlgItem(hwndDlg, IDC_TEXTSIZE), buffer);
        }
        break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY)
            {
                HWND hCombo = GetDlgItem(hwndDlg, IDC_TEXTFONT);
                int  sel    = static_cast<int>(SendMessage(hCombo, CB_GETCURSEL, 0, 0));
                if (sel >= 0 && sel < static_cast<int>(_countof(TEXT_FONTS)))
                    CIniSettings::Instance().SetString(L"Text", L"font", TEXT_FONTS[sel]);

                TCHAR buffer[32] = {0};
                GetWindowText(GetDlgItem(hwndDlg, IDC_TEXTSIZE), buffer, _countof(buffer));
                int size = _wtoi(buffer);
                if (size < 8)
                    size = 8;
                if (size > 256)
                    size = 256;
                CIniSettings::Instance().SetInt64(L"Text", L"defaultsize", size);
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

// Per-dialog working state for the Colors page: both palettes are edited
// in memory and only written to the INI on OK (PSN_APPLY). 0 = Light, 1 = Dark.
struct ColorsPageState
{
    COLORREF colors[2][10];
    int      theme; // which palette the swatches currently show
};

INT_PTR CALLBACK CMainWindow::ColorsPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<ColorsPageState*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
    switch (message)
    {
        case WM_INITDIALOG:
        {
            st = new ColorsPageState();
            for (int t = 0; t < 2; ++t)
            {
                const COLORREF* defs   = (t == 1) ? DEFAULT_COLORS_DARK : DEFAULT_COLORS_LIGHT;
                const wchar_t*  prefix = (t == 1) ? L"dark" : L"light";
                for (int i = 0; i < 10; ++i)
                {
                    wchar_t key[16] = {0};
                    swprintf_s(key, _countof(key), L"%s%d", prefix, i);
                    st->colors[t][i] = static_cast<COLORREF>(CIniSettings::Instance().GetInt64(L"Colors", key, defs[i]));
                }
            }
            st->theme = 0;
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            CheckRadioButton(hwndDlg, IDC_COLOR_THEME_LIGHT, IDC_COLOR_THEME_DARK, IDC_COLOR_THEME_LIGHT);
        }
        break;
        case WM_DESTROY:
            delete st;
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
            break;
        case WM_DRAWITEM:
        {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            int   idx = static_cast<int>(dis->CtlID) - IDC_SWATCH0;
            if (st && idx >= 0 && idx < 10)
            {
                COLORREF c     = st->colors[st->theme][idx];
                HBRUSH   brush = CreateSolidBrush(c);
                FillRect(dis->hDC, &dis->rcItem, brush);
                DeleteObject(brush);
                FrameRect(dis->hDC, &dis->rcItem, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                // index label, readable on either light or dark swatch
                wchar_t label[4] = {0};
                swprintf_s(label, _countof(label), L"%d", idx);
                int lum = (GetRValue(c) * 299 + GetGValue(c) * 587 + GetBValue(c) * 114) / 1000;
                SetBkMode(dis->hDC, TRANSPARENT);
                SetTextColor(dis->hDC, lum < 128 ? RGB(255, 255, 255) : RGB(0, 0, 0));
                DrawText(dis->hDC, label, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                if (dis->itemState & ODS_FOCUS)
                    DrawFocusRect(dis->hDC, &dis->rcItem);
                return TRUE;
            }
        }
        break;
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (!st)
                break;
            if (id == IDC_COLOR_THEME_LIGHT || id == IDC_COLOR_THEME_DARK)
            {
                st->theme = (id == IDC_COLOR_THEME_DARK) ? 1 : 0;
                for (int i = 0; i < 10; ++i)
                    InvalidateRect(GetDlgItem(hwndDlg, IDC_SWATCH0 + i), nullptr, TRUE);
            }
            else if (id == IDC_COLOR_RESET)
            {
                const COLORREF* defs = (st->theme == 1) ? DEFAULT_COLORS_DARK : DEFAULT_COLORS_LIGHT;
                for (int i = 0; i < 10; ++i)
                {
                    st->colors[st->theme][i] = defs[i];
                    InvalidateRect(GetDlgItem(hwndDlg, IDC_SWATCH0 + i), nullptr, TRUE);
                }
            }
            else if (id >= IDC_SWATCH0 && id <= IDC_SWATCH9 && HIWORD(wParam) == BN_CLICKED)
            {
                int            idx = id - IDC_SWATCH0;
                static COLORREF custom[16] = {0};
                CHOOSECOLOR    cc          = {0};
                cc.lStructSize             = sizeof(cc);
                cc.hwndOwner               = hwndDlg;
                cc.rgbResult               = st->colors[st->theme][idx];
                cc.lpCustColors            = custom;
                cc.Flags                   = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColor(&cc))
                {
                    st->colors[st->theme][idx] = cc.rgbResult;
                    InvalidateRect(GetDlgItem(hwndDlg, id), nullptr, TRUE);
                }
            }
        }
        break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY && st)
            {
                for (int t = 0; t < 2; ++t)
                {
                    const wchar_t* prefix = (t == 1) ? L"dark" : L"light";
                    for (int i = 0; i < 10; ++i)
                    {
                        wchar_t key[16] = {0};
                        swprintf_s(key, _countof(key), L"%s%d", prefix, i);
                        CIniSettings::Instance().SetInt64(L"Colors", key, st->colors[t][i]);
                    }
                }
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK CMainWindow::ScreenshotPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            CheckDlgButton(hwndDlg, IDC_SHOT_ENABLED, CIniSettings::Instance().GetInt64(L"Screenshot", L"enabled", 1) ? BST_CHECKED : BST_UNCHECKED);
            CheckDlgButton(hwndDlg, IDC_SHOT_MEETDETECT, CIniSettings::Instance().GetInt64(L"Screenshot", L"meetdetect", 1) ? BST_CHECKED : BST_UNCHECKED);
            std::wstring folder = CIniSettings::Instance().GetString(L"Screenshot", L"folder", L"");
            SetWindowText(GetDlgItem(hwndDlg, IDC_SHOT_FOLDER), folder.c_str());
        }
        break;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SHOT_BROWSE)
            {
                wchar_t      path[MAX_PATH] = {0};
                BROWSEINFO   bi             = {0};
                bi.hwndOwner                = hwndDlg;
                bi.lpszTitle                = L"Choose where to save screenshots";
                bi.ulFlags                  = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_EDITBOX;
                LPITEMIDLIST pidl           = SHBrowseForFolder(&bi);
                if (pidl)
                {
                    if (SHGetPathFromIDList(pidl, path))
                        SetWindowText(GetDlgItem(hwndDlg, IDC_SHOT_FOLDER), path);
                    CoTaskMemFree(pidl);
                }
            }
            break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY)
            {
                CIniSettings::Instance().SetInt64(L"Screenshot", L"enabled", IsDlgButtonChecked(hwndDlg, IDC_SHOT_ENABLED) ? 1 : 0);
                CIniSettings::Instance().SetInt64(L"Screenshot", L"meetdetect", IsDlgButtonChecked(hwndDlg, IDC_SHOT_MEETDETECT) ? 1 : 0);
                wchar_t buffer[MAX_PATH] = {0};
                GetWindowText(GetDlgItem(hwndDlg, IDC_SHOT_FOLDER), buffer, _countof(buffer));
                CIniSettings::Instance().SetString(L"Screenshot", L"folder", buffer);
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK CMainWindow::ShortcutsPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
        {
            for (const auto& def : SHORTCUTS)
            {
                HWND hEdit = GetDlgItem(hwndDlg, def.editId);
                SendMessage(hEdit, EM_SETLIMITTEXT, 1, 0); // one letter per action
                wchar_t letter[2] = {ResolveShortcutLetter(def), 0};
                SetWindowText(hEdit, letter);
            }
        }
        break;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDC_SHORTCUT_RESET)
            {
                for (const auto& def : SHORTCUTS)
                {
                    wchar_t letter[2] = {def.defaultLetter, 0};
                    SetWindowText(GetDlgItem(hwndDlg, def.editId), letter);
                }
            }
            break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY)
            {
                // Collect the chosen letters; reject blanks, non-letters and
                // duplicates so the accelerator table can't end up ambiguous.
                wchar_t chosen[_countof(SHORTCUTS)] = {0};
                for (int i = 0; i < static_cast<int>(_countof(SHORTCUTS)); ++i)
                {
                    wchar_t buffer[8] = {0};
                    GetWindowText(GetDlgItem(hwndDlg, SHORTCUTS[i].editId), buffer, _countof(buffer));
                    wchar_t c = towupper(buffer[0]);
                    if (c < L'A' || c > L'Z')
                    {
                        MessageBox(hwndDlg, L"Each shortcut must be a single letter (A-Z).", L"Shortcuts", MB_ICONWARNING | MB_OK);
                        SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                        return TRUE;
                    }
                    for (int j = 0; j < i; ++j)
                    {
                        if (chosen[j] == c)
                        {
                            wchar_t msg[96] = {0};
                            swprintf_s(msg, _countof(msg), L"The key '%c' is assigned to more than one action.", c);
                            MessageBox(hwndDlg, msg, L"Shortcuts", MB_ICONWARNING | MB_OK);
                            SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
                            return TRUE;
                        }
                    }
                    chosen[i] = c;
                }

                for (int i = 0; i < static_cast<int>(_countof(SHORTCUTS)); ++i)
                {
                    std::wstring key   = std::wstring(L"key_") + SHORTCUTS[i].iniKey;
                    wchar_t      val[2] = {chosen[i], 0};
                    CIniSettings::Instance().SetString(L"Shortcuts", key.c_str(), val);
                }
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

// Per-dialog working state for the Background page: the two solid-fill colors
// are edited in memory and only written to the INI on OK (PSN_APPLY).
struct BackgroundPageState
{
    COLORREF light;
    COLORREF dark;
};

INT_PTR CALLBACK CMainWindow::BackgroundPageProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* st = reinterpret_cast<BackgroundPageState*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
    switch (message)
    {
        case WM_INITDIALOG:
        {
            st        = new BackgroundPageState();
            st->light = BackgroundColor(false);
            st->dark  = BackgroundColor(true);
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            SetWindowText(GetDlgItem(hwndDlg, IDC_BG_IMAGE), BackgroundImagePath().c_str());
        }
        break;
        case WM_DESTROY:
            delete st;
            SetWindowLongPtr(hwndDlg, GWLP_USERDATA, 0);
            break;
        case WM_DRAWITEM:
        {
            auto* dis = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (st && (dis->CtlID == IDC_BG_LIGHT_SWATCH || dis->CtlID == IDC_BG_DARK_SWATCH))
            {
                COLORREF c     = (dis->CtlID == IDC_BG_DARK_SWATCH) ? st->dark : st->light;
                HBRUSH   brush = CreateSolidBrush(c);
                FillRect(dis->hDC, &dis->rcItem, brush);
                DeleteObject(brush);
                FrameRect(dis->hDC, &dis->rcItem, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                if (dis->itemState & ODS_FOCUS)
                    DrawFocusRect(dis->hDC, &dis->rcItem);
                return TRUE;
            }
        }
        break;
        case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            if (!st)
                break;
            if ((id == IDC_BG_LIGHT_SWATCH || id == IDC_BG_DARK_SWATCH) && HIWORD(wParam) == BN_CLICKED)
            {
                COLORREF*       slot       = (id == IDC_BG_DARK_SWATCH) ? &st->dark : &st->light;
                static COLORREF custom[16] = {0};
                CHOOSECOLOR     cc         = {0};
                cc.lStructSize             = sizeof(cc);
                cc.hwndOwner               = hwndDlg;
                cc.rgbResult               = *slot;
                cc.lpCustColors            = custom;
                cc.Flags                   = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColor(&cc))
                {
                    *slot = cc.rgbResult;
                    InvalidateRect(GetDlgItem(hwndDlg, id), nullptr, TRUE);
                }
            }
            else if (id == IDC_BG_RESET)
            {
                st->light = DEFAULT_BG_LIGHT;
                st->dark  = DEFAULT_BG_DARK;
                InvalidateRect(GetDlgItem(hwndDlg, IDC_BG_LIGHT_SWATCH), nullptr, TRUE);
                InvalidateRect(GetDlgItem(hwndDlg, IDC_BG_DARK_SWATCH), nullptr, TRUE);
            }
            else if (id == IDC_BG_IMAGE_BROWSE)
            {
                wchar_t      file[MAX_PATH] = {0};
                OPENFILENAME ofn            = {0};
                ofn.lStructSize             = sizeof(ofn);
                ofn.hwndOwner               = hwndDlg;
                ofn.lpstrFilter             = L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif\0All files\0*.*\0";
                ofn.lpstrFile               = file;
                ofn.nMaxFile                = _countof(file);
                ofn.Flags                   = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                if (GetOpenFileName(&ofn))
                    SetWindowText(GetDlgItem(hwndDlg, IDC_BG_IMAGE), file);
            }
            else if (id == IDC_BG_IMAGE_CLEAR)
            {
                SetWindowText(GetDlgItem(hwndDlg, IDC_BG_IMAGE), L"");
            }
        }
        break;
        case WM_NOTIFY:
        {
            auto pnmh = reinterpret_cast<LPNMHDR>(lParam);
            if (pnmh->code == PSN_APPLY && st)
            {
                CIniSettings::Instance().SetInt64(L"Background", L"light", st->light);
                CIniSettings::Instance().SetInt64(L"Background", L"dark", st->dark);
                wchar_t buffer[MAX_PATH] = {0};
                GetWindowText(GetDlgItem(hwndDlg, IDC_BG_IMAGE), buffer, _countof(buffer));
                CIniSettings::Instance().SetString(L"Background", L"image", buffer);
                SetWindowLongPtr(hwndDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
                return TRUE;
            }
        }
        break;
    }
    return FALSE;
}

WORD CMainWindow::HotKeyControl2HotKey(WORD hk)
{
    UINT flags = 0;
    if (HIBYTE(hk) & HOTKEYF_ALT)
        flags |= MOD_ALT;
    if (HIBYTE(hk) & HOTKEYF_SHIFT)
        flags |= MOD_SHIFT;
    if (HIBYTE(hk) & HOTKEYF_EXT)
        flags |= MOD_WIN;
    if (HIBYTE(hk) & HOTKEYF_CONTROL)
        flags |= MOD_CONTROL;
    return MAKEWORD(LOBYTE(hk), flags);
}

WORD CMainWindow::HotKey2HotKeyControl(WORD hk)
{
    UINT flags = 0;
    if (HIBYTE(hk) & MOD_ALT)
        flags |= HOTKEYF_ALT;
    if (HIBYTE(hk) & MOD_SHIFT)
        flags |= HOTKEYF_SHIFT;
    if (HIBYTE(hk) & MOD_WIN)
        flags |= HOTKEYF_EXT;
    if (HIBYTE(hk) & MOD_CONTROL)
        flags |= HOTKEYF_CONTROL;
    return MAKEWORD(LOBYTE(hk), flags);
}
