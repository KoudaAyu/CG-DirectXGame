#include "Ring.h"
#include "BufferUtil.h"

#include <cmath>
#include <cstring>
#include <numbers>

void Ring::Initialize(DirectXCom* dxCommon, uint32_t ringDivide, float outerRadius, float innerRadius)
{
    Finalize();

    dxCommon_ = dxCommon;
    if (!dxCommon_)
    {
        return;
    }

    auto verts = CreateMesh(ringDivide, outerRadius, innerRadius);
    vertexBuffer_ = BufferUtil::CreateVertexBuffer(dxCommon_, verts, vertexBufferView_);
    vertexCount_ = static_cast<uint32_t>(verts.size());
}

void Ring::Finalize()
{
    vertexBuffer_.Reset();
    vertexBufferView_ = {};
    vertexCount_ = 0;
    dxCommon_ = nullptr;
}

std::vector<Ring::Vertex> Ring::CreateMesh(uint32_t ringDivide, float outerRadius, float innerRadius) const
{
    std::vector<Vertex> verts;
    verts.reserve(ringDivide * 6);

    const float twoPi = std::numbers::pi_v<float> * 2.0f;
    for (uint32_t i = 0; i < ringDivide; ++i)
    {
        float a = float(i) * twoPi / float(ringDivide);
        float b = float(i + 1) * twoPi / float(ringDivide);
        float sinA = std::sin(a);
        float cosA = std::cos(a);
        float sinB = std::sin(b);
        float cosB = std::cos(b);
        float u = float(i) / float(ringDivide);
        float uNext = float(i + 1) / float(ringDivide);

        Vector3 vOuterA3 = { -sinA * outerRadius, cosA * outerRadius, 0.0f };
        Vector3 vOuterB3 = { -sinB * outerRadius, cosB * outerRadius, 0.0f };
        Vector3 vInnerA3 = { -sinA * innerRadius, cosA * innerRadius, 0.0f };
        Vector3 vInnerB3 = { -sinB * innerRadius, cosB * innerRadius, 0.0f };
        Vector3 normal = { 0.0f, 0.0f, 1.0f };

        Vector4 vOuterA = { vOuterA3.x, vOuterA3.y, vOuterA3.z, 1.0f };
        Vector4 vOuterB = { vOuterB3.x, vOuterB3.y, vOuterB3.z, 1.0f };
        Vector4 vInnerA = { vInnerA3.x, vInnerA3.y, vInnerA3.z, 1.0f };
        Vector4 vInnerB = { vInnerB3.x, vInnerB3.y, vInnerB3.z, 1.0f };

        verts.push_back({ vOuterA, { u, 1.0f }, normal });
        verts.push_back({ vOuterB, { uNext, 1.0f }, normal });
        verts.push_back({ vInnerA, { u, 0.0f }, normal });

        verts.push_back({ vOuterB, { uNext, 1.0f }, normal });
        verts.push_back({ vInnerB, { uNext, 0.0f }, normal });
        verts.push_back({ vInnerA, { u, 0.0f }, normal });
    }

    return verts;
}


