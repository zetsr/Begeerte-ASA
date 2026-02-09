#include "ConfigManager.h"
#include "Configs.h"
#include <regex>

void ConfigManager::Initialize(const std::string& configDir) {
    if (configDir.empty()) return;

    try {
        fs::path p = fs::absolute(configDir);
        m_configDir = p.string();
        if (!fs::exists(m_configDir)) {
            fs::create_directories(m_configDir);
        }
    }
    catch (const fs::filesystem_error&) {
        return;
    }

    RefreshFileList();
}

void ConfigManager::RefreshFileList() {
    if (m_configDir.empty()) {
        m_configs.clear();
        return;
    }

    fs::path dirPath = fs::absolute(m_configDir);
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) return;

    std::vector<fs::path> currentFiles;
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".ini") {
                currentFiles.push_back(fs::absolute(entry.path()));
            }
        }
    }
    catch (const fs::filesystem_error&) {
        return;
    }

    m_configs.erase(std::remove_if(m_configs.begin(), m_configs.end(), [&](const ConfigFile& c) {
        return std::find(currentFiles.begin(), currentFiles.end(), c.path) == currentFiles.end();
    }), m_configs.end());

    for (const auto& filePath : currentFiles) {
        auto it = std::find_if(m_configs.begin(), m_configs.end(), [&](const ConfigFile& c) {
            return c.path == filePath;
        });

        if (it == m_configs.end()) {
            ConfigFile newConfig;
            newConfig.name = filePath.filename().string();
            newConfig.path = filePath;
            m_configs.push_back(std::move(newConfig));
        }
    }
}

bool ConfigManager::IsValidConfigName(const std::string& name) {
    if (name.empty() || name.length() > 16) return false;
    std::regex validPattern("^[a-zA-Z0-9_-]+$");
    if (!std::regex_match(name, validPattern)) return false;
    if (name[0] == '.') return false;

    return true;
}

bool ConfigManager::CreateConfig(const std::string& name) {
    if (!IsValidConfigName(name)) return false;

    try {
        fs::path configPath = fs::path(m_configDir) / (name + ".ini");
        if (fs::exists(configPath)) return false;
        std::ofstream file(configPath);
        if (!file.is_open()) return false;
        
        file << "# Configuration File: " << name << "\n";
        file << "# Auto-generated\n\n";
        file.close();

        RefreshFileList();
        return true;
    }
    catch (...) {
        return false;
    }
}

