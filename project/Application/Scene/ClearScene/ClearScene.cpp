#include "ClearScene.h"
#include "ClearSceneTuning.h"
#include "KeyInput.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "Object3dCom.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "Baziru3_Engine/Graphics/Graphics/SceneRenderRequests.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/UI/TacticalUICanvas.h"
#include "Application/Particle/AppParticleManager.h"
#include "ParticleManager.h"
#include "ParticleEmitter.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

namespace
{
	using namespace ClearSceneTuning;
	using GameConfig::Math::kHalfPi;
	using GameConfig::Math::kPi;
	using GameConfig::Math::kTwoPi;

	Vector3 LerpVec(const Vector3& a, const Vector3& b, float t)
	{
		return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
	}

	float EaseInQuad(float t) { return t * t; }
	float EaseOutQuad(float t) { return t * (2.0f - t); }

	float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		const float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	float EaseOutExpo(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return (t >= Common::kExpoSnapThreshold) ? 1.0f : (1.0f - std::exp2(Common::kExpoDecayExponent * t));
	}

	float EaseInOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		if (t < 0.5f) return Common::kCubicInOutScale * t * t * t;
		const float u = -2.0f * t + 2.0f;
		return 1.0f - (u * u * u) * 0.5f;
	}

	float EaseOutWindow(float elapsed, float delaySec, float durationSec)
	{
		return EaseOutCubic(std::clamp((elapsed - delaySec) / durationSec, 0.0f, 1.0f));
	}

	float RandRange(float minV, float maxV)
	{
		const float u = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return minV + (maxV - minV) * u;
	}

	float SampleRange(const Particles::Range1D& r)
	{
		return (r.minVal == r.maxVal) ? r.minVal : RandRange(r.minVal, r.maxVal);
	}

	Vector3 SampleRange3D(const Particles::Range3D& r)
	{
		const float x = SampleRange({ r.minVal.x, r.maxVal.x });
		const float y = SampleRange({ r.minVal.y, r.maxVal.y });
		const float z = SampleRange({ r.minVal.z, r.maxVal.z });
		return { x, y, z };
	}

	Matrix4x4 MakeEulerRotationMatrix(const Vector3& rot)
	{
		return Multiply(MakeRotateXMatrix(rot.x), Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
	}

	Vector3 TransformLocalOffset(const Vector3& origin, const Matrix4x4& rotMat, const Vector3& localOffset, float uniformScale)
	{
		const Vector3 s = localOffset * uniformScale;
		return origin + Vector3{
			s.x * rotMat.m[Common::kAxisX][Common::kAxisX] + s.y * rotMat.m[Common::kAxisY][Common::kAxisX] + s.z * rotMat.m[Common::kAxisZ][Common::kAxisX],
			s.x * rotMat.m[Common::kAxisX][Common::kAxisY] + s.y * rotMat.m[Common::kAxisY][Common::kAxisY] + s.z * rotMat.m[Common::kAxisZ][Common::kAxisY],
			s.x * rotMat.m[Common::kAxisX][Common::kAxisZ] + s.y * rotMat.m[Common::kAxisY][Common::kAxisZ] + s.z * rotMat.m[Common::kAxisZ][Common::kAxisZ]
		};
	}

	void FormatIntCommas(int value, char* outBuf, size_t bufSize)
	{
		if (!outBuf || bufSize == 0) return;
		char raw[Common::kRawUintBufSize];
		const bool neg = (value < 0);
		const unsigned int absVal = neg ? static_cast<unsigned int>(-value) : static_cast<unsigned int>(value);
		std::snprintf(raw, sizeof(raw), "%u", absVal);
		const int len = static_cast<int>(std::strlen(raw));

		char temp[Common::kCommaTempBufSize];
		int outIdx = 0;
		if (neg) temp[outIdx++] = '-';
		for (int i = 0; i < len && outIdx < Common::kCommaMaxWriteIndex; ++i)
		{
			if (i > 0 && ((len - i) % Common::kCommaGroupDigits == 0)) temp[outIdx++] = ',';
			temp[outIdx++] = raw[i];
		}
		temp[outIdx] = '\0';
		std::snprintf(outBuf, bufSize, "%s", temp);
	}

	// 直方体1個を頂点・インデックス配列へ追加 (Add Axis-Aligned Box)
	void AddOrientedBox(Object3d::ModelData& data, const Meshes::ProceduralBoxSpec& box)
	{
		const Vector3 h = box.size * 0.5f;
		Vector3 corners[Meshes::kBoxCornerCount]{};
		for (size_t i = 0; i < Meshes::kBoxCornerCount; ++i)
		{
			const Vector3& s = Meshes::kBoxCornerSigns[i];
			corners[i] = { box.center.x + s.x * h.x, box.center.y + s.y * h.y, box.center.z + s.z * h.z };
		}

		for (size_t f = 0; f < Meshes::kBoxFaceCount; ++f)
		{
			const uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
			for (size_t v = 0; v < Meshes::kBoxFaceVerts; ++v)
			{
				const Vector3& pos = corners[Meshes::kBoxFaces[f][v]];
				const Vector2 uv = {
					Meshes::kQuadUseMaxU[v] ? box.uvMax.x : box.uvMin.x,
					Meshes::kQuadUseMaxV[v] ? box.uvMax.y : box.uvMin.y
				};
				data.vertices.push_back({ { pos.x, pos.y, pos.z, 1.0f }, uv, Meshes::kBoxNormals[f] });
			}
			for (uint32_t triIdx : Meshes::kQuadTriIndices) data.indices.push_back(fBase + triIdx);
		}
	}

	void BuildBoxListModel(Object3d::ModelData& data, std::span<const Meshes::ProceduralBoxSpec> boxes, float boundingRadius)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(boxes.size() * Meshes::kBoxFaceCount * Meshes::kBoxFaceVerts);
		data.indices.reserve(boxes.size() * Meshes::kBoxFaceCount * Meshes::kQuadIndexCount);
		for (const auto& box : boxes) AddOrientedBox(data, box);
		data.boundingRadius = boundingRadius;
	}

	std::unique_ptr<Object3d> CreateModelInstance(
		Object3dCom* com, Camera* camera, const Object3d::ModelData& data,
		const Vector3& scale, bool enableLighting = true, const Vector4& tint = Common::kWhiteColor)
	{
		auto obj = std::make_unique<Object3d>();
		obj->Initialize(com, data);
		obj->SetCamera(camera);
		obj->SetScale(scale);
		obj->SetColor(tint);
		obj->SetEnableLighting(enableLighting);
		return obj;
	}

	std::unique_ptr<Object3d> CreateObjModelInstance(
		Object3dCom* com, Camera* camera, const char* objFile, const char* texPath, uint32_t texIndex,
		float boundingRadius, const Vector3& scale, bool enableLighting = true, const Vector4& tint = Common::kWhiteColor)
	{
		Object3d::ModelData data = Object3d::LoadObjFile("Resources", objFile);
		data.material.textureFilePath = texPath;
		data.material.textureIndex    = texIndex;
		data.boundingRadius           = boundingRadius;
		return CreateModelInstance(com, camera, data, scale, enableLighting, tint);
	}

	std::unique_ptr<Object3d> CreateBoxModelInstance(
		Object3dCom* com, Camera* camera, std::span<const Meshes::ProceduralBoxSpec> boxes, float boundingRadius,
		const char* texPath, uint32_t texIndex, const Vector3& scale, const Vector4& tint = Common::kWhiteColor)
	{
		Object3d::ModelData data{};
		BuildBoxListModel(data, boxes, boundingRadius);
		data.material.textureFilePath = texPath;
		data.material.textureIndex    = texIndex;
		return CreateModelInstance(com, camera, data, scale, true, tint);
	}

	std::unique_ptr<Object3d> CreateSharedModelInstance(
		Object3dCom* com, Camera* camera, Object3d* master, float uniformScale,
		bool enableLighting = true, const Vector4& tint = Common::kWhiteColor)
	{
		auto obj = std::make_unique<Object3d>();
		obj->InitializeShared(com, master);
		obj->SetCamera(camera);
		obj->SetScale({ uniformScale, uniformScale, uniformScale });
		obj->SetColor(tint);
		obj->SetEnableLighting(enableLighting);
		return obj;
	}

	void SyncObjectTransform(Object3d* obj, const Vector3& pos, const Vector3& rot)
	{
		if (!obj) return;
		obj->SetTranslate(pos);
		obj->SetRotate(rot);
		obj->Update();
	}

	void DrawModel(Object3dCom* com, Object3d* model, const RenderContext& base, uint32_t texIndex)
	{
		if (!com || !model) return;
		RenderContext ctx = base;
		if (texIndex != UINT32_MAX) ctx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(texIndex);
		com->Draw(model, ctx, model->GetModelData(), true);
	}

	// 既存ModelDataを任意の位置・Y回転・スケールでバッチ結合 (Mesh Batching Helper)
	void AppendTransformedMesh(
		Object3d::ModelData& dst, const Object3d::ModelData& src,
		const Vector3& pos, float yaw, const Vector3& scale)
	{
		if (src.vertices.empty()) return;
		const float cosY = std::cos(yaw);
		const float sinY = std::sin(yaw);
		const uint32_t baseVertex = static_cast<uint32_t>(dst.vertices.size());

		dst.vertices.reserve(dst.vertices.size() + src.vertices.size());
		for (const auto& sv : src.vertices)
		{
			const float sx = sv.position.x * scale.x;
			const float sy = sv.position.y * scale.y;
			const float sz = sv.position.z * scale.z;
			const float rx = sx * cosY + sz * sinY;
			const float rz = -sx * sinY + sz * cosY;

			float nx = sv.normal.x * cosY + sv.normal.z * sinY;
			float ny = sv.normal.y;
			float nz = -sv.normal.x * sinY + sv.normal.z * cosY;
			const float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
			if (nLen > Common::kNormalMinLen) { nx /= nLen; ny /= nLen; nz /= nLen; }

			dst.vertices.push_back({ { pos.x + rx, pos.y + sy, pos.z + rz, 1.0f }, sv.texcoord, { nx, ny, nz } });
		}

		const size_t idxCount = src.indices.empty() ? src.vertices.size() : src.indices.size();
		dst.indices.reserve(dst.indices.size() + idxCount);
		if (!src.indices.empty())
		{
			for (uint32_t idx : src.indices) dst.indices.push_back(baseVertex + idx);
		}
		else
		{
			for (uint32_t i = 0; i < static_cast<uint32_t>(src.vertices.size()); ++i) dst.indices.push_back(baseVertex + i);
		}
	}

	// 360度パノラマ連峰リング (Panoramic Mountain Ring Builder)
	void BuildMountainRingModel(Object3d::ModelData& data, const Terrain::MountainLayerSpec& spec)
	{
		data.vertices.clear();
		data.indices.clear();
		data.vertices.reserve(static_cast<size_t>(spec.segments) * Terrain::kMtnVertsPerSeg);
		data.indices.reserve(static_cast<size_t>(spec.segments) * Terrain::kMtnIndicesPerSeg);

		auto EvalCrest = [&](float a) -> float {
			const float w1 = 0.5f + 0.5f * std::sin(a * Terrain::kMtnFreq1 + spec.phaseSeed);
			const float w2 = 1.0f - std::fabs(std::sin(a * Terrain::kMtnFreq2 + spec.phaseSeed * Terrain::kMtnPhaseMul2));
			const float w3 = 1.0f - std::fabs(std::cos(a * Terrain::kMtnFreq3 - spec.phaseSeed * Terrain::kMtnPhaseMul3));
			const float w4 = 1.0f - std::fabs(std::sin(a * Terrain::kMtnFreq4 + spec.phaseSeed * Terrain::kMtnPhaseMul4));
			const float w5 = 0.5f + 0.5f * std::cos(a * Terrain::kMtnFreq5 - spec.phaseSeed * Terrain::kMtnPhaseMul5);
			const float hNorm = std::clamp(
				w1 * Terrain::kMtnWeight1 + w2 * Terrain::kMtnWeight2 + w3 * Terrain::kMtnWeight3 +
				w4 * Terrain::kMtnWeight4 + w5 * Terrain::kMtnWeight5, 0.0f, 1.0f);
			return spec.minPeakH + (spec.maxPeakH - spec.minPeakH) * hNorm;
		};

		const float innerR  = spec.radius * Terrain::kMtnInnerRadiusRatio;
		const float outerR  = spec.radius * Terrain::kMtnOuterRadiusRatio;
		const float invSegs = 1.0f / static_cast<float>(spec.segments);
		const Vector3 nUp   = { 0.0f, 1.0f, 0.0f };

		float a0 = 0.0f;
		float h0 = EvalCrest(a0);
		float c0 = std::cos(a0);
		float s0 = std::sin(a0);

		for (int i = 0; i < spec.segments; ++i)
		{
			const float a1 = static_cast<float>(i + 1) * invSegs * kTwoPi;
			const float h1 = EvalCrest(a1);
			const float c1 = std::cos(a1);
			const float s1 = std::sin(a1);

			const Vector3 pIn0  = { c0 * innerR,      spec.bottomY, s0 * innerR };
			const Vector3 pIn1  = { c1 * innerR,      spec.bottomY, s1 * innerR };
			const Vector3 pTop0 = { c0 * spec.radius, h0,           s0 * spec.radius };
			const Vector3 pTop1 = { c1 * spec.radius, h1,           s1 * spec.radius };
			const Vector3 pOut0 = { c0 * outerR,      spec.bottomY, s0 * outerR };
			const Vector3 pOut1 = { c1 * outerR,      spec.bottomY, s1 * outerR };
			const Vector3 nIn0  = { -c0 * Terrain::kMtnSlopeNormalXZ, Terrain::kMtnSlopeNormalY, -s0 * Terrain::kMtnSlopeNormalXZ };
			const Vector3 nIn1  = { -c1 * Terrain::kMtnSlopeNormalXZ, Terrain::kMtnSlopeNormalY, -s1 * Terrain::kMtnSlopeNormalXZ };

			// 内側斜面: 中心カメラ側フロント面 + 背面保護の両面インデックス (Double-Sided Inner Slope)
			const uint32_t b = static_cast<uint32_t>(data.vertices.size());
			data.vertices.push_back({ { pIn0.x,  pIn0.y,  pIn0.z,  1.0f }, { 0.0f, 1.0f }, nIn0 });
			data.vertices.push_back({ { pIn1.x,  pIn1.y,  pIn1.z,  1.0f }, { 1.0f, 1.0f }, nIn1 });
			data.vertices.push_back({ { pTop1.x, pTop1.y, pTop1.z, 1.0f }, { 1.0f, 0.0f }, nUp  });
			data.vertices.push_back({ { pTop0.x, pTop0.y, pTop0.z, 1.0f }, { 0.0f, 0.0f }, nUp  });
			for (uint32_t triIdx : Meshes::kQuadReverseTriIndices) data.indices.push_back(b + triIdx);
			for (uint32_t triIdx : Meshes::kQuadTriIndices)        data.indices.push_back(b + triIdx);

			// 外側背面スロープ (Outer Back Slope)
			const uint32_t o = static_cast<uint32_t>(data.vertices.size());
			data.vertices.push_back({ { pTop0.x, pTop0.y, pTop0.z, 1.0f }, { 0.0f, 0.0f }, nUp });
			data.vertices.push_back({ { pTop1.x, pTop1.y, pTop1.z, 1.0f }, { 1.0f, 0.0f }, nUp });
			data.vertices.push_back({ { pOut1.x, pOut1.y, pOut1.z, 1.0f }, { 1.0f, 1.0f }, nUp });
			data.vertices.push_back({ { pOut0.x, pOut0.y, pOut0.z, 1.0f }, { 0.0f, 1.0f }, nUp });
			for (uint32_t triIdx : Meshes::kQuadTriIndices) data.indices.push_back(o + triIdx);

			a0 = a1; h0 = h1; c0 = c1; s0 = s1;
		}
		data.boundingRadius = outerR + Terrain::kMtnBoundingMargin;
	}

	// 周期 tileLen で完全シームレスに繋がる蛇行河川の中心X座標 (Seamless Meandering River Center)
	float EvalValleyRiverCenterX(float localZ, float tileLen)
	{
		const float phase = (localZ / tileLen) * kTwoPi;
		return std::sin(phase) * Terrain::kRiverMeanderAmp1 +
		       std::cos(phase * Terrain::kRiverMeanderFreq2 + Terrain::kRiverMeanderPhase2) * Terrain::kRiverMeanderAmp2;
	}

	// 周期 tileLen で完全C1連続に繋がる渓谷地形ハイトマップ (Seamless Valley Heightmap)
	float EvalValleyTerrainHeight(float localX, float localZ, float tileLen)
	{
		const float phase     = (localZ / tileLen) * kTwoPi;
		const float distRiver = std::fabs(localX - EvalValleyRiverCenterX(localZ, tileLen));
		if (distRiver < Terrain::kRiverBedHalfWidth) return Terrain::kRiverBedDepth;

		float bankBlend = std::clamp((distRiver - Terrain::kRiverBedHalfWidth) / Terrain::kRiverBankBlendWidth, 0.0f, 1.0f);
		bankBlend = bankBlend * bankBlend * (Common::kSmoothstepC1 - 2.0f * bankBlend);

		const float terraceSlope = std::clamp((distRiver - Terrain::kTerraceStartDist) / Terrain::kTerraceSpan, 0.0f, 1.0f) * Terrain::kTerraceRise;
		const float outerDist    = (std::max)(0.0f, distRiver - Terrain::kOuterSlopeStartDist);
		float outerRise          = std::clamp(outerDist / Terrain::kOuterSlopeSpan, 0.0f, 1.0f);
		outerRise *= outerRise;

		const float gentleWave =
			std::sin(localX * Terrain::kGentleWaveFreq1 + phase) * Terrain::kGentleWaveAmp1 +
			(0.5f + 0.5f * std::cos(localX * Terrain::kGentleWaveFreq2 - phase)) * (outerRise * Terrain::kGentleWaveOuterAmp);

		return Terrain::kRiverBedDepth * (1.0f - bankBlend) +
		       (terraceSlope + outerRise * Terrain::kOuterSlopeRise + gentleWave) * bankBlend;
	}

	// 実描画渓谷メッシュ上の正確な三角形補間高さを取得 (Valley Mesh Height Sampler)
	float SampleValleyMeshHeight(float localX, float localZ, float width, float tileLen)
	{
		const float halfW = width * 0.5f;
		const float halfL = tileLen * 0.5f;
		const float gx = std::clamp((localX + halfW) / width,   0.0f, Common::kGridClampMax) * static_cast<float>(Terrain::kValleyGridNX);
		const float gz = std::clamp((localZ + halfL) / tileLen, 0.0f, Common::kGridClampMax) * static_cast<float>(Terrain::kValleyGridNZ);

		const int ix = (std::min)(static_cast<int>(gx), Terrain::kValleyGridNX - 1);
		const int iz = (std::min)(static_cast<int>(gz), Terrain::kValleyGridNZ - 1);
		const float fx = gx - static_cast<float>(ix);
		const float fz = gz - static_cast<float>(iz);

		const float x0 = -halfW + (static_cast<float>(ix)     / static_cast<float>(Terrain::kValleyGridNX)) * width;
		const float x1 = -halfW + (static_cast<float>(ix + 1) / static_cast<float>(Terrain::kValleyGridNX)) * width;
		const float z0 = -halfL + (static_cast<float>(iz)     / static_cast<float>(Terrain::kValleyGridNZ)) * tileLen;
		const float z1 = -halfL + (static_cast<float>(iz + 1) / static_cast<float>(Terrain::kValleyGridNZ)) * tileLen;

		const float y00 = EvalValleyTerrainHeight(x0, z0, tileLen);
		const float y10 = EvalValleyTerrainHeight(x1, z0, tileLen);
		const float y11 = EvalValleyTerrainHeight(x1, z1, tileLen);
		const float y01 = EvalValleyTerrainHeight(x0, z1, tileLen);

		return (fx >= fz)
			? (y00 + (y10 - y00) * fx + (y11 - y10) * fz)
			: (y00 + (y11 - y01) * fx + (y01 - y00) * fz);
	}

	void BuildAerialValleyTileModel(Object3d::ModelData& data, float width, float tileLen)
	{
		data.vertices.clear();
		data.indices.clear();

		const int nx = Terrain::kValleyGridNX;
		const int nz = Terrain::kValleyGridNZ;
		const float halfW = width * 0.5f;
		const float halfL = tileLen * 0.5f;
		const float eps   = Terrain::kValleyNormalEps;

		data.vertices.reserve(static_cast<size_t>((nx + 1) * (nz + 1)));
		data.indices.reserve(static_cast<size_t>(nx * nz) * Meshes::kQuadIndexCount);

		for (int iz = 0; iz <= nz; ++iz)
		{
			const float z = -halfL + (static_cast<float>(iz) / static_cast<float>(nz)) * tileLen;
			for (int ix = 0; ix <= nx; ++ix)
			{
				const float x   = -halfW + (static_cast<float>(ix) / static_cast<float>(nx)) * width;
				const float y   = EvalValleyTerrainHeight(x, z, tileLen);
				const float hx0 = EvalValleyTerrainHeight(x - eps, z, tileLen);
				const float hx1 = EvalValleyTerrainHeight(x + eps, z, tileLen);
				const float hz0 = EvalValleyTerrainHeight(x, z - eps, tileLen);
				const float hz1 = EvalValleyTerrainHeight(x, z + eps, tileLen);

				Vector3 n = { hx0 - hx1, 2.0f * eps, hz0 - hz1 };
				const float nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
				if (nl > Common::kNormalMinLen) n = { n.x / nl, n.y / nl, n.z / nl };

				const Vector2 uv = { (x + halfW) * Terrain::kValleyUvScale, (z + halfL) * Terrain::kValleyUvScale };
				data.vertices.push_back({ { x, y, z, 1.0f }, uv, n });
			}
		}

		const int stride = nx + 1;
		for (int iz = 0; iz < nz; ++iz)
		{
			for (int ix = 0; ix < nx; ++ix)
			{
				const uint32_t v00 = static_cast<uint32_t>(iz * stride + ix);
				const uint32_t v10 = static_cast<uint32_t>(iz * stride + (ix + 1));
				const uint32_t v11 = static_cast<uint32_t>((iz + 1) * stride + (ix + 1));
				const uint32_t v01 = static_cast<uint32_t>((iz + 1) * stride + ix);
				data.indices.insert(data.indices.end(), { v00, v10, v11, v00, v11, v01 });
			}
		}
		data.boundingRadius = Terrain::kValleyBoundingRadius;
	}

	void BuildAerialRiverTileModel(Object3d::ModelData& data, float tileLen)
	{
		data.vertices.clear();
		data.indices.clear();

		const int segs    = Terrain::kRiverRibbonSegs;
		const float halfL = tileLen * 0.5f;
		const Vector3 nUp = { 0.0f, 1.0f, 0.0f };

		data.vertices.reserve(static_cast<size_t>((segs + 1) * Terrain::kRiverVertsPerRow));
		data.indices.reserve(static_cast<size_t>(segs) * Meshes::kQuadIndexCount);

		for (int i = 0; i <= segs; ++i)
		{
			const float t      = static_cast<float>(i) / static_cast<float>(segs);
			const float z      = -halfL + t * tileLen;
			const float cx     = EvalValleyRiverCenterX(z, tileLen);
			const float vCoord = t * Terrain::kRiverUvRepeatV;

			data.vertices.push_back({ { cx - Terrain::kRiverHalfWidth, Terrain::kRiverSurfaceY, z, 1.0f }, { 0.0f, vCoord }, nUp });
			data.vertices.push_back({ { cx + Terrain::kRiverHalfWidth, Terrain::kRiverSurfaceY, z, 1.0f }, { 1.0f, vCoord }, nUp });
		}

		for (int i = 0; i < segs; ++i)
		{
			const uint32_t v00 = static_cast<uint32_t>(i * Terrain::kRiverVertsPerRow);
			const uint32_t v10 = v00 + 1;
			const uint32_t v01 = static_cast<uint32_t>((i + 1) * Terrain::kRiverVertsPerRow);
			const uint32_t v11 = v01 + 1;
			data.indices.insert(data.indices.end(), { v00, v10, v11, v00, v11, v01 });
		}
		data.boundingRadius = Terrain::kRiverBoundingRadius;
	}

	void BuildSmokeFlareCanisterModel(Object3d::ModelData& data)
	{
		data.vertices.clear();
		data.indices.clear();

		const int segs      = Meshes::kFlareCylinderSegs;
		const float radius  = Meshes::kFlareCylinderRadius;
		const float invSegs = 1.0f / static_cast<float>(segs);

		for (const auto& band : Meshes::kFlareCylinderBands)
		{
			const float r = radius * band.radiusScale;
			for (int i = 0; i < segs; ++i)
			{
				const float a0 = static_cast<float>(i) * invSegs * kTwoPi;
				const float a1 = static_cast<float>(i + 1) * invSegs * kTwoPi;
				const float x0 = std::cos(a0) * r, z0 = std::sin(a0) * r;
				const float x1 = std::cos(a1) * r, z1 = std::sin(a1) * r;
				Vector3 norm = { (x0 + x1) * 0.5f, 0.0f, (z0 + z1) * 0.5f };
				const float nlen = std::sqrt(norm.x * norm.x + norm.z * norm.z);
				if (nlen > Common::kCylinderNormalMinLen) norm = { norm.x / nlen, 0.0f, norm.z / nlen };

				const uint32_t b = static_cast<uint32_t>(data.vertices.size());
				data.vertices.push_back({ { x0, band.y0, z0, 1.0f }, band.uv, norm });
				data.vertices.push_back({ { x1, band.y0, z1, 1.0f }, band.uv, norm });
				data.vertices.push_back({ { x1, band.y1, z1, 1.0f }, band.uv, norm });
				data.vertices.push_back({ { x0, band.y1, z0, 1.0f }, band.uv, norm });
				for (uint32_t triIdx : Meshes::kQuadTriIndices) data.indices.push_back(b + triIdx);
			}
		}

		const Vector3 nDown = { 0.0f, -1.0f, 0.0f };
		const Vector3 nUp   = { 0.0f,  1.0f, 0.0f };
		const float topY    = Meshes::kFlareCylinderTopY;
		for (int i = 0; i < segs; ++i)
		{
			const float a0 = static_cast<float>(i) * invSegs * kTwoPi;
			const float a1 = static_cast<float>(i + 1) * invSegs * kTwoPi;
			const float x0 = std::cos(a0) * radius, z0 = std::sin(a0) * radius;
			const float x1 = std::cos(a1) * radius, z1 = std::sin(a1) * radius;

			const uint32_t b = static_cast<uint32_t>(data.vertices.size());
			data.vertices.push_back({ { 0.0f, 0.0f, 0.0f, 1.0f }, Meshes::kUvFlareMetal, nDown });
			data.vertices.push_back({ { x0,   0.0f, z0,   1.0f }, Meshes::kUvFlareMetal, nDown });
			data.vertices.push_back({ { x1,   0.0f, z1,   1.0f }, Meshes::kUvFlareMetal, nDown });
			for (uint32_t triIdx : Meshes::kCapBottomTriIndices) data.indices.push_back(b + triIdx);

			const uint32_t t = static_cast<uint32_t>(data.vertices.size());
			data.vertices.push_back({ { 0.0f, topY, 0.0f, 1.0f }, Meshes::kUvFlareMetal, nUp });
			data.vertices.push_back({ { x0,   topY, z0,   1.0f }, Meshes::kUvFlareMetal, nUp });
			data.vertices.push_back({ { x1,   topY, z1,   1.0f }, Meshes::kUvFlareMetal, nUp });
			for (uint32_t triIdx : Meshes::kCapTopTriIndices) data.indices.push_back(t + triIdx);
		}

		for (const auto& box : Meshes::kFlareCanisterBoxes) AddOrientedBox(data, box);
		data.boundingRadius = Meshes::kFlareBoundingRadius;
	}

	// 仕様テーブルから AppParticle インスタンスを統一生成 (Particle Builder)
	AppParticle BuildParticleFromSpec(
		const Particles::ParticleEmitSpec& spec, const Vector3& basePos,
		const Vector3& dirVec, uint32_t textureIndex)
	{
		AppParticle p{};
		p.transform.Initialize();
		p.transform.SetTranslate(basePos + SampleRange3D(spec.posJitter));

		const float sc = SampleRange(spec.scale);
		const float ax = SampleRange(spec.aspectX);
		const float ay = SampleRange(spec.aspectY);
		p.scaleAspect = { ax, ay };
		p.transform.SetScale({ sc * ax, sc * ay, 1.0f });

		p.velocity = dirVec * SampleRange(spec.dirSpeed) + SampleRange3D(spec.velJitter);
		p.color    = { spec.colorRgb.x, spec.colorRgb.y, spec.colorRgb.z, SampleRange(spec.alpha) };
		p.endColor = spec.endColor;
		p.hasColorShift = true;

		p.lifeTime         = SampleRange(spec.lifeTime);
		p.currentTime      = 0.0f;
		p.textureIndex     = textureIndex;
		p.drag             = spec.drag;
		p.gravity          = spec.gravity;
		p.bounceElasticity = spec.bounceElasticity;

		p.initialScale = sc * spec.initScaleMul;
		const float tgt = SampleRange(spec.targetScale);
		p.targetScale  = spec.targetRelativeToSc ? (sc * tgt) : tgt;

		p.curlFreq = SampleRange(spec.curlFreq);
		p.curlAmp  = SampleRange(spec.curlAmp);
		if (spec.wobbleAmp.maxVal > 0.0f)
		{
			p.wobbleFreq = SampleRange(spec.wobbleFreq);
			p.wobbleAmp  = SampleRange(spec.wobbleAmp);
			p.wobbleAxis = spec.wobbleAxis;
		}
		p.fadeInRatio = spec.fadeInRatio;
		p.slotLayer   = spec.slotLayer;
		return p;
	}

	Vector3 SampleTurbineNozzleWorldPos(const Vector3& heliPos, const Matrix4x4& heliRotMat)
	{
		const float sideSign = ((std::rand() % Particles::kTwinEngineCount) == 0) ? -1.0f : 1.0f;
		const Vector3 localNozzle = {
			sideSign * Choreography::kTurbineNozzleLocal.x,
			Choreography::kTurbineNozzleLocal.y,
			Choreography::kTurbineNozzleLocal.z
		};
		return TransformLocalOffset(heliPos, heliRotMat, localNozzle, Choreography::kHeliScale);
	}

	uint32_t WithAlpha(uint32_t col, float a)
	{
		a = std::clamp(a, 0.0f, 1.0f);
		const uint32_t baseA = (col >> UI_COL32_A_SHIFT) & Common::kByteMask;
		const uint32_t newA  = static_cast<uint32_t>(static_cast<float>(baseA) * a);
		return (col & ~UI_COL32_A_MASK) | (newA << UI_COL32_A_SHIFT);
	}

	uint32_t LerpColor(uint32_t c0, uint32_t c1, float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		auto LerpChan = [&](int shift) -> int {
			const int v0 = static_cast<int>((c0 >> shift) & Common::kByteMask);
			const int v1 = static_cast<int>((c1 >> shift) & Common::kByteMask);
			return static_cast<int>(static_cast<float>(v0) + static_cast<float>(v1 - v0) * t);
		};
		return UI_COL32(LerpChan(UI_COL32_R_SHIFT), LerpChan(UI_COL32_G_SHIFT), LerpChan(UI_COL32_B_SHIFT), LerpChan(UI_COL32_A_SHIFT));
	}

	void DrawAtlasSprite(
		TacticalDrawList* dl, UITextureID tex, float atlasW, float atlasH,
		float srcX0, float srcY0, float srcX1, float srcY1,
		UIVec2 dstMin, UIVec2 dstMax, uint32_t tint = UI_COL32_WHITE)
	{
		if (!tex || ((tint >> UI_COL32_A_SHIFT) & Common::kByteMask) == 0) return;
		dl->AddImage(tex, dstMin, dstMax, UIVec2(srcX0 / atlasW, srcY0 / atlasH), UIVec2(srcX1 / atlasW, srcY1 / atlasH), tint);
	}

	bool LookupDigitGlyph(char ch, int& outCol, int& outRow, float& outAdvRatio)
	{
		if (ch >= '0' && ch <= '9')
		{
			outRow = 0; outCol = ch - '0'; outAdvRatio = UI::kDigitNumericAdvance;
			return true;
		}
		for (const auto& g : UI::kDigitGlyphTable)
		{
			if (g.ch == ch) { outRow = g.row; outCol = g.col; outAdvRatio = g.advanceRatio; return true; }
		}
		outRow = -1; outCol = -1; outAdvRatio = UI::kDigitFallbackAdvance;
		return false;
	}

	void DrawFlatDigits(
		TacticalDrawList* dl, UITextureID digitsTex, const char* text,
		float anchorX, float centerY, float height, uint32_t flatCol, int align = UI::kAlignLeft)
	{
		if (!digitsTex || !text || !text[0]) return;
		const uint32_t alphaByte = (flatCol >> UI_COL32_A_SHIFT) & Common::kByteMask;
		if (alphaByte == 0) return;

		float totalW = 0.0f;
		for (const char* p = text; *p; ++p)
		{
			int c = 0, r = 0;
			float adv = UI::kDigitFallbackAdvance;
			LookupDigitGlyph(*p, c, r, adv);
			totalW += adv * height;
		}

		float startX = anchorX;
		if (align == UI::kAlignCenter)     startX = anchorX - totalW * 0.5f;
		else if (align == UI::kAlignRight) startX = anchorX - totalW;

		const float quadW = height * UI::kDigitCellAspect;
		const float topY  = centerY - height * 0.5f;
		const uint32_t shadowCol = UI_COL32(0, 0, 0, static_cast<int>(static_cast<float>(alphaByte) * UI::kDigitShadowAlphaMul));
		float penX = startX;

		for (const char* p = text; *p; ++p)
		{
			int col = 0, row = 0;
			float advRatio = UI::kDigitFallbackAdvance;
			const bool hasGlyph = LookupDigitGlyph(*p, col, row, advRatio);
			const float adv = advRatio * height;
			if (hasGlyph)
			{
				const float gx0 = penX + adv * 0.5f - quadW * 0.5f;
				const float u0  = (static_cast<float>(col) + UI::kDigitUvInsetMin) / UI::kDigitAtlasCols;
				const float u1  = (static_cast<float>(col) + UI::kDigitUvInsetMax) / UI::kDigitAtlasCols;
				const float v0  = (static_cast<float>(row) + UI::kDigitUvInsetMin) / UI::kDigitAtlasRows;
				const float v1  = (static_cast<float>(row) + UI::kDigitUvInsetMax) / UI::kDigitAtlasRows;

				dl->AddImage(digitsTex, UIVec2(gx0, topY + UI::kDigitShadowOffsetY), UIVec2(gx0 + quadW, topY + height + UI::kDigitShadowOffsetY), UIVec2(u0, v0), UIVec2(u1, v1), shadowCol);
				dl->AddImage(digitsTex, UIVec2(gx0, topY),                           UIVec2(gx0 + quadW, topY + height),                           UIVec2(u0, v0), UIVec2(u1, v1), flatCol);
			}
			penX += adv;
		}
	}

	struct DebriefSummary
	{
		std::array<int, UI::kRewardRowCount> rowAmounts{};
		int lootBonus   = 0;
		int totalPayout = 0;
		int accuracyPct = 0;
		int rankIdx     = 0;
	};

	DebriefSummary ComputeDebriefSummary(const RaidStats& stats)
	{
		DebriefSummary out{};
		const int baseExfilBonus = Scoring::kBaseExfilBonus;
		const int killBonus      = stats.enemiesKilled * Scoring::kKillBonusPerEnemy;
		const int targetBonus    = stats.targetsDestroyed * Scoring::kTargetBonusPerUnit;
		const float maxRaidSec   = (stats.maxRaidTime > 1.0f) ? stats.maxRaidTime : Scoring::kFallbackMaxRaidSec;
		const float remRaidSec   = (std::max)(0.0f, maxRaidSec - stats.raidTime);
		const int timeBonus      = static_cast<int>(remRaidSec * Scoring::kTimeBonusPerSec);

		out.accuracyPct = (stats.shotsFired > 0)
			? std::clamp(static_cast<int>(std::round((static_cast<float>(stats.shotsHit) / static_cast<float>(stats.shotsFired)) * Scoring::kPercentScaleF)), 0, Scoring::kPercentScale)
			: Scoring::kPercentScale;

		const float hpRatio       = std::clamp(stats.remainingHp / (std::max)(1.0f, stats.maxHp), 0.0f, 1.0f);
		const int efficiencyBonus = out.accuracyPct * Scoring::kAccuracyBonusPerPct +
		                            static_cast<int>(std::round(hpRatio * Scoring::kHpBonusSteps)) * Scoring::kHpBonusPerStep;
		out.lootBonus   = (std::max)(0, stats.totalLootValue);
		out.rowAmounts  = { baseExfilBonus, killBonus, targetBonus, timeBonus, efficiencyBonus, out.lootBonus };
		out.totalPayout = baseExfilBonus + killBonus + targetBonus + timeBonus + efficiencyBonus + out.lootBonus;

		int rankScore = 0;
		rankScore += (stats.enemiesKilled >= Scoring::kKillFullThreshold) ? Scoring::kKillFullPoints : (stats.enemiesKilled * Scoring::kKillPointsEach);
		rankScore += (stats.totalTargets > 0 && stats.targetsDestroyed >= stats.totalTargets) ? Scoring::kTargetAllPoints : (stats.targetsDestroyed * Scoring::kTargetPointsEach);
		rankScore += (remRaidSec >= Scoring::kRemTimeFastSec) ? Scoring::kRemTimeFastPoints : ((remRaidSec >= Scoring::kRemTimeMidSec) ? Scoring::kRemTimeMidPoints : Scoring::kRemTimeBasePoints);
		rankScore += (out.accuracyPct >= Scoring::kAccHighPct) ? Scoring::kAccHighPoints : ((out.accuracyPct >= Scoring::kAccMidPct) ? Scoring::kAccMidPoints : 0);
		rankScore += (hpRatio >= Scoring::kHpHighRatio) ? Scoring::kHpHighPoints : ((hpRatio >= Scoring::kHpMidRatio) ? Scoring::kHpMidPoints : 0);
		if (stats.lootedGoldDucks > 0) rankScore += Scoring::kGoldDuckBonusPoints;

		out.rankIdx = (rankScore >= Scoring::kGradeThresholdS) ? Scoring::kGradeIndexS
			: ((rankScore >= Scoring::kGradeThresholdA) ? Scoring::kGradeIndexA
			: ((rankScore >= Scoring::kGradeThresholdB) ? Scoring::kGradeIndexB : Scoring::kGradeIndexC));
		return out;
	}
}

