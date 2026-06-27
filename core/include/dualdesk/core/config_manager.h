#pragma once

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

namespace dualdesk {

class ConfigManager {
public:
    ConfigManager() = default;
    ~ConfigManager() = default;

    bool Load(const std::string& path) {
        try {
            std::ifstream file(path);
            file >> config_;
            return true;
        } catch (...) {
            return false;
        }
    }

    bool Save(const std::string& path) {
        try {
            std::ofstream file(path);
            file << config_.dump(4);
            return true;
        } catch (...) {
            return false;
        }
    }

    template<typename T>
    T Get(const std::string& key, const T& default_value = T{}) {
        if (config_.contains(key)) {
            return config_[key].get<T>();
        }
        return default_value;
    }

    template<typename T>
    void Set(const std::string& key, const T& value) {
        config_[key] = value;
    }

private:
    nlohmann::json config_;
};

} // namespace dualdesk