#pragma once

#include"CrashDump.h"
#include"DirectXCom.h"
#include"Log.h"
#include"ResourceLeakCheak.h"
#include"WindowsAPI.h"

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

private:
	ResourceLeakCheak leakChecker; //リソースリークチェック用のオブジェクト
	CrashDump crashDump; //クラッシュダンプ生成用のオブジェクト
	Log log;
	WindowAPI* windowAPI = nullptr; //ウィンドウ関連のAPIをまとめたオブジェクト
	DirectXCom* directXCom = nullptr;
private:

	
	
};