// -----------------------------------------------------------------------------
// シーン初期化 / 終了 (Lifecycle)
// -----------------------------------------------------------------------------

void ClearScene::InitializeScene()
{
	ShowCursor(TRUE);
	if (DirectXCom* dx = GetDirectXCom())
	{
		input_ = new KeyInput();
		input_->Initialize(dx->GetWindowAPI());
	}

	raidStats_ = GetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	if (!raidStats_.isSurvived &&
		raidStats_.raidTime < FallbackStats::kEmptyRaidTimeThreshold &&
		raidStats_.enemiesKilled == 0 &&
		raidStats_.totalLootValue == 0)
	{
		raidStats_.isSurvived       = true;
		raidStats_.raidTime         = FallbackStats::kRaidTime;
		raidStats_.maxRaidTime      = FallbackStats::kMaxRaidTime;
		raidStats_.enemiesKilled    = FallbackStats::kEnemiesKilled;
		raidStats_.targetsDestroyed = FallbackStats::kTargetsDestroyed;
		raidStats_.totalTargets     = FallbackStats::kTotalTargets;
		raidStats_.shotsFired       = FallbackStats::kShotsFired;
		raidStats_.shotsHit         = FallbackStats::kShotsHit;
		raidStats_.remainingHp      = FallbackStats::kRemainingHp;
		raidStats_.maxHp            = FallbackStats::kMaxHp;
		raidStats_.damageTakenCount = FallbackStats::kDamageTakenCount;
		raidStats_.lootedGoldDucks  = FallbackStats::kLootedGoldDucks;
		raidStats_.lootedCashCount  = FallbackStats::kLootedCashCount;
		raidStats_.lootedCashValue  = FallbackStats::kLootedCashValue;
		raidStats_.lootedMedkits    = FallbackStats::kLootedMedkits;
		raidStats_.lootedAmmoBoxes  = FallbackStats::kLootedAmmoBoxes;
		raidStats_.totalLootValue   = FallbackStats::kTotalLootValue;
	}
	if (raidStats_.maxHp <= 0.0f) raidStats_.maxHp = FallbackStats::kMaxHp;

	state_                = State::Inbound;
	timer_                = 0.0f;
	actionTimer_          = 0.0f;
	transitionRequested_  = false;
	nextScene_            = "GAMEPLAY";
	doorSlideProgress_    = 1.0f;
	countUpProgress_      = 0.0f;
	resultCutFlash_       = 0.0f;
	rankRevealed_         = false;
	signalFlareEmitTimer_ = 0.0f;
	rotorDustEmitTimer_   = 0.0f;
	turbineExhaustTimer_  = 0.0f;
	bgPlumeEmitTimer_     = 0.0f;
	aerialCloudEmitTimer_ = 0.0f;
	aerialWindEmitTimer_  = 0.0f;
	aerialScrollOffset_   = 0.0f;
	nextRaidHoverAnim_    = 0.0f;
	toTitleHoverAnim_     = 0.0f;
	doorOpenPlayed_       = false;
	touchdownPlayed_      = false;
	boardSoundPlayed_     = false;
	takeoffSoundPlayed_   = false;
	prevRunStep_          = -1;
	prevHoverNext_        = false;
	prevHoverTitle_       = false;

	playerPos_      = kInitialPlayerPos;
	playerStartPos_ = playerPos_;

	InitializeDiorama();

	if (AudioManager* am = GetAudioManager())
	{
		duckSeSoundId_ = am->Load("Resources/Audio/SE_Duck.mp3");
	}
}

