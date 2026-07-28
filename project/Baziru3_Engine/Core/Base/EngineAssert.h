#pragma once

#ifdef _DEBUG
#define ASSERT_ENABLED
#endif

#ifdef ASSERT_ENABLED

// アサート失敗時に呼び出される内部ヘルパー関数
// trueを返した場合、__debugbreak()を呼び出す
bool ReportAssertionFailure(const char* expression, const char* message, const char* file, int line);

// デバッグブレークを引き起こすマクロ
#if defined(_MSC_VER)
#define DEBUG_BREAK() __debugbreak()
#else
#define DEBUG_BREAK() __builtin_trap()
#endif

// アサーションマクロ本体
#define ENGINE_ASSERT(expr, message) \
    do { \
        if (!(expr)) { \
            if (ReportAssertionFailure(#expr, message, __FILE__, __LINE__)) { \
                DEBUG_BREAK(); \
            } \
        } \
    } while (false)

#else

// リリースビルド時はコンパイル後に完全に消去される
#define ENGINE_ASSERT(expr, message) ((void)0)

#endif
