#include "Utils.h"

namespace Utils {

    std::wstring ToLower(std::wstring str) {
        std::transform(str.begin(), str.end(), str.begin(), ::towlower);
        return str;
    }

    std::wstring TrimWhitespace(const std::wstring& str) {
        const std::wstring W = L" \t\n\r\f\v";
        size_t F = str.find_first_not_of(W);
        if (F == std::wstring::npos) return L"";
        size_t L = str.find_last_not_of(W);
        return str.substr(F, (L - F + 1));
    }

    bool ResolveLnk(const wchar_t* P, std::wstring& T, std::wstring& A) {
        T.clear();
        A.clear();
        IShellLink* S = 0;
        HRESULT H = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&S);

        if (FAILED(H)) return 0;
        IPersistFile* F = 0;
        H = S->QueryInterface(IID_IPersistFile, (LPVOID*)&F);

        if (FAILED(H)) {
            S->Release();
            return 0;
        }

        H = F->Load(P, STGM_READ);

        if (FAILED(H)) {
            F->Release();
            S->Release();
            return 0;
        }

        wchar_t TP[MAX_PATH];
        H = S->GetPath(TP, MAX_PATH, 0, SLGP_UNCPRIORITY);

        if (SUCCEEDED(H)) T = TP;
        else T.clear();
        wchar_t AG[MAX_PATH];
        H = S->GetArguments(AG, MAX_PATH);
        if (SUCCEEDED(H)) A = AG;
        F->Release();
        S->Release();
        return !T.empty();
    }

    bool IsRunOnStartupEnabled(const wchar_t* appName) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
            bool enabled = (RegQueryValueExW(hKey, appName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS);
            RegCloseKey(hKey);
            return enabled;
        }
        return false;
    }

    void SetRunOnStartup(const wchar_t* appName, bool bEnable) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            if (bEnable) {
                wchar_t szPath[MAX_PATH];
                if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
                    RegSetValueExW(hKey, appName, 0, REG_SZ, (BYTE*)szPath, static_cast<DWORD>((wcslen(szPath) + 1) * sizeof(wchar_t)));
                }
            }
            else {
                RegDeleteValueW(hKey, appName);
            }
            RegCloseKey(hKey);
        }
    }

    void EnableShutdownPrivileges() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tkp;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid);
            tkp.PrivilegeCount = 1;
            tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
            CloseHandle(hToken);
        }
    }

    bool IsSafeToTerminate(const std::wstring& processName) {
        static const std::set<std::wstring> criticalProcesses = {
            L"explorer.exe", L"csrss.exe", L"wininit.exe", L"winlogon.exe",
            L"lsass.exe", L"services.exe", L"svchost.exe", L"smss.exe",
            L"conhost.exe", L"dwm.exe", L"System", L"Idle"
        };
        return criticalProcesses.find(ToLower(processName)) == criticalProcesses.end();
    }

    std::set<std::wstring> GetRunningProcessNames() {
        std::set<std::wstring> runningProcesses;
        HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnapShot == INVALID_HANDLE_VALUE) {
            return runningProcesses;
        }

        PROCESSENTRY32W pEntry;
        pEntry.dwSize = sizeof(pEntry);
        if (Process32FirstW(hSnapShot, &pEntry)) {
            do {
                runningProcesses.insert(ToLower(pEntry.szExeFile));
            } while (Process32NextW(hSnapShot, &pEntry));
        }
        CloseHandle(hSnapShot);
        return runningProcesses;
    }

}