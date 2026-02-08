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

	// TextureManager は SRV ヒープに依存するので DX 初期化の後に初期化
	TextureManager::GetInstance()->Initialize();
	TextureManager::GetInstance()->SetDirectXCom(directXCom);
	// 作業ディレクトリが project/ である前提の相対パスを使う
	TextureManager::GetInstance()->LoadTexture("Resources/uvChecker.png");


	spriteCom = new SpriteCom(logStream, directXCom);
	spriteCom->Initialize();

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