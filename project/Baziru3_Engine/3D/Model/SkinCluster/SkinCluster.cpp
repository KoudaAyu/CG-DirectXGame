#include "SkinCluster.h"

#include "DirectXCom.h"
#include "SRVManager.h"

SkinCluster SkinClusterLender::CreateSkinCluster(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const Skeleton& skeleton,
	const Model::ModelData& modelData, const Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, uint32_t descriptorSize, DirectXCom& directXCom, SRVManager& srvManager)
{
	SkinCluster skinCluster;
	skinCluster.paletteResource = directXCom.CreateBufferResource(device, sizeof(WellForGPU) * skeleton.joints.size());
	WellForGPU* mappedPalette = nullptr;
	skinCluster.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedPalette));
	skinCluster.mappedPalette = { mappedPalette,skeleton.joints.size() };
	uint32_t srvIndex = srvManager.Allocate();
	skinCluster.paletteSrvHandle.first = directXCom.GetCPUDescriptorHandle(descriptorHeap, descriptorSize, srvIndex);
	skinCluster.paletteSrvHandle.second = directXCom.GetGPUDescriptorHandle(descriptorHeap, descriptorSize, srvIndex);

	D3D12_SHADER_RESOURCE_VIEW_DESC paletteSrvDesc{};
	paletteSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	paletteSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	paletteSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	paletteSrvDesc.Buffer.FirstElement = 0;
	paletteSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	paletteSrvDesc.Buffer.NumElements = UINT(skeleton.joints.size());
	paletteSrvDesc.Buffer.StructureByteStride = sizeof(WellForGPU);
	device->CreateShaderResourceView(skinCluster.paletteResource.Get(), &paletteSrvDesc, skinCluster.paletteSrvHandle.first);

	skinCluster.influenceResource = directXCom.CreateBufferResource(device, sizeof(VertexInfluence) * std::max<size_t>(1, modelData.vertices.size()));
	VertexInfluence* mappedInfluence = nullptr;
	skinCluster.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedInfluence));
	std::memset(mappedInfluence, 0, sizeof(VertexInfluence) * std::max<size_t>(1, modelData.vertices.size())); //0埋め。weightsは0、jointIndicesは-1で初期化される。
	skinCluster.mappedInfluence = { mappedInfluence, modelData.vertices.size() };

	// Influence用のVBVを作成
	skinCluster.influenceBufferView.BufferLocation = skinCluster.influenceResource->GetGPUVirtualAddress();
	skinCluster.influenceBufferView.StrideInBytes = UINT(sizeof(VertexInfluence));
	skinCluster.influenceBufferView.SizeInBytes = UINT(sizeof(VertexInfluence) * std::max<size_t>(1, modelData.vertices.size()));

	//InverseBindPoseMatrixを格納する場所を生成して、単位行列を埋める
	skinCluster.inverseBindPoseMatrices.resize(skeleton.joints.size());
	std::generate(skinCluster.inverseBindPoseMatrices.begin(), skinCluster.inverseBindPoseMatrices.end(), [] { return MakeIdentity4x4(); });

	for (const auto& jointWeight : modelData.skinClusterData)
	{
		auto it = skeleton.jointMap.find(jointWeight.first); //jointWight.firstはjoint名なので、skeletonの対象となるjointが含まれているか判断
		if (it == skeleton.jointMap.end()) 
		{
			continue; //含まれていない場合はスキップ
		}
		skinCluster.inverseBindPoseMatrices[(*it).second] = jointWeight.second.inverseBindPoseMatrix;
		for (const auto& vertexWeight : jointWeight.second.vertexWeights)
		{
			auto& currentInfluence = skinCluster.mappedInfluence[vertexWeight.vertexIndex];
			for (uint32_t index = 0; index < kNumMaxInfluences; ++index)
			{
				if (currentInfluence.weights[index] == 0.0f)
				{
					currentInfluence.weights[index] = vertexWeight.weight;
					currentInfluence.jointIndices[index] = (*it).second;
					break;
				}
			}
		}

	}

	return skinCluster;
}

void SkinClusterLender::Update(SkinCluster& skinCluster, const Skeleton& skeleton)
{
	for (size_t jointIndex = 0; jointIndex < skeleton.joints.size(); ++jointIndex)
	{
		assert(jointIndex < skinCluster.inverseBindPoseMatrices.size());

		skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix =
			Multiply(skinCluster.inverseBindPoseMatrices[jointIndex], skeleton.joints[jointIndex].skeletonMatrix);
		skinCluster.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix =
			Transpose(Inverse(skinCluster.mappedPalette[jointIndex].skeletonSpaceMatrix));
	}
}
