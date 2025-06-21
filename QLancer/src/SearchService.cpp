#include "SearchService.h"
#include "Utils.h"

DWORD WINAPI SearchThreadProc(LPVOID lpParam) {
    HRESULT comInitHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SearchThreadParams* params = static_cast<SearchThreadParams*>(lpParam);

    SearchService* service = params->service;
    std::wstring originalQuery = params->query;
    LONG generation = params->generation;
    HWND notifyWindow = params->notifyWindow;
    delete params;

    auto* currentThreadResults = new std::vector<SearchResult>();
    std::wstring lowerQuery = Utils::ToLower(Utils::TrimWhitespace(originalQuery));

    service->PopulateAppCacheIfNeeded();

    if (lowerQuery.rfind(L"quit", 0) == 0) {
        std::set<std::wstring> runningProcesses = Utils::GetRunningProcessNames();
        std::set<std::wstring> processedProcesses;

        std::wstring filterQuery;
        if (lowerQuery.length() > 4 && iswspace(lowerQuery[4])) {
            filterQuery = Utils::ToLower(Utils::TrimWhitespace(lowerQuery.substr(5)));
        }

        if (filterQuery.empty()) {
            currentThreadResults->push_back({ L"Quit All Apps", CMD_TERMINATE_ALL, 0, true, 10001, SourcePriority::PRIORITY_LOW, NULL, L"" });
        }

        EnterCriticalSection(&service->m_csSearchData);
        std::vector<SearchResult> localAppCacheCopy = service->m_applicationCache;
        LeaveCriticalSection(&service->m_csSearchData);

        for (auto& app : localAppCacheCopy) {
            if (app.sourcePriority < SourcePriority::PRIORITY_DESKTOP) continue;
            std::wstring processName = Utils::ToLower(PathFindFileNameW(app.fullPath.c_str()));
            if (processName.empty() || processedProcesses.count(processName)) continue;

            if (runningProcesses.count(processName)) {
                std::wstring loweredDisplayName = Utils::ToLower(app.displayName);
                if (loweredDisplayName.find(L"windows software development kit") != std::wstring::npos) continue;

                int score = filterQuery.empty() ? 1000 : service->ScoreFuzzyMatch(filterQuery, loweredDisplayName);
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
        EnterCriticalSection(&service->m_csSearchData);
        std::vector<SearchResult> localAppCacheCopy = service->m_applicationCache;
        std::map<std::wstring, int> localLaunchFreq = service->m_launchFrequency;
        LeaveCriticalSection(&service->m_csSearchData);

        for (auto app : localAppCacheCopy) {
            int score = service->ScoreFuzzyMatch(lowerQuery, Utils::ToLower(app.displayName));
            if (score > 0) {
                app.score = score;
                app.launchCount = localLaunchFreq[app.fullPath];
                currentThreadResults->push_back(app);
            }
        }
        for (auto cmd : service->m_systemCommands) {
            int score = service->ScoreFuzzyMatch(lowerQuery, Utils::ToLower(cmd.displayName));
            if (score > 0) {
                cmd.score = score;
                cmd.launchCount = localLaunchFreq[cmd.fullPath];
                currentThreadResults->push_back(cmd);
            }
        }
    }

    std::sort(currentThreadResults->begin(), currentThreadResults->end(),
        [](const SearchResult& a, const SearchResult& b) {
            if (a.score != b.score) return a.score > b.score;
            if (a.launchCount != b.launchCount) return a.launchCount > b.launchCount;
            if (a.sourcePriority != b.sourcePriority) return a.sourcePriority > b.sourcePriority;
            return Utils::ToLower(a.displayName) < Utils::ToLower(b.displayName);
        });

    PostMessage(notifyWindow, WM_APP_SEARCH_COMPLETE, (WPARAM)currentThreadResults, (LPARAM)generation);
    if (SUCCEEDED(comInitHr)) CoUninitialize();
    return 0;
}

DWORD WINAPI PreloadCacheThreadProc(LPVOID lpParam) {
    SearchService* service = (SearchService*)lpParam;
    if (SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
        service->PopulateAppCacheIfNeeded();
        CoUninitialize();
    }
    return 0;
}


SearchService::SearchService() :
    m_systemCommands({
        {L"Shutdown", CMD_SHUTDOWN, 0, true, 0, SourcePriority::PRIORITY_LOW, NULL, L""},
        {L"Restart", CMD_RESTART, 0, true, 0, SourcePriority::PRIORITY_LOW, NULL, L""},
        {L"Sleep", CMD_SLEEP, 0, true, 0, SourcePriority::PRIORITY_LOW, NULL, L""},
        {L"Empty Recycle Bin", CMD_EMPTY_TRASH, 0, true, 0, SourcePriority::PRIORITY_LOW, NULL, L""}
        })
{
    InitializeCriticalSection(&m_csSearchData);
}

SearchService::~SearchService() {
    DeleteCriticalSection(&m_csSearchData);
}

void SearchService::StartSearch(const std::wstring& query, HWND notifyWindow) {
    SearchThreadParams* params = new SearchThreadParams;
    params->service = this;
    params->query = query;
    params->generation = InterlockedIncrement(&m_searchGeneration);
    params->notifyWindow = notifyWindow;

    HANDLE hSearchThread = CreateThread(NULL, 0, SearchThreadProc, params, 0, NULL);
    if (hSearchThread) {
        CloseHandle(hSearchThread); 
    }
    else {
        delete params;
    }
}

void SearchService::PreloadCacheAsync() {
    HANDLE hThread = CreateThread(NULL, 0, PreloadCacheThreadProc, this, 0, NULL);
    if (hThread) CloseHandle(hThread);
}

void SearchService::SetResults(std::vector<SearchResult>* results, LONG generation) {
    LONG currentGeneration = InterlockedExchangeAdd(&m_searchGeneration, 0);
    if (generation == currentGeneration) {
        EnterCriticalSection(&m_csSearchData);
        if (results) {
            m_searchResults.swap(*results);
        }
        else {
            m_searchResults.clear();
        }
        LeaveCriticalSection(&m_csSearchData);
    }
    else {
        delete results; 
    }
}

const std::vector<SearchResult>& SearchService::GetResults() const {
    return m_searchResults;
}

void SearchService::IncrementLaunchFrequency(const std::wstring& fullPath) {
    EnterCriticalSection(&m_csSearchData);
    m_launchFrequency[fullPath]++;
    LeaveCriticalSection(&m_csSearchData);
}

const std::vector<SearchResult>& SearchService::GetAppCache() const {
    return m_applicationCache;
}

void SearchService::PopulateAppCacheIfNeeded() {
    EnterCriticalSection(&m_csSearchData);
    if (m_appCachePopulated) {
        LeaveCriticalSection(&m_csSearchData);
        return;
    }
    LeaveCriticalSection(&m_csSearchData);

    wchar_t PTH[MAX_PATH];
    std::vector<SearchResult> TCache;
    if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_STARTMENU, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_START_MENU, 5, 0);
    if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_COMMON_STARTMENU, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_START_MENU, 5, 0);
    if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_DESKTOPDIRECTORY, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_DESKTOP, 1, 0);
    if (SUCCEEDED(SHGetFolderPathW(0, CSIDL_COMMON_DESKTOPDIRECTORY, 0, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_DESKTOP, 1, 0);
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_PROGRAM_FILES, 2, 0);
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILESX86, NULL, 0, PTH))) RecursiveScanDirectory(PTH, true, TCache, SourcePriority::PRIORITY_PROGRAM_FILES, 2, 0);

    EnterCriticalSection(&m_csSearchData);
    if (!m_appCachePopulated) {
        m_applicationCache = TCache;
        m_appCachePopulated = true;
    }
    LeaveCriticalSection(&m_csSearchData);
}


