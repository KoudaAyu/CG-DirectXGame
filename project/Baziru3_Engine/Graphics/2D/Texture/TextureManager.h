#pragma once
#include "DirectXCom.h"
#include "StringUtil.h"
#include "SrvManager.h"

#include<unordered_map>
#include <memory>
#include <vector>

/**
 * @brief テクスチャの読み込みとSRVディスクリプタ管理を行うクラス
 */
class TextureManager
{
public:
	//シングルトンインスタンスを取得
	static TextureManager* GetInstance();
	static void Destroy();

	~TextureManager() = default;

	/**
	 * @brief テクスチャマネージャー終了処理を行い、リソースを解放します
	 */
	void Finalize();


	/**
	 * @brief テクスチャマネージャー初期化処理を行います
	 */
	void Initialize();

	// アップロード用中間バッファの一括解放と同期
	void ReleaseUploadBuffers();

	void LoadTexture(const std::string& filePath);

	// DirectXCom を設定するためのセッターを追加
	void SetDirectXCom(DirectXCom* directXCom);

	// SRVManager へのアクセサ（SRV インデックス管理を一元化するため）
	SRVManager* GetSRVManager() const { return srvManager_.get(); }

	/**
	 * @brief ファイルパスからSRVインデックスを取得します
	 * @param filePath 検索対象のテクスチャファイルパス
	 * @return SRVインデックス
	 */
	uint32_t GetTextureIndexByFilePath(const std::string& filePath) const;

	//メタデータ取得 (オーバーロード: ファイルパスまたはインデックスで取得可能)
	const DirectX::TexMetadata& GetMetadata(const std::string& filePath) const
	{
		auto it = textureDates_.find(filePath);
		assert(it != textureDates_.end());
		return it->second.metadata_;
	}
	const DirectX::TexMetadata& GetMetadata(uint32_t index) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath) const
	{
		auto it = textureDates_.find(filePath);
		assert(it != textureDates_.end());
		return it->second.srvHandleGPU_;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t index) const;

	/**
	 * @brief テクスチャを読み込み、SRVを作成します
	 * @param filePath 読み込むテクスチャのパス
	 * @return 登録されたSRVディスクリプタのインデックス
	 */
	uint32_t Load(const std::string& filePath);

	// 便利ヘルパー: 作業ディレクトリ基準の単純ロード
	static uint32_t LoadTextureHandle(const std::string& filePath)
	{
		return GetInstance()->Load(filePath);
	}

	static constexpr uint32_t kInvalidTextureIndex = UINT32_MAX;

private:
	std::unique_ptr<SRVManager> srvManager_ = nullptr;

	TextureManager() = default;

private:
	TextureManager(TextureManager&) = delete;
	TextureManager& operator=(TextureManager&) = delete;

	// テクスチャ1枚分のデータ
	struct TextureData
	{
		std::string filePath_;
		DirectX::TexMetadata metadata_;
		Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
		uint32_t srvIndex_;
		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU_{};
		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU_{};


	};

	//テクスチャデータ
	std::unordered_map<std::string,TextureData> textureDates_;

	// SRVインデックスからファイルパスを引ける逆引きマップ (O(1) 検索)
	std::unordered_map<uint32_t, std::string> indexToFilePath_;

	DirectXCom* directXCom_ = nullptr;

	//SRVインデックスの開始番号
	uint32_t kSRVIndexTop = 1;

	// コピー用中間リソースの一時保持リスト
	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uploadBuffers_;

	
};