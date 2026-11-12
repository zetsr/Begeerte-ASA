#include "../../external/Minimal-D3D12-Hook-ImGui/Main/mdx12_api.h"
#include "../../external/CppSDK/SDK.hpp"
#include "../ESP/ESP.h"
#include "../Util/infoCPU.h"
#include "../Util/infoGPU.h"
#include "../Util/Util.h"
#include "../Log/LogManager.h"
#include "../Config/Configs.h"
#include "LuaManager.h"
#include <algorithm>
#include <iostream>
#include <shellapi.h>
#include <cmath>

std::string LuaManager::HttpRequest(const std::string& url) {
    std::string response;
    HINTERNET hInternet = InternetOpenA("Mozilla/5.0 (Windows NT 10.0; Win64; x64)", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_IGNORE_CERT_CN_INVALID;

        HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
        if (hConnect) {
            char buffer[4096];
            DWORD bytesRead;
            while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                response.append(buffer, bytesRead);
            }
            InternetCloseHandle(hConnect);
        }
        else {
            // 调试用：如果这里返回 0，说明 URL 打开失败
            // printf("Error: %lu\n", GetLastError()); 
        }
        InternetCloseHandle(hInternet);
    }
    return response;
}

void LuaManager::FetchWorkshopScripts() {
    std::thread([this]() {
        // --- 第一阶段：获取文件清单 ---
        std::string json;
        while (true) {
            json = HttpRequest("https://api.github.com/repos/zetsr/Begeerte-ASA/git/trees/main?recursive=1");
            if (!json.empty() && json.find("\"tree\":[") != std::string::npos) break;
            std::this_thread::sleep_for(std::chrono::seconds(1)); // 获取清单失败，死循环重试
        }

        std::vector<LuaScript> workshopList;
        size_t treeArrayStart = json.find("\"tree\":[");
        size_t entryStart = treeArrayStart;

        // --- 第二阶段：解析清单并建立下载任务 ---
        while ((entryStart = json.find("{", entryStart)) != std::string::npos) {
            size_t entryEnd = json.find("}", entryStart);
            if (entryEnd == std::string::npos) break;

            std::string entryBlock = json.substr(entryStart, entryEnd - entryStart);
            entryStart = entryEnd + 1;

            std::string pathKey = "\"path\":\"";
            size_t pPos = entryBlock.find(pathKey);
            if (pPos != std::string::npos) {
                pPos += pathKey.length();
                size_t pEnd = entryBlock.find("\"", pPos);
                std::string fullPath = entryBlock.substr(pPos, pEnd - pPos);

                if (fullPath.find("work_shop/") == 0 && fullPath.find(".lua") != std::string::npos) {
                    LuaScript s;
                    s.isWorkshop = true;
                    s.isLoaded = false;
                    s.hasError = false;

                    // 存储下载地址以便后续死循环下载
                    s.downloadUrl = "https://raw.githubusercontent.com/zetsr/Begeerte-ASA/main/" + fullPath;

                    // 路径逻辑保持不变
                    if (fullPath.find("work_shop/lib/") != std::string::npos) {
                        s.isLibrary = true;
                        size_t lastSlash = fullPath.find_last_of('/');
                        s.name = "work_shop/" + fullPath.substr(lastSlash + 1);
                    }
                    else {
                        s.isLibrary = false;
                        s.name = fullPath;
                    }
                    workshopList.push_back(std::move(s));
                }
            }
        }

        if (workshopList.empty()) return;

        // --- 第三阶段：死循环下载每一个文件，直到全部成功 ---
        for (auto& script : workshopList) {
            while (true) {
                script.scriptContent = HttpRequest(script.downloadUrl);
                if (!script.scriptContent.empty()) {
                    break; // 只有下载成功才跳出此文件的死循环
                }
                std::this_thread::sleep_for(std::chrono::seconds(1)); // 下载失败，1秒后重试
            }
        }

        // --- 第四阶段：全部下载完成后，一次性原子更新到系统 ---
        {
            std::lock_guard<std::mutex> lock(m_luaMutex);

            // 清理旧的 workshop 脚本
            m_scripts.erase(std::remove_if(m_scripts.begin(), m_scripts.end(),
                [](const LuaScript& s) { return s.isWorkshop; }), m_scripts.end());

            // 插入完整下载好的新脚本
            m_scripts.insert(m_scripts.begin(), workshopList.begin(), workshopList.end());

            // 自动加载逻辑
            if (m_enabled) {
                for (auto& s : m_scripts) {
                    if (s.isWorkshop && !s.isLibrary && !s.isLoaded) {
                        if (ExecuteScript(s)) s.isLoaded = true;
                    }
                }
            }
        }
        }).detach();
}

