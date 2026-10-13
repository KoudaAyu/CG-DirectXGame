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
	//音声ファイルのロード
	int32_t Load(const std::string& filename);
	//再生
	int32_t Play(int32_t soundId);
	//停止
	void Stop(int32_t playId);
	//全停止
	void StopAll();
	//アンロード
	void Unload(int32_t soundId);

	//マスターボリューム
	float GetMasterVolume() const;
	void SetMasterVolume(float volume);

	//個別ボリューム
	void SetVolume(int32_t playId, float volume);

	//ワンショット再生
	int32_t PlayOneShot(int32_t soundId, float volume = 1.0f);

private:
	std::unique_ptr<Sound> sound_;

private:
	struct PlayingVoice
	{
		IXAudio2SourceVoice* voice = nullptr;
        int32_t soundId = 0;
	};

	Sound* GetSound()
	{
		return sound_.get();
	}

	std::ostream& logStream_;
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::unordered_map<int32_t, std::unique_ptr<SoundData>> loadedSounds_;
	std::unordered_map<std::string, int32_t> soundPathCache_;
	std::unordered_map<int32_t, PlayingVoice> playingVoices_;
	std::atomic<int32_t> nextSoundId_{ 1 };
	std::atomic<int32_t> nextPlayId_{ 1 };
   
    float masterVolume_ = 1.0f;
	std::mutex mutex_;
};

