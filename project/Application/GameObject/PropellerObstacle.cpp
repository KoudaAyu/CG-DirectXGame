#include "PropellerObstacle.h"
#include "Object3dCom.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr float kTwoPi = 6.283185307f;

    inline float Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    inline float LengthSq(const Vector3& v)
    {
        return Dot(v, v);
    }

    inline float Length(const Vector3& v)
    {
        return std::sqrt(LengthSq(v));
    }

    inline Vector3 Normalize(const Vector3& v)
    {
        float len = Length(v);
        if (len > 1e-5f)
        {
            return v * (1.0f / len);
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    // 3x3 回転行列（行ベクトル形式: v * Rx * Ry * Rz）からエンジンのオイラー角 (rx, ry, rz) を逆算
    Vector3 MatrixToEulerXYZ(const Matrix4x4& R)
    {
        Vector3 euler;
        float sy = -R.m[0][2];
        sy = std::clamp(sy, -1.0f, 1.0f);
        euler.y = std::asin(sy);

        float cy = std::cos(euler.y);
        if (std::abs(cy) > 1e-4f)
        {
            euler.x = std::atan2(R.m[1][2], R.m[2][2]);
            euler.z = std::atan2(R.m[0][1], R.m[0][0]);
        }
        else
        {
            euler.x = std::atan2(-R.m[2][1], R.m[1][1]);
            euler.z = 0.0f;
        }
        return euler;
    }

    // 点 p から三角形 (a, b, c) 上の最近点を算出 (Real-Time Collision Detection, Christer Ericson 準拠)
    Vector3 ClosestPointOnTriangle(const Vector3& p, const Vector3& a, const Vector3& b, const Vector3& c)
    {
        Vector3 ab = b - a;
        Vector3 ac = c - a;
        Vector3 ap = p - a;
        float d1 = Dot(ab, ap);
        float d2 = Dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return a; // バーテックス領域 A

        Vector3 bp = p - b;
        float d3 = Dot(ab, bp);
        float d4 = Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return b; // バーテックス領域 B

        float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
        {
            float v = d1 / (d1 - d3);
            return a + ab * v; // エッジ領域 AB
        }

        Vector3 cp = p - c;
        float d5 = Dot(ab, cp);
        float d6 = Dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return c; // バーテックス領域 C

        float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
        {
            float w = d2 / (d2 - d6);
            return a + ac * w; // エッジ領域 AC
        }

        float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f)
        {
            float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            return b + (c - b) * w; // エッジ領域 BC
        }

        // フェース領域内部
        float denom = 1.0f / (va + vb + vc);
        float v = vb * denom;
        float w = vc * denom;
        return a + ab * v + ac * w;
    }
}

PropellerObstacle::~PropellerObstacle()
{
    Finalize();
}

void PropellerObstacle::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& basePosition,
                                   const Vector3& scale, float spinSpeed,
                                   const std::string& modelDirectory, const std::string& modelFilename)
{
    object3dCom_ = object3dCom;
    basePosition_ = basePosition;
    currentWorldPos_ = basePosition;
    scale_ = scale;
    spinSpeed_ = spinSpeed;
    currentAngle_ = 0.0f;

    // 1. プロペラモデルの読み込みと初期化
    object3d_ = std::make_unique<Object3d>();
    if (object3d_)
    {
        object3d_->Initialize(modelDirectory, modelFilename);
        object3d_->SetCamera(camera);
        object3d_->SetTranslate(currentWorldPos_);
        object3d_->SetScale(scale_);
        object3d_->SetRotate({ 0.0f, 0.0f, 0.0f });
        object3d_->SetEnableLighting(true);
        object3d_->Update();
        currentWorldMatrix_ = object3d_->GetWorldMatrix();
        currentInvWorldMatrix_ = Inverse(currentWorldMatrix_);
    }

    // 2. エンジン標準の MeshCollider を生成・登録
    BuildMeshCollider();

    isInitialized_ = true;
}

