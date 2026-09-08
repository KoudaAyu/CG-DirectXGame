#include "SlimeManager.h"
#include "Application/GameObject/SlimeCollision.h"
#include "Application/GameObject/SlimePhysics.h"
#include "Baziru3_Engine/Core/Base/KeyInput.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3dCom.h"
#include "DirectXCom.h"
#include "Baziru3_Engine/Core/Base/Pipeline/PipelineStateManager.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

void SlimeManager::Initialize(Object3dCom* object3dCom, Camera* camera) {
    object3dCom_ = object3dCom;
    camera_ = camera;
    slimes_.clear();

    CreateXRayPipeline();
}

void SlimeManager::CreateXRayPipeline() {
    if (!object3dCom_) return;
    DirectXCom* dxCommon = object3dCom_->GetDirectXCom();
    if (!dxCommon) return;

    auto rootSig = PipelineStateManager::GetInstance()->GetRootSignature("Slime");
    if (!rootSig) {
        OutputDebugStringA("SlimeManager: Slime root signature not found.\n");
        return;
    }

    // 遮蔽スライム専用ピクセルシェーダーと頂点シェーダーのコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon->CompileShader(
        L"Resources/shaders/Slime.VS.hlsl", L"vs_6_0",
        dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);

    Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon->CompileShader(
        L"Resources/shaders/SlimeXRay.PS.hlsl", L"ps_6_0",
        dxCommon->GetDxcUtils().Get(), dxCommon->GetDxcCompiler(), dxCommon->GetIncludeHandler(), std::cout);

    if (!vsBlob || !psBlob) {
        OutputDebugStringA("SlimeManager: Failed to compile shaders for SlimeXRay PSO.\n");
        return;
    }

    // インプットレイアウト
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[3]{};
    inputElementDescs[0].SemanticName = "POSITION";
    inputElementDescs[0].SemanticIndex = 0;
    inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[1].SemanticName = "TEXCOORD";
    inputElementDescs[1].SemanticIndex = 0;
    inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    inputElementDescs[2].SemanticName = "NORMAL";
    inputElementDescs[2].SemanticIndex = 0;
    inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSig.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // 半透明αブレンド
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

    // スライム表面ポリゴンのみ描画（裏面ポリゴンが自身に合格するのを防ぐ）
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 遮蔽時判定: 現在の深度バッファ値より奥にあるピクセルのみ描画 (GREATER)
    // 深度書き込みは行わない (ZERO)
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_GREATER;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr = dxCommon->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&xRayPSO_));
    if (FAILED(hr)) {
        OutputDebugStringA("SlimeManager: Failed to create SlimeXRay PSO.\n");
    } else {
        OutputDebugStringA("SlimeManager: Created SlimeXRay PSO successfully.\n");
    }
}

Slime* SlimeManager::SpawnSlime(const Vector3& pos, int size) {
    if (static_cast<int>(slimes_.size()) >= kMaxSlimes) {
        return nullptr; // 最大スライム数超過によるリソース枯渇を防止
    }
    auto slime = std::make_unique<Slime>();
    slime->Initialize(object3dCom_, camera_, pos, size);
    slimes_.push_back(std::move(slime));
    return slimes_.back().get();
}

void SlimeManager::SpawnSlimes(const Vector3& basePos, int count, int sizePerSlime) {
    for (int i = 0; i < count; ++i) {
        float offsetX = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        float offsetZ = ((std::rand() % 100) / 100.0f - 0.5f) * 2.0f;
        Vector3 pos = { basePos.x + offsetX, basePos.y + 0.2f, basePos.z + offsetZ };
        SpawnSlime(pos, sizePerSlime);
    }
}

void SlimeManager::Clear() {
    slimes_.clear();
}

void SlimeManager::TriggerStageBounce(const Vector2& stageTilt, const Vector2& pivot, float bouncePower) {
    for (auto& slime : slimes_) {
        if (!slime || !slime->IsActive()) continue;

        Vector3 pos = slime->GetPosition();
        bool hasGround = false;
        Vector3 groundNormal{ 0.0f, 1.0f, 0.0f };
        float groundY = SlimePhysics::CalculateGroundedCenterYEx(
            pos.x, pos.z, pos.y, stageTilt, slime->GetRadius(), &hasGround, &groundNormal, pivot, slime->IsGrounded());

        // 接地しているか、床面至近（0.35m以内）のスライムのみがステージの突き上げを受ける
        if (slime->IsGrounded() || (hasGround && (pos.y - groundY) < 0.35f && (pos.y - groundY) >= -0.1f)) {
            slime->BounceFromStage(groundNormal, bouncePower);
        }
    }
}

