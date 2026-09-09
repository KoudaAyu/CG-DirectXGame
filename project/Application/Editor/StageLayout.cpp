#include "Application/Editor/StageLayout.h"

#include "Application/GameObject/SlimePhysics.h"
#include "externals/nlohmann/json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

const char* StageLayout::kDefaultPath = "Resources/10days/stage_layout_10days.json";
const char* StageLayout::kTerrainDirectory = "Resources/10days";

namespace StagePlacement
{
    /// @brief 配置に必要な頭上クリアランス (m)。これより天井が低い場所は配置禁止
    static constexpr float kRequiredHeadroom = 2.5f;

    Result Test(float x, float z, float* outY)
    {
        SlimePhysics::GroundLayer layers[8];
        int count = SlimePhysics::QueryGroundLayers(x, z, layers, 8);

        if (count <= 0)
        {
            if (outY) *outY = 0.0f;
            return Result::NoGround;
        }

        // 一番上の床（QueryGroundLayers は Y 降順）＝プレイ面。
        //
        // 【変更の理由】以前は「2層あったら下段がプレイ area」という判定だった。
        // これは旧地形（下段の広場＋その上に一本道が架かっている）に合わせたもの。
        // いまの島（startLand / Land1）は**全域が上下2段**なので、その規則では
        // 実際に遊ぶ上面が丸ごと BLOCKED になり、置ける場所が全体の 6% しか残らない。
        // 上面を採り、代わりに「頭上が詰まっているか」で禁止を判断する
        const float floorY = layers[0].y;
        if (outY) *outY = floorY;

        // 頭上クリアランス判定。オーバーハングや上下段の隙間へ潜り込ませないため。
        // 探索距離を必要クリアランスの2倍で打ち切って、遠い地形を誤検出しないようにする
        float ceilingY = 0.0f;
        if (SlimePhysics::FindCeilingY(x, z, floorY, kRequiredHeadroom * 2.0f, ceilingY))
        {
            if ((ceilingY - floorY) < kRequiredHeadroom)
            {
                return Result::Obstructed;
            }
        }

        return Result::Ok;
    }

    const char* ResultLabel(Result r)
    {
        switch (r)
        {
        // ImGui のフォントに日本語グリフが無いので、表示文字列は全部 ASCII にすること
        case Result::Ok:         return "OK";
        case Result::NoGround:   return "NO GROUND (outside island)";
        case Result::Obstructed: return "BLOCKED (no headroom)";
        default:                 return "?";
        }
    }
}

void StageLayout::Clear()
{
    terrain.clear();
    playerStart = { 0.0f, 0.0f, 0.0f };
    enemies.clear();
    coins.clear();
    growthCubes.clear();
    boss = StageBossEntry{};
}

const char* StageLayout::TypeToName(EnemyType type)
{
    switch (type)
    {
    case EnemyType::Slime:         return "Slime";
    case EnemyType::FlowerClover:  return "FlowerClover";
    case EnemyType::FlowerLotus:   return "FlowerLotus";
    case EnemyType::FlowerSunward: return "FlowerSunward";
    default:                       return "Slime";
    }
}

bool StageLayout::NameToType(const std::string& name, EnemyType& outType)
{
    if (name == "Slime")              { outType = EnemyType::Slime;         return true; }
    if (name == "FlowerClover")       { outType = EnemyType::FlowerClover;  return true; }
    if (name == "FlowerLotus")        { outType = EnemyType::FlowerLotus;   return true; }
    if (name == "FlowerSunward")      { outType = EnemyType::FlowerSunward; return true; }
    return false;
}

namespace
{
    nlohmann::json ToJson(const Vector3& v)
    {
        nlohmann::json j;
        j["x"] = v.x;
        j["y"] = v.y;
        j["z"] = v.z;
        return j;
    }

    Vector3 FromJson(const nlohmann::json& j, const Vector3& fallback)
    {
        Vector3 v = fallback;
        if (j.is_object())
        {
            if (j.contains("x") && j["x"].is_number()) v.x = j["x"].get<float>();
            if (j.contains("y") && j["y"].is_number()) v.y = j["y"].get<float>();
            if (j.contains("z") && j["z"].is_number()) v.z = j["z"].get<float>();
        }
        return v;
    }

    /// @brief {"position": {...}} でも {"x":..,"y":..,"z":..} でも読めるようにする（手書き用の保険）
    Vector3 ReadPosition(const nlohmann::json& j)
    {
        if (j.is_object() && j.contains("position"))
        {
            return FromJson(j["position"], { 0.0f, 0.0f, 0.0f });
        }
        return FromJson(j, { 0.0f, 0.0f, 0.0f });
    }

