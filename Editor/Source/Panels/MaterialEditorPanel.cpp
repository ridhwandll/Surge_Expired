// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/MaterialEditorPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "ContentBrowserPanel.hpp"

#include <imgui.h>
#include <string>
#include "Utility/ImGuiAux.hpp"

namespace Surge
{
    static void DrawTextureProperty(const char* label, Material* material, const char* mapUseName, const char* textureName)
    {
        bool useMap = material->Get<int>(mapUseName);

        Ref<Texture2D> tex = material->GetTexture(textureName);
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        uint64_t imageSize = 0;
        ImageHandle handle = ImageHandle::Invalid();
        if(tex)
        {
            handle = tex->GetRHIImage();
            imageSize = rhi->GetImageSize(handle);
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        {
            ImGuiAux::ScopedBoldFont boldFont(18.0f);
            ImGui::TextUnformatted(label);
        }
        if(tex)
            ImGui::Text("Size: %.5f MB", static_cast<float>(imageSize) / (1024.0f * 1024.0f));
        ImGui::TableSetColumnIndex(1);

        ImGui::PushID(label);
        if(ImGui::Checkbox("##Use", &useMap))
            material->Set<int>(mapUseName, useMap);

        ImGui::SameLine();

        bool textureDropped = false;
        AssetID droppedAssetID = 0;
        const float thumbnailSize = 67.0f;

        if(tex)
        {
            ImTextureID texID = rhi->GetImGuiImage(handle);
            ImGui::Image(texID, ImVec2(thumbnailSize, thumbnailSize));
        }
        else
        {
            if(useMap) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

            ImGuiAux::ScopedBoldFont font(21.0f);
            ImGui::Button("Drop Texture2D Here", ImVec2(ImGui::GetContentRegionAvail().x, thumbnailSize));

            if(useMap) ImGui::PopStyleColor();
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                droppedAssetID = *(const AssetID*)payload->Data;
                textureDropped = true;
            }
            ImGui::EndDragDropTarget();
        }
        if(textureDropped)
        {
            AssetMetadata meta = Core::GetAssetManager()->GetMetadata(droppedAssetID);
            if(meta.Type == AssetType::TEXTURE2D)
            {
                Ref<Texture2D> newTex = Core::GetAssetManager()->Load<Texture2D>(droppedAssetID);
                material->SetTexture(textureName, newTex);
                material->Set<int>(mapUseName, 1);
            }
        }

        if(tex)
        {
            ImGui::SameLine();
            ImGui::BeginGroup();
            ImGui::TextDisabled("ID: %llu", tex->GetID().Get());
            if(ImGui::Button("CLEAR"))
            {
                material->SetTexture(textureName, nullptr);
            }
            ImGui::EndGroup();
        }

        ImGui::PopID();
    }

    void MaterialEditorPanel::Init(void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mSelectedMaterial = nullptr;
    }

    void MaterialEditorPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGui::Begin("Material Editor", show);