void LuaManager::SetEnabled(bool enabled) {
    if (m_enabled == enabled) return;

    m_enabled = enabled;

    if (!m_enabled) {
        std::lock_guard<std::mutex> lock(m_luaMutex);

        // --- 新增：先触发所有脚本的卸载回调 ---
        for (auto& script : m_scripts) {
            Lua_OnShutDown(script);
        }

        // 1. 将所有脚本状态设为未加载
        for (auto& script : m_scripts) {
            script.isLoaded = false;
            script.env = sol::nil;
            script.hasError = false;
        }
        // 2. 重置整个虚拟机（卸载所有内存占用）
        InitVM();

        // g_LogManager::AddLog(255, 255, 255, 255, "[Lua] 管理器已禁用，所有脚本已卸载");
    }
    else {
        // g_LogManager::AddLog(255, 255, 255, 255, "[Lua] 管理器已开启");
        RefreshFileList(); 
    }
}

void LuaManager::Initialize(const std::string& scriptDir) {
    if (scriptDir.empty()) return;

    try {
        fs::path p = fs::absolute(scriptDir);
        m_scriptDir = p.string();
        if (!fs::exists(m_scriptDir)) fs::create_directories(m_scriptDir);
    }
    catch (const fs::filesystem_error& e) {
        return;
    }

    InitVM();
    RefreshFileList();
}

void LuaManager::Bind_G() {
    if (!m_lua) return;

    sol::function tostring = (*m_lua)["tostring"];

    (*m_lua)["print"] = [this, tostring](sol::variadic_args args) {
        std::string output;
        for (auto it = args.begin(); it != args.end(); ++it) {
            // 使用 tostring(arg) 而不是直接 get<string>
            std::string s = tostring(*it);
            output += s;
            if (it + 1 != args.end()) output += "\t";
        }
        g_LogManager::AddLog(255.0f, 255.0f, 255.0f, 200.0f, output);
    };

    (*m_lua)["printf"] = [this](sol::variadic_args args) {
        if (args.size() == 0) return;

        sol::table string_lib = (*m_lua)["string"];
        sol::function format = string_lib["format"];

        auto result = format.call<sol::optional<std::string>>(sol::as_args(args));

        if (result) {
            g_LogManager::AddLog(255.0f, 255.0f, 255.0f, 200.0f, *result);
        }
        else {
            g_LogManager::AddLog(255.0f, 0.0f, 0.0f, 255.0f, "printf Error: invalid format or arguments");
        }
    };
}

void LuaManager::BindClient() {
    if (!m_lua) return;

    sol::table client = m_lua->create_named_table("Client");

    client.set_function("AddLog", [](float r, float g, float b, float a, const std::string& text) {
        g_LogManager::AddLog(r, g, b, a, text);
        });

    client.set_function("GetMenuColor", []() {
        return std::make_tuple(
            static_cast<int>(std::floor(g_Config::MenuColor[0] * 255.0f)),
            static_cast<int>(std::floor(g_Config::MenuColor[1] * 255.0f)),
            static_cast<int>(std::floor(g_Config::MenuColor[2] * 255.0f)),
            static_cast<int>(std::floor(g_Config::MenuColor[3] * 255.0f))
        );
        });
}