void PropellerObstacle::BuildMeshCollider()
{
    if (!object3d_) return;

    const auto& vertices = object3d_->GetModelData().vertices;
    if (vertices.empty()) return;

    // 情報表示用の寸法測定
    float maxRadius = 0.01f;
    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r > maxRadius) maxRadius = r;
    }
    detectedRadius_ = maxRadius;

    float hubRadius = 0.01f;
    float hubMinY = 1e9f, hubMaxY = -1e9f;
    int hubVertCount = 0;
    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r < maxRadius * 0.25f)
        {
            if (r > hubRadius) hubRadius = r;
            hubMinY = (std::min)(hubMinY, vtx.position.y);
            hubMaxY = (std::max)(hubMaxY, vtx.position.y);
            hubVertCount++;
        }
    }
    if (hubVertCount > 0)
    {
        detectedHubRadius_ = hubRadius;
        detectedHubCenterY_ = (hubMinY + hubMaxY) * 0.5f;
    }

    float wingMinY = 1e9f, wingMaxY = -1e9f;
    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r > maxRadius * 0.35f)
        {
            wingMinY = (std::min)(wingMinY, vtx.position.y);
            wingMaxY = (std::max)(wingMaxY, vtx.position.y);
        }
    }
    if (wingMinY < wingMaxY)
    {
        detectedWingThick_ = wingMaxY - wingMinY;
        detectedWingCenterY_ = (wingMinY + wingMaxY) * 0.5f;
    }
    detectedWingLen_ = (maxRadius - detectedHubRadius_) * 2.0f;
    detectedWingCount_ = 4;

    // エンジンに実装されている MeshCollider を生成して CollisionManager に登録！
    meshCollider_ = std::make_unique<MeshCollider>(object3d_.get(), CollisionAttribute::Obstacle);
    CollisionManager::GetInstance()->RegisterCollider(meshCollider_.get());
}

void PropellerObstacle::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot)
{
    if (!object3d_) return;

    // 1. プロペラの自転角度の進行
    currentAngle_ += spinSpeed_ * deltaTime;
    if (currentAngle_ > kTwoPi) currentAngle_ -= kTwoPi;
    else if (currentAngle_ < -kTwoPi) currentAngle_ += kTwoPi;

    // 2. ステージ傾斜の合成回転行列
    Matrix4x4 R_tilt = Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));

    // 3. 自転回転行列 (ローカルY軸回転)
    Matrix4x4 R_spin = MakeRotateYMatrix(currentAngle_);

    // 4. 合成姿勢行列
    Matrix4x4 R_combined = Multiply(R_spin, R_tilt);

    // 5. ステージ傾斜に伴う配置位置の回転
    Vector3 P = basePosition_;
    Vector3 P_rel = { P.x - pivot.x, P.y, P.z - pivot.y };
    Vector3 RP_rel = {
        P_rel.x * R_tilt.m[0][0] + P_rel.y * R_tilt.m[1][0] + P_rel.z * R_tilt.m[2][0],
        P_rel.x * R_tilt.m[0][1] + P_rel.y * R_tilt.m[1][1] + P_rel.z * R_tilt.m[2][1],
        P_rel.x * R_tilt.m[0][2] + P_rel.y * R_tilt.m[1][2] + P_rel.z * R_tilt.m[2][2]
    };
    currentWorldPos_ = { RP_rel.x + pivot.x, RP_rel.y, RP_rel.z + pivot.y };

    // 6. Object3d に反映
    Vector3 euler = MatrixToEulerXYZ(R_combined);
    object3d_->SetTranslate(currentWorldPos_);
    object3d_->SetRotate(euler);
    object3d_->SetScale(scale_);
    object3d_->Update();

    currentWorldMatrix_ = object3d_->GetWorldMatrix();
    currentInvWorldMatrix_ = Inverse(currentWorldMatrix_);

    // MeshCollider の更新（内部のワールド座標同期と AABBTree の更新）
    if (meshCollider_)
    {
        meshCollider_->SetWorldPosition(currentWorldPos_);
        meshCollider_->Update();
    }
}

