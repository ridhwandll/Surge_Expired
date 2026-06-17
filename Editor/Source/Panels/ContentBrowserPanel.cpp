// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ContentBrowserPanel.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "SurgeReflect/Enum.hpp"

#include "Editor.hpp"
#include "MaterialEditorPanel.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Asset/SourceWriters/MaterialSourceWriter.hpp"
#include <stb_image.h>
#include <Surge/ScriptEngine/ScriptAsset.hpp>
#include "Asset/SourceWriters/ScriptSourceWriter.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"

namespace Surge
{
    struct CBImageLoadData
    {
        Byte* Content = nullptr;
        Uint Width = 0;
        Uint Height = 0;
        Uint Channels = 0;
        String DebugName;
    };

    static CBImageLoadData LoadIcon(const String& path)
    {
        int width = 0, height = 0, channels = 0;
        stbi_uc* data = nullptr;
        data = stbi_load(path.c_str(), &width, &height, &channels, 4);
        if(!data)
        {
            Log<Severity::Error>("Failed to load texture at path: {0}", path);
            return CBImageLoadData {};
        }

        CBImageLoadData loadData;
        loadData.Content = data;
        loadData.Width = width;
        loadData.Height = height;
        loadData.Channels = channels;
        return loadData;
    }

    static void FreeIcon(CBImageLoadData& data)
    {
        stbi_image_free(data.Content);
        data.Content = nullptr;
        data.Width = 0;
        data.Height = 0;
        data.Channels = 0;
    }

    void ContentBrowserPanel::Init(void*)
    {
        mCode = GetStaticCode();
        OnAssetManagerInit();

        Renderer* renderer = Core::GetRenderer();
        Scope<GraphicsRHI>& rhi = renderer->GetRHI();

        ImageDesc desc = {};
        desc.Format = ImageFormat::RGBA8_UNORM;
        desc.Usage = ImageUsage::SAMPLED | ImageUsage::TRANSFER_DST;
        desc.GenerateImGuiID = true;
        desc.Sampler = renderer->GetDefaultSampler();

        CBImageLoadData loadData = LoadIcon("Editor/Assets/Textures/Folder.png");
        desc.Width = loadData.Width;
        desc.Height = loadData.Height;
        desc.DebugName = "EditorFolderIcon";
        desc.InitialData = loadData.Content;
        desc.DataSize = loadData.Width * loadData.Height * 4;
        mDirectoryIconHandle = rhi->CreateImage(desc);
        mDirectoryIconImGuiID = rhi->GetImGuiImage(mDirectoryIconHandle);
        FreeIcon(loadData);

        loadData = LoadIcon("Editor/Assets/Textures/File.png");
        desc.Width = loadData.Width;
        desc.Height = loadData.Height;
        desc.InitialData = loadData.Content;
        desc.DebugName = "EditorFileIcon";
        desc.DataSize = loadData.Width * loadData.Height * 4;
        mFileIconHandle = rhi->CreateImage(desc);
        mFileIconImGuiID = rhi->GetImGuiImage(mFileIconHandle);
        FreeIcon(loadData);

        loadData = LoadIcon("Editor/Assets/Textures/EmptyFolder.png");
        desc.Width = loadData.Width;
        desc.Height = loadData.Height;
        desc.InitialData = loadData.Content;
        desc.DebugName = "EditorEmptyFolderIcon";
        desc.DataSize = loadData.Width * loadData.Height * 4;
        mEmptyDirectoryIconHandle = rhi->CreateImage(desc);
        mEmptyDirectoryIconImGuiID = rhi->GetImGuiImage(mEmptyDirectoryIconHandle);
        FreeIcon(loadData);

        mAssetManager = Core::GetAssetManager();
    }

    void ContentBrowserPanel::OnAssetManagerInit()
    {
        mBaseDirectory = Core::GetAssetManager()->GetAssetsDirectory();
        mCurrentDirectory = mBaseDirectory;
        RefreshDirectoryCache();
    }