    float ReadFloat(const nlohmann::json& j, const char* key, float fallback)
    {
        if (j.is_object() && j.contains(key) && j[key].is_number()) return j[key].get<float>();
        return fallback;
    }

    int ReadInt(const nlohmann::json& j, const char* key, int fallback)
    {
        if (j.is_object() && j.contains(key) && j[key].is_number_integer()) return j[key].get<int>();
        return fallback;
    }

    bool ReadBool(const nlohmann::json& j, const char* key, bool fallback)
    {
        if (j.is_object() && j.contains(key) && j[key].is_boolean()) return j[key].get<bool>();
        return fallback;
    }

    std::string ReadString(const nlohmann::json& j, const char* key, const char* fallback)
    {
        if (j.is_object() && j.contains(key) && j[key].is_string()) return j[key].get<std::string>();
        return fallback;
    }
}

std::vector<StageTerrainEntry> StageLayout::MakeDefaultTerrain()
{
    // 以前 GamePlayScene::InitializeScene() にハードコードされていた配置。
    // groundScale_ = 0.25 / bridgeConnectMode_ = true 相当。
    //
    // 旧コードは baseOffset を「モデルローカル単位」で持ち、
    //   SetTranslate(baseOffset * groundScale_)
    // していたので、ここではワールド座標に直した値を入れてある
    //   45.563018 * 0.25 = 11.3907545
    const float kScale = 0.25f;
    const float kStepWorld = 45.563018f * kScale;

    std::vector<StageTerrainEntry> list;

    auto Add = [&](const char* mesh, const char* tex, float x, bool bossTrigger = false)
    {
        StageTerrainEntry e;
        e.mesh = mesh;
        e.texture = tex;
        e.position = { x, 0.0f, 0.0f };
        e.rotationY = 0.0f;
        e.scale = kScale;
        e.bossTrigger = bossTrigger;
        list.push_back(e);
    };

    Add("startLand.obj",  "Resources/10days/land.png",  0.0f);

    // 【既定のボス戦トリガー】橋を渡った先の島（Land1）に踏み入れるとボス戦が始まる。
    // 配置エディタ（F2）の Terrain レイヤーで、別のメッシュに付け替えられる
    Add("Land1.obj",      "Resources/10days/land2.png", 0.0f, true);

    Add("toLandRoad.obj", "Resources/10days/land.png",  0.0f);

    // 道ユニットセル: 基本幅 45.563m を6枚つないで島の間を渡れるようにする
    for (int i = 0; i <= 5; ++i)
    {
        Add("roadCell.obj", "Resources/10days/land.png", kStepWorld * static_cast<float>(i));
    }

    return list;
}

bool StageLayout::LoadFromFile(const std::string& path)
{
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;

    nlohmann::json j;
    try
    {
        ifs >> j;
    }
    catch (...)
    {
        // 壊れた JSON。呼び出し側がフォールバックへ回れるよう false を返すだけにする
        return false;
    }

    if (!j.is_object()) return false;

    StageLayout loaded;

    // --- 地形 ---
    if (j.contains("terrain") && j["terrain"].is_array())
    {
        for (const auto& t : j["terrain"])
        {
            if (!t.is_object()) continue;

            StageTerrainEntry entry;
            entry.mesh = ReadString(t, "mesh", "");
            if (entry.mesh.empty()) continue; // メッシュ名が無いエントリは捨てる

            entry.texture = ReadString(t, "texture", "");
            entry.position = ReadPosition(t);
            entry.rotationY = ReadFloat(t, "rotationY", 0.0f);
            entry.scale = ReadFloat(t, "scale", 0.25f);
            if (entry.scale <= 0.0001f) entry.scale = 0.25f;
            entry.bossTrigger = ReadBool(t, "bossTrigger", false);

            loaded.terrain.push_back(entry);
        }
    }

    if (j.contains("player"))
    {
        loaded.playerStart = FromJson(j["player"], { 0.0f, 0.0f, 0.0f });
    }

    if (j.contains("enemies") && j["enemies"].is_array())
    {
        for (const auto& e : j["enemies"])
        {
            if (!e.is_object()) continue;

            StageEnemyEntry entry;

            std::string typeName = ReadString(e, "type", "Slime");
            if (!NameToType(typeName, entry.type)) continue; // 知らない種類は捨てる

            entry.position = ReadPosition(e);
            entry.strength = ReadInt(e, "strength", -1);

            loaded.enemies.push_back(entry);
        }
    }

    if (j.contains("coins") && j["coins"].is_array())
    {
        for (const auto& c : j["coins"])
        {
            StageCoinEntry entry;
            entry.position = ReadPosition(c);
            loaded.coins.push_back(entry);
        }
    }

    if (j.contains("growthCubes") && j["growthCubes"].is_array())
    {
        for (const auto& g : j["growthCubes"])
        {
            StageGrowthCubeEntry entry;
            entry.position = ReadPosition(g);
            entry.size = ReadFloat(g, "size", 0.85f);
            if (entry.size <= 0.01f) entry.size = 0.85f;
            loaded.growthCubes.push_back(entry);
        }
    }

    if (j.contains("boss") && j["boss"].is_object())
    {
        loaded.boss.enabled = ReadBool(j["boss"], "enabled", true);
        loaded.boss.position = ReadPosition(j["boss"]);
        loaded.boss.hp = ReadInt(j["boss"], "hp", 100);
        if (loaded.boss.hp <= 0) loaded.boss.hp = 100;
    }

    *this = loaded;
    return true;
}

