#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <memory>

#include "../Minimal-D3D12-Hook-ImGui-1.0.2/Main/mdx12_api.h"

#define SOL_ALL_SAFETIES_ON 1
#include "../sol2/sol.hpp"

namespace fs = std::filesystem;

struct LuaScript {
    std::string name;
    fs::path path;
    bool isLoaded = false;
    bool hasError = false;
    std::string lastError;

    sol::environment env;
};

class LuaManager {
public:
    static LuaManager& Get() {
        static LuaManager instance;
        return instance;
    }

    void Initialize(const std::string& scriptDir);
    void Shutdown();
    void ReloadAll();
    void RefreshFileList();
    std::vector<LuaScript>& GetScripts() { return m_scripts; }
    bool SetScriptState(int index, bool load);

    lua_State* GetState() { return m_lua ? m_lua->lua_state() : nullptr; }
    const std::string& GetScriptDir() const { return m_scriptDir; }

    void Lua_OnPaint();
    void Update();
    void ActualReloadAll();

private:
    bool m_needsReload = false;
    std::mutex m_luaMutex;
    std::atomic<bool> m_pendingReset{ false };

    LuaManager() = default;
    ~LuaManager() { Shutdown(); }

    std::unique_ptr<sol::state> m_lua;
    std::string m_scriptDir;
    std::vector<LuaScript> m_scripts;

    void InitVM();
    void BindImGui();
    void BindSDK();
    void BindSystem();
    bool ExecuteScript(LuaScript& script);
};