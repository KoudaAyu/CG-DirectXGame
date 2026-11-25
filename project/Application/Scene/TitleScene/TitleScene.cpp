#include "TitleScene.h"
#include "TitleSceneTuning.h"
#include "../GameScene/RaidStats.h"
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
#include "Application/Audio/TacticalAudio.h"
#include "Application/Config/AppDebugConfig.h"
#include "Application/Config/AppFastObjLoader.h"
#include "Application/Transition/StingerTransition.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <unordered_map>
#include <vector>

namespace
{
	using namespace TitleSceneTuning;
	using GameConfig::Math::kHalfPi;
	using GameConfig::Math::kPi;
	using GameConfig::Math::kTwoPi;
	using GameConfig::Math::Square;

	// 3Dモデルデータ静的キャッシュ 
	std::unordered_map<std::string, Object3d::ModelData> sModelDataCache;

	const Object3d::ModelData& GetOrLoadModelData(const std::string& directoryPath, const std::string& filename)
	{
		const std::string key = directoryPath + "/" + filename;
		if (auto it = sModelDataCache.find(key); it != sModelDataCache.end()) return it->second;
		sModelDataCache[key] = AppFastObjLoader::LoadObjCached(directoryPath, filename);
		return sModelDataCache[key];
	}

	// シーン内静的オブジェクト・テクスチャ・状態保持 
	uint32_t sSmokeTexIndex = UINT32_MAX;
	uint32_t sGrassTexIndex = UINT32_MAX;
	uint32_t sSignpostTexIndex = UINT32_MAX;
	uint32_t sTargetTexIndex = UINT32_MAX;
	uint32_t sBridgeTexIndex = UINT32_MAX;
	uint32_t sWatchtowerTexIndex = UINT32_MAX;
	uint32_t sExtractionPadTexIndex = UINT32_MAX;
	uint32_t sWhiteTexIndex = UINT32_MAX;

	std::unique_ptr<Object3d> sNearGroundModel;
	std::unique_ptr<Object3d> sFarGroundModel;
	std::unique_ptr<Object3d> sExtractionPadModel;
	std::unique_ptr<Object3d> sShadowDecalsModel;
	std::unique_ptr<Object3d> sPathDecalModel;
	std::vector<std::unique_ptr<Object3d>> sEnemyDuckShadows;

	struct PropInstance { std::unique_ptr<Object3d> model; uint32_t textureIndex = UINT32_MAX; };
	std::vector<PropInstance> sEnemyProps;

	struct EnemyDuckInstance { std::unique_ptr<Object3d> model; EnemySquad::EnemyDuckSpec spec{}; };
	std::vector<EnemyDuckInstance> sEnemyDucks;

	struct FxTextures
	{
		uint32_t circle = UINT32_MAX;
		uint32_t star = UINT32_MAX;
		uint32_t puff = UINT32_MAX;
		uint32_t leaf = UINT32_MAX;
	};
	FxTextures sFx;

	struct SceneryVertexBase { Vector3 basePos{}; float swayFactor = 0.0f; };
	struct SceneryBatch
	{
		std::unique_ptr<Object3d> model;
		std::vector<SceneryVertexBase> baseVertices;
		uint32_t textureIndex = UINT32_MAX;
		bool hasSway = false;
		bool isFarForest = false;
	};
	std::vector<SceneryBatch> sScenery;

	struct FallingLeaf3D
	{
		std::unique_ptr<Object3d> model;
		Vector3 pos{ 0.0f, 0.0f, 0.0f };
		Vector3 baseVel{ 0.0f, 0.0f, 0.0f };
		Vector3 rot{ 0.0f, 0.0f, 0.0f };
		Vector3 rotSpeed{ 0.0f, 0.0f, 0.0f };
		Vector3 wobbleDir = Leaves::kDefaultWobbleDir;
		Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float scale = Leaves::kDefaultScale;
		float age = 0.0f;
		float maxLife = Leaves::kDefaultMaxLife;
		float wobbleFreq = Leaves::kDefaultWobbleFreq;
		float wobbleAmp = Leaves::kDefaultWobbleAmp;
		float wobblePhase = 0.0f;
		bool active = false;
	};
	std::vector<FallingLeaf3D> sFallingLeaves;

	// 演出・UI・オーディオ・カメラロック状態 
	float sIntroTimer = 0.0f;
	bool sLogoLanded = false;
	float sScreenW = UI::kDefaultScreenW;
	float sScreenH = UI::kDefaultScreenH;
	Vector2 sLogoFinalPos = UI::kLogoFinalPos;
	Vector2 sLogoSize = UI::kLogoSize;
	Vector2 sPromptBasePos = { 0.0f, 0.0f };

	Sprite* sLogoGhosts[UI::kLogoGhostCount] = { nullptr, nullptr };
	Sprite* sLogoShine = nullptr;
	Sprite* sPromptShine = nullptr;

	float sExtractionSmokeTimer = 0.0f;
	float sBeaconMoteTimer = 0.0f;
	float sRiverFoamTimer = 0.0f;
	float sAmbientDustTimer = 0.0f;
	float sRiverWaveTimer = 0.0f;
	float sRiverWakeTimer = 0.0f;
	float sRiverGlintTimer = 0.0f;
	float sLeafTimer = 0.0f;
	float sGroundDustTimer = 0.0f;

	float sDuckRecoilTimer = 0.0f;
	float sDuckClickHopTimer = 0.0f;
	int32_t sBgmSoundId = -1;
	int32_t sBgmPlayId = -1;
	float sBgmTimer = 0.0f;
	int32_t sDuckSeSoundId = -1;
	int32_t sDuckSePlayId = -1;

	bool sCameraLocked = false;
	Vector3 sCameraLockedPos = { 0.0f, 0.0f, 0.0f };
	Vector3 sCameraLockedRot = { 0.0f, 0.0f, 0.0f };

	// 数学・イージング・乱数・行列ヘルパー 
	AudioManager* GetAudioMgr()
	{
		return SceneManager::GetInstance() ? SceneManager::GetInstance()->GetAudioManager() : nullptr;
	}

