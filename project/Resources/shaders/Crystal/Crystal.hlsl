//======================================================================
// Crystal.hlsl
//
// クリスタル(水晶)表現用のVS/PS。
//   VSMain / PSMain は 背面パス・前面パス共通で使用し、
//   PassCB.g_IsBackfacePass で挙動を切り替える。
//======================================================================
#include "Crystal_Common.hlsli"

//----------------------------------------------------------------------
// 頂点シェーダー
//----------------------------------------------------------------------
struct VSInput {
	float4 PositionOS : POSITION0;
	float2 TexCoord : TEXCOORD0;
	float3 NormalOS   : NORMAL0;
};

struct VSOutput
{
	float4 PositionCS : SV_Position; // PS側では画面ピクセル座標として使える
	float3 PositionWS : TEXCOORD0;
	float3 NormalWS   : TEXCOORD1;
};

VSOutput VSMain(VSInput input)
{
	VSOutput o;

	float4 posWS = mul(float4(input.PositionOS.xyz, 1.0), g_World);
	o.PositionWS = posWS.xyz;
	o.NormalWS   = normalize(mul(input.NormalOS, (float3x3)g_WorldInvTranspose));
	o.PositionCS = mul(posWS, g_ViewProj);

	return o;
}

//----------------------------------------------------------------------
// ノイズ関数（内部の脈動グロー・エネルギー模様用）
// 外部テクスチャ非依存にするための簡易ハッシュベース Value Noise
//----------------------------------------------------------------------
float Hash21(float2 p)
{
	p = frac(p * float2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return frac(p.x * p.y);
}

float ValueNoise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);

	float a = Hash21(i);
	float b = Hash21(i + float2(1.0, 0.0));
	float c = Hash21(i + float2(0.0, 1.0));
	float d = Hash21(i + float2(1.0, 1.0));

	float2 u = f * f * (3.0 - 2.0 * f); // smoothstep補間
	return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

//----------------------------------------------------------------------
// ピクセルシェーダー
//----------------------------------------------------------------------
float4 PSMain(VSOutput input) : SV_Target
{
	float3 N = normalize(input.NormalWS);
	float3 V = normalize(g_CameraPosWS - input.PositionWS);

	// 背面パスでは裏側から見ているので法線を反転させて扱う
	if (g_IsBackfacePass != 0)
	{
		N = -N;
	}

	float NdotV    = saturate(dot(N, V));
	float fresnel  = pow(1.0 - NdotV, g_FresnelPower);

	// 内部を漂うようなアニメーションノイズ（多面体の面ごとの模様の代わりに簡易実装）
	float2 noiseUV   = input.PositionWS.xz * g_NoiseScale + float2(0.0, g_Time * g_NoiseSpeed);
	float  noiseValue = ValueNoise(noiseUV);
	float  innerGlow  = pow(saturate(noiseValue), g_InnerGlowPower);

	if (g_IsBackfacePass != 0)
	{
		// 背面パス：内部発光のみを加算合成でSceneColorに書き込む
		float3 backGlow = g_GlowColor * innerGlow * g_GlowIntensity * g_BackfaceGlowBoost;
		return float4(backGlow, 1.0);
	}
	else
	{
		// 前面パス：背景を法線に応じて歪ませてサンプリング（擬似屈折）
		float2 screenUV = input.PositionCS.xy * g_InvScreenSize;

		float3 Nview      = mul(N, (float3x3)g_View);
		float2 distortion = Nview.xy * g_DistortionStrength;

		float2 sampleUV = saturate(screenUV + distortion);
		float3 refractedColor = g_SceneColorCopy.Sample(g_LinearClampSampler, sampleUV).rgb;

		// 屈折背景とベースティントを混ぜる
		float3 baseColor = lerp(refractedColor, g_BaseTint, g_BaseAlpha);

		// フレネルによるリムグローと、内部ノイズによる仄かな発光を加算
		float3 rim  = g_GlowColor * fresnel * g_RimIntensity;
		float3 vein = g_GlowColor * innerGlow * g_GlowIntensity * 0.5;

		float3 finalColor = (baseColor + rim + vein) * 0.1;
		float  finalAlpha = saturate(g_BaseAlpha + fresnel * g_RimIntensity);

		return float4(finalColor, finalAlpha);
	}
}
