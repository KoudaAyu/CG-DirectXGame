#include "TacticalUICanvas.h"

#include "Application/Config/AppDebugConfig.h"
#include "Application/Config/GameConfig.h"
#include "DirectXCom.h"
#include "SRVManager.h"
#include "TextureManager.h"
#include "WindowsAPI.h"

#include <commctrl.h>
#include <cmath>
#include <cstring>
#include <iostream>

#pragma comment(lib, "comctl32.lib")

namespace
{
    // 数学・サブクラス定数
    constexpr float    kPi              = GameConfig::Math::kPi;
    constexpr float    kTwoPi           = GameConfig::Math::kTwoPi;
    constexpr float    kHalfPi          = GameConfig::Math::kHalfPi;
    constexpr float    kThreeHalfPi     = kPi + kHalfPi;
    constexpr UINT_PTR kWheelSubclassId = 0x54554943u; // 'TUIC'

    // sRGB -> Linear 変換テーブル定数
    constexpr int      kSrgbLutSize       = 256;
    constexpr float    kMaxByteFloat      = 255.0f;
    constexpr float    kInvMaxByteFloat   = 1.0f / kMaxByteFloat;
    constexpr float    kSrgbLinearCutoff  = 0.04045f;
    constexpr float    kSrgbLinearDivisor = 12.92f;
    constexpr float    kSrgbGammaOffset   = 0.055f;
    constexpr float    kSrgbGammaScale    = 1.055f;
    constexpr float    kSrgbGammaExponent = 2.4f;
    constexpr uint32_t kByteMask          = 0xFFu;
    constexpr size_t   kAlphaChannelIdx   = 3;

    // UTF-8 デコード定数
    constexpr uint8_t  kUtf8AsciiLimit        = 0x80u;
    constexpr uint8_t  kUtf8TwoByteMask       = 0xE0u;
    constexpr uint8_t  kUtf8TwoBytePrefix     = 0xC0u;
    constexpr uint8_t  kUtf8TwoByteLeadMask   = 0x1Fu;
    constexpr uint8_t  kUtf8ThreeByteMask     = 0xF0u;
    constexpr uint8_t  kUtf8ThreeBytePrefix   = 0xE0u;
    constexpr uint8_t  kUtf8ThreeByteLeadMask = 0x0Fu;
    constexpr uint8_t  kUtf8FourByteMask      = 0xF8u;
    constexpr uint8_t  kUtf8FourBytePrefix    = 0xF0u;
    constexpr uint8_t  kUtf8FourByteLeadMask  = 0x07u;
    constexpr uint8_t  kUtf8ContMask          = 0x3Fu;
    constexpr int      kUtf8Shift1            = 6;
    constexpr int      kUtf8Shift2            = 12;
    constexpr int      kUtf8Shift3            = 18;

    // DrawList バッファ予約・ジオメトリ許容値
    constexpr size_t   kInitialVertReserve     = 8192;
    constexpr size_t   kInitialIdxReserve      = 24576;
    constexpr size_t   kInitialCmdReserve      = 64;
    constexpr size_t   kInitialClipReserve     = 16;
    constexpr size_t   kInitialScratchReserve  = 128;
    constexpr float    kDuplicatePtDistSqEps   = 1e-4f;
    constexpr int      kMinPolyVerts           = 3;
    constexpr float    kEdgeLenEps             = 1e-5f;
    constexpr float    kMiterDenomEps          = 1e-4f;
    constexpr float    kMaxMiterScale          = 2.5f;
    constexpr float    kStrokeAaFringePx       = 0.65f;
    constexpr float    kThinStrokeThreshold    = 1.05f;
    constexpr float    kMinHalfCorePx          = 0.22f;
    constexpr float    kStrokeCoreInsetPx      = 0.60f;
    constexpr int      kStrokeProfileCount     = 4;
    constexpr uint32_t kStrokeBandCount        = 3;
    constexpr float    kFillInnerInsetPx       = 0.35f;
    constexpr float    kFillOuterFringePx      = 0.45f;
    constexpr float    kMinRoundingPx          = 0.25f;
    constexpr int      kRoundedCornerArcSegs   = 5;
    constexpr int      kRoundedImageArcSteps   = 4;
    constexpr int      kCornerCount            = 4;
    constexpr int      kQuadVertCount          = 4;
    constexpr uint32_t kQuadIndexCount         = 6;
    constexpr int      kTriangleVertCount      = 3;
    constexpr float    kMinCircleRadius        = 0.1f;
    constexpr int      kMaxCircleSegments      = 96;
    constexpr int      kAutoCircleMinSegs      = 12;
    constexpr int      kAutoCircleMaxSegs      = 64;
    constexpr float    kAutoCircleRadiusScale  = 0.9f;
    constexpr float    kAutoCircleBaseSegs     = 12.0f;

    // フォント・パイプライン・アトラス定数
    constexpr int      kMinFontBucketPx        = 10;
    constexpr int      kMaxFontBucketPx        = 96;
    constexpr float    kPixelSnapScaleEps      = 0.02f;
    constexpr float    kTitleBoldDefaultPx     = 56.0f;
    constexpr float    kSubBoldDefaultPx       = 20.0f;
    constexpr uint32_t kPrewarmAsciiFirst      = 32;
    constexpr uint32_t kPrewarmAsciiLast       = 126;
    constexpr UINT     kRootConstFloatCount    = 4;
    constexpr UINT     kTexcoordByteOffset     = 8;
    constexpr UINT     kColorByteOffset        = 16;
    constexpr size_t   kInputElementCount      = 3;
    constexpr uint32_t kTransparentWhiteTexel  = 0x00FFFFFFu;
    constexpr uint32_t kOpaqueWhiteTexel       = 0xFFFFFFFFu;
    constexpr int      kWhiteBlockSize         = 4;
    constexpr size_t   kDrawLayerCount         = 3;
    constexpr uint32_t kBmpMaxCodepoint        = 0xFFFFu;
    constexpr LONG     kTabSpaces              = 4;
    constexpr int      kGlyphAtlasPad          = 2;
    constexpr int      kDwordAlignMask         = 3;
    constexpr float    kGdiGray8Max            = 64.0f;
    constexpr float    kInvGdiGray8Max         = 1.0f / kGdiGray8Max;
    constexpr float    kGlyphCrispGamma        = 0.82f;

    struct SrgbLutTable
    {
        float toLinear[kSrgbLutSize]{};

        SrgbLutTable()
        {
            for (int i = 0; i < kSrgbLutSize; ++i)
            {
                const float c = static_cast<float>(i) * kInvMaxByteFloat;
                toLinear[i] = (c <= kSrgbLinearCutoff)
                    ? (c / kSrgbLinearDivisor)
                    : std::pow((c + kSrgbGammaOffset) / kSrgbGammaScale, kSrgbGammaExponent);
            }
        }
    };

    const SrgbLutTable gSrgbLut{};

    inline bool HasNonZeroAlpha(uint32_t col)
    {
        return ((col >> UI_COL32_A_SHIFT) & kByteMask) != 0;
    }

    inline void UnpackColorLinear(uint32_t col, float outRgba[kUIColorComponents])
    {
        const uint8_t r = static_cast<uint8_t>((col >> UI_COL32_R_SHIFT) & kByteMask);
        const uint8_t g = static_cast<uint8_t>((col >> UI_COL32_G_SHIFT) & kByteMask);
        const uint8_t b = static_cast<uint8_t>((col >> UI_COL32_B_SHIFT) & kByteMask);
        const uint8_t a = static_cast<uint8_t>((col >> UI_COL32_A_SHIFT) & kByteMask);
        outRgba[0] = gSrgbLut.toLinear[r];
        outRgba[1] = gSrgbLut.toLinear[g];
        outRgba[2] = gSrgbLut.toLinear[b];
        outRgba[kAlphaChannelIdx] = static_cast<float>(a) * kInvMaxByteFloat;
    }

    inline void UnpackOpaqueAndTransPair(uint32_t col, float cOpaque[kUIColorComponents], float cTrans[kUIColorComponents])
    {
        UnpackColorLinear(col, cOpaque);
        cTrans[0] = cOpaque[0];
        cTrans[1] = cOpaque[1];
        cTrans[2] = cOpaque[2];
        cTrans[kAlphaChannelIdx] = 0.0f;
    }

    // 重複頂点の除去（閉路時の終点一致トリム対応）
    void FilterUniquePoints(const UIVec2* points, int pointsCount, bool trimClosedLoop, std::vector<UIVec2>& outPts)
    {
        outPts.clear();
        if (outPts.capacity() < static_cast<size_t>(pointsCount))
        {
            outPts.reserve(static_cast<size_t>(pointsCount));
        }
        for (int i = 0; i < pointsCount; ++i)
        {
            if (!outPts.empty())
            {
                const float dx = points[i].x - outPts.back().x;
                const float dy = points[i].y - outPts.back().y;
                if (dx * dx + dy * dy < kDuplicatePtDistSqEps)
                {
                    continue;
                }
            }
            outPts.push_back(points[i]);
        }
        if (trimClosedLoop && outPts.size() >= 2)
        {
            const float dx = outPts.back().x - outPts.front().x;
            const float dy = outPts.back().y - outPts.front().y;
            if (dx * dx + dy * dy < kDuplicatePtDistSqEps)
            {
                outPts.pop_back();
            }
        }
    }