void SlimeManager::TriggerJump() {
    Vector3 center;
    float spread;
    GetGroupCenterAndSpread(center, spread);
    TriggerStageBounce({ 0.0f, 0.0f }, { center.x, center.z }, 13.5f);
}

void SlimeManager::TriggerSplit() {
    // 分裂で新しく生まれるスライムを一旦溜めておき、ループ後にまとめて追加する
    std::vector<std::unique_ptr<Slime>> newSlimes;

    for (auto& slime : slimes_) {
        if (!slime || !slime->IsActive()) continue;

        int currentSize = slime->GetSize();

        if (currentSize > 1) {
            Vector3 sPos = slime->GetPosition();
            int desiredCount = currentSize - 1; // 親以外の分裂数
            int availableSlots = kMaxSlimes - static_cast<int>(slimes_.size() + newSlimes.size());
            int spawnCount = (std::min)(desiredCount, (std::max)(0, availableSlots));

            // 親スライム自身を、分裂しきれなかった余剰サイズがあればそのサイズに、全部分裂できた場合はサイズ1に戻す
            int parentNewSize = 1 + (desiredCount - spawnCount);
            slime->SetSize(parentNewSize);
            slime->SetMergeCooldown(0.40f);
            Vector3 jumpVel = { 0.0f, splitUpPower_ * 0.6f, 0.0f };
            slime->Launch(jumpVel);

            if (spawnCount > 0) {
                // spawnCount 個の新しいサイズ1スライムを親の位置から放射状に発射
                float angleStep = (2.0f * kPi) / static_cast<float>(spawnCount);
                for (int i = 0; i < spawnCount; ++i) {
                    auto newSlime = std::make_unique<Slime>();
                    newSlime->Initialize(object3dCom_, camera_, sPos, 1);
                    newSlime->SetMergeCooldown(0.40f);

                    float angle = angleStep * i + ((std::rand() % 100) / 100.0f - 0.5f) * 0.35f;
                    float popSpeed = splitPopPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitPopPower_ * 0.25f);
                    float upSpeed = splitUpPower_ + ((std::rand() % 100) / 100.0f - 0.5f) * (splitUpPower_ * 0.25f);

                    Vector3 launchVel = {
                        std::sin(angle) * popSpeed,
                        upSpeed,
                        std::cos(angle) * popSpeed
                    };
                    newSlime->Launch(launchVel);

                    newSlimes.push_back(std::move(newSlime));
                }
            }
        } else {
            // もともとサイズ1の単独スライムは、周囲の分裂の波紋に合わせてその場で小さくホップ
            slime->SetMergeCooldown(0.40f);
            Vector3 jumpVel = { 0.0f, splitUpPower_ * 0.4f, 0.0f };
            slime->Launch(jumpVel);
        }
    }

    // 新しく生まれたスライムをリストに追加
    for (auto& ns : newSlimes) {
        slimes_.push_back(std::move(ns));
    }
}

void SlimeManager::CheckAndResolveMerge(const Vector2& stageTilt, const Vector2& pivot) {
    size_t count = slimes_.size();
    bool anyMerged = false;
    for (size_t i = 0; i < count; ++i) {
        if (!slimes_[i] || !slimes_[i]->CanMerge()) continue;

        for (size_t j = i + 1; j < count; ++j) {
            if (!slimes_[j] || !slimes_[j]->CanMerge()) continue;

            Vector3 posA = slimes_[i]->GetPosition();
            Vector3 posB = slimes_[j]->GetPosition();
            float dx = posA.x - posB.x;
            float dy = posA.y - posB.y;
            float dz = posA.z - posB.z;
            float distSq = dx * dx + dz * dz;

            float mergeDist = (std::max)(mergeThreshold_, slimes_[i]->GetRadius() + slimes_[j]->GetRadius() + 0.50f);
            if (distSq <= mergeDist * mergeDist && std::abs(dy) <= mergeDist) {
                // スライムiにスライムjが合体！
                int combinedSize = slimes_[i]->GetSize() + slimes_[j]->GetSize();

                // 合体位置: 大きい方に寄せる（半々だとめり込みやすいため）
                float totalSize = static_cast<float>(slimes_[i]->GetSize() + slimes_[j]->GetSize());
                float weightA = static_cast<float>(slimes_[i]->GetSize()) / totalSize;
                float weightB = static_cast<float>(slimes_[j]->GetSize()) / totalSize;
                Vector3 mergeCenter = {
                    posA.x * weightA + posB.x * weightB,
                    (std::max)(posA.y, posB.y), // Y は高い方を採用（めり込み防止）
                    posA.z * weightA + posB.z * weightB
                };

                // slimes_[j] を非アクティブに
                slimes_[j]->SetActive(false);
                slimes_[j]->SetSize(1);

                // slimes_[i] にサイズを集約
                slimes_[i]->SetSize(combinedSize);

                // 合体後の地面補正: 新しいスケールでの接地高さを最上空から厳密に算出して沈み込み・奈落落下を完全防止
                float groundOffset = slimes_[i]->GetScale().x * 0.75f;
                bool hasGround = false;
                float calcGroundY = SlimePhysics::CalculateGroundedCenterYEx(
                    mergeCenter.x, mergeCenter.z, SlimePhysics::kIgnoreCurrentY,
                    stageTilt, groundOffset, &hasGround, pivot, false);
                if (hasGround) {
                    mergeCenter.y = (std::max)(mergeCenter.y, calcGroundY);
                } else {
                    mergeCenter.y += groundOffset;
                }
                slimes_[i]->SetPosition(mergeCenter);

                slimes_[i]->GetSlimeParams().impulseStrength = 0.50f; // ポヨン！と合体弾性
                anyMerged = true;
            }
        }
    }

    // 非アクティブになったスライムを完全に破棄・解放（ゾンビオブジェクト累積とリソースリークの完全防止）
    if (anyMerged) {
        slimes_.erase(
            std::remove_if(slimes_.begin(), slimes_.end(),
                [](const std::unique_ptr<Slime>& s) { return !s || !s->IsActive(); }),
            slimes_.end());
    }
}