void ClearScene::Finalize()
{
	bgmLoop_.Stop();
	rotorLoop_.Stop();
	flareLoop_.Stop();

	delete input_;
	input_ = nullptr;

	heliBody_.reset();
	heliCabinInterior_.reset();
	heliMainRotor_.reset();
	heliMainRotorBlur1_.reset();
	heliMainRotorBlur2_.reset();
	heliTailRotor_.reset();
	heliCargoDoor_.reset();
	helipadModel_.reset();
	duckPlayer_.reset();
	duckRifle_.reset();
	mountainFar_.reset();
	mountainMid_.reset();
	mountainNear_.reset();
	groundModel_.reset();
	signalFlareModel_.reset();
	smokeParticleManager_.reset();
	glintParticleManager_.reset();
	props_.clear();

	for (size_t i = 0; i < kAerialTileCount; ++i)
	{
		aerialValleyTiles_[i].reset();
		aerialRiverTiles_[i].reset();
		aerialForestTiles_[i].reset();
		aerialOutpostTiles_[i].reset();
	}
	for (auto& eh : escortHelis_)
	{
		eh.body.reset();
		eh.mainRotor.reset();
		eh.mainRotorBlur.reset();
		eh.tailRotor.reset();
	}
}

void ClearScene::EnterResultState()
{
	flareLoop_.Stop();
	state_                = State::Result;
	actionTimer_          = 0.0f;
	countUpProgress_      = 0.0f;
	resultCutFlash_       = 1.0f;
	rankRevealed_         = false;
	turbineExhaustTimer_  = 0.0f;
	bgPlumeEmitTimer_     = 0.0f;
	aerialCloudEmitTimer_ = 0.0f;
	aerialWindEmitTimer_  = 0.0f;
	aerialScrollOffset_   = Terrain::kAerialInitialScroll;

	if (smokeParticleManager_) smokeParticleManager_->ClearParticles();
	if (glintParticleManager_) glintParticleManager_->ClearParticles();

	// 初回フレームから雲海・渓谷の戦場黒煙・編隊タービン排気が広がるようプリウォーム (Prewarm Aerial Cruise)
	heliPos_ = Choreography::kCruiseBasePos;
	heliRot_ = { Choreography::kCruiseBasePitch, Terrain::kCruiseFlightYaw, Choreography::kCruiseBaseRoll };
	for (size_t i = 0; i < kEscortHeliCount; ++i)
	{
		escortHelis_[i].worldPos = Choreography::kEscortSpecs[i].basePos;
		escortHelis_[i].worldRot = Choreography::kEscortSpecs[i].prewarmRot;
	}

	const float warmDt = Terrain::kResultPrewarmDt;
	for (int step = 0; step < Terrain::kResultPrewarmSteps; ++step)
	{
		UpdateAerialScrollBelt(warmDt * Terrain::kAerialScrollSpeed, false);
		EmitAerialCruiseAtmosphere(warmDt);
		if (smokeParticleManager_) smokeParticleManager_->Update(warmDt, heliPos_);
		if (glintParticleManager_) glintParticleManager_->Update(warmDt, heliPos_);
	}
}

void ClearScene::RequestSceneTransition(const std::string& targetScene)
{
	if (transitionRequested_) return;
	transitionRequested_ = true;
	nextScene_ = targetScene;
	TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", Audio::kVolTransitionClick);
	bgmLoop_.Stop();
	rotorLoop_.Stop();
	flareLoop_.Stop();
	if (StingerTransition::GetInstance()) StingerTransition::GetInstance()->StartTransition(targetScene);
	else if (SceneManager::GetInstance()) SceneManager::GetInstance()->ChangeScene(targetScene);
}

void ClearScene::PlayDuckVoice(float volume)
{
	if (duckSeSoundId_ < 0) return;
	if (AudioManager* am = GetAudioManager()) am->PlayOneShot(duckSeSoundId_, volume);
}

void ClearScene::HandleResultConfirmAction()
{
	if (actionTimer_ < Timeline::kFullRevealSec)
	{
		if (!rankRevealed_) TacticalAudio::Play2D("Resources/Audio/SE_Grade_Stamp.wav", Audio::kVolGradeStamp);
		TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", Audio::kVolRevealClick);
		actionTimer_     = Timeline::kFullRevealSec;
		countUpProgress_ = 1.0f;
		rankRevealed_    = true;
	}
	else
	{
		RequestSceneTransition("GAMEPLAY");
	}
}

// -----------------------------------------------------------------------------
// 3D ジオラマ初期化 (Diorama, Terrain, Props & Particle Initialization)
// -----------------------------------------------------------------------------

void ClearScene::LoadSceneTextures()
{
	TextureManager* tm = TextureManager::GetInstance();
	if (!tm) return;

	duckTexIndex_       = tm->Load("Resources/duck.png");
	heliTexIndex_       = tm->Load("Resources/helicopter_texture.png");
	rifleTexIndex_      = heliTexIndex_;
	helipadTexIndex_    = tm->Load("Resources/extraction_pad.png");
	if (helipadTexIndex_ == UINT32_MAX) helipadTexIndex_ = tm->Load("Resources/circle2.png");
	flareTexIndex_      = tm->Load("Resources/container_military.png");
	wispySmokeTexIndex_ = tm->Load("Resources/clear_smoke_wispy.png");
	smokeTexIndex_      = tm->Load("Resources/smoke_puff_white.png");
	if (wispySmokeTexIndex_ == UINT32_MAX) wispySmokeTexIndex_ = smokeTexIndex_;
	circleTexIndex_     = tm->Load("Resources/circle2.png");
	whiteTexIndex_      = tm->Load("Resources/white.png");
	waterTexIndex_      = tm->Load("Resources/water_river.jpg");
	fenceTexIndex_      = tm->Load("Resources/duckov_fence.png");
	signpostTexIndex_   = tm->Load("Resources/signpost.png");
	pineTexIndex_       = tm->Load("Resources/duckov_pine_tree.png");
	sandbagTexIndex_    = tm->Load("Resources/duckov_sandbag.png");
	barrelTexIndex_     = tm->Load("Resources/duckov_barrel.png");
	crateTexIndex_      = tm->Load("Resources/duckov_crate.png");
	groundTexIndex_     = tm->Load("Resources/grass.png");
	containerTexIndex_  = tm->Load("Resources/container_military.png");
	vignetteTexIndex_   = tm->Load("Resources/title_vignette.png");
	if (vignetteTexIndex_ == UINT32_MAX) vignetteTexIndex_ = tm->Load("Resources/vignette.png");

	clearDigitsTexIndex_       = tm->Load("Resources/clear_digits_atlas.png");
	clearUiAtlasTexIndex_      = tm->Load("Resources/clear_ui_atlas.png");
	clearCutsceneSubsTexIndex_ = tm->Load("Resources/clear_cutscene_subs.png");
}

void ClearScene::InitializeHelicopterModels(Object3dCom* com)
{
	const Vector3 heliScaleVec = { Choreography::kHeliScale, Choreography::kHeliScale, Choreography::kHeliScale };
	const char* heliTexPath    = "Resources/helicopter_texture.png";

	heliBody_ = CreateObjModelInstance(
		com, camera_, "helicopter_body.obj", heliTexPath, heliTexIndex_, Choreography::kHeliBoundingRadius, heliScaleVec);
	heliCabinInterior_ = CreateBoxModelInstance(
		com, camera_, Meshes::kCabinInteriorBoxes, Meshes::kCabinInteriorBoundingRadius, heliTexPath, heliTexIndex_, heliScaleVec, Choreography::kCabinInteriorTint);
	heliMainRotor_ = CreateObjModelInstance(
		com, camera_, "helicopter_rotor.obj", heliTexPath, heliTexIndex_, Choreography::kHeliBoundingRadius, heliScaleVec);
	heliMainRotorBlur1_ = CreateSharedModelInstance(com, camera_, heliMainRotor_.get(), Choreography::kHeliScale);
	heliMainRotorBlur2_ = CreateSharedModelInstance(com, camera_, heliMainRotor_.get(), Choreography::kHeliScale);
	heliTailRotor_ = CreateObjModelInstance(
		com, camera_, "helicopter_tailrotor.obj", heliTexPath, heliTexIndex_, Choreography::kHeliBoundingRadius, heliScaleVec);
	heliCargoDoor_ = CreateBoxModelInstance(
		com, camera_, Meshes::kHeliDoorBoxes, Meshes::kHeliDoorBoundingRadius, heliTexPath, heliTexIndex_, heliScaleVec);

	for (size_t i = 0; i < kEscortHeliCount; ++i)
	{
		const auto& spec = Choreography::kEscortSpecs[i];
		auto& eh         = escortHelis_[i];
		eh.scale         = spec.scale;
		eh.rotorPhase    = static_cast<float>(i) * Choreography::kEscortPhaseStep;
		eh.body          = CreateSharedModelInstance(com, camera_, heliBody_.get(), eh.scale, true, spec.tint);
		eh.mainRotor     = CreateSharedModelInstance(com, camera_, heliMainRotor_.get(), eh.scale, true, spec.tint);
		eh.mainRotorBlur = CreateSharedModelInstance(com, camera_, heliMainRotor_.get(), eh.scale, true, spec.tint);
		eh.tailRotor     = CreateSharedModelInstance(com, camera_, heliTailRotor_.get(), eh.scale, true, spec.tint);
	}
}

