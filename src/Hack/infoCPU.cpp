#include "infoCPU.h"
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

// 链接 PDH 库
#pragma comment(lib, "pdh.lib")

namespace g_infoCPU {

    // 内部私有变量，通过静态生存期管理
    static PDH_HQUERY cpuQuery = nullptr;
    static PDH_HCOUNTER usageCounter = nullptr;
    static PDH_HCOUNTER baseFreqCounter = nullptr;
    static PDH_HCOUNTER percPerfCounter = nullptr;
    static bool isInitialized = false;

    bool GetCPUInformation(CPUStats& stats) {
        // --- 自动初始化逻辑 ---
        if (!isInitialized) {
            if (PdhOpenQuery(NULL, NULL, &cpuQuery) != ERROR_SUCCESS) {
                return false;
            }

            // 1. 使用率计数器
            PDH_STATUS s1 = PdhAddCounter(cpuQuery, L"\\Processor Information(_Total)\\% Processor Utility", NULL, &usageCounter);
            // 2. 基准频率计数器
            PDH_STATUS s2 = PdhAddCounter(cpuQuery, L"\\Processor Information(_Total)\\Processor Frequency", NULL, &baseFreqCounter);
            // 3. 性能百分比计数器 (用于计算动态加速/睿频)
            PDH_STATUS s3 = PdhAddCounter(cpuQuery, L"\\Processor Information(_Total)\\% Processor Performance", NULL, &percPerfCounter);

            if (s1 != ERROR_SUCCESS || s2 != ERROR_SUCCESS || s3 != ERROR_SUCCESS) {
                return false;
            }

            // 预热收集：某些计数器第一次收集时没有差值，返回值为 0
            PdhCollectQueryData(cpuQuery);
            isInitialized = true;

            // 预热后短暂休眠或再次收集，确保第一次调用就有意义
            Sleep(50);
            PdhCollectQueryData(cpuQuery);
        }

        // --- 数据采集逻辑 ---
        PDH_STATUS status = PdhCollectQueryData(cpuQuery);
        if (status != ERROR_SUCCESS) return false;

        PDH_FMT_COUNTERVALUE usageVal, baseFreqVal, percPerfVal;

        // 获取 CPU 使用率
        if (PdhGetFormattedCounterValue(usageCounter, PDH_FMT_DOUBLE, NULL, &usageVal) == ERROR_SUCCESS) {
            stats.usagePercentage = usageVal.doubleValue;
        }
        else {
            stats.usagePercentage = 0.0;
        }

        // 获取基准频率和性能百分比
        PDH_STATUS resBase = PdhGetFormattedCounterValue(baseFreqCounter, PDH_FMT_DOUBLE, NULL, &baseFreqVal);
        PDH_STATUS resPerf = PdhGetFormattedCounterValue(percPerfCounter, PDH_FMT_DOUBLE, NULL, &percPerfVal);

        if (resBase == ERROR_SUCCESS && resPerf == ERROR_SUCCESS) {
            // 动态频率 = 基准频率 * 当前性能百分比
            // 睿频时 percPerfVal.doubleValue 会大于 100.0
            stats.frequencyMHz = (baseFreqVal.doubleValue * percPerfVal.doubleValue) / 100.0;
        }
        else {
            stats.frequencyMHz = 0.0;
        }

        return true;
    }

    double GetTotalUsage() {
        CPUStats s;
        return GetCPUInformation(s) ? s.usagePercentage : -1.0;
    }

    double GetAverageFrequency() {
        CPUStats s;
        return GetCPUInformation(s) ? s.frequencyMHz : -1.0;
    }

} // namespace g_infoCPU