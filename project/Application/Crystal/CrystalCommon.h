#pragma once
//======================================================================
// CrystalCommon.h
//
// クリスタル(水晶)シェーダー用の定数バッファ構造体。
// HLSL 側 (Crystal_Common.hlsli) の cbuffer レイアウトと1:1で一致させること。
// D3D12 の CBV は 256byte アラインメントが必要だが、構造体自体は 16byte
// アラインメントを満たしていれば良い（アップロード時にオフセットを256Bにする）。
//======================================================================

#include"Vector.h"
#include"Matrix4x4.h"

namespace Crystal {

	//----------------------------------------------------------------------
	// b0 : フレーム毎（1フレームに1回更新）
	//----------------------------------------------------------------------
	struct FrameConstants
	{
		Matrix4x4 View;
		Matrix4x4 ViewProj;
		Vector3 CameraPosWS;
		float Time;            // 秒。アニメーションノイズに使用
		Vector2 ScreenSize;       // ピクセル単位
		Vector2 InvScreenSize;    // 1.0 / ScreenSize
	};
	static_assert(sizeof(FrameConstants) % 16 == 0, "16byte alignment required for CBV");

	//----------------------------------------------------------------------
	// b1 : クリスタルのグロー・見た目パラメータ（CPU側からいつでも編集可能）
	//      ImGui のスライダー等でこの構造体の値を直接いじり、
	//      毎フレーム（もしくは変更時のみ）アップロードバッファへコピーする想定。
	//----------------------------------------------------------------------
	struct MaterialParams
	{
		// 発光色・強さ（リムグロー／内部グロー共通の色として使用）
		Vector3 GlowColor = { 0.35f, 0.85f, 1.00f };
		float GlowIntensity = 2.50f;

		// ベースの色味・半透明度
		Vector3 BaseTint = { 0.55f, 0.80f, 1.00f };
		float BaseAlpha = 0.35f;

		// フレネル（エッジ発光）関連
		float FresnelPower = 3.0f;   // 大きいほどエッジが締まる
		float RimIntensity = 1.6f;   // エッジ発光の強さ
		float DistortionStrength = 0.06f;  // 背景の屈折量（画面UVオフセット幅）
		float NoiseScale = 3.0f;   // 内部ノイズの空間スケール

		float NoiseSpeed = 0.25f;  // 内部ノイズのアニメーション速度
		float InnerGlowPower = 2.0f;   // 内部グローのコントラスト（べき乗）
		float BackfaceGlowBoost = 1.3f;   // 背面パスでの内部発光ブースト係数
		float _Pad0 = 0.0f;   // 16byteアラインメント用パディング
	};
	static_assert(sizeof(MaterialParams) % 16 == 0, "16byte alignment required for CBV");

	//----------------------------------------------------------------------
	// b3 : オブジェクト（クリスタルメッシュ）毎
	//----------------------------------------------------------------------
	struct ObjectConstants
	{
		Matrix4x4 World;
		Matrix4x4 WorldInvTranspose; // 法線変換用（非一様スケール対応）
	};
	static_assert(sizeof(ObjectConstants) % 16 == 0, "16byte alignment required for CBV");
}