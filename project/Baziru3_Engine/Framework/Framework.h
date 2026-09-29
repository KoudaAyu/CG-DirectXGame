#pragma once

#include "AbstractSceneFactory.h"
#include <memory>

// ゲームループの基本骨組みを管理する基底クラス（Gameクラスの親クラス）
class Framework
{
public:
	virtual ~Framework() = default;

	// ゲーム側でオーバーライドするライフサイクル
	virtual void Initialize() {}
	virtual void Finalize() {}
	virtual void Update() {}
	virtual void Draw() = 0;

	virtual bool IsEndRequest() { return endRequest_; }
	virtual bool IsQuitRequested() { return false; }
	virtual bool ProcessMessage() { return false; }

	// メインループ実行（初期化 → ループ → 終了処理）
	void Run();

protected:
	bool endRequest_ = false;
	AbstractSceneFactory* sceneFactory_ = nullptr;
};
