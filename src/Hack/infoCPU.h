#ifndef INFO_CPU_H
#define INFO_CPU_H

namespace g_infoCPU {

    struct CPUStats {
        double usagePercentage; // CPU 总使用率 (0-100)
        double frequencyMHz;    // 动态实时频率 (MHz)
    };

    // 开箱即用：直接调用即可获取数据，内部自动处理初始化
    bool GetCPUInformation(CPUStats& stats);

    // 辅助函数
    double GetTotalUsage();
    double GetAverageFrequency();

} // namespace g_infoCPU

#endif // INFO_CPU_H