void LuaManager::BindImGui() {
    if (!m_lua) return;

    sol::table imgui = m_lua->create_named_table("ImGui");
    auto dl = []() { return ImGui::GetBackgroundDrawList(); };

    imgui.set_function("Color", [](int r, int g, int b, int a) { return IM_COL32(r, g, b, a); });

    imgui.set_function("GetDeltaTime", []() {
        return ImGui::GetIO().DeltaTime;
        });

    imgui.set_function("GetFPS", []() {
        return ImGui::GetIO().Framerate;
        });

    imgui.set_function("IsMouseDown", [](int button) {
        return ImGui::IsMouseDown(button);
        });

    imgui.set_function("IsKeyDown", [](int key) {
        return ImGui::IsKeyDown((ImGuiKey)key);
        });

    imgui.set_function("GetScreenSize", [](sol::this_state s) {
        sol::state_view lua(s);
        ImVec2 size = ImGui::GetIO().DisplaySize;
        sol::table res = lua.create_table();
        res["x"] = size.x;
        res["y"] = size.y;
        return res;
        });

    imgui.set_function("GetMousePos", [](sol::this_state s) {
        sol::state_view lua(s);
        ImVec2 pos = ImGui::GetIO().MousePos;
        sol::table res = lua.create_table();
        res["x"] = pos.x;
        res["y"] = pos.y;
        return res;
        });

    imgui.set_function("CalcTextSize", [](sol::this_state s, const char* text) {
        sol::state_view lua(s);
        ImVec2 size = ImGui::CalcTextSize(text);
        sol::table res = lua.create_table();
        res["x"] = size.x;
        res["y"] = size.y;
        return res;
        });

    imgui.set_function("AddRect", [dl](float x1, float y1, float x2, float y2, ImU32 col, float rounding, float thickness) {
        dl()->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding, 0, thickness);
        });

    imgui.set_function("AddRectFilled", [dl](float x1, float y1, float x2, float y2, ImU32 col, float rounding) {
        dl()->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding);
        });

    imgui.set_function("AddRectFilledMultiColor", [dl](float x1, float y1, float x2, float y2, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) {
        dl()->AddRectFilledMultiColor(ImVec2(x1, y1), ImVec2(x2, y2), col_upr_left, col_upr_right, col_bot_right, col_bot_left);
        });

    imgui.set_function("AddQuad", [dl](float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, ImU32 col, float thickness) {
        dl()->AddQuad(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col, thickness);
        });

    imgui.set_function("AddQuadFilled", [dl](float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, ImU32 col) {
        dl()->AddQuadFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col);
        });

    imgui.set_function("AddTriangle", [dl](float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col, float thickness) {
        dl()->AddTriangle(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col, thickness);
        });

    imgui.set_function("AddTriangleFilled", [dl](float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col) {
        dl()->AddTriangleFilled(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col);
        });

    imgui.set_function("AddCircle", [dl](float x, float y, float radius, ImU32 col, int segments, float thickness) {
        dl()->AddCircle(ImVec2(x, y), radius, col, segments, thickness);
        });

    imgui.set_function("AddCircleFilled", [dl](float x, float y, float radius, ImU32 col, int segments) {
        dl()->AddCircleFilled(ImVec2(x, y), radius, col, segments);
        });

    imgui.set_function("AddNgon", [dl](float x, float y, float radius, ImU32 col, int segments, float thickness) {
        dl()->AddNgon(ImVec2(x, y), radius, col, segments, thickness);
        });

    imgui.set_function("AddNgonFilled", [dl](float x, float y, float radius, ImU32 col, int segments) {
        dl()->AddNgonFilled(ImVec2(x, y), radius, col, segments);
        });

    imgui.set_function("AddEllipse", [dl](float x, float y, float radius_x, float radius_y, ImU32 col, float rot, int segments, float thickness) {
        dl()->AddEllipse(ImVec2(x, y), ImVec2(radius_x, radius_y), col, rot, segments, thickness);
        });

    imgui.set_function("AddEllipseFilled", [dl](float x, float y, float radius_x, float radius_y, ImU32 col, float rot, int segments) {
        dl()->AddEllipseFilled(ImVec2(x, y), ImVec2(radius_x, radius_y), col, rot, segments);
        });

    imgui.set_function("AddBezierQuadratic", [dl](float x1, float y1, float x2, float y2, float x3, float y3, ImU32 col, float thickness, int segments) {
        dl()->AddBezierQuadratic(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), col, thickness, segments);
        });

    imgui.set_function("AddBezierCubic", [dl](float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, ImU32 col, float thickness, int segments) {
        dl()->AddBezierCubic(ImVec2(x1, y1), ImVec2(x2, y2), ImVec2(x3, y3), ImVec2(x4, y4), col, thickness, segments);
        });

    imgui.set_function("AddLine", [dl](float x1, float y1, float x2, float y2, ImU32 col, float thickness) {
        dl()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, thickness);
        });

    imgui.set_function("AddText", [dl](float x, float y, ImU32 col, const char* text) {
        dl()->AddText(ImVec2(x, y), col, text);
        });
}

