#pragma once
#include "Vector.h"
#include "Application/Config/GameConfig.h"
#include "../../Enemy/Enemy.h"
#include <cstdint>

namespace GamePlayLevelLayout
{
	// --- M18 シグナル発煙筒 プロシージャルメッシュ定義 ---
	constexpr int kCanisterSegments = 12;
	constexpr float kCanisterBaseRadius = 0.042f, kCanisterTopCapY = 0.155f, kCanisterBoundingRadius = 0.5f, kNormalMinLength = 0.001f;
	constexpr Vector2 kUvMetal = { 0.15f, 0.15f }, kUvGreen = { 0.45f, 0.45f }, kUvStripe = { 0.85f, 0.35f };
	constexpr Vector3 kFlarePadOffset = { 1.8f, 0.026f, 1.2f }, kFlarePadRotation = { 0.12f, 0.35f, -0.06f };

	constexpr Vector3 kUnitBoxCorners[] = {
		{ -1.0f, -1.0f, -1.0f }, {  1.0f, -1.0f, -1.0f }, {  1.0f,  1.0f, -1.0f }, { -1.0f,  1.0f, -1.0f },
		{ -1.0f, -1.0f,  1.0f }, {  1.0f, -1.0f,  1.0f }, {  1.0f,  1.0f,  1.0f }, { -1.0f,  1.0f,  1.0f },
	};
	constexpr uint32_t kUnitBoxFaces[][4] = {
		{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 }, { 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 },
	};
	constexpr Vector3 kUnitBoxNormals[] = {
		{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f },
	};
	constexpr uint32_t kQuadTriIndices[] = { 0, 1, 2, 0, 2, 3 };

	struct CylinderBandSpec { float y0, y1, radiusMult; Vector2 uv; };
	constexpr CylinderBandSpec kCanisterBands[] = {
		{ 0.000f, 0.015f, 1.05f, kUvMetal  }, // A. 底部金属カシメ縁
		{ 0.015f, 0.075f, 1.00f, kUvGreen  }, // B. 下部オリーブドラブ缶胴
		{ 0.075f, 0.115f, 1.02f, kUvStripe }, // C. 中央スモーク識別グリーン帯
		{ 0.115f, 0.145f, 1.00f, kUvGreen  }, // D. 上部缶胴
		{ 0.145f, 0.155f, 1.05f, kUvMetal  }, // E. 上部金属カシメ縁
	};

	struct BoxPartSpec { Vector3 center, size; Vector2 uvMin, uvMax; };
	constexpr BoxPartSpec kCanisterBoxes[] = {
		{ {  0.0f,   0.168f, 0.0f }, { 0.024f, 0.026f, 0.024f }, kUvMetal, kUvMetal }, // M201A1 信管ヘッド
		{ {  0.026f, 0.115f, 0.0f }, { 0.006f, 0.095f, 0.016f }, kUvMetal, kUvMetal }, // スプーン（安全レバー）縦
		{ {  0.016f, 0.168f, 0.0f }, { 0.022f, 0.008f, 0.016f }, kUvMetal, kUvMetal }, // スプーン上部
		{ { -0.018f, 0.170f, 0.0f }, { 0.014f, 0.014f, 0.004f }, kUvMetal, kUvMetal }, // プルリング
	};

	// --- ステージ地面グリッド・NavMesh・巡回ルート定義 ---
	constexpr int kGroundGridW = 6, kGroundGridH = 6;
	constexpr float kGroundTileSize = 25.0f, kGroundGridCenterZ = 25.0f, kGroundTileY = -0.020f;
	constexpr size_t kMinValidObstacleCount = 2;
	constexpr float kSpawnYawEpsilon = 1e-4f, kValidInitialYawMin = -900.0f;
	constexpr struct { float minX, maxX, minZ, maxZ, cellSize, agentRadius; } kNavMeshGrid = { -33.5f, 33.5f, -11.5f, 68.5f, 0.5f, 0.58f };

	constexpr float kFlankerPatrolSpeed = 2.65f, kStandardPatrolSpeed = 2.15f;
	constexpr Vector2 kDefaultPatrolOffsets[] = { { -3.6f, -0.8f }, { -1.2f, 1.0f }, { 1.2f, -0.6f }, { 3.6f, 0.8f } };
	constexpr float kNwWingPatrolMaxX = -2.0f, kNwWingPatrolMinZ = 33.0f, kNwWingPatrolMaxZ = 50.0f, kGenericPatrolHalfSpanX = 3.6f;
	constexpr Vector3 kNwWingPatrolRoute[]    = { { -22.5f, 0.0f, 44.0f }, { -17.8f, 0.0f, 44.0f }, { -13.2f, 0.0f, 44.0f }, { -8.5f, 0.0f, 44.0f } };
	constexpr float kQrfMidpointOffsetZ = -1.5f;
	constexpr Vector3 kFallbackQrfSpawnPos = { 3.2f, 0.0f, 48.2f };

