#pragma once
#include "common.h"
#include "MainWindow.h"
#include "SearchService.h"

class Application {
public:
    Application(HINSTANCE hInstance);
    ~Application();

    int Run();
    HINSTANCE GetInstance() const { return m_hInstance; }
    SearchService& GetSearchService() { return m_searchService; }
    int GetHotkeyId() const { return m_hotkeyId; }
    const wchar_t* GetAppNameReg() const { return m_appNameReg; }

private:
    bool Init();
    void Cleanup();

    HINSTANCE m_hInstance = NULL;
    HANDLE m_hMutex = NULL;
    std::unique_ptr<MainWindow> m_mainWindow;
    SearchService m_searchService;

    const int m_hotkeyId = 1;
    const wchar_t* m_className = L"QuicklyLaunchUltimate";
    const wchar_t* m_windowTitle = L"QuicklyLaunch";
    const wchar_t* m_appNameReg = L"QuicklyLaunch";
};