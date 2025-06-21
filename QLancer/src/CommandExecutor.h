#pragma once
#include "common.h"

class SearchService; 

class CommandExecutor {
public:
    static void Execute(const SearchResult& result, HWND ownerHwnd, SearchService& searchService);

private:
    static void TerminateProcessByName(const std::wstring& processName);
}; 
