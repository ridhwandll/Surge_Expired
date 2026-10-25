// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/MaterialEditorPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Editor.hpp"
#include "Asset/AssetFileOps.hpp"
#include "Asset/SourceWriters/MaterialSourceWriter.hpp"
#include "StatusBar/EditorStatusBar.hpp"
#include "Utility/ImGuiAux.hpp"

#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cctype>

namespace Surge
{
    namespace
    {
        constexpr float PREVIEW_FOV = 40.0f;
        constexpr float PREVIEW_NEAR = 0.02f;
        constexpr float PREVIEW_FAR = 100.0f;
        constexpr float LABEL_WIDTH = 130.0f;
        constexpr float THUMBNAIL_SIZE = 44.0f;
        constexpr float PICKER_THUMBNAIL_SIZE = 32.0f;
        constexpr const char* WINDOW_NAME = "Material Editor"; // imgui.ini docking keys on it, keep it stable
        constexpr const char* PICKER_POPUP = "##TexturePicker";

        // Shader names (Renderer3D.glsl): the sampler and the Use flags it drives
        struct TextureSlotInfo
        {
            const char* Name;
            const char* UseFlag;
            const char* SecondUseFlag; // Packed map: one flag per channel
        };
        constexpr std::array<TextureSlotInfo, MaterialEditorPanel::TEXTURE_SLOT_COUNT> sSlots = { {
            { "AlbedoMap", "UseAlbedoMap", nullptr },
            { "NormalMap", "UseNormalMap", nullptr },
            { "RoughnessMetallicMap", "UseMetallicMap", "UseRoughnessMap" },
            { "EmissionMap", "UseEmissionMap", nullptr },
        } };
        constexpr Uint ALBEDO_SLOT = 0;
        constexpr Uint NORMAL_SLOT = 1;
        constexpr Uint PACKED_SLOT = 2;
        constexpr Uint EMISSION_SLOT = 3;

        constexpr std::array<const char*, 4> sShapeNames = { "Sphere", "Cube", "Plane", "Torus" };

        const char* GetFormatName(ImageFormat format)
        {
            switch(format)
            {
                case ImageFormat::R8_UNORM: return "R8";
                case ImageFormat::RGBA8_SRGB: return "RGBA8 sRGB";
                case ImageFormat::RGBA8_UNORM: return "RGBA8";
                case ImageFormat::ASTC4x4_SRGB: return "ASTC sRGB";
                case ImageFormat::ASTC4x4_UNORM: return "ASTC";
                case ImageFormat::BC7_SRGB: return "BC7 sRGB";
                case ImageFormat::BC7_UNORM: return "BC7";
                case ImageFormat::ETC2_RGBA8_SRGB: return "ETC2 sRGB";
                case ImageFormat::ETC2_RGBA8_UNORM: return "ETC2";
                default: return "Other";
            }
        }

        String ToLower(const String& text)
        {
            String result = text;
            std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return result;
        }

        bool BeginProperties()
        {
            if(!ImGui::BeginTable("##Properties", 2, ImGuiTableFlags_SizingFixedFit))
                return false;
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, LABEL_WIDTH);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        void LabelCell(const char* label, const char* tooltip = nullptr)
        {
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            if(tooltip)
                ImGuiAux::DelayedToolTip(tooltip);
            ImGui::TableNextColumn();
        }

        // One submesh using material slot 0, bounds are rebuilt by the Mesh
        MeshSpecification BuildPreviewSpec(MeshGenerator::MeshData&& data, const Ref<Material>& material)
        {
            MeshSpecification spec;
            spec.Vertices = std::move(data.Vertices);
            spec.Indices = std::move(data.Indices);
            spec.Materials.push_back(material);

            Submesh& submesh = spec.Submeshes.emplace_back();
            submesh.BaseVertex = 0;
            submesh.BaseIndex = 0;
            submesh.MaterialIndex = 0;
            submesh.VertexCount = static_cast<Uint>(spec.Vertices.size());
            submesh.IndexCount = static_cast<Uint>(spec.Indices.size()) * 3;
            submesh.Transform = glm::mat4(1.0f);
            submesh.LocalTransform = glm::mat4(1.0f);
            submesh.MeshName = "MaterialPreview";
            submesh.NodeName = "MaterialPreview_Node";
            return spec;
        }

        // File names can't hold every character a glTF material name may use
        String MakeFileName(const String& name)
        {
            String result = name;
            for(char& c : result)
            {
                if(c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*' || static_cast<unsigned char>(c) < 32)
                    c = '_';
            }
            while(!result.empty() && (result.back() == ' ' || result.back() == '.'))
                result.pop_back();
            return result.empty() ? String("Material") : result;
        }

        Path CreateMaterialFile(const Path& folder, const String& baseName, Ref<Material>& outMaterial)
        {
            AssetManager* am = Core::GetAssetManager();
            Filesystem::CreateOrEnsureDirectories(folder);
            const Path path = AssetFileOps::MakeUnique(folder / (baseName + GetExtensionFromAssetType(AssetType::MATERIAL)));
            outMaterial = am->Create<Material>(AssetFileOps::ToRelative(path), *Core::GetRenderer(), path.stem().string());
            return outMaterial ? path : Path();
        }

        void RefreshContentBrowser()
        {
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            editor->GetPanelManager().GetPanel<ContentBrowserPanel>()->RequestRefresh();
        }
    } // namespace

    void MaterialEditorPanel::Init([[maybe_unused]] void* panelInitArgs)
    {
        mCode = GetStaticCode();
    }

