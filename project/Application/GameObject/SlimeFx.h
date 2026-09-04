#pragma once

#include "Vector.h"
#include "RenderContext.h"
#include "Baziru3_Engine/Graphics/3D/Object/Object3d.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

class Object3dCom;
class Camera;

/// <summary>パーティクル1粒の見た目の種類</summary>
enum class SlimeFxShape
{
    Billboard, // カメラに正対する板（キラキラ・しずく・バースト）
    Ground,    // 地面に寝かせた板（移動軌跡の「濡れた跡」）
};

/// <summary>パーティクル1粒の発生パラメータ</summary>
struct SlimeFxDesc
{
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    float gravity = 0.0f;   // 毎秒の下向き加速度（正の値で下に落ちる）
    float drag = 0.0f;      // 速度の減衰率（1/秒）
    Vector4 colorBegin{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
    float scaleBegin = 0.2f;
    float scaleEnd = 0.0f;
    float scaleAspect = 1.0f; // 横／縦の比。1.0 で正方形、大きいと横長
    float lifeTime = 0.6f;
    SlimeFxShape shape = SlimeFxShape::Billboard;
    bool useSparkTexture = false; // true: starburst（キラッ） / false: circle2（ぼんやり丸）
};

/// <summary>
/// タイトル演出用の軽量パーティクル。
///
/// engine の ParticleManager は使っていない。
/// 理由: ParticleManager::Draw() が CPU 側リスト（AddParticles）ではなく
/// GPU パーティクルバッファをバインドする作りで、CPU から追加した粒は描画されない。
/// さらに GPU 側の発生は EmitParticle.CS.hlsl の中で色・速度・寿命が
/// ハードコードされていて、演出ごとに作り分けができない。
///
/// ここでは Object3d の板ポリを固定数プールして自前で回している。
/// 描画は Object3dCom の RootSignature ＋ Object3D_Effect PSO
/// （アルファブレンド有効・デプス書き込み無効）を使うので、
/// 粒同士が不自然に隠し合わず、3D の前後関係だけは正しく出る。
/// </summary>
class SlimeFx
{
public:
    SlimeFx() = default;
    ~SlimeFx();

    void Initialize(Object3dCom* object3dCom, Camera* camera, uint32_t capacity = 192);
    void Finalize();

    /// <summary>ビルボードの向きに使うカメラのピッチ（カメラの yaw / roll は 0 前提）</summary>
    void SetCameraPitch(float pitch) { cameraPitch_ = pitch; }

    void Update(float deltaTime);
    void Draw(const RenderContext& ctx);
    void Clear();

    /// <summary>1粒だけ出す。空きが無ければ何もしない</summary>
    void Emit(const SlimeFxDesc& desc);

    // --- 用途別ヘルパー ---

    /// <summary>球状にランダムに散らしたキラキラ。ゆっくり上に昇る</summary>
    void EmitSparkle(std::mt19937& rng, const Vector3& center, float radius, int count,
                     const Vector4& color, float scale, float lifeTime);

    /// <summary>水平方向に円周状、少し上向きに飛び散るバースト</summary>
    void EmitBurst(std::mt19937& rng, const Vector3& center, int count, float speed,
                   float upSpeed, const Vector4& color, float scale, float lifeTime,
                   bool spark);

    /// <summary>from から to へ吸い寄せられる粒（マージの吸収表現）</summary>
    void EmitConverge(std::mt19937& rng, const Vector3& from, const Vector3& to, int count,
                      const Vector4& color, float scale, float lifeTime);

    /// <summary>2点の間に粒を並べる（分裂直後の「粘りの糸」）</summary>
    void EmitStrand(std::mt19937& rng, const Vector3& from, const Vector3& to, int count,
                    const Vector4& color, float scale, float lifeTime);

    /// <summary>単発のしずく</summary>
    void EmitDroplet(std::mt19937& rng, const Vector3& position, const Vector3& velocity,
                     const Vector4& color, float scale, float lifeTime);

    /// <summary>地面に寝かせた跡（移動軌跡）</summary>
    void EmitGroundMark(std::mt19937& rng, const Vector3& position, float scale,
                        const Vector4& color, float lifeTime);

    int GetActiveCount() const { return activeCount_; }
    uint32_t GetCapacity() const { return static_cast<uint32_t>(particles_.size()); }

private:
    struct Particle
    {
        bool isActive = false;
        bool isParked = false; // 消えたあと Object3d を片付け済みか（毎フレーム触らないため）
        Vector3 position{};
        Vector3 velocity{};
        float gravity = 0.0f;
        float drag = 0.0f;
        Vector4 colorBegin{};
        Vector4 colorEnd{};
        float scaleBegin = 0.0f;
        float scaleEnd = 0.0f;
        float scaleAspect = 1.0f;
        float lifeTime = 1.0f;
        float age = 0.0f;
        SlimeFxShape shape = SlimeFxShape::Billboard;
        bool useSparkTexture = false;
    };

    /// <summary>XY 平面に立てた 1x1 の板を作る</summary>
    static Object3d::ModelData MakeQuadMesh();

    /// <summary>同じテクスチャの粒だけまとめて描く</summary>
    void DrawGroup(const RenderContext& ctx, bool spark);

    int FindFreeIndex();

private:
    Object3dCom* object3dCom_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<Particle> particles_;
    std::vector<std::unique_ptr<Object3d>> objects_;
    Object3d::ModelData quadModelData_;

    uint32_t softTextureIndex_ = 0;  // ぼんやり丸
    uint32_t sparkTextureIndex_ = 0; // キラッ

    float cameraPitch_ = 0.0f;
    int activeCount_ = 0;
    int nextSearchIndex_ = 0;
};
