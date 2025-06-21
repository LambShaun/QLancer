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

// 预定义ID
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

// 命令常量
const std::wstring CMD_TERMINATE_PROCESS = L"cmd::terminate";
const std::wstring CMD_TERMINATE_ALL = L"cmd::terminate_all"; // Quit All Apps用
const std::wstring CMD_SHUTDOWN = L"cmd::shutdown";
const std::wstring CMD_RESTART = L"cmd::restart";
const std::wstring CMD_SLEEP = L"cmd::sleep";
const std::wstring CMD_EMPTY_TRASH = L"cmd::emptytrash";

enum SourcePriority {
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
	SourcePriority sourcePriority = PRIORITY_LOW;
	HWND hwnd = NULL;
	std::wstring processName;
};

struct SearchThreadParams {
	std::wstring query;
	LONG generation = 0;
};

const std::vector<SearchResult> g_systemCommands = {
	{L"Shutdown", CMD_SHUTDOWN, 0, true, 0, PRIORITY_LOW, NULL, L""},
	{L"Restart", CMD_RESTART, 0, true, 0, PRIORITY_LOW, NULL, L""},
	{L"Sleep", CMD_SLEEP, 0, true, 0, PRIORITY_LOW, NULL, L""},
	{L"Empty Recycle Bin", CMD_EMPTY_TRASH, 0, true, 0, PRIORITY_LOW, NULL, L""}
};

// 全局变量
HINSTANCE g_hInstance = NULL;
HWND g_hMainWnd = NULL;
int g_hotkeyId = 1;
HWND g_hEdit = NULL;
HWND g_hListBox = NULL;
HFONT g_hFont = NULL;
WNDPROC g_pfnOldEditProc = NULL;
WNDPROC g_pfnOldListBoxProc = NULL;
HANDLE g_hMutex = NULL;
std::vector<SearchResult> g_searchResults;
std::vector<SearchResult> g_applicationCache;
std::map<std::wstring, int> g_launchFrequency;
bool g_appCachePopulated = false;
HANDLE g_hSearchThread = NULL;
HANDLE g_hPreloadCacheThread = NULL;
CRITICAL_SECTION g_csSearchData;
UINT_PTR IDT_SEARCH_DELAY = 1;
const int SEARCH_DELAY_MS = 180;
std::wstring g_currentSearchQuery;
volatile LONG g_searchGeneration = 0;
HBRUSH g_hbrEditBackground = NULL;
HBRUSH g_hbrListBoxBackground = NULL;
HBRUSH g_hbrListBoxSelectedBackground = NULL;
COLORREF g_clrEditText = RGB(220, 220, 220);
COLORREF g_clrListBoxText = RGB(200, 200, 200);
COLORREF g_clrListBoxSelectedText = RGB(255, 255, 255);
COLORREF g_clrEditBg = RGB(45, 45, 48);
COLORREF g_clrListBoxBg = RGB(30, 30, 30);
COLORREF g_clrListBoxSelectedBg = RGB(0, 120, 215);
const int MAX_VISIBLE_LIST_ITEMS = 10;
const int G_EDIT_HEIGHT = 40;
const int G_PIXEL_GAP = 8;
int g_listBoxItemHeight = G_EDIT_HEIGHT / 2;
const wchar_t CLASS_NAME[] = L"QLancerUltimate";
const wchar_t WINDOW_TITLE[] = L"QLancer";
const wchar_t* g_appNameReg = L"QLancer";

// 函数前置声明
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CenterWindow(HWND hwnd);
void ToggleWindowVisibility();
void CreateAndSetFont(HWND hWnd);
void CleanupFont();
void UpdateListBoxVisibilityAndContent();
DWORD WINAPI SearchThreadProc(LPVOID lpParam);
std::wstring ToLower(std::wstring str);
std::wstring TrimWhitespace(const std::wstring& str);
bool ResolveLnk(const wchar_t* lnkPath, std::wstring& targetPath, std::wstring& arguments);
void AddItemToResults(std::vector<SearchResult>& results,
	const std::wstring& displayName,
	const std::wstring& fullPath,
	bool isApp, SourcePriority priority);
void RecursiveScanDirectory(const std::wstring& directory,
	bool isAppSearch,
	std::vector<SearchResult>& localResults,
	SourcePriority priority,
	int maxDepth, int currentDepth = 0);
void PopulateAppCacheIfNeeded();
void LaunchItemByIndex(HWND hWnd, INT_PTR index);
int ScoreFuzzyMatch(const std::wstring& query, const std::wstring& target);
void AddTrayIcon(HWND hWnd);
void RemoveTrayIcon(HWND hWnd);
void SetRunOnStartup(bool bEnable);
bool IsRunOnStartupEnabled();

bool IsSafeToTerminate(const std::wstring& processName) {
	static const std::set<std::wstring> criticalProcesses = {
		L"explorer.exe",
		L"csrss.exe",
		L"wininit.exe",
		L"winlogon.exe",
		L"lsass.exe",
		L"services.exe",
		L"svchost.exe",
		L"smss.exe",
		L"conhost.exe",
		L"dwm.exe",
		L"System",
		L"Idle"
	};
	return criticalProcesses.find(ToLower(processName)) == criticalProcesses.end();
}


