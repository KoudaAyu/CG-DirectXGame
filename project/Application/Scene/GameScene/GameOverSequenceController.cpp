#include "GameOverSequenceController.h"
#include "GamePlayScene.h"
#include "RaidStats.h"
#include "Application/Transition/StingerTransition.h"
#include "Application/Transition/DeathTransition.h"
#include "Camera.h"
#include "Application/Player/Player.h"
#include "Application/Enemy/Enemy.h"
#include "Obstacle.h"
#include "Target.h"
#include "TutorialSign.h"
#include "Application/Particle/AppParticleManager.h"
#include "Baziru3_Engine/Framework/Particle/ParticleManager.h"
#include "Baziru3_Engine/Framework/Collision/BoxCollider.h"
#include "Baziru3_Engine/Graphics/Light/Light.h"
#include "TextureManager.h"
#include "SceneManager.h"
#include "Baziru3_Engine/Framework/Audio/AudioManager.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <iostream>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace
{
	constexpr float kPi = 3.14159265358979323846f;

	float Rand01()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}

	float RandRange(float a, float b)
	{
		return a + (b - a) * Rand01();
	}

	float EaseOutQuad(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return t * (2.0f - t);
	}

	float EaseOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	float EaseInOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
	}

	float NormalizeAngleDiff(float a, float b)
	{
		float d = std::fmod(a - b + kPi, 2.0f * kPi);
		if (d < 0.0f) d += 2.0f * kPi;
		return d - kPi;
	}

	Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t
		};
	}

	Vector4 LerpVector4(const Vector4& a, const Vector4& b, float t)
	{
		return {
			a.x + (b.x - a.x) * t,
			a.y + (b.y - a.y) * t,
			a.z + (b.z - a.z) * t,
			a.w + (b.w - a.w) * t
		};
	}

	Vector3 CalcLookAtRotation(const Vector3& eye, const Vector3& target, float roll = 0.0f)
	{
		Vector3 dir = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
		float xzLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		float pitch = std::atan2(-dir.y, (std::max)(xzLen, 0.001f));
		float yaw = std::atan2(dir.x, dir.z);
		return { pitch, yaw, roll };
	}

	bool CheckRayOBB(
		const Vector3& rayStart,
		const Vector3& rayDir,
		float maxDist,
		const Vector3& boxCenter,
		const Vector3& boxSize,
		const Vector3& boxRot,
		float& outDist)
	{
		Vector3 e = { boxSize.x * 0.5f, boxSize.y * 0.5f, boxSize.z * 0.5f };
		Matrix4x4 R = Multiply(MakeRotateXMatrix(boxRot.x), Multiply(MakeRotateYMatrix(boxRot.y), MakeRotateZMatrix(boxRot.z)));
		Vector3 axisX = { R.m[0][0], R.m[0][1], R.m[0][2] };
		Vector3 axisY = { R.m[1][0], R.m[1][1], R.m[1][2] };
		Vector3 axisZ = { R.m[2][0], R.m[2][1], R.m[2][2] };

		Vector3 off = rayStart - boxCenter;
		float s[3] = {
			off.x * axisX.x + off.y * axisX.y + off.z * axisX.z,
			off.x * axisY.x + off.y * axisY.y + off.z * axisY.z,
			off.x * axisZ.x + off.y * axisZ.y + off.z * axisZ.z
		};
		float d[3] = {
			rayDir.x * axisX.x + rayDir.y * axisX.y + rayDir.z * axisX.z,
			rayDir.x * axisY.x + rayDir.y * axisY.y + rayDir.z * axisY.z,
			rayDir.x * axisZ.x + rayDir.y * axisZ.y + rayDir.z * axisZ.z
		};
		float ext[3] = { e.x, e.y, e.z };

		float tMin = 0.0f;
		float tMax = maxDist;
		for (int i = 0; i < 3; ++i)
		{
			if (std::abs(d[i]) < 1e-6f)
			{
				if (s[i] < -ext[i] || s[i] > ext[i]) return false;
			}
			else
			{
				float invD = 1.0f / d[i];
				float t1 = (-ext[i] - s[i]) * invD;
				float t2 = ( ext[i] - s[i]) * invD;
				if (t1 > t2) std::swap(t1, t2);
				tMin = (std::max)(tMin, t1);
				tMax = (std::min)(tMax, t2);
				if (tMin > tMax) return false;
			}
		}
		outDist = tMin;
		return true;
	}

	// 球体(sphereCenter, radius)とOBBの水平XZ押し出しベクトル算出（貫通していれば true と押し出し量を返す）
	bool ComputeSphereOBBPushOutXZ(
		const Vector3& sphereCenter,
		float radius,
		const Vector3& boxCenter,
		const Vector3& boxSize,
		const Vector3& boxRot,
		Vector3& outPushXZ,
		float& outPenetration)
	{
		Vector3 half = boxSize * 0.5f;
		// 上下方向で全く重なっていなければ衝突しない
		if (sphereCenter.y + radius < boxCenter.y - half.y ||
			sphereCenter.y - radius > boxCenter.y + half.y)
		{
			return false;
		}

		Matrix4x4 R = Multiply(MakeRotateXMatrix(boxRot.x), Multiply(MakeRotateYMatrix(boxRot.y), MakeRotateZMatrix(boxRot.z)));
		Vector3 axisX = { R.m[0][0], 0.0f, R.m[0][2] };
		Vector3 axisZ = { R.m[2][0], 0.0f, R.m[2][2] };
		float lenX = std::sqrt(axisX.x * axisX.x + axisX.z * axisX.z);
		float lenZ = std::sqrt(axisZ.x * axisZ.x + axisZ.z * axisZ.z);
		if (lenX > 1e-4f) { axisX.x /= lenX; axisX.z /= lenX; } else { axisX = { 1.0f, 0.0f, 0.0f }; }
		if (lenZ > 1e-4f) { axisZ.x /= lenZ; axisZ.z /= lenZ; } else { axisZ = { 0.0f, 0.0f, 1.0f }; }

		Vector3 rel = { sphereCenter.x - boxCenter.x, 0.0f, sphereCenter.z - boxCenter.z };
		float lx = rel.x * axisX.x + rel.z * axisX.z;
		float lz = rel.x * axisZ.x + rel.z * axisZ.z;

		float cx = (std::clamp)(lx, -half.x, half.x);
		float cz = (std::clamp)(lz, -half.z, half.z);

		float dx = lx - cx;
		float dz = lz - cz;
		float distSq = dx * dx + dz * dz;

		if (distSq >= radius * radius)
		{
			return false;
		}

		if (distSq > 1e-6f)
		{
			float dist = std::sqrt(distSq);
			outPenetration = radius - dist;
			float nxLocal = dx / dist;
			float nzLocal = dz / dist;
			Vector3 worldNormal = {
				nxLocal * axisX.x + nzLocal * axisZ.x,
				0.0f,
				nxLocal * axisX.z + nzLocal * axisZ.z
			};
			outPushXZ = worldNormal * outPenetration;
			return true;
		}

		// 球の中心がOBB内部にある場合：最も浅い水平面から外へ押し出す
		float penX = (half.x - std::abs(lx)) + radius;
		float penZ = (half.z - std::abs(lz)) + radius;
		if (penX <= penZ)
		{
			float signX = (lx >= 0.0f) ? 1.0f : -1.0f;
			outPenetration = penX;
			outPushXZ = axisX * (signX * penX);
		}
		else
		{
			float signZ = (lz >= 0.0f) ? 1.0f : -1.0f;
			outPenetration = penZ;
			outPushXZ = axisZ * (signZ * penZ);
		}
		return true;
	}

	void AppendBoxGeometry(
		Object3d::ModelData& data,
		const Vector3& center,
		const Vector3& size,
		const Vector2& uvMin = { 0.0f, 0.0f },
		const Vector2& uvMax = { 1.0f, 1.0f })
	{
		Vector3 h = size * 0.5f;
		Vector3 p[8] = {
			{ center.x - h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y - h.y, center.z - h.z },
			{ center.x + h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y + h.y, center.z - h.z },
			{ center.x - h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y - h.y, center.z + h.z },
			{ center.x + h.x, center.y + h.y, center.z + h.z },
			{ center.x - h.x, center.y + h.y, center.z + h.z },
		};
		int faces[6][4] = {
			{ 0, 1, 2, 3 }, { 5, 4, 7, 6 }, { 4, 0, 3, 7 },
			{ 1, 5, 6, 2 }, { 3, 2, 6, 7 }, { 4, 5, 1, 0 }
		};
		Vector3 normals[6] = {
			{ 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f, 1.0f }, { -1.0f, 0.0f, 0.0f },
			{ 1.0f, 0.0f, 0.0f },  { 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }
		};

		for (int f = 0; f < 6; ++f)
		{
			uint32_t fBase = static_cast<uint32_t>(data.vertices.size());
			for (int v = 0; v < 4; ++v)
			{
				Sprite::VertexData vd{};
				vd.position = { p[faces[f][v]].x, p[faces[f][v]].y, p[faces[f][v]].z, 1.0f };
				vd.texcoord = { (v == 1 || v == 2) ? uvMax.x : uvMin.x, (v == 2 || v == 3) ? uvMax.y : uvMin.y };
				vd.normal = normals[f];
				data.vertices.push_back(vd);
			}
			data.indices.push_back(fBase + 0);
			data.indices.push_back(fBase + 1);
			data.indices.push_back(fBase + 2);
			data.indices.push_back(fBase + 0);
			data.indices.push_back(fBase + 2);
			data.indices.push_back(fBase + 3);
		}
	}
}

// -----------------------------------------------------------------------------
// 3Dプロシージャルモデル生成（ライフル・救難フレア・ヘリスライドドア・担架・影）
// -----------------------------------------------------------------------------

void GameOverSequenceController::BuildRifleModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	// 最初から水平面(XZ)に自然な横倒し姿勢で構築（Y方向はレシーバー厚み ±0.034m、+Zが銃口、+X側がグリップ＆湾曲マガジン）
	// 1. アッパー＆ロアレシーバー（機関部）
	AppendBoxGeometry(data, {  0.000f, 0.0f,  0.00f }, { 0.082f, 0.064f, 0.34f });
	// 2. M-LOK ハンドガード（被筒）
	AppendBoxGeometry(data, { -0.004f, 0.0f,  0.30f }, { 0.058f, 0.052f, 0.26f });
	// 3. バレル（銃身）＆ フラッシュハイダー（消炎器）
	AppendBoxGeometry(data, { -0.010f, 0.0f,  0.52f }, { 0.024f, 0.024f, 0.20f });
	AppendBoxGeometry(data, { -0.010f, 0.0f,  0.65f }, { 0.032f, 0.032f, 0.07f });
	// 4. バッファチューブ ＆ タクティカル・バットストック（銃床）
	AppendBoxGeometry(data, { -0.005f, 0.0f, -0.26f }, { 0.040f, 0.040f, 0.18f });
	AppendBoxGeometry(data, {  0.028f, 0.0f, -0.39f }, { 0.115f, 0.048f, 0.13f });
	// 5. ピストルグリップ ＆ トリガーガード（これがあることで一目で銃と分かるシルエットになる）
	AppendBoxGeometry(data, {  0.088f, 0.0f, -0.09f }, { 0.095f, 0.038f, 0.052f });
	AppendBoxGeometry(data, {  0.055f, 0.0f, -0.01f }, { 0.028f, 0.024f, 0.075f });
	// 6. 3段テーパー湾曲マガジン（不自然な直方体の突き出しをなくし、前方に緩やかにカーブするリアル形状）
	AppendBoxGeometry(data, {  0.075f, 0.0f,  0.085f }, { 0.070f, 0.040f, 0.072f });
	AppendBoxGeometry(data, {  0.128f, 0.0f,  0.105f }, { 0.068f, 0.038f, 0.070f });
	AppendBoxGeometry(data, {  0.175f, 0.0f,  0.136f }, { 0.058f, 0.036f, 0.066f });
	// 7. ホロサイト光学照準器（上部レール -X 側）
	AppendBoxGeometry(data, { -0.066f, 0.0f,  0.01f }, { 0.048f, 0.046f, 0.11f });

	data.boundingRadius = 0.9f;
}

void GameOverSequenceController::BuildFlareCanisterModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	// リアルな手榴弾サイズの滑らかな16角円筒形 AN-M18 スモークグレネード (半径 3.6cm, 長さ 14.5cm)
	constexpr int kSegs = 16;
	constexpr float kRadius = 0.036f;
	constexpr float kCenterY = 0.038f;
	constexpr float z0 = -0.072f;
	constexpr float z1 =  0.072f;

	for (int i = 0; i < kSegs; ++i)
	{
		float a0 = static_cast<float>(i) * (2.0f * kPi / static_cast<float>(kSegs));
		float a1 = static_cast<float>(i + 1) * (2.0f * kPi / static_cast<float>(kSegs));
		float c0 = std::cos(a0), s0 = std::sin(a0);
		float c1 = std::cos(a1), s1 = std::sin(a1);

		uint32_t base = static_cast<uint32_t>(data.vertices.size());
		Sprite::VertexData v[4]{};
		v[0].position = { c0 * kRadius, kCenterY + s0 * kRadius, z0, 1.0f }; v[0].normal = { c0, s0, 0.0f }; v[0].texcoord = { 0.0f, 0.0f };
		v[1].position = { c1 * kRadius, kCenterY + s1 * kRadius, z0, 1.0f }; v[1].normal = { c1, s1, 0.0f }; v[1].texcoord = { 1.0f, 0.0f };
		v[2].position = { c1 * kRadius, kCenterY + s1 * kRadius, z1, 1.0f }; v[2].normal = { c1, s1, 0.0f }; v[2].texcoord = { 1.0f, 1.0f };
		v[3].position = { c0 * kRadius, kCenterY + s0 * kRadius, z1, 1.0f }; v[3].normal = { c0, s0, 0.0f }; v[3].texcoord = { 0.0f, 1.0f };
		for (int k = 0; k < 4; ++k) data.vertices.push_back(v[k]);

		data.indices.push_back(base + 0); data.indices.push_back(base + 2); data.indices.push_back(base + 1);
		data.indices.push_back(base + 0); data.indices.push_back(base + 3); data.indices.push_back(base + 2);
	}
	// 前後の円盤キャップ ＆ 小型信管ヘッド
	AppendBoxGeometry(data, { 0.0f, kCenterY, z0 }, { 0.050f, 0.050f, 0.012f });
	AppendBoxGeometry(data, { 0.0f, kCenterY, z1 + 0.014f }, { 0.032f, 0.032f, 0.028f });

	data.boundingRadius = 0.5f;
}

void GameOverSequenceController::BuildFlareGlowCapModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	// 信管先端の控えめな赤熱ノズル発光点
	AppendBoxGeometry(data, { 0.0f, 0.038f, 0.095f }, { 0.028f, 0.028f, 0.022f });
	data.boundingRadius = 0.5f;
}

void GameOverSequenceController::BuildHelicopterDoorModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	const Vector2 uvOliveMin{ 0.25f, 0.05f };
	const Vector2 uvOliveMax{ 0.35f, 0.15f };
	const Vector2 uvMetalMin{ 0.65f, 0.05f };
	const Vector2 uvMetalMax{ 0.75f, 0.15f };
	const Vector2 uvGlassMin{ 0.10f, 0.75f };
	const Vector2 uvGlassMax{ 0.25f, 0.85f };
	const Vector2 uvWarnMin { 0.85f, 0.05f };
	const Vector2 uvWarnMax { 0.95f, 0.15f };

	AppendBoxGeometry(data, { 0.0f, 0.0f, 0.0f }, { 0.04f, 2.12f, 2.72f }, uvOliveMin, uvOliveMax);
	AppendBoxGeometry(data, { 0.012f,  1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.012f, -1.08f, 0.0f }, { 0.065f, 0.06f, 2.76f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.022f, -0.45f, 0.0f }, { 0.015f, 0.04f, 2.62f }, uvOliveMin, uvOliveMax);
	AppendBoxGeometry(data, { 0.022f,  0.80f, 0.0f }, { 0.015f, 0.04f, 2.62f }, uvOliveMin, uvOliveMax);
	AppendBoxGeometry(data, { 0.022f, 0.35f, 0.45f }, { 0.012f, 0.65f, 0.75f }, uvGlassMin, uvGlassMax);
	AppendBoxGeometry(data, { 0.026f, 0.70f, 0.45f }, { 0.020f, 0.05f, 0.85f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.026f, 0.00f, 0.45f }, { 0.020f, 0.05f, 0.85f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.026f, 0.35f, 0.05f }, { 0.020f, 0.65f, 0.05f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.026f, 0.35f, 0.85f }, { 0.020f, 0.65f, 0.05f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.026f, -0.05f, 1.18f }, { 0.020f, 0.34f, 0.07f }, uvMetalMin, uvMetalMax);
	AppendBoxGeometry(data, { 0.042f, -0.05f, 1.18f }, { 0.020f, 0.26f, 0.03f }, uvWarnMin,  uvWarnMax);

	data.boundingRadius = 4.0f;
}