    // 隣接エッジ法線のマイター二等分ベクトル計算
    inline UIVec2 ComputeMiterNormal(const UIVec2& n0, const UIVec2& n1)
    {
        UIVec2 dm{ (n0.x + n1.x) * 0.5f, (n0.y + n1.y) * 0.5f };
        const float dSq = dm.x * dm.x + dm.y * dm.y;
        if (dSq > kMiterDenomEps)
        {
            const float inv = (std::min)(1.0f / dSq, kMaxMiterScale);
            dm.x *= inv;
            dm.y *= inv;
            return dm;
        }
        return n1;
    }

    // 円周ポリゴン頂点列の生成
    int BuildCirclePoints(UIVec2 center, float radius, int numSegments, UIVec2 outPts[kMaxCircleSegments])
    {
        const int segs = (numSegments > 2)
            ? (std::min)(numSegments, kMaxCircleSegments)
            : std::clamp(static_cast<int>(radius * kAutoCircleRadiusScale + kAutoCircleBaseSegs), kAutoCircleMinSegs, kAutoCircleMaxSegs);
        const float invSegs = 1.0f / static_cast<float>(segs);
        for (int i = 0; i < segs; ++i)
        {
            const float a = static_cast<float>(i) * invSegs * kTwoPi;
            outPts[i] = UIVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
        }
        return segs;
    }

    uint32_t DecodeNextUtf8(const char*& p, const char* end)
    {
        if (p >= end || *p == '\0')
        {
            return 0;
        }
        const uint8_t c0 = static_cast<uint8_t>(*p++);
        if (c0 < kUtf8AsciiLimit)
        {
            return c0;
        }
        if ((c0 & kUtf8TwoByteMask) == kUtf8TwoBytePrefix)
        {
            if (p < end && (*p != '\0'))
            {
                const uint8_t c1 = static_cast<uint8_t>(*p++);
                return ((c0 & kUtf8TwoByteLeadMask) << kUtf8Shift1) | (c1 & kUtf8ContMask);
            }
            return '?';
        }
        if ((c0 & kUtf8ThreeByteMask) == kUtf8ThreeBytePrefix)
        {
            if (p + 1 < end && p[0] != '\0' && p[1] != '\0')
            {
                const uint8_t c1 = static_cast<uint8_t>(*p++);
                const uint8_t c2 = static_cast<uint8_t>(*p++);
                return ((c0 & kUtf8ThreeByteLeadMask) << kUtf8Shift2) |
                       ((c1 & kUtf8ContMask) << kUtf8Shift1) |
                       (c2 & kUtf8ContMask);
            }
            p = end;
            return '?';
        }
        if ((c0 & kUtf8FourByteMask) == kUtf8FourBytePrefix)
        {
            if (p + 2 < end && p[0] != '\0' && p[1] != '\0' && p[2] != '\0')
            {
                const uint8_t c1 = static_cast<uint8_t>(*p++);
                const uint8_t c2 = static_cast<uint8_t>(*p++);
                const uint8_t c3 = static_cast<uint8_t>(*p++);
                return ((c0 & kUtf8FourByteLeadMask) << kUtf8Shift3) |
                       ((c1 & kUtf8ContMask) << kUtf8Shift2) |
                       ((c2 & kUtf8ContMask) << kUtf8Shift1) |
                       (c3 & kUtf8ContMask);
            }
            p = end;
            return '?';
        }
        return '?';
    }

    int CALLBACK EnumFontCheckProc(
        const LOGFONTW* /*lpelfe*/,
        const TEXTMETRICW* /*lpntme*/,
        DWORD /*FontType*/,
        LPARAM lParam)
    {
        if (lParam)
        {
            *reinterpret_cast<bool*>(lParam) = true;
        }
        return 0; // Stop enumeration on first match
    }

    std::wstring SelectBestJapaneseUIFont(HDC hdc)
    {
        const wchar_t* candidates[] = {
            L"BIZ UDPGothic",
            L"Yu Gothic UI",
            L"Meiryo UI",
            L"MS UI Gothic",
        };
        for (const wchar_t* face : candidates)
        {
            bool found = false;
            LOGFONTW lf{};
            lf.lfCharSet = DEFAULT_CHARSET;
            wcsncpy_s(lf.lfFaceName, face, _TRUNCATE);
            EnumFontFamiliesExW(hdc, &lf, EnumFontCheckProc, reinterpret_cast<LPARAM>(&found), 0);
            if (found)
            {
                return face;
            }
        }
        return L"MS UI Gothic";
    }

    LRESULT CALLBACK WheelSubclassProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam,
        UINT_PTR /*uIdSubclass*/,
        DWORD_PTR dwRefData)
    {
        if (msg == WM_MOUSEWHEEL && dwRefData != 0)
        {
            const short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            auto* canvas = reinterpret_cast<TacticalUICanvas*>(dwRefData);
            canvas->AddWheelDeltaFromSubclass(static_cast<float>(zDelta) / static_cast<float>(WHEEL_DELTA));
        }
        return DefSubclassProc(hwnd, msg, wParam, lParam);
    }
}

// =============================================================================
// TacticalFont
// =============================================================================

UIVec2 TacticalFont::CalcTextSizeA(
    float size,
    float /*maxWidth*/,
    float /*wrapWidth*/,
    const char* textBegin,
    const char* textEnd) const
{
    if (!owner_ || !textBegin)
    {
        return UIVec2(0.0f, 0.0f);
    }
    const float useSize = (size > 1.0f) ? size : defaultPixelSize_;
    return owner_->CalcTextSize(textBegin, textEnd, useSize, role_);
}

// =============================================================================
// TacticalDrawList
// =============================================================================

void TacticalDrawList::Reset(float displayW, float displayH)
{
    if (vertices_.capacity() < kInitialVertReserve) vertices_.reserve(kInitialVertReserve);
    if (indices_.capacity() < kInitialIdxReserve) indices_.reserve(kInitialIdxReserve);
    if (cmds_.capacity() < kInitialCmdReserve) cmds_.reserve(kInitialCmdReserve);
    if (clipStack_.capacity() < kInitialClipReserve) clipStack_.reserve(kInitialClipReserve);
    if (path_.capacity() < kInitialScratchReserve) path_.reserve(kInitialScratchReserve);
    if (scratchPts_.capacity() < kInitialScratchReserve) scratchPts_.reserve(kInitialScratchReserve);
    if (scratchNormals_.capacity() < kInitialScratchReserve) scratchNormals_.reserve(kInitialScratchReserve);

    vertices_.clear();
    indices_.clear();
    cmds_.clear();
    path_.clear();
    clipStack_.clear();
    clipStack_.push_back(UIClipRect{ 0.0f, 0.0f, displayW, displayH });
}

void TacticalDrawList::PushClipRect(UIVec2 clipMin, UIVec2 clipMax, bool intersectWithCurrentClipRect)
{
    UIClipRect cr{ clipMin.x, clipMin.y, clipMax.x, clipMax.y };
    if (intersectWithCurrentClipRect && !clipStack_.empty())
    {
        const UIClipRect& cur = clipStack_.back();
        cr.x0 = (std::max)(cr.x0, cur.x0);
        cr.y0 = (std::max)(cr.y0, cur.y0);
        cr.x1 = (std::min)(cr.x1, cur.x1);
        cr.y1 = (std::min)(cr.y1, cur.y1);
    }
    if (cr.x1 < cr.x0) cr.x1 = cr.x0;
    if (cr.y1 < cr.y0) cr.y1 = cr.y0;
    clipStack_.push_back(cr);
}

void TacticalDrawList::PopClipRect()
{
    if (clipStack_.size() > 1)
    {
        clipStack_.pop_back();
    }
}

void TacticalDrawList::RecordDrawElements(uint32_t indexCount, UITextureID texId)
{
    if (indexCount == 0 || texId == 0)
    {
        return;
    }
    const UIClipRect curClip = clipStack_.empty() ? UIClipRect{} : clipStack_.back();

    if (!cmds_.empty())
    {
        UIDrawCmd& prev = cmds_.back();
        if (prev.textureId == texId && prev.clipRect == curClip)
        {
            prev.indexCount += indexCount;
            return;
        }
    }

    UIDrawCmd cmd{};
    cmd.indexOffset = static_cast<uint32_t>(indices_.size()) - indexCount;
    cmd.indexCount  = indexCount;
    cmd.textureId   = texId;
    cmd.clipRect    = curClip;
    cmds_.push_back(cmd);
}

