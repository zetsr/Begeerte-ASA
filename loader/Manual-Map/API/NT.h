#pragma once
#ifndef NT_H
#define NT_H
#include <windows.h>
#include <winnt.h>
#pragma warning(disable: 28251)

typedef LONG KPRIORITY;
typedef PVOID PPEB;

typedef struct _UNICODE_STRING {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;
    PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, * POBJECT_ATTRIBUTES;

typedef LONG NTSTATUS;
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#define STATUS_UNSUCCESSFUL ((NTSTATUS)0xC0000001L)

typedef struct _PROCESS_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    PVOID PebBaseAddress;
    ULONG_PTR AffinityMask;
    KPRIORITY BasePriority;
    ULONG_PTR UniqueProcessId;
    ULONG_PTR InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION, * PPROCESS_BASIC_INFORMATION;

typedef struct _PEB_LDR_DATA {
    BYTE Reserved1[8];
    PVOID Reserved2[3];
    LIST_ENTRY InLoadOrderModuleList;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY {
    PVOID Reserved1[2];
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    union {
        PVOID Reserved2[2];
        struct {
            PVOID DllBase;
            PVOID EntryPoint;
        };
    };
    PVOID Reserved3[2];
    UNICODE_STRING FullDllName;
    BYTE Reserved4[8];
    PVOID Reserved5[3];
    union {
        ULONG CheckSum;
        PVOID Reserved6;
    };
    ULONG TimeDateStamp;
    PVOID LoadedImports;
    PVOID ImageBase;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

typedef NTSTATUS(NTAPI* pNtOpenProcess)(PHANDLE ProcessHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, PCLIENT_ID ClientId);
typedef NTSTATUS(NTAPI* pNtAllocateVirtualMemory)(HANDLE ProcessHandle, PVOID* BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, ULONG AllocationType, ULONG Protect);
typedef NTSTATUS(NTAPI* pNtFreeVirtualMemory)(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG FreeType);
typedef NTSTATUS(NTAPI* pNtWriteVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToWrite, PSIZE_T NumberOfBytesWritten);
typedef NTSTATUS(NTAPI* pNtReadVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, PVOID Buffer, SIZE_T NumberOfBytesToRead, PSIZE_T NumberOfBytesRead);
typedef NTSTATUS(NTAPI* pNtCreateThreadEx)(PHANDLE ThreadHandle, ACCESS_MASK DesiredAccess, POBJECT_ATTRIBUTES ObjectAttributes, HANDLE ProcessHandle, PVOID StartRoutine, PVOID Argument, ULONG CreateFlags, SIZE_T ZeroBits, SIZE_T StackSize, SIZE_T MaximumStackSize, PVOID AttributeList);
typedef NTSTATUS(NTAPI* pNtWaitForSingleObject)(HANDLE Handle, BOOLEAN Alertable, PLARGE_INTEGER Timeout);
typedef NTSTATUS(NTAPI* pNtClose)(HANDLE Handle);
typedef ULONG(NTAPI* pRtlNtStatusToDosError)(NTSTATUS Status);
typedef NTSTATUS(NTAPI* pNtGetContextThread)(HANDLE ThreadHandle, PCONTEXT Context);
typedef NTSTATUS(NTAPI* pNtSetContextThread)(HANDLE ThreadHandle, PCONTEXT Context);
typedef NTSTATUS(NTAPI* pNtProtectVirtualMemory)(HANDLE ProcessHandle, PVOID* BaseAddress, PSIZE_T RegionSize, ULONG NewProtect, PULONG OldProtect);
typedef NTSTATUS(NTAPI* pNtZeroVirtualMemory)(HANDLE ProcessHandle, PVOID BaseAddress, ULONG_PTR ZeroBits, PSIZE_T RegionSize, PSIZE_T NumberOfBytesCleared);
typedef NTSTATUS(NTAPI* pNtQueryInformationProcess)(HANDLE ProcessHandle, ULONG ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength, PULONG ReturnLength);

static HMODULE g_ntdll = NULL;
static pNtOpenProcess NtOpenProcess = NULL;
static pNtAllocateVirtualMemory NtAllocateVirtualMemory = NULL;
static pNtFreeVirtualMemory NtFreeVirtualMemory = NULL;
static pNtWriteVirtualMemory NtWriteVirtualMemory = NULL;
static pNtReadVirtualMemory NtReadVirtualMemory = NULL;
static pNtCreateThreadEx NtCreateThreadEx = NULL;
static pNtWaitForSingleObject NtWaitForSingleObject = NULL;
static pNtClose NtClose = NULL;
static pRtlNtStatusToDosError RtlNtStatusToDosError = NULL;
static pNtGetContextThread NtGetContextThread = NULL;
static pNtSetContextThread NtSetContextThread = NULL;
static pNtProtectVirtualMemory NtProtectVirtualMemory = NULL;
static pNtZeroVirtualMemory NtZeroVirtualMemory = NULL;
static pNtQueryInformationProcess NtQueryInformationProcess = NULL;

inline void LoadNtDll() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    g_ntdll = GetModuleHandleA("ntdll.dll");
    if (!g_ntdll) return;

#undef NtOpenProcess
#undef NtAllocateVirtualMemory
#undef NtFreeVirtualMemory
#undef NtWriteVirtualMemory
#undef NtReadVirtualMemory
#undef NtCreateThreadEx
#undef NtWaitForSingleObject
#undef NtClose
    NtOpenProcess = (pNtOpenProcess)GetProcAddress(g_ntdll, "NtOpenProcess");
    NtAllocateVirtualMemory = (pNtAllocateVirtualMemory)GetProcAddress(g_ntdll, "NtAllocateVirtualMemory");
    NtFreeVirtualMemory = (pNtFreeVirtualMemory)GetProcAddress(g_ntdll, "NtFreeVirtualMemory");
    NtWriteVirtualMemory = (pNtWriteVirtualMemory)GetProcAddress(g_ntdll, "NtWriteVirtualMemory");
    NtReadVirtualMemory = (pNtReadVirtualMemory)GetProcAddress(g_ntdll, "NtReadVirtualMemory");
    NtCreateThreadEx = (pNtCreateThreadEx)GetProcAddress(g_ntdll, "NtCreateThreadEx");
    NtWaitForSingleObject = (pNtWaitForSingleObject)GetProcAddress(g_ntdll, "NtWaitForSingleObject");
    NtClose = (pNtClose)GetProcAddress(g_ntdll, "NtClose");
    RtlNtStatusToDosError = (pRtlNtStatusToDosError)GetProcAddress(g_ntdll, "RtlNtStatusToDosError");
    NtGetContextThread = (pNtGetContextThread)GetProcAddress(g_ntdll, "NtGetContextThread");
    NtSetContextThread = (pNtSetContextThread)GetProcAddress(g_ntdll, "NtSetContextThread");
    NtProtectVirtualMemory = (pNtProtectVirtualMemory)GetProcAddress(g_ntdll, "NtProtectVirtualMemory");
    NtZeroVirtualMemory = (pNtZeroVirtualMemory)GetProcAddress(g_ntdll, "NtZeroVirtualMemory");
    NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(g_ntdll, "NtQueryInformationProcess");
}

#endif