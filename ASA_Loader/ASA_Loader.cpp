#include <windows.h>
#include <wininet.h>
#include <psapi.h>
#include <iostream>
#include <string>
#include <filesystem>
#include <thread>
#include <tlhelp32.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "psapi.lib")

namespace fs = std::filesystem;

const char* URL = "https://github.com/zetsr/Begeerte-ASA/raw/refs/heads/main/release/begeerte_ark_survival_ascended.dll";
const char* DLL_NAME = "begeerte_ark_survival_ascended.dll";
const char* TARGET_PROCESS = "ArkAscended.exe";

void WriteToConsoleBuffer(const std::string& text) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written = 0;
    WriteFile(h, text.c_str(), text.length(), &written, NULL);
}

bool DownloadDLL() {
    const char* TEMP_DLL_NAME = "begeerte_ark_survival_ascended.dll.tmp";

    HINTERNET hInternet = InternetOpenA("DLLDownloader/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) {
        char buf[256];
        sprintf_s(buf, 256, "Failed to initialize internet: %lu\n", GetLastError());
        WriteToConsoleBuffer(buf);
        return false;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, URL, "User-Agent: Mozilla/5.0\r\n", 0, INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        char buf[256];
        DWORD err = GetLastError();
        sprintf_s(buf, 256, "Failed to open URL: %lu\n", err);
        WriteToConsoleBuffer(buf);
        InternetCloseHandle(hInternet);
        return false;
    }

    DWORD fileSize = 0;
    DWORD sizeLen = sizeof(DWORD);
    HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &fileSize, &sizeLen, NULL);

    if (fileSize == 0) {
        WriteToConsoleBuffer("Failed to get file size\n");
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    HANDLE hFile = CreateFileA(TEMP_DLL_NAME, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        WriteToConsoleBuffer("Failed to create temp file\n");
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    const DWORD BUFFER_SIZE = 1048576;
    BYTE* buffer = (BYTE*)malloc(BUFFER_SIZE);
    if (!buffer) {
        CloseHandle(hFile);
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        WriteToConsoleBuffer("Failed to allocate buffer\n");
        return false;
    }

    DWORD downloaded = 0;
    DWORD totalKB = (fileSize + 1023) / 1024;
    bool success = true;

    while (true) {
        DWORD bytesRead = 0;
        if (!InternetReadFile(hUrl, buffer, BUFFER_SIZE, &bytesRead) || bytesRead == 0) {
            break;
        }

        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            success = false;
            break;
        }

        downloaded += bytesRead;
        DWORD currentKB = (downloaded + 1023) / 1024;
        std::string msg = "Download " + std::to_string(currentKB) + "/" + std::to_string(totalKB) + " KB\n";
        WriteToConsoleBuffer(msg);
    }

    free(buffer);
    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    if (!success || downloaded != fileSize) {
        DeleteFileA(TEMP_DLL_NAME);
        WriteToConsoleBuffer("Download failed or incomplete\n");
        return false;
    }

    if (!ReplaceFileA(DLL_NAME, TEMP_DLL_NAME, NULL, REPLACEFILE_IGNORE_MERGE_ERRORS, NULL, NULL)) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND) {
            if (!MoveFileA(TEMP_DLL_NAME, DLL_NAME)) {
                DeleteFileA(TEMP_DLL_NAME);
                WriteToConsoleBuffer("Failed to move temp file to DLL\n");
                return false;
            }
        }
        else {
            DeleteFileA(TEMP_DLL_NAME);
            WriteToConsoleBuffer("Failed to replace DLL\n");
            return false;
        }
    }

    return true;
}

DWORD FindProcessId(const char* procName) {
    DWORD aProcesses[1024], cProcesses, cbNeeded;
    DWORD processID = 0;

    while (true) {
        if (EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded)) {
            cProcesses = cbNeeded / sizeof(DWORD);

            for (DWORD i = 0; i < cProcesses; i++) {
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, aProcesses[i]);
                if (hProcess) {
                    DWORD dwSize = MAX_PATH;
                    char szProcessName[MAX_PATH] = { 0 };
                    GetModuleBaseNameA(hProcess, NULL, szProcessName, dwSize);

                    if (strcmp(szProcessName, procName) == 0) {
                        processID = aProcesses[i];
                        CloseHandle(hProcess);
                        return processID;
                    }
                    CloseHandle(hProcess);
                }
            }
        }

        WriteToConsoleBuffer("Waiting for target process...\n");
        Sleep(1000);
    }
}