	float EaseInOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		if (t < 0.5f) return Common::kCubicInScale * t * t * t;
		const float u = -2.0f * t + 2.0f;
		return 1.0f - (u * u * u) * 0.5f;
	}

	float EaseOutBack(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		const float u = t - 1.0f;
		const float u2 = u * u;
		return 1.0f + Common::kBackOvershootPlusOne * (u2 * u) + Common::kBackOvershoot * u2;
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
	float SampleRange(const RangeF& r) { return (r.minVal == r.maxVal) ? r.minVal : RandRange(r.minVal, r.maxVal); }
	float RandSign() { return (rand() % Common::kCoinFlipMod == 0) ? 1.0f : -1.0f; }
	float Length3(const Vector3& v) { return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

	Vector3 JitterPosXZ(float x, float y, float z, float jitter)
	{
		const float dx = RandRange(-jitter, jitter);
		const float dz = RandRange(-jitter, jitter);
		return { x + dx, y, z + dz };
	}

	float Hash01(uint32_t& s)
	{
		s = s * Common::kLcgMultiplier + Common::kLcgIncrement;
		return static_cast<float>(s >> Common::kHashShiftBits) * Common::kHashInvMax;
	}

	float HashRange(uint32_t& s, float a, float b) { return a + (b - a) * Hash01(s); }
	float HashSample(uint32_t& s, const RangeF& r) { return HashRange(s, r.minVal, r.maxVal); }

	template <typename Fn>
	void ConsumeInterval(float& timer, float dt, float interval, Fn&& fn)
	{
		timer += dt;
		while (timer >= interval)
		{
			timer -= interval;
			fn();
		}
	}

	Vector4 MultiplyRowVec4(const Vector4& c, const Matrix4x4& mat) noexcept
	{
		using namespace Common;
		return {
			c.x * mat.m[kMatAxisX][kMatAxisX] + c.y * mat.m[kMatAxisY][kMatAxisX] + c.z * mat.m[kMatAxisZ][kMatAxisX] + c.w * mat.m[kMatAxisW][kMatAxisX],
			c.x * mat.m[kMatAxisX][kMatAxisY] + c.y * mat.m[kMatAxisY][kMatAxisY] + c.z * mat.m[kMatAxisZ][kMatAxisY] + c.w * mat.m[kMatAxisW][kMatAxisY],
			c.x * mat.m[kMatAxisX][kMatAxisZ] + c.y * mat.m[kMatAxisY][kMatAxisZ] + c.z * mat.m[kMatAxisZ][kMatAxisZ] + c.w * mat.m[kMatAxisW][kMatAxisZ],
			c.x * mat.m[kMatAxisX][kMatAxisW] + c.y * mat.m[kMatAxisY][kMatAxisW] + c.z * mat.m[kMatAxisZ][kMatAxisW] + c.w * mat.m[kMatAxisW][kMatAxisW],
		};
	}

	Vector3 UnprojectClipPoint(const Vector4& clip, const Matrix4x4& invViewProj) noexcept
	{
		const Vector4 w = MultiplyRowVec4(clip, invViewProj);
		return (w.w != 0.0f) ? Vector3{ w.x / w.w, w.y / w.w, w.z / w.w } : Vector3{ 0.0f, 0.0f, 0.0f };
	}

	// 2D UI グリントセル・スプライト生成ヘルパー 
	void SetShineCell(Sprite* sp, int cell, float cellW, float cellH)
	{
		const int c = cell % UI::kShineCols;
		const int r = cell / UI::kShineCols;
		sp->SetTextureLeftTop({ static_cast<float>(c) * cellW, static_cast<float>(r) * cellH });
		sp->SetTextureSize({ cellW, cellH });
	}

	float ApplyShineFlash(Sprite* sp, float elapsed, float duration, float texW, float texH)
	{
		const float f = 1.0f - std::clamp(elapsed / duration, 0.0f, 1.0f);
		SetShineCell(sp, UI::kShineFlashCell, texW, texH);
		return f * f;
	}

	int ShineFrame(float t, float period, float duration)
	{
		if (t < 0.0f) return -1;
		const float ph = std::fmod(t, period);
		if (ph >= duration) return -1;
		return std::clamp(static_cast<int>(ph / duration * UI::kShineSweepFrames), 0, UI::kShineSweepFrames - 1);
	}

	Sprite* CreateSpriteInstance(SpriteCom* sc, std::vector<std::unique_ptr<Sprite>>& outList,
		const char* texturePath, const Vector2& anchor, const Vector2& pos, const Vector2& size, const Vector4& color)
	{
		const Sprite::Transform defaultTransform = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f } };
		auto sp = Sprite::Create(sc, defaultTransform, texturePath);
		if (!sp) return nullptr;
		sp->SetAnchorPoint(anchor);
		sp->SetPosition(pos);
		sp->SetSize(size);
		sp->SetColor(color);
		outList.emplace_back(std::move(sp));
		return outList.back().get();
	}

	// 3Dオブジェクト・プロシージャルメッシュ構築・描画ヘルパー 
	std::unique_ptr<Object3d> CreateSceneObject(Object3dCom* com, Camera* cam, const Object3d::ModelData& data,
		const Vector3& pos, const Vector3& scale, const Vector3& rot = { 0.0f, 0.0f, 0.0f },
		const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f }, bool enableLighting = true, bool allowWireframe = true)
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(com, data);
		obj->SetCamera(cam);
		obj->SetTranslate(pos);
		obj->SetScale(scale);
		obj->SetRotate(rot);
		obj->SetColor(color);
		obj->SetEnableLighting(enableLighting);
		obj->SetAllowWireframeOverlay(allowWireframe);
		return obj;
	}

	template <typename Fn>
	void UpdateMappedVertices(Object3d* model, size_t vertexCount, Fn&& fn)
	{
		if (!model || !model->GetVertexResource() || vertexCount == 0) return;
		Object3d::VertexData* vData = nullptr;
		D3D12_RANGE readRange{ 0, 0 };
		if (SUCCEEDED(model->GetVertexResource()->Map(0, &readRange, reinterpret_cast<void**>(&vData))) && vData)
		{
			fn(vData);
			D3D12_RANGE writeRange{ 0, sizeof(Object3d::VertexData) * vertexCount };
			model->GetVertexResource()->Unmap(0, &writeRange);
		}
	}

	void BuildGridIndices(Object3d::ModelData& data, int cols, int rows)
	{
		data.indices.reserve(static_cast<size_t>((rows - 1) * (cols - 1)) * Common::kIndicesPerQuad);
		for (int r = 0; r < rows - 1; ++r)
		{
			for (int c = 0; c < cols - 1; ++c)
			{
				const uint32_t i0 = static_cast<uint32_t>(r * cols + c);
				const uint32_t i1 = static_cast<uint32_t>(r * cols + (c + 1));
				const uint32_t i2 = static_cast<uint32_t>((r + 1) * cols + c);
				const uint32_t i3 = static_cast<uint32_t>((r + 1) * cols + (c + 1));
				data.indices.insert(data.indices.end(), { i0, i1, i2, i1, i3, i2 });
			}
		}
	}

	Vector3 SlopeNormalZ(float dYdZ)
	{
		const Vector3 n = { 0.0f, 1.0f, -dYdZ };
		const float len = std::sqrt(n.y * n.y + n.z * n.z);
		return { 0.0f, n.y / len, n.z / len };
	}

	void DrawModel(Object3dCom* com, Object3d* model, const RenderContext& base, uint32_t texIndex)
	{
		if (!com || !model) return;
		RenderContext ctx = base;
		if (texIndex != UINT32_MAX) ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
		com->Draw(model, ctx, model->GetModelData(), true);
	}

	void AppendQuad(Object3d::ModelData& data, const Vector3 positions[Common::kQuadVertCount],
		const Vector2 uvs[Common::kQuadVertCount], const Vector3& normal,
		std::span<const uint32_t, Common::kIndicesPerQuad> indices)
	{
		const uint32_t base = static_cast<uint32_t>(data.vertices.size());
		for (size_t i = 0; i < Common::kQuadVertCount; ++i)
		{
			Sprite::VertexData v{};
			v.position = { positions[i].x, positions[i].y, positions[i].z, 1.0f };
			v.texcoord = uvs[i];
			v.normal = normal;
			data.vertices.push_back(v);
		}
		for (uint32_t idx : indices) data.indices.push_back(base + idx);
	}

	void AddShadowDisc(Object3d::ModelData& data, float cx, float cy, float cz, float rx, float rz)
	{
		const Vector2 uvs[Common::kQuadVertCount] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f } };
		const Vector3 positions[Common::kQuadVertCount] = {
			{ cx - rx, cy, cz - rz }, { cx + rx, cy, cz - rz },
			{ cx - rx, cy, cz + rz }, { cx + rx, cy, cz + rz },
		};
		AppendQuad(data, positions, uvs, { 0.0f, 1.0f, 0.0f }, Common::kQuadTriIndices);
	}

	void MeshTri(Object3d::ModelData& d, const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& center)
	{
		const Vector3 u = b - a, v = c - a;
		Vector3 n = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
		const float l = Length3(n);
		if (l < Common::kMinNormalLen) return;
		n = n * (1.0f / l);
		const Vector3 fc = {
			(a.x + b.x + c.x) / Common::kTriVertCountF,
			(a.y + b.y + c.y) / Common::kTriVertCountF,
			(a.z + b.z + c.z) / Common::kTriVertCountF
		};
		const Vector3 o = fc - center;
		if (n.x * o.x + n.y * o.y + n.z * o.z < 0.0f) n = n * -1.0f;

		for (const Vector3* p : { &a, &b, &c })
		{
			Sprite::VertexData vtx{};
			vtx.position = { p->x, p->y, p->z, 1.0f };
			vtx.texcoord = { 0.5f, 0.5f };
			vtx.normal = n;
			d.indices.push_back(static_cast<uint32_t>(d.vertices.size()));
			d.vertices.push_back(vtx);
		}
	}

	void MeshBlob(Object3d::ModelData& d, const Vector3& c, const Vector3& r, uint32_t seed, float jitter)
	{
		using Common::kBlobRings;
		using Common::kBlobSegments;
		Vector3 p[kBlobRings + 1][kBlobSegments];
		for (int i = 0; i <= kBlobRings; ++i)
		{
			const float phi = kPi * static_cast<float>(i) / static_cast<float>(kBlobRings);
			const float k = (i == 0 || i == kBlobRings) ? 1.0f : 1.0f + HashRange(seed, -jitter, jitter);
			for (int j = 0; j < kBlobSegments; ++j)
			{
				const float th = kTwoPi * (static_cast<float>(j) + static_cast<float>(i & 1) * 0.5f) / static_cast<float>(kBlobSegments);
				const float kk = (i == 0 || i == kBlobRings) ? 1.0f : k * (1.0f + HashRange(seed, -jitter, jitter) * 0.5f);
				p[i][j] = {
					c.x + r.x * kk * std::sin(phi) * std::cos(th),
					c.y + r.y * kk * std::cos(phi),
					c.z + r.z * kk * std::sin(phi) * std::sin(th)
				};
			}
		}
		for (int i = 0; i < kBlobRings; ++i)
		{
			for (int j = 0; j < kBlobSegments; ++j)
			{
				const int jn = (j + 1) % kBlobSegments;
				MeshTri(d, p[i][j], p[i][jn], p[i + 1][jn], c);
				MeshTri(d, p[i][j], p[i + 1][jn], p[i + 1][j], c);
			}
		}
	}

	void MeshBlade(Object3d::ModelData& d, const Vector3& base, float width, float height, float leanX, float leanZ)
	{
		using Common::kBladeCenterRatio;
		using Common::kBladeSides;
		using Common::kBladeSidesF;
		const Vector3 tip = { base.x + leanX, base.y + height, base.z + leanZ };
		const Vector3 center = { base.x + leanX * kBladeCenterRatio, base.y + height * kBladeCenterRatio, base.z + leanZ * kBladeCenterRatio };
		Vector3 b[kBladeSides];
		for (int j = 0; j < kBladeSides; ++j)
		{
			const float th = kTwoPi * static_cast<float>(j) / kBladeSidesF;
			b[j] = { base.x + width * std::cos(th), base.y, base.z + width * std::sin(th) };
		}
		for (int j = 0; j < kBladeSides; ++j) MeshTri(d, b[j], b[(j + 1) % kBladeSides], tip, center);
	}

	void AppendTreeInstance(Object3d::ModelData& dst, const Object3d::ModelData& src,
		float tx, float ty, float tz, float scaleXZ, float scaleY, float yaw)
	{
		const size_t srcVertCount = src.vertices.size();
		if (srcVertCount == 0) return;

		const size_t oldVertCount = dst.vertices.size();
		const uint32_t vBase = static_cast<uint32_t>(oldVertCount);
		const float cy = std::cos(yaw), sy = std::sin(yaw);

		dst.vertices.resize(oldVertCount + srcVertCount);
		const Sprite::VertexData* inVerts = src.vertices.data();
		Sprite::VertexData* outVerts = dst.vertices.data() + oldVertCount;
		for (size_t i = 0; i < srcVertCount; ++i)
		{
			const Sprite::VertexData& sv = inVerts[i];
			Sprite::VertexData& dv = outVerts[i];
			const float lx = sv.position.x * scaleXZ;
			const float ly = sv.position.y * scaleY;
			const float lz = sv.position.z * scaleXZ;
			dv.position = { tx + lx * cy - lz * sy, ty + ly, tz + lx * sy + lz * cy, 1.0f };
			dv.texcoord = sv.texcoord;
			dv.normal = { sv.normal.x * cy - sv.normal.z * sy, sv.normal.y, sv.normal.x * sy + sv.normal.z * cy };
		}

		const size_t srcIdxCount = src.indices.size();
		const size_t oldIdxCount = dst.indices.size();
		if (srcIdxCount > 0)
		{
			dst.indices.resize(oldIdxCount + srcIdxCount);
			const uint32_t* inIdx = src.indices.data();
			uint32_t* outIdx = dst.indices.data() + oldIdxCount;
			for (size_t i = 0; i < srcIdxCount; ++i) outIdx[i] = vBase + inIdx[i];
		}
		else
		{
			dst.indices.resize(oldIdxCount + srcVertCount);
			uint32_t* outIdx = dst.indices.data() + oldIdxCount;
			for (uint32_t idx = 0; idx < static_cast<uint32_t>(srcVertCount); ++idx) outIdx[idx] = vBase + idx;
		}
	}

	float DistToPath(std::span<const Vector2> path, float x, float z)
	{
		float bestSq = Scenery::kDistInitMax;
		for (size_t i = 0; i + 1 < path.size(); ++i)
		{
			const float ax = path[i].x, az = path[i].y;
			const float dx = path[i + 1].x - ax, dz = path[i + 1].y - az;
			const float t = std::clamp(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz), 0.0f, 1.0f);
			const float px = ax + dx * t - x, pz = az + dz * t - z;
			bestSq = (std::min)(bestSq, px * px + pz * pz);
		}
		return std::sqrt(bestSq);
	}

	void FootprintPush(const Scenery::Footprint& f, float x, float z, float& outDist, float& nx, float& nz)
	{
		const float dx = x - f.x, dz = z - f.z;
		const float cx = std::clamp(dx, -f.hw, f.hw), cz = std::clamp(dz, -f.hd, f.hd);
		const float ex = dx - cx, ez = dz - cz;
		const float d = std::sqrt(ex * ex + ez * ez);
		if (d > Scenery::kFootprintEps)
		{
			outDist = d;
			nx = ex / d;
			nz = ez / d;
			return;
		}
		const float px = f.hw - std::abs(dx), pz = f.hd - std::abs(dz);
		if (px < pz) { nx = dx >= 0.0f ? 1.0f : -1.0f; nz = 0.0f; outDist = -px; }
		else { nx = 0.0f; nz = dz >= 0.0f ? 1.0f : -1.0f; outDist = -pz; }
	}

	bool IsFootprintBlocked(float x, float z, float radius)
	{
		for (const auto& f : Scenery::kPropFootprints)
		{
			float d = 0.0f, nx = 0.0f, nz = 0.0f;
			FootprintPush(f, x, z, d, nx, nz);
			if (d < f.r + radius) return true;
		}
		return false;
	}

	bool ResolveSceneryPlacement(float& x, float& z, float radius)
	{
		if (z >= Scenery::kNearSideSplitZ)
		{
			return z >= Terrain::FarShoreZ(x) + Scenery::kShoreMargin && !IsFootprintBlocked(x, z, radius);
		}
		for (int iter = 0; iter < Scenery::kMaxPushIters; ++iter)
		{
			bool moved = false;
			for (const auto& f : Scenery::kPropFootprints)
			{
				float d = 0.0f, nx = 0.0f, nz = 0.0f;
				FootprintPush(f, x, z, d, nx, nz);
				const float need = f.r + radius;
				if (d < need)
				{
					x += nx * (need - d + Scenery::kPushExtraMargin);
					z += nz * (need - d + Scenery::kPushExtraMargin);
					moved = true;
				}
			}
			if (!moved) break;
		}
		if (IsFootprintBlocked(x, z, radius * Scenery::kFinalBlockCheckScale)) return false;
		return z <= Terrain::NearShoreZ(x) - Scenery::kShoreMargin;
	}

	bool IsInsideEnemyCamp(float x, float z)
	{
		return x > Scenery::kCampMinX && x < Scenery::kCampMaxX && z > Scenery::kCampMinZ && z < Scenery::kCampMaxZ;
	}

	float ComputeSceneryVertexSway(int batchId, const Sprite::VertexData& v)
	{
		using namespace Scenery;
		const float gy = (v.position.z < kSwaySideSplitZ) ? Terrain::kLandY : Terrain::kFarLandY;
		const float relH = (std::max)(0.0f, v.position.y - gy);
		const bool isTreeTrunkUV = (v.texcoord.x < kTreeTrunkMaxU);

		if (batchId == kTrunk || ((batchId == kLeaf || batchId == kPine) && (isTreeTrunkUV || relH < kTreeTrunkRigidHeight)))
		{
			return 0.0f;
		}
		switch (batchId)
		{
		case kReed:      return std::clamp(relH / kReedFullHeight, 0.0f, 1.0f) * kReedMaxSway;
		case kGrass:     return std::clamp(relH / kGrassFullHeight, 0.0f, 1.0f) * kGrassMaxSway;
		case kBush:      return std::clamp(relH / kBushFullHeight, 0.0f, 1.0f) * kBushMaxSway;
		case kLeaf:
		case kLeafLight: return std::clamp((relH - kTreeTrunkRigidHeight) / kBroadleafSpanHeight, 0.0f, 1.0f) * kBroadleafMaxSway;
		case kPine:      return std::clamp((relH - kTreeTrunkRigidHeight) / kPineSpanHeight, 0.0f, 1.0f) * kPineMaxSway;
		default:         return 0.0f;
		}
	}

	Vector3 EvaluateWind(const Vector3& pos, float time, float sway)
	{
		if (sway <= Wind::kMinSwayEpsilon) return { 0.0f, 0.0f, 0.0f };

		const float spatial = (pos.x * Wind::kSpatialFreqX + pos.z * Wind::kSpatialFreqZ) - time * Wind::kWaveSpeed;
		const float gust = Wind::kGustBase + Wind::kGustAmp * std::sin(time * Wind::kGustTimeFreq + pos.x * Wind::kGustSpatialFreqX);
		const float mainWave = std::sin(spatial) * gust;
		const float subWave = std::sin(spatial * Wind::kSubWaveFreqMul + Wind::kSubWavePhase) * Wind::kSubWaveAmp;
		const float flutter = std::sin(time * Wind::kFlutterTimeFreq + pos.x * Wind::kFlutterFreqX + pos.z * Wind::kFlutterFreqZ) * Wind::kFlutterAmp;
		const float totalDisp = (mainWave + subWave) * Wind::kMainDispScale + flutter;

		return {
			totalDisp * Wind::kWindDirX * sway,
			-std::abs(totalDisp) * Wind::kWindSagY * sway,
			totalDisp * Wind::kWindDirZ * sway
		};
	}

	// パーティクル生成ヘルパー 
	void SpawnParticle(AppParticleManager* pm, const Vector3& pos, const Vector3& vel, const Vector4& col,
		float s0, float s1, float life, uint32_t tex,
		float gravity = 0.0f, float drag = 0.0f, float curl = 0.0f,
		const Vector4* endCol = nullptr, float bounce = 0.0f)
	{
		if (!pm || tex == UINT32_MAX) return;
		AppParticle p;
		p.transform.Initialize();
		p.transform.SetTranslate(pos);
		p.transform.SetRotate({ 0.0f, 0.0f, RandRange(0.0f, kTwoPi) });
		p.transform.SetScale({ s0, s0, 1.0f });
		p.velocity = vel;
		p.color = col;
		p.lifeTime = life;
		p.currentTime = 0.0f;
		p.textureIndex = tex;
		p.gravity = gravity;
		p.drag = drag;
		p.bounceElasticity = bounce;
		p.initialScale = (std::max)(s0, Common::kMinParticleScale);
		p.targetScale = (std::max)(s1, Common::kMinParticleScale);
		if (curl > 0.0f)
		{
			p.curlFreq = Common::kDefaultCurlFreq;
			p.curlAmp = curl;
		}
		if (endCol)
		{
			p.endColor = *endCol;
			p.hasColorShift = true;
		}
		pm->AddParticle(p);
	}

	void EmitParticle(AppParticleManager* pm, const Vector3& pos, const Vector3& vel, uint32_t tex, const ParticleEmitSpec& spec)
	{
		const float alpha = SampleRange(spec.alpha);
		const Vector4 col = { spec.startColor.x, spec.startColor.y, spec.startColor.z, alpha };
		const float s0 = SampleRange(spec.startScale);
		const float s1 = spec.endScaleRelative ? (s0 * SampleRange(spec.endScale)) : SampleRange(spec.endScale);
		const float life = SampleRange(spec.lifeTime);
		SpawnParticle(pm, pos, vel, col, s0, s1, life, tex, spec.gravity, spec.drag, spec.curl, spec.hasEndColor ? &spec.endColor : nullptr);
	}
}

