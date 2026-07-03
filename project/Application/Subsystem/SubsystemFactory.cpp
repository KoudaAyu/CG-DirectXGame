#include "SubsystemFactory.h"

#include "DirectXCom.h"
#include "Baziru3_Engine/Base/Pipeline/PipelineStateManager.h"
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

        PipelineStateManager::GetInstance()->Initialize(res.directXCom.get());

        res.spriteCom = std::make_unique<SpriteCom>(logStream, res.directXCom.get());
        res.spriteCom->Initialize();
        Logger::Log(logStream, "SpriteCom initialized\n");

        SceneManager::GetInstance()->SetSpriteCom(res.spriteCom.get());

        res.spriteManager = std::make_unique<SpriteManager>();
       res.spriteManager->Initialize(res.spriteCom.get(), "Resources/uvChecker.png", 0);
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
    SceneManager::GetInstance()->SetSpriteCom(nullptr);
    SceneManager::GetInstance()->SetMaterialManager(nullptr);
    SceneManager::GetInstance()->SetParticleManager(nullptr);
    SceneManager::GetInstance()->SetAudioManager(nullptr);


    // 逆順で破棄する（unique_ptr の破棄順はここで明確にする）
    if (r.spriteManager)
    {
        try { r.spriteManager->Finalize(); }
        catch (...) {}
        r.spriteManager.reset();
        Logger::Log(logStream, "SpriteManager finalized\n");
    }
    if (r.spriteCom)
    {
        try { r.spriteCom->Finalize(); }
        catch (...) {}
        r.spriteCom.reset();
        Logger::Log(logStream, "SpriteCom finalized\n");
    }
    PipelineStateManager::GetInstance()->Finalize();
    try { TextureManager::GetInstance()->Finalize(); Logger::Log(logStream, "TextureManager finalized\n"); }
    catch (...) {}
    if (r.directXCom)
    {
        //  DirectX 側で必要なクリーンナップは DirectXCom のデストラクタで行う想定
        try { r.directXCom->Finalize(); } catch (...) {}
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