void ClearScene::InitializeGroundLzProps(Object3dCom* com, const Object3d::ModelData* propMeshes)
{
	groundModel_ = CreateObjModelInstance(
		com, camera_, "ground.obj", "Resources/grass.png", groundTexIndex_, Props::kGroundBoundingRadius, Props::kGroundScale);
	groundModel_->SetTranslate(Props::kGroundTranslate);

	helipadModel_ = CreateObjModelInstance(
		com, camera_, "plane.obj", "Resources/extraction_pad.png", helipadTexIndex_, Props::kHelipadBoundingRadius, Props::kHelipadScale);
	helipadModel_->SetRotate({ -kHalfPi, 0.0f, 0.0f });
	helipadModel_->SetTranslate({ heliPos_.x, Props::kHelipadSurfaceY, heliPos_.z });

	constexpr size_t kPropKindCount = static_cast<size_t>(Props::LzPropKind::Count);
	const std::array<uint32_t, kPropKindCount> kindTexIndices = {
		pineTexIndex_, sandbagTexIndex_, barrelTexIndex_, crateTexIndex_,
		containerTexIndex_, crateTexIndex_, fenceTexIndex_, signpostTexIndex_
	};
	std::array<Object3d*, kPropKindCount> masterSlots{};

	props_.clear();
	props_.reserve(std::size(Props::kLzFobProps) + std::size(Props::kLzTrees));

	auto SpawnSharedProp = [&](Props::LzPropKind kind, const Vector3& pos, const Vector3& rot, const Vector3& scale, const Vector4& tint) {
		const size_t idx   = static_cast<size_t>(kind);
		const uint32_t tex = kindTexIndices[idx];
		auto obj           = std::make_unique<Object3d>();
		if (!masterSlots[idx])
		{
			Object3d::ModelData d = propMeshes[idx];
			d.material.textureFilePath = Props::kLzPropAssets[idx].texPath;
			d.material.textureIndex    = tex;
			obj->Initialize(com, d);
			masterSlots[idx] = obj.get();
		}
		else
		{
			obj->InitializeShared(com, masterSlots[idx]);
		}
		obj->SetCamera(camera_);
		obj->SetTranslate(pos);
		obj->SetRotate(rot);
		obj->SetScale(scale);
		obj->SetColor(tint);
		obj->SetEnableLighting(true);
		props_.push_back({ std::move(obj), tex });
	};

	for (const auto& fp : Props::kLzFobProps) SpawnSharedProp(fp.kind, fp.pos, fp.rot, fp.scale, fp.tint);
	for (const auto& tp : Props::kLzTrees)
	{
		Vector3 groundedPos = tp.pos;
		groundedPos.y += Props::kLzTreeGroundLiftRatio * tp.scale.y;
		SpawnSharedProp(Props::LzPropKind::Pine, groundedPos, { 0.0f, tp.yaw, 0.0f }, tp.scale, tp.tint);
	}
}

void ClearScene::InitializeAerialCruiseTiles(Object3dCom* com, const Object3d::ModelData* propMeshes)
{
	Object3d::ModelData valleyData{};
	BuildAerialValleyTileModel(valleyData, Terrain::kAerialTileWidth, Terrain::kAerialTileLen);
	valleyData.material.textureFilePath = "Resources/grass.png";
	valleyData.material.textureIndex    = groundTexIndex_;

	Object3d::ModelData riverData{};
	BuildAerialRiverTileModel(riverData, Terrain::kAerialTileLen);
	riverData.material.textureFilePath = "Resources/water_river.jpg";
	riverData.material.textureIndex    = waterTexIndex_;

	for (size_t s = 0; s < kOutpostsPerTile; ++s)
	{
		const float oz = Terrain::kAerialOutpostSites[s].localZ;
		const float ox = EvalValleyRiverCenterX(oz, Terrain::kAerialTileLen) + Terrain::kAerialOutpostSites[s].riverOffsetX;
		const float oy = SampleValleyMeshHeight(ox, oz, Terrain::kAerialTileWidth, Terrain::kAerialTileLen);
		aerialOutpostLocalPos_[s] = { ox, oy, oz };
	}

	// 針葉樹の幹と最下段の枝が斜面へ埋もれないよう、実描画三角形の5点高さを検査して接地 (Batched Valley Forest)
	const Object3d::ModelData& pineData = propMeshes[static_cast<size_t>(Props::LzPropKind::Pine)];
	Object3d::ModelData forestBatch{};
	forestBatch.material.textureFilePath = "Resources/duckov_pine_tree.png";
	forestBatch.material.textureIndex    = pineTexIndex_;
	forestBatch.boundingRadius           = Terrain::kForestBoundingRadius;

	for (int ti = 0; ti < Terrain::kAerialTreeCount; ++ti)
	{
		const float fIdx        = static_cast<float>(ti);
		const float zLocal      = -Terrain::kAerialTileLen * Terrain::kTreeZHalfSpanRatio +
		                          ((fIdx + 0.5f) / static_cast<float>(Terrain::kAerialTreeCount)) * (Terrain::kAerialTileLen * Terrain::kTreeZFullSpanRatio);
		const float rCenter     = EvalValleyRiverCenterX(zLocal, Terrain::kAerialTileLen);
		const float side        = ((ti % Terrain::kAlternateBankDivisor) == 0) ? -1.0f : 1.0f;
		const float groveJitter = std::sin(fIdx * Terrain::kTreeJitterFreq) * Terrain::kTreeJitterAmp;
		const float bankDist    = std::clamp(
			Terrain::kTreeBankDistBase + std::fmod(fIdx * Terrain::kTreeBankDistStep, Terrain::kTreeBankDistMod) + groveJitter,
			Terrain::kTreeBankDistMin, Terrain::kTreeBankDistMax);
		const float xLocal = rCenter + side * bankDist;

		bool nearOutpost = false;
		for (const auto& site : aerialOutpostLocalPos_)
		{
			if (std::fabs(xLocal - site.x) < Terrain::kOutpostClearingHalfExtent &&
				std::fabs(zLocal - site.z) < Terrain::kOutpostClearingHalfExtent)
			{
				nearOutpost = true;
				break;
			}
		}
		if (nearOutpost) continue;

		const float sc    = Terrain::kTreeScaleBase + std::fmod(fIdx * Terrain::kTreeScaleStep, Terrain::kTreeScaleMod);
		const float scY   = sc * Terrain::kTreeScaleYMul;
		const float rFoot = Terrain::kTreeFootRadiusMul * sc;

		float minFootY = SampleValleyMeshHeight(xLocal, zLocal, Terrain::kAerialTileWidth, Terrain::kAerialTileLen);
		float maxFootY = minFootY;
		for (size_t d = 1; d < std::size(Terrain::kTreeFootSampleDirs); ++d)
		{
			const Vector2& dir = Terrain::kTreeFootSampleDirs[d];
			const float fy = SampleValleyMeshHeight(xLocal + dir.x * rFoot, zLocal + dir.y * rFoot, Terrain::kAerialTileWidth, Terrain::kAerialTileLen);
			minFootY = (std::min)(minFootY, fy);
			maxFootY = (std::max)(maxFootY, fy);
		}
		if ((maxFootY - minFootY) > Terrain::kTreeMaxSlopeDeltaY) continue;

		const float yLocal = maxFootY + Terrain::kTreeTrunkLiftRatio * scY;
		const float yaw    = std::fmod(fIdx * Terrain::kTreeYawStep, kTwoPi);
		AppendTransformedMesh(forestBatch, pineData, { xLocal, yLocal, zLocal }, yaw, { sc, scY, sc });
	}

	// 渓谷両岸の2箇所に点在する戦術前哨拠点クラスタ (Batched Valley Outposts)
	Object3d::ModelData outpostBatch{};
	outpostBatch.material.textureFilePath = "Resources/container_military.png";
	outpostBatch.material.textureIndex    = containerTexIndex_;
	outpostBatch.boundingRadius           = Terrain::kOutpostBoundingRadius;

	for (size_t s = 0; s < kOutpostsPerTile; ++s)
	{
		const Vector3& center = aerialOutpostLocalPos_[s];
		const float yawBias   = Terrain::kAerialOutpostSites[s].yawBias;
		for (const auto& piece : Props::kAerialOutpostPieces)
		{
			const float px = center.x + piece.dx;
			const float pz = center.z + piece.dz;
			const float py = SampleValleyMeshHeight(px, pz, Terrain::kAerialTileWidth, Terrain::kAerialTileLen);
			const auto& srcMesh = propMeshes[static_cast<size_t>(piece.kind)];
			AppendTransformedMesh(outpostBatch, srcMesh, { px, py, pz }, piece.yaw + yawBias, { piece.scale, piece.scale, piece.scale });
		}
	}

	auto InitTileSeries = [&](std::array<std::unique_ptr<Object3d>, kAerialTileCount>& tiles, const Object3d::ModelData& data, const Vector4& tint) {
		tiles[0] = CreateModelInstance(com, camera_, data, { 1.0f, 1.0f, 1.0f }, true, tint);
		for (size_t i = 1; i < kAerialTileCount; ++i)
		{
			tiles[i] = CreateSharedModelInstance(com, camera_, tiles[0].get(), 1.0f, true, tint);
		}
	};
	InitTileSeries(aerialValleyTiles_,  valleyData,   Terrain::kValleyTileTint);
	InitTileSeries(aerialRiverTiles_,   riverData,    Terrain::kRiverTileTint);
	InitTileSeries(aerialForestTiles_,  forestBatch,  Terrain::kForestTileTint);
	InitTileSeries(aerialOutpostTiles_, outpostBatch, Terrain::kOutpostTileTint);
}

void ClearScene::InitializeDiorama()
{
	Object3dCom* com = GetObject3dCom();
	if (!com) return;

	if (SceneManager::GetInstance())
	{
		SceneManager::GetInstance()->SetShowSkybox(true);
		if (Light* light = SceneManager::GetInstance()->GetLight())
		{
			light->SetDirectionalLight(Choreography::kInitSunColor, Choreography::kInitSunDir, Choreography::kInitSunIntensity);
		}
	}

	if (!camera_) camera_ = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetCamera() : nullptr;
	if (camera_)
	{
		camera_->SetFarZ(Choreography::kCameraFarZ);
		camera_->SetTranslate(Choreography::kCameraInitPos);
		camera_->SetRotate(Choreography::kCameraInitRot);
		camera_->SetFovY(Choreography::kCameraDefaultFov);
		camera_->Update();
	}

	LoadSceneTextures();
	InitializeHelicopterModels(com);

	// プレイヤー主役アヒル ＆ 携行アサルトライフル (Operator & Rifle)
	duckPlayer_ = CreateObjModelInstance(
		com, camera_, "player.obj", "Resources/duck.png", duckTexIndex_,
		Choreography::kDuckBoundingRadius, { Choreography::kDuckScale, Choreography::kDuckScale, Choreography::kDuckScale });
	duckRifle_ = CreateBoxModelInstance(
		com, camera_, Meshes::kRifleBoxes, Meshes::kRifleBoundingRadius,
		"Resources/helicopter_texture.png", (rifleTexIndex_ != UINT32_MAX) ? rifleTexIndex_ : 0,
		{ 1.0f, 1.0f, 1.0f }, Choreography::kRifleTint);

	// 360度3層山岳パノラマ (3-Layer Panoramic Mountain Rings)
	auto CreateMountainLayer = [&](const Terrain::MountainLayerSpec& spec) {
		Object3d::ModelData mtnData{};
		BuildMountainRingModel(mtnData, spec);
		mtnData.material.textureFilePath = "Resources/white.png";
		mtnData.material.textureIndex    = whiteTexIndex_;
		return CreateModelInstance(com, camera_, mtnData, { 1.0f, 1.0f, 1.0f }, spec.enableLighting, spec.color);
	};
	mountainFar_  = CreateMountainLayer(Terrain::kMountainFarSpec);
	mountainMid_  = CreateMountainLayer(Terrain::kMountainMidSpec);
	mountainNear_ = CreateMountainLayer(Terrain::kMountainNearSpec);

	// 共通プロップOBJを1回ずつ読み込み、地上LZと上空渓谷バッチの双方で共有 (Shared Prop Asset Load)
	constexpr size_t kPropKindCount = static_cast<size_t>(Props::LzPropKind::Count);
	std::array<Object3d::ModelData, kPropKindCount> propMeshes{};
	for (size_t i = 0; i < kPropKindCount; ++i)
	{
		propMeshes[i]                = Object3d::LoadObjFile("Resources", Props::kLzPropAssets[i].objFile);
		propMeshes[i].boundingRadius = Props::kLzPropAssets[i].boundingRadius;
	}

	InitializeGroundLzProps(com, propMeshes.data());
	InitializeAerialCruiseTiles(com, propMeshes.data());

	// 発煙筒スモーク缶 ＆ 2系統パーティクル初期化・地上煙プリウォーム (Flare & Particle Systems)
	Object3d::ModelData flareData{};
	BuildSmokeFlareCanisterModel(flareData);
	flareData.material.textureFilePath = "Resources/container_military.png";
	flareData.material.textureIndex    = flareTexIndex_;
	signalFlareModel_ = CreateModelInstance(com, camera_, flareData, { 1.0f, 1.0f, 1.0f });
	signalFlareModel_->SetTranslate(signalFlarePos_);
	signalFlareModel_->SetRotate(signalFlareRot_);

	auto InitParticleSystem = [&](bool alphaBlend, bool premultiply) {
		auto mgr = std::make_unique<AppParticleManager>();
		mgr->Initialize(GetParticleManager());
		mgr->SetGroundY(0.0f);
		mgr->SetAlphaBlend(alphaBlend);
		mgr->SetPremultiplyAlpha(premultiply);
		mgr->SetStableInstanceSlots(true);
		return mgr;
	};
	smokeParticleManager_ = InitParticleSystem(true, false);
	glintParticleManager_ = InitParticleSystem(false, true);

	heliPos_ = Choreography::kHeliInboundStartPos;
	heliRot_ = Choreography::kHeliInboundStartRot;
	for (int step = 0; step < Props::kLzPrewarmSteps; ++step)
	{
		EmitGroundLzAtmosphere(Props::kLzPrewarmDt, false);
		smokeParticleManager_->Update(Props::kLzPrewarmDt, heliPos_);
		glintParticleManager_->Update(Props::kLzPrewarmDt, heliPos_);
	}
}

// -----------------------------------------------------------------------------
// 更新 ＆ オーディオミキシング (Frame Update & Audio)
// -----------------------------------------------------------------------------

void ClearScene::UpdateAudioLoops(float deltaTime)
{
	if (state_ != State::Result)
	{
		bgmLoop_.Stop();
		float rotorVol = Audio::kRotorVolDefault;
		if (state_ == State::Inbound)
		{
			const float inT = std::clamp(actionTimer_ / kInboundDuration, 0.0f, 1.0f);
			rotorVol = Audio::kRotorVolInboundMin + inT * Audio::kRotorVolInboundAdd;
		}
		else if (state_ == State::Takeoff)
		{
			rotorVol = Audio::kRotorVolTakeoff;
		}
		if (transitionRequested_) rotorVol = 0.0f;
		rotorLoop_.Update("Resources/Audio/SE_Heli_Rotor_Loop.wav", Audio::kRotorLoopSec, rotorVol, deltaTime);

		const float flareVol = transitionRequested_ ? 0.0f : ((state_ == State::Takeoff) ? Audio::kFlareVolTakeoff : Audio::kFlareVolGround);
		flareLoop_.Update("Resources/Audio/SE_Flare_Hiss_Loop.wav", Audio::kFlareLoopSec, flareVol, deltaTime);
	}
	else
	{
		flareLoop_.Stop();
		bgmLoop_.Update("Resources/Audio/BGM_Clear_Debrief.wav", Audio::kBgmLoopSec, transitionRequested_ ? 0.0f : Audio::kBgmVolResult, deltaTime);
		rotorLoop_.Update("Resources/Audio/SE_Heli_Rotor_Loop.wav", Audio::kRotorLoopSec, transitionRequested_ ? 0.0f : Audio::kRotorVolCruise, deltaTime);
	}
}