// =====================================================================
// TitleScene 初期化 
// =====================================================================
void TitleScene::InitializeScene()
{
	InitializeAudioAndInput();
	InitializeParticlesAndCamera();

	if (GetObject3dCom() && camera_)
	{
		InitializeHeroAndRiver();
		InitializeTerrain();
		InitializePropsAndEnemies();
		InitializeShadowDecals();
		InitializeSceneryAndPaths();
	}

	InitializeFallingLeaves();
	InitializeSprites();
}

void TitleScene::InitializeAudioAndInput()
{
	if (auto* cm = CollisionManager::GetInstance())
	{
		cm->SetShowDebugColliders(false);
		cm->SetShowMeshWireframe(false);
	}
	if (SceneManager::GetInstance()) SceneManager::GetInstance()->SetShowSkybox(true);

	if (auto* am = GetAudioMgr())
	{
		if (sBgmSoundId < 0) sBgmSoundId = am->Load("Resources/Audio/BGM_Title.mp3");
		if (sBgmSoundId >= 0 && sBgmPlayId < 0)
		{
			sBgmPlayId = am->PlayOneShot(sBgmSoundId, Timing::kBgmVolume);
			sBgmTimer = 0.0f;
		}
		if (sDuckSeSoundId < 0) sDuckSeSoundId = am->Load("Resources/Audio/SE_Duck.mp3");
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
	sDuckRecoilTimer = 0.0f;
	sRiverWaveTimer = 0.0f;
	sRiverWakeTimer = 0.0f;
	sRiverGlintTimer = 0.0f;
	sLeafTimer = 0.0f;
}

void TitleScene::InitializeParticlesAndCamera()
{
	if (GetParticleManager())
	{
		appParticleManager_ = std::make_unique<AppParticleManager>();
		appParticleManager_->Initialize(GetParticleManager());

		auto* texMgr = TextureManager::GetInstance();
		circleTexIndex_ = texMgr->Load("Resources/circle2.png");
		if (circleTexIndex_ == UINT32_MAX) circleTexIndex_ = texMgr->Load("Resources/CG4/circle2.png");
		starTexIndex_ = texMgr->Load("Resources/starburst.png");
		sSmokeTexIndex = texMgr->Load("Resources/smoke_dark.png");

		appParticleManager_->SetGroundY(Terrain::kLandY);
		appParticleManager_->SetPremultiplyAlpha(true);
		sFx.circle = circleTexIndex_;
		sFx.star = starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_;
		sFx.puff = texMgr->Load("Resources/title_smoke_puff.png");
		if (sFx.puff == UINT32_MAX) sFx.puff = circleTexIndex_;
		sFx.leaf = texMgr->Load("Resources/leaf.png");
		if (sFx.leaf == UINT32_MAX) sFx.leaf = circleTexIndex_;
	}

	cameraTranslate_ = CameraChoreo::kFinalPos;
	cameraRotate_ = CameraChoreo::kFinalRot;
	if (camera_)
	{
		camera_->SetFovY(CameraChoreo::kFovY);
		camera_->SetTranslate(CameraChoreo::kStartPos);
		camera_->SetRotate(CameraChoreo::kStartRot);
		camera_->Update();
	}
}

void TitleScene::InitializeHeroAndRiver()
{
	Object3d::ModelData duckData = GetOrLoadModelData("Resources", "player.obj");
	if (duckData.material.textureFilePath.empty()) duckData.material.textureFilePath = "Resources/duck.png";
	duckData.boundingRadius = HeroDuck::kBoundingRadius;

	duckBasePos_ = HeroDuck::kBasePos;
	duckScale_ = HeroDuck::kScale;
	duckModel_ = CreateSceneObject(GetObject3dCom(), camera_, duckData,
		duckBasePos_, { duckScale_, duckScale_, duckScale_ }, { 0.0f, HeroDuck::kFacingYaw, 0.0f });

	// 川面メッシュ生成 
	Object3d::ModelData riverData{};
	riverData.material.textureFilePath = "Resources/water.png";
	riverTexIndex_ = TextureManager::GetInstance()->Load("Resources/water.png");
	riverData.material.textureIndex = riverTexIndex_;
	riverData.boundingRadius = Water::kBoundingRadius;

	const size_t totalWaterVerts = static_cast<size_t>(kWaterGridCols * kWaterGridRows);
	waterBaseVertices_.clear();
	waterBaseVertices_.reserve(totalWaterVerts);
	riverData.vertices.reserve(totalWaterVerts);

	for (int r = 0; r < kWaterGridRows; ++r)
	{
		const float posZ = Water::kStartZ + (static_cast<float>(r) / static_cast<float>(kWaterGridRows - 1)) * Water::kSpanZ;
		for (int c = 0; c < kWaterGridCols; ++c)
		{
			const float posX = Terrain::MapDioramaSpanX(c, kWaterGridCols);
			const WaterVertexBase base{ posX, posZ, posX * Water::kUvScale, posZ * Water::kUvScale };
			waterBaseVertices_.push_back(base);

			Sprite::VertexData vtx{};
			vtx.position = { posX, Terrain::kWaterY, posZ, 1.0f };
			vtx.texcoord = { base.baseU, base.baseV };
			vtx.normal = { 0.0f, 1.0f, 0.0f };
			riverData.vertices.push_back(vtx);
		}
	}
	BuildGridIndices(riverData, kWaterGridCols, kWaterGridRows);

	riverModel_ = CreateSceneObject(GetObject3dCom(), camera_, riverData,
		{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, Water::kWaterColor, true, false);
	riverModel_->SetReflectionFactor(Water::kReflectionFactor);
	riverModel_->SetFresnelF0(Water::kFresnelF0);
}

void TitleScene::InitializeTerrain()
{
	sGrassTexIndex = TextureManager::GetInstance()->Load("Resources/grass.png");

	auto buildShoreModel = [&](int rows, const Vector4& color, auto&& sampleRow) -> std::unique_ptr<Object3d>
	{
		Object3d::ModelData data{};
		data.material.textureFilePath = "Resources/grass.png";
		data.material.textureIndex = sGrassTexIndex;
		data.boundingRadius = Terrain::kGroundBoundingRadius;
		data.vertices.reserve(static_cast<size_t>(Terrain::kGroundCols * rows));

		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < Terrain::kGroundCols; ++c)
			{
				const float posX = Terrain::MapDioramaSpanX(c, Terrain::kGroundCols);
				float posY = 0.0f, posZ = 0.0f;
				Vector3 normal = { 0.0f, 1.0f, 0.0f };
				sampleRow(r, posX, posY, posZ, normal);

				Sprite::VertexData v{};
				v.position = { posX, posY, posZ, 1.0f };
				v.texcoord = { posX * Terrain::kGroundUvScale, posZ * Terrain::kGroundUvScale };
				v.normal = normal;
				data.vertices.push_back(v);
			}
		}
		BuildGridIndices(data, Terrain::kGroundCols, rows);

		auto model = CreateSceneObject(GetObject3dCom(), camera_, data,
			{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, color, true, false);
		model->SetReflectionFactor(0.0f);
		model->SetFresnelF0(0.0f);
		return model;
	};

	// (A) 手前岸 
	sNearGroundModel = buildShoreModel(Terrain::kNearRows, Terrain::kNearGroundColor,
		[](int r, float posX, float& outY, float& outZ, Vector3& outNormal)
		{
			const float edge = Terrain::NearShoreZ(posX);
			if (r < Terrain::kNearFlatRows)
			{
				const float q = 1.0f - static_cast<float>(r) / Terrain::kNearFlatRowDiv;
				outZ = (edge - Terrain::kNearEdgeInset) - (Terrain::kNearLinearZ * q + Terrain::kNearCubicZ * q * q * q);
				outY = Terrain::kLandY;
			}
			else
			{
				const float t = static_cast<float>(r - Terrain::kNearFlatRows) / Terrain::kNearSlopeRowDiv;
				outZ = (edge - Terrain::kNearEdgeInset) + t * Terrain::kNearSlopeSpanZ;
				outY = Terrain::kLandY + (Terrain::kRiverBedY - Terrain::kLandY) * Common::Smoothstep01(t);
				const float dYdt = (Terrain::kRiverBedY - Terrain::kLandY) * Common::SmoothstepDeriv01(t);
				outNormal = SlopeNormalZ(dYdt / Terrain::kNearSlopeSpanZ);
			}
		});

	// (B) 対岸 
	sFarGroundModel = buildShoreModel(Terrain::kFarRows, Terrain::kFarGroundColor,
		[](int r, float posX, float& outY, float& outZ, Vector3& outNormal)
		{
			const float edge = Terrain::FarShoreZ(posX);
			if (r < Terrain::kFarSlopeRows)
			{
				const float t = static_cast<float>(r) / Terrain::kFarSlopeRowDiv;
				outZ = (edge - Terrain::kFarSlopeStartOffset) + t * Terrain::kFarSlopeSpanZ;
				outY = Terrain::kRiverBedY + (Terrain::kFarLandY - Terrain::kRiverBedY) * Common::Smoothstep01(t);
				const float dYdt = (Terrain::kFarLandY - Terrain::kRiverBedY) * Common::SmoothstepDeriv01(t);
				outNormal = SlopeNormalZ(dYdt / Terrain::kFarSlopeSpanZ);
			}
			else
			{
				const float t = static_cast<float>(r - Terrain::kFarSlopeRows) / Terrain::kFarFlatRowDiv;
				outZ = (edge + Terrain::kFarFlatInset) + (Terrain::kFarLinearZ * t + Terrain::kFarCubicZ * t * t * t);
				outY = Terrain::kFarLandY;
			}
		});
}

void TitleScene::InitializePropsAndEnemies()
{
	auto* texMgr = TextureManager::GetInstance();
	Object3dCom* com = GetObject3dCom();

	// 手前岸の物資プロップ 
	Object3d::ModelData fgCrateData = GetOrLoadModelData("Resources", "duckov_crate.obj");
	fgCrateData.material.textureFilePath = "Resources/duckov_crate.png";
	crateTexIndex_ = texMgr->Load("Resources/duckov_crate.png");
	fgCrateData.material.textureIndex = crateTexIndex_;
	fgCrateData.boundingRadius = Props::kCrateBoundingRadius;
	crateModel1_ = CreateSceneObject(com, camera_, fgCrateData, Props::kCrate1Pos, Props::kCrate1Scale, Props::kCrate1Rot);
	crateModel2_ = CreateSceneObject(com, camera_, fgCrateData, Props::kCrate2Pos, Props::kCrate2Scale, Props::kCrate2Rot);

	Object3d::ModelData fgSandData = GetOrLoadModelData("Resources", "duckov_sandbag_wall.obj");
	fgSandData.material.textureFilePath = "Resources/duckov_sandbag.png";
	sandbagTexIndex_ = texMgr->Load("Resources/duckov_sandbag.png");
	fgSandData.material.textureIndex = sandbagTexIndex_;
	fgSandData.boundingRadius = Props::kSandbagBoundingRadius;
	sandbagModel_ = CreateSceneObject(com, camera_, fgSandData, Props::kSandbagPos, Props::kSandbagScale, Props::kSandbagRot);

	// 敵側ステージ陣地 & 中央木造橋のモデル・テクスチャテーブル 
	sSignpostTexIndex = texMgr->Load("Resources/signpost.png");
	sTargetTexIndex = texMgr->Load("Resources/target.png");
	sBridgeTexIndex = texMgr->Load("Resources/duckov_bridge.png");
	barrelTexIndex_ = texMgr->Load("Resources/duckov_barrel.png");
	containerTexIndex_ = texMgr->Load("Resources/container_military.png");
	sWatchtowerTexIndex = texMgr->Load("Resources/duckov_crate.png");

	constexpr size_t kPropKindCount = static_cast<size_t>(Props::EnemyPropKind::Count);
	const std::array<const Object3d::ModelData*, kPropKindCount> propModels = {
		&GetOrLoadModelData("Resources", "bridge.obj"),
		&GetOrLoadModelData("Resources", "duckov_sandbag_wall.obj"),
		&GetOrLoadModelData("Resources", "signpost.obj"),
		&GetOrLoadModelData("Resources", "duckov_barrel_stack.obj"),
		&GetOrLoadModelData("Resources", "container.obj"),
		&GetOrLoadModelData("Resources", "duckov_crate.obj"),
		&GetOrLoadModelData("Resources", "watchtower.obj"),
		&GetOrLoadModelData("Resources", "shooting_target.obj"),
	};
	const std::array<uint32_t, kPropKindCount> propTextures = {
		sBridgeTexIndex, sandbagTexIndex_, sSignpostTexIndex, barrelTexIndex_,
		containerTexIndex_, crateTexIndex_, sWatchtowerTexIndex, sTargetTexIndex,
	};

	sEnemyProps.clear();
	sEnemyProps.reserve(std::size(Props::kEnemyPropSpecs));
	for (const auto& spec : Props::kEnemyPropSpecs)
	{
		const size_t idx = static_cast<size_t>(spec.kind);
		sEnemyProps.push_back({ CreateSceneObject(com, camera_, *propModels[idx], spec.position, spec.scale, spec.rotation), propTextures[idx] });
	}

	// 最奥脱出パッド 
	Object3d::ModelData padData = GetOrLoadModelData("Resources", "extraction_pad.obj");
	sExtractionPadTexIndex = texMgr->Load("Resources/extraction_pad.png");
	padData.material.textureIndex = sExtractionPadTexIndex;
	padData.boundingRadius = Props::kExtractionPadBoundingRadius;
	sExtractionPadModel = CreateSceneObject(com, camera_, padData,
		Props::kExtractionPadPos, Props::kExtractionPadScale, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, true, false);

	// 敵兵アヒル小隊 
	const Object3d::ModelData& enemyDuckData = GetOrLoadModelData("Resources", "player.obj");
	sEnemyDucks.clear();
	sEnemyDucks.reserve(std::size(EnemySquad::kEnemyDuckSpecs));
	for (const auto& spec : EnemySquad::kEnemyDuckSpecs)
	{
		sEnemyDucks.push_back({ CreateSceneObject(com, camera_, enemyDuckData, spec.basePos, EnemySquad::kEnemyDuckScale), spec });
	}
}

void TitleScene::InitializeShadowDecals()
{
	Object3dCom* com = GetObject3dCom();

	Object3d::ModelData shadowData{};
	shadowData.material.textureFilePath = "Resources/circle2.png";
	shadowData.material.textureIndex = circleTexIndex_;
	shadowData.boundingRadius = Shadows::kStaticBoundingRadius;
	for (const auto& disc : Shadows::kStaticShadowDiscs)
	{
		AddShadowDisc(shadowData, disc.cx, disc.cy, disc.cz, disc.rx, disc.rz);
	}
	sShadowDecalsModel = CreateSceneObject(com, camera_, shadowData,
		{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, Shadows::kStaticShadowColor, false, false);

	// 敵兵アヒル個別の追従影モデル 
	sEnemyDuckShadows.clear();
	sEnemyDuckShadows.reserve(sEnemyDucks.size());

	Object3d::ModelData singleShadowData{};
	singleShadowData.material.textureFilePath = "Resources/circle2.png";
	singleShadowData.material.textureIndex = circleTexIndex_;
	singleShadowData.boundingRadius = Shadows::kEnemyShadowBoundingRadius;
	AddShadowDisc(singleShadowData, 0.0f, 0.0f, 0.0f, Shadows::kEnemyShadowRadius, Shadows::kEnemyShadowRadius);

	for (size_t i = 0; i < sEnemyDucks.size(); ++i)
	{
		sEnemyDuckShadows.push_back(CreateSceneObject(com, camera_, singleShadowData,
			{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, Shadows::kEnemyShadowColor, false, false));
	}
}

void TitleScene::InitializeSceneryAndPaths()
{
	using namespace Scenery;
	auto* texMgr = TextureManager::GetInstance();
	Object3dCom* com = GetObject3dCom();

	sWhiteTexIndex = texMgr->Load("Resources/white.png");
	const uint32_t titlePineTexIdx = texMgr->Load("Resources/duckov_pine_tree.png");
	const uint32_t titleBirchTexIdx = texMgr->Load("Resources/duckov_birch_tree.png");
	const uint32_t titleRockTexIdx = texMgr->Load("Resources/duckov_river_bank_rock.png");
	const Object3d::ModelData& realisticPineSrc = GetOrLoadModelData("Resources", "duckov_pine_tree.obj");
	const Object3d::ModelData& realisticTallPineSrc = GetOrLoadModelData("Resources", "duckov_pine_tree_tall.obj");
	const Object3d::ModelData& realisticBirchSrc = GetOrLoadModelData("Resources", "duckov_birch_tree.obj");

	Object3d::ModelData batch[kBatchCount]{};
	const uint32_t batchTex[kBatchCount] = {
		sWhiteTexIndex, titleBirchTexIdx, titlePineTexIdx, titleBirchTexIdx,
		(titleRockTexIdx != TextureManager::kInvalidTextureIndex ? titleRockTexIdx : sWhiteTexIndex),
		sWhiteTexIndex, sWhiteTexIndex, sWhiteTexIndex, titleBirchTexIdx, titlePineTexIdx,
	};
	for (int bi = 0; bi < kBatchCount; ++bi)
	{
		batch[bi].material.textureFilePath = "Resources/white.png";
		batch[bi].material.textureIndex = batchTex[bi];
		batch[bi].boundingRadius = kBatchBoundingRadius;
	}
	// 大量樹木の遠景バッチを事前確保して再アロケーションを抑制
	batch[kFarLeaf].vertices.reserve(280000);
	batch[kFarLeaf].indices.reserve(280000);
	batch[kFarPine].vertices.reserve(680000);
	batch[kFarPine].indices.reserve(680000);

	uint32_t seed = kInitialSeed;

	auto addBroadleaf = [&](float x, float z, float g, float s)
	{
		if (IsInsideEnemyCamp(x, z) || !ResolveSceneryPlacement(x, z, kBroadleafRadius * s)) return;
		const float yaw = HashRange(seed, 0.0f, kTwoPi);
		const int targetBatch = (z < kNearSideSplitZ) ? kLeaf : kFarLeaf;
		AppendTreeInstance(batch[targetBatch], realisticBirchSrc, x, g, z, kBroadleafScaleXZ * s, kBroadleafScaleY * s, yaw);
	};

	auto addPine = [&](float x, float z, float g, float s)
	{
		if (IsInsideEnemyCamp(x, z) || !ResolveSceneryPlacement(x, z, kPineRadius * s)) return;
		const float yaw = HashRange(seed, 0.0f, kTwoPi);
		const bool useTall = (Hash01(seed) < kTallPineProb);
		const int targetBatch = (z < kNearSideSplitZ) ? kPine : kFarPine;
		AppendTreeInstance(batch[targetBatch], useTall ? realisticTallPineSrc : realisticPineSrc, x, g, z, kPineScaleXZ * s, kPineScaleY * s, yaw);
	};

	auto addBush = [&](float x, float z, float g, float s)
	{
		if (!ResolveSceneryPlacement(x, z, kBushRadius * s)) return;
		const float baseYaw = HashRange(seed, 0.0f, kTwoPi);
		const Vector2 uvs[Common::kQuadVertCount] = {
			{ kBushUvMinU, kBushUvMaxV }, { kBushUvMaxU, kBushUvMaxV },
			{ kBushUvMinU, kBushUvMinV }, { kBushUvMaxU, kBushUvMinV },
		};
		for (int fi = 0; fi < kBushFronds; ++fi)
		{
			const float a = baseYaw + static_cast<float>(fi) * (kTwoPi / static_cast<float>(kBushFronds));
			const float ca = std::cos(a), sa = std::sin(a);
			const float rx = -sa * kBushHalfWidth * s, rz = ca * kBushHalfWidth * s;
			const float h = kBushHeight * s, tiltOut = kBushTiltOut * s;
			const Vector3 positions[Common::kQuadVertCount] = {
				{ x - rx, g - kBushSinkY, z - rz },
				{ x + rx, g - kBushSinkY, z + rz },
				{ x - rx * kBushTopWidthMul + ca * tiltOut, g + h, z - rz * kBushTopWidthMul + sa * tiltOut },
				{ x + rx * kBushTopWidthMul + ca * tiltOut, g + h, z + rz * kBushTopWidthMul + sa * tiltOut },
			};
			AppendQuad(batch[kBush], positions, uvs, { ca * kBushNormXZ, kBushNormY, sa * kBushNormXZ }, kBushQuadIndices);
		}
	};

	auto addRock = [&](float x, float z, float g, float s)
	{
		if (g > Terrain::kWaterY && !ResolveSceneryPlacement(x, z, kRockRadius * s)) return;
		MeshBlob(batch[kRock], { x, g + kRockMainOffsetY * s, z }, kRockMainRadii * s, seed++, kRockMainJitter);
		MeshBlob(batch[kRock], { x + kRockSubOffset.x * s, g + kRockSubOffset.y * s, z + kRockSubOffset.z * s }, kRockSubRadii * s, seed++, kRockSubJitter);
	};

	auto placeItems = [&](std::span<const PlacementSpec> items)
	{
		for (const auto& item : items)
		{
			switch (item.kind)
			{
			case ItemKind::Broadleaf: addBroadleaf(item.x, item.z, item.groundY, item.scale); break;
			case ItemKind::Pine:      addPine(item.x, item.z, item.groundY, item.scale);      break;
			case ItemKind::Bush:      addBush(item.x, item.z, item.groundY, item.scale);      break;
			case ItemKind::Rock:      addRock(item.x, item.z, item.groundY, item.scale);      break;
			}
		}
	};

	auto addTuft = [&](float x, float z, float g, float s, int kind)
	{
		const int n = kTuftBaseBlades + static_cast<int>(Hash01(seed) * kTuftExtraBlades);
		for (int i = 0; i < n; ++i)
		{
			const float a = HashRange(seed, 0.0f, kTwoPi);
			const float r = HashSample(seed, kTuftOffsetR) * s;
			const float h = HashSample(seed, kTuftHeight) * s;
			MeshBlade(batch[kind], { x + std::cos(a) * r, g - kTuftSinkY, z + std::sin(a) * r },
				kTuftBladeWidth * s, h, std::cos(a) * h * kTuftLeanRatio, std::sin(a) * h * kTuftLeanRatio);
		}
	};

	auto addReeds = [&](float x, float z, float g)
	{
		const int n = kReedBaseBlades + static_cast<int>(Hash01(seed) * kReedExtraBlades);
		for (int i = 0; i < n; ++i)
		{
			const float bx = x + HashRange(seed, -kReedSpreadX, kReedSpreadX);
			const float bz = z + HashRange(seed, -kReedSpreadZ, kReedSpreadZ);
			const float h = HashSample(seed, kReedHeight);
			MeshBlade(batch[kReed], { bx, g, bz }, kReedBladeWidth, h,
				HashRange(seed, -kReedLeanX, kReedLeanX), HashRange(seed, -kReedLeanZ, kReedLeanZ));
		}
	};

	auto populateForestRow = [&](const ForestBandSpec& band, int row)
	{
		for (float x = band.xMin; x <= band.xMax; x += HashSample(seed, band.stepX))
		{
			const float z = band.zBase + static_cast<float>(row) * band.zRowStep + HashRange(seed, -band.zJitter, band.zJitter);
			const float s = HashSample(seed, band.scale) + static_cast<float>(row) * band.scaleRowStep;
			if (Hash01(seed) < band.broadleafProb) addBroadleaf(x, z, Terrain::kFarLandY, s * band.broadleafScaleMul);
			else addPine(x, z, Terrain::kFarLandY, s);
		}
	};

	// 1. 手前岸・川中の固定配置 & 2. 両岸の葦 
	placeItems(kForegroundPlacements);
	for (float x = kReedSweepMinX; x <= kReedSweepMaxX; x += kReedSweepStepX)
	{
		const float jx = x + HashRange(seed, -kReedJitterX, kReedJitterX);
		if (jx > kReedBridgeGapMinX && jx < kReedBridgeGapMaxX) continue;
		if (Hash01(seed) < kReedNearProb) addReeds(jx, Terrain::NearShoreZ(jx) + kReedShoreOffsetZ, Terrain::kWaterY - kReedWaterSinkY);
		if (Hash01(seed) < kReedFarProb) addReeds(jx, Terrain::FarShoreZ(jx) - kReedShoreOffsetZ, Terrain::kWaterY - kReedWaterSinkY);
	}

	// 3. 手前岸・対岸の草むら散布 
	for (int tries = 0, placed = 0; tries < kGrassMaxTries && placed < kNearGrassTargetCount; ++tries)
	{
		const float x = HashSample(seed, kNearGrassRangeX);
		const float z = HashRange(seed, kNearGrassMinZ, Terrain::NearShoreZ(x) - kNearGrassShoreMarginZ);
		bool ok = DistToPath(kNearPathWaypoints, x, z) > kNearGrassMinPathDist;
		for (const auto& a : kNearGrassAvoidCircles)
		{
			if (Square(x - a.x) + Square(z - a.z) < Square(a.r)) ok = false;
		}
		if (!ok || IsFootprintBlocked(x, z, kGrassBlockRadius)) continue;
		addTuft(x, z, Terrain::kLandY, HashSample(seed, kNearGrassScale), (placed % kNearReedMixModulo == 0) ? kReed : kGrass);
		++placed;
	}

	for (int tries = 0, placed = 0; tries < kGrassMaxTries && placed < kFarGrassTargetCount; ++tries)
	{
		const float x = HashSample(seed, kFarGrassRangeX);
		const float z = HashRange(seed, Terrain::FarShoreZ(x) + kFarGrassShoreMarginZ, kFarGrassMaxZ);
		if (DistToPath(kFarPathWaypoints, x, z) < kFarGrassMinPathDist) continue;
		if (x > kExclContainerMinX && x < kExclContainerMaxX && z > kExclContainerMinZ && z < kExclContainerMaxZ) continue;
		if (x > kExclTowerMinX && x < kExclTowerMaxX && z > kExclTowerMinZ && z < kExclTowerMaxZ) continue;
		if (std::abs(x - kExclPadCenterX) < kExclPadHalfExtent && std::abs(z - kExclPadCenterZ) < kExclPadHalfExtent) continue;
		if (z < kExclCheckpointMaxZ && x > kExclCheckpointMinX && x < kExclCheckpointMaxX) continue;
		if (IsFootprintBlocked(x, z, kGrassBlockRadius)) continue;
		addTuft(x, z, Terrain::kFarLandY, HashSample(seed, kFarGrassScale), kGrass);
		++placed;
	}

	// 4. 対岸の固定配置 & 遠景樹林帯 
	placeItems(kFarShorePlacements);
	for (int row = 0; row < kCenterBackForestBand.rows; ++row) populateForestRow(kCenterBackForestBand, row);
	for (int row = 0; row < kWingForestRows; ++row)
	{
		populateForestRow(kLeftWingForestBand, row);
		populateForestRow(kRightWingForestBand, row);
	}
	for (int row = 0; row < kSkylineForestBand.rows; ++row) populateForestRow(kSkylineForestBand, row);

	// 5. バッチメッシュ生成・風揺れウェイト事前計算 
	sScenery.clear();
	for (int i = 0; i < kBatchCount; ++i)
	{
		if (batch[i].vertices.empty()) continue;
		SceneryBatch sb;
		sb.textureIndex = batchTex[i];
		sb.model = CreateSceneObject(com, camera_, batch[i],
			{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, kBatchColors[i], true, false);
		if (i != kRock)
		{
			sb.model->SetReflectionFactor(0.0f);
			sb.model->SetFresnelF0(0.0f);
		}
		if (i == kFarLeaf || i == kFarPine)
		{
			// 遠景樹林帯（約87万頂点）は毎フレームのCPU頂点書き換えを行わず、バッチ全体の微風トランスフォーム揺れで60FPSを維持
			sb.isFarForest = true;
			sb.hasSway = false;
		}
		else if (i != kRock && i != kTrunk)
		{
			sb.hasSway = true;
			const size_t vCount = batch[i].vertices.size();
			sb.baseVertices.resize(vCount);
			const Sprite::VertexData* srcVerts = batch[i].vertices.data();
			SceneryVertexBase* dstBase = sb.baseVertices.data();
			for (size_t vi = 0; vi < vCount; ++vi)
			{
				const Sprite::VertexData& v = srcVerts[vi];
				dstBase[vi] = { { v.position.x, v.position.y, v.position.z }, ComputeSceneryVertexSway(i, v) };
			}
		}
		sScenery.push_back(std::move(sb));
	}

	// 6. 土の小道デカール 
	Object3d::ModelData pathData{};
	pathData.material.textureFilePath = "Resources/circle2.png";
	pathData.material.textureIndex = circleTexIndex_;
	pathData.boundingRadius = kPathBoundingRadius;

	auto layPath = [&](std::span<const Vector2> path, float g)
	{
		for (size_t i = 0; i + 1 < path.size(); ++i)
		{
			const float dx = path[i + 1].x - path[i].x, dz = path[i + 1].y - path[i].y;
			const int n = (std::max)(1, static_cast<int>(std::sqrt(dx * dx + dz * dz) / kPathStepDist));
			for (int k = 0; k < n; ++k)
			{
				const float t = static_cast<float>(k) / static_cast<float>(n);
				const float r = HashSample(seed, kPathRadius);
				AddShadowDisc(pathData,
					path[i].x + dx * t + HashRange(seed, -kPathJitter, kPathJitter),
					g + kPathHeightOffset,
					path[i].y + dz * t + HashRange(seed, -kPathJitter, kPathJitter), r, r);
			}
		}
	};
	layPath(kNearPathWaypoints, Terrain::kLandY);
	layPath(kFarPathWaypoints, Terrain::kFarLandY);

	sPathDecalModel = CreateSceneObject(com, camera_, pathData,
		{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, kPathColor, false, false);
}

void TitleScene::InitializeFallingLeaves()
{
	Object3d::ModelData leafData{};
	leafData.material.textureFilePath = "Resources/leaf.png";
	leafData.material.textureIndex = sFx.leaf;
	leafData.boundingRadius = Leaves::kBoundingRadius;

	leafData.vertices.reserve(std::size(Leaves::kDoubleSidedQuadVertices));
	for (const auto& sv : Leaves::kDoubleSidedQuadVertices)
	{
		Sprite::VertexData v{};
		v.position = sv.position;
		v.texcoord = sv.texcoord;
		v.normal = sv.normal;
		leafData.vertices.push_back(v);
	}
	leafData.indices.assign(std::begin(Leaves::kDoubleSidedQuadIndices), std::end(Leaves::kDoubleSidedQuadIndices));

	sFallingLeaves.clear();
	sFallingLeaves.resize(Leaves::kMaxFallingLeaves);
	for (auto& leaf : sFallingLeaves)
	{
		leaf.model = CreateSceneObject(GetObject3dCom(), camera_, leafData,
			{ 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f }, true, false);
		leaf.active = false;
	}
}

void TitleScene::InitializeSprites()
{
	SpriteCom* sc = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetSpriteCom() : nullptr;
	if (!sc) return;

	spriteManager_ = std::make_unique<SpriteManager>();
	spriteManager_->Initialize(sc, "Resources/uvChecker.png", 0);
	sprites_.clear();

	WindowAPI* win = dxCommon_ ? dxCommon_->GetWindowAPI() : nullptr;
	sScreenW = win ? static_cast<float>(win->GetClientWidth()) : UI::kDefaultScreenW;
	sScreenH = win ? static_cast<float>(win->GetClientHeight()) : UI::kDefaultScreenH;
	sLogoFinalPos = UI::kLogoFinalPos;
	sLogoSize = UI::kLogoSize;
	sPromptBasePos = { UI::kPromptBaseX, sScreenH * UI::kPromptPosYRatio };

	vignetteSprite_ = CreateSpriteInstance(sc, sprites_, "Resources/title_vignette.png",
		UI::kTopLeftAnchor, { 0.0f, 0.0f }, { sScreenW, sScreenH }, UI::kVignetteColor);

	const Vector2 logoOffscreenPos = { -sLogoSize.x * UI::kLogoInitOffscreenMul, sLogoFinalPos.y };
	for (auto& ghost : sLogoGhosts)
	{
		ghost = CreateSpriteInstance(sc, sprites_, "Resources/title_logo.png",
			UI::kTopLeftAnchor, logoOffscreenPos, sLogoSize, UI::kLogoGhostRgb);
	}
	titleLogoSprite_ = CreateSpriteInstance(sc, sprites_, "Resources/title_logo.png",
		UI::kTopLeftAnchor, logoOffscreenPos, sLogoSize, UI::kTransparentWhite);

	sLogoShine = CreateSpriteInstance(sc, sprites_, "Resources/title_logo_shine.png",
		UI::kTopLeftAnchor, { 0.0f, 0.0f }, sLogoSize, UI::kTransparentWhite);
	if (sLogoShine) SetShineCell(sLogoShine, UI::kShineFlashCell, UI::kLogoTexW, UI::kLogoTexH);

	pressSpaceSprite_ = CreateSpriteInstance(sc, sprites_, "Resources/title_press_space.png",
		UI::kCenterAnchor, sPromptBasePos, UI::kPromptSize, UI::kTransparentWhite);
	sPromptShine = CreateSpriteInstance(sc, sprites_, "Resources/title_press_space_shine.png",
		UI::kCenterAnchor, sPromptBasePos, UI::kPromptSize, UI::kTransparentWhite);
	if (sPromptShine) SetShineCell(sPromptShine, UI::kShineFlashCell, UI::kPromptTexW, UI::kPromptTexH);

	fadeOverlaySprite_ = CreateSpriteInstance(sc, sprites_, "Resources/CG4/human/white.png",
		UI::kTopLeftAnchor, { 0.0f, 0.0f }, { sScreenW, sScreenH }, UI::kOpaqueBlack);
}

// =====================================================================
// TitleScene 終了処理 
// =====================================================================
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
	if (auto* am = GetAudioMgr())
	{
		if (sBgmPlayId >= 0) { am->Stop(sBgmPlayId); sBgmPlayId = -1; }
		if (sDuckSePlayId >= 0) { am->Stop(sDuckSePlayId); sDuckSePlayId = -1; }
	}
	for (auto& ghost : sLogoGhosts) ghost = nullptr;
	sLogoShine = nullptr;
	sPromptShine = nullptr;
}

// =====================================================================
// TitleScene 毎フレーム更新 
// =====================================================================
void TitleScene::Update()
{
	if (input_) input_->Update();

	const auto now = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration<float>(now - lastTime_).count();
	lastTime_ = now;
	deltaTime = std::clamp(deltaTime, Timing::kMinDeltaTime, Timing::kMaxDeltaTime);
	bgTimer_ += deltaTime;

	const bool isTransitioningNow = (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());
	if (!isTransitioningNow && !sCameraLocked) sIntroTimer += deltaTime;

	if (auto* am = GetAudioMgr(); am && sBgmSoundId >= 0)
	{
		sBgmTimer += deltaTime;
		if (sBgmTimer >= Timing::kBgmLoopDuration)
		{
			sBgmTimer = 0.0f;
			sBgmPlayId = am->PlayOneShot(sBgmSoundId, Timing::kBgmVolume);
		}
	}

	mouseInput_.Update();
	UpdateStartAndTransition(deltaTime);

	const float idleBlend = std::clamp((sIntroTimer - Timing::kIntroDuration) / CameraChoreo::kIdleBlendFadeDuration, 0.0f, 1.0f);
	float normX = 0.0f;
	Vector3 mouseWaterHit{ 0.0f, Terrain::kWaterY, 0.0f };
	bool isHoveringRiver = false;
	UpdateCameraAndWaterRay(idleBlend, normX, mouseWaterHit, isHoveringRiver);

	for (auto& s : mouseWaveSources_) s.age += deltaTime;
	mouseWaveSources_.erase(
		std::remove_if(mouseWaveSources_.begin(), mouseWaveSources_.end(),
			[](const MouseWaveRippleSource& s) { return s.age >= s.maxLife; }),
		mouseWaveSources_.end());
	mouseRippleCooldown_ -= deltaTime;

	UpdateMouseRiverSpray(deltaTime, mouseWaterHit, isHoveringRiver);
	UpdateLightingAndHero(deltaTime, idleBlend, normX, isHoveringRiver);
	UpdateEnemySquad();
	UpdateDynamicMeshes();
	UpdateEnvironmentParticles(deltaTime);
	UpdateSpritesUI();
}

void TitleScene::UpdateStartAndTransition(float deltaTime)
{
	// デフォルトで自動デモON（[F7] でデモON/OFF切替、SPACE/ENTER/左クリック または デモON時は5.5秒経過で自動出撃）
	if (!isStarting_ && (!StingerTransition::GetInstance() || !StingerTransition::GetInstance()->IsTransitioning()))
	{
		if (input_ && input_->TriggerKey(DIK_F7))
		{
			RaidStats::GetInstance().autoDemoMode = !RaidStats::GetInstance().autoDemoMode;
			if (RaidStats::GetInstance().autoDemoMode)
			{
				RaidStats::GetInstance().autoDemoCycleIndex = 0;
			}
			TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", Timing::kStartUiClickVolume);
		}

		const bool manualStart = (input_ && (input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN)))
			|| mouseInput_.TriggerButton(Timing::kLeftMouseButton);
		const bool autoAttractStart = RaidStats::GetInstance().autoDemoMode && (sIntroTimer >= Timing::kIntroDuration + 2.5f);
		const bool pressed = manualStart || autoAttractStart;
		if (pressed && !sLogoLanded)
		{
			sIntroTimer = Timing::kIntroDuration;
			sLogoLanded = true;
		}

		if (pressed)
		{
			isStarting_ = true;
			startTransitionTimer_ = 0.0f;
			sDuckRecoilTimer = HeroDuck::kStartRecoilDuration;

			if (auto* am = GetAudioMgr(); am && sDuckSeSoundId >= 0) am->PlayOneShot(sDuckSeSoundId, Timing::kStartDuckSeVolume);
			TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", Timing::kStartUiClickVolume);
			TacticalAudio::Play2D("Resources/Audio/SE_Gun_Rifle_1.wav", Timing::kStartRifleShotVolume);

			if (appParticleManager_ && GetParticleManager())
			{
				auto& rng = GetParticleManager()->GetRandomEngine();
				const Vector3 muzzle = duckBasePos_ + HeroDuck::kMuzzleOffset;
				appParticleManager_->EmitMuzzleFlash(
					rng, muzzle, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f },
					HeroDuck::kMuzzleFlashColor, HeroDuck::kMuzzleFlashScale, circleTexIndex_);
				appParticleManager_->EmitMuzzleFlare(
					rng, muzzle, HeroDuck::kMuzzleFlareScale, HeroDuck::kMuzzleFlareColor, HeroDuck::kMuzzleFlareLife,
					starTexIndex_ != UINT32_MAX ? starTexIndex_ : circleTexIndex_);
			}

			// 出撃時のカメラ位置・姿勢を完全ロック 
			sCameraLocked = true;
			if (camera_)
			{
				sCameraLockedPos = camera_->GetTranslate();
				sCameraLockedRot = camera_->GetRotate();
			}
		}
		else if (input_ && input_->TriggerKey(DIK_ESCAPE))
		{
			PostQuitMessage(0);
		}
	}

	if (isStarting_)
	{
		startTransitionTimer_ += deltaTime;
		if (startTransitionTimer_ >= Timing::kStartTransitionDelay
			&& (!StingerTransition::GetInstance() || !StingerTransition::GetInstance()->IsTransitioning()))
		{
			SceneTransition::Change("GAMEPLAY", Timing::kGameplayTransitionDuration, TransitionTheme::TitleTactical);
		}
	}

	// ロゴ着弾シェイク 
	if (!sLogoLanded && sIntroTimer >= Timing::kLogoSlamTime + Timing::kLogoSlideTime)
	{
		sLogoLanded = true;
		TacticalAudio::Play2D("Resources/Audio/SE_Logo_Slam.wav", Timing::kLogoSlamSeVolume);
		if (camera_) camera_->Shake(CameraChoreo::kLogoSlamShakeDuration, CameraChoreo::kLogoSlamShakePower);
	}
}

void TitleScene::UpdateCameraAndWaterRay(float idleBlend, float& outNormX, Vector3& outWaterHit, bool& outHoveringRiver)
{
	const float introT = EaseInOutCubic(sIntroTimer / Timing::kIntroDuration);
	Vector3 camPos = Lerp3(CameraChoreo::kStartPos, CameraChoreo::kFinalPos, introT);
	Vector3 camRot = Lerp3(CameraChoreo::kStartRot, CameraChoreo::kFinalRot, introT);

	camPos.x += std::sin(bgTimer_ * CameraChoreo::kIdleSwayFreqX) * CameraChoreo::kIdleSwayAmpX * idleBlend;
	camPos.y += std::sin(bgTimer_ * CameraChoreo::kIdleSwayFreqY) * CameraChoreo::kIdleSwayAmpY * idleBlend;

	if (!dxCommon_ || !camera_) return;
	WindowAPI* win = dxCommon_->GetWindowAPI();
	if (!win || win->GetClientWidth() <= 0 || win->GetClientHeight() <= 0) return;

	const Vector2 mp = mouseInput_.GetScaledPosition();
	const float cw = static_cast<float>(win->GetClientWidth());
	const float ch = static_cast<float>(win->GetClientHeight());
	outNormX = std::clamp((mp.x / cw) * 2.0f - 1.0f, -1.0f, 1.0f);
	const float normY = std::clamp((mp.y / ch) * 2.0f - 1.0f, -1.0f, 1.0f);

	if (!sCameraLocked)
	{
		camPos.x += outNormX * CameraChoreo::kParallaxPosX * idleBlend;
		camPos.y -= normY * CameraChoreo::kParallaxPosY * idleBlend;
		camRot.y += outNormX * CameraChoreo::kParallaxRotY * idleBlend;
		camRot.x += normY * CameraChoreo::kParallaxRotX * idleBlend;
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
	const float ny = 1.0f - (mp.y / ch) * 2.0f;
	const Matrix4x4 inv = Inverse(Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix()));
	const Vector3 rayNear = UnprojectClipPoint({ outNormX, ny, 0.0f, 1.0f }, inv);
	const Vector3 rayFar = UnprojectClipPoint({ outNormX, ny, 1.0f, 1.0f }, inv);
	const Vector3 rayDir = rayFar - rayNear;

	if (rayDir.y < Water::kRayDownMinY)
	{
		const float t = (Terrain::kWaterY - rayNear.y) / rayDir.y;
		outWaterHit = { rayNear.x + rayDir.x * t, Terrain::kWaterY, rayNear.z + rayDir.z * t };
		if (outWaterHit.x >= Water::kHoverMinX && outWaterHit.x <= Water::kHoverMaxX)
		{
			outHoveringRiver = (outWaterHit.z >= Terrain::NearShoreZ(outWaterHit.x) && outWaterHit.z <= Terrain::FarShoreZ(outWaterHit.x));
		}
	}
}

void TitleScene::UpdateMouseRiverSpray(float deltaTime, const Vector3& mouseWaterHit, bool isHoveringRiver)
{
	if (!isHoveringRiver || !appParticleManager_)
	{
		hasPrevMousePos_ = false;
		return;
	}

	using namespace RiverSpray;
	AppParticleManager* pm = appParticleManager_.get();
	const float actualWaterY = SampleRiverSurfaceY(mouseWaterHit.x, mouseWaterHit.z);

	const Vector3 mouseDelta = hasPrevMousePos_ ? (mouseWaterHit - prevMouseWorldPos_) : Vector3{ 0.0f, 0.0f, 0.0f };
	const float moveDist = std::sqrt(mouseDelta.x * mouseDelta.x + mouseDelta.z * mouseDelta.z);
	const bool isClickTrigger = mouseInput_.TriggerButton(Timing::kLeftMouseButton);
	const bool isClickHold = mouseInput_.PushButton(Timing::kLeftMouseButton);
	const float speed = (deltaTime > kMinDeltaTimeForSpeed) ? (moveDist / deltaTime) : 0.0f;
	const float speedFactor = std::clamp(speed / kSpeedNormDiv, kSpeedFactorMin, kSpeedFactorMax);

	auto pushRipple = [&](float maxLife, float power)
	{
		mouseRippleCooldown_ = Water::kRippleEmitCooldown;
		mouseWaveSources_.push_back({ mouseWaterHit, 0.0f, maxLife, power });
	};

	mouseWaveEmitCooldown_ -= deltaTime;

	if (isClickTrigger)
	{
		// A. クリック時の大波しぶき 
		for (int d = 0; d < kClickDropCount; ++d)
		{
			const float angle = RandRange(0.0f, kTwoPi);
			const float radialSpd = SampleRange(kClickRadialSpd);
			const Vector3 splashVel = { std::cos(angle) * radialSpd, SampleRange(kClickUpSpd), std::sin(angle) * radialSpd };
			EmitParticle(pm, JitterPosXZ(mouseWaterHit.x, actualWaterY + kClickDropOffsetY, mouseWaterHit.z, kClickPosJitter), splashVel, sFx.circle, kClickDropSpec);
		}
		for (int m = 0; m < kClickMistCount; ++m)
		{
			const float mAngle = RandRange(0.0f, kTwoPi);
			const float mRadial = SampleRange(kClickMistRadial);
			const Vector3 mistVel = { std::cos(mAngle) * mRadial, SampleRange(kClickMistUp), std::sin(mAngle) * mRadial };
			EmitParticle(pm, { mouseWaterHit.x, actualWaterY + kClickMistOffsetY, mouseWaterHit.z }, mistVel, sFx.circle, kClickMistSpec);
		}
		for (int f = 0; f < kClickFoamCount; ++f)
		{
			const Vector3 foamVel = { RandRange(-kClickFoamVelJitter, kClickFoamVelJitter) + kRiverDriftSpeedX, 0.0f, RandRange(-kClickFoamVelJitter, kClickFoamVelJitter) };
			EmitParticle(pm, { mouseWaterHit.x, actualWaterY + kClickFoamOffsetY, mouseWaterHit.z }, foamVel, sFx.circle, kClickFoamSpec);
		}
		if (mouseWaveSources_.size() < Water::kMaxRippleSources) pushRipple(kClickRippleLife, kClickRipplePower);
		mouseWaveEmitCooldown_ = kClickEmitCooldown;
	}
	else if (moveDist > kDragMinMoveDist)
	{
		// B. なぞり・ドラッグ時の扇状波しぶき 
		const float interval = std::clamp(kDragBaseInterval / speedFactor, kDragMinInterval, kDragMaxInterval);
		if (mouseWaveEmitCooldown_ <= 0.0f)
		{
			mouseWaveEmitCooldown_ = interval;
			const Vector3 moveDir = { mouseDelta.x / moveDist, 0.0f, mouseDelta.z / moveDist };
			const Vector3 movePerp = { -moveDir.z, 0.0f, moveDir.x };

			int dropCount = static_cast<int>(static_cast<float>(kDragDropBase) + speedFactor * kDragDropSpeedMul);
			if (isClickHold) dropCount += kDragDropHoldBonus;
			dropCount = std::clamp(dropCount, kDragDropBase, kDragDropMax);

			for (int d = 0; d < dropCount; ++d)
			{
				const float sideSign = (Rand01() < 0.5f) ? 1.0f : -1.0f;
				const float sideSpread = sideSign * SampleRange(kDragSideSpread) * speedFactor;
				const float forwardSpread = SampleRange(kDragForwardSpread) * speedFactor;
				const float upSpeed = SampleRange(kDragUpSpd) * (kDragUpBaseMul + speedFactor * kDragUpSpeedMul);
				const Vector3 splashVel = moveDir * forwardSpread + movePerp * sideSpread
					+ JitterPosXZ(0.0f, upSpeed, 0.0f, kDragVelJitterXZ);
				EmitParticle(pm, JitterPosXZ(mouseWaterHit.x, actualWaterY + kClickDropOffsetY, mouseWaterHit.z, kClickPosJitter), splashVel, sFx.circle, kDragDropSpec);
			}

			const int mistCount = (speedFactor > kDragMistFastThreshold) ? kDragMistCountFast : kDragMistCountSlow;
			for (int m = 0; m < mistCount; ++m)
			{
				const Vector3 mistVel = {
					mouseDelta.x * kDragMistDeltaVelScale + RandRange(-kDragMistVelJitter, kDragMistVelJitter),
					SampleRange(kDragMistUp) * speedFactor * 0.5f,
					mouseDelta.z * kDragMistDeltaVelScale + RandRange(-kDragMistVelJitter, kDragMistVelJitter)
				};
				EmitParticle(pm, JitterPosXZ(mouseWaterHit.x, actualWaterY + kDragMistOffsetY, mouseWaterHit.z, kDragMistPosJitter), mistVel, sFx.circle, kDragMistSpec);
			}

			const Vector3 foamVel = { mouseDelta.x * kDragFoamDeltaVelScale + kRiverDriftSpeedX * 0.5f, 0.0f, mouseDelta.z * kDragFoamDeltaVelScale };
			EmitParticle(pm, { mouseWaterHit.x, actualWaterY + kClickFoamOffsetY, mouseWaterHit.z }, foamVel, sFx.circle, kDragFoamSpec);

			if (mouseRippleCooldown_ <= 0.0f && mouseWaveSources_.size() < Water::kMaxRippleSources)
			{
				pushRipple(kDragRippleLife, kDragRipplePowerScale * speedFactor);
			}
		}
	}
	else if (mouseWaveEmitCooldown_ <= 0.0f)
	{
		// C. 静止ホバー時の水流飛沫 
		mouseWaveEmitCooldown_ = kHoverEmitInterval;
		for (int d = 0; d < kHoverDropCount; ++d)
		{
			const Vector3 splashVel = { SampleRange(kHoverVelX), SampleRange(kHoverVelY), RandRange(-kHoverVelJitterZ, kHoverVelJitterZ) };
			EmitParticle(pm, JitterPosXZ(mouseWaterHit.x, actualWaterY + kHoverOffsetY, mouseWaterHit.z, kHoverPosJitter), splashVel, sFx.circle, kHoverDropSpec);
		}
		const Vector3 foamVel = { SampleRange(kHoverFoamVelX), 0.0f, RandRange(-kHoverFoamVelJitterZ, kHoverFoamVelJitterZ) };
		EmitParticle(pm, { mouseWaterHit.x, actualWaterY + kClickFoamOffsetY, mouseWaterHit.z }, foamVel, sFx.circle, kHoverFoamSpec);
	}

	prevMouseWorldPos_ = mouseWaterHit;
	hasPrevMousePos_ = true;
}

void TitleScene::UpdateLightingAndHero(float deltaTime, float idleBlend, float normX, bool isHoveringRiver)
{
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		const Vector3 sunDir = Lighting::kSunRawDirection * (1.0f / Length3(Lighting::kSunRawDirection));
		light->SetDirectionalLight(Lighting::kSunColor, sunDir, Lighting::kSunIntensity);

		const float flash = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / Lighting::kMuzzleFlashNormDuration) * Lighting::kMuzzleFlashIntensityBoost : 0.0f;
		light->SetPointLight(Lighting::kHeroLightColor, duckBasePos_ + Lighting::kHeroLightOffset,
			Lighting::kHeroLightBaseIntensity + flash, Lighting::kHeroLightRadius, Lighting::kHeroLightDecay);
	}

	if (sDuckRecoilTimer > 0.0f) sDuckRecoilTimer -= deltaTime;

	// 主役アヒルクリック時のホップ演出 
	if (!isStarting_ && camera_)
	{
		const Matrix4x4 vp = Multiply(camera_->GetViewMatrix(), camera_->GetProjectionMatrix());
		const Vector4 proj = MultiplyRowVec4({ duckBasePos_.x, duckBasePos_.y + HeroDuck::kHitCenterOffsetY, duckBasePos_.z, 1.0f }, vp);
		if (proj.w > HeroDuck::kMinClipW)
		{
			const float screenX = ((proj.x / proj.w) * 0.5f + 0.5f) * sScreenW;
			const float screenY = (1.0f - ((proj.y / proj.w) * 0.5f + 0.5f)) * sScreenH;
			const Vector2 mp = mouseInput_.GetScaledPosition();
			const float d2 = Square(mp.x - screenX) + Square(mp.y - screenY);
			if (d2 <= Square(HeroDuck::kClickHitRadiusPx) && mouseInput_.TriggerButton(Timing::kLeftMouseButton) && !isHoveringRiver)
			{
				sDuckClickHopTimer = HeroDuck::kHopDuration;
				if (auto* am = GetAudioMgr(); am && sDuckSeSoundId >= 0) am->PlayOneShot(sDuckSeSoundId, Timing::kClickHopDuckSeVolume);
				if (appParticleManager_)
				{
					for (int d = 0; d < HeroDuck::kHopDustCount; ++d)
					{
						const float a = Rand01() * kTwoPi;
						const Vector3 p = { duckBasePos_.x + std::cos(a) * HeroDuck::kHopDustRadius, Terrain::kLandY + HeroDuck::kHopDustOffsetY, duckBasePos_.z + std::sin(a) * HeroDuck::kHopDustRadius };
						const Vector3 v = { std::cos(a) * SampleRange(HeroDuck::kHopDustHorizSpd), SampleRange(HeroDuck::kHopDustUpSpd), std::sin(a) * SampleRange(HeroDuck::kHopDustHorizSpd) };
						EmitParticle(appParticleManager_.get(), p, v, sFx.circle, HeroDuck::kHopDustSpec);
					}
				}
			}
		}
	}

	float hopY = 0.0f, hopPitch = 0.0f;
	if (sDuckClickHopTimer > 0.0f)
	{
		sDuckClickHopTimer -= deltaTime;
		const float hopNorm = std::clamp(1.0f - sDuckClickHopTimer / HeroDuck::kHopDuration, 0.0f, 1.0f);
		hopY = std::sin(hopNorm * kPi) * HeroDuck::kHopHeight;
		hopPitch = std::sin(hopNorm * kTwoPi) * HeroDuck::kHopPitchAmp;
	}

	if (isStarting_ && startTransitionTimer_ < HeroDuck::kBattleCryDuration)
	{
		const float cryPhase = startTransitionTimer_ * HeroDuck::kBattleCryFreq;
		const float cryAmp = std::clamp(1.0f - startTransitionTimer_ / HeroDuck::kBattleCryDuration, 0.0f, 1.0f);
		hopPitch += std::sin(cryPhase) * HeroDuck::kBattleCryPitchAmp * cryAmp;
		hopY += std::abs(std::sin(cryPhase * 0.5f)) * HeroDuck::kBattleCryHopAmp * cryAmp;
	}

	if (duckModel_)
	{
		const float breathe = std::sin(bgTimer_ * HeroDuck::kBreatheFreq) * HeroDuck::kBreatheAmp;
		const float recoil = (sDuckRecoilTimer > 0.0f) ? (sDuckRecoilTimer / HeroDuck::kRecoilNormDiv) : 0.0f;
		const float yaw = HeroDuck::kFacingYaw + std::sin(bgTimer_ * HeroDuck::kIdleYawFreq) * HeroDuck::kIdleYawAmp + normX * HeroDuck::kAimFollowYaw * idleBlend;
		duckModel_->SetTranslate({ duckBasePos_.x, duckBasePos_.y + breathe + hopY, duckBasePos_.z - recoil * HeroDuck::kRecoilBackZ });
		duckModel_->SetRotate({ HeroDuck::kRecoilPitch * recoil + hopPitch, yaw, 0.0f });
		duckModel_->Update();
	}
}

