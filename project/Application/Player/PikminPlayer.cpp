#include "PikminPlayer.h"
#include "Application/Minion/MinionManager.h"
#include "Application/GameObject/SlimeMesh.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Application/GameObject/SlimeCollision.h"
#include "Application/GameObject/AimGuide.h"
#include "Baziru3_Engine/Core/IO/Mouse/MouseInput.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include "Baziru3_Engine/Core/Base/Allocator/ConstantBufferAllocator.h"
#include "Baziru3_Engine/Framework/Collision/CollisionManager.h"
#include "TextureManager.h"
#include "DirectXCom.h"
#include "SceneManager.h"
#include "Light.h"
#include "KeyInput.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

PikminPlayer::PikminPlayer() {
    normalModel_ = std::make_unique<Object3d>();
    giantModel_ = std::make_unique<Object3d>();
}

PikminPlayer::~PikminPlayer() {
    if (meshCollider_) {
        CollisionManager::GetInstance()->UnregisterCollider(meshCollider_.get());
    }
}

void PikminPlayer::Initialize(Object3dCom* object3dCom, Camera* camera, const Vector3& startPos) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    position_ = startPos;
    position_.y = 0.5f;
    rotation_ = { 0.0f, 0.0f, 0.0f };
    scale_ = { 0.8f, 0.8f, 0.8f };
    isMerged_ = false;

    // スライム球体メッシュを生成（滑らかな高密度メッシュ）
    normalModelData_ = SlimeMesh::GenerateSphere(64, 32, 1.0f);
    normalTextureIndex_ = TextureManager::GetInstance()->Load("Resources/uvChecker.png");
    normalModelData_.material.textureIndex = normalTextureIndex_;

    if (normalModel_) {
        normalModel_->Initialize(object3dCom_, normalModelData_);
        normalModel_->SetCamera(camera_);
        normalModel_->SetTranslate(position_);
        normalModel_->SetScale(scale_);
        normalModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f }); // カラーはシェーダー側のbaseColorで制御
        normalModel_->SetEnableLighting(true);
        normalModel_->Update();
    }

    // 巨大化モデル（合体モード用：さらに高密度）
    giantModelData_ = SlimeMesh::GenerateSphere(80, 40, 1.0f);
    giantTextureIndex_ = normalTextureIndex_;
    giantModelData_.material.textureIndex = giantTextureIndex_;

    if (giantModel_) {
        giantModel_->Initialize(object3dCom_, giantModelData_);
        giantModel_->SetCamera(camera_);
        giantModel_->SetTranslate(position_);
        giantModel_->SetScale(scale_);
        giantModel_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        giantModel_->SetEnableLighting(true);
        giantModel_->Update();
    }


    // スライムパラメータの初期設定（ゼリー感のあるぷるぷるスライム）
    slimeParams_.baseColor = { 0.2f, 0.85f, 1.0f, 0.9f }; // 水色スライム
    slimeParams_.wobbleStrength = 0.0f;
    slimeParams_.wobbleFrequency = 5.0f;
    slimeParams_.fresnelPower = 2.5f;
    slimeParams_.envReflection = 0.5f;
    slimeParams_.innerGlow = 0.5f;
    slimeParams_.specularShininess = 64.0f;

    // プレイヤーの当たり判定（MeshCollider: スライムモデルの精密メッシュ判定）
    meshCollider_ = std::make_unique<MeshCollider>(normalModel_.get(), CollisionAttribute::Player);
    meshCollider_->SetOnCollision([this](const CollisionInfo& info) {
        OnCollision(info);
    });
    CollisionManager::GetInstance()->RegisterCollider(meshCollider_.get());
}