        auto AcceptMaterialDrop = [&]() {
            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    SG_ASSERT(payload->DataSize == sizeof(AssetID), "Payload size mismatch!");
                    AssetID droppedAssetID = *(const AssetID*)payload->Data;
                    AssetMetadata meta = Core::GetAssetManager()->GetMetadata(droppedAssetID);

                    if(meta.Type == AssetType::MATERIAL)
                        mSelectedMaterial = Core::GetAssetManager()->Load<Material>(droppedAssetID);
                }
                ImGui::EndDragDropTarget();
            }
            };

        // EMPTY STATE (Drop Zone)
        if(!mSelectedMaterial)
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

            if(ImGui::BeginChild("EmptyDropZone", ImGui::GetContentRegionAvail(), true))
            {
                ImVec2 windowSize = ImGui::GetWindowSize();
                ImGui::SetCursorPosY((windowSize.y - ImGui::GetTextLineHeight() * 2.5f) * 0.5f);

                ImGuiAux::ScopedBoldFont font;
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f)); // Faded text
                ImGuiAux::TextCentered("No Material Selected");
                ImGui::PopStyleColor();

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Even more faded
                ImGuiAux::TextCentered("Select one in the Inspector or drop a Material Asset here.");
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();

            ImGui::PopStyleVar();
            ImGui::PopStyleColor();

            AcceptMaterialDrop();
        }

        // MATERIAL EDITING
        if(mSelectedMaterial)
        {
            bool clearSelectedMaterial = false;

            // glTF Read-Only Warning Banner
            if(mSelectedMaterial->GetID() == AssetID::INVALID)
            {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.18f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.5f, 0.4f, 0.1f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);

                if(ImGui::BeginChild("glTFWarning", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 2.5f + 10.0f), true, ImGuiWindowFlags_NoScrollbar))
                {
                    ImGui::SetCursorPosY(ImGui::GetStyle().WindowPadding.y);
                    ImGuiAux::ScopedBoldFont font;

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.1f, 1.0f)); // Golden/Yellow warning text
                    ImGuiAux::TextCentered("READ-ONLY GLTF MATERIAL");
                    ImGui::PopStyleColor();

                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGuiAux::TextCentered("Create a new material and apply it to the mesh to edit properties.");
                    ImGui::PopStyleColor();
                }
                ImGui::EndChild();

                ImGui::PopStyleVar();
                ImGui::PopStyleColor(2);
                ImGui::Spacing();
            }

            // Header & Toolbar
            float buttonSizeX = 70.0f;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float totalButtonsWidth = (mSelectedMaterial->GetID() != AssetID::INVALID) ? (buttonSizeX * 2.0f + spacing) : buttonSizeX;

            if(ImGui::BeginTable("##MaterialHeaderTable", 2))
            {
                ImGui::TableSetupColumn("##HeaderText", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##ToolbarBtns", ImGuiTableColumnFlags_WidthFixed, totalButtonsWidth);
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Editing Material");
                {
                    ImGuiAux::ScopedBoldFont font;
                    ImGui::Text("%s", mSelectedMaterial->GetName().c_str());
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeight() * 0.35f));

                {
                    ImGuiAux::ScopedBoldFont font;
                    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                    if(mSelectedMaterial->GetID() != AssetID::INVALID)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.50f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.60f, 0.25f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.40f, 0.15f, 1.0f));
                        if(ImGui::Button("SAVE", ImVec2(buttonSizeX, 0)))
                            Core::GetAssetManager()->Save(mSelectedMaterial->GetID());
                        ImGui::PopStyleColor(3);
                        ImGui::SameLine();
                    }
                    if(ImGui::Button("CLEAR", ImVec2(buttonSizeX, 0)))
                        clearSelectedMaterial = true;

                    ImGui::PopStyleVar();
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Properties Table
            if(ImGui::BeginTable("##MaterialProperties", 2, ImGuiTableFlags_Resizable))
            {
                // Albedo Color
                glm::vec3 albedo = mSelectedMaterial->Get<glm::vec3>("Albedo");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Albedo Color");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::ColorEdit3("##v", &albedo.x))
                    mSelectedMaterial->Set<glm::vec3>("Albedo", albedo);
                ImGui::PopItemWidth();

                // Metallic
                float metallic = mSelectedMaterial->Get<float>("Metallic");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Metallic");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::SliderFloat("##Metallic", &metallic, 0.0f, 1.0f))
                    mSelectedMaterial->Set<float>("Metallic", metallic);
                ImGui::PopItemWidth();

                // Roughness
                float roughness = mSelectedMaterial->Get<float>("Roughness");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted("Roughness");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::SliderFloat("##Roughness", &roughness, 0.0f, 1.0f))
                    mSelectedMaterial->Set<float>("Roughness", roughness);
                ImGui::PopItemWidth();

                DrawTextureProperty("Albedo Map", mSelectedMaterial.Raw(), "UseAlbedoMap", "AlbedoMap");
                DrawTextureProperty("Normal Map", mSelectedMaterial.Raw(), "UseNormalMap", "NormalMap");
                DrawTextureProperty("Roughness Map", mSelectedMaterial.Raw(), "UseMetallicMap", "RoughnessMetallicMap");

                ImGui::EndTable();
            }

            AcceptMaterialDrop();
            ImGui::Dummy(ImGui::GetContentRegionAvail());
            AcceptMaterialDrop();

            if(clearSelectedMaterial)
                mSelectedMaterial = nullptr;
        }
        ImGui::End();
    }

    void MaterialEditorPanel::Shutdown()
    {
        mSelectedMaterial = nullptr;
    }

} // namespace Surge