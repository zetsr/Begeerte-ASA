#pragma once
#ifndef MM_UTIL_H
#define MM_UTIL_H
#include "Config.h"
#include "NT.h"  // ÒýÈë NT API
#include <tlhelp32.h>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <windows.h>
#include <thread>
#include <chrono>

namespace ManualMapInjector {

    struct HandleDeleter {
        void operator()(void* ptr) const {
            if (ptr) {
                HANDLE handle = static_cast<HANDLE>(ptr);
                if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
                    LoadNtDll();
                    if (NtClose) {
                        NtClose(handle);
                    }
                    else {
                        CloseHandle(handle);
                    }
                }
            }
        }
    };
    using unique_handle = std::unique_ptr<void, HandleDeleter>;

    struct VirtualFreeDeleter {
        HANDLE hProcess;
        VirtualFreeDeleter(HANDLE process) : hProcess(process) {}
        void operator()(void* memory) const {
            if (memory) {
                LoadNtDll();
                if (NtFreeVirtualMemory) {
                    SIZE_T size = 0;
                    NtFreeVirtualMemory(hProcess, &memory, &size, MEM_RELEASE);
                }
                else {
                    VirtualFreeEx(hProcess, memory, 0, MEM_RELEASE);
                }
            }
        }
    };
    using unique_virtual_mem = std::unique_ptr<void, VirtualFreeDeleter>;

    inline std::string WstringToUtf8(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, NULL, NULL);
        str.resize(size - 1);
        return str;
    }

    inline HMODULE GetModuleBaseInTargetProcess(HANDLE hProcess, const std::wstring& moduleName) {
        const int maxRetries = 5;
        const int retryDelayMs = 1000;

        for (int retry = 0; retry < maxRetries; ++retry) {
            HANDLE rawSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetProcessId(hProcess));
            if (rawSnapshot == INVALID_HANDLE_VALUE) {
                if (retry < maxRetries - 1) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
                    continue;
                }
                return NULL;
            }
            unique_handle snapshotHandle(static_cast<void*>(rawSnapshot), HandleDeleter());
            MODULEENTRY32 me32 = { sizeof(MODULEENTRY32) };
            HANDLE snapshotH = static_cast<HANDLE>(snapshotHandle.get());
            if (Module32First(snapshotH, &me32)) {
                do {
                    if (_wcsicmp(me32.szModule, moduleName.c_str()) == 0) {
                        if (retry > 0) {
                        }
                        return (HMODULE)me32.modBaseAddr;
                    }
                } while (Module32Next(snapshotH, &me32));
            }
            if (retry < maxRetries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            }
        }
        return NULL;
    }

    inline FARPROC FindRemoteProcAddress(HANDLE hProcess, const std::wstring& moduleName, const std::string& procName) {
        HMODULE hTargetMod = GetModuleBaseInTargetProcess(hProcess, moduleName);
        if (!hTargetMod) {
            return NULL;
        }
        std::string moduleNameUtf8 = WstringToUtf8(moduleName);
        HMODULE hInjectorMod = GetModuleHandleA(moduleNameUtf8.c_str());
        bool loadedByUs = false;
        if (!hInjectorMod) {
            hInjectorMod = LoadLibraryA(moduleNameUtf8.c_str());
            if (!hInjectorMod) {
                return NULL;
            }
            loadedByUs = true;
        }
        FARPROC injectorFuncAddr = GetProcAddress(hInjectorMod, procName.c_str());
        if (!injectorFuncAddr) {
            if (loadedByUs) FreeLibrary(hInjectorMod);
            return NULL;
        }
        TULONGLONG funcOffset = (TULONGLONG)injectorFuncAddr - (TULONGLONG)hInjectorMod;
        FARPROC result = (FARPROC)((LPBYTE)hTargetMod + funcOffset);
        if (loadedByUs) {
            FreeLibrary(hInjectorMod);
        }
        return result;
    }

    inline DWORD RvaToFileOffset(PIMAGE_NT_HEADERS_CURRENT pNtHeaders, DWORD rva, size_t fileSize) {
        if (rva < pNtHeaders->OptionalHeader.SizeOfHeaders) return rva;
        PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
        for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
            if (rva >= pSectionHeader->VirtualAddress && rva < pSectionHeader->VirtualAddress + pSectionHeader->Misc.VirtualSize) {
                if (rva - pSectionHeader->VirtualAddress < pSectionHeader->SizeOfRawData) {
                    DWORD offset = rva - pSectionHeader->VirtualAddress + pSectionHeader->PointerToRawData;
                    if (offset < static_cast<DWORD>(fileSize)) return offset;
                }
                return 0;
            }
        }
        return 0;
    }

#pragma optimize("", off)
    template<typename T>
    size_t GetFunctionSize(T* function) {
        uint8_t* ptr = (uint8_t*)function;
        while (true) {
#if defined(_WIN64)
            if (ptr[0] == 0xC3 || ptr[0] == 0xC2)
#else
            if (ptr[0] == 0xC3 || ptr[0] == 0xC2 || ptr[0] == 0xC9 || ptr[0] == 0xCA)
#endif
            {
                bool isFunctionEnd = true;
                for (int i = 1; i < 8; i++) {
                    if (ptr[i] == 0xCC) break;
                    if (ptr[i] != 0x00 && ptr[i] != 0x90) {
                        isFunctionEnd = false;
                        break;
                    }
                }
                if (isFunctionEnd) {
                    return (ptr - (uint8_t*)function) + 1;
                }
            }
            ptr++;
        }
    }
#pragma optimize("", on)
}
#endif