void TacticalDrawList::AppendQuadGeometry(
    UIVec2 pMin,
    UIVec2 pMax,
    UIVec2 uvMin,
    UIVec2 uvMax,
    const float c0[kUIColorComponents],
    const float c1[kUIColorComponents],
    const float c2[kUIColorComponents],
    const float c3[kUIColorComponents])
{
    const uint32_t vBase = static_cast<uint32_t>(vertices_.size());
    const float xs[kQuadVertCount] = { pMin.x, pMax.x, pMax.x, pMin.x };
    const float ys[kQuadVertCount] = { pMin.y, pMin.y, pMax.y, pMax.y };
    const float us[kQuadVertCount] = { uvMin.x, uvMax.x, uvMax.x, uvMin.x };
    const float vs[kQuadVertCount] = { uvMin.y, uvMin.y, uvMax.y, uvMax.y };
    const float* cols[kQuadVertCount] = { c0, c1, c2, c3 };

    for (int i = 0; i < kQuadVertCount; ++i)
    {
        UIVertex v{};
        v.pos[0] = xs[i];
        v.pos[1] = ys[i];
        v.uv[0]  = us[i];
        v.uv[1]  = vs[i];
        std::memcpy(v.col, cols[i], sizeof(float) * kUIColorComponents);
        vertices_.push_back(v);
    }

    indices_.push_back(vBase + 0);
    indices_.push_back(vBase + 1);
    indices_.push_back(vBase + 2);
    indices_.push_back(vBase + 0);
    indices_.push_back(vBase + 2);
    indices_.push_back(vBase + static_cast<uint32_t>(kQuadVertCount - 1));
}

void TacticalDrawList::AppendUniformQuad(
    UIVec2 pMin,
    UIVec2 pMax,
    UIVec2 uvMin,
    UIVec2 uvMax,
    const float col[kUIColorComponents])
{
    AppendQuadGeometry(pMin, pMax, uvMin, uvMax, col, col, col, col);
}

void TacticalDrawList::AppendRoundedRectPath(UIVec2 pMin, UIVec2 pMax, float r)
{
    PathClear();
    const UIVec2 centers[kCornerCount] = {
        UIVec2(pMax.x - r, pMin.y + r),
        UIVec2(pMax.x - r, pMax.y - r),
        UIVec2(pMin.x + r, pMax.y - r),
        UIVec2(pMin.x + r, pMin.y + r),
    };
    const float startAngles[kCornerCount] = { -kHalfPi, 0.0f, kHalfPi, kPi };
    const float endAngles[kCornerCount]   = { 0.0f, kHalfPi, kPi, kThreeHalfPi };
    for (int i = 0; i < kCornerCount; ++i)
    {
        PathArcTo(centers[i], r, startAngles[i], endAngles[i], kRoundedCornerArcSegs);
    }
}

void TacticalDrawList::AddLine(UIVec2 p1, UIVec2 p2, uint32_t col, float thickness)
{
    if (!HasNonZeroAlpha(col))
    {
        return;
    }
    UIVec2 pts[2] = { p1, p2 };
    AddPolyline(pts, 2, col, UIDrawFlags_None, thickness);
}

void TacticalDrawList::AddPolyline(const UIVec2* points, int pointsCount, uint32_t col, int flags, float thickness)
{
    if (!owner_ || !points || pointsCount < 2 || !HasNonZeroAlpha(col))
    {
        return;
    }
    const UITextureID atlasTex = owner_->GetAtlasTextureID();
    if (atlasTex == 0)
    {
        return;
    }

    bool closed = (flags & UIDrawFlags_Closed) != 0;
    auto& pts = scratchPts_;
    FilterUniquePoints(points, pointsCount, closed, pts);
    if (pts.size() < 2)
    {
        return;
    }
    if (closed && static_cast<int>(pts.size()) < kMinPolyVerts)
    {
        closed = false;
    }

    const int n = static_cast<int>(pts.size());
    const int segCount = closed ? n : (n - 1);

    auto& edgeNormals = scratchNormals_;
    edgeNormals.assign(static_cast<size_t>(n), UIVec2(0.0f, -1.0f));
    for (int i = 0; i < segCount; ++i)
    {
        const int j = (i + 1) % n;
        const float dx = pts[j].x - pts[i].x;
        const float dy = pts[j].y - pts[i].y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > kEdgeLenEps)
        {
            edgeNormals[i] = UIVec2(dy / len, -dx / len);
        }
    }

    const float halfCore = (thickness <= kThinStrokeThreshold)
        ? kMinHalfCorePx
        : (std::max)(kMinHalfCorePx, (thickness - kStrokeCoreInsetPx) * 0.5f);
    const float rOuter = halfCore + kStrokeAaFringePx;

    float cOpaque[kUIColorComponents], cTrans[kUIColorComponents];
    UnpackOpaqueAndTransPair(col, cOpaque, cTrans);

    const UIVec2 whiteUv = owner_->GetWhitePixelUV();
    const uint32_t vBase = static_cast<uint32_t>(vertices_.size());
    const float offsets[kStrokeProfileCount] = { -rOuter, -halfCore, +halfCore, +rOuter };
    const float* cols[kStrokeProfileCount]   = { cTrans, cOpaque, cOpaque, cTrans };

    for (int i = 0; i < n; ++i)
    {
        UIVec2 dm{};
        if (!closed && i == 0)
        {
            dm = edgeNormals[0];
        }
        else if (!closed && i == n - 1)
        {
            dm = edgeNormals[n - 2];
        }
        else
        {
            dm = ComputeMiterNormal(edgeNormals[(i + n - 1) % n], edgeNormals[i]);
        }

        for (int k = 0; k < kStrokeProfileCount; ++k)
        {
            UIVertex v{};
            v.pos[0] = pts[i].x + dm.x * offsets[k];
            v.pos[1] = pts[i].y + dm.y * offsets[k];
            v.uv[0]  = whiteUv.x;
            v.uv[1]  = whiteUv.y;
            std::memcpy(v.col, cols[k], sizeof(float) * kUIColorComponents);
            vertices_.push_back(v);
        }
    }

    const uint32_t idxStart = static_cast<uint32_t>(indices_.size());
    for (int i = 0; i < segCount; ++i)
    {
        const int j = (i + 1) % n;
        const uint32_t b0 = vBase + static_cast<uint32_t>(i * kStrokeProfileCount);
        const uint32_t b1 = vBase + static_cast<uint32_t>(j * kStrokeProfileCount);

        for (uint32_t band = 0; band < kStrokeBandCount; ++band)
        {
            indices_.push_back(b0 + band);
            indices_.push_back(b1 + band);
            indices_.push_back(b1 + band + 1);

            indices_.push_back(b0 + band);
            indices_.push_back(b1 + band + 1);
            indices_.push_back(b0 + band + 1);
        }
    }

    RecordDrawElements(static_cast<uint32_t>(indices_.size()) - idxStart, atlasTex);
}

void TacticalDrawList::AddConvexPolyFilled(const UIVec2* points, int pointsCount, uint32_t col)
{
    if (!owner_ || !points || pointsCount < kMinPolyVerts || !HasNonZeroAlpha(col))
    {
        return;
    }
    const UITextureID atlasTex = owner_->GetAtlasTextureID();
    if (atlasTex == 0)
    {
        return;
    }

    auto& pts = scratchPts_;
    FilterUniquePoints(points, pointsCount, true, pts);
    const int n = static_cast<int>(pts.size());
    if (n < kMinPolyVerts)
    {
        return;
    }

    float area2 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        area2 += pts[i].x * pts[j].y - pts[j].x * pts[i].y;
    }
    const float orientSign = (area2 >= 0.0f) ? 1.0f : -1.0f;

    auto& edgeNormals = scratchNormals_;
    edgeNormals.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        const float dx = pts[j].x - pts[i].x;
        const float dy = pts[j].y - pts[i].y;
        const float len = std::sqrt(dx * dx + dy * dy);
        edgeNormals[i] = (len > kEdgeLenEps)
            ? UIVec2((dy / len) * orientSign, (-dx / len) * orientSign)
            : UIVec2(0.0f, -1.0f);
    }

    float cOpaque[kUIColorComponents], cTrans[kUIColorComponents];
    UnpackOpaqueAndTransPair(col, cOpaque, cTrans);

    const UIVec2 whiteUv = owner_->GetWhitePixelUV();
    const uint32_t vBase = static_cast<uint32_t>(vertices_.size());

    for (int i = 0; i < n; ++i)
    {
        const UIVec2 dm = ComputeMiterNormal(edgeNormals[(i + n - 1) % n], edgeNormals[i]);

        UIVertex vIn{};
        vIn.pos[0] = pts[i].x - dm.x * kFillInnerInsetPx;
        vIn.pos[1] = pts[i].y - dm.y * kFillInnerInsetPx;
        vIn.uv[0]  = whiteUv.x;
        vIn.uv[1]  = whiteUv.y;
        std::memcpy(vIn.col, cOpaque, sizeof(float) * kUIColorComponents);
        vertices_.push_back(vIn);

        UIVertex vOut{};
        vOut.pos[0] = pts[i].x + dm.x * kFillOuterFringePx;
        vOut.pos[1] = pts[i].y + dm.y * kFillOuterFringePx;
        vOut.uv[0]  = whiteUv.x;
        vOut.uv[1]  = whiteUv.y;
        std::memcpy(vOut.col, cTrans, sizeof(float) * kUIColorComponents);
        vertices_.push_back(vOut);
    }

    const uint32_t idxStart = static_cast<uint32_t>(indices_.size());

    // Inner core triangle fan
    for (int i = 1; i < n - 1; ++i)
    {
        indices_.push_back(vBase + 0);
        indices_.push_back(vBase + static_cast<uint32_t>(i * 2));
        indices_.push_back(vBase + static_cast<uint32_t>((i + 1) * 2));
    }

    // Perimeter anti-aliased fringe strip
    for (int i = 0; i < n; ++i)
    {
        const int j = (i + 1) % n;
        const uint32_t iIn  = vBase + static_cast<uint32_t>(i * 2);
        const uint32_t iOut = iIn + 1;
        const uint32_t jIn  = vBase + static_cast<uint32_t>(j * 2);
        const uint32_t jOut = jIn + 1;

        indices_.push_back(iIn);
        indices_.push_back(jIn);
        indices_.push_back(jOut);

        indices_.push_back(iIn);
        indices_.push_back(jOut);
        indices_.push_back(iOut);
    }

    RecordDrawElements(static_cast<uint32_t>(indices_.size()) - idxStart, atlasTex);
}

