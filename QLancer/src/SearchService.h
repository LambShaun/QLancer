#pragma once
#include "common.h"

struct SearchThreadParams {
    class SearchService* service;
    std::wstring query;
    LONG generation;
    HWND notifyWindow;
};

class SearchService {
public:
    SearchService();
    ~SearchService();

    void StartSearch(const std::wstring& query, HWND notifyWindow);
    void PreloadCacheAsync();
    void SetResults(std::vector<SearchResult>* results, LONG generation);    const std::vector<SearchResult>& GetResults() const;
    void IncrementLaunchFrequency(const std::wstring& fullPath);
    const std::vector<SearchResult>& GetAppCache() const;

private:
    friend DWORD WINAPI SearchThreadProc(LPVOID lpParam);
    friend DWORD WINAPI PreloadCacheThreadProc(LPVOID lpParam);

    void PopulateAppCacheIfNeeded();
    void RecursiveScanDirectory(const std::wstring& directory, bool isAppSearch, std::vector<SearchResult>& localResults, SourcePriority priority, int maxDepth, int currentDepth = 0);
    void AddItemToResults(std::vector<SearchResult>& results, const std::wstring& displayName, const std::wstring& fullPath, bool isApp, SourcePriority priority);
    int ScoreFuzzyMatch(const std::wstring& query, const std::wstring& target);

    std::vector<SearchResult> m_applicationCache;
    std::vector<SearchResult> m_searchResults;
    std::map<std::wstring, int> m_launchFrequency;
    bool m_appCachePopulated = false;

    CRITICAL_SECTION m_csSearchData;
    volatile LONG m_searchGeneration = 0;

    const std::vector<SearchResult> m_systemCommands;
};