bool StageLayout::SaveToFile(const std::string& path) const
{
    // 保存先のディレクトリが無ければ作る
    try
    {
        std::filesystem::path fsPath(path);
        if (fsPath.has_parent_path() && !fsPath.parent_path().empty())
        {
            std::filesystem::create_directories(fsPath.parent_path());
        }
    }
    catch (...)
    {
        // ディレクトリが作れなくても、既にあるなら書ける可能性がある。続行
    }

    nlohmann::json j;
    j["version"] = 2; // 2 で terrain / growthCubes / boss が入った

    nlohmann::json terrainArray = nlohmann::json::array();
    for (const auto& t : terrain)
    {
        nlohmann::json item;
        item["mesh"] = t.mesh;
        item["texture"] = t.texture;
        item["position"] = ToJson(t.position);
        item["rotationY"] = t.rotationY;
        item["scale"] = t.scale;
        item["bossTrigger"] = t.bossTrigger;
        terrainArray.push_back(item);
    }
    j["terrain"] = terrainArray;

    j["player"] = ToJson(playerStart);

    nlohmann::json enemyArray = nlohmann::json::array();
    for (const auto& e : enemies)
    {
        nlohmann::json item;
        item["type"] = TypeToName(e.type);
        item["position"] = ToJson(e.position);
        item["strength"] = e.strength;
        enemyArray.push_back(item);
    }
    j["enemies"] = enemyArray;

    nlohmann::json coinArray = nlohmann::json::array();
    for (const auto& c : coins)
    {
        nlohmann::json item;
        item["position"] = ToJson(c.position);
        coinArray.push_back(item);
    }
    j["coins"] = coinArray;

    nlohmann::json cubeArray = nlohmann::json::array();
    for (const auto& g : growthCubes)
    {
        nlohmann::json item;
        item["position"] = ToJson(g.position);
        item["size"] = g.size;
        cubeArray.push_back(item);
    }
    j["growthCubes"] = cubeArray;

    nlohmann::json bossObject;
    bossObject["enabled"] = boss.enabled;
    bossObject["position"] = ToJson(boss.position);
    bossObject["hp"] = boss.hp;
    j["boss"] = bossObject;

    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << j.dump(2);
    return ofs.good();
}

bool StageLayout::IsCompatibleWithCurrentTerrain(float* outValidRatio) const
{
    const size_t total = enemies.size() + coins.size() + growthCubes.size();
    if (total == 0)
    {
        // 空のレイアウトは「壊れている」とは言えないので、そのまま通す
        if (outValidRatio) *outValidRatio = 1.0f;
        return true;
    }

    size_t onGround = 0;
    for (const auto& e : enemies)
    {
        if (SlimePhysics::QueryGroundLayers(e.position.x, e.position.z, nullptr, 0) > 0) ++onGround;
    }
    for (const auto& c : coins)
    {
        if (SlimePhysics::QueryGroundLayers(c.position.x, c.position.z, nullptr, 0) > 0) ++onGround;
    }
    for (const auto& g : growthCubes)
    {
        if (SlimePhysics::QueryGroundLayers(g.position.x, g.position.z, nullptr, 0) > 0) ++onGround;
    }

    const float ratio = static_cast<float>(onGround) / static_cast<float>(total);
    if (outValidRatio) *outValidRatio = ratio;

    // 半分以上が島の外に出ていたら、別の地形で作ったデータとみなす
    return ratio >= 0.5f;
}