void TacticalDrawList::AddRectFilled(UIVec2 pMin, UIVec2 pMax, uint32_t col, float rounding, int /*flags*/)
{
    if (!owner_ || !HasNonZeroAlpha(col) || pMax.x <= pMin.x || pMax.y <= pMin.y)
    {
        return;
    }

    const float r = (std::min)(rounding, (std::min)(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f);
    if (r <= kMinRoundingPx)
    {
        AddRectFilledMultiColor(pMin, pMax, col, col, col, col);
        return;
    }

    AppendRoundedRectPath(pMin, pMax, r);
    PathFillConvex(col);
}

void TacticalDrawList::AddRectFilledMultiColor(
    UIVec2 pMin,
    UIVec2 pMax,
    uint32_t colUprLeft,
    uint32_t colUprRight,
    uint32_t colBotRight,
    uint32_t colBotLeft)
{
    if (!owner_ || pMax.x <= pMin.x || pMax.y <= pMin.y)
    {
        return;
    }
    const uint32_t anyA =
        (colUprLeft  & UI_COL32_A_MASK) |
        (colUprRight & UI_COL32_A_MASK) |
        (colBotRight & UI_COL32_A_MASK) |
        (colBotLeft  & UI_COL32_A_MASK);
    if (anyA == 0)
    {
        return;
    }
    const UITextureID atlasTex = owner_->GetAtlasTextureID();
    if (atlasTex == 0)
    {
        return;
    }

    float c0[kUIColorComponents], c1[kUIColorComponents], c2[kUIColorComponents], c3[kUIColorComponents];
    UnpackColorLinear(colUprLeft,  c0);
    UnpackColorLinear(colUprRight, c1);
    UnpackColorLinear(colBotRight, c2);
    UnpackColorLinear(colBotLeft,  c3);

    const UIVec2 whiteUv = owner_->GetWhitePixelUV();
    AppendQuadGeometry(pMin, pMax, whiteUv, whiteUv, c0, c1, c2, c3);
    RecordDrawElements(kQuadIndexCount, atlasTex);
}

void TacticalDrawList::AddRect(UIVec2 pMin, UIVec2 pMax, uint32_t col, float rounding, int /*flags*/, float thickness)
{
    if (!owner_ || !HasNonZeroAlpha(col) || pMax.x <= pMin.x || pMax.y <= pMin.y)
    {
        return;
    }

    const float r = (std::min)(rounding, (std::min)(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f);
    if (r <= kMinRoundingPx)
    {
        const float ht = (std::max)(0.5f, thickness * 0.5f);
        const float midX = (pMin.x + pMax.x) * 0.5f;
        const float midY = (pMin.y + pMax.y) * 0.5f;
        const float ox0 = pMin.x - ht;
        const float oy0 = pMin.y - ht;
        const float ox1 = pMax.x + ht;
        const float oy1 = pMax.y + ht;
        const float ix0 = (std::min)(pMin.x + ht, midX);
        const float iy0 = (std::min)(pMin.y + ht, midY);
        const float ix1 = (std::max)(pMax.x - ht, midX);
        const float iy1 = (std::max)(pMax.y - ht, midY);

        // Non-overlapping 4-frame border quads (prevents corner alpha doubling even on thick 48px vignette borders)
        AddRectFilledMultiColor(UIVec2(ox0, oy0), UIVec2(ox1, iy0), col, col, col, col);
        AddRectFilledMultiColor(UIVec2(ox0, iy1), UIVec2(ox1, oy1), col, col, col, col);
        if (iy1 > iy0)
        {
            AddRectFilledMultiColor(UIVec2(ox0, iy0), UIVec2(ix0, iy1), col, col, col, col);
            AddRectFilledMultiColor(UIVec2(ix1, iy0), UIVec2(ox1, iy1), col, col, col, col);
        }
        return;
    }

    AppendRoundedRectPath(pMin, pMax, r);
    PathStroke(col, UIDrawFlags_Closed, thickness);
}

void TacticalDrawList::AddTriangle(UIVec2 p1, UIVec2 p2, UIVec2 p3, uint32_t col, float thickness)
{
    UIVec2 pts[kTriangleVertCount] = { p1, p2, p3 };
    AddPolyline(pts, kTriangleVertCount, col, UIDrawFlags_Closed, thickness);
}

void TacticalDrawList::AddTriangleFilled(UIVec2 p1, UIVec2 p2, UIVec2 p3, uint32_t col)
{
    UIVec2 pts[kTriangleVertCount] = { p1, p2, p3 };
    AddConvexPolyFilled(pts, kTriangleVertCount, col);
}

void TacticalDrawList::AddCircle(UIVec2 center, float radius, uint32_t col, int numSegments, float thickness)
{
    if (radius <= kMinCircleRadius || !HasNonZeroAlpha(col))
    {
        return;
    }
    UIVec2 pts[kMaxCircleSegments];
    const int segs = BuildCirclePoints(center, radius, numSegments, pts);
    AddPolyline(pts, segs, col, UIDrawFlags_Closed, thickness);
}

void TacticalDrawList::AddCircleFilled(UIVec2 center, float radius, uint32_t col, int numSegments)
{
    if (radius <= kMinCircleRadius || !HasNonZeroAlpha(col))
    {
        return;
    }
    UIVec2 pts[kMaxCircleSegments];
    const int segs = BuildCirclePoints(center, radius, numSegments, pts);
    AddConvexPolyFilled(pts, segs, col);
}

void TacticalDrawList::PathArcTo(UIVec2 center, float radius, float aMin, float aMax, int numSegments)
{
    if (radius <= 0.0f)
    {
        path_.push_back(center);
        return;
    }
    const int segs = (std::max)(1, numSegments);
    path_.reserve(path_.size() + static_cast<size_t>(segs + 1));
    const float invSegs = 1.0f / static_cast<float>(segs);
    for (int i = 0; i <= segs; ++i)
    {
        const float a = aMin + (aMax - aMin) * (static_cast<float>(i) * invSegs);
        path_.push_back(UIVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius));
    }
}

void TacticalDrawList::PathStroke(uint32_t col, int flags, float thickness)
{
    if (path_.size() >= 2)
    {
        AddPolyline(path_.data(), static_cast<int>(path_.size()), col, flags, thickness);
    }
    path_.clear();
}

void TacticalDrawList::PathFillConvex(uint32_t col)
{
    if (static_cast<int>(path_.size()) >= kMinPolyVerts)
    {
        AddConvexPolyFilled(path_.data(), static_cast<int>(path_.size()), col);
    }
    path_.clear();
}

void TacticalDrawList::AddImage(
    UITextureID userTextureId,
    UIVec2 pMin,
    UIVec2 pMax,
    UIVec2 uvMin,
    UIVec2 uvMax,
    uint32_t col)
{
    if (userTextureId == 0 || !HasNonZeroAlpha(col) || pMax.x == pMin.x || pMax.y == pMin.y)
    {
        return;
    }

    float cLin[kUIColorComponents];
    UnpackColorLinear(col, cLin);
    AppendUniformQuad(pMin, pMax, uvMin, uvMax, cLin);
    RecordDrawElements(kQuadIndexCount, userTextureId);
}

void TacticalDrawList::AddImageRounded(
    UITextureID userTextureId,
    UIVec2 pMin,
    UIVec2 pMax,
    UIVec2 uvMin,
    UIVec2 uvMax,
    uint32_t col,
    float rounding,
    int /*flags*/)
{
    if (userTextureId == 0 || !HasNonZeroAlpha(col))
    {
        return;
    }
    const float w = pMax.x - pMin.x;
    const float h = pMax.y - pMin.y;
    if (w <= 0.0f || h <= 0.0f)
    {
        return;
    }
    const float r = (std::min)(rounding, (std::min)(w, h) * 0.5f);
    if (r <= kMinRoundingPx)
    {
        AddImage(userTextureId, pMin, pMax, uvMin, uvMax, col);
        return;
    }

    constexpr int kRingMaxCount = kCornerCount * (kRoundedImageArcSteps + 1);
    UIVec2 ring[kRingMaxCount];
    uint32_t ringCount = 0;

    const UIVec2 centers[kCornerCount] = {
        UIVec2(pMax.x - r, pMin.y + r),
        UIVec2(pMax.x - r, pMax.y - r),
        UIVec2(pMin.x + r, pMax.y - r),
        UIVec2(pMin.x + r, pMin.y + r),
    };
    const float startAngles[kCornerCount] = { -kHalfPi, 0.0f, kHalfPi, kPi };
    const float endAngles[kCornerCount]   = { 0.0f, kHalfPi, kPi, kThreeHalfPi };
    constexpr float kInvArcSteps = 1.0f / static_cast<float>(kRoundedImageArcSteps);

    for (int c = 0; c < kCornerCount; ++c)
    {
        for (int i = 0; i <= kRoundedImageArcSteps; ++i)
        {
            const float a = startAngles[c] + (endAngles[c] - startAngles[c]) * (static_cast<float>(i) * kInvArcSteps);
            ring[ringCount++] = UIVec2(centers[c].x + std::cos(a) * r, centers[c].y + std::sin(a) * r);
        }
    }

    float cLin[kUIColorComponents];
    UnpackColorLinear(col, cLin);

    const uint32_t vBase = static_cast<uint32_t>(vertices_.size());
    const float invW = 1.0f / w;
    const float invH = 1.0f / h;
    const float du = uvMax.x - uvMin.x;
    const float dv = uvMax.y - uvMin.y;

    for (uint32_t idx = 0; idx < ringCount; ++idx)
    {
        const UIVec2& pt = ring[idx];
        const float tx = std::clamp((pt.x - pMin.x) * invW, 0.0f, 1.0f);
        const float ty = std::clamp((pt.y - pMin.y) * invH, 0.0f, 1.0f);
        UIVertex v{};
        v.pos[0] = pt.x;
        v.pos[1] = pt.y;
        v.uv[0]  = uvMin.x + du * tx;
        v.uv[1]  = uvMin.y + dv * ty;
        std::memcpy(v.col, cLin, sizeof(float) * kUIColorComponents);
        vertices_.push_back(v);
    }

    const uint32_t idxStart = static_cast<uint32_t>(indices_.size());
    for (uint32_t i = 1; i + 1 < ringCount; ++i)
    {
        indices_.push_back(vBase + 0);
        indices_.push_back(vBase + i);
        indices_.push_back(vBase + i + 1);
    }

    RecordDrawElements(static_cast<uint32_t>(indices_.size()) - idxStart, userTextureId);
}

void TacticalDrawList::AddText(UIVec2 pos, uint32_t col, const char* textBegin, const char* textEnd)
{
    if (!owner_)
    {
        return;
    }
    AddText(owner_->GetFont(UIFontRole::DefaultJP), kUIDefaultFontSize, pos, col, textBegin, textEnd);
}

void TacticalDrawList::AddText(
    const TacticalFont* font,
    float fontSize,
    UIVec2 pos,
    uint32_t col,
    const char* textBegin,
    const char* textEnd)
{
    if (!owner_ || !textBegin || !HasNonZeroAlpha(col))
    {
        return;
    }
    const UITextureID atlasTex = owner_->GetAtlasTextureID();
    if (atlasTex == 0)
    {
        return;
    }
    if (!textEnd)
    {
        textEnd = textBegin + std::strlen(textBegin);
    }
    if (textBegin >= textEnd)
    {
        return;
    }

    const UIFontRole role = font ? font->GetRole() : UIFontRole::DefaultJP;
    const float reqSize = (fontSize > 1.0f) ? fontSize : (font ? font->GetDefaultSize() : kUIDefaultFontSize);
    const int bucketPx = std::clamp(static_cast<int>(std::round(reqSize)), kMinFontBucketPx, kMaxFontBucketPx);
    const float scale = reqSize / static_cast<float>(bucketPx);
    const float lineStep = owner_->GetBucketLineHeight(role, bucketPx) * scale;

    float cLin[kUIColorComponents];
    UnpackColorLinear(col, cLin);

    const bool snapToPixel = (std::abs(scale - 1.0f) < kPixelSnapScaleEps);
    const float baseX = snapToPixel ? std::round(pos.x) : pos.x;
    float curX = baseX;
    float curY = snapToPixel ? std::round(pos.y) : pos.y;

    const uint32_t idxStart = static_cast<uint32_t>(indices_.size());
    const char* p = textBegin;
    while (p < textEnd && *p != '\0')
    {
        const uint32_t cp = DecodeNextUtf8(p, textEnd);
        if (cp == 0)
        {
            break;
        }
        if (cp == '\r')
        {
            continue;
        }
        if (cp == '\n')
        {
            curX = baseX;
            curY += lineStep;
            continue;
        }

        const TacticalUICanvas::GlyphQuad* g = owner_->AcquireGlyph(role, bucketPx, cp);
        if (!g)
        {
            continue;
        }

        if (g->hasQuad)
        {
            AppendUniformQuad(
                UIVec2(curX + g->x0 * scale, curY + g->y0 * scale),
                UIVec2(curX + g->x1 * scale, curY + g->y1 * scale),
                UIVec2(g->u0, g->v0),
                UIVec2(g->u1, g->v1),
                cLin);
        }

        curX += g->advanceX * scale;
    }

    const uint32_t emitted = static_cast<uint32_t>(indices_.size()) - idxStart;
    if (emitted > 0)
    {
        RecordDrawElements(emitted, atlasTex);
    }
}

// =============================================================================
// TacticalUICanvas
// =============================================================================

TacticalUICanvas* TacticalUICanvas::GetInstance()
{
    static TacticalUICanvas instance;
    return &instance;
}

bool TacticalUICanvas::Initialize(DirectXCom* dx, WindowAPI* windowAPI)
{
    if (initialized_)
    {
        return true;
    }
    if (!dx || !dx->GetDevice())
    {
        return false;
    }

    dx_ = dx;
    windowAPI_ = windowAPI;
    startTime_ = std::chrono::steady_clock::now();

    backgroundList_.SetOwner(this);
    foregroundList_.SetOwner(this);
    postTransitionList_.SetOwner(this);

    fonts_[0] = TacticalFont(this, UIFontRole::DefaultJP,   kUIDefaultFontSize);
    fonts_[1] = TacticalFont(this, UIFontRole::TitleBoldJP, kTitleBoldDefaultPx);
    fonts_[2] = TacticalFont(this, UIFontRole::SubBoldJP,   kSubBoldDefaultPx);

    memDC_ = CreateCompatibleDC(nullptr);
    if (!memDC_)
    {
        return false;
    }
    SetMapMode(memDC_, MM_TEXT);
    fontFaceName_ = SelectBestJapaneseUIFont(memDC_);

    if (!CreatePipeline() || !CreateFontAtlas() || !CreateDynamicBuffers())
    {
        Finalize();
        return false;
    }

    if (windowAPI_ && windowAPI_->GetHwnd())
    {
        subclassedHwnd_ = windowAPI_->GetHwnd();
        SetWindowSubclass(
            subclassedHwnd_,
            WheelSubclassProc,
            kWheelSubclassId,
            reinterpret_cast<DWORD_PTR>(this));
    }

    // Pre-warm common ASCII & Japanese HUD characters at 16px so first gameplay frame is instant
    for (uint32_t ch = kPrewarmAsciiFirst; ch <= kPrewarmAsciiLast; ++ch)
    {
        AcquireGlyph(UIFontRole::DefaultJP, kDefaultBucketPx, ch);
    }

    initialized_ = true;
    BeginFrame();
    return true;
}

void TacticalUICanvas::Finalize()
{
    if (subclassedHwnd_)
    {
        RemoveWindowSubclass(subclassedHwnd_, WheelSubclassProc, kWheelSubclassId);
        subclassedHwnd_ = nullptr;
    }

    for (auto& kv : fontBuckets_)
    {
        if (kv.second.hFont)
        {
            DeleteObject(kv.second.hFont);
            kv.second.hFont = nullptr;
        }
    }
    fontBuckets_.clear();

    if (memDC_)
    {
        DeleteDC(memDC_);
        memDC_ = nullptr;
    }

    for (uint32_t i = 0; i < kFrameSlots; ++i)
    {
        if (vertexBuffers_[i] && vbMapped_[i])
        {
            vertexBuffers_[i]->Unmap(0, nullptr);
            vbMapped_[i] = nullptr;
        }
        if (indexBuffers_[i] && ibMapped_[i])
        {
            indexBuffers_[i]->Unmap(0, nullptr);
            ibMapped_[i] = nullptr;
        }
        vertexBuffers_[i].Reset();
        indexBuffers_[i].Reset();
    }

    if (atlasUploadBuffer_ && atlasUploadMapped_)
    {
        atlasUploadBuffer_->Unmap(0, nullptr);
        atlasUploadMapped_ = nullptr;
    }
    atlasUploadBuffer_.Reset();
    atlasGpuTexture_.Reset();

    if (atlasSrvIndex_ != UINT32_MAX)
    {
        if (TextureManager::GetInstance() && TextureManager::GetInstance()->GetSRVManager())
        {
            TextureManager::GetInstance()->GetSRVManager()->Free(atlasSrvIndex_);
        }
        atlasSrvIndex_ = UINT32_MAX;
    }
    atlasGpuHandle_ = 0;

    pipelineState_.Reset();
    rootSignature_.Reset();

    dx_ = nullptr;
    windowAPI_ = nullptr;
    initialized_ = false;
}

bool TacticalUICanvas::CreatePipeline()
{
    auto* device = dx_->GetDevice().Get();

    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors = 1;
    srvRange.BaseShaderRegister = 0;
    srvRange.RegisterSpace = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParams[2]{};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.ShaderRegister = 0;
    rootParams[0].Constants.RegisterSpace = 0;
    rootParams[0].Constants.Num32BitValues = kRootConstFloatCount;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[1].DescriptorTable.pDescriptorRanges = &srvRange;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC samplerDesc{};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = 0;
    samplerDesc.RegisterSpace = 0;
    samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters = _countof(rootParams);
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers = &samplerDesc;
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr))
    {
        return false;
    }
    hr = device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr))
    {
        return false;
    }

    auto vsBlob = dx_->CompileShader(
        L"Resources/shaders/TacticalUI.VS.hlsl",
        L"vs_6_0",
        dx_->GetDxcUtils(),
        dx_->GetDxcCompiler(),
        dx_->GetIncludeHandler(),
        std::cout);
    auto psBlob = dx_->CompileShader(
        L"Resources/shaders/TacticalUI.PS.hlsl",
        L"ps_6_0",
        dx_->GetDxcUtils(),
        dx_->GetDxcCompiler(),
        dx_->GetIncludeHandler(),
        std::cout);
    if (!vsBlob || !psBlob)
    {
        return false;
    }

    D3D12_INPUT_ELEMENT_DESC inputElems[kInputElementCount]{};
    inputElems[0] = { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,                   D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, kTexcoordByteOffset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
    inputElems[2] = { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, kColorByteOffset,    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElems, _countof(inputElems) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    auto& rtBlend = psoDesc.BlendState.RenderTarget[0];
    rtBlend.BlendEnable = TRUE;
    rtBlend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    rtBlend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOp = D3D12_BLEND_OP_ADD;
    rtBlend.SrcBlendAlpha = D3D12_BLEND_ONE;
    rtBlend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = dx_->GetRtvDesc().Format;
    psoDesc.SampleDesc.Count = 1;

    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    return SUCCEEDED(hr);
}

bool TacticalUICanvas::CreateFontAtlas()
{
    auto* device = dx_->GetDevice().Get();
    SRVManager* srvMgr = TextureManager::GetInstance() ? TextureManager::GetInstance()->GetSRVManager() : nullptr;
    if (!srvMgr)
    {
        return false;
    }

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = static_cast<UINT64>(kAtlasWidth);
    texDesc.Height = static_cast<UINT>(kAtlasHeight);
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        nullptr,
        IID_PPV_ARGS(&atlasGpuTexture_));
    if (FAILED(hr))
    {
        return false;
    }

    UINT numRows = 0;
    UINT64 rowSizeBytes = 0;
    UINT64 totalBytes = 0;
    device->GetCopyableFootprints(&texDesc, 0, 1, 0, &atlasFootprint_, &numRows, &rowSizeBytes, &totalBytes);

    atlasUploadBuffer_ = dx_->CreateBufferResource(dx_->GetDevice(), static_cast<size_t>(totalBytes));
    if (!atlasUploadBuffer_)
    {
        return false;
    }

    hr = atlasUploadBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&atlasUploadMapped_));
    if (FAILED(hr) || !atlasUploadMapped_)
    {
        return false;
    }

    // Initialize entire upload surface to pure white RGB + 0 alpha so bilinear filtering never darkens glyph edges
    const UINT rowPitch = atlasFootprint_.Footprint.RowPitch;
    for (int y = 0; y < kAtlasHeight; ++y)
    {
        uint32_t* rowPtr = reinterpret_cast<uint32_t*>(
            atlasUploadMapped_ + atlasFootprint_.Offset + static_cast<size_t>(y) * rowPitch);
        std::fill_n(rowPtr, kAtlasWidth, kTransparentWhiteTexel);
    }

    // 4x4 opaque white block at top-left (0..3, 0..3) shared by all solid vector geometry
    for (int y = 0; y < kWhiteBlockSize; ++y)
    {
        uint32_t* rowPtr = reinterpret_cast<uint32_t*>(
            atlasUploadMapped_ + atlasFootprint_.Offset + static_cast<size_t>(y) * rowPitch);
        std::fill_n(rowPtr, kWhiteBlockSize, kOpaqueWhiteTexel);
    }

    penX_ = kInitialPenX;
    penY_ = kInitialPenY;
    rowMaxH_ = 0;
    atlasDirtyYMin_ = 0;
    atlasDirtyYMax_ = kInitialPenY;

    atlasSrvIndex_ = srvMgr->Allocate();
    srvMgr->CreateSRVForTexture2D(atlasSrvIndex_, atlasGpuTexture_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    atlasGpuHandle_ = srvMgr->GetGPUDescriptorHandle(atlasSrvIndex_).ptr;
    return atlasGpuHandle_ != 0;
}

