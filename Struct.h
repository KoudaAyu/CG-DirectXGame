#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <xaudio2.h>

#include"Vector.h"

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
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

