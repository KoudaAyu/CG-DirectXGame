#include "Framework.h"

void Framework::Run()
{
    // 【Template Method パターンによるメインループの統括】
    // 基底クラス（Framework）がゲームの実行フロー（初期化 → メッセージ処理/更新/描画 → 終了）を厳密に制御。
    // 各処理（Initialize, Update, Draw, Finalize）は仮想関数経由で
    // 派生クラス（Game等）の実装がポリモーフィズムにより実行されます。
    // これにより、Frameworkが具体的なGameクラスを直接知る必要（逆依存）が完全に無くなります。

    // 1. ゲーム初期化
    Initialize();

    // 2. メインループ
    while (true)
    {
        // ウィンドウメッセージ処理（WM_QUITなど終了要求があればループを抜ける）
        if (ProcessMessage())
        {
            break;
        }

        // ゲームロジックの更新と描画
        Update();
        Draw();
    }

    // 3. ゲーム終了処理
    Finalize();
}


