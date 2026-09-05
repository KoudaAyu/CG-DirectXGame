// 花火用のバッチ描画シェーダ（このプロジェクトで追加）
//
// パーティクルの板ポリは CPU 側で1本の頂点バッファに展開済みなので、
// ここではワールド座標をそのまま ViewProjection に通すだけ。
// 粒ごとの色は頂点カラーに載っている。

struct SceneParams
{
    float32_t4x4 viewProjection;
};
ConstantBuffer<SceneParams> gScene : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0;
    float32_t4 color : COLOR0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    output.position = mul(input.position, gScene.viewProjection);
    output.texcoord = input.texcoord;
    output.color = input.color;
    return output;
}
