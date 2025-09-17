/*
    Original :
        Copyright Raphael Couto
        Apache License Version 2.0
    
    Modified/Altered :
        Copyright 2025 Guillaume Guillet

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
    #define NOMINMAX
#endif

#include <windows.h>
#include <Dbghelp.h>

#include <string>
#include <ranges>
#include <locale>
#include <codecvt>
#include <iostream>
#include <cstdint>
#include <optional>
#include <filesystem>
#include <set>
#include "CLI11.hpp"

std::optional<std::wstring> RetrieveModuleFullPath(std::wstring const& moduleName)
{
    HMODULE hm = LoadLibraryW(moduleName.c_str());

    if (hm == nullptr)
    {
        return std::nullopt;
    }

    WCHAR szPath[MAX_PATH];

    if (GetModuleFileNameW(hm, szPath, MAX_PATH))
    {
        FreeLibrary(hm);
        return szPath;
    }

    FreeLibrary(hm);
    return std::nullopt;
}

std::optional<std::vector<std::wstring>> RetrieveDependencies(HANDLE fileMap, DWORD fileSize)
{
    LPVOID mapView = MapViewOfFile(fileMap, FILE_MAP_READ, 0, 0, fileSize);
    if (mapView == nullptr)
    {
        return std::nullopt;
    }

    PIMAGE_DOS_HEADER dosHeader = static_cast<PIMAGE_DOS_HEADER>(mapView);
    PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(reinterpret_cast<uint8_t*>(dosHeader) + dosHeader->e_lfanew);

    if (IsBadReadPtr(ntHeader, sizeof(IMAGE_NT_HEADERS)))
    {
        UnmapViewOfFile(mapView);
        return std::nullopt;
    }

    char* pSig = reinterpret_cast<char*>(&ntHeader->Signature);
    if (pSig[0] != 'P' || pSig[1] != 'E')
    {
        UnmapViewOfFile(mapView);
        return std::nullopt;
    }

    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)ImageRvaToVa(ntHeader,
                                                                                  mapView,
                                                                                  ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress,
                                                                                  0);

    std::vector<std::wstring> outputs;

    while (pImportDesc && pImportDesc->Name)
    {
        PSTR szCurrMod = static_cast<PSTR>(ImageRvaToVa(ntHeader, mapView, pImportDesc->Name , 0));

        if (IsBadReadPtr(szCurrMod, 1))
        {
            continue;
        }

        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring currentModuleName = converter.from_bytes(szCurrMod);

        //Lets discover the full filename
        currentModuleName = RetrieveModuleFullPath(currentModuleName).value_or(currentModuleName);

        if (std::ranges::find(outputs, currentModuleName) == outputs.end())
        {
            outputs.push_back(std::move(currentModuleName));
        }

        ++pImportDesc;
    }

    UnmapViewOfFile(mapView);
    return outputs;
}

std::optional<std::size_t> RetrieveDependencies(std::filesystem::path const& filepath, 
                                                std::set<std::wstring>& dependencies,
                                                bool recursive)
{
    HANDLE hFile = CreateFileW(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    DWORD filesize = GetFileSize(hFile, NULL);
    if (filesize == 0 || filesize == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return std::nullopt;
    }

    HANDLE hFileMap = CreateFileMappingW(hFile, 0, PAGE_READONLY, 0, 0, L"_tempFileMappingName_");
    if (hFileMap == nullptr)
    {
        CloseHandle(hFile);
        return std::nullopt;
    }

    std::size_t totalCount = 0;
    auto nextDependencies = RetrieveDependencies(hFileMap, filesize);
    if (!nextDependencies)
    {
        CloseHandle(hFileMap);
        CloseHandle(hFile);
        std::wcerr << "Can't retrieve dependencies from " << filepath << '\n';
        return std::nullopt;
    }

    CloseHandle(hFileMap);
    CloseHandle(hFile);

    std::wcout << filepath.filename() << L" as " << nextDependencies->size() << L" dependencies\n";

    if (!recursive)
    {
        dependencies.insert(nextDependencies->begin(), nextDependencies->end());
        return dependencies.size();
    }

    for (auto& nextDependency : nextDependencies.value())
    {
        if (!dependencies.contains(nextDependency))
        {
            dependencies.insert(nextDependency);
            ++totalCount;

            totalCount += RetrieveDependencies(std::move(nextDependency), dependencies, true).value_or(0);
        }
    }

    return totalCount;
}

int main(int argc, char* argv[])
{
    CLI::App app{"Utility to extract DLL dependencies from executable", "depsRetriever"};
    argv = app.ensure_utf8(argv);

    std::filesystem::path filepath;
    app.add_option("-f,--file", filepath, "Executable file")
        ->required()
        ->check(CLI::ExistingFile);
    
    bool recursive = false;
    app.add_flag("-r,--recursive", recursive);

    bool ignoreWindows = false;
    app.add_flag("--ignoreWindows", ignoreWindows);

    std::filesystem::path copyDirPath;
    app.add_option("-c,--copy", copyDirPath, "Copy all dlls into the directory")
        ->check(CLI::ExistingDirectory);

    CLI11_PARSE(app, argc, argv);

    std::wcout << L"Checking file: " << filepath.wstring() << L'\n';

    std::set<std::wstring> dependencies;
    auto const result = RetrieveDependencies(filepath.wstring(), dependencies, recursive);
    if (!result)
    {
        std::wcerr << L"something went wrong\n";
        return 1;
    }

    for (auto const& dllPath : dependencies)
    {
        if (ignoreWindows && dllPath.find(L"C:\\Windows") != std::wstring::npos)
        {
            continue;
        }

        std::wcout << dllPath << '\n';
        if (!copyDirPath.empty())
        {
            std::error_code err;
            bool copyResult = std::filesystem::copy_file(dllPath, 
                copyDirPath / std::filesystem::path{dllPath}.filename(),
                std::filesystem::copy_options::overwrite_existing, err);
            if (!copyResult)
            {
                std::wcerr << L"can't copy " << dllPath << L" to " << copyDirPath << L'\n';
                std::cerr << err.message() << '\n';
                return 1;
            }
        }
    }

    if (dependencies.empty())
    {
        std::wcout << L"no dependencies found !\n";
    }
    else
    {
        std::wcout << L"total dependencies found : " << dependencies.size() << '\n';
    }

    return 0;
}
