#include "SlimeMesh.h"
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
}

Object3d::ModelData SlimeMesh::GenerateSphere(uint32_t sliceCount, uint32_t stackCount, float radius)
{
    Object3d::ModelData modelData;

    // --- 頂点の生成 ---
    // 北極点
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, 1.0f, 0.0f };
        v.texcoord = { 0.5f, 0.0f };
        modelData.vertices.push_back(v);
    }

    // 中間リング
    for (uint32_t i = 1; i < stackCount; ++i)
    {
        float phi = kPi * static_cast<float>(i) / static_cast<float>(stackCount);
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t j = 0; j <= sliceCount; ++j)
        {
            float theta = 2.0f * kPi * static_cast<float>(j) / static_cast<float>(sliceCount);
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            Sprite::VertexData v{};
            float nx = sinPhi * cosTheta;
            float ny = cosPhi;
            float nz = sinPhi * sinTheta;

            v.position = { nx * radius, ny * radius, nz * radius, 1.0f };
            v.normal   = { nx, ny, nz };
            v.texcoord = {
                static_cast<float>(j) / static_cast<float>(sliceCount),
                static_cast<float>(i) / static_cast<float>(stackCount)
            };
            modelData.vertices.push_back(v);
        }
    }

    // 南極点
    {
        Sprite::VertexData v{};
        v.position = { 0.0f, -radius, 0.0f, 1.0f };
        v.normal   = { 0.0f, -1.0f, 0.0f };
        v.texcoord = { 0.5f, 1.0f };
        modelData.vertices.push_back(v);
    }

    // --- インデックスの生成 ---
    // 北極キャップ（三角形ファン）
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        modelData.indices.push_back(0);
        modelData.indices.push_back(1 + j + 1);
        modelData.indices.push_back(1 + j);
    }

    // 中間リング（クワッド→2三角形）
    uint32_t ringVertexCount = sliceCount + 1;
    for (uint32_t i = 0; i < stackCount - 2; ++i)
    {
        uint32_t ringStart = 1 + i * ringVertexCount;
        uint32_t nextRingStart = ringStart + ringVertexCount;

        for (uint32_t j = 0; j < sliceCount; ++j)
        {
            // 上三角形
            modelData.indices.push_back(ringStart + j);
            modelData.indices.push_back(nextRingStart + j);
            modelData.indices.push_back(nextRingStart + j + 1);

            // 下三角形
            modelData.indices.push_back(ringStart + j);
            modelData.indices.push_back(nextRingStart + j + 1);
            modelData.indices.push_back(ringStart + j + 1);
        }
    }

    // 南極キャップ（三角形ファン）
    uint32_t southPoleIndex = static_cast<uint32_t>(modelData.vertices.size()) - 1;
    uint32_t lastRingStart = 1 + (stackCount - 2) * ringVertexCount;
    for (uint32_t j = 0; j < sliceCount; ++j)
    {
        modelData.indices.push_back(southPoleIndex);
        modelData.indices.push_back(lastRingStart + j);
        modelData.indices.push_back(lastRingStart + j + 1);
    }

    // バウンディング半径の設定
    modelData.boundingRadius = radius;

    return modelData;
}