void TerminateProcessByName(const std::wstring& processName) {
	if (processName.empty()) return;

	if (!IsSafeToTerminate(processName)) {
		// 为了安全，不终止系统关键进程
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

// 获取当前所有运行进程名的辅助函数
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


bool IsRunOnStartupEnabled() {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
		bool enabled = (RegQueryValueExW(hKey, g_appNameReg, NULL, NULL, NULL, NULL) == ERROR_SUCCESS);
		RegCloseKey(hKey);
		return enabled;
	}
	return false;
}

void SetRunOnStartup(bool bEnable) {
	HKEY hKey;
	if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
		if (bEnable) {
			wchar_t szPath[MAX_PATH];
			if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
				RegSetValueExW(hKey, g_appNameReg, 0, REG_SZ, (BYTE*)szPath, static_cast<DWORD>((wcslen(szPath) + 1) * sizeof(wchar_t)));
			}
		}
		else {
			RegDeleteValueW(hKey, g_appNameReg);
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

DWORD WINAPI PreloadCacheThreadProc(LPVOID lpParam) {
	if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
		PopulateAppCacheIfNeeded();
		CoUninitialize();
	}
	return 0;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
	g_hMutex = CreateMutex(NULL, TRUE, L"QLancerUltimate_Mutex");
	if (g_hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND existingWnd = FindWindowW(CLASS_NAME, NULL);

		if (existingWnd) {
			ShowWindow(existingWnd, SW_NORMAL);
			SetForegroundWindow(existingWnd);
		}

		if (g_hMutex) CloseHandle(g_hMutex);
		return 0;
	}

	g_hInstance = hInstance;
	g_hbrEditBackground = CreateSolidBrush(g_clrEditBg);
	g_hbrListBoxBackground = CreateSolidBrush(g_clrListBoxBg);
	g_hbrListBoxSelectedBackground = CreateSolidBrush(g_clrListBoxSelectedBg);

	if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
		return 1;
	}

	EnableShutdownPrivileges();
	InitializeCriticalSection(&g_csSearchData);

	INITCOMMONCONTROLSEX icex = {
		sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES
	};

	if (!InitCommonControlsEx(&icex)) {
		return 1;
	}

	WNDCLASSEX WindowClass = {};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.lpfnWndProc = WndProc;
	WindowClass.hInstance = hInstance;
	WindowClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPICON));
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WindowClass.hbrBackground = CreateSolidBrush(g_clrListBoxBg);
	WindowClass.lpszClassName = CLASS_NAME;

	if (!RegisterClassEx(&WindowClass)) {
		return 0;
	}

	g_hMainWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
		CLASS_NAME, WINDOW_TITLE, WS_POPUP | WS_SYSMENU,
		0, 0, 600, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP),
		NULL, NULL, hInstance, NULL
	);

	if (!g_hMainWnd) {
		return 0;
	}
	SetLayeredWindowAttributes(g_hMainWnd, 0, (255 * 95) / 100, LWA_ALPHA);
	RegisterHotKey(g_hMainWnd, g_hotkeyId, MOD_ALT | MOD_NOREPEAT, VK_SPACE);

	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	CleanupFont();
	if (g_hSearchThread) {
		WaitForSingleObject(g_hSearchThread, INFINITE);
		CloseHandle(g_hSearchThread);
	}

	if (g_hPreloadCacheThread) {
		WaitForSingleObject(g_hPreloadCacheThread, INFINITE);
		CloseHandle(g_hPreloadCacheThread);
	}

	DeleteCriticalSection(&g_csSearchData);
	CoUninitialize();

	if (g_hMutex) {
		ReleaseMutex(g_hMutex);
		CloseHandle(g_hMutex);
	}

	if (g_hbrEditBackground) DeleteObject(g_hbrEditBackground);
	if (g_hbrListBoxBackground) DeleteObject(g_hbrListBoxBackground);
	if (g_hbrListBoxSelectedBackground) DeleteObject(g_hbrListBoxSelectedBackground);
	if (WindowClass.hbrBackground) DeleteObject(WindowClass.hbrBackground);

	return (int)msg.wParam;
}

void AddTrayIcon(HWND hWnd) {
	NOTIFYICONDATAW nid = {};
	nid.cbSize = sizeof(NOTIFYICONDATAW);
	nid.hWnd = hWnd;
	nid.uID = 1;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_APP_TRAY_MSG;
	nid.hIcon = (HICON)LoadImage(g_hInstance,
		MAKEINTRESOURCE(IDI_APPICON), IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON), 0);
	wcscpy_s(nid.szTip, L"QLancer (Alt+Space)");
	Shell_NotifyIconW(NIM_ADD, &nid);
}

void RemoveTrayIcon(HWND hWnd) {
	NOTIFYICONDATAW nid = {};
	nid.cbSize = sizeof(NOTIFYICONDATAW);
	nid.hWnd = hWnd;
	nid.uID = 1;
	Shell_NotifyIconW(NIM_DELETE, &nid);
}

