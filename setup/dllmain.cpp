#include <windows.h>
#include <iostream>
#include <vector>
#include <stdio.h>
#include <wininet.h> // 必须包含网络库

#pragma comment(lib, "wininet.lib")

// 移除对 g_dll.h 的依赖
// #include "g_dll.h" 

const char* title = "[github.com/zetsr] ";
typedef BOOL(WINAPI* f_DLL_ENTRY_POINT)(void*, DWORD, void*);

// --- 工具函数：下载 DLL 到内存 ---
bool DownloadToMemory(const std::string& url, std::vector<BYTE>& outBuffer) {
    HINTERNET hInternet = NULL;
    HINTERNET hUrl = NULL;
    bool success = false;

    // 1. 初始化 Internet 环境
    hInternet = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    // 2. 开启 URL (包含安全和重定向标志)
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_SECURE;
    hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
    if (!hUrl) {
        InternetCloseHandle(hInternet);
        return false;
    }

    // 3. 获取文件大小 (解决大文件处理性能的关键)
    DWORD contentLength = 0;
    DWORD dwSize = sizeof(contentLength);
    DWORD dwIndex = 0;

    // 查询 Content-Length
    if (HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &contentLength, &dwSize, &dwIndex)) {
        outBuffer.reserve(contentLength); // 预留空间，防止多次内存重分配
    }

    // 4. 高效读取循环
    const DWORD BUFFER_SIZE = 65536; // 64KB 缓冲区
    BYTE tempBuffer[BUFFER_SIZE];
    DWORD bytesRead = 0;
    outBuffer.clear();

    try {
        while (InternetReadFile(hUrl, tempBuffer, BUFFER_SIZE, &bytesRead) && bytesRead > 0) {
            // 使用 push_back 的批量形式或直接 append
            // 此时因为已经 reserve 过，这里的操作几乎是纯内存拷贝，非常快
            outBuffer.insert(outBuffer.end(), tempBuffer, tempBuffer + bytesRead);
        }
        success = !outBuffer.empty();
    }
    catch (const std::bad_alloc&) {
        std::cerr << "[!] Memory allocation failed for large file." << std::endl;
        success = false;
    }

    // 5. 清理资源
    if (hUrl) InternetCloseHandle(hUrl);
    if (hInternet) InternetCloseHandle(hInternet);

    return success;
}

