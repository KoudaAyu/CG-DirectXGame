#include "TitleScene.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Camera.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "SpriteManager.h"
#include "Sprite.h"
#include "WindowsAPI.h"
#include "Matrix4x4.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/Transition/StingerTransition.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <unordered_map>

namespace {
	// ---------------------------------------------------------------
	// 3Dモデルデータ静的キ
	// --------------------------------------------
	static std::unordered_map<std::string, Object3d::ModelData> sModelDataCache;

	const Object3d::ModelData& GetOrLoadModelData(const std::string& directoryPath, const std::string& filename)
	{
		std::string key = directoryPath + "/" + filename;
		auto it = sModelDataCache.find(key);
		if (it != sModelDataCache.end())
		{
			return it->second;
		}
		Object3d::ModelData data = Object3d::LoadObjFile(directoryPath, filename);
		sModelDataCache[key] = std::move(data);
		return sModelDataCache[key];
	}

	constexpr float kPi = 3.14159265f;

	// ---------------------------------------------------------------
	// 構図パラメータ（B案＋C案融合：鳥瞰タクティカルジオラマ）
	// 手前岸・清流・
	// 木造橋・対岸敵陣地の空間階層を美しいミニチュアとして一望
	// ---------------------------------------------------------------
	constexpr float kLandY = -0.40f;      // 手前岸の地面
	constexpr float kFarLandY = -0.38f;   // 対岸の地面
	constexpr float kWaterY = -0.65f;     // 水面
	constexpr float kRiverBedY = -0.75f;  // 川底

	// 川は画面を左右に横切る（中景）。手前岸ライン / 対岸ライン（Z）
	inline float NearShoreZ(float x) { return 2.6f + 0.30f * std::sin(x * 0.30f); }
	inline float FarShoreZ(float x) { return 7.4f + 0.25f * std::cos(x * 0.28f); }

	// 最終カメラ：B案(急角度の俯瞰ジオラマ)とC案(目線高さの情緒構図)の中間
	// 高さ約4.4m・俯角約24度で、主役アヒルの右後方やや遠めから斜めに川を見渡す。
	// 主役は右1/3、橋は中央の導線、敵拠点と脱出煙は奥に全体が収まる。
	const Vector3 kCamFinalPos = { 4.20f, 4.00f, -5.20f };
	const Vector3 kCamFinalRot = { 0.42f, -0.38f, 0.0f };
	// イントロ開始カメラ：B案の真上寄り俯瞰から、C案の目線構図へクレーンダウン
	const Vector3 kCamStartPos = { 3.50f, 8.50f, -9.00f };
	const Vector3 kCamStartRot = { 0.80f, -0.30f, 0.0f };

	constexpr float kIntroDuration = 3.0f;   // カメラ導入
	constexpr float kLogoSlamTime = 2.40f;   // ロゴ着弾タイミング
	constexpr float kLogoSlideTime = 0.36f;  // ロゴのスライド時間

	// 主役アヒル：対岸の橋・敵陣地の方向（画面左奥）を静かに見つめる横顔3/4（C案）
	constexpr float kDuckFacingYaw = -1.25f;

	// ---------------------------------------------------------------
	uint32_t sSmokeTexIndex = UINT32_MAX;
	uint32_t sGrassTexIndex = UINT32_MAX;
	uint32_t sSignpostTexIndex = UINT32_MAX;
	uint32_t sTargetTexIndex = UINT32_MAX;
	uint32_t sBridgeTexIndex = UINT32_MAX;
	uint32_t sWatchtowerTexIndex = UINT32_MAX;

	std::unique_ptr<Object3d> sNearGroundModel;
	std::unique_ptr<Object3d> sFarGroundModel;
	std::unique_ptr<Object3d> sExtractionPadModel;
	uint32_t sExtractionPadTexIndex = UINT32_MAX;

	// 接地影デカール（地面の上に配置し、ミニチュアの接地感・立体感を向上）
	std::unique_ptr<Object3d> sShadowDecalsModel;
	std::vector<std::unique_ptr<Object3d>> sEnemyDuckShadows;

	// 敵陣地（対岸）の拡張プロップ群
	struct PropInstance
	{
		std::unique_ptr<Object3d> model;
		uint32_t textureIndex = UINT32_MAX;
	};
	std::vector<PropInstance> sEnemyProps;

	// 敵兵アヒル小隊
	enum class EnemyBehaviorType
	{
		SentryLookout, // 橋・川正面の定点見張り（首振り索敵・呼吸）
		OvalPatrol,    // 滑らかな楕円パトロール（進行方向を常に滑らかに向く）
		StandGuard,    // 監視塔・ドラム缶脇の警戒姿勢
		ScoutScan,     // 小道沿いでの周囲索敵・安全確認
		PatrolAcross,  // 対岸防衛ラインを横断行軍する巡回兵
	};

	struct EnemyDuckInstance
	{
		std::unique_ptr<Object3d> model;
		Vector3 basePos;
		float baseFacing = 0.0f;
		EnemyBehaviorType behavior = EnemyBehaviorType::SentryLookout;
		float radiusX = 0.0f;
		float radiusZ = 0.0f;
		float speed = 1.0f;
		float phaseOffset = 0.0f;
	};
	std::vector<EnemyDuckInstance> sEnemyDucks;

	// 演出タイマー
	float sIntroTimer = 0.0f;
	bool sLogoLanded = false;
	float sShakeTimer = 0.0f;
	float sShakePower = 0.0f;
	float sFlashTimer = 0.0f;
	float sScreenW = 1280.0f;
	float sScreenH = 720.0f;
	Vector2 sLogoFinalPos = { 0.0f, 0.0f };
	Vector2 sLogoSize = { 580.0f, 145.0f };

	// タイトル文字演出（ダッシュ残像・グリント掃引・着弾フラッシュ）
	// グリント用シートは 4x4 セル：0..14 = 光の掃引フレーム、15 = 全面白フラッシュ
	Sprite* sLogoGhosts[2] = { nullptr, nullptr };
	Sprite* sLogoShine = nullptr;
	Sprite* sPromptShine = nullptr;
	Vector2 sPromptBasePos = { 0.0f, 0.0f };
	const Vector2 kPromptSize = { 470.0f, 66.0f };
	constexpr int kShineCols = 4;
	constexpr int kShineRows = 4;
	constexpr int kShineFlashCell = kShineCols * kShineRows - 1;
	constexpr int kShineSweepFrames = kShineFlashCell;
	constexpr float kLogoTexW = 1000.0f;
	constexpr float kLogoTexH = 250.0f;
	constexpr float kPromptTexW = 700.0f;
	constexpr float kPromptTexH = 100.0f;

	void SetShineCell(Sprite* sp, int cell, float cellW, float cellH)
	{
		const int c = cell % kShineCols;
		const int r = cell / kShineCols;
		sp->SetTextureLeftTop({ static_cast<float>(c) * cellW, static_cast<float>(r) * cellH });
		sp->SetTextureSize({ cellW, cellH });
	}

	// period 秒ごとに duration 秒かけて光が走る。走っていない間は -1
	int ShineFrame(float t, float period, float duration)
	{
		if (t < 0.0f) return -1;
		const float ph = std::fmod(t, period);
		if (ph >= duration) return -1;
		return std::clamp(static_cast<int>(ph / duration * kShineSweepFrames), 0, kShineSweepFrames - 1);
	}

	// 環境パーティクルタイマー（C案：静謐な緊張感と美しい自然現象）
	float sExtractionSmokeTimer = 0.0f;
	float sBeaconMoteTimer = 0.0f;
	float sRiverFoamTimer = 0.0f;
	float sAmbientDustTimer = 0.0f;
	float sRiverWaveTimer = 0.0f;   // 川面を流れる白波・泡筋
	float sRiverWakeTimer = 0.0f;   // 岩や橋脚の激突白波・引き波ウェイク
	float sRiverGlintTimer = 0.0f;  // 水面のきらめき・跳ね水滴
	float sLeafTimer = 0.0f;        // 風に舞う木の葉エフェクトタイマー
	float sGroundDustTimer = 0.0f;  // 地面の微小土煙・砂埃パーティクルタイマー

	float sDuckRecoilTimer = 0.0f;
	float sDuckClickHopTimer = 0.0f; // アヒルをクリックしたときのホップ演出タイマー
	int32_t sAlarmSoundId = -1;
	int32_t sBgmSoundId = -1;
	int32_t sBgmPlayId = -1;
	float sBgmTimer = 0.0f;
	int32_t sDuckSeSoundId = -1;
	int32_t sDuckSePlayId = -1;

	// 出撃時カメラ完全ロック制御（トランジション開始前・実行中にカメラが動くのを完全に防止）
	bool sCameraLocked = false;
	Vector3 sCameraLockedPos = { 0.0f, 0.0f, 0.0f };
	Vector3 sCameraLockedRot = { 0.0f, 0.0f, 0.0f };

	float EaseInOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	}
	float EaseOutBack(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		const float c1 = 1.70158f;
		const float c3 = c1 + 1.0f;
		return 1.0f + c3 * std::pow(t - 1.0f, 3.0f) + c1 * std::pow(t - 1.0f, 2.0f);
	}
	float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		const float u = 1.0f - t;
		return 1.0f - u * u * u;
	}
	Vector3 Lerp3(const Vector3& a, const Vector3& b, float t)
	{
		return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
	}
	float Rand01() { return static_cast<float>(rand()) / static_cast<float>(RAND_MAX); }
	float RandRange(float a, float b) { return a + (b - a) * Rand01(); }
	float Length3(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

	Vector3 RandDir(float upBias)
	{
		float th = RandRange(0.0f, 2.0f * kPi);
		float y = RandRange(upBias, 1.0f);
		float r = std::sqrt((std::max)(0.0f, 1.0f - y * y));
		return { std::cos(th) * r, y, std::sin(th) * r };
	}

	void BuildGridIndices(Object3d::ModelData& data, int cols, int rows)
	{
		data.indices.reserve((rows - 1) * (cols - 1) * 6);
		for (int r = 0; r < rows - 1; ++r)
		{
			for (int c = 0; c < cols - 1; ++c)
			{
				uint32_t i0 = static_cast<uint32_t>(r * cols + c);
				uint32_t i1 = static_cast<uint32_t>(r * cols + (c + 1));
				uint32_t i2 = static_cast<uint32_t>((r + 1) * cols + c);
				uint32_t i3 = static_cast<uint32_t>((r + 1) * cols + (c + 1));
				data.indices.push_back(i0);
				data.indices.push_back(i1);
				data.indices.push_back(i2);
				data.indices.push_back(i1);
				data.indices.push_back(i3);
				data.indices.push_back(i2);
			}
		}
	}

	Vector3 SlopeNormalZ(float dYdZ)
	{
		Vector3 n = { 0.0f, 1.0f, -dYdZ };
		float len = std::sqrt(n.y * n.y + n.z * n.z);
		return { 0.0f, n.y / len, n.z / len };
	}

	void DrawModel(Object3dCom* com, Object3d* model, const RenderContext& base, uint32_t texIndex)
	{
		if (!com || !model) return;
		RenderContext ctx = base;
		if (texIndex != UINT32_MAX)
		{
			ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
		}
		com->Draw(model, ctx, model->GetModelData(), true);
	}

	void AddShadowDisc(Object3d::ModelData& data, float cx, float cy, float cz, float rx, float rz)
	{
		uint32_t base = static_cast<uint32_t>(data.vertices.size());
		Sprite::VertexData v0{}, v1{}, v2{}, v3{};
		v0.position = { cx - rx, cy, cz - rz, 1.0f };
		v0.texcoord = { 0.0f, 0.0f };
		v0.normal = { 0.0f, 1.0f, 0.0f };

		v1.position = { cx + rx, cy, cz - rz, 1.0f };
		v1.texcoord = { 1.0f, 0.0f };
		v1.normal = { 0.0f, 1.0f, 0.0f };

		v2.position = { cx - rx, cy, cz + rz, 1.0f };
		v2.texcoord = { 0.0f, 1.0f };
		v2.normal = { 0.0f, 1.0f, 0.0f };

		v3.position = { cx + rx, cy, cz + rz, 1.0f };
		v3.texcoord = { 1.0f, 1.0f };
		v3.normal = { 0.0f, 1.0f, 0.0f };

		data.vertices.push_back(v0);
		data.vertices.push_back(v1);
		data.vertices.push_back(v2);
		data.vertices.push_back(v3);

		data.indices.push_back(base + 0);
		data.indices.push_back(base + 1);
		data.indices.push_back(base + 2);
		data.indices.push_back(base + 1);
		data.indices.push_back(base + 3);
		data.indices.push_back(base + 2);
	}

	// =================================================================
	// パーティクル管理
	// =================================================================
	struct FxTextures
	{
		uint32_t circle = UINT32_MAX;
		uint32_t star = UINT32_MAX;
		uint32_t puff = UINT32_MAX;
		uint32_t leaf = UINT32_MAX;
	};
	FxTextures sFx;

	// =================================================================
	// 環境装飾（木・茂み・岩・葦・草むら・土の小道）
	// 外部モデルが無いため、ローポリ形状をコードで生成し、色ごとに1メッシュへバッチ化する
	// =================================================================
	uint32_t sWhiteTexIndex = UINT32_MAX;
	struct SceneryVertexBase
	{
		Vector3 basePos;
		float swayFactor = 0.0f; // 0.0f: 揺れなし, 1.0f: フルで揺れる
	};

	struct SceneryBatch
	{
		std::unique_ptr<Object3d> model;
		std::vector<SceneryVertexBase> baseVertices;
		bool hasSway = false;
	};
	std::vector<SceneryBatch> sScenery;
	std::unique_ptr<Object3d> sPathDecalModel;

	// 風シミュレーション（フィールドを吹き抜ける風の波面・突風・自然な木々のしなり）
	Vector3 EvaluateWind(const Vector3& pos, float time, float sway)
	{
		if (sway <= 0.001f) return { 0.0f, 0.0f, 0.0f };

		// 風の進行波（川に沿って左奥から右手前へ通り抜ける心地よい速度感）
		float spatial = (pos.x * 0.28f + pos.z * 0.40f) - time * 1.30f;

		// 突風のうねり（時折サァーッと風が強まるダイナミクス）
		float gust = 0.60f + 0.40f * std::sin(time * 0.55f + pos.x * 0.15f);
		float mainWave = std::sin(spatial) * gust;
		float subWave = std::sin(spatial * 1.9f + 1.2f) * 0.30f;

		// 木の葉・草先の自然なさざめき
		float flutter = std::sin(time * 3.2f + pos.x * 1.8f + pos.z * 1.2f) * 0.014f;

		// 「風が吹いている」としっかり実感できる心地よい振幅
		float totalDisp = (mainWave + subWave) * 0.090f + flutter;

		// 風向ベクトル (+0.92, 0, +0.38)
		Vector3 disp;
		disp.x = totalDisp * 0.92f * sway;
		disp.z = totalDisp * 0.38f * sway;
		disp.y = -std::abs(totalDisp) * 0.10f * sway;

		return disp;
	}

	float Hash01(uint32_t& s)
	{
		s = s * 1664525u + 1013904223u;
		return static_cast<float>(s >> 8) * (1.0f / 16777216.0f);
	}
	float HashRange(uint32_t& s, float a, float b) { return a + (b - a) * Hash01(s); }

	// フラットシェーディング用の三角形（法線は形状中心から外向きに揃える）
	void MeshTri(Object3d::ModelData& d, const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& center)
	{
		Vector3 u = b - a, v = c - a;
		Vector3 n = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
		float l = Length3(n);
		if (l < 1e-7f) return;
		n = n * (1.0f / l);
		Vector3 fc = { (a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f };
		Vector3 o = fc - center;
		if (n.x * o.x + n.y * o.y + n.z * o.z < 0.0f) n = n * -1.0f;
		uint32_t base = static_cast<uint32_t>(d.vertices.size());
		for (const Vector3* p : { &a, &b, &c })
		{
			Sprite::VertexData vtx{};
			vtx.position = { p->x, p->y, p->z, 1.0f };
			vtx.texcoord = { 0.5f, 0.5f };
			vtx.normal = n;
			d.vertices.push_back(vtx);
		}
		d.indices.push_back(base);
		d.indices.push_back(base + 1);
		d.indices.push_back(base + 2);
	}

	// でこぼこした低ポリ球（岩・茂み・樹冠）
	void MeshBlob(Object3d::ModelData& d, const Vector3& c, const Vector3& r, uint32_t seed, float jitter)
	{
		constexpr int R = 4, S = 7;
		Vector3 p[R + 1][S];
		for (int i = 0; i <= R; ++i)
		{
			float phi = kPi * static_cast<float>(i) / R;
			float k = (i == 0 || i == R) ? 1.0f : 1.0f + HashRange(seed, -jitter, jitter);
			for (int j = 0; j < S; ++j)
			{
				float th = 2.0f * kPi * (static_cast<float>(j) + (i % 2) * 0.5f) / S;
				float kk = (i == 0 || i == R) ? 1.0f : k * (1.0f + HashRange(seed, -jitter, jitter) * 0.5f);
				p[i][j] = { c.x + r.x * kk * std::sin(phi) * std::cos(th), c.y + r.y * kk * std::cos(phi), c.z + r.z * kk * std::sin(phi) * std::sin(th) };
			}
		}
		for (int i = 0; i < R; ++i)
		{
			for (int j = 0; j < S; ++j)
			{
				int jn = (j + 1) % S;
				MeshTri(d, p[i][j], p[i][jn], p[i + 1][jn], c);
				MeshTri(d, p[i][j], p[i + 1][jn], p[i + 1][j], c);
			}
		}
	}

	void MeshCone(Object3d::ModelData& d, const Vector3& base, float radius, float height, uint32_t seed)
	{
		constexpr int S = 7;
		Vector3 apex = { base.x + HashRange(seed, -0.04f, 0.04f), base.y + height, base.z + HashRange(seed, -0.04f, 0.04f) };
		Vector3 center = { base.x, base.y + height * 0.3f, base.z };
		float rot = HashRange(seed, 0.0f, 2.0f * kPi);
		Vector3 ring[S];
		for (int j = 0; j < S; ++j)
		{
			float th = rot + 2.0f * kPi * j / S;
			float rr = radius * (1.0f + HashRange(seed, -0.12f, 0.12f));
			ring[j] = { base.x + rr * std::cos(th), base.y + HashRange(seed, -0.05f, 0.05f), base.z + rr * std::sin(th) };
		}
		for (int j = 0; j < S; ++j)
		{
			MeshTri(d, ring[j], ring[(j + 1) % S], apex, center);
			MeshTri(d, ring[j], ring[(j + 1) % S], base, center); // 下面（見上げ時の抜け防止）
		}
	}

	void MeshCylinder(Object3d::ModelData& d, const Vector3& base, float r0, float r1, float height)
	{
		constexpr int S = 6;
		Vector3 center = { base.x, base.y + height * 0.5f, base.z };
		for (int j = 0; j < S; ++j)
		{
			float a0 = 2.0f * kPi * j / S, a1 = 2.0f * kPi * (j + 1) / S;
			Vector3 b0 = { base.x + r0 * std::cos(a0), base.y, base.z + r0 * std::sin(a0) };
			Vector3 b1 = { base.x + r0 * std::cos(a1), base.y, base.z + r0 * std::sin(a1) };
			Vector3 t0 = { base.x + r1 * std::cos(a0), base.y + height, base.z + r1 * std::sin(a0) };
			Vector3 t1 = { base.x + r1 * std::cos(a1), base.y + height, base.z + r1 * std::sin(a1) };
			MeshTri(d, b0, b1, t1, center);
			MeshTri(d, b0, t1, t0, center);
		}
	}

	// 細い葉（草・葦）：先細りの三角柱
	void MeshBlade(Object3d::ModelData& d, const Vector3& base, float width, float height, float leanX, float leanZ)
	{
		Vector3 tip = { base.x + leanX, base.y + height, base.z + leanZ };
		Vector3 center = { base.x + leanX * 0.3f, base.y + height * 0.3f, base.z + leanZ * 0.3f };
		Vector3 b[3];
		for (int j = 0; j < 3; ++j)
		{
			float th = 2.0f * kPi * j / 3.0f;
			b[j] = { base.x + width * std::cos(th), base.y, base.z + width * std::sin(th) };
		}
		for (int j = 0; j < 3; ++j) MeshTri(d, b[j], b[(j + 1) % 3], tip, center);
	}

	void Spawn(AppParticleManager* pm, const Vector3& pos, const Vector3& vel, const Vector4& col,
		float s0, float s1, float life, uint32_t tex,
		float gravity = 0.0f, float drag = 0.0f, float curl = 0.0f,
		const Vector4* endCol = nullptr, float bounce = 0.0f)
	{
		if (!pm || tex == UINT32_MAX) return;
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, 2.0f * kPi) });
		p.transform.SetScale({ s0, s0, 1.0f });
		p.velocity = vel;
		p.color = col;
		p.lifeTime = life;
		p.currentTime = 0.0f;
		p.textureIndex = tex;
		p.gravity = gravity;
		p.drag = drag;
		p.bounceElasticity = bounce;
		p.initialScale = (std::max)(s0, 0.001f);
		p.targetScale = (std::max)(s1, 0.001f);
		if (curl > 0.0f)
		{
			p.curlFreq = 1.6f;
			p.curlAmp = curl;
		}
		if (endCol)
		{
			p.endColor = *endCol;
			p.hasColorShift = true;
		}
		pm->AddParticle(p);
	}

	// ---------------------------------------------------------------
	// 風に舞い散るリアルな青葉・木の葉（実体3Dオブジェクト）
	// 加算ブレンドパーティクル（火花・光粒・花粉）を完全排除し、
	// 太陽光と陰影を持つ本物の緑の木の葉が3Dで翻りながら美しく舞い散る
	// ---------------------------------------------------------------
	struct FallingLeaf3D
	{
		std::unique_ptr<Object3d> model;
		Vector3 pos{ 0.0f, 0.0f, 0.0f };
		Vector3 baseVel{ 0.0f, 0.0f, 0.0f };
		Vector3 rot{ 0.0f, 0.0f, 0.0f };
		Vector3 rotSpeed{ 0.0f, 0.0f, 0.0f };
		Vector3 wobbleDir{ -0.38f, 0.0f, 0.92f };
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float scale = 0.32f;
		float age = 0.0f;
		float maxLife = 8.0f;
		float wobbleFreq = 3.0f;
		float wobbleAmp = 0.6f;
		float wobblePhase = 0.0f;
		bool active = false;
	};
	constexpr size_t kMaxFallingLeaves = 24;
	std::vector<FallingLeaf3D> sFallingLeaves;

}