void TitleScene::UpdateEnemySquad()
{
	using namespace EnemySquad;
	for (size_t i = 0; i < sEnemyDucks.size(); ++i)
	{
		auto& ed = sEnemyDucks[i];
		if (!ed.model) continue;

		const auto& sp = ed.spec;
		Vector3 curPos = sp.basePos;
		float curFacing = sp.baseFacing, curRoll = 0.0f, curPitch = 0.0f;

		curPos.y += std::sin(bgTimer_ * kBreatheFreq + sp.phaseOffset) * kBreatheAmp;
		const float phase = bgTimer_ * sp.speed + sp.phaseOffset;

		switch (sp.behavior)
		{
		case BehaviorType::SentryLookout:
			curFacing = sp.baseFacing + std::sin(phase) * kSentryScanAmp1 + std::sin(phase * kSentrySubFreqMul) * kSentryScanAmp2;
			break;
		case BehaviorType::OvalPatrol:
		{
			curPos.x += std::sin(phase) * sp.radiusX;
			curPos.z += std::cos(phase) * sp.radiusZ;
			curFacing = std::atan2(std::cos(phase) * sp.radiusX, -std::sin(phase) * sp.radiusZ);
			const float walkCycle = phase * kOvalWalkCycleMul;
			curPos.y += std::abs(std::sin(walkCycle)) * kWaddleHopAmp;
			curRoll = std::sin(walkCycle) * kWaddleRollAmp;
			break;
		}
		case BehaviorType::StandGuard:
			curFacing = sp.baseFacing + std::sin(phase) * kStandGuardScanAmp;
			curPitch = kStandGuardPitch;
			break;
		case BehaviorType::ScoutScan:
		{
			const float turn = std::clamp(std::sin(phase) * kScoutTurnGain, -1.0f, 1.0f);
			curFacing = sp.baseFacing + Common::Smoothstep01(std::abs(turn)) * (turn < 0.0f ? -1.0f : 1.0f) * kScoutYawAmp;
			break;
		}
		case BehaviorType::PatrolAcross:
		{
			curPos.x = sp.basePos.x + std::sin(phase) * sp.radiusX;
			const float turnRate = std::clamp(std::cos(phase) * kPatrolTurnGain, -1.0f, 1.0f);
			curFacing = (turnRate >= 0.0f) ? (kHalfPi - (1.0f - turnRate) * kHalfPi) : (-kHalfPi + (1.0f + turnRate) * kHalfPi);
			const float walkCycle = phase * kPatrolWalkCycleMul;
			curPos.y += std::abs(std::sin(walkCycle)) * kWaddleHopAmp;
			curRoll = std::sin(walkCycle) * kWaddleRollAmp;
			break;
		}
		}

		ed.model->SetTranslate(curPos);
		ed.model->SetRotate({ curPitch, curFacing, curRoll });
		ed.model->Update();

		if (i < sEnemyDuckShadows.size() && sEnemyDuckShadows[i])
		{
			sEnemyDuckShadows[i]->SetTranslate({ curPos.x, Terrain::kFarLandY + Shadows::kHeightOffset, curPos.z });
			sEnemyDuckShadows[i]->Update();
		}
	}
}

