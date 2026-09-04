#pragma once

#include"AbstractSceneFactory.h"
#include <memory>

class Game;

class Framework
{
public:

	virtual ~Framework();

	virtual void Initialize();

	virtual void Finalize();

	virtual void Update();

	virtual void Draw() = 0;

	virtual bool IsEndRequest() { return endRequest; }

	
	virtual bool IsQuitRequested() { return false; }

	void Run();

	void SetSceneFactory(std::unique_ptr<AbstractSceneFactory> factory) { sceneFactory_ = std::move(factory); }
	AbstractSceneFactory* GetSceneFactory() const { return sceneFactory_.get(); }

private:
	bool endRequest = false;

	std::unique_ptr<Game> game = nullptr;

	// シーンファクトリー
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;
};

