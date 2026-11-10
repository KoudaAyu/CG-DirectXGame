#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <wrl.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class DirectXCom;
class WindowAPI;

struct UIVec2
{
    float x = 0.0f;
    float y = 0.0f;

    constexpr UIVec2() = default;
    constexpr UIVec2(float _x, float _y) : x(_x), y(_y) {}
};

using UITextureID = uint64_t;

constexpr int UI_COL32_R_SHIFT = 0;
constexpr int UI_COL32_G_SHIFT = 8;
constexpr int UI_COL32_B_SHIFT = 16;
constexpr int UI_COL32_A_SHIFT = 24;
constexpr int UI_COL32_CHANNEL_MAX = 255;
constexpr uint32_t UI_COL32_A_MASK = 0xFF000000u;

constexpr float kUIDefaultFontSize      = 16.0f;
constexpr float kUIDefaultDisplayWidth  = 1280.0f;
constexpr float kUIDefaultDisplayHeight = 720.0f;
constexpr int   kUIDefaultArcSegments   = 10;
constexpr size_t kUIColorComponents     = 4;
constexpr SHORT kUIAsyncKeyDownMask     = static_cast<SHORT>(0x8000);

inline constexpr uint32_t UI_COL32(int r, int g, int b, int a)
{
    const uint32_t cr = static_cast<uint32_t>(r < 0 ? 0 : (r > UI_COL32_CHANNEL_MAX ? UI_COL32_CHANNEL_MAX : r));
    const uint32_t cg = static_cast<uint32_t>(g < 0 ? 0 : (g > UI_COL32_CHANNEL_MAX ? UI_COL32_CHANNEL_MAX : g));
    const uint32_t cb = static_cast<uint32_t>(b < 0 ? 0 : (b > UI_COL32_CHANNEL_MAX ? UI_COL32_CHANNEL_MAX : b));
    const uint32_t ca = static_cast<uint32_t>(a < 0 ? 0 : (a > UI_COL32_CHANNEL_MAX ? UI_COL32_CHANNEL_MAX : a));
    return (ca << UI_COL32_A_SHIFT) |
           (cb << UI_COL32_B_SHIFT) |
           (cg << UI_COL32_G_SHIFT) |
           (cr << UI_COL32_R_SHIFT);
}

constexpr uint32_t UI_COL32_WHITE       = UI_COL32(UI_COL32_CHANNEL_MAX, UI_COL32_CHANNEL_MAX, UI_COL32_CHANNEL_MAX, UI_COL32_CHANNEL_MAX);
constexpr uint32_t UI_COL32_BLACK_TRANS = UI_COL32(0, 0, 0, 0);

enum UIDrawFlags : int
{
    UIDrawFlags_None   = 0,
    UIDrawFlags_Closed = 1 << 0,
};

enum class UIFontRole : uint8_t
{
    DefaultJP   = 0, // BIZ UDPGothic / Yu Gothic UI Medium (HUD・案内用)
    TitleBoldJP = 1, // BIZ UDPGothic / Yu Gothic UI Bold (大見出し用)
    SubBoldJP   = 2, // BIZ UDPGothic / Yu Gothic UI Bold (無線字幕・小見出し用)
};

class TacticalUICanvas;

class TacticalFont
{
public:
    TacticalFont() = default;
    TacticalFont(TacticalUICanvas* owner, UIFontRole role, float defaultPixelSize)
        : owner_(owner), role_(role), defaultPixelSize_(defaultPixelSize) {}

    UIFontRole GetRole() const { return role_; }
    float GetDefaultSize() const { return defaultPixelSize_; }

    UIVec2 CalcTextSizeA(
        float size,
        float maxWidth,
        float wrapWidth,
        const char* textBegin,
        const char* textEnd = nullptr) const;

private:
    TacticalUICanvas* owner_ = nullptr;
    UIFontRole role_ = UIFontRole::DefaultJP;
    float defaultPixelSize_ = kUIDefaultFontSize;
};

