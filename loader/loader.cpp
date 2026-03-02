#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <wininet.h>
#include <tlhelp32.h>
#include <thread>
#include <chrono>

// 包含手动映射库头文件
#include "Manual-Map/API/ManualMapInjector.h" 

#pragma comment(lib, "wininet.lib")

// --- 工具函数：下载 DLL 到内存 ---
bool DownloadToMemory(const std::string& url, std::vector<BYTE>& outBuffer) {
    // 1. 初始化，增加异步或代理兼容性
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return false;

    // 2. 开启 URL，添加处理重定向和缓存的标志
    DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_RESYNCHRONIZE | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);

    if (!hUrl) {
        DWORD err = GetLastError();
        std::cout << "[!] InternetOpenUrlA failed. Error: " << err << std::endl;
        InternetCloseHandle(hInternet);
        return false;
    }

    // 3. 读取数据
    BYTE tempBuffer[4096];
    DWORD bytesRead = 0;
    outBuffer.clear();

    while (InternetReadFile(hUrl, tempBuffer, sizeof(tempBuffer), &bytesRead) && bytesRead > 0) {
        outBuffer.insert(outBuffer.end(), tempBuffer, tempBuffer + bytesRead);
    }

    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    return !outBuffer.empty();
}

// --- 工具函数：根据进程名获取 PID ---
DWORD GetProcessIdByName(const std::wstring& processName) {
    DWORD pid = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe;
        pe.dwSize = sizeof(PROCESSENTRY32W);
        if (Process32FirstW(hSnapshot, &pe)) {
            do {
                if (processName == pe.szExeFile) {
                    pid = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(hSnapshot, &pe));
        }
        CloseHandle(hSnapshot);
    }
    return pid;
}

int main() {
    const std::string dllUrl = "https://github.com/zetsr/Begeerte-ASA/raw/refs/heads/main/release/setup.dll";
    const std::wstring targetProcess = L"ArkAscended.exe";
    std::vector<BYTE> dllBuffer;

    // 1. 无限重试下载直到成功
    std::cout << "[*] 正在尝试下载 DLL..." << std::endl;
    while (true) {
        if (DownloadToMemory(dllUrl, dllBuffer)) {
            std::cout << "[+] 下载成功！大小: " << dllBuffer.size() << " 字节" << std::endl;
            break;
        }
        std::cout << "[!] 下载失败，1秒后重试..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // 2. 每毫秒查找一次进程
    std::cout << "[*] 正在等待进程 " << std::string(targetProcess.begin(), targetProcess.end()) << "..." << std::endl;
    DWORD targetPid = 0;
    while (true) {
        targetPid = GetProcessIdByName(targetProcess);
        if (targetPid != 0) {
            std::cout << "[+] 找到目标进程，PID: " << targetPid << std::endl;
            break;
        }
        // 每毫秒检查一次
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // 3. 执行手动映射注入
    std::cout << "[*] 正在执行手动映射注入..." << std::endl;
    // 注意：dllBuffer.data() 返回 BYTE*，dllBuffer.size() 返回字节数
    bool success = ManualMapInjector::ManualMapInject(targetPid, dllBuffer.data(), dllBuffer.size());

    if (success) {
        std::cout << "[SUCCESS] 注入完成！" << std::endl;
    }
    else {
        std::cerr << "[ERROR] 注入失败。" << std::endl;
    }

    std::cout << "[*] 正在退出程序..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}