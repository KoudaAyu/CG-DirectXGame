#pragma once
#include <ostream>
#include <wrl.h>
#include <xaudio2.h>
#include <mutex>

#include"Sound.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <vector>

class WindowsAPI;

class AudioManager
{
public:
	AudioManager(std::ostream& log);
	~AudioManager();

	bool Initialize();
	void Finalize();;
	void Update();
	int32_t Load(const std::string& filename);
	int32_t Play(int32_t soundId);
	void Stop(int32_t playId);
	void StopAll();
	void Unload(int32_t soundId);
	float GetMasterVolume() const;
	void SetMasterVolume(float volume);

private:
	std::unique_ptr<Sound> sound_;

private:
	struct PlayingVoice
	{
		IXAudio2SourceVoice* voice = nullptr;
		SoundData* soundData = nullptr;
	};

	Sound* GetSound()
	{
		return sound_.get();
	}

	std::ostream& logStream_;
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<int32_t, std::unique_ptr<SoundData>> loadedSounds_;
	std::unordered_map<int32_t, PlayingVoice> playingVoices_;
	std::atomic<int32_t> nextSoundId_{ 1 };
	std::atomic<int32_t> nextPlayId_{ 1 };
   
    float masterVolume_ = 1.0f;
	std::mutex mutex_;
};