bool TacticalUICanvas::CreateDynamicBuffers()
{
    for (uint32_t i = 0; i < kFrameSlots; ++i)
    {
        vertexBuffers_[i] = dx_->CreateBufferResource(dx_->GetDevice(), sizeof(UIVertex) * kMaxVerticesPerFrame);
        indexBuffers_[i]  = dx_->CreateBufferResource(dx_->GetDevice(), sizeof(uint32_t) * kMaxIndicesPerFrame);
        if (!vertexBuffers_[i] || !indexBuffers_[i])
        {
            return false;
        }
        if (FAILED(vertexBuffers_[i]->Map(0, nullptr, reinterpret_cast<void**>(&vbMapped_[i]))) ||
            FAILED(indexBuffers_[i]->Map(0, nullptr, reinterpret_cast<void**>(&ibMapped_[i]))))
        {
            return false;
        }
    }
    return true;
}

void TacticalUICanvas::BeginFrame()
{
    if (!initialized_)
    {
        return;
    }

    float w = static_cast<float>(WindowAPI::GetClientWidth());
    float h = static_cast<float>(WindowAPI::GetClientHeight());
    if (w <= 0.0f) w = kUIDefaultDisplayWidth;
    if (h <= 0.0f) h = kUIDefaultDisplayHeight;
    displaySize_ = UIVec2(w, h);

    backgroundList_.Reset(w, h);
    foregroundList_.Reset(w, h);
    postTransitionList_.Reset(w, h);

    frameWheel_ = wheelAccum_;
    wheelAccum_ = 0.0f;

    HWND hwnd = windowAPI_ ? windowAPI_->GetHwnd() : nullptr;
    const bool isForeground = (!hwnd || GetForegroundWindow() == hwnd);

    if (hwnd)
    {
        POINT pt{};
        RECT rc{};
        if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt) && GetClientRect(hwnd, &rc))
        {
            const float cw = static_cast<float>(rc.right - rc.left);
            const float ch = static_cast<float>(rc.bottom - rc.top);
            if (cw > 0.0f && ch > 0.0f)
            {
                mousePos_.x = static_cast<float>(pt.x) * (w / cw);
                mousePos_.y = static_cast<float>(pt.y) * (h / ch);
            }
        }
    }

    const int vkButtons[kMouseButtonCount] = { VK_LBUTTON, VK_RBUTTON, VK_MBUTTON };
    const bool imguiCapturingMouse = AppDebugState::IsImGuiCapturingMouse();
    for (size_t b = 0; b < kMouseButtonCount; ++b)
    {
        const bool curDown = isForeground && !imguiCapturingMouse && ((GetAsyncKeyState(vkButtons[b]) & kUIAsyncKeyDownMask) != 0);
        mouseClicked_[b] = (curDown && !mouseDown_[b]);
        mouseDown_[b] = curDown;
    }
}