void ClearScene::Update()
{
	const float deltaTime = Common::kFixedDeltaTime;
	const bool isTransitioning = (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning());

	if (!isTransitioning)
	{
		timer_       += deltaTime;
		actionTimer_ += deltaTime;
	}
	if (resultCutFlash_ > 0.0f)
	{
		resultCutFlash_ = (std::max)(0.0f, resultCutFlash_ - deltaTime * Timeline::kCutFlashDecayRate);
	}

	bool confirm = false;
	bool toTitle = false;
	if (input_ && !isTransitioning)
	{
		input_->Update();
		confirm = input_->TriggerKey(DIK_SPACE) || input_->TriggerKey(DIK_RETURN);
		toTitle = input_->TriggerKey(DIK_T);
	}

	switch (state_)
	{
	case State::Inbound:
	{
		const float inT = std::clamp(actionTimer_ / kInboundDuration, 0.0f, 1.0f);
		if (inT > Timeline::kInboundDoorOpenRatio && !doorOpenPlayed_)
		{
			doorOpenPlayed_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Heli_Door.wav", Audio::kVolHeliDoor);
		}
		if (inT > Timeline::kInboundTouchdownRatio && !touchdownPlayed_)
		{
			touchdownPlayed_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Boot_Land_Heavy.wav", Audio::kVolTouchdown);
		}
		if (timer_ >= Timeline::kSkipInputGuardSec && (confirm || actionTimer_ >= kInboundDuration))
		{
			if (confirm && actionTimer_ < kInboundDuration) TacticalAudio::Play2D("Resources/Audio/SE_UI_Click.wav", Audio::kVolSkipClick);
			state_          = State::RunningToHeli;
			actionTimer_    = 0.0f;
			playerStartPos_ = playerPos_;
			PlayDuckVoice(Audio::kVolDuckRun);
		}
		break;
	}
	case State::RunningToHeli:
		if (timer_ >= Timeline::kSkipInputGuardSec && (confirm || actionTimer_ >= kRunDuration))
		{
			state_       = State::Boarding;
			actionTimer_ = 0.0f;
			if (!boardSoundPlayed_)
			{
				boardSoundPlayed_ = true;
				TacticalAudio::Play2D("Resources/Audio/SE_Dodge_Roll.wav", Audio::kVolBoardRoll);
				TacticalAudio::Play2D("Resources/Audio/SE_Weapon_Holster.wav", Audio::kVolBoardHolster);
			}
			PlayDuckVoice(Audio::kVolDuckBoard);
		}
		break;

	case State::Boarding:
		if (timer_ >= Timeline::kSkipInputGuardSec && (confirm || actionTimer_ >= kBoardDuration))
		{
			state_       = State::Takeoff;
			actionTimer_ = 0.0f;
			if (!takeoffSoundPlayed_)
			{
				takeoffSoundPlayed_ = true;
				TacticalAudio::Play2D("Resources/Audio/SE_Exfil_Complete.wav", Audio::kVolTakeoffExfil);
			}
		}
		break;

	case State::Takeoff:
		if (confirm || actionTimer_ >= kTakeoffDuration) EnterResultState();
		break;

	case State::Result:
	{
		const float rawCount = std::clamp((actionTimer_ - Timeline::kCountUpStartDelaySec) / Timeline::kCountUpDurationSec, 0.0f, 1.0f);
		countUpProgress_ = EaseOutExpo(rawCount);

		if (actionTimer_ >= Timeline::kCountUpStartDelaySec && rawCount < Timeline::kCountTickEndRatio && !transitionRequested_)
		{
			TacticalAudio::Play2D("Resources/Audio/SE_Count_Tick.wav", Audio::kVolCountTick, Audio::kCountTickCooldown, "clear_count_tick");
		}
		if (!rankRevealed_ && actionTimer_ >= Timeline::kGradeRevealSec)
		{
			rankRevealed_ = true;
			TacticalAudio::Play2D("Resources/Audio/SE_Grade_Stamp.wav", Audio::kVolGradeStamp);
			PlayDuckVoice(Audio::kVolDuckGrade);
		}
		if (!transitionRequested_)
		{
			if (toTitle)      RequestSceneTransition("TITLE");
			else if (confirm) HandleResultConfirmAction();
		}
		break;
	}
	}

	UpdateAudioLoops(deltaTime);
	UpdateDiorama(deltaTime);
	DrawScreen();
}

// -----------------------------------------------------------------------------
// リアルな多段ボリューメトリック煙・砂塵・雲海エミッター (Atmosphere Emitters)
// -----------------------------------------------------------------------------

void ClearScene::EmitGroundLzAtmosphere(float deltaTime, bool includeRotorWash)
{
	if (!smokeParticleManager_ || wispySmokeTexIndex_ == UINT32_MAX) return;
	const Vector3 zeroDir   = { 0.0f, 0.0f, 0.0f };
	const Vector3 nozzlePos = signalFlarePos_ + Particles::kFlareNozzleOffset;

	// 1. M18緑シグナル発煙筒: 高速噴出コア + カール乱流メイン煙柱 + 外周ベール煙 (M18 Green Smoke)
	signalFlareEmitTimer_ += deltaTime;
	int emitStepCount = 0;
	while (signalFlareEmitTimer_ >= Particles::kFlareEmitInterval)
	{
		signalFlareEmitTimer_ -= Particles::kFlareEmitInterval;
		++emitStepCount;

		smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecFlareJet, nozzlePos, zeroDir, wispySmokeTexIndex_));
		smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecFlarePlume, nozzlePos, zeroDir, wispySmokeTexIndex_));
		if ((emitStepCount % Particles::kFlareVeilStepStride) == 0)
		{
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecFlareVeil, nozzlePos, zeroDir, wispySmokeTexIndex_));
		}
		if (glintParticleManager_ && circleTexIndex_ != UINT32_MAX && (std::rand() % Particles::kSparkChanceDivisor) == 0)
		{
			glintParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecFlareSpark, nozzlePos, zeroDir, circleTexIndex_));
		}
	}

	// 2. 森林奥のホットゾーンから立ち昇る遠景戦場黒煙 (LZ Background Smoke)
	bgPlumeEmitTimer_ += deltaTime;
	while (bgPlumeEmitTimer_ >= Particles::kLzBgPlumeInterval)
	{
		bgPlumeEmitTimer_ -= Particles::kLzBgPlumeInterval;
		for (const Vector3& site : Particles::kLzBgSmokeSites)
		{
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecLzBgSmoke, site, zeroDir, wispySmokeTexIndex_));
		}
	}

	if (!includeRotorWash) return;

	// 3. ローターダウンウォッシュによる煙柱の渦巻き押し広げ ＆ 地表砂塵リング (Rotor Downwash)
	if (heliPos_.y < Particles::kRotorWashMaxHeliY)
	{
		const float washRatio    = std::clamp(1.0f - (heliPos_.y - Particles::kRotorWashRefHeliY) / Particles::kRotorWashSpanY, 0.0f, 1.0f);
		const float washStrength = Particles::kRotorWashBaseStrength + washRatio * Particles::kRotorWashGainStrength;
		smokeParticleManager_->ApplyRotorDownwash(heliPos_ + Particles::kRotorWashCenterOffset, Particles::kRotorWashRadius, washStrength, deltaTime);

		rotorDustEmitTimer_ += deltaTime;
		while (rotorDustEmitTimer_ >= Particles::kRotorDustInterval)
		{
			rotorDustEmitTimer_ -= Particles::kRotorDustInterval;
			if (washRatio > Particles::kRotorDustMinWashRatio)
			{
				const float ang    = RandRange(0.0f, kTwoPi);
				const float rad    = SampleRange(Particles::kRotorDustSpawnRadius);
				const float dirX   = std::cos(ang);
				const float dirZ   = std::sin(ang);
				const Vector3 ring = { heliPos_.x + dirX * rad, 0.0f, heliPos_.z + dirZ * rad };

				AppParticle dust   = BuildParticleFromSpec(Particles::kSpecRotorDust, ring, zeroDir, wispySmokeTexIndex_);
				const float outSpd = (Particles::kRotorDustOutSpeedBase + Particles::kRotorDustOutSpeedGain * washRatio) * SampleRange(Particles::kRotorDustSpeedMul);
				dust.velocity      = { dirX * outSpd, SampleRange(Particles::kRotorDustUpSpeed) * washRatio, dirZ * outSpd };
				dust.color.w      *= washRatio;
				smokeParticleManager_->AddParticle(dust);
			}
		}
	}

	// 4. 主役ヘリ双発タービン排気スモーク (Ground Turbine Exhaust)
	turbineExhaustTimer_ += deltaTime;
	if (turbineExhaustTimer_ >= Particles::kGroundExhaustInterval)
	{
		const Matrix4x4 heliRotMat = MakeEulerRotationMatrix(heliRot_);
		const Vector3 aftDir       = { -std::sin(heliRot_.y), Particles::kGroundExhaustAftY, -std::cos(heliRot_.y) };
		while (turbineExhaustTimer_ >= Particles::kGroundExhaustInterval)
		{
			turbineExhaustTimer_ -= Particles::kGroundExhaustInterval;
			const Vector3 worldNozzle = SampleTurbineNozzleWorldPos(heliPos_, heliRotMat);
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecGroundExhaust, worldNozzle, aftDir, wispySmokeTexIndex_));
		}
	}
}

void ClearScene::EmitAerialCruiseAtmosphere(float deltaTime)
{
	if (!smokeParticleManager_ || wispySmokeTexIndex_ == UINT32_MAX) return;

	const Vector3 fwd        = { std::sin(Terrain::kCruiseFlightYaw), 0.0f, std::cos(Terrain::kCruiseFlightYaw) };
	const Vector3 slipDir    = { -fwd.x, 0.0f, -fwd.z };
	const Vector3 originZero = { 0.0f, 0.0f, 0.0f };

	// 1. 眼下スクロール渓谷の前哨拠点から立ち昇る戦場スモーク柱 ＆ 残り火グロー (Valley Plumes)
	bgPlumeEmitTimer_ += deltaTime;
	while (bgPlumeEmitTimer_ >= Particles::kAerialPlumeInterval)
	{
		bgPlumeEmitTimer_ -= Particles::kAerialPlumeInterval;
		for (const Vector3& site : aerialOutpostWorldPos_)
		{
			if (site.z < Particles::kAerialOutpostMinZ || site.z > Particles::kAerialOutpostMaxZ) continue;
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecValleyPlume, site, slipDir, wispySmokeTexIndex_));
			if ((std::rand() % Particles::kEmberChanceDivisor) == 0)
			{
				smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecValleyEmber, site, slipDir, wispySmokeTexIndex_));
			}
		}
	}

	// 2. 立体ボリューメトリック雲海バンク (Mid Cumulus & High Ridge Clouds)
	aerialCloudEmitTimer_ += deltaTime;
	while (aerialCloudEmitTimer_ >= Particles::kAerialCloudInterval)
	{
		aerialCloudEmitTimer_ -= Particles::kAerialCloudInterval;
		AppParticle midCloud = BuildParticleFromSpec(Particles::kSpecMidCloud, originZero, slipDir, wispySmokeTexIndex_);
		midCloud.slotLayer = (midCloud.transform.GetTranslate().z > Particles::kMidCloudLayerSplitZ)
			? Particles::kLayerBackground : Particles::kLayerMidground;
		smokeParticleManager_->AddParticle(midCloud);

		if ((std::rand() % Particles::kHighCloudChanceDivisor) == 0)
		{
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecHighCloud, originZero, slipDir, wispySmokeTexIndex_));
		}
	}

	// 3. 主役機 Dustoff-1 ＆ ウイングマン Dustoff-2 の双発タービン排気トレイル (Cruise Exhaust)
	turbineExhaustTimer_ += deltaTime;
	if (turbineExhaustTimer_ >= Particles::kCruiseExhaustInterval)
	{
		const Matrix4x4 heliRotMat = MakeEulerRotationMatrix(heliRot_);
		while (turbineExhaustTimer_ >= Particles::kCruiseExhaustInterval)
		{
			turbineExhaustTimer_ -= Particles::kCruiseExhaustInterval;
			const Vector3 worldNozzle = SampleTurbineNozzleWorldPos(heliPos_, heliRotMat);
			smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecCruiseExhaust, worldNozzle, slipDir, wispySmokeTexIndex_));
			if ((std::rand() % Particles::kWingmanExhaustChanceDivisor) == 0)
			{
				smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecWingmanExhaust, escortHelis_[0].worldPos, slipDir, wispySmokeTexIndex_));
			}
		}
	}

	// 4. 近景スリップストリーム水蒸気ウィスプ ＆ 陽光微細ダスト粒子 (Slipstream & Sun Motes)
	aerialWindEmitTimer_ += deltaTime;
	while (aerialWindEmitTimer_ >= Particles::kAerialWindInterval)
	{
		aerialWindEmitTimer_ -= Particles::kAerialWindInterval;
		const float yBand = ((std::rand() % Particles::kWispBandCount) == 0)
			? SampleRange(Particles::kWispBandLowY) : SampleRange(Particles::kWispBandHighY);
		const Vector3 wispSpawn = heliPos_ + fwd * SampleRange(Particles::kWispForwardDist) +
		                          Vector3{ SampleRange(Particles::kWispOffsetX), yBand, SampleRange(Particles::kWispOffsetZ) };
		smokeParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecSlipstreamWisp, wispSpawn, slipDir, wispySmokeTexIndex_));

		if (glintParticleManager_ && circleTexIndex_ != UINT32_MAX && (std::rand() % Particles::kSunMoteChanceDivisor) == 0)
		{
			const Vector3 moteSpawn = heliPos_ + fwd * SampleRange(Particles::kMoteForwardDist) + SampleRange3D(Particles::kMoteSpawnOffset);
			glintParticleManager_->AddParticle(BuildParticleFromSpec(Particles::kSpecSunMote, moteSpawn, slipDir, circleTexIndex_));
		}
	}
}

// -----------------------------------------------------------------------------
// 3D ジオラマ演出更新 (Diorama Choreography & Formation Update)
// -----------------------------------------------------------------------------

