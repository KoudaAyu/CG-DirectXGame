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
    constexpr float kHalfPi = 1.570796327f;

    // 3x3 回転行列（行ベクトル形式: v * Rx * Ry * Rz）からエンジンのオイラー角 (rx, ry, rz) を正確に逆算
    Vector3 MatrixToEulerXYZ(const Matrix4x4& R)
    {
        Vector3 euler;
        // R[0][2] = -sin(ry)
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
            // ジンバルロック時のフォールバック
            euler.x = std::atan2(-R.m[2][1], R.m[1][1]);
            euler.z = 0.0f;
        }
        return euler;
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
    }

    // 2. モデルの頂点群から羽根の枚数・寸法を自動検出し、コライダーを自動構築
    AutoDetectAndBuildColliders();

    isInitialized_ = true;
}

void PropellerObstacle::AutoDetectAndBuildColliders()
{
    wings_.clear();

    if (!object3d_) return;

    const auto& vertices = object3d_->GetModelData().vertices;
    if (vertices.empty()) return;

    // --- ステップ1: 最大半径および中心ハブ円柱の精密測定 ---
    float maxRadius = 0.01f;
    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r > maxRadius) maxRadius = r;
    }
    detectedRadius_ = maxRadius;

    // 中心付近の頂点群 (r < maxRadius * 0.25f) からハブ寸法を検出
    float hubRadius = 0.01f;
    float hubMinY = 1e9f, hubMaxY = -1e9f;
    int hubVertCount = 0;

    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r < maxRadius * 0.25f)
        {
            if (r > hubRadius) hubRadius = r;
            if (vtx.position.y < hubMinY) hubMinY = vtx.position.y;
            if (vtx.position.y > hubMaxY) hubMaxY = vtx.position.y;
            hubVertCount++;
        }
    }

    if (hubVertCount == 0 || hubMaxY <= hubMinY)
    {
        hubRadius = maxRadius * 0.15f;
        hubMinY = 0.0f;
        hubMaxY = 0.4f;
    }

    detectedHubRadius_ = hubRadius;
    detectedHubCenterY_ = (hubMinY + hubMaxY) * 0.5f;

    // --- ステップ2: 外側頂点による円形平均クラスタリング（羽根の枚数・基準角の精密検出） ---
    // 外側頂点 (r > maxRadius * 0.55f) を抽出（ハブ円柱頂点を完全に除外）
    // ※エンジンの行ベクトル形式回転 MakeRotateYMatrix(θ) では (1,0,0)*Ry = (cos θ, 0, -sin θ) となるため、
    //   モデル空間頂点 (x, z) に一致させる角度は θ = atan2(-z, x)
    struct AngleCluster
    {
        float mean = 0.0f;
        std::vector<float> angles;
    };

    std::vector<AngleCluster> clusters;
    constexpr float kClusterThreshold = 0.2618f; // 15度 (rad)

    for (const auto& vtx : vertices)
    {
        float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
        if (r > maxRadius * 0.55f)
        {
            float a = std::atan2(-vtx.position.z, vtx.position.x);
            if (a < 0.0f) a += kTwoPi;

            bool matched = false;
            for (auto& cl : clusters)
            {
                // 円周上の角度差（ラッピング考慮）
                float diff = std::atan2(std::sin(a - cl.mean), std::cos(a - cl.mean));
                if (std::abs(diff) < kClusterThreshold)
                {
                    cl.angles.push_back(a);
                    // 円形平均 (circular mean) の再計算
                    float sumSin = 0.0f, sumCos = 0.0f;
                    for (float ang : cl.angles)
                    {
                        sumSin += std::sin(ang);
                        sumCos += std::cos(ang);
                    }
                    cl.mean = std::atan2(sumSin, sumCos);
                    if (cl.mean < 0.0f) cl.mean += kTwoPi;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                AngleCluster newCl;
                newCl.mean = a;
                newCl.angles.push_back(a);
                clusters.push_back(newCl);
            }
        }
    }

    // 昇順にソート
    std::sort(clusters.begin(), clusters.end(), [](const AngleCluster& a, const AngleCluster& b) {
        return a.mean < b.mean;
    });

    std::vector<float> wingAngles;
    if (clusters.size() >= 2 && clusters.size() <= 8)
    {
        detectedWingCount_ = static_cast<int>(clusters.size());
        for (const auto& cl : clusters)
        {
            wingAngles.push_back(cl.mean);
        }
    }
    else
    {
        // フォールバック: 十字4枚羽 (0, 90, 180, 270度)
        detectedWingCount_ = 4;
        wingAngles = { 0.0f, kHalfPi, 3.14159265f, 3.14159265f + kHalfPi };
    }

    // --- ステップ3: 主軸射影による真の羽根寸法の自動抽出と精密 OBB 生成 ---
    float sumThick = 0.0f, sumWidth = 0.0f, sumCenterY = 0.0f;
    // 羽根を中心（原点）から先端まで伸ばし、中心ハブ部分も4本のBoxColliderの交差で美しくカバー
    float wingLen = maxRadius;
    float centerDistU = maxRadius * 0.5f;

    for (int i = 0; i < detectedWingCount_; ++i)
    {
        float angle = wingAngles[i];
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);

        // この羽根に属する外側頂点 (r > maxRadius * 0.35f) を抽出
        std::vector<Vector3> wingPts;
        for (const auto& vtx : vertices)
        {
            float r = std::sqrt(vtx.position.x * vtx.position.x + vtx.position.z * vtx.position.z);
            if (r < maxRadius * 0.35f) continue;

            float va = std::atan2(-vtx.position.z, vtx.position.x);
            float diff = std::atan2(std::sin(va - angle), std::cos(va - angle));
            if (std::abs(diff) < (kTwoPi / static_cast<float>(detectedWingCount_)) * 0.5f)
            {
                wingPts.push_back({ vtx.position.x, vtx.position.y, vtx.position.z });
            }
        }

        // 局所基底 (u: 長さ方向, v: 幅方向, y: 厚み方向) へ射影
        float minY = 1e9f, maxY = -1e9f;
        float minV = 1e9f, maxV = -1e9f;

        for (const auto& pt : wingPts)
        {
            float y = pt.y;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;

            float v = pt.x * sinA + pt.z * cosA;
            if (v < minV) minV = v;
            if (v > maxV) maxV = v;
        }

        float thick = (maxY > minY) ? (maxY - minY) : 0.2f;
        float width = (maxV > minV) ? (maxV - minV) : 0.2f;
        float centerY = (minY + maxY) * 0.5f;

        sumThick += thick;
        sumWidth += width;
        sumCenterY += centerY;

        // ワールド寸法へのスケーリング
        auto wing = std::make_unique<Wing>();
        wing->baseAngle = angle;
        wing->rotationEuler = { 0.0f, angle, 0.0f };
        wing->length = wingLen * scale_.x;
        wing->thickness = thick * scale_.y;
        wing->width = width * scale_.z;
        wing->centerDistU = centerDistU * scale_.x;
        wing->centerY = centerY * scale_.y;

        // 各羽根の直方体サイズ: X軸方向に長さ、Y軸に厚み、Z軸に幅
        Vector3 boxSize = { wing->length, wing->thickness, wing->width };

        // ヒープ固定された wing->rotationEuler のアドレスを渡すことで、ポインタの永続性を保証
        wing->collider = std::make_unique<BoxCollider>(
            boxSize,
            &currentWorldPos_,
            &wing->rotationEuler,
            CollisionAttribute::Obstacle
        );

        if (CollisionManager::GetInstance())
        {
            CollisionManager::GetInstance()->RegisterCollider(wing->collider.get());
        }

        wings_.push_back(std::move(wing));
    }

    detectedWingLen_ = wingLen;
    detectedWingThick_ = sumThick / static_cast<float>(detectedWingCount_);
    detectedWingWidth_ = sumWidth / static_cast<float>(detectedWingCount_);
    detectedWingCenterY_ = sumCenterY / static_cast<float>(detectedWingCount_);
}

