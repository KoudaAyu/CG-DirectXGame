#include "ProceduralGenerator.h"
#include <cmath>
#include <random>
#include <stack>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// --- ユーティリティ数学関数 ---
namespace
{
    // ベクトルの正規化
    Vector3 Normalize(const Vector3& v)
    {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (len > 0.0f)
        {
            return { v.x / len, v.y / len, v.z / len };
        }
        return { 0.0f, 0.0f, 0.0f };
    }

    // 外積
    Vector3 Cross(const Vector3& a, const Vector3& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    // 内積
    float Dot(const Vector3& a, const Vector3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // ロドリゲスの回転公式によるベクトルの回転
    Vector3 RotateVector(const Vector3& v, const Vector3& axis, float angleRad)
    {
        float cosA = std::cos(angleRad);
        float sinA = std::sin(angleRad);
        Vector3 a = Normalize(axis);
        
        // v * cosA + (a x v) * sinA + a * (a . v) * (1 - cosA)
        Vector3 crossAV = Cross(a, v);
        float dotAV = Dot(a, v);

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

    // フェード関数 (エルミート補間用の滑らかな重み)
    float Fade(float t)
    {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }
}

// 3Dバリューノイズの実装 (軽量かつ実用的なノイズ)
float ProceduralGenerator::Noise3D(float x, float y, float z)
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

    // 8つの格子点のハッシュ値を補間
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
float ProceduralGenerator::FractalNoise3D(float x, float y, float z, int octaves, float frequency)
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

// 簡易ボロノイノイズの実装 (断層風の段差を作るため)
float ProceduralGenerator::Voronoi3D(float x, float y, float z, int cellCount, unsigned int seed)
{
    std::mt19937 rand(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // セル中心点を生成
    std::vector<Vector3> centers(cellCount);
    for (int i = 0; i < cellCount; ++i)
    {
        centers[i] = { dist(rand), dist(rand), dist(rand) };
    }

    Vector3 pos = { x, y, z };
    float minDist = 1e10f;

    // 最も近い中心点への距離を算出
    for (int i = 0; i < cellCount; ++i)
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
Object3d::ModelData ProceduralGenerator::GenerateRock(const RockParameters& params)
{
    Object3d::ModelData data;

    // 球体の分割数設定
    int latSegments = params.subdivisions * 4;
    int lonSegments = params.subdivisions * 8;

    std::vector<Sprite::VertexData> tempVertices;

    // 1. 球体状の頂点を生成し、ノイズで変形させる
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

            // 球体上の位置ベクトル (半径 1.0)
            Vector3 basePos = { sinTheta * cosPhi, cosTheta, sinTheta * sinPhi };

            // 多重オクターブノイズの適用
            float n = FractalNoise3D(basePos.x, basePos.y, basePos.z, params.octaves, params.noiseFrequency);
            
            // ボロノイ断層の適用
            float v = Voronoi3D(basePos.x, basePos.y, basePos.z, params.voronoiCells, params.seed);

            // 半径の決定 (基本値 + ノイズ摂動 + ボロノイ段差)
            float radius = params.scale * (1.0f + (n - 0.5f) * params.noiseStrength + (v - 0.5f) * params.voronoiStrength);

            Sprite::VertexData vertex{};
            vertex.position = { basePos.x * radius, basePos.y * radius, basePos.z * radius, 1.0f };
            // UV 座標
            vertex.texcoord = { (float)lon / (float)lonSegments, (float)lat / (float)latSegments };
            // 初期法線は中心からの向き
            vertex.normal = { basePos.x, basePos.y, basePos.z };

            tempVertices.push_back(vertex);
        }
    }

    // 2. インデックスバッファの構築 (インデックス化によるメモリ削減)
    for (int lat = 0; lat < latSegments; ++lat)
    {
        for (int lon = 0; lon < lonSegments; ++lon)
        {
            uint32_t first = (lat * (lonSegments + 1)) + lon;
            uint32_t second = first + lonSegments + 1;

            // 三角形1
            data.indices.push_back(first);
            data.indices.push_back(second);
            data.indices.push_back(first + 1);

            // 三角形2
            data.indices.push_back(first + 1);
            data.indices.push_back(second);
            data.indices.push_back(second + 1);
        }
    }

    // 3. 正確な法線ベクトルの再計算 (フラット / スムースの中間を狙う)
    data.vertices = tempVertices;
    std::vector<Vector3> calculatedNormals(data.vertices.size(), { 0.0f, 0.0f, 0.0f });

    for (size_t i = 0; i < data.indices.size(); i += 3)
    {
        uint32_t idx0 = data.indices[i];
        uint32_t idx1 = data.indices[i + 1];
        uint32_t idx2 = data.indices[i + 2];

        Vector3 p0 = { data.vertices[idx0].position.x, data.vertices[idx0].position.y, data.vertices[idx0].position.z };
        Vector3 p1 = { data.vertices[idx1].position.x, data.vertices[idx1].position.y, data.vertices[idx1].position.z };
        Vector3 p2 = { data.vertices[idx2].position.x, data.vertices[idx2].position.y, data.vertices[idx2].position.z };

        Vector3 v0 = { p1.x - p0.x, p1.y - p0.y, p1.z - p0.z };
        Vector3 v1 = { p2.x - p0.x, p2.y - p0.y, p2.z - p0.z };

        Vector3 faceNormal = Cross(v0, v1);

        calculatedNormals[idx0].x += faceNormal.x; calculatedNormals[idx0].y += faceNormal.y; calculatedNormals[idx0].z += faceNormal.z;
        calculatedNormals[idx1].x += faceNormal.x; calculatedNormals[idx1].y += faceNormal.y; calculatedNormals[idx1].z += faceNormal.z;
        calculatedNormals[idx2].x += faceNormal.x; calculatedNormals[idx2].y += faceNormal.y; calculatedNormals[idx2].z += faceNormal.z;
    }

    for (size_t i = 0; i < data.vertices.size(); ++i)
    {
        data.vertices[i].normal = Normalize(calculatedNormals[i]);
    }

    return data;
}

// --- 樹木のプロシージャル生成 (L-System & タートルグラフィックス) ---
Object3d::ModelData ProceduralGenerator::GenerateTree(const TreeParameters& params)
{
    Object3d::ModelData data;

    // 1. L-System文の展開
    std::string currentString = params.axiom;
    
    // ルールの定義
    // 3D的な広がりを持たせるために、ピッチ(&, ^)とロール(/, \)の記号を含める
    for (int i = 0; i < params.iterations; ++i)
    {
        std::string nextString = "";
        for (char c : currentString)
        {
            if (c == 'X')
            {
                // 3D的な分岐を含めるルール
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

    // 2. タートルグラフィックスによる3Dメッシュ構築
    struct TurtleState
    {
        Vector3 position;
        Vector3 direction; // 前方 (Forward)
        Vector3 up;        // 上方 (Up)
        Vector3 right;     // 右方 (Right)
        float radius;
    };

    std::stack<TurtleState> stateStack;
    
    // 初期タートル状態
    TurtleState turtle = {
        { 0.0f, 0.0f, 0.0f }, // 地面から開始
        { 0.0f, 1.0f, 0.0f }, // 上向き
        { 0.0f, 0.0f, -1.0f },// 任意の直交ベクトル
        { 1.0f, 0.0f, 0.0f },
        params.branchRadius
    };

    float radAngle = params.angle * (float)M_PI / 180.0f;
    std::mt19937 rand(params.seed);
    std::uniform_real_distribution<float> angleDist(-0.15f, 0.15f);

    // シリンダ生成用関数
    auto BuildCylinder = [&](const Vector3& start, const Vector3& end, float startRad, float endRad, const TurtleState& t) {
        int radialSegments = 8;
        uint32_t baseIdx = (uint32_t)data.vertices.size();

        // 始点と終点の円周上に頂点を配置
        for (int i = 0; i <= radialSegments; ++i)
        {
            float theta = (float)i * 2.0f * (float)M_PI / (float)radialSegments;
            float cosT = std::cos(theta);
            float sinT = std::sin(theta);

            // 始点円周上のローカル座標
            Vector3 startRingOffset = {
                (t.right.x * cosT + t.up.x * sinT) * startRad,
                (t.right.y * cosT + t.up.y * sinT) * startRad,
                (t.right.z * cosT + t.up.z * sinT) * startRad
            };

            // 終点円周上のローカル座標
            Vector3 endRingOffset = {
                (t.right.x * cosT + t.up.x * sinT) * endRad,
                (t.right.y * cosT + t.up.y * sinT) * endRad,
                (t.right.z * cosT + t.up.z * sinT) * endRad
            };

            Sprite::VertexData vStart{}, vEnd{};
            vStart.position = { start.x + startRingOffset.x, start.y + startRingOffset.y, start.z + startRingOffset.z, 1.0f };
            vStart.normal = Normalize(startRingOffset);
            vStart.texcoord = { (float)i / (float)radialSegments, 0.0f };

            vEnd.position = { end.x + endRingOffset.x, end.y + endRingOffset.y, end.z + endRingOffset.z, 1.0f };
            vEnd.normal = Normalize(endRingOffset);
            vEnd.texcoord = { (float)i / (float)radialSegments, 1.0f };

            data.vertices.push_back(vStart);
            data.vertices.push_back(vEnd);
        }

        // 面インデックスの構築
        for (int i = 0; i < radialSegments; ++i)
        {
            uint32_t i0 = baseIdx + i * 2;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = baseIdx + (i + 1) * 2;
            uint32_t i3 = i2 + 1;

            // 三角形 1
            data.indices.push_back(i0);
            data.indices.push_back(i1);
            data.indices.push_back(i2);

            // 三角形 2
            data.indices.push_back(i2);
            data.indices.push_back(i1);
            data.indices.push_back(i3);
        }
    };

    // 葉（リーフ）生成用関数 (枝の先端に交差する2つの緑色四角形メッシュを配置する)
    auto BuildLeaf = [&](const Vector3& pos, float size, const TurtleState& t) {
        uint32_t baseIdx = (uint32_t)data.vertices.size();

        Vector3 right = Normalize(t.right);
        Vector3 up = Normalize(t.up);

        Sprite::VertexData v0{}, v1{}, v2{}, v3{};
        v0.position = { pos.x - right.x * size, pos.y - right.y * size, pos.z - right.z * size, 1.0f };
        v1.position = { pos.x + right.x * size, pos.y + right.y * size, pos.z + right.z * size, 1.0f };
        v2.position = { pos.x - up.x * size, pos.y - up.y * size, pos.z - up.z * size, 1.0f };
        v3.position = { pos.x + up.x * size, pos.y + up.y * size, pos.z + up.z * size, 1.0f };

        // 葉の法線
        v0.normal = up; v1.normal = up; v2.normal = right; v3.normal = right;
        // UV座標 (テクスチャの一部分を葉の色に割り当てられるようにする)
        v0.texcoord = { 0.0f, 0.0f };
        v1.texcoord = { 0.1f, 0.0f };
        v2.texcoord = { 0.0f, 0.1f };
        v3.texcoord = { 0.1f, 0.1f };

        data.vertices.push_back(v0);
        data.vertices.push_back(v1);
        data.vertices.push_back(v2);
        data.vertices.push_back(v3);

        data.indices.push_back(baseIdx + 0);
        data.indices.push_back(baseIdx + 2);
        data.indices.push_back(baseIdx + 1);

        data.indices.push_back(baseIdx + 1);
        data.indices.push_back(baseIdx + 2);
        data.indices.push_back(baseIdx + 3);
    };

    // 文字列を解析して枝を伸ばす
    for (char c : currentString)
    {
        if (c == 'F')
        {
            // 枝を伸ばす
            Vector3 nextPos = {
                turtle.position.x + turtle.direction.x * params.branchLength,
                turtle.position.y + turtle.direction.y * params.branchLength,
                turtle.position.z + turtle.direction.z * params.branchLength
            };

            float nextRadius = turtle.radius * params.taperRate;

            // 枝の円柱メッシュを作成
            BuildCylinder(turtle.position, nextPos, turtle.radius, nextRadius, turtle);

            turtle.position = nextPos;
            turtle.radius = nextRadius;
        }
        else if (c == 'X')
        {
            // 枝の先端(X)に葉っぱを生成
            BuildLeaf(turtle.position, params.branchLength * 0.4f, turtle);
        }
        else if (c == '+')
        {
            // ヨー（右回転）: Up軸を中心に Direction と Right を回転させる
            float variation = angleDist(rand);
            float rot = radAngle + variation;
            turtle.direction = RotateVector(turtle.direction, turtle.up, rot);
            turtle.right = RotateVector(turtle.right, turtle.up, rot);
        }
        else if (c == '-')
        {
            // ヨー（左回転）: Up軸を中心に Direction と Right を回転させる
            float variation = angleDist(rand);
            float rot = -radAngle + variation;
            turtle.direction = RotateVector(turtle.direction, turtle.up, rot);
            turtle.right = RotateVector(turtle.right, turtle.up, rot);
        }
        else if (c == '&')
        {
            // ピッチアップ: Right軸を中心に Direction と Up を回転させる
            float variation = angleDist(rand);
            float rot = radAngle + variation;
            turtle.direction = RotateVector(turtle.direction, turtle.right, rot);
            turtle.up = RotateVector(turtle.up, turtle.right, rot);
        }
        else if (c == '^')
        {
            // ピッチダウン: Right軸を中心に Direction と Up を回転させる
            float variation = angleDist(rand);
            float rot = -radAngle + variation;
            turtle.direction = RotateVector(turtle.direction, turtle.right, rot);
            turtle.up = RotateVector(turtle.up, turtle.right, rot);
        }
        else if (c == '/')
        {
            // ロール右: Direction軸を中心に Up と Right を回転させる
            turtle.up = RotateVector(turtle.up, turtle.direction, radAngle);
            turtle.right = RotateVector(turtle.right, turtle.direction, radAngle);
        }
        else if (c == '\\')
        {
            // ロール左: Direction軸を中心に Up と Right を回転させる
            turtle.up = RotateVector(turtle.up, turtle.direction, -radAngle);
            turtle.right = RotateVector(turtle.right, turtle.direction, -radAngle);
        }
        else if (c == '[')
        {
            // 状態保存
            stateStack.push(turtle);
        }
        else if (c == ']')
        {
            // 状態復帰
            if (!stateStack.empty())
            {
                turtle = stateStack.top();
                stateStack.pop();
            }
        }
    }

    return data;
}