bool ConfigManager::SaveConfig(const std::string& filename) {
    try {
        fs::path configPath = fs::path(m_configDir) / filename;
        std::ofstream file(configPath);
        if (!file.is_open()) return false;

        file << "# Configuration File\n";
        file << "# Format: key=value\n\n";

        // 生物列表
        file << "[EntityList]\n";
        CONFIG_STRING(g_Config::entitySearchBuf, 256);
        CONFIG_BOOL(g_Config::bEnableFilter);
        file << "\n";

        // 建筑列表
        file << "[StructureList]\n";
        CONFIG_STRING(g_Config::structureSearchBuf, 256);
        CONFIG_BOOL(g_Config::bEnableStructureFilter);
        file << "\n";

        // 自瞄
        file << "[Aimbot]\n";
        CONFIG_BOOL(g_Config::bAimbotEnabled);
        CONFIG_FLOAT(g_Config::AimbotFOV);
        CONFIG_FLOAT(g_Config::AimbotSmooth);
        file << "\n";

        // 扳机
        file << "[Triggerbot]\n";
        CONFIG_BOOL(g_Config::bTriggerbotEnabled);
        CONFIG_FLOAT(g_Config::TriggerDelay);
        CONFIG_FLOAT(g_Config::TriggerRandomPercent);
        CONFIG_FLOAT(g_Config::TriggerHitChance);
        file << "\n";

        // 掉落物
        file << "[DroppedItems]\n";
        CONFIG_BOOL(g_Config::bDrawDroppedItems);
        CONFIG_FLOAT(g_Config::DroppedItemMaxDistance);
        CONFIG_COLOR(g_Config::DroppedItemNameColor);
        CONFIG_COLOR(g_Config::DroppedItemDistanceColor);
        CONFIG_COLOR(g_Config::DroppedItemMeatColor);
        CONFIG_COLOR(g_Config::DroppedItemCryopodColor);
        CONFIG_COLOR(g_Config::DroppedItemPiledColor);
        file << "\n";

        // 宝箱
        file << "[SupplyDrops]\n";
        CONFIG_BOOL(g_Config::bDrawSupplyDrops);
        CONFIG_FLOAT(g_Config::SupplyDropMaxDistance);
        file << "\n";

        // 建筑
        file << "[Structures]\n";
        CONFIG_BOOL(g_Config::bDrawStructures);
        CONFIG_FLOAT(g_Config::StructureMaxDistance);
        CONFIG_COLOR(g_Config::StructureNameColor);
        CONFIG_COLOR(g_Config::StructureOwnerColor);
        CONFIG_COLOR(g_Config::StructureDistanceColor);
        file << "\n";

        // 水源
        file << "[Water]\n";
        CONFIG_BOOL(g_Config::bDrawWater);
        CONFIG_FLOAT(g_Config::WaterMaxDistance);
        CONFIG_COLOR(g_Config::WaterNameColor);
        CONFIG_COLOR(g_Config::WaterDistanceColor);
        file << "\n";

        // 全局
        file << "[Global]\n";
        CONFIG_BOOL(g_Config::bDrawBox);
        CONFIG_COLOR(g_Config::BoxColor);
        CONFIG_BOOL(g_Config::bDrawHealthBar);
        CONFIG_BOOL(g_Config::bDrawName);
        CONFIG_COLOR(g_Config::NameColor);
        CONFIG_BOOL(g_Config::bDrawSpecies);
        CONFIG_BOOL(g_Config::bDrawGrowth);
        CONFIG_BOOL(g_Config::bDrawTorpor);
        CONFIG_COLOR(g_Config::TorporColor);
        CONFIG_BOOL(g_Config::bDrawRagdoll);
        CONFIG_COLOR(g_Config::RagdollColor);
        CONFIG_BOOL(g_Config::bDrawDistance);
        CONFIG_COLOR(g_Config::DistanceColor);
        file << "\n";

        // 队友
        file << "[Team]\n";
        CONFIG_BOOL(g_Config::bDrawBoxTeam);
        CONFIG_COLOR(g_Config::BoxColorTeam);
        CONFIG_BOOL(g_Config::bDrawHealthBarTeam);
        CONFIG_BOOL(g_Config::bDrawNameTeam);
        CONFIG_COLOR(g_Config::NameColorTeam);
        CONFIG_BOOL(g_Config::bDrawSpeciesTeam);
        CONFIG_BOOL(g_Config::bDrawGrowthTeam);
        CONFIG_BOOL(g_Config::bDrawTorporTeam);
        CONFIG_COLOR(g_Config::TorporColorTeam);
        CONFIG_BOOL(g_Config::bDrawRagdollTeam);
        CONFIG_COLOR(g_Config::RagdollColorTeam);
        CONFIG_BOOL(g_Config::bDrawDistanceTeam);
        CONFIG_COLOR(g_Config::DistanceColorTeam);
        file << "\n";

        // OOF
        file << "[OOF]\n";
        CONFIG_BOOL(g_Config::bEnableOOF);
        CONFIG_COLOR(g_Config::OOFColor);
        CONFIG_FLOAT(g_Config::OOFRadius);
        CONFIG_FLOAT(g_Config::OOFSize);
        CONFIG_FLOAT(g_Config::OOFBreathSpeed);
        CONFIG_FLOAT(g_Config::OOFMinAlpha);
        CONFIG_FLOAT(g_Config::OOFMaxAlpha);
        file << "\n";

        file.close();
        return true;
    }
    catch (...) {
        return false;
    }
}