	struct EnemyRoleMatchRule { const char *typeName, *nameKeyword; bool isPatrol; Enemy::TacticalRole role; };
	constexpr EnemyRoleMatchRule kEnemyRoleRules[] = {
		{ "EnemySpawn_Warden",    "Warden",       false, Enemy::TacticalRole::Warden    },
		{ "EnemySpawn_Rookie",    "Rookie",       false, Enemy::TacticalRole::Rookie    },
		{ "EnemySpawn_Flanker",   "Flanker",      true,  Enemy::TacticalRole::Flanker   },
		{ "EnemySpawn_Overwatch", "Overwatch",    false, Enemy::TacticalRole::Overwatch },
		{ "EnemySpawn_Patrol",    "Enemy_Patrol", true,  Enemy::TacticalRole::Patrol    },
	};

	// --- フォールバック障害物・初期敵・標的・チュートリアル看板・ストレステスト定義 ---
	struct FallbackObstacleSpec { Vector3 pos, scale, rot; const char* modelFile; };
	constexpr Vector3 kUnitScale = { 1.0f, 1.0f, 1.0f }, kTreeScale = { 1.2f, 1.35f, 1.2f };
	constexpr float kPi = GameConfig::Math::kPi, kHalfPi = GameConfig::Math::kHalfPi;

