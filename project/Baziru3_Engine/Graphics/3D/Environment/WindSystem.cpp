#include "WindSystem.h"

#include <cmath>
#include <cstring>

namespace BaziruEngine::Environment
{

void WindSystem::Initialize(DirectXCom* dxCommon)
{
    dxCommon_ = dxCommon;
    if (!dxCommon_)
    {
        return;
    }

    params_.direction = { 0.85f, 0.0f, 0.52f };
    SetDirection(params_.direction);
    params_.time = 0.0f;
    params_.windSpeed = 2.4f;
    params_.gustStrength = 0.65f;
    params_.gustFrequency = 0.12f;
    params_.turbulence = 0.35f;
    params_.repulsorStrength = 1.0f;
    params_.padding = { 0.0f, 0.0f, 0.0f };
    ClearRepulsors();

    const size_t alignedSize = (sizeof(WindForGPU) + 255) & ~static_cast<size_t>(255);
    constantBuffer_ = dxCommon_->CreateBufferResource(dxCommon_->GetDevice().Get(), alignedSize);
    constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData_));
    if (mappedData_)
    {
        std::memcpy(mappedData_, &params_, sizeof(WindForGPU));
    }
}

void WindSystem::Finalize()
{
    if (constantBuffer_)
    {
        constantBuffer_->Unmap(0, nullptr);
        mappedData_ = nullptr;
        constantBuffer_.Reset();
    }
    dxCommon_ = nullptr;
}

void WindSystem::Update(float deltaTime)
{
    params_.time += deltaTime;
    if (mappedData_)
    {
        std::memcpy(mappedData_, &params_, sizeof(WindForGPU));
    }
}

void WindSystem::SetDirection(const Vector3& dir)
{
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 1e-4f)
    {
        params_.direction = { dir.x / len, dir.y / len, dir.z / len };
    }
    else
    {
        params_.direction = { 1.0f, 0.0f, 0.0f };
    }
}

void WindSystem::SetRepulsor(uint32_t index, const Vector3& worldPos, float radius)
{
    if (index < kMaxRepulsors)
    {
        params_.repulsors[index] = { worldPos.x, worldPos.y, worldPos.z, radius };
    }
}

void WindSystem::ClearRepulsors()
{
    for (uint32_t i = 0; i < kMaxRepulsors; ++i)
    {
        params_.repulsors[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
}

} // namespace BaziruEngine::Environment
