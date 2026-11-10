#pragma once

#include "SceneManager.h"
#include "AudioManager.h"
#include "Vector.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <unordered_map>

/// <summary>
/// タクティカル音響ユーティリティ (TacticalAudio)
/// - エンジン本体を変更せずに全シーン・サブシステムから遅延ロード・クールダウン制御・3D距離減衰・シームレスループ再生を提供
/// </summary>
namespace TacticalAudio
{
	// 全体音量バランス調整係数（XAudio2 マスター音量と合わせてBGM・SE全体を心地よい音量帯へ抑える）
	inline constexpr float kSeVolumeScale          = 0.65f;
	inline constexpr float kBgmVolumeScale         = 0.55f;
	inline constexpr float kLoopSeVolumeScale      = 0.60f;

	// 距離減衰・最小可聴閾値・ループ制御定数
	inline constexpr float    kDefaultMinDist3D       = 5.0f;
	inline constexpr float    kDefaultMaxDist3D       = 46.0f;
	inline constexpr float    kAttenuationQuadFalloff = 0.55f;
	inline constexpr float    kMinAudibleVolume2D     = 0.002f;
	inline constexpr float    kMinAudibleVolume3D     = 0.012f;
	inline constexpr uint32_t kRngInitialSeed         = 0x9E3779B9u;
	inline constexpr uint32_t kRngMultiplier          = 1664525u;
	inline constexpr uint32_t kRngIncrement           = 1013904223u;
	inline constexpr uint32_t kRngShiftBits           = 16u;
	inline constexpr float    kMaxLoopDeltaSec        = 0.25f;
	inline constexpr float    kMinLoopPeriodSec       = 0.25f;
	inline constexpr float    kPrevTailDurationSec    = 0.35f;
	inline constexpr float    kVolumeUpdateThreshold  = 0.005f;

	inline AudioManager* GetAudio()
	{
		SceneManager* sm = SceneManager::GetInstance();
		return sm ? sm->GetAudioManager() : nullptr;
	}

	inline bool CheckCooldown(const std::string& key, int minCooldownMs)
	{
		if (minCooldownMs <= 0) return true;

		static std::unordered_map<std::string, std::chrono::steady_clock::time_point> sLastPlayed;
		const auto now = std::chrono::steady_clock::now();
		if (auto it = sLastPlayed.find(key); it != sLastPlayed.end())
		{
			const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
			if (elapsedMs < minCooldownMs) return false;
		}
		sLastPlayed[key] = now;
		return true;
	}

	inline int32_t ResolveSoundId(AudioManager* am, const std::string& path)
	{
		if (!am || path.empty()) return -1;

		static AudioManager* sCachedMgr = nullptr;
		static std::unordered_map<std::string, int32_t> sSoundIdCache;
		if (sCachedMgr != am)
		{
			sCachedMgr = am;
			sSoundIdCache.clear();
		}
		if (auto it = sSoundIdCache.find(path); it != sSoundIdCache.end())
		{
			return it->second;
		}
		const int32_t sid = am->Load(path);
		if (sid >= 0)
		{
			sSoundIdCache.emplace(path, sid);
		}
		return sid;
	}

	inline float ComputeAttenuation(const Vector3& worldPos, const Vector3& listenerPos, float minDist, float maxDist)
	{
		if (maxDist <= minDist) return 0.0f;

		const float dx = worldPos.x - listenerPos.x;
		const float dy = worldPos.y - listenerPos.y;
		const float dz = worldPos.z - listenerPos.z;
		const float distSq = dx * dx + dy * dy + dz * dz;
		if (distSq >= maxDist * maxDist) return 0.0f;
		if (minDist > 0.0f && distSq <= minDist * minDist) return 1.0f;

		const float dist = std::sqrt(distSq);
		const float t = (dist - minDist) / (maxDist - minDist);
		return (1.0f - t) * (1.0f - kAttenuationQuadFalloff * t);
	}

	inline int32_t Play2D(
		const std::string& path,
		float volume = 1.0f,
		int minCooldownMs = 0,
		const std::string& cooldownKey = "")
	{
		const float clampedVol = std::clamp(volume * kSeVolumeScale, 0.0f, 1.0f);
		if (clampedVol <= kMinAudibleVolume2D || path.empty()) return -1;

		AudioManager* am = GetAudio();
		if (!am) return -1;

		const std::string& key = cooldownKey.empty() ? path : cooldownKey;
		if (!CheckCooldown(key, minCooldownMs)) return -1;

		const int32_t soundId = ResolveSoundId(am, path);
		return (soundId >= 0) ? am->PlayOneShot(soundId, clampedVol) : -1;
	}

	inline int32_t PlayRandom2D(
		std::initializer_list<const char*> paths,
		float volume = 1.0f,
		int minCooldownMs = 0,
		const std::string& cooldownKey = "")
	{
		if (paths.size() == 0) return -1;

		static uint32_t sStepCounter = kRngInitialSeed;
		sStepCounter = sStepCounter * kRngMultiplier + kRngIncrement;
		const size_t idx = static_cast<size_t>(sStepCounter >> kRngShiftBits) % paths.size();
		auto it = paths.begin();
		std::advance(it, static_cast<ptrdiff_t>(idx));
		const std::string chosen = (*it) ? (*it) : "";
		const std::string key = cooldownKey.empty() ? std::string(*paths.begin()) : cooldownKey;
		return Play2D(chosen, volume, minCooldownMs, key);
	}

