#ifndef MM_API_H
#define MM_API_H

/*
*
* 如果旧工程仍然 #include "ManualMapInjector.h"，则需要处理 ManualMapInjector.h
* API.h 负责暴露所有外部 API 的声明 — 不在此处实现逻辑
* 包含必要的模块的头文件（Config/Util/Shellcode）以保证类型可见
*
*/

#include "Config.h"
#include "Util.h"
#include "Shellcode.h"

namespace ManualMapInjector {

    bool ManualMapInject(DWORD targetPID, BYTE* dllBuffer, size_t fileSize);

}

#endif