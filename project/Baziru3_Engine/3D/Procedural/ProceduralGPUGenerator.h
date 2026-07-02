#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include "Sprite.h"
#include "BioProceduralGenerator.h"

class DirectXCom;

// DirectX 12 Compute Shader による岩石プロシージャル生成管理クラス
class ProceduralGPUGenerator
{
public:
    ProceduralGPUGenerator() = default;
    ~ProceduralGPUGenerator();

    /// <summary>
    /// パイプラインおよびリソースの初期化
    /// </summary>
    /// <param name="dxCom">DirectXComポインタ</param>
    /// <returns>初期化成否</returns>
    bool Initialize(DirectXCom* dxCom);

    /// <summary>
    /// ベースとなる綺麗な球体メッシュを設定・転送する (初回のみ)
    /// </summary>
    bool SetBaseMesh(const std::vector<Sprite::VertexData>& vertices);

    /// <summary>
    /// GPU側で変形Compute Shaderを実行する (Dispatch)
    /// </summary>
    /// <param name="params">岩石生成パラメータ</param>
    /// <param name="vertexCount">頂点数</param>
    /// <returns>実行成否</returns>
    bool Dispatch(const BioProcedural::RockParameters& params, uint32_t vertexCount);

    /// <summary>
    /// 出力された頂点バッファのビューを取得 (描画用)
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return vertexBufferView_; }

    /// <summary>
    /// 出力バッファ自体を取得
    /// </summary>
    ID3D12Resource* GetOutputVertexBuffer() const { return outputBuffer_.Get(); }

private:
    bool CreatePipeline();
    bool CreateBuffers(uint32_t maxVertices);

private:
    DirectXCom* dxCom_ = nullptr;

    // D3D12 Compute Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // GPU Buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> baseMeshSRV_;       // 入力: ベース球体頂点バッファ [SRV]
    Microsoft::WRL::ComPtr<ID3D12Resource> outputBuffer_;      // 出力: 変形後頂点バッファ [UAV & VBV]
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;    // 定数: パラメータ用 [CBV]

    // Descriptor Heaps (SRV/UAV用)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap_;

    // View & Descriptor Handles
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle_{};

    uint32_t currentMaxVertices_ = 0;
    bool isBaseMeshSet_ = false;
};