void ClearScene::UpdateHelicopterMotion(float deltaTime)
{
	float mainRotorSpeed = Choreography::kMainRotorSpeedBase;
	float tailRotorSpeed = Choreography::kTailRotorSpeedBase;

	if (state_ == State::Inbound)
	{
		const float t = std::clamp(actionTimer_ / kInboundDuration, 0.0f, 1.0f);
		const float e = EaseOutCubic(t);
		heliPos_ = LerpVec(Choreography::kHeliInboundStartPos, Choreography::kHeliLandPos, e);

		const float flareEnv = std::sin(std::clamp((t - Choreography::kInboundFlareStartT) / Choreography::kInboundFlareSpanT, 0.0f, 1.0f) * kPi);
		const float bankEnv  = 1.0f - e;
		heliRot_ = {
			Choreography::kInboundFlarePitch * flareEnv + Choreography::kInboundBankPitch * bankEnv,
			Choreography::kHeliLandYaw + Choreography::kInboundBankYaw * bankEnv,
			Choreography::kInboundBankRoll * bankEnv
		};
		doorSlideProgress_ = (t > Choreography::kInboundDoorStartT)
			? (1.0f - EaseOutCubic(std::clamp((t - Choreography::kInboundDoorStartT) / Choreography::kInboundDoorSpanT, 0.0f, 1.0f)) * Choreography::kInboundDoorOpenAmount)
			: 1.0f;
		mainRotorSpeed = Choreography::kMainRotorSpeedInbound;
		tailRotorSpeed = Choreography::kTailRotorSpeedInbound;
	}
	else if (state_ == State::RunningToHeli)
	{
		heliPos_ = { Choreography::kHeliLandPos.x, Choreography::kHeliLandPos.y, Choreography::kHeliLandPos.z + std::sin(timer_ * Choreography::kIdleSwayFreq) * Choreography::kIdleSwayAmpZ };
		heliRot_ = { 0.0f, Choreography::kHeliLandYaw, 0.0f };
		doorSlideProgress_ = (std::max)(Choreography::kRunDoorMinSlide, doorSlideProgress_ - deltaTime * Choreography::kRunDoorOpenSpeed);
		mainRotorSpeed = Choreography::kMainRotorSpeedRun;
		tailRotorSpeed = Choreography::kTailRotorSpeedRun;
	}
	else if (state_ == State::Boarding)
	{
		const float settle = std::sin(std::clamp(actionTimer_ / kBoardDuration, 0.0f, 1.0f) * kPi) * Choreography::kBoardSettleAmpY;
		heliPos_ = { Choreography::kHeliLandPos.x, Choreography::kHeliLandPos.y + settle, Choreography::kHeliLandPos.z };
		heliRot_ = { 0.0f, Choreography::kHeliLandYaw, settle * Choreography::kBoardSettleRollMul };
		doorSlideProgress_ = Choreography::kBoardDoorSlide;
		mainRotorSpeed = Choreography::kMainRotorSpeedBoard;
		tailRotorSpeed = Choreography::kTailRotorSpeedBoard;
	}
	else if (state_ == State::Takeoff)
	{
		const float t          = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
		const float spoolCurve = EaseInQuad(std::clamp(t / Choreography::kTakeoffSpoolSpanT, 0.0f, 1.0f));
		mainRotorSpeed     = Choreography::kMainRotorSpeedBoard + spoolCurve * Choreography::kTakeoffMainSpoolGain;
		tailRotorSpeed     = Choreography::kTailRotorSpeedBoard + spoolCurve * Choreography::kTakeoffTailSpoolGain;
		doorSlideProgress_ = Choreography::kTakeoffDoorSlide;

		if (t < Choreography::kTakeoffLiftStartT)
		{
			const float hoverRatio = t / Choreography::kTakeoffLiftStartT;
			const float vib        = std::sin(actionTimer_ * Choreography::kTakeoffVibFreq) * Choreography::kTakeoffVibAmp * hoverRatio;
			heliPos_ = { Choreography::kHeliLandPos.x + vib, Choreography::kHeliLandPos.y + EaseInQuad(hoverRatio) * Choreography::kTakeoffHoverClimbY, Choreography::kHeliLandPos.z };
			heliRot_ = { Choreography::kTakeoffHoverPitch * hoverRatio, Choreography::kHeliLandYaw, 0.0f };
		}
		else
		{
			const float liftT     = (t - Choreography::kTakeoffLiftStartT) / Choreography::kTakeoffLiftSpanT;
			const float climb     = EaseInQuad(liftT);
			const float liftCurve = std::pow(liftT, Choreography::kTakeoffCurveExp);
			heliPos_ = {
				Choreography::kHeliLandPos.x + liftCurve * Choreography::kTakeoffDriftX,
				Choreography::kTakeoffClimbStartY + climb * Choreography::kTakeoffClimbY,
				Choreography::kHeliLandPos.z + liftCurve * Choreography::kTakeoffDriftZ
			};
			heliRot_ = {
				Choreography::kTakeoffNosePitch * std::sin(liftT * kPi * Choreography::kTakeoffPitchSinMul),
				Choreography::kHeliLandYaw - liftT * Choreography::kTakeoffYawDelta,
				Choreography::kTakeoffBankRoll * std::sin(liftT * kPi * Choreography::kTakeoffRollSinMul)
			};
		}
	}
	else
	{
		const float cutIn = 1.0f - EaseOutCubic(std::clamp(actionTimer_ / Choreography::kCruiseCutInDuration, 0.0f, 1.0f));
		const float bobX  = std::sin(actionTimer_ * Choreography::kCruiseBobFreqX) * Choreography::kCruiseBobAmpX;
		const float bobY  = std::sin(actionTimer_ * Choreography::kCruiseBobFreqY1) * Choreography::kCruiseBobAmpY1 +
		                    std::cos(actionTimer_ * Choreography::kCruiseBobFreqY2) * Choreography::kCruiseBobAmpY2;
		const float bobZ  = std::cos(actionTimer_ * Choreography::kCruiseBobFreqZ) * Choreography::kCruiseBobAmpZ;

		heliPos_ = {
			Choreography::kCruiseBasePos.x + bobX + cutIn * Choreography::kCruiseCutInOffset.x,
			Choreography::kCruiseBasePos.y + bobY + cutIn * Choreography::kCruiseCutInOffset.y,
			Choreography::kCruiseBasePos.z + bobZ + cutIn * Choreography::kCruiseCutInOffset.z
		};
		heliRot_ = {
			Choreography::kCruiseBasePitch + std::sin(actionTimer_ * Choreography::kCruisePitchFreq) * Choreography::kCruisePitchAmp,
			Terrain::kCruiseFlightYaw,
			Choreography::kCruiseBaseRoll + std::cos(actionTimer_ * Choreography::kCruiseRollFreq) * Choreography::kCruiseRollAmp
		};
		doorSlideProgress_ = Choreography::kCruiseDoorSlide;
		mainRotorSpeed     = Choreography::kMainRotorSpeedCruise;
		tailRotorSpeed     = Choreography::kTailRotorSpeedCruise;
	}

	mainRotorAngle_ += deltaTime * mainRotorSpeed;
	tailRotorAngle_ += deltaTime * tailRotorSpeed;
}

void ClearScene::UpdateDuckOperator(float deltaTime, const Matrix4x4& heliRotMat)
{
	auto PlaceDuckWithRifle = [&](const Vector3& dPos, const Vector3& dRot, const Vector3& rifleLocalOff, const Vector3& rifleLocalRot) {
		if (duckPlayer_)
		{
			duckPlayer_->SetScale({ Choreography::kDuckScale, Choreography::kDuckScale, Choreography::kDuckScale });
			SyncObjectTransform(duckPlayer_.get(), dPos, dRot);
		}
		if (duckRifle_)
		{
			const Matrix4x4 duckRotMat = MakeEulerRotationMatrix(dRot);
			const Vector3 worldPos     = TransformLocalOffset(dPos, duckRotMat, rifleLocalOff, Choreography::kDuckScale);
			duckRifle_->SetScale({ Choreography::kRifleScale, Choreography::kRifleScale, Choreography::kRifleScale });
			SyncObjectTransform(duckRifle_.get(), worldPos, { dRot.x + rifleLocalRot.x, dRot.y + rifleLocalRot.y, dRot.z + rifleLocalRot.z });
		}
	};

	if (state_ == State::Inbound)
	{
		const float crouchBob = std::sin(timer_ * Choreography::kDuckCrouchFreq) * Choreography::kDuckCrouchAmp;
		const Vector3 dPos    = playerStartPos_ + Vector3{ 0.0f, (std::max)(0.0f, crouchBob), 0.0f };
		const Vector3 dRot    = { Choreography::kDuckIdlePitch, Choreography::kDuckIdleYaw, std::sin(timer_ * Choreography::kDuckIdleSwayFreq) * Choreography::kDuckIdleSwayAmp };
		PlaceDuckWithRifle(dPos, dRot, Choreography::kRifleOffInbound, Choreography::kRifleRotInbound);
	}
	else if (state_ == State::RunningToHeli)
	{
		runPhase_ += deltaTime * Choreography::kRunPhaseSpeed;
		if (!transitionRequested_)
		{
			const int stepIdx = static_cast<int>(runPhase_ / kPi);
			if (stepIdx != prevRunStep_)
			{
				prevRunStep_ = stepIdx;
				TacticalAudio::PlayRandom2D({ "Resources/Audio/SE_Step_Dirt_1.wav", "Resources/Audio/SE_Step_Dirt_2.wav" }, Audio::kVolRunStep, Audio::kRunStepCooldownMs, "clear_run_step");
			}
		}
		playerPos_ = LerpVec(playerStartPos_, Choreography::kDoorGroundSpot, EaseInOutCubic(std::clamp(actionTimer_ / kRunDuration, 0.0f, 1.0f)));
		const float hop    = std::fabs(std::sin(runPhase_)) * Choreography::kRunHopAmp;
		const float waddle = std::sin(runPhase_) * Choreography::kRunWaddleAmp;
		const Vector3 dir  = Choreography::kDoorGroundSpot - playerStartPos_;
		PlaceDuckWithRifle(playerPos_ + Vector3{ 0.0f, hop, 0.0f }, { Choreography::kRunLeanPitch, std::atan2(dir.x, dir.z), waddle }, Choreography::kRifleOffRun, Choreography::kRifleRotRun);
	}
	else if (state_ == State::Boarding)
	{
		const float t     = std::clamp(actionTimer_ / kBoardDuration, 0.0f, 1.0f);
		const float jumpT = EaseInOutCubic(std::clamp(t / Choreography::kBoardJumpSpanT, 0.0f, 1.0f));
		const float turnT = EaseInOutCubic(std::clamp((t - Choreography::kBoardTurnStartT) / Choreography::kBoardTurnSpanT, 0.0f, 1.0f));

		const Vector3 cabinWorld = TransformLocalOffset(heliPos_, heliRotMat, Choreography::kCabinHeroLocal, Choreography::kHeliScale);
		const Vector3 dPos       = LerpVec(Choreography::kDoorGroundSpot, cabinWorld, jumpT) + Vector3{ 0.0f, std::sin(jumpT * kPi) * Choreography::kBoardArcHeight, 0.0f };
		const Vector3 dir        = Choreography::kDoorGroundSpot - playerStartPos_;
		const float startYaw     = std::atan2(dir.x, dir.z);
		const float targetYaw    = heliRot_.y + kHalfPi + Choreography::kCabinHeroYawBias;
		PlaceDuckWithRifle(dPos, { heliRot_.x * turnT, startYaw + (targetYaw - startYaw) * turnT, heliRot_.z * turnT }, Choreography::kRifleOffBoard, Choreography::kRifleRotBoard);
	}
	else
	{
		const float breath  = std::sin(timer_ * Choreography::kHeroBreathFreq) * Choreography::kHeroBreathAmp;
		const float survey  = (state_ == State::Result) ? (std::sin(actionTimer_ * Choreography::kHeroSurveyFreq) * Choreography::kHeroSurveyAmp) : 0.0f;
		const Vector3 dPos  = TransformLocalOffset(heliPos_, heliRotMat, Choreography::kCabinHeroLocal, Choreography::kHeliScale) + Vector3{ 0.0f, breath, 0.0f };
		const Vector3 dRot  = { heliRot_.x * Choreography::kHeroTiltFollow, heliRot_.y + kHalfPi + Choreography::kCabinHeroYawBias + survey, heliRot_.z * Choreography::kHeroTiltFollow };
		PlaceDuckWithRifle(dPos, dRot, Choreography::kRifleOffHero, Choreography::kRifleRotHero);
	}
}

void ClearScene::UpdateCinematicCamera()
{
	if (!camera_) return;

	Vector3 camPos = Choreography::kCameraInitPos;
	Vector3 camRot = Choreography::kCameraInitRot;
	auto EvalCut = [&](const Choreography::CameraCutKeyframe& key, float factor) {
		camPos = LerpVec(key.posStart, key.posEnd, factor);
		camRot = LerpVec(key.rotStart, key.rotEnd, factor);
	};

	if (state_ == State::Inbound)            EvalCut(Choreography::kCamKeyInbound, EaseOutCubic(std::clamp(actionTimer_ / kInboundDuration, 0.0f, 1.0f)));
	else if (state_ == State::RunningToHeli) EvalCut(Choreography::kCamKeyRun,     EaseInOutCubic(std::clamp(actionTimer_ / kRunDuration, 0.0f, 1.0f)));
	else if (state_ == State::Boarding)      EvalCut(Choreography::kCamKeyBoard,   EaseInOutCubic(std::clamp(actionTimer_ / kBoardDuration, 0.0f, 1.0f)));
	else if (state_ == State::Takeoff)
	{
		const float t     = std::clamp(actionTimer_ / kTakeoffDuration, 0.0f, 1.0f);
		const float lookT = EaseInOutCubic(std::clamp((t - Choreography::kCamTakeoffLookStartT) / Choreography::kCamTakeoffLookSpanT, 0.0f, 1.0f));
		EvalCut(Choreography::kCamKeyTakeoff, lookT);

		const float shake = (1.0f - t * Choreography::kCamTakeoffShakeDecay) * Choreography::kCamTakeoffShakeAmp;
		camPos.x += std::sin(actionTimer_ * Choreography::kCamTakeoffShakeFreqX) * shake;
		camPos.y += std::cos(actionTimer_ * Choreography::kCamTakeoffShakeFreqY) * shake * Choreography::kCamTakeoffShakeYRatio;
	}
	else
	{
		const float cutIn      = 1.0f - EaseOutCubic(std::clamp(actionTimer_ / Choreography::kCruiseCutInDuration, 0.0f, 1.0f));
		const float chaseSwayX = std::sin(actionTimer_ * Choreography::kCamResultSwayFreqX) * Choreography::kCamResultSwayAmpX;
		const float chaseSwayY = std::cos(actionTimer_ * Choreography::kCamResultSwayFreqY) * Choreography::kCamResultSwayAmpY;

		camPos = {
			Choreography::kCamResultBasePos.x + chaseSwayX + cutIn * Choreography::kCamResultCutOffset.x,
			Choreography::kCamResultBasePos.y + chaseSwayY + cutIn * Choreography::kCamResultCutOffset.y,
			Choreography::kCamResultBasePos.z + cutIn * Choreography::kCamResultCutOffset.z
		};
		camRot = {
			Choreography::kCamResultBaseRot.x,
			Choreography::kCamResultBaseRot.y,
			Choreography::kCamResultBaseRot.z + std::sin(actionTimer_ * Choreography::kCamResultRollFreq) * Choreography::kCamResultRollAmp
		};
	}

	camera_->SetTranslate(camPos);
	camera_->SetRotate(camRot);
	camera_->SetFovY(Choreography::kCameraDefaultFov);
	camera_->Update();
}

void ClearScene::UpdateAerialScrollBelt(float scrollStep, bool updateTileModels)
{
	aerialScrollOffset_ += scrollStep;
	const float scrollMod = std::fmod(aerialScrollOffset_, Terrain::kAerialTileLen);

	const Vector3 fwd     = { std::sin(Terrain::kCruiseFlightYaw), 0.0f, std::cos(Terrain::kCruiseFlightYaw) };
	const Vector3 right   = { std::cos(Terrain::kCruiseFlightYaw), 0.0f, -std::sin(Terrain::kCruiseFlightYaw) };
	const Vector3 tileRot = { 0.0f, Terrain::kCruiseFlightYaw, 0.0f };

	for (size_t i = 0; i < kAerialTileCount; ++i)
	{
		const float sAlong      = (static_cast<float>(i) - 1.0f) * Terrain::kAerialTileLen + (Terrain::kAerialTileLen * 0.5f - scrollMod);
		const Vector3 tileWorld = Terrain::kAerialBeltAnchor + fwd * sAlong;

		if (updateTileModels)
		{
			SyncObjectTransform(aerialValleyTiles_[i].get(),  tileWorld, tileRot);
			SyncObjectTransform(aerialRiverTiles_[i].get(),   tileWorld, tileRot);
			SyncObjectTransform(aerialForestTiles_[i].get(),  tileWorld, tileRot);
			SyncObjectTransform(aerialOutpostTiles_[i].get(), tileWorld, tileRot);
		}
		for (size_t s = 0; s < kOutpostsPerTile; ++s)
		{
			const Vector3& loc = aerialOutpostLocalPos_[s];
			aerialOutpostWorldPos_[i * kOutpostsPerTile + s] = tileWorld + right * loc.x + Vector3{ 0.0f, loc.y, 0.0f } + fwd * loc.z;
		}
	}
}

void ClearScene::UpdateEscortFormation()
{
	for (size_t i = 0; i < kEscortHeliCount; ++i)
	{
		auto& eh       = escortHelis_[i];
		const float ph = actionTimer_ * (Choreography::kEscortTimeRateBase - Choreography::kEscortTimeRateDecay * static_cast<float>(i)) + eh.rotorPhase;

		eh.worldPos = Choreography::kEscortSpecs[i].basePos + Vector3{
			std::sin(ph * Choreography::kEscortSwayFreqX) * Choreography::kEscortSwayAmpX,
			std::sin(ph * Choreography::kEscortSwayFreqY) * Choreography::kEscortSwayAmpY,
			std::cos(ph * Choreography::kEscortSwayFreqZ) * Choreography::kEscortSwayAmpZ
		};
		eh.worldRot = {
			Choreography::kEscortBasePitch + std::sin(ph * Choreography::kEscortPitchFreq) * Choreography::kEscortPitchAmp,
			Terrain::kCruiseFlightYaw + std::sin(ph * Choreography::kEscortYawFreq) * Choreography::kEscortYawAmp,
			Choreography::kEscortBaseRoll + std::cos(ph * Choreography::kEscortRollFreq) * Choreography::kEscortRollAmp
		};

		const Matrix4x4 escortRotMat = MakeEulerRotationMatrix(eh.worldRot);
		const float rAng    = mainRotorAngle_ * Choreography::kEscortRotorSpeedMul + eh.rotorPhase;
		const float tAng    = tailRotorAngle_ * Choreography::kEscortRotorSpeedMul + eh.rotorPhase;
		const Vector3 mastW = TransformLocalOffset(eh.worldPos, escortRotMat, Choreography::kMainRotorLocalOffset, eh.scale);
		const Vector3 tailW = TransformLocalOffset(eh.worldPos, escortRotMat, Choreography::kTailRotorLocalOffset, eh.scale);

		SyncObjectTransform(eh.body.get(),          eh.worldPos, eh.worldRot);
		SyncObjectTransform(eh.mainRotor.get(),     mastW,       { eh.worldRot.x, eh.worldRot.y + rAng, eh.worldRot.z });
		SyncObjectTransform(eh.mainRotorBlur.get(), mastW,       { eh.worldRot.x, eh.worldRot.y + rAng - Choreography::kEscortRotorBlurOffset, eh.worldRot.z });
		SyncObjectTransform(eh.tailRotor.get(),     tailW,       { eh.worldRot.x + tAng, eh.worldRot.y, eh.worldRot.z });
	}
}

