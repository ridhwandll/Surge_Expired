// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ContentBrowserPanel.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"
#include "SurgeReflect/Enum.hpp"

#include "Editor.hpp"
#include "Asset/AssetFileOps.hpp"
#include "Asset/SourceWriters/MaterialSourceWriter.hpp"
#include "Asset/SourceWriters/ScriptSourceWriter.hpp"
#include "Panels/MaterialEditorPanel.hpp"
#include "Panels/UIEditorPanel.hpp"
#include "Panels/ParticleEditorPanel.hpp"
#include "Panels/EditorTitleBar.hpp"
#include "StatusBar/EditorStatusBar.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>

namespace Surge
{
    namespace fs = std::filesystem;

    static constexpr float TILE_PADDING = 6.0f;
    static constexpr const char* THUMBNAIL_TASK_KEY = "ContentBrowserThumbnails";
    static constexpr float TILE_SPACING = 8.0f;
    static constexpr float MIN_THUMBNAIL_SIZE = 48.0f;
    static constexpr float MAX_THUMBNAIL_SIZE = 256.0f;
    static constexpr size_t MAX_HISTORY = 64;
    static constexpr const char* IMPORT_FILTER = "Supported Assets\0*.png;*.jpg;*.jpeg;*.glb;*.gltf;*.smat;*.srg;*.lua;*.ttf;*.otf;*.wav;*.mp3;*.ogg;*.sui\0All Files (*.*)\0*.*\0";

    //////////////////////////////////////////////////////////////////////////
    // Helpers
    //////////////////////////////////////////////////////////////////////////

    static String ToLower(String text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return text;
    }

    static ImGuiID HashPath(const Path& path)
    {
        const String key = path.lexically_normal().generic_string();
        const ImGuiID id = ImHashStr(key.c_str(), key.size());
        return id ? id : 1; // 0 means "no item" for the selection storage
    }

    static ImVec4 GetTypeColor(AssetType type)
    {
        switch(type)
        {
            case AssetType::TEXTURE2D: return ImVec4(0.90f, 0.36f, 0.36f, 1.0f);
            case AssetType::SPRITE: return ImVec4(0.95f, 0.50f, 0.70f, 1.0f);
            case AssetType::MESH: return ImVec4(0.30f, 0.70f, 0.98f, 1.0f);
            case AssetType::MATERIAL: return ImVec4(0.42f, 0.82f, 0.38f, 1.0f);
            case AssetType::SCENE: return ImVec4(1.00f, 0.62f, 0.18f, 1.0f);
            case AssetType::SCRIPT: return ImVec4(0.96f, 0.84f, 0.30f, 1.0f);
            case AssetType::FONT: return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            case AssetType::AUDIO: return ImVec4(0.30f, 0.86f, 0.76f, 1.0f);
            case AssetType::UI_LAYOUT: return ImVec4(0.42f, 0.58f, 1.00f, 1.0f);
            case AssetType::PARTICLE_EFFECT: return ImVec4(0.35f, 0.86f, 0.96f, 1.0f);
            default: return ImVec4(0.45f, 0.45f, 0.45f, 1.0f);
        }
    }
    static constexpr ImVec4 FOLDER_COLOR = ImVec4(0.86f, 0.70f, 0.36f, 1.0f);

    static const char* GetTypeLabel(AssetType type)
    {
        switch(type)
        {
            case AssetType::TEXTURE2D: return "TEXTURE";
            case AssetType::SPRITE: return "SPRITE";
            case AssetType::MESH: return "MESH";
            case AssetType::MATERIAL: return "MATERIAL";
            case AssetType::SCENE: return "SCENE";
            case AssetType::SCRIPT: return "SCRIPT";
            case AssetType::FONT: return "FONT";
            case AssetType::AUDIO: return "AUDIO";
            case AssetType::UI_LAYOUT: return "UI LAYOUT";
            case AssetType::PARTICLE_EFFECT: return "PARTICLES";
            default: return "FILE";
        }
    }

    // Types with an Editor cooker, "Reimport" re-runs it
    static bool IsCookable(AssetType type)
    {
        return type == AssetType::TEXTURE2D || type == AssetType::MESH || type == AssetType::MATERIAL || type == AssetType::SCRIPT || type == AssetType::FONT || type == AssetType::AUDIO;
    }

    static String FormatSize(uintmax_t bytes)
    {
        if(bytes < 1024)
            return std::format("{} B", bytes);
        if(bytes < 1024 * 1024)
            return std::format("{:.1f} KB", static_cast<double>(bytes) / 1024.0);
        if(bytes < 1024ull * 1024 * 1024)
            return std::format("{:.1f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return std::format("{:.2f} GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }

    static String FormatTime(fs::file_time_type time)
    {
        const std::time_t seconds = std::chrono::system_clock::to_time_t(std::chrono::clock_cast<std::chrono::system_clock>(time));
        std::tm local = {};
        localtime_s(&local, &seconds);
        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);
        return buffer;
    }

