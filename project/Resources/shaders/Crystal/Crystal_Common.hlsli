//======================================================================
// Crystal_Common.hlsli
// C++側 CrystalCommon.h の各構造体と1:1で対応させること
//======================================================================
#ifndef CRYSTAL_COMMON_HLSLI
#define CRYSTAL_COMMON_HLSLI

#pragma pack_matrix(row_major)

// b0 : フレーム毎
cbuffer FrameCB : register(b0)
{
    float4x4 g_View;
    float4x4 g_ViewProj;
    float3   g_CameraPosWS;
    float    g_Time;
    float2   g_ScreenSize;
    float2   g_InvScreenSize;
};

// b1 : クリスタルのグロー・見た目パラメータ（CPU側からいつでも編集可能）
cbuffer CrystalMaterialCB : register(b1)
{
    float3 g_GlowColor;
    float  g_GlowIntensity;

    float3 g_BaseTint;
    float  g_BaseAlpha;

    float g_FresnelPower;
    float g_RimIntensity;
    float g_DistortionStrength;
    float g_NoiseScale;

    float g_NoiseSpeed;
    float g_InnerGlowPower;
    float g_BackfaceGlowBoost;
    float g_Pad0;
};

// b2 : パス切り替えフラグ（ルート定数 1DWORD）
cbuffer PassCB : register(b2)
{
    uint g_IsBackfacePass;
};

// b3 : オブジェクト毎
cbuffer ObjectCB : register(b3)
{
    float4x4 g_World;
    float4x4 g_WorldInvTranspose;
};

// t0 : 屈折サンプリング用の背景コピー（背面パス描画後のSceneColorのコピー）
Texture2D    g_SceneColorCopy      : register(t0);
SamplerState g_LinearClampSampler  : register(s0);

#endif // CRYSTAL_COMMON_HLSLI
