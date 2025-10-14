#pragma once

#include<fstream>
#include<xaudio2.h>
#pragma comment(lib,"xaudio2.lib")

#include<wrl.h>

struct SoundData
{
	//波型フォーマット
	WAVEFORMATEX wfex;
	//バッファの先頭アドレス
	BYTE* pBuffer;
	//バッファのサイズ
	unsigned int bufferSize;
};

struct ChunkHeader
{
	char id[4]; // チャンクID
	uint32_t size; // チャンクのサイズ
};

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

class Sound
{
public:
	void Initialize();
	void Finalize();

	void SoundPlayWave(Microsoft::WRL::ComPtr<IXAudio2>& xAudio2, const SoundData& soundData);

	void SoundUnload(SoundData* soundData);

	SoundData SoundLoadWave(const char* filename);

	Microsoft::WRL::ComPtr<IXAudio2>& GetXAudio2()
	{
		return xAudio2;
	}


private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
	IXAudio2MasteringVoice* masterVoice = nullptr;
};
