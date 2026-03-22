#include "AudioManager.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>

#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")
#pragma comment(lib,"xaudio2.lib")

AudioManager::AudioManager(std::ostream& log)
	: logStream_(log)
{
}

AudioManager::~AudioManager()
{
	Finalize();
}

bool AudioManager::Initialize()
{
    if (xAudio2_) return true;

    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
    if (FAILED(hr))
    {
        logStream_ << "AudioManager: MFStartup failed hr=" << std::hex << hr << std::dec << "\n";
        Finalize();
        return false;
    }

    hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr) || !xAudio2_)
    {
        logStream_ << "AudioManager: XAudio2Create failed hr=" << std::hex << hr << std::dec << "\n";
        Finalize();
        return false;
    }

    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
    if (FAILED(hr) || !masterVoice_)
    {
        logStream_ << "AudioManager: CreateMasteringVoice failed hr=" << std::hex << hr << std::dec << "\n";
        Finalize();
        return false;
    }

    logStream_ << "AudioManager: initialized\n";
    return true;
}

void AudioManager::Finalize()
{
    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& p : playingVoices_)
        {
            if (p.second.voice)
            {
                p.second.voice->Stop();
                p.second.voice->DestroyVoice();
                p.second.voice = nullptr;
            }
            p.second.soundData.reset();
        }
        playingVoices_.clear();
        loadedSounds_.clear();
    }

    if (masterVoice_)
    {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }

    xAudio2_.Reset();

    MFShutdown();

    logStream_ << "AudioManager: finalized\n";
}

int32_t AudioManager::Load(const std::string& filename)
{
    if (filename.empty())
    {
        logStream_ << "AudioManager::Load - empty filename\n";
        return -1;
    }

    if (!xAudio2_)
    {
        logStream_ << "AudioManager::Load - XAudio2 not initialized\n";
        return -1;
    }

    auto data = std::make_shared<SoundData>();
    if (!Sound::LoadFileToSoundData(filename, *data))
    {
        logStream_ << "AudioManager::Load - failed to load audio file: " << filename << "\n";
        return -1;
    }

    if (data->buffer.empty())
    {
        logStream_ << "AudioManager::Load - no audio data loaded: " << filename << "\n";
        return -1;
    }

    int32_t id = nextSoundId_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        loadedSounds_.emplace(id, std::move(data));
    }

    logStream_ << "AudioManager::Load - loaded id=" << id << " file=" << filename << "\n";
    return id;
}

int32_t AudioManager::Play(int32_t soundId)
{
    if (!xAudio2_)
    {
        logStream_ << "AudioManager::Play - XAudio2 not initialized\n";
        return -1;
    }

    std::shared_ptr<SoundData> data;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = loadedSounds_.find(soundId);
        if (it == loadedSounds_.end())
        {
            logStream_ << "AudioManager::Play - soundId not found: " << soundId << "\n";
            return -1;
        }
        data = it->second;
    }

    IXAudio2SourceVoice* src = nullptr;
    HRESULT hr = xAudio2_->CreateSourceVoice(&src, &data->wfex);
    if (FAILED(hr) || src == nullptr)
    {
        logStream_ << "AudioManager::Play - CreateSourceVoice failed hr=" << std::hex << hr << std::dec << "\n";
        return -1;
    }

    XAUDIO2_BUFFER buf{};
    buf.Flags = XAUDIO2_END_OF_STREAM;
    buf.pAudioData = data->buffer.data();
    buf.AudioBytes = static_cast<UINT32>(data->buffer.size());

    hr = src->SubmitSourceBuffer(&buf);
    if (FAILED(hr))
    {
        logStream_ << "AudioManager::Play - SubmitSourceBuffer failed hr=" << std::hex << hr << std::dec << "\n";
        src->DestroyVoice();
        return -1;
    }

    hr = src->Start(0);
    if (FAILED(hr))
    {
        logStream_ << "AudioManager::Play - Start failed hr=" << std::hex << hr << std::dec << "\n";
        src->DestroyVoice();
        return -1;
    }

    int32_t playId = nextPlayId_.fetch_add(1);
    {
        std::lock_guard<std::mutex> lk(mutex_);
        playingVoices_.emplace(playId, PlayingVoice{ src, std::move(data) });
    }

    logStream_ << "AudioManager::Play - playing playId=" << playId << " soundId=" << soundId << "\n";
    return playId;
}

void AudioManager::Stop(int32_t playId)
{
    std::lock_guard<std::mutex> lk(mutex_);
    auto it = playingVoices_.find(playId);
    if (it == playingVoices_.end())
    {
        return;
    }

    auto& playing = it->second;
    if (playing.voice)
    {
        playing.voice->Stop();
        playing.voice->DestroyVoice();
        playing.voice = nullptr;
    }
    playing.soundData.reset();
    playingVoices_.erase(it);
    logStream_ << "AudioManager::Stop - stopped playId=" << playId << "\n";
}

void AudioManager::Update()
{
    std::vector<int32_t> finished;

    {
        std::lock_guard<std::mutex> lk(mutex_);
        for (auto& p : playingVoices_)
        {
            auto* voice = p.second.voice;
            if (!voice) { finished.push_back(p.first); continue; }

            XAUDIO2_VOICE_STATE state{};
            voice->GetState(&state);
            if (state.BuffersQueued == 0)
            {
                finished.push_back(p.first);
            }
        }

        for (int32_t id : finished)
        {
            auto it = playingVoices_.find(id);
            if (it != playingVoices_.end())
            {
                if (it->second.voice)
                {
                    it->second.voice->DestroyVoice();
                    it->second.voice = nullptr;
                }
                it->second.soundData.reset();
                playingVoices_.erase(it);
                logStream_ << "AudioManager::Update - voice finished playId=" << id << "\n";
            }
        }
    }
}
