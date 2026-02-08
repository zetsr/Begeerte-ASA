#include "infoGPU.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <vector>
#include <string>

#pragma comment(lib, "pdh.lib")

namespace g_infoGPU {

    static PDH_HQUERY gpuQuery = nullptr;
    static std::vector<PDH_HCOUNTER> usageCounters;
    static bool isInitialized = false;

    // 内部清理函数
    void Cleanup() {
        if (gpuQuery) {
            PdhCloseQuery(gpuQuery);
            gpuQuery = nullptr;
        }
        usageCounters.clear();
        isInitialized = false;
    }

    bool GetGPUInformation(GPUStats& stats) {
        if (!isInitialized) {
            if (PdhOpenQuery(NULL, NULL, &gpuQuery) != ERROR_SUCCESS) {
                return false;
            }

            // 使用通配符路径：获取所有 GPU 引擎的 Utilization Percentage
            // 这是 Windows 任务管理器底层使用的标准路径
            const wchar_t* gpuPath = L"\\GPU Engine(*)\\Utilization Percentage";

            // 展开通配符，获取所有匹配的实例路径
            DWORD dwPathListSize = 0;
            PdhExpandWildCardPath(NULL, gpuPath, NULL, &dwPathListSize, 0);

            if (dwPathListSize > 0) {
                std::vector<wchar_t> pathList(dwPathListSize);
                if (PdhExpandWildCardPath(NULL, gpuPath, pathList.data(), &dwPathListSize, 0) == ERROR_SUCCESS) {
                    wchar_t* pPath = pathList.data();
                    while (*pPath != L'\0') {
                        PDH_HCOUNTER counter;
                        // 将每一个发现的 GPU 引擎（3D, Copy, Video等）加入查询
                        if (PdhAddCounter(gpuQuery, pPath, NULL, &counter) == ERROR_SUCCESS) {
                            usageCounters.push_back(counter);
                        }
                        pPath += wcslen(pPath) + 1;
                    }
                }
            }

            if (usageCounters.empty()) {
                Cleanup();
                return false;
            }

            PdhCollectQueryData(gpuQuery);
            isInitialized = true;
            Sleep(50); // 初始采样
            PdhCollectQueryData(gpuQuery);
        }

        PDH_STATUS status = PdhCollectQueryData(gpuQuery);
        if (status != ERROR_SUCCESS) return false;

        double maxUsage = 0.0;
        for (auto& counter : usageCounters) {
            PDH_FMT_COUNTERVALUE val;
            if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &val) == ERROR_SUCCESS) {
                // 重点：GPU 使用率不是累加的，而是取所有引擎（如 3D 引擎）中的最大值
                // 这与任务管理器的显示逻辑一致
                if (val.doubleValue > maxUsage) {
                    maxUsage = val.doubleValue;
                }
            }
        }

        stats.usagePercentage = (maxUsage > 100.0) ? 100.0 : maxUsage;
        return true;
    }

    double GetGPUUsage() {
        GPUStats s;
        return GetGPUInformation(s) ? s.usagePercentage : -1.0;
    }

} // namespace g_infoGPU