    void ContentBrowserPanel::RefreshDirectoryCache()
    {
        mCurrentDirectoryItems.clear();
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

        for(auto& directoryEntry : std::filesystem::directory_iterator(mCurrentDirectory))
        {
            ContentBrowserItem item;
            item.Path_ = directoryEntry.path();
            item.Filename = item.Path_.filename().string();

            if (item.Filename == "AssetRegistry.surge" || item.Filename == "Internal")
                continue;

            item.IsDirectory = directoryEntry.is_directory();
            item.IsDirectoryEmpty = directoryEntry.is_directory() && std::filesystem::is_empty(directoryEntry.path());
            item.IsRegisteredAsset = false;

            if(!item.IsDirectory)
            {
                String relativeToAssets = std::filesystem::relative(item.Path_, mBaseDirectory).generic_string();
                item.Id = mAssetManager->GetIDFromPath(relativeToAssets);

                if(item.Id.IsValid())
                {
                    item.IsRegisteredAsset = true;
                    AssetMetadata meta = mAssetManager->GetMetadata(item.Id);

                    if(meta.Type == AssetType::TEXTURE2D && meta.IsLoaded())
                        item.ThumbnailImGuiID = rhi->GetImGuiImage(mAssetManager->Load<Texture2D>(item.Id)->GetRHIImage());
                    else
                        item.ThumbnailImGuiID = NULL;

                    item.AssetTypeStr = SurgeReflect::EnumToString(meta.Type).data();
                }
            }
            mCurrentDirectoryItems.push_back(item);
        }

        // Sort directories first, then alphabetically
        std::sort(mCurrentDirectoryItems.begin(), mCurrentDirectoryItems.end(),
                  [](const auto& a, const auto& b)
                  {
                      // Folders come first
                      if(a.IsDirectory != b.IsDirectory)
                          return a.IsDirectory > b.IsDirectory;

                      // Registered assets come before raw/unregistered files
                      if(a.IsRegisteredAsset != b.IsRegisteredAsset)
                          return a.IsRegisteredAsset > b.IsRegisteredAsset;

                      // If they are the same type (both folders, both registered, or both raw), sort alphabetically
                      return a.Filename < b.Filename;
                  });

        mNeedsCacheRefresh = false;
    }