void TitleScene::UpdateDynamicMeshes()
{
	if (riverModel_)
	{
		const size_t waterVertCount = waterBaseVertices_.size();
		const WaterVertexBase* wbPtr = waterBaseVertices_.data();
		UpdateMappedVertices(riverModel_.get(), waterVertCount, [&](Object3d::VertexData* vData)
		{
			const float flowU = bgTimer_ * Water::kFlowUvSpeed;
			for (size_t i = 0; i < waterVertCount; ++i)
			{
				const WaterVertexBase& b = wbPtr[i];
				const WaterSurfaceSample s = EvaluateWaterSurface(b.basePosX, b.basePosZ, bgTimer_);
				vData[i].position = { b.basePosX, Terrain::kWaterY + s.displacement.y, b.basePosZ, 1.0f };
				vData[i].normal = s.normal;
				vData[i].texcoord = { b.baseU - flowU, b.baseV };
			}
		});
		riverModel_->Update();
	}

	if (sNearGroundModel) sNearGroundModel->Update();
	if (sFarGroundModel) sFarGroundModel->Update();
	if (sShadowDecalsModel) sShadowDecalsModel->Update();
	if (sPathDecalModel) sPathDecalModel->Update();

	for (size_t si = 0; si < sScenery.size(); ++si)
	{
		auto& s = sScenery[si];
		if (s.hasSway)
		{
			const size_t swayVertCount = s.baseVertices.size();
			const SceneryVertexBase* bvPtr = s.baseVertices.data();
			UpdateMappedVertices(s.model.get(), swayVertCount, [&](Object3d::VertexData* vData)
			{
				for (size_t i = 0; i < swayVertCount; ++i)
				{
					const SceneryVertexBase& bv = bvPtr[i];
					if (bv.swayFactor <= Wind::kMinSwayEpsilon) continue;
					const Vector3 offset = EvaluateWind(bv.basePos, bgTimer_, bv.swayFactor);
					vData[i].position = { bv.basePos.x + offset.x, bv.basePos.y + offset.y, bv.basePos.z + offset.z, 1.0f };
				}
			});
		}
		else if (s.isFarForest && s.model)
		{
			const float phase = static_cast<float>(si) * 0.9f;
			const float swayX = 0.018f * std::sin(bgTimer_ * 1.25f + phase) + 0.007f * std::sin(bgTimer_ * 2.65f);
			const float swayZ = 0.012f * std::cos(bgTimer_ * 1.05f + phase);
			s.model->SetTranslate({ swayX, 0.0f, swayZ });
		}
		if (s.model) s.model->Update();
	}

	if (sandbagModel_) sandbagModel_->Update();
	if (crateModel1_) crateModel1_->Update();
	if (crateModel2_) crateModel2_->Update();
	if (sExtractionPadModel) sExtractionPadModel->Update();
	for (auto& p : sEnemyProps) if (p.model) p.model->Update();
}