void LuaManager::BindSDK() {
    if (!m_lua) return;

    // --- 1. 基础结构 ---
    m_lua->new_usertype<SDK::FVector>("FVector",
        sol::constructors<SDK::FVector(), SDK::FVector(float, float, float)>(),
        "X", &SDK::FVector::X, "Y", &SDK::FVector::Y, "Z", &SDK::FVector::Z
        );

    // --- 注册 UNetConnection 类 ---
    // m_lua->new_usertype<SDK::UNetConnection>("UNetConnection",
        // 绑定我们封装的成员函数
        // "GetFirstIP", &SDK::UNetConnection::GetFirstIP,
        // "GetPort", &SDK::UNetConnection::GetPort
        // );

    // --- 注册 UNetDriver 类 ---
    m_lua->new_usertype<SDK::UNetDriver>("UNetDriver",
        // 绑定 ServerConnection 指针
        "ServerConnection", &SDK::UNetDriver::ServerConnection
        );

    sol::table sdk = m_lua->create_named_table("SDK");
    sdk.set_function("GetNetDriver", []() -> sol::optional<SDK::UNetDriver*> {
        SDK::UWorld* W = SDK::UWorld::GetWorld();
        if (W && W->NetDriver) {
            return W->NetDriver;
        }
        return sol::nullopt;
        });

    sdk.set_function("GetLocalPC", []() {
        SDK::APlayerController* pc = g_Util::GetLocalPC();
        return (pc && pc->Pawn) ? (uintptr_t)pc : 0;
        });

    sdk.set_function("GetActors", [this](sol::this_state s) {
        sol::state_view lua(s);
        sol::table res = lua.create_table();
        SDK::UWorld* W = SDK::UWorld::GetWorld();

        if (W && W->PersistentLevel) {
            int idx = 1;
            SDK::TArray<SDK::AActor*>& actors = W->PersistentLevel->Actors;
            for (int i = 0; i < actors.Num(); i++) {
                SDK::AActor* a = actors[i];
                if (a) {
                    res[idx++] = (uintptr_t)a;
                }
            }
        }
        return res;
        });

    sdk.set_function("GetCharacterClass", []() { return (uintptr_t)SDK::APrimalCharacter::StaticClass(); });
    sdk.set_function("GetDinoClass", []() { return (uintptr_t)SDK::APrimalDinoCharacter::StaticClass(); });
    sdk.set_function("GetDroppedItemClass", []() { return (uintptr_t)SDK::ADroppedItem::StaticClass(); });
    sdk.set_function("GetContainerClass", []() { return (uintptr_t)SDK::APrimalStructureItemContainer::StaticClass(); });
    sdk.set_function("GetTurretClass", []() { return (uintptr_t)SDK::APrimalStructureTurret::StaticClass(); });

    sol::table actor_api = m_lua->create_named_table("Actor");
    actor_api.set_function("IsA", [](uintptr_t a, uintptr_t cls) { return (a && cls) ? ((SDK::AActor*)a)->IsA((SDK::UClass*)cls) : false; });
    actor_api.set_function("GetLocation", [](uintptr_t a) -> sol::optional<SDK::FVector> { return (a) ? sol::make_optional(((SDK::AActor*)a)->K2_GetActorLocation()) : sol::nullopt; });
    actor_api.set_function("GetDistance", [](uintptr_t a, uintptr_t b) { return (a && b) ? ((SDK::AActor*)a)->GetDistanceTo((SDK::AActor*)b) : 0.0f; });
    actor_api.set_function("IsHidden", [](uintptr_t a) { return a ? (bool)((SDK::AActor*)a)->bHidden : true; });
    actor_api.set_function("GetClassName", [](uintptr_t a) -> std::string {
        SDK::AActor* act = (SDK::AActor*)a;
        return (act && act->Class) ? act->Class->GetName() : "";
        });

    sol::table char_api = m_lua->create_named_table("Character");
    char_api.set_function("GetInfo", [](uintptr_t a) {
        SDK::APrimalCharacter* c = (SDK::APrimalCharacter*)a;
        if (!c) return std::make_tuple(0.0f, 0.0f, false, std::string("Unknown"));

        std::string name = c->GetDescriptiveName().ToString();

        if (c->PlayerState) name = c->PlayerState->GetPlayerName().ToString();

        return std::make_tuple(c->GetHealth(), c->GetMaxHealth(), (bool)c->IsDead(), name);
        });

    char_api.set_function("GetExactPing", [](uintptr_t a) {
        float ping = 0.0f;

        SDK::APrimalCharacter* c = (SDK::APrimalCharacter*)a;
        if (!c) return ping;
        if (c->PlayerState) ping = c->PlayerState->GetExactPing();

        return ping;
        });

    char_api.set_function("GetRelation", [](uintptr_t target, uintptr_t local) -> int {
        if (!target || !local) return 0;
        return (int)g_ESP::GetRelation((SDK::APrimalCharacter*)target, (SDK::APrimalCharacter*)local);
        });

    sol::table item_api = m_lua->create_named_table("Item");
    item_api.set_function("GetDroppedInfo", [](uintptr_t a) {
        SDK::ADroppedItem* dropped = (SDK::ADroppedItem*)a;
        if (!dropped || !dropped->MyItem) return std::make_tuple(false, std::string(""), 0, 0.0f, false, std::string(""));

        SDK::UPrimalItem* it = dropped->MyItem;
        std::string name = it->DescriptiveNameBase.ToString();
        if (it->CustomItemName.IsValid() && !it->CustomItemName.ToString().empty()) name = it->CustomItemName.ToString();

        std::string className = it->Class ? it->Class->GetName() : "";
        return std::make_tuple(true, name, it->ItemQuantity, it->ItemRating, (bool)it->bIsBlueprint, className);
        });

    sol::table cont_api = m_lua->create_named_table("Container");
    cont_api.set_function("GetInfo", [](uintptr_t a) {
        SDK::APrimalStructureItemContainer* c = (SDK::APrimalStructureItemContainer*)a;
        std::string name = c ? c->GetDescriptiveName().ToString() : "";
        if (name.empty() || name == "None") name = "Supply Crate";
        return name;
        });

    sol::table pc_api = m_lua->create_named_table("PC");
    pc_api.set_function("GetPawn", [](uintptr_t pc) -> uintptr_t { return pc ? (uintptr_t)((SDK::APlayerController*)pc)->Pawn : 0; });
    pc_api.set_function("ProjectToScreen", [](uintptr_t pc, SDK::FVector worldLoc) -> std::tuple<bool, float, float> {
        SDK::FVector2D screenPos;
        bool ok = pc ? ((SDK::APlayerController*)pc)->ProjectWorldLocationToScreen(worldLoc, &screenPos, false) : false;
        return std::make_tuple(ok, (float)screenPos.X, (float)screenPos.Y);
        });
}

