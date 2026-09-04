#pragma once
#include <iostream>
#include "AbstractSceneFactory.h"
#include "SceneFactory.h"
#include "BaseScene.h"
#include <memory>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <any>
#include <type_traits>

class DirectXCom;
class Camera;
class Object3dCom;
class SkinningObject3dCom;
class MaterialManager;
class Light;
class ParticleManager;
class SpriteCom;
class AudioManager;
class SkyBox;
class SkyboxCom;
class Fade;
struct ID3D12GraphicsCommandList;
struct SceneRenderRequests;

// シーンの切り替えと、各シーンが必要とするサブシステムの仲介を担うクラス。
// 各シーンは SceneManager 経由でカメラ・ライト・パーティクル等を取得する。
class SceneManager
{
public:
    SceneManager(DirectXCom* dxCommon = nullptr, std::ostream& logStream = std::cerr)
        : dxCommon_(dxCommon), logStream_(logStream) {}

    ~SceneManager();

    // SceneManager を初期化する
    void Initialize(DirectXCom* dxCommon);

    // アクティブなシーンとエンジンサブシステムを更新する
    void Update(float deltaTime);

    // 現在のシーンを描画する
    void Draw(SceneRenderRequests& renderRequests);

    // スカイボックスを描画する
    void DrawSkybox(ID3D12GraphicsCommandList* commandList) const;

    // 次のシーンを名前で予約する（フェードアウト → 切り替え → フェードイン）
    void ChangeScene(const std::string& sceneName);

    // 保留中のシーン遷移を適用する
    void ApplyPendingSceneChange();

    [[nodiscard]] static SceneManager* GetInstance();
    static void Destroy();

    // サブシステムの登録・取得
    void SetDirectXCom(DirectXCom* dxCommon) { dxCommon_ = dxCommon; }
    [[nodiscard]] DirectXCom* GetDirectXCom() const { return dxCommon_; }

