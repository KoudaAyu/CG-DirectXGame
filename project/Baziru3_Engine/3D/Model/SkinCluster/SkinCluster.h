#pragma once

#include "Matrix4x4.h"
#include "Model.h"


#include <array>
#include <d3d12.h>
#include <vector>
#include <wrl.h>
#include <span>

const uint32_t kNumMaxInfluences = 4; // 頂点あたりの最大ジョイント影響数
struct VertexInfluence
{
	std::array<float, kNumMaxInfluences> weights; // ジョイントの影響度（ウェイト）
	std::array<int32_t, kNumMaxInfluences> jointIndices;
};

struct WellForGPU
{
	Matrix4x4 skeletonSpaceMatrix; // ジョイントのスケルトンスペース行列
	Matrix4x4 skeletonSpaceInverseTransposeMatrix; // ジョイントのスケルトンスペース行列の逆転置行列
};

struct SkinCluster
{
	std::vector<Matrix4x4> inverseBindPoseMatrices; // 各ジョイントの逆バインド行列
	Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource; // 頂点ごとのジョイント影響データを格納するGPUリソース
	std::span<VertexInfluence> mappedInfluence;

	Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource; // ジョイントのバインド行列を格納するGPUリソース
	std::span<WellForGPU> mappedPalette;
	std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle; // ジョイントのバインド行列をシェーダーで参照するためのSRVハンドル
};

class SkinCluster
{
public:
	SkinCluster CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& ekeleton,
		const Model::ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize);;
};
