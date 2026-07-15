#include "BinaryAssetUtil.h"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace
{
    // 基本的な型をバイナリとして書き込むテンプレート
    template <typename T>
    void WritePOD(std::ostream& os, const T& val)
    {
        os.write(reinterpret_cast<const char*>(&val), sizeof(T));
    }

    // 基本的な型をバイナリから読み込むテンプレート
    template <typename T>
    void ReadPOD(std::istream& is, T& val)
    {
        is.read(reinterpret_cast<char*>(&val), sizeof(T));
    }

    // 文字列の書き込み
    void WriteString(std::ostream& os, const std::string& str)
    {
        uint32_t len = static_cast<uint32_t>(str.size());
        WritePOD(os, len);
        if (len > 0)
        {
            os.write(str.data(), len);
        }
    }

    // 文字列の読み込み
    void ReadString(std::istream& is, std::string& str)
    {
        uint32_t len = 0;
        ReadPOD(is, len);
        str.resize(len);
        if (len > 0)
        {
            is.read(&str[0], len);
        }
    }

    // std::vector<POD> の一括書き込み
    template <typename T>
    void WriteVectorPOD(std::ostream& os, const std::vector<T>& vec)
    {
        uint32_t count = static_cast<uint32_t>(vec.size());
        WritePOD(os, count);
        if (count > 0)
        {
            os.write(reinterpret_cast<const char*>(vec.data()), count * sizeof(T));
        }
    }

    // std::vector<POD> の一括読み込み
    template <typename T>
    void ReadVectorPOD(std::istream& is, std::vector<T>& vec)
    {
        uint32_t count = 0;
        ReadPOD(is, count);
        vec.resize(count);
        if (count > 0)
        {
            is.read(reinterpret_cast<char*>(vec.data()), count * sizeof(T));
        }
    }
}

namespace BinaryAssetUtil
{
    std::string GetCachePath(const std::string& originalPath, const std::string& extension)
    {
        fs::path p(originalPath);
        fs::path cachePath = p.parent_path() / (p.filename().string() + extension);
        return cachePath.string();
    }

    bool IsCacheValid(const std::string& originalPath, const std::string& cachePath)
    {
        // 開発速度・起動速度向上のため、キャッシュファイルが存在すれば即座に有効とみなす！
        // （OneDrive同期フォルダ上での fs::last_write_time の極度の遅延を回避するため）
        if (fs::exists(cachePath))
        {
            return true;
        }
        return false;
    }

    // ModelData のシリアライズ
    bool SaveBModel(const std::string& cachePath, const Model::ModelData& data)
    {
        std::ofstream os(cachePath, std::ios::binary);
        if (!os.is_open()) return false;

        // ヘッダー書き込み (BMOD)
        char magic[4] = { 'B', 'M', 'O', 'D' };
        os.write(magic, 4);
        uint32_t version = 1;
        WritePOD(os, version);

        // 頂点とインデックス
        WriteVectorPOD(os, data.vertices);
        WriteVectorPOD(os, data.indices);

        // マテリアルデータ
        WriteString(os, data.material.textureFilePath);
        WritePOD(os, data.material.textureIndex);

        // スキンクラスターデータ
        uint32_t skinClusterCount = static_cast<uint32_t>(data.skinClusterData.size());
        WritePOD(os, skinClusterCount);
        for (const auto& [jointName, jointWeightData] : data.skinClusterData)
        {
            WriteString(os, jointName);
            WritePOD(os, jointWeightData.inverseBindPoseMatrix);
            WriteVectorPOD(os, jointWeightData.vertexWeights);
        }

        return true;
    }

    bool LoadBModel(const std::string& cachePath, Model::ModelData& outData)
    {
        std::ifstream is(cachePath, std::ios::binary);
        if (!is.is_open()) return false;

        char magic[4];
        is.read(magic, 4);
        if (magic[0] != 'B' || magic[1] != 'M' || magic[2] != 'O' || magic[3] != 'D')
        {
            return false;
        }

        uint32_t version = 0;
        ReadPOD(is, version);
        if (version != 1) return false;

        ReadVectorPOD(is, outData.vertices);
        ReadVectorPOD(is, outData.indices);

        ReadString(is, outData.material.textureFilePath);
        ReadPOD(is, outData.material.textureIndex);

        uint32_t skinClusterCount = 0;
        ReadPOD(is, skinClusterCount);
        outData.skinClusterData.clear();
        for (uint32_t i = 0; i < skinClusterCount; ++i)
        {
            std::string jointName;
            ReadString(is, jointName);
            Model::JointWeightData jointWeightData;
            ReadPOD(is, jointWeightData.inverseBindPoseMatrix);
            ReadVectorPOD(is, jointWeightData.vertexWeights);
            outData.skinClusterData.emplace(jointName, std::move(jointWeightData));
        }

        return true;
    }