void TitleScene::InitializeScene()
{
	if (CollisionManager::GetInstance())
	{
		CollisionManager::GetInstance()->SetShowDebugColliders(false);
		CollisionManager::GetInstance()->SetShowMeshWireframe(false);
	}
	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetShowSkybox(true);
	}
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		// BGM (162秒ごとに再開してループ、キャッシュ保持で再ロードゼロ化)
		if (sBgmSoundId < 0)
		{
			sBgmSoundId = am->Load("Resources/Audio/BGM_Title.mp3");
		}
		if (sBgmSoundId >= 0 && sBgmPlayId < 0)
		{
			sBgmPlayId = am->Play(sBgmSoundId);
			sBgmTimer = 0.0f;
		}

		// アヒルSE (SPACEキー押下時、キャッシュ保持で再ロードゼロ化)
		if (sDuckSeSoundId < 0)
		{
			sDuckSeSoundId = am->Load("Resources/Audio/SE_Duck.mp3");
			if (sDuckSeSoundId < 0)
			{
				sDuckSeSoundId = am->Load("Resources/Alarm01.wav");
			}
		}
	}

	if (dxCommon_)
	{
		input_ = new KeyInput();
		input_->Initialize(dxCommon_->GetWindowAPI());
		mouseInput_.Initialize(dxCommon_->GetWindowAPI());
	}
	hasPrevMousePos_ = false;
	mouseRippleCooldown_ = 0.0f;
	mouseWaveEmitCooldown_ = 0.0f;
	lastTime_ = std::chrono::steady_clock::now();
	bgTimer_ = 0.0f;
	startTransitionTimer_ = 0.0f;
	isStarting_ = false;
	sCameraLocked = false;
	sDuckSePlayId = -1;

	sIntroTimer = 0.0f;
	sLogoLanded = false;
	sShakeTimer = 0.0f;
	sShakePower = 0.0f;
	sFlashTimer = 0.0f;
	sDuckRecoilTimer = 0.0f;
	sRiverWaveTimer = 0.0f;
	sRiverWakeTimer = 0.0f;
	sRiverGlintTimer = 0.0f;
	sLeafTimer = 0.0f;

	if (GetParticleManager())
	{
		appParticleManager_ = std::make_unique<AppParticleManager>();
		appParticleManager_->Initialize(GetParticleManager());

		circleTexIndex_ = TextureManager::GetInstance()->Load("Resources/circle2.png");
		if (circleTexIndex_ == UINT32_MAX)
		{
			circleTexIndex_ = TextureManager::GetInstance()->Load("Resources/CG4/circle2.png");
		}
		starTexIndex_ = TextureManager::GetInstance()->Load("Resources/starburst.png");
		sSmokeTexIndex = TextureManager::GetInstance()->Load("Resources/smoke_dark.png");

		// 静謐なジオラマ用パーティクル設定
		appParticleManager_->SetGroundY(kLandY);
		appParticleManager_->SetPremultiplyAlpha(true);
		sFx.circle = circleTexIndex_;
		sFx.star = starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_;
		sFx.puff = TextureManager::GetInstance()->Load("Resources/title_smoke_puff.png");
		if (sFx.puff == UINT32_MAX) sFx.puff = circleTexIndex_;
		sFx.leaf = TextureManager::GetInstance()->Load("Resources/leaf.png");
		if (sFx.leaf == UINT32_MAX) sFx.leaf = circleTexIndex_;
	}

	// ---------------------------------------------------------------
	// カメラ初期化（B案＋C案：鳥瞰ジオラマ構図 FOV 0.70rad）
	// ---------------------------------------------------------------
	cameraTranslate_ = kCamFinalPos;
	cameraRotate_ = kCamFinalRot;
	if (camera_)
	{
		camera_->SetFovY(0.70f);
		camera_->SetTranslate(kCamStartPos);
		camera_->SetRotate(kCamStartRot);
		camera_->Update();
	}

	if (GetObject3dCom() && camera_)
	{
		Object3d::ModelData duckData = GetOrLoadModelData("Resources", "player.obj");
		if (duckData.material.textureFilePath.empty())
		{
			duckData.material.textureFilePath = "Resources/duck.png";
		}
		duckData.boundingRadius = 10.0f;

		// =========================================================
		// 1. 主役アヒル（画面右手前：土嚢の陰から対岸を見つめる）
		// =========================================================
		duckModel_ = std::make_unique<Object3d>();
		duckModel_->Initialize(GetObject3dCom(), duckData);
		duckModel_->SetCamera(camera_);
		duckBasePos_ = { 3.30f, kLandY, 0.90f };
		duckScale_ = 0.75f;
		duckModel_->SetScale({ duckScale_, duckScale_, duckScale_ });
		duckModel_->SetTranslate(duckBasePos_);
		duckModel_->SetRotate({ 0.0f, kDuckFacingYaw, 0.0f });
		duckModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		duckModel_->SetEnableLighting(true);

		// =========================================================
		// 2. 川（中景を横切る清流：俯瞰により川幅と水面反射が美しく映える）
		// =========================================================
		{
			Object3d::ModelData riverData{};
			riverData.material.textureFilePath = "Resources/water.png";
			riverTexIndex_ = TextureManager::GetInstance()->Load("Resources/water.png");
			riverData.material.textureIndex = riverTexIndex_;
			riverData.boundingRadius = 60.0f;

			const float kStartX = -20.0f, kSpanX = 40.0f;
			const float kStartZ = 0.5f, kSpanZ = 10.5f;

			waterBaseVertices_.clear();
			waterBaseVertices_.reserve(kWaterGridCols * kWaterGridRows);
			riverData.vertices.reserve(kWaterGridCols * kWaterGridRows);

			for (int r = 0; r < kWaterGridRows; ++r)
			{
				float rz = static_cast<float>(r) / static_cast<float>(kWaterGridRows - 1);
				float posZ = kStartZ + rz * kSpanZ;
				for (int c = 0; c < kWaterGridCols; ++c)
				{
					float rx = static_cast<float>(c) / static_cast<float>(kWaterGridCols - 1);
					float posX = kStartX + rx * kSpanX;

					WaterVertexBase base{};
					base.basePosX = posX;
					base.basePosZ = posZ;
					base.baseU = posX * 0.22f;
					base.baseV = posZ * 0.22f;
					waterBaseVertices_.push_back(base);

					Sprite::VertexData vtx{};
					vtx.position = { posX, kWaterY, posZ, 1.0f };
					vtx.texcoord = { base.baseU, base.baseV };
					vtx.normal = { 0.0f, 1.0f, 0.0f };
					riverData.vertices.push_back(vtx);
				}
			}
			BuildGridIndices(riverData, kWaterGridCols, kWaterGridRows);

			riverModel_ = std::make_unique<Object3d>();
			riverModel_->Initialize(GetObject3dCom(), riverData);
			riverModel_->SetCamera(camera_);
			riverModel_->SetColor({ 0.42f, 0.82f, 0.94f, 0.90f });
			riverModel_->SetEnableLighting(true);
			riverModel_->SetReflectionFactor(0.82f);
			riverModel_->SetFresnelF0(0.04f);
			riverModel_->SetAllowWireframeOverlay(false);
		}

		// =========================================================
		// 3. 地形（手前岸 / 対岸）
		// =========================================================
		sGrassTexIndex = TextureManager::GetInstance()->Load("Resources/grass.png");
		const int kCols = 40;
		const float kXMin = -20.0f, kXMax = 20.0f;

		// (A) 手前岸
		{
			Object3d::ModelData data{};
			data.material.textureFilePath = "Resources/grass.png";
			data.material.textureIndex = sGrassTexIndex;
			data.boundingRadius = 50.0f;
			const int kRows = 12;
			for (int r = 0; r < kRows; ++r)
			{
				for (int c = 0; c < kCols; ++c)
				{
					float posX = kXMin + (kXMax - kXMin) * static_cast<float>(c) / static_cast<float>(kCols - 1);
					float edge = NearShoreZ(posX);
					float posZ, posY;
					Vector3 n = { 0.0f, 1.0f, 0.0f };
					if (r < 6)
					{
						float t = static_cast<float>(r) / 5.0f;
						posZ = -10.0f + t * (edge - 0.05f + 10.0f);
						posY = kLandY;
					}
					else
					{
						float t = static_cast<float>(r - 6) / 5.0f;
						float s = t * t * (3.0f - 2.0f * t);
						posZ = (edge - 0.05f) + t * 0.50f;
						posY = kLandY + (kRiverBedY - kLandY) * s;
						float dYdt = (kRiverBedY - kLandY) * 6.0f * t * (1.0f - t);
						n = SlopeNormalZ(dYdt / 0.50f);
					}
					Sprite::VertexData v{};
					v.position = { posX, posY, posZ, 1.0f };
					v.texcoord = { posX * 0.30f, posZ * 0.30f };
					v.normal = n;
					data.vertices.push_back(v);
				}
			}
			BuildGridIndices(data, kCols, kRows);
			sNearGroundModel = std::make_unique<Object3d>();
			sNearGroundModel->Initialize(GetObject3dCom(), data);
			sNearGroundModel->SetCamera(camera_);
			sNearGroundModel->SetColor({ 0.88f, 0.90f, 0.88f, 1.0f });
			sNearGroundModel->SetEnableLighting(true);
			sNearGroundModel->SetAllowWireframeOverlay(false);
		}

		// (B) 対岸
		{
			Object3d::ModelData data{};
			data.material.textureFilePath = "Resources/grass.png";
			data.material.textureIndex = sGrassTexIndex;
			data.boundingRadius = 50.0f;
			const int kRows = 12;
			for (int r = 0; r < kRows; ++r)
			{
				for (int c = 0; c < kCols; ++c)
				{
					float posX = kXMin + (kXMax - kXMin) * static_cast<float>(c) / static_cast<float>(kCols - 1);
					float edge = FarShoreZ(posX);
					float posZ, posY;
					Vector3 n = { 0.0f, 1.0f, 0.0f };
					if (r < 6)
					{
						float t = static_cast<float>(r) / 5.0f;
						float s = t * t * (3.0f - 2.0f * t);
						posZ = (edge - 0.45f) + t * 0.50f;
						posY = kRiverBedY + (kFarLandY - kRiverBedY) * s;
						float dYdt = (kFarLandY - kRiverBedY) * 6.0f * t * (1.0f - t);
						n = SlopeNormalZ(dYdt / 0.50f);
					}
					else
					{
						float t = static_cast<float>(r - 6) / 5.0f;
						posZ = (edge + 0.05f) + t * (34.0f - edge);
						posY = kFarLandY;
					}
					Sprite::VertexData v{};
					v.position = { posX, posY, posZ, 1.0f };
					v.texcoord = { posX * 0.30f, posZ * 0.30f };
					v.normal = n;
					data.vertices.push_back(v);
				}
			}
			BuildGridIndices(data, kCols, kRows);
			sFarGroundModel = std::make_unique<Object3d>();
			sFarGroundModel->Initialize(GetObject3dCom(), data);
			sFarGroundModel->SetCamera(camera_);
			sFarGroundModel->SetColor({ 0.86f, 0.88f, 0.86f, 1.0f });
			sFarGroundModel->SetEnableLighting(true);
			sFarGroundModel->SetAllowWireframeOverlay(false);
		}

		// =========================================================
		// 4. 手前岸の物資プロップ（アヒル分隊の左側に配置）
		// =========================================================
		{
			Object3d::ModelData crateData = GetOrLoadModelData("Resources", "duckov_crate.obj");
			crateData.material.textureFilePath = "Resources/duckov_crate.png";
			crateTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_crate.png");
			crateData.material.textureIndex = crateTexIndex_;
			crateData.boundingRadius = 6.0f;

			crateModel1_ = std::make_unique<Object3d>();
			crateModel1_->Initialize(GetObject3dCom(), crateData);
			crateModel1_->SetCamera(camera_);
			crateModel1_->SetScale({ 0.58f, 0.58f, 0.58f });
			crateModel1_->SetTranslate({ 4.40f, kLandY, 1.60f });
			crateModel1_->SetRotate({ 0.0f, 0.35f, 0.0f });

			crateModel2_ = std::make_unique<Object3d>();
			crateModel2_->Initialize(GetObject3dCom(), crateData);
			crateModel2_->SetCamera(camera_);
			crateModel2_->SetScale({ 0.42f, 0.42f, 0.42f });
			crateModel2_->SetTranslate({ 4.45f, kLandY + 0.70f, 1.63f }); // 下段の高さ(1.2×0.58)の上に載せる
			crateModel2_->SetRotate({ 0.0f, -0.15f, 0.0f });
		}
		{
			Object3d::ModelData sandData = GetOrLoadModelData("Resources", "duckov_sandbag_wall.obj");
			sandData.material.textureFilePath = "Resources/duckov_sandbag.png";
			sandbagTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_sandbag.png");
			sandData.material.textureIndex = sandbagTexIndex_;
			sandData.boundingRadius = 10.0f;
			sandbagModel_ = std::make_unique<Object3d>();
			sandbagModel_->Initialize(GetObject3dCom(), sandData);
			sandbagModel_->SetCamera(camera_);
			sandbagModel_->SetScale({ 0.46f, 0.46f, 0.46f });
			sandbagModel_->SetTranslate({ 2.95f, kLandY, 1.85f });
			sandbagModel_->SetRotate({ 0.0f, 0.15f, 0.0f });
		}

		// =========================================================
		// 5. 【敵側ステージ陣地 & 中央木造橋】整然としたタクティカルゾーン配置
		// =========================================================
		sSignpostTexIndex = TextureManager::GetInstance()->Load("Resources/signpost.png");
		sTargetTexIndex = TextureManager::GetInstance()->Load("Resources/target.png");
		sBridgeTexIndex = TextureManager::GetInstance()->Load("Resources/duckov_bridge.png");
		barrelTexIndex_ = TextureManager::GetInstance()->Load("Resources/duckov_barrel.png");
		containerTexIndex_ = TextureManager::GetInstance()->Load("Resources/container_military.png");
		sWatchtowerTexIndex = TextureManager::GetInstance()->Load("Resources/duckov_crate.png");

		Object3d::ModelData bridgeData = GetOrLoadModelData("Resources", "bridge.obj");
		Object3d::ModelData signpostData = GetOrLoadModelData("Resources", "signpost.obj");
		Object3d::ModelData targetData = GetOrLoadModelData("Resources", "shooting_target.obj");
		Object3d::ModelData crateData = GetOrLoadModelData("Resources", "duckov_crate.obj");
		Object3d::ModelData sandData = GetOrLoadModelData("Resources", "duckov_sandbag_wall.obj");
		Object3d::ModelData barrelData = GetOrLoadModelData("Resources", "duckov_barrel_stack.obj");
		Object3d::ModelData containerData = GetOrLoadModelData("Resources", "container.obj");
		Object3d::ModelData towerData = GetOrLoadModelData("Resources", "watchtower.obj");
		Object3d::ModelData padData = GetOrLoadModelData("Resources", "extraction_pad.obj");
		Object3d::ModelData enemyDuckData = GetOrLoadModelData("Resources", "player.obj");

		sEnemyProps.clear();

		auto addEnemyProp = [&](const Object3d::ModelData& mData, uint32_t tex, const Vector3& pos, const Vector3& scale, const Vector3& rot, const Vector4& col = {1.0f, 1.0f, 1.0f, 1.0f}) {
			PropInstance p;
			p.model = std::make_unique<Object3d>();
			p.model->Initialize(GetObject3dCom(), mData);
			p.model->SetCamera(camera_);
			p.model->SetTranslate(pos);
			p.model->SetScale(scale);
			p.model->SetRotate(rot);
			p.model->SetColor(col);
			p.model->SetEnableLighting(true);
			p.textureIndex = tex;
			sEnemyProps.push_back(std::move(p));
		};

		// ---------------------------------------------------------
		// [川を跨ぎ対岸と繋ぐ木造橋梁（中央）]
		// ---------------------------------------------------------
		addEnemyProp(bridgeData, sBridgeTexIndex, { 0.60f, kFarLandY + 0.06f, 5.0f }, { 0.62f, 0.55f, 0.72f }, { 0.0f, 0.0f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 1: 渡河橋頭堡検問所（橋の出口を左右から挟む。橋板・小道にはかぶせない）]
		// ---------------------------------------------------------
		addEnemyProp(sandData, sandbagTexIndex_, { -0.50f, kFarLandY, 8.70f }, { 0.56f, 0.56f, 0.56f }, { 0.0f, 0.0f, 0.0f });
		addEnemyProp(signpostData, sSignpostTexIndex, { -0.75f, kFarLandY, 8.10f }, { 0.45f, 0.45f, 0.45f }, { 0.0f, 0.25f, 0.0f });
		addEnemyProp(barrelData, barrelTexIndex_, { 2.30f, kFarLandY, 8.50f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, 0.20f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 2: 左翼軍用コンテナ物流拠点]
		// ---------------------------------------------------------
		addEnemyProp(containerData, containerTexIndex_, { -4.80f, kFarLandY, 12.40f }, { 0.80f, 0.80f, 0.80f }, { 0.0f, 0.38f, 0.0f });
		// 上段は下段の高さ(2.2×0.8)の上に載せる
		addEnemyProp(containerData, containerTexIndex_, { -4.75f, kFarLandY + 1.77f, 12.45f }, { 0.76f, 0.76f, 0.76f }, { 0.0f, 0.36f, 0.0f });
		addEnemyProp(containerData, containerTexIndex_, { -2.00f, kFarLandY, 13.20f }, { 0.78f, 0.78f, 0.78f }, { 0.0f, -0.22f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { -1.60f, kFarLandY, 10.40f }, { 0.65f, 0.65f, 0.65f }, { 0.0f, 0.15f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { -1.55f, kFarLandY + 0.79f, 10.50f }, { 0.50f, 0.50f, 0.50f }, { 0.0f, -0.20f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 3: 右翼高台の木造監視塔（脚の間に物を置かない）]
		// ---------------------------------------------------------
		addEnemyProp(towerData, sWatchtowerTexIndex, { 4.20f, kFarLandY, 12.00f }, { 0.85f, 0.85f, 0.85f }, { 0.0f, -0.30f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { 6.30f, kFarLandY, 11.20f }, { 0.65f, 0.65f, 0.65f }, { 0.0f, -0.10f, 0.0f });
		addEnemyProp(barrelData, barrelTexIndex_, { 6.20f, kFarLandY, 12.70f }, { 0.60f, 0.60f, 0.60f }, { 0.0f, 0.30f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 4: 最奥の開けた脱出地点 & 射撃場]
		// ---------------------------------------------------------
		addEnemyProp(targetData, sTargetTexIndex, { -1.60f, kFarLandY, 16.10f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, -0.15f, 0.0f });
		addEnemyProp(targetData, sTargetTexIndex, { -0.10f, kFarLandY, 16.30f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, 0.20f, 0.0f });

		sExtractionPadTexIndex = TextureManager::GetInstance()->Load("Resources/extraction_pad.png");
		padData.material.textureIndex = sExtractionPadTexIndex;
		padData.boundingRadius = 15.0f;
		sExtractionPadModel = std::make_unique<Object3d>();
		sExtractionPadModel->Initialize(GetObject3dCom(), padData);
		sExtractionPadModel->SetCamera(camera_);
		sExtractionPadModel->SetScale({ 0.80f, 0.80f, 0.80f });
		sExtractionPadModel->SetTranslate({ 2.70f, kFarLandY, 15.70f });
		sExtractionPadModel->SetEnableLighting(true);
		sExtractionPadModel->SetAllowWireframeOverlay(false);

		// ---------------------------------------------------------
		// [敵兵アヒル小隊：役割に応じた位置配備]
		// ---------------------------------------------------------
		sEnemyDucks.clear();

		auto addEnemyDuck = [&](const Vector3& pos, float facing, EnemyBehaviorType type, float rx = 0.0f, float rz = 0.0f, float spd = 1.0f, float phase = 0.0f) {
			EnemyDuckInstance ed;
			ed.model = std::make_unique<Object3d>();
			ed.model->Initialize(GetObject3dCom(), enemyDuckData);
			ed.model->SetCamera(camera_);
			ed.model->SetScale({ 0.46f, 0.46f, 0.46f });
			ed.model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			ed.basePos = pos;
			ed.baseFacing = facing;
			ed.behavior = type;
			ed.radiusX = rx;
			ed.radiusZ = rz;
			ed.speed = spd;
			ed.phaseOffset = phase;
			sEnemyDucks.push_back(std::move(ed));
		};

		// 1. 検問所の歩哨（橋の出口正面で橋・川を鋭く警戒）
		addEnemyDuck({ 0.90f, kFarLandY, 9.40f }, kPi * 0.96f, EnemyBehaviorType::SentryLookout, 0.0f, 0.0f, 0.55f, 0.0f);
		// 2. コンテナヤード前の巡回兵（滑らかな細長楕円パトロール、進行方向を向く）
		addEnemyDuck({ 0.05f, kFarLandY, 10.85f }, kPi, EnemyBehaviorType::OvalPatrol, 0.42f, 0.16f, 0.35f, 1.2f);
		// 3. 監視塔手前の警戒兵（手前岸の主役アヒル方向を狙い警戒）
		addEnemyDuck({ 2.40f, kFarLandY, 9.90f }, kPi - 0.35f, EnemyBehaviorType::StandGuard, 0.0f, 0.0f, 0.70f, 2.4f);
		// 4. 最奥脱出地点手前の斥候兵（小道沿いで手前と奥を交互に索敵）
		addEnemyDuck({ 0.60f, kFarLandY, 13.20f }, kPi * 0.90f, EnemyBehaviorType::ScoutScan, 0.0f, 0.0f, 0.45f, 3.6f);
		// 5. 対岸の警戒ラインを左右に堂々と横断行軍するパトロール兵（近く・視界内を通り過ぎる敵の演出）
		addEnemyDuck({ 0.0f, kFarLandY, 11.20f }, kPi * 0.5f, EnemyBehaviorType::PatrolAcross, 4.2f, 0.0f, 0.32f, 0.0f);

		// =========================================================
		// 6. 接地影デカール（静的オブジェクト＆主役アヒルの影を一括バッチ生成）
		// =========================================================
		{
			Object3d::ModelData shadowData{};
			shadowData.material.textureFilePath = "Resources/circle2.png";
			shadowData.material.textureIndex = circleTexIndex_;
			shadowData.boundingRadius = 60.0f;

			// 主役アヒル（手前岸）
			AddShadowDisc(shadowData, 3.30f, kLandY + 0.012f, 0.90f, 0.48f, 0.48f);

			// 手前岸の物資
			AddShadowDisc(shadowData, 4.40f, kLandY + 0.012f, 1.60f, 0.45f, 0.45f);
			AddShadowDisc(shadowData, 2.95f, kLandY + 0.012f, 1.85f, 0.55f, 0.35f);

			// 渡河木造橋の両端（岸に掛かる部分）
			AddShadowDisc(shadowData, 0.60f, kLandY + 0.012f, 2.30f, 1.05f, 0.30f);
			AddShadowDisc(shadowData, 0.60f, kFarLandY + 0.012f, 7.75f, 1.05f, 0.30f);

			// 対岸検問所（Zone 1）
			AddShadowDisc(shadowData, -0.50f, kFarLandY + 0.012f, 8.70f, 0.60f, 0.30f);
			AddShadowDisc(shadowData, -0.75f, kFarLandY + 0.012f, 8.10f, 0.28f, 0.28f);
			AddShadowDisc(shadowData, 2.30f, kFarLandY + 0.012f, 8.50f, 0.48f, 0.40f);

			// 左翼コンテナヤード（Zone 2）
			AddShadowDisc(shadowData, -4.80f, kFarLandY + 0.012f, 12.40f, 1.10f, 1.40f);
			AddShadowDisc(shadowData, -2.00f, kFarLandY + 0.012f, 13.20f, 1.10f, 1.30f);
			AddShadowDisc(shadowData, -1.60f, kFarLandY + 0.012f, 10.40f, 0.55f, 0.55f);

			// 右翼監視塔（Zone 3）
			AddShadowDisc(shadowData, 3.6f, kFarLandY + 0.012f, 11.4f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 4.8f, kFarLandY + 0.012f, 11.4f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 3.6f, kFarLandY + 0.012f, 12.6f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 4.8f, kFarLandY + 0.012f, 12.6f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 6.30f, kFarLandY + 0.012f, 11.20f, 0.48f, 0.48f);
			AddShadowDisc(shadowData, 6.20f, kFarLandY + 0.012f, 12.70f, 0.48f, 0.40f);

			// 最奥脱出地点・射撃標的（Zone 4）
			AddShadowDisc(shadowData, 2.70f, kFarLandY + 0.012f, 15.70f, 1.35f, 1.35f);
			AddShadowDisc(shadowData, -1.60f, kFarLandY + 0.012f, 16.10f, 0.35f, 0.35f);
			AddShadowDisc(shadowData, -0.10f, kFarLandY + 0.012f, 16.30f, 0.35f, 0.35f);

			sShadowDecalsModel = std::make_unique<Object3d>();
			sShadowDecalsModel->Initialize(GetObject3dCom(), shadowData);
			sShadowDecalsModel->SetCamera(camera_);
			sShadowDecalsModel->SetColor({ 0.05f, 0.06f, 0.08f, 0.52f });
			sShadowDecalsModel->SetEnableLighting(false);
			sShadowDecalsModel->SetAllowWireframeOverlay(false);

			// 敵兵アヒル個別の追従影モデル
			sEnemyDuckShadows.clear();
			Object3d::ModelData singleShadowData{};
			singleShadowData.material.textureFilePath = "Resources/circle2.png";
			singleShadowData.material.textureIndex = circleTexIndex_;
			singleShadowData.boundingRadius = 5.0f;
			AddShadowDisc(singleShadowData, 0.0f, 0.0f, 0.0f, 0.32f, 0.32f);

			for (size_t i = 0; i < sEnemyDucks.size(); ++i)
			{
				auto shadow = std::make_unique<Object3d>();
				shadow->Initialize(GetObject3dCom(), singleShadowData);
				shadow->SetCamera(camera_);
				shadow->SetColor({ 0.05f, 0.06f, 0.08f, 0.50f });
				shadow->SetEnableLighting(false);
				shadow->SetAllowWireframeOverlay(false);
				sEnemyDuckShadows.push_back(std::move(shadow));
			}
		}

		// =========================================================
		// 7. 環境装飾（木立・茂み・岩・葦・草むら・土の小道）
		//    更地感を無くし、生きたフィールドとしての密度を与える
		// =========================================================
		{
			sWhiteTexIndex = TextureManager::GetInstance()->Load("Resources/white.png");

			enum { kTrunk, kLeaf, kPine, kBush, kRock, kReed, kGrass, kLeafLight, kBatchCount };
			Object3d::ModelData batch[kBatchCount]{};
			for (auto& b : batch)
			{
				b.material.textureFilePath = "Resources/white.png";
				b.material.textureIndex = sWhiteTexIndex;
				b.boundingRadius = 60.0f;
			}
			uint32_t seed = 20261006u;

			// 小道（手前岸：画面右手前→橋のたもと / 対岸：橋→脱出地点）
			const std::vector<Vector2> nearPath = { {4.6f,-2.0f},{3.9f,-0.6f},{2.9f,0.5f},{1.9f,1.3f},{1.0f,1.9f},{0.6f,2.3f} };
			const std::vector<Vector2> farPath = { {0.6f,7.8f},{1.2f,10.0f},{2.0f,12.2f},{2.6f,13.8f} };
			auto distToPath = [](const std::vector<Vector2>& path, float x, float z) {
				float best = 1e9f;
				for (size_t i = 0; i + 1 < path.size(); ++i)
				{
					float ax = path[i].x, az = path[i].y, bx = path[i + 1].x, bz = path[i + 1].y;
					float dx = bx - ax, dz = bz - az;
					float t = std::clamp(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz), 0.0f, 1.0f);
					float px = ax + dx * t - x, pz = az + dz * t - z;
					best = (std::min)(best, std::sqrt(px * px + pz * pz));
				}
				return best;
			};

			// --- 配置済みプロップの占有領域（植生がめり込まないよう押し出し判定に使用） ---
			// 箱(hw,hd) + 余白半径 r の角丸矩形。値は各OBJの実寸×スケール（回転分は外接矩形で包含）
			struct Footprint { float x, z, hw, hd, r; };
			const Footprint props[] = {
				// 手前岸
				{ 3.30f, 0.90f, 0.00f, 0.00f, 0.50f }, // 主役アヒル
				{ 4.40f, 1.60f, 0.00f, 0.00f, 0.55f }, // 木箱
				{ 2.95f, 1.85f, 0.85f, 0.20f, 0.05f }, // 土嚢
				{ 0.60f, 5.00f, 0.95f, 2.90f, 0.05f }, // 木造橋
				// 対岸 Zone1 検問所
				{ -0.50f, 8.70f, 1.00f, 0.25f, 0.05f }, // 土嚢
				{ -0.75f, 8.10f, 0.45f, 0.15f, 0.05f }, // 標識
				{ 2.30f, 8.50f, 0.47f, 0.25f, 0.05f }, // ドラム缶
				{ 0.90f, 9.40f, 0.35f, 0.00f, 0.45f }, // 歩哨アヒル
				// Zone2 コンテナヤード
				{ -4.80f, 12.40f, 1.55f, 2.20f, 0.05f }, // コンテナ(下段・上段)
				{ -2.00f, 13.20f, 1.27f, 2.10f, 0.05f }, // コンテナ
				{ -1.60f, 10.40f, 0.00f, 0.00f, 0.55f }, // 木箱
				{ 0.05f, 10.85f, 0.55f, 0.00f, 0.45f }, // 巡回アヒル
				// Zone3 監視塔
				{ 4.20f, 12.00f, 1.55f, 1.55f, 0.05f }, // 監視塔
				{ 6.30f, 11.20f, 0.00f, 0.00f, 0.55f }, // 木箱
				{ 6.20f, 12.70f, 0.51f, 0.25f, 0.05f }, // ドラム缶
				{ 2.40f, 9.90f, 0.25f, 0.00f, 0.45f }, // 警戒アヒル
				// Zone4 脱出地点・射撃場（緑煙が木に隠れないよう広めに確保）
				{ 2.70f, 15.70f, 0.00f, 0.00f, 2.40f }, // 脱出パッド
				{ -1.60f, 16.10f, 0.42f, 0.12f, 0.10f }, // 標的
				{ -0.10f, 16.30f, 0.42f, 0.12f, 0.10f }, // 標的
				{ 0.60f, 13.20f, 0.40f, 0.00f, 0.45f }, // 斥候アヒル
			};
			// 点(x,z)から占有領域までの距離と押し出し方向
			auto footprintPush = [](const Footprint& f, float x, float z, float& outDist, float& nx, float& nz) {
				float dx = x - f.x, dz = z - f.z;
				float cx = std::clamp(dx, -f.hw, f.hw), cz = std::clamp(dz, -f.hd, f.hd);
				float ex = dx - cx, ez = dz - cz;
				float d = std::sqrt(ex * ex + ez * ez);
				if (d > 1e-4f) { outDist = d; nx = ex / d; nz = ez / d; return; }
				// 矩形内部：最も近い辺の外へ
				float px = f.hw - std::abs(dx), pz = f.hd - std::abs(dz);
				if (px < pz) { nx = dx >= 0.0f ? 1.0f : -1.0f; nz = 0.0f; outDist = -px; }
				else { nx = 0.0f; nz = dz >= 0.0f ? 1.0f : -1.0f; outDist = -pz; }
			};
			auto isBlocked = [&](float x, float z, float radius) {
				for (const auto& f : props)
				{
					float d, nx, nz;
					footprintPush(f, x, z, d, nx, nz);
					if (d < f.r + radius) return true;
				}
				return false;
			};
			// 植生の配置可否。手前岸はプロップの外へ押し出し、対岸は重なったら配置しない
			// （対岸で押し出すと陣地の真ん中に木が移動してプロップを隠してしまうため）
			auto resolvePlacement = [&](float& x, float& z, float radius) {
				const bool nearSide = z < 5.0f;
				if (!nearSide)
				{
					return z >= FarShoreZ(x) + 0.10f && !isBlocked(x, z, radius);
				}
				for (int iter = 0; iter < 8; ++iter)
				{
					bool moved = false;
					for (const auto& f : props)
					{
						float d, nx, nz;
						footprintPush(f, x, z, d, nx, nz);
						float need = f.r + radius;
						if (d < need)
						{
							x += nx * (need - d + 0.02f);
							z += nz * (need - d + 0.02f);
							moved = true;
						}
					}
					if (!moved) break;
				}
				if (isBlocked(x, z, radius * 0.95f)) return false;
				if (z > NearShoreZ(x) - 0.10f) return false;
				return true;
			};
			// 対岸の敵陣地エリア（ここには木を立てず、プロップが木に隠れないようにする）
			auto inCamp = [](float x, float z) {
				return x > -3.4f && x < 5.9f && z > 7.3f && z < 15.8f;
			};

			auto broadleaf = [&](float x, float z, float g, float s) {
				if (inCamp(x, z) || !resolvePlacement(x, z, 0.85f * s)) return;
				MeshCylinder(batch[kTrunk], { x, g - 0.05f, z }, 0.10f * s, 0.06f * s, 1.25f * s);
				MeshBlob(batch[kLeaf], { x, g + 1.45f * s, z }, { 0.75f * s, 0.60f * s, 0.75f * s }, seed++, 0.12f);
				MeshBlob(batch[kLeafLight], { x + 0.45f * s, g + 1.20f * s, z - 0.20f * s }, { 0.48f * s, 0.40f * s, 0.48f * s }, seed++, 0.15f);
				MeshBlob(batch[kLeaf], { x - 0.40f * s, g + 1.25f * s, z + 0.25f * s }, { 0.50f * s, 0.42f * s, 0.50f * s }, seed++, 0.15f);
				MeshBlob(batch[kLeafLight], { x + 0.05f * s, g + 1.95f * s, z + 0.05f * s }, { 0.45f * s, 0.38f * s, 0.45f * s }, seed++, 0.15f);
			};
			auto pine = [&](float x, float z, float g, float s) {
				if (inCamp(x, z) || !resolvePlacement(x, z, 0.62f * s)) return;
				MeshCylinder(batch[kTrunk], { x, g - 0.05f, z }, 0.07f * s, 0.05f * s, 0.55f * s);
				MeshCone(batch[kPine], { x, g + 0.35f * s, z }, 0.62f * s, 1.00f * s, seed++);
				MeshCone(batch[kPine], { x, g + 0.80f * s, z }, 0.48f * s, 0.85f * s, seed++);
				MeshCone(batch[kPine], { x, g + 1.22f * s, z }, 0.32f * s, 0.70f * s, seed++);
			};
			auto bush = [&](float x, float z, float g, float s) {
				if (!resolvePlacement(x, z, 0.60f * s)) return;
				MeshBlob(batch[kBush], { x, g + 0.18f * s, z }, { 0.42f * s, 0.30f * s, 0.38f * s }, seed++, 0.18f);
				MeshBlob(batch[kBush], { x + 0.30f * s, g + 0.12f * s, z + 0.10f * s }, { 0.28f * s, 0.22f * s, 0.26f * s }, seed++, 0.18f);
				MeshBlob(batch[kLeafLight], { x - 0.25f * s, g + 0.14f * s, z - 0.08f * s }, { 0.26f * s, 0.22f * s, 0.24f * s }, seed++, 0.18f);
			};
			auto rock = [&](float x, float z, float g, float s) {
				// 川の中の岩は判定不要（陸上の岩のみ押し出し）
				if (g > kWaterY && !resolvePlacement(x, z, 0.38f * s)) return;
				MeshBlob(batch[kRock], { x, g + 0.06f * s, z }, { 0.28f * s, 0.18f * s, 0.24f * s }, seed++, 0.22f);
				MeshBlob(batch[kRock], { x + 0.22f * s, g + 0.02f * s, z + 0.12f * s }, { 0.13f * s, 0.09f * s, 0.12f * s }, seed++, 0.25f);
			};
			auto tuft = [&](float x, float z, float g, float s, int kind) {
				int n = 4 + static_cast<int>(Hash01(seed) * 3.0f);
				for (int i = 0; i < n; ++i)
				{
					float a = HashRange(seed, 0.0f, 2.0f * kPi);
					float r = HashRange(seed, 0.0f, 0.07f) * s;
					float h = HashRange(seed, 0.14f, 0.26f) * s;
					MeshBlade(batch[kind], { x + std::cos(a) * r, g - 0.01f, z + std::sin(a) * r },
						0.018f * s, h, std::cos(a) * h * 0.35f, std::sin(a) * h * 0.35f);
				}
			};
			auto reeds = [&](float x, float z, float g) {
				int n = 6 + static_cast<int>(Hash01(seed) * 5.0f);
				for (int i = 0; i < n; ++i)
				{
					float bx = x + HashRange(seed, -0.22f, 0.22f), bz = z + HashRange(seed, -0.12f, 0.12f);
					float h = HashRange(seed, 0.45f, 0.85f);
					MeshBlade(batch[kReed], { bx, g, bz }, 0.016f, h, HashRange(seed, -0.10f, 0.10f), HashRange(seed, -0.08f, 0.08f));
				}
			};

			// --- 手前岸：画面を縁取る木立（左に大木と松、右端に松） ---
			broadleaf(-1.8f, -2.0f, kLandY, 2.7f); // 画面左上の角・最上部を手前から覆う大木
			broadleaf(-2.2f, -1.2f, kLandY, 2.3f); // 画面左上を手前から美しく覆う大木
			broadleaf(-2.8f, -1.8f, kLandY, 2.4f); // 左上角の重なりを厚くする
			broadleaf(-2.6f, 0.2f, kLandY, 1.7f);
			broadleaf(-4.2f, 0.5f, kLandY, 1.8f);
			pine(-1.1f, 1.5f, kLandY, 1.1f);
			pine(-3.0f, -0.6f, kLandY, 1.6f);
			broadleaf(-3.6f, 2.0f, kLandY, 1.3f);
			pine(6.0f, 2.0f, kLandY, 1.3f);
			pine(6.8f, 0.6f, kLandY, 1.0f);

			// --- 手前岸：茂み・岩 ---
			bush(-0.5f, 2.15f, kLandY, 1.0f);
			bush(5.35f, 1.20f, kLandY, 1.1f);
			bush(-1.9f, -0.6f, kLandY, 1.2f);
			bush(-3.3f, 1.0f, kLandY, 0.9f);
			bush(-2.5f, -1.0f, kLandY, 1.1f);
			rock(1.45f, 0.45f, kLandY, 0.8f);
			rock(5.25f, 0.30f, kLandY, 1.0f);
			rock(-1.55f, 2.30f, kLandY, 0.9f);
			rock(-0.4f, 0.2f, kLandY, 0.6f);
			// 川の中の岩（水面から頭を出す）
			rock(-2.2f, 4.2f, kWaterY - 0.12f, 1.3f);
			rock(2.6f, 5.6f, kWaterY - 0.12f, 1.0f);
			rock(4.4f, 3.4f, kWaterY - 0.12f, 1.1f);
			rock(-4.4f, 6.1f, kWaterY - 0.12f, 0.9f);

			// --- 両岸の葦（橋の周辺は空ける） ---
			for (float x = -8.5f; x <= 8.5f; x += 0.55f)
			{
				float jx = x + HashRange(seed, -0.2f, 0.2f);
				if (jx > -0.5f && jx < 1.7f) continue;
				if (Hash01(seed) < 0.65f) reeds(jx, NearShoreZ(jx) + 0.18f, kWaterY - 0.06f);
				if (Hash01(seed) < 0.55f) reeds(jx, FarShoreZ(jx) - 0.18f, kWaterY - 0.06f);
			}

			// --- 草むら（障害物・小道を避けて散らす） ---
			struct Avoid { float x, z, r; };
			const Avoid nearAvoid[] = {
				{3.30f,0.90f,0.65f},{4.40f,1.60f,0.55f},{2.95f,1.85f,0.60f},{-2.6f,0.2f,0.5f},{-1.1f,1.5f,0.3f},
			};
			int placed = 0;
			for (int tries = 0; tries < 450 && placed < 95; ++tries)
			{
				float x = HashRange(seed, -5.5f, 7.5f);
				float z = HashRange(seed, -2.5f, NearShoreZ(x) - 0.15f);
				bool ok = distToPath(nearPath, x, z) > 0.55f;
				for (const auto& a : nearAvoid)
				{
					float dx = x - a.x, dz = z - a.z;
					if (dx * dx + dz * dz < a.r * a.r) ok = false;
				}
				if (!ok || isBlocked(x, z, 0.12f)) continue;
				tuft(x, z, kLandY, HashRange(seed, 0.8f, 1.4f), (placed % 3 == 0) ? kReed : kGrass);
				++placed;
			}
			placed = 0;
			for (int tries = 0; tries < 450 && placed < 75; ++tries)
			{
				float x = HashRange(seed, -9.0f, 8.5f);
				float z = HashRange(seed, FarShoreZ(x) + 0.2f, 15.0f);
				if (distToPath(farPath, x, z) < 0.6f) continue;
				if (x > -6.4f && x < -0.7f && z > 10.2f && z < 15.3f) continue; // コンテナヤード
				if (x > 2.9f && x < 5.5f && z > 10.7f && z < 13.3f) continue;  // 監視塔
				if (std::abs(x - 2.7f) < 2.0f && std::abs(z - 15.7f) < 2.0f) continue; // 脱出地点
				if (z < 9.0f && x > -1.2f && x < 2.9f) continue; // 検問所
				if (isBlocked(x, z, 0.12f)) continue;
				tuft(x, z, kFarLandY, HashRange(seed, 1.0f, 1.6f), kGrass);
				++placed;
			}

			// --- 対岸：茂みと陣地脇の木立（左翼はコンテナの外側に森を密集させ、空白を埋める） ---
			// [左翼森林：コンテナとは間隔を空け、x≦-7 の帯に高い木を重ねる]
			pine(-8.0f, 9.6f, kFarLandY, 2.0f);
			pine(-8.6f, 11.8f, kFarLandY, 2.4f);
			pine(-8.2f, 13.9f, kFarLandY, 2.6f);
			pine(-9.8f, 9.9f, kFarLandY, 2.2f);
			pine(-10.6f, 13.0f, kFarLandY, 2.6f);
			broadleaf(-9.2f, 15.0f, kFarLandY, 2.0f);
			pine(-11.8f, 11.2f, kFarLandY, 2.5f);
			pine(-12.2f, 14.4f, kFarLandY, 2.8f);
			broadleaf(-6.2f, 16.6f, kFarLandY, 1.9f);

			bush(-6.2f, 7.95f, kFarLandY, 1.0f);
			bush(-6.8f, 8.6f, kFarLandY, 1.2f);
			bush(-8.6f, 8.4f, kFarLandY, 1.1f);
			rock(-0.9f, 9.6f, kFarLandY, 1.0f);
			rock(-6.9f, 9.9f, kFarLandY, 1.0f);
			rock(-7.9f, 8.9f, kFarLandY, 0.9f);

			// [中央・右翼の木立]
			bush(2.9f, 8.6f, kFarLandY, 1.0f);
			bush(5.8f, 9.2f, kFarLandY, 1.3f);
			broadleaf(8.0f, 9.6f, kFarLandY, 1.4f);
			pine(7.9f, 14.0f, kFarLandY, 1.8f);

			// --- 最奥：針葉樹林の背景（3列・左端-16mまで拡張しスカイラインを完全に覆う） ---
			for (int row = 0; row < 3; ++row)
			{
				for (float x = -16.0f; x <= 12.0f; x += HashRange(seed, 1.0f, 1.5f))
				{
					float z = 16.5f + row * 2.4f + HashRange(seed, -0.5f, 0.5f);
					float s = HashRange(seed, 2.0f, 3.0f) + row * 0.4f;
					if (Hash01(seed) < 0.28f) broadleaf(x, z, kFarLandY, s * 0.85f);
					else pine(x, z, kFarLandY, s);
				}
			}

			const Vector4 colors[kBatchCount] = {
				{ 0.42f, 0.30f, 0.20f, 1.0f }, // 幹
				{ 0.34f, 0.54f, 0.24f, 1.0f }, // 広葉樹
				{ 0.20f, 0.40f, 0.24f, 1.0f }, // 松
				{ 0.30f, 0.50f, 0.22f, 1.0f }, // 茂み
				{ 0.56f, 0.56f, 0.53f, 1.0f }, // 岩
				{ 0.60f, 0.62f, 0.32f, 1.0f }, // 葦
				{ 0.40f, 0.62f, 0.25f, 1.0f }, // 草
				{ 0.46f, 0.66f, 0.30f, 1.0f }, // 明るい葉
			};
			sScenery.clear();
			for (int i = 0; i < kBatchCount; ++i)
			{
				if (batch[i].vertices.empty()) continue;
				SceneryBatch sb;
				sb.model = std::make_unique<Object3d>();
				sb.model->Initialize(GetObject3dCom(), batch[i]);
				sb.model->SetCamera(camera_);
				sb.model->SetColor(colors[i]);
				sb.model->SetEnableLighting(true);
				sb.model->SetAllowWireframeOverlay(false);

				if (i != kRock && i != kTrunk) // 岩と幹はどっしり固定
				{
					sb.hasSway = true;
					sb.baseVertices.reserve(batch[i].vertices.size());
					for (const auto& v : batch[i].vertices)
					{
						SceneryVertexBase sv;
						sv.basePos = { v.position.x, v.position.y, v.position.z };

						// 地面からの高さ（手前岸: kLandY, 対岸: kFarLandY）
						float gy = (sv.basePos.z < 4.0f) ? kLandY : kFarLandY;
						float relH = (std::max)(0.0f, sv.basePos.y - gy);

						if (i == kTrunk)
						{
							// 幹：どっしり地面に根を張り固定（揺らさないことで木の剛性感・リアルさを保持）
							sv.swayFactor = 0.0f;
						}
						else if (i == kReed)
						{
							// 葦：川風でしなやかになびく
							sv.swayFactor = std::clamp(relH / 0.8f, 0.0f, 1.0f) * 0.75f;
						}
						else if (i == kGrass)
						{
							// 草：風の波面に合わせて穂先が綺麗に揺れる
							sv.swayFactor = std::clamp(relH / 0.35f, 0.0f, 1.0f) * 0.60f;
						}
						else if (i == kBush)
						{
							// 茂み：風を受けて優しく弾む
							sv.swayFactor = 0.20f;
						}
						else if (i == kLeaf || i == kLeafLight)
						{
							// 広葉樹の葉：樹冠全体がふわりと風を受ける
							sv.swayFactor = 0.65f;
						}
						else if (i == kPine)
						{
							// 松：しなやかに心地よくそよぐ
							sv.swayFactor = 0.50f;
						}
						sb.baseVertices.push_back(sv);
					}
				}
				sScenery.push_back(std::move(sb));
			}

			// --- 土の小道デカール（柔らかい円を連ねて踏み固められた道に） ---
			Object3d::ModelData pathData{};
			pathData.material.textureFilePath = "Resources/circle2.png";
			pathData.material.textureIndex = circleTexIndex_;
			pathData.boundingRadius = 60.0f;
			auto layPath = [&](const std::vector<Vector2>& path, float g) {
				for (size_t i = 0; i + 1 < path.size(); ++i)
				{
					float dx = path[i + 1].x - path[i].x, dz = path[i + 1].y - path[i].y;
					int n = (std::max)(1, static_cast<int>(std::sqrt(dx * dx + dz * dz) / 0.25f));
					for (int k = 0; k < n; ++k)
					{
						float t = static_cast<float>(k) / n;
						float r = HashRange(seed, 0.40f, 0.52f);
						AddShadowDisc(pathData, path[i].x + dx * t + HashRange(seed, -0.05f, 0.05f), g + 0.015f,
							path[i].y + dz * t + HashRange(seed, -0.05f, 0.05f), r, r);
					}
				}
			};
			layPath(nearPath, kLandY);
			layPath(farPath, kFarLandY);
			sPathDecalModel = std::make_unique<Object3d>();
			sPathDecalModel->Initialize(GetObject3dCom(), pathData);
			sPathDecalModel->SetCamera(camera_);
			sPathDecalModel->SetColor({ 0.50f, 0.38f, 0.24f, 0.45f });
			sPathDecalModel->SetEnableLighting(false);
			sPathDecalModel->SetAllowWireframeOverlay(false);
		}
	}

	// ---------------------------------------------------------------
	// 風に舞い散るリアルな紅葉（物理3Dオブジェクトプール初期化）
	// 表裏両面を描画するダブルサイドメッシュで、3D回転しても自然な陰影を保つ
	// ---------------------------------------------------------------
	{
		Object3d::ModelData leafData{};
		leafData.material.textureFilePath = "Resources/leaf.png";
		leafData.material.textureIndex = sFx.leaf;
		leafData.boundingRadius = 1.0f;

		// 表側ポリゴン（+Z法線）
		Sprite::VertexData f0{}, f1{}, f2{}, f3{};
		f0.position = { -0.5f,  0.5f, 0.0f, 1.0f }; f0.texcoord = { 0.0f, 0.0f }; f0.normal = { 0.0f, 0.0f, 1.0f };
		f1.position = {  0.5f,  0.5f, 0.0f, 1.0f }; f1.texcoord = { 1.0f, 0.0f }; f1.normal = { 0.0f, 0.0f, 1.0f };
		f2.position = { -0.5f, -0.5f, 0.0f, 1.0f }; f2.texcoord = { 0.0f, 1.0f }; f2.normal = { 0.0f, 0.0f, 1.0f };
		f3.position = {  0.5f, -0.5f, 0.0f, 1.0f }; f3.texcoord = { 1.0f, 1.0f }; f3.normal = { 0.0f, 0.0f, 1.0f };
		leafData.vertices.push_back(f0);
		leafData.vertices.push_back(f1);
		leafData.vertices.push_back(f2);
		leafData.vertices.push_back(f3);
		leafData.indices.push_back(0); leafData.indices.push_back(1); leafData.indices.push_back(2);
		leafData.indices.push_back(1); leafData.indices.push_back(3); leafData.indices.push_back(2);

		// 裏側ポリゴン（-Z法線）
		Sprite::VertexData b0{}, b1{}, b2{}, b3{};
		b0.position = { -0.5f,  0.5f, 0.0f, 1.0f }; b0.texcoord = { 0.0f, 0.0f }; b0.normal = { 0.0f, 0.0f, -1.0f };
		b1.position = {  0.5f,  0.5f, 0.0f, 1.0f }; b1.texcoord = { 1.0f, 0.0f }; b1.normal = { 0.0f, 0.0f, -1.0f };
		b2.position = { -0.5f, -0.5f, 0.0f, 1.0f }; b2.texcoord = { 0.0f, 1.0f }; b2.normal = { 0.0f, 0.0f, -1.0f };
		b3.position = {  0.5f, -0.5f, 0.0f, 1.0f }; b3.texcoord = { 1.0f, 1.0f }; b3.normal = { 0.0f, 0.0f, -1.0f };
		leafData.vertices.push_back(b0);
		leafData.vertices.push_back(b1);
		leafData.vertices.push_back(b2);
		leafData.vertices.push_back(b3);
		leafData.indices.push_back(5); leafData.indices.push_back(4); leafData.indices.push_back(7);
		leafData.indices.push_back(4); leafData.indices.push_back(6); leafData.indices.push_back(7);

		sFallingLeaves.clear();
		sFallingLeaves.resize(kMaxFallingLeaves);
		for (auto& leaf : sFallingLeaves)
		{
			leaf.model = std::make_unique<Object3d>();
			leaf.model->Initialize(GetObject3dCom(), leafData);
			leaf.model->SetCamera(camera_);
			leaf.model->SetEnableLighting(true);
			leaf.model->SetAllowWireframeOverlay(false);
			leaf.active = false;
		}
	}

	InitializeSprites();
}

void TitleScene::InitializeSprites()
{
	SpriteCom* sc = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetSpriteCom() : nullptr;
	if (!sc) return;

	spriteManager_ = std::make_unique<SpriteManager>();
	spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);
	sprites_.clear();

	WindowAPI* win = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
	sScreenW = win ? static_cast<float>(win->GetClientWidth()) : 1280.0f;
	sScreenH = win ? static_cast<float>(win->GetClientHeight()) : 720.0f;

	Sprite::Transform defaultTransform = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	// 1. 周辺減光ビネット（シックで淡い落ち着いた周辺減光。中央楕円の白浮きを解消）
	if (auto vig = Sprite::Create(sc, defaultTransform, "Resources/title_vignette.png"))
	{
		vig->SetAnchorPoint({ 0.0f, 0.0f });
		vig->SetPosition({ 0.0f, 0.0f });
		vig->SetSize({ sScreenW, sScreenH });
		vig->SetColor({ 0.10f, 0.12f, 0.15f, 0.20f });
		sprites_.emplace_back(std::move(vig));
		vignetteSprite_ = sprites_.back().get();
	}

	// 2. タイトルロゴ（画面左上：右側の主役アヒルと脱出煙に被らない位置）
	sLogoFinalPos = { 48.0f, 34.0f };
	// 2a. ダッシュ残像（ロゴの背面に描画）
	for (auto& ghost : sLogoGhosts)
	{
		ghost = nullptr;
		if (auto g = Sprite::Create(sc, defaultTransform, "Resources/title_logo.png"))
		{
			g->SetAnchorPoint({ 0.0f, 0.0f });
			g->SetPosition({ -sLogoSize.x * 2.0f, sLogoFinalPos.y });
			g->SetSize(sLogoSize);
			g->SetColor({ 1.0f, 0.8f, 0.35f, 0.0f });
			sprites_.emplace_back(std::move(g));
			ghost = sprites_.back().get();
		}
	}
	if (auto logo = Sprite::Create(sc, defaultTransform, "Resources/title_logo.png"))
	{
		logo->SetAnchorPoint({ 0.0f, 0.0f });
		logo->SetPosition({ -sLogoSize.x * 2.0f, sLogoFinalPos.y });
		logo->SetSize(sLogoSize);
		logo->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(logo));
		titleLogoSprite_ = sprites_.back().get();
	}
	// 2b. ロゴのグリント／着弾フラッシュ（ロゴの前面に重ねる）
	sLogoShine = nullptr;
	if (auto shine = Sprite::Create(sc, defaultTransform, "Resources/title_logo_shine.png"))
	{
		shine->SetAnchorPoint({ 0.0f, 0.0f });
		shine->SetSize(sLogoSize);
		SetShineCell(shine.get(), kShineFlashCell, kLogoTexW, kLogoTexH);
		shine->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(shine));
		sLogoShine = sprites_.back().get();
	}

	// 3. PRESS SPACE（左下：ロゴと縦ラインを揃える）
	sPromptBasePos = { 48.0f + 235.0f, sScreenH * 0.88f };
	if (auto prompt = Sprite::Create(sc, defaultTransform, "Resources/title_press_space.png"))
	{
		prompt->SetAnchorPoint({ 0.5f, 0.5f });
		prompt->SetPosition(sPromptBasePos);
		prompt->SetSize(kPromptSize);
		prompt->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(prompt));
		pressSpaceSprite_ = sprites_.back().get();
	}
	sPromptShine = nullptr;
	if (auto shine = Sprite::Create(sc, defaultTransform, "Resources/title_press_space_shine.png"))
	{
		shine->SetAnchorPoint({ 0.5f, 0.5f });
		shine->SetPosition(sPromptBasePos);
		shine->SetSize(kPromptSize);
		SetShineCell(shine.get(), kShineFlashCell, kPromptTexW, kPromptTexH);
		shine->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(shine));
		sPromptShine = sprites_.back().get();
	}

	// 4. 全画面黒フェードオーバーレイ
	if (auto fade = Sprite::Create(sc, defaultTransform, "Resources/CG4/human/white.png"))
	{
		fade->SetAnchorPoint({ 0.0f, 0.0f });
		fade->SetPosition({ 0.0f, 0.0f });
		fade->SetSize({ sScreenW, sScreenH });
		fade->SetColor({ 0.0f, 0.0f, 0.0f, 1.0f });
		sprites_.emplace_back(std::move(fade));
		fadeOverlaySprite_ = sprites_.back().get();
	}
}

