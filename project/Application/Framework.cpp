#include "Framework.h"
#include "Game.h"

Framework::~Framework() = default;

void Framework::Initialize()
{
}

void Framework::Finalize()
{
}

void Framework::Update()
{
}

void Framework::Run()
{
    // Game インスタンスをここで生成し、メインループを管理する
    game = std::make_unique<Game>();
    game->Initialize();

    MSG& msg = game->GetDirectXCom()->GetMsg();

    // ウィンドウの x ボタンが押されて WM_QUIT が飛んでくるまでループ
    while (msg.message != WM_QUIT)
    {
        // Window にメッセージが来ていたら最優先で処理
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            game->Update();
            game->Draw();
        }
    }

    game->Finalize();
    game.reset();
}