bool ConfigManager::LoadConfig(const std::string& filename) {
    try {
        fs::path configPath = fs::path(m_configDir) / filename;
        std::ifstream file(configPath);
        if (!file.is_open()) return false;

        std::unordered_map<std::string, std::string> data;
        std::string line;

        while (std::getline(file, line)) {
            line = Trim(line);

            if (line.empty() || line[0] == '#' || line[0] == '[') continue;
            size_t pos = line.find('=');
            if (pos == std::string::npos) continue;

            std::string key = Trim(line.substr(0, pos));
            std::string value = Trim(line.substr(pos + 1));

            if (!key.empty()) {
                data[key] = value;
            }
        }

        file.close();
        // 生物列表
        LOAD_STRING(g_Config::entitySearchBuf, 256);
        LOAD_BOOL(g_Config::bEnableFilter);

        // 建筑列表
        LOAD_STRING(g_Config::structureSearchBuf, 256);
        LOAD_BOOL(g_Config::bEnableStructureFilter);
        
        // 自瞄
        LOAD_BOOL(g_Config::bAimbotEnabled);
        LOAD_FLOAT(g_Config::AimbotFOV);
        LOAD_FLOAT(g_Config::AimbotSmooth);
        
        // 扳机
        LOAD_BOOL(g_Config::bTriggerbotEnabled);
        LOAD_FLOAT(g_Config::TriggerDelay);
        LOAD_FLOAT(g_Config::TriggerRandomPercent);
        LOAD_FLOAT(g_Config::TriggerHitChance);
        
        // 掉落物
        LOAD_BOOL(g_Config::bDrawDroppedItems);
        LOAD_FLOAT(g_Config::DroppedItemMaxDistance);
        LOAD_COLOR(g_Config::DroppedItemNameColor);
        LOAD_COLOR(g_Config::DroppedItemDistanceColor);
        LOAD_COLOR(g_Config::DroppedItemMeatColor);
        LOAD_COLOR(g_Config::DroppedItemCryopodColor);
        LOAD_COLOR(g_Config::DroppedItemPiledColor);

        // 宝箱
        LOAD_BOOL(g_Config::bDrawSupplyDrops);
        LOAD_FLOAT(g_Config::SupplyDropMaxDistance);
        
        // 建筑
        LOAD_BOOL(g_Config::bDrawStructures);
        LOAD_FLOAT(g_Config::StructureMaxDistance);
        LOAD_COLOR(g_Config::StructureNameColor);
        LOAD_COLOR(g_Config::StructureOwnerColor);
        LOAD_COLOR(g_Config::StructureDistanceColor);

        // 水源
        LOAD_BOOL(g_Config::bDrawWater);
        LOAD_FLOAT(g_Config::WaterMaxDistance);
        LOAD_COLOR(g_Config::WaterNameColor);
        LOAD_COLOR(g_Config::WaterDistanceColor);

        // 全局
        LOAD_BOOL(g_Config::bDrawBox);
        LOAD_COLOR(g_Config::BoxColor);
        LOAD_BOOL(g_Config::bDrawHealthBar);
        LOAD_BOOL(g_Config::bDrawName);
        LOAD_COLOR(g_Config::NameColor);
        LOAD_BOOL(g_Config::bDrawSpecies);
        LOAD_BOOL(g_Config::bDrawGrowth);
        LOAD_BOOL(g_Config::bDrawTorpor);
        LOAD_COLOR(g_Config::TorporColor);
        LOAD_BOOL(g_Config::bDrawRagdoll);
        LOAD_COLOR(g_Config::RagdollColor);
        LOAD_BOOL(g_Config::bDrawDistance);
        LOAD_COLOR(g_Config::DistanceColor);
        
        // 队友
        LOAD_BOOL(g_Config::bDrawBoxTeam);
        LOAD_COLOR(g_Config::BoxColorTeam);
        LOAD_BOOL(g_Config::bDrawHealthBarTeam);
        LOAD_BOOL(g_Config::bDrawNameTeam);
        LOAD_COLOR(g_Config::NameColorTeam);
        LOAD_BOOL(g_Config::bDrawSpeciesTeam);
        LOAD_BOOL(g_Config::bDrawGrowthTeam);
        LOAD_BOOL(g_Config::bDrawTorporTeam);
        LOAD_COLOR(g_Config::TorporColorTeam);
        LOAD_BOOL(g_Config::bDrawRagdollTeam);
        LOAD_COLOR(g_Config::RagdollColorTeam);
        LOAD_BOOL(g_Config::bDrawDistanceTeam);
        LOAD_COLOR(g_Config::DistanceColorTeam);
        
        // OOF
        LOAD_BOOL(g_Config::bEnableOOF);
        LOAD_COLOR(g_Config::OOFColor);
        LOAD_FLOAT(g_Config::OOFRadius);
        LOAD_FLOAT(g_Config::OOFSize);
        LOAD_FLOAT(g_Config::OOFBreathSpeed);
        LOAD_FLOAT(g_Config::OOFMinAlpha);
        LOAD_FLOAT(g_Config::OOFMaxAlpha);

        return true;
    }
    catch (...) {
        return false;
    }
}