    void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> sceneFactory)
        { sceneFactory_ = std::move(sceneFactory); }
    [[nodiscard]] BaseScene* GetCurrentScene() const { return scene_.get(); }

    void SetCamera(Camera* camera) { camera_ = camera; }
    [[nodiscard]] Camera* GetCamera() const { return camera_; }

    void SetObject3dCom(Object3dCom* object3dCom) { object3dCom_ = object3dCom; }
    [[nodiscard]] Object3dCom* GetObject3dCom() const { return object3dCom_; }

    void SetSkinningObject3dCom(SkinningObject3dCom* skinningObject3dCom) { skinningObject3dCom_ = skinningObject3dCom; }
    [[nodiscard]] SkinningObject3dCom* GetSkinningObject3dCom() const { return skinningObject3dCom_; }

    void SetMaterialManager(MaterialManager* materialManager) { materialManager_ = materialManager; }
    [[nodiscard]] MaterialManager* GetMaterialManager() const { return materialManager_; }

    void SetLight(Light* light) { light_ = light; }
    [[nodiscard]] Light* GetLight() const { return light_; }

    void SetParticleManager(ParticleManager* particleManager) { particleManager_ = particleManager; }
    [[nodiscard]] ParticleManager* GetParticleManager() const { return particleManager_; }

    void SetSpriteCom(SpriteCom* spriteCom) { spriteCom_ = spriteCom; }
    [[nodiscard]] SpriteCom* GetSpriteCom() const { return spriteCom_; }

    void SetFadeApplication(Fade* fade) { fadeApplication_ = fade; }
    [[nodiscard]] Fade* GetFadeApplication() const { return fadeApplication_; }

    void SetAudioManager(AudioManager* audioManager) { audioManager_ = audioManager; }
    [[nodiscard]] AudioManager* GetAudioManager() const { return audioManager_; }

    void SetSkyBox(SkyBox* skyBox) { skybox_ = skyBox; }
    [[nodiscard]] SkyBox* GetSkyBox() const { return skybox_; }

    void SetSkyboxCom(SkyboxCom* skyboxCom) { skyboxCom_ = skyboxCom; }
    [[nodiscard]] SkyboxCom* GetSkyboxCom() const { return skyboxCom_; }

    void SetSkyboxTextureIndex(uint32_t textureIndex) { skyboxTextureIndex_ = textureIndex; }
    [[nodiscard]] uint32_t GetSkyboxTextureIndex() const { return skyboxTextureIndex_; }

    void SetShowSkybox(bool show) { showSkybox_ = show; }
    [[nodiscard]] bool  GetShowSkybox()    const { return showSkybox_; }
    [[nodiscard]] bool* GetShowSkyboxPtr()       { return &showSkybox_; }

    // --- 1行シーン登録テンプレート ---
    template <typename T>
    void RegisterScene(const std::string& sceneName)
    {
        if (!sceneFactory_) {
            sceneFactory_ = std::make_unique<SceneFactory>();
        }
        sceneFactory_->Register(sceneName, []() -> std::unique_ptr<BaseScene> {
            return std::make_unique<T>();
        });
    }

    // シーン遷移状態
    enum class TransitionState
    {
        None,
        FadeOut,
        Switching,
        FadeIn
    };

    // シーン遷移中かどうか
    [[nodiscard]] bool IsTransitioning() const { return transitionState_ != TransitionState::None; }
    [[nodiscard]] TransitionState GetTransitionState() const { return transitionState_; }
    [[nodiscard]] const std::string& GetCurrentSceneName() const { return currentSceneName_; }
    [[nodiscard]] std::vector<std::string> GetAvailableSceneNames() const;

    // 現在のシーンを即座に再起動（F5リロード等）
    void RestartCurrentScene() { ChangeScene(currentSceneName_); }

    // =========================================================================
    // シーン間データ共有（Scene Context）
    // 基本型（int, float, bool等）はもちろん、任意の構造体やクラスも受け渡し可能
    // =========================================================================

    /// @brief 任意の型 T のデータを保存します（スコア構造体など）
    template <typename T>
    void SetSceneData(const std::string& key, const T& value)
    {
        sceneAnyData_[key] = value;
        if constexpr (std::is_same_v<T, std::string>) {
            sceneContextData_[key] = value;
        } else if constexpr (std::is_same_v<T, const char*>) {
            sceneContextData_[key] = std::string(value);
        } else if constexpr (std::is_same_v<T, bool>) {
            sceneContextData_[key] = value ? "true" : "false";
        } else if constexpr (std::is_arithmetic_v<T>) {
            sceneContextData_[key] = std::to_string(value);
        }
    }

    /// @brief 文字列リテラル専用オーバーロード
    void SetSceneData(const std::string& key, const char* value)
    {
        SetSceneData<std::string>(key, std::string(value));
    }

    /// @brief 直感的な基本型オーバーロード
    void SetSceneDataInt(const std::string& key, int value) { SetSceneData<int>(key, value); }
    void SetSceneDataFloat(const std::string& key, float value) { SetSceneData<float>(key, value); }
    void SetSceneDataDouble(const std::string& key, double value) { SetSceneData<double>(key, value); }
    void SetSceneDataBool(const std::string& key, bool value) { SetSceneData<bool>(key, value); }
    void SetSceneDataString(const std::string& key, const std::string& value) { SetSceneData<std::string>(key, value); }

    /// @brief 任意の型 T のデータを取得します（型不一致時は文字列辞書からのフォールバックも実行）
    template <typename T>
    [[nodiscard]] T GetSceneData(const std::string& key, const T& defaultVal = T{}) const
    {
        // 1. any データから検索
        auto itAny = sceneAnyData_.find(key);
        if (itAny != sceneAnyData_.end()) {
            try {
                return std::any_cast<T>(itAny->second);
            } catch (const std::bad_any_cast&) {
                // 型不一致時は文字列辞書側を探索
            }
        }
        // 2. 文字列データからの変換フォールバック
        auto itStr = sceneContextData_.find(key);
        if (itStr != sceneContextData_.end()) {
            if constexpr (std::is_same_v<T, std::string>) {
                return itStr->second;
            } else if constexpr (std::is_same_v<T, int>) {
                try { return std::stoi(itStr->second); } catch (...) {}
            } else if constexpr (std::is_same_v<T, float>) {
                try { return std::stof(itStr->second); } catch (...) {}
            } else if constexpr (std::is_same_v<T, double>) {
                try { return std::stod(itStr->second); } catch (...) {}
            } else if constexpr (std::is_same_v<T, bool>) {
                return (itStr->second == "1" || itStr->second == "true" || itStr->second == "TRUE");
            }
        }
        return defaultVal;
    }

    /// @brief 文字列型取得（後方互換性用）
    [[nodiscard]] std::string GetSceneData(const std::string& key, const std::string& defaultVal = "") const
    {
        return GetSceneData<std::string>(key, defaultVal);
    }

    /// @brief 直感的な基本型ゲッター
    [[nodiscard]] int         GetSceneDataInt(const std::string& key, int defaultVal = 0) const { return GetSceneData<int>(key, defaultVal); }
    [[nodiscard]] float       GetSceneDataFloat(const std::string& key, float defaultVal = 0.0f) const { return GetSceneData<float>(key, defaultVal); }
    [[nodiscard]] double      GetSceneDataDouble(const std::string& key, double defaultVal = 0.0) const { return GetSceneData<double>(key, defaultVal); }
    [[nodiscard]] bool        GetSceneDataBool(const std::string& key, bool defaultVal = false) const { return GetSceneData<bool>(key, defaultVal); }
    [[nodiscard]] std::string GetSceneDataString(const std::string& key, const std::string& defaultVal = "") const { return GetSceneData<std::string>(key, defaultVal); }

    /// @brief データが存在するか確認
    [[nodiscard]] bool HasSceneData(const std::string& key) const
    {
        return (sceneAnyData_.find(key) != sceneAnyData_.end() || sceneContextData_.find(key) != sceneContextData_.end());
    }

    /// @brief 特定のキーのデータを削除
    void RemoveSceneData(const std::string& key)
    {
        sceneAnyData_.erase(key);
        sceneContextData_.erase(key);
    }

    /// @brief 全てのシーン共有データをクリア
    void ClearAllSceneData()
    {
        sceneAnyData_.clear();
        sceneContextData_.clear();
    }

    // ImGui によるシーン切り替えデバッグUI
    void DrawSceneSelectorUI();

    void SetFadeDuration(float duration) { fadeDuration_ = duration; }
    [[nodiscard]] float GetFadeDuration() const { return fadeDuration_; }

