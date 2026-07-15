#pragma once
#include <string>
#include <unordered_map>
#include <any>

namespace BaziruEngine::AI {

class Blackboard {
public:
    Blackboard() = default;
    ~Blackboard() = default;

    template<typename T>
    void Set(const std::string& key, const T& value) {
        data_[key] = value;
    }

    template<typename T>
    T Get(const std::string& key, const T& defaultValue = T()) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                // 型が一致しない場合はデフォルト値を返す
            }
        }
        return defaultValue;
    }

    bool Has(const std::string& key) const {
        return data_.find(key) != data_.end();
    }

    void Clear(const std::string& key) {
        data_.erase(key);
    }

    void ClearAll() {
        data_.clear();
    }

private:
    std::unordered_map<std::string, std::any> data_;
};

} // namespace BaziruEngine::AI