// --- 原有的反射式加载核心逻辑（保持不变） ---
bool ReflectiveLoad(unsigned char* pRawData) {
    if (!pRawData) return false;

    auto* pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(pRawData);
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return false;

    auto* pOldNtHeader = reinterpret_cast<IMAGE_NT_HEADERS64*>(pRawData + pDosHeader->e_lfanew);
    auto* pOldOptHeader = &pOldNtHeader->OptionalHeader;

    // 1. 分配空间
    unsigned char* pTargetBase = reinterpret_cast<unsigned char*>(VirtualAlloc(
        nullptr, pOldOptHeader->SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE
    ));
    if (!pTargetBase) return false;

    printf("%s[+] Allocated Memory at: 0x%p (Size: 0x%X)\n", title, pTargetBase, pOldOptHeader->SizeOfImage);

    // 2. 拷贝 Headers & Sections
    memcpy(pTargetBase, pRawData, pOldOptHeader->SizeOfHeaders);
    auto* pSectionHeader = IMAGE_FIRST_SECTION(pOldNtHeader);
    for (int i = 0; i < pOldNtHeader->FileHeader.NumberOfSections; ++i, ++pSectionHeader) {
        if (pSectionHeader->SizeOfRawData) {
            memcpy(pTargetBase + pSectionHeader->VirtualAddress, pRawData + pSectionHeader->PointerToRawData, pSectionHeader->SizeOfRawData);
            printf("%s[*] Section %s mapped to 0x%p\n", title, pSectionHeader->Name, pTargetBase + pSectionHeader->VirtualAddress);
        }
    }

    // 3. 修复重定位 (Relocations)
    auto delta = reinterpret_cast<ULONG_PTR>(pTargetBase) - pOldOptHeader->ImageBase;
    if (delta != 0) {
        printf("%s[>] Relocating image (Delta: 0x%llX)...\n", title, delta);
        auto* pRelocDir = &pOldOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (pRelocDir->Size) {
            auto* pRelocBlock = reinterpret_cast<IMAGE_BASE_RELOCATION*>(pTargetBase + pRelocDir->VirtualAddress);
            while (pRelocBlock->VirtualAddress && pRelocBlock->SizeOfBlock) {
                UINT amount = (pRelocBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
                WORD* pRelativeInfo = reinterpret_cast<WORD*>(pRelocBlock + 1);

                for (UINT i = 0; i < amount; ++i) {
                    if ((pRelativeInfo[i] >> 12) == IMAGE_REL_BASED_DIR64) {
                        ULONG_PTR* pPatch = reinterpret_cast<ULONG_PTR*>(pTargetBase + pRelocBlock->VirtualAddress + (pRelativeInfo[i] & 0xFFF));
                        *pPatch += delta;
                    }
                }
                pRelocBlock = reinterpret_cast<IMAGE_BASE_RELOCATION*>(reinterpret_cast<unsigned char*>(pRelocBlock) + pRelocBlock->SizeOfBlock);
            }
        }
    }

    // 4. 修复导入表 (IAT)
    auto* pImportDir = &pOldOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (pImportDir->Size) {
        auto* pImportDescr = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(pTargetBase + pImportDir->VirtualAddress);
        while (pImportDescr->Name) {
            char* szMod = reinterpret_cast<char*>(pTargetBase + pImportDescr->Name);
            HINSTANCE hMod = GetModuleHandleA(szMod);
            if (!hMod) hMod = LoadLibraryA(szMod);

            if (!hMod) return false;

            auto* pThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(pTargetBase + pImportDescr->FirstThunk);
            auto* pRawThunk = reinterpret_cast<IMAGE_THUNK_DATA64*>(pTargetBase + pImportDescr->OriginalFirstThunk);

            while (pRawThunk->u1.AddressOfData) {
                ULONG_PTR fnAddr = 0;
                if (IMAGE_SNAP_BY_ORDINAL64(pRawThunk->u1.Ordinal)) {
                    fnAddr = reinterpret_cast<ULONG_PTR>(GetProcAddress(hMod, reinterpret_cast<char*>(pRawThunk->u1.Ordinal & 0xFFFF)));
                }
                else {
                    auto* pImportData = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(pTargetBase + pRawThunk->u1.AddressOfData);
                    fnAddr = reinterpret_cast<ULONG_PTR>(GetProcAddress(hMod, pImportData->Name));
                }
                pThunk->u1.Function = fnAddr;
                pThunk++;
                pRawThunk++;
            }
            pImportDescr++;
        }
    }

    // 5. 处理 TLS
    auto* pTlsDir = &pOldOptHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_TLS];
    if (pTlsDir->Size) {
        auto* pTls = reinterpret_cast<IMAGE_TLS_DIRECTORY64*>(pTargetBase + pTlsDir->VirtualAddress);
        if (pTls->AddressOfIndex) {
            static DWORD dummy_index = 0;
            *reinterpret_cast<DWORD*>(pTls->AddressOfIndex) = dummy_index;
        }
        if (pTls->AddressOfCallBacks) {
            auto** ppCallback = reinterpret_cast<PIMAGE_TLS_CALLBACK*>(pTls->AddressOfCallBacks);
            while (ppCallback && *ppCallback) {
                (*ppCallback)(pTargetBase, DLL_PROCESS_ATTACH, nullptr);
                ppCallback++;
            }
        }
    }

    // 7. 执行 DllMain
    auto pEntry = reinterpret_cast<f_DLL_ENTRY_POINT>(pTargetBase + pOldOptHeader->AddressOfEntryPoint);
    printf("[!] Calling Final Entry Point at 0x%p...\n", pEntry);
    return pEntry(pTargetBase, DLL_PROCESS_ATTACH, nullptr);
}

// --- 修改后的线程主函数 ---
void MainThread(HMODULE hModule) {
    AllocConsole();
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    SetConsoleTitleA(title);

    const char* targetDllUrl = "https://github.com/zetsr/Begeerte-ASA/raw/refs/heads/main/release/begeerte_ark_survival_ascended.dll";
    std::vector<unsigned char> dllBuffer;

    printf("%s[*] Starting secondary loader...\n", title);
    printf("%s[*] Downloading final payload from GitHub...\n", title);

    // 循环下载直到成功（防止瞬时网络抖动）
    while (!DownloadToMemory(targetDllUrl, dllBuffer)) {
        printf("%s[!] Download failed, retrying in 2 seconds...\n", title);
        Sleep(2000);
    }

    printf("%s[+] Payload downloaded (%zu bytes). Initializing reflective load...\n", title, dllBuffer.size());

    if (ReflectiveLoad(dllBuffer.data())) {
        printf("\n%s[SUCCESS] Final DLL is now running in memory.\n", title);
    }
    else {
        printf("\n%s[ERROR] Reflective loading failed.\n", title);
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}