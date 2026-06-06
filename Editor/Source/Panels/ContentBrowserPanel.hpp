// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Core/Path.hpp"
#include <imgui.h>

#define CONTENT_BROWSER_PAYLOAD "CONTENT_BROWSER_ASSET"

namespace Surge
{
    class ContentBrowserPanel : public IPanel
    {
    public:
        ContentBrowserPanel() = default;
        virtual ~ContentBrowserPanel() override = default;
        static PanelCode GetStaticCode() { return PanelCode::ContentBrowser; }

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent(Event& e) override;
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        void OnAssetManagerInit();
        void RefreshDirectoryCache();
    private:
        void RenderAssetRegistry(bool* show);
        void ClearContentBrowserSearchBuffer() { mContentBrowserSearchBuffer[0] = '\0'; }
    private:
        AssetManager* mAssetManager = nullptr;
        struct ContentBrowserItem
        {
            Path Path_;
            String Filename;
            bool IsDirectory;
            bool IsDirectoryEmpty;
            bool IsRegisteredAsset;
            AssetID Id = UUID::INVALID;
            String AssetTypeStr;
        };
        Vector<ContentBrowserItem> mCurrentDirectoryItems;
        float mCacheRefreshTimer = 0.0f;
        bool mNeedsCacheRefresh = true;
        bool mShowOnlyRegisteredAssets = false;

        PanelCode mCode;

        ImageHandle mDirectoryIconHandle;
        ImTextureID mDirectoryIconImGuiID;

        ImageHandle mEmptyDirectoryIconHandle;
        ImTextureID mEmptyDirectoryIconImGuiID;

        ImageHandle mFileIconHandle;
        ImTextureID mFileIconImGuiID;


        Path mBaseDirectory;
        Path mCurrentDirectory;
        Path mSelectedPath;

        // Pointers to items that pass the search filter/registered asset checkbox (Filtered from mCurrentDirectoryItems)
        // Then ImGuiClipping is done on this list to avoid overdraw
        Vector<ContentBrowserItem*> mItemsToDisplay;

        char mContentBrowserSearchBuffer[256] = {};

        float mThumbnailSize = 90.0f;

        // Asset Registry
        char mAssetRegistrySearchBuffer[256] = "";
        Vector<AssetID> mToUnregister;
        Vector<AssetID> mToUnload;
    };
} // namespace Surge
