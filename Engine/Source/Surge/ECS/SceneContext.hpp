// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

namespace Surge
{
    class Window;
    class Clock;
    class Renderer;
    class Physics;
    class AudioEngine;
    class ScriptEngine;
    class AssetManager;

    // The engine systems a Scene drives. Every Scene gets one when it is created (Core's GetSceneContext() above the engine,
    // the AssetContext when a scene is loaded), so the ECS never reaches into Core
    struct SceneContext
    {
        Window* Window_ = nullptr;
        Clock* Clock_ = nullptr; // Frame delta + game time scale
        Renderer* Renderer_ = nullptr;
        Physics* Physics_ = nullptr;
        AudioEngine* AudioEngine_ = nullptr;
        ScriptEngine* ScriptEngine_ = nullptr;
        AssetManager* AssetManager_ = nullptr;
    };

} // namespace Surge
