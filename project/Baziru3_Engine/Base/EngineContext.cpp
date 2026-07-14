#include "EngineContext.h"

#include <iostream>
#include <sstream>

#include "DirectXCom.h"
#include"Log.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "WindowsAPI.h"

EngineContext::~EngineContext()
{
    Finalize();
}


bool EngineContext::Initialize(std::ostream& log, const InitConfig& cfg)
{
    logStream_ = &log;
    cfg_ = cfg;
    auto res = SubsystemFactory::InitializeAll(*logStream_, cfg_);
    if (!res.success)
    {
        Logger::Log(*logStream_, std::string("EngineContext: InitializeAll failed: ") + res.errorMessage + "\n");
       //　ここで部分的に初期化されているかを確認する
        SubsystemFactory::FinalizeAll(res, *logStream_);
        return false;
    }
    // 所有権を受け取る
    res_ = std::move(res);

  
    std::ostringstream oss;
    oss << "EngineContext: Initialized subsystems\n";
    oss << "  directXCom=0x" << std::hex << (uintptr_t)(res_.directXCom.get()) << std::dec << "\n";
    oss << "  windowAPI=0x" << std::hex << (uintptr_t)(res_.windowAPI.get()) << std::dec << "\n";
    oss << "  spriteCom=0x" << std::hex << (uintptr_t)(res_.spriteCom.get()) << std::dec << "\n";
    oss << "  spriteManager=0x" << std::hex << (uintptr_t)(res_.spriteManager.get()) << std::dec << "\n";
    Logger::Log(*logStream_, oss.str());

    return true;
}

void EngineContext::Finalize()
{
    if(finalized_)
    {
        return;
    }
    finalized_ = true;

    if (logStream_)
    {
        Logger::Log(*logStream_, "EngineContext: Finalizing subsystems\n");
    }

    // SubsystemFactory側で部分的に初期化済みのものを適切に破棄するための想定
    SubsystemFactory::FinalizeAll(res_, logStream_ ? *logStream_ : std::cout);

    // 明確敵に持ち物をクリアして二重開放を防ぐ
    res_ = {};
	logStream_ = nullptr;
}