bool PropellerObstacle::ResolveSlimeCollision(Vector3& slimePos, Vector3& slimeVel, float slimeRadius,
                                             bool isMerged, Vector3& slimeSquash, float& outImpulse)
{
    if (!object3d_ || !meshCollider_) return false;

    // 1. スライム位置をプロペラのローカル空間へ変換
    const Matrix4x4& invW = currentInvWorldMatrix_;
    Vector3 localPos = {
        slimePos.x * invW.m[0][0] + slimePos.y * invW.m[1][0] + slimePos.z * invW.m[2][0] + invW.m[3][0],
        slimePos.x * invW.m[0][1] + slimePos.y * invW.m[1][1] + slimePos.z * invW.m[2][1] + invW.m[3][1],
        slimePos.x * invW.m[0][2] + slimePos.y * invW.m[1][2] + slimePos.z * invW.m[2][2] + invW.m[3][2]
    };

    float propScale = (scale_.x + scale_.y + scale_.z) / 3.0f;
    if (propScale < 1e-4f) return false;
    float localRadius = slimeRadius / propScale;

    // 2. ブロードフェーズ判定: AABBTree のルート境界で超高速アーリーアウト
    Vector3 rootMin, rootMax;
    if (meshCollider_->GetAABBTree().GetRootBounds(rootMin, rootMax))
    {
        if (localPos.x + localRadius < rootMin.x || localPos.x - localRadius > rootMax.x ||
            localPos.y + localRadius < rootMin.y || localPos.y - localRadius > rootMax.y ||
            localPos.z + localRadius < rootMin.z || localPos.z - localRadius > rootMax.z)
        {
            return false;
        }
    }

    // 3. ナローフェーズ判定: モデルの全ポリゴン三角形に対する点・三角形最近点テスト
    const auto& modelData = object3d_->GetModelData();
    const auto& vertices = modelData.vertices;
    const auto& indices = modelData.indices;

    size_t numTris = indices.empty() ? (vertices.size() / 3) : (indices.size() / 3);
    if (numTris == 0) return false;

    bool collided = false;
    float maxPenetrationLocal = 0.0f;
    Vector3 bestNormalLocal{ 0.0f, 1.0f, 0.0f };
    Vector3 bestContactLocal{ 0.0f, 0.0f, 0.0f };

    for (size_t t = 0; t < numTris; ++t)
    {
        Vector3 v0, v1, v2;
        if (!indices.empty())
        {
            const auto& p0 = vertices[indices[t * 3 + 0]].position;
            const auto& p1 = vertices[indices[t * 3 + 1]].position;
            const auto& p2 = vertices[indices[t * 3 + 2]].position;
            v0 = { p0.x, p0.y, p0.z };
            v1 = { p1.x, p1.y, p1.z };
            v2 = { p2.x, p2.y, p2.z };
        }
        else
        {
            const auto& p0 = vertices[t * 3 + 0].position;
            const auto& p1 = vertices[t * 3 + 1].position;
            const auto& p2 = vertices[t * 3 + 2].position;
            v0 = { p0.x, p0.y, p0.z };
            v1 = { p1.x, p1.y, p1.z };
            v2 = { p2.x, p2.y, p2.z };
        }

        // 三角形上の最近点を算出
        Vector3 q = ClosestPointOnTriangle(localPos, v0, v1, v2);
        Vector3 diff = localPos - q;
        float distSq = LengthSq(diff);

        if (distSq < localRadius * localRadius)
        {
            float dist = std::sqrt(distSq);
            float pen = localRadius - dist;
            if (pen > maxPenetrationLocal)
            {
                maxPenetrationLocal = pen;
                bestContactLocal = q;

                if (dist > 1e-5f)
                {
                    bestNormalLocal = diff * (1.0f / dist);
                }
                else
                {
                    // 点が三角形と完全に重なっている場合は面法線を採用
                    Vector3 triNorm = Cross(v1 - v0, v2 - v0);
                    bestNormalLocal = Normalize(triNorm);
                }
                collided = true;
            }
        }
    }

    if (!collided) return false;

    // 4. 接触情報をワールド空間へ変換
    const Matrix4x4& W = currentWorldMatrix_;
    Vector3 contactWorld = {
        bestContactLocal.x * W.m[0][0] + bestContactLocal.y * W.m[1][0] + bestContactLocal.z * W.m[2][0] + W.m[3][0],
        bestContactLocal.x * W.m[0][1] + bestContactLocal.y * W.m[1][1] + bestContactLocal.z * W.m[2][1] + W.m[3][1],
        bestContactLocal.x * W.m[0][2] + bestContactLocal.y * W.m[1][2] + bestContactLocal.z * W.m[2][2] + W.m[3][2]
    };

    // 法線ベクトルのワールド変換（回転成分のみ）
    Vector3 normalWorld = {
        bestNormalLocal.x * W.m[0][0] + bestNormalLocal.y * W.m[1][0] + bestNormalLocal.z * W.m[2][0],
        bestNormalLocal.x * W.m[0][1] + bestNormalLocal.y * W.m[1][1] + bestNormalLocal.z * W.m[2][1],
        bestNormalLocal.x * W.m[0][2] + bestNormalLocal.y * W.m[1][2] + bestNormalLocal.z * W.m[2][2]
    };
    normalWorld = Normalize(normalWorld);

    float penetrationWorld = maxPenetrationLocal * propScale;

    // 5. めり込みの精密押し出し解消
    slimePos.x += normalWorld.x * penetrationWorld;
    slimePos.y += normalWorld.y * penetrationWorld;
    slimePos.z += normalWorld.z * penetrationWorld;

    // 6. プロペラの自転による打撃・弾き飛ばし線速度の算出
    // プロペラの回転軸（ワールド空間の上向き法線軸）
    Vector3 upAxisWorld = { W.m[1][0], W.m[1][1], W.m[1][2] };
    upAxisWorld = Normalize(upAxisWorld);

    // プロペラ中心から接触点への動径ベクトル
    Vector3 r = contactWorld - currentWorldPos_;
    // 回転軸に沿った角速度ベクトル omega
    Vector3 omega = upAxisWorld * spinSpeed_;
    // 接触点におけるプロペラ羽根の線速度 v_blade = omega x r
    Vector3 bladeVel = Cross(omega, r);

    // 外向き動径方向ベクトル
    Vector3 radial = { r.x, 0.0f, r.z };
    float rLen = Length(radial);
    Vector3 escapeDir = (rLen > 0.1f) ? (radial * (1.0f / rLen)) : normalWorld;

    // 相対速度
    Vector3 relVel = slimeVel - bladeVel;
    float normalRelSpeed = Dot(relVel, normalWorld);

    // 羽根がスライムに向かって衝突している、あるいは接近している場合
    if (normalRelSpeed < 0.0f)
    {
        // 反発係数 0.85 でパカーンと打撃
        float restitution = 0.85f;
        slimeVel = slimeVel - normalWorld * (normalRelSpeed * (1.0f + restitution));
    }

    // 羽根の回転慣性をスライムに伝達（接線方向のスピードを加算）
    slimeVel.x += bladeVel.x * 0.75f;
    slimeVel.z += bladeVel.z * 0.75f;

    // 外向き脱出インパルス
    float launchSpeed = isMerged ? 6.0f : 9.5f;
    slimeVel.x += escapeDir.x * launchSpeed;
    slimeVel.z += escapeDir.z * launchSpeed;

    // ミニオンなら小さく空中放物線を描く上向き成分
    if (!isMerged)
    {
        slimeVel.y = 3.5f;
    }

    // 7. スライムの変形演出と衝撃強度
    outImpulse = std::clamp(penetrationWorld * 3.0f + Length(bladeVel) * 0.15f, 0.35f, 1.0f);
    slimeSquash = { 0.22f, -0.25f, 0.22f };

    return true;
}

void PropellerObstacle::Draw(const RenderContext& ctx)
{
    if (!object3d_) return;
    object3d_->Draw(object3dCom_);
}

void PropellerObstacle::SetColor(const Vector4& color)
{
    if (object3d_)
    {
        object3d_->SetColor(color);
    }
}

void PropellerObstacle::Finalize()
{
    if (meshCollider_)
    {
        CollisionManager::GetInstance()->UnregisterCollider(meshCollider_.get());
        meshCollider_.reset();
    }
    if (object3d_)
    {
        object3d_.reset();
    }
    isInitialized_ = false;
}