void TitleScene::Finalize()
{
	delete input_;
	input_ = nullptr;
	duckModel_.reset();
	riverModel_.reset();
	sandbagModel_.reset();
	crateModel1_.reset();
	crateModel2_.reset();
	sNearGroundModel.reset();
	sFarGroundModel.reset();
	sExtractionPadModel.reset();
	sShadowDecalsModel.reset();
	sEnemyDuckShadows.clear();
	sScenery.clear();
	sPathDecalModel.reset();
	sEnemyProps.clear();
	sEnemyDucks.clear();
	sFallingLeaves.clear();
	appParticleManager_.reset();
	if (spriteManager_)
	{
		spriteManager_->Finalize();
		spriteManager_.reset();
	}
	sprites_.clear();
	vignetteSprite_ = nullptr;
	titleLogoSprite_ = nullptr;
	pressSpaceSprite_ = nullptr;
	fadeOverlaySprite_ = nullptr;
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		if (sBgmPlayId >= 0)
		{
			am->Stop(sBgmPlayId);
			sBgmPlayId = -1;
		}
		if (sDuckSePlayId >= 0)
		{
			am->Stop(sDuckSePlayId);
			sDuckSePlayId = -1;
		}
	}
	sLogoGhosts[0] = sLogoGhosts[1] = nullptr;
	sLogoShine = nullptr;
	sPromptShine = nullptr;
}

