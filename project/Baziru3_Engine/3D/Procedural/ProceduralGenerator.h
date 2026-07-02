#pragma once

#include "Vector.h"
#include "Object3d.h"
#include "BioProceduralGenerator.h"
#include <vector>
#include <string>

// プロシージャルなアセット生成を司るクラス (BioProceduralGeneratorへのアダプター)
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
        float crackStrength = 0.4f;   // 岩の亀裂（クラック）の強さ
        float crackFrequency = 3.0f;  // 亀裂の周波数
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
        int radialSegments = 8;      // 枝の円柱分割数 (LOD制御用)
        int maxSegments = 2000;      // 枝の最大生成数
        int maxLeaves = 1000;        // 葉の最大生成数
        float minBranchRadiusLimit = 0.0f; // この半径未満の枝・葉の生成をスキップする制限値
    };

    /// <summary>
    /// プロシージャルな岩石メッシュを生成する (アダプター経由)
    /// </summary>
    static Object3d::ModelData GenerateRock(const RockParameters& params);

    /// <summary>
    /// プロシージャルな樹木メッシュを生成する (L-System, アダプター経由)
    /// </summary>
    static Object3d::ModelData GenerateTree(const TreeParameters& params);

    /// <summary>
    /// 現在のモデルデータをOBJ形式でエクスポートする
    /// </summary>
    static BioProcedural::ExportResult ExportToObj(const std::string& directoryPath, const std::string& fileName, const Object3d::ModelData& modelData);

    /// <summary>
    /// LOD0〜LOD2のメッシュを自動段階削減して一括でエクスポートする
    /// </summary>
    static BioProcedural::LODExportResult ExportLODsToObj(
        const std::string& directoryPath, 
        const std::string& fileName, 
        int mode, // 0: Tree, 1: Rock
        const TreeParameters& treeParams,
        const RockParameters& rockParams,
        const std::vector<Sprite::VertexData>& lod0GpuVertices = {}
    );
};