void ClearScene::UpdateSceneLighting(const Matrix4x4& heliRotMat)
{
	if (!SceneManager::GetInstance() || !SceneManager::GetInstance()->GetLight()) return;
	Light* light = SceneManager::GetInstance()->GetLight();

	if (state_ == State::Result)
	{
		light->SetDirectionalLight(Choreography::kCruiseSunColor, Choreography::kCruiseSunDir, Choreography::kCruiseSunIntensity);
		const Vector3 cabinRimPos = TransformLocalOffset(heliPos_, heliRotMat, Choreography::kCabinRimLocalOffset, Choreography::kHeliScale);
		light->SetPointLight(Choreography::kCabinRimColor, cabinRimPos, Choreography::kCabinRimIntensity, Choreography::kCabinRimRadius, Choreography::kCabinRimDecay);
	}
	else
	{
		light->SetDirectionalLight(Choreography::kLzSunColor, Choreography::kLzSunDir, Choreography::kLzSunIntensity);
		light->SetPointLight(Choreography::kFlarePointColor, signalFlarePos_ + Choreography::kFlareLightOffset, Choreography::kFlarePointIntensity, Choreography::kFlarePointRadius, Choreography::kFlarePointDecay);
	}
}

void ClearScene::UpdateDiorama(float deltaTime)
{
	UpdateHelicopterMotion(deltaTime);
	const Matrix4x4 heliRotMat = MakeEulerRotationMatrix(heliRot_);

	SyncObjectTransform(heliBody_.get(),          heliPos_, heliRot_);
	SyncObjectTransform(heliCabinInterior_.get(), heliPos_, heliRot_);

	if (heliMainRotor_)
	{
		const Vector3 mastWorld = TransformLocalOffset(heliPos_, heliRotMat, Choreography::kMainRotorLocalOffset, Choreography::kHeliScale);
		const float blurOffset  = (state_ == State::Takeoff || state_ == State::Result) ? Choreography::kRotorBlurOffsetFast : Choreography::kRotorBlurOffsetGround;
		SyncObjectTransform(heliMainRotor_.get(),      mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_, heliRot_.z });
		SyncObjectTransform(heliMainRotorBlur1_.get(), mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset, heliRot_.z });
		SyncObjectTransform(heliMainRotorBlur2_.get(), mastWorld, { heliRot_.x, heliRot_.y + mainRotorAngle_ - blurOffset * Choreography::kRotorBlurSecondMul, heliRot_.z });
	}
	if (heliTailRotor_)
	{
		const Vector3 tailWorld = TransformLocalOffset(heliPos_, heliRotMat, Choreography::kTailRotorLocalOffset, Choreography::kHeliScale);
		SyncObjectTransform(heliTailRotor_.get(), tailWorld, { heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });
	}
	if (heliCargoDoor_)
	{
		const float curX = Choreography::kCargoDoorBaseX - Choreography::kCargoDoorSlideDeltaX * doorSlideProgress_;
		const float curZ = Choreography::kCargoDoorOpenZ + (Choreography::kCargoDoorClosedZ - Choreography::kCargoDoorOpenZ) * doorSlideProgress_;
		SyncObjectTransform(heliCargoDoor_.get(), TransformLocalOffset(heliPos_, heliRotMat, { curX, Choreography::kCargoDoorLocalY, curZ }, Choreography::kHeliScale), heliRot_);
	}

	UpdateDuckOperator(deltaTime, heliRotMat);
	UpdateCinematicCamera();

	const Vector3 mtnCenter = (state_ == State::Result)
		? Vector3{ heliPos_.x, Terrain::kValleyBaseY, heliPos_.z }
		: Vector3{ 0.0f, Terrain::kGroundMtnCenterY, 0.0f };
	for (Object3d* mtn : { mountainFar_.get(), mountainMid_.get(), mountainNear_.get() })
	{
		if (mtn) { mtn->SetTranslate(mtnCenter); mtn->Update(); }
	}

	if (state_ != State::Result)
	{
		if (groundModel_)      groundModel_->Update();
		if (helipadModel_)     helipadModel_->Update();
		if (signalFlareModel_) signalFlareModel_->Update();
		for (auto& prop : props_) if (prop.model) prop.model->Update();
		EmitGroundLzAtmosphere(deltaTime, true);
	}
	else
	{
		UpdateAerialScrollBelt(deltaTime * Terrain::kAerialScrollSpeed, true);
		UpdateEscortFormation();
		EmitAerialCruiseAtmosphere(deltaTime);
	}

	UpdateSceneLighting(heliRotMat);
	if (smokeParticleManager_) smokeParticleManager_->Update(deltaTime, heliPos_);
	if (glintParticleManager_) glintParticleManager_->Update(deltaTime, heliPos_);
}

// -----------------------------------------------------------------------------
// 3D 描画 (3D Scene Rendering)
// -----------------------------------------------------------------------------

void ClearScene::Draw(SceneRenderRequests& renderRequests)
{
	DirectXCom* dx   = GetDirectXCom();
	Object3dCom* com = GetObject3dCom();
	if (!dx || !com || !camera_) return;

	renderRequests.sceneDrawn = true;
	camera_->Update();

	// Skybox は Game::Draw にて全シーン共通で直前に1回描画済み (Skybox Pre-rendered)
	RenderContext base{};
	base.commandList = dx->GetCommandList().Get();
	base.windowAPI   = dx->GetWindowAPI();
	base.camera      = camera_;
	base.light       = SceneManager::GetInstance() ? SceneManager::GetInstance()->GetLight() : nullptr;
	if (MaterialManager* matMgr = GetMaterialManager(); matMgr && matMgr->GetMaterialResource())
	{
		base.materialGPUAddress = matMgr->GetMaterialResource()->GetGPUVirtualAddress();
	}

	// 1. 360度3層山岳パノラマ: 最遠景 -> 中景 -> 近景山裾の順に描画 (Mountain Rings)
	DrawModel(com, mountainFar_.get(),  base, whiteTexIndex_);
	DrawModel(com, mountainMid_.get(),  base, whiteTexIndex_);
	DrawModel(com, mountainNear_.get(), base, whiteTexIndex_);

	// 2. 地上LZ前哨基地 または 上空スクロール渓谷＆護衛編隊 (World Geometry)
	if (state_ != State::Result)
	{
		DrawModel(com, groundModel_.get(),      base, groundTexIndex_);
		DrawModel(com, helipadModel_.get(),     base, helipadTexIndex_);
		for (auto& prop : props_) DrawModel(com, prop.model.get(), base, prop.texIndex);
		DrawModel(com, signalFlareModel_.get(), base, flareTexIndex_);
	}
	else
	{
		for (size_t i = 0; i < kAerialTileCount; ++i)
		{
			DrawModel(com, aerialValleyTiles_[i].get(),  base, groundTexIndex_);
			DrawModel(com, aerialRiverTiles_[i].get(),   base, waterTexIndex_);
			DrawModel(com, aerialForestTiles_[i].get(),  base, pineTexIndex_);
			DrawModel(com, aerialOutpostTiles_[i].get(), base, containerTexIndex_);
		}
		for (int i = static_cast<int>(kEscortHeliCount) - 1; i >= 0; --i)
		{
			auto& eh = escortHelis_[static_cast<size_t>(i)];
			DrawModel(com, eh.body.get(),          base, heliTexIndex_);
			DrawModel(com, eh.mainRotorBlur.get(), base, heliTexIndex_);
			DrawModel(com, eh.mainRotor.get(),     base, heliTexIndex_);
			DrawModel(com, eh.tailRotor.get(),     base, heliTexIndex_);
		}
	}

	// 3. 主役回収ヘリ Dustoff-1 機内構造・主役アヒル・外装・ローター (Hero Helicopter & Operator)
	DrawModel(com, heliCabinInterior_.get(), base, heliTexIndex_);
	DrawModel(com, duckPlayer_.get(),        base, duckTexIndex_);
	DrawModel(com, duckRifle_.get(),         base, rifleTexIndex_);
	DrawModel(com, heliBody_.get(),          base, heliTexIndex_);
	DrawModel(com, heliCargoDoor_.get(),     base, heliTexIndex_);

	if (state_ == State::Takeoff || state_ == State::Result)
	{
		DrawModel(com, heliMainRotorBlur2_.get(), base, heliTexIndex_);
		DrawModel(com, heliMainRotorBlur1_.get(), base, heliTexIndex_);
	}
	DrawModel(com, heliMainRotor_.get(), base, heliTexIndex_);
	DrawModel(com, heliTailRotor_.get(), base, heliTexIndex_);

	// 4. ストレートアルファ合成ボリューメトリック煙・雲海・砂塵 ＋ 陽光微粒子 (Particles)
	if (smokeParticleManager_) smokeParticleManager_->Draw(base);
	if (glintParticleManager_) glintParticleManager_->Draw(base);
}

// -----------------------------------------------------------------------------
// 2D 描画: 無線字幕 ＆ 戦果デブリーフィングUI (2D Cutscene & Debriefing UI)
// -----------------------------------------------------------------------------

void ClearScene::DrawCutsceneOverlay(TacticalDrawList* dl, float sw, float sh, float ox, float uiScale)
{
	TextureManager* tm  = TextureManager::GetInstance();
	const UITextureID subsTex = (tm && clearCutsceneSubsTexIndex_ != UINT32_MAX)
		? static_cast<UITextureID>(tm->GetSrvHandleGPU(clearCutsceneSubsTexIndex_).ptr)
		: static_cast<UITextureID>(0);
	auto SX = [&](float vx) -> float { return ox + vx * uiScale; };

	const float barTopH = UI::kCutsceneTopBarH * uiScale;
	const float barBotH = UI::kCutsceneBotBarH * uiScale;
	dl->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(sw, barTopH), UI::kColCutsceneTopBar);
	dl->AddRectFilled(UIVec2(0.0f, sh - barBotH), UIVec2(sw, sh), UI::kColCutsceneBotBar);
	dl->AddLine(UIVec2(0.0f, barTopH), UIVec2(sw, barTopH), UI::kColCutsceneRule, 1.0f);
	dl->AddLine(UIVec2(0.0f, sh - barBotH), UIVec2(sw, sh - barBotH), UI::kColCutsceneRule, 1.0f);

	if (!subsTex) return;

	int cutIdx = UI::kSubIdxInbound;
	if (state_ == State::RunningToHeli) cutIdx = UI::kSubIdxRunning;
	else if (state_ == State::Boarding) cutIdx = UI::kSubIdxBoarding;
	else if (state_ == State::Takeoff)  cutIdx = UI::kSubIdxTakeoff;

	const float subFade = EaseOutWindow(actionTimer_, 0.0f, UI::kSubFadeDuration);
	const int fullChan  = static_cast<int>(Common::kByteMask);
	DrawAtlasSprite(
		dl, subsTex, UI::kSubsAtlasW, UI::kSubsAtlasH,
		0.0f, static_cast<float>(cutIdx) * UI::kSubsRowH,
		UI::kSubsAtlasW, static_cast<float>(cutIdx + 1) * UI::kSubsRowH,
		UIVec2(SX(UI::kSubsDstLeftX),  sh - barBotH + UI::kSubsDstTopOffset * uiScale),
		UIVec2(SX(UI::kSubsDstRightX), sh - barBotH + UI::kSubsDstBotOffset * uiScale),
		UI_COL32(fullChan, fullChan, fullChan, static_cast<int>(UI::kSubMaxAlpha * subFade)));

	DrawAtlasSprite(
		dl, subsTex, UI::kSubsAtlasW, UI::kSubsAtlasH,
		UI::kSkipSrcX0, UI::kSkipSrcY0, UI::kSkipSrcX1, UI::kSkipSrcY1,
		UIVec2(sw - UI::kSkipRightMargin1 * uiScale, sh - barBotH + UI::kSkipTopOffset * uiScale),
		UIVec2(sw - UI::kSkipRightMargin0 * uiScale, sh - barBotH + UI::kSkipBotOffset * uiScale),
		UI::kColSkipHint);
}

