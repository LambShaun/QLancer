#include "MainWindow.h"
#include "Application.h"
#include "SearchService.h"
#include "CommandExecutor.h"
#include "Utils.h"

MainWindow::MainWindow(Application* app) : m_app(app) {}

MainWindow::~MainWindow() {
    CleanupFont();
    if (m_hbrEditBackground) DeleteObject(m_hbrEditBackground);
    if (m_hbrListBoxBackground) DeleteObject(m_hbrListBoxBackground);
    if (m_hbrListBoxSelectedBackground) DeleteObject(m_hbrListBoxSelectedBackground);
    if (m_hbrClassBackground) DeleteObject(m_hbrClassBackground);
}

bool MainWindow::Create(HINSTANCE hInstance, const wchar_t* className, const wchar_t* windowTitle) {
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = MainWindow::WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    m_hbrClassBackground = CreateSolidBrush(m_clrListBoxBg);
    wcex.hbrBackground = m_hbrClassBackground;
    wcex.lpszClassName = className;

    if (!RegisterClassExW(&wcex)) {
        if (m_hbrClassBackground) DeleteObject(m_hbrClassBackground);
        return false;
    }

    m_hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        className, windowTitle, WS_POPUP | WS_SYSMENU,
        0, 0, 600, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP),
        NULL, NULL, hInstance, this 
    );

    if (m_hWnd) {
        SetLayeredWindowAttributes(m_hWnd, 0, (255 * 95) / 100, LWA_ALPHA);
        RegisterHotKey(m_hWnd, m_app->GetHotkeyId(), MOD_ALT | MOD_NOREPEAT, VK_SPACE);
    }

    return m_hWnd != NULL;
}

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = nullptr;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<MainWindow*>(pCreate->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        pThis->m_hWnd = hWnd; 
    }
    else {
        pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE: {
        AddTrayIcon();
        m_app->GetSearchService().PreloadCacheAsync();
        RECT clientRect;
        GetClientRect(m_hWnd, &clientRect);
        int controlWidth = clientRect.right - (2 * G_PIXEL_GAP);

        m_hEdit = CreateWindowEx(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
            G_PIXEL_GAP, G_PIXEL_GAP,
            controlWidth, G_EDIT_HEIGHT,
            m_hWnd, (HMENU)101, m_app->GetInstance(), NULL
        );

        if (m_hEdit) {
            m_pfnOldEditProc = (WNDPROC)SetWindowLongPtr(m_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
            SetWindowLongPtr(m_hEdit, GWLP_USERDATA, (LONG_PTR)this);
        }

        m_hListBox = CreateWindowEx(0, L"LISTBOX", L"",
            WS_CHILD | LBS_NOTIFY | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
            G_PIXEL_GAP, G_PIXEL_GAP + G_EDIT_HEIGHT + G_PIXEL_GAP,
            controlWidth, 0, m_hWnd,
            (HMENU)102, m_app->GetInstance(), NULL
        );

        if (m_hListBox) {
            m_pfnOldListBoxProc = (WNDPROC)SetWindowLongPtr(m_hListBox, GWLP_WNDPROC, (LONG_PTR)ListBoxSubclassProc);
            SetWindowLongPtr(m_hListBox, GWLP_USERDATA, (LONG_PTR)this);
        }

        m_hbrEditBackground = CreateSolidBrush(m_clrEditBg);
        m_hbrListBoxBackground = CreateSolidBrush(m_clrListBoxBg);
        m_hbrListBoxSelectedBackground = CreateSolidBrush(m_clrListBoxSelectedBg);

        CreateAndSetFont();
        if (m_hListBox && m_hFont) {
            HDC hdc = GetDC(m_hListBox);
            if (hdc) {
                TEXTMETRIC tm;
                HFONT hOldFont = (HFONT)SelectObject(hdc, m_hFont);
                GetTextMetrics(hdc, &tm);
                SelectObject(hdc, hOldFont);
                ReleaseDC(m_hListBox, hdc);
                m_listBoxItemHeight = tm.tmHeight + G_PIXEL_GAP;
                SendMessage(m_hListBox, LB_SETITEMHEIGHT, 0, (LPARAM)m_listBoxItemHeight);
            }
        }
        if (m_hListBox) ShowWindow(m_hListBox, SW_HIDE);
        return 0;
    }
    case WM_MEASUREITEM: ((LPMEASUREITEMSTRUCT)lParam)->itemHeight = m_listBoxItemHeight; return TRUE;
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (!lpdis || lpdis->CtlID != 102 || lpdis->CtlType != ODT_LISTBOX || lpdis->itemID == (UINT_PTR)-1) return TRUE;

        const auto& results = m_app->GetSearchService().GetResults();
        if (lpdis->itemID >= results.size()) {
            FillRect(lpdis->hDC, &lpdis->rcItem, m_hbrListBoxBackground);
            return TRUE;
        }

        const SearchResult& currentItem = results[lpdis->itemID];
        bool isSelected = (lpdis->itemState & ODS_SELECTED);
        LRESULT topIndex = SendMessage(m_hListBox, LB_GETTOPINDEX, 0, 0);
        INT_PTR visibleSlotIndex = static_cast<INT_PTR>(lpdis->itemID) - topIndex;

        FillRect(lpdis->hDC, &lpdis->rcItem, isSelected ? m_hbrListBoxSelectedBackground : m_hbrListBoxBackground);
        SetTextColor(lpdis->hDC, isSelected ? m_clrListBoxSelectedText : m_clrListBoxText);
        SetBkMode(lpdis->hDC, TRANSPARENT);

        std::wstring shortcutHintText;
        if (visibleSlotIndex >= 0 && visibleSlotIndex < MAX_VISIBLE_LIST_ITEMS) {
            shortcutHintText = L" #" + std::to_wstring((static_cast<int>(visibleSlotIndex) + 1) % 10);
        }

        HFONT hOldFont = (HFONT)SelectObject(lpdis->hDC, m_hFont);
        if (!shortcutHintText.empty()) {
            RECT rcShortcut = lpdis->rcItem; rcShortcut.right -= G_PIXEL_GAP;
            DrawText(lpdis->hDC, shortcutHintText.c_str(), -1, &rcShortcut, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        RECT rcDisplayName = lpdis->rcItem;
        rcDisplayName.left += G_PIXEL_GAP;
        rcDisplayName.right -= (G_PIXEL_GAP * 2 + 30);
        DrawText(lpdis->hDC, currentItem.displayName.c_str(), -1, &rcDisplayName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
        SelectObject(lpdis->hDC, hOldFont);
        return TRUE;
    }
    case WM_CTLCOLOREDIT: {
        HDC hE = (HDC)wParam;
        SetTextColor(hE, m_clrEditText);
        SetBkColor(hE, m_clrEditBg);
        return (LRESULT)m_hbrEditBackground;
    }
    case WM_CTLCOLORLISTBOX: return (LRESULT)m_hbrListBoxBackground;
    case WM_COMMAND: {
        if (LOWORD(wParam) == IDM_TRAY_EXIT) {
            DestroyWindow(m_hWnd);
            return 0;
        }
        if (LOWORD(wParam) == IDM_TRAY_STARTUP) {
            bool isEnabled = Utils::IsRunOnStartupEnabled(m_app->GetAppNameReg());
            Utils::SetRunOnStartup(m_app->GetAppNameReg(), !isEnabled);
            return 0;
        }
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == m_hEdit) {
            KillTimer(m_hWnd, IDT_SEARCH_DELAY);
            wchar_t cT[512];
            GetWindowText(m_hEdit, cT, 512);
            m_currentSearchQuery = cT;
            SetTimer(m_hWnd, IDT_SEARCH_DELAY, SEARCH_DELAY_MS, NULL);
        }
        else if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == m_hListBox) {
            LaunchItemByIndex(SendMessage(m_hListBox, LB_GETCURSEL, 0, 0));
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == IDT_SEARCH_DELAY) {
            KillTimer(m_hWnd, IDT_SEARCH_DELAY);
            m_app->GetSearchService().StartSearch(m_currentSearchQuery, m_hWnd);
        }
        break;
    }
    case WM_APP_SEARCH_COMPLETE: {
        m_app->GetSearchService().SetResults((std::vector<SearchResult>*)wParam, (LONG)lParam);
        UpdateListBoxVisibilityAndContent();
        if (m_hListBox && SendMessage(m_hListBox, LB_GETCOUNT, 0, 0) > 0) {
            SendMessage(m_hListBox, LB_SETCURSEL, 0, 0);
        }
        break;
    }
    case WM_APP_TRAY_MSG: {
        switch (lParam) {
        case WM_RBUTTONUP: {
            HMENU hMenu = CreatePopupMenu();
            UINT startupFlags = MF_BYPOSITION | MF_STRING | (Utils::IsRunOnStartupEnabled(m_app->GetAppNameReg()) ? MF_CHECKED : MF_UNCHECKED);
            InsertMenu(hMenu, 0, startupFlags, IDM_TRAY_STARTUP, L"开机时启动 (Run on startup)");
            InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
            InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"退出 (Exit)");
            SetForegroundWindow(m_hWnd);
            POINT pt; GetCursorPos(&pt);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, m_hWnd, NULL);
            DestroyMenu(hMenu);
        }
                         break;
        case WM_LBUTTONUP: ToggleVisibility(); break;
        }
        return 0;
    }
    case WM_ACTIVATE: {
        if (wParam == WA_INACTIVE && IsWindowVisible(m_hWnd)) {
            ShowWindow(m_hWnd, SW_HIDE);
        }
        else {
            if (m_hEdit) SetFocus(m_hEdit);
        }
        break;
    }
    case WM_DESTROY: {
        RemoveTrayIcon();
        KillTimer(m_hWnd, IDT_SEARCH_DELAY);
        UnregisterHotKey(m_hWnd, m_app->GetHotkeyId());
        PostQuitMessage(0);
        break;
    }
    case WM_HOTKEY: {
        if (wParam == m_app->GetHotkeyId()) ToggleVisibility();
        break;
    }
    case WM_CLOSE: ShowWindow(m_hWnd, SW_HIDE); return 0;
    default: return DefWindowProc(m_hWnd, uMsg, wParam, lParam);
    }
    return 0;
}