struct UIVertex
{
    float pos[2];
    float uv[2];
    float col[kUIColorComponents]; // Linear RGBA [0..1] for _SRGB BackBuffer accuracy
};

struct UIClipRect
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = kUIDefaultDisplayWidth;
    float y1 = kUIDefaultDisplayHeight;

    bool operator==(const UIClipRect& rhs) const
    {
        return x0 == rhs.x0 && y0 == rhs.y0 && x1 == rhs.x1 && y1 == rhs.y1;
    }
};

struct UIDrawCmd
{
    uint32_t indexOffset = 0;
    uint32_t indexCount  = 0;
    UITextureID textureId = 0;
    UIClipRect clipRect{};
};

class TacticalDrawList
{
public:
    explicit TacticalDrawList(TacticalUICanvas* owner = nullptr) : owner_(owner) {}

    void SetOwner(TacticalUICanvas* owner) { owner_ = owner; }
    void Reset(float displayW, float displayH);

    void PushClipRect(UIVec2 clipMin, UIVec2 clipMax, bool intersectWithCurrentClipRect = false);
    void PopClipRect();

    void AddLine(UIVec2 p1, UIVec2 p2, uint32_t col, float thickness = 1.0f);
    void AddRect(UIVec2 pMin, UIVec2 pMax, uint32_t col, float rounding = 0.0f, int flags = 0, float thickness = 1.0f);
    void AddRectFilled(UIVec2 pMin, UIVec2 pMax, uint32_t col, float rounding = 0.0f, int flags = 0);
    void AddRectFilledMultiColor(
        UIVec2 pMin,
        UIVec2 pMax,
        uint32_t colUprLeft,
        uint32_t colUprRight,
        uint32_t colBotRight,
        uint32_t colBotLeft);

    void AddTriangle(UIVec2 p1, UIVec2 p2, UIVec2 p3, uint32_t col, float thickness = 1.0f);
    void AddTriangleFilled(UIVec2 p1, UIVec2 p2, UIVec2 p3, uint32_t col);

    void AddCircle(UIVec2 center, float radius, uint32_t col, int numSegments = 0, float thickness = 1.0f);
    void AddCircleFilled(UIVec2 center, float radius, uint32_t col, int numSegments = 0);

    void AddConvexPolyFilled(const UIVec2* points, int pointsCount, uint32_t col);
    void AddPolyline(const UIVec2* points, int pointsCount, uint32_t col, int flags, float thickness);

    void PathClear() { path_.clear(); }
    void PathLineTo(UIVec2 pos) { path_.push_back(pos); }
    void PathArcTo(UIVec2 center, float radius, float aMin, float aMax, int numSegments = kUIDefaultArcSegments);
    void PathStroke(uint32_t col, int flags = 0, float thickness = 1.0f);
    void PathFillConvex(uint32_t col);

    void AddImage(
        UITextureID userTextureId,
        UIVec2 pMin,
        UIVec2 pMax,
        UIVec2 uvMin = UIVec2(0.0f, 0.0f),
        UIVec2 uvMax = UIVec2(1.0f, 1.0f),
        uint32_t col = UI_COL32_WHITE);

    void AddImageRounded(
        UITextureID userTextureId,
        UIVec2 pMin,
        UIVec2 pMax,
        UIVec2 uvMin,
        UIVec2 uvMax,
        uint32_t col = UI_COL32_WHITE,
        float rounding = 0.0f,
        int flags = 0);

    void AddText(UIVec2 pos, uint32_t col, const char* textBegin, const char* textEnd = nullptr);
    void AddText(
        const TacticalFont* font,
        float fontSize,
        UIVec2 pos,
        uint32_t col,
        const char* textBegin,
        const char* textEnd = nullptr);

