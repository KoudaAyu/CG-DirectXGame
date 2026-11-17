#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <array>
#include <cstdint>

#include "DirectXCom.h"
#include "Vector.h"

namespace BaziruEngine::Environment
{

struct WindForGPU
{
    Vector3 direction;        // 正規化された風向き
    float   time;             // 累積時間(秒)
    float   windSpeed;        // 基本風速
    float   gustStrength;     // 風のうねり（ガスト波）強度
    float   gustFrequency;    // 風のうねり空間周波数
    float   turbulence;       // 葉先・穂先の高周波フラッター強度
    Vector4 repulsors[4];     // xyz: 押し分け中心ワールド座標, w: 半径 (<=0で無効)
    float   repulsorStrength; // 押し分け強度
    Vector3 padding;
};

/// <summary>
/// エンジン共通の風シミュレーション＆草木インタラクション（押し分け）管理システム
/// </summary>
class WindSystem
{
public:
    static constexpr uint32_t kMaxRepulsors = 4;

    WindSystem() = default;
    ~WindSystem() { Finalize(); }

    void Initialize(DirectXCom* dxCommon);
    void Finalize();
    void Update(float deltaTime);

    void SetDirection(const Vector3& dir);
    const Vector3& GetDirection() const { return params_.direction; }

    void SetWindSpeed(float speed) { params_.windSpeed = speed; }
    float GetWindSpeed() const { return params_.windSpeed; }

    void SetGustStrength(float strength) { params_.gustStrength = strength; }
    float GetGustStrength() const { return params_.gustStrength; }

    void SetGustFrequency(float freq) { params_.gustFrequency = freq; }
    float GetGustFrequency() const { return params_.gustFrequency; }

    void SetTurbulence(float turb) { params_.turbulence = turb; }
    float GetTurbulence() const { return params_.turbulence; }

    void SetRepulsorStrength(float strength) { params_.repulsorStrength = strength; }
    float GetRepulsorStrength() const { return params_.repulsorStrength; }

    void SetRepulsor(uint32_t index, const Vector3& worldPos, float radius);
    void ClearRepulsors();
    Vector4 GetRepulsor(uint32_t index) const { return (index < kMaxRepulsors) ? params_.repulsors[index] : Vector4{0, 0, 0, 0}; }

    float GetTime() const { return params_.time; }

    D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const
    {
        return constantBuffer_ ? constantBuffer_->GetGPUVirtualAddress() : 0;
    }

private:
    DirectXCom* dxCommon_ = nullptr;
    WindForGPU params_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    WindForGPU* mappedData_ = nullptr;
};

} // namespace BaziruEngine::Environment