void SearchService::RecursiveScanDirectory(const std::wstring& Dir, bool IsApp, std::vector<SearchResult>& Res, SourcePriority Prio, int MaxDepth, int CurrentDepth) {
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
            std::wstring CINL = Utils::ToLower(DFN);

            if (IsApp && CINL.length() >= 4) {
                std::wstring E = CINL.substr(CINL.length() - 4);

                if (E == L".lnk") {
                    std::wstring Ldn = DFN.substr(0, DFN.length() - 4);
                    std::wstring RT, Args;
                    if (Utils::ResolveLnk(CIP.c_str(), RT, Args)) AddItemToResults(Res, Ldn, RT, true, Prio);
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

void SearchService::AddItemToResults(std::vector<SearchResult>& R, const std::wstring& D, const std::wstring& FP, bool AP, SourcePriority Prio) {
    for (const auto& I : R) if (I.fullPath == FP && I.displayName == D) return;

    EnterCriticalSection(&m_csSearchData);
    int launchCount = m_launchFrequency[FP];
    LeaveCriticalSection(&m_csSearchData);

    R.push_back({ D, FP, launchCount, AP, 0, Prio, NULL, L"" });
}

int SearchService::ScoreFuzzyMatch(const std::wstring& query, const std::wstring& target) {
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