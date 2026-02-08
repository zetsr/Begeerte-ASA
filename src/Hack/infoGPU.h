#ifndef INFO_GPU_H
#define INFO_GPU_H

namespace g_infoGPU {

    struct GPUStats {
        double usagePercentage; // GPU 利用率 (0-100)
    };

    // 开箱即用
    bool GetGPUInformation(GPUStats& stats);

    double GetGPUUsage();

} // namespace g_infoGPU

#endif // INFO_GPU_H