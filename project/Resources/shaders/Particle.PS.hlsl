Texture2D gTex : register(t0); // テクスチャ（PSから参照）
SamplerState gSamp : register(s0);

struct PSIn
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

float4 main(PSIn pin) : SV_Target
{
    return gTex.Sample(gSamp, pin.uv);
}