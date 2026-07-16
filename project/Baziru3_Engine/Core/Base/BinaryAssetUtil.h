#pragma once
#include <string>
#include <vector>
#include <map>
#include "Model.h"
#include "Skeleton.h"
#include "AnimationData.h"

namespace BinaryAssetUtil
{
    // キャッシュファイルのパスを取得するヘルパー関数
    std::string GetCachePath(const std::string& originalPath, const std::string& extension);

    // キャッシュが有効（存在し、かつ元ファイルより新しい）かどうかを確認する関数
    bool IsCacheValid(const std::string& originalPath, const std::string& cachePath);

    // モデルデータのシリアライズ / デシリアライズ
    bool SaveBModel(const std::string& cachePath, const Model::ModelData& data);
    bool LoadBModel(const std::string& cachePath, Model::ModelData& outData);

    // スケルトンデータのシリアライズ / デシリアライズ
    bool SaveBSkel(const std::string& cachePath, const Skeleton& data);
    bool LoadBSkel(const std::string& cachePath, Skeleton& outData);

    // アニメーションデータのシリアライズ / デシリアライズ
    bool SaveBAnim(const std::string& cachePath, const Animation& data);
    bool LoadBAnim(const std::string& cachePath, Animation& outData);
}
