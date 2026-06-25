#pragma once

#include "Vector.h"
#include "Object3d.h"
#include <vector>
#include <string>
#include <map>

// プロシージャルなアセット生成を司るクラス
class ProceduralGenerator
{
public:
    // --- 岩石生成用パラメータ構造体 ---
    struct RockParameters
    {
        float scale = 1.0f;          // 全体スケール
        int subdivisions = 4;        // 分割数 (球体の細かさ)
        float noiseStrength = 0.3f;   // ノイズの強さ
        float noiseFrequency = 2.0f;  // ノイズの周波数
        int octaves = 3;             // ノイズのオクターブ数 (細かさ)
        float voronoiStrength = 0.2f; // ボロノイ断層の強さ
        int voronoiCells = 10;       // ボロノイセルの中心数
        unsigned int seed = 12345;   // ランダムシード値
    };

    // --- 樹木生成用パラメータ構造体 (L-System) ---
    struct TreeParameters
    {
        int iterations = 3;          // 再帰の深さ (世代数)
        float branchLength = 1.0f;   // 枝の長さ
        float branchRadius = 0.1f;   // 枝の太さ (根本)
        float taperRate = 0.8f;      // 先端に行くほど細くなる割合 (テーパリング)
        float angle = 25.0f;         // 分岐する角度 (度数法)
        std::string axiom = "X";     // 初期記号 (Axiom)
        unsigned int seed = 54321;   // ランダムシード値
    };

    /// <summary>
    /// プロシージャルな岩石メッシュを生成する
    /// </summary>
    /// <param name="params">生成パラメータ</param>
    /// <returns>生成されたモデルデータ</returns>
    static Object3d::ModelData GenerateRock(const RockParameters& params);

    /// <summary>
    /// プロシージャルな樹木メッシュを生成する (L-System)
    /// </summary>
    /// <param name="params">生成パラメータ</param>
    /// <returns>生成されたモデルデータ</returns>
    static Object3d::ModelData GenerateTree(const TreeParameters& params);

private:
    // 3Dノイズ (Perlin/Simplexノイズの簡易実装)
    static float Noise3D(float x, float y, float z);
    static float FractalNoise3D(float x, float y, float z, int octaves, float frequency);
    
    // 簡易ボロノイノイズ
    static float Voronoi3D(float x, float y, float z, int cellCount, unsigned int seed);
};