    void MaterialEditorPanel::Shutdown()
    {
        if(mView != INVALID_RENDER_VIEW)
        {
            Core::GetRenderer()->DestroyView(mView);
            mView = INVALID_RENDER_VIEW;
        }
        mPreviewMesh = nullptr;
        mPreviewPlaceholder = nullptr;
        mMaterial = nullptr;
        mOwnerMesh = nullptr;
        mUndoStack.clear();
        mRedoStack.clear();
        mPickerEntries.clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // Document
    //////////////////////////////////////////////////////////////////////////

    void MaterialEditorPanel::OpenMaterial(const Ref<Material>& material, const Ref<Mesh>& ownerMesh, Uint slot)
    {
        if(!material)
            return;

        Editor* editor = static_cast<Editor*>(Core::GetClient());
        editor->GetPanelManager().GetAllPanels()[mCode].Show = true;
        mFocusWindow = true;

        if(mMaterial.Raw() == material.Raw())
        {
            mOwnerMesh = ownerMesh;
            mOwnerSlot = slot;
            return;
        }

        auto open = [this, material, ownerMesh, slot]() {
            if(mMaterial && mUnsaved)
                Revert(); // The discarded edits live in the old asset, meshes must not keep them

            mMaterial = material;
            mOwnerMesh = ownerMesh;
            mOwnerSlot = slot;
            ResetDocument();
        };

        if(mMaterial && mUnsaved)
            ImGuiAux::ShowConfirmationBox("Unsaved Material", "The open material has unsaved changes.\nDiscard them and open the other material?", open);
        else
            open();
    }

    void MaterialEditorPanel::CloseMaterial()
    {
        Core::AddFrameEndCallback([this]() {

            auto close = [this]() {
                if(mUnsaved)
                    Revert();
                if(mPreviewMesh)
                    mPreviewMesh->ClearMaterialOverride(0); // Doesn't keep the closed material alive
                mMaterial = nullptr;
                mOwnerMesh = nullptr;
                mUndoStack.clear();
                mRedoStack.clear();
                mUnsaved = false;
                mDocumentChanged = false;
            };

            if(mMaterial && mUnsaved)
                ImGuiAux::ShowConfirmationBox("Unsaved Material", "Discard the changes of the open material?", close);
            else
                close();
        });
    }

    MaterialEditorPanel::Snapshot MaterialEditorPanel::TakeSnapshot() const
    {
        Snapshot snapshot;
        const MemoryBlock& properties = mMaterial->GetCPUData();
        snapshot.Properties.assign(properties.As<Byte>(), properties.As<Byte>() + properties.GetSize());

        const auto& textures = mMaterial->GetTextures();
        for(Uint i = 0; i < TEXTURE_SLOT_COUNT; i++)
        {
            const auto it = textures.find(sSlots[i].Name);
            if(it != textures.end() && it->second.Data1)
                snapshot.Textures[i] = it->second.Data1->GetID().Get();
        }
        return snapshot;
    }

    void MaterialEditorPanel::RestoreSnapshot(const Snapshot& snapshot)
    {
        MemoryBlock& properties = mMaterial->GetCPUData();
        if(snapshot.Properties.size() == properties.GetSize())
            properties.Write((void*)snapshot.Properties.data(), properties.GetSize());

        AssetManager* am = Core::GetAssetManager();
        for(Uint i = 0; i < TEXTURE_SLOT_COUNT; i++)
        {
            const AssetID id = AssetID(snapshot.Textures[i]);
            mMaterial->SetTexture(sSlots[i].Name, id.IsValid() ? am->Load<Texture2D>(id) : Ref<Texture2D>());
        }
        mMaterial->MarkDirty();
    }

    void MaterialEditorPanel::ResetDocument()
    {
        mUndoStack.clear();
        mRedoStack.clear();
        mUnsaved = false;
        mDocumentChanged = false;
        if(mMaterial)
        {
            mCommitted = TakeSnapshot();
            mSaved = mCommitted;
        }
    }

    void MaterialEditorPanel::Save()
    {
        if(!mMaterial || IsReadOnly())
            return;

        AssetManager* am = Core::GetAssetManager();
        const String& relativePath = am->GetMetadata(mMaterial->GetID()).RelativePath;
        if(!MaterialSourceWriter::Write(mMaterial, am->GetAbsolutePath(relativePath)))
        {
            EditorStatusBar::Get().Notify(Severity::Error, std::format("Failed to save {}", Filesystem::GetFilenameWithExt(relativePath)));
            return;
        }
        am->Save(mMaterial->GetID());

        mSaved = TakeSnapshot();
        mUnsaved = false;
        Log<Severity::Info>("[MaterialEditor] Saved {}", relativePath);
        EditorStatusBar::Get().Notify(Severity::Info, std::format("Saved {}", Filesystem::GetFilenameWithExt(relativePath)), 2.0f);
    }

    void MaterialEditorPanel::Revert()
    {
        if(!mMaterial)
            return;

        RestoreSnapshot(mSaved);
        mUnsaved = false;
    }

    void MaterialEditorPanel::MarkChanged()
    {
        mDocumentChanged = true;
        mUnsaved = true;
    }

    void MaterialEditorPanel::CommitUndoIfIdle()
    {
        if(!mMaterial || !mDocumentChanged)
            return;

        // Coalesce: a whole drag or color pick becomes a single undo step
        if(ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left))
            return;

        mUndoStack.push_back(std::move(mCommitted));
        if(mUndoStack.size() > MAX_UNDO_STEPS)
            mUndoStack.erase(mUndoStack.begin());
        mRedoStack.clear();
        mCommitted = TakeSnapshot();
        mDocumentChanged = false;
        mUnsaved = !(mCommitted == mSaved);
    }

