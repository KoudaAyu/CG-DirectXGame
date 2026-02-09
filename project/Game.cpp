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


	for (uint32_t i = 0; i < 5; ++i)
	{
		Sprite* sprite = new Sprite();
		// Resources フォルダ直下の uvChecker.png を指定
		sprite->Initialize(spriteCom, "Resources/uvChecker.png");
		sprites.push_back(sprite);
	}

	spriteCom->CreateGraphicsPipeline();

	// 既存の手動テクスチャ読み込みはそのまま利用（Sphere用）

	object3dCom = new Object3dCom(logStream);
	object3dCom->Initialize(GetDirectXCom());

#pragma region 最初のシーンの初期化
	object3d = new Object3d();
	object3d->Initialize(object3dCom);

	particleManager = new ParticleManager(logStream, GetDirectXCom());
	particleManager->Initialize();
#pragma endregion 最初のシーン終了

	//パイプラインステートの生成に失敗した場合はエラー
	assert(SUCCEEDED(GetDirectXCom()->GetHr()));

	sphere = new Sphere();
	sphere->Initialize(directXCom);

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