void TitleScene::UpdateEnvironmentParticles(float deltaTime)
{
	if (!appParticleManager_ || !GetParticleManager()) return;

	using namespace Particles;
	AppParticleManager* pm = appParticleManager_.get();
	const Vector3 padPos = sExtractionPadModel ? sExtractionPadModel->GetTranslate() : Props::kExtractionPadFallbackPos;

	// (A) 最奥脱出地点のエメラルドグリーン信号煙 
	ConsumeInterval(sExtractionSmokeTimer, deltaTime, kSmokeInterval, [&]()
	{
		const Vector3 pos = JitterPosXZ(padPos.x, padPos.y + kSmokeOffsetY, padPos.z, kSmokeSpreadXZ);
		const Vector3 vel = { SampleRange(kSmokeVelX), SampleRange(kSmokeVelY), RandRange(-kSmokeVelJitterZ, kSmokeVelJitterZ) };
		EmitParticle(pm, pos, vel, sFx.puff, kSmokeSpec);
	});

	// (B) 脱出地点外周の光粒 
	ConsumeInterval(sBeaconMoteTimer, deltaTime, kBeaconInterval, [&]()
	{
		const float th = Rand01() * kTwoPi;
		const Vector3 pos = { padPos.x + std::cos(th) * kBeaconRadius, padPos.y + kBeaconOffsetY, padPos.z + std::sin(th) * kBeaconRadius };
		EmitParticle(pm, pos, { 0.0f, SampleRange(kBeaconVelY), 0.0f }, sFx.circle, kBeaconSpec);
	});

	// (C-1) 川面の微細な白泡 
	ConsumeInterval(sRiverWaveTimer, deltaTime, kRiverWaveInterval, [&]()
	{
		const float fx = SampleRange(kRiverWaveRangeX);
		const float fz = RandRange(Terrain::NearShoreZ(fx) + kRiverWaveShoreMargin, Terrain::FarShoreZ(fx) - kRiverWaveShoreMargin);
		const Vector3 vel = { SampleRange(kRiverWaveSpeed), 0.0f, RandRange(-kRiverWaveJitterZ, kRiverWaveJitterZ) };
		EmitParticle(pm, { fx, SampleRiverSurfaceY(fx, fz, kRiverSurfaceOffsetY), fz }, vel, sFx.circle, kRiverWaveSpec);
	});

	// (C-2) 川中の岩・橋脚ウェイク 
	ConsumeInterval(sRiverWakeTimer, deltaTime, kRiverWakeInterval, [&]()
	{
		const auto& obs = kRiverObstacles[rand() % kRiverObstacleCount];
		const float spawnX = obs.x - obs.r * SampleRange(kWakeUpstreamRatio);
		const float spawnZ = obs.z + RandRange(-obs.r * 0.5f, obs.r * 0.5f);
		const float spawnY = SampleRiverSurfaceY(spawnX, spawnZ, kRiverSurfaceOffsetY);

		const int rDrops = kWakeDropBaseCount + rand() % kWakeDropRandSpan;
		for (int rd = 0; rd < rDrops; ++rd)
		{
			const Vector3 dropVel = { SampleRange(kWakeDropVelX), SampleRange(kWakeDropVelY), RandRange(-kWakeDropVelJitterZ, kWakeDropVelJitterZ) };
			EmitParticle(pm, JitterPosXZ(spawnX, spawnY + kWakeDropOffsetY, spawnZ, kWakeDropPosJitter), dropVel, sFx.circle, kWakeDropSpec);
		}
		if (rand() % kWakeMistChanceMod == 0)
		{
			const Vector3 mistVel = { SampleRange(kWakeMistVelX), SampleRange(kWakeMistVelY), RandRange(-kWakeMistVelJitterZ, kWakeMistVelJitterZ) };
			EmitParticle(pm, { spawnX, spawnY + kWakeMistOffsetY, spawnZ }, mistVel, sFx.circle, kWakeMistSpec);
		}
		const float sideSign = (spawnZ >= obs.z) ? 1.0f : -1.0f;
		const Vector3 foamVel = { SampleRange(kWakeFoamVelX), 0.0f, sideSign * SampleRange(kWakeFoamSideZ) };
		EmitParticle(pm, { spawnX, spawnY, spawnZ }, foamVel, sFx.circle, kWakeFoamSpec);
	});

	// (C-3) 両岸さざ波 
	ConsumeInterval(sRiverFoamTimer, deltaTime, kShoreFoamInterval, [&]()
	{
		const float fx = SampleRange(kShoreFoamRangeX);
		const bool nearSide = (Rand01() < 0.5f);
		const float fz = nearSide ? (Terrain::NearShoreZ(fx) + SampleRange(kShoreFoamOffsetZ)) : (Terrain::FarShoreZ(fx) - SampleRange(kShoreFoamOffsetZ));
		EmitParticle(pm, { fx, SampleRiverSurfaceY(fx, fz, kRiverSurfaceOffsetY), fz }, { SampleRange(kShoreFoamVelX), 0.0f, 0.0f }, sFx.circle, kShoreFoamSpec);
	});

	// (C-4) 水面の陽光きらめき 
	ConsumeInterval(sRiverGlintTimer, deltaTime, kGlintInterval, [&]()
	{
		const float gx = SampleRange(kGlintRangeX);
		const float gz = RandRange(Terrain::NearShoreZ(gx) + kGlintShoreMargin, Terrain::FarShoreZ(gx) - kGlintShoreMargin);
		EmitParticle(pm, { gx, SampleRiverSurfaceY(gx, gz, kRiverSurfaceOffsetY), gz }, { SampleRange(kGlintVelX), SampleRange(kGlintVelY), 0.0f }, sFx.circle, kGlintSpec);
	});

	// (D) 陽光ダストモート 
	ConsumeInterval(sAmbientDustTimer, deltaTime, kAmbientDustInterval, [&]()
	{
		const Vector3 p = { SampleRange(kAmbientDustBox.x), SampleRange(kAmbientDustBox.y), SampleRange(kAmbientDustBox.z) };
		const Vector3 v = { SampleRange(kAmbientDustVelX), SampleRange(kAmbientDustVelY), RandRange(-kAmbientDustVelJitterZ, kAmbientDustVelJitterZ) };
		EmitParticle(pm, p, v, sFx.circle, kAmbientDustSpec);
	});

	// (E) 地面の微小土煙・砂埃 
	ConsumeInterval(sGroundDustTimer, deltaTime, kGroundDustInterval, [&]()
	{
		const bool nearSide = (Rand01() < 0.5f);
		const Vector3 dustPos = nearSide
			? Vector3{ SampleRange(kGroundDustNearX), Terrain::kLandY + kGroundDustOffsetY, SampleRange(kGroundDustNearZ) }
			: Vector3{ SampleRange(kGroundDustFarX), Terrain::kFarLandY + kGroundDustOffsetY, SampleRange(kGroundDustFarZ) };
		const Vector3 dustVel = { SampleRange(kGroundDustVelX), SampleRange(kGroundDustVelY), RandRange(-kGroundDustVelJitterZ, kGroundDustVelJitterZ) };
		EmitParticle(pm, dustPos, dustVel, sFx.circle, kGroundDustSpec);
	});

	// (F) 風に舞い散る木の葉 (物理3Dオブジェクト) 
	const float windGust = Wind::kGustBase + Wind::kGustAmp * std::sin(bgTimer_ * Wind::kGustTimeFreq);
	const float leafInterval = (windGust > Leaves::kGustThreshold) ? Leaves::kIntervalGusting : Leaves::kIntervalCalm;

	ConsumeInterval(sLeafTimer, deltaTime, leafInterval, [&]()
	{
		auto it = std::find_if(sFallingLeaves.begin(), sFallingLeaves.end(), [](const FallingLeaf3D& l) { return !l.active; });
		if (it == sFallingLeaves.end() || !it->model) return;

		FallingLeaf3D& slot = *it;
		slot.active = true;
		slot.age = 0.0f;
		slot.maxLife = SampleRange(Leaves::kLife);

		const auto& box = Leaves::kSpawnBoxes[rand() % Leaves::kSpawnBoxCount];
		slot.pos = { SampleRange(box.x), SampleRange(box.y), SampleRange(box.z) };

		const float windSpeed = SampleRange(Leaves::kWindSpeed) * (Leaves::kWindGustBaseMul + windGust * Leaves::kWindGustExtraMul);
		slot.baseVel = {
			windSpeed * Wind::kWindDirX + RandRange(-Leaves::kWindVelJitterXZ, Leaves::kWindVelJitterXZ),
			SampleRange(Leaves::kFallVelY) - (windGust * Leaves::kFallGustExtraY),
			windSpeed * Wind::kWindDirZ + RandRange(-Leaves::kWindVelJitterXZ, Leaves::kWindVelJitterXZ)
		};

		slot.wobbleDir = Leaves::kDefaultWobbleDir;
		slot.wobbleFreq = SampleRange(Leaves::kWobbleFreq);
		slot.wobbleAmp = SampleRange(Leaves::kWobbleAmp);
		slot.wobblePhase = RandRange(0.0f, kTwoPi);

		slot.rot = { RandRange(0.0f, kTwoPi), RandRange(0.0f, kTwoPi), RandRange(-Leaves::kInitRollRange, Leaves::kInitRollRange) };
		slot.rotSpeed = { SampleRange(Leaves::kRotPitch) * RandSign(), SampleRange(Leaves::kRotYaw) * RandSign(), RandRange(-Leaves::kRotRollRange, Leaves::kRotRollRange) };
		slot.color = Leaves::kColorPalettes[rand() % Leaves::kPaletteCount];
		slot.scale = SampleRange(Leaves::kScale);

		slot.model->SetColor(slot.color);
		slot.model->SetScale({ slot.scale, slot.scale, slot.scale });
		slot.model->SetTranslate(slot.pos);
		slot.model->SetRotate(slot.rot);
		slot.model->Update();
	});

	for (auto& leaf : sFallingLeaves)
	{
		if (!leaf.active || !leaf.model) continue;
		leaf.age += deltaTime;
		if (leaf.age >= leaf.maxLife || leaf.pos.y <= (Terrain::kWaterY + Leaves::kDespawnWaterMarginY) || leaf.pos.x > Leaves::kDespawnMaxX)
		{
			leaf.active = false;
			continue;
		}

		const float sway = std::sin(leaf.age * leaf.wobbleFreq + leaf.wobblePhase);
		const Vector3 swayVel = {
			leaf.wobbleDir.x * (sway * leaf.wobbleAmp),
			Leaves::kSwayLiftVelAmp * std::cos(leaf.age * leaf.wobbleFreq * 2.0f + leaf.wobblePhase),
			leaf.wobbleDir.z * (sway * leaf.wobbleAmp)
		};

		leaf.pos = leaf.pos + (leaf.baseVel + swayVel) * deltaTime;
		leaf.rot.x += leaf.rotSpeed.x * deltaTime;
		leaf.rot.y += leaf.rotSpeed.y * deltaTime;
		leaf.rot.z = leaf.rotSpeed.z + sway * Leaves::kSwayRollGain;

		leaf.model->SetTranslate(leaf.pos);
		leaf.model->SetRotate(leaf.rot);
		leaf.model->SetScale({ leaf.scale, leaf.scale, leaf.scale });
		leaf.model->Update();
	}

	appParticleManager_->Update(deltaTime, duckBasePos_);
}

