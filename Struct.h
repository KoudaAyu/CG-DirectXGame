#pragma once
#include"Vector.h"

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Transform
{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3]; // パディングを追加して16バイト境界に揃える
	Matrix4x4 uvTransform; // UV変換行列
};

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct MaterialData
{
	std::string textureFilePath; // テクスチャファイルのパス
};

//objファイル関係
struct ModelData
{
	std::vector<VertexData> vertices; // 頂点データ
	MaterialData material; // マテリアルデータ
};

//リソースリークチェック
struct D3DResourceLeakChecker
{
	~D3DResourceLeakChecker()
	{
		Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
		{
			debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
			debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);

		}
	}
};

//チャンクヘッダ
struct ChunkHeader
{
	char id[4]; // チャンクID
	uint32_t size; // チャンクのサイズ
};
//RIFFヘッダチャンク
struct RiffHeader
{
	ChunkHeader chunk; // チャンクヘッダ
	char type[4]; // フォーマット（"WAVE"）
};

struct FormatChunk
{
	ChunkHeader chunk; //fmt
	WAVEFORMATEX fmt; // フォーマット情報
};

struct SoundData
{
	//波型フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};
