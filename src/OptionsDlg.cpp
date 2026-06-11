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
#include <shellapi.h>

// Builds the tabbed Options dialog as a PropertySheet. Each tab is a child
// property page; every page reads its settings on WM_INITDIALOG and writes
// them back on PSN_APPLY (i.e. when the user clicks OK), then they are saved
// to the INI once here.
void CMainWindow::ShowOptionsSheet(HWND hParent)
{
    PROPSHEETPAGE psp[2] = {0};

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
