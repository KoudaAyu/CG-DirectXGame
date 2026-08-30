#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "KeyInput.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"

#include <memory>

/**
 * @brief ゲーム開発スターターシーン (GamePlayScene)
 * @details このシーンをベースに、プレイヤー、敵、ステージ、ゲームルールを実装します。
 *          スペースキーでシーン遷移の確認が可能です。
 */
class GamePlayScene : public BaseScene
{
public:
    GamePlayScene() = default;
    ~GamePlayScene() override = default;

    void InitializeScene() override;
    void Finalize() override;
    void Update() override;
    void Draw(SceneRenderRequests& renderRequests) override;

    const char* GetSceneType() const { return "GAMEPLAY"; }

private:
    std::unique_ptr<KeyInput> keyInput_;
    std::unique_ptr<Camera> camera_;
};
