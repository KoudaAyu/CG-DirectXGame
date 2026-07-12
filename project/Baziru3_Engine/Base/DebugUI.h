#pragma once
#include "Sprite.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

class Camera;
class DebugCamera;
class MaterialManager;
class SpriteManager;
class OffScreenRendering;

namespace BaziruEngine::AI {
    class BehaviorTreeEditor;
}

class DebugUI
{
public:
    DebugUI(MaterialManager* materialManager, SpriteManager* spriteManager, Camera* camera,
            Sprite::Transform* transformObject, bool* useMonsterBall, bool* drawObject, bool* drawSprite);
    ~DebugUI(); // デストラクタ明示的宣言

    void Initialize();
    void Update();
    void Finalize();

    void SetOffScreenRendering(OffScreenRendering* offScreenRendering) { offScreenRendering_ = offScreenRendering; }

private:
    Sprite::Transform* transformObject_ = nullptr;
    bool* useMonsterBall_ = nullptr;
    bool* drawSphere_ = nullptr;
    bool* drawObject_ = nullptr;
    bool* drawSprite_ = nullptr;

private:
    Camera* camera_ = nullptr;
    DebugCamera* debugCamera_ = nullptr;
    SpriteManager* spriteManager_ = nullptr;
    MaterialManager* materialManager_ = nullptr;
    OffScreenRendering* offScreenRendering_ = nullptr;

    std::vector<std::unique_ptr<Sprite>> sprites;

    std::unique_ptr<BaziruEngine::AI::BehaviorTreeEditor> btEditor_ = nullptr;

    // パフォーマンス履歴データ (毎フレームの動的ヒープ確保を回避する固定配列構造)
    static constexpr int kMaxHistoryFrames = 100;
    static constexpr int kMaxStages = 16;
    
    struct StageProfile
    {
        char name[64];
        float history[kMaxHistoryFrames];
        bool active;
    };
    
    StageProfile stages_[kMaxStages];
    int historyOffset_ = 0;
};

