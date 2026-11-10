#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

#include "Matrix4x4.h"
#include "TextureManager.h"

class DirectXCom;
class Camera;

/// <summary>
/// アプリケーション層・リアルタイムアニメーション雲システム
/// エンジン本体（Baziru3_Engine / 既存シェーダー）を一切変更せず、
/// Perlin-Worley シームレスノイズを用いた3層パララックス雲（巻雲・主積雲・低層ちぎり雲）を
/// Skybox直後に深度書き込みなしで合成描画する。
/// </summary>
class SkyCloudSystem
{
public:
    SkyCloudSystem() = default;
    ~SkyCloudSystem() = default;

    bool Initialize(DirectXCom* dx);
    void Finalize();
    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* cmdList, Camera* camera);

private:
    bool CreateGeometry();
    bool CreatePipeline();

private:
    DirectXCom* dx_ = nullptr;
    bool initialized_ = false;
    float time_ = 0.0f;
    uint32_t cloudNoiseTexIndex_ = TextureManager::kInvalidTextureIndex;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};
    D3D12_INDEX_BUFFER_VIEW ibView_{};
};

