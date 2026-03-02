#ifndef MM_MANUALMAP_H
#define MM_MANUALMAP_H
#include "API.h"
#include <cstdio>
#include "Util.h"
#include "NT.h"
#include <thread>
#include <chrono>

#pragma warning(disable: 28251)  // 忽略 NTSTATUS 警告

namespace ManualMapInjector {
    // 等待进程稳定
    inline bool WaitForProcessStability(HANDLE processH, DWORD timeoutMs = 2000) {
        const int maxChecks = 20;
        const int checkDelayMs = timeoutMs / maxChecks;

        for (int check = 0; check < maxChecks; ++check) {
            HMODULE kernel32Base = GetModuleBaseInTargetProcess(processH, L"kernel32.dll");
            HMODULE ntdllBase = GetModuleBaseInTargetProcess(processH, L"ntdll.dll");

            if (kernel32Base && ntdllBase) {
                return true;
            }

            if (check < maxChecks - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(checkDelayMs));
            }
        }

        return false;
    }

    inline bool ManualMapInject(DWORD targetPID, BYTE* dllBuffer, size_t fileSize) {
        if (fileSize < sizeof(IMAGE_DOS_HEADER)) {
            return false;
        }
        PIMAGE_DOS_HEADER pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(dllBuffer);
        if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
            return false;
        }
        if (pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS_CURRENT) > fileSize) {
            return false;
        }
        PIMAGE_NT_HEADERS_CURRENT pNtHeaders = reinterpret_cast<PIMAGE_NT_HEADERS_CURRENT>(dllBuffer + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
            return false;
        }
        if (pNtHeaders->FileHeader.Machine != TARGET_MACHINE) {
            return false;
        }

        LoadNtDll();
        HANDLE processHandle = NULL;
        OBJECT_ATTRIBUTES objAttr = { sizeof(OBJECT_ATTRIBUTES) };
        CLIENT_ID clientId = { (HANDLE)targetPID, NULL };
        NTSTATUS status = NtOpenProcess ? NtOpenProcess(&processHandle, PROCESS_ALL_ACCESS, &objAttr, &clientId) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status) || !processHandle) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            return false;
        }
        unique_handle uniqueProcessHandle(static_cast<void*>(processHandle), HandleDeleter());
        HANDLE processH = static_cast<HANDLE>(uniqueProcessHandle.get());

        if (!WaitForProcessStability(processH)) {
            return false;
        }

        // ==================== 分配远程内存 ====================
        void* allocatedBase = reinterpret_cast<void*>(pNtHeaders->OptionalHeader.ImageBase);
        SIZE_T imageSize = pNtHeaders->OptionalHeader.SizeOfImage;
        SIZE_T regionSize = imageSize;
        status = NtAllocateVirtualMemory ? NtAllocateVirtualMemory(processH, &allocatedBase, 0, &regionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status)) {
            DWORD preferredAllocError = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            allocatedBase = NULL;
            regionSize = imageSize;
            status = NtAllocateVirtualMemory ? NtAllocateVirtualMemory(processH, &allocatedBase, 0, &regionSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE) : STATUS_UNSUCCESSFUL;
            if (!NT_SUCCESS(status)) {
                DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
                return false;
            }
        }
        VirtualFreeDeleter deleter(processH);
        unique_virtual_mem allocatedBaseWrapper(allocatedBase, deleter);

        // ==================== 在本地构建完整镜像（包含重定位） ====================
        BYTE* localImage = new BYTE[imageSize]();  // 初始化为零

        // 复制 PE 头
        DWORD sizeOfHeaders = pNtHeaders->OptionalHeader.SizeOfHeaders;
        if (sizeOfHeaders > fileSize) {
            delete[] localImage;
            return false;
        }
        memcpy(localImage, dllBuffer, sizeOfHeaders);

        // 复制节数据
        PIMAGE_SECTION_HEADER pSectionHeader = IMAGE_FIRST_SECTION(pNtHeaders);
        for (WORD i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
            if (pSectionHeader->PointerToRawData != 0 &&
                (pSectionHeader->PointerToRawData > fileSize ||
                    pSectionHeader->PointerToRawData + pSectionHeader->SizeOfRawData > fileSize)) {
                delete[] localImage;
                return false;
            }
            if (static_cast<TULONGLONG>(pSectionHeader->VirtualAddress) + pSectionHeader->Misc.VirtualSize > imageSize) {
                delete[] localImage;
                return false;
            }
            if (pSectionHeader->SizeOfRawData > 0) {
                void* sectionTargetAddress = localImage + pSectionHeader->VirtualAddress;
                LPVOID sectionSourceAddress = dllBuffer + pSectionHeader->PointerToRawData;
                memcpy(sectionTargetAddress, sectionSourceAddress, pSectionHeader->SizeOfRawData);
            }
        }

        // ==================== 在本地处理重定位 ====================
        TULONGLONG delta = (TULONGLONG)allocatedBase - pNtHeaders->OptionalHeader.ImageBase;
        if (delta != 0) {
            IMAGE_DATA_DIRECTORY relocDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            if (relocDir.VirtualAddress == 0 || relocDir.Size == 0) {
            }
            else {
                DWORD relocOffset = RvaToFileOffset(pNtHeaders, relocDir.VirtualAddress, fileSize);
                if (relocOffset == 0 || relocOffset + relocDir.Size > fileSize) {
                    delete[] localImage;
                    return false;
                }
                PIMAGE_BASE_RELOCATION pRelocBlock = (PIMAGE_BASE_RELOCATION)(dllBuffer + relocOffset);
                LPBYTE relocTableEnd = (LPBYTE)pRelocBlock + relocDir.Size;

                unsigned long relocationSuccessCount = 0;
                unsigned long relocationSkippedCount = 0;
                unsigned long relocationFailedCount = 0;
                unsigned long long relocationCounter = 0;

                while ((LPBYTE)pRelocBlock < relocTableEnd && pRelocBlock->SizeOfBlock > 0) {
                    if ((LPBYTE)pRelocBlock + pRelocBlock->SizeOfBlock > relocTableEnd) {
                        delete[] localImage;
                        return false;
                    }
                    DWORD count = (pRelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                    PWORD pRelocEntry = (PWORD)((LPBYTE)pRelocBlock + sizeof(IMAGE_BASE_RELOCATION));

                    for (DWORD i = 0; i < count; ++i, ++pRelocEntry) {
                        WORD type = (*pRelocEntry >> 12);
                        WORD offset = (*pRelocEntry & 0xFFF);
                        relocationCounter++;

                        if (type == IMAGE_REL_BASED_ABSOLUTE) { // 0
                            relocationSkippedCount++;
                            continue;
                        }

                        DWORD rva = pRelocBlock->VirtualAddress + offset;
                        void* patchAddrLocal = localImage + rva;

                        if ((LPBYTE)patchAddrLocal < localImage || (LPBYTE)patchAddrLocal >= localImage + imageSize) {
                            // 重定位失败日志 - 使用指定格式

                            relocationFailedCount++;
                            continue;
                        }

#if defined(_WIN64)
                        // x64 注入 (只处理 DIR64) - 保持原有操作
                        if (type == IMAGE_REL_BASED_DIR64) { // 10
                            TULONGLONG* patchAddr = reinterpret_cast<TULONGLONG*>(patchAddrLocal);
                            TULONGLONG originalValue = *patchAddr;
                            TULONGLONG newValue = originalValue + delta;
                            *patchAddr = newValue;

                            // 重定位成功日志 - 使用指定格式

                            relocationSuccessCount++;
                        }
                        else {
                            // 重定位失败日志（不支持的类型）

                            relocationFailedCount++;
                        }
#else
                        // x86 注入 (处理 HIGHLOW 和 HIGHADJ) - 保持原有操作
                        if (type == IMAGE_REL_BASED_HIGHLOW) { // 3
                            DWORD* patchAddr = reinterpret_cast<DWORD*>(patchAddrLocal);
                            DWORD originalValue = *patchAddr;
                            DWORD newValue = originalValue + static_cast<DWORD>(delta);
                            *patchAddr = newValue;

                            // 重定位成功日志 - 使用指定格式

                            relocationSuccessCount++;
                        }
                        else if (type == IMAGE_REL_BASED_HIGH) { // 1
                            WORD* patchAddr = reinterpret_cast<WORD*>(patchAddrLocal);
                            WORD originalValue = *patchAddr;
                            WORD newValue = originalValue + HIWORD(static_cast<DWORD>(delta));
                            *patchAddr = newValue;

                            // 重定位成功日志 - 使用指定格式

                            relocationSuccessCount++;
                        }
                        else if (type == IMAGE_REL_BASED_LOW) { // 2
                            WORD* patchAddr = reinterpret_cast<WORD*>(patchAddrLocal);
                            WORD originalValue = *patchAddr;
                            WORD newValue = originalValue + LOWORD(static_cast<DWORD>(delta));
                            *patchAddr = newValue;

                            // 重定位成功日志 - 使用指定格式

                            relocationSuccessCount++;
                        }
                        else if (type == IMAGE_REL_BASED_HIGHADJ) { // 4
                            if (i + 1 >= count) {
                                // 重定位失败日志（HIGHADJ 缺少调整值）

                                relocationFailedCount++;
                                continue;
                            }
                            ++i; // 消耗下一个条目
                            ++pRelocEntry;
                            SHORT adjustment = static_cast<SHORT>(*pRelocEntry); // 这是调整值

                            WORD* highPartAddr = reinterpret_cast<WORD*>(patchAddrLocal);
                            WORD originalValue = *highPartAddr;

                            // 计算完整的 32 位地址
                            DWORD originalAddr = (originalValue << 16) + adjustment;
                            DWORD newAddr = originalAddr + static_cast<DWORD>(delta);

                            // 写回新的 16 位高位，必须 +0x8000 来处理有符号的低位
                            WORD newValue = static_cast<WORD>((newAddr + 0x8000) >> 16);
                            *highPartAddr = newValue;

                            // 重定位成功日志 - 使用指定格式

                            relocationSuccessCount++;
                        }
                        else {
                            // 重定位失败日志（不支持的类型）

                            relocationFailedCount++;
                        }
#endif
                    }
                    pRelocBlock = (PIMAGE_BASE_RELOCATION)((LPBYTE)pRelocBlock + pRelocBlock->SizeOfBlock);
                }

            }
        }
        else {

        }

        // ==================== 一次性写入完整镜像 ====================
        SIZE_T bytesWritten;
        status = NtWriteVirtualMemory ? NtWriteVirtualMemory(processH, allocatedBase, localImage, imageSize, &bytesWritten) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status) || bytesWritten != imageSize) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();

            delete[] localImage;
            return false;
        }


        // 清理本地镜像
        delete[] localImage;

        // ==================== 准备 shellcode 数据 ====================
        FARPROC pLoadLibraryA_Remote = FindRemoteProcAddress(processH, L"kernel32.dll", "LoadLibraryA");
        FARPROC pGetProcAddress_Remote = FindRemoteProcAddress(processH, L"kernel32.dll", "GetProcAddress");
        if (!pLoadLibraryA_Remote || !pGetProcAddress_Remote) {

            return false;
        }

        // 验证导入表目录
        IMAGE_DATA_DIRECTORY importDir = pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.VirtualAddress == 0 || importDir.Size == 0) {

        }

        void* shellcodeDataMem = NULL;
        SIZE_T shellcodeDataSize = sizeof(ShellcodeData);
        status = NtAllocateVirtualMemory ? NtAllocateVirtualMemory(processH, &shellcodeDataMem, 0, &shellcodeDataSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status)) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();

            return false;
        }
        unique_virtual_mem shellcodeDataWrapper(shellcodeDataMem, deleter);

        // ==================== 关键修复：确保 ShellcodeData 结构正确 ====================
        ShellcodeData data;
        memset(&data, 0, sizeof(ShellcodeData));  // 清零初始化

        data.InjectedDllBase = allocatedBase;
        data.pLoadLibraryA = (LoadLibraryA_t)pLoadLibraryA_Remote;
        data.pGetProcAddress = (GetProcAddress_t)pGetProcAddress_Remote;
        data.ImportDirRVA = importDir.VirtualAddress;
        data.ImportDirSize = importDir.Size;

        status = NtWriteVirtualMemory ? NtWriteVirtualMemory(processH, shellcodeDataMem, &data, sizeof(ShellcodeData), &bytesWritten) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status) || bytesWritten != sizeof(ShellcodeData)) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            return false;
        }

        // ==================== 写入 shellcode ====================
        SIZE_T shellcodeSize = GetFunctionSize(Shellcode);

        // 验证 shellcode 大小
        if (shellcodeSize == 0 || shellcodeSize > 4096) {
            return false;
        }

        void* shellcodeMem = NULL;
        SIZE_T shellcodeAllocSize = shellcodeSize;
        status = NtAllocateVirtualMemory ? NtAllocateVirtualMemory(processH, &shellcodeMem, 0, &shellcodeAllocSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status)) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            return false;
        }
        unique_virtual_mem shellcodeWrapper(shellcodeMem, deleter);

        status = NtWriteVirtualMemory ? NtWriteVirtualMemory(processH, shellcodeMem, (LPVOID)Shellcode, shellcodeSize, &bytesWritten) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status) || bytesWritten != shellcodeSize) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            return false;
        }

        // ==================== 清理和创建线程 ====================
        SecureZeroMemory(dllBuffer, fileSize);

        // 添加短暂延迟确保内存稳定
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        HANDLE threadHandle = NULL;
        OBJECT_ATTRIBUTES threadObjAttr = { sizeof(OBJECT_ATTRIBUTES) };
        status = NtCreateThreadEx ? NtCreateThreadEx(&threadHandle, THREAD_ALL_ACCESS, &threadObjAttr, processH, (PVOID)shellcodeMem, shellcodeDataMem, 0, 0, 0, 0, NULL) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status) || !threadHandle) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
            return false;
        }
        unique_handle uniqueThreadHandle(static_cast<void*>(threadHandle), HandleDeleter());

        status = NtWaitForSingleObject ? NtWaitForSingleObject(static_cast<HANDLE>(uniqueThreadHandle.get()), FALSE, NULL) : STATUS_UNSUCCESSFUL;
        if (!NT_SUCCESS(status)) {
            DWORD error = RtlNtStatusToDosError ? RtlNtStatusToDosError(status) : GetLastError();
        }

        DWORD exitCode = 0;
        GetExitCodeThread(static_cast<HANDLE>(uniqueThreadHandle.get()), &exitCode);

        allocatedBaseWrapper.release();
        shellcodeDataWrapper.release();
        shellcodeWrapper.release();
        return exitCode == 1;  // 成功时 shellcode 应该返回 1
    }
}
#endif