std::wstring ToLower(std::wstring str) {
	std::transform(str.begin(), str.end(), str.begin(), ::towlower);
	return str;
}

std::wstring TrimWhitespace(const std::wstring& str) {
	const std::wstring W = L" \t\n\r\f\v";
	size_t F = str.find_first_not_of(W);
	if (F == std::wstring::npos)return L"";
	size_t L = str.find_last_not_of(W);
	return str.substr(F, (L - F + 1));
}

bool ResolveLnk(const wchar_t* P, std::wstring& T, std::wstring& A) {
	T.clear();
	A.clear();
	IShellLink* S = 0;
	HRESULT H = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&S);

	if (FAILED(H))return 0;
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

	if (SUCCEEDED(H))T = TP;
	else T.clear();
	wchar_t AG[MAX_PATH];
	H = S->GetArguments(AG, MAX_PATH);
	if (SUCCEEDED(H))A = AG;
	F->Release();
	S->Release();
	return!T.empty();
}

void AddItemToResults(std::vector<SearchResult>& R, const std::wstring& D, const std::wstring& FP, bool AP, SourcePriority Prio) {
	for (const auto& I : R)if (I.fullPath == FP && I.displayName == D)return;
	R.push_back({ D, FP, g_launchFrequency[FP], AP, 0, Prio, NULL, L"" });
}

int ScoreFuzzyMatch(const std::wstring& query, const std::wstring& target) {
	if (query.empty()) return 1;
	if (target.empty()) return 0;
	int score = 0;
	int consecutive_bonus = 10;
	int acronym_bonus = 15;
	int penalty = -1;
	size_t target_idx = 0;
	size_t last_match_idx = (size_t)-1;

	for (const auto q_char : query) {
		bool found = false;
		for (size_t i = target_idx; i < target.length(); ++i) {
			if (target[i] == q_char) {
				found = true;
				score += 1;
				if (i == 0 || target[i - 1] == L' ' || (iswupper(target[i]) && !iswupper(target[i - 1]))) {
					score += acronym_bonus;
				}
				if (last_match_idx != (size_t)-1 && i == last_match_idx + 1) {
					score += consecutive_bonus;
				}
				else {
					if (last_match_idx != (size_t)-1) score += (penalty * static_cast<int>(i - last_match_idx - 1));
				}
				last_match_idx = i;
				target_idx = i + 1; break;
			}
		}
		if (!found) return 0;
	}
	return max(1, score);
}

void RecursiveScanDirectory(const std::wstring& Dir,
	bool IsApp, std::vector<SearchResult>& Res,
	SourcePriority Prio, int MaxDepth, int CurrentDepth) {
	if (CurrentDepth > MaxDepth) return;
	std::wstring SrchP = Dir + L"\\*";
	WIN32_FIND_DATAW FD;
	HANDLE HF = FindFirstFileW(SrchP.c_str(), &FD);

	if (HF == INVALID_HANDLE_VALUE) return;
	do {
		if (wcscmp(FD.cFileName, L".") == 0 || wcscmp(FD.cFileName, L"..") == 0) continue;
		std::wstring CIP = Dir + L"\\" + FD.cFileName;

		if (FD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			RecursiveScanDirectory(CIP, IsApp, Res, Prio, MaxDepth, CurrentDepth + 1);
		}
		else {
			std::wstring DFN = FD.cFileName;
			std::wstring CINL = ToLower(DFN);

			if (IsApp && CINL.length() >= 4) {
				std::wstring E = CINL.substr(CINL.length() - 4);

				if (E == L".lnk") {
					std::wstring Ldn = DFN.substr(0, DFN.length() - 4);
					std::wstring RT, Args;
					if (ResolveLnk(CIP.c_str(), RT, Args)) AddItemToResults(Res, Ldn, RT, true, Prio);
				}
				else if (E == L".exe") {
					std::wstring Edn = DFN.substr(0, DFN.length() - 4);
					AddItemToResults(Res, Edn, CIP, true, Prio);
				}
			}
		}
	} while (FindNextFileW(HF, &FD) != 0);
	FindClose(HF);
}

void PopulateAppCacheIfNeeded() {
	EnterCriticalSection(&g_csSearchData);
	if (g_appCachePopulated) {
		LeaveCriticalSection(&g_csSearchData);
		return;
	}
	LeaveCriticalSection(&g_csSearchData);

	wchar_t PTH[MAX_PATH];
	std::vector<SearchResult> TCache;
	if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_STARTMENU, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_START_MENU, 5, 0);
	if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_COMMON_STARTMENU, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_START_MENU, 5, 0);
	if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_DESKTOPDIRECTORY, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_DESKTOP, 1, 0);
	if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_COMMON_DESKTOPDIRECTORY, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_DESKTOP, 1, 0);
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_PROGRAM_FILES, 2, 0);
	if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, PRIORITY_PROGRAM_FILES, 2, 0);

	EnterCriticalSection(&g_csSearchData);
	if (!g_appCachePopulated) {
		g_applicationCache = TCache;
		g_appCachePopulated = true;
	}
	LeaveCriticalSection(&g_csSearchData);
}

