#include "Framework.h"
#include "Game.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) //Windowsアプリでのエントリーポイント(main関数)
{	
	Framework* game = new Game();
	game->Run();
	delete game;
	return 0;
}


