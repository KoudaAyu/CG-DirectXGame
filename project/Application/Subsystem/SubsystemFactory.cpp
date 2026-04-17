#include "SubsystemFactory.h"

#include "DirectXCom.h"
#include "Log.h"
#include "SceneManager.h"
#include "SpriteCom.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "WindowsAPI.h"


SubsystemResult SubsystemFactory::InitializeAll(std::ostream& logStream, const InitConfig& cfg)
{
    SubsystemResult res;

    try
    {
        // Window
        res.windowAPI = std::make_unique<WindowAPI>();
        res.windowAPI->Initialize();
        res.windowAPI->Show();
        Logger::Log(logStream, "WindowAPI initialized\n");

        // DirectX (依存のため windowAPI.get() を渡す)
        res.directXCom = std::make_unique<DirectXCom>(res.windowAPI.get(), logStream);
        res.directXCom->DebugLayer();
        res.directXCom->Initialize();
        Logger::Log(logStream, "DirectXCom initialized\n");

        TextureManager::GetInstance()->Initialize();
        TextureManager::GetInstance()->SetDirectXCom(res.directXCom.get());
        Logger::Log(logStream, "TextureManager initialized and bound to DirectXCom\n");

        res.spriteCom = std::make_unique<SpriteCom>(logStream, res.directXCom.get());
        res.spriteCom->Initialize();
        res.spriteCom->CreateGraphicsPipeline();
        Logger::Log(logStream, "SpriteCom initialized and pipeline created\n");

        SceneManager::GetInstance()->SetSpriteCom(res.spriteCom.get());

        res.spriteManager = std::make_unique<SpriteManager>();
        res.spriteManager->Initialize(res.spriteCom.get(), "Resources/uvChecker.png", 5);
        Logger::Log(logStream, "SpriteManager initialized\n");

        res.success = true;
    }
    catch (const std::exception& e)
    {
        res.success = false;
        res.errorMessage = e.what();
        Logger::Log(logStream, std::string("SubsystemFactory init exception: ") + e.what() + "\n");
    }
    return res;
}

void SubsystemFactory::FinalizeAll(SubsystemResult& r, std::ostream& logStream)
{
    // 逆順で破棄する（unique_ptr の破棄順はここで明確にする）
    if (r.directXCom)
    {
        // DirectX 側で必要なクリーンナップは DirectXCom のデストラクタで行う想定
        r.directXCom.reset();
        Logger::Log(logStream, "DirectXCom finalized\n");
    }
    if (r.windowAPI)
    {
        r.windowAPI->Finalize();
        r.windowAPI.reset();
        Logger::Log(logStream, "WindowAPI finalized\n");
    }
    r.success = false;
}
