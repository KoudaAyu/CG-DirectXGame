#pragma once

#include "Vector.h"
#include "MappedResource.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <utility>
#include <vector>

class DirectXCom;
class Camera;

/// <summary>パーティクル1粒の見た目の種類</summary>
enum class FireworkFxShape
{
    Billboard, // カメラに正対する板
    Ground,    // 地面に寝かせた板
};

/// <summary>花火1発の見せ方</summary>
enum class FireworkStyle
{
    Normal, // 開いてすぐ散る、いわゆる普通の菊
    Willow, // ゆっくり垂れ下がりながら長く尾を引く「柳」
};

/// <summary>パーティクル1粒の発生パラメータ</summary>
struct FireworkFxDesc
{
    Vector3 position{0.0f, 0.0f, 0.0f};
    Vector3 velocity{0.0f, 0.0f, 0.0f};
    float gravity = 0.0f; // 毎秒の下向き加速度（正の値で下に落ちる）
    float drag = 0.0f;    // 速度の減衰率（1/秒）
    Vector4 colorBegin{1.0f, 1.0f, 1.0f, 1.0f};
    Vector4 colorEnd{1.0f, 1.0f, 1.0f, 0.0f};
    float scaleBegin = 0.2f;
    float scaleEnd = 0.0f;
    float scaleAspect = 1.0f; // 横／縦の比。1.0 で正方形
    float lifeTime = 0.6f;
    FireworkFxShape shape = FireworkFxShape::Billboard;
    bool useSparkTexture = false; // true: starburst（キラッ） / false: circle2（ぼんやり丸）

    /// <summary>RGB を SetColorField() のベクター場から毎フレーム引き直す</summary>
    bool useColorField = false;

    /// <summary>板の縦軸を速度方向に向ける（尾を引く線に見せる）</summary>
    bool alignToVelocity = false;

    // --- 軌跡 ---
    // trailInterval が正のとき、この粒は飛びながら trailInterval 秒おきに
    // 「その場に置き去りにする粒」を撒く。撒かれた粒はさらに軌跡を出さない。
    float trailInterval = 0.0f;
    float trailLifeTime = 0.3f;
    float trailScale = 0.08f;
};

/// <summary>
/// 花火用のバッチ描画パーティクル（このプロジェクトで追加）。
///
/// SlimeFx は粒1個につき Object3d を1個持つ作りで、Object3d は頂点／
/// インデックスバッファを CreateCommittedResource で確保する。
/// D3D12 のコミット済みバッファは 64KB 粒度なので、板ポリ1枚でも 128KB 食う。
/// さらに描画のたびに定数バッファを3個切り出すため、
/// ConstantBufferAllocator の 1フレーム 2.67MB という枠にも当たる。
///   → 4096粒だと 512MB / CB も溢れる。あの方式では 1000粒あたりが天井。
///
/// こちらは全粒を1本の動的頂点バッファへ毎フレーム展開して、
/// テクスチャごとに1回ずつドローコールを出す。
///   4096粒 = 頂点 4096 x 4 x 40B ≒ 640KB、定数バッファは1フレーム1個、
///   ドローコールは2回。10000粒でも余裕がある。
///
/// 粒ごとの色は頂点カラーに載せるので、専用のシェーダを持っている
/// （Resources/shaders/FireworkFx.VS.hlsl / .PS.hlsl）。
/// ルートシグネチャと PSO もこのクラスが自前で作るので、engine には手を入れていない。
///
/// SlimeFx はそのまま残してある（TitleScene が使っている）。
/// </summary>
class FireworkFx
{
public:
    FireworkFx() = default;
    ~FireworkFx();

    FireworkFx(const FireworkFx&) = delete;
    FireworkFx& operator=(const FireworkFx&) = delete;

    /// <summary>初期化。capacity は同時に生きられる粒の数</summary>
    void Initialize(DirectXCom* dxCommon, Camera* camera, uint32_t capacity = 4096);
    void Finalize();

    void SetCamera(Camera* camera) { camera_ = camera; }

    /// <summary>加算合成にするか。false ならアルファブレンド</summary>
    void SetAdditive(bool additive) { isAdditive_ = additive; }
    bool IsAdditive() const { return isAdditive_; }

    /// <summary>PSO の作成に成功しているか。失敗しているとこのクラスは何も描かない</summary>
    bool IsReady() const { return isReady_; }

    /// <summary>
    /// 粒の色を決めるベクター場 color(time, position)。
    /// useColorField を立てた粒だけが、毎フレームこの関数で塗り直される。
    /// 戻り値の xyz が RGB、w は α に掛かる係数。
    /// </summary>
    using ColorField = std::function<Vector4(float, const Vector3&)>;
    void SetColorField(ColorField colorField) { colorField_ = std::move(colorField); }
    void ClearColorField() { colorField_ = nullptr; }
    bool HasColorField() const { return static_cast<bool>(colorField_); }