void LuaManager::BindSystem() {
    if (!m_lua) return;
    sol::table system_api = m_lua->create_named_table("System");

    system_api.set_function("GetCPUStats", []() -> std::tuple<double, double> {
        g_infoCPU::CPUStats stats;
        if (g_infoCPU::GetCPUInformation(stats)) {
            // 返回元组：第一个是使用率，第二个是频率
            return std::make_tuple(stats.usagePercentage, stats.frequencyMHz);
        }
        return std::make_tuple(0.0, 0.0);
        });

    system_api.set_function("GetGPUStats", []() -> std::tuple<double, double> {
        g_infoGPU::GPUStats stats;
        if (g_infoGPU::GetGPUInformation(stats)) {
            // 返回元组：第一个是使用率
            return std::make_tuple(stats.usagePercentage, 0.0);
        }
        return std::make_tuple(0.0, 0.0);
        });
}

void LuaManager::InitVM() {
    // 1. 彻底重置
    for (auto& script : m_scripts) { 
        Lua_OnShutDown(script);
        script.env = sol::nil; 
    }

    m_lua.reset(new sol::state());
    m_lua->open_libraries();

    if (!m_lua || !m_lua->lua_state()) return;

    // 2. 备份原始 require (必须存入 registry 或全局，防止被回收)
    sol::protected_function old_require = (*m_lua)["require"];

    // 3. 定义新的 require 逻辑
    auto new_require = [this, old_require](std::string module_name, sol::this_state s) -> sol::object {
        std::lock_guard<std::mutex> lock(this->m_luaMutex);

        // --- 检查缓存 ---
        sol::state_view lua(s);
        sol::table loaded = lua["package"]["loaded"];
        if (loaded[module_name].valid()) {
            return loaded[module_name];
        }

        // --- 内存搜索逻辑 ---
        std::string normalized = module_name;
        std::replace(normalized.begin(), normalized.end(), '.', '/');

        for (auto& script : m_scripts) {
            if (!script.isWorkshop) continue;

            if (script.name == module_name ||
                script.name == (module_name + ".lua") ||
                script.name == normalized ||
                script.name == (normalized + ".lua"))
            {
                // 命中：加载并执行
                auto res = lua.load(script.scriptContent, script.name.c_str());
                if (res.valid()) {
                    sol::protected_function_result pfr = res.call();
                    if (pfr.valid()) {
                        // 如果脚本有 return，就拿 return 的值；否则按照 Lua 惯例返回 true
                        sol::object result = pfr.return_count() > 0 ? pfr.get<sol::object>() : sol::make_object(lua, true);

                        loaded[module_name] = result; // 缓存结果
                        return result;
                    }
                    else {
                        sol::error err = pfr;
                        printf("[Lua Error] %s\n", err.what());
                    }
                }
            }
        }

        // --- 回退逻辑 ---
        // 如果内存没找到，调用原始 require
        auto fallback_res = old_require(module_name);
        if (fallback_res.valid()) {
            return fallback_res.get<sol::object>();
        }

        // 如果还是失败，sol 会自动抛出 Lua 错误（带堆栈的那个）
        return sol::nil;
        };

    // 4. 强制覆盖全局 require
    (*m_lua)["require"] = new_require;

    // 5. 绑定其他 API 
    Bind_G();
    BindClient();
    BindImGui();
    BindSDK();
    BindSystem();
}

void LuaManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    for (auto& s : m_scripts) s.env = sol::nil;
    m_scripts.clear();
    m_lua.reset();
}

bool LuaManager::SetScriptState(int index, bool load) {
    if (index < 0 || index >= (int)m_scripts.size()) return false;
    auto& script = m_scripts[index];

    if (load) {
        if (ExecuteScript(script)) {
            script.isLoaded = true;
            return true;
        }
        else {
            script.isLoaded = false;
            return false;
        }
    }
    else {
        std::lock_guard<std::mutex> lock(m_luaMutex);
        Lua_OnShutDown(script);

        script.isLoaded = false;
        script.env = sol::nil;
        script.hasError = false;
        return true;
    }
}

void LuaManager::Update() {
    if (!m_enabled) return;
    std::lock_guard<std::mutex> lock(m_luaMutex);

    if (m_pendingReset) {
        InitVM();
        for (auto& script : m_scripts) {
            if (script.isLoaded) ExecuteScript(script);
        }
        m_pendingReset = false;
        m_needsReload = false;
        return;
    }

    if (m_needsReload) {
        ActualReloadAll();
        m_needsReload = false;
    }
}

void LuaManager::ActualReloadAll() {
    if (!m_lua) return;

    m_lua->collect_garbage();

    for (auto& script : m_scripts) {
        if (script.isLoaded) {
            ExecuteScript(script);
        }
    }
}

