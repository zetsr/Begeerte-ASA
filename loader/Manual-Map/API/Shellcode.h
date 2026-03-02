#ifndef MM_SHELLCODE_H
#define MM_SHELLCODE_H

#include "Config.h"
#include <windows.h>

namespace ManualMapInjector {

    typedef HMODULE(WINAPI* LoadLibraryA_t)(LPCSTR lpFileName);
    typedef FARPROC(WINAPI* GetProcAddress_t)(HMODULE hModule, LPCSTR lpProcName);
    typedef BOOL(WINAPI* DllEntryProc)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved);

    struct ShellcodeData {
        LPVOID InjectedDllBase;
        LoadLibraryA_t pLoadLibraryA;
        GetProcAddress_t pGetProcAddress;
        DWORD ImportDirRVA;
        DWORD ImportDirSize;
    };

    inline DWORD WINAPI Shellcode(LPVOID lpParameter) {
        if (!lpParameter) return (DWORD)-1;

        ShellcodeData* data = (ShellcodeData*)lpParameter;
        LPVOID imageBase = data->InjectedDllBase;
        LoadLibraryA_t pLoadLibraryA = data->pLoadLibraryA;
        GetProcAddress_t pGetProcAddress = data->pGetProcAddress;
        DWORD importDirRVA = data->ImportDirRVA;
        DWORD importDirSize = data->ImportDirSize;

        PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)imageBase;
        // 修复：跳过 DOS 签名验证（unlink 后无效，但已知注入内存 OK）
        // if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) return (DWORD)-2;  // 注释掉

        PIMAGE_NT_HEADERS_CURRENT pNtHeaders = (PIMAGE_NT_HEADERS_CURRENT)((LPBYTE)imageBase + pDosHeader->e_lfanew);
        if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) return (DWORD)-3;

        if (!(pNtHeaders->FileHeader.Characteristics & IMAGE_FILE_DLL)) return (DWORD)-4;

        DWORD imageSize = pNtHeaders->OptionalHeader.SizeOfImage;

        // 导入表处理（保持安全检查）
        if (importDirRVA != 0 && importDirSize > 0) {
            if (importDirRVA >= imageSize) return (DWORD)-7;

            PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)imageBase + importDirRVA);
            LPBYTE importDirEnd = (LPBYTE)pImportDesc + importDirSize;

            while ((LPBYTE)pImportDesc < importDirEnd) {
                if (pImportDesc->Name == 0 && pImportDesc->OriginalFirstThunk == 0 && pImportDesc->FirstThunk == 0) {
                    break;
                }

                if (pImportDesc->Name == 0 || pImportDesc->Name >= imageSize) {
                    pImportDesc++;
                    continue;
                }

                char* dllName = (char*)((LPBYTE)imageBase + pImportDesc->Name);
                HMODULE hMod = pLoadLibraryA(dllName);
                if (!hMod) return (DWORD)-5;

                DWORD thunkRVA = pImportDesc->OriginalFirstThunk ? pImportDesc->OriginalFirstThunk : pImportDesc->FirstThunk;
                if (thunkRVA == 0 || thunkRVA >= imageSize) {
                    pImportDesc++;
                    continue;
                }

                PIMAGE_THUNK_DATA pThunkILT = (PIMAGE_THUNK_DATA)((LPBYTE)imageBase + thunkRVA);

                DWORD iatRVA = pImportDesc->FirstThunk;
                if (iatRVA >= imageSize) {
                    pImportDesc++;
                    continue;
                }
                PIMAGE_THUNK_DATA pThunkIAT = (PIMAGE_THUNK_DATA)((LPBYTE)imageBase + iatRVA);

                LPBYTE thunkEnd = (LPBYTE)imageBase + imageSize;

                while ((LPBYTE)pThunkILT < thunkEnd && pThunkILT->u1.AddressOfData != 0) {
                    FARPROC funcAddress = NULL;
                    if (IMAGE_SNAP_BY_ORDINAL(pThunkILT->u1.Ordinal)) {
                        WORD ordinal = IMAGE_ORDINAL(pThunkILT->u1.Ordinal);
                        funcAddress = pGetProcAddress(hMod, (LPCSTR)ordinal);
                    }
                    else {
                        if (pThunkILT->u1.AddressOfData >= imageSize) break;
                        PIMAGE_IMPORT_BY_NAME pImportByName = (PIMAGE_IMPORT_BY_NAME)((LPBYTE)imageBase + pThunkILT->u1.AddressOfData);
                        if ((LPBYTE)pImportByName->Name >= (LPBYTE)imageBase + imageSize) break;
                        char* funcName = (char*)pImportByName->Name;
                        funcAddress = pGetProcAddress(hMod, funcName);
                    }
                    if (!funcAddress) return (DWORD)-6;

#if defined(_WIN64)
                    pThunkIAT->u1.Function = (TULONGLONG)funcAddress;
#else
                    pThunkIAT->u1.Function = (DWORD)funcAddress;
#endif

                    pThunkILT++;
                    pThunkIAT++;
                }
                pImportDesc++;
            }
        }

        DWORD entryPointRVA = pNtHeaders->OptionalHeader.AddressOfEntryPoint;
        if (entryPointRVA != 0) {
            if (entryPointRVA >= imageSize) return (DWORD)-7;

            DllEntryProc dllMain = (DllEntryProc)((LPBYTE)imageBase + entryPointRVA);
            BOOL success = FALSE;

            __try {
                success = dllMain((HINSTANCE)imageBase, DLL_PROCESS_ATTACH, NULL);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return (DWORD)-8;
            }

            return (DWORD)success;
        }
        return (DWORD)1;
    }

}

#endif