void TitleScene::Update()
{
	if (input_)
	{
		input_->Update();
	}

	auto now = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
	lastTime_ = now;
	deltaTime = std::clamp(deltaTime, 0.0001f, 1.0f / 30.0f);
	bgTimer_ += deltaTime;

	// トランジション中または出撃ロック中はカメラタイマーを進めず、トランジション前の予期せぬカメラ移動を完全に防止
	const bool isTransitioningNow = (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());
	if (!isTransitioningNow && !sCameraLocked)
	{
		sIntroTimer += deltaTime;
	}

	const bool introPlaying = sIntroTimer < kIntroDuration;

	// BGM ループ再生（曲の長さ162秒で自動再開）
	if (sBgmSoundId >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		sBgmTimer += deltaTime;
		if (sBgmTimer >= 162.0f)
		{
			sBgmTimer = 0.0f;
			sBgmPlayId = SceneManager::GetInstance()->GetAudioManager()->Play(sBgmSoundId);
		}
	}

	// ---------------------------------------------------------------
	// 入力：SPACEで出撃（トランジション中は入力を受け付けない）
	// ---------------------------------------------------------------
	if (!isStarting_ && input_ && (!StingerTransition::GetInstance() || !StingerTransition::GetInstance()->IsTransitioning()))
	{
		bool pressed = input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN);
		if (pressed && !sLogoLanded)
		{
			sIntroTimer = kIntroDuration;
		}
		else if (pressed)
		{
			isStarting_ = true;
			startTransitionTimer_ = 0.0f;
			sDuckRecoilTimer = 0.25f;

			// アヒルSEをワンショット再生
			if (sDuckSeSoundId >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
			{
				SceneManager::GetInstance()->GetAudioManager()->PlayOneShot(sDuckSeSoundId, 1.0f);
			}

			if (appParticleManager_ && GetParticleManager())
			{
				auto& rng = GetParticleManager()->GetRandomEngine();
				Vector3 muzzle = duckBasePos_ + Vector3{ 0.15f, 0.42f, 0.35f };
				appParticleManager_->EmitMuzzleFlash(rng, muzzle, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.85f, 0.35f, 1.0f }, 2.0f, circleTexIndex_);
				appParticleManager_->EmitMuzzleFlare(rng, muzzle, 0.70f, { 1.0f, 0.95f, 0.80f, 1.0f }, 0.12f, starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_);
			}

			// 出撃時のカメラ位置・姿勢を完全にロック（トランジション開始前にカメラが動いたり揺れたりするのを完全防止）
			sCameraLocked = true;
			if (camera_)
			{
				sCameraLockedPos = camera_->GetTranslate();
				sCameraLockedRot = camera_->GetRotate();
			}
		}
		else if (input_->TriggerKey(DIK_ESCAPE))
		{
			PostQuitMessage(0);
		}
	}

	if (isStarting_)
	{
		startTransitionTimer_ += deltaTime;

		// アヒルが完全に鳴き終わった後（約3.15秒）にトランジションを開始
		const float kDuckCryDuration = 3.15f;
		if (startTransitionTimer_ >= kDuckCryDuration && (!StingerTransition::GetInstance() || !StingerTransition::GetInstance()->IsTransitioning()))
		{
			// ゲーム本編へ出撃（ヘリ降下・ステージ演出開始）
			SceneTransition::Change("GAMEPLAY", 1.80f, TransitionTheme::TitleTactical);
		}
	}

	mouseInput_.Update();

	// ---------------------------------------------------------------
	// ロゴ着弾（落ち着いた金属音的な微小シェイク）
	// ---------------------------------------------------------------
	if (!sLogoLanded && sIntroTimer >= kLogoSlamTime + kLogoSlideTime)
	{
		sLogoLanded = true;
		if (camera_) {
			camera_->Shake(0.20f, 0.035f);
		}
	}

	// ---------------------------------------------------------------
	// カメラ：シネマティッククレーンダウン → 鳥瞰ジオラマ構図
	// ---------------------------------------------------------------
	float introT = EaseInOutCubic(sIntroTimer / kIntroDuration);
	Vector3 camPos = Lerp3(kCamStartPos, kCamFinalPos, introT);
	Vector3 camRot = Lerp3(kCamStartRot, kCamFinalRot, introT);

	float idleBlend = std::clamp((sIntroTimer - kIntroDuration) / 1.0f, 0.0f, 1.0f);
	camPos.x += std::sin(bgTimer_ * 0.35f) * 0.04f * idleBlend;
	camPos.y += std::sin(bgTimer_ * 0.50f) * 0.02f * idleBlend;

	float normX = 0.0f, normY = 0.0f;

	Vector3 mouseWaterHit{ 0.0f, kWaterY, 0.0f };
	bool isHoveringRiver = false;

	if (dxCommon_ && camera_)
	{
		WindowAPI* win = dxCommon_->GetWindowAPI();
		if (win && win->GetClientWidth() > 0 && win->GetClientHeight() > 0)
		{
			Vector2 mp = mouseInput_.GetScaledPosition();
			float cw = static_cast<float>(win->GetClientWidth());
			float ch = static_cast<float>(win->GetClientHeight());
			normX = std::clamp((mp.x / cw) * 2.0f - 1.0f, -1.0f, 1.0f);
			normY = std::clamp((mp.y / ch) * 2.0f - 1.0f, -1.0f, 1.0f);

			if (!sCameraLocked)
			{
				camPos.x += normX * 0.12f * idleBlend;
				camPos.y -= normY * 0.06f * idleBlend;
				camRot.y += normX * 0.015f * idleBlend;
				camRot.x += normY * 0.008f * idleBlend;

				camera_->SetTranslate(camPos);
				camera_->SetRotate(camRot);
			}
			else
			{
				camera_->SetTranslate(sCameraLockedPos);
				camera_->SetRotate(sCameraLockedRot);
			}
			camera_->Update();

			// マウス光線と水面 (y = kWaterY) の交差判定
			float ny = 1.0f - (mp.y / ch) * 2.0f;
			Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));
			Vector4 cNear = { normX, ny, 0.0f, 1.0f };
			Vector4 cFar = { normX, ny, 1.0f, 1.0f };

			auto transformVec4 = [&](const Vector4& c) -> Vector3 {
				float rx = c.x * inv.m[0][0] + c.y * inv.m[1][0] + c.z * inv.m[2][0] + c.w * inv.m[3][0];
				float ry = c.x * inv.m[0][1] + c.y * inv.m[1][1] + c.z * inv.m[2][1] + c.w * inv.m[3][1];
				float rz = c.x * inv.m[0][2] + c.y * inv.m[1][2] + c.z * inv.m[2][2] + c.w * inv.m[3][2];
				float rw = c.x * inv.m[0][3] + c.y * inv.m[1][3] + c.z * inv.m[2][3] + c.w * inv.m[3][3];
				return (rw != 0.0f) ? Vector3{ rx / rw, ry / rw, rz / rw } : Vector3{ 0.0f, 0.0f, 0.0f };
			};

			Vector3 rayNear = transformVec4(cNear);
			Vector3 rayFar = transformVec4(cFar);
			Vector3 rayDir = { rayFar.x - rayNear.x, rayFar.y - rayNear.y, rayFar.z - rayNear.z };

			if (rayDir.y < -1e-5f)
			{
				float t = (kWaterY - rayNear.y) / rayDir.y;
				mouseWaterHit = { rayNear.x + rayDir.x * t, kWaterY, rayNear.z + rayDir.z * t };

				// 川の範囲内か判定（手前岸ラインと対岸ラインの間）
				if (mouseWaterHit.x >= -12.0f && mouseWaterHit.x <= 12.0f)
				{
					float nZ = NearShoreZ(mouseWaterHit.x);
					float fZ = FarShoreZ(mouseWaterHit.x);
					if (mouseWaterHit.z >= nZ && mouseWaterHit.z <= fZ)
					{
						isHoveringRiver = true;
					}
				}
			}
		}
	}

	for (auto& s : mouseWaveSources_) s.age += deltaTime;
	mouseWaveSources_.erase(
		std::remove_if(mouseWaveSources_.begin(), mouseWaveSources_.end(),
			[](const MouseWaveRippleSource& s) { return s.age >= s.maxLife; }),
		mouseWaveSources_.end());
	mouseRippleCooldown_ -= deltaTime;

	// ---------------------------------------------------------------
	// 川にマウスが触れた／なぞったときの「波しぶき（Wave Spray）」パーティクル＆3D波紋
	// ---------------------------------------------------------------
	if (isHoveringRiver && appParticleManager_)
	{
		AppParticleManager* pm = appParticleManager_.get();
		WaterSurfaceSample ws = EvaluateWaterSurface(mouseWaterHit.x, mouseWaterHit.z, bgTimer_);
		float actualWaterY = kWaterY + ws.displacement.y;

		Vector3 mouseDelta = hasPrevMousePos_ ? (mouseWaterHit - prevMouseWorldPos_) : Vector3{ 0.0f, 0.0f, 0.0f };
		float moveDist = std::sqrt(mouseDelta.x * mouseDelta.x + mouseDelta.z * mouseDelta.z);
		bool isClickTrigger = mouseInput_.TriggerButton(0);
		bool isClickHold = mouseInput_.PushButton(0);
		float speed = (deltaTime > 1e-4f) ? (moveDist / deltaTime) : 0.0f;
		float speedFactor = std::clamp(speed / 1.5f, 0.5f, 3.5f);

		mouseWaveEmitCooldown_ -= deltaTime;

		if (isClickTrigger)
		{
			// =======================================================
			// A. 【クリック時：ドカンと大きく弾け飛ぶ大波しぶき (Massive Wave Splash)】
			// =======================================================
			// 1. 360度王冠状・噴水状に勢いよく飛び散る放物線水滴群
			const int kClickDrops = 34;
			for (int d = 0; d < kClickDrops; ++d)
			{
				float angle = RandRange(0.0f, 2.0f * kPi);
				float radialSpd = RandRange(0.8f, 2.8f);
				float upSpd = RandRange(2.2f, 4.4f);

				Vector3 splashVel = {
					std::cos(angle) * radialSpd,
					upSpd,
					std::sin(angle) * radialSpd
				};

				Vector3 spawnPos = {
					mouseWaterHit.x + RandRange(-0.06f, 0.06f),
					actualWaterY + 0.015f,
					mouseWaterHit.z + RandRange(-0.06f, 0.06f)
				};

				float s0 = RandRange(0.024f, 0.054f);
				Spawn(pm, spawnPos, splashVel,
					{ 1.0f, 1.0f, 1.0f, RandRange(0.85f, 1.0f) },
					s0, s0 * 0.25f,
					RandRange(0.32f, 0.55f), sFx.circle,
					16.0f, 0.20f);
			}

			// 2. パッと白く立ち昇る水煙・ミスト
			for (int m = 0; m < 6; ++m)
			{
				float mAngle = RandRange(0.0f, 2.0f * kPi);
				float mRadial = RandRange(0.3f, 1.0f);
				Vector3 mistVel = {
					std::cos(mAngle) * mRadial,
					RandRange(1.2f, 2.2f),
					std::sin(mAngle) * mRadial
				};
				Vector4 mistEnd = { 0.80f, 0.94f, 1.0f, 0.0f };
				Spawn(pm, { mouseWaterHit.x, actualWaterY + 0.03f, mouseWaterHit.z },
					mistVel, { 0.96f, 0.98f, 1.0f, RandRange(0.40f, 0.65f) },
					RandRange(0.08f, 0.14f), RandRange(0.24f, 0.40f),
					RandRange(0.40f, 0.65f), sFx.circle,
					1.8f, 2.2f, 0.20f, &mistEnd);
			}

			// 3. 水面に広がる着水白波・泡リング
			for (int f = 0; f < 3; ++f)
			{
				Vector4 foamEnd = { 0.65f, 0.88f, 1.0f, 0.0f };
				Spawn(pm, { mouseWaterHit.x, actualWaterY + 0.008f, mouseWaterHit.z },
					{ RandRange(-0.15f, 0.15f) + 0.364f, 0.0f, RandRange(-0.15f, 0.15f) },
					{ 0.96f, 0.99f, 1.0f, RandRange(0.60f, 0.85f) },
					RandRange(0.06f, 0.10f), RandRange(0.22f, 0.38f),
					RandRange(0.60f, 1.10f), sFx.circle,
					0.0f, 0.0f, 0.05f, &foamEnd);
			}

			// 4. 3Dメッシュの大きな波紋
			if (mouseWaveSources_.size() < 16)
			{
				mouseRippleCooldown_ = 0.08f;
				MouseWaveRippleSource rip;
				rip.center = mouseWaterHit;
				rip.maxLife = 1.6f;
				rip.power = 0.50f;
				mouseWaveSources_.push_back(rip);
			}
			mouseWaveEmitCooldown_ = 0.06f;
		}
		else if (moveDist > 0.003f)
		{
			// =======================================================
			// B. 【なぞり・ドラッグ時：扇状に弾け飛ぶ側方波しぶき (Dynamic Fan Spray)】
			// =======================================================
			float interval = std::clamp(0.045f / speedFactor, 0.018f, 0.050f);
			if (mouseWaveEmitCooldown_ <= 0.0f)
			{
				mouseWaveEmitCooldown_ = interval;

				Vector3 moveDir = { mouseDelta.x / moveDist, 0.0f, mouseDelta.z / moveDist };
				Vector3 movePerp = { -moveDir.z, 0.0f, moveDir.x };

				// 1. 進行方向の左右に激しく弾け飛ぶ扇状スプラッシュ水滴
				int dropCount = static_cast<int>(8 + speedFactor * 5.0f);
				if (isClickHold) dropCount += 8;
				dropCount = std::clamp(dropCount, 8, 26);

				for (int d = 0; d < dropCount; ++d)
				{
					float sideSign = (Rand01() < 0.5f) ? 1.0f : -1.0f;
					float sideSpread = sideSign * RandRange(0.5f, 1.8f) * speedFactor;
					float forwardSpread = RandRange(0.1f, 1.2f) * speedFactor;
					float upSpeed = RandRange(1.4f, 3.4f) * (0.75f + speedFactor * 0.25f);

					Vector3 splashVel = moveDir * forwardSpread
						+ movePerp * sideSpread
						+ Vector3{ RandRange(-0.25f, 0.25f), upSpeed, RandRange(-0.25f, 0.25f) };

					Vector3 spawnPos = {
						mouseWaterHit.x + RandRange(-0.06f, 0.06f),
						actualWaterY + 0.015f,
						mouseWaterHit.z + RandRange(-0.06f, 0.06f)
					};

					float s0 = RandRange(0.020f, 0.048f);
					Spawn(pm, spawnPos, splashVel,
						{ 1.0f, 1.0f, 1.0f, RandRange(0.80f, 1.0f) },
						s0, s0 * 0.30f,
						RandRange(0.28f, 0.48f), sFx.circle,
						15.5f, 0.22f);
				}

				// 2. しぶきに伴う白い水煙ミスト
				int mistCount = (speedFactor > 1.2f) ? 3 : 1;
				for (int m = 0; m < mistCount; ++m)
				{
					Vector3 mistVel = {
						mouseDelta.x * 0.8f + RandRange(-0.35f, 0.35f),
						RandRange(0.8f, 1.6f) * speedFactor * 0.5f,
						mouseDelta.z * 0.8f + RandRange(-0.35f, 0.35f)
					};
					Vector4 mistEnd = { 0.78f, 0.92f, 1.0f, 0.0f };
					Spawn(pm, { mouseWaterHit.x + RandRange(-0.04f, 0.04f), actualWaterY + 0.02f, mouseWaterHit.z + RandRange(-0.04f, 0.04f) },
						mistVel, { 0.94f, 0.98f, 1.0f, RandRange(0.35f, 0.55f) },
						RandRange(0.06f, 0.11f), RandRange(0.18f, 0.32f),
						RandRange(0.32f, 0.50f), sFx.circle,
						2.0f, 1.8f, 0.15f, &mistEnd);
				}

				// 3. なぞり跡に残る引き波白泡
				Vector4 foamEnd = { 0.65f, 0.88f, 1.0f, 0.0f };
				Spawn(pm, { mouseWaterHit.x, actualWaterY + 0.008f, mouseWaterHit.z },
					{ mouseDelta.x * 0.2f + 0.364f * 0.5f, 0.0f, mouseDelta.z * 0.2f },
					{ 0.94f, 0.98f, 1.0f, RandRange(0.50f, 0.75f) },
					RandRange(0.05f, 0.09f), RandRange(0.12f, 0.22f),
					RandRange(0.50f, 0.90f), sFx.circle,
					0.0f, 0.0f, 0.05f, &foamEnd);

				// 4. 3D波紋リップル
				if (mouseRippleCooldown_ <= 0.0f && mouseWaveSources_.size() < 16)
				{
					mouseRippleCooldown_ = 0.08f;
					MouseWaveRippleSource rip;
					rip.center = mouseWaterHit;
					rip.maxLife = 1.1f;
					rip.power = 0.16f * speedFactor;
					mouseWaveSources_.push_back(rip);
				}
			}
		}
		else
		{
			// =======================================================
			// C. 【静止ホバー時：川の流れが触れた指先に当たって出る自然な波しぶき】
			// =======================================================
			if (mouseWaveEmitCooldown_ <= 0.0f)
			{
				mouseWaveEmitCooldown_ = 0.060f;

				// 水流 (+X) に当たってパシャッと跳ねる2〜3粒の水滴
				for (int d = 0; d < 3; ++d)
				{
					Vector3 splashVel = {
						RandRange(0.25f, 0.65f),
						RandRange(0.9f, 1.7f),
						RandRange(-0.35f, 0.35f)
					};
					Spawn(pm, { mouseWaterHit.x + RandRange(-0.03f, 0.03f), actualWaterY + 0.01f, mouseWaterHit.z + RandRange(-0.03f, 0.03f) },
						splashVel, { 0.98f, 1.0f, 1.0f, 0.80f },
						RandRange(0.016f, 0.032f), 0.010f,
						RandRange(0.24f, 0.40f), sFx.circle,
						14.0f, 0.2f);
				}

				// 後方に流れる小さな引き波泡
				Vector4 foamEnd = { 0.70f, 0.90f, 1.0f, 0.0f };
				Spawn(pm, { mouseWaterHit.x, actualWaterY + 0.008f, mouseWaterHit.z },
					{ RandRange(0.32f, 0.45f), 0.0f, RandRange(-0.08f, 0.08f) },
					{ 0.92f, 0.97f, 1.0f, RandRange(0.40f, 0.60f) },
					RandRange(0.035f, 0.060f), RandRange(0.08f, 0.14f),
					RandRange(0.6f, 1.1f), sFx.circle,
					0.0f, 0.0f, 0.04f, &foamEnd);
			}
		}

		prevMouseWorldPos_ = mouseWaterHit;
		hasPrevMousePos_ = true;
	}
	else
	{
		hasPrevMousePos_ = false;
	}

	// ---------------------------------------------------------------
	// ライティング：C案の澄んだ午後の陽光（黄金色のキーライト + 柔らかな水面反射）
	// 白飛び・ちらつきを抑制した落ち着いたトーン
	// ---------------------------------------------------------------
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		Vector3 sunDir = { -0.42f, -0.78f, 0.46f };
		float len = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
		sunDir = { sunDir.x / len, sunDir.y / len, sunDir.z / len };
		light->SetDirectionalLight({ 0.98f, 0.94f, 0.86f, 1.0f }, sunDir, 1.10f);

		float flash = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / 0.14f) * 1.8f : 0.0f;
		light->SetPointLight(
			{ 1.0f, 0.95f, 0.82f, 1.0f },
			{ duckBasePos_.x + 0.2f, duckBasePos_.y + 0.8f, duckBasePos_.z - 0.5f },
			0.75f + flash, 5.5f, 1.6f);
	}

	if (sDuckRecoilTimer > 0.0f) sDuckRecoilTimer -= deltaTime;

	// ---------------------------------------------------------------
	// 主役アヒル：マウスクリック時の鳴き声SE＆ピョン！と跳ねるインタラクション
	// ---------------------------------------------------------------
	if (!isStarting_ && camera_)
	{
		Matrix4x4 vp = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
		Vector4 clip = { duckBasePos_.x, duckBasePos_.y + 0.3f, duckBasePos_.z, 1.0f };
		Vector4 proj = {
			clip.x * vp.m[0][0] + clip.y * vp.m[1][0] + clip.z * vp.m[2][0] + clip.w * vp.m[3][0],
			clip.x * vp.m[0][1] + clip.y * vp.m[1][1] + clip.z * vp.m[2][1] + clip.w * vp.m[3][1],
			clip.x * vp.m[0][2] + clip.y * vp.m[1][2] + clip.z * vp.m[2][2] + clip.w * vp.m[3][2],
			clip.x * vp.m[0][3] + clip.y * vp.m[1][3] + clip.z * vp.m[2][3] + clip.w * vp.m[3][3],
		};
		if (proj.w > 0.01f)
		{
			float ndcX = proj.x / proj.w;
			float ndcY = proj.y / proj.w;
			float screenX = (ndcX * 0.5f + 0.5f) * sScreenW;
			float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * sScreenH;
			Vector2 mp = mouseInput_.GetScaledPosition();
			float d2 = (mp.x - screenX) * (mp.x - screenX) + (mp.y - screenY) * (mp.y - screenY);
			if (d2 <= 95.0f * 95.0f && mouseInput_.TriggerButton(0) && !isHoveringRiver)
			{
				sDuckClickHopTimer = 0.35f;
				if (sDuckSeSoundId >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
				{
					SceneManager::GetInstance()->GetAudioManager()->PlayOneShot(sDuckSeSoundId, 1.0f);
				}
				if (appParticleManager_)
				{
					for (int d = 0; d < 6; ++d)
					{
						float a = Rand01() * 2.0f * kPi;
						Vector3 p = { duckBasePos_.x + std::cos(a) * 0.15f, kLandY + 0.02f, duckBasePos_.z + std::sin(a) * 0.15f };
						Vector3 v = { std::cos(a) * RandRange(0.4f, 0.8f), RandRange(0.2f, 0.5f), std::sin(a) * RandRange(0.4f, 0.8f) };
						Vector4 endC = { 0.76f, 0.70f, 0.58f, 0.0f };
						Spawn(appParticleManager_.get(), p, v, { 0.82f, 0.76f, 0.65f, 0.45f }, 0.035f, 0.10f, 0.40f, sFx.circle, 0.0f, 0.0f, 0.04f, &endC);
					}
				}
			}
		}
	}

	float hopY = 0.0f;
	float hopPitch = 0.0f;
	if (sDuckClickHopTimer > 0.0f)
	{
		sDuckClickHopTimer -= deltaTime;
		float hopNorm = std::clamp(1.0f - sDuckClickHopTimer / 0.35f, 0.0f, 1.0f);
		hopY = std::sin(hopNorm * kPi) * 0.18f;
		hopPitch = std::sin(hopNorm * kPi * 2.0f) * 0.08f;
	}

	// 出撃時（SPACE押下後）：アヒルが完全に鳴き終わるまでの雄叫び・気合入れアクション
	if (isStarting_ && startTransitionTimer_ < 3.12f)
	{
		float cryPhase = startTransitionTimer_ * 14.0f;
		float cryAmp = std::clamp(1.0f - startTransitionTimer_ / 3.12f, 0.0f, 1.0f);
		hopPitch += std::sin(cryPhase) * 0.08f * cryAmp;
		hopY += std::abs(std::sin(cryPhase * 0.5f)) * 0.04f * cryAmp;
	}

	// ---------------------------------------------------------------
	// 主役アヒル（分隊長）：渡河橋を見据え、マウスXにわずかにエイム追従
	// ---------------------------------------------------------------
	if (duckModel_)
	{
		float breathe = std::sin(bgTimer_ * 2.0f) * 0.008f;
		float recoil = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / 0.14f) : 0.0f;
		float yaw = kDuckFacingYaw + std::sin(bgTimer_ * 0.8f) * 0.04f + normX * 0.15f * idleBlend;
		duckModel_->SetTranslate({ duckBasePos_.x, duckBasePos_.y + breathe + hopY, duckBasePos_.z - recoil * 0.04f });
		duckModel_->SetRotate({ 0.06f * recoil + hopPitch, yaw, 0.0f });
		duckModel_->Update();
	}

	// ---------------------------------------------------------------
	// 対岸の敵兵アヒル小隊の更新（ミリタリージオラマのリアルな警戒・哨戒行動）
	// ---------------------------------------------------------------
	for (size_t i = 0; i < sEnemyDucks.size(); ++i)
	{
		auto& ed = sEnemyDucks[i];
		if (!ed.model) continue;

		Vector3 curPos = ed.basePos;
		float curFacing = ed.baseFacing;
		float curRoll = 0.0f;
		float curPitch = 0.0f;

		// 呼吸のような自然な微小上下動（5mm、接地感を維持し跳ね上がりを防止）
		float breathe = std::sin(bgTimer_ * 1.8f + ed.phaseOffset) * 0.005f;
		curPos.y += breathe;

		switch (ed.behavior)
		{
		case EnemyBehaviorType::SentryLookout:
		{
			// 1. 検問所の歩哨：橋・川の正面を手前に睨み、ゆっくり首を左右に振って索敵
			float scan1 = std::sin(bgTimer_ * ed.speed + ed.phaseOffset) * 0.22f;
			float scan2 = std::sin((bgTimer_ * ed.speed + ed.phaseOffset) * 0.37f) * 0.10f;
			curFacing = ed.baseFacing + scan1 + scan2;
			break;
		}
		case EnemyBehaviorType::OvalPatrol:
		{
			// 2. コンテナヤード前の巡回兵：滑らかな細長楕円を周回（瞬間反転なし・常時進行方向を向く）
			float phase = bgTimer_ * ed.speed + ed.phaseOffset;
			float offX = std::sin(phase) * ed.radiusX;
			float offZ = std::cos(phase) * ed.radiusZ;
			curPos.x += offX;
			curPos.z += offZ;

			// 速度ベクトルから進行方向のYaw角を滑らかに算出
			float vx = std::cos(phase) * ed.radiusX;
			float vz = -std::sin(phase) * ed.radiusZ;
			curFacing = std::atan2(vx, vz);

			// 歩行に連動したアヒルの自然なよちよち歩き（振幅3.5mm、控えめなロール）
			float walkCycle = phase * 4.0f;
			curPos.y += std::abs(std::sin(walkCycle)) * 0.0035f;
			curRoll = std::sin(walkCycle) * 0.015f;
			break;
		}
		case EnemyBehaviorType::StandGuard:
		{
			// 3. 監視塔手前の警戒兵：手前岸（プレイヤー側）を低姿勢でじっと見張り
			float scan = std::sin(bgTimer_ * ed.speed + ed.phaseOffset) * 0.14f;
			curFacing = ed.baseFacing + scan;
			curPitch = 0.035f; // わずかに前傾姿勢で警戒
			break;
		}
		case EnemyBehaviorType::ScoutScan:
		{
			// 4. 最奥小道沿いの斥候兵：手前側と奥側を落ち着いたテンポで滑らかに交互確認
			float cycle = bgTimer_ * ed.speed + ed.phaseOffset;
			float wave = std::sin(cycle);
			// 遷移部のみスムーズに回転し、各方向で一定時間静止して索敵
			float turn = std::clamp(wave * 2.2f, -1.0f, 1.0f);
			float smoothTurn = turn * turn * (3.0f - 2.0f * std::abs(turn)) * (turn < 0.0f ? -1.0f : 1.0f);
			curFacing = ed.baseFacing + smoothTurn * 0.70f;
			break;
		}
		case EnemyBehaviorType::PatrolAcross:
		{
			// 5. 対岸の警戒ラインを左右に堂々と横断行軍するパトロール兵（視界内を通り過ぎる演出）
			float phase = bgTimer_ * ed.speed + ed.phaseOffset;
			float sinVal = std::sin(phase);
			curPos.x = ed.basePos.x + sinVal * ed.radiusX;

			// 移動方向に応じた向き（+Xなら右向き、-Xなら左向き、端で滑らかに旋回）
			float cosVal = std::cos(phase);
			float turnRate = std::clamp(cosVal * 3.5f, -1.0f, 1.0f);
			curFacing = (turnRate >= 0.0f) ? (kPi * 0.5f - (1.0f - turnRate) * 0.5f * kPi) : (-kPi * 0.5f + (1.0f + turnRate) * 0.5f * kPi);

			// よちよち歩きのアニメーション（Y微上下＆Roll揺れ）
			float walkCycle = phase * 6.0f;
			curPos.y += std::abs(std::sin(walkCycle)) * 0.0035f;
			curRoll = std::sin(walkCycle) * 0.015f;
			break;
		}
		}

		ed.model->SetTranslate(curPos);
		ed.model->SetRotate({ curPitch, curFacing, curRoll });
		ed.model->Update();

		if (i < sEnemyDuckShadows.size() && sEnemyDuckShadows[i])
		{
			sEnemyDuckShadows[i]->SetTranslate({ curPos.x, kFarLandY + 0.012f, curPos.z });
			sEnemyDuckShadows[i]->Update();
		}
	}

	// ---------------------------------------------------------------
	// 川の水面（左→右へ流れる）
	// ---------------------------------------------------------------
	if (riverModel_ && riverModel_->GetVertexResource() && !waterBaseVertices_.empty())
	{
		struct VertexData { Vector4 position; Vector2 texcoord; Vector3 normal; };
		VertexData* vData = nullptr;
		D3D12_RANGE readRange{ 0, 0 };
		if (SUCCEEDED(riverModel_->GetVertexResource()->Map(0, &readRange, reinterpret_cast<void**>(&vData))) && vData)
		{
			float flowU = bgTimer_ * 0.08f;
			for (size_t i = 0; i < waterBaseVertices_.size(); ++i)
			{
				const auto& b = waterBaseVertices_[i];
				WaterSurfaceSample s = EvaluateWaterSurface(b.basePosX, b.basePosZ, bgTimer_);
				vData[i].position = { b.basePosX, kWaterY + s.displacement.y, b.basePosZ, 1.0f };
				vData[i].normal = s.normal;
				vData[i].texcoord = { b.baseU - flowU, b.baseV };
			}
			D3D12_RANGE writeRange{ 0, sizeof(VertexData) * waterBaseVertices_.size() };
			riverModel_->GetVertexResource()->Unmap(0, &writeRange);
		}
		riverModel_->Update();
	}

	AppParticleManager* pm = appParticleManager_.get();

	// 背景モデル更新
	if (sNearGroundModel) sNearGroundModel->Update();
	if (sFarGroundModel) sFarGroundModel->Update();
	if (sShadowDecalsModel) sShadowDecalsModel->Update();
	if (sPathDecalModel) sPathDecalModel->Update();

	// 環境装飾（木・草・葦・茂みの風によるしなり＆ざわめきアニメーション）
	for (auto& s : sScenery)
	{
		if (s.model && s.hasSway && s.model->GetVertexResource() && !s.baseVertices.empty())
		{
			struct VertexData { Vector4 position; Vector2 texcoord; Vector3 normal; };
			VertexData* vData = nullptr;
			D3D12_RANGE readRange{ 0, 0 };
			if (SUCCEEDED(s.model->GetVertexResource()->Map(0, &readRange, reinterpret_cast<void**>(&vData))) && vData)
			{
				for (size_t i = 0; i < s.baseVertices.size(); ++i)
				{
					const auto& bv = s.baseVertices[i];
					if (bv.swayFactor <= 0.001f) continue;
					Vector3 offset = EvaluateWind(bv.basePos, bgTimer_, bv.swayFactor);
					vData[i].position = { bv.basePos.x + offset.x, bv.basePos.y + offset.y, bv.basePos.z + offset.z, 1.0f };
				}
				D3D12_RANGE writeRange{ 0, sizeof(VertexData) * s.baseVertices.size() };
				s.model->GetVertexResource()->Unmap(0, &writeRange);
			}
		}
		if (s.model) s.model->Update();
	}
	if (sandbagModel_) sandbagModel_->Update();
	if (crateModel1_) crateModel1_->Update();
	if (crateModel2_) crateModel2_->Update();
	if (sExtractionPadModel) sExtractionPadModel->Update();

	for (auto& p : sEnemyProps)
	{
		if (p.model) p.model->Update();
	}

	// ---------------------------------------------------------------
	// 環境パーティクル（C案：静謐な緊張感と美しい自然現象）
	// ---------------------------------------------------------------
	if (appParticleManager_ && GetParticleManager())
	{
		auto& rng = GetParticleManager()->GetRandomEngine();
		Vector3 padPos = sExtractionPadModel ? sExtractionPadModel->GetTranslate() : Vector3{ 0.50f, kFarLandY, 15.50f };

		// (A) 最奥脱出地点のエメラルドグリーン信号煙（本家Duckovの象徴的な脱出シグナル）
		sExtractionSmokeTimer += deltaTime;
		while (sExtractionSmokeTimer >= 0.032f)
		{
			sExtractionSmokeTimer -= 0.032f;
			const Vector4 greenEnd = { 0.22f, 0.55f, 0.32f, 1.0f };
			Spawn(pm, { padPos.x + RandRange(-0.20f, 0.20f), padPos.y + 0.15f, padPos.z + RandRange(-0.20f, 0.20f) },
				{ RandRange(0.12f, 0.28f), RandRange(1.1f, 1.8f), RandRange(-0.06f, 0.06f) },
				{ 0.25f, 0.95f, 0.42f, 0.65f }, RandRange(0.40f, 0.60f), RandRange(2.0f, 3.2f), RandRange(3.2f, 4.5f),
				sFx.puff, 0.0f, 0.12f, 0.35f, &greenEnd);
		}

		// (B) 脱出地点の外周から立ち昇る光の粒
		sBeaconMoteTimer += deltaTime;
		while (sBeaconMoteTimer >= 0.04f)
		{
			sBeaconMoteTimer -= 0.04f;
			float th = Rand01() * 2.0f * kPi;
			Vector3 o = { padPos.x + std::cos(th) * 1.5f, padPos.y + 0.1f, padPos.z + std::sin(th) * 1.5f };
			Spawn(pm, o, { 0.0f, RandRange(1.0f, 2.2f), 0.0f }, { 0.55f, 1.0f, 0.55f, 1.0f },
				RandRange(0.04f, 0.08f), 0.02f, RandRange(1.2f, 2.0f), sFx.circle, 0.0f, 0.0f, 0.35f);
		}

		// (C) 川の波・水流パーティクル（清流の自然な水面・微細な白泡・岩の跳ね水滴・水面きらめき）
		// 1. 川面を穏やかに流れる微細な白泡・さざ波（Wave Foam & Ripples）
		//    水面テクスチャのスクロール速度 (約0.36m/s) に厳密に同期させ、水の上に浮いて流れるリアルな挙動を実現
		sRiverWaveTimer += deltaTime;
		while (sRiverWaveTimer >= 0.055f)
		{
			sRiverWaveTimer -= 0.055f;
			float fx = RandRange(-7.5f, 6.5f);
			float nZ = NearShoreZ(fx) + 0.50f;
			float fZ = FarShoreZ(fx) - 0.50f;
			float fz = RandRange(nZ, fZ);

			WaterSurfaceSample ws = EvaluateWaterSurface(fx, fz, bgTimer_);
			float fy = kWaterY + ws.displacement.y + 0.008f;

			// 水面テクスチャの流速 (0.36m/s) と一致した自然な速度
			float flowSpeed = RandRange(0.32f, 0.42f);
			float flowZ = RandRange(-0.02f, 0.02f);
			Vector3 vel = { flowSpeed, 0.0f, flowZ };

			// 煙ではなく、水滴サークルテクスチャで透き通る柔らかな水泡
			Vector4 startCol = { 0.90f, 0.96f, 1.0f, RandRange(0.22f, 0.38f) };
			Vector4 endCol = { 0.65f, 0.88f, 1.0f, 0.0f };

			// 直径4〜8cm程度の微細な泡（巨大な直立煙玉を完全排除）
			float s0 = RandRange(0.035f, 0.065f);
			float s1 = s0 * RandRange(1.4f, 1.9f);
			float life = RandRange(1.6f, 2.8f);

			Spawn(pm, { fx, fy, fz }, vel, startCol, s0, s1, life, sFx.circle, 0.0f, 0.0f, 0.04f, &endCol);
		}

		// 2. 川の中の岩（4箇所）と橋脚の自然な激突スプラッシュ＆引き波（Rock & Pier Wakes）
		sRiverWakeTimer += deltaTime;
		while (sRiverWakeTimer >= 0.040f)
		{
			sRiverWakeTimer -= 0.040f;

			struct Obstacle { float x, z, r; };
			const Obstacle obstacles[] = {
				{ -2.2f, 4.2f, 0.55f },
				{  2.6f, 5.6f, 0.48f },
				{  4.4f, 3.4f, 0.50f },
				{ -4.4f, 6.1f, 0.42f },
				{ -0.2f, 3.6f, 0.30f }, // 橋の手前側橋脚
				{ -0.2f, 6.4f, 0.30f }, // 橋の奥側橋脚
			};

			int obsIdx = rand() % 6;
			const auto& obs = obstacles[obsIdx];

			// 岩・支柱の上流側前面（-X側）
			float spawnX = obs.x - obs.r * RandRange(0.45f, 0.70f);
			float spawnZ = obs.z + RandRange(-obs.r * 0.5f, obs.r * 0.5f);
			WaterSurfaceSample ws = EvaluateWaterSurface(spawnX, spawnZ, bgTimer_);
			float spawnY = kWaterY + ws.displacement.y + 0.008f;

			// 岩に水流が激突して激しく跳ね散るリアルな波しぶき水滴群 (3〜5粒)
			int rDrops = 3 + rand() % 3;
			for (int rd = 0; rd < rDrops; ++rd)
			{
				Vector3 dropVel = {
					RandRange(-0.10f, 0.45f),
					RandRange(0.9f, 2.1f),
					RandRange(-0.65f, 0.65f)
				};
				Spawn(pm, { spawnX + RandRange(-0.04f, 0.04f), spawnY + 0.012f, spawnZ + RandRange(-0.04f, 0.04f) }, dropVel,
					{ 1.0f, 1.0f, 1.0f, RandRange(0.75f, 0.95f) },
					RandRange(0.018f, 0.038f), 0.010f,
					RandRange(0.24f, 0.44f), sFx.circle, 14.0f, 0.20f);
			}

			// 岩頭で砕ける微細な水煙ミスト
			if (rand() % 3 == 0)
			{
				Vector4 rMistEnd = { 0.80f, 0.94f, 1.0f, 0.0f };
				Spawn(pm, { spawnX, spawnY + 0.02f, spawnZ },
					{ RandRange(0.10f, 0.35f), RandRange(0.6f, 1.2f), RandRange(-0.25f, 0.25f) },
					{ 0.96f, 0.98f, 1.0f, 0.40f },
					RandRange(0.05f, 0.09f), RandRange(0.15f, 0.26f),
					RandRange(0.30f, 0.50f), sFx.circle,
					2.2f, 1.8f, 0.12f, &rMistEnd);
			}

			// 岩の脇をすり抜ける白波・引き波の泡
			float sideSign = (spawnZ >= obs.z) ? 1.0f : -1.0f;
			Vector3 foamVel = { RandRange(0.35f, 0.50f), 0.0f, sideSign * RandRange(0.08f, 0.24f) };
			Vector4 foamCol = { 0.94f, 0.98f, 1.0f, RandRange(0.40f, 0.65f) };
			Vector4 foamEndCol = { 0.70f, 0.90f, 1.0f, 0.0f };
			Spawn(pm, { spawnX, spawnY, spawnZ }, foamVel, foamCol,
				RandRange(0.035f, 0.065f), RandRange(0.070f, 0.120f), RandRange(0.8f, 1.5f),
				sFx.circle, 0.0f, 0.0f, 0.05f, &foamEndCol);
		}

		// 3. 両岸の控えめなさざ波（Shoreline Lapping Ripples）
		sRiverFoamTimer += deltaTime;
		while (sRiverFoamTimer >= 0.075f)
		{
			sRiverFoamTimer -= 0.075f;
			float fx = RandRange(-7.5f, 7.0f);
			bool nearSide = Rand01() < 0.5f;
			float fz = nearSide ? (NearShoreZ(fx) + RandRange(0.10f, 0.25f)) : (FarShoreZ(fx) - RandRange(0.10f, 0.25f));
			WaterSurfaceSample ws = EvaluateWaterSurface(fx, fz, bgTimer_);
			float fy = kWaterY + ws.displacement.y + 0.008f;

			// 岸辺でプチッと弾ける微細な水滴
			Vector4 shoreCol = { 0.90f, 0.96f, 1.0f, RandRange(0.25f, 0.45f) };
			Vector4 shoreEndCol = { 0.70f, 0.90f, 1.0f, 0.0f };
			Spawn(pm, { fx, fy, fz }, { RandRange(0.25f, 0.35f), 0.0f, 0.0f },
				shoreCol, RandRange(0.025f, 0.045f), RandRange(0.050f, 0.085f), RandRange(0.8f, 1.5f),
				sFx.circle, 0.0f, 0.0f, 0.03f, &shoreEndCol);
		}

		// 4. 水面の繊細な陽光きらめき（Subtle Sun Sparkles）
		sRiverGlintTimer += deltaTime;
		while (sRiverGlintTimer >= 0.040f)
		{
			sRiverGlintTimer -= 0.040f;
			float gx = RandRange(-7.0f, 6.0f);
			float gz = RandRange(NearShoreZ(gx) + 0.6f, FarShoreZ(gx) - 0.6f);
			WaterSurfaceSample ws = EvaluateWaterSurface(gx, gz, bgTimer_);
			float gy = kWaterY + ws.displacement.y + 0.008f;

			// 太陽光が波頭でチカッと光る極小の光粒（星型は避け、自然な円形ハイライト）
			Vector4 glintCol = { 1.0f, 0.98f, 0.92f, RandRange(0.50f, 0.80f) };
			Spawn(pm, { gx, gy, gz },
				{ RandRange(0.32f, 0.40f), RandRange(0.05f, 0.15f), 0.0f },
				glintCol, RandRange(0.015f, 0.028f), 0.005f, RandRange(0.18f, 0.32f),
				sFx.circle, 2.0f, 0.1f);
		}

		// (D) 陽光の中を漂う金色のダストモート
		sAmbientDustTimer += deltaTime;
		while (sAmbientDustTimer >= 0.045f)
		{
			sAmbientDustTimer -= 0.045f;
			Vector3 p = { RandRange(-6.0f, 6.0f), RandRange(-0.2f, 3.0f), RandRange(0.0f, 14.0f) };
			Spawn(pm, p, { RandRange(0.08f, 0.18f), RandRange(-0.02f, 0.05f), RandRange(-0.04f, 0.04f) },
				{ 1.0f, 0.94f, 0.75f, 0.40f },
				RandRange(0.020f, 0.045f), 0.015f, RandRange(3.0f, 5.0f), sFx.circle, 0.0f, 0.0f, 0.20f);
		}

		// (E) 地面から風に乗ってふわりと漂う微小な土煙・砂埃パーティクル（手前岸・対岸）
		sGroundDustTimer += deltaTime;
		while (sGroundDustTimer >= 0.14f)
		{
			sGroundDustTimer -= 0.14f;
			bool nearSide = (Rand01() < 0.5f);
			Vector3 dustPos;
			if (nearSide)
			{
				dustPos = { RandRange(0.8f, 5.2f), kLandY + 0.02f, RandRange(-1.6f, 2.2f) };
			}
			else
			{
				dustPos = { RandRange(-3.5f, 4.5f), kFarLandY + 0.02f, RandRange(8.5f, 15.0f) };
			}
			Vector3 dustVel = { RandRange(0.18f, 0.42f), RandRange(0.06f, 0.16f), RandRange(-0.06f, 0.06f) };
			Vector4 dustCol = { 0.76f, 0.70f, 0.58f, RandRange(0.16f, 0.30f) };
			Vector4 dustEndCol = { 0.72f, 0.66f, 0.54f, 0.0f };
			Spawn(pm, dustPos, dustVel, dustCol, RandRange(0.04f, 0.08f), RandRange(0.14f, 0.26f), RandRange(0.9f, 1.7f), sFx.circle, 0.0f, 0.0f, 0.05f, &dustEndCol);
		}

		// (F) 風に舞い散るリアルな紅葉（物理3Dオブジェクト：非発光・太陽光受光・3Dタンブリング）
		//     加算パーティクル（火花・光粒・花粉）を完全排除し、実体モデルとして美しく舞わせる
		float windGust = 0.60f + 0.40f * std::sin(bgTimer_ * 0.55f);
		bool isWindGusting = (windGust > 0.68f);

		// 突風時は約0.45秒に1枚、平時は約1.35秒に1枚
		float leafInterval = isWindGusting ? 0.45f : 1.35f;
		sLeafTimer += deltaTime;
		while (sLeafTimer >= leafInterval)
		{
			sLeafTimer -= leafInterval;

			FallingLeaf3D* slot = nullptr;
			for (auto& l : sFallingLeaves)
			{
				if (!l.active)
				{
					slot = &l;
					break;
				}
			}

			if (slot && slot->model)
			{
				slot->active = true;
				slot->age = 0.0f;
				slot->maxLife = RandRange(7.5f, 11.0f);

				// 発生源：カメラから自然に見える木々の樹冠
				int source = rand() % 4;
				if (source == 0)
				{
					// 左手手前の広葉樹 (X: -7.5 ~ -4.5, Y: 2.2 ~ 3.8, Z: 3.5 ~ 7.5)
					slot->pos = { RandRange(-7.5f, -4.5f), RandRange(2.2f, 3.8f), RandRange(3.5f, 7.5f) };
				}
				else if (source == 1)
				{
					// 左手近景の茂み (X: -5.0 ~ -2.0, Y: 1.6 ~ 2.8, Z: 1.0 ~ 4.5)
					slot->pos = { RandRange(-5.0f, -2.0f), RandRange(1.6f, 2.8f), RandRange(1.0f, 4.5f) };
				}
				else if (source == 2)
				{
					// 渡河橋上流側の木立 (X: -6.5 ~ -2.0, Y: 2.2 ~ 4.0, Z: 5.0 ~ 9.0)
					slot->pos = { RandRange(-6.5f, -2.0f), RandRange(2.2f, 4.0f), RandRange(5.0f, 9.0f) };
				}
				else
				{
					// 左手奥の樹冠 (X: -9.0 ~ -3.5, Y: 2.6 ~ 4.8, Z: 8.5 ~ 13.0)
					slot->pos = { RandRange(-9.0f, -3.5f), RandRange(2.6f, 4.8f), RandRange(8.5f, 13.0f) };
				}

				// 風速ベクトル：心地よい滑空速度 (1.6〜2.6 m/s)
				float windSpeed = RandRange(1.6f, 2.6f) * (0.82f + windGust * 0.32f);
				slot->baseVel = {
					windSpeed * 0.92f + RandRange(-0.15f, 0.15f),
					RandRange(-0.32f, -0.16f) - (windGust * 0.06f),
					windSpeed * 0.38f + RandRange(-0.15f, 0.15f)
				};

				slot->wobbleDir = { -0.38f, 0.0f, 0.92f };
				slot->wobbleFreq = RandRange(2.2f, 3.8f);
				slot->wobbleAmp = RandRange(0.40f, 0.85f);
				slot->wobblePhase = RandRange(0.0f, 2.0f * kPi);

				// 3Dタンブリング回転（ピッチ宙返り、ヨー水平回転、ロールバンク揺れ）
				slot->rot = { RandRange(0.0f, 2.0f * kPi), RandRange(0.0f, 2.0f * kPi), RandRange(-0.4f, 0.4f) };
				slot->rotSpeed = {
					RandRange(1.4f, 3.2f) * (rand() % 2 == 0 ? 1.0f : -1.0f),
					RandRange(0.8f, 2.0f) * (rand() % 2 == 0 ? 1.0f : -1.0f),
					RandRange(-0.3f, 0.3f)
				};

				// 瑞々しい青葉・緑葉パレット（新緑・深緑・エメラルド・日向の若葉・オリーブグリーン）
				const Vector4 palettes[] = {
					{ 1.00f, 1.00f, 1.00f, 1.0f }, // 基準グリーン（自然な木の葉の緑）
					{ 1.06f, 1.14f, 0.90f, 1.0f }, // 若葉・ライムグリーン（陽光を浴びた明るい新緑）
					{ 0.78f, 0.90f, 0.72f, 1.0f }, // 深緑・フォレストグリーン（落ち着いた木陰の葉）
					{ 0.88f, 1.08f, 0.84f, 1.0f }, // エメラルドグリーン（鮮やかな夏の青葉）
					{ 1.08f, 1.06f, 0.76f, 1.0f }, // 日向のオリーブグリーン（温かい木漏れ日の葉）
				};
				slot->color = palettes[rand() % 5];
				slot->scale = RandRange(0.28f, 0.40f);

				slot->model->SetColor(slot->color);
				slot->model->SetScale({ slot->scale, slot->scale, slot->scale });
				slot->model->SetTranslate(slot->pos);
				slot->model->SetRotate(slot->rot);
				slot->model->Update();
			}
		}

		// 舞い散る葉の物理挙動シミュレーション更新
		for (auto& leaf : sFallingLeaves)
		{
			if (!leaf.active || !leaf.model) continue;

			leaf.age += deltaTime;
			if (leaf.age >= leaf.maxLife || leaf.pos.y <= (kWaterY + 0.02f) || leaf.pos.x > 8.0f)
			{
				leaf.active = false;
				continue;
			}

			// 左右の振り子滑空運動
			float sway = std::sin(leaf.age * leaf.wobbleFreq + leaf.wobblePhase);
			Vector3 swayVel = {
				leaf.wobbleDir.x * (sway * leaf.wobbleAmp),
				0.08f * std::cos(leaf.age * leaf.wobbleFreq * 2.0f + leaf.wobblePhase),
				leaf.wobbleDir.z * (sway * leaf.wobbleAmp)
			};

			Vector3 totalVel = {
				leaf.baseVel.x + swayVel.x,
				leaf.baseVel.y + swayVel.y,
				leaf.baseVel.z + swayVel.z
			};
			leaf.pos.x += totalVel.x * deltaTime;
			leaf.pos.y += totalVel.y * deltaTime;
			leaf.pos.z += totalVel.z * deltaTime;

			// 3Dタンブリング回転（風を受けながら回転・翻る）
			leaf.rot.x += leaf.rotSpeed.x * deltaTime;
			leaf.rot.y += leaf.rotSpeed.y * deltaTime;
			leaf.rot.z = leaf.rotSpeed.z + sway * 0.70f;

			leaf.model->SetTranslate(leaf.pos);
			leaf.model->SetRotate(leaf.rot);
			leaf.model->SetScale({ leaf.scale, leaf.scale, leaf.scale });
			leaf.model->Update();
		}

		appParticleManager_->Update(deltaTime, duckBasePos_);
	}

	// ---------------------------------------------------------------
	// 2D演出：開幕フェード → タイトルロゴ着弾 → PRESS SPACE
	// ---------------------------------------------------------------
	if (fadeOverlaySprite_ && !isStarting_)
	{
		float a = 1.0f - std::clamp(sIntroTimer / 0.8f, 0.0f, 1.0f);
		fadeOverlaySprite_->SetColor({ 0.0f, 0.0f, 0.0f, a });
	}

	if (titleLogoSprite_)
	{
		const float lt = (sIntroTimer - kLogoSlamTime) / kLogoSlideTime;
		const float startX = -sLogoSize.x - 60.0f;
		auto dashX = [&](float t) { return startX + (sLogoFinalPos.x - startX) * EaseOutCubic(t); };

		if (lt <= 0.0f)
		{
			titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
			for (Sprite* g : sLogoGhosts) if (g) g->SetColor({ 1.0f, 0.8f, 0.35f, 0.0f });
			if (sLogoShine) sLogoShine->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		}
		else
		{
			const float since = sLogoLanded ? sIntroTimer - (kLogoSlamTime + kLogoSlideTime) : -1.0f;

			// 左から突っ込んでくるダッシュイン → 停止後はゆるく浮遊
			float x = dashX(lt);
			float y = sLogoFinalPos.y;
			if (since >= 0.0f)
			{
				y += std::sin(since * 1.8f) * 3.0f * std::clamp(since / 0.6f, 0.0f, 1.0f);
			}

			// 移動中は横に伸び、急ブレーキで潰れてバネのように戻る
			float sx = 1.0f, sy = 1.0f;
			if (lt < 1.0f)
			{
				const float speed = 1.0f - lt;
				sx = 1.0f + 0.18f * speed;
				sy = 1.0f - 0.10f * speed;
			}
			if (since >= 0.0f)
			{
				const float e = std::exp(-since * 7.0f) * std::cos(since * 22.0f);
				sx = 1.0f - 0.07f * e;
				sy = 1.0f + 0.09f * e;
			}
			const Vector2 size = { sLogoSize.x * sx, sLogoSize.y * sy };
			const Vector2 pos = { x - (size.x - sLogoSize.x) * 0.5f, y - (size.y - sLogoSize.y) * 0.5f };
			titleLogoSprite_->SetPosition(pos);
			titleLogoSprite_->SetSize(size);
			titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, std::clamp(lt * 4.0f, 0.0f, 1.0f) });

			// ダッシュ残像：本体を少し遅れて追いかけ、追いついたら消える
			for (int i = 0; i < 2; ++i)
			{
				Sprite* g = sLogoGhosts[i];
				if (!g) continue;
				const float gt = lt - 0.12f * static_cast<float>(i + 1);
				const float base = (i == 0) ? 0.50f : 0.28f;
				const float ga = base * std::clamp(gt * 5.0f, 0.0f, 1.0f) * (1.0f - std::clamp((gt - 0.55f) / 0.45f, 0.0f, 1.0f));
				const Vector2 gs = { sLogoSize.x * 1.22f, sLogoSize.y * 0.94f };
				g->SetPosition({ dashX(gt) - (gs.x - sLogoSize.x), sLogoFinalPos.y + (sLogoSize.y - gs.y) * 0.5f });
				g->SetSize(gs);
				g->SetColor({ 1.0f, 0.8f, 0.35f, ga });
			}

			// グリント：着弾フラッシュ → 定期的に光が走る / 出撃時に再フラッシュ
			if (sLogoShine)
			{
				sLogoShine->SetPosition(pos);
				sLogoShine->SetSize(size);
				float shineA = 0.0f;
				if (isStarting_)
				{
					const float f = 1.0f - std::clamp(startTransitionTimer_ / 0.30f, 0.0f, 1.0f);
					SetShineCell(sLogoShine, kShineFlashCell, kLogoTexW, kLogoTexH);
					shineA = f * f;
				}
				else if (since >= 0.0f && since < 0.28f)
				{
					const float f = 1.0f - since / 0.28f;
					SetShineCell(sLogoShine, kShineFlashCell, kLogoTexW, kLogoTexH);
					shineA = f * f;
				}
				else if (since >= 0.0f)
				{
					const int frame = ShineFrame(since - 0.9f, 3.8f, 0.55f);
					if (frame >= 0)
					{
						SetShineCell(sLogoShine, frame, kLogoTexW, kLogoTexH);
						shineA = 1.0f;
					}
				}
				sLogoShine->SetColor({ 1.0f, 1.0f, 1.0f, shineA });
			}
		}
	}

	if (pressSpaceSprite_)
	{
		if (!isStarting_)
		{
			// 左からスッと滑り込み → 呼吸するように脈動、定期的に光が走る
			const float appear = sLogoLanded ? std::clamp((sIntroTimer - kIntroDuration) / 0.45f, 0.0f, 1.0f) : 0.0f;
			const float e = EaseOutCubic(appear);
			const float pulse = 0.5f + 0.5f * std::sin(bgTimer_ * 3.2f);
			const float scale = 1.0f + 0.025f * pulse;
			const Vector2 pos = { sPromptBasePos.x - 80.0f * (1.0f - EaseOutBack(appear)), sPromptBasePos.y };
			const Vector2 size = { kPromptSize.x * scale, kPromptSize.y * scale };
			pressSpaceSprite_->SetPosition(pos);
			pressSpaceSprite_->SetSize(size);
			pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, e * (0.82f + 0.18f * pulse) });
			if (sPromptShine)
			{
				sPromptShine->SetPosition(pos);
				sPromptShine->SetSize(size);
				const int frame = ShineFrame(sIntroTimer - kIntroDuration - 0.6f, 2.6f, 0.42f);
				if (frame >= 0) SetShineCell(sPromptShine, frame, kPromptTexW, kPromptTexH);
				sPromptShine->SetColor({ 1.0f, 1.0f, 1.0f, frame >= 0 ? e : 0.0f });
			}
		}
		else
		{
			// 決定：ポンと膨らんで白く弾けてフェードアウト
			const float t = startTransitionTimer_;
			const float pop = 1.0f + 0.16f * EaseOutCubic((std::clamp)(t / 0.18f, 0.0f, 1.0f));
			const float alpha = (std::clamp)(1.0f - (t - 0.15f) / 0.35f, 0.0f, 1.0f);
			const Vector2 size = { kPromptSize.x * pop, kPromptSize.y * pop };
			pressSpaceSprite_->SetPosition(sPromptBasePos);
			pressSpaceSprite_->SetSize(size);
			pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });
			if (sPromptShine)
			{
				const float f = 1.0f - std::clamp(t / 0.30f, 0.0f, 1.0f);
				SetShineCell(sPromptShine, kShineFlashCell, kPromptTexW, kPromptTexH);
				sPromptShine->SetPosition(sPromptBasePos);
				sPromptShine->SetSize(size);
				sPromptShine->SetColor({ 1.0f, 1.0f, 1.0f, f * f });
			}
		}
	}

	if (spriteManager_) spriteManager_->Update();
	for (auto& sp : sprites_)
	{
		if (sp) sp->Update();
	}
}

