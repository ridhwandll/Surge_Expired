// Copyright (c) - SurgeTechnologies - All rights reserved
// Windows entry point
#ifdef SURGE_PLATFORM_WINDOWS

#include "Surge/Core/Core.hpp"
#include "Surge/Core/Client.hpp"
#include "Player.hpp"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    Surge::ClientOptions clientOptions;
    clientOptions.EnableImGui = false;
    clientOptions.RenderFinalImageToSwapchian = true;
    clientOptions.WindowDescription = { INITIAL_WIDTH, INITIAL_HEIGHT, "Runtime", Surge::WindowFlags::DEFAULT };

    Surge::Player* app = Surge::MakeClient<Surge::Player>();
    app->SetOptions(clientOptions);

    Surge::Core::Initialize(app);
    Surge::Core::Run();
    Surge::Core::Shutdown();
}
#endif