void GameOverSequenceController::BuildStretcherModel(Object3d::ModelData& data)
{
	data.vertices.clear();
	data.indices.clear();

	AppendBoxGeometry(data, { 0.0f, 0.0f, -0.60f }, { 2.40f, 0.06f, 0.06f });
	AppendBoxGeometry(data, { 0.0f, 0.0f,  0.60f }, { 2.40f, 0.06f, 0.06f });
	AppendBoxGeometry(data, { -1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
	AppendBoxGeometry(data, { -1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });
	AppendBoxGeometry(data, {  1.15f, 0.0f, -0.60f }, { 0.16f, 0.08f, 0.08f });
	AppendBoxGeometry(data, {  1.15f, 0.0f,  0.60f }, { 0.16f, 0.08f, 0.08f });
	AppendBoxGeometry(data, { 0.0f, -0.015f, 0.0f }, { 1.85f, 0.03f, 1.15f });
	AppendBoxGeometry(data, { -0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
	AppendBoxGeometry(data, { -0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });
	AppendBoxGeometry(data, {  0.75f, -0.07f, -0.60f }, { 0.05f, 0.12f, 0.05f });
	AppendBoxGeometry(data, {  0.75f, -0.07f,  0.60f }, { 0.05f, 0.12f, 0.05f });
	AppendBoxGeometry(data, { -0.60f, 0.04f, 0.0f }, { 0.45f, 0.08f, 0.85f });

	data.boundingRadius = 4.0f;
}

void GameOverSequenceController::BuildShadowDiscModel(Object3d::ModelData& data, float rx, float rz)
{
	data.vertices.clear();
	data.indices.clear();

	Sprite::VertexData v0{}, v1{}, v2{}, v3{};
	v0.position = { -rx, 0.0f, -rz, 1.0f }; v0.texcoord = { 0.0f, 0.0f }; v0.normal = { 0.0f, 1.0f, 0.0f };
	v1.position = {  rx, 0.0f, -rz, 1.0f }; v1.texcoord = { 1.0f, 0.0f }; v1.normal = { 0.0f, 1.0f, 0.0f };
	v2.position = { -rx, 0.0f,  rz, 1.0f }; v2.texcoord = { 0.0f, 1.0f }; v2.normal = { 0.0f, 1.0f, 0.0f };
	v3.position = {  rx, 0.0f,  rz, 1.0f }; v3.texcoord = { 1.0f, 1.0f }; v3.normal = { 0.0f, 1.0f, 0.0f };

	data.vertices.push_back(v0);
	data.vertices.push_back(v1);
	data.vertices.push_back(v2);
	data.vertices.push_back(v3);

	data.indices.push_back(0); data.indices.push_back(1); data.indices.push_back(2);
	data.indices.push_back(1); data.indices.push_back(3); data.indices.push_back(2);
	data.boundingRadius = 15.0f;
}

void GameOverSequenceController::BuildVolumetricConeModel(
	Object3d::ModelData& data,
	float apexRadius,
	float baseRadius,
	int radialSegs,
	int heightRings)
{
	data.vertices.clear();
	data.indices.clear();

	// ローカル +Z 方向 (z=0: 光源レンズ -> z=1: 照射先) へ伸びる滑らかな法線付き3D円錐台メッシュ
	const float slopeZ = -(baseRadius - apexRadius);
	for (int r = 0; r <= heightRings; ++r)
	{
		float v = static_cast<float>(r) / static_cast<float>(heightRings);
		float curRadius = apexRadius + (baseRadius - apexRadius) * v;

		for (int s = 0; s <= radialSegs; ++s)
		{
			float u = static_cast<float>(s) / static_cast<float>(radialSegs);
			float theta = u * 2.0f * kPi;
			float c = std::cos(theta);
			float sn = std::sin(theta);

			Vector3 nrm = { c, sn, slopeZ };
			float nLen = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y + nrm.z * nrm.z);
			if (nLen > 1e-5f) { nrm.x /= nLen; nrm.y /= nLen; nrm.z /= nLen; }

			Sprite::VertexData vd{};
			vd.position = { c * curRadius, sn * curRadius, v, 1.0f };
			vd.texcoord = { u, v };
			vd.normal = nrm;
			data.vertices.push_back(vd);
		}
	}

	const uint32_t stride = static_cast<uint32_t>(radialSegs + 1);
	for (int r = 0; r < heightRings; ++r)
	{
		for (int s = 0; s < radialSegs; ++s)
		{
			uint32_t i0 = static_cast<uint32_t>(r) * stride + static_cast<uint32_t>(s);
			uint32_t i1 = i0 + 1;
			uint32_t i2 = i0 + stride;
			uint32_t i3 = i2 + 1;

			data.indices.push_back(i0);
			data.indices.push_back(i2);
			data.indices.push_back(i1);

			data.indices.push_back(i1);
			data.indices.push_back(i2);
			data.indices.push_back(i3);
		}
	}
	data.boundingRadius = 40.0f;
}

void GameOverSequenceController::InitializeVolumetricPipeline()
{
	volumetricAdditivePso_.Reset();
	if (!scene_ || !scene_->directXCom || !scene_->object3dCom) return;

	DirectXCom* dx = scene_->directXCom;
	auto rootSig = scene_->object3dCom->GetRootSignature();
	if (!rootSig) return;

	std::wstring vsPath;
	std::wstring psPath;
	const std::wstring vsCandidates[2] = { L"Resources/shaders/Object3D.VS.hlsl", L"project/Resources/shaders/Object3D.VS.hlsl" };
	const std::wstring psCandidates[2] = { L"Resources/shaders/MedevacVolumetric.PS.hlsl", L"project/Resources/shaders/MedevacVolumetric.PS.hlsl" };

	for (const auto& p : vsCandidates)
	{
		if (std::filesystem::exists(p)) { vsPath = p; break; }
	}
	for (const auto& p : psCandidates)
	{
		if (std::filesystem::exists(p)) { psPath = p; break; }
	}
	if (vsPath.empty() || psPath.empty()) return;

	auto vsBlob = dx->CompileShader(vsPath, L"vs_6_0", dx->GetDxcUtils().Get(), dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);
	auto psBlob = dx->CompileShader(psPath, L"ps_6_0", dx->GetDxcUtils().Get(), dx->GetDxcCompiler(), dx->GetIncludeHandler(), std::cout);
	if (!vsBlob || !psBlob) return;

	D3D12_INPUT_ELEMENT_DESC inputElems[3]{};
	inputElems[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElems[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	inputElems[2] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
	desc.pRootSignature = rootSig.Get();
	desc.InputLayout = { inputElems, _countof(inputElems) };
	desc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	desc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

	// リアル光学加算合成 (SRC_ALPHA + ONE) ＆ 3D深度テスト有効・深度書き込みゼロ
	auto& rt = desc.BlendState.RenderTarget[0];
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	rt.BlendEnable = TRUE;
	rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt.BlendOp = D3D12_BLEND_OP_ADD;
	rt.DestBlend = D3D12_BLEND_ONE;
	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt.DestBlendAlpha = D3D12_BLEND_ONE;

	desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.SampleDesc.Count = 1;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	dx->GetDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&volumetricAdditivePso_));
}

void GameOverSequenceController::DrawVolumetricObject(Object3d* obj, const RenderContext& ctx)
{
	if (!obj || !ctx.commandList || !ctx.camera || !scene_ || !scene_->object3dCom) return;
	if (!volumetricAdditivePso_) return;

	ctx.commandList->SetGraphicsRootSignature(scene_->object3dCom->GetRootSignature().Get());
	ctx.commandList->SetPipelineState(volumetricAdditivePso_.Get());

	if (whiteTexIndex_ != UINT32_MAX)
	{
		D3D12_GPU_DESCRIPTOR_HANDLE whiteSrv = TextureManager::GetInstance()->GetSrvHandleGPU(whiteTexIndex_);
		if (whiteSrv.ptr != 0)
		{
			ctx.commandList->SetGraphicsRootDescriptorTable(2, whiteSrv);
		}
	}
	if (ctx.camera->GetCameraGpuAddress() != 0)
	{
		ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.camera->GetCameraGpuAddress());
	}

	obj->DrawInternal(ctx);
}

// -----------------------------------------------------------------------------
// ライフサイクル
// -----------------------------------------------------------------------------

GameOverSequenceController::GameOverSequenceController(GamePlayScene* scene)
	: scene_(scene)
{
}

GameOverSequenceController::~GameOverSequenceController()
{
	Finalize();
}

void GameOverSequenceController::Initialize()
{
	isActive_ = false;
	timer_ = 0.0f;
	transitionTriggered_ = false;
	groundImpactTriggered_ = false;
	alarmTriggered_ = false;
	flareActive_ = false;
	heliVisible_ = false;
	gunSmokeTimer_ = 0.0f;
	featherTimer_ = 0.0f;
	skidDustTimer_ = 0.0f;
	flareSmokeTimer_ = 0.0f;
	flareSparkTimer_ = 0.0f;
	downwashTimer_ = 0.0f;
	rotorVaporTimer_ = 0.0f;
	strobeTimer_ = 0.0f;
	darkVignetteAlpha_ = 0.0f;
	nearbyObstacles_.clear();

	if (!scene_ || !scene_->object3dCom || !scene_->camera_) return;

	auto* tm = TextureManager::GetInstance();
	if (tm)
	{
		circle2TextureIndex_ = tm->Load("Resources/CG4/circle2.png");
		if (circle2TextureIndex_ == UINT32_MAX)
		{
			circle2TextureIndex_ = tm->Load("Resources/circle2.png");
		}
		whitePuffTextureIndex_ = tm->Load("Resources/smoke_puff_white.png");
		if (whitePuffTextureIndex_ == UINT32_MAX)
		{
			whitePuffTextureIndex_ = circle2TextureIndex_;
		}
		smokeDarkTextureIndex_ = tm->Load("Resources/smoke_dark.png");
		featherTextureIndex_ = tm->Load("Resources/feather.png");
		canisterTexIndex_ = tm->Load("Resources/container_military.png");
		whiteTexIndex_ = tm->Load("Resources/CG4/human/white.png");
		if (whiteTexIndex_ == UINT32_MAX)
		{
			whiteTexIndex_ = tm->Load("Resources/white.png");
		}
		woundedDuckTexIndex_ = tm->Load("Resources/duck_player_wounded.png");
		medicDuckTexIndex_ = tm->Load("Resources/duck_medic.png");
		normalTexIndex_ = tm->Load("Resources/duck.png");
		heliTexIndex_ = tm->Load("Resources/helicopter_texture.png");
		leafTextureIndex_ = tm->Load("Resources/leaf.png");
	}

	const uint32_t fallbackTex = (whiteTexIndex_ != UINT32_MAX) ? whiteTexIndex_ : 0;
	const uint32_t milTex = (canisterTexIndex_ != UINT32_MAX) ? canisterTexIndex_ : fallbackTex;
	const uint32_t heliTex = (heliTexIndex_ != UINT32_MAX) ? heliTexIndex_ : fallbackTex;
	const uint32_t medicTex = (medicDuckTexIndex_ != UINT32_MAX) ? medicDuckTexIndex_
		: ((normalTexIndex_ != UINT32_MAX) ? normalTexIndex_ : fallbackTex);

	// 1. 吹き飛ぶライフルオブジェクトの生成
	{
		Object3d::ModelData rifleData{};
		BuildRifleModel(rifleData);
		rifleData.material.textureFilePath = "Resources/container_military.png";
		rifleData.material.textureIndex = milTex;

		droppedRifle_ = std::make_unique<Object3d>();
		droppedRifle_->Initialize(scene_->object3dCom, rifleData);
		droppedRifle_->SetCamera(scene_->camera_);
		droppedRifle_->SetColor({ 0.20f, 0.20f, 0.22f, 1.0f });
		droppedRifle_->SetEnableLighting(true);
		droppedRifle_->SetAllowWireframeOverlay(false);
	}

	// 2. 緊急救難スモークフレア発煙筒（AN-M18）の生成
	{
		Object3d::ModelData canData{};
		BuildFlareCanisterModel(canData);
		canData.material.textureFilePath = "Resources/container_military.png";
		canData.material.textureIndex = milTex;

		flareCanister_ = std::make_unique<Object3d>();
		flareCanister_->Initialize(scene_->object3dCom, canData);
		flareCanister_->SetCamera(scene_->camera_);
		flareCanister_->SetColor({ 0.32f, 0.37f, 0.27f, 1.0f }); // 軍用オリーブドラブ円筒缶
		flareCanister_->SetEnableLighting(true);
		flareCanister_->SetAllowWireframeOverlay(false);

		Object3d::ModelData capData{};
		BuildFlareGlowCapModel(capData);
		capData.material.textureFilePath = "Resources/CG4/human/white.png";
		capData.material.textureIndex = fallbackTex;

		flareGlowCap_ = std::make_unique<Object3d>();
		flareGlowCap_->Initialize(scene_->object3dCom, capData);
		flareGlowCap_->SetCamera(scene_->camera_);
		flareGlowCap_->SetColor({ 1.0f, 0.35f, 0.15f, 1.0f });
		flareGlowCap_->SetEnableLighting(false);
		flareGlowCap_->SetAllowWireframeOverlay(false);
	}

	// 3. GameOverScene と完全同一の 3D 救難ヘリコプター（MEDEVAC）＆ 機内衛生兵クルー生成
	{
		Object3d::ModelData bodyData = Object3d::LoadObjFile("Resources", "helicopter_body.obj");
		bodyData.material.textureFilePath = "Resources/helicopter_texture.png";
		bodyData.material.textureIndex = heliTex;
		bodyData.boundingRadius = 100.0f;
		heliBody_ = std::make_unique<Object3d>();
		heliBody_->Initialize(scene_->object3dCom, bodyData);
		heliBody_->SetCamera(scene_->camera_);
		heliBody_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliBody_->SetEnableLighting(true);
		heliBody_->SetAllowWireframeOverlay(false);

		Object3d::ModelData rotorData = Object3d::LoadObjFile("Resources", "helicopter_rotor.obj");
		rotorData.material.textureFilePath = "Resources/helicopter_texture.png";
		rotorData.material.textureIndex = heliTex;
		rotorData.boundingRadius = 100.0f;

		heliMainRotor_ = std::make_unique<Object3d>();
		heliMainRotor_->Initialize(scene_->object3dCom, rotorData);
		heliMainRotor_->SetCamera(scene_->camera_);
		heliMainRotor_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliMainRotor_->SetEnableLighting(true);
		heliMainRotor_->SetAllowWireframeOverlay(false);

		heliMainRotorBlur1_ = std::make_unique<Object3d>();
		heliMainRotorBlur1_->Initialize(scene_->object3dCom, rotorData);
		heliMainRotorBlur1_->SetCamera(scene_->camera_);
		heliMainRotorBlur1_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliMainRotorBlur1_->SetColor({ 0.85f, 0.88f, 0.90f, 0.55f });
		heliMainRotorBlur1_->SetEnableLighting(true);
		heliMainRotorBlur1_->SetAllowWireframeOverlay(false);

		heliMainRotorBlur2_ = std::make_unique<Object3d>();
		heliMainRotorBlur2_->Initialize(scene_->object3dCom, rotorData);
		heliMainRotorBlur2_->SetCamera(scene_->camera_);
		heliMainRotorBlur2_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliMainRotorBlur2_->SetColor({ 0.80f, 0.84f, 0.88f, 0.35f });
		heliMainRotorBlur2_->SetEnableLighting(true);
		heliMainRotorBlur2_->SetAllowWireframeOverlay(false);

		Object3d::ModelData tailData = Object3d::LoadObjFile("Resources", "helicopter_tailrotor.obj");
		tailData.material.textureFilePath = "Resources/helicopter_texture.png";
		tailData.material.textureIndex = heliTex;
		tailData.boundingRadius = 100.0f;
		heliTailRotor_ = std::make_unique<Object3d>();
		heliTailRotor_->Initialize(scene_->object3dCom, tailData);
		heliTailRotor_->SetCamera(scene_->camera_);
		heliTailRotor_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliTailRotor_->SetEnableLighting(true);
		heliTailRotor_->SetAllowWireframeOverlay(false);

		Object3d::ModelData doorData{};
		BuildHelicopterDoorModel(doorData);
		doorData.material.textureFilePath = "Resources/helicopter_texture.png";
		doorData.material.textureIndex = heliTex;
		heliCargoDoor_ = std::make_unique<Object3d>();
		heliCargoDoor_->Initialize(scene_->object3dCom, doorData);
		heliCargoDoor_->SetCamera(scene_->camera_);
		heliCargoDoor_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliCargoDoor_->SetEnableLighting(true);
		heliCargoDoor_->SetAllowWireframeOverlay(false);

		Object3d::ModelData ropeData = Object3d::LoadObjFile("Resources", "fast_rope.obj");
		ropeData.material.textureFilePath = "Resources/helicopter_texture.png";
		ropeData.material.textureIndex = heliTex;
		ropeData.boundingRadius = 50.0f;
		heliFastRope_ = std::make_unique<Object3d>();
		heliFastRope_->Initialize(scene_->object3dCom, ropeData);
		heliFastRope_->SetCamera(scene_->camera_);
		heliFastRope_->SetScale({ 0.0f, 0.0f, 0.0f });
		heliFastRope_->SetEnableLighting(true);
		heliFastRope_->SetAllowWireframeOverlay(false);

		Object3d::ModelData shadowData{};
		BuildShadowDiscModel(shadowData, 3.2f, 5.0f);
		shadowData.material.textureFilePath = "Resources/circle2.png";
		shadowData.material.textureIndex = (circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : fallbackTex;
		heliShadow_ = std::make_unique<Object3d>();
		heliShadow_->Initialize(scene_->object3dCom, shadowData);
		heliShadow_->SetCamera(scene_->camera_);
		heliShadow_->SetColor({ 0.03f, 0.05f, 0.08f, 0.45f });
		heliShadow_->SetEnableLighting(false);
		heliShadow_->SetAllowWireframeOverlay(false);

		// HLSL 専用ボリュメトリック加算シェーダーPSOの初期化（2Dべた張りではなく本物の3D大気散乱＆地上コースティクス）
		InitializeVolumetricPipeline();

		// (A) 救難ヘリのハロゲン投光器が地表に落とす 3D HLSL コースティクス・ライトプール (Mode 1)
		Object3d::ModelData spotData{};
		BuildShadowDiscModel(spotData, 2.35f, 2.35f);
		spotData.material.textureFilePath = "Resources/CG4/human/white.png";
		spotData.material.textureIndex = fallbackTex;
		searchlightSpot_ = std::make_unique<Object3d>();
		searchlightSpot_->Initialize(scene_->object3dCom, spotData);
		searchlightSpot_->SetCamera(scene_->camera_);
		searchlightSpot_->SetColor({ 1.0f, 0.90f, 0.58f, 0.0f });
		searchlightSpot_->SetFresnelF0(1.0f);
		searchlightSpot_->SetEnableLighting(false);
		searchlightSpot_->SetAllowWireframeOverlay(false);

		// (B) 3D ボリュメトリック・サーチライト光条コーン（外側ミー散乱シェル ＋ 内側高輝度ハロゲンコア, Mode 0）
		Object3d::ModelData outerConeData{};
		BuildVolumetricConeModel(outerConeData, 0.11f, 1.95f, 32, 8);
		outerConeData.material.textureIndex = fallbackTex;
		volumetricOuterBeam_ = std::make_unique<Object3d>();
		volumetricOuterBeam_->Initialize(scene_->object3dCom, outerConeData);
		volumetricOuterBeam_->SetCamera(scene_->camera_);
		volumetricOuterBeam_->SetFresnelF0(0.0f);
		volumetricOuterBeam_->SetEnableLighting(false);
		volumetricOuterBeam_->SetAllowWireframeOverlay(false);

		Object3d::ModelData innerConeData{};
		BuildVolumetricConeModel(innerConeData, 0.06f, 0.82f, 28, 8);
		innerConeData.material.textureIndex = fallbackTex;
		volumetricInnerBeam_ = std::make_unique<Object3d>();
		volumetricInnerBeam_->Initialize(scene_->object3dCom, innerConeData);
		volumetricInnerBeam_->SetCamera(scene_->camera_);
		volumetricInnerBeam_->SetFresnelF0(0.0f);
		volumetricInnerBeam_->SetEnableLighting(false);
		volumetricInnerBeam_->SetAllowWireframeOverlay(false);

		// (C) 開放カーゴドア内部から漏れ出す暖色アンバー3D室内投光コーン (Mode 0)
		Object3d::ModelData cabinConeData{};
		BuildVolumetricConeModel(cabinConeData, 0.42f, 1.45f, 24, 6);
		cabinConeData.material.textureIndex = fallbackTex;
		volumetricCabinBeam_ = std::make_unique<Object3d>();
		volumetricCabinBeam_->Initialize(scene_->object3dCom, cabinConeData);
		volumetricCabinBeam_->SetCamera(scene_->camera_);
		volumetricCabinBeam_->SetFresnelF0(0.0f);
		volumetricCabinBeam_->SetEnableLighting(false);
		volumetricCabinBeam_->SetAllowWireframeOverlay(false);

		// (D) 3D 地上衝撃波 ＆ 真紅ビーコン電波パルスディスク (Mode 2: 遮蔽物との正確な前後関係を保持)
		Object3d::ModelData unitWaveData{};
		BuildShadowDiscModel(unitWaveData, 1.0f, 1.0f);
		unitWaveData.material.textureIndex = fallbackTex;

		impactShockDisc_ = std::make_unique<Object3d>();
		impactShockDisc_->Initialize(scene_->object3dCom, unitWaveData);
		impactShockDisc_->SetCamera(scene_->camera_);
		impactShockDisc_->SetFresnelF0(2.055f);
		impactShockDisc_->SetEnableLighting(false);
		impactShockDisc_->SetAllowWireframeOverlay(false);

		beaconWaveDisc1_ = std::make_unique<Object3d>();
		beaconWaveDisc1_->Initialize(scene_->object3dCom, unitWaveData);
		beaconWaveDisc1_->SetCamera(scene_->camera_);
		beaconWaveDisc1_->SetFresnelF0(2.045f);
		beaconWaveDisc1_->SetEnableLighting(false);
		beaconWaveDisc1_->SetAllowWireframeOverlay(false);

		beaconWaveDisc2_ = std::make_unique<Object3d>();
		beaconWaveDisc2_->Initialize(scene_->object3dCom, unitWaveData);
		beaconWaveDisc2_->SetCamera(scene_->camera_);
		beaconWaveDisc2_->SetFresnelF0(2.045f);
		beaconWaveDisc2_->SetEnableLighting(false);
		beaconWaveDisc2_->SetAllowWireframeOverlay(false);

		// 機内から顔を出して救助を指揮する 2羽の衛生兵アヒル ＆ 担架（GameOverScene へ直結）
		Object3d::ModelData medicData = Object3d::LoadObjFile("Resources", "player.obj");
		medicData.material.textureFilePath = "Resources/duck_medic.png";
		medicData.material.textureIndex = medicTex;
		medicData.boundingRadius = 8.0f;

		cabinFrontMedic_ = std::make_unique<Object3d>();
		cabinFrontMedic_->Initialize(scene_->object3dCom, medicData);
		cabinFrontMedic_->SetCamera(scene_->camera_);
		cabinFrontMedic_->SetScale({ 0.38f, 0.38f, 0.38f });
		cabinFrontMedic_->SetEnableLighting(true);
		cabinFrontMedic_->SetAllowWireframeOverlay(false);

		cabinRearMedic_ = std::make_unique<Object3d>();
		cabinRearMedic_->Initialize(scene_->object3dCom, medicData);
		cabinRearMedic_->SetCamera(scene_->camera_);
		cabinRearMedic_->SetScale({ 0.38f, 0.38f, 0.38f });
		cabinRearMedic_->SetEnableLighting(true);
		cabinRearMedic_->SetAllowWireframeOverlay(false);

		Object3d::ModelData stretcherData{};
		BuildStretcherModel(stretcherData);
		stretcherData.material.textureFilePath = "Resources/container_military.png";
		stretcherData.material.textureIndex = milTex;
		cabinStretcher_ = std::make_unique<Object3d>();
		cabinStretcher_->Initialize(scene_->object3dCom, stretcherData);
		cabinStretcher_->SetCamera(scene_->camera_);
		cabinStretcher_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		cabinStretcher_->SetEnableLighting(true);
		cabinStretcher_->SetAllowWireframeOverlay(false);
	}

	// オーディオのロード
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		alarmSoundId_ = am->Load("Resources/Alarm01.wav");
		duckSoundId_ = am->Load("Resources/Audio/SE_Duck.mp3");
	}
}

void GameOverSequenceController::SuppressGameplayHUD()
{
	if (!scene_) return;

	// ゲームプレイ中のHPバー・スタミナバー・リロードバー・照準レティクル・敵HPバー・集中線・赤ビネットを完全非表示化
	for (auto& sp : scene_->sprites)
	{
		if (sp)
		{
			sp->SetSize({ 0.0f, 0.0f });
			sp->Update();
		}
	}
	scene_->vignetteAlpha_ = 0.0f;
	scene_->floatingTexts_.clear();
	scene_->playerSoundTimer_ = 0.0f;
	scene_->playerSoundRadius_ = 0.0f;

	if (!hasSavedHudFlags_)
	{
		savedShowPerfTracker_ = scene_->showPerformanceTracker_;
		savedShowDebugGizmos_ = scene_->showDebugGizmos_;
		hasSavedHudFlags_ = true;
	}
	scene_->showPerformanceTracker_ = false;
	scene_->showDebugGizmos_ = false;
}

void GameOverSequenceController::RestoreGameplayHUD()
{
	if (scene_ && hasSavedHudFlags_)
	{
		scene_->showPerformanceTracker_ = savedShowPerfTracker_;
		scene_->showDebugGizmos_ = savedShowDebugGizmos_;
	}
	hasSavedHudFlags_ = false;
}

void GameOverSequenceController::Finalize()
{
	RestoreSceneLighting();
	RestoreGameplayHUD();

	isActive_ = false;
	nearbyObstacles_.clear();
	if (scene_ && scene_->GetPlayer() && normalTexIndex_ != UINT32_MAX)
	{
		scene_->GetPlayer()->SetTextureIndex(normalTexIndex_);
	}
	if (scene_ && scene_->camera_)
	{
		scene_->camera_->SetFovY(initialCamFov_);
	}

	droppedRifle_.reset();
	flareCanister_.reset();
	flareGlowCap_.reset();
	heliBody_.reset();
	heliMainRotor_.reset();
	heliMainRotorBlur1_.reset();
	heliMainRotorBlur2_.reset();
	heliTailRotor_.reset();
	heliCargoDoor_.reset();
	heliFastRope_.reset();
	heliShadow_.reset();
	searchlightSpot_.reset();
	volumetricOuterBeam_.reset();
	volumetricInnerBeam_.reset();
	volumetricCabinBeam_.reset();
	impactShockDisc_.reset();
	beaconWaveDisc1_.reset();
	beaconWaveDisc2_.reset();
	volumetricAdditivePso_.Reset();
	cabinFrontMedic_.reset();
	cabinRearMedic_.reset();
	cabinStretcher_.reset();
}

// -----------------------------------------------------------------------------
// 周辺障害物OBBの収集・全身埋まり回避・360度クリア視線カメラリグ選定
// （※ステージ上のオブジェクトは1つも消去・縮小しない！）
// -----------------------------------------------------------------------------

void GameOverSequenceController::CollectNearbyObstacles()
{
	nearbyObstacles_.clear();
	if (!scene_) return;

	auto addBoxFromCollider = [&](BoxCollider* col) {
		if (!col || !col->IsEnabled()) return;
		const Vector3 center = col->GetWorldPosition();
		const Vector3 sz = col->GetSize();
		const Vector3 rot = col->GetWorldRotation();

		// 川やマップ外周の巨大進入禁止バリア
		if (sz.x > 14.0f || sz.z > 14.0f)
		{
			ObstacleBox b{};
			b.center = center;
			b.size = sz;
			b.rotation = rot;
			b.isBoundary = true;
			b.isCanopy = false;
			nearbyObstacles_.push_back(b);
			return;
		}

		const float dx = center.x - deathPos_.x;
		const float dz = center.z - deathPos_.z;
		if (dx * dx + dz * dz > 26.0f * 26.0f) return;

		// 針葉樹（幹コライダーは細いが、上部には円錐状の枝葉が広がっている）
		if (sz.x < 1.15f && sz.z < 1.15f && sz.y > 3.2f)
		{
			ObstacleBox trunk{};
			trunk.center = center;
			trunk.size = { sz.x + 0.16f, sz.y, sz.z + 0.16f };
			trunk.rotation = rot;
			nearbyObstacles_.push_back(trunk);

			ObstacleBox canopy{};
			canopy.center = { center.x, groundY_ + sz.y * 0.62f, center.z };
			canopy.size = { sz.x * 3.8f, sz.y * 0.78f, sz.z * 3.8f };
			canopy.rotation = rot;
			canopy.isCanopy = true;
			nearbyObstacles_.push_back(canopy);
			return;
		}

		// 通常遮蔽物（Obstacle.cpp で 0.90倍に縮小されているXZコライダーを見た目メッシュ実寸へ復元）
		ObstacleBox solid{};
		solid.center = center;
		solid.size = { sz.x / 0.90f + 0.08f, sz.y + 0.06f, sz.z / 0.90f + 0.08f };
		solid.rotation = rot;
		nearbyObstacles_.push_back(solid);
	};

	for (const auto& obs : scene_->GetObstacles())
	{
		if (!obs) continue;
		addBoxFromCollider(obs->GetCollider());
		for (const auto& extra : obs->GetExtraColliders())
		{
			addBoxFromCollider(extra.get());
		}
	}

	// コライダーを持たない木製チュートリアル看板（signpost.obj）も実寸OBBとして登録
	for (const auto& sign : scene_->GetTutorialSigns())
	{
		if (!sign) continue;
		Vector3 sp = sign->GetPosition();
		float dx = sp.x - deathPos_.x;
		float dz = sp.z - deathPos_.z;
		if (dx * dx + dz * dz <= 20.0f * 20.0f)
		{
			ObstacleBox sb{};
			sb.center = { sp.x, groundY_ + 0.90f, sp.z };
			sb.size = { 1.15f, 1.80f, 0.50f };
			nearbyObstacles_.push_back(sb);
		}
	}

	// 未破壊の標的スタンドも実寸OBBとして登録
	for (const auto& target : scene_->GetTargets())
	{
		if (!target || target->IsDead()) continue;
		Vector3 tp = target->GetPosition();
		float dx = tp.x - deathPos_.x;
		float dz = tp.z - deathPos_.z;
		if (dx * dx + dz * dz <= 20.0f * 20.0f)
		{
			ObstacleBox tb{};
			tb.center = { tp.x, groundY_ + 0.85f, tp.z };
			tb.size = { 1.10f, 1.70f, 0.50f };
			nearbyObstacles_.push_back(tb);
		}
	}
}

void GameOverSequenceController::ResolveDuckPoseCollision(
	Vector3& ioPos,
	const Vector3& rot,
	const Vector3& scale) const
{
	// player.obj (足元 Y=0 〜 ヘルメット頂部 Y=1.756, 横幅 ±0.74) に沿った5個の全身衝突球
	struct BodySphere { Vector3 localCenter; float radius; };
	static const BodySphere kBodySpheres[5] = {
		{ { 0.0f, 0.24f, 0.00f }, 0.42f }, // 足元
		{ { 0.0f, 0.62f, 0.02f }, 0.56f }, // 腰・胴下部
		{ { 0.0f, 1.02f, 0.05f }, 0.62f }, // 胸部・両翼
		{ { 0.0f, 1.38f, 0.08f }, 0.52f }, // 首・くちばし
		{ { 0.0f, 1.66f, 0.04f }, 0.45f }, // 頭部・ヘルメット先端
	};

	for (int iter = 0; iter < 4; ++iter)
	{
		Matrix4x4 W = MakeAffineMatrix(scale, rot, ioPos);
		Vector3 maxPush = { 0.0f, 0.0f, 0.0f };
		float maxPen = 0.0f;

		for (const auto& bs : kBodySpheres)
		{
			Vector3 worldPt = {
				bs.localCenter.x * W.m[0][0] + bs.localCenter.y * W.m[1][0] + bs.localCenter.z * W.m[2][0] + W.m[3][0],
				bs.localCenter.x * W.m[0][1] + bs.localCenter.y * W.m[1][1] + bs.localCenter.z * W.m[2][1] + W.m[3][1],
				bs.localCenter.x * W.m[0][2] + bs.localCenter.y * W.m[1][2] + bs.localCenter.z * W.m[2][2] + W.m[3][2]
			};

			for (const auto& ob : nearbyObstacles_)
			{
				if (ob.isCanopy) continue;
				Vector3 pushXZ{};
				float pen = 0.0f;
				if (ComputeSphereOBBPushOutXZ(worldPt, bs.radius, ob.center, ob.size, ob.rotation, pushXZ, pen))
				{
					if (pen > maxPen)
					{
						maxPen = pen;
						maxPush = pushXZ;
					}
				}
			}
		}

		if (maxPen <= 1e-4f) break;
		ioPos.x += maxPush.x;
		ioPos.z += maxPush.z;
	}
}

void GameOverSequenceController::ResolveHorizontalPointClearance(
	Vector3& ioPos,
	float radius,
	float testHeightOffset) const
{
	for (int iter = 0; iter < 3; ++iter)
	{
		Vector3 testPt = { ioPos.x, groundY_ + testHeightOffset, ioPos.z };
		Vector3 maxPush = { 0.0f, 0.0f, 0.0f };
		float maxPen = 0.0f;
		for (const auto& ob : nearbyObstacles_)
		{
			if (ob.isCanopy) continue;
			Vector3 pushXZ{};
			float pen = 0.0f;
			if (ComputeSphereOBBPushOutXZ(testPt, radius, ob.center, ob.size, ob.rotation, pushXZ, pen))
			{
				if (pen > maxPen)
				{
					maxPen = pen;
					maxPush = pushXZ;
				}
			}
		}
		if (maxPen <= 1e-4f) break;
		ioPos.x += maxPush.x;
		ioPos.z += maxPush.z;
	}
}

void GameOverSequenceController::PlanSafeDuckFallPose(float defaultKillerYaw)
{
	// 16方位の転倒候補（本来の被弾ノックバック方向を最優先しつつ、狭い通路でも頭・胴が一切埋まらない向きを探索）
	const float preferredKnockAngle = std::atan2(flingDir_.x, flingDir_.z);
	static const float kCandidateDistances[4] = { 1.00f, 0.62f, 0.28f, 0.0f };

	float bestScore = -1e9f;
	Vector3 bestLanding = deathPos_;
	Vector3 bestFlingDir = flingDir_;
	float bestFallYaw = defaultKillerYaw;

	struct BodySphere { Vector3 localCenter; float radius; };
	static const BodySphere kTestSpheres[5] = {
		{ { 0.0f, 0.24f, 0.00f }, 0.42f },
		{ { 0.0f, 0.62f, 0.02f }, 0.56f },
		{ { 0.0f, 1.02f, 0.05f }, 0.62f },
		{ { 0.0f, 1.38f, 0.08f }, 0.52f },
		{ { 0.0f, 1.66f, 0.04f }, 0.45f },
	};

	for (int aIdx = 0; aIdx < 16; ++aIdx)
	{
		float knockAngle = preferredKnockAngle + static_cast<float>(aIdx) * (2.0f * kPi / 16.0f);
		Vector3 candFling = { std::sin(knockAngle), 0.0f, std::cos(knockAngle) };
		// 仰向け (-1.22 rad) で倒れるため、顔・胸が向く正面Yawは candFling の逆方向
		float candYaw = knockAngle + kPi;

		for (float dist : kCandidateDistances)
		{
			// 移動経路の途中に壁がないかチェック
			if (dist > 0.05f)
			{
				float hitD = dist + 0.45f;
				bool pathBlocked = false;
				for (const auto& ob : nearbyObstacles_)
				{
					if (ob.isCanopy) continue;
					float d = 0.0f;
					if (CheckRayOBB(deathPos_ + Vector3{ 0.0f, 0.55f, 0.0f }, candFling, dist + 0.45f, ob.center, ob.size, ob.rotation, d))
					{
						if (d < hitD) pathBlocked = true;
					}
				}
				if (pathBlocked) continue;
			}

			Vector3 candPos = deathPos_ + candFling * dist;
			candPos.y = groundY_ + 0.14f;
			Vector3 testRot = { -1.22f, candYaw, 0.0f };
			Vector3 testScale = { 1.04f, 0.96f, 1.04f };
			Matrix4x4 W = MakeAffineMatrix(testScale, testRot, candPos);

			float totalPen = 0.0f;
			for (const auto& bs : kTestSpheres)
			{
				Vector3 worldPt = {
					bs.localCenter.x * W.m[0][0] + bs.localCenter.y * W.m[1][0] + bs.localCenter.z * W.m[2][0] + W.m[3][0],
					bs.localCenter.x * W.m[0][1] + bs.localCenter.y * W.m[1][1] + bs.localCenter.z * W.m[2][1] + W.m[3][1],
					bs.localCenter.x * W.m[0][2] + bs.localCenter.y * W.m[1][2] + bs.localCenter.z * W.m[2][2] + W.m[3][2]
				};
				for (const auto& ob : nearbyObstacles_)
				{
					if (ob.isCanopy) continue;
					Vector3 pushXZ{};
					float pen = 0.0f;
					if (ComputeSphereOBBPushOutXZ(worldPt, bs.radius, ob.center, ob.size, ob.rotation, pushXZ, pen))
					{
						totalPen += pen;
					}
				}
			}

			float angleDiff = std::abs(NormalizeAngleDiff(knockAngle, preferredKnockAngle));
			float score = -totalPen * 120.0f - angleDiff * 2.5f + dist * 0.8f;
			if (score > bestScore)
			{
				bestScore = score;
				bestLanding = { candPos.x, groundY_, candPos.z };
				bestFlingDir = candFling;
				bestFallYaw = candYaw;
			}
		}
	}

	// 最終セーフティ押し出しで1ミリのめり込みも残さない
	Vector3 refinedPos = { bestLanding.x, groundY_ + 0.14f, bestLanding.z };
	ResolveDuckPoseCollision(refinedPos, Vector3{ -1.22f, bestFallYaw, 0.0f }, Vector3{ 1.04f, 0.96f, 1.04f });

	safeLandingPos_ = { refinedPos.x, groundY_, refinedPos.z };
	flingDir_ = bestFlingDir;
	fallBaseYaw_ = bestFallYaw;
}

bool GameOverSequenceController::IsCameraSightClear(
	const Vector3& eye,
	const std::vector<Vector3>& targets) const
{
	// 1. カメラ自身が障害物や樹冠の内部に入っていないか確認
	for (const auto& ob : nearbyObstacles_)
	{
		if (ob.isBoundary) continue;
		Vector3 pushXZ{};
		float pen = 0.0f;
		if (ComputeSphereOBBPushOutXZ(eye, 0.60f, ob.center, ob.size, ob.rotation, pushXZ, pen))
		{
			return false;
		}
	}

	// 2. カメラからアヒルの各部位（頭・胸・足）への視線レイが遮蔽物に一切当たらないか確認
	for (const Vector3& tgt : targets)
	{
		Vector3 diff = tgt - eye;
		float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
		if (len < 0.05f) continue;
		Vector3 dir = diff * (1.0f / len);
		float maxTestDist = (std::max)(0.1f, len - 0.18f);

		for (const auto& ob : nearbyObstacles_)
		{
			if (ob.isBoundary) continue;
			float hitD = 0.0f;
			if (CheckRayOBB(eye, dir, maxTestDist, ob.center, ob.size, ob.rotation, hitD))
			{
				return false;
			}
		}
	}

	return true;
}

void GameOverSequenceController::SelectUnobstructedCameraRig()
{
	// 最終転倒姿勢でのアヒルの足・胸・頭の位置を算出
	const Vector3 landPos = { safeLandingPos_.x, groundY_ + 0.14f, safeLandingPos_.z };
	const Matrix4x4 W = MakeAffineMatrix(Vector3{ 1.04f, 0.96f, 1.04f }, Vector3{ -1.22f, fallBaseYaw_, 0.0f }, landPos);
	auto localPt = [&](float ly, float lz) -> Vector3 {
		return {
			ly * W.m[1][0] + lz * W.m[2][0] + W.m[3][0],
			ly * W.m[1][1] + lz * W.m[2][1] + W.m[3][1],
			ly * W.m[1][2] + lz * W.m[2][2] + W.m[3][2]
		};
	};

	const Vector3 focusCenter = localPt(0.88f, 0.06f);
	const std::vector<Vector3> sightTargets = {
		localPt(0.30f, 0.02f), // 足元付近
		focusCenter,           // 胸・顔中心
		localPt(1.52f, 0.06f)  // ヘルメット側
	};

	// ゲームプレイ中の通常見下ろしカメラの方位（通常は南側 -Z: azimuth = π）を基準にすれば切り替わりが最も自然
	Vector3 initOff = initialCamPos_ - deathPos_;
	float preferredAzimuth = (initOff.x * initOff.x + initOff.z * initOff.z > 0.25f)
		? std::atan2(initOff.x, initOff.z)
		: kPi;

	// 3段階の俯瞰カメラティア（ゆったり引き目の構図で周囲状況も一望でき、上下黒帯にも絶対に見切れない）
	struct RigTier
	{
		float hDist1; float height1;
		float hDist2; float height2;
		float tierBonus;
	};
	static const RigTier kTiers[3] = {
		{ 5.85f, 4.95f, 7.10f, 5.20f, 30.0f }, // Tier 0: 倒れた自機＋上空救難ヘリをゆったり一望できる映画的中俯瞰
		{ 4.85f, 5.75f, 5.95f, 6.00f, 18.0f }, // Tier 1: コンテナ路地でも視線が抜けるやや高めの俯瞰
		{ 3.75f, 6.65f, 4.75f, 6.95f,  6.0f }, // Tier 2: 四方を高い壁に囲まれた極狭地点用の高俯瞰
	};

	float bestScore = -1e9f;
	Vector3 bestDir = { std::sin(preferredAzimuth), 0.0f, std::cos(preferredAzimuth) };
	RigTier bestTier = kTiers[0];

	for (int tIdx = 0; tIdx < 3; ++tIdx)
	{
		const RigTier& tier = kTiers[tIdx];
		for (int aIdx = 0; aIdx < 24; ++aIdx)
		{
			float az = preferredAzimuth + static_cast<float>(aIdx) * (2.0f * kPi / 24.0f);
			Vector3 dir = { std::sin(az), 0.0f, std::cos(az) };

			Vector3 eye1 = focusCenter + dir * tier.hDist1 + Vector3{ 0.0f, tier.height1, 0.0f };
			Vector3 eye2 = focusCenter + dir * tier.hDist2 + Vector3{ 0.0f, tier.height2, 0.0f };
			Vector3 midDolly = LerpVector3(initialCamPos_, eye1, 0.50f) + Vector3{ 0.0f, 0.65f, 0.0f };

			bool clear1 = IsCameraSightClear(eye1, sightTargets);
			bool clear2 = IsCameraSightClear(eye2, sightTargets);
			if (!clear1 && !clear2) continue;

			bool clearMid = IsCameraSightClear(midDolly, { focusCenter });
			float azPenalty = std::abs(NormalizeAngleDiff(az, preferredAzimuth)) * 3.2f;
			Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
			float sideDot = std::abs(dir.x * perp.x + dir.z * perp.z);

			float score = tier.tierBonus
				+ (clear1 ? 50.0f : 0.0f)
				+ (clear2 ? 40.0f : 0.0f)
				+ (clearMid ? 16.0f : 0.0f)
				+ sideDot * 4.5f
				- azPenalty;

			if (score > bestScore)
			{
				bestScore = score;
				bestDir = dir;
				bestTier = tier;
			}
		}
	}

	// 万一全ティアで部分遮蔽があった場合でも、最も高い安全俯瞰（Tier 2）を選択
	if (bestScore < -1e8f)
	{
		bestTier = kTiers[2];
	}

	camAzimuthDir_ = bestDir;
	rigHorizDistAct1_ = bestTier.hDist1;
	rigHeightAct1_ = bestTier.height1;
	rigHorizDistAct2_ = bestTier.hDist2;
	rigHeightAct2_ = bestTier.height2;

	Vector3 perp = { -flingDir_.z, 0.0f, flingDir_.x };
	float sideDot = camAzimuthDir_.x * perp.x + camAzimuthDir_.z * perp.z;
	cameraSide_ = (sideDot >= 0.0f) ? 1.0f : -1.0f;
}

void GameOverSequenceController::FinalizeDuckPoseForCamera()
{
	// 死亡ごとに3種類の自然な転倒パターンをローテーションし、かつ【どのカメラ方位でも負傷した顔＆くちばしがヘルメットに隠れずカメラ側を向く】姿勢を確定する
	static int sFallVariantCycle = 0;
	fallVariant_ = (sFallVariantCycle++) % 3;

	const float camAz = std::atan2(camAzimuthDir_.x, camAzimuthDir_.z);
	float diffToCam = NormalizeAngleDiff(fallBaseYaw_, camAz);
	float spinSign = (diffToCam >= 0.0f) ? 1.0f : -1.0f;
	if (std::abs(diffToCam) < 0.15f)
	{
		spinSign = (cameraSide_ >= 0.0f) ? 1.0f : -1.0f;
	}

	// MakeAffineMatrix において local +Z(顔・くちばし)の水平向きは sin(ry), cos(ry)。
	// ry を camAz から ±0.68〜1.02 rad（カメラ側斜め正面〜横顔）に収めることで、ヘルメット頂部が手前を向いて顔を隠す現象を100%根絶！
	if (fallVariant_ == 0)
	{
		// パターン0: 衝撃で斜め後方へ吹き飛び、顔とガーゼがカメラへしっかり見える3/4仰向けダウン
		float targetYaw = camAz + 0.68f * spinSign;
		fallFinalRot_ = { -1.24f, targetYaw, 0.14f * spinSign };
	}
	else if (fallVariant_ == 1)
	{
		// パターン1: 被弾の勢いでくるりと半回転スピンしながら横向き〜斜め仰向けに倒れ込むアクション風ダウン
		float targetYaw = camAz + 1.02f * spinSign;
		fallFinalRot_ = { -1.08f, targetYaw, 0.26f * spinSign };
	}
	else
	{
		// パターン2: 一瞬ガクッと前のめりに膝をついて耐えてから、力尽きてカメラ側へ顔を向けながら崩れ落ちるドラマチックダウン
		float targetYaw = camAz - 0.76f * spinSign;
		fallFinalRot_ = { -1.20f, targetYaw, -0.18f * spinSign };
	}

	// 確定したカメラ向き転倒姿勢で再度5球OBB押し出しを実行し、壁・木箱へのめり込みゼロを保証
	Vector3 refinedPos = { safeLandingPos_.x, groundY_ + 0.14f, safeLandingPos_.z };
	ResolveDuckPoseCollision(refinedPos, fallFinalRot_, Vector3{ 1.04f, 0.96f, 1.04f });
	safeLandingPos_ = { refinedPos.x, groundY_, refinedPos.z };
}

void GameOverSequenceController::PushCameraOutOfObstacles(Vector3& ioCamPos) const
{
	// ドリー移動中のカメラ自身が木やコンテナ・見張り塔の内部を絶対に横切らないようリアルタイム球押し出し
	for (int iter = 0; iter < 3; ++iter)
	{
		float maxPen = 0.0f;
		Vector3 bestPushXZ{ 0.0f, 0.0f, 0.0f };
		float liftTargetY = ioCamPos.y;
		bool preferLift = false;

		for (const auto& ob : nearbyObstacles_)
		{
			if (ob.isBoundary) continue;
			Vector3 pushXZ{};
			float pen = 0.0f;
			if (ComputeSphereOBBPushOutXZ(ioCamPos, 0.68f, ob.center, ob.size, ob.rotation, pushXZ, pen))
			{
				if (pen > maxPen)
				{
					maxPen = pen;
					bestPushXZ = pushXZ;
					float topClearY = ob.center.y + ob.size.y * 0.5f + 0.68f;
					if (topClearY - ioCamPos.y > 0.0f && topClearY - ioCamPos.y < 1.35f)
					{
						preferLift = true;
						liftTargetY = topClearY;
					}
					else
					{
						preferLift = false;
					}
				}
			}
		}

		if (maxPen <= 1e-4f) break;
		if (preferLift)
		{
			ioCamPos.y = liftTargetY;
		}
		else
		{
			ioCamPos.x += bestPushXZ.x;
			ioCamPos.z += bestPushXZ.z;
		}
	}
}

// -----------------------------------------------------------------------------
// 死亡シネマティック開始
// -----------------------------------------------------------------------------

void GameOverSequenceController::Start(
	const Vector3& deathPos,
	const Vector3& killerPos,
	const std::string& killerName,
	const std::string& causeOfDeath,
	bool isMIA)
{
	isActive_ = true;
	timer_ = 0.0f;
	deathPos_ = deathPos;
	groundY_ = deathPos.y;
	killerPos_ = killerPos;
	killerName_ = killerName.empty() ? "HOSTILE SENTRY (SVD SNIPER)" : killerName;
	causeOfDeath_ = causeOfDeath.empty() ? "7.62x54mmR AP // CHEST VITAL PENETRATION" : causeOfDeath;
	isMIA_ = isMIA;
	transitionTriggered_ = false;
	groundImpactTriggered_ = false;
	alarmTriggered_ = false;
	flareActive_ = false;
	heliVisible_ = false;
	shockDiscActive_ = false;
	beaconWave1Active_ = false;
	beaconWave2Active_ = false;
	gunSmokeTimer_ = 0.0f;
	featherTimer_ = 0.0f;
	skidDustTimer_ = 0.0f;
	flareSmokeTimer_ = 0.0f;
	flareSparkTimer_ = 0.0f;
	downwashTimer_ = 0.0f;
	rotorVaporTimer_ = 0.0f;
	exhaustTimer_ = 0.0f;
	strobeTimer_ = 0.0f;
	tyndallMoteTimer_ = 0.0f;
	doorOpenProgress_ = 0.0f;
	searchlightIntensity_ = 0.0f;
	rifleBounceSparked_ = false;
	impactFlashAlpha_ = 1.0f;
	darkVignetteAlpha_ = 0.62f;

	// ゲームプレイ中のHPバー・スタミナバー・照準・浮遊ダメージ文字等の残存UIを即座に全消去
	SuppressGameplayHUD();

	// シーン照明の退避（GameOverScene の薄明ブルー＋救難ヘリ投光器へ滑らかにブリッジするため）
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight() && !hasSavedLighting_)
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		savedDirLight_ = light->GetDirectionalLightData();
		savedPointLight_ = light->GetPointLightData();
		hasSavedLighting_ = true;
	}

	// 負傷アヒルテクスチャへの切り替え ＆ 被弾直前のプレイヤー向きを保存
	if (scene_ && scene_->GetPlayer())
	{
		initialPlayerYaw_ = scene_->GetPlayer()->GetRotation().y;
		if (woundedDuckTexIndex_ != UINT32_MAX)
		{
			scene_->GetPlayer()->SetTextureIndex(woundedDuckTexIndex_);
		}
	}

	// 初期カメラ状態の保存 ＆ エンジン側の大振幅ランダム並進シェイクを即時停止
	if (scene_ && scene_->camera_)
	{
		scene_->camera_->StopShake();
		initialCamPos_ = scene_->camera_->GetTranslate();
		initialCamRot_ = scene_->camera_->GetRotate();
		initialCamFov_ = scene_->camera_->GetFovY();
		scene_->camera_->MoveTo(initialCamPos_, 0.0f);
	}

	// キラーからの基本ノックバック方向算出
	Vector3 toKiller = killerPos_ - deathPos_;
	killerDistance_ = std::sqrt(toKiller.x * toKiller.x + toKiller.z * toKiller.z);
	float defaultKillerYaw = std::atan2(toKiller.x, toKiller.z);
	if (killerDistance_ > 0.1f)
	{
		flingDir_ = { -toKiller.x / killerDistance_, 0.0f, -toKiller.z / killerDistance_ };
	}
	else
	{
		flingDir_ = { 0.0f, 0.0f, -1.0f };
	}
	if (!scene_ || !scene_->GetPlayer())
	{
		initialPlayerYaw_ = defaultKillerYaw;
	}

	// 1. ステージ障害物の実寸OBBを収集（オブジェクトは1つも消去しない）
	CollectNearbyObstacles();

	// 2. 木箱やコンテナに頭・胴体が一切埋まらない安全な転倒着地地点を計画
	PlanSafeDuckFallPose(defaultKillerYaw);

	// 3. 360度全周レイキャストで遮蔽物に一切邪魔されない俯瞰カメラリグを選択
	SelectUnobstructedCameraRig();

	// 4. カメラ方位に対して確実に顔・くちばしが見える3種の転倒バリエーション姿勢を確定
	FinalizeDuckPoseForCamera();

	duckPos_ = deathPos_;
	duckRot_ = { 0.0f, initialPlayerYaw_, 0.0f };
	duckScale_ = { 1.0f, 1.0f, 1.0f };

	// カメラ視線基準の水平基底ベクトル（GameOverScene と同じ機体向きを作るため）
	Vector3 camForward = { -camAzimuthDir_.x, 0.0f, -camAzimuthDir_.z };
	Vector3 screenRight = { -camAzimuthDir_.z, 0.0f, camAzimuthDir_.x };
	float camViewYaw = std::atan2(camForward.x, camForward.z);

	// 緊急救難スモークフレア発煙筒の着地位置（倒れたアヒルのすぐ脇・遮蔽物外へ自動クリアランス補正）
	Vector3 focusFinal = GetFinalFallenFocus();
	flareStartPos_ = focusFinal + Vector3{ 0.0f, 0.35f, 0.0f };
	flareTargetPos_ = focusFinal + camAzimuthDir_ * 0.72f - screenRight * 0.65f;
	flareTargetPos_.y = groundY_;
	ResolveHorizontalPointClearance(flareTargetPos_, 0.32f, 0.18f);
	flarePos_ = flareStartPos_;
	flareYaw_ = camViewYaw + 0.65f;

	// 吹き飛ぶライフルの初期化（障害物にも倒れたプレイヤー自身にも絶対に重ならない安全着地点を探索して弾き飛ばす）
	if (scene_ && scene_->GetPlayer())
	{
		riflePos_ = scene_->GetPlayer()->GetRiflePosition();
	}
	else
	{
		riflePos_ = deathPos_ + Vector3{ 0.0f, 0.68f, 0.0f };
	}
	{
		Vector3 finalDuckLand = { safeLandingPos_.x, groundY_ + 0.14f, safeLandingPos_.z };
		Matrix4x4 Wduck = MakeAffineMatrix(
			Vector3{ 1.04f, 0.96f, 1.04f },
			fallFinalRot_,
			finalDuckLand
		);
		struct DuckSphere { Vector3 worldPt; float radius; };
		const float localY[5] = { 0.24f, 0.62f, 1.02f, 1.38f, 1.66f };
		const float localZ[5] = { 0.00f, 0.02f, 0.05f, 0.08f, 0.04f };
		const float radii[5]  = { 0.44f, 0.58f, 0.64f, 0.54f, 0.48f };
		DuckSphere duckSpheres[5]{};
		for (int i = 0; i < 5; ++i)
		{
			duckSpheres[i].worldPt = {
				localY[i] * Wduck.m[1][0] + localZ[i] * Wduck.m[2][0] + Wduck.m[3][0],
				localY[i] * Wduck.m[1][1] + localZ[i] * Wduck.m[2][1] + Wduck.m[3][1],
				localY[i] * Wduck.m[1][2] + localZ[i] * Wduck.m[2][2] + Wduck.m[3][2]
			};
			duckSpheres[i].radius = radii[i];
		}

		// カメラ右手前方向を第一候補としつつ、16方位×2距離から「プレイヤー全身球・救難フレア・障害物のすべてと干渉ゼロ」の着地点を選択
		Vector3 prefDir = camAzimuthDir_ * 0.55f + screenRight * 0.83f;
		float prefAngle = std::atan2(prefDir.x, prefDir.z);
		static const float kRifleDists[2] = { 1.85f, 2.25f };

		Vector3 bestTarget = focusFinal + screenRight * 1.85f;
		float bestRifleScore = -1e9f;

		for (float rDist : kRifleDists)
		{
			for (int a = 0; a < 16; ++a)
			{
				float ang = prefAngle + static_cast<float>(a) * (2.0f * kPi / 16.0f);
				Vector3 cand = focusFinal + Vector3{ std::sin(ang) * rDist, 0.0f, std::cos(ang) * rDist };
				cand.y = groundY_ + 0.16f;

				float penSum = 0.0f;
				// 1. ステージ障害物との干渉ペナルティ
				for (const auto& ob : nearbyObstacles_)
				{
					if (ob.isCanopy) continue;
					Vector3 pushXZ{};
					float pen = 0.0f;
					if (ComputeSphereOBBPushOutXZ(cand, 0.74f, ob.center, ob.size, ob.rotation, pushXZ, pen))
					{
						penSum += pen * 2.5f;
					}
				}
				// 2. 倒れたプレイヤー自身の全身5球との干渉ペナルティ（銃の全長 0.72m 分のマージン確保）
				for (const auto& ds : duckSpheres)
				{
					float dx = cand.x - ds.worldPt.x;
					float dz = cand.z - ds.worldPt.z;
					float minClear = ds.radius + 0.74f;
					float d = std::sqrt(dx * dx + dz * dz);
					if (d < minClear)
					{
						penSum += (minClear - d) * 3.0f;
					}
				}
				// 3. 救難フレア発煙筒との重なり回避
				float fdx = cand.x - flareTargetPos_.x;
				float fdz = cand.z - flareTargetPos_.z;
				float fd = std::sqrt(fdx * fdx + fdz * fdz);
				if (fd < 0.85f)
				{
					penSum += (0.85f - fd) * 1.5f;
				}

				float angDiff = std::abs(NormalizeAngleDiff(ang, prefAngle));
				float score = -penSum * 100.0f - angDiff * 1.8f;
				if (score > bestRifleScore)
				{
					bestRifleScore = score;
					bestTarget = { cand.x, groundY_, cand.z };
				}
			}
		}

		// 約 0.56秒の放物線で安全着地点へ正確に弾き飛ばす初速を算出
		constexpr float kFlightT = 0.56f;
		float startH = (std::max)(0.20f, riflePos_.y - (groundY_ + 0.06f));
		float vy = (0.5f * 12.0f * kFlightT * kFlightT - startH) / kFlightT;
		vy = (std::clamp)(vy, 2.2f, 3.6f);
		rifleVel_ = {
			(bestTarget.x - riflePos_.x) / kFlightT,
			vy,
			(bestTarget.z - riflePos_.z) / kFlightT
		};
	}
	rifleRot_ = { 0.28f, fallBaseYaw_, -0.35f };
	rifleRotVel_ = { 0.0f, RandRange(8.5f, 11.5f), 0.0f };
	rifleGrounded_ = false;

	// 4. 飛来する 3D 救難ヘリコプター（MEDEVAC）の飛行ルート＆安全ホバリング高度算出
	// GameOverScene の着陸ヘリと完全一致するカメラ相対アングル（右側奥にホバリングし、左前方の+X開放ドアをカメラへ向ける）
	float maxLocalTopY = groundY_ + 2.8f;
	for (const auto& ob : nearbyObstacles_)
	{
		if (ob.isBoundary) continue;
		float dx = ob.center.x - focusFinal.x;
		float dz = ob.center.z - focusFinal.z;
		if (dx * dx + dz * dz < 6.5f * 6.5f)
		{
			float topY = ob.center.y + ob.size.y * 0.5f;
			if (topY > maxLocalTopY) maxLocalTopY = topY;
		}
	}
	heliHoverY_ = (std::max)(groundY_ + 4.35f, maxLocalTopY + 1.15f);
	heliBaseYaw_ = camViewYaw + 2.55f;

	Vector3 heliNoseDir = { std::sin(heliBaseYaw_), 0.0f, std::cos(heliBaseYaw_) };
	heliHoverPos_ = focusFinal + screenRight * 1.45f + camForward * 1.45f;
	heliHoverPos_.y = heliHoverY_;
	heliStartPos_ = heliHoverPos_ - heliNoseDir * 22.0f + Vector3{ 0.0f, 10.5f, 0.0f };
	heliPos_ = heliStartPos_;
	heliRot_ = { 0.18f, heliBaseYaw_, 0.0f };
	mainRotorAngle_ = 0.0f;
	tailRotorAngle_ = 0.0f;

	if (scene_)
	{
		scene_->TriggerHitStop(0.14f);
	}

	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
	{
		auto* am = SceneManager::GetInstance()->GetAudioManager();
		if (duckSoundId_ >= 0) am->PlayOneShot(duckSoundId_, 1.0f);
	}

	// 着弾瞬間の超音速弾道トレイル・アーマー火花・真鍮薬莢排莢・羽毛バースト
	if (scene_ && scene_->particleManager && scene_->appParticleManager_)
	{
		auto& randEng = scene_->particleManager->GetRandomEngine();
		uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
		uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;
		uint32_t flashTex = (scene_->starburstTextureIndex_ != TextureManager::kInvalidTextureIndex)
			? scene_->starburstTextureIndex_
			: ((circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : scene_->particleTextureB);
		uint32_t shellTex = (scene_->cylinderTextureIndex_ != TextureManager::kInvalidTextureIndex)
			? scene_->cylinderTextureIndex_ : flashTex;

		Vector3 chestHitPos = duckPos_ + Vector3{ 0.0f, 0.72f, 0.0f };

		// 1. 狙撃方向から胸部を貫く 3D 超音速曳光弾＆衝撃波ベイパートンネル（2D線ではなく3Dパーティクルで立体描画）
		for (int s = 0; s < 24; ++s)
		{
			float frac = static_cast<float>(s) / 23.0f;
			float distFromChest = -7.2f + frac * 8.4f;
			Vector3 trailPt = chestHitPos + flingDir_ * distFromChest + Vector3{ 0.0f, (1.0f - frac) * 0.25f, 0.0f };

			AppParticle tracerCore{};
			tracerCore.transform.Initialize();
			tracerCore.transform.SetTranslate(trailPt);
			tracerCore.initialScale = 0.26f;
			tracerCore.targetScale = 0.03f;
			tracerCore.transform.SetScale({ 0.26f, 0.26f, 1.0f });
			tracerCore.velocity = flingDir_ * 4.5f;
			tracerCore.color = { 1.0f, 0.88f, 0.52f, 0.92f };
			tracerCore.endColor = { 1.0f, 0.35f, 0.12f, 0.0f };
			tracerCore.hasColorShift = true;
			tracerCore.lifeTime = 0.24f;
			tracerCore.currentTime = 0.0f;
			tracerCore.drag = 4.0f;
			tracerCore.textureIndex = flashTex;
			scene_->appParticleManager_->AddParticle(tracerCore);

			if (s % 2 == 0)
			{
				scene_->appParticleManager_->EmitDustWithVelocity(
					randEng, trailPt, 0.18f,
					{ 0.90f, 0.94f, 1.0f, 0.36f },
					flingDir_ * 5.0f, 0.26f,
					(whitePuffTextureIndex_ != UINT32_MAX) ? whitePuffTextureIndex_ : scene_->smokeTextureIndex_
				);
			}
		}

		// 2. 防弾ベスト着弾スターバースト閃光 ＆ 18本の高輝度金属スパーク
		scene_->appParticleManager_->EmitMuzzleFlare(
			randEng, chestHitPos, 1.35f, { 1.0f, 0.90f, 0.65f, 1.0f }, 0.14f, flashTex
		);
		for (int i = 0; i < 18; ++i)
		{
			Vector3 spkVel = flingDir_ * RandRange(3.5f, 11.0f)
				+ Vector3{ RandRange(-4.5f, 4.5f), RandRange(1.2f, 5.2f), RandRange(-4.5f, 4.5f) };
			scene_->appParticleManager_->EmitSparkWithVelocity(
				randEng, chestHitPos, spkVel,
				{ 1.0f, 0.82f, 0.35f, 1.0f }, 0.12f, 0.28f, flashTex
			);
		}

		// 3. 衝撃硝煙＆羽毛バースト（回転しない不自然な縦長ビルボード薬莢は廃止）
		for (int i = 0; i < 10; ++i)
		{
			float speed = RandRange(9.0f, 16.0f);
			Vector3 streakVel = flingDir_ * speed + Vector3{ RandRange(-0.25f, 0.25f), RandRange(-0.15f, 0.15f), RandRange(-0.25f, 0.25f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, chestHitPos, 0.24f,
				{ 0.12f, 0.11f, 0.10f, 0.36f }, streakVel, 0.20f, darkSmokeTex
			);
		}

		for (int i = 0; i < 16; ++i)
		{
			scene_->appParticleManager_->EmitFeather(randEng, chestHitPos, { 0.96f, 0.93f, 0.82f, 0.78f }, fTex);
		}
	}

	UpdateMotion(0.0f);
	UpdateWeaponMotion(0.0f);
	UpdateCamera(0.0f);
}

// -----------------------------------------------------------------------------
// 毎フレーム更新
// -----------------------------------------------------------------------------

void GameOverSequenceController::Update(float realDeltaTime)
{
	if (!isActive_) return;

	// プレイ中の残存UIスプライト・浮遊テキストを毎フレーム確実に抑止し、チュートリアル看板ガイドも隠す
	SuppressGameplayHUD();
	if (scene_)
	{
		for (const auto& sign : scene_->GetTutorialSigns())
		{
			if (sign) sign->Update({ 9999.0f, 9999.0f, 9999.0f });
		}
	}

	if (impactFlashAlpha_ > 0.0f)
	{
		impactFlashAlpha_ = (std::max)(0.0f, impactFlashAlpha_ - realDeltaTime * 8.5f);
	}

	// 致命弾着弾の瞬間 (0.0s 〜 0.20s) は 18% の重厚バレットタイム・スローモーション
	float timeScale = 1.0f;
	if (timer_ < 0.20f)
	{
		timeScale = 0.18f;
	}
	else if (timer_ < 0.36f)
	{
		float u = (timer_ - 0.20f) / 0.16f;
		timeScale = 0.18f + 0.82f * (u * u);
	}
	float simDelta = realDeltaTime * timeScale;
	timer_ += simDelta;

	// GamePlayScene 側は死亡演出中 0.15倍速でしかパーティクルを進めないため、バレットタイム終了後は等倍(1.0x)のリアル流体速度へ補正！
	if (scene_ && scene_->appParticleManager_ && timeScale > 0.18f)
	{
		float extraParticleDt = realDeltaTime * (std::max)(0.0f, timeScale - 0.15f);
		if (extraParticleDt > 0.0f)
		{
			scene_->appParticleManager_->Update(extraParticleDt, duckPos_);
		}
	}

	// 地面激突時の 3D HLSL 衝撃波ディスク更新（パターン2の膝つき転倒は 0.50s、通常は 0.36s に着地）
	const float impactStart = (fallVariant_ == 2) ? 0.50f : 0.36f;
	if (impactShockDisc_)
	{
		if (timer_ >= impactStart && timer_ < impactStart + 0.34f)
		{
			shockDiscActive_ = true;
			float impU = (timer_ - impactStart) / 0.34f;
			float r = 0.45f + EaseOutCubic(impU) * 2.65f;
			float a = (1.0f - impU) * (1.0f - impU) * 0.55f;
			impactShockDisc_->SetTranslate({ duckPos_.x, groundY_ + 0.042f, duckPos_.z });
			impactShockDisc_->SetRotate({ 0.0f, 0.0f, 0.0f });
			impactShockDisc_->SetScale({ r, 1.0f, r });
			impactShockDisc_->SetColor({ 0.95f, 0.90f, 0.80f, a });
			impactShockDisc_->SetReflectionFactor(timer_);
			impactShockDisc_->SetFresnelF0(2.065f);
			impactShockDisc_->Update();
		}
		else
		{
			shockDiscActive_ = false;
		}
	}

	// 緊急救難ビーコン起動音（0.58sで発動）
	if (timer_ >= 0.58f && !alarmTriggered_)
	{
		alarmTriggered_ = true;
		if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetAudioManager())
		{
			auto* am = SceneManager::GetInstance()->GetAudioManager();
			if (alarmSoundId_ >= 0) am->PlayOneShot(alarmSoundId_, 0.75f);
		}
	}

	UpdateMotion(simDelta);
	UpdateWeaponMotion(simDelta);
	UpdateFlareAndAtmosphere(simDelta, realDeltaTime);
	UpdateMedevacHelicopter(realDeltaTime);
	UpdateCamera(realDeltaTime);

	// [SPACE] または [ENTER] キーでの軽快な即時スキップ
	bool skipPressed = false;
	if ((GetAsyncKeyState(VK_SPACE) & 0x8000) != 0 || (GetAsyncKeyState(VK_RETURN) & 0x8000) != 0)
	{
		skipPressed = true;
	}
#ifdef USE_IMGUI
	if (ImGui::IsKeyPressed(ImGuiKey_Space) || ImGui::IsKeyPressed(ImGuiKey_Enter))
	{
		skipPressed = true;
	}
#endif
	if (skipPressed && timer_ > 0.22f)
	{
		Skip();
		return;
	}

	// タイムライン完走時、重装甲メディバック・シャッターで GameOverScene（救難ヘリ担架搬送）へシームレス移行
	if (timer_ >= kDuration && !transitionTriggered_)
	{
		FinishSequence();
	}
}

// -----------------------------------------------------------------------------
// アヒル兵士の3種転倒モーション（カメラへ顔・くちばしを向けつつ全身OBB押し出しで埋まり完全防止）
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateMotion(float simDelta)
{
	if (!scene_ || !scene_->GetPlayer()) return;
	Player* player = scene_->GetPlayer();
	Object3d* obj = player->GetObject3d();
	if (!obj) return;

	auto emitGroundSlamBurst = [&]() {
		if (groundImpactTriggered_) return;
		groundImpactTriggered_ = true;
		if (scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			auto& randEng = scene_->particleManager->GetRandomEngine();
			uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
			uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;

			for (int i = 0; i < 14; ++i)
			{
				float angle = i * (2.0f * kPi / 14.0f);
				Vector3 vel = { std::cos(angle) * 4.0f, RandRange(0.08f, 0.22f), std::sin(angle) * 4.0f };
				scene_->appParticleManager_->EmitDustWithVelocity(
					randEng, duckPos_ + Vector3{ 0.0f, 0.03f, 0.0f }, 0.72f,
					{ 0.16f, 0.14f, 0.12f, 0.35f }, vel, 0.45f, darkSmokeTex
				);
			}

			for (int i = 0; i < 10; ++i)
			{
				Vector3 fOffset = { RandRange(-0.50f, 0.50f), RandRange(0.12f, 0.40f), RandRange(-0.50f, 0.50f) };
				scene_->appParticleManager_->EmitFeather(randEng, duckPos_ + fOffset, { 0.95f, 0.92f, 0.82f, 0.65f }, fTex);
			}
		}
	};

	const float baseYawDelta = NormalizeAngleDiff(fallFinalRot_.y, initialPlayerYaw_);
	// パターン1（スピン転倒）は同方向へもう半回転〜1回転近い勢いをつけてダイナミックに回す
	const float totalYawSpin = (fallVariant_ == 1)
		? (baseYawDelta + ((baseYawDelta >= 0.0f) ? (kPi * 1.15f) : (-kPi * 1.15f)))
		: baseYawDelta;

	if (fallVariant_ == 2)
	{
		// 【バリエーション2: ガクッと前のめりに膝をついて耐え → 力尽きてカメラ側へ顔を向けながら倒れ込む】
		if (timer_ < 0.34f)
		{
			float t = timer_ / 0.34f;
			float staggerEase = EaseOutQuad(t);
			duckPos_ = LerpVector3(deathPos_, safeLandingPos_, staggerEase * 0.42f);
			duckPos_.y = groundY_ + 0.02f * std::sin(t * kPi);

			duckRot_.x = 0.28f * std::sin(t * kPi * 0.85f); // 一瞬前傾してよろめく
			duckRot_.y = initialPlayerYaw_ + totalYawSpin * (staggerEase * 0.45f);
			duckRot_.z = fallFinalRot_.z * (staggerEase * 0.35f);
			duckScale_ = { 1.06f, 0.92f, 1.06f };
		}
		else if (timer_ < kAct1Duration)
		{
			float t = (timer_ - 0.34f) / (kAct1Duration - 0.34f);
			float fallEase = EaseInOutCubic(t);
			if (timer_ >= 0.50f) emitGroundSlamBurst();

			float bounce = (timer_ >= 0.50f && timer_ < 0.68f)
				? (std::sin(((timer_ - 0.50f) / 0.18f) * kPi) * 0.045f)
				: 0.0f;
			duckPos_ = LerpVector3(deathPos_, safeLandingPos_, 0.42f + 0.58f * EaseOutCubic(t));
			duckPos_.y = groundY_ + 0.14f * fallEase + bounce;

			float startPitch = 0.28f * std::sin(kPi * 0.85f);
			duckRot_.x = startPitch + (fallFinalRot_.x - startPitch) * EaseOutCubic(t);
			duckRot_.y = initialPlayerYaw_ + totalYawSpin * (0.45f + 0.55f * EaseOutCubic(t));
			duckRot_.z = fallFinalRot_.z * (0.35f + 0.65f * fallEase);
			duckScale_ = { 1.04f, 0.96f, 1.04f };
		}
		else
		{
			duckPos_ = safeLandingPos_;
			duckPos_.y = groundY_ + 0.14f;
			duckRot_ = fallFinalRot_;
			float breath = std::sin((timer_ - kAct1Duration) * 2.4f) * 0.012f;
			duckScale_ = { 1.04f + breath, 0.96f + breath * 0.5f, 1.04f };
		}
	}
	else
	{
		// 【バリエーション0: 3/4仰向け吹き飛びダウン】 / 【バリエーション1: バレットタイムきりもみスピン＆横倒れ】
		constexpr float kImpactTime = 0.36f;
		constexpr float kAirProgressRatio = 0.72f;

		if (timer_ < kImpactTime)
		{
			float t = timer_ / kImpactTime;
			float arcHeight = (fallVariant_ == 1) ? 0.30f : 0.23f;
			float jumpArc = arcHeight * (4.0f * t * (1.0f - t));
			float moveProgress = kAirProgressRatio * (1.0f - (1.0f - t) * (1.0f - t));

			duckPos_ = LerpVector3(deathPos_, safeLandingPos_, moveProgress);
			duckPos_.y = groundY_ + 0.08f + jumpArc;

			float pitchT = EaseOutQuad((std::min)(1.0f, t * 1.22f));
			duckRot_.x = pitchT * fallFinalRot_.x;
			duckRot_.y = initialPlayerYaw_ + totalYawSpin * EaseOutQuad(t) * 0.86f;
			duckRot_.z = EaseOutQuad(t) * fallFinalRot_.z * 1.12f;

			if (t < 0.25f)
			{
				float impactSquash = std::sin(t / 0.25f * kPi);
				duckScale_ = { 1.08f + impactSquash * 0.08f, 0.90f - impactSquash * 0.10f, 1.08f + impactSquash * 0.08f };
			}
			else
			{
				float airStretch = std::sin((t - 0.25f) / 0.75f * kPi);
				duckScale_ = { 0.96f, 1.05f + airStretch * 0.06f, 0.96f };
			}
		}
		else if (timer_ < kAct1Duration)
		{
			float t = (timer_ - kImpactTime) / (kAct1Duration - kImpactTime);
			emitGroundSlamBurst();

			float bounceArc = (t < 0.35f) ? (std::sin((t / 0.35f) * kPi) * 0.05f) : 0.0f;
			float skidEase = 1.0f - std::pow(1.0f - t, 3.0f);
			float moveProgress = kAirProgressRatio + (1.0f - kAirProgressRatio) * skidEase;

			duckPos_ = LerpVector3(deathPos_, safeLandingPos_, moveProgress);
			duckPos_.y = groundY_ + 0.14f + bounceArc;

			skidDustTimer_ += simDelta;
			if (t < 0.70f && skidDustTimer_ >= 0.08f && scene_ && scene_->particleManager && scene_->appParticleManager_)
			{
				skidDustTimer_ = 0.0f;
				auto& randEng = scene_->particleManager->GetRandomEngine();
				uint32_t darkSmokeTex = (smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_;
				Vector3 dVel = flingDir_ * -0.6f + Vector3{ RandRange(-0.2f, 0.2f), RandRange(0.1f, 0.35f), RandRange(-0.2f, 0.2f) };
				scene_->appParticleManager_->EmitDustWithVelocity(
					randEng, duckPos_ + Vector3{ 0.0f, 0.04f, 0.0f }, 0.40f,
					{ 0.14f, 0.12f, 0.10f, 0.30f }, dVel, 0.34f, darkSmokeTex
				);
			}

			float neckWhip = (t < 0.40f) ? std::sin(t / 0.40f * kPi) * 0.07f : 0.0f;
			duckRot_.x = fallFinalRot_.x - neckWhip;
			duckRot_.y = initialPlayerYaw_ + totalYawSpin * (0.86f + 0.14f * skidEase);
			duckRot_.z = fallFinalRot_.z * (1.12f - 0.12f * skidEase);

			if (t < 0.20f)
			{
				float impactSquash = std::sin(t / 0.20f * kPi);
				duckScale_ = { 1.10f + impactSquash * 0.08f, 0.86f - impactSquash * 0.08f, 1.10f + impactSquash * 0.08f };
			}
			else
			{
				duckScale_ = { 1.04f, 0.96f, 1.04f };
			}
		}
		else
		{
			duckPos_ = safeLandingPos_;
			duckPos_.y = groundY_ + 0.14f;
			duckRot_ = fallFinalRot_;

			float breath = std::sin((timer_ - kAct1Duration) * 2.4f) * 0.012f;
			duckScale_ = { 1.04f + breath, 0.96f + breath * 0.5f, 1.04f };
		}
	}

	// 転倒中・静止中ともに全身5球で障害物めり込みをゼロに保つ
	ResolveDuckPoseCollision(duckPos_, duckRot_, duckScale_);

	obj->SetTranslate(duckPos_);
	obj->SetRotate(duckRot_);
	obj->SetScale(duckScale_);
	obj->SetColor({ 0.96f, 0.96f, 0.98f, 1.0f });
	obj->Update();
}

// -----------------------------------------------------------------------------
// 吹き飛ぶライフル（武器）の物理運動（銃身・マガジン最下点計算で地面＆障害物埋まり完全防止）
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateWeaponMotion(float simDelta)
{
	if (!droppedRifle_) return;

	// 横倒し専用BuildRifleModel実寸 (X:[-0.10,0.21], Y:[-0.036,0.036], Z:[-0.46,0.69]) から最下点ワールドYを厳密算出
	auto calcDynamicFloorY = [&](const Vector3& rot) -> float {
		Matrix4x4 R = Multiply(MakeRotateXMatrix(rot.x), Multiply(MakeRotateYMatrix(rot.y), MakeRotateZMatrix(rot.z)));
		float minLocalWorldY =
			((R.m[0][1] >= 0.0f) ? -0.100f : 0.210f) * R.m[0][1] +
			((R.m[1][1] >= 0.0f) ? -0.038f : 0.038f) * R.m[1][1] +
			((R.m[2][1] >= 0.0f) ? -0.470f : 0.690f) * R.m[2][1];
		return (groundY_ + 0.020f) - minLocalWorldY;
	};

	// 銃床(-0.44m)・機関部(0.08m)・銃口(+0.66m)の3点がプレイヤーの身体球に絶対にめり込まないよう水平押し出し
	auto resolveRiflePlayerClearance = [&]() {
		if (timer_ < 0.12f) return;

		Matrix4x4 Rgun = Multiply(MakeRotateXMatrix(rifleRot_.x), Multiply(MakeRotateYMatrix(rifleRot_.y), MakeRotateZMatrix(rifleRot_.z)));
		const float sampleZ[3] = { -0.44f, 0.08f, 0.66f };

		Matrix4x4 Wduck = MakeAffineMatrix(duckScale_, duckRot_, duckPos_);
		const float duckLocalY[5] = { 0.24f, 0.62f, 1.02f, 1.38f, 1.66f };
		const float duckLocalZ[5] = { 0.00f, 0.02f, 0.05f, 0.08f, 0.04f };
		const float duckRadii[5]  = { 0.45f, 0.60f, 0.66f, 0.56f, 0.50f };

		for (int iter = 0; iter < 3; ++iter)
		{
			float maxPen = 0.0f;
			Vector3 bestPush = { 0.0f, 0.0f, 0.0f };

			for (int g = 0; g < 3; ++g)
			{
				Vector3 gunPt = {
					riflePos_.x + sampleZ[g] * Rgun.m[2][0],
					riflePos_.y + sampleZ[g] * Rgun.m[2][1],
					riflePos_.z + sampleZ[g] * Rgun.m[2][2]
				};

				for (int d = 0; d < 5; ++d)
				{
					Vector3 duckPt = {
						duckLocalY[d] * Wduck.m[1][0] + duckLocalZ[d] * Wduck.m[2][0] + Wduck.m[3][0],
						duckLocalY[d] * Wduck.m[1][1] + duckLocalZ[d] * Wduck.m[2][1] + Wduck.m[3][1],
						duckLocalY[d] * Wduck.m[1][2] + duckLocalZ[d] * Wduck.m[2][2] + Wduck.m[3][2]
					};
					if (std::abs(gunPt.y - duckPt.y) > duckRadii[d] + 0.22f) continue;

					float dx = gunPt.x - duckPt.x;
					float dz = gunPt.z - duckPt.z;
					float distSq = dx * dx + dz * dz;
					float minDist = duckRadii[d] + 0.20f;
					if (distSq < minDist * minDist)
					{
						float dist = std::sqrt(distSq);
						float pen = minDist - dist;
						if (pen > maxPen)
						{
							maxPen = pen;
							if (dist > 1e-4f)
							{
								bestPush = { (dx / dist) * pen, 0.0f, (dz / dist) * pen };
							}
							else
							{
								bestPush = { camAzimuthDir_.x * pen, 0.0f, camAzimuthDir_.z * pen };
							}
						}
					}
				}
			}

			if (maxPen <= 1e-4f) break;
			riflePos_.x += bestPush.x;
			riflePos_.z += bestPush.z;
		}
	};

	if (!rifleGrounded_)
	{
		rifleVel_.y -= 12.0f * simDelta;
		riflePos_ += rifleVel_ * simDelta;
		// 自然な水平スピン (rot.y) で回転しながら、傾き (rot.x, rot.z) は滑らかに水平(0)へ収束
		rifleRot_.y += rifleRotVel_.y * simDelta;
		float levelRate = (std::min)(1.0f, simDelta * 7.5f);
		rifleRot_.x *= (1.0f - levelRate);
		rifleRot_.z *= (1.0f - levelRate);

		resolveRiflePlayerClearance();
		ResolveHorizontalPointClearance(riflePos_, 0.72f, 0.16f);
		resolveRiflePlayerClearance();

		float dynamicFloorY = calcDynamicFloorY(rifleRot_);
		if (riflePos_.y <= dynamicFloorY)
		{
			if (!rifleBounceSparked_ && scene_ && scene_->particleManager && scene_->appParticleManager_)
			{
				rifleBounceSparked_ = true;
				auto& randEng = scene_->particleManager->GetRandomEngine();
				uint32_t sparkTex = (scene_->starburstTextureIndex_ != TextureManager::kInvalidTextureIndex)
					? scene_->starburstTextureIndex_
					: ((circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : scene_->particleTextureB);
				for (int i = 0; i < 6; ++i)
				{
					Vector3 spkVel = { RandRange(-2.4f, 2.4f), RandRange(0.9f, 2.6f), RandRange(-2.4f, 2.4f) };
					scene_->appParticleManager_->EmitSparkWithVelocity(
						randEng, { riflePos_.x, groundY_ + 0.05f, riflePos_.z }, spkVel,
						{ 1.0f, 0.86f, 0.42f, 0.95f }, 0.08f, 0.18f, sparkTex
					);
				}
			}

			rifleRot_.x *= 0.20f;
			rifleRot_.z *= 0.20f;
			riflePos_.y = calcDynamicFloorY(rifleRot_);

			rifleVel_.y = -rifleVel_.y * 0.25f;
			rifleVel_.x *= 0.46f;
			rifleVel_.z *= 0.46f;
			rifleRotVel_.y *= 0.34f;

			if (std::abs(rifleVel_.y) < 0.22f)
			{
				rifleGrounded_ = true;
				rifleVel_ = { 0.0f, 0.0f, 0.0f };
				rifleRot_.x = 0.0f;
				rifleRot_.z = 0.0f;
				riflePos_.y = calcDynamicFloorY(rifleRot_);
			}
		}
	}
	else
	{
		rifleRot_.x = 0.0f;
		rifleRot_.z = 0.0f;
		resolveRiflePlayerClearance();
		riflePos_.y = calcDynamicFloorY(rifleRot_);
	}

	droppedRifle_->SetTranslate(riflePos_);
	droppedRifle_->SetRotate(rifleRot_);
	droppedRifle_->SetScale({ 1.0f, 1.0f, 1.0f });
	droppedRifle_->Update();
}

// -----------------------------------------------------------------------------
// 緊急救難スモークフレア発煙 ＆ GameOverScene 薄明＋ハロゲン救難灯ライティング接続
// -----------------------------------------------------------------------------

void GameOverSequenceController::UpdateFlareAndAtmosphere(float simDelta, float realDeltaTime)
{
	// 1. 緊急救難フレア（AN-M18）がベストから転がり落ち、上空へ真紅の誘導スモーク柱を噴出 (0.38s 以降)
	if (timer_ >= 0.38f)
	{
		flareActive_ = true;
		float dropU = (std::clamp)((timer_ - 0.38f) / 0.24f, 0.0f, 1.0f);
		float arcY = (dropU < 1.0f) ? (0.14f * 4.0f * dropU * (1.0f - dropU)) : 0.0f;
		flarePos_ = LerpVector3(flareStartPos_ - Vector3{ 0.0f, 0.22f, 0.0f }, flareTargetPos_, EaseOutQuad(dropU));
		flarePos_.y = groundY_ + arcY;

		float rollSpin = (1.0f - EaseOutQuad(dropU)) * 4.5f;
		if (flareCanister_)
		{
			flareCanister_->SetTranslate(flarePos_);
			flareCanister_->SetRotate({ 0.0f, flareYaw_, rollSpin });
			flareCanister_->SetScale({ 1.0f, 1.0f, 1.0f });
			flareCanister_->Update();
		}
		if (flareGlowCap_)
		{
			float flicker = 0.85f + 0.15f * std::sin(timer_ * 28.0f);
			flareGlowCap_->SetTranslate(flarePos_);
			flareGlowCap_->SetRotate({ 0.0f, flareYaw_, 0.0f });
			flareGlowCap_->SetScale({ flicker, flicker, 1.0f });
			flareGlowCap_->SetColor({ 1.0f, 0.35f * flicker, 0.15f, 1.0f });
			flareGlowCap_->Update();
		}

		// 上空の救難ヘリへ位置を知らせる赤色救難スモーク柱（ヘリ到着後はダウンウォッシュで外へたなびく）
		if (timer_ >= 0.65f && scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			auto& randEng = scene_->particleManager->GetRandomEngine();
			Vector3 nozzleDir = { std::sin(flareYaw_), 0.0f, std::cos(flareYaw_) };
			Vector3 nozzlePos = flarePos_ + nozzleDir * 0.10f + Vector3{ 0.0f, 0.05f, 0.0f };
			uint32_t puffTex = (whitePuffTextureIndex_ != UINT32_MAX) ? whitePuffTextureIndex_ : scene_->particleTextureB;

			float downwashSwirl = (std::clamp)((timer_ - 1.35f) / 0.65f, 0.0f, 1.0f);

			flareSmokeTimer_ += realDeltaTime;
			while (flareSmokeTimer_ >= 0.036f)
			{
				flareSmokeTimer_ -= 0.036f;
				float upSpeed = RandRange(3.2f, 4.8f) * (1.0f - 0.35f * downwashSwirl);
				float lateral = 0.35f + 1.45f * downwashSwirl;
				Vector3 smokeVel = nozzleDir * RandRange(0.25f, 0.65f)
					+ Vector3{ RandRange(-lateral, lateral), upSpeed, RandRange(-lateral, lateral) };
				float smokeScale = RandRange(0.60f, 1.05f);

				scene_->appParticleManager_->EmitDustWithVelocity(
					randEng,
					nozzlePos + Vector3{ RandRange(-0.05f, 0.05f), 0.0f, RandRange(-0.05f, 0.05f) },
					smokeScale,
					{ 0.95f, 0.20f, 0.15f, 0.48f },
					smokeVel,
					1.05f,
					puffTex
				);
			}

			flareSparkTimer_ += realDeltaTime;
			if (flareSparkTimer_ >= 0.060f)
			{
				flareSparkTimer_ = 0.0f;
				uint32_t glowTex = (circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : scene_->particleTextureB;
				Vector3 sparkVel = nozzleDir * RandRange(1.2f, 2.4f) + Vector3{ RandRange(-0.8f, 0.8f), RandRange(1.5f, 2.8f), RandRange(-0.8f, 0.8f) };
				scene_->appParticleManager_->EmitSparkWithVelocity(
					randEng, nozzlePos, sparkVel,
					{ 1.0f, 0.72f, 0.28f, 0.90f }, 0.09f, 0.18f, glowTex
				);

				// 赤色スモーク柱の内部をカール乱流で舞い昇る 3D 熱火の粉パーティクル
				AppParticle ember{};
				ember.transform.Initialize();
				ember.transform.SetTranslate(nozzlePos + Vector3{ RandRange(-0.12f, 0.12f), 0.08f, RandRange(-0.12f, 0.12f) });
				ember.initialScale = RandRange(0.09f, 0.15f);
				ember.targetScale = 0.02f;
				ember.transform.SetScale({ ember.initialScale, ember.initialScale, 1.0f });
				ember.velocity = { RandRange(-0.45f, 0.45f), RandRange(3.4f, 5.2f), RandRange(-0.45f, 0.45f) };
				ember.color = { 1.0f, 0.85f, 0.38f, 0.95f };
				ember.endColor = { 1.0f, 0.16f, 0.08f, 0.0f };
				ember.hasColorShift = true;
				ember.curlFreq = 3.2f;
				ember.curlAmp = 0.42f;
				ember.twinkleSpeed = 16.0f;
				ember.twinklePhase = RandRange(0.0f, 6.28f);
				ember.lifeTime = RandRange(0.65f, 1.05f);
				ember.currentTime = 0.0f;
				ember.textureIndex = glowTex;
				scene_->appParticleManager_->AddParticle(ember);
			}
		}

		// 3D HLSL 救難ビーコン電磁パルスディスク更新（遮蔽物の後ろへ自然に回り込む本物の3D地上波紋）
		auto updateBeaconWave = [&](Object3d* disc, bool& outActive, float startTime) {
			if (!disc) { outActive = false; return; }
			float lt = timer_ - startTime;
			if (lt >= 0.0f && lt < 1.05f)
			{
				outActive = true;
				float u = lt / 1.05f;
				float r = 0.30f + EaseOutCubic(u) * 5.40f;
				float a = (1.0f - u) * (1.0f - u) * 0.58f;
				disc->SetTranslate({ flarePos_.x, groundY_ + 0.044f, flarePos_.z });
				disc->SetRotate({ 0.0f, 0.0f, 0.0f });
				disc->SetScale({ r, 1.0f, r });
				disc->SetColor({ 1.0f, 0.24f, 0.16f, a });
				disc->SetReflectionFactor(timer_);
				disc->SetFresnelF0(2.045f);
				disc->Update();
			}
			else
			{
				outActive = false;
			}
		};
		updateBeaconWave(beaconWaveDisc1_.get(), beaconWave1Active_, 0.65f);
		updateBeaconWave(beaconWaveDisc2_.get(), beaconWave2Active_, 1.22f);
	}

	// 2. GameOverScene と完全一致する「薄明ブルー平行光 ＋ 救難ヘリの暖色ハロゲン投光器」への連続遷移
	if (hasSavedLighting_ && SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		float lightBlend = EaseInOutCubic((std::clamp)((timer_ - 0.45f) / 1.55f, 0.0f, 1.0f));

		// GameOverScene::InitializeScene (L279) と完全同一の薄明ブルー平行光 {0.38, 0.44, 0.58}, intensity 0.85
		Vector4 targetDirColor = { 0.38f, 0.44f, 0.58f, 1.0f };
		float targetDirIntensity = 0.85f;
		Vector4 curDirColor = LerpVector4(savedDirLight_.color, targetDirColor, lightBlend);
		float curDirIntensity = savedDirLight_.intensity + (targetDirIntensity - savedDirLight_.intensity) * lightBlend;
		light->SetDirectionalLightColor(curDirColor);
		light->SetDirectionalLightIntensity(curDirIntensity);

		// 前半は赤色フレア光、ヘリ到着後は GameOverScene (L281) と完全同一のハロゲン救難スポットライト {1.0, 0.88, 0.60} が自機を照らす
		if (timer_ >= 0.65f)
		{
			float flareRamp = (std::clamp)((timer_ - 0.65f) / 0.25f, 0.0f, 1.0f);
			float heliSpotBlend = EaseInOutCubic((std::clamp)((timer_ - 1.25f) / 0.75f, 0.0f, 1.0f));

			Vector4 flareColor = { 1.0f, 0.38f, 0.20f, 1.0f };
			Vector4 halogenColor = { 1.0f, 0.88f, 0.60f, 1.0f };
			Vector4 ptColor = LerpVector4(flareColor, halogenColor, heliSpotBlend);

			Vector3 flareLightPos = flarePos_ + Vector3{ 0.0f, 0.65f, 0.0f };
			Vector3 spotLightPos = GetFinalFallenFocus() + Vector3{ 0.0f, 3.60f, 0.0f };
			Vector3 ptPos = LerpVector3(flareLightPos, spotLightPos, heliSpotBlend);

			float intensity = (2.8f + 1.7f * heliSpotBlend) * flareRamp;
			float radius = 11.0f + 18.0f * heliSpotBlend;
			light->SetPointLight(ptColor, ptPos, intensity, radius, 1.2f);
		}
	}

	// 3. 被弾硝煙 ＆ 舞い散る羽毛
	if (timer_ >= 0.45f)
	{
		gunSmokeTimer_ += simDelta;
		if (gunSmokeTimer_ >= 0.13f && scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			gunSmokeTimer_ = 0.0f;
			auto& randEng = scene_->particleManager->GetRandomEngine();
			Vector3 sPos = GetDuckVisualCenter() + Vector3{ 0.0f, 0.08f, 0.0f };
			Vector3 sVel = { RandRange(-0.12f, 0.12f), RandRange(0.75f, 1.25f), RandRange(-0.12f, 0.12f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, sPos, 0.34f, { 0.09f, 0.08f, 0.08f, 0.20f }, sVel, 0.38f,
				(smokeDarkTextureIndex_ != UINT32_MAX) ? smokeDarkTextureIndex_ : scene_->smokeTextureIndex_
			);
		}
	}

	featherTimer_ += simDelta;
	if (featherTimer_ >= 0.26f && scene_ && scene_->particleManager && scene_->appParticleManager_)
	{
		featherTimer_ = 0.0f;
		auto& randEng = scene_->particleManager->GetRandomEngine();
		Vector3 focus = GetDuckVisualCenter();
		Vector3 fPos = focus + Vector3{ RandRange(-1.1f, 1.1f), RandRange(0.3f, 1.4f), RandRange(-1.1f, 1.1f) };
		uint32_t fTex = (featherTextureIndex_ != UINT32_MAX) ? featherTextureIndex_ : scene_->particleTextureB;
		scene_->appParticleManager_->EmitFeather(randEng, fPos, { 0.94f, 0.92f, 0.84f, 0.55f }, fTex);
	}
}

// -----------------------------------------------------------------------------
// 3D 救難ヘリコプター（MEDEVAC）上空急行・制動ホバリング・衛生兵クルー演出
// （GameOverScene のヘリ＆衛生兵と完全同一のモデル・スケール・向きでシームレス直結）
// -----------------------------------------------------------------------------

Vector3 GameOverSequenceController::LocalToHeliWorld(const Vector3& localUnscaledOffset) const
{
	Matrix4x4 R = Multiply(MakeRotateXMatrix(heliRot_.x), Multiply(MakeRotateYMatrix(heliRot_.y), MakeRotateZMatrix(heliRot_.z)));
	Vector3 s = localUnscaledOffset * kHeliScale;
	return {
		heliPos_.x + s.x * R.m[0][0] + s.y * R.m[1][0] + s.z * R.m[2][0],
		heliPos_.y + s.x * R.m[0][1] + s.y * R.m[1][1] + s.z * R.m[2][1],
		heliPos_.z + s.x * R.m[0][2] + s.y * R.m[1][2] + s.z * R.m[2][2]
	};
}

void GameOverSequenceController::UpdateMedevacHelicopter(float realDeltaTime)
{
	if (timer_ < 0.74f)
	{
		heliVisible_ = false;
		return;
	}
	heliVisible_ = true;

	// 1. 飛行アプローチ (0.74s 〜 1.66s) → 制動ホバリング＆救出降下 (1.66s 〜 2.68s)
	float arrivalU = (std::clamp)((timer_ - 0.74f) / 0.92f, 0.0f, 1.0f);
	float arrivalEase = EaseOutCubic(arrivalU);
	heliPos_ = LerpVector3(heliStartPos_, heliHoverPos_, arrivalEase);

	// Act 3 で回収ホバリングのためわずかに高度を下げる（障害物頂部からは常に +0.85m 以上の安全余白を維持）
	if (timer_ > kAct2Duration)
	{
		float descendU = EaseInOutCubic((std::clamp)((timer_ - kAct2Duration) / (kDuration - kAct2Duration), 0.0f, 1.0f));
		heliPos_.y -= 0.32f * descendU;
	}
	heliPos_.y += std::sin(timer_ * 3.8f) * 0.05f;

	// 高速進入のダイナミック前傾＆コンバットバンク (+0.22 pitch, -0.26 roll) → 豪快な制動フレア機首上げ (-0.25 pitch) → 重厚ホバリング
	float pitch = 0.0f;
	float roll = 0.0f;
	float yawSweep = (1.0f - arrivalEase) * (-0.28f); // 旋回しながら現場上空へ滑り込む！
	if (arrivalU < 0.44f)
	{
		float pU = arrivalU / 0.44f;
		pitch = 0.22f * (1.0f - pU) - 0.25f * pU;
		roll = -0.26f * std::sin(pU * kPi);
	}
	else if (arrivalU < 1.0f)
	{
		float pU = (arrivalU - 0.44f) / 0.56f;
		pitch = -0.25f * (1.0f - EaseOutQuad(pU));
		roll = 0.10f * std::sin(pU * kPi);
	}
	else
	{
		pitch = std::sin(timer_ * 2.5f) * 0.022f;
		roll = std::cos(timer_ * 2.1f) * 0.022f;
	}
	heliRot_ = { pitch, heliBaseYaw_ + yawSweep, roll };

	// ローター高速回転（ストロボ静止錯視を回避する角速度 33 rad/s & 68 rad/s）
	mainRotorAngle_ += realDeltaTime * 33.5f;
	tailRotorAngle_ += realDeltaTime * 68.0f;

	if (heliBody_)
	{
		heliBody_->SetTranslate(heliPos_);
		heliBody_->SetRotate(heliRot_);
		heliBody_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliBody_->Update();
	}

	if (heliMainRotor_)
	{
		Vector3 mastWorld = LocalToHeliWorld({ 0.0f, 4.15f, 0.75f });
		heliMainRotor_->SetTranslate(mastWorld);
		heliMainRotor_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_, heliRot_.z });
		heliMainRotor_->Update();

		if (heliMainRotorBlur1_)
		{
			heliMainRotorBlur1_->SetTranslate(mastWorld);
			heliMainRotorBlur1_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - 0.09f, heliRot_.z });
			heliMainRotorBlur1_->Update();
		}
		if (heliMainRotorBlur2_)
		{
			heliMainRotorBlur2_->SetTranslate(mastWorld);
			heliMainRotorBlur2_->SetRotate({ heliRot_.x, heliRot_.y + mainRotorAngle_ - 0.18f, heliRot_.z });
			heliMainRotorBlur2_->Update();
		}
	}

	if (heliTailRotor_)
	{
		Vector3 tailWorld = LocalToHeliWorld({ 0.20f, 3.35f, -8.60f });
		heliTailRotor_->SetTranslate(tailWorld);
		heliTailRotor_->SetRotate({ heliRot_.x + tailRotorAngle_, heliRot_.y, heliRot_.z });
		heliTailRotor_->Update();
	}

	// 2. 大型スライドカーゴドア開放アニメーション (1.12s 〜 1.60s で全開！)
	doorOpenProgress_ = EaseInOutCubic((std::clamp)((timer_ - 1.12f) / 0.48f, 0.0f, 1.0f));
	if (heliCargoDoor_)
	{
		// doorSlideProgress: 1.0(閉) -> 0.0(全開、GameOverScene の待機中と完全一致)
		float closedRatio = 1.0f - doorOpenProgress_;
		float curX = 1.470f - 0.015f * closedRatio;
		float curZ = -1.90f + (0.75f - (-1.90f)) * closedRatio;
		heliCargoDoor_->SetTranslate(LocalToHeliWorld({ curX, 1.14f, curZ }));
		heliCargoDoor_->SetRotate(heliRot_);
		heliCargoDoor_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		heliCargoDoor_->Update();
	}

	// 3. 開放されたキャビン内で救難室内灯に照らされて待機する衛生兵アヒル2羽 ＆ 担架（GameOverScene の機内配置と統一）
	const float medicBob = std::sin(timer_ * 5.5f) * 0.022f;
	const Vector4 cabinLitColor = { 1.28f, 1.18f, 1.02f, 1.0f };
	if (cabinFrontMedic_)
	{
		cabinFrontMedic_->SetTranslate(LocalToHeliWorld({ 0.62f, 0.12f + medicBob, 0.72f }));
		cabinFrontMedic_->SetRotate({ heliRot_.x + 0.20f, heliRot_.y + kPi * 0.5f, heliRot_.z });
		cabinFrontMedic_->SetScale({ 0.39f, 0.39f, 0.39f });
		cabinFrontMedic_->SetColor(cabinLitColor);
		cabinFrontMedic_->Update();
	}
	if (cabinRearMedic_)
	{
		cabinRearMedic_->SetTranslate(LocalToHeliWorld({ 0.60f, 0.12f - medicBob, -0.16f }));
		cabinRearMedic_->SetRotate({ heliRot_.x + 0.15f, heliRot_.y + kPi * 0.5f, heliRot_.z });
		cabinRearMedic_->SetScale({ 0.39f, 0.39f, 0.39f });
		cabinRearMedic_->SetColor(cabinLitColor);
		cabinRearMedic_->Update();
	}
	if (cabinStretcher_)
	{
		cabinStretcher_->SetTranslate(LocalToHeliWorld({ -0.25f, 0.20f, 0.55f }));
		cabinStretcher_->SetRotate({ heliRot_.x, heliRot_.y - kPi * 0.5f, heliRot_.z });
		cabinStretcher_->SetScale({ kHeliScale, kHeliScale, kHeliScale });
		cabinStretcher_->SetColor({ 1.18f, 1.12f, 1.00f, 1.0f });
		cabinStretcher_->Update();
	}

	if (heliFastRope_)
	{
		heliFastRope_->SetScale({ 0.0f, 0.0f, 0.0f });
		heliFastRope_->Update();
	}

	// 5. 地上投影シャドウ ＆ 負傷アヒルを捕捉スイープするハロゲン救難サーチライト！
	if (heliShadow_)
	{
		float alt = (std::max)(0.5f, heliPos_.y - groundY_);
		float shadowAlpha = (std::clamp)(0.52f - alt * 0.025f, 0.08f, 0.48f) * arrivalEase;
		heliShadow_->SetTranslate({ heliPos_.x, groundY_ + 0.035f, heliPos_.z });
		heliShadow_->SetRotate({ 0.0f, heliRot_.y, 0.0f });
		heliShadow_->SetScale({ 1.0f, 1.0f, 1.0f });
		heliShadow_->SetColor({ 0.02f, 0.04f, 0.07f, shadowAlpha });
		heliShadow_->Update();
	}

	// 開放キャビン内部から斜め下方へ広がる 3D アンバー室内投光コーン更新 (Mode 0)
	if (volumetricCabinBeam_ && doorOpenProgress_ > 0.02f)
	{
		Vector3 cabOrigin = LocalToHeliWorld({ 0.35f, 1.55f, 0.20f });
		Vector3 cabTarget = LocalToHeliWorld({ 2.45f, -0.75f, 0.20f });
		Vector3 cabDiff = cabTarget - cabOrigin;
		float cabLen = (std::max)(0.2f, std::sqrt(cabDiff.x * cabDiff.x + cabDiff.y * cabDiff.y + cabDiff.z * cabDiff.z));
		volumetricCabinBeam_->SetTranslate(cabOrigin);
		volumetricCabinBeam_->SetRotate(CalcLookAtRotation(cabOrigin, cabTarget));
		volumetricCabinBeam_->SetScale({ 1.0f, 1.0f, cabLen });
		volumetricCabinBeam_->SetColor({ 1.0f, 0.78f, 0.38f, 0.20f * doorOpenProgress_ });
		volumetricCabinBeam_->SetReflectionFactor(timer_);
		volumetricCabinBeam_->SetFresnelF0(0.0f);
		volumetricCabinBeam_->Update();
	}

	searchlightTurretPos_ = LocalToHeliWorld({ 0.0f, 0.18f, 2.35f });
	if (timer_ >= 1.00f)
	{
		float sweepU = EaseOutCubic((std::clamp)((timer_ - 1.00f) / 0.46f, 0.0f, 1.0f));
		searchlightIntensity_ = EaseOutQuad((std::clamp)((timer_ - 1.00f) / 0.26f, 0.0f, 1.0f));
		Vector3 focusTarget = GetFinalFallenFocus();
		Vector3 screenRight = { -camAzimuthDir_.z, 0.0f, camAzimuthDir_.x };
		Vector3 sweepStart = focusTarget + screenRight * 2.85f + camAzimuthDir_ * 1.10f;
		searchlightGroundPos_ = LerpVector3(sweepStart, focusTarget, sweepU);
		searchlightGroundPos_.y = groundY_ + 0.038f;

		// 3D ボリュメトリック・サーチライト光条コーン（外側ミー散乱＋内側高輝度コア）を機首タレットから地表スポットへ正確にストレッチ
		Vector3 beamVec = searchlightGroundPos_ - searchlightTurretPos_;
		float beamLen = (std::max)(0.5f, std::sqrt(beamVec.x * beamVec.x + beamVec.y * beamVec.y + beamVec.z * beamVec.z));
		Vector3 beamRot = CalcLookAtRotation(searchlightTurretPos_, searchlightGroundPos_);

		if (volumetricOuterBeam_)
		{
			volumetricOuterBeam_->SetTranslate(searchlightTurretPos_);
			volumetricOuterBeam_->SetRotate(beamRot);
			volumetricOuterBeam_->SetScale({ 1.0f, 1.0f, beamLen });
			volumetricOuterBeam_->SetColor({ 1.0f, 0.88f, 0.54f, 0.22f * searchlightIntensity_ });
			volumetricOuterBeam_->SetReflectionFactor(timer_);
			volumetricOuterBeam_->SetFresnelF0(0.0f);
			volumetricOuterBeam_->Update();
		}
		if (volumetricInnerBeam_)
		{
			volumetricInnerBeam_->SetTranslate(searchlightTurretPos_);
			volumetricInnerBeam_->SetRotate(beamRot);
			volumetricInnerBeam_->SetScale({ 1.0f, 1.0f, beamLen });
			volumetricInnerBeam_->SetColor({ 1.0f, 0.96f, 0.80f, 0.30f * searchlightIntensity_ });
			volumetricInnerBeam_->SetReflectionFactor(timer_ * 1.25f);
			volumetricInnerBeam_->SetFresnelF0(0.0f);
			volumetricInnerBeam_->Update();
		}

		// 3D HLSL コースティクス地上ライトプール更新 (Mode 1)
		if (searchlightSpot_)
		{
			float spotPulse = 0.94f + 0.06f * std::sin(timer_ * 14.0f);
			searchlightSpot_->SetTranslate(searchlightGroundPos_);
			searchlightSpot_->SetRotate({ 0.0f, timer_ * 0.45f, 0.0f });
			searchlightSpot_->SetScale({ spotPulse, 1.0f, spotPulse });
			searchlightSpot_->SetColor({ 1.0f, 0.90f, 0.58f, 0.48f * searchlightIntensity_ });
			searchlightSpot_->SetReflectionFactor(timer_);
			searchlightSpot_->SetFresnelF0(1.0f);
			searchlightSpot_->Update();
		}

		// ビーム体積内できらめき浮遊する 3D チンダル微粒子（光の柱の中を実際に舞うリアル光学ダスト！）
		if (scene_ && scene_->particleManager && scene_->appParticleManager_)
		{
			tyndallMoteTimer_ += realDeltaTime;
			while (tyndallMoteTimer_ >= 0.026f)
			{
				tyndallMoteTimer_ -= 0.026f;
				float h = RandRange(0.10f, 0.94f);
				float coneR = (0.10f + 1.55f * h) * std::sqrt(Rand01());
				float theta = RandRange(0.0f, 2.0f * kPi);
				Vector3 axisPt = LerpVector3(searchlightTurretPos_, searchlightGroundPos_, h);
				Vector3 motePos = axisPt + Vector3{ std::cos(theta) * coneR, 0.0f, std::sin(theta) * coneR };

				uint32_t moteTex = (circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : scene_->particleTextureB;
				AppParticle mote{};
				mote.transform.Initialize();
				mote.transform.SetTranslate(motePos);
				mote.initialScale = RandRange(0.045f, 0.10f);
				mote.targetScale = 0.015f;
				mote.transform.SetScale({ mote.initialScale, mote.initialScale, 1.0f });
				mote.velocity = { RandRange(-0.85f, 0.85f), RandRange(-0.95f, 0.45f), RandRange(-0.85f, 0.85f) };
				mote.color = { 1.0f, 0.94f, 0.72f, 0.78f * searchlightIntensity_ };
				mote.endColor = { 1.0f, 0.82f, 0.42f, 0.0f };
				mote.hasColorShift = true;
				mote.curlFreq = 2.6f;
				mote.curlAmp = 0.45f;
				mote.twinkleSpeed = 18.0f;
				mote.twinklePhase = RandRange(0.0f, 6.28f);
				mote.lifeTime = RandRange(0.38f, 0.72f);
				mote.currentTime = 0.0f;
				mote.textureIndex = moteTex;
				scene_->appParticleManager_->AddParticle(mote);
			}
		}
	}
	else
	{
		searchlightIntensity_ = 0.0f;
	}

	// 6. 暴風ダウンウォッシュ渦（砂塵＋舞い散る木の葉＋白羽）＆ ツインタービン排気 ＆ 4枚翼端ベイパー ＆ 航空灯ストロボ
	if (scene_ && scene_->particleManager && scene_->appParticleManager_)
	{
		auto& randEng = scene_->particleManager->GetRandomEngine();
		Vector3 focusGround = GetFinalFallenFocus();
		focusGround.y = groundY_ + 0.05f;
		uint32_t flashTex = (scene_->starburstTextureIndex_ != TextureManager::kInvalidTextureIndex)
			? scene_->starburstTextureIndex_
			: ((circle2TextureIndex_ != UINT32_MAX) ? circle2TextureIndex_ : scene_->particleTextureB);
		uint32_t leafTex = (leafTextureIndex_ != UINT32_MAX) ? leafTextureIndex_ : scene_->particleTextureB;

		if (timer_ >= 1.18f)
		{
			downwashTimer_ += realDeltaTime;
			if (downwashTimer_ >= 0.045f)
			{
				downwashTimer_ = 0.0f;
				for (int i = 0; i < 8; ++i)
				{
					float a = i * (2.0f * kPi / 8.0f) + timer_ * 2.2f + RandRange(-0.20f, 0.20f);
					float spd = RandRange(4.2f, 7.2f);
					// 放射方向＋螺旋接線ベクトルでリアルなローター渦気流を表現
					float dirX = std::cos(a) * 0.82f - std::sin(a) * 0.57f;
					float dirZ = std::sin(a) * 0.82f + std::cos(a) * 0.57f;
					Vector3 spawnP = focusGround + Vector3{ std::cos(a) * 1.25f, 0.02f, std::sin(a) * 1.25f };
					Vector3 vel = { dirX * spd, RandRange(0.22f, 0.65f), dirZ * spd };
					scene_->appParticleManager_->EmitDustWithVelocity(
						randEng, spawnP, 0.82f,
						{ 0.85f, 0.86f, 0.90f, 0.34f }, vel, 0.42f, scene_->smokeTextureIndex_
					);
				}

				// ローター暴風でサーチライトの中を舞い上がる木の葉！
				for (int l = 0; l < 2; ++l)
				{
					float la = RandRange(0.0f, 2.0f * kPi);
					float lr = RandRange(1.0f, 3.4f);
					Vector3 leafPos = focusGround + Vector3{ std::cos(la) * lr, RandRange(0.10f, 0.55f), std::sin(la) * lr };
					Vector3 leafVel = { std::cos(la + 0.6f) * RandRange(3.5f, 6.5f), RandRange(1.6f, 3.6f), std::sin(la + 0.6f) * RandRange(3.5f, 6.5f) };
					Vector4 leafCol = (l == 0)
						? Vector4{ 0.30f, 0.62f, 0.22f, 0.92f }
						: Vector4{ 0.48f, 0.58f, 0.20f, 0.90f };
					scene_->appParticleManager_->EmitSparkWithVelocity(
						randEng, leafPos, leafVel, leafCol, 0.22f, 0.65f, leafTex
					);
				}
			}
		}

		// ツインタービン高温排気ジェット
		exhaustTimer_ += realDeltaTime;
		if (exhaustTimer_ >= 0.040f)
		{
			exhaustTimer_ = 0.0f;
			Vector3 noseDir = { std::sin(heliRot_.y), 0.0f, std::cos(heliRot_.y) };
			Vector3 exhVel = noseDir * (-5.8f) + Vector3{ RandRange(-0.3f, 0.3f), -0.4f, RandRange(-0.3f, 0.3f) };
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, LocalToHeliWorld({ -0.75f, 3.45f, -0.45f }), 0.36f,
				{ 0.38f, 0.37f, 0.36f, 0.26f }, exhVel, 0.32f, scene_->smokeTextureIndex_
			);
			scene_->appParticleManager_->EmitDustWithVelocity(
				randEng, LocalToHeliWorld({  0.75f, 3.45f, -0.45f }), 0.36f,
				{ 0.38f, 0.37f, 0.36f, 0.26f }, exhVel, 0.32f, scene_->smokeTextureIndex_
			);
		}

		// 4枚ローターすべての翼端から螺旋ベイパーリボンを発生
		rotorVaporTimer_ += realDeltaTime;
		if (rotorVaporTimer_ >= 0.026f)
		{
			rotorVaporTimer_ = 0.0f;
			for (int b = 0; b < 4; ++b)
			{
				float a = mainRotorAngle_ + b * (kPi * 0.5f);
				Vector3 tipPos = LocalToHeliWorld({ std::cos(a) * 3.25f, 4.10f, 0.75f + std::sin(a) * 3.25f });
				scene_->appParticleManager_->EmitDustWithVelocity(
					randEng, tipPos, 0.26f,
					{ 0.94f, 0.97f, 1.0f, 0.30f }, { 0.0f, -2.2f, 0.0f }, 0.16f, scene_->smokeTextureIndex_
				);
			}
		}

		// 尾翼レッドビーコン ＆ 機体腹部ダブルホワイトストロボ
		strobeTimer_ += realDeltaTime;
		if (strobeTimer_ >= 0.48f)
		{
			strobeTimer_ = 0.0f;
			Vector3 tailBeacon = LocalToHeliWorld({ 0.05f, 4.35f, -8.70f });
			Vector3 bellyStrobe = LocalToHeliWorld({ 0.0f, 0.15f, 0.45f });
			scene_->appParticleManager_->EmitMuzzleFlare(
				randEng, tailBeacon, 1.15f, { 1.0f, 0.14f, 0.08f, 1.0f }, 0.09f, flashTex
			);
			scene_->appParticleManager_->EmitMuzzleFlare(
				randEng, bellyStrobe, 1.25f, { 0.95f, 0.98f, 1.0f, 0.95f }, 0.07f, flashTex
			);
		}
	}
}

void GameOverSequenceController::RestoreSceneLighting()
{
	if (!hasSavedLighting_) return;
	if (SceneManager::GetInstance() && SceneManager::GetInstance()->GetLight())
	{
		Light* light = SceneManager::GetInstance()->GetLight();
		light->SetDirectionalLight(savedDirLight_.color, savedDirLight_.direction, savedDirLight_.intensity);
		light->SetPointLight(
			savedPointLight_.color,
			savedPointLight_.position,
			savedPointLight_.intensity,
			savedPointLight_.radius,
			savedPointLight_.decay
		);
	}
	hasSavedLighting_ = false;
}

// -----------------------------------------------------------------------------
// アヒル中心座標算出 ＆ 3幕連続カメラワーク（オブジェクト非消去・クリア俯瞰 → ヘリ収容接続）
// -----------------------------------------------------------------------------

Vector3 GameOverSequenceController::GetDuckVisualCenter() const
{
	Matrix4x4 W = MakeAffineMatrix(duckScale_, duckRot_, duckPos_);
	return {
		0.88f * W.m[1][0] + 0.06f * W.m[2][0] + W.m[3][0],
		0.88f * W.m[1][1] + 0.06f * W.m[2][1] + W.m[3][1],
		0.88f * W.m[1][2] + 0.06f * W.m[2][2] + W.m[3][2]
	};
}

Vector3 GameOverSequenceController::GetFinalFallenFocus() const
{
	Vector3 landPos = { safeLandingPos_.x, groundY_ + 0.14f, safeLandingPos_.z };
	Matrix4x4 W = MakeAffineMatrix(
		Vector3{ 1.04f, 0.96f, 1.04f },
		fallFinalRot_,
		landPos
	);
	return {
		0.88f * W.m[1][0] + 0.06f * W.m[2][0] + W.m[3][0],
		0.88f * W.m[1][1] + 0.06f * W.m[2][1] + W.m[3][1],
		0.88f * W.m[1][2] + 0.06f * W.m[2][2] + W.m[3][2]
	};
}

void GameOverSequenceController::UpdateCamera(float /*realDeltaTime*/)
{
	if (!scene_ || !scene_->camera_) return;

	const Vector3 duckFocusNow = GetDuckVisualCenter();
	const Vector3 focusLanding = GetFinalFallenFocus();
	const Vector3 act1Eye = focusLanding + camAzimuthDir_ * rigHorizDistAct1_ + Vector3{ 0.0f, rigHeightAct1_, 0.0f };
	const Vector3 act2Eye = focusLanding + camAzimuthDir_ * rigHorizDistAct2_ + Vector3{ 0.0f, rigHeightAct2_, 0.0f };
	const Vector3 screenRight = { -camAzimuthDir_.z, 0.0f, camAzimuthDir_.x };

	Vector3 camPos{};
	Vector3 lookTarget = duckFocusNow;
	float camRoll = 0.0f;
	float camFov = 0.56f;
	float impactPitchKick = 0.0f;
	float impactYawKick = 0.0f;
	const float rollSign = -cameraSide_;

	if (timer_ < kAct1Duration)
	{
		// 【Act 1: 通常見下ろし視点 → 上方アークを描きながらクリア俯瞰ショットへ滑らかにドリー】 (0.00s 〜 0.84s)
		float u = (std::clamp)(timer_ / kAct1Duration, 0.0f, 1.0f);
		float smoothDolly = EaseOutCubic(u);
		camPos = LerpVector3(initialCamPos_, act1Eye, smoothDolly);
		// ドリー途中の遮蔽物上部をふんわり越えるクリアランスアーク
		camPos.y += std::sin(smoothDolly * kPi) * 0.65f;

		// 被弾瞬間および地面激突時の繊細な回転パンチ（カメラ位置が地面や箱にめり込まない安全振動）
		if (timer_ < 0.34f)
		{
			float shock = (1.0f - timer_ / 0.34f);
			shock *= shock;
			impactPitchKick = std::sin(timer_ * 52.0f) * 0.015f * shock;
			impactYawKick   = std::cos(timer_ * 43.0f) * 0.012f * shock;
		}
		else
		{
			const float slamStart = (fallVariant_ == 2) ? 0.50f : 0.36f;
			float slamT = (timer_ - slamStart) / 0.28f;
			if (slamT >= 0.0f && slamT < 1.0f)
			{
				float decay = 1.0f - slamT;
				impactPitchKick = std::sin(slamT * kPi * 3.0f) * 0.016f * decay;
			}
		}

		lookTarget = duckFocusNow;
		camRoll = -0.032f * std::sin(u * kPi) * rollSign;
		camFov = initialCamFov_ + (0.56f - initialCamFov_) * smoothDolly;
	}
	else if (timer_ < kAct2Duration)
	{
		// 【Act 2: 倒れた自機（下部黒帯に絶対かからない余裕位置）と上空飛来する救難ヘリの同時フレーミング】 (0.84s 〜 1.98s)
		float u = (std::clamp)((timer_ - kAct1Duration) / (kAct2Duration - kAct1Duration), 0.0f, 1.0f);
		float ease = EaseInOutCubic(u);

		camPos = LerpVector3(act1Eye, act2Eye, ease);

		float rotorVib = (std::clamp)((timer_ - 1.15f) / 0.55f, 0.0f, 1.0f);
		impactPitchKick = std::sin(timer_ * 42.0f) * 0.004f * rotorVib;
		impactYawKick   = std::cos(timer_ * 37.0f) * 0.003f * rotorVib;

		// 注視点の上昇を 32% に抑え、画面下部の黒帯でプレイヤーが見切れるのを完全に防止！
		Vector3 cabinDoorWorld = LocalToHeliWorld({ 1.05f, 0.95f, 0.20f });
		Vector3 midRescueTarget = {
			focusLanding.x * 0.68f + cabinDoorWorld.x * 0.32f,
			focusLanding.y * 0.68f + cabinDoorWorld.y * 0.32f,
			focusLanding.z * 0.68f + cabinDoorWorld.z * 0.32f
		};
		lookTarget = LerpVector3(duckFocusNow, midRescueTarget, ease);
		camRoll = 0.0f;
		camFov = 0.56f + 0.13f * ease; // 0.56 -> 0.69 rad: 下部の自機も上部の救難ヘリも余裕を持って一望
	}
	else
	{
		// 【Act 3: 救難ヘリの開放カーゴドア＆衛生兵クルーへクレーンアップ → GameOverScene へマッチカット接続！】 (1.98s 〜 2.68s)
		float u = (std::clamp)((timer_ - kAct2Duration) / (kDuration - kAct2Duration), 0.0f, 1.0f);
		float ease = EaseInOutCubic(u);

		Vector3 cabinDoorWorld = LocalToHeliWorld({ 1.05f, 0.95f, 0.20f });
		Vector3 midRescueTarget = {
			focusLanding.x * 0.68f + cabinDoorWorld.x * 0.32f,
			focusLanding.y * 0.68f + cabinDoorWorld.y * 0.32f,
			focusLanding.z * 0.68f + cabinDoorWorld.z * 0.32f
		};
		Vector3 cabinFocusTarget = {
			focusLanding.x * 0.44f + cabinDoorWorld.x * 0.56f,
			focusLanding.y * 0.45f + cabinDoorWorld.y * 0.55f,
			focusLanding.z * 0.44f + cabinDoorWorld.z * 0.56f
		};

		Vector3 act3Eye = act2Eye - camAzimuthDir_ * 1.05f + screenRight * 0.42f + Vector3{ 0.0f, 0.50f, 0.0f };
		camPos = LerpVector3(act2Eye, act3Eye, ease);
		lookTarget = LerpVector3(midRescueTarget, cabinFocusTarget, ease);

		impactPitchKick = std::sin(timer_ * 44.0f) * 0.005f;
		impactYawKick   = std::cos(timer_ * 39.0f) * 0.004f;
		camRoll = 0.0f;
		camFov = 0.69f - 0.07f * ease;
	}

	// ドリー中・クレーン移動中も木やコンテナにカメラが一切潜り込まないようリアルタイム押し出し
	PushCameraOutOfObstacles(camPos);

	if (camPos.y < groundY_ + 2.30f)
	{
		camPos.y = groundY_ + 2.30f;
	}

	Vector3 camRot = CalcLookAtRotation(camPos, lookTarget, camRoll);
	camRot.x += impactPitchKick;
	camRot.y += impactYawKick;

	scene_->camera_->StopShake();
	scene_->camera_->SetTranslate(camPos);
	scene_->camera_->SetRotate(camRot);
	scene_->camera_->SetFovY(camFov);
	scene_->camera_->Update();
}

// -----------------------------------------------------------------------------
// シーン遷移（DeathTransition 重装甲シャッター → GameOverScene 前線ヘリパッド担架収容）
// -----------------------------------------------------------------------------

void GameOverSequenceController::FinishSequence()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;

	if (scene_)
	{
		scene_->SetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	}
	DeathTransition::GetInstance()->StartTransition("GAMEOVER", 1.40f);
}

