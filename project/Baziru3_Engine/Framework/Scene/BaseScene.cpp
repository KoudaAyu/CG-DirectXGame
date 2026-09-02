#include "BaseScene.h"
#include "SceneManager.h"

Object3dCom* BaseScene::GetObject3dCom() const
{
	return sceneManager_ ? sceneManager_->GetObject3dCom() : nullptr;
}

SkinningObject3dCom* BaseScene::GetSkinningObject3dCom() const
{
	return sceneManager_ ? sceneManager_->GetSkinningObject3dCom() : nullptr;
}

MaterialManager* BaseScene::GetMaterialManager() const
{
	return sceneManager_ ? sceneManager_->GetMaterialManager() : nullptr;
}

Light* BaseScene::GetLight() const
{
	return sceneManager_ ? sceneManager_->GetLight() : nullptr;
}

ParticleManager* BaseScene::GetParticleManager() const
{
	return sceneManager_ ? sceneManager_->GetParticleManager() : nullptr;
}

AudioManager* BaseScene::GetAudioManager() const
{
	return sceneManager_ ? sceneManager_->GetAudioManager() : nullptr;
}

SpriteCom* BaseScene::GetSpriteCom() const
{
	return sceneManager_ ? sceneManager_->GetSpriteCom() : nullptr;
}

void BaseScene::ChangeScene(const std::string &sceneName)
{
	if (sceneManager_) {
		sceneManager_->ChangeScene(sceneName);
	}
}

void BaseScene::RestartScene()
{
	if (sceneManager_) {
		sceneManager_->RestartCurrentScene();
	}
}

void BaseScene::SetSceneData(const std::string &key, const std::string &value)
{
	if (sceneManager_) {
		sceneManager_->SetSceneData(key, value);
	}
}

void BaseScene::SetSceneDataInt(const std::string &key, int value)
{
	if (sceneManager_) {
		sceneManager_->SetSceneDataInt(key, value);
	}
}

void BaseScene::SetSceneDataFloat(const std::string &key, float value)
{
	if (sceneManager_) {
		sceneManager_->SetSceneDataFloat(key, value);
	}
}

std::string BaseScene::GetSceneData(const std::string &key, const std::string &defaultVal) const
{
	return sceneManager_ ? sceneManager_->GetSceneData(key, defaultVal) : defaultVal;
}

int BaseScene::GetSceneDataInt(const std::string &key, int defaultVal) const
{
	return sceneManager_ ? sceneManager_->GetSceneDataInt(key, defaultVal) : defaultVal;
}

float BaseScene::GetSceneDataFloat(const std::string &key, float defaultVal) const
{
	return sceneManager_ ? sceneManager_->GetSceneDataFloat(key, defaultVal) : defaultVal;
}