void LuaManager::ReloadAll() {
    m_pendingReset = true;
}

bool LuaManager::ExecuteScript(LuaScript& script) {
    if (!m_lua) return false;

    script.hasError = false;
    script.lastError.clear();

    sol::table env_table = m_lua->create_table();
    sol::table env_mt = m_lua->create_table();
    env_mt["__index"] = m_lua->globals();
    env_table[sol::metatable_key] = env_mt;
    script.env = sol::environment(*m_lua, env_table);

    sol::load_result load_result;
    if (script.isWorkshop) {
        load_result = m_lua->load(script.scriptContent, script.name);
    }
    else {
        load_result = m_lua->load_file(script.path.string());
    }

    if (!load_result.valid()) {
        sol::error err = load_result;
        script.hasError = true;
        script.lastError = "Load Error: " + std::string(err.what());

        std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
        g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
        return false;
    }

    sol::protected_function script_func = load_result;
    script.env.set_on(script_func);

    auto exec_result = script_func();
    if (!exec_result.valid()) {
        sol::error err = exec_result;
        script.hasError = true;
        script.lastError = "Exec Error: " + std::string(err.what());

        std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
        g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
        return false;
    }

    return true;
}

void LuaManager::RefreshFileList() {
    // 0. 安全防御：确保 this 不为空（如果是静态/单例调用）
    if (this == nullptr) return;

    std::lock_guard<std::mutex> lock(m_luaMutex);

    std::vector<LuaScript> nextList;

    // 1. 保留 Workshop 脚本
    for (auto& s : m_scripts) {
        if (s.isWorkshop) {
            nextList.push_back(std::move(s));
        }
    }

    // 2. 使用 std::error_code 避免 filesystem 抛出异常导致崩溃
    std::error_code ec;
    if (!m_scriptDir.empty() && fs::exists(m_scriptDir, ec) && !ec) {

        // 使用带 ec 的 directory_iterator，防止权限等问题抛异常
        fs::directory_iterator dirIt(m_scriptDir, ec);
        if (!ec) {
            for (const auto& entry : dirIt) {
                // 判断是否为 .lua 普通文件
                if (entry.is_regular_file(ec) && entry.path().extension() == ".lua") {
                    fs::path fullPath = fs::absolute(entry.path(), ec);
                    if (ec) continue;

                    // 查找旧列表中是否存在该文件（只在未被 move 的非 workshop 脚本中找）
                    auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&](const LuaScript& old) {
                        // 避开已移动或 Workshop 的对象
                        return !old.isWorkshop && !old.path.empty() && fs::absolute(old.path, ec) == fullPath;
                        });

                    if (it != m_scripts.end()) {
                        nextList.push_back(std::move(*it));
                        // 移动后清空 old.path，标记该节点已被取走，避免重复匹配
                        it->path.clear();
                    }
                    else {
                        LuaScript s;
                        s.name = entry.path().filename().string();
                        s.path = fullPath;
                        s.isWorkshop = false;
                        s.isLoaded = false;
                        s.hasError = false;
                        nextList.push_back(std::move(s));
                    }
                }
            }
        }
    }

    // 3. 更新列表
    m_scripts = std::move(nextList);
}

