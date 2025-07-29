#pragma once
#include <d3d12.h>
#include <dxgidebug.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <windows.h>
#include<wrl.h>

class Debug
{
public:
	void Initialize();

	static void Log(std::ostream& os, const std::string& message);
	
	void Info(const std::string& message);

	void EnableDebugLayer();
	void SetupInfoQueue(Microsoft::WRL::ComPtr<ID3D12Device>& device);

	std::ofstream& GetStream() { return logStream; }

private:
	std::ofstream logStream;
};
