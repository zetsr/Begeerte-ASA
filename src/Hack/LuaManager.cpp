#include "LuaManager.h"
#include <algorithm>
#include <iostream>

void LuaManager::Initialize(const std::string& scriptDir) {
    if (scriptDir.empty()) return;

    try {
        fs::path p = fs::absolute(scriptDir);
        m_scriptDir = p.string();
        if (!fs::exists(m_scriptDir)) fs::create_directories(m_scriptDir);
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "LuaManager: failed: " << e.what() << std::endl;
        return;
    }

    InitVM();
    RefreshFileList();
}

void LuaManager::BindImGui() {
    if (!m_lua) return;
    auto imgui = m_lua->create_named_table("imgui");

    imgui.set_function("Color", [](int r, int g, int b, int a) { return IM_COL32(r, g, b, a); });

    imgui.set_function("AddText", [](float posX, float posY, ImU32 col, const char* text) {
        ImGui::GetBackgroundDrawList()->AddText(ImVec2(posX, posY), col, text);
        });

    imgui.set_function("AddLine", [](float x1, float y1, float x2, float y2, ImU32 col, float thickness) {
        ImGui::GetBackgroundDrawList()->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), col, thickness);
        });

    imgui.set_function("AddRect", [](float x1, float y1, float x2, float y2, ImU32 col, float rounding, float thickness) {
        ImGui::GetBackgroundDrawList()->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding, 0, thickness);
        });

    imgui.set_function("AddCircle", [](float x, float y, float radius, ImU32 col, int segments, float thickness) {
        ImGui::GetBackgroundDrawList()->AddCircle(ImVec2(x, y), radius, col, segments, thickness);
        });

    imgui.set_function("AddRectFilled", [](float x1, float y1, float x2, float y2, ImU32 col, float rounding) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(x1, y1), ImVec2(x2, y2), col, rounding);
        });
}

void LuaManager::InitVM() {
    for (auto& script : m_scripts) {
        script.env = sol::nil;
    }

    m_lua.reset(new sol::state());

    m_lua->open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string);
    BindImGui();
}

void LuaManager::Shutdown() {
    std::lock_guard<std::mutex> lock(m_luaMutex);
    for (auto& s : m_scripts) s.env = sol::nil;
    m_scripts.clear();
    m_lua.reset();
}

void LuaManager::SetScriptState(int index, bool load) {
    if (index < 0 || index >= (int)m_scripts.size()) return;
    m_scripts[index].isLoaded = load;
    m_needsReload = true;
}

void LuaManager::Update() {
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
    for (auto& script : m_scripts) {
        script.env = sol::nil;
        script.hasError = false;
        script.lastError.clear();
    }
    m_lua->collect_garbage();
    for (auto& script : m_scripts) {
        if (script.isLoaded) ExecuteScript(script);
    }
}

void LuaManager::ReloadAll() {
    m_pendingReset = true;
}

void LuaManager::ExecuteScript(LuaScript& script) {
    if (!m_lua) return;

    sol::table env_table = m_lua->create_table();
    sol::table env_mt = m_lua->create_table();
    env_mt["__index"] = m_lua->globals();
    env_table[sol::metatable_key] = env_mt;
    script.env = sol::environment(*m_lua, env_table);

    auto load_result = m_lua->load_file(script.path.string());
    if (!load_result.valid()) {
        sol::error err = load_result;
        script.hasError = true;
        script.lastError = "Load Error: " + std::string(err.what());
        return;
    }

    sol::protected_function script_func = load_result;
    script.env.set_on(script_func);

    auto exec_result = script_func();
    if (!exec_result.valid()) {
        sol::error err = exec_result;
        script.hasError = true;
        script.lastError = "Exec Error: " + std::string(err.what());
    }
}

void LuaManager::RefreshFileList() {
    if (m_scriptDir.empty()) { m_scripts.clear(); return; }
    fs::path dirPath = fs::absolute(m_scriptDir);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) return;

    std::vector<fs::path> currentFiles;
    for (const auto& entry : fs::directory_iterator(dirPath)) {
        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
            currentFiles.push_back(entry.path());
        }
    }

    m_scripts.erase(std::remove_if(m_scripts.begin(), m_scripts.end(), [&](const LuaScript& s) {
        return std::find_if(currentFiles.begin(), currentFiles.end(), [&](const fs::path& p) {
            return fs::equivalent(p, s.path);
            }) == currentFiles.end();
        }), m_scripts.end());

    for (const auto& filePath : currentFiles) {
        auto it = std::find_if(m_scripts.begin(), m_scripts.end(), [&](const LuaScript& s) {
            return fs::equivalent(filePath, s.path);
            });
        if (it == m_scripts.end()) {
            LuaScript newScript;
            newScript.name = filePath.filename().string();
            newScript.path = filePath;
            m_scripts.push_back(std::move(newScript));
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
            }
        }
    }
}