void ClearScene::DrawResultDebriefUI(TacticalUICanvas* ui, TacticalDrawList* dl, float sw, float sh, float ox, float oy, float uiScale)
{
	TextureManager* tm = TextureManager::GetInstance();
	auto GetUiTex = [&](uint32_t idx) -> UITextureID {
		return (tm && idx != UINT32_MAX) ? static_cast<UITextureID>(tm->GetSrvHandleGPU(idx).ptr) : static_cast<UITextureID>(0);
	};
	auto SX = [&](float vx) -> float { return ox + vx * uiScale; };
	auto SY = [&](float vy) -> float { return oy + vy * uiScale; };

	const UITextureID vignetteTex = GetUiTex(vignetteTexIndex_);
	const UITextureID digitsTex   = GetUiTex(clearDigitsTexIndex_);
	const UITextureID uiAtlasTex  = GetUiTex(clearUiAtlasTexIndex_);

	const float panelFade      = EaseOutWindow(actionTimer_, 0.0f, UI::kPanelFadeDuration);
	const float scrimEdgeMid   = SX(UI::kScrimMidX);
	const float scrimEdgeRight = SX(UI::kScrimRightX);
	const uint32_t cScrimL     = WithAlpha(UI::kColScrimLeft, panelFade);
	const uint32_t cScrimM     = WithAlpha(UI::kColScrimMid, panelFade);

	dl->AddRectFilledMultiColor(UIVec2(0.0f, 0.0f), UIVec2(scrimEdgeMid, sh), cScrimL, cScrimM, cScrimM, cScrimL);
	dl->AddRectFilledMultiColor(UIVec2(scrimEdgeMid, 0.0f), UIVec2(scrimEdgeRight, sh), cScrimM, UI::kColScrimRight, UI::kColScrimRight, cScrimM);

	if (vignetteTex) dl->AddImage(vignetteTex, UIVec2(0.0f, 0.0f), UIVec2(sw, sh), UIVec2(0.0f, 0.0f), UIVec2(1.0f, 1.0f), WithAlpha(UI::kColVignetteTint, panelFade));
	if (resultCutFlash_ > UI::kAnimVisibleEps)
	{
		const int flashA = static_cast<int>(EaseOutQuad(resultCutFlash_) * UI::kCutFlashMaxAlpha);
		dl->AddRectFilled(UIVec2(0.0f, 0.0f), UIVec2(sw, sh), UI_COL32(UI::kCutFlashR, UI::kCutFlashG, UI::kCutFlashB, flashA));
	}
	if (!uiAtlasTex || !digitsTex) return;

	auto DrawLeftUiBand = [&](float vx0, float vy0, float vx1, float vy1, float slideVx, uint32_t tint) {
		DrawAtlasSprite(
			dl, uiAtlasTex, UI::kUiAtlasSize, UI::kUiAtlasSize,
			(vx0 - UI::kLeftBandAtlasOriginX) * UI::kLeftBandAtlasScale, vy0 * UI::kLeftBandAtlasScale,
			(vx1 - UI::kLeftBandAtlasOriginX) * UI::kLeftBandAtlasScale, vy1 * UI::kLeftBandAtlasScale,
			UIVec2(SX(vx0 + slideVx), SY(vy0)), UIVec2(SX(vx1 + slideVx), SY(vy1)), tint);
	};
	auto DrawAccentRule = [&](float y, float endX, float accentEndX, uint32_t accentCol, uint32_t restCol) {
		const float clampedAccent = (std::min)(accentEndX, endX);
		dl->AddLine(UIVec2(SX(UI::kContentLeftX), SY(y)), UIVec2(SX(clampedAccent), SY(y)), accentCol, 1.0f);
		if (endX > accentEndX) dl->AddLine(UIVec2(SX(accentEndX), SY(y)), UIVec2(SX(endX), SY(y)), restCol, 1.0f);
	};

	const DebriefSummary summary = ComputeDebriefSummary(raidStats_);

	// Band 1: Header (EXTRACTION CONFIRMED / MISSION CLEAR / GRADE S-C)
	{
		const float hdrT   = EaseOutWindow(actionTimer_, 0.0f, UI::kHeaderFadeDuration);
		const float slideX = (1.0f - hdrT) * UI::kHeaderSlideOffset;
		DrawLeftUiBand(UI::kHeaderBandX0, UI::kHeaderBandY0, UI::kHeaderBandX1, UI::kHeaderBandY1, slideX, WithAlpha(UI_COL32_WHITE, hdrT));
		dl->AddLine(UIVec2(SX(UI::kHeaderDivX + slideX), SY(UI::kHeaderDivY0)), UIVec2(SX(UI::kHeaderDivX + slideX), SY(UI::kHeaderDivY1)), WithAlpha(UI::kColHeaderDivider, hdrT), 1.0f);

		const float lineW = UI::kHeaderLineMaxW * EaseOutExpo(std::clamp((actionTimer_ - UI::kHeaderLineDelay) / UI::kHeaderLineDuration, 0.0f, 1.0f));
		if (lineW > 1.0f)
		{
			DrawAccentRule(UI::kHeaderLineY, UI::kContentLeftX + lineW, UI::kHeaderAccentEndX, WithAlpha(UI::kColHeaderAccent, hdrT), WithAlpha(UI::kColHeaderDivider, hdrT));
		}

		const float gradeT = EaseOutWindow(actionTimer_, UI::kGradeAnimDelay, UI::kGradeAnimDuration);
		if (gradeT > UI::kAnimVisibleEps)
		{
			const int rc       = summary.rankIdx % UI::kGradeGridCols;
			const int rr       = summary.rankIdx / UI::kGradeGridCols;
			const float halfSz = UI::kGradeBaseHalfSize * (UI::kGradeStartScale - UI::kGradeScaleDelta * gradeT);
			DrawAtlasSprite(
				dl, uiAtlasTex, UI::kUiAtlasSize, UI::kUiAtlasSize,
				UI::kGradeAtlasBaseX + static_cast<float>(rc) * UI::kGradeAtlasCellSize,     static_cast<float>(rr) * UI::kGradeAtlasCellSize,
				UI::kGradeAtlasBaseX + static_cast<float>(rc + 1) * UI::kGradeAtlasCellSize, static_cast<float>(rr + 1) * UI::kGradeAtlasCellSize,
				UIVec2(SX(UI::kGradeCenterX - halfSz), SY(UI::kGradeCenterY - halfSz)),
				UIVec2(SX(UI::kGradeCenterX + halfSz), SY(UI::kGradeCenterY + halfSz)),
				WithAlpha(UI_COL32_WHITE, gradeT));
		}
	}

	// Band 2: Telemetry Strip (TIME IN RAID / ELIMINATIONS / ACCURACY)
	{
		const float telT = EaseOutWindow(actionTimer_, UI::kTelemetryDelay, UI::kTelemetryDuration);
		if (telT > UI::kAnimVisibleEps)
		{
			const float slideX = (1.0f - telT) * UI::kTelemetrySlideOffset;
			DrawLeftUiBand(UI::kHeaderBandX0, UI::kTelemetryBandY0, UI::kHeaderBandX1, UI::kTelemetryBandY1, slideX, WithAlpha(UI_COL32_WHITE, telT));

			for (float sepX : UI::kTelemetrySepXs)
			{
				dl->AddLine(UIVec2(SX(sepX), SY(UI::kTelemetrySepY0)), UIVec2(SX(sepX), SY(UI::kTelemetrySepY1)), WithAlpha(UI::kColTelemetryVertSep, telT), 1.0f);
			}
			dl->AddLine(UIVec2(SX(UI::kContentLeftX), SY(UI::kTelemetryBottomLineY)), UIVec2(SX(UI::kContentRightX), SY(UI::kTelemetryBottomLineY)), WithAlpha(UI::kColSectionHairline, telT), 1.0f);

			const int raidSecTotal = static_cast<int>(std::round(raidStats_.raidTime * countUpProgress_));
			const int dispKills    = static_cast<int>(std::round(static_cast<float>(raidStats_.enemiesKilled) * countUpProgress_));
			const int dispAcc      = static_cast<int>(std::round(static_cast<float>(summary.accuracyPct) * countUpProgress_));

			char colTexts[UI::kTelemetryColCount][Common::kStatTextBufSize]{};
			std::snprintf(colTexts[UI::kTelColTime],     sizeof(colTexts[UI::kTelColTime]),     "%02d:%02d", raidSecTotal / Common::kSecondsPerMinute, raidSecTotal % Common::kSecondsPerMinute);
			std::snprintf(colTexts[UI::kTelColKills],    sizeof(colTexts[UI::kTelColKills]),    "%d", dispKills);
			std::snprintf(colTexts[UI::kTelColAccuracy], sizeof(colTexts[UI::kTelColAccuracy]), "%d%%", dispAcc);

			const float valH      = UI::kTelemetryDigitH * uiScale;
			const uint32_t valCol = WithAlpha(UI::kColOffWhite, telT);
			for (size_t c = 0; c < UI::kTelemetryColCount; ++c)
			{
				DrawFlatDigits(dl, digitsTex, colTexts[c], SX(UI::kTelemetryColXs[c] + slideX), SY(UI::kTelemetryDigitCenterY), valH, valCol, UI::kAlignLeft);
			}
		}
	}

	// Band 3..9: REWARD BREAKDOWN (6-Row Hairline Ledger & Loot Stencils)
	{
		const float secT = EaseOutWindow(actionTimer_, UI::kRewardHdrDelay, UI::kRewardHdrDuration);
		if (secT > UI::kAnimVisibleEps)
		{
			DrawLeftUiBand(UI::kHeaderBandX0, UI::kRewardHdrY0, UI::kRewardHdrX1, UI::kRewardHdrY1, (1.0f - secT) * UI::kRewardHdrSlideOffset, WithAlpha(UI_COL32_WHITE, secT));
		}

		for (int i = 0; i < UI::kRewardRowCount; ++i)
		{
			const float rowT = EaseOutWindow(actionTimer_, UI::kRewardRowBaseDelay + static_cast<float>(i) * UI::kRewardRowDelayStep, UI::kRewardRowDuration);
			if (rowT <= UI::kAnimVisibleEps) continue;

			const float slideX = (1.0f - rowT) * UI::kRewardRowSlideOffset;
			const float vyTop  = UI::kRewardRowStartY + static_cast<float>(i) * UI::kRewardRowHeight;
			const float vyBot  = vyTop + UI::kRewardRowHeight;

			DrawLeftUiBand(UI::kHeaderBandX0, vyTop, UI::kRewardRowLabelX1, vyBot, slideX, WithAlpha(UI_COL32_WHITE, rowT));
			dl->AddLine(UIVec2(SX(UI::kContentLeftX), SY(vyBot)), UIVec2(SX(UI::kContentRightX), SY(vyBot)), WithAlpha(UI::kColLedgerRowLine, rowT), 1.0f);

			const int dispAmt = static_cast<int>(std::round(static_cast<float>(summary.rowAmounts[static_cast<size_t>(i)]) * countUpProgress_));
			char numComma[Common::kCommaOutBufSize];
			char amountBuf[Common::kAmountTextBufSize];
			FormatIntCommas(dispAmt, numComma, sizeof(numComma));
			std::snprintf(amountBuf, sizeof(amountBuf), "+%s", numComma);

			const uint32_t amtCol = (i == UI::kLootBonusRowIndex && summary.lootBonus > 0) ? WithAlpha(UI::kColChampagne, rowT) : WithAlpha(UI::kColOffWhite, rowT);
			const float cyDigits  = vyTop + UI::kRewardRowCenterOffsetY;
			DrawFlatDigits(dl, digitsTex, amountBuf, SX(UI::kRewardAmtRightX + slideX), SY(cyDigits), UI::kRewardAmtDigitH * uiScale, amtCol, UI::kAlignRight);
			DrawFlatDigits(dl, digitsTex, "RUB",     SX(UI::kContentRightX + slideX),   SY(cyDigits + UI::kRewardUnitOffsetY), UI::kRewardUnitDigitH * uiScale, WithAlpha(UI::kColMicroGray, rowT), UI::kAlignRight);
		}

		const float lootT = EaseOutWindow(actionTimer_, UI::kLootStripDelay, UI::kLootStripDuration);
		if (lootT > UI::kAnimVisibleEps)
		{
			const std::array<int, UI::kLootIconCount> lootCounts = {
				raidStats_.lootedGoldDucks, raidStats_.lootedCashCount, raidStats_.lootedMedkits, raidStats_.lootedAmmoBoxes
			};
			for (int ic = 0; ic < UI::kLootIconCount; ++ic)
			{
				const float lx        = UI::kContentLeftX + static_cast<float>(ic) * UI::kLootIconStepX;
				const int count       = lootCounts[static_cast<size_t>(ic)];
				const bool hasItem    = (count > 0);
				const float itemAlpha = lootT * (hasItem ? 1.0f : UI::kLootEmptyAlphaMul);
				const uint32_t iconCol = (ic == 0 && hasItem) ? WithAlpha(UI::kColChampagne, itemAlpha) : WithAlpha(UI::kColMuted, itemAlpha);

				DrawAtlasSprite(
					dl, uiAtlasTex, UI::kUiAtlasSize, UI::kUiAtlasSize,
					UI::kLootAtlasBaseX + static_cast<float>(ic) * UI::kLootAtlasCellW,     UI::kLootAtlasY0,
					UI::kLootAtlasBaseX + static_cast<float>(ic + 1) * UI::kLootAtlasCellW, UI::kLootAtlasY1,
					UIVec2(SX(lx), SY(UI::kLootIconY0)), UIVec2(SX(lx + UI::kLootIconSize), SY(UI::kLootIconY1)), iconCol);

				char cntBuf[Common::kLootCountBufSize];
				std::snprintf(cntBuf, sizeof(cntBuf), "x%d", count);
				DrawFlatDigits(dl, digitsTex, cntBuf, SX(lx + UI::kLootCountOffsetX), SY(UI::kLootCountCenterY), UI::kLootCountDigitH * uiScale, WithAlpha(UI::kColMuted, itemAlpha), UI::kAlignLeft);
			}
		}
	}

	// Band 10: TOTAL PAYOUT / 総獲得報酬 (Total Payout)
	{
		const float totT = EaseOutWindow(actionTimer_, UI::kTotalDelay, UI::kTotalDuration);
		if (totT > UI::kAnimVisibleEps)
		{
			const float slideX = (1.0f - totT) * UI::kTotalSlideOffset;
			DrawAccentRule(UI::kTotalLineY, UI::kContentRightX, UI::kTotalAccentEndX, WithAlpha(UI::kColTotalAccent, totT), WithAlpha(UI::kColSectionHairline, totT));
			DrawLeftUiBand(UI::kHeaderBandX0, UI::kTotalBandY0, UI::kTotalBandX1, UI::kTotalBandY1, slideX, WithAlpha(UI_COL32_WHITE, totT));

			const int dispTotal = static_cast<int>(std::round(static_cast<float>(summary.totalPayout) * countUpProgress_));
			char totalComma[Common::kCommaOutBufSize];
			FormatIntCommas(dispTotal, totalComma, sizeof(totalComma));

			DrawFlatDigits(dl, digitsTex, totalComma, SX(UI::kTotalAmtRightX + slideX), SY(UI::kTotalAmtCenterY),  UI::kTotalAmtDigitH * uiScale,  WithAlpha(UI::kColChampagne, totT), UI::kAlignRight);
			DrawFlatDigits(dl, digitsTex, "RUB",      SX(UI::kContentRightX + slideX),  SY(UI::kTotalUnitCenterY), UI::kTotalUnitDigitH * uiScale, WithAlpha(UI::kColBrass, totT),     UI::kAlignRight);
		}
	}

	// Band 11: Minimalist Keycap Action Prompts ([SPACE] Deploy / [T] Title)
	{
		const float btnT = EaseOutWindow(actionTimer_, UI::kButtonsDelay, UI::kButtonsDuration);
		if (btnT > UI::kAnimVisibleEps)
		{
			const UIVec2 mousePos = ui->GetMousePos();
			auto UpdateButtonHover = [&](float hitX0, float hitX1, bool& prevHover, float& anim) -> bool {
				const bool hovered = (mousePos.x >= SX(hitX0) && mousePos.x <= SX(hitX1) && mousePos.y >= SY(UI::kBtnHitY0) && mousePos.y <= SY(UI::kBtnHitY1));
				if (!transitionRequested_ && hovered && !prevHover)
				{
					TacticalAudio::Play2D("Resources/Audio/SE_UI_Hover.wav", Audio::kVolUiHover, Audio::kHoverCooldownMs, "clear_ui_hover");
				}
				prevHover = hovered;
				anim = std::clamp(anim + (hovered ? 1.0f : -1.0f) * Common::kFixedDeltaTime * UI::kHoverAnimRate, 0.0f, 1.0f);
				return hovered;
			};

			const bool hoverNext  = UpdateButtonHover(UI::kNextHitX0,  UI::kNextHitX1,  prevHoverNext_,  nextRaidHoverAnim_);
			const bool hoverTitle = UpdateButtonHover(UI::kTitleHitX0, UI::kTitleHitX1, prevHoverTitle_, toTitleHoverAnim_);

			if (nextRaidHoverAnim_ > UI::kHoverDrawThreshold)
			{
				dl->AddRectFilled(UIVec2(SX(UI::kNextHitX0), SY(UI::kHoverFillY0)), UIVec2(SX(UI::kNextFillX1), SY(UI::kHoverFillY1)), WithAlpha(UI::kColHoverNextFill, btnT * nextRaidHoverAnim_));
			}
			if (toTitleHoverAnim_ > UI::kHoverDrawThreshold)
			{
				dl->AddRectFilled(UIVec2(SX(UI::kTitleHitX0), SY(UI::kHoverFillY0)), UIVec2(SX(UI::kTitleFillX1), SY(UI::kHoverFillY1)), WithAlpha(UI::kColHoverTitleFill, btnT * toTitleHoverAnim_));
			}

			DrawLeftUiBand(UI::kHeaderBandX0, UI::kBtnBandY0, UI::kNextBandX1,  UI::kBtnBandY1, 0.0f, WithAlpha(UI_COL32_WHITE, btnT));
			DrawLeftUiBand(UI::kTitleBandX0,  UI::kBtnBandY0, UI::kTitleBandX1, UI::kBtnBandY1, 0.0f, WithAlpha(LerpColor(UI::kColTitleBandIdle, UI_COL32_WHITE, toTitleHoverAnim_), btnT));

			const float nextLineEnd = UI::kNextUnderlineBaseEndX + nextRaidHoverAnim_ * UI::kNextUnderlineHoverGainX;
			dl->AddLine(UIVec2(SX(UI::kContentLeftX), SY(UI::kBtnUnderlineY)), UIVec2(SX(nextLineEnd), SY(UI::kBtnUnderlineY)), WithAlpha(LerpColor(UI::kColNextLineIdle, UI::kColNextLineHover, nextRaidHoverAnim_), btnT), UI::kBtnUnderlineThickness);

			if (toTitleHoverAnim_ > UI::kHoverDrawThreshold)
			{
				const float titleLineEnd = UI::kTitleUnderlineStartX + UI::kTitleUnderlineSpanX * EaseOutCubic(toTitleHoverAnim_);
				dl->AddLine(UIVec2(SX(UI::kTitleUnderlineStartX), SY(UI::kBtnUnderlineY)), UIVec2(SX(titleLineEnd), SY(UI::kBtnUnderlineY)), WithAlpha(UI::kColTitleLineHover, btnT * toTitleHoverAnim_), UI::kBtnUnderlineThickness);
			}

			if (!transitionRequested_ && ui->IsMouseClicked(0))
			{
				if (hoverNext)       HandleResultConfirmAction();
				else if (hoverTitle) RequestSceneTransition("TITLE");
			}
		}
	}
}

void ClearScene::DrawScreen()
{
	if (StingerTransition::GetInstance() && StingerTransition::GetInstance()->IsTransitioning()) return;

	TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
	if (!ui) return;

	const UIVec2 disp = ui->GetDisplaySize();
	if (disp.x <= 0.0f || disp.y <= 0.0f) return;

	TacticalDrawList* dl = ui->GetBackgroundDrawList();
	if (!dl || !TextureManager::GetInstance()) return;

	const float uiScale = (std::min)(disp.x / UI::kVirtualWidth, disp.y / UI::kVirtualHeight);
	const float ox      = (disp.x - UI::kVirtualWidth * uiScale) * 0.5f;
	const float oy      = (disp.y - UI::kVirtualHeight * uiScale) * 0.5f;

	if (state_ != State::Result)
	{
		DrawCutsceneOverlay(dl, disp.x, disp.y, ox, uiScale);
	}
	else
	{
		DrawResultDebriefUI(ui, dl, disp.x, disp.y, ox, oy, uiScale);
	}
}