LRESULT CALLBACK MainWindow::EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
    case WM_CHAR: if (wParam == VK_RETURN || wParam == VK_ESCAPE) return 0; break;
    case WM_KEYDOWN: {
        if (GetKeyState(VK_CONTROL) < 0) {
            INT_PTR VSN = -1;
            if (wParam >= '1' && wParam <= '9') VSN = static_cast<INT_PTR>(wParam - '1');
            else if (wParam == '0') VSN = 9;
            if (VSN != -1 && pThis->m_hListBox && IsWindowVisible(pThis->m_hListBox)) {
                LRESULT topIndex = SendMessage(pThis->m_hListBox, LB_GETTOPINDEX, 0, 0);
                pThis->LaunchItemByIndex(topIndex + VSN);
                return 0;
            }
        }
        switch (wParam) {
        case VK_RETURN:
            if (pThis->m_hListBox && IsWindowVisible(pThis->m_hListBox) && SendMessage(pThis->m_hListBox, LB_GETCOUNT, 0, 0) > 0) {
                LRESULT sel = SendMessage(pThis->m_hListBox, LB_GETCURSEL, 0, 0);
                pThis->LaunchItemByIndex((sel == LB_ERR) ? 0 : sel);
            } return 0;
        case VK_DOWN:
        case VK_UP:
            if (pThis->m_hListBox && IsWindowVisible(pThis->m_hListBox)) {
                LRESULT currentSel = SendMessage(pThis->m_hListBox, LB_GETCURSEL, 0, 0);
                INT_PTR count = SendMessage(pThis->m_hListBox, LB_GETCOUNT, 0, 0);
                if (count > 0) {
                    if (wParam == VK_DOWN) currentSel = (currentSel == LB_ERR || currentSel >= count - 1) ? 0 : currentSel + 1;
                    else currentSel = (currentSel == LB_ERR || currentSel <= 0) ? count - 1 : currentSel - 1;
                    SendMessage(pThis->m_hListBox, LB_SETCURSEL, currentSel, 0);
                    InvalidateRect(pThis->m_hListBox, NULL, TRUE);
                }
            }
            return 0;
        case VK_ESCAPE: ShowWindow(pThis->m_hWnd, SW_HIDE); return 0;
        }
        break;
    }
    case WM_MOUSEWHEEL: {
        if (!pThis->m_hListBox || !IsWindowVisible(pThis->m_hListBox) || pThis->m_listBoxItemHeight <= 0) break;
        PostMessage(pThis->m_hListBox, uMsg, wParam, lParam);
        return 0;
    }
    case WM_NCDESTROY: {
        if (pThis->m_pfnOldEditProc) {
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pThis->m_pfnOldEditProc);
        }
        break;
    }
    }
    return CallWindowProc(pThis->m_pfnOldEditProc, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    MainWindow* pThis = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    if (!pThis) return DefWindowProc(hWnd, uMsg, wParam, lParam);

    switch (uMsg) {
    case WM_MOUSEWHEEL: {
        if (pThis->m_listBoxItemHeight <= 0) break;
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        INT_PTR currentTopIndex = SendMessage(hWnd, LB_GETTOPINDEX, 0, 0);
        INT_PTR totalItems = SendMessage(hWnd, LB_GETCOUNT, 0, 0);
        RECT rcC; GetClientRect(hWnd, &rcC);
        int itemsInView = (rcC.bottom > 0 && pThis->m_listBoxItemHeight > 0) ? (rcC.bottom / pThis->m_listBoxItemHeight) : 1;
        if (totalItems <= itemsInView) return 0;

        INT_PTR newTopIndex = currentTopIndex + (delta > 0 ? -3 : 3);
        INT_PTR maxTopIndex = totalItems - static_cast<INT_PTR>(itemsInView);
        if (maxTopIndex < 0) maxTopIndex = 0;
        newTopIndex = max((INT_PTR)0, min(newTopIndex, maxTopIndex));

        if (newTopIndex != currentTopIndex) {
            SendMessage(hWnd, LB_SETTOPINDEX, newTopIndex, 0);
            InvalidateRect(hWnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_NCDESTROY: {
        if (pThis->m_pfnOldListBoxProc) {
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)pThis->m_pfnOldListBoxProc);
        }
        break;
    }
    }
    return CallWindowProc(pThis->m_pfnOldListBoxProc, hWnd, uMsg, wParam, lParam);
}


void MainWindow::ToggleVisibility() {
    if (IsWindowVisible(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    }
    else {
        RECT mainRc; GetWindowRect(m_hWnd, &mainRc);
        SetWindowPos(m_hWnd, 0, 0, 0, mainRc.right - mainRc.left, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP), SWP_NOMOVE | SWP_NOZORDER);
        Center();
        ShowWindow(m_hWnd, SW_SHOW);
        SetForegroundWindow(m_hWnd);
        if (m_hEdit) {
            SetFocus(m_hEdit);
            SetWindowText(m_hEdit, L"");
        }
    }
}

void MainWindow::Center() {
    RECT rd; SystemParametersInfo(SPI_GETWORKAREA, 0, &rd, 0);
    RECT rw; GetWindowRect(m_hWnd, &rw);
    int w = rw.right - rw.left;
    int h = rw.bottom - rw.top;
    int x = (rd.right - w) / 2;
    int y = (rd.bottom - h) / 3;
    SetWindowPos(m_hWnd, 0, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void MainWindow::CreateAndSetFont() {
    CleanupFont();
    LOGFONT L = { 0 };
    L.lfHeight = -(G_EDIT_HEIGHT / 2 + G_EDIT_HEIGHT / 8);
    wcscpy_s(L.lfFaceName, LF_FACESIZE, L"Segoe UI");
    m_hFont = CreateFontIndirect(&L);
    if (m_hFont) {
        if (m_hEdit) SendMessage(m_hEdit, WM_SETFONT, (WPARAM)m_hFont, TRUE);
        if (m_hListBox) SendMessage(m_hListBox, WM_SETFONT, (WPARAM)m_hFont, TRUE);
    }
}

void MainWindow::CleanupFont() {
    if (m_hFont) {
        DeleteObject(m_hFont);
        m_hFont = NULL;
    }
}

void MainWindow::UpdateListBoxVisibilityAndContent() {
    if (!m_hEdit || !m_hWnd || !m_hListBox) return;

    SendMessage(m_hListBox, LB_RESETCONTENT, 0, 0);
    const auto& results = m_app->GetSearchService().GetResults();
    for (const auto& res : results) {
        SendMessage(m_hListBox, LB_ADDSTRING, 0, (LPARAM)res.displayName.c_str());
    }
    size_t numRes = results.size();

    bool hasRes = (numRes > 0);
    int txtLen = GetWindowTextLength(m_hEdit);
    RECT mainRc; GetWindowRect(m_hWnd, &mainRc);
    int curW = mainRc.right - mainRc.left;

    if ((txtLen > 0 || !m_currentSearchQuery.empty()) && hasRes) {
        ShowWindow(m_hListBox, SW_SHOW);
        int itemsToShow = min(static_cast<int>(numRes), MAX_VISIBLE_LIST_ITEMS);
        int listH = itemsToShow * m_listBoxItemHeight;
        SetWindowPos(m_hWnd, 0, 0, 0, curW, G_EDIT_HEIGHT + listH + (G_PIXEL_GAP * 2), SWP_NOMOVE | SWP_NOZORDER);
        SetWindowPos(m_hListBox, 0, G_PIXEL_GAP, G_EDIT_HEIGHT + G_PIXEL_GAP, curW - (2 * G_PIXEL_GAP), listH, SWP_NOZORDER | SWP_NOMOVE);
    }
    else {
        ShowWindow(m_hListBox, SW_HIDE);
        SetWindowPos(m_hWnd, 0, 0, 0, curW, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP), SWP_NOMOVE | SWP_NOZORDER);
    }
    InvalidateRect(m_hListBox, NULL, TRUE);
    SendMessage(m_hListBox, LB_SETTOPINDEX, 0, 0);
}

void MainWindow::AddTrayIcon() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY_MSG;
    nid.hIcon = (HICON)LoadImage(m_app->GetInstance(), MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wcscpy_s(nid.szTip, L"QuicklyLaunch (Alt+Space)");
    Shell_NotifyIconW(NIM_ADD, &nid);
}

void MainWindow::RemoveTrayIcon() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hWnd;
    nid.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void MainWindow::LaunchItemByIndex(INT_PTR index) {
    if (index < 0) return;
    const auto& results = m_app->GetSearchService().GetResults();
    if (static_cast<size_t>(index) < results.size()) {
        CommandExecutor::Execute(results[index], m_hWnd, m_app->GetSearchService());
    }
}