void TitleScene::UpdateSpritesUI()
{
	if (fadeOverlaySprite_ && !isStarting_)
	{
		const float a = 1.0f - std::clamp(sIntroTimer / UI::kIntroFadeDuration, 0.0f, 1.0f);
		fadeOverlaySprite_->SetColor({ 0.0f, 0.0f, 0.0f, a });
	}

	if (titleLogoSprite_)
	{
		const float lt = (sIntroTimer - Timing::kLogoSlamTime) / Timing::kLogoSlideTime;
		const float startX = -sLogoSize.x - UI::kLogoDashExtraOffsetX;
		auto dashX = [&](float t) { return startX + (sLogoFinalPos.x - startX) * EaseOutCubic(t); };

		if (lt <= 0.0f)
		{
			titleLogoSprite_->SetColor(UI::kTransparentWhite);
			for (Sprite* g : sLogoGhosts) if (g) g->SetColor(UI::kLogoGhostRgb);
			if (sLogoShine) sLogoShine->SetColor(UI::kTransparentWhite);
		}
		else
		{
			const float since = sLogoLanded ? sIntroTimer - (Timing::kLogoSlamTime + Timing::kLogoSlideTime) : -1.0f;
			const float x = dashX(lt);
			float y = sLogoFinalPos.y;
			if (since >= 0.0f)
			{
				y += std::sin(since * UI::kLogoFloatFreq) * UI::kLogoFloatAmpY * std::clamp(since / UI::kLogoFloatFadeTime, 0.0f, 1.0f);
			}

			float sx = 1.0f, sy = 1.0f;
			if (lt < 1.0f)
			{
				const float speed = 1.0f - lt;
				sx = 1.0f + UI::kLogoStretchX * speed;
				sy = 1.0f - UI::kLogoSquashY * speed;
			}
			if (since >= 0.0f)
			{
				const float e = std::exp(-since * UI::kLogoSpringDecay) * std::cos(since * UI::kLogoSpringFreq);
				sx = 1.0f - UI::kLogoSpringAmpX * e;
				sy = 1.0f + UI::kLogoSpringAmpY * e;
			}

			const Vector2 size = { sLogoSize.x * sx, sLogoSize.y * sy };
			const Vector2 pos = { x - (size.x - sLogoSize.x) * 0.5f, y - (size.y - sLogoSize.y) * 0.5f };
			titleLogoSprite_->SetPosition(pos);
			titleLogoSprite_->SetSize(size);
			titleLogoSprite_->SetColor({ 1.0f, 1.0f, 1.0f, std::clamp(lt * UI::kLogoAlphaFadeSpeed, 0.0f, 1.0f) });

			for (int i = 0; i < UI::kLogoGhostCount; ++i)
			{
				Sprite* g = sLogoGhosts[i];
				if (!g) continue;
				const float gt = lt - UI::kGhostLagStep * static_cast<float>(i + 1);
				const float ga = UI::kGhostBaseAlpha[i]
					* std::clamp(gt * UI::kGhostFadeInSpeed, 0.0f, 1.0f)
					* (1.0f - std::clamp((gt - UI::kGhostFadeOutStart) / UI::kGhostFadeOutSpan, 0.0f, 1.0f));
				const Vector2 gs = { sLogoSize.x * UI::kGhostScaleX, sLogoSize.y * UI::kGhostScaleY };
				g->SetPosition({ dashX(gt) - (gs.x - sLogoSize.x), sLogoFinalPos.y + (sLogoSize.y - gs.y) * 0.5f });
				g->SetSize(gs);
				g->SetColor({ UI::kLogoGhostRgb.x, UI::kLogoGhostRgb.y, UI::kLogoGhostRgb.z, ga });
			}

			if (sLogoShine)
			{
				sLogoShine->SetPosition(pos);
				sLogoShine->SetSize(size);
				float shineA = 0.0f;
				if (isStarting_)
				{
					shineA = ApplyShineFlash(sLogoShine, startTransitionTimer_, UI::kLogoStartFlashDuration, UI::kLogoTexW, UI::kLogoTexH);
				}
				else if (since >= 0.0f && since < UI::kLogoLandFlashDuration)
				{
					shineA = ApplyShineFlash(sLogoShine, since, UI::kLogoLandFlashDuration, UI::kLogoTexW, UI::kLogoTexH);
				}
				else if (since >= 0.0f)
				{
					const int frame = ShineFrame(since - UI::kLogoShineDelay, UI::kLogoShinePeriod, UI::kLogoShineDuration);
					if (frame >= 0)
					{
						SetShineCell(sLogoShine, frame, UI::kLogoTexW, UI::kLogoTexH);
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
			const float appear = sLogoLanded ? std::clamp((sIntroTimer - Timing::kIntroDuration) / UI::kPromptAppearDuration, 0.0f, 1.0f) : 0.0f;
			const float e = EaseOutCubic(appear);
			const float pulse = 0.5f + 0.5f * std::sin(bgTimer_ * UI::kPromptPulseFreq);
			const float scale = 1.0f + UI::kPromptPulseScaleAmp * pulse;
			const Vector2 pos = { sPromptBasePos.x - UI::kPromptSlideOffsetX * (1.0f - EaseOutBack(appear)), sPromptBasePos.y };
			const Vector2 size = { UI::kPromptSize.x * scale, UI::kPromptSize.y * scale };
			pressSpaceSprite_->SetPosition(pos);
			pressSpaceSprite_->SetSize(size);
			pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, e * (UI::kPromptAlphaBase + UI::kPromptAlphaPulse * pulse) });

			if (sPromptShine)
			{
				sPromptShine->SetPosition(pos);
				sPromptShine->SetSize(size);
				const int frame = ShineFrame(sIntroTimer - Timing::kIntroDuration - UI::kPromptShineDelay, UI::kPromptShinePeriod, UI::kPromptShineDuration);
				if (frame >= 0) SetShineCell(sPromptShine, frame, UI::kPromptTexW, UI::kPromptTexH);
				sPromptShine->SetColor({ 1.0f, 1.0f, 1.0f, frame >= 0 ? e : 0.0f });
			}
		}
		else
		{
			const float t = startTransitionTimer_;
			const float pop = 1.0f + UI::kPromptPopScaleAmp * EaseOutCubic(std::clamp(t / UI::kPromptPopDuration, 0.0f, 1.0f));
			const float alpha = std::clamp(1.0f - (t - UI::kPromptFadeOutDelay) / UI::kPromptFadeOutDuration, 0.0f, 1.0f);
			const Vector2 size = { UI::kPromptSize.x * pop, UI::kPromptSize.y * pop };
			pressSpaceSprite_->SetPosition(sPromptBasePos);
			pressSpaceSprite_->SetSize(size);
			pressSpaceSprite_->SetColor({ 1.0f, 1.0f, 1.0f, alpha });

			if (sPromptShine)
			{
				const float flashA = ApplyShineFlash(sPromptShine, t, UI::kPromptStartFlashDuration, UI::kPromptTexW, UI::kPromptTexH);
				sPromptShine->SetPosition(sPromptBasePos);
				sPromptShine->SetSize(size);
				sPromptShine->SetColor({ 1.0f, 1.0f, 1.0f, flashA });
			}
		}
	}

	if (spriteManager_) spriteManager_->Update();
	for (auto& sp : sprites_) if (sp) sp->Update();
}

// =====================================================================
// TitleScene 描画 
// =====================================================================
void TitleScene::Draw(SceneRenderRequests& renderRequests)
{
	if (camera_) camera_->Update();

	if (GetObject3dCom() && dxCommon_ && camera_)
	{
		RenderContext base{};
		base.commandList = dxCommon_->GetCommandList().Get();
		base.windowAPI = dxCommon_->GetWindowAPI();
		base.camera = camera_;
		base.light = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
		if (MaterialManager* matMgr = GetMaterialManager(); matMgr && matMgr->GetMaterialResource())
		{
			base.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
		}

		Object3dCom* com = GetObject3dCom();

		// 1. 地形・小道・接地影デカール 
		DrawModel(com, sNearGroundModel.get(), base, sGrassTexIndex);
		DrawModel(com, sFarGroundModel.get(), base, sGrassTexIndex);
		DrawModel(com, sPathDecalModel.get(), base, circleTexIndex_);
		DrawModel(com, sShadowDecalsModel.get(), base, circleTexIndex_);
		for (auto& s : sEnemyDuckShadows) DrawModel(com, s.get(), base, circleTexIndex_);

		// 2. 環境装飾・水面・対岸プロップ 
		for (auto& s : sScenery) DrawModel(com, s.model.get(), base, s.textureIndex);
		DrawModel(com, riverModel_.get(), base, riverTexIndex_);
		DrawModel(com, sExtractionPadModel.get(), base, sExtractionPadTexIndex);
		for (auto& p : sEnemyProps) DrawModel(com, p.model.get(), base, p.textureIndex);

		// 3. 敵兵アヒル・手前岸プロップ・主役アヒル 
		const uint32_t enemyTex = TextureManager::GetInstance()->Load("Resources/duck_enemy.png");
		for (auto& ed : sEnemyDucks) DrawModel(com, ed.model.get(), base, enemyTex);
		DrawModel(com, sandbagModel_.get(), base, sandbagTexIndex_);
		DrawModel(com, crateModel1_.get(), base, crateTexIndex_);
		DrawModel(com, crateModel2_.get(), base, crateTexIndex_);
		DrawModel(com, duckModel_.get(), base, TextureManager::GetInstance()->Load("Resources/duck.png"));

		// 4. 3D舞い散る木の葉・パーティクル・2D UI 
		for (auto& leaf : sFallingLeaves)
		{
			if (leaf.active && leaf.model) DrawModel(com, leaf.model.get(), base, sFx.leaf);
		}
		if (appParticleManager_)
		{
			RenderContext pctx = base;
			if (circleTexIndex_ != UINT32_MAX) pctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(circleTexIndex_);
			appParticleManager_->Draw(pctx);
		}
		if (spriteManager_) spriteManager_->DrawAll(base, camera_, &sprites_);
	}

	renderRequests.sceneDrawn = true;
}

// =====================================================================
// 水面高さ・法線解析計算 
// =====================================================================
TitleScene::WaterSurfaceSample TitleScene::EvaluateWaterSurface(float x, float z, float time) const
{
	WaterSurfaceSample result{};
	float dispY = 0.0f, gradX = 0.0f, gradZ = 0.0f;

	for (const auto& w : Water::kGerstnerWaves)
	{
		const float k = kTwoPi / w.len;
		const float th = k * (w.dirX * x + w.dirZ * z) - k * w.speed * time;
		dispY += w.amp * std::cos(th);
		gradX -= w.dirX * k * w.amp * std::sin(th);
		gradZ -= w.dirZ * k * w.amp * std::sin(th);
	}

	for (const auto& rip : mouseWaveSources_)
	{
		if (rip.age >= rip.maxLife) continue;
		const float fade = Square(1.0f - rip.age / rip.maxLife);
		const float rx = x - rip.center.x, rz = z - rip.center.z;
		const float r = std::sqrt(rx * rx + rz * rz);
		const float dr = r - (Water::kRippleBaseRadius + rip.age * Water::kRippleExpandSpeed);
		if (std::abs(dr) < Water::kRippleBandWidth && r > Water::kRippleMinCenterDist)
		{
			const float env = 0.5f + 0.5f * std::cos(dr / Water::kRippleBandWidth * kPi);
			const float a = rip.power * fade * env * Water::kRippleAmpScale;
			dispY += a;
			const float dw = -a * (dr / Water::kRippleBandWidth);
			gradX -= dw * (rx / r);
			gradZ -= dw * (rz / r);
		}
	}

	const Vector3 n = { -gradX, 1.0f, -gradZ };
	const float len = Length3(n);
	result.displacement = { 0.0f, dispY, 0.0f };
	result.normal = { n.x / len, n.y / len, n.z / len };
	result.foamFactor = 0.0f;
	return result;
}

float TitleScene::SampleRiverSurfaceY(float x, float z, float offsetY) const
{
	return Terrain::kWaterY + EvaluateWaterSurface(x, z, bgTimer_).displacement.y + offsetY;
}
