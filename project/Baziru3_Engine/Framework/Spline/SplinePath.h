#pragma once
#include "Baziru3_Engine/Core/Base/Vector.h"
#include <vector>
#include <cmath>

namespace BaziruEngine
{
    /// <summary>
    /// スプライン補間方式
    /// </summary>
    enum class SplineType
    {
        CatmullRom,  // 通過点をすべて滑らかに繋ぐスプライン（標準）
        Linear,      // 折れ線（直線補間）
        BezierCubic  // 3次ベジェ曲線
    };

    /// <summary>
    /// スプライン上のサンプル情報
    /// </summary>
    struct SplineSample
    {
        Vector3 position{ 0.0f, 0.0f, 0.0f }; // 3D座標
        Vector3 tangent{ 0.0f, 0.0f, 1.0f };  // 進行方向接線ベクトル（正規化済み）
        Vector3 normal{ 0.0f, 1.0f, 0.0f };   // 上方向法線ベクトル（正規化済み）
        float distance = 0.0f;                // 始点からの累積距離
        float t = 0.0f;                       // 正規化進行度 [0.0, 1.0]
    };

    /// <summary>
    /// スプラインパス管理クラス
    /// 制御点群から Catmull-Rom 等の滑らかな3D曲線を構築し、
    /// 距離に応じた等速サンプリングや接線（向き）の算出を行います。
    /// </summary>
    class SplinePath
    {
    public:
        SplinePath() = default;
        ~SplinePath() = default;

        /// <summary>
        /// 制御点（通過点）を追加
        /// </summary>
        void AddPoint(const Vector3& point);

        /// <summary>
        /// 制御点群を一括設定
        /// </summary>
        void SetPoints(const std::vector<Vector3>& points);

        /// <summary>
        /// 特定のインデックスの制御点を更新
        /// </summary>
        void SetPoint(size_t index, const Vector3& point);

        /// <summary>
        /// 制御点の取得
        /// </summary>
        const std::vector<Vector3>& GetPoints() const { return controlPoints_; }
        size_t GetPointCount() const { return controlPoints_.size(); }

        /// <summary>
        /// 制御点をクリア
        /// </summary>
        void Clear();

        /// <summary>
        /// ループ（閉曲線）フラグの設定・取得
        /// </summary>
        void SetLoop(bool isLoop);
        bool IsLoop() const { return isLoop_; }

        /// <summary>
        /// スプライン種別の設定・取得
        /// </summary>
        void SetSplineType(SplineType type) { splineType_ = type; dirty_ = true; }
        SplineType GetSplineType() const { return splineType_; }

        /// <summary>
        /// 累積距離テーブル（Arc-length table）の再構築
        /// 制御点の変更後に自動的に呼ばれます。
        /// </summary>
        void BuildDistanceTable(uint32_t samplesPerSegment = 20);

        /// <summary>
        /// パス全体の総距離（全長）を取得
        /// </summary>
        float GetTotalLength() const;

        /// <summary>
        /// 正規化進行度 t [0.0, 1.0] における位置を取得（非等速）
        /// </summary>
        Vector3 EvaluatePosition(float t) const;

        /// <summary>
        /// 正規化進行度 t [0.0, 1.0] における接線方向（進行方向）を取得
        /// </summary>
        Vector3 EvaluateTangent(float t) const;

        /// <summary>
        /// 始点からの距離 distance [0.0, TotalLength] における詳細サンプルを取得（等速移動用）
        /// </summary>
        SplineSample EvaluateByDistance(float distance) const;

        /// <summary>
        /// 均等速度での正規化進行度 t [0.0, 1.0] における詳細サンプルを取得（等速移動用）
        /// </summary>
        SplineSample EvaluateNormalized(float normalizedT) const;

        // --- 静的数学ヘルパー関数 ---

        /// <summary>
        /// 4点 Catmull-Rom スプライン補間 (p1からp2への補間、t: 0.0~1.0)
        /// </summary>
        static Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

        /// <summary>
        /// Catmull-Rom スプラインの1階微分（接線方向ベクトル）
        /// </summary>
        static Vector3 CatmullRomDerivative(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

        /// <summary>
        /// 3次ベジェ曲線補間
        /// </summary>
        static Vector3 BezierCubic(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t);

        /// <summary>
        /// 線形補間
        /// </summary>
        static Vector3 Lerp(const Vector3& a, const Vector3& b, float t);

    private:
        struct DistanceSample
        {
            float t = 0.0f;
            float distance = 0.0f;
            Vector3 position{ 0.0f, 0.0f, 0.0f };
        };

        void EnsureBuilt() const;

        // 内部サンプリングヘルパー
        void GetSegmentControlPoints(float globalT, size_t& outSegment, float& outLocalT,
                                     Vector3& outP0, Vector3& outP1, Vector3& outP2, Vector3& outP3) const;

    private:
        std::vector<Vector3> controlPoints_;
        bool isLoop_ = false;
        SplineType splineType_ = SplineType::CatmullRom;
        uint32_t samplesPerSegment_ = 20;

        mutable bool dirty_ = true;
        mutable std::vector<DistanceSample> distanceTable_;
        mutable float totalLength_ = 0.0f;
    };
}