StageLayout StageLayout::MakeFallback(int enemyCount, int coinCount, int growthCubeCount)
{
    StageLayout layout;

    Vector3 bmin, bmax;
    if (!SlimePhysics::GetGroundWorldBounds(bmin, bmax))
    {
        // 地形が未登録。原点だけ返す（敵・コインは置かない）
        return layout;
    }

    // 2m 刻みでステージ全体を走査して「置けるセル」を集める。
    // 座標のハードコードを避けることで、地形を差し替えても壊れない
    const float kStep = 2.0f;
    struct Cell { float x; float z; float y; };
    std::vector<Cell> valid;

    for (float z = bmin.z; z <= bmax.z; z += kStep)
    {
        for (float x = bmin.x; x <= bmax.x; x += kStep)
        {
            float y = 0.0f;
            if (StagePlacement::Test(x, z, &y) == StagePlacement::Result::Ok)
            {
                valid.push_back({ x, z, y });
            }
        }
    }

    if (valid.empty())
    {
        return layout;
    }

    // 置けるセルの重心に一番近いセルをプレイヤー初期位置にする
    float cx = 0.0f, cz = 0.0f;
    for (const auto& c : valid) { cx += c.x; cz += c.z; }
    cx /= static_cast<float>(valid.size());
    cz /= static_cast<float>(valid.size());

    const Cell* center = &valid[0];
    float bestDist = 1e18f;
    for (const auto& c : valid)
    {
        float d = (c.x - cx) * (c.x - cx) + (c.z - cz) * (c.z - cz);
        if (d < bestDist) { bestDist = d; center = &c; }
    }

    layout.playerStart = { center->x, center->y, center->z };

    // プレイヤーからの距離順に並べて、近いところにコイン・遠いところに敵を撒く
    std::vector<const Cell*> sorted;
    sorted.reserve(valid.size());
    for (const auto& c : valid) sorted.push_back(&c);

    const float pxx = center->x;
    const float pzz = center->z;
    std::sort(sorted.begin(), sorted.end(), [pxx, pzz](const Cell* a, const Cell* b) {
        float da = (a->x - pxx) * (a->x - pxx) + (a->z - pzz) * (a->z - pzz);
        float db = (b->x - pxx) * (b->x - pxx) + (b->z - pzz) * (b->z - pzz);
        return da < db;
    });

    auto Distance = [pxx, pzz](const Cell* c) {
        return std::sqrt((c->x - pxx) * (c->x - pxx) + (c->z - pzz) * (c->z - pzz));
    };

    // コイン: プレイヤーから 4m〜18m。1つ飛ばしで拾って固まらせない
    int placedCoins = 0;
    for (size_t i = 0; i < sorted.size() && placedCoins < coinCount; ++i)
    {
        float d = Distance(sorted[i]);
        if (d < 4.0f || d > 18.0f) continue;
        if ((i % 3) != 0) continue;
        layout.coins.push_back({ { sorted[i]->x, sorted[i]->y, sorted[i]->z } });
        ++placedCoins;
    }

    // 成長キューブ: プレイヤーから 6m〜24m。コインと同じセルを踏まないよう位相をずらす
    int placedCubes = 0;
    for (size_t i = 0; i < sorted.size() && placedCubes < growthCubeCount; ++i)
    {
        float d = Distance(sorted[i]);
        if (d < 6.0f || d > 24.0f) continue;
        if ((i % 7) != 1) continue;

        StageGrowthCubeEntry entry;
        entry.position = { sorted[i]->x, sorted[i]->y, sorted[i]->z };
        entry.size = 0.85f;
        layout.growthCubes.push_back(entry);
        ++placedCubes;
    }

    // 敵: プレイヤーから 10m 以上離す。種類は順番に回す
    const EnemyType kTypes[] = { EnemyType::Slime, EnemyType::FlowerClover,
                                 EnemyType::FlowerLotus, EnemyType::FlowerSunward };
    int placedEnemies = 0;
    for (size_t i = 0; i < sorted.size() && placedEnemies < enemyCount; ++i)
    {
        float d = Distance(sorted[i]);
        if (d < 10.0f) continue;
        if ((i % 5) != 0) continue;

        StageEnemyEntry entry;
        entry.type = kTypes[placedEnemies % 4];
        entry.position = { sorted[i]->x, sorted[i]->y, sorted[i]->z };
        entry.strength = -1; // ランダム
        layout.enemies.push_back(entry);
        ++placedEnemies;
    }

    // ボス: プレイヤーから一番遠い置けるセル（＝ステージの果て。既定地形なら
    // 橋の向こうの Land1 側になる）に置く。
    // MakeDefaultTerrain() が Land1 にボス戦トリガーを立てているので、
    // 何もしなくても「橋を渡るとボス戦」が成立する状態になる。
    // 位置も HP も配置エディタ（F2）から変えられる
    layout.boss.enabled = true;
    layout.boss.hp = 100;
    layout.boss.position = { sorted.back()->x, sorted.back()->y, sorted.back()->z };

    return layout;
}