    static ImU32 WithAlpha(const ImVec4& color, float alpha)
    {
        return ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, color.w * alpha));
    }

    // p is 'from' or below it: the same place below 'to'
    static bool Rebase(Path& path, const Path& from, const Path& to)
    {
        if(path.empty() || !AssetFileOps::IsInside(path, from))
            return false;
        const Path relative = path.lexically_normal().lexically_relative(from.lexically_normal());
        path = (relative == "." ? to : to / relative).lexically_normal();
        return true;
    }

    // Up to two centered lines, the last one ends with "..." when the text doesn't fit
    static void DrawWrappedLabel(ImDrawList* drawList, const String& text, ImVec2 pos, float width, ImU32 color)
    {
        constexpr int maxLines = 2;
        const float lineHeight = ImGui::GetTextLineHeight();
        const char* cursor = text.c_str();
        const char* end = cursor + text.size();

        for(int line = 0; line < maxLines && cursor < end; line++)
        {
            const bool lastLine = line == maxLines - 1;
            const char* lineEnd = end;
            if(ImGui::CalcTextSize(cursor, end).x > width)
            {
                // Longest prefix that fits, preferably broken after a separator
                const float available = lastLine ? width - ImGui::CalcTextSize("...").x : width;
                lineEnd = cursor;
                while(lineEnd < end)
                {
                    const char* next = lineEnd + 1;
                    while(next < end && (static_cast<unsigned char>(*next) & 0xC0) == 0x80)
                        next++; // Whole UTF-8 sequences
                    if(ImGui::CalcTextSize(cursor, next).x > available)
                        break;
                    lineEnd = next;
                }
                if(lineEnd == cursor)
                    lineEnd = cursor + 1;

                if(!lastLine)
                {
                    for(const char* breakAt = lineEnd; breakAt > cursor + 1; breakAt--)
                    {
                        const char c = *(breakAt - 1);
                        if(c == ' ' || c == '_' || c == '-' || c == '.')
                        {
                            lineEnd = breakAt;
                            break;
                        }
                    }
                }
            }

            String lineText(cursor, lineEnd);
            if(lastLine && lineEnd < end)
                lineText += "...";
            const float lineWidth = ImGui::CalcTextSize(lineText.c_str()).x;
            drawList->AddText(ImVec2(pos.x + std::max(0.0f, (width - lineWidth) * 0.5f), pos.y), color, lineText.c_str());

            cursor = lineEnd;
            while(cursor < end && *cursor == ' ')
                cursor++;
            pos.y += lineHeight;
        }
    }

    // Small rounded label drawn over thumbnails, horizontally centered on centerX. anchorBottom: y is the badge's bottom edge, otherwise its top.
    // The font shrinks when the badge would be wider than maxWidth (small thumbnails)
    static void DrawBadge(ImDrawList* drawList, float centerX, float y, bool anchorBottom, float maxWidth, const char* text, ImU32 background, ImU32 textColor)
    {
        constexpr float paddingX = 4.0f;
        constexpr float paddingY = 2.0f;
        ImFont* font = ImGui::GetIO().Fonts->Fonts[1];

        float fontSize = 15.0f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        if(textSize.x + paddingX * 2.0f > maxWidth && textSize.x > 0.0f)
        {
            fontSize = std::max(8.0f, fontSize * (maxWidth - paddingX * 2.0f) / textSize.x);
            textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        }

        const ImVec2 badgeSize(textSize.x + paddingX * 2.0f, textSize.y + paddingY * 2.0f);
        const ImVec2 min(std::floor(centerX - badgeSize.x * 0.5f), std::floor(anchorBottom ? y - badgeSize.y : y));
        drawList->AddRectFilled(min, ImVec2(min.x + badgeSize.x, min.y + badgeSize.y), background, 3.0f);
        drawList->AddText(font, fontSize, ImVec2(min.x + paddingX, min.y + paddingY), textColor, text);
    }

    // Full width bar ending at bottomY (the preview's bottom edge flows into it), label centered, the font shrinks to fit narrow tiles
    static void DrawTypeBar(ImDrawList* drawList, float minX, float maxX, float bottomY, const char* text, ImU32 background, ImU32 textColor)
    {
        constexpr float paddingX = 4.0f;
        constexpr float paddingY = 2.0f;
        ImFont* font = ImGui::GetIO().Fonts->Fonts[1];
        const float width = maxX - minX;

        float fontSize = 15.0f;
        ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        if(textSize.x + paddingX * 2.0f > width && textSize.x > 0.0f)
        {
            fontSize = std::max(8.0f, fontSize * (width - paddingX * 2.0f) / textSize.x);
            textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
        }

        const float top = std::floor(bottomY - textSize.y - paddingY * 2.0f);
        drawList->AddRectFilled(ImVec2(minX, top), ImVec2(maxX, bottomY), background, 4.0f, ImDrawFlags_RoundCornersBottom); // Same rounding as the preview
        drawList->AddText(font, fontSize, ImVec2(std::floor(minX + (width - textSize.x) * 0.5f), top + paddingY), textColor, text);
    }

    //////////////////////////////////////////////////////////////////////////
    // Panel
    //////////////////////////////////////////////////////////////////////////

    ContentBrowserSettings& ContentBrowserPanel::Settings() const
    {
        return static_cast<Editor*>(Core::GetClient())->GetSettings().ContentBrowser;
    }

    void ContentBrowserPanel::Init(void*)
    {
        mCode = GetStaticCode();
        mAssetManager = Core::GetAssetManager();

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        mFolderIcon = editor->LoadEditorIcon("Editor/Assets/Textures/Folder.png");
        mEmptyFolderIcon = editor->LoadEditorIcon("Editor/Assets/Textures/EmptyFolder.png");
        mFileIcon = editor->LoadEditorIcon("Editor/Assets/Textures/File.png");
        mFolderIconID = rhi->GetImGuiImage(mFolderIcon);
        mEmptyFolderIconID = rhi->GetImGuiImage(mEmptyFolderIcon);
        mFileIconID = rhi->GetImGuiImage(mFileIcon);
        mThumbnails.Initialize(editor->GetIconSampler());

        // Selection is stored by path hash, so it survives refreshes and re-sorting
        mSelection.UserData = this;
        mSelection.AdapterIndexToStorageId = [](ImGuiSelectionBasicStorage* storage, int index) -> ImGuiID {
            const ContentBrowserPanel* self = static_cast<const ContentBrowserPanel*>(storage->UserData);
            return self->mItems[self->mVisible[index]].SelectionID;
        };

        OnAssetManagerInit();
    }

    void ContentBrowserPanel::OnAssetManagerInit()
    {
        mBaseDirectory = AssetFileOps::GetAssetsRoot();
        mCurrentDirectory = mBaseDirectory;
        mBackHistory.clear();
        mForwardHistory.clear();
        mSelection.Clear();
        mClipboard.clear();
        mRenamingPath.clear();
        mSearchBuffer[0] = '\0';
        mRevealInTree = true;
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::OnProjectSettingsLoaded()
    {
        const String& lastDirectory = Settings().LastDirectory;
        if(lastDirectory.empty())
            return;

        const Path folder = (mBaseDirectory / lastDirectory).lexically_normal();
        if(fs::is_directory(folder) && AssetFileOps::IsInside(folder, mBaseDirectory))
        {
            mCurrentDirectory = folder;
            mRevealInTree = true;
            mNeedsRefresh = true;
        }
    }

    void ContentBrowserPanel::SetSelectedAsset(AssetID asset)
    {
        const AssetMetadata& meta = mAssetManager->GetMetadata(asset);
        if(!meta.ID || HasFlag(meta.Flags, AssetFlags::MEMORY))
            return;

        const Path path = (mBaseDirectory / meta.RelativePath).lexically_normal();
        Navigate(path.parent_path());
        mSearchBuffer[0] = '\0';
        SelectAfterRefresh({ path });
        ImGui::SetWindowFocus(PanelCodeToString(mCode));
    }

    void ContentBrowserPanel::ReleaseGPUResources()
    {
        mThumbnails.Shutdown();

        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        for(ImageHandle* icon : { &mFolderIcon, &mEmptyFolderIcon, &mFileIcon })
        {
            if(!icon->IsNull())
                rhi->DestroyImage(*icon);
            *icon = {};
        }
        mFolderIconID = mEmptyFolderIconID = mFileIconID = 0;
    }

    void ContentBrowserPanel::Shutdown()
    {
        ReleaseGPUResources();
    }

    void ContentBrowserPanel::OnEvent(Event& e)
    {
        // Handled next Render, where the panel's rect tells whether the files were dropped on it
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<FilesDroppedEvent>([this](FilesDroppedEvent& dropEvent) {
            mDroppedFiles = dropEvent.GetPaths();
            mDropPoint = ImVec2(dropEvent.GetX(), dropEvent.GetY());
        });
    }

    //////////////////////////////////////////////////////////////////////////
    // Data
    //////////////////////////////////////////////////////////////////////////

    void ContentBrowserPanel::Refresh()
    {
        mNeedsRefresh = false;
        if(mBaseDirectory.empty() || !fs::is_directory(mBaseDirectory))
        {
            mItems.clear();
            mVisible.clear();
            return;
        }

        // The folder may have been deleted or renamed outside of the Editor
        if(!fs::is_directory(mCurrentDirectory) || !AssetFileOps::IsInside(mCurrentDirectory, mBaseDirectory))
            mCurrentDirectory = mBaseDirectory;

        RebuildFolderTree();
        RebuildItems();
        SortAndFilter();
        ApplyPendingSelection();

        std::error_code error;
        mCurrentDirectoryWriteTime = fs::last_write_time(mCurrentDirectory, error);
        mRegistrySize = mAssetManager->GetRegistryMap().size();
        mChangeCheckTimer = 0.0f;
    }

    void ContentBrowserPanel::RebuildFolderTree()
    {
        mFolderTree = {};
        mFolderTree.AbsolutePath = mBaseDirectory;
        mFolderTree.Name = "Assets";
        ScanFolder(mFolderTree);
    }

    void ContentBrowserPanel::ScanFolder(FolderNode& node)
    {
        std::error_code error;
        for(const fs::directory_entry& entry : fs::directory_iterator(node.AbsolutePath, fs::directory_options::skip_permission_denied, error))
        {
            if(!entry.is_directory(error) || AssetFileOps::IsHidden(entry.path()))
                continue;

            FolderNode& child = node.Children.emplace_back();
            child.AbsolutePath = entry.path().lexically_normal();
            child.Name = entry.path().filename().string();
            ScanFolder(child);
        }
        std::sort(node.Children.begin(), node.Children.end(), [](const FolderNode& a, const FolderNode& b) { return ToLower(a.Name) < ToLower(b.Name); });
    }

    void ContentBrowserPanel::RebuildItems()
    {
        mItems.clear();
        mAppliedSearch = mSearchBuffer;

        // One lookup table instead of a registry scan per file
        std::unordered_map<String, AssetID> assetsByPath;
        assetsByPath.reserve(mAssetManager->GetRegistryMap().size());
        for(const auto& [id, meta] : mAssetManager->GetRegistryMap())
        {
            if(!HasFlag(meta.Flags, AssetFlags::MEMORY))
                assetsByPath.emplace(meta.RelativePath, id);
        }

        const String search = ToLower(mSearchBuffer);
        auto matches = [&search](const fs::directory_entry& entry) { return search.empty() || ToLower(entry.path().filename().string()).find(search) != String::npos; };

        std::error_code error;
        if(!search.empty() && Settings().SearchAllFolders)
        {
            for(auto it = fs::recursive_directory_iterator(mBaseDirectory, fs::directory_options::skip_permission_denied, error); !error && it != fs::recursive_directory_iterator(); it.increment(error))
            {
                if(AssetFileOps::IsHidden(it->path()))
                {
                    if(it->is_directory(error))
                        it.disable_recursion_pending();
                    continue;
                }
                if(matches(*it))
                    mItems.push_back(MakeItem(*it, assetsByPath));
            }
        }
        else
        {
            for(const fs::directory_entry& entry : fs::directory_iterator(mCurrentDirectory, fs::directory_options::skip_permission_denied, error))
            {
                if(!AssetFileOps::IsHidden(entry.path()) && matches(entry))
                    mItems.push_back(MakeItem(entry, assetsByPath));
            }
        }
        mNeedsSort = true;
    }

    ContentBrowserPanel::Item ContentBrowserPanel::MakeItem(const fs::directory_entry& entry, const std::unordered_map<String, AssetID>& assetsByPath) const
    {
        std::error_code error;
        Item item;
        item.AbsolutePath = entry.path().lexically_normal();
        item.Name = item.AbsolutePath.filename().string();
        item.SortKey = ToLower(item.Name);
        item.SelectionID = HashPath(item.AbsolutePath);
        item.IsDirectory = entry.is_directory(error);
        item.WriteTime = entry.last_write_time(error);
        if(!error)
            item.ModifiedText = FormatTime(item.WriteTime);

        const String parent = AssetFileOps::ToRelative(item.AbsolutePath.parent_path());
        item.Location = parent.empty() ? String("Assets") : "Assets/" + parent;

        if(item.IsDirectory)
        {
            item.IsEmptyDirectory = fs::is_empty(item.AbsolutePath, error);
            item.DisplayName = item.Name;
            return item;
        }

        item.Size = entry.file_size(error);
        item.SizeText = error ? String() : FormatSize(item.Size);
        item.IsImage = AssetFileOps::GetTypeFromExtension(item.AbsolutePath) == AssetType::TEXTURE2D;

        if(const auto it = assetsByPath.find(AssetFileOps::ToRelative(item.AbsolutePath)); it != assetsByPath.end())
        {
            const AssetMetadata& meta = mAssetManager->GetMetadata(it->second);
            item.ID = it->second;
            item.IsRegistered = true;
            item.Type = meta.Type;
            item.IsMissing = meta.IsMissing();
            item.DisplayName = item.AbsolutePath.stem().string();
        }
        else
        {
            item.Type = AssetFileOps::GetTypeFromExtension(item.AbsolutePath);
            item.DisplayName = item.Name;
        }
        return item;
    }

    bool ContentBrowserPanel::PassesFilter(const Item& item) const
    {
        if(item.IsDirectory)
            return true;

        const ContentBrowserSettings& settings = Settings();
        if(!item.IsRegistered && !settings.ShowUnimported)
            return false;
        return item.Type == AssetType::NONE || !(settings.HiddenTypes & (1u << static_cast<Uint>(item.Type)));
    }

    void ContentBrowserPanel::SortAndFilter()
    {
        mVisible.clear();
        mVisible.reserve(mItems.size());
        for(int i = 0; i < static_cast<int>(mItems.size()); i++)
        {
            if(PassesFilter(mItems[i]))
                mVisible.push_back(i);
        }

        const SortField field = static_cast<SortField>(Settings().SortField);
        const bool descending = Settings().SortDescending;
        std::stable_sort(mVisible.begin(), mVisible.end(), [&](int a, int b) {
            const Item& x = mItems[a];
            const Item& y = mItems[b];
            if(x.IsDirectory != y.IsDirectory)
                return x.IsDirectory; // Folders always first

            int order = 0;
            switch(field)
            {
                case SortField::TYPE: order = static_cast<int>(x.Type) - static_cast<int>(y.Type); break;
                case SortField::SIZE: order = x.Size < y.Size ? -1 : (x.Size > y.Size ? 1 : 0); break;
                case SortField::MODIFIED: order = x.WriteTime < y.WriteTime ? -1 : (x.WriteTime > y.WriteTime ? 1 : 0); break;
                default: break;
            }
            if(order == 0)
                order = x.SortKey.compare(y.SortKey);
            return descending ? order > 0 : order < 0;
        });
        mNeedsSort = false;
    }

    void ContentBrowserPanel::ApplyPendingSelection()
    {
        // Drop selected items that are gone (deleted, moved, filtered out)
        Vector<ImGuiID> kept;
        for(int index : mVisible)
        {
            if(mSelection.Contains(mItems[index].SelectionID))
                kept.push_back(mItems[index].SelectionID);
        }
        mSelection.Clear();
        for(ImGuiID id : kept)
            mSelection.SetItemSelected(id, true);

        if(mSelectAfterRefresh.empty())
            return;

        mSelection.Clear();
        for(const Path& path : mSelectAfterRefresh)
            mSelection.SetItemSelected(HashPath(path), true);
        mScrollToPath = mSelectAfterRefresh.front().lexically_normal();
        if(mRenameAfterRefresh && mSelectAfterRefresh.size() == 1)
            BeginRename(mSelectAfterRefresh.front());

        mSelectAfterRefresh.clear();
        mRenameAfterRefresh = false;
    }

    void ContentBrowserPanel::CheckForExternalChanges()
    {
        // A stat per second: catches files added in Explorer and assets registered by other panels
        mChangeCheckTimer += ImGui::GetIO().DeltaTime;
        if(mChangeCheckTimer < 1.0f)
            return;
        mChangeCheckTimer = 0.0f;

        std::error_code error;
        const fs::file_time_type writeTime = fs::last_write_time(mCurrentDirectory, error);
        if(error || writeTime != mCurrentDirectoryWriteTime || mAssetManager->GetRegistryMap().size() != mRegistrySize)
            mNeedsRefresh = true;
    }

    //////////////////////////////////////////////////////////////////////////
    // Navigation
    //////////////////////////////////////////////////////////////////////////

    void ContentBrowserPanel::Navigate(const Path& folder, bool recordHistory)
    {
        const Path target = folder.lexically_normal();
        if(target == mCurrentDirectory)
            return;

        if(recordHistory)
        {
            mBackHistory.push_back(mCurrentDirectory);
            if(mBackHistory.size() > MAX_HISTORY)
                mBackHistory.erase(mBackHistory.begin());
            mForwardHistory.clear();
        }

        mCurrentDirectory = target;
        Settings().LastDirectory = AssetFileOps::ToRelative(target);
        mSelection.Clear();
        mRenamingPath.clear();
        mSearchBuffer[0] = '\0';
        mRevealInTree = true;
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::NavigateBack()
    {
        if(mBackHistory.empty())
            return;

        const Path from = mCurrentDirectory;
        const Path target = mBackHistory.back();
        mBackHistory.pop_back();
        mForwardHistory.push_back(from);
        Navigate(target, false);
        SelectAfterRefresh({ from });
    }

    void ContentBrowserPanel::NavigateForward()
    {
        if(mForwardHistory.empty())
            return;

        const Path target = mForwardHistory.back();
        mForwardHistory.pop_back();
        mBackHistory.push_back(mCurrentDirectory);
        Navigate(target, false);
    }

    void ContentBrowserPanel::NavigateUp()
    {
        if(mCurrentDirectory == mBaseDirectory)
            return;

        const Path from = mCurrentDirectory;
        Navigate(mCurrentDirectory.parent_path());
        SelectAfterRefresh({ from }); // Keeps the user's place
    }

    //////////////////////////////////////////////////////////////////////////
    // UI
    //////////////////////////////////////////////////////////////////////////

    void ContentBrowserPanel::Render(bool* show)
    {
        constexpr float drawerSlideSeconds = 0.15f;
        const float slideStep = ImGui::GetIO().DeltaTime / drawerSlideSeconds;
        mDrawerAnimation = mDrawerOpen ? std::min(1.0f, mDrawerAnimation + slideStep) : std::max(0.0f, mDrawerAnimation - slideStep);
        const bool drawerVisible = mDrawerAnimation > 0.0f;

        if(!*show && !drawerVisible)
        {
            mDroppedFiles.clear();
            UpdateThumbnailTask(0); // Loading pauses while hidden
            return;
        }

        mThumbnails.Update();
        UpdateThumbnailTask(mThumbnails.GetPendingCount());
        if(mNeedsRefresh)
            Refresh();
        else if(mAppliedSearch != mSearchBuffer)
        {
            RebuildItems();
            SortAndFilter();
            ApplyPendingSelection();
        }
        else if(mNeedsSort)
            SortAndFilter();

        if(*show)
        {
            if(mFocusTab)
            {
                ImGui::SetNextWindowFocus();
                mFocusTab = false;
            }
            if(ImGui::Begin(PanelCodeToString(mCode), show, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                if(drawerVisible) // The drawer has the browser while it is out
                {
                    ImGui::SetCursorPosY(std::floor((ImGui::GetWindowHeight() - ImGui::GetFontSize()) * 0.5f));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    ImGuiAux::TextCentered("Open in the Content Drawer (Ctrl+Space)");
                    ImGui::PopStyleColor();
                }
                else
                    DrawBody();
            }
            else if(!drawerVisible)
                mDroppedFiles.clear();
            ImGui::End();
        }

        if(drawerVisible)
            RenderDrawer();
    }

    void ContentBrowserPanel::DrawBody()
    {
        HandleOsDrop();
        CheckForExternalChanges();
        DrawToolbar();

        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        const float statusHeight = ImGui::GetFrameHeightWithSpacing();
        const float bodyHeight = std::max(60.0f, ImGui::GetContentRegionAvail().y - statusHeight);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
        ImGui::BeginChild("##FolderTree", ImVec2(Settings().FolderTreeWidth, bodyHeight), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PopStyleVar();

        // Shadow tint
        {
            const ImVec2 treePos = ImGui::GetWindowPos();
            const ImVec2 treeSize = ImGui::GetWindowSize();
            const ImU32 shadowLeft = IM_COL32(0, 0, 0, 10);
            const ImU32 shadowRight = IM_COL32(0, 0, 0, 200);
            ImGui::GetWindowDrawList()->AddRectFilledMultiColor(treePos, ImVec2(treePos.x + treeSize.x, treePos.y + treeSize.y), shadowLeft, shadowRight, shadowRight, shadowLeft);
        }
        DrawFolderTree();
        ImGui::EndChild();

        ImGui::SameLine(0.0f, 0.0f);
        DrawSplitter(bodyHeight);
        ImGui::SameLine(0.0f, 0.0f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.12f));
        // The list scrolls inside its table, Ctrl+Wheel zooms the grid instead of scrolling it
        ImGuiWindowFlags contentFlags = ImGuiWindowFlags_None;
        if(Settings().ListView)
            contentFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        else if(ImGui::GetIO().KeyCtrl)
            contentFlags = ImGuiWindowFlags_NoScrollWithMouse;
        ImGui::BeginChild("##Content", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None, contentFlags);
        ImGui::PopStyleColor();
        DrawContent();
        ImGui::EndChild();

        DrawStatusBar();

        if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            HandleShortcuts();
        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows))
        {
            if(ImGui::IsMouseClicked(3)) // Mouse "back" button
                NavigateBack();
            if(ImGui::IsMouseClicked(4))
                NavigateForward();
        }
    }

    void ContentBrowserPanel::SetDrawerOpen(bool open)
    {
        if(open == mDrawerOpen)
            return;

        mDrawerOpen = open;
        mFocusDrawer = open;
        mDrawerWasFocused = false;
        mDrawerDragTracked = false;
        mDragFromDrawer = false;
    }

    void ContentBrowserPanel::RenderDrawer()
    {
        // Between the title bar and the status bar, both step aside for a fullscreen viewport
        const bool bars = static_cast<Editor*>(Core::GetClient())->IsTitlebarShown();
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const float top = viewport->WorkPos.y + (bars ? EditorTitleBar::HEIGHT : 0.0f);
        const float bottom = viewport->WorkPos.y + viewport->WorkSize.y - (bars ? EditorStatusBar::HEIGHT : 0.0f);
        const float areaHeight = std::max(bottom - top, 1.0f);
        const float height = std::floor(glm::clamp(Settings().DrawerHeight * areaHeight, std::min(200.0f, areaHeight), areaHeight));
        const float slide = 1.0f - std::pow(1.0f - mDrawerAnimation, 3.0f); // Ease out

        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, std::floor(bottom - height * slide)));
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, height));
        ImGui::SetNextWindowViewport(viewport->ID);
        if(mFocusDrawer)
        {
            ImGui::SetNextWindowFocus();
            mFocusDrawer = false;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse;
        const bool open = ImGui::Begin("Content Drawer###ContentDrawer", nullptr, flags);
        ImGui::PopStyleVar(2);
        if(open)
        {
            const ImVec2 min = ImGui::GetWindowPos();
            const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
            ImGui::GetWindowDrawList()->AddLine(min, ImVec2(max.x, min.y), ImGui::GetColorU32(ImVec4(0.26f, 0.26f, 0.26f, 1.0f)));

            DrawDrawerHeader(areaHeight);
            DrawBody();

            // Closes like Unreal's: the focus went elsewhere (not to a popup of its own or a confirmation box), Esc, or a drag that started in it left it
            const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            const bool hovered = ImGui::IsMouseHoveringRect(min, max, false);
            const bool dragging = ImGui::GetDragDropPayload() != nullptr;
            if(dragging && !mDrawerDragTracked)
                mDragFromDrawer = hovered;
            mDrawerDragTracked = dragging;

            if(mDrawerOpen)
            {
                const ImGuiIO& io = ImGui::GetIO();
                if(mDrawerWasFocused && !focused && !ImGui::GetTopMostPopupModal())
                    SetDrawerOpen(false);
                else if(focused && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !io.WantTextInput && mRenamingPath.empty() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
                    SetDrawerOpen(false);
                else if(dragging && mDragFromDrawer && !hovered)
                    SetDrawerOpen(false); // The payload outlives its source, it can still be dropped
                else
                    mDrawerWasFocused = focused;
            }
        }
        ImGui::End();

        // While sliding the drawer passes under the status bar, not over it
        if(bars && mDrawerAnimation < 1.0f)
        {
            if(ImGuiWindow* statusBar = ImGui::FindWindowByName(EditorStatusBar::WINDOW_NAME))
                ImGui::BringWindowToDisplayFront(statusBar);
        }
    }

    void ContentBrowserPanel::DrawDrawerHeader(float areaHeight)
    {
        constexpr float headerHeight = 24.0f;
        constexpr const char* dockLabel = "Dock in Layout";
        const ImGuiStyle& style = ImGui::GetStyle();
        const ImVec2 start = ImGui::GetCursorPos();
        const float width = ImGui::GetContentRegionAvail().x;
        const float dockWidth = ImGui::CalcTextSize(dockLabel).x + style.FramePadding.x * 2.0f;

        // The header is the drawer's top edge: dragging it resizes the drawer
        ImGui::InvisibleButton("##DrawerResize", ImVec2(std::max(width - dockWidth - style.ItemSpacing.x, 1.0f), headerHeight));
        if(ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if(ImGui::IsItemActive() && ImGui::GetIO().MouseDelta.y != 0.0f)
        {
            float& heightRatio = Settings().DrawerHeight;
            heightRatio = glm::clamp(heightRatio - ImGui::GetIO().MouseDelta.y / areaHeight, 0.2f, 0.9f);
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 headerMin = ImGui::GetItemRectMin();
        {
            ImGuiAux::ScopedBoldFont font;
            drawList->AddText(ImVec2(headerMin.x, std::floor(headerMin.y + (headerHeight - ImGui::GetFontSize()) * 0.5f)), ImGui::GetColorU32(ImGuiCol_TextDisabled), "CONTENT DRAWER");
        }
        const float gripX = std::floor(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() * 0.5f);
        const float gripY = std::floor(headerMin.y + headerHeight * 0.5f);
        drawList->AddRectFilled(ImVec2(gripX - 18.0f, gripY - 1.5f), ImVec2(gripX + 18.0f, gripY + 1.5f), ImGui::GetColorU32(ImVec4(0.4f, 0.4f, 0.4f, 1.0f)), 1.5f);

        ImGui::SetCursorPos(ImVec2(start.x + width - dockWidth, start.y + std::floor((headerHeight - ImGui::GetFrameHeight()) * 0.5f)));
        if(ImGui::Button(dockLabel))
        {
            // The tab takes over right away, no slide
            static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetAllPanels()[mCode].Show = true;
            SetDrawerOpen(false);
            mDrawerAnimation = 0.0f;
            mFocusTab = true;
        }
        ImGuiAux::ItemTooltip("Close the drawer and show the Content Browser as a tab");

        ImGui::SetCursorPos(ImVec2(start.x, start.y + headerHeight + 2.0f));
    }

    void ContentBrowserPanel::DrawToolbar()
    {
        ContentBrowserSettings& settings = Settings();
        const ImGuiStyle& style = ImGui::GetStyle();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

        {
            ImGuiAux::ScopedBoldFont bold;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImGuiAux::Colors::ThemeColor1));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.74f, 0.25f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.85f, 0.55f, 0.0f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
            if(ImGui::Button("+ ADD"))
                ImGui::OpenPopup("##CreatePopup");
            ImGui::PopStyleColor(4);
        }
        ImGuiAux::ItemTooltip("Create a folder or a new asset in this folder");

        ImGui::SameLine();
        if(ImGui::Button("IMPORT"))
        {
            const Vector<String> files = FileDialog::OpenFiles(IMPORT_FILTER);
            if(!files.empty())
                ImportExternal(files, mCurrentDirectory);
        }
        ImGuiAux::ItemTooltip("Copy files into this folder and import them (or drop them here from Explorer)");

        // History
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::BeginDisabled(mBackHistory.empty());
        if(ImGui::ArrowButton("##Back", ImGuiDir_Left))
            NavigateBack();
        ImGui::EndDisabled();
        ImGuiAux::ItemTooltip("Back (Alt+Left, Mouse 4)");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::BeginDisabled(mForwardHistory.empty());
        if(ImGui::ArrowButton("##Forward", ImGuiDir_Right))
            NavigateForward();
        ImGui::EndDisabled();
        ImGuiAux::ItemTooltip("Forward (Alt+Right, Mouse 5)");
        ImGui::SameLine(0.0f, 2.0f);
        ImGui::BeginDisabled(mCurrentDirectory == mBaseDirectory);
        if(ImGui::ArrowButton("##Up", ImGuiDir_Up))
            NavigateUp();
        ImGui::EndDisabled();
        ImGuiAux::ItemTooltip("Parent folder (Backspace)");

        // Right side: search, filter, sort, view
        constexpr float searchWidth = 210.0f;
        const bool filterActive = settings.HiddenTypes != 0 || !settings.ShowUnimported;
        const char* filterLabel = filterActive ? "Filter *" : "Filter";
        auto buttonWidth = [&style](const char* label) { return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f; };
        const float rightWidth = searchWidth + buttonWidth(filterLabel) + buttonWidth("Sort") + buttonWidth("Grid") + buttonWidth("List") + style.ItemSpacing.x * 3.0f + 2.0f;

        ImGui::SameLine(0.0f, 8.0f);
        DrawBreadcrumbs(std::max(40.0f, ImGui::GetContentRegionAvail().x - rightWidth - style.ItemSpacing.x));

        ImGui::SameLine();
        if(mFocusSearch)
        {
            ImGui::SetKeyboardFocusHere();
            mFocusSearch = false;
        }
        ImGui::SetNextItemWidth(searchWidth);
        ImGui::InputTextWithHint("##Search", settings.SearchAllFolders ? "Search project (Ctrl+F)" : "Search folder (Ctrl+F)", mSearchBuffer, IM_COUNTOF(mSearchBuffer));
        if(ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Escape))
            mSearchBuffer[0] = '\0';

        ImGui::SameLine();
        if(filterActive)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImGuiAux::Colors::ThemeColor1));
        if(ImGui::Button(filterLabel))
            ImGui::OpenPopup("##FilterPopup");
        if(filterActive)
            ImGui::PopStyleColor();
        ImGuiAux::ItemTooltip("Asset types to show, unimported files, search scope");

        ImGui::SameLine();
        if(ImGui::Button("Sort"))
            ImGui::OpenPopup("##SortPopup");

        // Grid | List
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, style.ItemSpacing.y));
        for(int view = 0; view < 2; view++)
        {
            const bool active = settings.ListView == (view == 1);
            if(active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImGuiAux::Colors::Selection));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImGuiAux::Colors::Selection));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            }
            if(ImGui::Button(view == 0 ? "Grid" : "List"))
                settings.ListView = view == 1;
            if(active)
                ImGui::PopStyleColor(3);
            if(view == 0)
                ImGui::SameLine();
        }
        ImGui::PopStyleVar();
        ImGui::PopStyleVar(); // FrameRounding

        // Popups
        ImGuiAux::StyledPopupVars::Push();
        if(ImGui::BeginPopup("##CreatePopup"))
        {
            DrawCreateMenuItems(mCurrentDirectory);
            ImGuiAux::EndStyledPopup();
        }
        else
            ImGuiAux::StyledPopupVars::Pop();

        DrawFilterPopup();
        DrawSortPopup();
    }

    void ContentBrowserPanel::DrawBreadcrumbs(float width)
    {
        Vector<Path> segments;
        for(Path path = mCurrentDirectory;; path = path.parent_path())
        {
            segments.push_back(path);
            if(path == mBaseDirectory || !AssetFileOps::IsInside(path, mBaseDirectory) || path == path.parent_path())
                break;
        }
        std::reverse(segments.begin(), segments.end());

        ImGui::BeginChild("##Breadcrumbs", ImVec2(width, ImGui::GetFrameHeight()), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));
        for(size_t i = 0; i < segments.size(); i++)
        {
            const bool last = i + 1 == segments.size();
            ImGui::PushID(static_cast<int>(i));
            if(last)
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
            if(ImGui::Button(i == 0 ? "Assets" : segments[i].filename().string().c_str()))
                Navigate(segments[i]);
            if(last)
                ImGui::PopFont();
            AcceptDrop(segments[i]);
            if(!last)
            {
                ImGui::SameLine();
                ImGui::TextDisabled(">");
                ImGui::SameLine();
            }
            ImGui::PopID();
        }
        ImGui::SetScrollX(ImGui::GetScrollMaxX()); // The deepest folders stay visible when the path doesn't fit
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::EndChild();
    }

    void ContentBrowserPanel::DrawFilterPopup()
    {
        ImGuiAux::StyledPopupVars::Push();
        if(!ImGui::BeginPopup("##FilterPopup"))
        {
            ImGuiAux::StyledPopupVars::Pop();
            return;
        }

        ContentBrowserSettings& settings = Settings();
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SHOW");
        }
        ImGuiAux::StyledSeparator();

        for(AssetType type : sAssetTypeArray)
        {
            const Uint bit = 1u << static_cast<Uint>(type);
            bool visible = !(settings.HiddenTypes & bit);
            ImGui::PushID(static_cast<int>(type));
            ImGui::ColorButton("##Color", GetTypeColor(type), ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(10.0f, 10.0f));
            ImGui::SameLine();
            if(ImGui::Checkbox(GetTypeLabel(type), &visible))
            {
                settings.HiddenTypes = visible ? (settings.HiddenTypes & ~bit) : (settings.HiddenTypes | bit);
                mNeedsSort = true;
            }
            ImGui::PopID();
        }

        ImGuiAux::StyledSeparator();
        if(ImGui::Checkbox("Unimported files", &settings.ShowUnimported))
            mNeedsSort = true;
        if(ImGui::Checkbox("Search the whole project", &settings.SearchAllFolders))
            mAppliedSearch = "\x01"; // Rebuild with the new scope
        ImGuiAux::StyledSeparator();
        if(ImGuiAux::StyledMenuItem("Reset Filters"))
        {
            settings.HiddenTypes = 0;
            settings.ShowUnimported = true;
            mNeedsSort = true;
        }
        ImGuiAux::EndStyledPopup();
    }

    void ContentBrowserPanel::DrawSortPopup()
    {
        ImGuiAux::StyledPopupVars::Push();
        if(!ImGui::BeginPopup("##SortPopup"))
        {
            ImGuiAux::StyledPopupVars::Pop();
            return;
        }

        ContentBrowserSettings& settings = Settings();
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "SORT BY");
        }
        ImGuiAux::StyledSeparator();

        constexpr const char* fields[] = { "Name", "Type", "Size", "Date Modified" };
        for(int i = 0; i < 4; i++)
        {
            if(ImGui::RadioButton(fields[i], settings.SortField == i))
            {
                settings.SortField = i;
                mNeedsSort = true;
            }
        }
        ImGuiAux::StyledSeparator();
        if(ImGui::Checkbox("Descending", &settings.SortDescending))
            mNeedsSort = true;
        ImGui::TextDisabled("Folders are always listed first");
        ImGuiAux::EndStyledPopup();
    }

    void ContentBrowserPanel::DrawFolderTree()
    {
        ContentBrowserSettings& settings = Settings();
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

        if(!settings.Favorites.empty())
        {
            ImGui::PushFont(boldFont);
            ImGui::TextDisabled("FAVORITES");
            ImGui::PopFont();

            int removeIndex = -1;
            for(int i = 0; i < static_cast<int>(settings.Favorites.size()); i++)
            {
                const Path folder = (mBaseDirectory / settings.Favorites[i]).lexically_normal();
                const bool exists = fs::is_directory(folder);
                ImGui::PushID(i);
                if(!exists)
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.35f, 0.35f, 1.0f));
                if(ImGui::Selectable(folder.filename().string().c_str(), folder == mCurrentDirectory) && exists)
                    Navigate(folder);
                if(!exists)
                    ImGui::PopStyleColor();
                ImGuiAux::ItemTooltip(exists ? ("Assets/" + settings.Favorites[i]).c_str() : "This folder no longer exists");
                if(exists)
                    AcceptDrop(folder);

                if(ImGuiAux::BeginStyledPopupContextItem())
                {
                    if(ImGuiAux::StyledMenuItem("Remove from Favorites"))
                        removeIndex = i;
                    if(exists && ImGuiAux::StyledMenuItem("Show in Explorer"))
                        Platform::OpenFolderInExplorer(folder.string());
                    ImGuiAux::EndStyledPopup();
                }
                ImGui::PopID();
            }
            if(removeIndex >= 0)
                settings.Favorites.erase(settings.Favorites.begin() + removeIndex);
            ImGui::Spacing();
        }

        ImGui::PushFont(boldFont);
        ImGui::TextDisabled("FOLDERS");
        ImGui::PopFont();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 14.0f);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.07f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.0f, 1.0f, 1.0f, 0.12f));
        DrawFolderNode(mFolderTree, true);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        mRevealInTree = false;
    }

    void ContentBrowserPanel::DrawFolderNode(const FolderNode& node, bool isRoot)
    {
        const bool isCurrent = node.AbsolutePath == mCurrentDirectory;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding |
                                   ImGuiTreeNodeFlags_DrawLinesToNodes | ImGuiTreeNodeFlags_NavLeftJumpsToParent;
        if(node.Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if(isCurrent)
            flags |= ImGuiTreeNodeFlags_Selected;
        if(isRoot)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if(mRevealInTree && !isCurrent && AssetFileOps::IsInside(mCurrentDirectory, node.AbsolutePath))
            ImGui::SetNextItemOpen(true);

        // The open folder is highlighted like the selection in the Hierarchy
        if(isCurrent)
        {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(ImGuiAux::Colors::Selection));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(ImGuiAux::Colors::Selection));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(ImGuiAux::Colors::Selection));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        }
        const bool open = ImGui::TreeNodeEx(node.Name.c_str(), flags, "%s", node.Name.c_str());
        if(isCurrent)
        {
            ImGui::PopFont();
            ImGui::PopStyleColor(4);
        }

        // On press, unless the press was on the arrow (that only opens/closes the node)
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
            Navigate(node.AbsolutePath);
        if(isCurrent && mRevealInTree && !ImGui::IsItemVisible())
            ImGui::SetScrollHereY(0.5f);
        AcceptDrop(node.AbsolutePath);

        if(ImGuiAux::BeginStyledPopupContextItem())
        {
            DrawFolderContextMenu(node.AbsolutePath, isRoot);
            ImGuiAux::EndStyledPopup();
        }

        if(open)
        {
            for(const FolderNode& child : node.Children)
                DrawFolderNode(child, false);
            ImGui::TreePop();
        }
    }

    void ContentBrowserPanel::DrawFolderContextMenu(const Path& folder, bool isRoot)
    {
        {
            ImGuiAux::ScopedBoldFont font;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", isRoot ? "Assets" : folder.filename().string().c_str());
        }
        ImGuiAux::StyledSeparator();

        if(ImGuiAux::StyledMenuItem("New Folder"))
        {
            Navigate(folder);
            CreateFolderIn(folder);
        }
        if(!isRoot)
        {
            if(ImGuiAux::StyledMenuItem("Rename"))
            {
                // Renamed inline, in its parent folder
                Navigate(folder.parent_path());
                SelectAfterRefresh({ folder }, true);
            }
            if(ImGuiAux::StyledMenuItem("Delete"))
                DeletePaths({ folder });
        }
        if(mClipboard.size() > 0 && ImGuiAux::StyledMenuItem("Paste Here"))
            PasteInto(folder);
        ImGuiAux::StyledSeparator();

        if(!isRoot)
        {
            Vector<String>& favorites = Settings().Favorites;
            const String relative = AssetFileOps::ToRelative(folder);
            const auto it = std::find(favorites.begin(), favorites.end(), relative);
            if(ImGuiAux::StyledMenuItem(it == favorites.end() ? "Add to Favorites" : "Remove from Favorites"))
            {
                if(it == favorites.end())
                    favorites.push_back(relative);
                else
                    favorites.erase(it);
            }
        }
        if(ImGuiAux::StyledMenuItem("Import All Unimported Files"))
            ImportUnregistered({ folder });
        if(ImGuiAux::StyledMenuItem("Copy Path"))
            ImGui::SetClipboardText(("Assets/" + AssetFileOps::ToRelative(folder)).c_str());
        if(ImGuiAux::StyledMenuItem("Show in Explorer"))
            Platform::OpenFolderInExplorer(folder.string());
    }

    void ContentBrowserPanel::DrawSplitter(float height)
    {
        ImGui::InvisibleButton("##FolderTreeSplitter", ImVec2(6.0f, height));
        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();
        if(hovered || active)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if(active)
        {
            float& width = Settings().FolderTreeWidth;
            width = std::clamp(width + ImGui::GetIO().MouseDelta.x, 120.0f, std::max(120.0f, ImGui::GetWindowWidth() - 240.0f));
        }

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        const float x = (min.x + max.x) * 0.5f;
        const ImU32 color = active ? WithAlpha(ImVec4(ImGuiAux::Colors::ThemeColor1), 1.0f) : (hovered ? IM_COL32(120, 120, 120, 255) : IM_COL32(50, 50, 50, 255));
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), color, active || hovered ? 2.0f : 1.0f);
    }

    void ContentBrowserPanel::DrawContent()
    {
        if(Settings().ListView)
            DrawList();
        else
            DrawGrid();
    }

    void ContentBrowserPanel::DrawGrid()
    {
        ContentBrowserSettings& settings = Settings();
        const float thumbnailSize = std::floor(settings.ThumbnailSize);
        const float lineHeight = ImGui::GetTextLineHeight();
        const ImVec2 tileSize(thumbnailSize + TILE_PADDING * 2.0f, TILE_PADDING + thumbnailSize + 7.0f + lineHeight * 2.0f + TILE_PADDING);
        const ImVec2 step(tileSize.x + TILE_SPACING, tileSize.y + TILE_SPACING);

        const int itemCount = static_cast<int>(mVisible.size());
        const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x - TILE_SPACING) / step.x));
        const int rows = (itemCount + columns - 1) / columns;

        ImVec2 start = ImGui::GetCursorScreenPos();
        start = ImVec2(start.x + TILE_SPACING, start.y + TILE_SPACING);
        ImGui::SetCursorScreenPos(start);

        const ImGuiMultiSelectFlags msFlags = ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_BoxSelect2d | ImGuiMultiSelectFlags_NavWrapX;
        ImGuiMultiSelectIO* msIO = ImGui::BeginMultiSelect(msFlags, mSelection.Size, itemCount);
        mSelection.ApplyRequests(msIO);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // The tiles show selection/focus themselves, no ImGui focus rectangle

        // The highlight is drawn by the tiles themselves
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(TILE_SPACING, TILE_SPACING));

        int scrollToIndex = -1;
        if(!mScrollToPath.empty())
        {
            for(int i = 0; i < itemCount; i++)
            {
                if(mItems[mVisible[i]].AbsolutePath == mScrollToPath)
                    scrollToIndex = i;
            }
            if(scrollToIndex < 0)
                mScrollToPath.clear(); // Not shown (filtered out), don't look for it every frame
        }

        ImGuiListClipper clipper;
        clipper.Begin(rows, step.y);
        if(scrollToIndex >= 0)
            clipper.IncludeItemByIndex(scrollToIndex / columns);
        if(msIO->RangeSrcItem != -1)
            clipper.IncludeItemByIndex(static_cast<int>(msIO->RangeSrcItem) / columns);
        while(clipper.Step())
        {
            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
            {
                const int first = row * columns;
                const int last = std::min(first + columns, itemCount);
                for(int index = first; index < last; index++)
                {
                    const ImVec2 pos(start.x + static_cast<float>(index % columns) * step.x, start.y + static_cast<float>(row) * step.y);
                    ImGui::SetCursorScreenPos(pos);
                    DrawTile(mItems[mVisible[index]], index, pos, tileSize);
                    if(index == scrollToIndex)
                    {
                        ImGui::SetScrollHereY(0.5f);
                        mScrollToPath.clear();
                    }
                }
            }
        }
        clipper.End();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if(itemCount == 0)
        {
            ImGui::Dummy(ImVec2(0.0f, 0.0f)); // The cursor was moved, an item has to follow
            DrawEmptyHint();
        }

        DrawItemContextMenu();
        DrawBackgroundContextMenu();

        ImGui::PopStyleColor(); // NavCursor
        msIO = ImGui::EndMultiSelect();
        mSelection.ApplyRequests(msIO);

        // Ctrl+Wheel zoom
        const ImGuiIO& io = ImGui::GetIO();
        if(ImGui::IsWindowHovered() && io.KeyCtrl && io.MouseWheel != 0.0f && !ImGui::IsAnyItemActive())
            settings.ThumbnailSize = std::clamp(settings.ThumbnailSize * std::pow(1.1f, io.MouseWheel), MIN_THUMBNAIL_SIZE, MAX_THUMBNAIL_SIZE);
    }

    void ContentBrowserPanel::DrawTile(Item& item, int index, const ImVec2& pos, const ImVec2& size)
    {
        const bool renaming = !mRenamingPath.empty() && item.AbsolutePath == mRenamingPath;

        ImGui::PushID(static_cast<int>(item.SelectionID));
        ImGui::SetNextItemSelectionUserData(index);
        bool selected = mSelection.Contains(item.SelectionID);
        const bool visible = ImGui::IsRectVisible(size);
        ImGui::Selectable("##Tile", selected, renaming ? ImGuiSelectableFlags_AllowOverlap : ImGuiSelectableFlags_None, size);
        if(ImGui::IsItemToggledSelection())
            selected = !selected;
        const bool hovered = ImGui::IsItemHovered();
        HandleItemInteraction(item, selected);

        if(visible)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 max(pos.x + size.x, pos.y + size.y);
            const ImVec4 accent(ImGuiAux::Colors::ThemeColor1);

            // Card
            const ImU32 background = selected ? IM_COL32(255, 255, 255, 0) : (hovered ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 7));
            drawList->AddRectFilled(pos, max, background, 6.0f);
            if(selected)
                drawList->AddRect(pos, max, WithAlpha(accent, 0.9f), 6.0f, ImDrawFlags_None, 1.5f);

            // Preview
            const float thumbnailSize = size.x - TILE_PADDING * 2.0f;
            const ImVec2 previewMin(pos.x + TILE_PADDING, pos.y + TILE_PADDING);
            const ImVec2 previewMax(previewMin.x + thumbnailSize, previewMin.y + thumbnailSize);
            drawList->AddRectFilled(previewMin, previewMax, IM_COL32(0, 0, 0, 55), 4.0f);
            const bool isCut = mClipboardCut && std::find(mClipboard.begin(), mClipboard.end(), item.AbsolutePath) != mClipboard.end();
            DrawItemIcon(drawList, item, previewMin, previewMax, isCut ? 0.4f : 1.0f);

            // State badge, centered at the top of the preview
            const float centerX = (previewMin.x + previewMax.x) * 0.5f;
            if(!item.IsDirectory)
            {
                const float maxBadgeWidth = thumbnailSize - 6.0f;
                if(item.IsMissing)
                    DrawBadge(drawList, centerX, previewMin.y + 3.0f, false, maxBadgeWidth, "MISSING", IM_COL32(205, 45, 45, 240), IM_COL32(255, 255, 255, 255));
                else if(!item.IsRegistered && item.Type != AssetType::NONE)
                    DrawBadge(drawList, centerX, previewMin.y + 3.0f, false, maxBadgeWidth, "NOT IMPORTED", IM_COL32(25, 25, 25, 230), IM_COL32(200, 200, 200, 255));
            }

            // Type strip under the preview, assets carry their type label in it
            const ImVec4 stripColor = item.IsDirectory ? FOLDER_COLOR : GetTypeColor(item.Type);
            if(!item.IsDirectory && item.Type != AssetType::NONE)
            {
                const ImU32 barColor = WithAlpha(stripColor, item.IsRegistered ? 1.0f : 0.3f);
                const ImU32 labelColor = item.IsRegistered ? IM_COL32(0, 0, 0, 255) : WithAlpha(stripColor, 1.0f);
                DrawTypeBar(drawList, previewMin.x, previewMax.x, previewMax.y + 4.0f, GetTypeLabel(item.Type), barColor, labelColor);
            }
            else
                drawList->AddRectFilled(ImVec2(previewMin.x, previewMax.y + 2.0f), ImVec2(previewMax.x, previewMax.y + 4.0f), WithAlpha(stripColor, item.IsRegistered || item.IsDirectory ? 1.0f : 0.35f), 1.0f);

            if(!renaming)
            {
                const ImU32 textColor = item.IsRegistered || item.IsDirectory ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
                DrawWrappedLabel(drawList, item.DisplayName, ImVec2(previewMin.x, previewMax.y + 7.0f), thumbnailSize, textColor);
            }
        }

        if(renaming)
        {
            ImGui::SetCursorScreenPos(ImVec2(pos.x + TILE_PADDING, pos.y + size.x + 1.0f)); // Where the label is
            DrawRenameField(size.x - TILE_PADDING * 2.0f);
        }
        ImGui::PopID();
    }

    void ContentBrowserPanel::DrawItemIcon(ImDrawList* drawList, const Item& item, const ImVec2& min, const ImVec2& max, float alpha)
    {
        const float boxSize = max.x - min.x;
        const ImVec2 center((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);

        if(item.IsImage)
        {
            const ThumbnailCache::Thumbnail thumbnail = mThumbnails.Get(item.AbsolutePath, item.WriteTime);
            if(thumbnail.TextureID)
            {
                // Fit, keeping the aspect ratio
                const float fit = boxSize * 0.92f;
                const ImVec2 half = thumbnail.AspectRatio >= 1.0f ? ImVec2(fit * 0.5f, fit * 0.5f / thumbnail.AspectRatio) : ImVec2(fit * 0.5f * thumbnail.AspectRatio, fit * 0.5f);
                drawList->AddImage(thumbnail.TextureID, ImVec2(center.x - half.x, center.y - half.y), ImVec2(center.x + half.x, center.y + half.y), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                                   IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
                return;
            }
        }

        const ImTextureID icon = item.IsDirectory ? (item.IsEmptyDirectory ? mEmptyFolderIconID : mFolderIconID) : mFileIconID;
        const float iconHalf = boxSize * (item.IsDirectory ? 0.40f : 0.33f);
        const float iconAlpha = item.IsRegistered || item.IsDirectory ? alpha : alpha * 0.45f;
        drawList->AddImage(icon, ImVec2(center.x - iconHalf, center.y - iconHalf - (item.IsDirectory ? 0.0f : boxSize * 0.04f)), ImVec2(center.x + iconHalf, center.y + iconHalf - (item.IsDirectory ? 0.0f : boxSize * 0.04f)),
                           ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, static_cast<int>(255.0f * iconAlpha)));
    }

    void ContentBrowserPanel::DrawList()
    {
        ContentBrowserSettings& settings = Settings();
        const bool searchingProject = mSearchBuffer[0] != '\0' && settings.SearchAllFolders;
        const int columnCount = searchingProject ? 5 : 4;

        constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerV |
                                               ImGuiTableFlags_Hideable | ImGuiTableFlags_Reorderable;
        if(!ImGui::BeginTable(searchingProject ? "##ListSearch" : "##List", columnCount, tableFlags))
            return;

        auto sortFlags = [&settings](SortField field) {
            ImGuiTableColumnFlags flags = ImGuiTableColumnFlags_None;
            if(settings.SortField == static_cast<int>(field))
                flags |= ImGuiTableColumnFlags_DefaultSort | (settings.SortDescending ? ImGuiTableColumnFlags_PreferSortDescending : ImGuiTableColumnFlags_None);
            return flags;
        };
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoHide | sortFlags(SortField::NAME), 0.0f, static_cast<ImGuiID>(SortField::NAME));
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed | sortFlags(SortField::TYPE), 90.0f, static_cast<ImGuiID>(SortField::TYPE));
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed | sortFlags(SortField::SIZE), 80.0f, static_cast<ImGuiID>(SortField::SIZE));
        ImGui::TableSetupColumn("Modified", ImGuiTableColumnFlags_WidthFixed | sortFlags(SortField::MODIFIED), 125.0f, static_cast<ImGuiID>(SortField::MODIFIED));
        if(searchingProject)
            ImGui::TableSetupColumn("Location", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort, 180.0f, 100);
        ImGui::TableHeadersRow();

        if(ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsDirty && specs->SpecsCount > 0)
        {
            settings.SortField = static_cast<int>(specs->Specs[0].ColumnUserID);
            settings.SortDescending = specs->Specs[0].SortDirection == ImGuiSortDirection_Descending;
            specs->SpecsDirty = false;
            SortAndFilter();
        }

        const int itemCount = static_cast<int>(mVisible.size());
        const float rowHeight = ImGui::GetFrameHeight();
        ImGuiMultiSelectIO* msIO = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnEscape | ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_BoxSelect1d, mSelection.Size, itemCount);
        mSelection.ApplyRequests(msIO);
        ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f)); // Row highlight only, no ImGui focus rectangle

        int scrollToIndex = -1;
        if(!mScrollToPath.empty())
        {
            for(int i = 0; i < itemCount; i++)
            {
                if(mItems[mVisible[i]].AbsolutePath == mScrollToPath)
                    scrollToIndex = i;
            }
            if(scrollToIndex < 0)
                mScrollToPath.clear(); // Not shown (filtered out), don't look for it every frame
        }

        ImGuiListClipper clipper;
        clipper.Begin(itemCount, rowHeight);
        if(scrollToIndex >= 0)
            clipper.IncludeItemByIndex(scrollToIndex);
        if(msIO->RangeSrcItem != -1)
            clipper.IncludeItemByIndex(static_cast<int>(msIO->RangeSrcItem));
        while(clipper.Step())
        {
            for(int index = clipper.DisplayStart; index < clipper.DisplayEnd; index++)
            {
                Item& item = mItems[mVisible[index]];
                const bool renaming = !mRenamingPath.empty() && item.AbsolutePath == mRenamingPath;

                ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(item.SelectionID));
                ImGui::SetNextItemSelectionUserData(index);
                bool selected = mSelection.Contains(item.SelectionID);
                ImGui::Selectable("##Row", selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap, ImVec2(0.0f, rowHeight));
                if(ImGui::IsItemToggledSelection())
                    selected = !selected;
                HandleItemInteraction(item, selected);
                if(index == scrollToIndex)
                {
                    ImGui::SetScrollHereY(0.5f);
                    mScrollToPath.clear();
                }

                // Icon + name
                const ImVec2 rowMin = ImGui::GetItemRectMin();
                const float iconSize = rowHeight - 4.0f;
                const ImVec2 iconMin(rowMin.x + 2.0f, rowMin.y + 2.0f);
                DrawItemIcon(ImGui::GetWindowDrawList(), item, iconMin, ImVec2(iconMin.x + iconSize, iconMin.y + iconSize), 1.0f);
                ImGui::SetCursorScreenPos(ImVec2(iconMin.x + iconSize + 6.0f, rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f));
                if(renaming)
                {
                    ImGui::SetCursorScreenPos(ImVec2(iconMin.x + iconSize + 4.0f, rowMin.y));
                    DrawRenameField(std::max(80.0f, ImGui::GetContentRegionAvail().x));
                }
                else if(item.IsRegistered || item.IsDirectory)
                    ImGui::TextUnformatted(item.DisplayName.c_str());
                else
                    ImGui::TextDisabled("%s", item.DisplayName.c_str());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                if(item.IsDirectory)
                    ImGui::TextColored(FOLDER_COLOR, "FOLDER");
                else if(item.Type != AssetType::NONE)
                {
                    const ImVec4 color = GetTypeColor(item.Type);
                    ImGui::TextColored(item.IsRegistered ? color : ImVec4(color.x, color.y, color.z, 0.5f), "%s", GetTypeLabel(item.Type));
                    if(item.IsMissing)
                    {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.9f, 0.25f, 0.25f, 1.0f), "MISSING");
                    }
                }
                else
                    ImGui::TextDisabled("FILE");

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", item.SizeText.c_str());

                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", item.ModifiedText.c_str());

                if(searchingProject)
                {
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextDisabled("%s", item.Location.c_str());
                }
                ImGui::PopID();
            }
        }
        clipper.End();

        DrawItemContextMenu();
        DrawBackgroundContextMenu();

        ImGui::PopStyleColor(); // NavCursor
        msIO = ImGui::EndMultiSelect();
        mSelection.ApplyRequests(msIO);
        ImGui::EndTable();

        if(itemCount == 0)
            DrawEmptyHint();
    }

    void ContentBrowserPanel::DrawRenameField(float width)
    {
        if(mFocusRename)
        {
            ImGui::SetKeyboardFocusHere();
            mFocusRename = false;
        }

        ImGui::SetNextItemWidth(width);
        ImGui::InputText("##Rename", &mRenameBuffer, ImGuiInputTextFlags_AutoSelectAll);

        // Enter or clicking elsewhere applies, Esc cancels
        if(ImGui::IsItemDeactivated())
        {
            if(ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                mRenamingPath.clear();
            else
                CommitRename();
        }
        else if(!ImGui::IsItemActive() && --mRenameGraceFrames < 0)
            mRenamingPath.clear(); // Never got the keyboard focus
    }

    void ContentBrowserPanel::HandleItemInteraction(Item& item, bool selected)
    {
        // Pressing an unselected item arms ImGui's box-select, which activates on the same drag threshold as the drag & drop source
        // but earlier in the frame (BeginMultiSelect) and steals the active ID: dragging an item drags it, box-select starts from empty space only
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            ImGuiBoxSelectState& boxSelect = ImGui::GetCurrentContext()->BoxSelectState;
            if(boxSelect.IsStarting && !boxSelect.IsStartedFromVoid)
            {
                boxSelect.IsStarting = false;
                boxSelect.ID = 0;
            }
        }

        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            OpenItem(item);

        // Right click acts on the selection, or on this item alone when it isn't part of it. The menu opens on release like every
        // other context menu: opened on press, the release would open the empty space menu over it (the item no longer counts as hovered)
        if(ImGui::IsItemClicked(ImGuiMouseButton_Right) && !selected)
        {
            mSelection.Clear();
            mSelection.SetItemSelected(item.SelectionID, true);
        }
        if(ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            mOpenItemContext = true;

        if(ImGui::BeginDragDropSource())
        {
            if(!ImGui::GetDragDropPayload())
            {
                Vector<Path> paths = selected ? GetSelectedPaths() : Vector<Path> { item.AbsolutePath };
                if(paths.empty())
                    paths.push_back(item.AbsolutePath);

                // A single registered asset travels as its AssetID: the Inspector, Viewport, Material Editor... accept that one
                if(paths.size() == 1 && paths.front() == item.AbsolutePath && item.IsRegistered)
                    ImGui::SetDragDropPayload(CONTENT_BROWSER_PAYLOAD, &item.ID, sizeof(AssetID));
                else
                {
                    String data;
                    for(const Path& path : paths)
                        data += path.generic_string() + '\n';
                    ImGui::SetDragDropPayload(CONTENT_BROWSER_ITEMS_PAYLOAD, data.c_str(), data.size() + 1);
                }
            }

            const ImGuiPayload* payload = ImGui::GetDragDropPayload();
            if(payload && payload->IsDataType(CONTENT_BROWSER_ITEMS_PAYLOAD))
            {
                const char* data = static_cast<const char*>(payload->Data);
                ImGui::Text("%d items", static_cast<int>(std::count(data, data + payload->DataSize, '\n')));
            }
            else
                ImGui::TextUnformatted(item.Name.c_str());
            ImGui::EndDragDropSource();
        }

        if(item.IsDirectory)
            AcceptDrop(item.AbsolutePath);

        if(!ImGui::GetDragDropPayload() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary))
        {
            String tooltip = item.Name;
            if(item.IsDirectory)
                tooltip += "\nFolder";
            else
            {
                tooltip += item.IsRegistered ? std::format("\n{}", SurgeReflect::EnumToString(item.Type).data()) : (item.Type != AssetType::NONE ? String("\nNot imported, double-click to import") : String("\nNot an asset type"));
                tooltip += std::format("\n{}  |  {}", item.SizeText, item.ModifiedText);
                if(item.IsRegistered)
                    tooltip += std::format("\nAsset ID: {}", item.ID.Get());
                if(item.IsMissing)
                    tooltip += "\nThe asset failed to load (missing or corrupted cooked data), try Reimport";
            }
            if(mSearchBuffer[0] != '\0')
                tooltip += "\n" + item.Location;
            ImGuiAux::ItemTooltip(tooltip.c_str());
        }
    }

    bool ContentBrowserPanel::AcceptDrop(const Path& folder)
    {
        if(!ImGui::BeginDragDropTarget())
            return false;

        Vector<Path> paths;
        if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_ITEMS_PAYLOAD))
        {
            const String data(static_cast<const char*>(payload->Data));
            size_t start = 0;
            for(size_t end = data.find('\n'); end != String::npos; start = end + 1, end = data.find('\n', start))
                paths.emplace_back(data.substr(start, end - start));
        }
        else if(const ImGuiPayload* assetPayload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
        {
            const AssetID id = *static_cast<const AssetID*>(assetPayload->Data);
            const AssetMetadata& meta = mAssetManager->GetMetadata(id);
            if(meta.ID && !HasFlag(meta.Flags, AssetFlags::MEMORY))
                paths.push_back(mBaseDirectory / meta.RelativePath);
        }
        ImGui::EndDragDropTarget();

        if(paths.empty())
            return false;
        MoveInto(paths, folder);
        return true;
    }

    void ContentBrowserPanel::DrawItemContextMenu()
    {
        if(mOpenItemContext)
        {
            ImGui::OpenPopup("##ItemContext");
            mOpenItemContext = false;
        }

        ImGuiAux::StyledPopupVars::Push();
        if(!ImGui::BeginPopup("##ItemContext"))
        {
            ImGuiAux::StyledPopupVars::Pop();
            return;
        }

        const Vector<const Item*> selection = GetSelection();
        if(selection.empty())
        {
            ImGui::CloseCurrentPopup();
            ImGuiAux::EndStyledPopup();
            return;
        }

        const Item* single = selection.size() == 1 ? selection.front() : nullptr;
        bool anyUnregistered = false, anyCookable = false;
        for(const Item* item : selection)
        {
            anyUnregistered |= !item->IsRegistered && (item->IsDirectory || item->Type != AssetType::NONE);
            anyCookable |= item->IsRegistered && IsCookable(item->Type);
        }

        {
            ImGuiAux::ScopedBoldFont font;
            if(single)
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", single->Name.c_str());
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%zu ITEMS", selection.size());
        }
        ImGuiAux::StyledSeparator();

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        if(single && ImGuiAux::StyledMenuItem(single->IsDirectory ? "Open" : (single->IsRegistered ? "Open" : "Import"), "Enter"))
            OpenItem(*single);
        if(anyUnregistered && !(single && !single->IsRegistered && !single->IsDirectory) && ImGuiAux::StyledMenuItem("Import"))
        {
            Vector<Path> paths;
            for(const Item* item : selection)
                paths.push_back(item->AbsolutePath);
            ImportUnregistered(paths);
        }
        if(anyCookable && ImGuiAux::StyledMenuItem("Reimport"))
            Reimport(selection);
        ImGuiAux::StyledSeparator();

        if(ImGuiAux::StyledMenuItem("Rename", "F2", nullptr, single != nullptr))
            BeginRename(single->AbsolutePath);
        if(ImGuiAux::StyledMenuItem("Duplicate", "Ctrl+D"))
            Duplicate(GetSelectedPaths());
        if(ImGuiAux::StyledMenuItem("Cut", "Ctrl+X"))
        {
            mClipboard = GetSelectedPaths();
            mClipboardCut = true;
        }
        if(ImGuiAux::StyledMenuItem("Copy", "Ctrl+C"))
        {
            mClipboard = GetSelectedPaths();
            mClipboardCut = false;
        }
        if(single && single->IsDirectory && !mClipboard.empty() && ImGuiAux::StyledMenuItem("Paste Into", "Ctrl+V"))
            PasteInto(single->AbsolutePath);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            if(ImGuiAux::StyledMenuItem("Delete", "Del"))
                DeletePaths(GetSelectedPaths());
            ImGui::PopStyleColor();
        }
        ImGuiAux::StyledSeparator();

        if(ImGuiAux::StyledMenuItem("Copy Path"))
        {
            String text;
            for(const Item* item : selection)
                text += (text.empty() ? "" : "\n") + ("Assets/" + AssetFileOps::ToRelative(item->AbsolutePath));
            ImGui::SetClipboardText(text.c_str());
        }
        if(single && single->IsRegistered && ImGuiAux::StyledMenuItem("Copy Asset ID"))
            ImGui::SetClipboardText(std::to_string(single->ID.Get()).c_str());
        if(single && ImGuiAux::StyledMenuItem("Show in Explorer"))
            Platform::OpenInExplorer(single->AbsolutePath.string());

        if(single && !single->IsDirectory && (single->Type == AssetType::SCRIPT || single->Type == AssetType::SCENE || single->Type == AssetType::MATERIAL || single->Type == AssetType::UI_LAYOUT || single->Type == AssetType::PARTICLE_EFFECT) &&
           ImGuiAux::StyledMenuItem("Open in VS Code"))
            Platform::OpenInVSCode(mAssetManager->GetAssetsDirectory(), single->AbsolutePath.string());

        if(single && single->IsRegistered && single->Type == AssetType::SCENE)
        {
            const bool isStart = editor->GetCurrentProject().StartScene == single->ID;
            if(ImGuiAux::StyledMenuItem(isStart ? "Start Scene" : "Set as Start Scene", nullptr, nullptr, !isStart))
                editor->SetStartScene(single->ID);
        }
        if(single && single->IsDirectory)
        {
            Vector<String>& favorites = Settings().Favorites;
            const String relative = AssetFileOps::ToRelative(single->AbsolutePath);
            const auto it = std::find(favorites.begin(), favorites.end(), relative);
            if(ImGuiAux::StyledMenuItem(it == favorites.end() ? "Add to Favorites" : "Remove from Favorites"))
            {
                if(it == favorites.end())
                    favorites.push_back(relative);
                else
                    favorites.erase(it);
            }
        }
        ImGuiAux::EndStyledPopup();
    }

    void ContentBrowserPanel::DrawBackgroundContextMenu()
    {
        if(!ImGuiAux::BeginStyledPopupContextWindow("##BackgroundContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_NoOpenOverExistingPopup))
            return;

        {
            ImGuiAux::ScopedBoldFont font;
            const String relative = AssetFileOps::ToRelative(mCurrentDirectory);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", relative.empty() ? "Assets" : ("Assets/" + relative).c_str());
        }
        ImGuiAux::StyledSeparator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
        if(ImGui::BeginMenu("Create"))
        {
            DrawCreateMenuItems(mCurrentDirectory);
            ImGui::EndMenu();
        }
        ImGui::PopStyleVar();

        if(ImGuiAux::StyledMenuItem("Import Files..."))
        {
            const Vector<String> files = FileDialog::OpenFiles(IMPORT_FILTER);
            if(!files.empty())
                ImportExternal(files, mCurrentDirectory);
        }
        if(ImGuiAux::StyledMenuItem("Import All Unimported Files"))
            ImportUnregistered({ mCurrentDirectory });
        if(ImGuiAux::StyledMenuItem("Paste", "Ctrl+V", nullptr, !mClipboard.empty()))
            PasteInto(mCurrentDirectory);
        ImGuiAux::StyledSeparator();

        if(ImGuiAux::StyledMenuItem("Show in Explorer"))
            Platform::OpenFolderInExplorer(mCurrentDirectory.string());
        if(ImGuiAux::StyledMenuItem("Refresh", "Ctrl+R"))
            mNeedsRefresh = true;
        ImGuiAux::EndStyledPopup();
    }

    void ContentBrowserPanel::DrawCreateMenuItems(const Path& folder)
    {
        if(ImGuiAux::StyledMenuItem("Folder", "Ctrl+Shift+N"))
            CreateFolderIn(folder);
        ImGuiAux::StyledSeparator();
        if(ImGuiAux::StyledMenuItem("Scene"))
            CreateAssetIn(AssetType::SCENE, folder);
        if(ImGuiAux::StyledMenuItem("Material"))
            CreateAssetIn(AssetType::MATERIAL, folder);
        if(ImGuiAux::StyledMenuItem("Script"))
            CreateAssetIn(AssetType::SCRIPT, folder);
        if(ImGuiAux::StyledMenuItem("UI Layout"))
            CreateAssetIn(AssetType::UI_LAYOUT, folder);
        if(ImGuiAux::StyledMenuItem("Particle Effect"))
            CreateAssetIn(AssetType::PARTICLE_EFFECT, folder);
    }

    void ContentBrowserPanel::DrawEmptyHint()
    {
        const char* hint = mSearchBuffer[0] != '\0' ? "No results" : "This folder is empty\nRight-click to create, or drop files from Explorer to import";
        const ImVec2 textSize = ImGui::CalcTextSize(hint);
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        ImGui::GetWindowDrawList()->AddText(ImVec2(windowPos.x + (windowSize.x - textSize.x) * 0.5f, windowPos.y + windowSize.y * 0.4f), ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
    }

    void ContentBrowserPanel::UpdateThumbnailTask(size_t pending)
    {
        if(pending == mThumbnailTaskCount)
            return;

        mThumbnailTaskCount = pending;
        if(pending > 0)
            EditorStatusBar::Get().SetTask(THUMBNAIL_TASK_KEY, std::format("Loading thumbnails ({})", pending));
        else
            EditorStatusBar::Get().EndTask(THUMBNAIL_TASK_KEY);
    }

    void ContentBrowserPanel::DrawStatusBar()
    {
        ContentBrowserSettings& settings = Settings();
        ImGui::AlignTextToFramePadding();

        // Left: counts
        const Vector<const Item*> selection = GetSelection();
        String counts = std::format("{} item{}", mVisible.size(), mVisible.size() == 1 ? "" : "s");
        if(!selection.empty())
        {
            uintmax_t bytes = 0;
            for(const Item* item : selection)
                bytes += item->Size;
            counts += std::format("  |  {} selected", selection.size());
            if(bytes > 0)
                counts += std::format(" ({})", FormatSize(bytes));
            if(selection.size() == 1 && selection.front()->IsRegistered)
                counts += std::format("  |  ID {}", selection.front()->ID.Get());
        }
        ImGui::TextDisabled("%s", counts.c_str());

        // Middle: last operation, fades out
        mStatusTimer -= ImGui::GetIO().DeltaTime;
        if(mStatusTimer > 0.0f && !mStatusMessage.empty())
        {
            const float alpha = std::min(1.0f, mStatusTimer);
            ImGui::SameLine(0.0f, 24.0f);
            ImGui::TextColored(mStatusIsError ? ImVec4(1.0f, 0.4f, 0.4f, alpha) : ImVec4(0.55f, 0.85f, 0.55f, alpha), "%s", mStatusMessage.c_str());
        }

        // Right: zoom
        if(!settings.ListView)
        {
            constexpr float sliderWidth = 130.0f;
            ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 8.0f, ImGui::GetWindowContentRegionMax().x - sliderWidth));
            ImGui::SetNextItemWidth(sliderWidth);
            ImGui::SliderFloat("##ThumbnailSize", &settings.ThumbnailSize, MIN_THUMBNAIL_SIZE, MAX_THUMBNAIL_SIZE, "%.0f px");
            ImGuiAux::ItemTooltip("Thumbnail size (Ctrl+Wheel)");
        }
    }

    void ContentBrowserPanel::HandleShortcuts()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if(io.WantTextInput || !mRenamingPath.empty())
            return;

        const bool ctrl = io.KeyCtrl;
        const bool shift = io.KeyShift;
        const bool alt = io.KeyAlt;

        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
            mFocusSearch = true;
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_R, false))
            mNeedsRefresh = true;
        if(ctrl && shift && ImGui::IsKeyPressed(ImGuiKey_N, false))
            CreateFolderIn(mCurrentDirectory);
        if(alt && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
            NavigateBack();
        if(alt && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
            NavigateForward();
        if((alt && ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            NavigateUp();
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_V, false))
            PasteInto(mCurrentDirectory);

        const Vector<Path> selection = GetSelectedPaths();
        if(selection.empty())
            return;

        if(ImGui::IsKeyPressed(ImGuiKey_Delete, false))
            DeletePaths(selection);
        if(ImGui::IsKeyPressed(ImGuiKey_F2, false) && selection.size() == 1)
            BeginRename(selection.front());
        if(ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
            Duplicate(selection);
        if(ctrl && (ImGui::IsKeyPressed(ImGuiKey_C, false) || ImGui::IsKeyPressed(ImGuiKey_X, false)))
        {
            mClipboard = selection;
            mClipboardCut = ImGui::IsKeyPressed(ImGuiKey_X, false);
        }
        if(!ctrl && !alt && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) && selection.size() == 1)
        {
            for(const Item* item : GetSelection())
                OpenItem(*item);
        }
    }

    void ContentBrowserPanel::HandleOsDrop()
    {
        if(mDroppedFiles.empty())
            return;

        const ImVec2 viewportPos = ImGui::GetMainViewport()->Pos;
        const ImVec2 point(viewportPos.x + mDropPoint.x, viewportPos.y + mDropPoint.y);
        const ImVec2 min = ImGui::GetWindowPos();
        const ImVec2 max(min.x + ImGui::GetWindowWidth(), min.y + ImGui::GetWindowHeight());
        if(point.x >= min.x && point.y >= min.y && point.x < max.x && point.y < max.y)
        {
            ImportExternal(mDroppedFiles, mCurrentDirectory);
            ImGui::SetWindowFocus();
        }
        else
            Log<Severity::Info>("[ContentBrowser] Drop files onto the Content Browser to import them");
        mDroppedFiles.clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // Operations
    //////////////////////////////////////////////////////////////////////////

    Vector<const ContentBrowserPanel::Item*> ContentBrowserPanel::GetSelection() const
    {
        Vector<const Item*> result;
        if(mSelection.Size == 0)
            return result;

        for(int index : mVisible)
        {
            if(mSelection.Contains(mItems[index].SelectionID))
                result.push_back(&mItems[index]);
        }
        return result;
    }

    Vector<Path> ContentBrowserPanel::GetSelectedPaths() const
    {
        Vector<Path> result;
        for(const Item* item : GetSelection())
            result.push_back(item->AbsolutePath);
        return result;
    }

    void ContentBrowserPanel::OpenItem(const Item& item)
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        if(item.IsDirectory)
        {
            Navigate(item.AbsolutePath);
            return;
        }
        if(!item.IsRegistered)
        {
            if(item.Type != AssetType::NONE)
                ImportUnregistered({ item.AbsolutePath });
            else
                Platform::OpenInExplorer(item.AbsolutePath.string());
            return;
        }

        switch(item.Type)
        {
            case AssetType::SCENE: editor->OpenScene(item.ID); break; // Asks first when there are unsaved changes
            case AssetType::MATERIAL: editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->OpenMaterial(mAssetManager->Load<Material>(item.ID)); break;
            case AssetType::UI_LAYOUT: editor->GetPanelManager().GetPanel<UIEditorPanel>()->OpenLayout(item.ID); break;
            case AssetType::PARTICLE_EFFECT: editor->GetPanelManager().GetPanel<ParticleEditorPanel>()->OpenEffect(item.ID); break;
            case AssetType::SCRIPT: Platform::OpenInVSCode(mAssetManager->GetAssetsDirectory(), item.AbsolutePath.string()); break;
            default: Notify(std::format("{} has no editor, drag it onto a component or the Viewport", item.DisplayName)); break;
        }
    }

    void ContentBrowserPanel::BeginRename(const Path& path)
    {
        mRenamingPath = path.lexically_normal();
        if(fs::is_directory(path))
        {
            mRenameBuffer = path.filename().string();
            mRenameExtension.clear();
        }
        else
        {
            mRenameBuffer = path.stem().string();
            mRenameExtension = path.extension().string();
        }
        mFocusRename = true;
        mRenameGraceFrames = 3;
        mScrollToPath = mRenamingPath;
    }

    void ContentBrowserPanel::CommitRename()
    {
        const Path path = mRenamingPath;
        mRenamingPath.clear();
        if(path.empty())
            return;

        String name = mRenameBuffer;
        name.erase(name.find_last_not_of(" \t") + 1);
        name.erase(0, name.find_first_not_of(" \t"));
        name += mRenameExtension;
        if(name == path.filename().string())
            return;

        Path renamed;
        String error;
        if(!AssetFileOps::Rename(path, name, renamed, error))
        {
            Notify(error, true);
            return;
        }

        OnPathMoved(path, renamed);
        SelectAfterRefresh({ renamed });
        Notify(std::format("Renamed to {}", renamed.filename().string()));
    }

    void ContentBrowserPanel::OnPathMoved(const Path& from, const Path& to)
    {
        // Everything that remembers a path follows the file
        Rebase(mCurrentDirectory, from, to);
        for(Path& path : mBackHistory)
            Rebase(path, from, to);
        for(Path& path : mForwardHistory)
            Rebase(path, from, to);
        for(Path& path : mClipboard)
            Rebase(path, from, to);
        for(String& favorite : Settings().Favorites)
        {
            Path absolute = (mBaseDirectory / favorite).lexically_normal();
            if(Rebase(absolute, from, to))
                favorite = AssetFileOps::ToRelative(absolute);
        }
        Settings().LastDirectory = AssetFileOps::ToRelative(mCurrentDirectory);

        // The open scene's file may have been renamed/moved
        static_cast<Editor*>(Core::GetClient())->GetViewports().OnSceneContextChanged();
        mNeedsRefresh = true;
    }

    bool ContentBrowserPanel::IsProtected(const Vector<Path>& paths, String& outReason) const
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        const AssetID sceneID = editor->GetCurrentScene() ? editor->GetCurrentScene()->GetID() : AssetID(AssetID::INVALID);
        const AssetMetadata& meta = mAssetManager->GetMetadata(sceneID);
        if(!meta.ID)
            return false;

        const Path scenePath = mBaseDirectory / meta.RelativePath;
        for(const Path& path : paths)
        {
            if(AssetFileOps::IsInside(scenePath, path))
            {
                outReason = std::format("'{}' is the open scene{}, open another scene first", scenePath.filename().string(), path == scenePath.lexically_normal() ? "" : " (inside the selection)");
                return true;
            }
        }
        return false;
    }

    void ContentBrowserPanel::DeletePaths(const Vector<Path>& paths)
    {
        if(paths.empty())
            return;

        String reason;
        if(IsProtected(paths, reason))
        {
            Notify(reason, true);
            return;
        }

        size_t registered = 0;
        String names;
        for(size_t i = 0; i < paths.size(); i++)
        {
            registered += AssetFileOps::CollectAssets(paths[i]).size();
            if(i < 6)
                names += "\n  - " + paths[i].filename().string();
        }
        if(paths.size() > 6)
            names += std::format("\n  ... and {} more", paths.size() - 6);

        String message = std::format("Move {} item{} to the Recycle Bin?{}", paths.size(), paths.size() == 1 ? "" : "s", names);
        if(registered > 0)
            message += std::format("\n\n{} registered asset{} will be unregistered. Anything still using them shows them as missing once reloaded.", registered, registered == 1 ? "" : "s");

        ImGuiAux::ShowConfirmationBox("Delete", message, [this, paths]() {
            size_t deleted = 0;
            for(const Path& path : paths)
            {
                String error;
                if(AssetFileOps::Delete(path, error))
                {
                    deleted++;
                    if(AssetFileOps::IsInside(mCurrentDirectory, path))
                        mCurrentDirectory = path.parent_path().lexically_normal();
                    std::erase_if(mClipboard, [&path](const Path& entry) { return AssetFileOps::IsInside(entry, path); });
                }
                else
                    Notify(error, true);
            }
            if(deleted > 0)
                Notify(std::format("Moved {} item{} to the Recycle Bin", deleted, deleted == 1 ? "" : "s"));
            mNeedsRefresh = true;
        });
    }

    void ContentBrowserPanel::Duplicate(const Vector<Path>& paths)
    {
        Vector<Path> copies;
        for(const Path& path : paths)
        {
            Path copy;
            String error;
            if(AssetFileOps::Copy(path, path.parent_path(), copy, error))
                copies.push_back(copy);
            else
                Notify(error, true);
        }
        if(!copies.empty())
        {
            Notify(std::format("Duplicated {} item{}", copies.size(), copies.size() == 1 ? "" : "s"));
            SelectAfterRefresh(copies);
        }
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::MoveInto(const Vector<Path>& paths, const Path& folder)
    {
        size_t moved = 0;
        for(const Path& path : paths)
        {
            const Path source = path.lexically_normal();
            if(source == folder.lexically_normal() || source.parent_path() == folder.lexically_normal())
                continue; // Onto itself, or already there

            Path destination;
            String error;
            if(AssetFileOps::Move(source, folder, destination, error))
            {
                OnPathMoved(source, destination);
                moved++;
            }
            else
                Notify(error, true);
        }
        if(moved > 0)
            Notify(std::format("Moved {} item{} to {}", moved, moved == 1 ? "" : "s", folder == mBaseDirectory ? String("Assets") : folder.filename().string()));
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::PasteInto(const Path& folder)
    {
        if(mClipboard.empty())
            return;

        if(mClipboardCut)
        {
            const Vector<Path> paths = mClipboard;
            mClipboard.clear();
            mClipboardCut = false;
            MoveInto(paths, folder);
            return;
        }

        Vector<Path> copies;
        for(const Path& path : mClipboard)
        {
            if(!fs::exists(path))
                continue;
            Path copy;
            String error;
            if(AssetFileOps::Copy(path, folder, copy, error))
                copies.push_back(copy);
            else
                Notify(error, true);
        }
        if(!copies.empty())
        {
            Notify(std::format("Pasted {} item{}", copies.size(), copies.size() == 1 ? "" : "s"));
            if(folder.lexically_normal() == mCurrentDirectory)
                SelectAfterRefresh(copies);
        }
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::ImportExternal(const Vector<String>& files, const Path& folder)
    {
        Vector<Path> created;
        size_t imported = 0;
        for(const String& file : files)
        {
            const Path source = Path(file).lexically_normal();
            if(!fs::exists(source))
                continue;

            // Already part of the project: register in place
            if(AssetFileOps::IsInside(source, mBaseDirectory))
            {
                imported += fs::is_directory(source) ? AssetFileOps::ImportFolder(source) : (AssetFileOps::Import(source) ? 1 : 0);
                continue;
            }

            const size_t registeredBefore = mAssetManager->GetRegistryMap().size();
            Path copy;
            String error;
            if(AssetFileOps::Copy(source, folder, copy, error))
            {
                created.push_back(copy);
                imported += mAssetManager->GetRegistryMap().size() - registeredBefore;
            }
            else
                Notify(error, true);
        }

        mSearchBuffer[0] = '\0';
        if(!created.empty() && folder.lexically_normal() == mCurrentDirectory)
            SelectAfterRefresh(created);
        Notify(std::format("Imported {} asset{}{}", imported, imported == 1 ? "" : "s", created.empty() ? "" : std::format(" ({} item{} copied)", created.size(), created.size() == 1 ? "" : "s")));
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::ImportUnregistered(const Vector<Path>& paths)
    {
        Uint imported = 0;
        for(const Path& path : paths)
            imported += fs::is_directory(path) ? AssetFileOps::ImportFolder(path) : (AssetFileOps::Import(path) ? 1 : 0);
        Notify(imported > 0 ? std::format("Imported {} asset{}", imported, imported == 1 ? "" : "s") : String("Nothing new to import"));
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::Reimport(const Vector<const Item*>& items)
    {
        AssetImporter& importer = static_cast<Editor*>(Core::GetClient())->GetAssetImporter();
        size_t succeeded = 0, failed = 0;
        for(const Item* item : items)
        {
            if(!item->IsRegistered || !IsCookable(item->Type))
                continue;
            importer.RecookAsset(item->ID).Success ? succeeded++ : failed++;
        }
        if(failed > 0)
            Notify(std::format("Reimported {}, {} failed (see the Console)", succeeded, failed), true);
        else
            Notify(std::format("Reimported {} asset{} (assets in use update when reloaded)", succeeded, succeeded == 1 ? "" : "s"));
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::CreateFolderIn(const Path& folder)
    {
        Path created;
        String error;
        if(!AssetFileOps::CreateFolder(folder, "New Folder", created, error))
        {
            Notify(error, true);
            return;
        }
        mSearchBuffer[0] = '\0';
        if(folder.lexically_normal() == mCurrentDirectory)
            SelectAfterRefresh({ created }, true);
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::CreateAssetIn(AssetType type, const Path& folder)
    {
        Path created;
        if(type == AssetType::UI_LAYOUT)
        {
            const AssetID layout = UIEditorPanel::CreateLayoutAsset(folder);
            if(layout)
                created = mBaseDirectory / mAssetManager->GetMetadata(layout).RelativePath;
        }
        else if(type == AssetType::PARTICLE_EFFECT)
        {
            const AssetID effect = ParticleEditorPanel::CreateEffectAsset(folder);
            if(effect)
                created = mBaseDirectory / mAssetManager->GetMetadata(effect).RelativePath;
        }
        else
        {
            const char* baseName = type == AssetType::SCENE ? "NewScene" : (type == AssetType::MATERIAL ? "NewMaterial" : "NewScript");
            const Path path = AssetFileOps::MakeUnique(folder / (String(baseName) + GetExtensionFromAssetType(type)));
            const String relative = AssetFileOps::ToRelative(path);

            if(type == AssetType::SCENE)
            {
                if(mAssetManager->Create<Scene>(relative, Filesystem::GetFilenameWithoutExt(path), Core::GetSceneContext()))
                    created = path;
            }
            else if(type == AssetType::MATERIAL)
            {
                if(Ref<Material> material = mAssetManager->Create<Material>(relative, *Core::GetRenderer(), path.stem().string()))
                {
                    MaterialSourceWriter::Write(material, path.generic_string());
                    created = path;
                }
            }
            else if(type == AssetType::SCRIPT)
            {
                ScriptSourceWriter::WriteNew(path.generic_string());
                Vector<Byte> bytecode = Core::GetScriptEngine()->Compile(ScriptSourceWriter::GetDefaultScriptContent(), path.filename().string());
                if(mAssetManager->Create<Script>(relative, std::move(bytecode)))
                    created = path;
            }
        }

        if(created.empty())
        {
            Notify(std::format("Could not create the {}", GetTypeLabel(type)), true);
            return;
        }
        mSearchBuffer[0] = '\0';
        if(folder.lexically_normal() == mCurrentDirectory)
            SelectAfterRefresh({ created }, true);
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::SelectAfterRefresh(Vector<Path> paths, bool rename)
    {
        mSelectAfterRefresh = std::move(paths);
        mRenameAfterRefresh = rename;
        mNeedsRefresh = true;
    }

    void ContentBrowserPanel::Notify(String message, bool isError)
    {
        if(isError)
            Log<Severity::Warn>("[ContentBrowser] {}", message);
        else
            Log<Severity::Info>("[ContentBrowser] {}", message);

        mStatusMessage = std::move(message);
        mStatusIsError = isError;
        mStatusTimer = 5.0f;
    }

} // namespace Surge