void PikminPlayer::OnCollision(const CollisionInfo& info) {
    if (info.other && info.other->GetAttribute() == CollisionAttribute::Obstacle) {
        // 同一フレーム内の多重衝突および連続ヒットを防止（中心部での振動・多重加速を防止）
        if (obstacleCooldown_ > 0.0f) return;

        // 障害物（プロペラなど）の基準位置（回転中心）を正確に取得
        Vector3 obstacleBasePos = info.other->GetWorldPosition() - info.other->GetPositionOffset();

        // プロペラ中心からプレイヤーへ向かう動径ベクトル（水平面）
        Vector3 radial = { position_.x - obstacleBasePos.x, 0.0f, position_.z - obstacleBasePos.z };
        float rLen = std::sqrt(radial.x * radial.x + radial.z * radial.z);

        Vector3 escapeDir{ 0.0f, 0.0f, 0.0f };

        if (rLen > 0.2f) {
            // 外向き動径方向へ弾き出す
            escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
        } else {
            // プロペラ回転中心へのド直撃（特異点）の場合:
            // 進行方向の逆向き（跳ね返り反射ベクトル）を優先採用
            Vector3 incoming = { -velocity_.x, 0.0f, -velocity_.z };
            float incSpeed = std::sqrt(incoming.x * incoming.x + incoming.z * incoming.z);
            if (incSpeed > 0.1f) {
                escapeDir = { incoming.x / incSpeed, 0.0f, incoming.z / incSpeed };
            } else if (rLen > 1e-4f) {
                escapeDir = { radial.x / rLen, 0.0f, radial.z / rLen };
            } else {
                escapeDir = { 0.0f, 0.0f, -1.0f };
            }
        }

        // 1. めり込みの強制解消（エンジン側の押し出しに加えて、外向きへ安全マージンを補正）
        if (info.depth > 0.01f) {
            position_.x += escapeDir.x * (info.depth * 0.5f);
            position_.z += escapeDir.z * (info.depth * 0.5f);
        }

        // 2. 外向き脱出初速の付与（プロペラから勢いよく弾き出される慣性）
        float launchSpeed = isMerged_ ? 6.5f : 8.5f;
        velocity_.x = escapeDir.x * launchSpeed;
        velocity_.z = escapeDir.z * launchSpeed;

        // クールダウン設定（0.12秒間、重複ヒットを無効化）
        obstacleCooldown_ = 0.12f;

        // 衝突時のスライム変形（衝撃波紋とスクワッシュ）
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, 0.4f);
        slimeParams_.squashStretch = { 0.25f, -0.2f, 0.25f };
    }

    // 高速衝突時の衝撃波紋
    float impactSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
    if (impactSpeed > 1.5f) {
        float strength = (std::min)(0.35f, impactSpeed * 0.03f);
        slimeParams_.impulseStrength = (std::max)(slimeParams_.impulseStrength, strength);
    }
}

float PikminPlayer::CalculateScaleBySize(int size) const {
    if (size <= 1) return 0.40f;
    // 1から10にかけて滑らかに0.40fから約1.85fへ拡大
    return 0.40f + 0.11f * (size - 1) + 0.05f * std::pow(static_cast<float>(size - 1), 1.25f);
}

void PikminPlayer::SetSize(int s) {
    size_ = (std::max)(1, s);
    isMerged_ = (size_ > 1);
    lastAbsorbedCount_ = size_ - 1;
    currentMergedScale_ = CalculateScaleBySize(size_);
    scale_ = { currentMergedScale_, currentMergedScale_, currentMergedScale_ };
    slimeParams_.baseColor = SlimePhysics::GetColorBySize(size_);
    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
    if (size_ >= 3 && giantModel_) {
        giantModel_->SetScale(scale_);
        giantModel_->Update();
    } else if (normalModel_) {
        normalModel_->SetScale(scale_);
        normalModel_->Update();
    }
}

float PikminPlayer::CalculateMergedScale(int minionCount) const {
    return CalculateScaleBySize(minionCount + 1);
}

void PikminPlayer::SetPosition(const Vector3& pos) {
    position_ = pos;
    if (normalModel_) {
        normalModel_->SetTranslate(pos);
        normalModel_->Update();
    }
    if (giantModel_) {
        giantModel_->SetTranslate(pos);
        giantModel_->Update();
    }
}

Vector3 PikminPlayer::GetForwardVector() const {
    return { std::sin(rotation_.y), 0.0f, std::cos(rotation_.y) };
}

void PikminPlayer::ToggleMerge() {
    SetMerged(!isMerged_);
}

void PikminPlayer::SetMerged(bool merged) {
    isMerged_ = merged;
    mergeScaleAnimation_ = 0.0f;
    if (!merged) {
        lastAbsorbedCount_ = 0;
        size_ = 1;
        currentMergedScale_ = 0.4f;
    }

    // 合体/分裂時に衝撃波紋を発生させる
    slimeParams_.impulseStrength = 0.3f;

    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
}

