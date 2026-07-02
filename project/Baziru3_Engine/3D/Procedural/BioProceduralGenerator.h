#pragma once
#include <vector>
#include <string>

namespace BioProcedural
{
    // 依存関係を完全に排除したプリミティブな数学・3Dモデル用構造体
    struct Vec2 { float u, v; };
    struct Vec3 { float x, y, z; };
    struct Vec4 { float r, g, b, a; }; // 頂点カラー用 (苔ウェイト、風ウェイトなど)

    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texcoord;
        Vec4 color; // r: 苔ウェイト, g: 風揺れウェイト, b: 空き, a: 空き
    };

    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;
        std::string texturePath; // エクスポート時のテクスチャ参照用
    };

    // --- エクスポート統計情報構造体 (就活ポートフォリオ用アピール) ---
    struct ExportResult
    {
        bool success = false;
        uint32_t totalVertices = 0;
        uint32_t totalIndices = 0;
        uint32_t mossVertices = 0;   // 苔ウェイト (color.r > 0.1) が焼き付けられた頂点数
        uint32_t windVertices = 0;   // 風揺れウェイト (color.g > 0.1) が埋め込まれた頂点数
        float mossRatio = 0.0f;      // 全頂点に対する苔ウェイトの割合 (0.0〜1.0)
        float windRatio = 0.0f;      // 全頂点に対する風揺れウェイトの割合 (0.0〜1.0)
        std::string outputMessage = "";
    };

    // --- LOD一括エクスポート結果構造体 ---
    struct LODExportResult
    {
        bool success = false;
        uint32_t lod0Vertices = 0, lod0Indices = 0;
        uint32_t lod1Vertices = 0, lod1Indices = 0;
        uint32_t lod2Vertices = 0, lod2Indices = 0;
        std::string outputMessage = "";
    };

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
        int maxSegments = 2000;      // 枝の最大生成数 (クランプ用)
        int maxLeaves = 1000;        // 葉の最大生成数 (クランプ用)
        float minBranchRadiusLimit = 0.0f; // この半径未満の枝・葉の生成をスキップする制限値 (LOD用)
    };

    // GPUで並列展開するための樹木の骨格セグメント定義
    struct GPUBranchesSegment {
        Vec3 startPos;
        float startRadius;
        Vec3 endPos;
        float endRadius;
        Vec3 right;
        uint32_t isLeafEmitter;
        Vec3 up;
        float padding[3];
    };

    class BioProceduralGenerator
    {
    public:
        /// <summary>
        /// プロシージャルな岩石メッシュを生成する
        /// </summary>
        static MeshData GenerateRock(const RockParameters& params);

        /// <summary>
        /// プロシージャルな樹木メッシュを生成する (L-System)
        /// </summary>
        static MeshData GenerateTree(const TreeParameters& params);

        /// <summary>
        /// GPU並列生成用の樹木骨格データを生成する
        /// </summary>
        static std::vector<GPUBranchesSegment> GenerateTreeSkeleton(const TreeParameters& params);

        /// <summary>
        /// メッシュデータを標準的なOBJ形式で書き出す (詳細統計結果を返す)
        /// </summary>
        static ExportResult ExportToObj(const std::string& directoryPath, const std::string& fileName, const MeshData& meshData);

        /// <summary>
        /// LOD0〜LOD2のメッシュを自動段階削減して一括でエクスポートする
        /// </summary>
        static LODExportResult ExportLODsToObj(
            const std::string& directoryPath, 
            const std::string& fileName, 
            int mode, // 0: Tree, 1: Rock
            const TreeParameters& treeParams,
            const RockParameters& rockParams,
            const MeshData& lod0BaseMesh
        );

    private:
        // 内部ユーティリティ数学関数
        static float Noise3D(float x, float y, float z);
        static float FractalNoise3D(float x, float y, float z, int octaves, float frequency);
        static float Voronoi3D(float x, float y, float z, int cellCount, unsigned int seed);
        
        static Vec3 RotateVector(const Vec3& v, const Vec3& axis, float angleRad);
        static Vec3 Normalize(const Vec3& v);
        static Vec3 Cross(const Vec3& a, const Vec3& b);
        static float Dot(const Vec3& a, const Vec3& b);
    };
}