void TacticalUICanvas::FlushAtlasUpload(ID3D12GraphicsCommandList* cmdList)
{
    if (!cmdList || atlasDirtyYMin_ >= atlasDirtyYMax_ || !atlasGpuTexture_ || !atlasUploadBuffer_)
    {
        return;
    }

    const int y0 = std::clamp(atlasDirtyYMin_, 0, kAtlasHeight - 1);
    const int y1 = std::clamp(atlasDirtyYMax_, y0 + 1, kAtlasHeight);

    D3D12_RESOURCE_BARRIER toCopy{};
    toCopy.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toCopy.Transition.pResource = atlasGpuTexture_.Get();
    toCopy.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toCopy.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    toCopy.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
    cmdList->ResourceBarrier(1, &toCopy);

    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
    dstLoc.pResource = atlasGpuTexture_.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
    srcLoc.pResource = atlasUploadBuffer_.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint = atlasFootprint_;

    D3D12_BOX box{ 0, static_cast<UINT>(y0), 0, static_cast<UINT>(kAtlasWidth), static_cast<UINT>(y1), 1 };
    cmdList->CopyTextureRegion(&dstLoc, 0, static_cast<UINT>(y0), 0, &srcLoc, &box);

    D3D12_RESOURCE_BARRIER toSrv = toCopy;
    toSrv.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toSrv.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &toSrv);

    atlasDirtyYMin_ = kAtlasHeight;
    atlasDirtyYMax_ = 0;
}