    void MaterialEditorPanel::Undo()
    {
        if(mDocumentChanged) // A pending edit becomes its own step first
        {
            mUndoStack.push_back(std::move(mCommitted));
            mCommitted = TakeSnapshot();
            mDocumentChanged = false;
        }
        if(mUndoStack.empty())
            return;

        mRedoStack.push_back(std::move(mCommitted));
        mCommitted = std::move(mUndoStack.back());
        mUndoStack.pop_back();
        RestoreSnapshot(mCommitted);
        mUnsaved = !(mCommitted == mSaved);
    }

    void MaterialEditorPanel::Redo()
    {
        if(mRedoStack.empty())
            return;

        mUndoStack.push_back(std::move(mCommitted));
        mCommitted = std::move(mRedoStack.back());
        mRedoStack.pop_back();
        RestoreSnapshot(mCommitted);
        mUnsaved = !(mCommitted == mSaved);
    }

    void MaterialEditorPanel::HandleShortcuts()
    {
        const ImGuiIO& io = ImGui::GetIO();
        if(io.WantTextInput)
            return;

        if(io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
            Save();
        if(io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))
            Undo();
        if(io.KeyCtrl && (ImGui::IsKeyPressed(ImGuiKey_Y) || (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z))))
            Redo();
    }

    void MaterialEditorPanel::MakeEditableCopy()
    {
        AssetManager* am = Core::GetAssetManager();

        // Next to the mesh the material comes from, Assets/Materials when it is unknown
        Path folder = AssetFileOps::GetAssetsRoot() / "Materials";
        if(mOwnerMesh && mOwnerMesh->GetID())
            folder = Path(am->GetAbsolutePath(am->GetMetadata(mOwnerMesh->GetID()).RelativePath)).parent_path();

        Ref<Material> copy;
        const Path path = CreateMaterialFile(folder, MakeFileName(mMaterial->GetName()), copy);
        if(!copy)
        {
            EditorStatusBar::Get().Notify(Severity::Error, "Failed to create the material copy, see the Console");
            return;
        }

        const MemoryBlock& source = mMaterial->GetCPUData();
        copy->GetCPUData().Write((void*)source.As<Byte>(), source.GetSize());
        for(const TextureSlotInfo& slot : sSlots)
            copy->SetTexture(slot.Name, mMaterial->GetTexture(slot.Name));
        copy->MarkDirty();

        MaterialSourceWriter::Write(copy, path.generic_string());
        am->Save(copy->GetID());

        // The slot this was opened from now uses the copy, like dropping it on the Inspector slot
        if(mOwnerMesh && mOwnerSlot < mOwnerMesh->GetMaterials().size())
        {
            mOwnerMesh->SetMaterialOverride(mOwnerSlot, copy);
            if(mOwnerMesh->GetID())
                am->Save(mOwnerMesh->GetID());
        }

        RefreshContentBrowser();
        EditorStatusBar::Get().Notify(Severity::Info, std::format("Created {}", AssetFileOps::ToRelative(path)), 3.0f);

        const Ref<Mesh> ownerMesh = mOwnerMesh;
        const Uint ownerSlot = mOwnerSlot;
        OpenMaterial(copy, ownerMesh, ownerSlot);
    }

    //////////////////////////////////////////////////////////////////////////
    // Panel
    //////////////////////////////////////////////////////////////////////////

    void MaterialEditorPanel::Render(bool* show)
    {
        mOwnsShortcuts = false;
        mPreviewVisible = false;

        if(*show)
        {
            if(mFocusWindow)
            {
                ImGui::SetNextWindowFocus();
                mFocusWindow = false;
            }

            if(ImGui::Begin(WINDOW_NAME, show))
            {
                const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                mOwnsShortcuts = mMaterial && focused && !IsReadOnly();

                if(!mMaterial)
                    DrawEmptyState();
                else
                {
                    DrawToolbar();
                    ImGui::Separator();

                    if(ImGui::BeginChild("##MaterialBody"))
                    {
                        if(IsReadOnly())
                            DrawReadOnlyBanner();
                        DrawPreview();
                        DrawProperties();
                        DrawTexturePicker();
                    }
                    ImGui::EndChild();

                    if(mOwnsShortcuts)
                        HandleShortcuts();
                }
            }
            ImGui::End();
        }

        CommitUndoIfIdle();
        UpdatePreview();
    }

