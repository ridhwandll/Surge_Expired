// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ContentBrowserPanel.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include <SurgeReflect/Enum.hpp>
#include "Utility/ImGuiAux.hpp"
#include <Surge/ECS/Scene.hpp>
#include "Surge/Serializer/Serializer.hpp"
#include <Surge/Asset/Texture2D.hpp>
#include <Surge/Graphics/Renderer/Renderer.hpp>
#include "Surge/Core/Core.hpp"

namespace Surge
{
    void ContentBrowserPanel::Init(void* panelInitArgs)
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

        TextureLoadData loadData = Texture2D::LoadData("Editor/Assets/Textures/Folder.png");
        desc.Width = loadData.Width;
        desc.Height = loadData.Height;
        desc.DebugName = "EditorFolderIcon";
        desc.InitialData = loadData.Content;
        desc.DataSize = loadData.Width * loadData.Height * 4;
        mDirectoryIconHandle = rhi->CreateImage(desc);
        mDirectoryIconImGuiID = rhi->GetImGuiImage(mDirectoryIconHandle);
        Texture2D::FreeData(loadData);

        loadData = Texture2D::LoadData("Editor/Assets/Textures/File.png");
        desc.Width = loadData.Width;
        desc.Height = loadData.Height;
        desc.InitialData = loadData.Content;
        desc.DebugName = "EditorFileIcon";
        desc.DataSize = loadData.Width * loadData.Height * 4;
        mFileIconHandle = rhi->CreateImage(desc);
        mFileIconImGuiID = rhi->GetImGuiImage(mFileIconHandle);
        Texture2D::FreeData(loadData);
    }

    void ContentBrowserPanel::OnAssetManagerInit()
    {
        mBaseDirectory = AssetManager::GetAssetsDirectory();
        mCurrentDirectory = mBaseDirectory;
    }

    void ContentBrowserPanel::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);
        // TODO: Press spacebar to hide/show content browser like in Unreal Engine 5
        //dispatcher.Dispatch<KeyPressedEvent>([&](KeyPressedEvent& keyEvent) {});
    }

    void ContentBrowserPanel::Render(bool* show)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImVec4 accentColor = ImGuiAux::Colors::ThemeColor1;

        // CONTENT BROWSER
        if(*show)
        {
            if(ImGui::Begin("Content Browser", show))
            {
                // Navigation Bar
                if(mCurrentDirectory != mBaseDirectory)
                {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.16f, 1.0f));
                    if(ImGuiAux::Button(" BACK "))
                    {
                        mCurrentDirectory = mCurrentDirectory.parent_path();
                        mSelectedPath.clear();
                    }
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                }

                String relativePathStr = std::filesystem::relative(mCurrentDirectory, mBaseDirectory).string();
                if(relativePathStr == ".") relativePathStr = "Assets";
                else relativePathStr = "Assets/" + relativePathStr;

                ImGui::TextColored(ImVec4(0.50f, 0.50f, 0.55f, 1.0f), "%s", relativePathStr.c_str());
                ImGui::Dummy(ImVec2(0.0f, 5.0f));

                float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing() + 10.0f;

                if(ImGui::BeginChild("ContentGridArea", ImVec2(0, -footerHeightToReserve), false))
                {
                    static float padding = 16.0f;
                    float cellSize = mThumbnailSize + padding;
                    int columnCount = std::max(1, (int)(ImGui::GetContentRegionAvail().x / cellSize));

                    if(ImGui::BeginTable("ContentBrowserGrid", columnCount))
                    {
                        for(auto& directoryEntry : std::filesystem::directory_iterator(mCurrentDirectory))
                        {
                            const auto& path = directoryEntry.path();
                            String filenameString = path.filename().string();

                            ImGui::TableNextColumn();
                            ImGui::PushID(filenameString.c_str());

                            ImVec4 itemColor = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
                            bool isDirectory = directoryEntry.is_directory();
                            bool isRegisteredAsset = false;
                            AssetID assetId = UUID::INVALID;

                            if(isDirectory)
                                itemColor = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
                            else
                            {
                                String relativeToAssets = std::filesystem::relative(path, mBaseDirectory).generic_string();
                                assetId = AssetManager::GetIDFromPath(relativeToAssets);
                                if(assetId.IsValid())
                                {
                                    isRegisteredAsset = true;
                                    itemColor = ImGuiAux::Colors::LightGreen;
                                }
                            }

                            if(mSelectedPath == path)
                                itemColor = accentColor;

                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 0.0f)); // Transparent button bg
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(itemColor.x + 0.1f, itemColor.y + 0.1f, itemColor.z + 0.1f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(itemColor.x - 0.1f, itemColor.y - 0.1f, itemColor.z - 0.1f, 1.0f));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);

                            ImVec2 thumbnailPos = ImGui::GetCursorPos();
                            if(isDirectory)
                            {
                                if(ImGui::ImageButton("##Directory", mDirectoryIconImGuiID, ImVec2(mThumbnailSize, mThumbnailSize)))
                                {
                                    mCurrentDirectory /= path.filename();
                                    mSelectedPath.clear();
                                }
                            }
                            else
                            {
                                if(ImGui::ImageButton("##File", mFileIconImGuiID, ImVec2(mThumbnailSize, mThumbnailSize)))
                                    mSelectedPath = path;
                            }
                            // Drag Drop
                            if(isRegisteredAsset && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                            {
                                ImGui::SetDragDropPayload(CONTENT_BROWSER_PAYLOAD, &assetId, sizeof(AssetID));
                                ImGui::Text("%s - %llu", filenameString.c_str(), assetId.Get());
                                ImGui::EndDragDropSource();
                            }
                            ImVec2 afterThumbnailPos = ImGui::GetCursorPos();

                            // ASSET TYPE OVERLAY
                            if(isRegisteredAsset)
                            {
                                AssetMetadata meta = AssetManager::GetMetadata(assetId);
                                String typeStr = SurgeReflect::EnumToString(meta.Type).data();

                                ImGui::PushFont(boldFont);
                                float typeWidth = ImGui::CalcTextSize(typeStr.c_str()).x;

                                // Align perfectly to the top-right corner
                                ImGui::SetCursorPos(ImVec2(thumbnailPos.x + mThumbnailSize - typeWidth, thumbnailPos.y + 2.0f));

                                ImVec2 screenPos = ImGui::GetCursorScreenPos();
                                ImGui::GetWindowDrawList()->AddRectFilled(
                                    ImVec2(screenPos.x - 4.0f, screenPos.y - 2.0f),
                                    ImVec2(screenPos.x + typeWidth + 4.0f, screenPos.y + ImGui::GetTextLineHeight() + 2.0f),
                                    IM_COL32(10, 10, 10, 250),
                                    3.0f);

                                ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", typeStr.c_str());
                                ImGui::PopFont();

                                // Restore cursor to the bottom of the thumbnail so the DragDrop and Filename render properly
                                ImGui::SetCursorPos(afterThumbnailPos);
                            }
                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor(3);

                            // Context Menu for specific files
                            if(!isDirectory && !isRegisteredAsset)
                            {
                                if(ImGui::BeginPopupContextItem())
                                {
                                    if(ImGui::MenuItem("IMPORT Asset"))
                                    {
                                        String extension = path.extension().string();
                                        AssetType typeToImport = AssetType::NONE;
                                        if(extension == ".gltf" || extension == ".glb" || extension == ".fbx") typeToImport = AssetType::MESH;
                                        else if(extension == ".png" || extension == ".jpg" || extension == ".tga") typeToImport = AssetType::TEXTURE2D;
                                        else if(extension == ".srg") typeToImport = AssetType::SCENE;

                                        if(typeToImport != AssetType::NONE)
                                        {
                                            String relativeToAssets = std::filesystem::relative(path, mBaseDirectory).generic_string();
                                            AssetManager::Import(relativeToAssets, typeToImport);
                                        }
                                    }
                                    ImGui::EndPopup();
                                }
                            }

                            // Filename
                            float textWidth = ImGui::CalcTextSize(filenameString.c_str()).x;
                            float textOffset = (mThumbnailSize - textWidth) * 0.5f;
                            if(textOffset > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);

                            if(textWidth > mThumbnailSize)
                            {
                                ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + mThumbnailSize);
                                ImGui::TextWrapped("%s", filenameString.c_str());
                                ImGui::PopTextWrapPos();
                            }
                            else
                            {
                                ImGui::Text("%s", filenameString.c_str());
                            }

                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }

                    // CONTEXT MENU (CREATE ASSETS)
                    if(ImGui::BeginPopupContextWindow("ContentBrowserBackground", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                    {
                        if(ImGui::BeginMenu("Create"))
                        {
                            if(ImGui::MenuItem("Scene"))
                            {
                                std::filesystem::path newFilePath = mCurrentDirectory / "NewScene.srg";
                                int count = 1;
                                while(std::filesystem::exists(newFilePath))
                                {
                                    newFilePath = mCurrentDirectory / ("NewScene (" + std::to_string(count) + ").srg");
                                    count++;
                                }

                                Ref<Scene> newScene = Ref<Scene>::Create();
                                Serializer::SerializeScene(newFilePath.string(), newScene.Raw());
                                String relativeToAssets = std::filesystem::relative(newFilePath, mBaseDirectory).generic_string();
                                AssetID newId = AssetManager::ImportLive(relativeToAssets, AssetType::SCENE, newScene);

                                if(newId.IsValid())
                                    mSelectedPath = newFilePath;
                            }
                            if(ImGui::MenuItem("Material")) { Log<Severity::Warn>("[ContentBrowserPanel] TODO: Create new material asset"); }
                            if(ImGui::MenuItem("Physics Material")) { Log<Severity::Warn>("[ContentBrowserPanel] TODO: Create new physics material asset"); }

                            ImGui::EndMenu();
                        }
                        ImGui::EndPopup();
                    }
                }
                ImGui::EndChild();

                // Footer
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 2.0f));
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", mSelectedPath.string().c_str());

                float sliderWidth = 150.0f;
                ImGui::SameLine(ImGui::GetWindowWidth() - sliderWidth - ImGui::GetStyle().WindowPadding.x * 2.0f);
                ImGui::SetNextItemWidth(sliderWidth);
                ImGui::SliderFloat("##ThumbnailSize", &mThumbnailSize, 32.0f, 256.0f, "%.0f px");
            }
            ImGui::End();
        }

        // ASSET REGISTRY
        if(ImGui::Begin("Asset Registry"))
        {
            if(*show)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                if(ImGuiAux::Button("Save Registry to Disk"))
                    AssetManager::SerializeRegistry();

                ImGui::SameLine();

                // SEARCH BAR
                static char searchBuffer[256] = "";
                ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f - ImGui::GetStyle().WindowPadding.x);
                ImGui::SetNextItemWidth(250.0f);
                ImGui::InputTextWithHint("##RegistrySearch", "Search ID, Type, or Path...", searchBuffer, IM_ARRAYSIZE(searchBuffer));

                // Prepare search string
                String searchStr = searchBuffer;
                std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                bool hasSearch = !searchStr.empty();

                ImGui::PopStyleVar();
                ImGui::Dummy(ImVec2(0, 5.0f));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0, 5.0f));

                // REGISTRY DATA TABLE
                static ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_BordersInnerH;

                if(ImGui::BeginTable("RegistryTable", 5, flags))
                {
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableSetupColumn("Asset ID", ImGuiTableColumnFlags_WidthFixed, 180.0f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 120.0f);
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Relative Path", ImGuiTableColumnFlags_WidthStretch);
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableHeadersRow();

                    auto& registryMap = AssetManager::GetRegistryMap();
                    Vector<AssetID> toDelete;
                    Vector<AssetID> toUnload;

                    for(const auto& [id, meta] : registryMap)
                    {
                        // APPLY SEARCH FILTER
                        if(hasSearch)
                        {
                            String idStr = std::to_string(id.Get());
                            String typeStr = SurgeReflect::EnumToString(meta.Type).data();
                            String pathStr = meta.RelativePath;

                            std::transform(typeStr.begin(), typeStr.end(), typeStr.begin(), tolower);
                            std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), tolower);

                            // If the search string isn't found in ID, Type, OR Path, skip this row
                            if(idStr.find(searchStr) == std::string::npos && typeStr.find(searchStr) == std::string::npos && pathStr.find(searchStr) == std::string::npos)
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
                        ImGui::PushFont(boldFont, 16.0f);
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
                        ImGui::TextWrapped("%s", meta.RelativePath.c_str());

                        // Column 4: Actions
                        ImGui::TableSetColumnIndex(4);
                        ImGui::PushFont(boldFont, 16.0f);
                        if(HasFlag(meta.Flags, AssetFlags::MEMORY))
                        {
                            ImGui::BeginDisabled();
                            ImGuiAux::Button("BUILT-IN");
                            ImGui::EndDisabled();
                        }
                        else if(HasFlag(meta.Flags, AssetFlags::LOADED))
                        {
                            size_t refCount = AssetManager::GetAssetRefCount(id);

                            if(refCount > 1)
                            {
                                ImGui::BeginDisabled();
                                ImGuiAux::Button(String("REFERENCES: " + std::to_string(refCount - 1)).c_str());
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
                                    toUnload.push_back(id);

                                ImGui::PopStyleColor(4);

                                if(ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Free this asset from GPU/CPU memory");
                            }
                        }
                        else
                        {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

                            if(ImGuiAux::Button("UNREGISTER"))
                                toDelete.push_back(id);

                            ImGui::PopStyleColor(4);
                        }
                        ImGui::PopFont();
                        ImGui::PopID();
                    }

                    for(const AssetID& id : toUnload)
                        AssetManager::Unload(id);
                    for(const AssetID& id : toDelete)
                        AssetManager::UnregisterAsset(id);

                    ImGui::EndTable();
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

    void ContentBrowserPanel::Shutdown()
    {
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        rhi->DestroyImage(mDirectoryIconHandle);
        rhi->DestroyImage(mFileIconHandle);
    }

} // namespace Surge