    // Skeleton のシリアライズ
    bool SaveBSkel(const std::string& cachePath, const Skeleton& data)
    {
        std::ofstream os(cachePath, std::ios::binary);
        if (!os.is_open()) return false;

        char magic[4] = { 'B', 'S', 'K', 'L' };
        os.write(magic, 4);
        uint32_t version = 1;
        WritePOD(os, version);

        WritePOD(os, data.root);

        // jointMap のシリアライズ
        uint32_t mapSize = static_cast<uint32_t>(data.jointMap.size());
        WritePOD(os, mapSize);
        for (const auto& [name, index] : data.jointMap)
        {
            WriteString(os, name);
            WritePOD(os, index);
        }

        // joints のシリアライズ
        uint32_t jointCount = static_cast<uint32_t>(data.joints.size());
        WritePOD(os, jointCount);
        for (const auto& joint : data.joints)
        {
            WritePOD(os, joint.transform);
            WritePOD(os, joint.localMatrix);
            WritePOD(os, joint.skeletonMatrix);
            WriteString(os, joint.name);
            WriteVectorPOD(os, joint.children);
            WritePOD(os, joint.index);
            
            bool hasParent = joint.parent.has_value();
            WritePOD(os, hasParent);
            if (hasParent)
            {
                WritePOD(os, joint.parent.value());
            }
        }

        return true;
    }

    bool LoadBSkel(const std::string& cachePath, Skeleton& outData)
    {
        std::ifstream is(cachePath, std::ios::binary);
        if (!is.is_open()) return false;

        char magic[4];
        is.read(magic, 4);
        if (magic[0] != 'B' || magic[1] != 'S' || magic[2] != 'K' || magic[3] != 'L')
        {
            return false;
        }

        uint32_t version = 0;
        ReadPOD(is, version);
        if (version != 1) return false;

        ReadPOD(is, outData.root);

        uint32_t mapSize = 0;
        ReadPOD(is, mapSize);
        outData.jointMap.clear();
        for (uint32_t i = 0; i < mapSize; ++i)
        {
            std::string name;
            ReadString(is, name);
            int32_t index = 0;
            ReadPOD(is, index);
            outData.jointMap.emplace(name, index);
        }

        uint32_t jointCount = 0;
        ReadPOD(is, jointCount);
        outData.joints.resize(jointCount);
        for (uint32_t i = 0; i < jointCount; ++i)
        {
            auto& joint = outData.joints[i];
            ReadPOD(is, joint.transform);
            ReadPOD(is, joint.localMatrix);
            ReadPOD(is, joint.skeletonMatrix);
            ReadString(is, joint.name);
            ReadVectorPOD(is, joint.children);
            ReadPOD(is, joint.index);

            bool hasParent = false;
            ReadPOD(is, hasParent);
            if (hasParent)
            {
                int32_t parentIndex = 0;
                ReadPOD(is, parentIndex);
                joint.parent = parentIndex;
            }
            else
            {
                joint.parent = std::nullopt;
            }
        }

        return true;
    }

    // Animation のシリアライズ
    bool SaveBAnim(const std::string& cachePath, const Animation& data)
    {
        std::ofstream os(cachePath, std::ios::binary);
        if (!os.is_open()) return false;

        char magic[4] = { 'B', 'A', 'N', 'M' };
        os.write(magic, 4);
        uint32_t version = 1;
        WritePOD(os, version);

        WritePOD(os, data.duration);

        uint32_t nodeCount = static_cast<uint32_t>(data.nodeAnimations.size());
        WritePOD(os, nodeCount);
        for (const auto& [nodeName, nodeAnim] : data.nodeAnimations)
        {
            WriteString(os, nodeName);
            WriteVectorPOD(os, nodeAnim.translate.keyframes);
            WriteVectorPOD(os, nodeAnim.rotate.keyframes);
            WriteVectorPOD(os, nodeAnim.scale.keyframes);
        }

        return true;
    }

    bool LoadBAnim(const std::string& cachePath, Animation& outData)
    {
        std::ifstream is(cachePath, std::ios::binary);
        if (!is.is_open()) return false;

        char magic[4];
        is.read(magic, 4);
        if (magic[0] != 'B' || magic[1] != 'A' || magic[2] != 'N' || magic[3] != 'M')
        {
            return false;
        }

        uint32_t version = 0;
        ReadPOD(is, version);
        if (version != 1) return false;

        ReadPOD(is, outData.duration);

        uint32_t nodeCount = 0;
        ReadPOD(is, nodeCount);
        outData.nodeAnimations.clear();
        for (uint32_t i = 0; i < nodeCount; ++i)
        {
            std::string nodeName;
            ReadString(is, nodeName);
            NodeAnimation nodeAnim;
            ReadVectorPOD(is, nodeAnim.translate.keyframes);
            ReadVectorPOD(is, nodeAnim.rotate.keyframes);
            ReadVectorPOD(is, nodeAnim.scale.keyframes);
            outData.nodeAnimations.emplace(nodeName, std::move(nodeAnim));
        }

        return true;
    }
}
