#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

struct ConfigFile {
    std::string name;
    fs::path path;
};

class ConfigManager {
public:
    static ConfigManager& Get() {
        static ConfigManager instance;
        return instance;
    }

    void Initialize(const std::string& configDir);
    void RefreshFileList();
    std::vector<ConfigFile>& GetConfigs() { return m_configs; }
    const std::string& GetConfigDir() const { return m_configDir; }

    bool SaveConfig(const std::string& filename);
    bool LoadConfig(const std::string& filename);
    bool CreateConfig(const std::string& name);
    static bool IsValidConfigName(const std::string& name);

private:
    ConfigManager() = default;
    ~ConfigManager() = default;

    std::string m_configDir;
    std::vector<ConfigFile> m_configs;

    template<typename T>
    void WriteValue(std::ofstream& file, const std::string& key, const T& value);
    void WriteColorArray(std::ofstream& file, const std::string& key, const float* color);
    void WriteCharArray(std::ofstream& file, const std::string& key, const char* str, size_t maxLen);
    bool ReadValue(const std::string& value, bool& out);
    bool ReadValue(const std::string& value, float& out);
    bool ReadValue(const std::string& value, char* out, size_t maxLen);
    bool ReadColorArray(const std::string& value, float* color);

    static std::string Trim(const std::string& str);
};

#define CONFIG_REGISTER_BEGIN() namespace ConfigRegistry { \
    inline void SaveAllConfigs(std::ofstream& file) {

#define CONFIG_BOOL(var) \
    ConfigManager::Get().WriteValue(file, #var, var);

#define CONFIG_FLOAT(var) \
    ConfigManager::Get().WriteValue(file, #var, var);

#define CONFIG_COLOR(var) \
    ConfigManager::Get().WriteColorArray(file, #var, var);

#define CONFIG_STRING(var, size) \
    ConfigManager::Get().WriteCharArray(file, #var, var, size);

#define CONFIG_REGISTER_END() } \
    inline void LoadAllConfigs(const std::unordered_map<std::string, std::string>& data) { \
        (void)data; \
    } }

#define CONFIG_LOAD_BEGIN() namespace ConfigRegistry { \
    inline void LoadAllConfigs(const std::unordered_map<std::string, std::string>& data) {

#define LOAD_BOOL(var) \
    { auto it = data.find(#var); \
      if (it != data.end()) ConfigManager::Get().ReadValue(it->second, var); }

#define LOAD_FLOAT(var) \
    { auto it = data.find(#var); \
      if (it != data.end()) ConfigManager::Get().ReadValue(it->second, var); }

#define LOAD_COLOR(var) \
    { auto it = data.find(#var); \
      if (it != data.end()) ConfigManager::Get().ReadColorArray(it->second, var); }

#define LOAD_STRING(var, size) \
    { auto it = data.find(#var); \
      if (it != data.end()) ConfigManager::Get().ReadValue(it->second, var, size); }

#define CONFIG_LOAD_END() } }
