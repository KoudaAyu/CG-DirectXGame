#include "Log.h"
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <format>
#include <fstream>

void Logger::Log(std::ostream& os, const std::string& message)
{
	os << message << std::endl;
	OutputDebugStringA(message.c_str()); //出力ウィンドウに文字を出力
}

void Log::Initialize()
{
	//ログファイル関係
	//ログのディレクトリを用意
	std::filesystem::create_directories("logs");

	//現在時刻を取得(UTC時刻)
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

	//ログファイルの名前にコンマ何秒はいらないため、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSecound = std::chrono::time_point_cast<std::chrono::seconds>(now);

	//日本時間(PCの設定時間に変換)
	std::chrono::zoned_time localTime{ std::chrono::current_zone(), nowSecound };

	//formatを使って年月日_時分秒の形式にする
	std::string datString = std::format("%Y%m%d_%H%M%S", localTime);

	//時刻を使ってファイル名を決定
	std::string logFilePath = std::string("logs/") + datString + ".log";

	// メンバのファイルストリームを開く
	logStream.open(logFilePath, std::ios::out | std::ios::app);
	if (!logStream.is_open())
	{
		// 開けなかったら標準出力へエラーメッセージ
		std::cerr << "Failed to open log file: " << logFilePath << std::endl;
		OutputDebugStringA("Failed to open log file.\n");
	}
	else
	{
		Logger::Log(logStream, "Log initialized.");
	}
}

