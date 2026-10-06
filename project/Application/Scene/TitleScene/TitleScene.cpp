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
#include <cmath>
#include <algorithm>
#include <vector>

namespace {
	constexpr float kPi = 3.14159265f;

	// ---------------------------------------------------------------
	// 構図パラメータ（B案＋C案融合：鳥瞰タクティカルジオラマ）
	// 手前岸・清流・木造橋・対岸敵陣地の空間階層を美しいミニチュアとして一望
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
	uint32_t sFenceTexIndex = UINT32_MAX;
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
	struct EnemyDuckInstance
	{
		std::unique_ptr<Object3d> model;
		Vector3 basePos;
		float patrolRadius = 0.0f;
		float patrolSpeed = 0.0f;
		float baseFacing = 0.0f;
		float bobSpeed = 3.5f;
		float bobAmp = 0.020f;
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
	Vector2 sLogoSize = { 520.0f, 156.0f };

	// 環境パーティクルタイマー（C案：静謐な緊張感と美しい自然現象）
	float sExtractionSmokeTimer = 0.0f;
	float sBeaconMoteTimer = 0.0f;
	float sRiverFoamTimer = 0.0f;
	float sAmbientDustTimer = 0.0f;

	struct TracerRound
	{
		Vector3 position;
		Vector3 velocity;
		Vector4 color;
		float age = 0.0f;
		float maxLife = 1.0f;
		bool isPlayerShot = false;
	};
	std::vector<TracerRound> sTracers;

	float sDuckRecoilTimer = 0.0f;
	int32_t sAlarmSoundId = -1;

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
	};
	FxTextures sFx;

	// =================================================================
	// 環境装飾（木・茂み・岩・葦・草むら・土の小道）
	// 外部モデルが無いため、ローポリ形状をコードで生成し、色ごとに1メッシュへバッチ化する
	// =================================================================
	uint32_t sWhiteTexIndex = UINT32_MAX;
	struct SceneryBatch
	{
		std::unique_ptr<Object3d> model;
	};
	std::vector<SceneryBatch> sScenery;
	std::unique_ptr<Object3d> sPathDecalModel;

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

