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



bool Sound::LoadFileToSoundData(const std::string& filename, SoundData& out)
{
	// 読み込み前に以前のデータをリセット
	auto& storage = SoundStorage();
	if (storage)
	{
		storage->SoundUnload();
	}

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
						out.wfex = format.fmt;
						out.buffer.resize(data.size);
						file.read(reinterpret_cast<char*>(out.buffer.data()), data.size);
						file.close();
						return true;
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
	out.wfex = *waveFormat;
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
			size_t oldSize = out.buffer.size();
			out.buffer.resize(oldSize + currentLength);
			memcpy(&out.buffer[oldSize], pData, currentLength);
			pBuffer->Unlock();
		}
	}

	return false;
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