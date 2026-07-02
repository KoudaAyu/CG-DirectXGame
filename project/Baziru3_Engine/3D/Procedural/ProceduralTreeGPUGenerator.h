#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <string>
#include "BioProceduralGenerator.h"
#include "Vector.h"

class DirectXCom;

// TreeConstantBuffer and Generator use BioProcedural::GPUBranchesSegment defined in BioProceduralGenerator.h

// 定数バッファ構造体
struct TreeConstantBuffer {
    Vector3 windDirection;
    float windStrength;
    float time;
    uint32_t maxSegments;
    uint32_t currentSegments;
    float padding;
};

// DirectX 12 Compute Shader による樹木プロシージャル生成管理クラス
class ProceduralTreeGPUGenerator
{
public:
    ProceduralTreeGPUGenerator() = default;
    ~ProceduralTreeGPUGenerator();

    /// <summary>
    /// パイプラインおよびリソースの初期化
    /// </summary>
    bool Initialize(DirectXCom* dxCom);
    
    /// <summary>
    /// CPUで算出した骨格ノード配列をSRVへ転送する
    /// </summary>
    bool SetSkeletonData(const std::vector<BioProcedural::GPUBranchesSegment>& segments);

    /// <summary>
    /// CSを実行して並列メッシュ拡張を行う
    /// </summary>
    bool Dispatch(const Vector3& windDirection, float windStrength, float timeValue, uint32_t currentSegments);

    /// <summary>
    /// 出力された頂点バッファのビューを取得
    /// </summary>
    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return vertexBufferView_; }
    
    /// <summary>
    /// 出力バッファリソース自体のポインタを取得
    /// </summary>
    ID3D12Resource* GetOutputVertexBuffer() const { return outputBuffer_.Get(); }

    // 最大セグメント数および頂点数定義
    static const uint32_t kMaxSegments = 2000;
    static const uint32_t kMaxVertices = kMaxSegments * 18 + kMaxSegments * 32;

private:
    bool CreatePipeline();
    bool CreateBuffers();

private:
    DirectXCom* dxCom_ = nullptr;

    // Compute Pipeline
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // GPU Buffers
    Microsoft::WRL::ComPtr<ID3D12Resource> skeletonSRV_;       // 入力: 骨格セグメントバッファ [SRV]
    Microsoft::WRL::ComPtr<ID3D12Resource> outputBuffer_;      // 出力: 頂点バッファ [UAV & VBV]
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;    // 定数: パラメータ用 [CBV]

    // Descriptor Heap (UAV / SRV 用)
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descHeap_;

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE uavGpuHandle_{};

    bool isSkeletonDataTransferred_ = false;
};
