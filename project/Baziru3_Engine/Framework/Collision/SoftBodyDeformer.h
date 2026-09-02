#pragma once

#include "Vector.h"
#include <cstdint>

/// <summary>
/// 軟体・液体オブジェクト（スライムやゼリー状の敵など）の変形計算クラス
/// </summary>
namespace SoftBodyDeformer
{
    /// <summary>
    /// アニメーション時間を進める
    /// </summary>
    /// <param name="dt">経過時間（秒）</param>
    void AdvanceGlobalTime(float dt = 0.0166667f);

    /// <summary>
    /// 現在のアニメーション時間を取得
    /// </summary>
    /// <returns>経過時間（秒）</returns>
    float GetGlobalTime();

    /// <summary>
    /// 3Dパーリンノイズの計算
    /// </summary>
    /// <param name="p">入力座標</param>
    /// <returns>ノイズ値（-1.0 〜 1.0）</returns>
    float FastNoise3D(const Vector3& p);

    /// <summary>
    /// 頂点の変形後座標を計算（波打ち・重力垂れ下がり・接地偏平）
    /// </summary>
    /// <param name="localP">変形前のローカル座標</param>
    /// <param name="normal">法線ベクトル</param>
    /// <param name="time">現在時間（秒）</param>
    /// <param name="wobbleStr">波打ちの強さ</param>
    /// <param name="wobbleFreq">波打ちの周波数</param>
    /// <returns>変形後のローカル座標</returns>
    Vector3 CalculateDeformedPosition(const Vector3& localP, const Vector3& normal, float time, float wobbleStr = 0.22f, float wobbleFreq = 5.0f);
}
