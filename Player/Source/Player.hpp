// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Client.hpp"
#include "Surge/Graphics/Camera/EditorCamera.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"

namespace Surge
{
    class Player : public Surge::Client
    {
    public:
        Player() = default;
        virtual ~Player() = default;

        virtual void OnInitialize() override;
        virtual void OnUpdate() override;
        virtual void OnImGuiRender() override;
        virtual void OnEvent(Event& e) override;
        virtual void OnShutdown() override;
    private:
        void Resize(Uint width, Uint height);

    private:
        Entity mRotatingCube;
        Entity mFloor;
        Renderer* mRenderer;
    };
} // namespace Surge