    const std::vector<UIVertex>& GetVertices() const { return vertices_; }
    const std::vector<uint32_t>& GetIndices() const { return indices_; }
    const std::vector<UIDrawCmd>& GetCommands() const { return cmds_; }
    bool IsEmpty() const { return indices_.empty() || cmds_.empty(); }

private:
    void RecordDrawElements(uint32_t indexCount, UITextureID texId);
    void AppendQuadGeometry(
        UIVec2 pMin,
        UIVec2 pMax,
        UIVec2 uvMin,
        UIVec2 uvMax,
        const float c0[kUIColorComponents],
        const float c1[kUIColorComponents],
        const float c2[kUIColorComponents],
        const float c3[kUIColorComponents]);
    void AppendUniformQuad(UIVec2 pMin, UIVec2 pMax, UIVec2 uvMin, UIVec2 uvMax, const float col[kUIColorComponents]);
    void AppendRoundedRectPath(UIVec2 pMin, UIVec2 pMax, float r);

    TacticalUICanvas* owner_ = nullptr;
    std::vector<UIVertex> vertices_;
    std::vector<uint32_t> indices_;
    std::vector<UIDrawCmd> cmds_;
    std::vector<UIClipRect> clipStack_;
    std::vector<UIVec2> path_;
    std::vector<UIVec2> scratchPts_;
    std::vector<UIVec2> scratchNormals_;
};

class TacticalUICanvas
{
public:
    static TacticalUICanvas* GetInstance();

    bool Initialize(DirectXCom* dx, WindowAPI* windowAPI);
    void Finalize();

    void BeginFrame();
    void RenderAll(ID3D12GraphicsCommandList* cmdList);

    TacticalDrawList* GetBackgroundDrawList() { return &backgroundList_; }
    TacticalDrawList* GetForegroundDrawList() { return &foregroundList_; }
    TacticalDrawList* GetPostTransitionDrawList() { return &postTransitionList_; }

    const TacticalFont* GetFont(UIFontRole role = UIFontRole::DefaultJP) const;
    UIVec2 CalcTextSize(
        const char* textBegin,
        const char* textEnd = nullptr,
        float fontSize = kUIDefaultFontSize,
        UIFontRole role = UIFontRole::DefaultJP);

    UIVec2 GetDisplaySize() const { return displaySize_; }
    double GetTime() const;

    UIVec2 GetMousePos() const { return mousePos_; }
    bool IsMouseClicked(int button = 0) const;
    float ConsumeMouseWheel();
    bool IsWindowFocused() const;
    bool IsKeyDown(int vk) const;

    UITextureID GetAtlasTextureID() const { return atlasGpuHandle_; }
    UIVec2 GetWhitePixelUV() const { return whiteUv_; }

    // Internal glyph layout helper used by TacticalDrawList / TacticalFont
    struct GlyphQuad
    {
        bool hasQuad = false;
        float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        float advanceX = 0.0f;
    };

    const GlyphQuad* AcquireGlyph(UIFontRole role, int pixelHeight, uint32_t codepoint);
    float GetBucketLineHeight(UIFontRole role, int pixelHeight);

    void AddWheelDeltaFromSubclass(float wheelSteps) { wheelAccum_ += wheelSteps; }

private:
    static constexpr int      kAtlasWidth           = 2048;
    static constexpr int      kAtlasHeight          = 2048;
    static constexpr uint32_t kFrameSlots           = 3;
    static constexpr uint32_t kMaxVerticesPerFrame  = 131072;
    static constexpr uint32_t kMaxIndicesPerFrame   = 393216;
    static constexpr int      kDefaultBucketPx      = 16;
    static constexpr int      kDefaultAscentPx      = 14;
    static constexpr int      kDefaultDescentPx     = 2;
    static constexpr float    kDefaultBaseOffsetY   = 14.0f;
    static constexpr size_t   kBucketRoleShift      = 16;
    static constexpr float    kWhiteUvCenterPx      = 1.5f;
    static constexpr int      kInitialPenX          = 2;
    static constexpr int      kInitialPenY          = 8;
    static constexpr size_t   kFontRoleCount        = 3;
    static constexpr size_t   kMouseButtonCount     = 3;
    static constexpr float    kInitialMouseOffscreen = -1000.0f;

