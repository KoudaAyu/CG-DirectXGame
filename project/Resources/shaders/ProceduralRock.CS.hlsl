// ProceduralRock.CS.hlsl
// Compute Shader for dynamic rock mesh deformation

struct Vertex
{
    float3 position;
    float3 normal;
    float2 texcoord;
    float4 color;
};

struct RockParameters
{
    float scale;
    int subdivisions;
    float noiseStrength;
    float noiseFrequency;
    int octaves;
    float voronoiStrength;
    int voronoiCells;
    float crackStrength;
    float crackFrequency;
    uint seed;
    float4 voronoiCenters[50]; // アライメントを考慮して float4 配列にする (xyzを使用)
};

ConstantBuffer<RockParameters> gParams : register(b0);
StructuredBuffer<Vertex> gBaseVertices : register(t0);
RWStructuredBuffer<Vertex> gOutputVertices : register(u0);

// --- ノイズユーティリティ関数 ---

float Fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float Hash(int x, int y, int z)
{
    int n = x + y * 57 + z * 997;
    n = (n << 13) ^ n;
    // ビット演算を安全に処理するため符号なしからキャスト
    uint unsignedN = asuint(n);
    uint randVal = (unsignedN * (unsignedN * unsignedN * 15731u + 789221u) + 1376312589u) & 0x7fffffffu;
    return (1.0f - float(randVal) / 1073741824.0f) * 0.5f + 0.5f;
}

float Noise3D(float x, float y, float z)
{
    int ix = (int)floor(x);
    int iy = (int)floor(y);
    int iz = (int)floor(z);

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

    float x1 = lerp(h000, h100, u);
    float x2 = lerp(h010, h110, u);
    float x3 = lerp(h001, h101, u);
    float x4 = lerp(h011, h111, u);

    float y1 = lerp(x1, x2, v);
    float y2 = lerp(x3, x4, w); // Z補間に w を使用

    return lerp(y1, y2, w);
}

float FractalNoise3D(float x, float y, float z, int octaves, float frequency)
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

// CBufferから送られてきた中心点を用いるボロノイ計算
float Voronoi3D(float3 pos, int cellCount)
{
    float minDist = 1e10f;
    for (int i = 0; i < cellCount; ++i)
    {
        float3 center = gParams.voronoiCenters[i].xyz;
        float d = distance(pos, center);
        if (d < minDist)
        {
            minDist = d;
        }
    }
    return minDist;
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // スレッドIDが全頂点数を超えている場合は終了
    // 頂点数は CBuffer で直接取得できないため、Dispatch時に適切な数をバインドする
    // バッファの長さ情報等を確認
    uint totalVertices = 0;
    uint dummy = 0;
    gBaseVertices.GetDimensions(totalVertices, dummy);

    if (DTid.x >= totalVertices)
    {
        return;
    }

    Vertex base = gBaseVertices[DTid.x];
    float3 basePos = base.position;

    // 1. ボロノイ断崖 (崖を作る平滑化)
    float v = Voronoi3D(basePos, gParams.voronoiCells);
    if (v > 0.35f)
    {
        v = 0.35f + (v - 0.35f) * 0.05f;
    }

    // 2. FBMノイズ
    float n = FractalNoise3D(basePos.x, basePos.y, basePos.z, gParams.octaves, gParams.noiseFrequency);

    // 3. 亀裂（クラック）の彫刻
    float crackN = FractalNoise3D(basePos.x * gParams.crackFrequency, 
                                  basePos.y * gParams.crackFrequency, 
                                  basePos.z * gParams.crackFrequency, 3, 1.3f);
    float crackVal = 1.0f - abs(crackN - 0.5f) * 2.0f;
    
    float crackOffset = 0.0f;
    if (crackVal > 0.60f)
    {
        float t = (crackVal - 0.60f) / 0.40f;
        crackOffset = -gParams.crackStrength * pow(t, 2.0f) * 0.8f;
    }

    // 新しい位置の計算
    float radius = gParams.scale * (1.0f + (n - 0.5f) * gParams.noiseStrength + (v - 0.5f) * gParams.voronoiStrength * 1.5f + crackOffset);
    if (radius < 0.05f) radius = 0.05f;

    float3 deformedPos = basePos * radius;

    // 頂点カラーの計算 (苔ブレンドウェイト: 上向き法線判定)
    // 法線のY成分に基づいてグラデーションを焼き付ける
    // 初期法線は base.normal
    float3 normal = base.normal; // 簡易的に初期法線を使用し、ピクセルシェーダー側やG-Bufferで再計算するか、もしくはここで簡易法線を適用
    // 面法線はCS内では近傍頂点が無いため簡易的に中心からの方向（球体ベース）とする
    float3 calculatedNormal = normalize(basePos); // 初期球体の向き
    float upDot = calculatedNormal.y;
    float mossWeight = max(0.0f, upDot);
    mossWeight = pow(mossWeight, 2.5f);

    // 頂点データの出力
    Vertex output;
    output.position = deformedPos;
    output.normal = calculatedNormal; // 初期法線をひとまず設定 (頂点シェーダーでのトランスフォームに対応)
    output.texcoord = base.texcoord;
    output.color = float4(mossWeight, 0.0f, 0.0f, 1.0f);

    gOutputVertices[DTid.x] = output;
}
