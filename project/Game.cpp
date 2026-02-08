#include"Game.h"
#include <combaseapi.h>


void Game::Initialize()
{
	crashDump.Install();
	log.Initialize();
	windowAPI = new WindowAPI();
	windowAPI->Initialize();
	
	directXCom = new DirectXCom(windowAPI, logStream);
	directXCom->DebugLayer();
	//ウィンドウを表示する
	windowAPI->Show();
	directXCom->Initialize();
}
void Game::Finalize()
{
}
void Game::Update()
{
}
void Game::Draw()
{
}