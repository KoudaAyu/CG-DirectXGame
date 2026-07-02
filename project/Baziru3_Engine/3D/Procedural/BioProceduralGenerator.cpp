#include "BioProceduralGenerator.h"
#include <cmath>
#include <random>
#include <stack>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

namespace BioProcedural
{
    // --- ユーティリティ数学関数 ---
    namespace
    {
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

        // 【最適化: 就活ポートフォリオ用】
        // std::vector の動的拡張によるメモリ再確保・コピーを防ぐため、事前に必要なサイズを計算して一括確保
        size_t numF = 0;
        size_t numX = 0;
        for (char c : currentString)
        {
            if (c == 'F') numF++;
            else if (c == 'X') numX++;
        }

        // 1つのF（枝シリンダー）＝(radialSegments+1)*2 頂点、radialSegments*6 インデックス
        // 1つのXまたは細い枝先（葉クラスター4個）＝32頂点、96インデックス
        size_t estimatedVertices = numF * (params.radialSegments + 1) * 2 + (numX + numF) * 32;
        size_t estimatedIndices = numF * params.radialSegments * 6 + (numX + numF) * 96;

        data.vertices.reserve(estimatedVertices);
        data.indices.reserve(estimatedIndices);

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

        float maxExpectedLen = params.branchLength * std::pow(2.0f, static_cast<float>(params.iterations)) * 1.5f;
        if (maxExpectedLen <= 0.0f) maxExpectedLen = 1.0f;

        auto BuildCylinder = [&](const Vec3& start, const Vec3& end, float startRad, float endRad, const TurtleState& t) {
            int radialSegments = params.radialSegments;
            uint32_t baseIdx = (uint32_t)data.vertices.size();

            float windWeightStart = std::min(1.0f, t.distanceToRoot / maxExpectedLen);
            float windWeightEnd = std::min(1.0f, (t.distanceToRoot + params.branchLength) / maxExpectedLen);

            windWeightStart = std::pow(windWeightStart, 2.0f);
            windWeightEnd = std::pow(windWeightEnd, 2.0f);

            for (int i = 0; i <= radialSegments; ++i)
            {
                float theta = (float)i * 2.0f * (float)M_PI / (float)radialSegments;
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);

                Vec3 startRingOffset = {
                    (t.right.x * cosT + t.up.x * sinT) * startRad,
                    (t.right.y * cosT + t.up.y * sinT) * startRad,
                    (t.right.z * cosT + t.up.z * sinT) * startRad
                };

                Vec3 endRingOffset = {
                    (t.right.x * cosT + t.up.x * sinT) * endRad,
                    (t.right.y * cosT + t.up.y * sinT) * endRad,
                    (t.right.z * cosT + t.up.z * sinT) * endRad
                };

                Vertex vStart{}, vEnd{};
                vStart.position = { start.x + startRingOffset.x, start.y + startRingOffset.y, start.z + startRingOffset.z };
                vStart.normal = NormalizeVec3(startRingOffset);
                vStart.texcoord = { (float)i / (float)radialSegments, 0.0f };
                vStart.color = { 0.0f, windWeightStart, 0.0f, 1.0f };

                vEnd.position = { end.x + endRingOffset.x, end.y + endRingOffset.y, end.z + endRingOffset.z };
                vEnd.normal = NormalizeVec3(endRingOffset);
                vEnd.texcoord = { (float)i / (float)radialSegments, 1.0f };
                vEnd.color = { 0.0f, windWeightEnd, 0.0f, 1.0f }; 

                data.vertices.push_back(vStart);
                data.vertices.push_back(vEnd);
            }

            for (int i = 0; i < radialSegments; ++i)
            {
                uint32_t i0 = baseIdx + i * 2;
                uint32_t i1 = i0 + 1;
                uint32_t i2 = baseIdx + (i + 1) * 2;
                uint32_t i3 = i2 + 1;

                data.indices.push_back(i0);
                data.indices.push_back(i1);
                data.indices.push_back(i2);

                data.indices.push_back(i2);
                data.indices.push_back(i1);
                data.indices.push_back(i3);
            }
        };