void TacticalUICanvas::RenderAll(ID3D12GraphicsCommandList* cmdList)
{
    if (!initialized_ || !cmdList || !dx_)
    {
        return;
    }

    FlushAtlasUpload(cmdList);

    const TacticalDrawList* layers[kDrawLayerCount] = {
        &backgroundList_,
        &foregroundList_,
        &postTransitionList_,
    };

    size_t totalVerts = 0;
    size_t totalIndices = 0;
    for (const TacticalDrawList* layer : layers)
    {
        totalVerts   += layer->GetVertices().size();
        totalIndices += layer->GetIndices().size();
    }
    if (totalVerts == 0 || totalIndices == 0)
    {
        return;
    }

    frameSlot_ = (frameSlot_ + 1) % kFrameSlots;
    UIVertex* vDst = vbMapped_[frameSlot_];
    uint32_t* iDst = ibMapped_[frameSlot_];
    if (!vDst || !iDst)
    {
        return;
    }

    uint32_t layerBaseVertex[kDrawLayerCount] = {};
    uint32_t layerBaseIndex[kDrawLayerCount]  = {};
    bool layerValid[kDrawLayerCount]          = {};

    uint32_t curV = 0;
    uint32_t curI = 0;
    for (size_t li = 0; li < kDrawLayerCount; ++li)
    {
        const auto& verts = layers[li]->GetVertices();
        const auto& idxs  = layers[li]->GetIndices();
        if (verts.empty() || idxs.empty())
        {
            continue;
        }
        if (curV + verts.size() > kMaxVerticesPerFrame ||
            curI + idxs.size()  > kMaxIndicesPerFrame)
        {
            break;
        }

        layerBaseVertex[li] = curV;
        layerBaseIndex[li]  = curI;
        layerValid[li]      = true;

        std::memcpy(vDst + curV, verts.data(), verts.size() * sizeof(UIVertex));
        std::memcpy(iDst + curI, idxs.data(),  idxs.size()  * sizeof(uint32_t));

        curV += static_cast<uint32_t>(verts.size());
        curI += static_cast<uint32_t>(idxs.size());
    }

    if (curI == 0)
    {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = { dx_->GetSrvDescriptorHeap().Get() };
    cmdList->SetDescriptorHeaps(1, heaps);

    cmdList->SetGraphicsRootSignature(rootSignature_.Get());
    cmdList->SetPipelineState(pipelineState_.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv{ vertexBuffers_[frameSlot_]->GetGPUVirtualAddress(), curV * static_cast<UINT>(sizeof(UIVertex)), sizeof(UIVertex) };
    cmdList->IASetVertexBuffers(0, 1, &vbv);

    D3D12_INDEX_BUFFER_VIEW ibv{ indexBuffers_[frameSlot_]->GetGPUVirtualAddress(), curI * static_cast<UINT>(sizeof(uint32_t)), DXGI_FORMAT_R32_UINT };
    cmdList->IASetIndexBuffer(&ibv);

    D3D12_VIEWPORT vp{ 0.0f, 0.0f, displaySize_.x, displaySize_.y, 0.0f, 1.0f };
    cmdList->RSSetViewports(1, &vp);

    const float rootConsts[kRootConstFloatCount] = { 1.0f / displaySize_.x, 1.0f / displaySize_.y, 0.0f, 0.0f };
    cmdList->SetGraphicsRoot32BitConstants(0, kRootConstFloatCount, rootConsts, 0);

    UITextureID boundTex = 0;
    D3D12_RECT boundScissor{ -1, -1, -1, -1 };

    const LONG minCoord = 0;
    const LONG maxW = static_cast<LONG>(displaySize_.x);
    const LONG maxH = static_cast<LONG>(displaySize_.y);

    for (size_t li = 0; li < kDrawLayerCount; ++li)
    {
        if (!layerValid[li])
        {
            continue;
        }
        const int32_t vBias = static_cast<int32_t>(layerBaseVertex[li]);
        const uint32_t iBias = layerBaseIndex[li];

        for (const UIDrawCmd& cmd : layers[li]->GetCommands())
        {
            if (cmd.indexCount == 0 || cmd.textureId == 0)
            {
                continue;
            }

            D3D12_RECT sc{};
            sc.left   = std::clamp(static_cast<LONG>(std::floor(cmd.clipRect.x0)), minCoord, maxW);
            sc.top    = std::clamp(static_cast<LONG>(std::floor(cmd.clipRect.y0)), minCoord, maxH);
            sc.right  = std::clamp(static_cast<LONG>(std::ceil(cmd.clipRect.x1)),  minCoord, maxW);
            sc.bottom = std::clamp(static_cast<LONG>(std::ceil(cmd.clipRect.y1)),  minCoord, maxH);
            if (sc.right <= sc.left || sc.bottom <= sc.top)
            {
                continue;
            }

            if (sc.left != boundScissor.left || sc.top != boundScissor.top ||
                sc.right != boundScissor.right || sc.bottom != boundScissor.bottom)
            {
                cmdList->RSSetScissorRects(1, &sc);
                boundScissor = sc;
            }

            if (cmd.textureId != boundTex)
            {
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
                gpuHandle.ptr = cmd.textureId;
                cmdList->SetGraphicsRootDescriptorTable(1, gpuHandle);
                boundTex = cmd.textureId;
            }

            cmdList->DrawIndexedInstanced(cmd.indexCount, 1, iBias + cmd.indexOffset, vBias, 0);
        }
    }

    // Restore default engine viewport & scissor rect
    cmdList->RSSetViewports(1, &dx_->GetViewport());
    cmdList->RSSetScissorRects(1, &dx_->GetScissorRect());
}

const TacticalFont* TacticalUICanvas::GetFont(UIFontRole role) const
{
    const size_t idx = static_cast<size_t>(role);
    return (idx < kFontRoleCount) ? &fonts_[idx] : &fonts_[0];
}

UIVec2 TacticalUICanvas::CalcTextSize(
    const char* textBegin,
    const char* textEnd,
    float fontSize,
    UIFontRole role)
{
    if (!initialized_ || !textBegin)
    {
        return UIVec2(0.0f, 0.0f);
    }
    if (!textEnd)
    {
        textEnd = textBegin + std::strlen(textBegin);
    }
    if (textBegin >= textEnd)
    {
        return UIVec2(0.0f, fontSize);
    }

    const float reqSize = (fontSize > 1.0f) ? fontSize : kUIDefaultFontSize;
    const int bucketPx = std::clamp(static_cast<int>(std::round(reqSize)), kMinFontBucketPx, kMaxFontBucketPx);
    const float scale = reqSize / static_cast<float>(bucketPx);
    const float lineStep = GetBucketLineHeight(role, bucketPx) * scale;

    float maxW = 0.0f;
    float curW = 0.0f;
    int lines = 1;

    const char* p = textBegin;
    while (p < textEnd && *p != '\0')
    {
        const uint32_t cp = DecodeNextUtf8(p, textEnd);
        if (cp == 0)
        {
            break;
        }
        if (cp == '\r')
        {
            continue;
        }
        if (cp == '\n')
        {
            if (curW > maxW) maxW = curW;
            curW = 0.0f;
            ++lines;
            continue;
        }

        const GlyphQuad* g = AcquireGlyph(role, bucketPx, cp);
        if (g)
        {
            curW += g->advanceX * scale;
        }
    }
    if (curW > maxW)
    {
        maxW = curW;
    }

    return UIVec2(maxW, static_cast<float>(lines) * lineStep);
}

double TacticalUICanvas::GetTime() const
{
    if (!initialized_)
    {
        return 0.0;
    }
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
}

bool TacticalUICanvas::IsMouseClicked(int button) const
{
    if (button < 0 || static_cast<size_t>(button) >= kMouseButtonCount)
    {
        return false;
    }
    return mouseClicked_[button];
}

float TacticalUICanvas::ConsumeMouseWheel()
{
    const float w = frameWheel_;
    frameWheel_ = 0.0f;
    return w;
}

TacticalUICanvas::FontBucket& TacticalUICanvas::GetOrCreateFontBucket(UIFontRole role, int pixelHeight)
{
    FontBucketKey key{ static_cast<uint8_t>(role), pixelHeight };
    auto it = fontBuckets_.find(key);
    if (it != fontBuckets_.end())
    {
        return it->second;
    }

    FontBucket bucket{};
    bucket.pixelHeight = pixelHeight;

    const int weight = (role == UIFontRole::DefaultJP) ? FW_MEDIUM : FW_BOLD;
    bucket.hFont = CreateFontW(
        -pixelHeight,
        0,
        0,
        0,
        weight,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_TT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        fontFaceName_.c_str());

    if (memDC_ && bucket.hFont)
    {
        SelectObject(memDC_, bucket.hFont);
        TEXTMETRICW tm{};
        if (GetTextMetricsW(memDC_, &tm))
        {
            bucket.ascent  = tm.tmAscent;
            bucket.descent = tm.tmDescent;
            bucket.baseOffsetY = static_cast<float>(pixelHeight - (tm.tmAscent + tm.tmDescent)) * 0.5f +
                                 static_cast<float>(tm.tmAscent);
        }
    }

    auto inserted = fontBuckets_.emplace(key, std::move(bucket));
    return inserted.first->second;
}

float TacticalUICanvas::GetBucketLineHeight(UIFontRole role, int pixelHeight)
{
    FontBucket& b = GetOrCreateFontBucket(role, pixelHeight);
    return static_cast<float>((std::max)(b.pixelHeight, b.ascent + b.descent));
}

const TacticalUICanvas::GlyphQuad* TacticalUICanvas::AcquireGlyph(
    UIFontRole role,
    int pixelHeight,
    uint32_t codepoint)
{
    if (!memDC_ || !atlasUploadMapped_)
    {
        return nullptr;
    }

    FontBucket& bucket = GetOrCreateFontBucket(role, pixelHeight);
    auto found = bucket.glyphs.find(codepoint);
    if (found != bucket.glyphs.end())
    {
        return &found->second;
    }
    if (!bucket.hFont)
    {
        return nullptr;
    }

    const auto emplaceSpacingGlyph = [&](float advanceX) -> const GlyphQuad*
    {
        GlyphQuad g{};
        g.hasQuad  = false;
        g.advanceX = advanceX;
        auto res = bucket.glyphs.emplace(codepoint, g);
        return &res.first->second;
    };

    SelectObject(memDC_, bucket.hFont);

    const wchar_t wch = (codepoint <= kBmpMaxCodepoint) ? static_cast<wchar_t>(codepoint) : L'?';

    // Explicit fast path for ASCII / full-width spaces
    if (wch == L' ' || wch == L'\u3000' || wch == L'\t')
    {
        SIZE sz{};
        if (!GetTextExtentPoint32W(memDC_, &wch, 1, &sz))
        {
            sz.cx = (wch == L'\u3000') ? pixelHeight : (pixelHeight / 2);
        }
        return emplaceSpacingGlyph(static_cast<float>((wch == L'\t') ? (sz.cx * kTabSpaces) : sz.cx));
    }

    GLYPHMETRICS gm{};
    MAT2 mat{};
    mat.eM11.value = 1;
    mat.eM22.value = 1;

    const DWORD bufSize = GetGlyphOutlineW(memDC_, wch, GGO_GRAY8_BITMAP, &gm, 0, nullptr, &mat);
    if (bufSize == GDI_ERROR || bufSize == 0 || gm.gmBlackBoxX == 0 || gm.gmBlackBoxY == 0)
    {
        SIZE sz{};
        GetTextExtentPoint32W(memDC_, &wch, 1, &sz);
        return emplaceSpacingGlyph((sz.cx > 0) ? static_cast<float>(sz.cx) : static_cast<float>(gm.gmCellIncX));
    }

    glyphScratch_.assign(bufSize, 0);
    if (GetGlyphOutlineW(memDC_, wch, GGO_GRAY8_BITMAP, &gm, bufSize, glyphScratch_.data(), &mat) == GDI_ERROR)
    {
        return emplaceSpacingGlyph(static_cast<float>(gm.gmCellIncX));
    }

    const int gw = static_cast<int>(gm.gmBlackBoxX);
    const int gh = static_cast<int>(gm.gmBlackBoxY);

    if (penX_ + gw + kGlyphAtlasPad >= kAtlasWidth)
    {
        penX_ = kInitialPenX;
        penY_ += rowMaxH_ + kGlyphAtlasPad;
        rowMaxH_ = 0;
    }
    if (penY_ + gh + kGlyphAtlasPad >= kAtlasHeight)
    {
        // Atlas full safety fallback (2048x2048 holds >15,000 glyphs so never hit in normal operation)
        return emplaceSpacingGlyph(static_cast<float>(gm.gmCellIncX));
    }

    const int dstX = penX_;
    const int dstY = penY_;
    penX_ += gw + kGlyphAtlasPad;
    if (gh > rowMaxH_)
    {
        rowMaxH_ = gh;
    }

    const int srcPitch = (gw + kDwordAlignMask) & ~kDwordAlignMask;
    const UINT dstRowPitch = atlasFootprint_.Footprint.RowPitch;

    for (int y = 0; y < gh; ++y)
    {
        const uint8_t* srcRow = glyphScratch_.data() + static_cast<size_t>(y) * srcPitch;
        uint32_t* dstRow = reinterpret_cast<uint32_t*>(
            atlasUploadMapped_ + atlasFootprint_.Offset + static_cast<size_t>(dstY + y) * dstRowPitch);

        for (int x = 0; x < gw; ++x)
        {
            const uint8_t raw64 = srcRow[x];
            if (raw64 == 0)
            {
                dstRow[dstX + x] = kTransparentWhiteTexel;
            }
            else
            {
                const float cov = std::clamp(static_cast<float>(raw64) * kInvGdiGray8Max, 0.0f, 1.0f);
                const float crispCov = std::pow(cov, kGlyphCrispGamma);
                const uint32_t alphaByte = static_cast<uint32_t>(
                    std::clamp(static_cast<int>(std::round(crispCov * kMaxByteFloat)), 0, UI_COL32_CHANNEL_MAX));
                dstRow[dstX + x] = (alphaByte << UI_COL32_A_SHIFT) | kTransparentWhiteTexel;
            }
        }
    }

    atlasDirtyYMin_ = (std::min)(atlasDirtyYMin_, (std::max)(0, dstY - 1));
    atlasDirtyYMax_ = (std::max)(atlasDirtyYMax_, (std::min)(kAtlasHeight, dstY + gh + 1));

    constexpr float kInvAtlasW = 1.0f / static_cast<float>(kAtlasWidth);
    constexpr float kInvAtlasH = 1.0f / static_cast<float>(kAtlasHeight);

    GlyphQuad quad{};
    quad.hasQuad  = true;
    quad.x0       = static_cast<float>(gm.gmptGlyphOrigin.x);
    quad.y0       = bucket.baseOffsetY - static_cast<float>(gm.gmptGlyphOrigin.y);
    quad.x1       = quad.x0 + static_cast<float>(gw);
    quad.y1       = quad.y0 + static_cast<float>(gh);
    quad.u0       = static_cast<float>(dstX) * kInvAtlasW;
    quad.v0       = static_cast<float>(dstY) * kInvAtlasH;
    quad.u1       = static_cast<float>(dstX + gw) * kInvAtlasW;
    quad.v1       = static_cast<float>(dstY + gh) * kInvAtlasH;
    quad.advanceX = static_cast<float>(gm.gmCellIncX);

    auto res = bucket.glyphs.emplace(codepoint, quad);
    return &res.first->second;
}

bool TacticalUICanvas::IsWindowFocused() const
{
    HWND hwnd = windowAPI_ ? windowAPI_->GetHwnd() : nullptr;
    return (!hwnd || GetForegroundWindow() == hwnd);
}

bool TacticalUICanvas::IsKeyDown(int vk) const
{
    return IsWindowFocused() && ((GetAsyncKeyState(vk) & kUIAsyncKeyDownMask) != 0);
}
