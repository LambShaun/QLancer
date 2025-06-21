#pragma once
#include "common.h"

namespace Utils {
    std::wstring ToLower(std::wstring str);
    std::wstring TrimWhitespace(const std::wstring& str);
    bool ResolveLnk(const wchar_t* lnkPath, std::wstring& targetPath, std::wstring& arguments);
    bool IsRunOnStartupEnabled(const wchar_t* appName);
    void SetRunOnStartup(const wchar_t* appName, bool bEnable);
    void EnableShutdownPrivileges();
    bool IsSafeToTerminate(const std::wstring& processName);
    std::set<std::wstring> GetRunningProcessNames();
}