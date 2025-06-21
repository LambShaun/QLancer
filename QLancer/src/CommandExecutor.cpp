#include "CommandExecutor.h"
#include "SearchService.h"
#include "Utils.h"

void CommandExecutor::Execute(const SearchResult& result, HWND ownerHwnd, SearchService& searchService) {
    if (result.fullPath != CMD_TERMINATE_PROCESS && result.fullPath != CMD_TERMINATE_ALL) {
        searchService.IncrementLaunchFrequency(result.fullPath);
    }

#pragma warning(suppress: 28159)
    if (result.fullPath == CMD_SHUTDOWN) {
        InitiateSystemShutdownExW(NULL, NULL, 0, TRUE, FALSE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
    }
#pragma warning(suppress: 28159)
    else if (result.fullPath == CMD_RESTART) {
        InitiateSystemShutdownExW(NULL, NULL, 0, TRUE, TRUE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
    }
    else if (result.fullPath == CMD_SLEEP) {
        SetSuspendState(FALSE, TRUE, FALSE);
        ShowWindow(ownerHwnd, SW_HIDE);
    }
    else if (result.fullPath == CMD_EMPTY_TRASH) {
        SHEmptyRecycleBinW(ownerHwnd, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        ShowWindow(ownerHwnd, SW_HIDE);
    }
    else if (result.fullPath == CMD_TERMINATE_PROCESS) {
        if (!result.processName.empty()) {
            TerminateProcessByName(result.processName);
        }
        ShowWindow(ownerHwnd, SW_HIDE);
    }
    else if (result.fullPath == CMD_TERMINATE_ALL) {
        std::set<std::wstring> runningProcesses = Utils::GetRunningProcessNames();
        const auto& appCache = searchService.GetAppCache();

        for (const auto& app : appCache) {
            if (app.sourcePriority >= SourcePriority::PRIORITY_DESKTOP) {
                std::wstring processName = Utils::ToLower(PathFindFileNameW(app.fullPath.c_str()));
                if (runningProcesses.count(processName)) {
                    TerminateProcessByName(processName);
                }
            }
        }
        ShowWindow(ownerHwnd, SW_HIDE);
    }
    else if (!result.fullPath.empty()) {
        HINSTANCE execResult = ShellExecuteW(ownerHwnd, L"open", result.fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(execResult) > 32) {
            ShowWindow(ownerHwnd, SW_HIDE);
        }
    }
}


void CommandExecutor::TerminateProcessByName(const std::wstring& processName) {
    if (processName.empty() || !Utils::IsSafeToTerminate(processName)) {
        return;
    }

    HANDLE hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapShot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pEntry;
    pEntry.dwSize = sizeof(pEntry);
    if (Process32FirstW(hSnapShot, &pEntry)) {
        do {
            if (_wcsicmp(pEntry.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pEntry.th32ProcessID);
                if (hProcess != NULL) {
                    TerminateProcess(hProcess, 1);
                    CloseHandle(hProcess);
                }
            }
        } while (Process32NextW(hSnapShot, &pEntry));
    }
    CloseHandle(hSnapShot);
}