#pragma once

#include <Windows.h>
#include <CommCtrl.h>
#include <shlobj.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <set>
#include <shlwapi.h>
#include <sstream>
#include <iomanip>
#include <windowsx.h>
#include <powrprof.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "resource/resource.h"

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "UxTheme.lib")
#pragma comment(lib, "Powrprof.lib")
#pragma comment(lib, "Psapi.lib")

#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif

#ifndef IDM_TRAY_EXIT
#define IDM_TRAY_EXIT           104
#endif
#ifndef IDM_TRAY_STARTUP
#define IDM_TRAY_STARTUP        105
#endif
#ifndef WM_APP_SEARCH_COMPLETE
#define WM_APP_SEARCH_COMPLETE  (WM_APP + 1)
#endif
#ifndef WM_APP_TRAY_MSG
#define WM_APP_TRAY_MSG         (WM_APP + 2)
#endif

const std::wstring CMD_TERMINATE_PROCESS = L"cmd::terminate";
const std::wstring CMD_TERMINATE_ALL = L"cmd::terminate_all";
const std::wstring CMD_SHUTDOWN = L"cmd::shutdown";
const std::wstring CMD_RESTART = L"cmd::restart";
const std::wstring CMD_SLEEP = L"cmd::sleep";
const std::wstring CMD_EMPTY_TRASH = L"cmd::emptytrash";

enum class SourcePriority {
	PRIORITY_LOW = 0,
	PRIORITY_PROGRAM_FILES = 1,
	PRIORITY_DESKTOP = 2,
	PRIORITY_START_MENU = 3,
};

struct SearchResult {
	std::wstring displayName;
	std::wstring fullPath;
	int launchCount = 0;
	bool isApp = false;
	int score = 0;
	SourcePriority sourcePriority = SourcePriority::PRIORITY_LOW;
	HWND hwnd = NULL;
	std::wstring processName;
};