template<typename T>
void ConfigManager::WriteValue(std::ofstream& file, const std::string& key, const T& value) {
    file << key << "=" << value << "\n";
}

void ConfigManager::WriteColorArray(std::ofstream& file, const std::string& key, const float* color) {
    file << key << "=" << color[0] << "," << color[1] << "," << color[2] << "," << color[3] << "\n";
}

void ConfigManager::WriteCharArray(std::ofstream& file, const std::string& key, const char* str, size_t maxLen) {
    std::string safe_str;
    for (size_t i = 0; i < maxLen && str[i] != '\0'; ++i) {
        char c = str[i];
        if (c == '\n') safe_str += "\\n";
        else if (c == '\r') safe_str += "\\r";
        else if (c == '\\') safe_str += "\\\\";
        else safe_str += c;
    }
    file << key << "=" << safe_str << "\n";
}

bool ConfigManager::ReadValue(const std::string& value, bool& out) {
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "true" || lower == "1") {
        out = true;
        return true;
    }
    else if (lower == "false" || lower == "0") {
        out = false;
        return true;
    }
    return false;
}

bool ConfigManager::ReadValue(const std::string& value, float& out) {
    try {
        out = std::stof(value);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool ConfigManager::ReadValue(const std::string& value, char* out, size_t maxLen) {
    if (value.length() >= maxLen) return false;
    
    size_t outIdx = 0;
    for (size_t i = 0; i < value.length() && outIdx < maxLen - 1; ++i) {
        if (value[i] == '\\' && i + 1 < value.length()) {
            if (value[i + 1] == 'n') {
                out[outIdx++] = '\n';
                ++i;
            }
            else if (value[i + 1] == 'r') {
                out[outIdx++] = '\r';
                ++i;
            }
            else if (value[i + 1] == '\\') {
                out[outIdx++] = '\\';
                ++i;
            }
            else {
                out[outIdx++] = value[i];
            }
        }
        else {
            out[outIdx++] = value[i];
        }
    }
    out[outIdx] = '\0';
    return true;
}

bool ConfigManager::ReadColorArray(const std::string& value, float* color) {
    try {
        std::stringstream ss(value);
        std::string token;
        int idx = 0;
        
        while (std::getline(ss, token, ',') && idx < 4) {
            color[idx++] = std::stof(Trim(token));
        }
        
        return idx == 4;
    }
    catch (...) {
        return false;
    }
}

std::string ConfigManager::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}
