struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

struct IrisParams
{
    float2 center;      // 円の中心座標 (UV空間: 0.0〜1.0)
    float  radius;      // 円の半径
    float  aspectRatio; // 画面アスペクト比 (width / height)
    float  feather;     // 境界アンチエイリアスぼかし幅
    float3 pad;
};

ConstantBuffer<IrisParams> gIrisParams : register(b0);

float4 main(PSInput input) : SV_TARGET
{
    // アスペクト比を考慮した中心からの距離計算（正円にする）
    float2 diff = input.uv - gIrisParams.center;
    diff.x *= gIrisParams.aspectRatio;
    float dist = length(diff);

    // 半径以内は透明(0.0)、外側は黒(1.0)
    // smoothstep により境界のギザギザ（ジャギー）を滑らかにアンチエイリアス
    float feather = max(0.001f, gIrisParams.feather);
    float alpha = smoothstep(gIrisParams.radius - feather, gIrisParams.radius, dist);

    return float4(0.0f, 0.0f, 0.0f, alpha);
}
