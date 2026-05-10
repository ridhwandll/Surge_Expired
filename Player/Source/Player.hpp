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
        virtual void OnEvent(Event& e) override;
        virtual void OnShutdown() override;
    private:
        void Resize(Uint width, Uint height);
        void OnImGuiRender();
        void FillTextures();
	private:
		SamplerHandle mQuadSampler;
        Vector<TextureHandle> mTextures;
        int mChangeQuadAmount = 10;
        bool mMoveEnabled = true;
        Vector<Entity> mQuads;
        Renderer* mRenderer;
    };
} // namespace Surge