        auto BuildLeaf = [&](const Vec3& pos, float size, const TurtleState& t, std::mt19937& r) {
            int numLeavesInCluster = 4;
            std::uniform_real_distribution<float> offsetDist(-size * 0.25f, size * 0.25f);
            std::uniform_real_distribution<float> scaleDist(0.7f, 1.3f);
            std::uniform_real_distribution<float> rAngleDist(0.0f, 360.0f);

            float windWeight = 1.0f;

            for (int cIdx = 0; cIdx < numLeavesInCluster; ++cIdx)
            {
                Vec3 leafPos = {
                    pos.x + offsetDist(r),
                    pos.y + offsetDist(r),
                    pos.z + offsetDist(r)
                };

                float leafSize = size * scaleDist(r);

                float rollAngleRad = rAngleDist(r) * (float)M_PI / 180.0f;
                Vec3 right = RotateVectorVec3(t.right, t.direction, rollAngleRad);
                Vec3 up = RotateVectorVec3(t.up, t.direction, rollAngleRad);
                Vec3 dir = t.direction;

                for (int quadIdx = 0; quadIdx < 2; ++quadIdx)
                {
                    uint32_t baseIdx = (uint32_t)data.vertices.size();
                    float rotAngleRad = (float)quadIdx * (float)M_PI / 2.0f;

                    Vec3 rotRight = RotateVectorVec3(right, dir, rotAngleRad);
                    Vec3 rotUp = RotateVectorVec3(up, dir, rotAngleRad);

                    Vec3 p0 = { leafPos.x - rotRight.x * leafSize - rotUp.x * leafSize, leafPos.y - rotRight.y * leafSize - rotUp.y * leafSize, leafPos.z - rotRight.z * leafSize - rotUp.z * leafSize };
                    Vec3 p1 = { leafPos.x + rotRight.x * leafSize - rotUp.x * leafSize, leafPos.y + rotRight.y * leafSize - rotUp.y * leafSize, leafPos.z + rotRight.z * leafSize - rotUp.z * leafSize };
                    Vec3 p2 = { leafPos.x + rotRight.x * leafSize + rotUp.x * leafSize, leafPos.y + rotRight.y * leafSize + rotUp.y * leafSize, leafPos.z + rotRight.z * leafSize + rotUp.z * leafSize };
                    Vec3 p3 = { leafPos.x - rotRight.x * leafSize + rotUp.x * leafSize, leafPos.y - rotRight.y * leafSize + rotUp.y * leafSize, leafPos.z - rotRight.z * leafSize + rotUp.z * leafSize };

                    Vertex v0{}, v1{}, v2{}, v3{};
                    v0.position = p0; v1.position = p1; v2.position = p2; v3.position = p3;

                    Vec3 normal = NormalizeVec3(CrossVec3(rotRight, rotUp));
                    v0.normal = normal; v1.normal = normal; v2.normal = normal; v3.normal = normal;

                    v0.texcoord = { 0.0f, 0.0f };
                    v1.texcoord = { 0.1f, 0.0f };
                    v2.texcoord = { 0.1f, 0.1f };
                    v3.texcoord = { 0.0f, 0.1f };

                    v0.color = { 0.0f, windWeight, 0.0f, 1.0f };
                    v1.color = { 0.0f, windWeight, 0.0f, 1.0f };
                    v2.color = { 0.0f, windWeight, 0.0f, 1.0f };
                    v3.color = { 0.0f, windWeight, 0.0f, 1.0f };

                    data.vertices.push_back(v0);
                    data.vertices.push_back(v1);
                    data.vertices.push_back(v2);
                    data.vertices.push_back(v3);

                    // 表
                    data.indices.push_back(baseIdx + 0);
                    data.indices.push_back(baseIdx + 1);
                    data.indices.push_back(baseIdx + 2);
                    data.indices.push_back(baseIdx + 0);
                    data.indices.push_back(baseIdx + 2);
                    data.indices.push_back(baseIdx + 3);

                    // 裏
                    data.indices.push_back(baseIdx + 0);
                    data.indices.push_back(baseIdx + 2);
                    data.indices.push_back(baseIdx + 1);
                    data.indices.push_back(baseIdx + 0);
                    data.indices.push_back(baseIdx + 3);
                    data.indices.push_back(baseIdx + 2);
                }
            }
        };

        size_t segmentCount = 0;
        size_t leafCount = 0;