    void MaterialEditorPanel::DrawEmptyState()
    {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        {
            ImGuiAux::ScopedBoldFont bold(20.0f);
            ImGuiAux::TextCentered("No material open");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextWrapped("Double click a material in the Content Browser, click a material slot in the Inspector, drop a material on the button below, or create a new one.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGuiAux::ScopedBoldFont bold;
        if(ImGui::Button("Create New Material", ImVec2(-FLT_MIN, 40.0f)))
        {
            Ref<Material> material;
            const Path path = CreateMaterialFile(AssetFileOps::GetAssetsRoot() / "Materials", "NewMaterial", material);
            if(material)
            {
                MaterialSourceWriter::Write(material, path.generic_string());
                RefreshContentBrowser();
                OpenMaterial(material);
            }
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                AssetManager* am = Core::GetAssetManager();
                const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                if(am->GetMetadata(dropped).Type == AssetType::MATERIAL)
                    OpenMaterial(am->Load<Material>(dropped));
                else
                    EditorStatusBar::Get().Notify(Severity::Warn, "Drop a material (.smat) asset");
            }
            ImGui::EndDragDropTarget();
        }
    }

    void MaterialEditorPanel::DrawToolbar()
    {
        AssetManager* am = Core::GetAssetManager();
        const bool readOnly = IsReadOnly();
        const String relativePath = readOnly ? String() : am->GetMetadata(mMaterial->GetID()).RelativePath;
        const String title = readOnly ? mMaterial->GetName() : Filesystem::GetFilenameWithExt(relativePath);
        {
            ImGuiAux::ScopedBoldFont bold(18.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s%s", title.c_str(), mUnsaved ? " *" : "");
        }
        if(!relativePath.empty())
            ImGuiAux::DelayedToolTip(relativePath.c_str());

        if(!readOnly)
        {
            ImGui::SameLine();
            if(ImGui::Button("Save"))
                Save();
            ImGuiAux::DelayedToolTip("Ctrl+S");

            ImGui::SameLine();
            ImGui::BeginDisabled(!mUnsaved);
            if(ImGui::Button("Revert"))
                ImGuiAux::ShowConfirmationBox("Revert Material", "Go back to the last saved state and drop every unsaved change?", [this]() {
                    Revert();
                    ResetDocument();
                });
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::BeginDisabled(mUndoStack.empty() && !mDocumentChanged);
            if(ImGui::Button("Undo"))
                Undo();
            ImGui::EndDisabled();
            ImGuiAux::DelayedToolTip("Ctrl+Z");

            ImGui::SameLine();
            ImGui::BeginDisabled(mRedoStack.empty());
            if(ImGui::Button("Redo"))
                Redo();
            ImGui::EndDisabled();
            ImGuiAux::DelayedToolTip("Ctrl+Y / Ctrl+Shift+Z");
        }

        ImGui::SameLine();
        if(ImGui::Button("Close"))
            CloseMaterial();
    }

    void MaterialEditorPanel::DrawReadOnlyBanner()
    {
        AssetManager* am = Core::GetAssetManager();
        const String meshName = mOwnerMesh && mOwnerMesh->GetID() ? Filesystem::GetFilenameWithExt(am->GetMetadata(mOwnerMesh->GetID()).RelativePath) : String("a mesh file");

        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.33f, 0.13f, 1.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        if(ImGui::BeginChild("##ReadOnlyBanner", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.84f, 0.54f, 1.0f));
            {
                ImGuiAux::ScopedBoldFont bold;
                ImGui::TextUnformatted("Read-only glTF material");
            }
            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text("It lives inside %s and is rebuilt from it on every import, so edits here would be lost.", meshName.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();

            if(ImGui::Button("Make Editable Copy"))
                MakeEditableCopy();
            ImGuiAux::DelayedToolTip(mOwnerMesh ? "Saves a .smat with the same values next to the mesh and uses it for this mesh slot"
                                                : "Saves a .smat with the same values to Assets/Materials");
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    bool MaterialEditorPanel::BeginSection(const char* name)
    {
        ImGui::PushID(name);
        bool open = false;
        {
            ImGuiAux::ScopedBoldFont bold;
            open = ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen);
        }
        if(open && BeginProperties())
            return true;

        ImGui::PopID();
        return false;
    }

    void MaterialEditorPanel::EndSection()
    {
        ImGui::EndTable();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PopID();
    }

    void MaterialEditorPanel::DrawProperties()
    {
        auto colorRow = [this](const char* label, const char* property, const char* tooltip = nullptr) {
            ImGui::PushID(property);
            LabelCell(label, tooltip);
            glm::vec3 value = mMaterial->Get<glm::vec3>(property);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::ColorEdit3("##v", &value.x, ImGuiColorEditFlags_Float))
            {
                mMaterial->Set<glm::vec3>(property, value);
                MarkChanged();
            }
            ImGui::PopID();
        };

        auto sliderRow = [this](const char* label, const char* property, const char* tooltip = nullptr) {
            ImGui::PushID(property);
            LabelCell(label, tooltip);
            float value = mMaterial->Get<float>(property);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::SliderFloat("##v", &value, 0.0f, 1.0f, "%.2f", ImGuiSliderFlags_AlwaysClamp))
            {
                mMaterial->Set<float>(property, value);
                MarkChanged();
            }
            ImGui::PopID();
        };

        ImGui::BeginDisabled(IsReadOnly());

        if(BeginSection("Surface"))
        {
            colorRow("Albedo", "Albedo", "Base color, multiplied by the Albedo Map");
            DrawTextureSlot("Albedo Map", ALBEDO_SLOT);
            DrawTextureSlot("Normal Map", NORMAL_SLOT);
            EndSection();
        }

        if(BeginSection("Metallic & Roughness"))
        {
            sliderRow("Metallic", "Metallic", "Multiplied by the packed map's blue channel");
            sliderRow("Roughness", "Roughness", "Multiplied by the packed map's green channel");
            sliderRow("Reflectance", "Reflectance", "Specular strength of non-metals, 0.5 = 4% (most materials)");
            DrawTextureSlot("Packed Map", PACKED_SLOT);
            EndSection();
        }

        if(BeginSection("Emission"))
        {
            colorRow("Color", "Emission", "Added on top of the lighting, black = no emission");

            ImGui::PushID("EmissionIntensity");
            LabelCell("Intensity", "HDR multiplier, the result above the bloom threshold (1.0 by default) glows");
            float intensity = mMaterial->Get<float>("EmissionIntensity");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::DragFloat("##v", &intensity, 0.05f, 0.0f, FLT_MAX, "%.2f", ImGuiSliderFlags_AlwaysClamp))
            {
                mMaterial->Set<float>("EmissionIntensity", intensity);
                MarkChanged();
            }
            ImGui::PopID();

            DrawTextureSlot("Emission Map", EMISSION_SLOT);
            EndSection();
        }

        ImGui::EndDisabled();
    }