void PikminPlayer::Update(float deltaTime, KeyInput* keyInput, MinionManager* minionManager, MouseInput* mouseInput, AimGuide* aimGuide, const Vector2& stageTilt) {
    throwCooldownTimer_ -= deltaTime;
    if (obstacleCooldown_ > 0.0f) {
        obstacleCooldown_ -= deltaTime;
    }
    mergeScaleAnimation_ = (std::min)(1.0f, mergeScaleAnimation_ + deltaTime * 4.0f);
    totalTime_ += deltaTime;

    // 衝撃波紋の減衰
    slimeParams_.impulseStrength *= (1.0f - deltaTime * 5.0f);
    if (slimeParams_.impulseStrength < 0.001f) slimeParams_.impulseStrength = 0.0f;

    if (keyInput) {
        if (keyInput->TriggerKey(DIK_E)) {
            // ロコロコ方式分裂: 合体中ならパァンと全員飛び散って小ロコロコに分裂！
            if (minionManager && (size_ > 1 || minionManager->GetAbsorbedCount() > 0)) {
                minionManager->TriggerSplit(position_, size_);
                SetSize(1);
                slimeParams_.impulseStrength = 0.55f;
                slimeParams_.squashStretch = { 0.35f, -0.25f, 0.35f };
            }
        }

        // ジャンプ入力 (SPACEキー: 接地時のみジャンプ可能)
        if (keyInput->TriggerKey(DIK_SPACE)) {
            if (isGrounded_) {
                velocity_.y = 13.0f; // ジャンプ初速
                isGrounded_ = false;
                // ジャンプ時の縦伸び（スライムストレッチ）
                slimeParams_.squashStretch = { -0.15f, 0.30f, -0.15f };
                slimeParams_.impulseStrength = 0.28f;
            }
        }
    }

    // --- 水平移動（ティルト加速度と慣性） ---
    // stageTilt.x: ピッチ（手前/奥）、stageTilt.y: ロール（左/右）
    float accelScale = isMerged_ ? (tiltAccel_ * 1.25f) : tiltAccel_;
    float accelX = std::sin(stageTilt.y) * accelScale;
    float accelZ = std::sin(stageTilt.x) * accelScale;

    velocity_.x += accelX * deltaTime;
    velocity_.z += accelZ * deltaTime;

    // 摩擦減速（空中では摩擦を大幅低減して滑らかに慣性飛行）
    float currentFriction = isGrounded_ ? SlimePhysics::GetFriction() : (SlimePhysics::GetFriction() * 0.15f);
    float decay = 1.0f - (std::min)(1.0f, currentFriction * deltaTime);
    velocity_.x *= decay;
    velocity_.z *= decay;

    // 水平位置の更新
    position_.x += velocity_.x * deltaTime;
    position_.z += velocity_.z * deltaTime;

    // --- 大きさ（1-10）と色（小:青, 中:黄, 大:赤）の管理 ---
    int absorbedCount = minionManager ? minionManager->GetAbsorbedCount() : 0;
    int newSize = 1 + absorbedCount;
    isMerged_ = (newSize > 1);

    if (newSize > size_) {
        // 新たなくっつきが発生！（1+1=2、2+1=3...）
        bool tierChanged = (size_ <= 2 && newSize >= 3) || (size_ <= 7 && newSize >= 8);
        slimeParams_.impulseStrength = tierChanged ? 0.40f : (std::min)(0.5f, slimeParams_.impulseStrength + 0.22f);
    }
    size_ = newSize;

    // 大きさに応じた色設定（小 1-2: 青, 中 3-7: 黄色, 大 8-10以上: 赤）
    slimeParams_.baseColor = SlimePhysics::GetColorBySize(size_);

    float targetScale = CalculateScaleBySize(size_);
    currentMergedScale_ += (targetScale - currentMergedScale_) * (std::min)(1.0f, deltaTime * 10.0f);
    float currentScale = currentMergedScale_;
    scale_ = { currentScale, currentScale, currentScale };

    // 地形メッシュの壁・垂直面との衝突押し出し（角や壁へのめり込み・テレポートを防止）
    float colRadius = currentScale * 0.45f;
    SlimePhysics::ResolveWallCollision(position_, velocity_, colRadius);

    // --- 垂直重力とリアルタイム地形・落下物理 ---
    const float kGravity = -32.0f; // 重力加速度
    bool hasGround = false;
    Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
    float baseOffset = currentScale * 0.73f;
    float targetGroundedY = SlimePhysics::CalculateGroundedCenterYEx(
        position_.x, position_.z, position_.y, stageTilt, baseOffset, &hasGround, &groundNormal, { position_.x, position_.z }, isGrounded_);

    if (!isGrounded_) {
        // 空中状態: 純粋な鉛直重力で落下
        velocity_.y += kGravity * deltaTime;
        if (velocity_.y < -45.0f) velocity_.y = -45.0f; // 終端落下速度制限

        position_.y += velocity_.y * deltaTime;

        // 空中での姿勢: 進行方向を向く
        rotation_.x = 0.0f;
        rotation_.z = 0.0f;
        float horizSpeed = std::sqrt(velocity_.x * velocity_.x + velocity_.z * velocity_.z);
        if (horizSpeed > 0.3f) {
            rotation_.y = std::atan2(velocity_.x, velocity_.z);
        }

        // 頭上の天井・高層足場との衝突チェック（ジャンプ上昇中に上の床を突き抜けるのを防止）
        if (velocity_.y > 0.0f) {
            float topGroundY = SlimePhysics::CalculateGroundHeight(position_.x, position_.z, stageTilt, { position_.x, position_.z });
            // もし最上階の床がプレイヤーの頭上直近にあり、頭が衝突した場合
            if (topGroundY > position_.y && position_.y + baseOffset >= topGroundY) {
                position_.y = topGroundY - baseOffset;
                velocity_.y = -1.0f; // 頭をぶつけて下へ跳ね返る
                slimeParams_.squashStretch = { 0.15f, -0.15f, 0.15f };
            }
        }

        // 着地判定（足元に地面があり、落下中であり、地面より下に到達した場合）
        if (hasGround && velocity_.y <= 0.0f && position_.y <= targetGroundedY) {
            position_.y = targetGroundedY;
            float impactSpeed = -velocity_.y;
            isGrounded_ = true;

            // スライム特有の弾性着地バウンド
            if (impactSpeed > 7.0f) {
                velocity_.y = impactSpeed * 0.22f; // 小バウンド
                isGrounded_ = false;
            } else {
                velocity_.y = 0.0f;
            }

            // 着地時の弾力スクワッシュ（ぷるんと潰れて復元）
            float squashAmount = std::clamp(impactSpeed * 0.02f, 0.08f, 0.35f);
            slimeParams_.squashStretch = { squashAmount * 0.5f, -squashAmount, squashAmount * 0.5f };
            slimeParams_.impulseStrength = std::clamp(impactSpeed * 0.04f, 0.15f, 0.60f);
        }
    } else {
        // 接地状態: 地面スロープへの密着追従
        if (!hasGround) {
            // 足元に地面がなくなった（島の外へ飛び出した！） -> 空中落下へ！
            isGrounded_ = false;
        } else {
            // --- 急斜面スロープスライディング＆登坂制限 ---
            // 急斜面（groundNormal.y < 0.82f, 傾斜約35度以上）では下り坂方向へ重力滑走を発生させ、上り登坂を制限
            float slopeHorizLen = std::sqrt(groundNormal.x * groundNormal.x + groundNormal.z * groundNormal.z);
            if (groundNormal.y < 0.82f && slopeHorizLen > 0.01f) {
                Vector2 slopeDown = { groundNormal.x / slopeHorizLen, groundNormal.z / slopeHorizLen };
                // 傾斜が急なほど強く下へ滑り落ちる力
                float slideStrength = (1.0f - groundNormal.y) * 22.0f;
                velocity_.x += slopeDown.x * slideStrength * deltaTime;
                velocity_.z += slopeDown.y * slideStrength * deltaTime;

                // 上り坂方向（slopeDownと逆方向）への移動速度を抑制・相殺（急斜面を登れないようにする）
                float velDotDown = velocity_.x * slopeDown.x + velocity_.z * slopeDown.y;
                if (velDotDown < 0.0f) {
                    float cancelFactor = std::clamp((0.82f - groundNormal.y) / 0.18f, 0.0f, 1.0f);
                    velocity_.x -= slopeDown.x * (velDotDown * cancelFactor);
                    velocity_.z -= slopeDown.y * (velDotDown * cancelFactor);
                }
            }

            float dy = targetGroundedY - position_.y;
            if (dy < -4.0f) {
                // 大きな段差・崖から飛び出した -> 空中落下へ！
                isGrounded_ = false;
            } else {
                // 地面の上に常に乗る（傾斜で床が持ち上がっても絶対に埋まらない！）
                if (dy > 0.0f) {
                    // 床が下から押し上げてくる場合: 瞬時に接地高さへ（傾斜時の埋まり・すり抜けを物理的に100%防止）
                    position_.y = targetGroundedY;
                    velocity_.y = 0.0f;
                } else {
                    // 床が下がった場合: 滑らかに密着追従
                    position_.y += dy * (std::min)(1.0f, deltaTime * 40.0f);
                    velocity_.y = 0.0f;
                }
            }
        }

        // 接地中の姿勢（局所地形法線に正しく沿って密着）
        float targetRotX = std::atan2(groundNormal.z, groundNormal.y);
        float targetRotZ = -std::atan2(groundNormal.x, groundNormal.y);
        rotation_.x += (targetRotX - rotation_.x) * (std::min)(1.0f, deltaTime * 20.0f);
        rotation_.y = 0.0f;
        rotation_.z += (targetRotZ - rotation_.z) * (std::min)(1.0f, deltaTime * 20.0f);
    }

    // 奈落への落下防止セーフティ（万が一島の外へ真っ逆さまに落ちた場合は安全に復帰）
    if (position_.y < -120.0f) {
        position_ = { 0.0f, 4.0f, 0.0f };
        velocity_ = { 0.0f, 0.0f, 0.0f };
        isGrounded_ = false;
    }

    // --- 液体スライムの動的変形（空中/接地状態を正しく反映） ---
    SlimePhysics::DeformInput deformInput;
    deformInput.velocity = velocity_;
    deformInput.prevVelocity = prevVelocity_;
    deformInput.stageTilt = stageTilt;
    deformInput.deltaTime = deltaTime;
    deformInput.isGrounded = isGrounded_;
    deformInput.isMerged = isMerged_;
    deformInput.massScale = scale_.x;
    SlimePhysics::UpdateDeformation(slimeParams_, deformInput);
    prevVelocity_ = velocity_;

    slimeParams_.time = totalTime_;
    // モデルTransformの更新（通常・巨大どちらも現在の正確なscale_で更新）
    if (normalModel_) {
        normalModel_->SetTranslate(position_);
        normalModel_->SetRotate(rotation_);
        normalModel_->SetScale(scale_);
        normalModel_->Update();
    }
    if (giantModel_) {
        giantModel_->SetTranslate(position_);
        giantModel_->SetRotate(rotation_);
        giantModel_->SetScale(scale_);
        giantModel_->Update();
    }
    if (meshCollider_) {
        meshCollider_->SetWorldPosition(position_);
        meshCollider_->Update();
    }
}


