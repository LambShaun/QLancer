#pragma once
#include "common.h"

class Application; // 前向声明

class MainWindow {
public:
    MainWindow(Application* app);
    ~MainWindow();

    bool Create(HINSTANCE hInstance, const wchar_t* className, const wchar_t* windowTitle);
    HWND GetHwnd() const { return m_hWnd; }

    void ToggleVisibility();

private:
    // 静态成员用于窗口过程
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    // 实例成员用于处理消息
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // UI辅助函数
    void Center();
    void CreateAndSetFont();
    void CleanupFont();
    void UpdateListBoxVisibilityAndContent();
    void AddTrayIcon();
    void RemoveTrayIcon();
    void LaunchItemByIndex(INT_PTR index);

    Application* m_app; 
    HWND m_hWnd = NULL;
    HWND m_hEdit = NULL;
    HWND m_hListBox = NULL;
    HFONT m_hFont = NULL;
    HBRUSH m_hbrClassBackground = NULL;
    WNDPROC m_pfnOldEditProc = NULL;
    WNDPROC m_pfnOldListBoxProc = NULL;

    HBRUSH m_hbrEditBackground = NULL;
    HBRUSH m_hbrListBoxBackground = NULL;
    HBRUSH m_hbrListBoxSelectedBackground = NULL;
    COLORREF m_clrEditText = RGB(220, 220, 220);
    COLORREF m_clrListBoxText = RGB(200, 200, 200);
    COLORREF m_clrListBoxSelectedText = RGB(255, 255, 255);
    COLORREF m_clrEditBg = RGB(45, 45, 48);
    COLORREF m_clrListBoxBg = RGB(30, 30, 30);
    COLORREF m_clrListBoxSelectedBg = RGB(0, 120, 215);

    static const int MAX_VISIBLE_LIST_ITEMS = 10;
    static const int G_EDIT_HEIGHT = 40;
    static const int G_PIXEL_GAP = 8;
    int m_listBoxItemHeight = G_EDIT_HEIGHT / 2;

    UINT_PTR IDT_SEARCH_DELAY = 1;
    const int SEARCH_DELAY_MS = 180;
    std::wstring m_currentSearchQuery;
};