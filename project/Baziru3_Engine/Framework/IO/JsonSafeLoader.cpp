#include "JsonSafeLoader.h"
#include <sstream>

namespace BaziruEngine::IO {

bool JsonSafeLoader::LoadSafe(
    const std::string& filePath,
    nlohmann::json& outJson,
    const nlohmann::json& fallbackJson)
{
    try {
        std::filesystem::path path(filePath);
        if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
            std::string warn = "[JsonSafeLoader Warning] Target file does not exist or is a directory: " + filePath + ". Falling through to fallback data.\n";
            OutputDebugStringA(warn.c_str());
            outJson = fallbackJson;
            return false;
        }

        std::ifstream file(path);
        if (!file.is_open()) {
            std::string warn = "[JsonSafeLoader Warning] Failed to open file: " + filePath + ". Falling through to fallback data.\n";
            OutputDebugStringA(warn.c_str());
            outJson = fallbackJson;
            return false;
        }

        // 0バイトの空ファイル検出
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        if (size <= 0) {
            std::string warn = "[JsonSafeLoader Warning] File is empty (0 bytes): " + filePath + ". Falling through to fallback data.\n";
            OutputDebugStringA(warn.c_str());
            outJson = fallbackJson;
            return false;
        }

        // JSONパースの実行
        file >> outJson;
        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        std::stringstream ss;
        ss << "[JsonSafeLoader Error] JSON Parse Error in '" << filePath 
           << "': " << e.what() << " (Byte offset: " << e.byte << "). Falling through to fallback data.\n";
        OutputDebugStringA(ss.str().c_str());
        outJson = fallbackJson;
        return false;
    }
    catch (const nlohmann::json::exception& e) {
        std::stringstream ss;
        ss << "[JsonSafeLoader Error] JSON Exception in '" << filePath 
           << "': " << e.what() << ". Falling through to fallback data.\n";
        OutputDebugStringA(ss.str().c_str());
        outJson = fallbackJson;
        return false;
    }
    catch (const std::exception& e) {
        std::stringstream ss;
        ss << "[JsonSafeLoader Error] Standard Exception loading '" << filePath 
           << "': " << e.what() << ". Falling through to fallback data.\n";
        OutputDebugStringA(ss.str().c_str());
        outJson = fallbackJson;
        return false;
    }
    catch (...) {
        std::string warn = "[JsonSafeLoader Error] Unknown exception loading '" + filePath + "'. Falling through to fallback data.\n";
        OutputDebugStringA(warn.c_str());
        outJson = fallbackJson;
        return false;
    }
}

bool JsonSafeLoader::SaveSafe(
    const std::string& filePath,
    const nlohmann::json& json,
    int indent)
{
    try {
        std::filesystem::path targetPath(filePath);
        if (targetPath.has_parent_path()) {
            std::filesystem::create_directories(targetPath.parent_path());
        }

        std::filesystem::path tempPath = targetPath;
        tempPath += ".tmp";

        // 1. 一時ファイルへの完全書き出し
        {
            std::ofstream outFile(tempPath);
            if (!outFile.is_open()) {
                std::string err = "[JsonSafeLoader Error] Could not open temporary file for writing: " + tempPath.string() + "\n";
                OutputDebugStringA(err.c_str());
                return false;
            }

            if (indent >= 0) {
                outFile << json.dump(indent);
            } else {
                outFile << json.dump();
            }
            outFile.flush();
            outFile.close();
        }

        // 2. アトミックな置換（アトミックリネームまたは安全コピー）
        std::error_code ec;
        std::filesystem::rename(tempPath, targetPath, ec);
        if (ec) {
            // Windowsでの排他ロックや別ドライブ間移動などでrenameが失敗した場合はコピーで上書き
            std::filesystem::copy_file(tempPath, targetPath, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tempPath, ec);
        }

        return true;
    }
    catch (const std::exception& e) {
        std::string err = "[JsonSafeLoader Error] Exception while saving to '" + filePath + "': " + e.what() + "\n";
        OutputDebugStringA(err.c_str());
        return false;
    }
    catch (...) {
        std::string err = "[JsonSafeLoader Error] Unknown exception while saving to '" + filePath + "'\n";
        OutputDebugStringA(err.c_str());
        return false;
    }
}

bool JsonSafeLoader::IsValidJsonFile(const std::string& filePath) {
    try {
        std::filesystem::path path(filePath);
        if (!std::filesystem::exists(path) || std::filesystem::is_directory(path)) {
            return false;
        }

        std::ifstream file(path);
        if (!file.is_open()) return false;

        file.seekg(0, std::ios::end);
        if (file.tellg() <= 0) return false;
        file.seekg(0, std::ios::beg);

        nlohmann::json testJson;
        file >> testJson;
        return !testJson.is_discarded();
    }
    catch (...) {
        return false;
    }
}

bool JsonSafeLoader::SanitizeFile(
    const std::string& filePath,
    const nlohmann::json& fallbackJson)
{
    try {
        std::filesystem::path path(filePath);
        if (!std::filesystem::exists(path)) {
            return true; // ファイルが存在しない場合は修復不要
        }

        if (!IsValidJsonFile(filePath)) {
            std::string warn = "[JsonSafeLoader] Corrupted JSON detected in '" + filePath + "'. Sanitizing with fallback data.\n";
            OutputDebugStringA(warn.c_str());
            CreateBackup(filePath); // 念のため.bakを残す
            return SaveSafe(filePath, fallbackJson);
        }
        return true;
    }
    catch (...) {
        return false;
    }
}

bool JsonSafeLoader::CreateBackup(const std::string& filePath) {
    try {
        std::filesystem::path src(filePath);
        if (!std::filesystem::exists(src)) return false;

        std::filesystem::path dst = src;
        dst += ".bak";

        std::error_code ec;
        return std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing, ec);
    }
    catch (...) {
        return false;
    }
}

} // namespace BaziruEngine::IO