    float GetElapsedTime() const { return elapsedTime_; }
    void SetElapsedTime(float time) { elapsedTime_ = time; }

    /// <summary>軌跡をまとめて止める／密度を変える（1.0 で既定）</summary>
    void SetTrailEnabled(bool enabled) { isTrailEnabled_ = enabled; }
    bool IsTrailEnabled() const { return isTrailEnabled_; }
    void SetTrailDensity(float density) { trailDensity_ = density; }
    float GetTrailDensity() const { return trailDensity_; }

    void Update(float deltaTime);
    void Draw(ID3D12GraphicsCommandList* commandList);
    void Clear();

    /// <summary>1粒だけ出す。空きが無ければ何もしない</summary>
    void Emit(const FireworkFxDesc& desc);

    // --- 用途別ヘルパー ---

    /// <summary>全方位に飛び散るバースト。花火の殻</summary>
    void EmitSphereBurst(std::mt19937& rng, const Vector3& center, int count, float speed,
                         float gravity, float drag, const Vector4& color, float scale,
                         float lifeTime, bool spark, bool useColorField, float trailInterval = 0.0f,
                         float trailLifeTime = 0.3f);

    /// <summary>打ち上げ花火1発。閃光 + 殻 + 尾を引く火花をまとめて出す</summary>
    void EmitFirework(std::mt19937& rng, const Vector3& center, const Vector4& coreColor,
                      const Vector4& shellColor, float power, bool useColorField,
                      FireworkStyle style);

    /// <summary>単発の粒（上昇中の軌跡や環境の粒に使う）</summary>
    void EmitSpark(std::mt19937& rng, const Vector3& position, const Vector3& velocity,
                   const Vector4& color, float scale, float lifeTime, bool spark,
                   bool useColorField);

    int GetActiveCount() const { return activeCount_; }
    uint32_t GetCapacity() const { return static_cast<uint32_t>(particles_.size()); }

    /// <summary>今フレームに積んだ頂点数（ImGui の負荷表示用）</summary>
    uint32_t GetDrawnQuadCount() const { return drawnQuadCount_; }

private:
    /// <summary>シェーダに渡す頂点。POSITION / TEXCOORD / COLOR の40バイト</summary>
    struct Vertex
    {
        Vector4 position;
        Vector2 texcoord;
        Vector4 color;
    };

    struct Particle
    {
        bool isActive = false;
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
        FireworkFxShape shape = FireworkFxShape::Billboard;
        bool useSparkTexture = false;
        bool useColorField = false;
        bool alignToVelocity = false;

        float trailInterval = 0.0f;
        float trailAccum = 0.0f;
        float trailLifeTime = 0.3f;
        float trailScale = 0.08f;
    };

    bool CreateRootSignature(DirectXCom* dxCommon);
    bool CreatePipelineStates(DirectXCom* dxCommon);
    bool CreateBuffers(DirectXCom* dxCommon, uint32_t capacity);

    /// <summary>同じテクスチャの粒だけ頂点バッファに積んで、1回で描く</summary>
    uint32_t BuildVertices(Vertex* destination, uint32_t destinationCapacityInQuads, bool spark);

    int FindFreeIndex();

private:
    DirectXCom* dxCommon_ = nullptr;
    Camera* camera_ = nullptr;

    std::vector<Particle> particles_;

    // Update() の途中で軌跡を撒くと、生えたばかりの粒を同じフレームで
    // 更新してしまうので、いったんここに溜めてループを抜けてから流す
    std::vector<FireworkFxDesc> pendingEmits_;

    uint32_t softTextureIndex_ = 0;  // ぼんやり丸
    uint32_t sparkTextureIndex_ = 0; // キラッ

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> additivePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> alphaPipelineState_;

    // 頂点バッファは3枚を順番に使う。
    // GPU が前フレームの分をまだ読んでいる可能性があるので、その場で上書きしない
    static constexpr uint32_t kFrameCount = 3;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResources_[kFrameCount];
    Baziru3::PersistentMap<Vertex> vertexMaps_[kFrameCount];
    D3D12_VERTEX_BUFFER_VIEW vertexBufferViews_[kFrameCount]{};
    uint32_t frameIndex_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    uint32_t maxQuads_ = 0;

    // 今フレームぶんの積み上げ結果。Update() で作って Draw() が使う
    uint32_t softQuadCount_ = 0;
    uint32_t sparkQuadCount_ = 0;
    uint32_t drawnQuadCount_ = 0;

    ColorField colorField_;
    float elapsedTime_ = 0.0f;

    bool isAdditive_ = true;
    bool isReady_ = false;
    bool isTrailEnabled_ = true;
    float trailDensity_ = 1.0f;

    int activeCount_ = 0;
    int nextSearchIndex_ = 0;
};