void GameOverSequenceController::Skip()
{
	if (transitionTriggered_) return;
	transitionTriggered_ = true;

	if (scene_)
	{
		scene_->SetSceneData<RaidStats>("RaidStats", RaidStats::GetInstance());
	}
	DeathTransition::GetInstance()->StartTransition("GAMEOVER", 0.85f);
}

// -----------------------------------------------------------------------------
// 3Dレンダリング（ライフル・救難フレア・飛来する3D救難ヘリ＆衛生兵クルー）
// -----------------------------------------------------------------------------

void GameOverSequenceController::Draw3D(const RenderContext& ctx)
{
	if (!isActive_) return;

	// 1. 不透明・通常ブレンドの3Dモデル群を描画
	if (heliVisible_ && heliShadow_)
	{
		heliShadow_->Draw(ctx);
	}

	if (droppedRifle_)
	{
		droppedRifle_->Draw(ctx);
	}

	if (flareActive_)
	{
		if (flareCanister_)
		{
			flareCanister_->Draw(ctx);
		}
		if (flareGlowCap_ && timer_ >= 0.58f)
		{
			flareGlowCap_->Draw(ctx);
		}
	}

	if (heliVisible_)
	{
		if (cabinStretcher_)     cabinStretcher_->Draw(ctx);
		if (cabinFrontMedic_)    cabinFrontMedic_->Draw(ctx);
		if (cabinRearMedic_)     cabinRearMedic_->Draw(ctx);
		if (heliBody_)           heliBody_->Draw(ctx);
		if (heliCargoDoor_)      heliCargoDoor_->Draw(ctx);
		if (heliMainRotor_)      heliMainRotor_->Draw(ctx);
		if (heliMainRotorBlur1_) heliMainRotorBlur1_->Draw(ctx);
		if (heliMainRotorBlur2_) heliMainRotorBlur2_->Draw(ctx);
		if (heliTailRotor_)      heliTailRotor_->Draw(ctx);
	}

	// 2. 専用 HLSL ボリュメトリック加算シェーダーパス（3D深度テストあり・深度書き込みゼロで煙パーティクルも一切遮らない）
	if (volumetricAdditivePso_ && ctx.commandList && scene_ && scene_->object3dCom)
	{
		if (shockDiscActive_ && impactShockDisc_)
		{
			DrawVolumetricObject(impactShockDisc_.get(), ctx);
		}
		if (beaconWave1Active_ && beaconWaveDisc1_)
		{
			DrawVolumetricObject(beaconWaveDisc1_.get(), ctx);
		}
		if (beaconWave2Active_ && beaconWaveDisc2_)
		{
			DrawVolumetricObject(beaconWaveDisc2_.get(), ctx);
		}
		if (heliVisible_ && doorOpenProgress_ > 0.02f && volumetricCabinBeam_)
		{
			DrawVolumetricObject(volumetricCabinBeam_.get(), ctx);
		}
		if (heliVisible_ && searchlightIntensity_ > 0.02f)
		{
			if (searchlightSpot_)     DrawVolumetricObject(searchlightSpot_.get(), ctx);
			if (volumetricOuterBeam_) DrawVolumetricObject(volumetricOuterBeam_.get(), ctx);
			if (volumetricInnerBeam_) DrawVolumetricObject(volumetricInnerBeam_.get(), ctx);
		}

		// 後続パスのため通常の Object3D パイプラインへ確実に戻す
		ctx.commandList->SetPipelineState(scene_->object3dCom->GetPipelineState().Get());
	}
}

