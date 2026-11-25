#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <d3d12.h>
#include <wrl.h>
#include "Bullet.h"
#include "RenderContext.h"

class GamePlayScene;
class Enemy;

/// <summary>
/// 戦闘システムクラス
/// プレイヤーや敵の射撃処理、3D実弾（Bullet）・超音速トレーサー・3D真鍮薬莢排莢・銃口マズルブラスト＆硝煙などの戦闘ロジックを管理します。
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
	/// リソース解放処理
	/// </summary>
	void Finalize();

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
	/// 排出された3D真鍮薬莢の物理バウンド・回転更新
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void UpdateShellCasings(float deltaTime);

	/// <summary>
	/// 排出された3D真鍮薬莢の描画
	/// </summary>
	/// <param name="ctx">描画コンテキスト</param>
	void DrawShellCasings(const RenderContext& ctx);

	/// <summary>
	/// 3D真鍮薬莢を機関部エジェクションポートから排出します
	/// </summary>
	void SpawnShellCasing(const Vector3& breechPos, const Vector3& forward, bool isEnemy = false);

	/// <summary>
	/// リロード時にマグウェルから3D湾曲ライフルマガジン（残弾あり／空フォロワー）を落下排出します
	/// </summary>
	void SpawnDroppedMagazine(const Vector3& magwellPos, const Vector3& forward, bool hasRemainingAmmo, bool isEnemy = false);

	/// <summary>
	/// 排出された3D湾曲マガジンの重力落下・地面バウンド更新
	/// </summary>
	void UpdateDroppedMagazines(float deltaTime);

	/// <summary>
	/// 地面に落下した3D湾曲マガジンの描画
	/// </summary>
	void DrawDroppedMagazines(const RenderContext& ctx);

	/// <summary>
	/// 銃口から射線方向へ伸びる3Dマズルフラッシュコーンの更新・加算描画
	/// </summary>
	void UpdateMuzzleFlashes3D(float deltaTime);
	void DrawMuzzleFlashes3D(const RenderContext& ctx);

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

	/// <summary>
	/// 曳光火球・残像用のプリマルチプライ加算合成PSOを取得
	/// </summary>
	ID3D12PipelineState* GetAdditiveTracerPSO();

private:
	static constexpr float kDefaultCasingLifeTime = 3.4f;
	static constexpr float kDefaultMagLifeTime    = 16.0f;

	struct ShellCasingInstance
	{
		std::unique_ptr<Object3d> obj;
		Vector3 pos{};
		Vector3 vel{};
		Vector3 rot{};
		Vector3 angVel{};
		float age = 0.0f;
		float lifeTime = kDefaultCasingLifeTime;
		int bounceCount = 0;
		bool settled = false;
	};

	struct DroppedMagazineInstance
	{
		std::unique_ptr<Object3d> obj;
		Vector4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Vector3 pos{};
		Vector3 vel{};
		Vector3 rot{};
		Vector3 angVel{};
		float age = 0.0f;
		float lifeTime = kDefaultMagLifeTime;
		int bounceCount = 0;
		bool settled = false;
	};

	struct MuzzleFlash3DInstance
	{
		std::unique_ptr<Object3d> obj;
		Vector3 pos{};
		Vector3 rot{};
		Vector3 baseScale{ 1.0f, 1.0f, 1.0f };
		Vector4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		float age = 0.0f;
		float lifeTime = 0.055f;
	};

	/// <summary>
	/// プレイヤーや敵キャラクターの射撃トリガー判定と弾丸生成処理
	/// </summary>
	/// <param name="deltaTime">経過時間</param>
	void UpdateCombat(float deltaTime);
	void UpdateBarrelHeatSmoke(float deltaTime);

	void EnsureBallisticAssetsLoaded();
	void SpawnMuzzleFlashCone3D(const Vector3& muzzlePos, const Vector3& dir, bool isShotgun, bool isEnemy);
	void EmitRealisticMuzzleBlast(const Vector3& muzzlePos, const Vector3& dir, bool isShotgun, bool isEnemy);
	void EmitRiverBallisticSplash(const Vector3& waterPos, const Vector3& bulletDir, bool fullPlunge);

private:
	GamePlayScene* scene_ = nullptr;                  // ゲームプレイシーンへの参照
	std::vector<std::unique_ptr<Bullet>> bullets_;    // 画面上に存在するアクティブな弾丸のリスト

	std::unique_ptr<Object3d> casingMasterObj_;       // 3D真鍮薬莢の共有マスターメッシュ（GPUバッファ共有元）
	std::vector<ShellCasingInstance> casings_;        // 地面に跳ねて転がる3D真鍮薬莢プール
	std::unique_ptr<Object3d> magFullMasterObj_;      // 残弾あり3D湾曲PMAGの共有マスターメッシュ
	std::unique_ptr<Object3d> magEmptyMasterObj_;     // 空フォロワー3D湾曲PMAGの共有マスターメッシュ
	std::vector<DroppedMagazineInstance> droppedMagazines_; // リロードで地面に落下・散乱する3Dマガジン
	std::unique_ptr<Object3d> muzzleConeMasterObj_;   // 射線方向へ展開する3D A2マズルフラッシュコーン共有メッシュ
	std::vector<MuzzleFlash3DInstance> muzzleFlashes3D_; // アクティブな3Dマズルフラッシュインスタンス
	std::unordered_map<const Enemy*, bool> enemyMagDroppedState_; // 敵リロード1回につき1個だけマガジン排出する管理マップ
	Microsoft::WRL::ComPtr<ID3D12PipelineState> additiveTracerPSO_; // 曳光火球・3Dマズル炎用 加算合成PSO（デプス書き込みOFF）
	uint32_t ballisticsAtlasTexIndex_ = UINT32_MAX;
	uint32_t muzzleFlashTexIndex_ = UINT32_MAX;
	uint32_t muzzleConeTexIndex_ = UINT32_MAX;
	uint32_t magazineTexIndex_ = UINT32_MAX;
	float barrelHeat_ = 0.0f;
	float barrelSmokeTimer_ = 0.0f;
	float timeSinceLastPlayerShot_ = 99.0f;
};
