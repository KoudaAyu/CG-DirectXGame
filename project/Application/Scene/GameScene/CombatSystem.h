#pragma once
#include <memory>
#include <vector>
#include "Bullet.h"

class GamePlayScene;

/// <summary>
/// 戦闘システムクラス
/// プレイヤーや敵の射撃処理、弾丸（Bullet）の生成・更新・削除などの戦闘ロジックを管理します。
/// </summary>
class CombatSystem
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="scene">呼び出し元のゲームプレイシーンへのポインタ</param>
	CombatSystem(GamePlayScene* scene);
	~CombatSystem() = default;

	/// <summary>
	/// 更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void Update(float deltaTime);

	/// <summary>
	/// すべての弾丸の更新処理
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void UpdateBullets(float deltaTime);

	/// <summary>
	/// 寿命が切れた、または着弾した不要な弾丸の削除処理
	/// </summary>
	void RemoveDeadBullets();

	/// <summary>
	/// 新しい弾丸を追加します
	/// </summary>
	/// <param name="bullet">追加する弾丸のスマートポインタ</param>
	void AddBullet(std::unique_ptr<Bullet> bullet);

	/// <summary>
	/// 管理している弾丸リストの取得
	/// </summary>
	std::vector<std::unique_ptr<Bullet>>& GetBullets() { return bullets_; }
	const std::vector<std::unique_ptr<Bullet>>& GetBullets() const { return bullets_; }

private:
	/// <summary>
	/// プレイヤーや敵キャラクターの射撃トリガー判定と弾丸生成処理
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void UpdateCombat(float deltaTime);

private:
	GamePlayScene* scene_ = nullptr;                  // ゲームプレイシーンへの参照
	std::vector<std::unique_ptr<Bullet>> bullets_;    // 画面上に存在するアクティブな弾丸のリスト
};