	constexpr FallbackObstacleSpec kFallbackObstacles[] = {
		// 大型コンテナ (2)
		{ {  -3.5f, 0.0f,  14.5f }, kUnitScale, { 0.0f,  0.22f, 0.0f }, "container.obj" },
		{ {   3.6f, 0.0f,  18.5f }, kUnitScale, { 0.0f, -0.25f, 0.0f }, "container.obj" },
		// 土嚢バリケード (13)
		{ {  -3.5f, 0.0f,   8.0f }, kUnitScale, { 0.0f,  0.35f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -6.0f, 0.0f,  15.0f }, kUnitScale, { 0.0f, kHalfPi, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -2.0f, 0.0f,  22.0f }, kUnitScale, { 0.0f,  -0.2f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -3.5f, 0.0f,  27.0f }, kUnitScale, { 0.0f,   0.0f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {   3.5f, 0.0f,  27.0f }, kUnitScale, { 0.0f,   0.0f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -3.5f, 0.0f,  33.5f }, kUnitScale, { 0.0f,    kPi, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {   3.5f, 0.0f,  33.5f }, kUnitScale, { 0.0f,    kPi, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -3.8f, 0.0f,  45.0f }, kUnitScale, { 0.0f,   0.0f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {   3.8f, 0.0f,  45.0f }, kUnitScale, { 0.0f,   0.0f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -3.2f, 0.0f,  56.5f }, kUnitScale, { 0.0f,   0.3f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {   3.2f, 0.0f,  56.5f }, kUnitScale, { 0.0f,  -0.3f, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {  -3.8f, 0.0f,  62.0f }, kUnitScale, { 0.0f, kHalfPi, 0.0f }, "duckov_sandbag_wall.obj" },
		{ {   3.8f, 0.0f,  62.0f }, kUnitScale, { 0.0f, kHalfPi, 0.0f }, "duckov_sandbag_wall.obj" },
		// ドラム缶スタック (9)
		{ {   4.5f, 0.0f,   2.8f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {   3.8f, 0.0f,   9.2f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {   5.2f, 0.0f,  20.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {  -5.5f, 0.0f,  26.5f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {   5.8f, 0.0f,  34.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {  -4.5f, 0.0f,  40.5f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {   0.0f, 0.0f,  58.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {  -3.0f, 0.0f,  64.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		{ {   3.0f, 0.0f,  64.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "duckov_barrel_stack.obj" },
		// 木箱 (10)
		{ {  -3.6f, 0.0f,   3.2f }, kUnitScale, { 0.0f,  0.25f, 0.0f }, "duckov_crate.obj" },
		{ {   5.5f, 0.0f,   4.2f }, kUnitScale, { 0.0f,  0.15f, 0.0f }, "duckov_crate.obj" },
		{ {   6.0f, 0.0f,  18.0f }, kUnitScale, { 0.0f,  0.50f, 0.0f }, "duckov_crate.obj" },
		{ {  -4.5f, 0.0f,  21.5f }, kUnitScale, { 0.0f,  0.30f, 0.0f }, "duckov_crate.obj" },
		{ {   5.5f, 0.0f,  26.5f }, kUnitScale, { 0.0f, -0.15f, 0.0f }, "duckov_crate.obj" },
		{ {  -5.8f, 0.0f,  34.0f }, kUnitScale, { 0.0f,  0.40f, 0.0f }, "duckov_crate.obj" },
		{ {   4.5f, 0.0f,  40.5f }, kUnitScale, { 0.0f,  0.20f, 0.0f }, "duckov_crate.obj" },
		{ {   0.0f, 0.0f,  48.0f }, kUnitScale, { 0.0f,  0.25f, 0.0f }, "duckov_crate.obj" },
		{ {  -5.5f, 0.0f,  57.5f }, kUnitScale, { 0.0f,  0.15f, 0.0f }, "duckov_crate.obj" },
		{ {   0.0f, 0.0f,  66.0f }, kUnitScale, { 0.0f,  0.0f,  0.0f }, "duckov_crate.obj" },
		// 東西木造橋 (2)
		{ { -14.0f, 0.0f,  30.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "bridge.obj" },
		{ {  14.0f, 0.0f,  30.0f }, kUnitScale, { 0.0f, 0.0f, 0.0f }, "bridge.obj" },
		// 外周マツ林 (11)
		{ { -10.5f, 0.0f, -11.5f }, kTreeScale, { 0.0f, 0.45f, 0.0f }, "duckov_pine_tree.obj" },
		{ {  -3.8f, 0.0f, -11.2f }, kTreeScale, { 0.0f, 0.90f, 0.0f }, "duckov_pine_tree.obj" },
		{ {   0.0f, 0.0f, -13.0f }, kTreeScale, { 0.0f, 2.30f, 0.0f }, "duckov_pine_tree.obj" },
		{ {   3.5f, 0.0f, -11.4f }, kTreeScale, { 0.0f, 1.20f, 0.0f }, "duckov_pine_tree.obj" },
		{ {  10.8f, 0.0f, -11.6f }, kTreeScale, { 0.0f, 0.60f, 0.0f }, "duckov_pine_tree.obj" },
		{ { -14.5f, 0.0f,  -8.0f }, kTreeScale, { 0.0f, 0.85f, 0.0f }, "duckov_pine_tree.obj" },
		{ { -15.2f, 0.0f,  -3.5f }, kTreeScale, { 0.0f, 2.10f, 0.0f }, "duckov_pine_tree.obj" },
		{ {  14.5f, 0.0f,  -8.0f }, kTreeScale, { 0.0f, 1.50f, 0.0f }, "duckov_pine_tree.obj" },
		{ {  15.5f, 0.0f,  -3.0f }, kTreeScale, { 0.0f, 0.70f, 0.0f }, "duckov_pine_tree.obj" },
		{ { -10.2f, 0.0f,  -6.5f }, kTreeScale, { 0.0f, 1.30f, 0.0f }, "duckov_pine_tree.obj" },
		{ {  10.2f, 0.0f,  -6.5f }, kTreeScale, { 0.0f, 2.00f, 0.0f }, "duckov_pine_tree.obj" },
	};

	struct FallbackEnemySpec { Vector3 pos; bool isPatrol; float yaw; Enemy::TacticalRole role; bool useNwRoute; };
	constexpr FallbackEnemySpec kFallbackEnemies[] = {
		{ { -15.2f, 0.0f, 22.2f }, false,     kPi, Enemy::TacticalRole::Rookie,   false },
		{ { -22.5f, 0.0f, 44.0f }, true,  -999.0f, Enemy::TacticalRole::Patrol,   true  },
		{ {  14.5f, 0.0f, 45.2f }, false,  -2.35f, Enemy::TacticalRole::Garrison, false },
	};

	constexpr float kDefaultTargetRadius = 0.8f;
	constexpr Vector3 kFallbackTargetPositions[] = {
		{ 18.5f, 0.0f, 13.2f }, { -22.2f, 0.0f, 22.5f }, { -21.5f, 0.0f, 47.8f }, { 21.5f, 0.0f, 47.2f }
	};

	struct TutorialSignSpec { Vector3 pos; float triggerRadius; const char8_t* message; };
	constexpr TutorialSignSpec kTutorialSigns[] = {
		{ {   2.2f, 0.0f,  5.0f }, 3.0f, u8"[WASD] 移動  /  [C] ダッシュ  /  [M] 地図\nまずは右の東棟で補給と射撃へ" },
		{ {  10.5f, 0.0f,  2.5f }, 2.5f, u8"[E]長押し：弾薬回収  /  [R]：装填\n奥の【標的 [1]】を [左クリック] で射撃" },
		{ { -11.2f, 0.0f,  8.8f }, 3.0f, u8"土嚢密着：遮蔽  /  赤レーザー：[SPACE] 回避\n【標的 [2]】を破壊して北の橋へ" },
		{ { -17.8f, 0.0f, 35.0f }, 3.0f, u8"青い扇形＝敵の視界（死角から奇襲）\n【標的 [3]】を破壊（北西奥に秘宝あり）" },
		{ {   2.8f, 0.0f, 46.8f }, 3.0f, u8"標的4基の破壊で北東の脱出地点が開放\n緑スモーク内で 3秒 待機して生還" },
	};

	constexpr int kStressTestRows = 10, kStressTestCols = 10;
	constexpr float kStressSpacing = 3.5f, kStressBaseOffsetZ = 15.0f;
}