void LuaManager::Lua_OnShutDown(LuaScript& script) {
    // 必须确保环境有效且脚本已加载
    if (!script.isLoaded || !script.env.valid()) return;

    sol::object shutdownObj = script.env["OnShutDown"];
    if (shutdownObj.is<sol::protected_function>()) {
        sol::protected_function shutdownFunc = shutdownObj;

        // 使用同步调用。在 Lua 脚本执行完 OnShutDown 之前，C++ 线程会阻塞在这里
        // 这保证了脚本提交指令的逻辑能够完整运行
        auto result = shutdownFunc();

        if (!result.valid()) {
            sol::error err = result;
            // 卸载时的错误通常只记录到日志，不打断销毁流程
            std::string errorMsg = "[ShutDown Error] " + script.name + ": " + std::string(err.what());
            g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
        }
    }
}

void LuaManager::Lua_OnPostRender() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object PostRenderObj = script.env["OnPostRender"];
        if (PostRenderObj.is<sol::protected_function>()) {
            sol::protected_function PostRenderFunc = PostRenderObj;
            auto result = PostRenderFunc();
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnConsoleMessage(std::string Message) {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object ConsoleMessageObj = script.env["OnConsoleMessage"];
        if (ConsoleMessageObj.is<sol::protected_function>()) {
            sol::protected_function ConsoleMessageFunc = ConsoleMessageObj;
            auto result = ConsoleMessageFunc(Message);
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnDisconnect(std::string ServerIP, int32_t ServerPort) {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object DisconnectObj = script.env["OnDisconnect"];
        if (DisconnectObj.is<sol::protected_function>()) {
            sol::protected_function DisconnectFunc = DisconnectObj;
            auto result = DisconnectFunc(ServerIP, ServerPort);
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnWorldTick() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object WorldTickObj = script.env["OnWorldTick"];
        if (WorldTickObj.is<sol::protected_function>()) {
            sol::protected_function WorldTickFunc = WorldTickObj;
            auto result = WorldTickFunc();
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnPaintMenu(float MenuAlpha) {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object drawMenuObj = script.env["OnPaintMenu"];
        if (drawMenuObj.is<sol::protected_function>()) {
            sol::protected_function drawMenuFunc = drawMenuObj;
            auto result = drawMenuFunc(MenuAlpha);
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnPaint() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object drawObj = script.env["OnPaint"];
        if (drawObj.is<sol::protected_function>()) {
            sol::protected_function drawFunc = drawObj;
            auto result = drawFunc();
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}

void LuaManager::Lua_OnMenuOpen() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    if (!m_lua || !m_lua->lua_state()) return;

    for (auto& script : m_scripts) {
        if (!script.isLoaded || script.hasError || !script.env.valid()) continue;

        sol::object MenuOpenObj = script.env["OnMenuOpen"];
        if (MenuOpenObj.is<sol::protected_function>()) {
            sol::protected_function MenuOpenFunc = MenuOpenObj;
            auto result = MenuOpenFunc();
            if (!result.valid()) {
                sol::error err = result;
                script.hasError = true;
                script.lastError = "Runtime Error: " + std::string(err.what());

                std::string errorMsg = "[!] " + script.name + ": " + std::string(err.what());
                g_LogManager::AddLog(255.f, 50.f, 50.f, 255.f, errorMsg);
            }
        }
    }
}