DWORD WINAPI SearchThreadProc(LPVOID lpParam) {
	HRESULT comInitHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	SearchThreadParams* params = static_cast<SearchThreadParams*>(lpParam);
	std::wstring originalQuery = params->query;
	LONG generation = params->generation;
	delete params;

	std::vector<SearchResult>* currentThreadResults = new std::vector<SearchResult>();
	std::wstring lowerQuery = ToLower(TrimWhitespace(originalQuery));

	PopulateAppCacheIfNeeded();

	if (lowerQuery.rfind(L"quit", 0) == 0) {
		std::set<std::wstring> runningProcesses = GetRunningProcessNames();
		std::set<std::wstring> processedProcesses; // 用于去重

		std::wstring filterQuery;
		if (lowerQuery.length() > 4 && iswspace(lowerQuery[4])) {
			filterQuery = ToLower(TrimWhitespace(lowerQuery.substr(5)));
		}

		if (filterQuery.empty()) {
			currentThreadResults->push_back({ L"Quit All Apps", CMD_TERMINATE_ALL, 0, true, 10001, PRIORITY_LOW, NULL, L"" });
		}

		EnterCriticalSection(&g_csSearchData);
		std::vector<SearchResult> localAppCacheCopy = g_applicationCache;
		LeaveCriticalSection(&g_csSearchData);

		for (auto& app : localAppCacheCopy) {
			if (app.sourcePriority < PRIORITY_DESKTOP) {
				continue;
			}

			std::wstring processName = ToLower(PathFindFileNameW(app.fullPath.c_str()));
			if (processName.empty()) continue;

			if (processedProcesses.count(processName)) {
				continue;
			}

			if (runningProcesses.count(processName)) {
				std::wstring loweredDisplayName = ToLower(app.displayName);
				if (loweredDisplayName.find(L"windows software development kit") != std::wstring::npos) {
					continue;
				}

				int score = filterQuery.empty() ? 1000 : ScoreFuzzyMatch(filterQuery, loweredDisplayName);
				if (score > 0) {
					SearchResult quitResult = app;
					quitResult.score = score;
					quitResult.displayName = L"Quit: " + app.displayName;
					quitResult.fullPath = CMD_TERMINATE_PROCESS;
					quitResult.processName = PathFindFileNameW(app.fullPath.c_str());
					currentThreadResults->push_back(quitResult);
					processedProcesses.insert(processName);
				}
			}
		}
	}
	else if (!lowerQuery.empty()) {
		EnterCriticalSection(&g_csSearchData);
		std::vector<SearchResult> localAppCacheCopy = g_applicationCache;
		LeaveCriticalSection(&g_csSearchData);

		for (auto app : localAppCacheCopy) {
			int score = ScoreFuzzyMatch(lowerQuery, ToLower(app.displayName));
			if (score > 0) {
				app.score = score;
				app.launchCount = g_launchFrequency[app.fullPath];
				currentThreadResults->push_back(app);
			}
		}
		for (auto cmd : g_systemCommands) {
			int score = ScoreFuzzyMatch(lowerQuery, ToLower(cmd.displayName));
			if (score > 0) {
				cmd.score = score;
				cmd.launchCount = g_launchFrequency[cmd.fullPath];
				currentThreadResults->push_back(cmd);
			}
		}
	}

	std::sort(currentThreadResults->begin(), currentThreadResults->end(),
		[](const SearchResult& a, const SearchResult& b) {
			if (a.score != b.score) return a.score > b.score;
			if (a.launchCount != b.launchCount) return a.launchCount > b.launchCount;
			if (a.sourcePriority != b.sourcePriority) return a.sourcePriority > b.sourcePriority;
			return ToLower(a.displayName) < ToLower(b.displayName);
		});

	PostMessage(g_hMainWnd, WM_APP_SEARCH_COMPLETE, (WPARAM)currentThreadResults, (LPARAM)generation);
	if (SUCCEEDED(comInitHr)) CoUninitialize();
	return 0;
}

