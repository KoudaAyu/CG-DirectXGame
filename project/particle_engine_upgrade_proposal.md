# パーティクルシステムのエンジン統合に伴う機能拡張設計提案

## 1. 背景と目的

現在、アプリケーション層で動作する `AppParticleManager` の描画パイプライン（PSOやバッファ）はエンジン側の `ParticleManager` へ一本化されました。しかし、ゲームとして必要な以下の「高度な物理挙動」は、未だアプリケーション層のCPU側で毎フレーム計算され、描画直前にエンジンへコピーされています。

*   **重力 (Gravity)**
*   **地面バウンド (Bounce Elasticity)**
*   **角速度 (Angular Velocity)**
*   **指定ターゲット（プレイヤー）への追従移動 (Player Relative)**

この設計は一時的な回避策としては有効ですが、CPUの毎フレーム処理とデータコピーのオーバーヘッドが発生します。将来的にこれらすべての挙動を**エンジン層側で一元管理（可能であればGPUパーティクル化）**するための拡張設計を提案します。

---

## 2. 拡張設計案

### 2.1 CPUシミュレーションの拡張 (`ParticleManager` 側)

エンジン側の `ParticleManager::Particle` 構造体に、以下の物理パラメータを追加します。

```cpp
// Baziru3_Engine/Particle/ParticleManager.h 内の Particle 構造体に拡張
struct Particle
{
    Transform transform;
    Vector3 velocity;
    Vector4 color;
    float lifeTime;
    float currentTime;
    uint32_t textureIndex = 0;

    // --- 追加拡張パラメータ ---
    float gravity = 0.0f;            // 重力加速度
    float bounceElasticity = 0.0f;   // 地面バウンド時の反発係数 (0.0でバウンドなし)
    float angularVelocity = 0.0f;    // Z軸まわりの回転角速度
    bool followPlayer = false;       // 指定座標（プレイヤーなど）に相対追従するか
    Vector3 offsetFromPlayer;        // 追従時のオフセット座標
};
```

これに伴い、[ParticleManager.cpp](file:///c:/Users/k024g/OneDrive/デスクトップ/Engine_ver2026/project/Baziru3_Engine/Particle/ParticleManager.cpp) の `Update` 関数内のリスト更新ロジックを以下のように拡張します。

```cpp
// ParticleManager::Update 内 of ラムダ式 updateParticleList を以下のように拡張
auto updateParticleList = [&](std::list<Particle>& targetParticles)
{
    auto it = targetParticles.begin();
    while (it != targetParticles.end())
    {
        it->currentTime += deltaTime;
        if (it->currentTime >= it->lifeTime)
        {
            it = targetParticles.erase(it);
            continue;
        }

        Vector3 pos;
        if (it->followPlayer)
        {
            // 指定プレイヤーの現在位置（引数等で取得）に対して相対移動
            it->offsetFromPlayer.y -= it->gravity * deltaTime;
            it->offsetFromPlayer += it->velocity * deltaTime;
            // ※ playerPos は Update 関数の引数、あるいは SceneManager 経由で取得可能にする
            pos = playerPos + it->offsetFromPlayer;
        }
        else
        {
            // 重力加速
            it->velocity.y -= it->gravity * deltaTime;
            pos = it->transform.GetTranslate() + it->velocity * deltaTime;

            // 地面バウンド（簡易衝突検知 y = 0.0f）
            float groundY = 0.0f;
            if (pos.y < groundY && it->velocity.y < 0.0f)
            {
                pos.y = groundY;
                it->velocity.y = -it->velocity.y * it->bounceElasticity;
                // 摩擦による減衰
                it->velocity.x *= 0.7f;
                it->velocity.z *= 0.7f;
            }
        }
        it->transform.SetTranslate(pos);

        // Z軸回転の更新
        Vector3 rot = it->transform.GetRotate();
        rot.z += it->angularVelocity * deltaTime;
        it->transform.SetRotate(rot);

        ++it;
    }
};
```

---

### 2.2 GPUシミュレーション（Compute Shader）への拡張設計

エンジンが誇る GPU Particle (Compute Shader) の性能をフルに活かすため、最終的には上記の物理パラメータを定数バッファおよび構造化バッファにマッピングし、`UpdateParticle.CS.hlsl` でシミュレーションさせます。

#### 1. CS用構造体への追加 (`ParticleManager.h`)
```cpp
struct ParticleCS
{
    Vector3 translate;
    float lifeTime;
    Vector3 scale;
    float currentTime;
    Vector3 velocity;
    float gravity;           // 追加
    Vector4 color;
    float bounceElasticity;  // 追加
    float angularVelocity;   // 追加
    uint32_t followPlayer;   // 追加 (bool型はHLSL互換のためuint32_t)
    Vector3 offsetFromPlayer;// 追加
};
```

#### 2. Compute Shader の拡張 (`UpdateParticle.CS.hlsl`)
CS内で上記パラメータを元に、スレッド並列で以下の演算を行います。
```hlsl
// UpdateParticle.CS.hlsl の処理拡張イメージ
[numthreads(1024, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint index = DTid.x;
    if (index >= gPerView.maxParticles) return;

    if (gParticles[index].currentTime < gParticles[index].lifeTime)
    {
        gParticles[index].currentTime += gPerView.deltaTime;

        if (gParticles[index].followPlayer != 0)
        {
            // プレイヤー座標（定数バッファ経由）に追従
            gParticles[index].offsetFromPlayer.y -= gParticles[index].gravity * gPerView.deltaTime;
            gParticles[index].offsetFromPlayer += gParticles[index].velocity * gPerView.deltaTime;
            gParticles[index].translate = gPlayer.pos + gParticles[index].offsetFromPlayer;
        }
        else
        {
            // 通常物理
            gParticles[index].velocity.y -= gParticles[index].gravity * gPerView.deltaTime;
            gParticles[index].translate += gParticles[index].velocity * gPerView.deltaTime;

            // 地面バウンド
            if (gParticles[index].translate.y < 0.0f && gParticles[index].velocity.y < 0.0f)
            {
                gParticles[index].translate.y = 0.0f;
                gParticles[index].velocity.y = -gParticles[index].velocity.y * gParticles[index].bounceElasticity;
                gParticles[index].velocity.x *= 0.7f;
                gParticles[index].velocity.z *= 0.7f;
            }
        }
        
        // Z回転などの適用...
    }
}
```

## 3. この拡張によるメリット

1.  **CPUの完全解放 (GPUへのオフロード):**
    CSでの更新に移行することで、数万パーティクルの物理シミュレーションをGPUだけで処理できるようになり、Debug/Release問わずFPSの向上が見込めます。
2.  **メモリバッファの削減:**
    CPUとGPU間でのデータ転送（Structured Bufferのマッピング・コピー）が不要になり、メモリ帯域幅の負荷が低減されます。
3.  **エンジン設計の極致:**
    アプリケーション層には一切の「物理や描画のルール」を置かず、データ（エミッター設定など）を渡すだけでエンジンが最適なハードウェア実行（GPU CS）を選択する、商用エンジンさながらの疎結合な設計を実現できます。