	// 曳光弾の軌跡
	void FxTracerSegment(AppParticleManager* pm, const Vector3& a, const Vector3& b, const Vector4& col)
	{
		Vector3 d = b - a;
		int n = std::clamp(static_cast<int>(Length3(d) / 0.12f), 1, 24);
		for (int i = 0; i < n; ++i)
		{
			float t = static_cast<float>(i) / static_cast<float>(n);
			Spawn(pm, a + d * t, { 0.0f, 0.0f, 0.0f }, col, 0.08f, 0.02f, 0.14f, sFx.circle);
		}
		Spawn(pm, b, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.90f, 1.0f }, 0.20f, 0.08f, 0.05f, sFx.circle);
	}

	// 銃口炎
	void FxMuzzle(AppParticleManager* pm, const Vector3& p, const Vector3& dir, float s)
	{
		Spawn(pm, p, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.88f, 0.50f, 1.0f }, 0.35f * s, 0.65f * s, 0.07f, sFx.star);
		Spawn(pm, p, { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.60f, 0.20f, 1.0f }, 0.50f * s, 0.85f * s, 0.08f, sFx.circle);
		for (int i = 0; i < 4; ++i)
		{
			Spawn(pm, p, dir * RandRange(3.0f, 5.5f) + RandDir(-1.0f) * 0.6f, { 1.0f, 0.80f, 0.40f, 1.0f }, 0.05f, 0.02f, 0.10f, sFx.circle);
		}
	}

	// 着弾（地面：土煙／水面：水柱と波紋）
	void FxBulletImpact(AppParticleManager* pm, const Vector3& p, bool water)
	{
		if (water)
		{
			for (int i = 0; i < 10; ++i)
			{
				Vector3 d = RandDir(0.80f);
				Spawn(pm, p, d * RandRange(1.5f, 3.2f), { 0.85f, 0.95f, 1.0f, 0.85f }, RandRange(0.04f, 0.08f), 0.02f, RandRange(0.4f, 0.7f), sFx.circle, 9.8f, 0.3f);
			}
			Spawn(pm, p + Vector3{ 0.0f, 0.03f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.85f, 0.95f, 1.0f, 0.45f }, 0.08f, 0.45f, 0.45f, sFx.puff);
		}
		else
		{
			for (int i = 0; i < 6; ++i)
			{
				Vector3 d = RandDir(0.25f);
				Spawn(pm, p, d * RandRange(1.5f, 3.8f), { 1.0f, 0.85f, 0.45f, 1.0f }, RandRange(0.03f, 0.06f), 0.015f, RandRange(0.25f, 0.5f), sFx.circle, 9.8f, 0.3f);
			}
			Spawn(pm, p, { RandRange(-0.1f, 0.1f), RandRange(0.3f, 0.6f), 0.0f }, { 0.60f, 0.52f, 0.40f, 0.35f }, 0.12f, 0.50f, RandRange(0.6f, 1.0f), sFx.puff, 0.0f, 0.8f);
		}
	}
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
		sAlarmSoundId = SceneManager::GetInstance()->GetAudioManager()->Load("Resources/Alarm01.wav");
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

	sIntroTimer = 0.0f;
	sLogoLanded = false;
	sShakeTimer = 0.0f;
	sShakePower = 0.0f;
	sFlashTimer = 0.0f;
	sTracers.clear();
	sDuckRecoilTimer = 0.0f;

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
		Object3d::ModelData duckData = Object3d::LoadObjFile("Resources", "player.obj");
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
			sNearGroundModel->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
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
			sFarGroundModel->SetColor({ 0.95f, 0.97f, 0.95f, 1.0f });
			sFarGroundModel->SetEnableLighting(true);
			sFarGroundModel->SetAllowWireframeOverlay(false);
		}

		// =========================================================
		// 4. 手前岸の物資プロップ（アヒル分隊の左側に配置）
		// =========================================================
		{
			Object3d::ModelData crateData = Object3d::LoadObjFile("Resources", "duckov_crate.obj");
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
			crateModel2_->SetTranslate({ 4.45f, kLandY + 0.58f, 1.63f });
			crateModel2_->SetRotate({ 0.0f, -0.15f, 0.0f });
		}
		{
			Object3d::ModelData sandData = Object3d::LoadObjFile("Resources", "duckov_sandbag_wall.obj");
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
		sFenceTexIndex = TextureManager::GetInstance()->Load("Resources/fence.png");
		sWatchtowerTexIndex = TextureManager::GetInstance()->Load("Resources/duckov_crate.png");

		Object3d::ModelData bridgeData = Object3d::LoadObjFile("Resources", "bridge.obj");
		Object3d::ModelData signpostData = Object3d::LoadObjFile("Resources", "signpost.obj");
		Object3d::ModelData targetData = Object3d::LoadObjFile("Resources", "shooting_target.obj");
		Object3d::ModelData crateData = Object3d::LoadObjFile("Resources", "duckov_crate.obj");
		Object3d::ModelData sandData = Object3d::LoadObjFile("Resources", "duckov_sandbag_wall.obj");
		Object3d::ModelData barrelData = Object3d::LoadObjFile("Resources", "duckov_barrel_stack.obj");
		Object3d::ModelData containerData = Object3d::LoadObjFile("Resources", "container.obj");
		Object3d::ModelData fenceData = Object3d::LoadObjFile("Resources", "fence.obj");
		Object3d::ModelData towerData = Object3d::LoadObjFile("Resources", "watchtower.obj");
		Object3d::ModelData padData = Object3d::LoadObjFile("Resources", "extraction_pad.obj");
		Object3d::ModelData enemyDuckData = Object3d::LoadObjFile("Resources", "player.obj");

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
		// [Zone 1: 渡河橋頭堡検問所（中央手前）]
		// ---------------------------------------------------------
		addEnemyProp(sandData, sandbagTexIndex_, { 0.40f, kFarLandY, 7.55f }, { 0.56f, 0.56f, 0.56f }, { 0.0f, 0.05f, 0.0f });
		addEnemyProp(signpostData, sSignpostTexIndex, { -0.35f, kFarLandY, 7.35f }, { 0.45f, 0.45f, 0.45f }, { 0.0f, 0.25f, 0.0f });
		addEnemyProp(barrelData, barrelTexIndex_, { 1.45f, kFarLandY, 7.70f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, 0.20f, 0.0f });
		addEnemyProp(fenceData, sFenceTexIndex, { -1.50f, kFarLandY, 8.40f }, { 1.10f, 1.10f, 1.10f }, { 0.0f, 0.0f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 2: 左翼軍用コンテナ物流拠点 & 防壁境界]
		// ---------------------------------------------------------
		addEnemyProp(containerData, containerTexIndex_, { -4.20f, kFarLandY, 11.20f }, { 0.80f, 0.80f, 0.80f }, { 0.0f, 0.38f, 0.0f });
		addEnemyProp(containerData, containerTexIndex_, { -4.10f, kFarLandY + 1.55f, 11.30f }, { 0.76f, 0.76f, 0.76f }, { 0.0f, 0.36f, 0.0f });
		addEnemyProp(containerData, containerTexIndex_, { -2.70f, kFarLandY, 12.80f }, { 0.78f, 0.78f, 0.78f }, { 0.0f, -0.22f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { -1.80f, kFarLandY, 10.40f }, { 0.65f, 0.65f, 0.65f }, { 0.0f, 0.15f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { -1.75f, kFarLandY + 0.82f, 10.50f }, { 0.50f, 0.50f, 0.50f }, { 0.0f, -0.20f, 0.0f });
		addEnemyProp(fenceData, sFenceTexIndex, { -4.00f, kFarLandY, 9.20f }, { 1.10f, 1.10f, 1.10f }, { 0.0f, 0.15f, 0.0f });
		addEnemyProp(fenceData, sFenceTexIndex, { -6.20f, kFarLandY, 9.50f }, { 1.10f, 1.10f, 1.10f }, { 0.0f, 0.05f, 0.0f });
		addEnemyProp(barrelData, barrelTexIndex_, { -5.40f, kFarLandY, 10.80f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, -0.30f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 3: 右翼高台の木造監視塔]
		// ---------------------------------------------------------
		addEnemyProp(towerData, sWatchtowerTexIndex, { 4.20f, kFarLandY, 12.00f }, { 0.85f, 0.85f, 0.85f }, { 0.0f, -0.30f, 0.0f });
		addEnemyProp(crateData, crateTexIndex_, { 3.00f, kFarLandY, 11.40f }, { 0.65f, 0.65f, 0.65f }, { 0.0f, -0.10f, 0.0f });
		addEnemyProp(barrelData, barrelTexIndex_, { 5.10f, kFarLandY, 12.50f }, { 0.60f, 0.60f, 0.60f }, { 0.0f, 0.30f, 0.0f });

		// ---------------------------------------------------------
		// [Zone 4: 最奥の開けた脱出地点 & 射撃場]
		// ---------------------------------------------------------
		addEnemyProp(targetData, sTargetTexIndex, { -1.50f, kFarLandY, 14.50f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, -0.15f, 0.0f });
		addEnemyProp(targetData, sTargetTexIndex, { 0.40f, kFarLandY, 15.20f }, { 0.55f, 0.55f, 0.55f }, { 0.0f, 0.20f, 0.0f });

		sExtractionPadTexIndex = TextureManager::GetInstance()->Load("Resources/extraction_pad.png");
		padData.material.textureIndex = sExtractionPadTexIndex;
		padData.boundingRadius = 15.0f;
		sExtractionPadModel = std::make_unique<Object3d>();
		sExtractionPadModel->Initialize(GetObject3dCom(), padData);
		sExtractionPadModel->SetCamera(camera_);
		sExtractionPadModel->SetScale({ 0.80f, 0.80f, 0.80f });
		sExtractionPadModel->SetTranslate({ 2.80f, kFarLandY, 14.50f });
		sExtractionPadModel->SetEnableLighting(true);
		sExtractionPadModel->SetAllowWireframeOverlay(false);

		// ---------------------------------------------------------
		// [敵兵アヒル小隊：役割に応じた位置配備]
		// ---------------------------------------------------------
		sEnemyDucks.clear();

		auto addEnemyDuck = [&](const Vector3& pos, float patrolR, float spd, float facing, float bobSpd = 3.5f, float bobAmp = 0.020f) {
			EnemyDuckInstance ed;
			ed.model = std::make_unique<Object3d>();
			ed.model->Initialize(GetObject3dCom(), enemyDuckData);
			ed.model->SetCamera(camera_);
			ed.model->SetScale({ 0.46f, 0.46f, 0.46f });
			ed.model->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			ed.basePos = pos;
			ed.patrolRadius = patrolR;
			ed.patrolSpeed = spd;
			ed.baseFacing = facing;
			ed.bobSpeed = bobSpd;
			ed.bobAmp = bobAmp;
			sEnemyDucks.push_back(std::move(ed));
		};

		// 1. 検問所の歩哨（橋の方向を警戒）
		addEnemyDuck({ 0.55f, kFarLandY, 8.20f }, 0.35f, 0.40f, kPi * 0.95f);
		// 2. 左翼コンテナヤードの巡回兵
		addEnemyDuck({ -2.50f, kFarLandY, 10.80f }, 0.55f, 0.50f, kPi + 0.30f);
		// 3. 右翼監視塔下の警戒兵（手前側を狙う）
		addEnemyDuck({ 3.50f, kFarLandY, 11.50f }, 0.25f, 0.30f, kPi - 0.40f);
		// 4. 最奥脱出地点の斥候兵
		addEnemyDuck({ 0.80f, kFarLandY, 14.00f }, 0.40f, 0.45f, kPi * 0.85f);

		// =========================================================
		// 6. 接地影デカール（静的オブジェクト＆主役アヒルの影を一括バッチ生成）
		// =========================================================
		{
			Object3d::ModelData shadowData{};
			shadowData.material.textureFilePath = "Resources/circle2.png";
			shadowData.material.textureIndex = circleTexIndex_;
			shadowData.boundingRadius = 60.0f;

			// 主役アヒル（手前岸）
			AddShadowDisc(shadowData, 3.30f, kLandY + 0.003f, 0.90f, 0.48f, 0.48f);

			// 手前岸の物資
			AddShadowDisc(shadowData, 4.40f, kLandY + 0.003f, 1.60f, 0.45f, 0.45f);
			AddShadowDisc(shadowData, 2.95f, kLandY + 0.003f, 1.85f, 0.55f, 0.35f);

			// 渡河木造橋の両端（岸に掛かる部分）
			AddShadowDisc(shadowData, 0.60f, kLandY + 0.003f, 2.30f, 1.05f, 0.30f);
			AddShadowDisc(shadowData, 0.60f, kFarLandY + 0.003f, 7.75f, 1.05f, 0.30f);

			// 対岸検問所（Zone 1）
			AddShadowDisc(shadowData, 0.40f, kFarLandY + 0.003f, 7.55f, 0.55f, 0.35f);
			AddShadowDisc(shadowData, -0.35f, kFarLandY + 0.003f, 7.35f, 0.28f, 0.28f);
			AddShadowDisc(shadowData, 1.45f, kFarLandY + 0.003f, 7.70f, 0.48f, 0.48f);

			// 左翼コンテナヤード（Zone 2）
			AddShadowDisc(shadowData, -4.20f, kFarLandY + 0.003f, 11.20f, 1.10f, 1.40f);
			AddShadowDisc(shadowData, -2.70f, kFarLandY + 0.003f, 12.80f, 1.10f, 1.30f);
			AddShadowDisc(shadowData, -1.80f, kFarLandY + 0.003f, 10.40f, 0.55f, 0.55f);
			AddShadowDisc(shadowData, -4.00f, kFarLandY + 0.003f, 9.20f, 0.55f, 0.25f);
			AddShadowDisc(shadowData, -6.20f, kFarLandY + 0.003f, 9.50f, 0.55f, 0.25f);
			AddShadowDisc(shadowData, -5.40f, kFarLandY + 0.003f, 10.80f, 0.40f, 0.40f);

			// 右翼監視塔（Zone 3）
			AddShadowDisc(shadowData, 3.6f, kFarLandY + 0.003f, 11.4f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 4.8f, kFarLandY + 0.003f, 11.4f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 3.6f, kFarLandY + 0.003f, 12.6f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 4.8f, kFarLandY + 0.003f, 12.6f, 0.30f, 0.30f);
			AddShadowDisc(shadowData, 3.00f, kFarLandY + 0.003f, 11.40f, 0.48f, 0.48f);
			AddShadowDisc(shadowData, 5.10f, kFarLandY + 0.003f, 12.50f, 0.45f, 0.45f);

			// 最奥脱出地点・射撃標的（Zone 4）
			AddShadowDisc(shadowData, 2.80f, kFarLandY + 0.003f, 14.50f, 1.35f, 1.35f);
			AddShadowDisc(shadowData, -1.50f, kFarLandY + 0.003f, 14.50f, 0.35f, 0.35f);
			AddShadowDisc(shadowData, 0.40f, kFarLandY + 0.003f, 15.20f, 0.35f, 0.35f);

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
			const std::vector<Vector2> farPath = { {0.6f,7.8f},{1.2f,10.0f},{2.2f,12.5f},{2.8f,14.0f} };
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

			auto broadleaf = [&](float x, float z, float g, float s) {
				MeshCylinder(batch[kTrunk], { x, g - 0.05f, z }, 0.10f * s, 0.06f * s, 1.25f * s);
				MeshBlob(batch[kLeaf], { x, g + 1.45f * s, z }, { 0.75f * s, 0.60f * s, 0.75f * s }, seed++, 0.12f);
				MeshBlob(batch[kLeafLight], { x + 0.45f * s, g + 1.20f * s, z - 0.20f * s }, { 0.48f * s, 0.40f * s, 0.48f * s }, seed++, 0.15f);
				MeshBlob(batch[kLeaf], { x - 0.40f * s, g + 1.25f * s, z + 0.25f * s }, { 0.50f * s, 0.42f * s, 0.50f * s }, seed++, 0.15f);
				MeshBlob(batch[kLeafLight], { x + 0.05f * s, g + 1.95f * s, z + 0.05f * s }, { 0.45f * s, 0.38f * s, 0.45f * s }, seed++, 0.15f);
			};
			auto pine = [&](float x, float z, float g, float s) {
				MeshCylinder(batch[kTrunk], { x, g - 0.05f, z }, 0.07f * s, 0.05f * s, 0.55f * s);
				MeshCone(batch[kPine], { x, g + 0.35f * s, z }, 0.62f * s, 1.00f * s, seed++);
				MeshCone(batch[kPine], { x, g + 0.80f * s, z }, 0.48f * s, 0.85f * s, seed++);
				MeshCone(batch[kPine], { x, g + 1.22f * s, z }, 0.32f * s, 0.70f * s, seed++);
			};
			auto bush = [&](float x, float z, float g, float s) {
				MeshBlob(batch[kBush], { x, g + 0.18f * s, z }, { 0.42f * s, 0.30f * s, 0.38f * s }, seed++, 0.18f);
				MeshBlob(batch[kBush], { x + 0.30f * s, g + 0.12f * s, z + 0.10f * s }, { 0.28f * s, 0.22f * s, 0.26f * s }, seed++, 0.18f);
				MeshBlob(batch[kLeafLight], { x - 0.25f * s, g + 0.14f * s, z - 0.08f * s }, { 0.26f * s, 0.22f * s, 0.24f * s }, seed++, 0.18f);
			};
			auto rock = [&](float x, float z, float g, float s) {
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
			bush(2.15f, 2.25f, kLandY, 0.8f);
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
				if (!ok) continue;
				tuft(x, z, kLandY, HashRange(seed, 0.8f, 1.4f), (placed % 3 == 0) ? kReed : kGrass);
				++placed;
			}
			placed = 0;
			for (int tries = 0; tries < 450 && placed < 75; ++tries)
			{
				float x = HashRange(seed, -9.0f, 8.5f);
				float z = HashRange(seed, FarShoreZ(x) + 0.2f, 15.0f);
				if (distToPath(farPath, x, z) < 0.6f) continue;
				if (x > -5.2f && x < -1.2f && z > 9.8f && z < 13.8f) continue; // コンテナヤード
				if (x > 2.6f && x < 5.6f && z > 10.8f && z < 13.2f) continue;  // 監視塔
				if (std::abs(x - 2.8f) < 1.6f && std::abs(z - 14.5f) < 1.6f) continue; // 脱出地点
				if (z < 8.8f && x > -1.9f && x < 2.0f) continue; // 検問所
				tuft(x, z, kFarLandY, HashRange(seed, 1.0f, 1.6f), kGrass);
				++placed;
			}

			// --- 対岸：茂みと陣地脇の木立（特に左翼の更地を埋める鬱蒼とした森林地帯） ---
			// [左翼森林：コンテナの裏・横から川沿い・ロゴ背後までを高く重層的にカバー]
			pine(-3.6f, 11.2f, kFarLandY, 2.5f);
			pine(-4.8f, 9.6f, kFarLandY, 1.8f);
			pine(-4.6f, 12.2f, kFarLandY, 2.7f);
			pine(-5.8f, 10.5f, kFarLandY, 2.2f);
			pine(-6.0f, 12.8f, kFarLandY, 2.9f);
			pine(-7.2f, 11.2f, kFarLandY, 2.4f);
			pine(-7.6f, 13.6f, kFarLandY, 3.0f);
			pine(-8.6f, 10.2f, kFarLandY, 2.0f);
			broadleaf(-5.0f, 13.2f, kFarLandY, 1.9f);
			broadleaf(-6.8f, 13.8f, kFarLandY, 2.0f);
			pine(-6.4f, 11.5f, kFarLandY, 2.0f);
			pine(-8.8f, 13.0f, kFarLandY, 2.6f);
			pine(-10.5f, 11.5f, kFarLandY, 2.5f);
			broadleaf(-9.5f, 14.6f, kFarLandY, 2.1f);
			pine(-11.2f, 13.8f, kFarLandY, 2.8f);

			bush(-3.2f, 8.4f, kFarLandY, 1.2f);
			bush(-4.5f, 8.8f, kFarLandY, 1.2f);
			bush(-5.8f, 9.0f, kFarLandY, 1.3f);
			bush(-7.2f, 9.4f, kFarLandY, 1.2f);
			bush(-8.4f, 10.8f, kFarLandY, 1.1f);
			rock(-0.9f, 9.6f, kFarLandY, 1.0f);
			rock(-5.0f, 9.0f, kFarLandY, 1.1f);
			rock(-7.5f, 9.6f, kFarLandY, 0.9f);

			// [中央・右翼の木立]
			bush(2.9f, 8.6f, kFarLandY, 1.0f);
			bush(5.8f, 9.2f, kFarLandY, 1.3f);
			broadleaf(6.6f, 10.6f, kFarLandY, 1.4f);
			pine(7.2f, 13.0f, kFarLandY, 1.8f);

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
						AddShadowDisc(pathData, path[i].x + dx * t + HashRange(seed, -0.05f, 0.05f), g + 0.002f,
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

	// 1. 周辺減光ビネット
	if (auto vig = Sprite::Create(sc, defaultTransform, "Resources/title_vignette.png"))
	{
		vig->SetAnchorPoint({ 0.0f, 0.0f });
		vig->SetPosition({ 0.0f, 0.0f });
		vig->SetSize({ sScreenW, sScreenH });
		vig->SetColor({ 1.0f, 1.0f, 1.0f, 0.75f });
		sprites_.emplace_back(std::move(vig));
		vignetteSprite_ = sprites_.back().get();
	}

	// 2. タイトルロゴ（画面左上：右側の主役アヒルと脱出煙に被らない位置）
	if (auto logo = Sprite::Create(sc, defaultTransform, "Resources/title_logo.png"))
	{
		sLogoFinalPos = { 48.0f, 34.0f };
		logo->SetAnchorPoint({ 0.0f, 0.0f });
		logo->SetPosition({ sLogoFinalPos.x, -sLogoSize.y - 20.0f });
		logo->SetSize(sLogoSize);
		logo->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(logo));
		titleLogoSprite_ = sprites_.back().get();
	}

	// 3. PRESS SPACE（左下：ロゴと縦ラインを揃える）
	if (auto prompt = Sprite::Create(sc, defaultTransform, "Resources/title_press_space.png"))
	{
		prompt->SetAnchorPoint({ 0.5f, 0.5f });
		prompt->SetPosition({ 48.0f + 235.0f, sScreenH * 0.88f });
		prompt->SetSize({ 470.0f, 66.0f });
		prompt->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		sprites_.emplace_back(std::move(prompt));
		pressSpaceSprite_ = sprites_.back().get();
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
	sTracers.clear();
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
	deltaTime = std::clamp(deltaTime, 0.0001f, 0.1f);
	bgTimer_ += deltaTime;
	sIntroTimer += deltaTime;

	const bool introPlaying = sIntroTimer < kIntroDuration;

	// ---------------------------------------------------------------
	// 入力：SPACEで出撃
	// ---------------------------------------------------------------
	if (!isStarting_ && input_)
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
			sShakeTimer = 0.20f;
			sShakePower = 0.03f;
			if (sAlarmSoundId >= 0 && SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
			{
				SceneManager::GetInstance()->GetAudioManager()->Play(sAlarmSoundId);
			}
			if (appParticleManager_ && GetParticleManager())
			{
				auto& rng = GetParticleManager()->GetRandomEngine();
				Vector3 muzzle = duckBasePos_ + Vector3{ 0.15f, 0.42f, 0.35f };
				appParticleManager_->EmitMuzzleFlash(rng, muzzle, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.85f, 0.35f, 1.0f }, 2.0f, circleTexIndex_);
				appParticleManager_->EmitMuzzleFlare(rng, muzzle, 0.70f, { 1.0f, 0.95f, 0.80f, 1.0f }, 0.12f, starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_);
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
		float fadeAlpha = (std::min)(1.0f, startTransitionTimer_ / 0.55f);
		if (fadeOverlaySprite_)
		{
			fadeOverlaySprite_->SetColor({ 0.03f, 0.04f, 0.05f, fadeAlpha });
		}
		if (startTransitionTimer_ >= 0.60f)
		{
			SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
			return;
		}
	}

	mouseInput_.Update();

	// ---------------------------------------------------------------
	// ロゴ着弾（落ち着いた金属音的な微小シェイク）
	// ---------------------------------------------------------------
	if (!sLogoLanded && sIntroTimer >= kLogoSlamTime + kLogoSlideTime)
	{
		sLogoLanded = true;
		sShakeTimer = 0.20f;
		sShakePower = 0.035f;
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

	Vector3 mouseWaterPos{ 0.0f, kWaterY, 5.0f };
	Vector3 mouseGroundPos{ 0.0f, kLandY, 5.0f };
	bool hasMouseRay = false;
	bool isOverWater = false;
	float normX = 0.0f, normY = 0.0f;

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

			camPos.x += normX * 0.12f * idleBlend;
			camPos.y -= normY * 0.06f * idleBlend;
			camRot.y += normX * 0.015f * idleBlend;
			camRot.x += normY * 0.008f * idleBlend;

			if (sShakeTimer > 0.0f)
			{
				float k = sShakePower * (sShakeTimer / 0.20f);
				camPos.x += (Rand01() - 0.5f) * k;
				camPos.y += (Rand01() - 0.5f) * k;
			}

			camera_->SetTranslate(camPos);
			camera_->SetRotate(camRot);
			camera_->Update();

			float ny = 1.0f - (mp.y / ch) * 2.0f;
			Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));
			auto unproject = [&](float z) -> Vector3 {
				Vector4 c = { normX, ny, z, 1.0f };
				Vector3 r;
				r.x = c.x * inv.m[0][0] + c.y * inv.m[1][0] + c.z * inv.m[2][0] + c.w * inv.m[3][0];
				r.y = c.x * inv.m[0][1] + c.y * inv.m[1][1] + c.z * inv.m[2][1] + c.w * inv.m[3][1];
				r.z = c.x * inv.m[0][2] + c.y * inv.m[1][2] + c.z * inv.m[2][2] + c.w * inv.m[3][2];
				float w = c.x * inv.m[0][3] + c.y * inv.m[1][3] + c.z * inv.m[2][3] + c.w * inv.m[3][3];
				if (w != 0.0f) { r.x /= w; r.y /= w; r.z /= w; }
				return r;
			};
			Vector3 pN = unproject(0.0f);
			Vector3 pF = unproject(1.0f);
			Vector3 dir = { pF.x - pN.x, pF.y - pN.y, pF.z - pN.z };
			if (dir.y < -1e-5f)
			{
				float tw = (kWaterY - pN.y) / dir.y;
				mouseWaterPos = { pN.x + dir.x * tw, kWaterY, pN.z + dir.z * tw };
				float tg = (kLandY - pN.y) / dir.y;
				mouseGroundPos = { pN.x + dir.x * tg, kLandY, pN.z + dir.z * tg };
				isOverWater = mouseWaterPos.z > NearShoreZ(mouseWaterPos.x) && mouseWaterPos.z < FarShoreZ(mouseWaterPos.x);
				hasMouseRay = true;
			}
		}
	}

	if (sShakeTimer > 0.0f) sShakeTimer -= deltaTime;

	for (auto& s : mouseWaveSources_) s.age += deltaTime;
	mouseWaveSources_.erase(
		std::remove_if(mouseWaveSources_.begin(), mouseWaveSources_.end(),
			[](const MouseWaveRippleSource& s) { return s.age >= s.maxLife; }),
		mouseWaveSources_.end());
	mouseRippleCooldown_ -= deltaTime;

	// ---------------------------------------------------------------
	// ライティング：C案の澄んだ午後の陽光（黄金色のキーライト + 柔らかな水面反射）
	// ---------------------------------------------------------------
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		Vector3 sunDir = { -0.42f, -0.78f, 0.46f };
		float len = std::sqrt(sunDir.x * sunDir.x + sunDir.y * sunDir.y + sunDir.z * sunDir.z);
		sunDir = { sunDir.x / len, sunDir.y / len, sunDir.z / len };
		light->SetDirectionalLight({ 1.0f, 0.94f, 0.82f, 1.0f }, sunDir, 1.35f);

		float flash = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / 0.14f) * 1.8f : 0.0f;
		light->SetPointLight(
			{ 1.0f, 0.95f, 0.82f, 1.0f },
			{ duckBasePos_.x + 0.2f, duckBasePos_.y + 0.8f, duckBasePos_.z - 0.5f },
			1.0f + flash, 5.5f, 1.6f);
	}

	if (sDuckRecoilTimer > 0.0f) sDuckRecoilTimer -= deltaTime;

	// ---------------------------------------------------------------
	// 主役アヒル（分隊長）：渡河橋を見据え、マウスXにわずかにエイム追従
	// ---------------------------------------------------------------
	if (duckModel_)
	{
		float breathe = std::sin(bgTimer_ * 2.0f) * 0.008f;
		float recoil = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / 0.14f) : 0.0f;
		float yaw = kDuckFacingYaw + std::sin(bgTimer_ * 0.8f) * 0.04f + normX * 0.15f * idleBlend;
		duckModel_->SetTranslate({ duckBasePos_.x, duckBasePos_.y + breathe, duckBasePos_.z - recoil * 0.04f });
		duckModel_->SetRotate({ 0.06f * recoil, yaw, 0.0f });
		duckModel_->Update();
	}

	// ---------------------------------------------------------------
	// 対岸の敵兵アヒル小隊の更新（各アヒルが生き生きと警戒行動）
	// ---------------------------------------------------------------
	for (size_t i = 0; i < sEnemyDucks.size(); ++i)
	{
		auto& ed = sEnemyDucks[i];
		if (!ed.model) continue;
		float phase = bgTimer_ * ed.patrolSpeed + static_cast<float>(i) * 1.57f;
		float offX = (ed.patrolRadius > 0.0f) ? std::sin(phase) * ed.patrolRadius : 0.0f;
		float bob = std::abs(std::sin(bgTimer_ * ed.bobSpeed + static_cast<float>(i))) * ed.bobAmp;
		float facing = ed.baseFacing;
		if (ed.patrolRadius > 0.35f)
		{
			facing = (std::cos(phase) > 0.0f) ? (kPi * 0.5f) : (-kPi * 0.5f);
		}
		Vector3 curPos = { ed.basePos.x + offX, ed.basePos.y + bob, ed.basePos.z };
		ed.model->SetTranslate(curPos);
		ed.model->SetRotate({ 0.0f, facing, std::sin(bgTimer_ * ed.bobSpeed) * 0.02f });
		ed.model->Update();

		if (i < sEnemyDuckShadows.size() && sEnemyDuckShadows[i])
		{
			sEnemyDuckShadows[i]->SetTranslate({ curPos.x, kFarLandY + 0.003f, curPos.z });
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

	// ---------------------------------------------------------------
	// マウス射撃（待機中のみ：クリックで銃弾発射＆水面波紋）
	// ---------------------------------------------------------------
	bool isMouseFire = !introPlaying && !isStarting_ &&
		(mouseInput_.TriggerButton(0) || (mouseInput_.PushButton(0) && mouseRippleCooldown_ <= 0.0f));
	if (isMouseFire && hasMouseRay)
	{
		mouseRippleCooldown_ = 0.16f;
		sDuckRecoilTimer = 0.14f;
		sShakeTimer = (std::max)(sShakeTimer, 0.06f);
		sShakePower = (std::max)(sShakePower, 0.015f);

		Vector3 muzzle = duckBasePos_ + Vector3{ 0.15f, 0.42f, 0.35f };
		Vector3 target = isOverWater ? mouseWaterPos : mouseGroundPos;
		Vector3 d = target - muzzle;
		float l = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
		if (l > 0.0f) d = { d.x / l, d.y / l, d.z / l };

		TracerRound tr;
		tr.position = muzzle;
		tr.velocity = d * 50.0f;
		tr.color = { 1.0f, 0.88f, 0.40f, 1.0f };
		tr.maxLife = 0.7f;
		tr.isPlayerShot = true;
		sTracers.push_back(tr);

		if (appParticleManager_ && GetParticleManager())
		{
			auto& rng = GetParticleManager()->GetRandomEngine();
			Vector3 right = { d.z, 0.0f, -d.x };
			appParticleManager_->EmitMuzzleFlash(rng, muzzle, d, right, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.85f, 0.35f, 1.0f }, 1.6f, circleTexIndex_);
			appParticleManager_->EmitMuzzleFlare(rng, muzzle, 0.45f, { 1.0f, 0.95f, 0.8f, 1.0f }, 0.08f, starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_);
			appParticleManager_->EmitShellCasing(rng, muzzle, { 0.6f, 1.1f, -0.4f }, { 0.95f, 0.82f, 0.28f, 1.0f }, { 0.045f, 0.045f, 0.045f }, circleTexIndex_);
		}
	}

	AppParticleManager* pm = appParticleManager_.get();

	// 曳光弾更新
	for (auto it = sTracers.begin(); it != sTracers.end(); )
	{
		it->age += deltaTime;
		if (it->age < 0.0f) { ++it; continue; }

		Vector3 step = it->velocity * deltaTime;
		Vector3 next = it->position + step;
		bool remove = false;
		bool impact = false;
		bool impactWater = false;
		Vector3 hp = next;

		if (it->isPlayerShot && it->position.y > kWaterY && next.y <= kWaterY && isOverWater)
		{
			hp = { next.x, kWaterY, next.z };
			remove = impact = impactWater = true;
		}
		else if (it->isPlayerShot && it->position.y > kLandY && next.y <= kLandY && !isOverWater)
		{
			hp = { next.x, kLandY, next.z };
			remove = impact = true;
		}
		if (!remove && it->age >= it->maxLife)
		{
			remove = true;
		}

		if (pm)
		{
			FxTracerSegment(pm, it->position, impact ? hp : next, it->color);
			if (impact)
			{
				FxBulletImpact(pm, hp, impactWater);
				if (impactWater && mouseWaveSources_.size() < 16)
				{
					MouseWaveRippleSource rip;
					rip.center = hp;
					rip.maxLife = 1.6f;
					rip.power = 0.35f;
					mouseWaveSources_.push_back(rip);
				}
			}
		}

		it->position = next;
		it = remove ? sTracers.erase(it) : it + 1;
	}

	// 背景モデル更新
	if (sNearGroundModel) sNearGroundModel->Update();
	if (sFarGroundModel) sFarGroundModel->Update();
	if (sShadowDecalsModel) sShadowDecalsModel->Update();
	if (sPathDecalModel) sPathDecalModel->Update();
	for (auto& s : sScenery)
	{
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

		// (C) 両岸の白波
		sRiverFoamTimer += deltaTime;
		while (sRiverFoamTimer >= 0.05f)
		{
			sRiverFoamTimer -= 0.05f;
			float fx = RandRange(-7.0f, 7.0f);
			bool nearSide = Rand01() < 0.5f;
			float fz = nearSide ? NearShoreZ(fx) + 0.30f : FarShoreZ(fx) - 0.30f;
			appParticleManager_->EmitRiverSplashDroplet(rng, { fx, kWaterY + 0.02f, fz }, circleTexIndex_);
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
		float lt = (sIntroTimer - kLogoSlamTime) / kLogoSlideTime;
		if (lt <= 0.0f)
		{
			titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, 0.0f });
		}
		else
		{
			float e = EaseOutBack(lt);
			float startY = -sLogoSize.y - 20.0f;
			float y = startY + (sLogoFinalPos.y - startY) * e;
			float punch = 1.0f;
			if (sLogoLanded)
			{
				float since = sIntroTimer - (kLogoSlamTime + kLogoSlideTime);
				punch = 1.0f + 0.08f * std::exp(-since * 8.0f);
			}
			Vector2 size = { sLogoSize.x * punch, sLogoSize.y * punch };
			Vector2 pos = { sLogoFinalPos.x - (size.x - sLogoSize.x) * 0.5f, y - (size.y - sLogoSize.y) * 0.5f };
			titleLogoSprite_->SetPosition(pos);
			titleLogoSprite_->SetSize(size);
			titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, std::clamp(lt * 2.5f, 0.0f, 1.0f) });
		}
	}

	if (pressSpaceSprite_ && !isStarting_)
	{
		float appear = sLogoLanded ? std::clamp((sIntroTimer - kIntroDuration) / 0.6f, 0.0f, 1.0f) : 0.0f;
		float pulse = 0.5f + 0.5f * std::sin(bgTimer_ * 3.2f);
		pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, appear * (0.45f + 0.55f * pulse) });
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