    TacticalUICanvas() = default;
    ~TacticalUICanvas() = default;
    TacticalUICanvas(const TacticalUICanvas&) = delete;
    TacticalUICanvas& operator=(const TacticalUICanvas&) = delete;

    bool CreatePipeline();
    bool CreateFontAtlas();
    bool CreateDynamicBuffers();
    void FlushAtlasUpload(ID3D12GraphicsCommandList* cmdList);

    struct FontBucketKey
    {
        uint8_t role = 0;
        int pixelHeight = kDefaultBucketPx;

        bool operator==(const FontBucketKey& o) const
        {
            return role == o.role && pixelHeight == o.pixelHeight;
        }
    };

    struct FontBucketKeyHash
    {
        size_t operator()(const FontBucketKey& k) const noexcept
        {
            return (static_cast<size_t>(k.role) << kBucketRoleShift) ^ static_cast<size_t>(k.pixelHeight);
        }
    };

    struct FontBucket
    {
        HFONT hFont = nullptr;
        int pixelHeight = kDefaultBucketPx;
        int ascent = kDefaultAscentPx;
        int descent = kDefaultDescentPx;
        float baseOffsetY = kDefaultBaseOffsetY;
        std::unordered_map<uint32_t, GlyphQuad> glyphs;
    };

    FontBucket& GetOrCreateFontBucket(UIFontRole role, int pixelHeight);

private:
    DirectXCom* dx_ = nullptr;
    WindowAPI* windowAPI_ = nullptr;
    HWND subclassedHwnd_ = nullptr;
    bool initialized_ = false;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> atlasGpuTexture_;
    Microsoft::WRL::ComPtr<ID3D12Resource> atlasUploadBuffer_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT atlasFootprint_{};
    uint8_t* atlasUploadMapped_ = nullptr;
    uint32_t atlasSrvIndex_ = UINT32_MAX;
    UITextureID atlasGpuHandle_ = 0;
    UIVec2 whiteUv_{ kWhiteUvCenterPx / static_cast<float>(kAtlasWidth), kWhiteUvCenterPx / static_cast<float>(kAtlasHeight) };

    int penX_ = kInitialPenX;
    int penY_ = kInitialPenY;
    int rowMaxH_ = 0;
    int atlasDirtyYMin_ = 0;
    int atlasDirtyYMax_ = 0;

    HDC memDC_ = nullptr;
    std::wstring fontFaceName_ = L"BIZ UDPGothic";
    std::unordered_map<FontBucketKey, FontBucket, FontBucketKeyHash> fontBuckets_;
    std::vector<uint8_t> glyphScratch_;
    TacticalFont fonts_[kFontRoleCount]{};

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffers_[kFrameSlots];
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffers_[kFrameSlots];
    UIVertex* vbMapped_[kFrameSlots] = {};
    uint32_t* ibMapped_[kFrameSlots] = {};
    uint32_t frameSlot_ = 0;

    TacticalDrawList backgroundList_;
    TacticalDrawList foregroundList_;
    TacticalDrawList postTransitionList_;

    UIVec2 displaySize_{ kUIDefaultDisplayWidth, kUIDefaultDisplayHeight };
    UIVec2 mousePos_{ kInitialMouseOffscreen, kInitialMouseOffscreen };
    bool mouseDown_[kMouseButtonCount] = {};
    bool mouseClicked_[kMouseButtonCount] = {};
    float wheelAccum_ = 0.0f;
    float frameWheel_ = 0.0f;
    std::chrono::steady_clock::time_point startTime_{};
};

inline bool IsAppWindowFocused()
{
    const TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
    return ui ? ui->IsWindowFocused() : true;
}

inline bool IsAppKeyDown(int vk)
{
    const TacticalUICanvas* ui = TacticalUICanvas::GetInstance();
    return ui ? ui->IsKeyDown(vk) : ((GetAsyncKeyState(vk) & kUIAsyncKeyDownMask) != 0);
}
