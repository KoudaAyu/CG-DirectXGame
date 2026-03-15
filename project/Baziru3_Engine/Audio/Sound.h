#pragma once

#include <fstream>
#include <xaudio2.h>
#include <wrl.h>
#include <vector>
#include <string>    
#include <algorithm>
#include <cctype>   
#include <memory>

#pragma comment(lib,"xaudio2.lib")

struct SoundData
{
	WAVEFORMATEX wfex;
	std::vector<BYTE> buffer;
};

struct ChunkHeader
{
	char id[4];
	uint32_t size;
};

struct RiffHeader
{
	ChunkHeader chunk;
	char type[4];
};

struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

class Sound
{
public:
    
    static Sound* GetInstance();
    static void Destroy();

    void Initialize();
    void Finalize();

    // 引数なしで内部の soundData をクリアするように変更
    void SoundUnload();

    // ファイル読み込み
    void SoundLoadFile(const std::string& filename);

  
    void SoundPlayWave();

    const SoundData& GetSoundData() const { return soundData; }
    Microsoft::WRL::ComPtr<IXAudio2>& GetXAudio2() { return xAudio2; }

private:
    Microsoft::WRL::ComPtr<IXAudio2> xAudio2;
    IXAudio2MasteringVoice* masterVoice = nullptr;

   
    IXAudio2SourceVoice* pSourceVoice = nullptr;

    // 読み込んだデータを保持する変数
    SoundData soundData;

    };