void TitleScene::Draw(SceneRenderRequests& renderRequests)
{
	if (camera_)
	{
		camera_->Update();
	}

	if (GetObject3dCom() && dxCommon_ && camera_)
	{
		RenderContext base{};
		base.commandList = dxCommon_->GetCommandList().Get();
		base.windowAPI = dxCommon_->GetWindowAPI();
		base.camera = camera_;
		base.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
		MaterialManager* matMgr = GetMaterialManager();
		if (matMgr && matMgr->GetMaterialResource())
		{
			base.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
		}

		Object3dCom* com = GetObject3dCom();

		// 1. 地形（手前岸・対岸）
		DrawModel(com, sNearGroundModel.get(), base, sGrassTexIndex);
		DrawModel(com, sFarGroundModel.get(), base, sGrassTexIndex);

		// 1.5 土の小道
		DrawModel(com, sPathDecalModel.get(), base, circleTexIndex_);

		// 2. 接地影デカール（地形の直後、水面やプロップの手前に描画して地面に密着）
		if (sShadowDecalsModel)
		{
			DrawModel(com, sShadowDecalsModel.get(), base, circleTexIndex_);
		}
		for (auto& s : sEnemyDuckShadows)
		{
			DrawModel(com, s.get(), base, circleTexIndex_);
		}

		// 2.5 環境装飾（水面より先に描画し、水中の葦・岩が水色に馴染むように）
		for (auto& s : sScenery)
		{
			DrawModel(com, s.model.get(), base, sWhiteTexIndex);
		}

		// 3. 水面（半透明・Gerstner波）
		DrawModel(com, riverModel_.get(), base, riverTexIndex_);

		// 4. 対岸の最奥脱出地点
		DrawModel(com, sExtractionPadModel.get(), base, sExtractionPadTexIndex);

		// 5. 敵陣地の整理されたゾーンプロップ群（木造橋梁含む）
		for (auto& p : sEnemyProps)
		{
			DrawModel(com, p.model.get(), base, p.textureIndex);
		}

		// 6. 敵兵アヒル小隊
		uint32_t enemyTex = TextureManager::GetInstance()->Load("Resources/duck_enemy.png");
		for (auto& ed : sEnemyDucks)
		{
			DrawModel(com, ed.model.get(), base, enemyTex);
		}

		// 7. 手前岸のプロップ（木箱・土嚢）
		DrawModel(com, sandbagModel_.get(), base, sandbagTexIndex_);
		DrawModel(com, crateModel1_.get(), base, crateTexIndex_);
		DrawModel(com, crateModel2_.get(), base, crateTexIndex_);

		// 8. 主役アヒル（凛々しい単独潜入オペレーター）
		uint32_t duckTex = TextureManager::GetInstance()->Load("Resources/duck.png");
		DrawModel(com, duckModel_.get(), base, duckTex);

		// 8.5 風に舞い散るリアルな紅葉（物理3Dオブジェクト）
		for (auto& leaf : sFallingLeaves)
		{
			if (leaf.active && leaf.model)
			{
				DrawModel(com, leaf.model.get(), base, sFx.leaf);
			}
		}

		// 9. パーティクル（脱出緑煙、光の粒、川辺の白波、陽光のダストモート）
		if (appParticleManager_)
		{
			RenderContext pctx = base;
			if (circleTexIndex_ != UINT32_MAX)
			{
				pctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(circleTexIndex_);
			}
			appParticleManager_->Draw(pctx);
		}

		// 10. 2DスプライトUI
		if (spriteManager_)
		{
			spriteManager_->DrawAll(base, camera_, &sprites_);
		}
	}

	renderRequests.sceneDrawn = true;
}

