// 入力と一致させる：POSITION(float4), TEXCOORD(float2)
struct VSIn
{
    float4 pos : POSITION;
    float2 uv : TEXCOORD;
    uint iid : SV_InstanceID; // インスタンシング用
};

// 1インスタンスにつき1個のWVP行列（t1 にバインドする）
StructuredBuffer<float4x4> gWVP : register(t1);

// VS出力
struct VSOut
{
    float4 svpos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOut main(VSIn vin)
{
    VSOut vout;
    float4x4 wvp = gWVP[vin.iid];
    vout.svpos = mul(vin.pos, wvp);
    vout.uv = vin.uv;
    return vout;
}