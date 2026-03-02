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
    const std::string dllUrl = "https://github.com/zetsr/Begeerte-ASA/raw/refs/heads/main/release/begeerte_ark_survival_ascended.dll";
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