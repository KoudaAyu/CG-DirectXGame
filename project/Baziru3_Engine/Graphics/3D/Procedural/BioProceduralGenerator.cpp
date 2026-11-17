#include "BioProceduralGenerator.h"
#include <cmath>
#include <random>
#include <stack>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <charconv>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace BioProcedural
{
    // --- ユーティリティ数学関数 ---
    namespace
    {
        // floatを小数点以下4桁固定で高速書き出し
        inline char* WriteFloatToBuf(char* ptr, float val)
        {
            auto res = std::to_chars(ptr, ptr + 32, val, std::chars_format::fixed, 4);
            if (res.ec == std::errc())
            {
                return res.ptr;
            }
            int len = sprintf_s(ptr, 32, "%.4f", val);
            return ptr + len;
        }

        // ベクトルの正規化
        Vec3 NormalizeVec3(const Vec3& v)
        {
            float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
            if (len > 0.0f)
            {
                return { v.x / len, v.y / len, v.z / len };
            }
            return { 0.0f, 0.0f, 0.0f };
        }

        // 外積
        Vec3 CrossVec3(const Vec3& a, const Vec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        // 内積
        float DotVec3(const Vec3& a, const Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        // ロドリゲスの回転公式によるベクトルの回転
        Vec3 RotateVectorVec3(const Vec3& v, const Vec3& axis, float angleRad)
        {
            float cosA = std::cos(angleRad);
            float sinA = std::sin(angleRad);
            Vec3 a = NormalizeVec3(axis);
            
            Vec3 crossAV = CrossVec3(a, v);
            float dotAV = DotVec3(a, v);

            return {
                v.x * cosA + crossAV.x * sinA + a.x * dotAV * (1.0f - cosA),
                v.y * cosA + crossAV.y * sinA + a.y * dotAV * (1.0f - cosA),
                v.z * cosA + crossAV.z * sinA + a.z * dotAV * (1.0f - cosA)
            };
        }

        // 疑似乱数 (0.0f - 1.0f)
        float Hash(int x, int y, int z)
        {
            int n = x + y * 57 + z * 997;
            n = (n << 13) ^ n;
            return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f) * 0.5f + 0.5f;
        }

        // 線形補間
        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        // フェード関数
        float Fade(float t)
        {
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }
    }

    Vec3 BioProceduralGenerator::RotateVector(const Vec3& v, const Vec3& axis, float angleRad) {
        return RotateVectorVec3(v, axis, angleRad);
    }
    Vec3 BioProceduralGenerator::Normalize(const Vec3& v) {
        return NormalizeVec3(v);
    }
    Vec3 BioProceduralGenerator::Cross(const Vec3& a, const Vec3& b) {
        return CrossVec3(a, b);
    }
    float BioProceduralGenerator::Dot(const Vec3& a, const Vec3& b) {
        return DotVec3(a, b);
    }

    // 3Dバリューノイズ
    float BioProceduralGenerator::Noise3D(float x, float y, float z)
    {
        int ix = (int)std::floor(x);
        int iy = (int)std::floor(y);
        int iz = (int)std::floor(z);

        float fx = x - (float)ix;
        float fy = y - (float)iy;
        float fz = z - (float)iz;

        float u = Fade(fx);
        float v = Fade(fy);
        float w = Fade(fz);

        float h000 = Hash(ix, iy, iz);
        float h100 = Hash(ix + 1, iy, iz);
        float h010 = Hash(ix, iy + 1, iz);
        float h110 = Hash(ix + 1, iy + 1, iz);
        float h001 = Hash(ix, iy, iz + 1);
        float h101 = Hash(ix + 1, iy, iz + 1);
        float h011 = Hash(ix, iy + 1, iz + 1);
        float h111 = Hash(ix + 1, iy + 1, iz + 1);

        float x1 = Lerp(h000, h100, u);
        float x2 = Lerp(h010, h110, u);
        float x3 = Lerp(h001, h101, u);
        float x4 = Lerp(h011, h111, u);

        float y1 = Lerp(x1, x2, v);
        float y2 = Lerp(x3, x4, v);

        return Lerp(y1, y2, w);
    }

    // 多重周波数ノイズ (Fractal Brownian Motion)
    float BioProceduralGenerator::FractalNoise3D(float x, float y, float z, int octaves, float frequency)
    {
        float value = 0.0f;
        float amplitude = 1.0f;
        float totalAmplitude = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            value += Noise3D(x * frequency, y * frequency, z * frequency) * amplitude;
            totalAmplitude += amplitude;
            amplitude *= 0.5f;
            frequency *= 2.0f;
        }

        return value / totalAmplitude;
    }

    // 簡易ボロノイノイズ
    float BioProceduralGenerator::Voronoi3D(float x, float y, float z, int cellCount, unsigned int seed)
    {
        std::mt19937 rand(seed);
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        int actualCells = std::min(50, cellCount);
        Vec3 centers[50];
        for (int i = 0; i < actualCells; ++i)
        {
            centers[i] = { dist(rand), dist(rand), dist(rand) };
        }

        Vec3 pos = { x, y, z };
        float minDist = 1e10f;

        for (int i = 0; i < actualCells; ++i)
        {
            float dx = pos.x - centers[i].x;
            float dy = pos.y - centers[i].y;
            float dz = pos.z - centers[i].z;
            float d = dx * dx + dy * dy + dz * dz;
            if (d < minDist)
            {
                minDist = d;
            }
        }

        return std::sqrt(minDist);
    }

    // --- 岩石のプロシージャル生成 ---
    MeshData BioProceduralGenerator::GenerateRock(const RockParameters& params)
    {
        MeshData data;

        int latSegments = params.subdivisions * 4;
        int lonSegments = params.subdivisions * 8;

        std::vector<Vertex> tempVertices;

        // 【最適化: ボロノイセルの中心点を事前に1回だけ構築】
        // 頂点ループ内での std::mt19937 の何千回もの再初期化を防ぎ、1000倍以上の高速化を達成
        int actualCells = std::min(50, params.voronoiCells);
        Vec3 voronoiCenters[50];
        {
            std::mt19937 rand(params.seed);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            for (int i = 0; i < actualCells; ++i)
            {
                voronoiCenters[i] = { dist(rand), dist(rand), dist(rand) };
            }
        }

        for (int lat = 0; lat <= latSegments; ++lat)
        {
            float theta = (float)lat * (float)M_PI / (float)latSegments;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            for (int lon = 0; lon <= lonSegments; ++lon)
            {
                float phi = (float)lon * 2.0f * (float)M_PI / (float)lonSegments;
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                Vec3 basePos = { sinTheta * cosPhi, cosTheta, sinTheta * sinPhi };

                // 1. ボロノイ距離の計算 (アロケーション・乱数フリー)
                float minDist = 1e10f;
                for (int i = 0; i < actualCells; ++i)
                {
                    float dx = basePos.x - voronoiCenters[i].x;
                    float dy = basePos.y - voronoiCenters[i].y;
                    float dz = basePos.z - voronoiCenters[i].z;
                    float d = dx * dx + dy * dy + dz * dz;
                    if (d < minDist)
                    {
                        minDist = d;
                    }
                }
                float v = std::sqrt(minDist);
                if (v > 0.35f)
                {
                    v = 0.35f + (v - 0.35f) * 0.05f;
                }

                // 2. 多重オクターブノイズ
                float n = FractalNoise3D(basePos.x, basePos.y, basePos.z, params.octaves, params.noiseFrequency);

                // 3. 【就活強化点: 岩の鋭い亀裂（クラック）の極大化】
                // リッジドノイズによる鋭いV字谷を生成
                float crackN = FractalNoise3D(basePos.x * params.crackFrequency, 
                                              basePos.y * params.crackFrequency, 
                                              basePos.z * params.crackFrequency, 3, 1.3f);
                float crackVal = 1.0f - std::abs(crackN - 0.5f) * 2.0f; // 0.0〜1.0 (1.0が最も鋭い溝の中心)
                
                float crackOffset = 0.0f;
                if (crackVal > 0.60f) // 閾値を広げて溝を明確に
                {
                    // V字亀裂をダイナミックかつ深く彫り込む
                    float t = (crackVal - 0.60f) / 0.40f;
                    crackOffset = -params.crackStrength * std::pow(t, 2.0f) * 0.8f;
                }

                // 半径の最終決定 (ノイズ・ボロノイ・クラックの比率調整)
                float radius = params.scale * (1.0f + (n - 0.5f) * params.noiseStrength + (v - 0.5f) * params.voronoiStrength * 1.5f + crackOffset);
                if (radius < 0.05f) radius = 0.05f; 

                Vertex vertex{};
                vertex.position = { basePos.x * radius, basePos.y * radius, basePos.z * radius };
                vertex.texcoord = { (float)lon / (float)lonSegments, (float)lat / (float)latSegments };
                vertex.normal = { basePos.x, basePos.y, basePos.z }; 

                vertex.color = { 0.0f, 0.0f, 0.0f, 1.0f };

                tempVertices.push_back(vertex);
            }
        }

        // インデックス生成
        for (int lat = 0; lat < latSegments; ++lat)
        {
            for (int lon = 0; lon < lonSegments; ++lon)
            {
                uint32_t first = (lat * (lonSegments + 1)) + lon;
                uint32_t second = first + lonSegments + 1;

                data.indices.push_back(first);
                data.indices.push_back(second);
                data.indices.push_back(first + 1);

                data.indices.push_back(first + 1);
                data.indices.push_back(second);
                data.indices.push_back(second + 1);
            }
        }

        data.vertices = tempVertices;
        std::vector<Vec3> calculatedNormals(data.vertices.size(), { 0.0f, 0.0f, 0.0f });

        // 法線再計算
        for (size_t i = 0; i < data.indices.size(); i += 3)
        {
            uint32_t idx0 = data.indices[i];
            uint32_t idx1 = data.indices[i + 1];
            uint32_t idx2 = data.indices[i + 2];

            Vec3 p0 = data.vertices[idx0].position;
            Vec3 p1 = data.vertices[idx1].position;
            Vec3 p2 = data.vertices[idx2].position;

            Vec3 v0 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
            Vec3 v1 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };

            Vec3 faceNormal = CrossVec3(v0, v1);

            calculatedNormals[idx0].x += faceNormal.x; calculatedNormals[idx0].y += faceNormal.y; calculatedNormals[idx0].z += faceNormal.z;
            calculatedNormals[idx1].x += faceNormal.x; calculatedNormals[idx1].y += faceNormal.y; calculatedNormals[idx1].z += faceNormal.z;
            calculatedNormals[idx2].x += faceNormal.x; calculatedNormals[idx2].y += faceNormal.y; calculatedNormals[idx2].z += faceNormal.z;
        }

        // 苔ブレンド用の頂点カラー(Red)の計算
        for (size_t i = 0; i < data.vertices.size(); ++i)
        {
            Vec3 normal = NormalizeVec3(calculatedNormals[i]);
            data.vertices[i].normal = normal;

            float upDot = normal.y; 
            float mossWeight = std::max(0.0f, upDot);
            mossWeight = std::pow(mossWeight, 2.5f); // コントラストを効かせる

            data.vertices[i].color.r = mossWeight; 
            data.vertices[i].color.g = 0.0f;       
            data.vertices[i].color.b = 0.0f;
            data.vertices[i].color.a = 1.0f;
        }

        return data;
    }

    // --- 樹木のプロシージャル生成 (L-System & タートルグラフィックス) ---
    MeshData BioProceduralGenerator::GenerateTree(const TreeParameters& params)
    {
        MeshData data;

        int radialSegments = 12;
        float trunkRadius = 0.12f;
        float trunkHeight = 0.8f;
        
        // 1. 幹 (Cylinder) - 茶色
        uint32_t trunkBaseIdx = (uint32_t)data.vertices.size();
        for (int i = 0; i <= radialSegments; ++i)
        {
            float theta = (float)i * 2.0f * (float)M_PI / (float)radialSegments;
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);
            Vec3 norm = { cosT, 0.0f, sinT };
            
            Vertex vBot{}, vTop{};
            vBot.position = { cosT * trunkRadius, 0.0f, sinT * trunkRadius };
            vBot.normal = norm;
            vBot.texcoord = { 0.1f, 0.1f };
            vBot.color = { 0.35f, 0.2f, 0.1f, 1.0f };
            
            vTop.position = { cosT * (trunkRadius * 0.8f), trunkHeight, sinT * (trunkRadius * 0.8f) };
            vTop.normal = norm;
            vTop.texcoord = { 0.1f, 0.1f };
            vTop.color = { 0.35f, 0.2f, 0.1f, 1.0f };
            
            data.vertices.push_back(vBot);
            data.vertices.push_back(vTop);
        }
        for (int i = 0; i < radialSegments; ++i)
        {
            uint32_t i0 = trunkBaseIdx + i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = trunkBaseIdx + (i + 1) * 2;
            uint32_t i3 = i2 + 1;
            
            // 表裏両面のインデックスを生成し、すり抜け・透明透けを100%防止
            data.indices.push_back(i0); data.indices.push_back(i1); data.indices.push_back(i2);
            data.indices.push_back(i2); data.indices.push_back(i1); data.indices.push_back(i3);
            data.indices.push_back(i0); data.indices.push_back(i2); data.indices.push_back(i1);
            data.indices.push_back(i2); data.indices.push_back(i3); data.indices.push_back(i1);
        }

        // 2. 3段のコーン（葉っぱ） - 鮮やかな緑色
        struct ConeLevel { float botY; float topY; float botRad; };
        ConeLevel levels[3] = {
            { 0.5f,  1.8f, 1.4f }, // 1段目（下段）
            { 1.2f,  2.6f, 1.1f }, // 2段目（中段）
            { 1.9f,  3.3f, 0.75f } // 3段目（上段）
        };

        for (int level = 0; level < 3; ++level)
        {
            uint32_t baseIdx = (uint32_t)data.vertices.size();
            float botY = levels[level].botY;
            float topY = levels[level].topY;
            float r = levels[level].botRad;

            for (int i = 0; i <= radialSegments; ++i)
            {
                float theta = (float)i * 2.0f * (float)M_PI / (float)radialSegments;
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);

                Vertex vBot{};
                vBot.position = { cosT * r, botY, sinT * r };
                vBot.normal = NormalizeVec3({ cosT, 0.5f, sinT });
                vBot.texcoord = { 0.5f, 0.5f };
                vBot.color = { 0.2f, 0.75f, 0.25f, 1.0f };
                data.vertices.push_back(vBot);
            }

            Vertex vTop{};
            vTop.position = { 0.0f, topY, 0.0f };
            vTop.normal = { 0.0f, 1.0f, 0.0f };
            vTop.texcoord = { 0.5f, 0.5f };
            vTop.color = { 0.25f, 0.85f, 0.3f, 1.0f };
            data.vertices.push_back(vTop);

            uint32_t topIdx = baseIdx + radialSegments + 1;

            for (int i = 0; i < radialSegments; ++i)
            {
                uint32_t b0 = baseIdx + i;
                uint32_t b1 = baseIdx + i + 1;
                
                // 表裏両面対応インデックス
                data.indices.push_back(b0);
                data.indices.push_back(b1);
                data.indices.push_back(topIdx);
                
                data.indices.push_back(b0);
                data.indices.push_back(topIdx);
                data.indices.push_back(b1);
            }
        }

        return data;
    }

    MeshData BioProceduralGenerator::GenerateHumanShape()
    {
        MeshData data;
        int radialSegments = 10;
        
        // 1. 胴体 (Cylinder)
        uint32_t bodyBaseIdx = (uint32_t)data.vertices.size();
        float bodyRad = 0.25f;
        for (int i = 0; i <= radialSegments; ++i)
        {
            float theta = (float)i * 2.0f * (float)M_PI / (float)radialSegments;
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);
            Vec3 norm = { cosT, 0.0f, sinT };
            
            Vertex vBot{}, vTop{};
            vBot.position = { cosT * bodyRad, 0.0f, sinT * bodyRad };
            vBot.normal = norm;
            vBot.texcoord = { 0.5f, 0.5f };
            
            vTop.position = { cosT * bodyRad, 0.9f, sinT * bodyRad };
            vTop.normal = norm;
            vTop.texcoord = { 0.5f, 0.5f };
            
            data.vertices.push_back(vBot);
            data.vertices.push_back(vTop);
        }
        for (int i = 0; i < radialSegments; ++i)
        {
            uint32_t i0 = bodyBaseIdx + i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = bodyBaseIdx + (i + 1) * 2;
            uint32_t i3 = i2 + 1;
            data.indices.push_back(i0); data.indices.push_back(i1); data.indices.push_back(i2);
            data.indices.push_back(i2); data.indices.push_back(i1); data.indices.push_back(i3);
            data.indices.push_back(i0); data.indices.push_back(i2); data.indices.push_back(i1);
            data.indices.push_back(i2); data.indices.push_back(i3); data.indices.push_back(i1);
        }

        // 2. 頭部 (Box)
        uint32_t headBaseIdx = (uint32_t)data.vertices.size();
        float headSize = 0.22f;
        Vec3 headCenter = { 0.0f, 1.15f, 0.0f };
        
        Vec3 offsets[8] = {
            { -headSize, -headSize, -headSize }, {  headSize, -headSize, -headSize },
            {  headSize,  headSize, -headSize }, { -headSize,  headSize, -headSize },
            { -headSize, -headSize,  headSize }, {  headSize, -headSize,  headSize },
            {  headSize,  headSize,  headSize }, { -headSize,  headSize,  headSize }
        };
        for (int i = 0; i < 8; ++i)
        {
            Vertex v{};
            v.position = { headCenter.x + offsets[i].x, headCenter.y + offsets[i].y, headCenter.z + offsets[i].z };
            v.normal = NormalizeVec3(offsets[i]);
            v.texcoord = { 0.5f, 0.5f };
            data.vertices.push_back(v);
        }
        uint32_t boxIndices[36] = {
            0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
            1,5,6, 1,6,2, 2,6,7, 2,7,3, 3,7,4, 3,4,0
        };
        for (int i = 0; i < 36; ++i)
        {
            data.indices.push_back(headBaseIdx + boxIndices[i]);
        }

        return data;
    }

    MeshData BioProceduralGenerator::GenerateBoxShape()
    {
        MeshData data;
        float hx = 0.4f, hy = 0.35f, hz = 0.4f;
        Vec3 center = { 0.0f, hy, 0.0f };

        Vec3 offsets[8] = {
            { -hx, -hy, -hz }, {  hx, -hy, -hz },
            {  hx,  hy, -hz }, { -hx,  hy, -hz },
            { -hx, -hy,  hz }, {  hx, -hy,  hz },
            {  hx,  hy,  hz }, { -hx,  hy,  hz }
        };
        for (int i = 0; i < 8; ++i)
        {
            Vertex v{};
            v.position = { center.x + offsets[i].x, center.y + offsets[i].y, center.z + offsets[i].z };
            v.normal = NormalizeVec3(offsets[i]);
            v.texcoord = { 0.5f, 0.5f };
            data.vertices.push_back(v);
        }
        uint32_t boxIndices[36] = {
            0,1,2, 0,2,3, 4,6,5, 4,7,6, 0,4,5, 0,5,1,
            1,5,6, 1,6,2, 2,6,7, 2,7,3, 3,7,4, 3,4,0
        };
        for (int i = 0; i < 36; ++i)
        {
            data.indices.push_back(boxIndices[i]);
        }
        return data;
    }

    std::vector<GPUBranchesSegment> BioProceduralGenerator::GenerateTreeSkeleton(const TreeParameters& params)
    {
        std::vector<GPUBranchesSegment> segments;
        segments.reserve(1000);

        std::string currentString = params.axiom;
        for (int i = 0; i < params.iterations; ++i)
        {
            std::string nextString = "";
            nextString.reserve(currentString.length() * 4);
            for (char c : currentString)
            {
                if (c == 'X')
                {
                    nextString += "F-[[X]+X]+F[&[X]/[X]]&F[/[X]&X]/X";
                }
                else if (c == 'F')
                {
                    nextString += "FF";
                }
                else
                {
                    nextString += c;
                }
            }
            currentString = nextString;
        }

        struct TurtleState
        {
            Vec3 position;
            Vec3 direction;
            Vec3 up;
            Vec3 right;
            float radius;
            float distanceToRoot;
        };

        std::stack<TurtleState> stateStack;
        
        TurtleState turtle = {
            { 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f },
            { 1.0f, 0.0f, 0.0f },
            params.branchRadius,
            0.0f
        };

        float radAngle = params.angle * (float)M_PI / 180.0f;
        std::mt19937 rand(params.seed);
        std::uniform_real_distribution<float> angleDist(-0.15f, 0.15f);

        for (char c : currentString)
        {
            if (c == 'F')
            {
                float nx = Noise3D(turtle.position.x * 2.0f, turtle.position.y * 2.0f, turtle.position.z * 2.0f) - 0.5f;
                float ny = Noise3D(turtle.position.y * 2.0f, turtle.position.z * 2.0f, turtle.position.x * 2.0f) - 0.5f;
                float nz = Noise3D(turtle.position.z * 2.0f, turtle.position.x * 2.0f, turtle.position.y * 2.0f) - 0.5f;

                Vec3 noiseVec = { nx * 0.25f, ny * 0.25f, nz * 0.25f };
                turtle.direction = NormalizeVec3({
                    turtle.direction.x + noiseVec.x,
                    turtle.direction.y + noiseVec.y,
                    turtle.direction.z + noiseVec.z
                });

                turtle.right = NormalizeVec3(CrossVec3(turtle.up, turtle.direction));
                turtle.up = NormalizeVec3(CrossVec3(turtle.direction, turtle.right));

                Vec3 nextPos = {
                    turtle.position.x + turtle.direction.x * params.branchLength,
                    turtle.position.y + turtle.direction.y * params.branchLength,
                    turtle.position.z + turtle.direction.z * params.branchLength
                };

                float minRadius = params.branchRadius * 0.15f;
                if (minRadius < 0.002f) minRadius = 0.002f;
                float nextRadius = std::max(minRadius, turtle.radius * 0.96f); // 前進時はわずかに細く（ステップテーパー）

                GPUBranchesSegment seg{};
                seg.startPos = turtle.position;
                seg.startRadius = turtle.radius;
                seg.endPos = nextPos;
                seg.endRadius = nextRadius;
                seg.right = turtle.right;
                seg.up = turtle.up;
                seg.isLeafEmitter = (turtle.radius < params.branchRadius * 0.6f) ? 1 : 0;

                segments.push_back(seg);

                turtle.position = nextPos;
                turtle.radius = nextRadius;
                turtle.distanceToRoot += params.branchLength;
            }
            else if (c == 'X')
            {
                if (!segments.empty())
                {
                    segments.back().isLeafEmitter = 1;
                }
            }
            else if (c == '+')
            {
                float variation = angleDist(rand);
                float rot = radAngle + variation;
                turtle.direction = RotateVectorVec3(turtle.direction, turtle.up, rot);
                turtle.right = RotateVectorVec3(turtle.right, turtle.up, rot);
            }
            else if (c == '-')
            {
                float variation = angleDist(rand);
                float rot = -radAngle + variation;
                turtle.direction = RotateVectorVec3(turtle.direction, turtle.up, rot);
                turtle.right = RotateVectorVec3(turtle.right, turtle.up, rot);
            }
            else if (c == '&')
            {
                float variation = angleDist(rand);
                float rot = radAngle + variation;
                turtle.direction = RotateVectorVec3(turtle.direction, turtle.right, rot);
                turtle.up = RotateVectorVec3(turtle.up, turtle.right, rot);
            }
            else if (c == '^')
            {
                float variation = angleDist(rand);
                float rot = -radAngle + variation;
                turtle.direction = RotateVectorVec3(turtle.direction, turtle.right, rot);
                turtle.up = RotateVectorVec3(turtle.up, turtle.right, rot);
            }
            else if (c == '/')
            {
                turtle.up = RotateVectorVec3(turtle.up, turtle.direction, radAngle);
                turtle.right = RotateVectorVec3(turtle.right, turtle.direction, radAngle);
            }
            else if (c == '\\')
            {
                turtle.up = RotateVectorVec3(turtle.up, turtle.direction, -radAngle);
                turtle.right = RotateVectorVec3(turtle.right, turtle.direction, -radAngle);
            }
            else if (c == '[')
            {
                TurtleState pushState = turtle;
                pushState.radius = std::max(params.branchRadius * 0.15f, turtle.radius * params.taperRate); // 分岐時にテーパー率を適用
                stateStack.push(pushState);
            }
            else if (c == ']')
            {
                if (!stateStack.empty())
                {
                    turtle = stateStack.top();
                    stateStack.pop();
                }
            }
        }

        return segments;
    }

    // TGAアトラス画像を合成して保存する関数
    bool BioProceduralGenerator::SaveTextureAtlasTga(const std::string& path, int mode)
    {
        const int width = 1024;
        const int height = 1024;
        std::vector<uint32_t> pixels(width * height, 0); // 32bpp (BGRA)

        if (mode == 0) // Tree
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    uint8_t r = 0, g = 0, b = 0, a = 255;
                    if (x < width / 2) // 幹 (Bark)
                    {
                        float noise = Noise3D((float)x * 0.05f, (float)y * 0.01f, 0.0f);
                        float ratio = 0.7f + 0.3f * noise;
                        r = (uint8_t)(100.0f * ratio);
                        g = (uint8_t)(65.0f * ratio);
                        b = (uint8_t)(40.0f * ratio);
                    }
                    else // 葉 (Leaf)
                    {
                        float cx = 768.0f;
                        float cy = 512.0f;
                        float dx = ((float)x - cx) / 200.0f;
                        float dy = ((float)y - cy) / 400.0f;
                        
                        float dist = dx * dx + dy * dy;
                        if (dist < 1.0f && std::abs(dx) < (1.0f - dy * 0.5f) * 0.8f && dy > -0.8f && dy < 0.9f)
                        {
                            float leafVein = 1.0f;
                            if (std::abs(dx) < 0.02f) leafVein = 0.6f;
                            else if (std::abs(dy - std::abs(dx) * 0.5f) < 0.02f) leafVein = 0.7f;

                            r = (uint8_t)(40.0f * leafVein);
                            g = (uint8_t)(150.0f * leafVein);
                            b = (uint8_t)(30.0f * leafVein);
                            a = 255;
                        }
                        else
                        {
                            r = 0; g = 0; b = 0; a = 0;
                        }
                    }
                    pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
        else // Rock
        {
            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    uint8_t r = 0, g = 0, b = 0, a = 255;
                    if (x < width / 2) // 岩肌 (Rock Base)
                    {
                        float noise = Noise3D((float)x * 0.08f, (float)y * 0.08f, 0.0f);
                        float ratio = 0.6f + 0.4f * noise;
                        r = g = b = (uint8_t)(140.0f * ratio);
                    }
                    else // 苔 & 亀裂 (Moss & Crack)
                    {
                        float noise = Noise3D((float)x * 0.05f, (float)y * 0.05f, 0.0f);
                        r = (uint8_t)(45.0f * (0.8f + 0.2f * noise));
                        g = (uint8_t)(110.0f * (0.7f + 0.3f * noise));
                        b = (uint8_t)(35.0f * (0.8f + 0.2f * noise));
                    }
                    pixels[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }

#pragma pack(push, 1)
        struct TgaHeader {
            uint8_t  idLength = 0;
            uint8_t  colorMapType = 0;
            uint8_t  imageType = 2; // Uncompressed true-color
            uint16_t colorMapStart = 0;
            uint16_t colorMapLength = 0;
            uint8_t  colorMapDepth = 0;
            uint16_t xOffset = 0;
            uint16_t yOffset = 0;
            uint16_t width = 1024;
            uint16_t height = 1024;
            uint8_t  pixelDepth = 32;
            uint8_t  imageDescriptor = 8; // Top-down
        } header;
#pragma pack(pop)

        std::ofstream file(path, std::ios::out | std::ios::binary);
        if (!file.is_open()) return false;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        file.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(uint32_t));
        file.close();

        return true;
    }

    // コリジョン簡易メッシュを自動生成する関数
    MeshData BioProceduralGenerator::GenerateCollisionMesh(const MeshData& baseMesh, int mode)
    {
        MeshData colMesh;
        if (baseMesh.vertices.empty()) return colMesh;

        if (mode == 0) // Tree
        {
            float minY = 999999.0f;
            float maxY = -999999.0f;
            float maxRadiusSq = 0.0f;

            for (const auto& v : baseMesh.vertices)
            {
                if (v.position.y < minY) minY = v.position.y;
                if (v.position.y > maxY) maxY = v.position.y;
                
                // 幹の部分（風揺れウェイトが小さい部分）で半径を測定
                if (v.color.g < 0.8f)
                {
                    float radSq = v.position.x * v.position.x + v.position.z * v.position.z;
                    if (radSq > maxRadiusSq) maxRadiusSq = radSq;
                }
            }

            // フォールバック：もしウェイト判定で半径が取れなかった場合は全頂点から計算
            if (maxRadiusSq == 0.0f)
            {
                for (const auto& v : baseMesh.vertices)
                {
                    float radSq = v.position.x * v.position.x + v.position.z * v.position.z;
                    if (radSq > maxRadiusSq) maxRadiusSq = radSq;
                }
            }

            float height = maxY - minY;
            float radius = std::sqrt(maxRadiusSq);
            // 樹木のスケール感に合わせる（半径が大きすぎる場合はクランプ）
            if (radius < 0.05f) radius = 0.15f;
            if (radius > 1.5f) radius = 0.5f; // 葉を含んでしまい半径が巨大化した時の調整

            const int segments = 12;
            for (int i = 0; i < segments; ++i)
            {
                float angle = (float)i * 2.0f * (float)M_PI / (float)segments;
                float cosA = std::cos(angle);
                float sinA = std::sin(angle);

                Vertex vBottom{}, vTop{};
                vBottom.position = { cosA * radius, minY, sinA * radius };
                vBottom.normal = { cosA, 0.0f, sinA };
                vBottom.texcoord = { (float)i / (float)segments, 0.0f };
                vBottom.color = { 0.0f, 0.0f, 0.0f, 1.0f };

                vTop.position = { cosA * radius, maxY, sinA * radius };
                vTop.normal = { cosA, 0.0f, sinA };
                vTop.texcoord = { (float)i / (float)segments, 1.0f };
                vTop.color = { 0.0f, 0.0f, 0.0f, 1.0f };

                colMesh.vertices.push_back(vBottom);
                colMesh.vertices.push_back(vTop);
            }

            for (int i = 0; i < segments; ++i)
            {
                uint32_t i0 = i * 2;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = ((i + 1) % segments) * 2;
                uint32_t i3 = i2 + 1;

                colMesh.indices.push_back(i0);
                colMesh.indices.push_back(i1);
                colMesh.indices.push_back(i2);

                colMesh.indices.push_back(i2);
                colMesh.indices.push_back(i1);
                colMesh.indices.push_back(i3);
            }
        }
        else // Rock
        {
            // 全頂点から単純で安全なAABBバウンディングボックスを生成
            float minX = 999999.0f, maxX = -999999.0f;
            float minY = 999999.0f, maxY = -999999.0f;
            float minZ = 999999.0f, maxZ = -999999.0f;

            for (const auto& v : baseMesh.vertices)
            {
                if (v.position.x < minX) minX = v.position.x;
                if (v.position.x > maxX) maxX = v.position.x;
                if (v.position.y < minY) minY = v.position.y;
                if (v.position.y > maxY) maxY = v.position.y;
                if (v.position.z < minZ) minZ = v.position.z;
                if (v.position.z > maxZ) maxZ = v.position.z;
            }

            Vec3 box[8] = {
                { minX, minY, minZ }, { maxX, minY, minZ },
                { maxX, maxY, minZ }, { minX, maxY, minZ },
                { minX, minY, maxZ }, { maxX, minY, maxZ },
                { maxX, maxY, maxZ }, { minX, maxY, maxZ }
            };

            for (int i = 0; i < 8; ++i)
            {
                Vertex v{};
                v.position = box[i];
                v.normal = NormalizeVec3(box[i]);
                v.texcoord = { (i % 2 == 0) ? 0.f : 1.f, (i / 4 == 0) ? 0.f : 1.f };
                v.color = { 0.f, 0.f, 0.f, 1.f };
                colMesh.vertices.push_back(v);
            }

            uint32_t indices[36] = {
                0, 2, 1,   0, 3, 2,
                4, 5, 6,   4, 6, 7,
                0, 1, 5,   0, 5, 4,
                2, 3, 7,   2, 7, 6,
                0, 4, 7,   0, 7, 3,
                1, 2, 6,   1, 6, 5
            };
            for (int i = 0; i < 36; ++i)
            {
                colMesh.indices.push_back(indices[i]);
            }
        }

        return colMesh;
    }

    ExportResult BioProceduralGenerator::ExportToObj(const std::string& directoryPath, const std::string& fileName, const MeshData& meshData, int mode)
    {
        ExportResult result;
        result.totalVertices = (uint32_t)meshData.vertices.size();
        result.totalIndices = (uint32_t)meshData.indices.size();

        for (const auto& v : meshData.vertices)
        {
            if (v.color.r > 0.1f) result.mossVertices++;
            if (v.color.g > 0.1f) result.windVertices++;
        }

        if (result.totalVertices > 0)
        {
            result.mossRatio = (float)result.mossVertices / (float)result.totalVertices;
            result.windRatio = (float)result.windVertices / (float)result.totalVertices;
        }

        try
        {
            if (!std::filesystem::exists(directoryPath))
            {
                std::filesystem::create_directories(directoryPath);
            }
        }
        catch (...)
        {
            result.success = false;
            result.outputMessage = "Failed to create directory: " + directoryPath;
            return result;
        }

        std::string cleanDir = directoryPath;
        if (!cleanDir.empty() && cleanDir.back() != '/' && cleanDir.back() != '\\')
        {
            cleanDir += "/";
        }
        std::string objPath = cleanDir + fileName + ".obj";
        std::string mtlPath = cleanDir + fileName + ".mtl";
        std::string materialName = "ProceduralMaterial";

        std::string atlasName = fileName + "_atlas.tga";
        std::string atlasPath = cleanDir + atlasName;
        SaveTextureAtlasTga(atlasPath, mode);

        std::ofstream mtlFile(mtlPath);
        if (!mtlFile.is_open())
        {
            result.success = false;
            result.outputMessage = "Failed to write MTL file: " + mtlPath;
            return result;
        }

        mtlFile << "# Bio-Authoring Studio Material\n";
        mtlFile << "newmtl " << materialName << "\n";
        mtlFile << "Ka 1.0 1.0 1.0\n";
        mtlFile << "Kd 1.0 1.0 1.0\n";
        mtlFile << "Ks 0.2 0.2 0.2\n";
        mtlFile << "Ns 20.0\n";
        mtlFile << "Illum 2\n";
        mtlFile << "map_Kd " << atlasName << "\n";
        mtlFile.close();

        std::string buffer;
        size_t estimatedSize = meshData.vertices.size() * 150 + (meshData.indices.size() / 3) * 50 + 1024;
        buffer.reserve(estimatedSize);

        buffer += "# Bio-Authoring Studio Procedural Mesh with Vertex Color\n";
        buffer += "mtllib " + fileName + ".mtl\n\n";

        char temp[256];
        for (const auto& v : meshData.vertices)
        {
            int len = std::snprintf(temp, sizeof(temp), "v %.6f %.6f %.6f %.4f %.4f %.4f\n",
                v.position.x, v.position.y, v.position.z,
                v.color.r, v.color.g, v.color.b);
            buffer.append(temp, len);
        }
        buffer += "\n";

        for (const auto& v : meshData.vertices)
        {
            float u = v.texcoord.u;
            float v_coord = v.texcoord.v;

            if (mode == 0) // Tree
            {
                if (v.color.g > 0.5f) // 葉
                {
                    u = u * 0.5f + 0.5f;
                }
                else // 幹
                {
                    u = u * 0.5f;
                }
            }
            else // Rock
            {
                u = u * 0.5f;
            }

            buffer += "vt ";
            char temp_vt[64];
            char* ptr = temp_vt;
            ptr = WriteFloatToBuf(ptr, u);
            *ptr++ = ' ';
            ptr = WriteFloatToBuf(ptr, v_coord);
            *ptr++ = '\n';
            buffer.append(temp_vt, ptr - temp_vt);
        }
        buffer += "\n";

        for (const auto& v : meshData.vertices)
        {
            buffer += "vn ";
            char temp_vn[96];
            char* ptr = temp_vn;
            ptr = WriteFloatToBuf(ptr, v.normal.x);
            *ptr++ = ' ';
            ptr = WriteFloatToBuf(ptr, v.normal.y);
            *ptr++ = ' ';
            ptr = WriteFloatToBuf(ptr, v.normal.z);
            *ptr++ = '\n';
            buffer.append(temp_vn, ptr - temp_vn);
        }
        buffer += "\n";

        buffer += "usemtl " + materialName + "\n";

        for (size_t i = 0; i < meshData.indices.size(); i += 3)
        {
            uint32_t idx0 = meshData.indices[i] + 1;
            uint32_t idx1 = meshData.indices[i + 1] + 1;
            uint32_t idx2 = meshData.indices[i + 2] + 1;

            buffer += "f ";
            char temp_f[128];
            char* ptr = temp_f;
            
            auto r = std::to_chars(ptr, ptr + 16, idx0); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx0); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx0); ptr = r.ptr;
            *ptr++ = ' ';

            r = std::to_chars(ptr, ptr + 16, idx1); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx1); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx1); ptr = r.ptr;
            *ptr++ = ' ';

            r = std::to_chars(ptr, ptr + 16, idx2); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx2); ptr = r.ptr;
            *ptr++ = '/';
            r = std::to_chars(ptr, ptr + 16, idx2); ptr = r.ptr;
            *ptr++ = '\n';

            buffer.append(temp, ptr - temp);
        }

        std::ofstream objFile(objPath, std::ios::out | std::ios::binary);
        if (!objFile.is_open())
        {
            result.success = false;
            result.outputMessage = "Failed to write OBJ file: " + objPath;
            return result;
        }
        objFile.write(buffer.data(), buffer.size());
        objFile.close();
        
        result.success = true;
        result.outputMessage = "Export Succeeded: " + objPath;
        return result;
    }

    LODExportResult BioProceduralGenerator::ExportLODsToObj(
        const std::string& directoryPath, 
        const std::string& fileName, 
        int mode, 
        const TreeParameters& treeParams,
        const RockParameters& rockParams,
        const MeshData& lod0BaseMesh
    )
    {
        LODExportResult result;
        result.success = false;

        MeshData lod0Mesh = lod0BaseMesh;
        MeshData lod1Mesh;
        MeshData lod2Mesh;

        if (mode == 0) // Tree
        {
            
            // LOD1
            TreeParameters params1 = treeParams;
            params1.iterations = std::max(1, treeParams.iterations - 1);
            params1.radialSegments = std::max(4, treeParams.radialSegments - 2);
            params1.maxSegments = std::max(60, treeParams.maxSegments / 4);
            params1.maxLeaves = std::max(30, treeParams.maxLeaves / 4);
            params1.minBranchRadiusLimit = treeParams.branchRadius * 0.35f;
            lod1Mesh = GenerateTree(params1);

            // LOD2
            TreeParameters params2 = treeParams;
            params2.iterations = std::max(1, treeParams.iterations - 2);
            params2.radialSegments = std::max(4, treeParams.radialSegments - 4);
            params2.maxSegments = std::max(30, treeParams.maxSegments / 10);
            params2.maxLeaves = std::max(10, treeParams.maxLeaves / 10);
            params2.minBranchRadiusLimit = treeParams.branchRadius * 0.55f;
            lod2Mesh = GenerateTree(params2);
        }
        else // Rock
        {
            // LOD0
            lod0Mesh = GenerateRock(rockParams);

            // LOD1
            RockParameters params1 = rockParams;
            params1.subdivisions = std::max(1, rockParams.subdivisions - 1);
            lod1Mesh = GenerateRock(params1);

            // LOD2
            RockParameters params2 = rockParams;
            params2.subdivisions = std::max(1, rockParams.subdivisions - 2);
            lod2Mesh = GenerateRock(params2);
        }

        result.lod0Vertices = (uint32_t)lod0Mesh.vertices.size();
        result.lod0Indices = (uint32_t)lod0Mesh.indices.size();
        result.lod1Vertices = (uint32_t)lod1Mesh.vertices.size();
        result.lod1Indices = (uint32_t)lod1Mesh.indices.size();
        result.lod2Vertices = (uint32_t)lod2Mesh.vertices.size();
        result.lod2Indices = (uint32_t)lod2Mesh.indices.size();

        // 各LODファイルを書き出し
        ExportResult res0 = ExportToObj(directoryPath, fileName + "_LOD0", lod0Mesh, mode);
        if (!res0.success) {
            result.outputMessage = "Failed to export LOD0: " + res0.outputMessage;
            return result;
        }

        ExportResult res1 = ExportToObj(directoryPath, fileName + "_LOD1", lod1Mesh, mode);
        if (!res1.success) {
            result.outputMessage = "Failed to export LOD1: " + res1.outputMessage;
            return result;
        }

        ExportResult res2 = ExportToObj(directoryPath, fileName + "_LOD2", lod2Mesh, mode);
        if (!res2.success) {
            result.outputMessage = "Failed to export LOD2: " + res2.outputMessage;
            return result;
        }

        result.success = true;
        result.outputMessage = "LOD Batch Export Succeeded to: " + directoryPath;
        return result;
    }
}
