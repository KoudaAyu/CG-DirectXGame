#pragma once

#include"CrashDump.h"
#include"DirectXCom.h"
#include"Log.h"
#include"Object3d.h"
#include"Object3dCom.h"
#include"ParticleManager.h"
#include"Sphere.h"
#include"Sprite.h"
#include"SpriteCom.h"
#include"ResourceLeakCheak.h"
#include"TextureManager.h"
#include"WindowsAPI.h"

#include <vector>

class Game
{
public:
	void Initialize();
	void Finalize();
	void Update();
	void Draw();

public:
	std::ostream& logStream = log.GetLogStream();

	WindowAPI* GetWindowAPI() { return windowAPI; }
	const WindowAPI* GetWindowAPI() const { return windowAPI; }
	DirectXCom* GetDirectXCom() { return directXCom; }
	const DirectXCom* GetDirectXCom() const { return directXCom; }
	SpriteCom* GetSpriteCom() { return spriteCom; }
	const SpriteCom* GetSpriteCom() const { return spriteCom; }
	Sprite* GetSprites(size_t index)
	{
		if (index < sprites.size())
		{
			return sprites[index];
		}
		return nullptr;
	}
	const Sprite* GetSprites(size_t index) const
	{
		if (index < sprites.size())
		{
			return sprites[index];
		}
		return nullptr;
	}
	std::vector<Sprite*>& GetSprites()
	{
		return sprites;
	}
	const std::vector<Sprite*>& GetSprites() const
	{
		return sprites;
	}
	Object3d* GetObject3d() { return object3d; }
	const Object3d* GetObject3d() const { return object3d; }
	Object3dCom* GetObject3dCom() { return object3dCom; }
	const Object3dCom* GetObject3dCom() const { return object3dCom; }
	ParticleManager* GetParticleManager() { return particleManager; }
	const ParticleManager* GetParticleManager() const { return particleManager; }
	Sphere* GetSphere() { return sphere; }
	const Sphere* GetSphere() const { return sphere; }

private:
	ResourceLeakCheak leakChecker; //リソースリークチェック用のオブジェクト
	CrashDump crashDump; //クラッシュダンプ生成用のオブジェクト
	Log log;
	WindowAPI* windowAPI = nullptr; //ウィンドウ関連のAPIをまとめたオブジェクト
	DirectXCom* directXCom = nullptr;
	SpriteCom* spriteCom = nullptr;
	Object3d* object3d = nullptr;
	Object3dCom* object3dCom = nullptr;
	ParticleManager* particleManager = nullptr;
	Sphere* sphere = nullptr;
private:
	std::vector<Sprite*>sprites;
	
	
};
