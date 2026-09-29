#pragma once

#include "AbstractSceneFactory.h"
#include <memory>

/// <summary>
/// ゲームエンジンの実行骨組み（ライフサイクル・メインループ）を提供する抽象基底クラス。
/// 
/// 【デザインパターン：Template Method パターン】
/// ・基底クラス（Framework）が全体の実行手順（Run: 初期化 → ループ[メッセージ処理・更新・描画] → 終了処理）を統括。
/// ・具体的なゲーム固有の初期化や描画内容は、派生クラス（Game等）がオーバーライドして実装。
/// 
/// 【カプセル化の利点】
/// ・エンジン層（Baziru3_Engine）に配置することで、アプリケーション固有の Game クラスに対する逆依存を完全解消。
/// ・将来別のゲームを開発する場合でも、この Framework を継承するだけでエンジン機能を利用可能。
/// </summary>
class Framework
{
public:
	virtual ~Framework() = default;

	// --- 派生クラス（具象ゲームクラス）でオーバーライドするライフサイクル ---
	virtual void Initialize() {}
	virtual void Finalize() {}
	virtual void Update() {}
	virtual void Draw() = 0;

	// --- カプセル化されたメインループ制御インターフェース ---
	virtual bool IsEndRequest() { return endRequest_; }
	virtual bool IsQuitRequested() { return false; }

	/// <summary>
	/// ウィンドウメッセージの処理（カプセル化インターフェース）
	/// </summary>
	/// <returns>アプリケーション終了要求（WM_QUIT等）を受信した場合は true</returns>
	virtual bool ProcessMessage() { return false; }

	/// <summary>
	/// メインループを実行する（初期化 → メッセージ処理/更新/描画のループ → 終了処理）
	/// </summary>
	void Run();

protected:
	bool endRequest_ = false;

	// シーンファクトリー
	AbstractSceneFactory* sceneFactory_ = nullptr;
};