private:
    void CommitPendingSceneChange();

    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
    std::unique_ptr<BaseScene>            scene_     = nullptr;
    std::unique_ptr<BaseScene>            nextScene_ = nullptr;

    std::string currentSceneName_ = "DEFAULT";
    std::string pendingSceneName_ = "";
    TransitionState transitionState_ = TransitionState::None;
    float fadeDuration_ = 0.4f;
    float transitionTimer_ = 0.0f;

    DirectXCom* dxCommon_ = nullptr;

    Camera*              camera_              = nullptr;
    Object3dCom*         object3dCom_         = nullptr;
    SkinningObject3dCom* skinningObject3dCom_ = nullptr;
    MaterialManager*     materialManager_     = nullptr;
    Light*               light_               = nullptr;
    ParticleManager*     particleManager_     = nullptr;
    SpriteCom*           spriteCom_           = nullptr;
    AudioManager*        audioManager_        = nullptr;
    SkyBox*              skybox_              = nullptr;
    SkyboxCom*           skyboxCom_           = nullptr;
    uint32_t             skyboxTextureIndex_  = 0;
    bool                 showSkybox_          = true;
    Fade*                fadeApplication_     = nullptr;

    std::unordered_map<std::string, std::string> sceneContextData_;
    std::unordered_map<std::string, std::any>    sceneAnyData_;

    bool isSceneTransitioning_       = false;
    bool hasSwitchedSceneDuringFade_ = false;

    std::ostream& logStream_ = std::cerr;
};

/// @brief アプリケーション側で簡単にシーンを登録できるマクロ
#define REGISTER_SCENE(SceneClass, SceneName) \
    SceneManager::GetInstance()->RegisterScene<SceneClass>(SceneName)

// BaseScene テンプレートメソッドのインライン実装
template <typename T>
inline void BaseScene::SetSceneData(const std::string &key, const T &value)
{
    if (sceneManager_) {
        sceneManager_->SetSceneData<T>(key, value);
    }
}

template <typename T>
inline T BaseScene::GetSceneData(const std::string &key, const T &defaultVal) const
{
    return sceneManager_ ? sceneManager_->GetSceneData<T>(key, defaultVal) : defaultVal;
}
