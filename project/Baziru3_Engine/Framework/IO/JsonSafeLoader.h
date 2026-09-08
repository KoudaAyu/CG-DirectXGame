#pragma once
#include <string>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <Windows.h>

namespace BaziruEngine::IO {

// =================================================================
// JsonSafeLoader
// -----------------------------------------------------------------
// JSONファイルの読み書きにおいて例外スローや強制終了（即死）を防ぎ、
// 破損時・欠損時に安全なフォールバック（代替データ）へフォールスルーする
// エンジン層の堅牢なI/Oユーティリティクラス。
// =================================================================
class JsonSafeLoader {
public:
    // ファイルから安全にJSONをロードします。
    // ファイルが存在しない、サイズが0バイト、構文エラー、フォーマット不整合等が発生した場合でも、
    // クラッシュせず警告ログを出力し、fallbackJsonをoutJsonに設定してfalseを返します。
    static bool LoadSafe(
        const std::string& filePath,
        nlohmann::json& outJson,
        const nlohmann::json& fallbackJson = nlohmann::json::object());

    // JSONを安全にファイルへ保存します。
    // 一時ファイル（.tmp）へ書き出してからアトミックに置換を行うため、
    // 書き込み途中のクラッシュや電源断でJSONが0バイト破損するのを防止します。
    static bool SaveSafe(
        const std::string& filePath,
        const nlohmann::json& json,
        int indent = 4);

    // JSONオブジェクトから指定した型の値を安全に取得します。
    // キーが存在しない、nullである、あるいは型が一致しない場合でも例外を投げずdefaultValueを返します。
    template <typename T>
    static T SafeGet(const nlohmann::json& j, const std::string& key, const T& defaultValue) {
        try {
            if (j.is_object() && j.contains(key) && !j[key].is_null()) {
                return j[key].get<T>();
            }
        } catch (...) {
            // 型変換例外等を安全にキャッチしてデフォルト値を返却
        }
        return defaultValue;
    }

    // JSONオブジェクトから指定した型の値の取得を試行します。
    // キーが存在し正常に取得できた場合のみtrueを返します。
    template <typename T>
    static bool TryGet(const nlohmann::json& j, const std::string& key, T& outValue) {
        try {
            if (j.is_object() && j.contains(key) && !j[key].is_null()) {
                outValue = j[key].get<T>();
                return true;
            }
        } catch (...) {
        }
        return false;
    }

    // 指定されたファイルが構文的に正常なJSONであるか確認します。
    static bool IsValidJsonFile(const std::string& filePath);

    // 破損したJSONファイルを安全に修復・初期化します。
    // 外部ライブラリ（crude_json等）が構文エラーでASSERT終了するのを未然に防ぎます。
    static bool SanitizeFile(
        const std::string& filePath,
        const nlohmann::json& fallbackJson = nlohmann::json::object());

    // ファイルのバックアップ（.bak）を作成します。
    static bool CreateBackup(const std::string& filePath);
};

} // namespace BaziruEngine::IO