bool IsModuleLoaded(DWORD procID, const char* moduleName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procID);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    MODULEENTRY32 moduleEntry;
    moduleEntry.dwSize = sizeof(MODULEENTRY32);

    bool found = false;
    if (Module32First(hSnapshot, &moduleEntry)) {
        do {
            if (_stricmp(moduleEntry.szModule, moduleName) == 0) {
                found = true;
                break;
            }
        } while (Module32Next(hSnapshot, &moduleEntry));
    }

    CloseHandle(hSnapshot);
    return found;
}

bool WaitForDX12Initialization(DWORD procID) {
    WriteToConsoleBuffer("Waiting for DirectX 12 initialization...\n");

    const char* dx12Modules[] = {
        "d3d12.dll",
        "dxgi.dll",
        "d3d12core.dll"
    };

    const int MAX_WAIT_TIME = 30000;
    const int CHECK_INTERVAL = 100;
    int elapsed = 0;

    bool allModulesLoaded = false;

    while (elapsed < MAX_WAIT_TIME) {
        bool currentCheck = true;

        for (int i = 0; i < sizeof(dx12Modules) / sizeof(dx12Modules[0]); i++) {
            if (!IsModuleLoaded(procID, dx12Modules[i])) {
                currentCheck = false;
                break;
            }
        }

        if (currentCheck) {
            if (!allModulesLoaded) {
                allModulesLoaded = true;
                WriteToConsoleBuffer("DirectX 12 modules detected, waiting for stabilization...\n");
            }

            bool stillLoaded = true;
            for (int i = 0; i < sizeof(dx12Modules) / sizeof(dx12Modules[0]); i++) {
                if (!IsModuleLoaded(procID, dx12Modules[i])) {
                    stillLoaded = false;
                    break;
                }
            }

            if (stillLoaded) {
                WriteToConsoleBuffer("DirectX 12 fully initialized\n");
                return true;
            }
            else {
                allModulesLoaded = false;
            }
        }

        Sleep(CHECK_INTERVAL);
        elapsed += CHECK_INTERVAL;

        if (elapsed % 5000 == 0) {
            char buf[256];
            sprintf_s(buf, 256, "Still waiting for DX12... (%d seconds)\n", elapsed / 1000);
            WriteToConsoleBuffer(buf);
        }
    }

    WriteToConsoleBuffer("Warning: Max wait time reached, proceeding with injection\n");
    return false;
}

bool InjectDLL(DWORD procID) {
    HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, procID);
    if (!hProcess) {
        WriteToConsoleBuffer("Failed to open process\n");
        return false;
    }

    char dllPath[MAX_PATH];
    GetFullPathNameA(DLL_NAME, MAX_PATH, dllPath, NULL);

    size_t pathLen = strlen(dllPath) + 1;
    LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!pDllPath) {
        WriteToConsoleBuffer("Failed to allocate memory in target process\n");
        CloseHandle(hProcess);
        return false;
    }

    if (!WriteProcessMemory(hProcess, pDllPath, dllPath, pathLen, NULL)) {
        WriteToConsoleBuffer("Failed to write DLL path to target process\n");
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    LPVOID pLoadLibrary = GetProcAddress(hKernel32, "LoadLibraryA");

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pLoadLibrary, pDllPath, 0, NULL);
    if (!hThread) {
        WriteToConsoleBuffer("Failed to create remote thread\n");
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    return true;
}

int main() {
    WriteToConsoleBuffer("Starting DLL download...\n");

    if (!DownloadDLL()) {
        WriteToConsoleBuffer("Download failed\n");
        return 1;
    }

    WriteToConsoleBuffer("Download completed\n");
    WriteToConsoleBuffer("Finding target process...\n");

    DWORD procID = FindProcessId(TARGET_PROCESS);
    if (procID == 0) {
        WriteToConsoleBuffer("Target process not found\n");
        return 1;
    }

    char buf[256];
    sprintf_s(buf, 256, "Target process found (PID: %lu)\n", procID);
    WriteToConsoleBuffer(buf);

    WaitForDX12Initialization(procID);
    Sleep(5000);

    WriteToConsoleBuffer("Injecting DLL...\n");

    if (!InjectDLL(procID)) {
        WriteToConsoleBuffer("Injection failed\n");
        return 1;
    }

    WriteToConsoleBuffer("Injection successful\n");
    WriteToConsoleBuffer("Waiting 5 seconds...\n");

    Sleep(5000);

    WriteToConsoleBuffer("Exiting\n");
    return 0;
}