void PropellerObstacle::Update(float deltaTime, const Vector2& stageTilt, const Vector2& pivot)
{
    if (!isInitialized_) return;

    // 1. 自転角度の更新 (2πでラップ)
    currentAngle_ += spinSpeed_ * deltaTime;
    if (currentAngle_ > kTwoPi) currentAngle_ -= kTwoPi;
    if (currentAngle_ < -kTwoPi) currentAngle_ += kTwoPi;

    // 2. ステージ傾斜の追従（床面高さの算出）
    currentWorldPos_.x = basePosition_.x;
    currentWorldPos_.z = basePosition_.z;
    currentWorldPos_.y = SlimePhysics::CalculateGroundHeight(basePosition_.x, basePosition_.z, stageTilt, pivot);

    // 3. 正しい回転行列の合成:
    // 床面の傾斜行列 (Rx * Rz)
    Matrix4x4 R_tilt = Multiply(MakeRotateXMatrix(stageTilt.x), MakeRotateZMatrix(-stageTilt.y));

    // 4. 自動生成された各羽根の姿勢と中心位置オフセットを更新
    for (auto& wing : wings_)
    {
        if (!wing || !wing->collider) continue;

        // 羽根の総合角度 (自転 + 羽根の基準角)
        float totalAngle = currentAngle_ + wing->baseAngle;

        // 羽根の合成回転行列 (R_spin * R_tilt)
        Matrix4x4 R_wing = Multiply(MakeRotateYMatrix(totalAngle), R_tilt);

        // エンジンのオイラー角 (Rx * Ry * Rz) に分解し、ヒープ上の rotationEuler を直接更新
        wing->rotationEuler = MatrixToEulerXYZ(R_wing);

        // 羽根の動径中心オフセット (ローカルX軸方向 wing->centerDistU を R_wing で回転)
        // 行ベクトル形式: v * R
        Vector3 rotatedOffset = {
            wing->centerDistU * R_wing.m[0][0],
            wing->centerDistU * R_wing.m[0][1],
            wing->centerDistU * R_wing.m[0][2]
        };

        // 床面法線（R_tiltの第1行）に沿った高さ中心オフセットと合成
        Vector3 worldOffset = {
            rotatedOffset.x + wing->centerY * R_tilt.m[1][0],
            rotatedOffset.y + wing->centerY * R_tilt.m[1][1],
            rotatedOffset.z + wing->centerY * R_tilt.m[1][2]
        };

        wing->collider->SetPositionOffset(worldOffset);
    }

    // 5. 3Dオブジェクトのトランスフォーム更新
    if (object3d_)
    {
        Matrix4x4 R_model = Multiply(MakeRotateYMatrix(currentAngle_), R_tilt);
        object3d_->SetTranslate(currentWorldPos_);
        object3d_->SetRotate(MatrixToEulerXYZ(R_model));
        object3d_->Update();
    }
}

void PropellerObstacle::Draw(const RenderContext& ctx)
{
    if (!isInitialized_ || !object3d_) return;

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
    if (!isInitialized_) return;

    if (CollisionManager::GetInstance())
    {
        for (auto& wing : wings_)
        {
            if (wing && wing->collider)
            {
                CollisionManager::GetInstance()->UnregisterCollider(wing->collider.get());
                wing->collider.reset();
            }
        }
        wings_.clear();
    }

    object3d_.reset();
    isInitialized_ = false;
}
