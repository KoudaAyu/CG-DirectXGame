#include "Framework.h"

void Framework::Run()
{
    // 初期化
    Initialize();

    // メインループ
    while (true)
    {
        // 終了メッセージがあればループを抜ける
        if (ProcessMessage())
        {
            break;
        }

        Update();
        Draw();
    }

    // 終了処理
    Finalize();
}
