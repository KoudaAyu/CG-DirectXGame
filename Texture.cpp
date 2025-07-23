#include "Texture.h"


std::wstring ConvertString(const std::string& str)
{
	if (str.empty())
	{
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0)
	{
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

void Texture::Load(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filePath)
{
	// ステップ1: テクスチャファイルをCPUメモリに読み込む
	// LoadTexture 関数を呼び出し、結果を一時変数 mipImages に格納
	DirectX::ScratchImage mipImages = LoadTexture(filePath);
	// 読み込んだテクスチャのメタデータを、Textureクラスのメンバ変数 metadata_ に保存
	metadata_ = mipImages.GetMetadata();

	// ステップ2: GPU上にテクスチャリソースを作成する
	// CreateTextureResource は ComPtr<ID3D12Device> の参照を受け取るため、
	// Load関数の引数 device (ID3D12Device* の生ポインタ) を一時的に ComPtr でラップします。
	Microsoft::WRL::ComPtr<ID3D12Device> tempDeviceComPtr;
	// device が有効なポインタであることを確認
	if (device)
	{
		// 生ポインタからComPtrを作成し、所有権を移します。
		// ※注意：もし呼び出し元が生ポインタの所有権を維持したい場合は、
		// CreateTextureResourceの引数をID3D12Device*に変更するのが理想的です。
		// ここでは、現在のCreateTextureResourceの引数の型に合わせてAttachを使用します。
		tempDeviceComPtr.Attach(device);
	}
	else
	{
		// device が無効な場合の基本的なエラー処理
		assert(false && "Error: ID3D12Device* device is nullptr in Texture::Load.");
		return;
	}

	// 作成したGPUリソースを、Textureクラスのメンバ変数 resource_ に格納
	resource_ = CreateTextureResource(tempDeviceComPtr, metadata_);

	// ステップ3: CPUメモリのテクスチャデータをGPUリソースにアップロードする
	// UploadTextureData の引数に合わせ、commandListも一時的に ComPtr でラップします。
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> tempCommandListComPtr;
	if (commandList)
	{
		tempCommandListComPtr.Attach(commandList);
	}
	else
	{
		assert(false && "Error: ID3D12GraphicsCommandList* commandList is nullptr in Texture::Load.");
		return;
	}

	// UploadTextureData を呼び出し、アップロード用の中間リソースを uploadHeap_ に格納
	// resource_ は既に ComPtr<ID3D12Resource> 型なので、そのまま渡します（参照渡し）。
	// tempDeviceComPtr のアドレス (&tempDeviceComPtr) を渡します（UploadTextureDataの引数はComPtrへのポインタ）。
	uploadHeap_ = UploadTextureData(resource_, mipImages, &tempDeviceComPtr, tempCommandListComPtr);

	// ステップ4: シェーダーリソースビュー (SRV) の設定
	// これにより、シェーダーからこのテクスチャにアクセスするための情報が準備されます。
	srvDesc_.Format = metadata_.format;
	srvDesc_.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして扱う
	srvDesc_.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 標準的なコンポーネントマッピング
	srvDesc_.Texture2D.MipLevels = UINT(metadata_.mipLevels); // 全てのミップマップを使用
}

DirectX::ScratchImage Texture::LoadTexture(const std::string& filePath)
{
	//テクスチャファイルを読み込んでプログラムで使えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_DEFAULT_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	//ミニマップの作成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	//ミニマップ付きのデータを返す
	return mipImages;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Texture::CreateTextureResource(const Microsoft::WRL::ComPtr<ID3D12Device>& device, const DirectX::TexMetadata& metadata)
{
	//1. metadataを基にResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);//Textureの幅
	resourceDesc.Height = UINT(metadata.height);//Textureの高さ
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);//mipmapの数
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;//サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);//Textureの次元数。普段使っているのは2次元

	//2. 利用するHeapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//細かい設定を行う
	//heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;//WriteBackポリシーでCPUアクセス可能
	//heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;//プロセッサの近くに配列

	//3. Resourceを生成する
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,//Heapの設定
		D3D12_HEAP_FLAG_NONE,//Heapの特殊な設定
		&resourceDesc,//Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST,//初回のResourceState。Textureは基本読むだけ
		nullptr,//Clear最適値。使わないのでnullptr
		IID_PPV_ARGS(&resource));//作成するResourceポインタへのポインタ
	assert(SUCCEEDED(hr));
	return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> Texture::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource>& texture, const DirectX::ScratchImage& mipImages, const Microsoft::WRL::ComPtr<ID3D12Device>* device, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device->Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = Dx12::CreateBufferResource(device->Get(), intermediateSize);
	UpdateSubresources(commandList.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	//textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier(1, &barrier);
	return intermediateResource;
}
