#pragma once
#include <cstdint>
#include <xaudio2.h>

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
