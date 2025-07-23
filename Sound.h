#pragma once
#include<xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

#include<cassert>
#include<fstream>
#include<wrl.h>

#include"Struct.h"

class Sound
{
public:
	Sound();
	~Sound();

	void Initialize();

	void Update();

	void Play();

	SoundData SoundLoadWave(const char* filename);

	void SoundUnload(SoundData* soundData);

	void SoundPlayWave(Microsoft::WRL::ComPtr<IXAudio2>& xAudio2, const SoundData& soundData);

	void Load();

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2;

	SoundData soundData;
};