void PikminPlayer::DrawSlime(Object3d* object, const Object3d::ModelData& modelData,
                              const RenderContext& ctx, uint32_t textureIndex)
{
    if (!object || !object3dCom_ || !ctx.commandList) return;

    DirectXCom* dx = object3dCom_->GetDirectXCom();
    if (!dx) return;

    auto* cbAllocator = dx->GetCBAllocator();
    if (!cbAllocator) return;

    // スライム専用ルートシグネチャとPSOを取得
    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    auto slimePSO = PipelineStateManager::GetInstance()->GetPipelineState("Slime_Normal");
    if (!rootSig || !slimePSO) {
        // フォールバック：通常のObject3D描画
        RenderContext localCtx = ctx;
        if (textureIndex != TextureManager::kInvalidTextureIndex) {
            localCtx.textureHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex);
        }
        object3dCom_->Draw(object, localCtx, modelData, true);
        return;
    }

    // 定数バッファの準備（Object3d内部でマテリアル・変換行列をアロケート）
    object->PrepareConstantBuffers(dx);

    // スライム定数バッファをGPUに書き込み
    auto slimeAlloc = cbAllocator->Allocate(sizeof(SlimeParamsCPU));
    std::memcpy(slimeAlloc.cpuAddress, &slimeParams_, sizeof(SlimeParamsCPU));

    // ルートシグネチャとPSOを設定
    ctx.commandList->SetGraphicsRootSignature(rootSig.Get());
    ctx.commandList->SetPipelineState(slimePSO.Get());

    // 0: Material (b0 Pixel)
    ctx.commandList->SetGraphicsRootConstantBufferView(0, object->GetMaterialGPUAddress());

    // 1: TransformationMatrix (b0 Vertex)
    ctx.commandList->SetGraphicsRootConstantBufferView(1, object->GetTransformationMatrixGPUAddress());

    // 2: Main Texture (t3 All)
    D3D12_GPU_DESCRIPTOR_HANDLE texHandle{};
    if (textureIndex != TextureManager::kInvalidTextureIndex) {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(textureIndex);
    } else {
        texHandle = TextureManager::GetInstance()->GetSrvHandleGPU(
            TextureManager::GetInstance()->GetTextureIndexByFilePath("Resources/CG4/human/white.png"));
    }
    if (texHandle.ptr != 0) {
        ctx.commandList->SetGraphicsRootDescriptorTable(2, texHandle);
    }

    // 3: SlimeParams (b1 All: Vertex & Pixel)
    ctx.commandList->SetGraphicsRootConstantBufferView(3, slimeAlloc.gpuAddress);

    // 4: DirectionalLight (b2 Pixel)
    if (ctx.light && ctx.light->GetDirectionalLightResource()) {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, ctx.light->GetDirectionalLightResource()->GetGPUVirtualAddress());
    } else {
        ctx.commandList->SetGraphicsRootConstantBufferView(4, object->GetDirectionalLightGPUAddress());
    }

    // 5: Camera (b3 Pixel)
    if (ctx.camera && ctx.camera->GetCameraGpuAddress() != 0) {
        ctx.commandList->SetGraphicsRootConstantBufferView(5, ctx.camera->GetCameraGpuAddress());
    }

    // 6: Cube Environment Map (t4 Pixel)
    uint32_t skyboxIndex = SceneManager::GetInstance()->GetSkyboxTextureIndex();
    D3D12_GPU_DESCRIPTOR_HANDLE skyboxHandle{};
    if (skyboxIndex != TextureManager::kInvalidTextureIndex) {
        skyboxHandle = TextureManager::GetInstance()->GetSrvHandleGPU(skyboxIndex);
    } else {
        skyboxHandle = texHandle;
    }
    if (skyboxHandle.ptr != 0) {
        ctx.commandList->SetGraphicsRootDescriptorTable(6, skyboxHandle);
    }

    // 頂点バッファとインデックスバッファを設定して描画
    auto vbv = object->GetVertexBufferView();
    ctx.commandList->IASetVertexBuffers(0, 1, &vbv);
    ctx.commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if (object->HasIndexBuffer()) {
        auto ibv = object->GetIndexBufferView();
        ctx.commandList->IASetIndexBuffer(&ibv);
        ctx.commandList->DrawIndexedInstanced(static_cast<UINT>(modelData.indices.size()), 1, 0, 0, 0);
    } else {
        ctx.commandList->DrawInstanced(static_cast<UINT>(modelData.vertices.size()), 1, 0, 0);
    }
}

void PikminPlayer::Draw(const RenderContext& ctx) {
    if (!object3dCom_) return;

    if (size_ >= 3 && giantModel_) {
        DrawSlime(giantModel_.get(), giantModelData_, ctx, giantTextureIndex_);
    } else if (normalModel_) {
        DrawSlime(normalModel_.get(), normalModelData_, ctx, normalTextureIndex_);
    }
}


void PikminPlayer::DrawDebug(Camera* camera) {
#ifdef _DEBUG
    if (camera) {
        auto shape = SlimeCollision::BuildMultiSphere(position_, scale_, slimeParams_.squashStretch, rotation_);
        uint32_t color = isMerged_ ? 0xFF00D7FF : 0xFFFFFF00; // 金色 or 水色 (ABGR/ImGui)
        SlimeCollision::DrawDebugMultiSphere(shape, camera, color);
    }
#endif
}
