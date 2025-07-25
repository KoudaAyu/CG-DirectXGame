#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <xaudio2.h>

#include"Matrix4x4.h"
#include"Vector.h"

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

struct DirectionalLight
{
	Vector4 color;
	Vector3 direction;
	float intensity;
};

struct Material
{
	Vector4 color;
	int32_t enableLighting;
	float padding[3]; // パディングを追加して16バイト境界に揃える
	Matrix4x4 uvTransform; // UV変換行列
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