// -----------------------------------------------------------------------------
// 戦術心電図テレメトリ・スリム 2.35:1 レターボックス・簡潔な無線コール描画
// -----------------------------------------------------------------------------

void GameOverSequenceController::DrawUI()
{
#ifdef USE_IMGUI
	if (!isActive_) return;
	if (DeathTransition::GetInstance() && DeathTransition::GetInstance()->IsTransitioning()) return;

	ImDrawList* drawList = ImGui::GetForegroundDrawList();
	ImVec2 disp = ImGui::GetIO().DisplaySize;
	if (disp.x <= 0.0f || disp.y <= 0.0f) return;

	auto projectToScreen = [&](const Vector3& wp, ImVec2& outScreen) -> bool {
		if (!scene_ || !scene_->camera_) return false;
		Matrix4x4 vp = scene_->camera_->GetViewProjectionMatrix();
		float cx = wp.x * vp.m[0][0] + wp.y * vp.m[1][0] + wp.z * vp.m[2][0] + vp.m[3][0];
		float cy = wp.x * vp.m[0][1] + wp.y * vp.m[1][1] + wp.z * vp.m[2][1] + vp.m[3][1];
		float cw = wp.x * vp.m[0][3] + wp.y * vp.m[1][3] + wp.z * vp.m[2][3] + vp.m[3][3];
		if (cw <= 0.15f) return false;
		float ndcX = cx / cw;
		float ndcY = cy / cw;
		outScreen.x = (ndcX + 1.0f) * 0.5f * disp.x;
		outScreen.y = (1.0f - ndcY) * 0.5f * disp.y;
		return true;
	};

	// 1. 致命被弾瞬間のシネマティック・インパクトフラッシュ（0.10秒で鋭く減衰）
	if (impactFlashAlpha_ > 0.01f)
	{
		int whiteA = static_cast<int>(115 * impactFlashAlpha_);
		int redEdgeA = static_cast<int>(185 * impactFlashAlpha_);
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), disp, IM_COL32(255, 240, 230, whiteA));
		drawList->AddRect(ImVec2(0.0f, 0.0f), disp, IM_COL32(235, 35, 30, redEdgeA), 0.0f, 0, 56.0f);
	}

	// 2. サーチライト捕捉時の控えめなタクティカル・ロックオンコーナー枠
	if (timer_ >= 1.28f && timer_ < 2.38f)
	{
		ImVec2 duckScr{};
		if (projectToScreen(GetDuckVisualCenter(), duckScr))
		{
			float fadeIn = (std::clamp)((timer_ - 1.28f) / 0.16f, 0.0f, 1.0f);
			float fadeOut = (std::clamp)((2.38f - timer_) / 0.22f, 0.0f, 1.0f);
			float lockAlpha = fadeIn * fadeOut;
			float boxHalf = 34.0f + 14.0f * (1.0f - EaseOutQuad(fadeIn));
			float brk = 9.0f;
			ImU32 lockCol = IM_COL32(255, 222, 125, static_cast<int>(190 * lockAlpha));

			float x0 = duckScr.x - boxHalf, y0 = duckScr.y - boxHalf * 0.72f;
			float x1 = duckScr.x + boxHalf, y1 = duckScr.y + boxHalf * 0.72f;
			drawList->AddLine(ImVec2(x0, y0), ImVec2(x0 + brk, y0), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x0, y0), ImVec2(x0, y0 + brk), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x1, y0), ImVec2(x1 - brk, y0), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x1, y0), ImVec2(x1, y0 + brk), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x0, y1), ImVec2(x0 + brk, y1), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x0, y1), ImVec2(x0, y1 - brk), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x1, y1), ImVec2(x1 - brk, y1), lockCol, 1.8f);
			drawList->AddLine(ImVec2(x1, y1), ImVec2(x1, y1 - brk), lockCol, 1.8f);
		}
	}

	if (darkVignetteAlpha_ > 0.05f)
	{
		float pulse = 0.85f + 0.15f * std::sin(timer_ * 4.5f);
		int vigA = static_cast<int>(155 * darkVignetteAlpha_ * pulse);
		drawList->AddRect(ImVec2(0.0f, 0.0f), disp, IM_COL32(0, 0, 0, vigA / 2), 0.0f, 0, 40.0f);
	}

	// 4. スリム化した映画風レターボックス黒帯（7.6%高に抑え、上下の帯でプレイヤーが見切れる問題を完全解消）
	float maxBarH = disp.y * 0.076f;
	float barRatio = (std::min)(1.0f, timer_ / 0.22f);
	float curBarH = maxBarH * barRatio;

	ImFont* fontDefault = (ImGui::GetIO().Fonts && ImGui::GetIO().Fonts->Fonts.Size > 0) ? ImGui::GetIO().Fonts->Fonts[0] : ImGui::GetFont();
	ImFont* fontSub     = (ImGui::GetIO().Fonts && ImGui::GetIO().Fonts->Fonts.Size > 2) ? ImGui::GetIO().Fonts->Fonts[2] : fontDefault;

	if (curBarH > 0.5f)
	{
		drawList->AddRectFilled(ImVec2(0.0f, 0.0f), ImVec2(disp.x, curBarH), IM_COL32(4, 6, 10, 248));
		drawList->AddRectFilled(ImVec2(0.0f, disp.y - curBarH), ImVec2(disp.x, disp.y), IM_COL32(4, 6, 10, 248));

		ImU32 lineCol = (timer_ >= 1.02f) ? IM_COL32(225, 185, 85, 140) : IM_COL32(205, 58, 50, 140);
		drawList->AddLine(ImVec2(0.0f, curBarH), ImVec2(disp.x, curBarH), lineCol, 1.2f);
		drawList->AddLine(ImVec2(0.0f, disp.y - curBarH), ImVec2(disp.x, disp.y - curBarH), lineCol, 1.2f);

		if (barRatio > 0.85f)
		{
			const float midTopY = curBarH * 0.5f;

			// 左上：ステータスインジケーター ＋ リアルタイム心電図
			bool isMedevacPhase = (timer_ >= 1.02f);
			float dotBlink = 0.5f + 0.5f * std::sin(timer_ * 12.0f);
			ImU32 statusCol = isMedevacPhase
				? IM_COL32(255, 205, 85, static_cast<int>(180 + 75 * dotBlink))
				: IM_COL32(245, 65, 55, static_cast<int>(180 + 75 * dotBlink));

			drawList->AddCircleFilled(ImVec2(26.0f, midTopY), 4.0f, statusCol);
			const char* leftLabel = isMedevacPhase
				? "MEDEVAC 1-1 // INBOUND"
				: (isMIA_ ? "STATUS: MIA" : "VITAL ALERT // WIA");
			drawList->AddText(fontDefault, 13.0f, ImVec2(38.0f, midTopY - 6.5f), IM_COL32(226, 232, 240, 225), leftLabel);

			// ミニ心電図モニター波形
			const float ecgStartX = 215.0f;
			const float ecgW = 96.0f;
			if (disp.x > 760.0f)
			{
				ImVec2 prevPt{ ecgStartX, midTopY };
				for (int s = 1; s <= 26; ++s)
				{
					float u = static_cast<float>(s) / 26.0f;
					float x = ecgStartX + u * ecgW;
					float phase = std::fmod(u * 2.0f - timer_ * 2.2f + 10.0f, 1.0f);
					float amp = (timer_ < 0.70f) ? (1.0f - timer_ / 0.90f) : 0.08f;
					float spike = 0.0f;
					if (phase > 0.42f && phase < 0.48f) spike = -9.5f * amp;
					else if (phase >= 0.48f && phase < 0.54f) spike = 6.5f * amp;
					ImVec2 curPt{ x, midTopY + spike };
					drawList->AddLine(prevPt, curPt, statusCol, 1.4f);
					prevPt = curPt;
				}
			}

			// 右上：コンパクトなテレメトリ表記
			char rightBuf[140]{};
			if (!isMedevacPhase)
			{
				std::snprintf(rightBuf, sizeof(rightBuf), "%s  [%.1fm]", killerName_.c_str(), killerDistance_);
			}
			else
			{
				float curAlt = (std::max)(0.0f, heliPos_.y - groundY_);
				std::snprintf(rightBuf, sizeof(rightBuf), "UH-60 MEDEVAC  //  ALT %.1fm", curAlt);
			}
			ImVec2 rLabelSz = fontDefault->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, rightBuf);
			drawList->AddText(fontDefault, 13.0f, ImVec2(disp.x - rLabelSz.x - 24.0f, midTopY - 6.5f), IM_COL32(185, 205, 225, 215), rightBuf);
		}

		// 下部黒帯：スキップ案内
		float pulse = 0.55f + 0.45f * std::sin(timer_ * 5.0f);
		int alpha = static_cast<int>(210 * pulse);
		const char* skipPrompt = (const char*)u8"SPACE : スキップ";
		ImVec2 sSize = fontDefault->CalcTextSizeA(13.5f, FLT_MAX, 0.0f, skipPrompt);
		drawList->AddText(fontDefault, 13.5f, ImVec2((disp.x - sSize.x) * 0.5f, disp.y - curBarH * 0.5f - sSize.y * 0.5f), IM_COL32(203, 213, 225, alpha), skipPrompt);
	}

	// 5. 説明過多な長文ナレーションを廃止し、救難ヘリ到着時の短い無線コール1行のみで小粋に演出！
	if (timer_ >= 1.12f && timer_ < 2.42f)
	{
		const char* radioText = (const char*)u8"「救難ヘリ1-1、負傷兵を回収する！」";
		float fadeIn  = (std::clamp)((timer_ - 1.12f) / 0.16f, 0.0f, 1.0f);
		float fadeOut = (std::clamp)((2.42f - timer_) / 0.24f, 0.0f, 1.0f);
		float textAlpha = fadeIn * fadeOut;

		if (textAlpha > 0.01f)
		{
			ImVec2 rSize = fontSub->CalcTextSizeA(18.5f, FLT_MAX, 0.0f, radioText);
			int bgA = static_cast<int>(125 * textAlpha);
			int shadowA = static_cast<int>(230 * textAlpha);
			ImU32 subColor = IM_COL32(250, 230, 145, static_cast<int>(245 * textAlpha));
			const float subX = (disp.x - rSize.x) * 0.5f;
			const float subY = disp.y * 0.858f - rSize.y * 0.5f;
			drawList->AddRectFilled(
				ImVec2(subX - 14.0f, subY - 4.0f),
				ImVec2(subX + rSize.x + 14.0f, subY + rSize.y + 4.0f),
				IM_COL32(6, 10, 16, bgA), 4.0f
			);
			drawList->AddText(fontSub, 18.5f, ImVec2(subX + 1.5f, subY + 1.5f), IM_COL32(0, 0, 0, shadowA), radioText);
			drawList->AddText(fontSub, 18.5f, ImVec2(subX, subY), subColor, radioText);
		}
	}
#endif
}
