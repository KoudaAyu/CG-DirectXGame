#include "Framework.h"
#include "Game.h"
#include <memory>

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nCmdShow*/) //Windowsアプリでのエントリーポイント(main関数)
{    
    std::unique_ptr<Framework> game = std::make_unique<Game>();
    game->Run();
    return 0;
}


