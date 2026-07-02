#include "ProceduralGenerator.h"
#include "BioProceduralGenerator.h"
#include <algorithm>

namespace
{
    // BioProcedural::MeshData から Object3d::ModelData への変換
    Object3d::ModelData ConvertToModelData(const BioProcedural::MeshData& src)
    {
        Object3d::ModelData dst;
        dst.vertices.reserve(src.vertices.size());
        
        for (const auto& sVert : src.vertices)
        {
            Sprite::VertexData dVert{};
            dVert.position = { sVert.position.x, sVert.position.y, sVert.position.z, 1.0f };
            dVert.normal = { sVert.normal.x, sVert.normal.y, sVert.normal.z };
            dVert.texcoord = { sVert.texcoord.u, sVert.texcoord.v };
            dst.vertices.push_back(dVert);
        }

        dst.indices = src.indices;
        return dst;
    }

    // Object3d::ModelData から BioProcedural::MeshData への変換
    BioProcedural::MeshData ConvertToMeshData(const Object3d::ModelData& src)
    {
        BioProcedural::MeshData dst;
        dst.vertices.reserve(src.vertices.size());

        for (const auto& sVert : src.vertices)
        {
            BioProcedural::Vertex dVert{};
            dVert.position = { sVert.position.x, sVert.position.y, sVert.position.z };
            dVert.normal = { sVert.normal.x, sVert.normal.y, sVert.normal.z };
            dVert.texcoord = { sVert.texcoord.x, sVert.texcoord.y };
            dst.vertices.push_back(dVert);
        }

        dst.indices = src.indices;
        dst.texturePath = src.material.textureFilePath; // テクスチャパスの引き継ぎ
        return dst;
    }
}

// --- 岩石のプロシージャル生成アダプター ---
Object3d::ModelData ProceduralGenerator::GenerateRock(const RockParameters& params)
{
    BioProcedural::RockParameters bParams;
    bParams.scale = params.scale;
    bParams.subdivisions = params.subdivisions;
    bParams.noiseStrength = params.noiseStrength;
    bParams.noiseFrequency = params.noiseFrequency;
    bParams.octaves = params.octaves;
    bParams.voronoiStrength = params.voronoiStrength;
    bParams.voronoiCells = params.voronoiCells;
    bParams.crackStrength = params.crackStrength;
    bParams.crackFrequency = params.crackFrequency;
    bParams.seed = params.seed;

    BioProcedural::MeshData bMeshData = BioProcedural::BioProceduralGenerator::GenerateRock(bParams);
    return ConvertToModelData(bMeshData);
}

// --- 樹木のプロシージャル生成アダプター ---
Object3d::ModelData ProceduralGenerator::GenerateTree(const TreeParameters& params)
{
    BioProcedural::TreeParameters bParams;
    bParams.iterations = params.iterations;
    bParams.branchLength = params.branchLength;
    bParams.branchRadius = params.branchRadius;
    bParams.taperRate = params.taperRate;
    bParams.angle = params.angle;
    bParams.axiom = params.axiom;
    bParams.seed = params.seed;
    bParams.radialSegments = params.radialSegments;
    bParams.maxSegments = params.maxSegments;
    bParams.maxLeaves = params.maxLeaves;

    BioProcedural::MeshData bMeshData = BioProcedural::BioProceduralGenerator::GenerateTree(bParams);
    return ConvertToModelData(bMeshData);
}

// --- エクスポート機能のアダプター ---
BioProcedural::ExportResult ProceduralGenerator::ExportToObj(const std::string& directoryPath, const std::string& fileName, const Object3d::ModelData& modelData)
{
    BioProcedural::MeshData bMeshData = ConvertToMeshData(modelData);
    return BioProcedural::BioProceduralGenerator::ExportToObj(directoryPath, fileName, bMeshData);
}

// --- LOD一括エクスポート機能のアダプター ---
BioProcedural::LODExportResult ProceduralGenerator::ExportLODsToObj(
    const std::string& directoryPath, 
    const std::string& fileName, 
    int mode, 
    const TreeParameters& treeParams,
    const RockParameters& rockParams
)
{
    BioProcedural::TreeParameters bTreeParams;
    bTreeParams.iterations = treeParams.iterations;
    bTreeParams.branchLength = treeParams.branchLength;
    bTreeParams.branchRadius = treeParams.branchRadius;
    bTreeParams.taperRate = treeParams.taperRate;
    bTreeParams.angle = treeParams.angle;
    bTreeParams.axiom = treeParams.axiom;
    bTreeParams.seed = treeParams.seed;
    bTreeParams.radialSegments = treeParams.radialSegments;
    bTreeParams.maxSegments = treeParams.maxSegments;
    bTreeParams.maxLeaves = treeParams.maxLeaves;

    BioProcedural::RockParameters bRockParams;
    bRockParams.scale = rockParams.scale;
    bRockParams.subdivisions = rockParams.subdivisions;
    bRockParams.noiseStrength = rockParams.noiseStrength;
    bRockParams.noiseFrequency = rockParams.noiseFrequency;
    bRockParams.octaves = rockParams.octaves;
    bRockParams.voronoiStrength = rockParams.voronoiStrength;
    bRockParams.voronoiCells = rockParams.voronoiCells;
    bRockParams.crackStrength = rockParams.crackStrength;
    bRockParams.crackFrequency = rockParams.crackFrequency;
    bRockParams.seed = rockParams.seed;

    return BioProcedural::BioProceduralGenerator::ExportLODsToObj(directoryPath, fileName, mode, bTreeParams, bRockParams);
}