void CreateAndSetFont(HWND H) {
	CleanupFont();
	LOGFONT L = { 0 };
	L.lfHeight = -(G_EDIT_HEIGHT / 2 + G_EDIT_HEIGHT / 8);
	wcscpy_s(L.lfFaceName, LF_FACESIZE, L"Segoe UI");
	g_hFont = CreateFontIndirect(&L);
	if (g_hFont) {
		if (g_hEdit) SendMessage(g_hEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
		if (g_hListBox) SendMessage(g_hListBox, WM_SETFONT, (WPARAM)g_hFont, TRUE);
	}
}
void CleanupFont() { if (g_hFont) { DeleteObject(g_hFont); g_hFont = 0; } }
void CenterWindow(HWND H) { RECT RD; SystemParametersInfo(SPI_GETWORKAREA, 0, &RD, 0); RECT RW; GetWindowRect(H, &RW); int W = RW.right - RW.left; int HT = RW.bottom - RW.top; int X = (RD.right - W) / 2; int Y = (RD.bottom - HT) / 3; SetWindowPos(H, 0, X, Y, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }

void UpdateListBoxVisibilityAndContent() {
	if (!g_hEdit || !g_hMainWnd) return;
	if (g_hListBox) {
		SendMessage(g_hListBox, LB_RESETCONTENT, 0, 0);

		EnterCriticalSection(&g_csSearchData);
		for (const auto& res : g_searchResults) {
			SendMessage(g_hListBox, LB_ADDSTRING, 0, (LPARAM)res.displayName.c_str());
		}
		size_t numRes = g_searchResults.size();
		LeaveCriticalSection(&g_csSearchData);

		bool hasRes = (numRes > 0);
		int txtLen = GetWindowTextLength(g_hEdit);
		RECT mainRc;
		GetWindowRect(g_hMainWnd, &mainRc);
		int curW = mainRc.right - mainRc.left;

		if ((txtLen > 0 || !g_currentSearchQuery.empty()) && hasRes) {
			ShowWindow(g_hListBox, SW_SHOW);

			int itemsToShow = static_cast<int>(numRes);
			if (itemsToShow > MAX_VISIBLE_LIST_ITEMS) {
				itemsToShow = MAX_VISIBLE_LIST_ITEMS;
			}
			int listH = itemsToShow * g_listBoxItemHeight;

			SetWindowPos(g_hMainWnd, 0, 0, 0, curW, G_EDIT_HEIGHT + listH + (G_PIXEL_GAP * 2), SWP_NOMOVE | SWP_NOZORDER);
			SetWindowPos(g_hListBox, 0, G_PIXEL_GAP, G_EDIT_HEIGHT + G_PIXEL_GAP, curW - (2 * G_PIXEL_GAP), listH, SWP_NOZORDER | SWP_NOMOVE);
		}
		else {
			ShowWindow(g_hListBox, SW_HIDE);
			SetWindowPos(g_hMainWnd, 0, 0, 0, curW, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP), SWP_NOMOVE | SWP_NOZORDER);
		}
		InvalidateRect(g_hListBox, NULL, TRUE);
		SendMessage(g_hListBox, LB_SETTOPINDEX, 0, 0);
	}
}

void ToggleWindowVisibility() { if (IsWindowVisible(g_hMainWnd)) { ShowWindow(g_hMainWnd, SW_HIDE); } else { RECT mainRc; GetWindowRect(g_hMainWnd, &mainRc); SetWindowPos(g_hMainWnd, 0, 0, 0, mainRc.right - mainRc.left, G_EDIT_HEIGHT + (2 * G_PIXEL_GAP), SWP_NOMOVE | SWP_NOZORDER); CenterWindow(g_hMainWnd); ShowWindow(g_hMainWnd, SW_SHOW); SetForegroundWindow(g_hMainWnd); if (g_hEdit) { SetFocus(g_hEdit); SetWindowText(g_hEdit, L""); } } }

void LaunchItemByIndex(HWND hWnd, INT_PTR index) {
	if (index < 0) return;
	EnterCriticalSection(&g_csSearchData);
	bool validIndex = (static_cast<size_t>(index) < g_searchResults.size());
	SearchResult resultToLaunch;
	if (validIndex) {
		resultToLaunch = g_searchResults[static_cast<size_t>(index)];
		if (resultToLaunch.fullPath != CMD_TERMINATE_PROCESS && resultToLaunch.fullPath != CMD_TERMINATE_ALL) {
			g_launchFrequency[resultToLaunch.fullPath]++;
		}
	}
	LeaveCriticalSection(&g_csSearchData);

	if (validIndex) {
#pragma warning(suppress: 28159)
		if (resultToLaunch.fullPath == CMD_SHUTDOWN) {
			InitiateSystemShutdownExW(NULL, NULL, 0, TRUE, FALSE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
		}
#pragma warning(suppress: 28159)
		else if (resultToLaunch.fullPath == CMD_RESTART) {
			InitiateSystemShutdownExW(NULL, NULL, 0, TRUE, TRUE, SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED);
		}
		else if (resultToLaunch.fullPath == CMD_SLEEP) {
			SetSuspendState(FALSE, TRUE, FALSE);
			ToggleWindowVisibility();
		}
		else if (resultToLaunch.fullPath == CMD_EMPTY_TRASH) {
			SHEmptyRecycleBinW(hWnd, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
			ToggleWindowVisibility();
		}
		else if (resultToLaunch.fullPath == CMD_TERMINATE_PROCESS) {
			if (!resultToLaunch.processName.empty()) {
				TerminateProcessByName(resultToLaunch.processName);
			}
			ToggleWindowVisibility();
		}
		else if (resultToLaunch.fullPath == CMD_TERMINATE_ALL) {
			std::set<std::wstring> runningProcesses = GetRunningProcessNames();
			EnterCriticalSection(&g_csSearchData);
			std::vector<SearchResult> localAppCacheCopy = g_applicationCache;
			LeaveCriticalSection(&g_csSearchData);

			for (const auto& app : localAppCacheCopy) {
				if (app.sourcePriority >= PRIORITY_DESKTOP) {
					std::wstring processName = ToLower(PathFindFileNameW(app.fullPath.c_str()));
					if (runningProcesses.count(processName)) {
						TerminateProcessByName(processName);
					}
				}
			}
			ToggleWindowVisibility();
		}
		else if (!resultToLaunch.fullPath.empty()) {
			HINSTANCE execResult = ShellExecuteW(hWnd, L"open", resultToLaunch.fullPath.c_str(), NULL, NULL, SW_SHOWNORMAL);
			if (reinterpret_cast<INT_PTR>(execResult) > 32) {
				ToggleWindowVisibility();
			}
		}
	}
}

LRESULT CALLBACK EditSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
	case WM_CHAR:
		if (wParam == VK_RETURN || wParam == VK_ESCAPE)
			return 0;
		break;
	case WM_KEYDOWN: {
		if (GetKeyState(VK_CONTROL) < 0) {
			INT_PTR VSN = -1;
			if (wParam >= '1' && wParam <= '9') VSN = static_cast<INT_PTR>(wParam - '1');
			else if (wParam == '0') VSN = 9;
			if (VSN != -1 && g_hListBox && IsWindowVisible(g_hListBox)) {
				LRESULT topIndex = SendMessage(g_hListBox, LB_GETTOPINDEX, 0, 0);
				LaunchItemByIndex(g_hMainWnd, topIndex + VSN);
				return 0;
			}
		}
		switch (wParam) {
		case VK_RETURN:
			if (g_hListBox && IsWindowVisible(g_hListBox) && SendMessage(g_hListBox, LB_GETCOUNT, 0, 0) > 0) {
				LRESULT sel = SendMessage(g_hListBox, LB_GETCURSEL, 0, 0);
				LaunchItemByIndex(g_hMainWnd, (sel == LB_ERR) ? 0 : sel);
			} return 0;
		case VK_DOWN:
		case VK_UP:
			if (g_hListBox && IsWindowVisible(g_hListBox)) {
				LRESULT currentSel = SendMessage(g_hListBox, LB_GETCURSEL, 0, 0);
				INT_PTR count = SendMessage(g_hListBox, LB_GETCOUNT, 0, 0);
				if (count > 0) {
					if (wParam == VK_DOWN) {
						currentSel = (currentSel == LB_ERR || currentSel >= count - 1) ? 0 : currentSel + 1;
					}
					else {
						currentSel = (currentSel == LB_ERR || currentSel <= 0) ? count - 1 : currentSel - 1;
					}
					SendMessage(g_hListBox, LB_SETCURSEL, currentSel, 0);
					InvalidateRect(g_hListBox, NULL, TRUE);
				}
			}
			return 0;
		case VK_ESCAPE: ShowWindow(g_hMainWnd, SW_HIDE); return 0;
		}
		break;
	}
	case WM_MOUSEWHEEL: {
		if (!g_hListBox || !IsWindowVisible(g_hListBox) || g_listBoxItemHeight <= 0) break;
		POINT mpS = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		RECT lbR;
		GetWindowRect(g_hListBox, &lbR);
		if (PtInRect(&lbR, mpS)) {
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			INT_PTR currentTopIndex = SendMessage(g_hListBox, LB_GETTOPINDEX, 0, 0);
			INT_PTR totalItems = SendMessage(g_hListBox, LB_GETCOUNT, 0, 0);
			RECT rcLBC;
			GetClientRect(g_hListBox, &rcLBC);
			int itemsInView = (rcLBC.bottom > 0 && g_listBoxItemHeight > 0) ? (rcLBC.bottom / g_listBoxItemHeight) : 0;
			if (totalItems <= itemsInView) return 0;

			INT_PTR newTopIndex = currentTopIndex + (delta > 0 ? -3 : 3);
			newTopIndex = max((INT_PTR)0, min(newTopIndex, totalItems - static_cast<INT_PTR>(itemsInView)));

			if (newTopIndex != currentTopIndex) {
				SendMessage(g_hListBox, LB_SETTOPINDEX, newTopIndex, 0);
				InvalidateRect(g_hListBox, NULL, FALSE);
			}
			return 0;
		}
		break;
	}
	case WM_NCDESTROY: {
		if (g_pfnOldEditProc) {
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)g_pfnOldEditProc);
			WNDPROC tempProc = g_pfnOldEditProc;
			g_pfnOldEditProc = NULL;
			return CallWindowProc(tempProc, hWnd, uMsg, wParam, lParam);
		}
		break;
	}
	}
	return g_pfnOldEditProc ? CallWindowProc(g_pfnOldEditProc, hWnd, uMsg, wParam, lParam) : DefWindowProc(hWnd, uMsg, wParam, lParam);
}
LRESULT CALLBACK ListBoxSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_MOUSEWHEEL: {
		if (g_listBoxItemHeight <= 0) break;
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		INT_PTR currentTopIndex = SendMessage(hWnd, LB_GETTOPINDEX, 0, 0);
		INT_PTR totalItems = SendMessage(hWnd, LB_GETCOUNT, 0, 0);
		RECT rcC; GetClientRect(hWnd, &rcC);
		int itemsInView = (rcC.bottom > 0 && g_listBoxItemHeight > 0) ? (rcC.bottom / g_listBoxItemHeight) : 1;
		if (totalItems <= itemsInView) return 0;
		int scrollAmount = 3;
		INT_PTR newTopIndex = currentTopIndex + (delta > 0 ? -scrollAmount : scrollAmount);
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
		if (g_pfnOldListBoxProc) {
			SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)g_pfnOldListBoxProc);
			WNDPROC tempProc = g_pfnOldListBoxProc;
			g_pfnOldListBoxProc = NULL;
			return CallWindowProc(tempProc, hWnd, uMsg, wParam, lParam);
		}
		break;
	}
	}
	return g_pfnOldListBoxProc ? CallWindowProc(g_pfnOldListBoxProc, hWnd, uMsg, wParam, lParam) : DefWindowProc(hWnd, uMsg, wParam, lParam);
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE: {
		AddTrayIcon(hWnd);
		g_hPreloadCacheThread = CreateThread(NULL, 0, PreloadCacheThreadProc, NULL, 0, NULL);
		RECT clientRect;
		GetClientRect(hWnd, &clientRect);
		int controlWidth = clientRect.right - (2 * G_PIXEL_GAP);

		g_hEdit = CreateWindowEx(0, L"EDIT", L"",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_LEFT,
			G_PIXEL_GAP, G_PIXEL_GAP,
			controlWidth, G_EDIT_HEIGHT,
			hWnd, (HMENU)101, g_hInstance, NULL
		);

		if (g_hEdit) {
			g_pfnOldEditProc = (WNDPROC)SetWindowLongPtr(g_hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
		}
		else {
			return -1;
		}

		g_hListBox = CreateWindowEx(0, L"LISTBOX", L"",
			WS_CHILD | LBS_NOTIFY | LBS_HASSTRINGS | LBS_OWNERDRAWFIXED,
			G_PIXEL_GAP, G_PIXEL_GAP + G_EDIT_HEIGHT + G_PIXEL_GAP,
			controlWidth, 0, hWnd,
			(HMENU)102, g_hInstance, NULL
		);

		if (g_hListBox) {
			g_pfnOldListBoxProc = (WNDPROC)SetWindowLongPtr(g_hListBox, GWLP_WNDPROC, (LONG_PTR)ListBoxSubclassProc);
		}

		CreateAndSetFont(hWnd);
		if (g_hListBox && g_hFont) {
			HDC hdc = GetDC(g_hListBox);
			if (hdc) {
				TEXTMETRIC tm;
				HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
				GetTextMetrics(hdc, &tm);
				SelectObject(hdc, hOldFont);
				ReleaseDC(g_hListBox, hdc);
				g_listBoxItemHeight = tm.tmHeight + G_PIXEL_GAP;
				SendMessage(g_hListBox, LB_SETITEMHEIGHT, 0, (LPARAM)g_listBoxItemHeight);
			}
		}
		if (g_hListBox) ShowWindow(g_hListBox, SW_HIDE);
		return 0;
	}
	case WM_MEASUREITEM: ((LPMEASUREITEMSTRUCT)lParam)->itemHeight = g_listBoxItemHeight; return TRUE;
	case WM_DRAWITEM: {
		LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
		if (!lpdis || lpdis->CtlID != 102 || lpdis->CtlType != ODT_LISTBOX) return TRUE;
		UINT_PTR itemIndex = lpdis->itemID;
		if (itemIndex == (UINT_PTR)-1) return TRUE;
		HDC hdc = lpdis->hDC;
		RECT rcItem = lpdis->rcItem;
		bool isSelected = (lpdis->itemState & ODS_SELECTED);
		LRESULT topIndex = SendMessage(g_hListBox, LB_GETTOPINDEX, 0, 0);
		INT_PTR visibleSlotIndex = static_cast<INT_PTR>(itemIndex) - topIndex;
		EnterCriticalSection(&g_csSearchData);
		if (itemIndex >= g_searchResults.size()) {
			LeaveCriticalSection(&g_csSearchData); FillRect(hdc, &rcItem, g_hbrListBoxBackground); return TRUE;
		}

		SearchResult currentItem = g_searchResults[itemIndex];
		LeaveCriticalSection(&g_csSearchData);
		FillRect(hdc, &rcItem, isSelected ? g_hbrListBoxSelectedBackground : g_hbrListBoxBackground);
		SetTextColor(hdc, isSelected ? g_clrListBoxSelectedText : g_clrListBoxText);
		SetBkMode(hdc, TRANSPARENT);
		std::wstring displayName = currentItem.displayName;
		std::wstring shortcutHintText;

		if (visibleSlotIndex >= 0 && visibleSlotIndex < static_cast<INT_PTR>(MAX_VISIBLE_LIST_ITEMS)) {
			shortcutHintText = L" #" + std::to_wstring((static_cast<int>(visibleSlotIndex) + 1) % 10);
		}
		HFONT hOldFont = (HFONT)SelectObject(hdc, g_hFont);
		if (!shortcutHintText.empty()) {
			RECT rcShortcut = rcItem; rcShortcut.right -= G_PIXEL_GAP;
			DrawText(hdc, shortcutHintText.c_str(), -1, &rcShortcut, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		}
		RECT rcDisplayName = rcItem;
		rcDisplayName.left += G_PIXEL_GAP;
		rcDisplayName.right -= (G_PIXEL_GAP * 2 + 30);
		DrawText(hdc, displayName.c_str(), -1, &rcDisplayName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		SelectObject(hdc, hOldFont);
		return TRUE;
	}
	case WM_CTLCOLOREDIT: {
		HDC hE = (HDC)wParam;
		SetTextColor(hE, g_clrEditText);
		SetBkColor(hE, g_clrEditBg);
		return(LRESULT)g_hbrEditBackground;
	}
	case WM_CTLCOLORLISTBOX: {
		return(LRESULT)g_hbrListBoxBackground;
	}
	case WM_COMMAND: {
		if (LOWORD(wParam) == IDM_TRAY_EXIT) {
			DestroyWindow(hWnd);
			return 0;
		}
		if (LOWORD(wParam) == IDM_TRAY_STARTUP) {
			SetRunOnStartup(!IsRunOnStartupEnabled());
			return 0;
		}

		if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == g_hEdit) {
			KillTimer(hWnd, IDT_SEARCH_DELAY); wchar_t cT[512];
			GetWindowText(g_hEdit, cT, 512);
			g_currentSearchQuery = cT;
			SetTimer(hWnd, IDT_SEARCH_DELAY, SEARCH_DELAY_MS, NULL);
		}
		else if (HIWORD(wParam) == LBN_DBLCLK && (HWND)lParam == g_hListBox) {
			LRESULT sel = SendMessage(g_hListBox, LB_GETCURSEL, 0, 0); LaunchItemByIndex(hWnd, sel);
		}
		break;
	}
	case WM_TIMER: {
		if (wParam == IDT_SEARCH_DELAY) {
			KillTimer(hWnd, IDT_SEARCH_DELAY);
			if (g_hSearchThread) {
				g_hSearchThread = NULL;
			}
			SearchThreadParams* params = new SearchThreadParams;
			params->query = g_currentSearchQuery;
			params->generation = InterlockedIncrement(&g_searchGeneration);
			g_hSearchThread = CreateThread(NULL, 0, SearchThreadProc, params, 0, NULL);
			if (!g_hSearchThread) {
				delete params;
			}
		}
		break;
	}
	case WM_APP_SEARCH_COMPLETE: {
		LONG resultGeneration = (LONG)lParam;
		if (resultGeneration == InterlockedExchangeAdd(&g_searchGeneration, 0)) {
			std::vector<SearchResult>* results = (std::vector<SearchResult>*)wParam;
			EnterCriticalSection(&g_csSearchData);
			if (results) { g_searchResults.swap(*results); }
			else { g_searchResults.clear(); }
			LeaveCriticalSection(&g_csSearchData);
			UpdateListBoxVisibilityAndContent();
			if (g_hListBox && SendMessage(g_hListBox, LB_GETCOUNT, 0, 0) > 0) {
				SendMessage(g_hListBox, LB_SETCURSEL, 0, 0);
			}
		}
		std::vector<SearchResult>* results = (std::vector<SearchResult>*)wParam;
		if (results) {
			delete results;
		}
		break;
	}
	case WM_APP_TRAY_MSG: {
		switch (lParam) {
		case WM_RBUTTONUP:
		{
			HMENU hMenu = CreatePopupMenu();

			UINT startupFlags = MF_BYPOSITION | MF_STRING | (IsRunOnStartupEnabled() ? MF_CHECKED : MF_UNCHECKED);
			InsertMenu(hMenu, 0, startupFlags, IDM_TRAY_STARTUP, L"开机时启动 (Run on startup)");
			InsertMenu(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
			InsertMenu(hMenu, 2, MF_BYPOSITION | MF_STRING, IDM_TRAY_EXIT, L"退出 (Exit)");

			SetForegroundWindow(hWnd);
			POINT pt;
			GetCursorPos(&pt);
			TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
			DestroyMenu(hMenu);
		}
		break;
		case WM_LBUTTONUP:
			ToggleWindowVisibility();
			break;
		}
		return 0;
	}
	case WM_ACTIVATE: {
		if (wParam == WA_INACTIVE) {
			if (IsWindowVisible(hWnd)) {
				ShowWindow(hWnd, SW_HIDE);
			}
		}
		else {
			if (g_hEdit) SetFocus(g_hEdit);
		}
		break;
	}
	case WM_DESTROY: {
		RemoveTrayIcon(hWnd);
		KillTimer(hWnd, IDT_SEARCH_DELAY);
		CleanupFont();
		UnregisterHotKey(hWnd, g_hotkeyId);
		PostQuitMessage(0);
		break;
	}
	case WM_HOTKEY: {
		if (wParam == g_hotkeyId) ToggleWindowVisibility();
		break;
	}
	case WM_CLOSE: {
		ShowWindow(hWnd, SW_HIDE);
		return 0;
	}
	default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}