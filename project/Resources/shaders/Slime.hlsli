// スライム用シェーダー共通構造体
struct VertexShaderOutput
{
    float32_t4 position : SV_Position;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
    float32_t3 worldPosition : TEXCOORD1;
    float32_t  deformAmount : TEXCOORD2; // ぶよぶよ変形量（PSでの色変調用）
};

// スライム用定数バッファ (b3 = ライトと同じレジスタだが、スライムPSOでは b3 をスライム用に使う)
struct SlimeParams
{
    float32_t  time;             // 経過時間
    float32_t  wobbleStrength;   // ぶよぶよ揺れの強さ (0.0 ~ 0.3)
    float32_t  wobbleFrequency;  // ぶよぶよ揺れの周波数 (2.0 ~ 8.0)
    float32_t  impulseStrength;  // 衝撃波紋の強さ (被弾・着地時)
    float32_t3 squashStretch;    // スクワッシュ＆ストレッチ変形ベクトル (x,y,z)
    float32_t  padding1;
    float32_t4 baseColor;        // スライムのベースカラー (RGBA)
    float32_t  fresnelPower;     // フレネル反射の強さ (1.0 ~ 5.0)
    float32_t  envReflection;    // 環境マップ反射率 (0.0 ~ 1.0)
    float32_t  innerGlow;        // 内部散乱グローの強さ (0.0 ~ 1.0)
    float32_t  specularShininess; // スペキュラハイライトの鋭さ
};