        for (char c : currentString)
        {
            if (segmentCount >= (size_t)params.maxSegments)
            {
                break;
            }

            if (c == 'F')
            {
                segmentCount++;
                // 【就活強化点: 枝のノイズによる有機的うねり(曲がり)】
                // 3Dバリューノイズにより、枝の成長方向にわずかにゆらぎを与えてクネクネと曲がらせる
                float nx = Noise3D(turtle.position.x * 2.0f, turtle.position.y * 2.0f, turtle.position.z * 2.0f) - 0.5f;
                float ny = Noise3D(turtle.position.y * 2.0f, turtle.position.z * 2.0f, turtle.position.x * 2.0f) - 0.5f;
                float nz = Noise3D(turtle.position.z * 2.0f, turtle.position.x * 2.0f, turtle.position.y * 2.0f) - 0.5f;

                Vec3 noiseVec = { nx * 0.25f, ny * 0.25f, nz * 0.25f };
                turtle.direction = NormalizeVec3({
                    turtle.direction.x + noiseVec.x,
                    turtle.direction.y + noiseVec.y,
                    turtle.direction.z + noiseVec.z
                });

                // 直交座標系を再構築
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

                if (turtle.radius >= params.minBranchRadiusLimit)
                {
                    BuildCylinder(turtle.position, nextPos, turtle.radius, nextRadius, turtle);
                }

                turtle.position = nextPos;
                turtle.radius = nextRadius;
                turtle.distanceToRoot += params.branchLength;

                if (turtle.radius < params.branchRadius * 0.6f && turtle.radius >= params.minBranchRadiusLimit)
                {
                    if (leafCount < (size_t)params.maxLeaves)
                    {
                        BuildLeaf(turtle.position, params.branchLength * 0.35f, turtle, rand);
                        leafCount++;
                    }
                }
            }
            else if (c == 'X')
            {
                if (turtle.radius >= params.minBranchRadiusLimit)
                {
                    if (leafCount < (size_t)params.maxLeaves)
                    {
                        BuildLeaf(turtle.position, params.branchLength * 0.4f, turtle, rand);
                        leafCount++;
                    }
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

    // --- OBJ形式でエクスポートする機能の実装 (統計結果を返す) ---
    ExportResult BioProceduralGenerator::ExportToObj(const std::string& directoryPath, const std::string& fileName, const MeshData& meshData)
    {
        ExportResult result;
        result.totalVertices = (uint32_t)meshData.vertices.size();
        result.totalIndices = (uint32_t)meshData.indices.size();

        // 統計情報の計算
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

        // 1. ディレクトリの作成
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

        std::string objPath = (std::filesystem::path(directoryPath) / (fileName + ".obj")).string();
        std::string mtlPath = (std::filesystem::path(directoryPath) / (fileName + ".mtl")).string();

        // 2. MTLファイルの書き出し
        std::ofstream mtlFile(mtlPath);
        if (!mtlFile.is_open())
        {
            result.success = false;
            result.outputMessage = "Failed to write MTL file: " + mtlPath;
            return result;
        }

        std::string materialName = fileName + "_Material";
        mtlFile << "# Bio-Authoring Studio Material\n";
        mtlFile << "newmtl " << materialName << "\n";
        mtlFile << "Ka 1.0 1.0 1.0\n";
        mtlFile << "Kd 1.0 1.0 1.0\n";
        mtlFile << "Ks 0.2 0.2 0.2\n";
        mtlFile << "Ns 20.0\n";
        mtlFile << "Illum 2\n";
        
        if (!meshData.texturePath.empty())
        {
            std::string texPath = meshData.texturePath;
            std::replace(texPath.begin(), texPath.end(), '\\', '/');
            mtlFile << "map_Kd " << texPath << "\n";
        }
        mtlFile.close();

        // 3. OBJファイルの書き出し (メモリバッファを用いた超高速一括書き出し)
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
            int len = std::snprintf(temp, sizeof(temp), "vt %.6f %.6f\n", v.texcoord.u, v.texcoord.v);
            buffer.append(temp, len);
        }
        buffer += "\n";

        for (const auto& v : meshData.vertices)
        {
            int len = std::snprintf(temp, sizeof(temp), "vn %.6f %.6f %.6f\n", v.normal.x, v.normal.y, v.normal.z);
            buffer.append(temp, len);
        }
        buffer += "\n";

        buffer += "usemtl " + materialName + "\n";

        for (size_t i = 0; i < meshData.indices.size(); i += 3)
        {
            uint32_t idx0 = meshData.indices[i] + 1;
            uint32_t idx1 = meshData.indices[i + 1] + 1;
            uint32_t idx2 = meshData.indices[i + 2] + 1;

            int len = std::snprintf(temp, sizeof(temp), "f %u/%u/%u %u/%u/%u %u/%u/%u\n",
                idx0, idx0, idx0,
                idx1, idx1, idx1,
                idx2, idx2, idx2);
            buffer.append(temp, len);
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
        ExportResult res0 = ExportToObj(directoryPath, fileName + "_LOD0", lod0Mesh);
        if (!res0.success) {
            result.outputMessage = "Failed to export LOD0: " + res0.outputMessage;
            return result;
        }

        ExportResult res1 = ExportToObj(directoryPath, fileName + "_LOD1", lod1Mesh);
        if (!res1.success) {
            result.outputMessage = "Failed to export LOD1: " + res1.outputMessage;
            return result;
        }

        ExportResult res2 = ExportToObj(directoryPath, fileName + "_LOD2", lod2Mesh);
        if (!res2.success) {
            result.outputMessage = "Failed to export LOD2: " + res2.outputMessage;
            return result;
        }

        result.success = true;
        result.outputMessage = "LOD Batch Export Succeeded to: " + directoryPath;
        return result;
    }
}
