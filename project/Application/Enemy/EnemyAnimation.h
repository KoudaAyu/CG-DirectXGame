#pragma once

#include <string>
#include <vector>

#include "Baziru3_Engine/Framework/Animation/AnimationData.h"
#include "Baziru3_Engine/Framework/Animation/Skeleton/Skeleton.h"
#include "Baziru3_Engine/Graphics/3D/Model/Model.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"

class Object3dCom;
class Camera;

/**
 * @brief 敵モデルのマルチクリップアニメーション（engine 無改変）
 *
 * engine の `LoadAnimationFile()` は `scene->mAnimations[0]` の1本しか読まないので、
 * assimp を直接叩いて全クリップを読むローダーをここに置いている。
 *
 * 再生も engine の `Animator` は使わない。理由:
 *   - `Object3d` は `animator_` を公開していないので、外からクリップを切り替えられない
 *   - `Object3d::SetupAnimation()` を呼び直すと `CreateSkinCluster()` が走り、
 *     SRV ディスクリプタが切り替えのたびにリークする
 *
 * 代わりに `Object3d::SetupAnimation(nullptr, skeleton, modelData)` で
 * **スケルトンと SkinCluster だけ**作らせ（animator は無効のまま）、
 * 毎フレーム `Object3d::GetSkeleton().ApplyAnimation(clip, time)` を自分で呼ぶ。
 * `Object3d::Update()` がその後で `skeleton_.Update()` と
 * `skinClusterLender_.Update()` をやってくれるので、これで最後まで通る。
 * 再生時刻を自分で持てるので、ワンショット再生・速度変更も素直に書ける。
 *
 * @note 描画は `Object3d::Draw()`（引数なしの2引数オーバーロード）を使うこと。
 *       `Object3d::Draw(const RenderContext&)` は **スキニング経路に入らない**。
 */

/// @brief 1本ぶんのアニメーションクリップ
struct EnemyAnimationClip
{
    std::string name;      //!< gltf 内のクリップ名（例 "Idle" / "walk"）
    Animation animation;   //!< キーフレーム本体
};

/// @brief モデル1つぶんのアニメーション資産（パスごとにキャッシュされる）
struct EnemyAnimationAsset
{
    bool valid = false;                    //!< スキン・スケルトン・クリップが揃っているか
    std::string key;                       //!< "dir/file"
    Model::ModelData modelData;            //!< skinClusterData 付き（SetupAnimation に渡す）
    Object3d::ModelData object3dModel;     //!< 上と同じ頂点順で作った Object3d 用
    Skeleton skeleton;
    std::vector<EnemyAnimationClip> clips;

    /// @brief クリップ名から番号を引く（大文字小文字は無視）。無ければ -1
    int FindClip(const std::string& name) const;
};

/**
 * @brief アニメーション資産を読み込む（同じパスは1回だけ読んで使い回す）
 * @param directoryPath 例 "Resources/Enemy/Slime"
 * @param fileName 例 "slime.gltf"
 * @return 資産へのポインタ。読めなかった場合も非 null だが `valid == false`
 */
const EnemyAnimationAsset* LoadEnemyAnimationAsset(const std::string& directoryPath, const std::string& fileName);

/**
 * @brief スキン付き Object3d のプール（取得）
 *
 * `Object3d::SetupAnimation()` が中で呼ぶ `SkinClusterLender::CreateSkinCluster()` は
 * SRV ディスクリプタを **4個** 確保するが、engine 側に解放する仕組みが無い
 * （`SkinCluster` はハンドルしか持たず、確保したインデックスを覚えていない）。
 * つまりスキン付き Object3d を作っては捨てるとディスクリプタが減り続け、
 * SRV ヒープ（8192個・テクスチャと共用）を食い潰すと最後はデバイスロストになる。
 *
 * そこでモデルごとに Object3d を使い回す。ディスクリプタの消費はプールの実体数だけで済む。
 *
 * @param asset 対象モデルのアニメーション資産（valid であること）
 * @param object3dCom 3D描画コンポーネント
 * @param camera カメラ
 * @return 使える Object3d。失敗時 nullptr。所有権はプール側にある
 */
Object3d* AcquireSkinnedObject3d(const EnemyAnimationAsset* asset, Object3dCom* object3dCom, Camera* camera);

/**
 * @brief スキン付き Object3d をプールへ返す（破棄はしない）
 * @param asset 取得時と同じ資産
 * @param object 返す Object3d
 */
void ReleaseSkinnedObject3d(const EnemyAnimationAsset* asset, Object3d* object);

/// @brief プールが抱えている Object3d の総数（ImGui 表示用）
int GetSkinnedObject3dPoolTotal();

/**
 * @brief 1体ぶんの再生状態
 *
 * `Object3d` が持つスケルトンは `SetupAnimation()` でコピーされた個体ごとの実体なので、
 * 敵ごとにバラバラの時刻・クリップで再生できる。
 */
class EnemyAnimator
{
public:
    /// @brief 対象の Object3d と資産を結びつける（Object3d::SetupAnimation の後に呼ぶ）
    void Initialize(Object3d* object, const EnemyAnimationAsset* asset);

    bool IsValid() const { return object_ != nullptr && asset_ != nullptr && asset_->valid; }

    /**
     * @brief ループ再生に切り替える
     * @param clipName クリップ名。空文字や未知の名前は無視される
     * @note ワンショット再生中は無視される（そちらが終わってから戻る）
     */
    void Play(const std::string& clipName);

    /**
     * @brief 1回だけ再生して、終わったら別のクリップへ戻る
     * @param clipName 再生するクリップ名
     * @param returnClipName 終わったあとに戻るクリップ名（空ならその場で止まる）
     */
    void PlayOneShot(const std::string& clipName, const std::string& returnClipName);

    /// @brief 時刻を進める
    void Update(float deltaTime);

    /// @brief 現在時刻のポーズをスケルトンへ流し込む（Object3d::Update() の前に呼ぶ）
    void ApplyToSkeleton();

    void SetSpeed(float speed) { speed_ = speed; }
    float GetSpeed() const { return speed_; }

    bool IsOneShotPlaying() const { return !loop_ && !finished_; }
    const char* GetCurrentClipName() const;

private:
    int ResolveClip(const std::string& name) const;
    float CurrentDuration() const;

private:
    Object3d* object_ = nullptr;
    const EnemyAnimationAsset* asset_ = nullptr;

    int currentIndex_ = -1;
    int returnIndex_ = -1;
    float time_ = 0.0f;
    float speed_ = 1.0f;
    bool loop_ = true;
    bool finished_ = false;
};
