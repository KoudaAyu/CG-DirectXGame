#pragma once
#include <atomic>

// =============================================================================
// デバッグ機能フラグ ＆ ImGui表示状態共有（アプリ側）
// -----------------------------------------------------------------------------
// 全ゲームUI（HUD・作戦マップ・ヘリ降下・救難演出・クリア・ゲームオーバー）は
// DirectX 12 ネイティブ描画基盤（TacticalUICanvas）で直接バックバッファへ描画する。
// Release（SHIPPING_BUILD）では F12 等を含め ImGui の開閉・描画を完全に無効化する。
// Debug / Development 構成のみ、起動時デフォルト非表示から F2 / F9 / F12 で開閉可能とする。
// =============================================================================
#if !defined(SHIPPING_BUILD)
#define APP_DEBUG_UI 1
#else
#define APP_DEBUG_UI 0
#endif

namespace AppDebugState
{
inline std::atomic<bool>& DeveloperImGuiVisibleStorage()
{
    static std::atomic<bool> s_visible{ false };
    return s_visible;
}

inline std::atomic<bool>& ImGuiCapturingMouseStorage()
{
    static std::atomic<bool> s_capturingMouse{ false };
    return s_capturingMouse;
}

inline std::atomic<bool>& ImGuiCapturingTextInputStorage()
{
    static std::atomic<bool> s_capturingText{ false };
    return s_capturingText;
}

inline bool IsDeveloperImGuiVisible()
{
#if APP_DEBUG_UI
    return DeveloperImGuiVisibleStorage().load(std::memory_order_relaxed);
#else
    return false;
#endif
}

inline void SetDeveloperImGuiVisible(bool visible)
{
#if APP_DEBUG_UI
    DeveloperImGuiVisibleStorage().store(visible, std::memory_order_relaxed);
#else
    (void)visible;
#endif
}

inline bool IsImGuiCapturingMouse()
{
#if APP_DEBUG_UI
    return ImGuiCapturingMouseStorage().load(std::memory_order_relaxed);
#else
    return false;
#endif
}

inline void SetImGuiCapturingMouse(bool capturing)
{
#if APP_DEBUG_UI
    ImGuiCapturingMouseStorage().store(capturing, std::memory_order_relaxed);
#else
    (void)capturing;
#endif
}

inline bool IsImGuiCapturingTextInput()
{
#if APP_DEBUG_UI
    return ImGuiCapturingTextInputStorage().load(std::memory_order_relaxed);
#else
    return false;
#endif
}

inline void SetImGuiCapturingTextInput(bool capturing)
{
#if APP_DEBUG_UI
    ImGuiCapturingTextInputStorage().store(capturing, std::memory_order_relaxed);
#else
    (void)capturing;
#endif
}
} // namespace AppDebugState
