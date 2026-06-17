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
			// 頂点インデックスのバウンズチェック
			if (vertexWeight.vertexIndex >= modelData.vertices.size())
			{
				continue;
			}
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

	// ウェイトの正規化と、どのジョイントにも紐付いていない頂点のフォールバック
	for (size_t i = 0; i < modelData.vertices.size(); ++i)
	{
		auto& currentInfluence = skinCluster.mappedInfluence[i];
		float totalWeight = 0.0f;
		for (uint32_t index = 0; index < kNumMaxInfluences; ++index)
		{
			totalWeight += currentInfluence.weights[index];
		}

		if (totalWeight == 0.0f)
		{
			// ルートジョイント（0番）にウェイト1.0fを割り当てて引っ張られないようにする
			currentInfluence.weights[0] = 1.0f;
			currentInfluence.jointIndices[0] = 0;
		}
		else
		{
			// 合計が1.0fになるように正規化
			for (uint32_t index = 0; index < kNumMaxInfluences; ++index)
			{
				currentInfluence.weights[index] /= totalWeight;
			}
		}
	}

	//Skinning結果を書き込むリソース(バッファ)を生成する
	//Vertexは3D/Object/Object3D.h値腕定義されている構造体
	size_t vertexCount = modelData.vertices.size();
	skinCluster.uavResource = directXCom.CreateBufferResource(device,sizeof(Vertex) * vertexCount);

	//DescriptorHeapからUAV用のIndexを1つ割り当てる
	uint32_t uavIndex = srvManager.Allocate();
	skinCluster.uavDescriptorHandle.first = directXCom.GetCPUDescriptorHandle(descriptorHeap,descriptorSize,uavIndex);
	skinCluster.uavDescriptorHandle.second = directXCom.GetGPUDescriptorHandle(descriptorHeap,descriptorSize,uavIndex);

	//DirectXComに生成したUAVリソースを渡して、Unordered Access View(UAV)を作成する
	directXCom.CreateUnroaderedAccessView(skinCluster.uavResource,UINT(vertexCount),sizeof(Vertex),skinCluster.uavDescriptorHandle.first);

	// 入力頂点用のSRVを作成
	uint32_t inputVertexSrvIndex = srvManager.Allocate();
	skinCluster.inputVertexSrvHandle.first = directXCom.GetCPUDescriptorHandle(descriptorHeap, descriptorSize, inputVertexSrvIndex);
	skinCluster.inputVertexSrvHandle.second = directXCom.GetGPUDescriptorHandle(descriptorHeap, descriptorSize, inputVertexSrvIndex);
	// 入力頂点用のバッファはObject3dが作成するリソースをBindするため、ここではハンドル割り当てのみ行い、バインド時にSRVを生成するか、ここで空で作成します。
	// ここではSRVManagerの関数を使用してStructuredBuffer用SRVを作成します。
	// ※Object3dの頂点リソースが生成された後に生成する必要があるため、バインド時に直前で作成するか、あるいはこのSkinCluster作成時点ではObject3dのリソース生成前である可能性があるため、空ハンドルとして扱い呼び出し側で解決できるようにします。

	// インフルエンス用のSRVを作成
	uint32_t influenceSrvIndex = srvManager.Allocate();
	skinCluster.influenceSrvHandle.first = directXCom.GetCPUDescriptorHandle(descriptorHeap, descriptorSize, influenceSrvIndex);
	skinCluster.influenceSrvHandle.second = directXCom.GetGPUDescriptorHandle(descriptorHeap, descriptorSize, influenceSrvIndex);
	srvManager.CreateSRVForStructuredBuffer(influenceSrvIndex, skinCluster.influenceResource.Get(), UINT(vertexCount), sizeof(VertexInfluence));

	// SkinningInformation用の定数バッファを作成
	skinCluster.skinningInfoResource = directXCom.CreateBufferResource(device, sizeof(uint32_t) * 4); // 16バイトアライメントのため4つのuint32_t分のサイズにする
	uint32_t* infoData = nullptr;
	skinCluster.skinningInfoResource->Map(0, nullptr, reinterpret_cast<void**>(&infoData));
	infoData[0] = UINT(vertexCount); // numVertices
	skinCluster.skinningInfoResource->Unmap(0, nullptr);

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
