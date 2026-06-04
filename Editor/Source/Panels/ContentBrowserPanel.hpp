// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include <filesystem>
#include <imgui.h>

#define CONTENT_BROWSER_PAYLOAD "CONTENT_BROWSER_ASSET"

namespace Surge
{
    class ContentBrowserPanel : public IPanel
    {
    public:
        ContentBrowserPanel() = default;
        virtual ~ContentBrowserPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        void OnAssetManagerInit();
    public:
        static PanelCode GetStaticCode() { return PanelCode::ContentBrowser; }

    private:
        PanelCode mCode;

        ImageHandle mDirectoryIconHandle;
        ImTextureID mDirectoryIconImGuiID;

        ImageHandle mFileIconHandle;
        ImTextureID mFileIconImGuiID;


        std::filesystem::path mBaseDirectory;
        std::filesystem::path mCurrentDirectory;
        std::filesystem::path mSelectedPath;

        float mThumbnailSize = 90.0f;
    };
} // namespace Surge