	inline int32_t Play3D(
		const std::string& path,
		const Vector3& worldPos,
		const Vector3& listenerPos,
		float baseVolume = 1.0f,
		float minDist = kDefaultMinDist3D,
		float maxDist = kDefaultMaxDist3D,
		int minCooldownMs = 0,
		const std::string& cooldownKey = "")
	{
		const float finalVol = baseVolume * ComputeAttenuation(worldPos, listenerPos, minDist, maxDist);
		if (finalVol <= kMinAudibleVolume3D) return -1;
		return Play2D(path, finalVol, minCooldownMs, cooldownKey);
	}

	inline int32_t PlayRandom3D(
		std::initializer_list<const char*> paths,
		const Vector3& worldPos,
		const Vector3& listenerPos,
		float baseVolume = 1.0f,
		float minDist = kDefaultMinDist3D,
		float maxDist = kDefaultMaxDist3D,
		int minCooldownMs = 0,
		const std::string& cooldownKey = "")
	{
		const float finalVol = baseVolume * ComputeAttenuation(worldPos, listenerPos, minDist, maxDist);
		if (finalVol <= kMinAudibleVolume3D) return -1;
		return PlayRandom2D(paths, finalVol, minCooldownMs, cooldownKey);
	}

	/// <summary>
	/// クロスフェードテール付きWAV音源をフレーム境界ノイズなしでシームレスにループ再生するハンドル
	/// </summary>
	class LoopHandle
	{
	public:
		void Update(const std::string& path, float periodSec, float volume, float dt)
		{
			if (path.empty())
			{
				Stop();
				return;
			}
			if (active_ && currentPath_ != path)
			{
				Stop();
			}

			const float categoryScale = active_
				? cachedCategoryScale_
				: ((path.find("BGM_") != std::string::npos) ? kBgmVolumeScale : kLoopSeVolumeScale);
			const float clampedVol = std::clamp(volume * categoryScale, 0.0f, 1.0f);
			if (clampedVol <= kMinAudibleVolume2D)
			{
				Stop();
				return;
			}

			AudioManager* am = GetAudio();
			if (!am) return;

			if (!active_)
			{
				const int32_t sid = ResolveSoundId(am, path);
				if (sid < 0) return;

				currentPlayId_ = am->PlayOneShot(sid, clampedVol);
				if (currentPlayId_ != -1)
				{
					currentPath_ = path;
					cachedSoundId_ = sid;
					cachedCategoryScale_ = categoryScale;
					timer_ = 0.0f;
					prevPlayId_ = -1;
					prevTailTimer_ = 0.0f;
					lastVolume_ = clampedVol;
					active_ = true;
				}
				return;
			}

			const float safeDt = std::clamp(dt, 0.0f, kMaxLoopDeltaSec);
			timer_ += safeDt;

			if (prevPlayId_ != -1)
			{
				prevTailTimer_ += safeDt;
				if (prevTailTimer_ > kPrevTailDurationSec)
				{
					prevPlayId_ = -1;
				}
			}

			const float safePeriod = (std::max)(periodSec, kMinLoopPeriodSec);
			if (timer_ >= safePeriod)
			{
				timer_ = std::fmod(timer_, safePeriod);
				if (prevPlayId_ != -1)
				{
					am->Stop(prevPlayId_);
				}
				prevPlayId_ = currentPlayId_;
				prevTailTimer_ = 0.0f;

				const int32_t sid = (cachedSoundId_ >= 0) ? cachedSoundId_ : ResolveSoundId(am, path);
				cachedSoundId_ = sid;
				currentPlayId_ = (sid >= 0) ? am->PlayOneShot(sid, clampedVol) : -1;
				lastVolume_ = clampedVol;
			}
			else if (std::abs(clampedVol - lastVolume_) > kVolumeUpdateThreshold)
			{
				if (currentPlayId_ != -1) am->SetVolume(currentPlayId_, clampedVol);
				if (prevPlayId_ != -1)    am->SetVolume(prevPlayId_, clampedVol);
				lastVolume_ = clampedVol;
			}
		}

		void Stop()
		{
			if (!active_ && currentPlayId_ == -1 && prevPlayId_ == -1) return;

			if (AudioManager* am = GetAudio())
			{
				if (prevPlayId_ != -1)    am->Stop(prevPlayId_);
				if (currentPlayId_ != -1) am->Stop(currentPlayId_);
			}
			prevPlayId_ = -1;
			currentPlayId_ = -1;
			cachedSoundId_ = -1;
			timer_ = 0.0f;
			prevTailTimer_ = 0.0f;
			lastVolume_ = -1.0f;
			active_ = false;
			currentPath_.clear();
		}

		[[nodiscard]] bool IsActive() const { return active_; }

	private:
		std::string currentPath_;
		int32_t cachedSoundId_ = -1;
		int32_t currentPlayId_ = -1;
		int32_t prevPlayId_ = -1;
		float cachedCategoryScale_ = 1.0f;
		float timer_ = 0.0f;
		float prevTailTimer_ = 0.0f;
		float lastVolume_ = -1.0f;
		bool active_ = false;
	};
}