void SlimeManager::ResolveSeparation(const Vector3& rotation, const Vector2& stageTilt, const Vector2& pivot) {
    Matrix4x4 rotMat = Multiply(MakeRotateXMatrix(rotation.x),
                                Multiply(MakeRotateYMatrix(rotation.y), MakeRotateZMatrix(rotation.z)));
    Vector3 stageNormal = { rotMat.m[1][0], rotMat.m[1][1], rotMat.m[1][2] };

    size_t count = slimes_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!slimes_[i] || !slimes_[i]->IsActive()) continue;

        for (size_t j = i + 1; j < count; ++j) {
            if (!slimes_[j] || !slimes_[j]->IsActive()) continue;

            Vector3 posA = slimes_[i]->GetPosition();
            Vector3 posB = slimes_[j]->GetPosition();
            Vector3 scaleA = slimes_[i]->GetScale();
            Vector3 scaleB = slimes_[j]->GetScale();
            const Vector3& squashA = slimes_[i]->GetSlimeParams().squashStretch;
            const Vector3& squashB = slimes_[j]->GetSlimeParams().squashStretch;

            float impulse = 0.0f;
            if (SlimeCollision::ResolveCollision(posA, scaleA, squashA, 0.5f,
                                                 posB, scaleB, squashB, 0.5f,
                                                 impulse, rotation, rotation, stageNormal)) {
                bool hasGroundA = false, hasGroundB = false;
                float gyA = SlimePhysics::CalculateGroundedCenterYEx(posA.x, posA.z, posA.y, stageTilt, 0.22f, &hasGroundA, pivot, slimes_[i]->IsGrounded());
                float gyB = SlimePhysics::CalculateGroundedCenterYEx(posB.x, posB.z, posB.y, stageTilt, 0.22f, &hasGroundB, pivot, slimes_[j]->IsGrounded());
                if (hasGroundA && slimes_[i]->GetState() == SlimeState::Rolling) posA.y = gyA;
                if (hasGroundB && slimes_[j]->GetState() == SlimeState::Rolling) posB.y = gyB;

                slimes_[i]->SetPosition(posA);
                slimes_[j]->SetPosition(posB);

                Vector3 diff = posB - posA;
                float diffLenSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (diffLenSq > 1e-6f) {
                    Vector3 norm = diff * (1.0f / std::sqrt(diffLenSq));
                    Vector3 velA = slimes_[i]->GetVelocity();
                    Vector3 velB = slimes_[j]->GetVelocity();
                    float closingSpeed = (velB.x - velA.x) * norm.x + (velB.y - velA.y) * norm.y + (velB.z - velA.z) * norm.z;
                    if (closingSpeed < 0.0f) {
                        Vector3 relImpulse = norm * (closingSpeed * 0.5f);
                        slimes_[i]->SetVelocity(velA + relImpulse);
                        slimes_[j]->SetVelocity(velB - relImpulse);
                    }
                }

                if (impulse > 0.05f) {
                    slimes_[i]->GetSlimeParams().impulseStrength = (std::max)(slimes_[i]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                    slimes_[j]->GetSlimeParams().impulseStrength = (std::max)(slimes_[j]->GetSlimeParams().impulseStrength, impulse * 0.4f);
                }
            }
        }
    }
}

