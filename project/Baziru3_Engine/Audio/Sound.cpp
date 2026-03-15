#include "Sound.h"
#include <cassert>
#include <cstring>

// Windows SDK ヘッダの警告を一時的に無効化
#pragma warning(push, 0)
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma warning(pop)

#include "StringUtil.h"

#pragma comment(lib,"mfplat.lib")
#pragma comment(lib,"Mfreadwrite.lib")
#pragma comment(lib,"mfuuid.lib")

namespace {
    static std::unique_ptr<Sound>& SoundStorage()
    {
        static std::unique_ptr<Sound> instance;
        return instance;
    }
}

Sound* Sound::GetInstance()
{
    auto& instance = SoundStorage();
    if (!instance)
    {
        instance = std::make_unique<Sound>();
        instance->Initialize();
    }
    return instance.get();
}

void Sound::Destroy()
{
    if (auto& inst = SoundStorage())
    {
        inst->Finalize();
        inst.reset();
    }
}

void Sound::Initialize()
{
	HRESULT result = XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	result = xAudio2->CreateMasteringVoice(&masterVoice);
	assert(SUCCEEDED(result));

	result = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(result));
}

void Sound::Finalize()
{
	// 実行中に明示的に呼ぶ終了処理用。ここで安全に後片付けを行う
	SoundUnload();

	if (masterVoice)
	{
		masterVoice->DestroyVoice();
		masterVoice = nullptr;
	}

	xAudio2.Reset();
	MFShutdown();
}

void Sound::SoundUnload()
{
	// SourceVoice を安全に破棄
	if (pSourceVoice)
	{
		pSourceVoice->Stop();
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}

	soundData.buffer.clear();
	soundData.wfex = {};
}

void Sound::SoundLoadFile(const std::string& filename)
{
	// 読み込み前に以前のデータをリセット
	SoundUnload();

	std::string ext = "";
	size_t pos = filename.find_last_of('.');
	if (pos != std::string::npos)
	{
		ext = filename.substr(pos);
		// ★修正：ラムダ式を使用してtolowerの曖昧さエラーを回避
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});
	}

	// --- WAV直接読み込み ---
	if (ext == ".wav")
	{
		std::ifstream file(filename, std::ios_base::binary);
		if (file.is_open())
		{
			RiffHeader riff;
			file.read((char*)&riff, sizeof(riff));
			if (strncmp(riff.chunk.id, "RIFF", 4) == 0 && strncmp(riff.type, "WAVE", 4) == 0)
			{
				FormatChunk format = {};
				file.read((char*)&format, sizeof(ChunkHeader));
				if (strncmp(format.chunk.id, "fmt ", 4) == 0)
				{
					file.read((char*)&format.fmt, format.chunk.size);
					ChunkHeader data;
					file.read((char*)&data, sizeof(data));
					while (strncmp(data.id, "data", 4) != 0 && !file.eof())
					{
						file.seekg(data.size, std::ios_base::cur);
						file.read((char*)&data, sizeof(data));
					}
					if (strncmp(data.id, "data", 4) == 0)
					{
						this->soundData.wfex = format.fmt;
						this->soundData.buffer.resize(data.size);
						file.read(reinterpret_cast<char*>(this->soundData.buffer.data()), data.size);
						file.close();
						return;
					}
				}
			}
		}
	}

	// --- Media Foundation 読み込み ---
	std::wstring filePathW = StringUtil::ConvertString(filename);
	Microsoft::WRL::ComPtr<IMFSourceReader> pReader;
	HRESULT hr = MFCreateSourceReaderFromURL(filePathW.c_str(), nullptr, &pReader);
	assert(SUCCEEDED(hr));

	Microsoft::WRL::ComPtr<IMFMediaType> pPCMType;
	MFCreateMediaType(&pPCMType);
	pPCMType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPCMType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	pReader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPCMType.Get());

	Microsoft::WRL::ComPtr<IMFMediaType> pOutType;
	pReader->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);

	WAVEFORMATEX* waveFormat = nullptr;
	MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &waveFormat, nullptr);
	this->soundData.wfex = *waveFormat;
	CoTaskMemFree(waveFormat);

	while (true)
	{
		Microsoft::WRL::ComPtr<IMFSample> pSample;
		DWORD flags = 0;
		hr = pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &pSample);
		if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) break;

		if (pSample)
		{
			Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
			pSample->ConvertToContiguousBuffer(&pBuffer);
			BYTE* pData = nullptr;
			DWORD currentLength = 0;
			pBuffer->Lock(&pData, nullptr, &currentLength);
			size_t oldSize = this->soundData.buffer.size();
			this->soundData.buffer.resize(oldSize + currentLength);
			memcpy(&this->soundData.buffer[oldSize], pData, currentLength);
			pBuffer->Unlock();
		}
	}
}

void Sound::SoundPlayWave()
{
	if (soundData.buffer.empty()) return;

	// 前回のボイスが残っていれば一度停止してから破棄
	if (pSourceVoice)
	{
		pSourceVoice->Stop();
		pSourceVoice->DestroyVoice();
		pSourceVoice = nullptr;
	}

	HRESULT result = xAudio2->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	XAUDIO2_BUFFER buf = {};
	buf.pAudioData = soundData.buffer.data();
	buf.AudioBytes = (UINT32)soundData.buffer.size();
	buf.Flags = XAUDIO2_END_OF_STREAM;

	result = pSourceVoice->SubmitSourceBuffer(&buf);
	assert(SUCCEEDED(result));

	result = pSourceVoice->Start(0);
	assert(SUCCEEDED(result));
}