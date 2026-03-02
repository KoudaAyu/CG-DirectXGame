#include"GamePlayScene.h"

void GamePlayScene::Initialize(DirectXCom* dxCommon)
{

	directXCom = dxCommon;

	//スプライト共通テクスチャ読み込み

	//スプライトの生成

	//球体の生成
	sphere = new Sphere();
	sphere->Initialize(directXCom);

	//OBJからモデルデータを読み込む

	//3Dオブジェクトの生成

	//音声読み込み

	sound = Sound::GetInstance();
	sound->SoundLoadFile("Resources/Alarm01.wav");

	//必要なら音声生成
	sound->SoundPlayWave();
}

void GamePlayScene::Finalize()
{
	delete sphere;
}

void GamePlayScene::Update()
{
}

void GamePlayScene::Draw()
{
}