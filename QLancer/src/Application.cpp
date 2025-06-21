#include "Application.h"
#include "Utils.h"

Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

Application::~Application() {
    Cleanup();
}

bool Application::Init() {
    m_hMutex = CreateMutex(NULL, TRUE, L"QuicklyLaunchUltimate_Mutex");
    if (m_hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existingWnd = FindWindowW(m_className, NULL);
        if (existingWnd) {
            ShowWindow(existingWnd, SW_NORMAL);
            SetForegroundWindow(existingWnd);
        }
        if (m_hMutex) CloseHandle(m_hMutex);
        m_hMutex = NULL; 
        return false;
    }

    if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        return false;
    }

    Utils::EnableShutdownPrivileges();

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    if (!InitCommonControlsEx(&icex)) {
        return false;
    }

    m_mainWindow = std::make_unique<MainWindow>(this);
    if (!m_mainWindow->Create(m_hInstance, m_className, m_windowTitle)) {
        return false;
    }

    return true;
}

int Application::Run() {
    if (!Init()) {
        return 0; 
    }

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

void Application::Cleanup() {
    CoUninitialize();
    if (m_hMutex) {
        ReleaseMutex(m_hMutex);
        CloseHandle(m_hMutex);
    }
}