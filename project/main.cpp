#include"Game.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)//Windowsアプリでのエントリーポイント(main関数)
{	Game game;
	game.Initialize();
	//ウィンドウのxボタンが押されるまでループ
	while (game.GetDirectXCom()->GetMsg().message != WM_QUIT)
	{	////Windowに目セージが来ていたら最優先で処理される
		if (PeekMessage(&game.GetDirectXCom()->GetMsg(), NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&game.GetDirectXCom()->GetMsg()); //メッセージを変換
			DispatchMessage(&game.GetDirectXCom()->GetMsg()); //メッセージをウィンドウプロシージャに送る
		}else
		{
			game.Update();
			game.Draw();
		}}
	game.Finalize();
	return 0;}