    void MaterialEditorPanel::SetSlotTexture(Uint slot, const Ref<Texture2D>& texture)
    {
        const TextureSlotInfo& info = sSlots[slot];
        mMaterial->SetTexture(info.Name, texture);

        const int use = texture ? 1 : 0;
        mMaterial->Set<int>(info.UseFlag, use);
        if(info.SecondUseFlag)
            mMaterial->Set<int>(info.SecondUseFlag, use);
        MarkChanged();
    }

    void MaterialEditorPanel::DrawTextureSlot(const char* label, Uint slot)
    {
        const TextureSlotInfo& info = sSlots[slot];
        Renderer* renderer = Core::GetRenderer();
        Scope<GraphicsRHI>& rhi = renderer->GetRHI();
        AssetManager* am = Core::GetAssetManager();
        const Ref<Texture2D> texture = mMaterial->GetTexture(info.Name);

        ImGui::PushID(info.Name);
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::TableNextColumn();

        ImGui::BeginGroup();

        // Thumbnail (or an empty well), hover shows it large
        const ImVec2 thumbMin = ImGui::GetCursorScreenPos();
        const ImVec2 thumbSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE);
        if(texture)
        {
            ImGui::Image(static_cast<ImTextureID>(rhi->GetImGuiImage(texture->GetRHIImage())), thumbSize);
            if(ImGui::BeginItemTooltip())
            {
                const ImageDesc& desc = rhi->GetDesc(texture->GetRHIImage());
                const float scale = 256.0f / static_cast<float>(std::max(std::max(desc.Width, desc.Height), 1u));
                ImGui::Image(static_cast<ImTextureID>(rhi->GetImGuiImage(texture->GetRHIImage())), ImVec2(desc.Width * scale, desc.Height * scale));
                ImGui::EndTooltip();
            }
        }
        else
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImVec2 thumbMax(thumbMin.x + thumbSize.x, thumbMin.y + thumbSize.y);
            drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
            drawList->AddRect(thumbMin, thumbMax, ImGui::GetColorU32(ImGuiCol_Border), 3.0f);
            const ImVec2 center((thumbMin.x + thumbMax.x) * 0.5f, (thumbMin.y + thumbMax.y) * 0.5f);
            const ImU32 plusColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            drawList->AddLine(ImVec2(center.x - 7.0f, center.y), ImVec2(center.x + 7.0f, center.y), plusColor, 1.5f);
            drawList->AddLine(ImVec2(center.x, center.y - 7.0f), ImVec2(center.x, center.y + 7.0f), plusColor, 1.5f);
            ImGui::Dummy(thumbSize);
        }

        // Right of the thumbnail: name, details, then the buttons and Use flags in the normal flow (no right alignment: inside a
        // group SameLine(x) adds the group's offset, which pushed the buttons out of the cell)
        ImGui::SameLine();
        ImGui::BeginGroup();
        {
            // Long names stop at the cell edge. Only the text is clipped: the clip rect also limits where items can be clicked
            const ImVec2 textMin = ImGui::GetCursorScreenPos();
            ImGui::PushClipRect(textMin, ImVec2(textMin.x + std::max(ImGui::GetContentRegionAvail().x, 1.0f), textMin.y + ImGui::GetTextLineHeightWithSpacing() * 2.0f), true);
            if(texture)
            {
                const String fileName = Filesystem::GetFilenameWithExt(am->GetMetadata(texture->GetID()).RelativePath);
                {
                    ImGuiAux::ScopedBoldFont bold;
                    ImGui::TextUnformatted(fileName.c_str());
                }
                const ImageDesc& desc = rhi->GetDesc(texture->GetRHIImage());
                const float megaBytes = static_cast<float>(rhi->GetImageSize(texture->GetRHIImage())) / (1024.0f * 1024.0f);
                ImGui::TextDisabled("%ux%u  %s  %.2f MB", desc.Width, desc.Height, GetFormatName(desc.Format), megaBytes);
            }
            else
            {
                ImGui::TextDisabled("None");
                ImGui::TextDisabled("Drop a texture here or pick one");
            }
            ImGui::PopClipRect();
        }

        if(ImGui::Button("Pick..."))
            OpenTexturePicker(slot);
        ImGuiAux::DelayedToolTip("Choose from every texture in the project");
        if(texture)
        {
            ImGui::SameLine();
            if(ImGui::Button("Remove"))
                SetSlotTexture(slot, nullptr);
            ImGuiAux::DelayedToolTip("Clears the slot and turns its Use flag off");
        }

        // Use flags: a map can stay assigned while the shader ignores it. Without a map they would only multiply by white
        auto useFlagCheckbox = [this](const char* text, const char* flag, const char* tooltip) {
            bool use = mMaterial->Get<int>(flag) != 0;
            if(ImGui::Checkbox(text, &use))
            {
                mMaterial->Set<int>(flag, use ? 1 : 0);
                MarkChanged();
            }
            ImGuiAux::DelayedToolTip(tooltip);
        };
        ImGui::BeginDisabled(!texture);
        if(info.SecondUseFlag)
        {
            useFlagCheckbox("Use Metallic (B)", info.UseFlag, "Metallic is multiplied by the map's blue channel");
            ImGui::SameLine();
            useFlagCheckbox("Use Roughness (G)", info.SecondUseFlag, "Roughness is multiplied by the map's green channel");
        }
        else
        {
            ImGui::SameLine();
            useFlagCheckbox("Use Texture Map", info.UseFlag, "Off: the map stays assigned but the shader ignores it");
        }
        ImGui::EndDisabled();
        ImGui::EndGroup();

        ImGui::EndGroup();

        // The whole slot takes Content Browser drops
        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                if(am->GetMetadata(dropped).Type == AssetType::TEXTURE2D)
                {
                    if(Ref<Texture2D> newTexture = am->Load<Texture2D>(dropped))
                        SetSlotTexture(slot, newTexture);
                }
                else
                    EditorStatusBar::Get().Notify(Severity::Warn, "Material maps need a texture (TEXTURE2D) asset");
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PopID();
    }

    void MaterialEditorPanel::OpenTexturePicker(Uint slot)
    {
        AssetManager* am = Core::GetAssetManager();
        mPickerSlot = slot;
        mPickerFilter.clear();
        mPickerEntries.clear();

        for(const auto& [id, metadata] : am->GetRegistryMap())
        {
            if(metadata.Type != AssetType::TEXTURE2D)
                continue;

            PickerEntry& entry = mPickerEntries.emplace_back();
            entry.ID = id;
            const Path relative = Path(metadata.RelativePath);
            entry.Name = relative.filename().string();
            entry.Folder = relative.parent_path().generic_string();
            entry.SearchKey = ToLower(metadata.RelativePath);
            entry.AbsolutePath = am->GetAbsolutePath(metadata.RelativePath);
            std::error_code error;
            entry.WriteTime = std::filesystem::last_write_time(entry.AbsolutePath, error);
        }
        std::sort(mPickerEntries.begin(), mPickerEntries.end(), [](const PickerEntry& a, const PickerEntry& b) { return ToLower(a.Name) < ToLower(b.Name); });
        mOpenPicker = true;
    }

    void MaterialEditorPanel::DrawTexturePicker()
    {
        if(mOpenPicker)
        {
            ImGui::OpenPopup(PICKER_POPUP);
            mOpenPicker = false;
        }

        ImGui::SetNextWindowSize(ImVec2(380.0f, 440.0f), ImGuiCond_Appearing);
        if(!ImGui::BeginPopup(PICKER_POPUP))
            return;

        const TextureSlotInfo& info = sSlots[mPickerSlot];
        const Ref<Texture2D> current = mMaterial->GetTexture(info.Name);
        const AssetID currentID = current ? current->GetID() : AssetID(UUID::INVALID);

        if(ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Filter", "Search textures", &mPickerFilter);
        const String filter = ToLower(mPickerFilter);

        ThumbnailCache& thumbnails = static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ContentBrowserPanel>()->GetThumbnails();
        AssetManager* am = Core::GetAssetManager();
        Ref<Texture2D> picked;
        bool pickedNone = false;

        if(ImGui::BeginChild("##PickerList"))
        {
            if(ImGui::Selectable("None", !current))
                pickedNone = true;

            const float rowHeight = PICKER_THUMBNAIL_SIZE + 4.0f;
            for(const PickerEntry& entry : mPickerEntries)
            {
                if(!filter.empty() && entry.SearchKey.find(filter) == String::npos)
                    continue;

                ImGui::PushID(static_cast<int>(entry.ID.Get() & 0x7FFFFFFF) ^ static_cast<int>(entry.ID.Get() >> 32));
                const ImVec2 rowMin = ImGui::GetCursorScreenPos();
                if(ImGui::Selectable("##Row", entry.ID == currentID, 0, ImVec2(0.0f, rowHeight)))
                    picked = am->Load<Texture2D>(entry.ID);

                // Thumbnail from the Content Browser's cache (decoded while the browser is shown), a well until then
                if(ImGui::IsItemVisible())
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImVec2 thumbMin(rowMin.x + 2.0f, rowMin.y + 2.0f);
                    const ImVec2 thumbMax(thumbMin.x + PICKER_THUMBNAIL_SIZE, thumbMin.y + PICKER_THUMBNAIL_SIZE);
                    const ThumbnailCache::Thumbnail thumbnail = thumbnails.Get(entry.AbsolutePath, entry.WriteTime);
                    if(thumbnail.TextureID)
                    {
                        ImVec2 size(PICKER_THUMBNAIL_SIZE, PICKER_THUMBNAIL_SIZE);
                        if(thumbnail.AspectRatio >= 1.0f)
                            size.y /= thumbnail.AspectRatio;
                        else
                            size.x *= thumbnail.AspectRatio;
                        const ImVec2 imageMin(thumbMin.x + (PICKER_THUMBNAIL_SIZE - size.x) * 0.5f, thumbMin.y + (PICKER_THUMBNAIL_SIZE - size.y) * 0.5f);
                        drawList->AddImage(thumbnail.TextureID, imageMin, ImVec2(imageMin.x + size.x, imageMin.y + size.y));
                    }
                    else
                        drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);

                    const float textX = thumbMax.x + 8.0f;
                    const float lineHeight = ImGui::GetTextLineHeight();
                    drawList->AddText(ImVec2(textX, rowMin.y + rowHeight * 0.5f - lineHeight), ImGui::GetColorU32(ImGuiCol_Text), entry.Name.c_str());
                    drawList->AddText(ImVec2(textX, rowMin.y + rowHeight * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), entry.Folder.empty() ? "Assets" : entry.Folder.c_str());
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();

        if(picked || pickedNone)
        {
            SetSlotTexture(mPickerSlot, picked);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    //////////////////////////////////////////////////////////////////////////
    // Preview
    //////////////////////////////////////////////////////////////////////////

    glm::mat4 MaterialEditorPanel::GetPreviewView() const
    {
        const glm::vec3 direction = { std::cos(mPitch) * std::sin(mYaw), std::sin(mPitch), std::cos(mPitch) * std::cos(mYaw) };
        return glm::lookAt(direction * mDistance, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 MaterialEditorPanel::GetPreviewProjection(float aspect) const
    {
        glm::mat4 projection = glm::perspective(glm::radians(PREVIEW_FOV), aspect, PREVIEW_NEAR, PREVIEW_FAR);
        projection[1][1] *= -1.0f; // Vulkan clip space, like EditorCamera
        return projection;
    }

    void MaterialEditorPanel::ResetPreviewCamera()
    {
        mYaw = 0.6f;
        mPitch = 0.3f;
        mDistance = mPreviewShape == PreviewShape::CUBE ? 2.6f : 2.0f;
        mMeshYaw = 0.0f;
        mLightYaw = -0.8f;
        mLightPitch = 0.75f;
    }

    void MaterialEditorPanel::EnsurePreviewMesh()
    {
        if(!mPreviewPlaceholder)
            mPreviewPlaceholder = Material::Create(*Core::GetRenderer(), "MaterialPreview");

        if(!mPreviewMesh || mBuiltShape != mPreviewShape)
        {
            MeshGenerator::MeshData data;
            switch(mPreviewShape)
            {
                case PreviewShape::CUBE: data = MeshGenerator::GenerateCube(); break;
                case PreviewShape::PLANE: data = MeshGenerator::GeneratePlane(1.2f, 1.2f); break;
                case PreviewShape::TORUS: data = MeshGenerator::GenerateTorus(0.45f, 0.18f, 48, 24); break;
                default: data = MeshGenerator::GenerateSphere(0.5f, 64, 32); break;
            }
            mPreviewMesh = Mesh::Create(*Core::GetRenderer(), *Core::GetAssetManager(), BuildPreviewSpec(std::move(data), mPreviewPlaceholder)); // Its own mesh: the shared default mesh assets keep their materials
            mBuiltShape = mPreviewShape;
        }

        if(mPreviewMesh->GetMaterialPtrAtIndex(0) != mMaterial.Raw())
            mPreviewMesh->SetMaterialOverride(0, mMaterial);
    }

    void MaterialEditorPanel::UpdatePreview()
    {
        if(!mMaterial)
            return;

        Renderer* renderer = Core::GetRenderer();
        if(mView == INVALID_RENDER_VIEW)
        {
            RenderViewDesc desc;
            desc.DebugName = "MaterialPreview";
            desc.Isolated = true;
            mView = renderer->CreateView(desc);
        }

        if(!mPreviewVisible || mPreviewSize.x < 1.0f || mPreviewSize.y < 1.0f)
            return;

        EnsurePreviewMesh();
        if(mAutoRotate)
            mMeshYaw += ImGui::GetIO().DeltaTime * 0.6f;

        glm::mat4 transform = glm::rotate(glm::mat4(1.0f), mMeshYaw, glm::vec3(0.0f, 1.0f, 0.0f));
        if(mPreviewShape == PreviewShape::PLANE)
            transform = transform * glm::rotate(glm::mat4(1.0f), glm::radians(70.0f), glm::vec3(1.0f, 0.0f, 0.0f)); // Faces the default camera
        renderer->SubmitMesh(transform, mPreviewMesh.Raw(), false, mView);

        // Studio lighting: a warm key light the user rotates + a cool rim light behind the object (relative to the camera)
        const glm::vec3 keyDirection = { std::cos(mLightPitch) * std::sin(mLightYaw), std::sin(mLightPitch), std::cos(mLightPitch) * std::cos(mLightYaw) };
        const glm::vec3 cameraDirection = { std::cos(mPitch) * std::sin(mYaw), std::sin(mPitch), std::cos(mPitch) * std::cos(mYaw) };
        const glm::vec3 rimDirection = glm::normalize(-cameraDirection + glm::vec3(0.0f, 0.6f, 0.0f));

        Light key = {};
        key.PositionType = glm::vec4(-keyDirection, static_cast<float>(LightType::DIRECTIONAL)); // xyz: the direction the light travels
        key.Color = glm::vec3(1.0f, 0.96f, 0.9f);
        key.Intensity = 3.0f;
        renderer->SubmitLight(key, mView);

        Light rim = {};
        rim.PositionType = glm::vec4(-rimDirection, static_cast<float>(LightType::DIRECTIONAL));
        rim.Color = glm::vec3(0.75f, 0.85f, 1.0f);
        rim.Intensity = 1.5f;
        renderer->SubmitLight(rim, mView);

        // Neutral studio ambient (the scene's sky would tint every material blue), no sky behind the object
        Environnment environment;
        environment.ShowSky = false;
        environment.SkyAmbient = glm::vec3(0.50f, 0.53f, 0.58f) * mAmbient;
        environment.HorizonAmbient = glm::vec3(0.42f, 0.42f, 0.44f) * mAmbient;
        environment.GroundAmbient = glm::vec3(0.20f, 0.19f, 0.18f) * mAmbient;
        renderer->SubmitEnvironment(environment, mView);

        renderer->ResizeView(mView, static_cast<Uint>(mPreviewSize.x), static_cast<Uint>(mPreviewSize.y));
        renderer->SetViewCamera(mView, GetPreviewView(), GetPreviewProjection(mPreviewSize.x / mPreviewSize.y), { PREVIEW_NEAR, PREVIEW_FAR });
        renderer->RequestViewRender(mView);
    }

    void MaterialEditorPanel::DrawPreview()
    {
        {
            ImGuiAux::ScopedBoldFont bold;
            mPreviewOpen = ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen);
        }
        if(!mPreviewOpen)
            return; // Not drawn: the view isn't rendered either

        Renderer* renderer = Core::GetRenderer();
        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 16.0f), mPreviewHeight);
        mPreviewSize = size;
        mPreviewVisible = true;

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max(min.x + size.x, min.y + size.y);
        if(mView != INVALID_RENDER_VIEW && renderer->HasViewOutput(mView))
            ImGui::Image(static_cast<ImTextureID>(renderer->GetViewImGuiID(mView)), size);
        else
        {
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(10, 10, 12, 255));
            ImGui::Dummy(size);
        }
        const bool imageHovered = ImGui::IsItemHovered();
        const ImVec2 belowImage = ImGui::GetCursorScreenPos();

        ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 6.0f, max.y - ImGui::GetTextLineHeight() - 4.0f), IM_COL32(200, 200, 200, 170),
                                            "LMB orbit  //  RMB rotate light  //  Wheel zoom  //  F reset");

        // Input item over the image: during a drag it is the active item and owns the mouse (the window doesn't move, other widgets don't react)
        ImGui::SetCursorScreenPos(min);
        ImGui::SetNextItemAllowOverlap(); // The toolbar drawn on top keeps its clicks
        ImGui::InvisibleButton("##PreviewInput", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if(ImGui::IsItemActivated())
            mDragAnchor = ImGui::GetIO().MousePos;
        mDragging = ImGui::IsItemActive();
        if(imageHovered)
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetID("##PreviewInput")); // The wheel zooms, it never scrolls the panel

        DrawPreviewToolbar(min);
        HandlePreviewInput(imageHovered);

        // Splitter: drag to resize the preview
        ImGui::SetCursorScreenPos(belowImage);
        ImGui::InvisibleButton("##PreviewSplitter", ImVec2(size.x, 6.0f));
        const bool splitterActive = ImGui::IsItemActive();
        if(ImGui::IsItemHovered() || splitterActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if(splitterActive)
            mPreviewHeight = glm::clamp(mPreviewHeight + ImGui::GetIO().MouseDelta.y, 120.0f, 900.0f);
        const float splitterY = belowImage.y + 3.0f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x + size.x * 0.4f, splitterY), ImVec2(min.x + size.x * 0.6f, splitterY),
                                            ImGui::GetColorU32(splitterActive ? ImGuiCol_SeparatorActive : ImGuiCol_Separator), 2.0f);
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
    }

    void MaterialEditorPanel::DrawPreviewToolbar(const ImVec2& min)
    {
        ImGui::SetCursorScreenPos(ImVec2(min.x + 4.0f, min.y + 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(15, 15, 15, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

        for(Uint i = 0; i < static_cast<Uint>(PreviewShape::COUNT); i++)
        {
            const bool active = mPreviewShape == static_cast<PreviewShape>(i);
            if(active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGuiAux::Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiAux::Colors::Selection);
            }
            if(ImGui::SmallButton(sShapeNames[i]) && !active)
            {
                const bool cubeChanged = (mPreviewShape == PreviewShape::CUBE) != (static_cast<PreviewShape>(i) == PreviewShape::CUBE);
                mPreviewShape = static_cast<PreviewShape>(i);
                if(cubeChanged)
                    mDistance = mPreviewShape == PreviewShape::CUBE ? 2.6f : 2.0f;
            }
            if(active)
                ImGui::PopStyleColor(2);
            ImGui::SameLine();
        }

        ImGui::Dummy(ImVec2(6.0f, 0.0f));
        ImGui::SameLine();
        if(mAutoRotate)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGuiAux::Colors::Selection);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiAux::Colors::Selection);
        }
        const bool toggleRotate = ImGui::SmallButton("Auto-Rotate");
        if(mAutoRotate)
            ImGui::PopStyleColor(2);
        if(toggleRotate)
            mAutoRotate = !mAutoRotate;

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(6.0f, 0.0f));
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(15, 15, 15, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 0.0f)); // As tall as the small buttons
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("##Ambient", &mAmbient, 0.0f, 3.0f, "Ambient %.2f", ImGuiSliderFlags_AlwaysClamp);
        ImGuiAux::DelayedToolTip("Preview only: strength of the sky/ground fill light");
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    void MaterialEditorPanel::HandlePreviewInput(bool hovered)
    {
        ImGuiIO& io = ImGui::GetIO();
        if(hovered && io.MouseWheel != 0.0f)
            mDistance = glm::clamp(mDistance * (io.MouseWheel > 0.0f ? 0.9f : 1.1f), 0.8f, 20.0f);

        if(mDragging)
        {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                mYaw -= io.MouseDelta.x * 0.01f;
                mPitch = glm::clamp(mPitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
            }
            else
            {
                mLightYaw -= io.MouseDelta.x * 0.01f;
                mLightPitch = glm::clamp(mLightPitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
            }

            // Hidden cursor goes back to where the drag started every frame
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            if(io.MousePos.x != mDragAnchor.x || io.MousePos.y != mDragAnchor.y)
            {
                Input::SetMousePosition(mDragAnchor.x, mDragAnchor.y);
                io.MousePos = mDragAnchor;
            }
        }

        if(hovered && !mDragging && !io.WantTextInput && !io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false))
            ResetPreviewCamera();
    }

} // namespace Surge