    void ContentBrowserPanel::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        // TODO: Press spacebar to hide/show content browser like in Unreal Engine 5
        dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) {
            if(keyEvent.GetKeyCode() == Key::F2)
                StartRename(mSelectedPath);
                                             });
    }

    void ContentBrowserPanel::Render(bool* show)
    {
        // CONTENT BROWSER
        if(*show)
        {
            ImFont* regularFont = ImGui::GetIO().Fonts->Fonts[0];
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];

            if(ImGui::Begin("Content Browser", show))
            {
                constexpr float autoRefreshInterval = 15.0f; //Seconds
                mCacheRefreshTimer += ImGui::GetIO().DeltaTime;
                if(mCacheRefreshTimer > autoRefreshInterval)
                {
                    mNeedsCacheRefresh = true;
                    mCacheRefreshTimer = 0.0f;
                }

                // Navigation Bar
                if(mCurrentDirectory != mBaseDirectory)
                {
                    if(ImGuiAux::Button(" BACK "))
                    {
                        mCurrentDirectory = mCurrentDirectory.parent_path();
                        mSelectedPath.clear();
                        mNeedsCacheRefresh = true;
                        mIsRenaming = false;
                        ClearContentBrowserSearchBuffer();
                    }
                    ImGui::SameLine();
                }

                String relativePathStr = std::filesystem::relative(mCurrentDirectory, mBaseDirectory).generic_string();
                if(relativePathStr == ".")
                    relativePathStr = "Assets";
                else
                    relativePathStr = "Assets/" + relativePathStr;

                ImGui::PushFont(boldFont, 18.0f);
                ImGui::Text("%s", relativePathStr.c_str());
                ImGui::PopFont();

                // CONTENT SEARCH BAR
                float searchBarWidth = 300.0f;
                ImGui::SameLine(ImGui::GetWindowWidth() - searchBarWidth - ImGui::GetStyle().WindowPadding.x);
                ImGui::SetNextItemWidth(searchBarWidth);
                ImGui::InputTextWithHint("##ContentSearch", "Search items in current directory...", mContentBrowserSearchBuffer, IM_ARRAYSIZE(mContentBrowserSearchBuffer));

                String contentSearchStr = mContentBrowserSearchBuffer;
                std::transform(contentSearchStr.begin(), contentSearchStr.end(), contentSearchStr.begin(), ::tolower);
                bool hasContentSearch = !contentSearchStr.empty();

                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 10.0f;

                if(ImGui::BeginChild("ContentGridArea", ImVec2(0, -footerHeightToReserve), false))
                {
                    constexpr float padding = 16.0f;
                    float cellSize = mThumbnailSize + padding;
                    int columnCount = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellSize));

                    // Build Filtered List for Clipper
                    mItemsToDisplay.clear();
                    mItemsToDisplay.reserve(mCurrentDirectoryItems.size());

                    for(auto& item : mCurrentDirectoryItems)
                    {
                        if(mShowOnlyRegisteredAssets && !item.IsDirectory && !item.IsRegisteredAsset)
                            continue;

                        if(hasContentSearch)
                        {
                            String filenameLower = item.Filename;
                            std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
                            if(filenameLower.find(contentSearchStr) != std::string::npos)
                                mItemsToDisplay.push_back(&item);
                        }
                        else
                            mItemsToDisplay.push_back(&item);
                    }

                    if(ImGui::BeginTable("ContentBrowserGrid", columnCount))
                    {
                        int rowCount = (int)std::ceil((float)mItemsToDisplay.size() / (float)columnCount);

                        ImGuiListClipper clipper;
                        clipper.Begin(rowCount);
                        while(clipper.Step())
                        {
                            for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                            {
                                for(int col = 0; col < columnCount; ++col)
                                {
                                    int itemIndex = row * columnCount + col;
                                    if(itemIndex >= (int)mItemsToDisplay.size())
                                        break;

                                    auto& item = *mItemsToDisplay[itemIndex];

                                    ImGui::TableNextColumn();
                                    ImGui::PushID(item.Filename.c_str());

                                    ImVec4 itemColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                                    if(item.IsDirectory)
                                        itemColor = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                                    else if(item.IsRegisteredAsset)
                                        itemColor = ImGuiAux::Colors::LightGreen;

                                    bool isSelected = (mSelectedPath == item.Path_);

                                    ImGui::PushStyleColor(ImGuiCol_Button, isSelected ? ImVec4(ImGuiAux::Colors::Nickel) : ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(itemColor.x + 0.1f, itemColor.y + 0.1f, itemColor.z + 0.1f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(itemColor.x - 0.1f, itemColor.y - 0.1f, itemColor.z - 0.1f, 1.0f));
                                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

                                    ImTextureID iconID = mFileIconImGuiID;
                                    if(item.IsDirectory)
                                        iconID = item.IsDirectoryEmpty ? mEmptyDirectoryIconImGuiID : mDirectoryIconImGuiID;
                                    else if(item.IsRegisteredAsset && item.ThumbnailImGuiID != NULL)
                                        iconID = item.ThumbnailImGuiID;

                                    if(ImGui::ImageButton("##Icon", iconID, ImVec2(mThumbnailSize, mThumbnailSize)))
                                        mSelectedPath = item.Path_;

                                    ImVec2 btnMin = ImGui::GetItemRectMin();
                                    ImVec2 btnMax = ImGui::GetItemRectMax();

                                    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                                    {
                                        if(item.IsDirectory)
                                        {
                                            mCurrentDirectory /= item.Filename;
                                            mSelectedPath.clear();
                                            mNeedsCacheRefresh = true;
                                            mIsRenaming = false;
                                            ClearContentBrowserSearchBuffer();
                                        }
                                        else if(item.IsRegisteredAsset)
                                        {
                                            if (item.AssetTypeStr == "MATERIAL")
                                            {
                                                Editor* editor = (Editor*)Core::GetClient();
                                                editor->GetPanelManager().GetPanel<MaterialEditorPanel>()->SetSelectedMaterial(mAssetManager->Load<Material>(item.Id));
                                            }
                                            else // TODO: Double clicking an asset should open it in the Inspector
                                                Log<Severity::Info>("[ContentBrowser] Double-clicked asset: {}", item.Filename);
                                        }
                                    }

                                    // Drag Drop
                                    if(item.IsRegisteredAsset && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                                    {
                                        ImGui::SetDragDropPayload(CONTENT_BROWSER_PAYLOAD, &item.Id, sizeof(AssetID));
                                        ImGui::Text("%s - %llu", item.Filename.c_str(), item.Id.Get());
                                        ImGui::EndDragDropSource();
                                    }

                                    // ASSET TYPE OVERLAY
                                    const char* overlayText = nullptr;
                                    if(item.IsRegisteredAsset)
                                        overlayText = item.AssetTypeStr.c_str();
                                    else if(item.IsDirectory)
                                        overlayText = "FOLDER";

                                    if(overlayText)
                                    {
                                        ImGui::PushFont(boldFont);
                                        constexpr float bannerPadding = 3.0f;
                                        float typeWidth = ImGui::CalcTextSize(overlayText).x;
                                        float actualLineHeight = ImGui::GetTextLineHeight();
                                        float textHeight = actualLineHeight * 1.5f;
                                        float bannerHeight = textHeight + (bannerPadding * 2.0f);
                                        float startY = btnMax.y - bannerHeight;

                                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                                        drawList->AddRectFilled(
                                            ImVec2(btnMin.x, startY), btnMax,
                                            ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ExtraDark),
                                            7.0f, ImDrawFlags_RoundCornersBottom);

                                        float textX = btnMin.x + ((btnMax.x - btnMin.x) - typeWidth) * 0.5f;
                                        float textY = startY + bannerPadding + (textHeight - actualLineHeight) * 0.5f;
                                        drawList->AddText(ImVec2(textX, textY), ImGui::ColorConvertFloat4ToU32(ImGuiAux::Colors::ThemeColor1), overlayText);
                                        ImGui::PopFont();
                                    }
                                    ImGui::PopStyleVar();
                                    ImGui::PopStyleColor(3);

                                    // Context Menu for specific files
                                    if(ImGui::BeginPopupContextItem())
                                    {
                                        // IMPORT
                                        if(!item.IsDirectory && !item.IsRegisteredAsset)
                                        {
                                            if(ImGui::MenuItem("IMPORT"))
                                            {
                                                String extension = item.Path_.extension().string();
                                                AssetType typeToImport = AssetTypeFromExtension(extension.c_str());
                                                if(typeToImport != AssetType::NONE)
                                                {
                                                    String relativeToAssets =  Filesystem::GetRelativePath(item.Path_, mBaseDirectory).generic_string();
                                                    mAssetManager->Import(relativeToAssets, typeToImport);
                                                    mNeedsCacheRefresh = true;
                                                }
                                            }
                                        }
                                        if(ImGui::MenuItem("Rename"))
                                            StartRename(item.Path_);

                                        ImGui::EndPopup();
                                    }

                                    ImGui::PushFont(regularFont, 15.0f);

                                    // RENAME INPUT
                                    if(mIsRenaming && mRenamingPath == item.Path_)
                                    {
                                        ImGui::PushItemWidth(mThumbnailSize);
                                        if(mFocusRenameInput)
                                        {
                                            ImGui::SetKeyboardFocusHere();
                                            mFocusRenameInput = false;
                                        }
                                        // Execute rename on Enter key
                                        if(ImGui::InputText("##Rename", mRenameBuffer, IM_ARRAYSIZE(mRenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                                        {
                                            std::filesystem::path newPath = mCurrentDirectory / mRenameBuffer;
                                            if(newPath != item.Path_ && !std::filesystem::exists(newPath))
                                            {
                                                std::filesystem::rename(item.Path_, newPath);

                                                if(item.IsRegisteredAsset)
                                                {
                                                    String newRelativePath = std::filesystem::relative(newPath, mBaseDirectory).generic_string();
                                                    mAssetManager->UpdateAssetPath(item.Id, newRelativePath);

                                                    if (item.AssetTypeStr == "MATERIAL")
                                                    {
                                                        Ref<Material> newMaterial = mAssetManager->Load<Material>(item.Id);
                                                        newMaterial->SetName(Filesystem::GetFilenameWithoutExt(mRenameBuffer));
                                                        MaterialSourceWriter::Write(newMaterial, newPath.generic_string());
                                                        mAssetManager->Save(item.Id);
                                                        mAssetManager->Unload(item.Id);
                                                    }
                                                    else if (item.AssetTypeStr == "SCRIPT")
                                                    {
                                                        mAssetManager->Save(item.Id);
                                                        mAssetManager->Unload(item.Id);
                                                    }
                                                }
                                                mSelectedPath = newPath;
                                                mNeedsCacheRefresh = true;
                                            }
                                            else
                                                Log<Severity::Warn>("Cannot rename to {0}: File already exists or invalid name", newPath.string());

                                            mIsRenaming = false;
                                        }

                                        // Cancel rename if the user clicks anywhere else
                                        if(!ImGui::IsItemActive() && !mFocusRenameInput && (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(1)))
                                            mIsRenaming = false;

                                        ImGui::PopItemWidth();
                                    }
                                    else // FILENAME
                                    {
                                        float textWidth = ImGui::CalcTextSize(item.Filename.c_str()).x;
                                        if(textWidth > mThumbnailSize)
                                        {
                                            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + mThumbnailSize);
                                            ImGui::TextWrapped("%s", item.Filename.c_str());
                                            ImGui::PopTextWrapPos();
                                        }
                                        else
                                        {
                                            float textOffset = (mThumbnailSize - textWidth) * 0.5f;
                                            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
                                            ImGui::Text("%s", item.Filename.c_str());
                                        }
                                    }

                                    ImGui::PopFont();
                                    ImGui::PopID();
                                }
                            }
                        }
                        clipper.End();
                        ImGui::EndTable();
                    }

                    // CONTEXT MENU (CREATE ASSETS)
                    if(ImGui::BeginPopupContextWindow("ContentBrowserBackground", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                    {
                        if(ImGui::BeginMenu("Create"))
                        {
                            if(ImGui::MenuItem("Scene"))
                            {
                                const char* extension = GetExtensionFromAssetType(AssetType::SCENE);
                                std::filesystem::path newFilePath = mCurrentDirectory / ("NewScene" + String(extension));
                                int count = 1;
                                while(std::filesystem::exists(newFilePath))
                                {
                                    newFilePath = mCurrentDirectory / ("NewScene (" + std::to_string(count) + ")" + extension);
                                    count++;
                                }
                                String relativeToAssets = std::filesystem::relative(newFilePath, mBaseDirectory).generic_string();
                                Ref<Scene> newScene = mAssetManager->Create<Scene>(relativeToAssets);
                                if(newScene)
                                {
                                    mSelectedPath = newFilePath;
                                    StartRename(newFilePath);
                                }

                                mNeedsCacheRefresh = true;
                            }
                            if(ImGui::MenuItem("Material"))
                            {
                                const char* extension = GetExtensionFromAssetType(AssetType::MATERIAL);
                                std::filesystem::path newFilePath = mCurrentDirectory / ("NewMaterial" + String(extension));
                                int count = 1;
                                while(std::filesystem::exists(newFilePath))
                                {
                                    newFilePath = mCurrentDirectory / ("NewMaterial (" + std::to_string(count) + ")" + extension);
                                    count++;
                                }
                                String relativeToAssets = std::filesystem::relative(newFilePath, mBaseDirectory).generic_string();
                                Ref<Material> newMaterial = mAssetManager->Create<Material>(relativeToAssets, "New Material");
                                if(newMaterial)
                                {
                                    MaterialSourceWriter::Write(newMaterial, newFilePath.generic_string());
                                    mSelectedPath = newFilePath;
                                    StartRename(newFilePath);
                                }
                                mNeedsCacheRefresh = true;
                            }
                            if(ImGui::MenuItem("Script"))
                            {
                                const char* extension = GetExtensionFromAssetType(AssetType::SCRIPT);
                                std::filesystem::path newFilePath = mCurrentDirectory / ("NewScript" + String(extension));
                                int count = 1;
                                while(std::filesystem::exists(newFilePath))
                                {
                                    newFilePath = mCurrentDirectory / ("NewScript (" + std::to_string(count) + ")" + extension);
                                    count++;
                                }
                                String relativeToAssets = std::filesystem::relative(newFilePath, mBaseDirectory).generic_string();
                                ScriptSourceWriter::WriteNew(newFilePath.generic_string());
                                ScriptEngine* scriptEngine = Core::GetScriptEngine();
                                Vector<Byte> bytecode = scriptEngine->Compile(ScriptSourceWriter::GetDefaultScriptContent());
                                Ref<Script> newScript = mAssetManager->Create<Script>(relativeToAssets, std::move(bytecode));

                                mSelectedPath = newFilePath;
                                StartRename(newFilePath);
                                
                                mNeedsCacheRefresh = true;
                            }
                            if(ImGui::MenuItem("Physics Material")) { Log<Severity::Warn>("[ContentBrowserPanel] TODO: Create new physics material asset"); }

                            ImGui::EndMenu();
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChild();

                //// Footer
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 2.0f));

                // Left Side: Selected Path
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", mSelectedPath.string().c_str());

                // Right Side: Width Calculations for Perfect Alignment
                constexpr float sliderWidth = 150.0f;
                float refreshButtonWidth = ImGui::CalcTextSize("Refresh").x + ImGui::GetStyle().FramePadding.x * 2.0f;

                float checkboxWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x;
                float spacing = ImGui::GetStyle().ItemSpacing.x;
                float totalRightWidth = sliderWidth + refreshButtonWidth + checkboxWidth + (spacing * 2.0f);

                ImGui::SameLine(ImGui::GetWindowWidth() - totalRightWidth - ImGui::GetStyle().WindowPadding.x);

                ImGui::Checkbox("##RegisteredOnly", &mShowOnlyRegisteredAssets);
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Show only Registered assets");

                ImGui::SameLine();
                if(ImGuiAux::Button("Refresh"))
                    mNeedsCacheRefresh = true;

                ImGui::SameLine();
                ImGui::SetNextItemWidth(sliderWidth);
                ImGui::SliderFloat("##ThumbnailSize", &mThumbnailSize, 32.0f, 256.0f, "%.0f px");
            }
            ImGui::End();
        }

        RenderAssetRegistry(show);

        if(mNeedsCacheRefresh)
            RefreshDirectoryCache();
    }

    void ContentBrowserPanel::RenderAssetRegistry(bool* show)
    {
        // ASSET REGISTRY
        if(ImGui::Begin("Asset Registry"))
        {
            ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
            if(*show)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

                ImGui::PushFont(boldFont);
                if(ImGuiAux::Button("SERIALIZE TO DISK"))
                    mAssetManager->SerializeRegistry();
                ImGui::SameLine();

                if(ImGuiAux::Button("UNLOAD UNUSED"))
                {
                    const auto& currentRegistry = mAssetManager->GetRegistryMap();
                    for(const auto& [id, meta] : currentRegistry)
                    {
                        if(HasFlag(meta.Flags, AssetFlags::LOADED) && !HasFlag(meta.Flags, AssetFlags::MEMORY))
                        {
                            // If the AssetManager is the ONLY thing holding a reference (refCount <= 1)
                            if(mAssetManager->GetAssetRefCount(id) <= 1)
                                mToUnload.push_back(id);
                        }
                    }
                }
                if(ImGui::IsItemHovered())
                    ImGui::SetTooltip("Frees ALL LOADED assets that are NOT actively referenced in the scene");
                ImGui::SameLine();
                ImGui::PopFont();

                if(mAssetRegistrySearchBuffer[0] != '\0')
                {
                    ImGui::PushFont(boldFont);
                    ImGui::TextColored(ImGuiAux::Colors::Gold, "SEARCH FILTER IS ACTIVE");
                    ImGui::PopFont();
                    ImGui::SameLine();
                }
                const auto& registryMap = mAssetManager->GetRegistryMap();

                float searchBarWidth = 250.0f;
                float comboWidth = 140.0f;
                float spacing = ImGui::GetStyle().ItemSpacing.x;
                float totalRightWidth = searchBarWidth + comboWidth + spacing;

                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - totalRightWidth);

                // FILTER COMBO BOX
                ImGui::SetNextItemWidth(comboWidth);
                String currentComboPreview = mSelectedFilterType == AssetType::NONE ? "ALL TYPES" : SurgeReflect::EnumToString(mSelectedFilterType).data();
                if(ImGui::BeginCombo("##TypeFilter", currentComboPreview.c_str()))
                {
                    if(ImGui::Selectable("ALL TYPES", mSelectedFilterType == AssetType::NONE))
                        mSelectedFilterType = AssetType::NONE;

                    for(AssetType type : sAssetTypeArray)
                    {
                        bool isSelected = (mSelectedFilterType == type);
                        if(ImGui::Selectable(SurgeReflect::EnumToString(type).data(), isSelected))
                            mSelectedFilterType = type;

                        if(isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::SameLine();

                // ASSET REGISTRY SEARCH BAR 
                ImGui::SetNextItemWidth(searchBarWidth);
                ImGui::InputTextWithHint("##RegistrySearch", "Search ID, Type, or Path...", mAssetRegistrySearchBuffer, IM_ARRAYSIZE(mAssetRegistrySearchBuffer));

                String searchStr = mAssetRegistrySearchBuffer;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                ImGui::PopStyleVar(); // Frame rounding

                ImGui::Dummy(ImVec2(0, 5.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 5.0f));

                // REGISTRY DATA TABLE
                if(ImGui::BeginTable("RegistryTable", 5, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Asset ID", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Relative Path", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableHeadersRow();

                    for(const auto& entry : registryMap)
                    {
                        const AssetID& id = entry.first;
                        const AssetMetadata& meta = entry.second;

                        if (meta.Type != mSelectedFilterType && mSelectedFilterType != AssetType::NONE)
                            continue;

                        if (!searchStr.empty())
                        {
                            String idStr = std::to_string(id.Get());
                            String typeStr = SurgeReflect::EnumToString(meta.Type).data();
                            String pathStr = meta.RelativePath;

                            std::transform(idStr.begin(), idStr.end(), idStr.begin(), ::tolower);
                            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), ::tolower);
                            std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);

                            bool matches = false;
                            if (idStr.find(searchStr) != String::npos || typeStr.find(searchStr) != String::npos || pathStr.find(searchStr) != String::npos)
                                matches = true;

                            if (!matches)
                                continue;
                        }

                        ImGui::PushID(id.Get());
                        ImGui::TableNextRow();

                        // Column 0: Asset ID
                        ImGui::TableSetColumnIndex(0);
                        String idStr = std::to_string(id.Get());
                        if(ImGui::Selectable(idStr.c_str()))
                            ImGui::SetClipboardText(idStr.c_str());

                        // Column 1: Asset Type
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", SurgeReflect::EnumToString(meta.Type).data());

                        // Column 2: Memory/Disk Status
                        ImGui::TableSetColumnIndex(2);
                        ImGui::PushFont(boldFont);
                        if(HasFlag(meta.Flags, AssetFlags::MISSING))
                            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "MISSING");
                        else if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                            ImGui::TextColored(ImVec4(0.8f, 0.5f, 0.2f, 1.0f), "MEMORY");
                        else if(HasFlag(meta.Flags, AssetFlags::LOADED))
                            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "LOADED");
                        else
                            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "VALID");
                        ImGui::PopFont();

                        // Column 3: Relative Path
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%s", meta.RelativePath.c_str());

                        // Column 4: Actions
                        ImGui::TableSetColumnIndex(4);
                        ImGui::PushFont(boldFont);
                        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                        {
                            ImGui::BeginDisabled();
                            ImGuiAux::Button("BUILT-IN");
                            ImGui::EndDisabled();
                        }
                        else if(HasFlag(meta.Flags, AssetFlags::LOADED))
                        {
                            size_t refCount = mAssetManager->GetAssetRefCount(id);

                            if(refCount > 1)
                            {
                                ImGui::BeginDisabled();
                                ImGuiAux::Button(String("REFS: " + std::to_string(refCount - 1)).c_str());
                                ImGui::EndDisabled();

                                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                    ImGui::SetTooltip("Asset is actively referenced by scene components. Cannot unload");
                            }
                            else
                            {
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.6f, 0.3f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.7f, 0.4f, 0.1f, 1.0f));
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

                                if(ImGuiAux::Button("UNLOAD"))
                                    mToUnload.push_back(id);

                                ImGui::PopStyleColor(4);

                                if(ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Free this asset from GPU/CPU memory");
                            }
                        }
                        else
                        {
                            bool isMissing = HasFlag(meta.Flags, AssetFlags::MISSING);
                            ImGui::PushStyleColor(ImGuiCol_Button, isMissing ? ImVec4(0.8f, 0.2f, 0.2f, 1.0f) : ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

                            if(ImGuiAux::Button("UNREGISTER"))
                                mToUnregister.push_back(id);

                            ImGui::PopStyleColor(4);
                        }
                        ImGui::PopFont();
                        ImGui::PopID();
                    }

                    for(const AssetID& id : mToUnload) { mAssetManager->Unload(id); }
                    for(const AssetID& id : mToUnregister) { mAssetManager->UnregisterAsset(id); }

                    ImGui::EndTable();

                    if(!mToUnload.empty())
                        mToUnload.clear();
                    if(!mToUnregister.empty())
                        mToUnregister.clear();
                }
            }
            else
            {
                ImGui::PushFont(boldFont, 20.0f);
                ImGuiAux::TextCentered("Open Content Browser Panel to view the Asset Registry");
                ImGui::PopFont();
            }
        }
        ImGui::End();
    }

    void ContentBrowserPanel::StartRename(const Path& path)
    {
        // We don't allow renaming of directories in this implementation, 
        // but this can be added in the future if needed. It just requires updating all child paths in the registry when a directory is renamed
        if (std::filesystem::is_directory(path))
            return;

        mRenamingPath = path;
        strncpy(mRenameBuffer, path.filename().string().c_str(), IM_ARRAYSIZE(mRenameBuffer));
        mIsRenaming = true;
        mFocusRenameInput = true;
    }

    void ContentBrowserPanel::Shutdown()
    {
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        rhi->DestroyImage(mDirectoryIconHandle);
        rhi->DestroyImage(mEmptyDirectoryIconHandle);
        rhi->DestroyImage(mFileIconHandle);
    }

} // namespace Surge