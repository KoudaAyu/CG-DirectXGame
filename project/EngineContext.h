#pragma once

#include "SubsystemFactory.h"
#include <ostream>
#include <sstream>

class EngineContext
{
public:

    ~EngineContext();
 
    bool Initialize(std::ostream& log, const InitConfig& cfg);
    void Finalize();
  
    DirectXCom* GetDirectXCom() const { return res_.directXCom.get(); }
    SpriteCom* GetSpriteCom() const { return res_.spriteCom.get(); }
    SpriteManager* GetSpriteManager() const { return res_.spriteManager.get(); }
    WindowAPI* GetWindowAPI() const { return res_.windowAPI.get(); }

private:
   
    SubsystemResult res_;
    std::ostream* logStream_ = nullptr;

    // 念のため
    InitConfig cfg_;

	bool finalized_ = false;
};

