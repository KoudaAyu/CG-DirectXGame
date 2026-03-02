#include"GamePlayScene.h"

void GamePlayScene::Initialize()
{
	//スプライト共通テクスチャ読み込み

	//スプライトの生成

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
}

void GamePlayScene::Update()
{
}

void GamePlayScene::Draw()
{
}