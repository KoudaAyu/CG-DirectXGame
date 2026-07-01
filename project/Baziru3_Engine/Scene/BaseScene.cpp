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
