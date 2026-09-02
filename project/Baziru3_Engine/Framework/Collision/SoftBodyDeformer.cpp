#include "SoftBodyDeformer.h"
#include <cmath>
#include <algorithm>

namespace SoftBodyDeformer
{
    static float s_globalTime = 0.0f;

    // 256 Permutation テーブル (Fast 3D Noise)
    static const uint8_t s_perm[512] = {
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
        8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
        35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
        134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
        55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
        18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
        250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
        189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
        172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
        228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
        107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
        151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
        8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
        35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,74,165,71,
        134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
        55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,187,208,89,
        18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,52,217,226,
        250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
        189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,153,101,155,167,43,
        172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,218,246,97,
        228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,145,235,249,14,239,
        107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,50,45,127,4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
    };

    inline float FastGrad(int hash, float x, float y, float z) {
        int h = hash & 15;
        float u = h < 8 ? x : y;
        float v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
        return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
    }

    // アニメーション時間を進める
    void AdvanceGlobalTime(float dt)
    {
        s_globalTime += dt;
    }

    // 現在のアニメーション時間を取得
    float GetGlobalTime()
    {
        return s_globalTime;
    }

    // 3Dパーリンノイズ計算（256順列テーブル参照）
    float FastNoise3D(const Vector3& p)
    {
        // 格子座標の整数部
        int X = static_cast<int>(std::floor(p.x)) & 255;
        int Y = static_cast<int>(std::floor(p.y)) & 255;
        int Z = static_cast<int>(std::floor(p.z)) & 255;

        // 格子内の小数位置
        float x = p.x - std::floor(p.x);
        float y = p.y - std::floor(p.y);
        float z = p.z - std::floor(p.z);

        // 5次エルミート曲線補間
        float u = x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
        float v = y * y * y * (y * (y * 6.0f - 15.0f) + 10.0f);
        float w = z * z * z * (z * (z * 6.0f - 15.0f) + 10.0f);

        // テーブル参照によるハッシュ生成
        int A = s_perm[X] + Y, AA = s_perm[A] + Z, AB = s_perm[A + 1] + Z;
        int B = s_perm[X + 1] + Y, BA = s_perm[B] + Z, BB = s_perm[B + 1] + Z;

        // トリリニア補間
        float x1 = std::lerp(FastGrad(s_perm[AA], x, y, z), FastGrad(s_perm[BA], x - 1.0f, y, z), u);
        float x2 = std::lerp(FastGrad(s_perm[AB], x, y - 1.0f, z), FastGrad(s_perm[BB], x - 1.0f, y - 1.0f, z), u);
        float y1 = std::lerp(x1, x2, v);

        float x3 = std::lerp(FastGrad(s_perm[AA + 1], x, y, z - 1.0f), FastGrad(s_perm[BA + 1], x - 1.0f, y, z - 1.0f), u);
        float x4 = std::lerp(FastGrad(s_perm[AB + 1], x, y - 1.0f, z - 1.0f), FastGrad(s_perm[BB + 1], x - 1.0f, y - 1.0f, z - 1.0f), u);
        float y2 = std::lerp(x3, x4, v);

        return std::lerp(y1, y2, w);
    }

    // 軟体変形計算（頂点シェーダーの変形計算と連動）
    Vector3 CalculateDeformedPosition(const Vector3& localP, const Vector3& normal, float time, float wobbleStr, float wobbleFreq)
    {
        Vector3 p = localP;

        // 1. 波打ち変形（三角関数 + ノイズ合成）
        float wave1 = std::sin(p.x * wobbleFreq + time * 3.5f) * std::cos(p.z * wobbleFreq * 0.8f + time * 2.7f);
        float wave2 = std::sin(p.y * wobbleFreq * 1.3f + time * 4.2f) * std::cos(p.x * wobbleFreq * 0.6f + time * 1.8f);
        float wave3 = FastNoise3D({ p.x * wobbleFreq * 0.5f + time * 1.5f,
                                    p.y * wobbleFreq * 0.5f + time * 1.5f,
                                    p.z * wobbleFreq * 0.5f + time * 1.5f }) * 2.0f;

        float wobbleOffset = (wave1 * 0.45f + wave2 * 0.35f + wave3 * 0.20f) * wobbleStr;

        // 2. 重力による垂れ下がり変形（洋梨形状）
        float sagFactor = 1.0f + 0.32f * (std::clamp)((1.0f - p.y) * 0.5f, 0.0f, 1.0f);
        Vector3 def = p;
        def.x *= sagFactor;
        def.z *= sagFactor;
        def.y = p.y * 0.85f - 0.08f;

        // 3. 接地偏平（床に接する部分を平らに広げる）
        if (def.y < -0.55f) {
            float flattenRate = (std::clamp)((-0.55f - def.y) / 0.45f, 0.0f, 1.0f);
            def.y = def.y * (1.0f - flattenRate * 0.75f) + (-0.68f) * (flattenRate * 0.75f);
            def.x *= (1.0f + flattenRate * 0.22f);
            def.z *= (1.0f + flattenRate * 0.22f);
        }

        // 4. 法線方向への波打ち加算
        def.x += normal.x * wobbleOffset;
        def.y += normal.y * wobbleOffset;
        def.z += normal.z * wobbleOffset;

        return def;
    }
}