TitleScene::WaterSurfaceSample TitleScene::EvaluateWaterSurface(float x, float z, float time) const
{
	WaterSurfaceSample result{};

	struct Wave { float dirX, dirZ, amp, len, speed; };
	static const Wave kWaves[3] = {
		{ 0.97f,  0.24f, 0.020f, 6.5f, 1.10f },
		{ 0.89f, -0.45f, 0.010f, 4.2f, 1.30f },
		{ 0.99f,  0.14f, 0.004f, 2.4f, 1.60f },
	};

	float dispY = 0.0f, gradX = 0.0f, gradZ = 0.0f;
	for (const auto& w : kWaves)
	{
		float k = 2.0f * kPi / w.len;
		float th = k * (w.dirX * x + w.dirZ * z) - k * w.speed * time;
		dispY += w.amp * std::cos(th);
		gradX -= w.dirX * k * w.amp * std::sin(th);
		gradZ -= w.dirZ * k * w.amp * std::sin(th);
	}

	for (const auto& rip : mouseWaveSources_)
	{
		if (rip.age >= rip.maxLife) continue;
		float fade = 1.0f - rip.age / rip.maxLife;
		fade *= fade;
		float rx = x - rip.center.x, rz = z - rip.center.z;
		float r = std::sqrt(rx * rx + rz * rz);
		float dr = r - (0.4f + rip.age * 2.2f);
		const float width = 1.1f;
		if (std::abs(dr) < width && r > 0.05f)
		{
			float env = 0.5f + 0.5f * std::cos(dr / width * kPi);
			float a = rip.power * fade * env * 0.022f;
			dispY += a;
			float dw = -a * (dr / width);
			gradX -= dw * (rx / r);
			gradZ -= dw * (rz / r);
		}
	}

	Vector3 n = { -gradX, 1.0f, -gradZ };
	float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
	result.displacement = { 0.0f, dispY, 0.0f };
	result.normal = { n.x / len, n.y / len, n.z / len };
	result.foamFactor = 0.0f;
	return result;
}
