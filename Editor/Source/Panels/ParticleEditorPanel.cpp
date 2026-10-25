// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ParticleEditorPanel.hpp"
#include "Panels/ContentBrowserPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Core/Input/Input.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Asset/Serializer/ParticleEffect/ParticleEffectSerializer.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Editor.hpp"
#include "Gizmos/GizmoIcons.hpp"
#include "Gizmos/ParticleGizmos.hpp"
#include "StatusBar/EditorStatusBar.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Utility/ParticleWidgets.hpp"

#include "SurgeMath/Math.hpp"

#include <imgui_stdlib.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Surge
{
    namespace
    {
        constexpr float PREVIEW_FOV = 45.0f;
        constexpr float PREVIEW_NEAR = 0.05f;
        constexpr float PREVIEW_FAR = 500.0f;
        constexpr float LABEL_WIDTH = 140.0f;
        constexpr int PREVIEW_GIZMO_ID = 0x5EF0; // ImGuizmo ID, the Scene views use their index
        constexpr ImU32 SHAPE_COLOR = GizmoIcons::PARTICLES_COLOR;
        constexpr ImU32 COLLISION_COLOR = IM_COL32(90, 220, 200, 255);
        constexpr ImU32 BOUNDS_COLOR = IM_COL32(255, 210, 90, 255);

        constexpr std::array<const char*, 2> sSpaceNames = { "Local", "World" };
        constexpr std::array<const char*, 7> sShapeNames = { "Point", "Sphere", "Hemisphere", "Cone", "Box", "Circle", "Edge" };
        constexpr std::array<const char*, 2> sSheetTimingNames = { "Over Lifetime", "Frames Per Second" };
        constexpr std::array<const char*, 3> sSubEmitterEventNames = { "Birth", "Death", "Collision" };
        constexpr std::array<const char*, 4> sRenderModeNames = { "Billboard", "Stretched", "Horizontal", "Vertical" };
        constexpr std::array<const char*, 4> sSortModeNames = { "None", "By Distance", "Oldest First", "Youngest First" };

        bool DragFloatRow(const char* label, float* value, float speed, float min = 0.0f, float max = 0.0f, const char* format = "%.2f")
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool changed = ImGui::DragFloat("##v", value, speed, min, max, format, (min != 0.0f || max != 0.0f) ? ImGuiSliderFlags_AlwaysClamp : 0);
            ImGui::PopID();
            return changed;
        }

        bool SliderFloatRow(const char* label, float* value, float min, float max, const char* format = "%.2f")
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool changed = ImGui::SliderFloat("##v", value, min, max, format, ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopID();
            return changed;
        }

        template<typename T>
        bool DragIntRow(const char* label, T* value, int min, int max)
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            int v = static_cast<int>(*value);
            const bool changed = ImGui::DragInt("##v", &v, 0.2f, min, max, "%d", ImGuiSliderFlags_AlwaysClamp);
            if(changed)
                *value = static_cast<T>(v);
            ImGui::PopID();
            return changed;
        }

        bool Vec3Row(const char* label, glm::vec3* value, float speed)
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            const bool changed = ImGui::DragFloat3("##v", glm::value_ptr(*value), speed, 0.0f, 0.0f, "%.2f");
            ImGui::PopID();
            return changed;
        }

        bool CheckboxRow(const char* label, bool* value)
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            const bool changed = ImGui::Checkbox("##v", value);
            ImGui::PopID();
            return changed;
        }

        template<typename T, size_t N>
        bool ComboRow(const char* label, T& value, const std::array<const char*, N>& names)
        {
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            bool changed = false;
            const size_t current = static_cast<size_t>(value);
            if(ImGui::BeginCombo("##v", current < N ? names[current] : "?"))
            {
                for(size_t i = 0; i < N; i++)
                {
                    if(ImGui::Selectable(names[i], i == current) && i != current)
                    {
                        value = static_cast<T>(i);
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();
            return changed;
        }

        void HintRow(const char* text)
        {
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", text);
            ImGui::PopTextWrapPos();
        }

        // Texture2D slot: drop from the Content Browser, x clears it
        bool TextureRow(const char* label, Ref<Asset>& texture)
        {
            bool changed = false;
            AssetManager* am = Core::GetAssetManager();
            ImGui::PushID(label);
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();

            const String text = texture ? Filesystem::GetFilenameWithExt(am->GetMetadata(texture->GetID()).RelativePath) : String("Default (soft particle)");
            const float clearWidth = texture ? ImGui::GetFrameHeight() + 4.0f : 0.0f;
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
            ImGui::Button(text.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clearWidth, 0.0f));
            ImGui::PopStyleVar();
            ImGuiAux::DelayedToolTip("Drop a texture from the Content Browser");
            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                    if(am->GetMetadata(dropped).Type == AssetType::TEXTURE2D)
                    {
                        texture = am->Load<Texture2D>(dropped);
                        changed = true;
                    }
                    else
                        EditorStatusBar::Get().Notify(Severity::Warn, "Particles need a texture (TEXTURE2D) asset");
                }
                ImGui::EndDragDropTarget();
            }

            if(texture)
            {
                ImGui::SameLine(0.0f, 4.0f);
                if(ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                {
                    texture = nullptr;
                    changed = true;
                }
                ImGuiAux::DelayedToolTip("Back to the default particle");
            }
            ImGui::PopID();
            return changed;
        }

        bool BeginProperties()
        {
            if(!ImGui::BeginTable("##Properties", 2, ImGuiTableFlags_SizingFixedFit))
                return false;
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, LABEL_WIDTH);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }

        // Clip space -> panel pixels (Vulkan: y down, like ImGui)
        bool ProjectToPanel(const glm::mat4& viewProjection, const glm::vec3& p, const ImVec2& min, const ImVec2& size, ImVec2& out)
        {
            const glm::vec4 clip = viewProjection * glm::vec4(p, 1.0f);
            if(clip.w < PREVIEW_NEAR)
                return false;
            out = ImVec2(min.x + (clip.x / clip.w * 0.5f + 0.5f) * size.x, min.y + (clip.y / clip.w * 0.5f + 0.5f) * size.y);
            return true;
        }
    } // namespace

    void ParticleEditorPanel::Init([[maybe_unused]] void* panelInitArgs)
    {
        mCode = GetStaticCode();
        mGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
    }

    void ParticleEditorPanel::Shutdown()
    {
        SetFullscreen(false);
        if(mView != INVALID_RENDER_VIEW)
        {
            Core::GetRenderer()->DestroyView(mView);
            mView = INVALID_RENDER_VIEW;
        }
        mPreview = ParticleSystemComponent {};
        mEffect = nullptr;
    }

    AssetID ParticleEditorPanel::CreateEffectAsset(const std::filesystem::path& absoluteDirectory, const String& baseName)
    {
        AssetManager* am = Core::GetAssetManager();
        Filesystem::CreateOrEnsureDirectories(absoluteDirectory);

        const char* extension = GetExtensionFromAssetType(AssetType::PARTICLE_EFFECT);
        std::filesystem::path path = absoluteDirectory / (baseName + extension);
        for(int count = 1; std::filesystem::exists(path); count++)
            path = absoluteDirectory / std::format("{} ({}){}", baseName, count, extension);

        const String relativePath = Filesystem::GetRelativePath(path, am->GetAssetsDirectory()).generic_string();
        Ref<ParticleEffect> effect = am->Create<ParticleEffect>(relativePath);
        return effect ? effect->GetID() : AssetID(UUID::INVALID);
    }

    //////////////////////////////////////////////////////////////////////////
    // Document
    //////////////////////////////////////////////////////////////////////////

    void ParticleEditorPanel::OpenEffect(AssetID effectID)
    {
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        editor->GetPanelManager().GetAllPanels()[mCode].Show = true;
        mFocusWindow = true;

        if(mEffect && mEffect->GetID() == effectID)
            return;

        auto open = [this, effectID]() {
            Ref<ParticleEffect> effect = Core::GetAssetManager()->Load<ParticleEffect>(effectID);
            if(!effect)
            {
                Log<Severity::Error>("[ParticleEditor] Failed to load particle effect {}", effectID.Get());
                return;
            }

            if(mEffect && mUnsaved)
                Revert(); // The discarded edits live in the old asset, scene instances must not keep them

            mEffect = effect;
            mSelectedEmitter = 0;
            mUnsaved = false;
            mDocumentChanged = false;
            mLayoutChanged = false;
            mUndoStack.clear();
            mRedoStack.clear();
            mCommitted = ParticleEffectSerializer::ToJson(*mEffect);

            mPreview = ParticleSystemComponent {};
            mPreview.Effect = mEffect;
            mPreview.Culling = ParticleCullingMode::ALWAYS_SIMULATE;
            mPreview.Runtime.Play();
            mPreviewPaused = false;
        };

        if(mEffect && mUnsaved)
            ImGuiAux::ShowConfirmationBox("Unsaved Particle Effect", "The open particle effect has unsaved changes.\nDiscard them and open the other effect?", open);
        else
            open();
    }

    void ParticleEditorPanel::CloseEffect()
    {
        auto close = [this]() {
            if(mUnsaved)
                Revert();
            mEffect = nullptr;
            mPreview = ParticleSystemComponent {};
            mUndoStack.clear();
            mRedoStack.clear();
            mCommitted.clear();
            mUnsaved = false;
            mDocumentChanged = false;
        };

        if(mEffect && mUnsaved)
            ImGuiAux::ShowConfirmationBox("Unsaved Particle Effect", "Discard the changes of the open particle effect?", close);
        else
            close();
    }

    void ParticleEditorPanel::Save()
    {
        if(!mEffect)
            return;

        Core::GetAssetManager()->Save(mEffect->GetID());
        mUnsaved = false;
        const String& path = Core::GetAssetManager()->GetMetadata(mEffect->GetID()).RelativePath;
        Log<Severity::Info>("[ParticleEditor] Saved {}", path);
        EditorStatusBar::Get().Notify(Severity::Info, std::format("Saved {}", Filesystem::GetFilenameWithExt(path)), 2.0f);
    }

    void ParticleEditorPanel::Revert()
    {
        if(!mEffect)
            return;

        AssetManager* am = Core::GetAssetManager();
        String json;
        if(Filesystem::ReadTextFile(am->GetAbsolutePath(am->GetMetadata(mEffect->GetID()).RelativePath), json))
            ParticleEffectSerializer::FromJson(*am, json, *mEffect);
        mUnsaved = false;
    }

    void ParticleEditorPanel::MarkChanged(bool layoutChanged)
    {
        mDocumentChanged = true;
        mLayoutChanged |= layoutChanged;
        mUnsaved = true;
        mEffect->MarkChanged(layoutChanged);
    }

    void ParticleEditorPanel::CommitUndoIfIdle()
    {
        if(!mEffect || !mDocumentChanged)
            return;

        // Coalesce: a whole drag/typing session becomes a single undo step
        if(ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left))
            return;

        mUndoStack.push_back(std::move(mCommitted));
        if(mUndoStack.size() > MAX_UNDO_STEPS)
            mUndoStack.erase(mUndoStack.begin());
        mRedoStack.clear();
        mCommitted = ParticleEffectSerializer::ToJson(*mEffect);
        mDocumentChanged = false;
        mLayoutChanged = false;
    }

    void ParticleEditorPanel::RestoreState(const String& json)
    {
        ParticleEffectSerializer::FromJson(*Core::GetAssetManager(), json, *mEffect); // Marks the layout changed: instances restart with the restored settings
        mSelectedEmitter = std::clamp(mSelectedEmitter, 0, std::max(static_cast<int>(mEffect->Emitters.size()) - 1, 0));
        mDocumentChanged = false;
        mLayoutChanged = false;
        mUnsaved = true;
    }

    void ParticleEditorPanel::Undo()
    {
        if(mDocumentChanged) // A pending edit becomes its own step first
        {
            mUndoStack.push_back(std::move(mCommitted));
            mCommitted = ParticleEffectSerializer::ToJson(*mEffect);
            mDocumentChanged = false;
        }
        if(mUndoStack.empty())
            return;

        mRedoStack.push_back(mCommitted);
        mCommitted = std::move(mUndoStack.back());
        mUndoStack.pop_back();
        RestoreState(mCommitted);
    }

    void ParticleEditorPanel::Redo()
    {
        if(mRedoStack.empty())
            return;

        mUndoStack.push_back(mCommitted);
        mCommitted = std::move(mRedoStack.back());
        mRedoStack.pop_back();
        RestoreState(mCommitted);
    }

    void ParticleEditorPanel::HandleShortcuts()
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

    //////////////////////////////////////////////////////////////////////////
    // Emitter list edits, sub-emitter references follow the emitters they point to
    //////////////////////////////////////////////////////////////////////////

    void ParticleEditorPanel::AddEmitter()
    {
        if(mEffect->Emitters.size() >= ParticleEffect::MAX_EMITTERS)
            return;

        ParticleEmitterDesc& emitter = mEffect->Emitters.emplace_back();
        emitter.Name = std::format("Emitter {}", mEffect->Emitters.size());
        mSelectedEmitter = static_cast<int>(mEffect->Emitters.size()) - 1;
        MarkChanged(true);
    }

    void ParticleEditorPanel::DuplicateEmitter(int index)
    {
        if(mEffect->Emitters.size() >= ParticleEffect::MAX_EMITTERS)
            return;

        ParticleEmitterDesc copy = mEffect->Emitters[index];
        copy.Name += " (Copy)";
        for(ParticleEmitterDesc& emitter : mEffect->Emitters)
        {
            for(ParticleSubEmitter& entry : emitter.SubEmitters.Entries)
                if(entry.EmitterIndex > index)
                    entry.EmitterIndex++;
        }
        for(ParticleSubEmitter& entry : copy.SubEmitters.Entries)
            if(entry.EmitterIndex > index)
                entry.EmitterIndex++;

        mEffect->Emitters.insert(mEffect->Emitters.begin() + index + 1, std::move(copy));
        mSelectedEmitter = index + 1;
        MarkChanged(true);
    }

    void ParticleEditorPanel::DeleteEmitter(int index)
    {
        mEffect->Emitters.erase(mEffect->Emitters.begin() + index);
        for(ParticleEmitterDesc& emitter : mEffect->Emitters)
        {
            for(ParticleSubEmitter& entry : emitter.SubEmitters.Entries)
            {
                if(entry.EmitterIndex == index)
                    entry.EmitterIndex = -1;
                else if(entry.EmitterIndex > index)
                    entry.EmitterIndex--;
            }
        }
        mSelectedEmitter = std::clamp(mSelectedEmitter, 0, std::max(static_cast<int>(mEffect->Emitters.size()) - 1, 0));
        MarkChanged(true);
    }

    void ParticleEditorPanel::MoveEmitter(int index, int direction)
    {
        const int other = index + direction;
        if(other < 0 || other >= static_cast<int>(mEffect->Emitters.size()))
            return;

        std::swap(mEffect->Emitters[index], mEffect->Emitters[other]);
        for(ParticleEmitterDesc& emitter : mEffect->Emitters)
        {
            for(ParticleSubEmitter& entry : emitter.SubEmitters.Entries)
            {
                if(entry.EmitterIndex == index)
                    entry.EmitterIndex = other;
                else if(entry.EmitterIndex == other)
                    entry.EmitterIndex = index;
            }
        }
        mSelectedEmitter = other;
        MarkChanged(true);
    }

    //////////////////////////////////////////////////////////////////////////
    // Panel
    //////////////////////////////////////////////////////////////////////////

    void ParticleEditorPanel::Render(bool* show)
    {
        mOwnsShortcuts = false;
        mPreviewVisible = false;

        if(!*show)
            SetFullscreen(false); // Closed from the menu while fullscreen: the title bar comes back

        if(*show)
        {
            ImGuiWindowFlags windowFlags = 0;
            if(mFullscreen)
            {
                // Same as a fullscreen viewport: the whole Editor window, the title and status bars step aside
                const ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->Pos);
                ImGui::SetNextWindowSize(viewport->Size);
                ImGui::SetNextWindowViewport(viewport->ID);
                windowFlags |= ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            }
            else if(mRestoreDockAfterFullscreen)
            {
                if(mDockBeforeFullscreen)
                    ImGui::SetNextWindowDockID(mDockBeforeFullscreen, ImGuiCond_Always);
                else
                {
                    ImGui::SetNextWindowPos(mWindowPos);
                    ImGui::SetNextWindowSize(mWindowSize);
                }
                mRestoreDockAfterFullscreen = false;
                mFocusWindow = true;
            }

            if(mFocusWindow)
            {
                ImGui::SetNextWindowFocus();
                mFocusWindow = false;
            }

            if(ImGui::Begin(PanelCodeToString(mCode), mFullscreen ? nullptr : show, windowFlags))
            {
                const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                mOwnsShortcuts = mEffect && focused;
                if(!mFullscreen)
                {
                    mDockID = ImGui::GetWindowDockID();
                    mWindowPos = ImGui::GetWindowPos();
                    mWindowSize = ImGui::GetWindowSize();
                }

                // F11 toggles like the viewports (only while hovered, a hovered viewport toggles itself), Esc leaves
                if(!ImGui::GetIO().WantTextInput)
                {
                    if(ImGui::IsKeyPressed(ImGuiKey_F11, false) && (mFullscreen || ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)))
                        SetFullscreen(!mFullscreen);
                    else if(mFullscreen && ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
                        SetFullscreen(false);
                }

                if(!mEffect)
                    DrawEmptyState();
                else
                {
                    DrawToolbar();
                    ImGui::Separator();

                    if(ImGui::BeginTable("##ParticleEditorSplit", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, ImGui::GetContentRegionAvail().y)))
                    {
                        ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch, 0.45f);
                        ImGui::TableSetupColumn("Emitter", ImGuiTableColumnFlags_WidthStretch, 0.55f);
                        ImGui::TableNextRow();

                        ImGui::TableSetColumnIndex(0);
                        if(ImGui::BeginChild("##PreviewColumn"))
                        {
                            DrawPreview(std::max(ImGui::GetContentRegionAvail().y * 0.6f, 120.0f));
                            DrawEmitterList();
                        }
                        ImGui::EndChild();

                        ImGui::TableSetColumnIndex(1);
                        if(ImGui::BeginChild("##EmitterColumn"))
                        {
                            if(mSelectedEmitter >= 0 && mSelectedEmitter < static_cast<int>(mEffect->Emitters.size()))
                                DrawEmitter(mEffect->Emitters[mSelectedEmitter]);
                        }
                        ImGui::EndChild();

                        ImGui::EndTable();
                    }

                    if(focused)
                        HandleShortcuts();
                }
            }
            ImGui::End();
        }

        CommitUndoIfIdle();
        UpdatePreview();
    }

    void ParticleEditorPanel::DrawEmptyState()
    {
        ImGui::Dummy(ImVec2(0.0f, 10.0f));
        {
            ImGuiAux::ScopedBoldFont bold(20.0f);
            ImGuiAux::TextCentered("No particle effect open");
        }
        ImGui::Dummy(ImVec2(0.0f, 6.0f));
        ImGui::TextWrapped("Double click a .spfx file in the Content Browser, drop one on the button below, or create a new effect. "
                           "Add a Particle System component to an entity and assign the effect to see it in the scene.");
        ImGui::Dummy(ImVec2(0.0f, 6.0f));

        ImGuiAux::ScopedBoldFont bold;
        if(ImGui::Button("Create New Particle Effect", ImVec2(-FLT_MIN, 40.0f)))
        {
            const String assetsDir = Core::GetAssetManager()->GetAssetsDirectory();
            const AssetID id = CreateEffectAsset(std::filesystem::path(assetsDir) / "Effects");
            if(id)
                OpenEffect(id);
        }

        if(ImGui::BeginDragDropTarget())
        {
            if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
            {
                const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                if(Core::GetAssetManager()->GetMetadata(dropped).Type == AssetType::PARTICLE_EFFECT)
                    OpenEffect(dropped);
            }
            ImGui::EndDragDropTarget();
        }
    }

    void ParticleEditorPanel::DrawToolbar()
    {
        const String fileName = Filesystem::GetFilenameWithExt(Core::GetAssetManager()->GetMetadata(mEffect->GetID()).RelativePath);
        {
            ImGuiAux::ScopedBoldFont bold(18.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%s%s", fileName.c_str(), mUnsaved ? " *" : "");
        }

        ImGui::SameLine();
        if(ImGui::Button("Save"))
            Save();
        ImGuiAux::DelayedToolTip("Ctrl+S");

        ImGui::SameLine();
        ImGui::BeginDisabled(!mUnsaved);
        if(ImGui::Button("Revert"))
            ImGuiAux::ShowConfirmationBox("Revert Particle Effect", "Reload the effect from its file and drop every unsaved change?", [this]() {
                Revert();
                mUndoStack.clear();
                mRedoStack.clear();
                mCommitted = ParticleEffectSerializer::ToJson(*mEffect);
                mDocumentChanged = false;
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

        ImGui::SameLine();
        if(ImGui::Button("Close"))
            CloseEffect();

        ImGui::SameLine();
        if(ImGui::Button(mFullscreen ? "Exit Fullscreen" : "Fullscreen"))
            SetFullscreen(!mFullscreen);
        ImGuiAux::DelayedToolTip(mFullscreen ? "F11 / Esc" : "F11 while the panel is hovered, Esc leaves");
    }

    void ParticleEditorPanel::SetFullscreen(bool fullscreen)
    {
        if(mFullscreen == fullscreen)
            return;

        mFullscreen = fullscreen;
        if(fullscreen)
        {
            mDockBeforeFullscreen = mDockID;
            mFocusWindow = true;
        }
        else
            mRestoreDockAfterFullscreen = true;

        static_cast<Editor*>(Core::GetClient())->ShowTitlebar(!fullscreen);
    }

    //////////////////////////////////////////////////////////////////////////
    // Preview
    //////////////////////////////////////////////////////////////////////////

    glm::mat4 ParticleEditorPanel::GetPreviewView() const
    {
        const glm::vec3 direction = { std::cos(mPitch) * std::sin(mYaw), std::sin(mPitch), std::cos(mPitch) * std::cos(mYaw) };
        return glm::lookAt(mTarget + direction * mDistance, mTarget, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    glm::mat4 ParticleEditorPanel::GetPreviewProjection(float aspect) const
    {
        glm::mat4 projection = glm::perspective(glm::radians(PREVIEW_FOV), aspect, PREVIEW_NEAR, PREVIEW_FAR);
        projection[1][1] *= -1.0f; // Vulkan clip space, like EditorCamera
        return projection;
    }

    void ParticleEditorPanel::UpdatePreview()
    {
        if(!mEffect)
            return;

        Renderer* renderer = Core::GetRenderer();
        if(mView == INVALID_RENDER_VIEW)
        {
            RenderViewDesc desc;
            desc.DebugName = "ParticlePreview";
            desc.Isolated = true;
            mView = renderer->CreateView(desc);
        }

        // Simulated even while the panel is hidden: scene instances share the asset, the preview stays in sync with them
        const float deltaTime = mPreviewPaused ? 0.0f : ImGui::GetIO().DeltaTime * mPreviewSpeed;
        glm::mat4 transform = glm::mat4(1.0f);
        if(mMoveEmitter)
        {
            mMoveTime += deltaTime;
            transform = glm::translate(glm::mat4(1.0f), glm::vec3(std::cos(mMoveTime) * 3.0f, 0.0f, std::sin(mMoveTime) * 3.0f));
        }

        mPreviewTransform = transform;

        ParticleBudget budget;
        budget.MaxParticles = renderer->GetRenderGraphBlackBoard().Particles.MaxParticles;
        mPreview.Runtime.Update(mPreview, transform, deltaTime, nullptr, budget);

        // One-shot effects play again after a short pause
        if(mLoopPreview && !mPreviewPaused && mPreview.Runtime.IsFinished())
        {
            mRestartTimer += deltaTime;
            if(mRestartTimer > 0.6f)
            {
                mRestartTimer = 0.0f;
                mPreview.Runtime.Restart();
            }
        }

        if(!mPreviewVisible || mPreviewSize.x < 1.0f || mPreviewSize.y < 1.0f)
            return;

        mPreview.Runtime.Submit(*renderer, mView);
        renderer->ResizeView(mView, static_cast<Uint>(mPreviewSize.x), static_cast<Uint>(mPreviewSize.y));
        renderer->SetViewCamera(mView, GetPreviewView(), GetPreviewProjection(mPreviewSize.x / mPreviewSize.y), { PREVIEW_NEAR, PREVIEW_FAR });
        renderer->RequestViewRender(mView);
    }

    void ParticleEditorPanel::DrawPreviewGrid(const ImVec2& min, const ImVec2& size, const glm::mat4& viewProjection)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        constexpr int extent = 5;
        for(int i = -extent; i <= extent; i++)
        {
            const ImU32 color = i == 0 ? IM_COL32(255, 255, 255, 70) : IM_COL32(255, 255, 255, 28);
            ImVec2 a, b;
            if(ProjectToPanel(viewProjection, glm::vec3(static_cast<float>(i), 0.0f, -extent), min, size, a) && ProjectToPanel(viewProjection, glm::vec3(static_cast<float>(i), 0.0f, extent), min, size, b))
                drawList->AddLine(a, b, color);
            if(ProjectToPanel(viewProjection, glm::vec3(-extent, 0.0f, static_cast<float>(i)), min, size, a) && ProjectToPanel(viewProjection, glm::vec3(extent, 0.0f, static_cast<float>(i)), min, size, b))
                drawList->AddLine(a, b, color);
        }
    }

    void ParticleEditorPanel::DrawPreview(float height)
    {
        Renderer* renderer = Core::GetRenderer();
        const ImVec2 size(std::max(ImGui::GetContentRegionAvail().x, 16.0f), std::max(height - ImGui::GetFrameHeightWithSpacing() * 1.2f, 16.0f));
        mPreviewSize = size;
        mPreviewVisible = true;

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
        if(mView != INVALID_RENDER_VIEW && renderer->HasViewOutput(mView))
            ImGui::Image(static_cast<ImTextureID>(renderer->GetViewImGuiID(mView)), size);
        else
        {
            ImGui::GetWindowDrawList()->AddRectFilled(min, max, IM_COL32(10, 10, 12, 255));
            ImGui::Dummy(size);
        }
        const bool imageHovered = ImGui::IsItemHovered();
        const ImVec2 belowImage = ImGui::GetCursorScreenPos();

        // Wires first, then the manipulator and the toolbar on top of them
        const glm::mat4 view = GetPreviewView();
        const glm::mat4 projection = GetPreviewProjection(size.x / size.y);
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->PushClipRect(min, max, true);
        if(mShowGrid)
            DrawPreviewGrid(min, size, projection * view);
        DrawPreviewGizmos(min, size, view, projection);

        // Stats in the bottom corner, the toolbar takes the top one
        {
            const String stats = std::format("{} particles  |  {:.2f} s", mPreview.Runtime.GetParticleCount(), mPreview.Runtime.GetTime());
            drawList->AddText(ImVec2(min.x + 6.0f, max.y - ImGui::GetTextLineHeight() - 4.0f), IM_COL32(220, 220, 220, 220), stats.c_str());
        }
        drawList->PopClipRect();

        if (mGizmoOperation != -1)
            ManipulateSelectedShape(min, size, view, projection);

        // Input item over the image: during a camera drag it is the active item and owns the mouse (a floating window doesn't
        // move, other widgets don't react). Left out while the manipulator is under the mouse: ImGuizmo only starts a drag on
        // a click that no ImGui item hovers
        if(!mGizmoActive || mOrbiting)
        {
            ImGui::SetCursorScreenPos(min);
            ImGui::SetNextItemAllowOverlap(); // The toolbar drawn on top keeps its clicks
            ImGui::InvisibleButton("##PreviewInput", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            if(ImGui::IsItemActivated())
                mDragAnchor = ImGui::GetIO().MousePos;
            mOrbiting = ImGui::IsItemActive();
        }
        else
            mOrbiting = false;
        if(imageHovered)
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetID("##PreviewInput")); // The wheel zooms, it never scrolls the column

        DrawPreviewToolbar(min);
        HandlePreviewInput(imageHovered);

        // Playback
        ImGui::SetCursorScreenPos(belowImage);
        const float buttonSize = ImGui::GetFrameHeight();
        const ImVec4 glyphColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
        if(GizmoIcons::IconButton("##PlayPause", mPreviewPaused ? GizmoIcon::PLAY : GizmoIcon::PAUSE, ImVec2(buttonSize, buttonSize), glyphColor))
            mPreviewPaused = !mPreviewPaused;
        ImGuiAux::DelayedToolTip(mPreviewPaused ? "Play" : "Pause");
        ImGui::SameLine(0.0f, 2.0f);
        if(GizmoIcons::IconButton("##Restart", GizmoIcon::STEP, ImVec2(buttonSize, buttonSize), glyphColor))
        {
            mPreview.Runtime.Restart();
            mPreviewPaused = false;
        }
        ImGuiAux::DelayedToolTip("Restart");
        ImGui::SameLine(0.0f, 2.0f);
        if(GizmoIcons::IconButton("##Stop", GizmoIcon::STOP, ImVec2(buttonSize, buttonSize), glyphColor))
            mPreview.Runtime.Stop();
        ImGuiAux::DelayedToolTip("Stop emitting (the live particles finish)");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderFloat("##Speed", &mPreviewSpeed, 0.05f, 3.0f, "Speed %.2fx");
        ImGui::SameLine();
        ImGui::Checkbox("Loop", &mLoopPreview);
        ImGuiAux::DelayedToolTip("Plays one-shot effects again once they finished");
        ImGui::SameLine();
        ImGui::Checkbox("Move", &mMoveEmitter);
        ImGuiAux::DelayedToolTip("Moves the emitter on a circle: world space trails, rate over distance");
    }

    void ParticleEditorPanel::HandlePreviewInput(bool hovered)
    {
        // Orbit camera: left drag orbits, right/middle drag pans, wheel zooms
        ImGuiIO& io = ImGui::GetIO();
        if(hovered && io.MouseWheel != 0.0f)
            mDistance = glm::clamp(mDistance * (io.MouseWheel > 0.0f ? 0.88f : 1.12f), 0.5f, 200.0f);

        if(mOrbiting)
        {
            if(ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                mYaw -= io.MouseDelta.x * 0.01f;
                mPitch = glm::clamp(mPitch + io.MouseDelta.y * 0.01f, -1.5f, 1.5f);
            }
            else
            {
                const glm::mat4 view = GetPreviewView();
                const glm::vec3 right = { view[0][0], view[1][0], view[2][0] };
                const glm::vec3 up = { view[0][1], view[1][1], view[2][1] };
                const float scale = mDistance * 0.0015f;
                mTarget += (-right * io.MouseDelta.x + up * io.MouseDelta.y) * scale;
            }

            // Captured: the hidden cursor goes back to where the drag started every frame, so a drag never stops at the edge of
            // the preview or the screen. ImGui keeps MousePos as next frame's MousePosPrev: the next delta starts at the anchor
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
            if(io.MousePos.x != mDragAnchor.x || io.MousePos.y != mDragAnchor.y)
            {
                Input::SetMousePosition(mDragAnchor.x, mDragAnchor.y);
                io.MousePos = mDragAnchor;
            }
        }

        if(!hovered || !mOwnsShortcuts || io.WantTextInput || io.KeyCtrl || mOrbiting || ImGuizmo::IsUsingAny())
            return;

        if(ImGui::IsKeyPressed(ImGuiKey_Q, false))
            mGizmoOperation = -1;
        if(ImGui::IsKeyPressed(ImGuiKey_W, false))
            mGizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
        if(ImGui::IsKeyPressed(ImGuiKey_E, false))
            mGizmoOperation = ImGuizmo::OPERATION::ROTATE;
        if(ImGui::IsKeyPressed(ImGuiKey_R, false))
            mGizmoOperation = ImGuizmo::OPERATION::SCALE;
        if(ImGui::IsKeyPressed(ImGuiKey_T, false))
            mGizmoOperation = ImGuizmo::OPERATION::UNIVERSAL;
        if(ImGui::IsKeyPressed(ImGuiKey_X, false))
            mGizmoWorldSpace = !mGizmoWorldSpace;
        if(ImGui::IsKeyPressed(ImGuiKey_F, false))
            FrameSelectedEmitter();
    }

    //////////////////////////////////////////////////////////////////////////
    // Preview gizmos
    //////////////////////////////////////////////////////////////////////////

    ParticleEmitterDesc* ParticleEditorPanel::GetSelectedEmitter()
    {
        if(!mEffect || mSelectedEmitter < 0 || mSelectedEmitter >= static_cast<int>(mEffect->Emitters.size()))
            return nullptr;
        return &mEffect->Emitters[mSelectedEmitter];
    }

    void ParticleEditorPanel::FrameSelectedEmitter()
    {
        const ParticleEmitterDesc* emitter = GetSelectedEmitter();
        if(!emitter)
            return;

        const ParticleShapeModule& shape = emitter->Shape;
        float extent = 1.0f;
        if(shape.Enabled && shape.Type == ParticleShapeType::BOX)
            extent = glm::length(shape.BoxSize) * 0.5f;
        else if(shape.Enabled && shape.Type != ParticleShapeType::POINT)
            extent = shape.Radius;

        // The shape plus some room for the particles leaving it
        mTarget = glm::vec3(mPreviewTransform * glm::vec4(shape.Enabled ? shape.Position : glm::vec3(0.0f), 1.0f));
        mDistance = glm::clamp((std::max(extent, 0.5f) + 1.5f) * 2.6f, 0.5f, 200.0f);
    }

    void ParticleEditorPanel::DrawPreviewGizmos(const ImVec2& min, const ImVec2& size, const glm::mat4& view, const glm::mat4& projection)
    {
        mCanvas.Begin(ImGui::GetWindowDrawList(), view, projection, PREVIEW_NEAR, min, size);

        // Every emitter faint, the selected one on top at full strength
        if(mShowShapes)
        {
            for(int i = 0; i < static_cast<int>(mEffect->Emitters.size()); i++)
            {
                const ParticleEmitterDesc& emitter = mEffect->Emitters[i];
                if(i == mSelectedEmitter || !emitter.Enabled || !emitter.Shape.Enabled || emitter.IsSubEmitterTarget)
                    continue;
                ParticleGizmos::DrawEmitterShape(mCanvas, emitter.Shape, mPreviewTransform, GizmoCanvas::ScaleAlpha(SHAPE_COLOR, 0.35f), 1.0f);
            }
        }

        const ParticleEmitterDesc* selected = GetSelectedEmitter();
        if(selected && selected->Enabled)
        {
            const float alpha = selected->IsSubEmitterTarget ? 0.5f : 1.0f; // Sub-emitters spawn on other particles, the shape only shows its size
            if(selected->Shape.Enabled)
            {
                ParticleGizmos::DrawEmitterShape(mCanvas, selected->Shape, mPreviewTransform, GizmoCanvas::ScaleAlpha(SHAPE_COLOR, alpha), 1.75f);
                if(mShowDirection)
                    ParticleGizmos::DrawEmitDirection(mCanvas, selected->Shape, mPreviewTransform, GizmoCanvas::ScaleAlpha(IM_COL32(255, 255, 255, 220), alpha), 1.5f);
            }

            if(mShowCollision && selected->Collision.Enabled)
            {
                const glm::vec3 focus = glm::vec3(mPreviewTransform * glm::vec4(selected->Shape.Position, 1.0f));
                for(Uint p = 0; p < selected->Collision.PlaneCount; p++)
                    ParticleGizmos::DrawCollisionPlane(mCanvas, selected->Collision.Planes[p], focus, 3.0f, COLLISION_COLOR, 1.25f);
            }
        }

        if(mShowBounds)
        {
            glm::vec3 boundsMin, boundsMax;
            if(mPreview.Runtime.GetWorldBounds(boundsMin, boundsMax))
                mCanvas.DrawWireBox(boundsMin, boundsMax, GizmoCanvas::ScaleAlpha(BOUNDS_COLOR, 0.8f), 1.0f);
        }

        if(selected && selected->IsSubEmitterTarget)
        {
            const char* hint = "Sub-emitter: spawns on the particles of another emitter";
            ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 6.0f, min.y + ImGui::GetFrameHeight() + 10.0f), IM_COL32(220, 220, 220, 170), hint);
        }
    }

    void ParticleEditorPanel::ManipulateSelectedShape(const ImVec2& min, const ImVec2& size, const glm::mat4& view, const glm::mat4& projection)
    {
        mGizmoActive = false;
        ParticleEmitterDesc* emitter = GetSelectedEmitter();
        if(!emitter || mGizmoOperation <= 0 || !emitter->Enabled || !emitter->Shape.Enabled)
            return;

        // Scale edits the size of the shape: the box size, or the radius (the axis dragged the most wins)
        ParticleShapeModule& shape = emitter->Shape;
        const bool hasSize = shape.Type != ParticleShapeType::POINT;
        int operation = mGizmoOperation;
        if(!hasSize)
            operation &= ~(ImGuizmo::OPERATION::SCALE | ImGuizmo::OPERATION::SCALEU);
        if(operation == 0)
            return;

        const glm::vec3 shapeSize = shape.Type == ParticleShapeType::BOX ? shape.BoxSize : glm::vec3(shape.Radius);
        const glm::vec3 matrixSize = hasSize ? glm::max(shapeSize, glm::vec3(1e-3f)) : glm::vec3(1.0f);
        glm::mat4 transform = mPreviewTransform * ParticleGizmos::GetShapeTransform(shape) * glm::scale(glm::mat4(1.0f), matrixSize);

        glm::mat4 gizmoProjection = projection;
        gizmoProjection[1][1] *= -1.0f; // ImGuizmo wants the GL convention

        // Same snapping as the Scene views, holding Ctrl inverts the toggle
        const EditorSettings& settings = static_cast<Editor*>(Core::GetClient())->GetSettings();
        const bool snap = settings.SnapEnabled != ImGui::GetIO().KeyCtrl;
        float snapValue = settings.SnapTranslate;
        if(mGizmoOperation == ImGuizmo::OPERATION::ROTATE)
            snapValue = settings.SnapRotate;
        else if(mGizmoOperation == ImGuizmo::OPERATION::SCALE)
            snapValue = settings.SnapScale;
        const float snapValues[3] = { snapValue, snapValue, snapValue };

        ImGuizmo::PushID(PREVIEW_GIZMO_ID);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(min.x, min.y, size.x, size.y);
        ImGuizmo::SetGizmoSizeClipSpace(0.18f);
        ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(gizmoProjection), static_cast<ImGuizmo::OPERATION>(operation), mGizmoWorldSpace ? ImGuizmo::WORLD : ImGuizmo::LOCAL,
                             glm::value_ptr(transform), nullptr, snap ? snapValues : nullptr);

        const bool usingGizmo = ImGuizmo::IsUsing();
        mGizmoActive = usingGizmo || ImGuizmo::IsOver();
        if(usingGizmo)
        {
            glm::vec3 position, rotation, scale;
            Math::DecomposeTransform(glm::inverse(mPreviewTransform) * transform, position, rotation, scale);

            // Only what the drag changed: decomposing adds float noise to the rest
            bool changed = false;
            if(glm::any(glm::greaterThan(glm::abs(position - shape.Position), glm::vec3(1e-5f))))
            {
                shape.Position = position;
                changed = true;
            }
            const glm::vec3 deltaRotation = glm::degrees(rotation) - shape.Rotation;
            if(glm::any(glm::greaterThan(glm::abs(deltaRotation), glm::vec3(1e-3f))))
            {
                shape.Rotation += deltaRotation;
                changed = true;
            }
            if(hasSize && glm::any(glm::greaterThan(glm::abs(scale - matrixSize), glm::vec3(1e-5f))))
            {
                if(shape.Type == ParticleShapeType::BOX)
                    shape.BoxSize = scale;
                else
                {
                    const glm::vec3 delta = glm::abs(scale - matrixSize);
                    const int axis = delta.x >= delta.y && delta.x >= delta.z ? 0 : (delta.y >= delta.z ? 1 : 2);
                    shape.Radius = scale[axis];
                }
                changed = true;
            }
            if(changed)
                MarkChanged();
        }
        ImGuizmo::PopID();
    }

    void ParticleEditorPanel::DrawPreviewToolbar(const ImVec2& min)
    {
        struct Tool
        {
            ToolIcon Icon;
            int Operation;
            const char* Tooltip;
        };
        static constexpr Tool sTools[] = {
            { ToolIcon::SELECT, -1, "No manipulator (Q)" },
            { ToolIcon::MOVE, ImGuizmo::OPERATION::TRANSLATE, "Move the emitter shape (W)" },
            { ToolIcon::ROTATE, ImGuizmo::OPERATION::ROTATE, "Rotate the emitter shape (E)" },
            { ToolIcon::SCALE, ImGuizmo::OPERATION::SCALE, "Resize the emitter shape: box size or radius (R)" },
            { ToolIcon::UNIVERSAL, ImGuizmo::OPERATION::UNIVERSAL, "Move + Rotate + Resize (T)" },
        };
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

        ImGui::SetCursorScreenPos(ImVec2(min.x + 3.0f, min.y + 3.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(15, 15, 15, 200));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

        const float frameHeight = ImGui::GetFrameHeight();
        const ImVec2 toolButtonSize(frameHeight * 1.15f, frameHeight);
        for(const Tool& tool : sTools)
        {
            const bool active = mGizmoOperation == tool.Operation;
            if(active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGuiAux::Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGuiAux::Colors::Selection);
            }

            ImGui::PushID(tool.Operation);
            const ImVec4 tint = active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            if(GizmoIcons::IconButton("##PreviewTool", rhi->GetImGuiImage(editor->GetToolIcon(tool.Icon)), toolButtonSize, tint, 0.9f))
                mGizmoOperation = tool.Operation;
            ImGui::PopID();

            if(active)
                ImGui::PopStyleColor(2);
            ImGuiAux::ItemTooltip(tool.Tooltip);
            ImGui::SameLine();
        }

        ImGui::SameLine(0.0f, 8.0f);
        if(ImGui::Button(mGizmoWorldSpace ? "World" : "Local"))
            mGizmoWorldSpace = !mGizmoWorldSpace;
        ImGuiAux::ItemTooltip("Manipulator space: local / world (X)");

        ImGui::SameLine(0.0f, 8.0f);
        if(ImGui::Button("Gizmos"))
            ImGui::OpenPopup("PreviewGizmosPopup");
        ImGuiAux::ItemTooltip("What the preview draws");

        ImGui::SameLine(0.0f, 8.0f);
        if(ImGui::Button("Frame"))
            FrameSelectedEmitter();
        ImGuiAux::ItemTooltip("Centers the camera on the selected emitter (F)");

        ImGui::PopStyleVar();
        ImGui::PopStyleColor();

        ImGuiAux::StyledPopupVars::Push();
        if(ImGui::BeginPopup("PreviewGizmosPopup"))
        {
            ImGui::Checkbox("Grid", &mShowGrid);
            ImGui::Checkbox("Other Emitter Shapes", &mShowShapes);
            ImGui::SetItemTooltip("Shapes of the other emitters, the selected one is always drawn");
            ImGui::Checkbox("Emit Direction", &mShowDirection);
            ImGui::Checkbox("Collision Planes", &mShowCollision);
            ImGui::SetItemTooltip("Collision planes of the selected emitter");
            ImGui::Checkbox("Culling Bounds", &mShowBounds);
            ImGui::SetItemTooltip("World box the scene culls the effect with (live particles + emitting shapes)");
            ImGui::EndPopup();
        }
        ImGuiAux::StyledPopupVars::Pop();
    }

    //////////////////////////////////////////////////////////////////////////
    // Emitters
    //////////////////////////////////////////////////////////////////////////

    void ParticleEditorPanel::DrawEmitterList()
    {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        {
            ImGuiAux::ScopedBoldFont bold;
            ImGui::TextUnformatted("EMITTERS");
        }

        const bool full = mEffect->Emitters.size() >= ParticleEffect::MAX_EMITTERS;
        const bool hasSelection = mSelectedEmitter >= 0 && mSelectedEmitter < static_cast<int>(mEffect->Emitters.size());
        int pendingDelete = -1;
        int pendingDuplicate = -1;
        int pendingMove = 0;

        ImGui::BeginDisabled(full);
        if(ImGui::Button("+ Add"))
            AddEmitter();
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSelection || full);
        if(ImGui::Button("Duplicate"))
            pendingDuplicate = mSelectedEmitter;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSelection || mEffect->Emitters.size() <= 1);
        if(ImGui::Button("Delete"))
            pendingDelete = mSelectedEmitter;
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!hasSelection);
        if(ImGui::ArrowButton("##Up", ImGuiDir_Up))
            pendingMove = -1;
        ImGui::SameLine();
        if(ImGui::ArrowButton("##Down", ImGuiDir_Down))
            pendingMove = 1;
        ImGui::EndDisabled();

        for(int i = 0; i < static_cast<int>(mEffect->Emitters.size()); i++)
        {
            ParticleEmitterDesc& emitter = mEffect->Emitters[i];
            ImGui::PushID(i);
            if(ImGui::Checkbox("##Enabled", &emitter.Enabled))
                MarkChanged();
            ImGuiAux::DelayedToolTip("Enabled");
            ImGui::SameLine();

            const char* role = emitter.IsSubEmitterTarget ? "  (sub-emitter)" : "";
            const String label = std::format("{}{}", emitter.Name, role);
            if(ImGui::Selectable(label.c_str(), mSelectedEmitter == i))
                mSelectedEmitter = i;
            ImGui::PopID();
        }

        if(pendingDuplicate >= 0)
            DuplicateEmitter(pendingDuplicate);
        else if(pendingDelete >= 0)
            DeleteEmitter(pendingDelete);
        else if(pendingMove != 0)
            MoveEmitter(mSelectedEmitter, pendingMove);
    }

    bool ParticleEditorPanel::BeginModule(const char* name, bool* enabled, bool defaultOpen)
    {
        ImGui::PushID(name);
        if(enabled)
        {
            if(ImGui::Checkbox("##ModuleEnabled", enabled))
                MarkChanged();
            ImGui::SameLine(0.0f, 4.0f);
        }

        const bool off = enabled && !*enabled;
        if(off)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        bool open = false;
        {
            ImGuiAux::ScopedBoldFont bold;
            open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
        }
        if(off)
            ImGui::PopStyleColor();

        if(open && BeginProperties())
            return true;

        ImGui::PopID();
        return false;
    }

    void ParticleEditorPanel::EndModule()
    {
        ImGui::EndTable();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::PopID();
    }

    void ParticleEditorPanel::DrawEmitter(ParticleEmitterDesc& emitter)
    {
        bool changed = false;
        bool layoutChanged = false;

        {
            ImGuiAux::ScopedBoldFont bold;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::InputTextWithHint("##EmitterName", "Emitter name", &emitter.Name))
                changed = true;
        }
        if(emitter.IsSubEmitterTarget)
            ImGui::TextDisabled("Sub-emitter: it only spawns when another emitter triggers it");

        if(BeginModule("Main", nullptr, true))
        {
            changed |= DragFloatRow("Duration", &emitter.Main.Duration, 0.05f, 0.05f, 600.0f, "%.2f s");
            changed |= CheckboxRow("Loop", &emitter.Main.Loop);
            if(emitter.Main.Loop)
                changed |= CheckboxRow("Prewarm", &emitter.Main.Prewarm);
            changed |= DragFloatRow("Start Delay", &emitter.Main.StartDelay, 0.05f, 0.0f, 600.0f, "%.2f s");
            changed |= ParticleWidgets::FloatRow("Start Lifetime", emitter.Main.StartLifetime, 0.05f, 0.0f, 0.0f, "%.2f s");
            changed |= ParticleWidgets::FloatRow("Start Speed", emitter.Main.StartSpeed);
            changed |= ParticleWidgets::FloatRow("Start Size", emitter.Main.StartSize, 0.01f);
            changed |= ParticleWidgets::FloatRow("Start Rotation", emitter.Main.StartRotation, 1.0f, 0.0f, 0.0f, "%.0f deg");
            changed |= ParticleWidgets::ColorRow("Start Color", emitter.Main.StartColor);
            changed |= DragFloatRow("Gravity Scale", &emitter.Main.GravityScale, 0.01f);
            changed |= ComboRow("Simulation Space", emitter.Main.SimulationSpace, sSpaceNames);
            changed |= DragIntRow("Max Particles", &emitter.Main.MaxParticles, 1, 100000);
            HintRow("Pool size of this emitter, allocated once when the effect starts");
            EndModule();
        }

        if(BeginModule("Emission", &emitter.Emission.Enabled))
        {
            changed |= ParticleWidgets::FloatRow("Rate Over Time", emitter.Emission.RateOverTime, 0.1f, 0.0f, 0.0f, "%.1f /s");
            changed |= ParticleWidgets::FloatRow("Rate Over Distance", emitter.Emission.RateOverDistance, 0.1f, 0.0f, 0.0f, "%.1f /u");

            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Bursts");
            ImGui::TableNextColumn();
            int removeBurst = -1;
            for(int b = 0; b < static_cast<int>(emitter.Emission.Bursts.size()); b++)
            {
                ParticleBurst& burst = emitter.Emission.Bursts[b];
                ImGui::PushID(b);
                const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 12.0f) / 3.0f;
                ImGui::SetNextItemWidth(w);
                changed |= ImGui::DragFloat("##Time", &burst.Time, 0.01f, 0.0f, 600.0f, "at %.2fs", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine(0.0f, 4.0f);
                int counts[2] = { static_cast<int>(burst.MinCount), static_cast<int>(burst.MaxCount) };
                ImGui::SetNextItemWidth(w);
                if(ImGui::DragInt2("##Count", counts, 0.2f, 0, 10000))
                {
                    burst.MinCount = static_cast<Uint>(std::max(counts[0], 0));
                    burst.MaxCount = static_cast<Uint>(std::max(counts[1], 0));
                    changed = true;
                }
                ImGuiAux::DelayedToolTip("Count: min, max");
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(w);
                changed |= ImGui::DragInt("##Cycles", &burst.Cycles, 0.1f, 0, 1000, burst.Cycles <= 0 ? "repeat" : "%d x");
                ImGuiAux::DelayedToolTip("Cycles (0 = repeat for the whole duration)");
                ImGui::SameLine(0.0f, 4.0f);
                if(ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                    removeBurst = b;

                if(burst.Cycles != 1)
                {
                    ImGui::SetNextItemWidth(w);
                    changed |= ImGui::DragFloat("##Interval", &burst.Interval, 0.01f, 0.01f, 60.0f, "every %.2fs", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SameLine(0.0f, 4.0f);
                }
                ImGui::SetNextItemWidth(w);
                changed |= ImGui::SliderFloat("##Probability", &burst.Probability, 0.0f, 1.0f, "%.2f chance");
                ImGui::PopID();
            }
            if(removeBurst >= 0)
            {
                emitter.Emission.Bursts.erase(emitter.Emission.Bursts.begin() + removeBurst);
                layoutChanged = true;
            }
            if(ImGui::Button("+ Burst"))
            {
                emitter.Emission.Bursts.emplace_back();
                layoutChanged = true;
            }
            EndModule();
        }

        if(BeginModule("Shape", &emitter.Shape.Enabled))
        {
            ParticleShapeModule& shape = emitter.Shape;
            changed |= ComboRow("Shape", shape.Type, sShapeNames);
            if(shape.Type == ParticleShapeType::BOX)
                changed |= Vec3Row("Box Size", &shape.BoxSize, 0.05f);
            else if(shape.Type != ParticleShapeType::POINT)
            {
                changed |= DragFloatRow("Radius", &shape.Radius, 0.01f, 0.0f, 1000.0f);
                if(shape.Type != ParticleShapeType::EDGE)
                {
                    changed |= SliderFloatRow("Radius Thickness", &shape.RadiusThickness, 0.0f, 1.0f);
                    changed |= SliderFloatRow("Arc", &shape.Arc, 0.0f, 360.0f, "%.0f deg");
                }
            }
            if(shape.Type == ParticleShapeType::CONE)
                changed |= SliderFloatRow("Angle", &shape.Angle, 0.0f, 90.0f, "%.1f deg");
            changed |= Vec3Row("Position", &shape.Position, 0.05f);
            changed |= Vec3Row("Rotation", &shape.Rotation, 0.5f);
            changed |= SliderFloatRow("Randomize Direction", &shape.RandomizeDirection, 0.0f, 1.0f);
            EndModule();
        }

        if(BeginModule("Velocity over Lifetime", &emitter.Velocity.Enabled))
        {
            changed |= Vec3Row("Linear", &emitter.Velocity.Linear, 0.05f);
            changed |= ComboRow("Space", emitter.Velocity.Space, sSpaceNames);
            changed |= ParticleWidgets::CurveRow("Speed Modifier", emitter.Velocity.SpeedModifier);
            EndModule();
        }

        if(BeginModule("Limit Velocity & Drag", &emitter.LimitVelocity.Enabled))
        {
            changed |= DragFloatRow("Speed Limit", &emitter.LimitVelocity.SpeedLimit, 0.05f, 0.0f, 10000.0f);
            changed |= SliderFloatRow("Dampen", &emitter.LimitVelocity.Dampen, 0.0f, 1.0f);
            changed |= DragFloatRow("Drag", &emitter.LimitVelocity.Drag, 0.01f, 0.0f, 100.0f);
            EndModule();
        }

        if(BeginModule("Force over Lifetime", &emitter.Force.Enabled))
        {
            changed |= Vec3Row("Force", &emitter.Force.Force, 0.05f);
            changed |= ComboRow("Space", emitter.Force.Space, sSpaceNames);
            EndModule();
        }

        if(BeginModule("Noise", &emitter.Noise.Enabled))
        {
            changed |= DragFloatRow("Strength", &emitter.Noise.Strength, 0.01f, 0.0f, 100.0f);
            changed |= DragFloatRow("Frequency", &emitter.Noise.Frequency, 0.01f, 0.0f, 50.0f);
            changed |= DragFloatRow("Scroll Speed", &emitter.Noise.ScrollSpeed, 0.01f, 0.0f, 50.0f);
            EndModule();
        }

        if(BeginModule("Color over Lifetime", &emitter.ColorOverLifetime.Enabled))
        {
            changed |= ParticleWidgets::GradientRow("Color", emitter.ColorOverLifetime.Gradient);
            HintRow("Multiplies the start color");
            EndModule();
        }

        if(BeginModule("Size over Lifetime", &emitter.SizeOverLifetime.Enabled))
        {
            changed |= ParticleWidgets::CurveRow("Size", emitter.SizeOverLifetime.Size);
            HintRow("Multiplies the start size");
            EndModule();
        }

        if(BeginModule("Rotation over Lifetime", &emitter.RotationOverLifetime.Enabled))
        {
            changed |= ParticleWidgets::FloatRow("Angular Velocity", emitter.RotationOverLifetime.AngularVelocity, 1.0f, 0.0f, 0.0f, "%.0f deg/s");
            EndModule();
        }

        if(BeginModule("Texture Sheet Animation", &emitter.TextureSheet.Enabled))
        {
            ParticleTextureSheetModule& sheet = emitter.TextureSheet;
            changed |= DragIntRow("Columns", &sheet.Columns, 1, 16);
            changed |= DragIntRow("Rows", &sheet.Rows, 1, 16);
            changed |= ComboRow("Timing", sheet.Timing, sSheetTimingNames);
            if(sheet.Timing == ParticleSheetTiming::LIFETIME)
                changed |= DragFloatRow("Cycles", &sheet.Cycles, 0.05f, 0.0f, 100.0f);
            else
                changed |= DragFloatRow("FPS", &sheet.FPS, 0.5f, 0.0f, 240.0f);
            changed |= CheckboxRow("Random Start Frame", &sheet.RandomStartFrame);
            changed |= CheckboxRow("Frame Blending", &sheet.FrameBlending);
            HintRow("Frame blending reads the texture twice per pixel");
            EndModule();
        }

        if(BeginModule("Collision", &emitter.Collision.Enabled))
        {
            ParticleCollisionModule& collision = emitter.Collision;
            for(Uint p = 0; p < collision.PlaneCount; p++)
            {
                ImGui::PushID(static_cast<int>(p));
                ParticleCollisionPlane& plane = collision.Planes[p];
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Plane %u", p + 1);
                ImGui::TableNextColumn();
                const float w = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 8.0f;
                ImGui::SetNextItemWidth(w * 0.7f);
                changed |= ImGui::DragFloat3("##Normal", glm::value_ptr(plane.Normal), 0.01f, -1.0f, 1.0f, "%.2f");
                ImGuiAux::DelayedToolTip("Normal (world space)");
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(w * 0.3f);
                changed |= ImGui::DragFloat("##Offset", &plane.Offset, 0.01f, 0.0f, 0.0f, "%.2f");
                ImGuiAux::DelayedToolTip("Offset along the normal (height of a ground plane)");
                ImGui::SameLine(0.0f, 4.0f);
                if(ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)) && collision.PlaneCount > 0)
                {
                    for(Uint q = p; q + 1 < collision.PlaneCount; q++)
                        collision.Planes[q] = collision.Planes[q + 1];
                    collision.PlaneCount--;
                    changed = true;
                }
                ImGui::PopID();
            }
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(collision.PlaneCount >= ParticleCollisionModule::MAX_PLANES);
            if(ImGui::Button("+ Plane"))
            {
                collision.Planes[collision.PlaneCount++] = ParticleCollisionPlane {};
                changed = true;
            }
            ImGui::EndDisabled();

            changed |= SliderFloatRow("Bounce", &collision.Bounce, 0.0f, 1.0f);
            changed |= SliderFloatRow("Dampen", &collision.Dampen, 0.0f, 1.0f);
            changed |= SliderFloatRow("Lifetime Loss", &collision.LifetimeLoss, 0.0f, 1.0f);
            changed |= DragFloatRow("Radius Scale", &collision.RadiusScale, 0.01f, 0.0f, 10.0f);
            EndModule();
        }

        if(BeginModule("Sub Emitters", &emitter.SubEmitters.Enabled))
        {
            int removeEntry = -1;
            for(int s = 0; s < static_cast<int>(emitter.SubEmitters.Entries.size()); s++)
            {
                ParticleSubEmitter& entry = emitter.SubEmitters.Entries[s];
                ImGui::PushID(s);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-FLT_MIN);
                {
                    const size_t current = static_cast<size_t>(entry.Event);
                    if(ImGui::BeginCombo("##Event", sSubEmitterEventNames[current]))
                    {
                        for(size_t e = 0; e < sSubEmitterEventNames.size(); e++)
                        {
                            if(ImGui::Selectable(sSubEmitterEventNames[e], e == current))
                            {
                                entry.Event = static_cast<ParticleSubEmitterEvent>(e);
                                changed = true;
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::TableNextColumn();
                const float w = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - 4.0f;
                ImGui::SetNextItemWidth(w * 0.5f);
                const bool validTarget = entry.EmitterIndex >= 0 && entry.EmitterIndex < static_cast<int>(mEffect->Emitters.size());
                if(ImGui::BeginCombo("##Target", validTarget ? mEffect->Emitters[entry.EmitterIndex].Name.c_str() : "None"))
                {
                    for(int e = 0; e < static_cast<int>(mEffect->Emitters.size()); e++)
                    {
                        if(&mEffect->Emitters[e] == &emitter)
                            continue;
                        ImGui::PushID(e);
                        if(ImGui::Selectable(mEffect->Emitters[e].Name.c_str(), e == entry.EmitterIndex))
                        {
                            entry.EmitterIndex = e;
                            changed = true;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
                ImGuiAux::DelayedToolTip("Emitter that spawns (usually with its emission disabled)");
                ImGui::SameLine(0.0f, 4.0f);
                int count = static_cast<int>(entry.Count);
                ImGui::SetNextItemWidth(w * 0.25f - 4.0f);
                if(ImGui::DragInt("##Count", &count, 0.2f, 1, 1000, "%d x", ImGuiSliderFlags_AlwaysClamp))
                {
                    entry.Count = static_cast<Uint>(count);
                    changed = true;
                }
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(w * 0.25f - 4.0f);
                changed |= ImGui::SliderFloat("##Probability", &entry.Probability, 0.0f, 1.0f, "%.2f");
                ImGuiAux::DelayedToolTip("Probability");
                ImGui::SameLine(0.0f, 4.0f);
                if(ImGui::Button("x", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                    removeEntry = s;
                ImGui::PopID();
            }
            if(removeEntry >= 0)
            {
                emitter.SubEmitters.Entries.erase(emitter.SubEmitters.Entries.begin() + removeEntry);
                changed = true;
            }

            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            if(ImGui::Button("+ Sub Emitter"))
            {
                emitter.SubEmitters.Entries.emplace_back();
                changed = true;
            }
            HintRow("Spawns particles of another emitter of this effect where a particle is born, dies or collides");
            EndModule();
        }

        if(BeginModule("Renderer", nullptr, true))
        {
            ParticleRendererModule& renderer = emitter.Renderer;
            changed |= ComboRow("Render Mode", renderer.Mode, sRenderModeNames);
            if(renderer.Mode == ParticleRenderMode::STRETCHED)
            {
                changed |= DragFloatRow("Speed Scale", &renderer.StretchSpeedScale, 0.005f, 0.0f, 10.0f);
                changed |= DragFloatRow("Length Scale", &renderer.StretchLengthScale, 0.01f, 0.0f, 50.0f);
            }
            changed |= TextureRow("Texture", renderer.Texture);
            changed |= SliderFloatRow("Additive", &renderer.AdditiveAmount, 0.0f, 1.0f);
            HintRow("0 = alpha blended (smoke), 1 = additive (fire, sparks), both in one draw");
            changed |= DragFloatRow("Emissive Intensity", &renderer.EmissiveIntensity, 0.01f, 0.0f, 100.0f);
            HintRow("Above 1 the particles glow with Bloom");
            changed |= ComboRow("Sort Mode", renderer.Sort, sSortModeNames);
            changed |= DragIntRow("Sorting Order", &renderer.SortingOrder, -1000, 1000);
            changed |= SliderFloatRow("Max Screen Size", &renderer.MaxScreenSize, 0.01f, 1.0f);
            HintRow("Caps the particle height to a fraction of the screen, limits overdraw close to the camera (mobile)");
            EndModule();
        }

        if(changed || layoutChanged)
            MarkChanged(layoutChanged);
    }

} // namespace Surge