void SlimeManager::Update(float deltaTime, KeyInput* keyInput, const Vector2& stageTilt) {
    if (keyInput) {
        // Fキー: 合体リクエスト
        if (keyInput->TriggerKey(DIK_F)) {
            RequestMerge();
        }
        // Eキー: 全員分裂
        if (keyInput->TriggerKey(DIK_E)) {
            TriggerSplit();
        }
    }

    // 重心ピボット
    Vector3 center;
    float spread;
    GetGroupCenterAndSpread(center, spread);
    Vector2 pivot = { center.x, center.z };

    // 合体判定
    if (mergeRequested_) {
        CheckAndResolveMerge(stageTilt, pivot);
        mergeRequested_ = false;
        GetGroupCenterAndSpread(center, spread);
        pivot = { center.x, center.z };
    }

    // 各スライムの物理更新
    for (auto& slime : slimes_) {
        if (slime) {
            slime->Update(deltaTime, stageTilt, pivot);
        }
    }

    // 非アクティブ（奈落落下など）になったスライムをリストから除外
    slimes_.erase(
        std::remove_if(slimes_.begin(), slimes_.end(),
            [](const std::unique_ptr<Slime>& s) { return !s || !s->IsActive(); }),
        slimes_.end());

    // スライム同士の衝突分離（2パス）
    Vector3 rot = { stageTilt.x, 0.0f, -stageTilt.y };
    for (int iter = 0; iter < 2; ++iter) {
        ResolveSeparation(rot, stageTilt, pivot);
    }
}

void SlimeManager::GetGroupCenterAndSpread(Vector3& outCenter, float& outSpread) const {
    Vector3 sumPos = { 0.0f, 0.0f, 0.0f };
    int totalWeight = 0;

    for (const auto& slime : slimes_) {
        if (slime && slime->IsActive()) {
            // レベル3のスライムは同座標にレベル1スライムが3つあるのと同じ重み（質量加重平均）
            int weight = (std::max)(1, slime->GetSize());
            Vector3 pos = slime->GetPosition();
            sumPos += pos * static_cast<float>(weight);
            totalWeight += weight;
        }
    }

    if (totalWeight == 0) {
        outCenter = { 0.0f, 0.0f, 0.0f };
        outSpread = 1.0f;
        return;
    }

    outCenter = { sumPos.x / totalWeight, sumPos.y / totalWeight, sumPos.z / totalWeight };

    float maxDistSq = 0.0f;
    float sumDist = 0.0f;
    int activeSlimeCount = 0;
    for (const auto& slime : slimes_) {
        if (slime && slime->IsActive()) {
            Vector3 diff = slime->GetPosition() - outCenter;
            float distSq = diff.x * diff.x + diff.z * diff.z;
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
            }
            sumDist += std::sqrt(distSq);
            activeSlimeCount++;
        }
    }

    float maxDist = std::sqrt(maxDistSq);
    float avgDist = (activeSlimeCount > 0) ? (sumDist / static_cast<float>(activeSlimeCount)) : 0.0f;
    // 単一の外れ値に引っ張られすぎないよう、平均広がりと最大広がりをブレンド（群れのまとまり重視）
    float blendedSpread = avgDist * 1.4f * 0.65f + maxDist * 0.35f;
    outSpread = (std::min)(10.0f, (std::max)(0.5f, blendedSpread));
}

int SlimeManager::GetActiveCount() const {
    int count = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) count++;
    }
    return count;
}

int SlimeManager::GetTotalCount() const {
    return static_cast<int>(slimes_.size());
}

int SlimeManager::GetMaxSlimeSize() const {
    int maxS = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) {
            maxS = (std::max)(maxS, s->GetSize());
        }
    }
    return maxS;
}

int SlimeManager::GetTotalSize() const {
    int total = 0;
    for (const auto& s : slimes_) {
        if (s && s->IsActive()) {
            total += s->GetSize();
        }
    }
    return total;
}

void SlimeManager::Draw(const RenderContext& ctx) {
    // 1. 遮蔽時 X-Ray 描画（障害物の裏に隠れたスライムを描画）
    // ※ 通常描画の前に実行することで、スライム自身のポリゴン深度との自己干渉を完全防止！
    //    デプスバッファにはステージや障害物の深度しか入っていないため、
    //    手前に何もない平地では GREATER テストが 100% 不合格となり、白浮きが絶対に発生しない。
    if (xRayPSO_) {
        for (auto& slime : slimes_) {
            if (slime && slime->IsActive()) {
                slime->DrawXRay(ctx, xRayPSO_.Get());
            }
        }
    }

    // 2. 通常描画（手前に遮蔽物のない可視スライムを描画）
    for (auto& slime : slimes_) {
        if (slime && slime->IsActive()) {
            slime->Draw(ctx);
        }
    }
}

void SlimeManager::DrawDebug(Camera* cam) {
    for (auto& slime : slimes_) {
        if (slime) {
            slime->DrawDebug(cam);
        }
    }
}
