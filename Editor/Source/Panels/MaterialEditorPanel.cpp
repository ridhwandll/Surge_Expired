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
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(label);

        ImGui::TableSetColumnIndex(1);

        bool useMap = material->Get<int>(mapUseName);
        Ref<Texture2D> tex = material->GetTexture(textureName);

        ImGui::PushID(label);
        if(ImGui::Checkbox("##Use", &useMap))
            material->Set<int>(mapUseName, useMap);

        ImGui::SameLine();

        bool textureDropped = false;
        AssetID droppedAssetID = 0;
        const float thumbnailSize = 67.0f;

        if(tex)
        {
            ImTextureID texID = Core::GetRenderer()->GetRHI()->GetImGuiImage(tex->GetRHIImage());
            ImGui::Image(texID, ImVec2(thumbnailSize, thumbnailSize));
        }
        else
        {
            if(useMap) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

            ImGuiAux::ScopedBoldFont font;
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

                // QoL: Automatically check the box when a texture is dropped!
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

        if(!mSelectedMaterial)
        {
            ImGuiAux::ScopedBoldFont font(17.0f);
            ImGuiAux::TextCentered("Select a material in the Inspector(Mesh Component)\nor drop a Material Asset here from the Content Browser!");
            ImGui::Dummy(ImGui::GetContentRegionAvail());
        }

        if(mSelectedMaterial)
        {
            // Do not let users edit the material if it is loaded from gltf (i.e. it doesn't have a valid AssetID and thus cannot be saved)
            if(mSelectedMaterial->GetID() == AssetID::INVALID)
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::TextUnformatted("This material is loaded from a glTF file and cannot be edited/saved");
                ImGui::TextUnformatted("Create a new material, drop it to the mesh and then edit it!");
                ImGui::BeginDisabled();
            }
            bool clearSelectedMaterial = false;
            {
                ImGuiAux::ScopedBoldFont font;
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                if(ImGui::Button("SAVE"))
                    Core::GetAssetManager()->Save(mSelectedMaterial->GetID());
                ImGui::SameLine();
                if(ImGui::Button("CLEAR"))
                    clearSelectedMaterial = true;

                ImGui::PopStyleVar();
            }

            ImGui::Spacing();
            ImGui::Text("Editing: %s", mSelectedMaterial->GetName().c_str());
            ImGui::Separator();
            ImGui::Spacing();

            if(ImGui::BeginTable("##MaterialProperties", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerH))
            {
                // Albedo Color
                glm::vec3 albedo = mSelectedMaterial->Get<glm::vec3>("Albedo");
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
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
                ImGui::TextUnformatted("Roughness");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-FLT_MIN);
                if(ImGui::SliderFloat("##Roughness", &roughness, 0.0f, 1.0f))
                    mSelectedMaterial->Set<float>("Roughness", roughness);
                ImGui::PopItemWidth();

                DrawTextureProperty("Albedo Map", mSelectedMaterial.Raw(), "UseAlbedoMap", "AlbedoMap");
                DrawTextureProperty("Normal Map", mSelectedMaterial.Raw(), "UseNormalMap", "NormalMap");
                DrawTextureProperty("RoughnessMetallic Map", mSelectedMaterial.Raw(), "UseMetallicMap", "RoughnessMetallicMap");

                ImGui::EndTable();
            }

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

            if(mSelectedMaterial->GetID() == AssetID::INVALID)
                ImGui::EndDisabled();

            if (clearSelectedMaterial)
                mSelectedMaterial = nullptr;
        }
        ImGui::End();

    }

    void MaterialEditorPanel::Shutdown()
    {
        mSelectedMaterial = nullptr;
    }

} // namespace Surge