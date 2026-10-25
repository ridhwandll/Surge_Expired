// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/PhysicsPanel.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Physics/Physics.hpp"
#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cstdio>
#include <format>

namespace Surge
{
    namespace
    {
        constexpr float LABEL_WIDTH = 130.0f;

        void SectionTitle(const char* title)
        {
            ImGuiAux::ScopedBoldFont font;
            ImGuiAux::ScopedColor color({ ImGuiCol_Separator }, ImGuiAux::Colors::ThemeColor1);
            ImGui::SeparatorText(title);
        }

        ImU32 ToU32(const glm::vec4& color) { return ImGui::ColorConvertFloat4ToU32(ImVec4(color.r, color.g, color.b, color.a)); }

        // (Rid) ImGui has no rotated text: the text is laid out horizontally at a visible spot (ImGui culls glyphs outside the clip rect before any transform),
        // then its vertices are turned 90 degrees counter clockwise around bottomLeft, so it reads upwards
        void AddTextRotated(ImDrawList* drawList, const ImVec2& layoutPos, const ImVec2& bottomLeft, ImU32 color, const char* text)
        {
            const int first = drawList->VtxBuffer.Size;
            drawList->AddText(layoutPos, color, text);
            for(int i = first; i < drawList->VtxBuffer.Size; i++)
            {
                ImVec2& pos = drawList->VtxBuffer[i].pos;
                const ImVec2 offset(pos.x - layoutPos.x, pos.y - layoutPos.y);
                pos = ImVec2(bottomLeft.x + offset.y, bottomLeft.y - offset.x);
            }
        }

        bool BeginProperties(const char* id)
        {
            if(!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingFixedFit))
                return false;
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, LABEL_WIDTH);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            return true;
        }
    }

    void PhysicsPanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void PhysicsPanel::Render(bool* show)
    {
        if(!*show)
            return;

        if(ImGui::Begin(PanelCodeToString(mCode), show))
        {
            bool changed = DrawWorld();
            changed |= DrawLayers();
            changed |= DrawCollisionMatrix();
            DrawStats();

            Editor* editor = static_cast<Editor*>(Core::GetClient());
            if(changed)
            {
                Core::GetPhysics()->ApplySettings(editor->GetPhysicsSettings());
                mPendingSave = true;
            }

            // Drags and text fields change the settings every frame, the project file is written once they are released
            if(mPendingSave && !ImGui::IsAnyItemActive())
            {
                editor->SaveProject();
                mPendingSave = false;
            }
        }
        ImGui::End();
    }

    bool PhysicsPanel::DrawWorld()
    {
        PhysicsSettings& settings = static_cast<Editor*>(Core::GetClient())->GetPhysicsSettings();
        bool changed = false;

        SectionTitle("WORLD");
        if(BeginProperties("##World"))
        {
            changed |= ImGuiAux::TProperty<glm::vec3>("Gravity", &settings.Gravity);
            changed |= ImGuiAux::TSlider<int>("Fixed Rate (Hz)", &settings.FixedRate, PhysicsSettings::MIN_FIXED_RATE, PhysicsSettings::MAX_FIXED_RATE);
            ImGuiAux::ItemTooltip("Simulation steps per second. Every step costs CPU: 30-60 Hz suits phones, higher rates only help fast or stacked bodies.");
            changed |= ImGuiAux::TSlider<int>("Substeps", &settings.CollisionSubsteps, 1, PhysicsSettings::MAX_SUBSTEPS);
            ImGuiAux::ItemTooltip("Collision iterations per step. More = stabler stacks and joints, linearly more CPU.");
            ImGuiAux::TString("Fixed Step", "%.2f ms (Time.GetFixedDeltaTime)", settings.GetFixedTimestep() * 1000.0f);
            ImGui::EndTable();
        }

        if(ImGuiAux::Button("Reset World"))
        {
            const PhysicsSettings defaults;
            settings.Gravity = defaults.Gravity;
            settings.FixedRate = defaults.FixedRate;
            settings.CollisionSubsteps = defaults.CollisionSubsteps;
            changed = true;
        }
        return changed;
    }

    bool PhysicsPanel::DrawLayers()
    {
        PhysicsSettings& settings = static_cast<Editor*>(Core::GetClient())->GetPhysicsSettings();
        bool changed = false;
        int removeLayer = -1;
        int freeLayer = -1;

        SectionTitle("LAYERS");

        if(ImGui::BeginTable("##Layers", 3, ImGuiTableFlags_SizingFixedFit))
        {
            const float buttonSize = ImGui::GetFrameHeight();
            ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, LABEL_WIDTH);
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, buttonSize);

            for(int i = 0; i < PhysicsSettings::MAX_LAYERS; i++)
            {
                if(settings.LayerNames[i].empty())
                {
                    if(freeLayer < 0)
                        freeLayer = i;
                    continue;
                }

                ImGui::PushID(i);
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::Text("Layer %d", i);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1.0f);

                // Edited through a copy: an emptied field is not applied (an empty name means unused, the row would vanish while typing)
                String name = settings.LayerNames[i];
                ImGui::BeginDisabled(i == 0); // "Default": every body starts there
                if(ImGui::InputText("##Name", &name) && !name.empty())
                {
                    settings.LayerNames[i] = name;
                    changed = true;
                }
                ImGui::TableNextColumn();
                if(i != 0 && ImGuiAux::Button("X", ImVec2(buttonSize, buttonSize)))
                    removeLayer = i;
                if(i != 0)
                    ImGuiAux::ItemTooltip("Remove the layer. Bodies still on it keep its index until they are moved to another layer");
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        // A removed layer collides with everything again, so adding it back starts clean
        if(removeLayer > 0)
        {
            settings.LayerNames[removeLayer].clear();
            for(int i = 0; i < PhysicsSettings::MAX_LAYERS; i++)
                settings.SetCollision(removeLayer, i, true);
            changed = true;
        }

        ImGui::BeginDisabled(freeLayer < 0);
        if(ImGuiAux::Button("ADD LAYER", ImVec2(-1.0f, 0.0f)))
        {
            settings.LayerNames[freeLayer] = std::format("Layer {}", freeLayer);
            changed = true;
        }
        ImGui::EndDisabled();
        if(freeLayer < 0)
            ImGuiAux::ItemTooltip("Every layer is in use");
        return changed;
    }

    bool PhysicsPanel::DrawCollisionMatrix()
    {
        PhysicsSettings& settings = static_cast<Editor*>(Core::GetClient())->GetPhysicsSettings();
        bool changed = false;

        SectionTitle("COLLISION MATRIX");
        int layers[PhysicsSettings::MAX_LAYERS];
        int layerCount = 0;
        float labelWidth = 0.0f;
        for(int i = 0; i < PhysicsSettings::MAX_LAYERS; i++)
        {
            if(settings.LayerNames[i].empty())
                continue;
            layers[layerCount++] = i;
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(settings.LayerNames[i].c_str()).x);
        }

        // Unity layout: rows are layers top to bottom, columns the same layers reversed (rotated names on top),
        // so the symmetric matrix is one triangle and row r has cells for the layers >= its own.
        // Labels are "<index> <name>", indices right aligned in a two digit slot so the names line up
        const ImGuiStyle& style = ImGui::GetStyle();
        const float cell = ImGui::GetFrameHeight();
        const float pitch = cell + 3.0f;
        const float labelGap = style.ItemInnerSpacing.x * 2.0f;
        const float indexWidth = ImGui::CalcTextSize("00").x;
        const float indexGap = style.ItemInnerSpacing.x;
        labelWidth += indexWidth + indexGap;
        const float rowLabelWidth = labelWidth + labelGap;
        const float headerHeight = labelWidth + labelGap;
        const ImVec2 size(rowLabelWidth + pitch * static_cast<float>(layerCount), headerHeight + pitch * static_cast<float>(layerCount));
        auto columnLayerIndex = [layerCount](int column) { return layerCount - 1 - column; };

        ImGui::BeginChild("##CollisionMatrix", ImVec2(0.0f, size.y + style.ScrollbarSize), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##Matrix", size);
        const bool hovered = ImGui::IsItemHovered();
        const bool activated = ImGui::IsItemActivated();
        const bool active = ImGui::IsItemActive();

        // Hit test: a cell, a row label or a column label (rotated name)
        const ImVec2 local(ImGui::GetMousePos().x - origin.x, ImGui::GetMousePos().y - origin.y);
        const int mouseRow = local.y >= headerHeight ? static_cast<int>((local.y - headerHeight) / pitch) : -1;
        const int mouseColumn = local.x >= rowLabelWidth ? static_cast<int>((local.x - rowLabelWidth) / pitch) : -1;
        const bool rowInRange = mouseRow >= 0 && mouseRow < layerCount;
        const bool columnInRange = mouseColumn >= 0 && mouseColumn < layerCount;

        int hoveredRow = -1;
        int hoveredColumn = -1;
        int hoveredLabelLayer = -1; // Index into layers
        if(hovered || active)
        {
            if(rowInRange && columnInRange && columnLayerIndex(mouseColumn) >= mouseRow)
            {
                hoveredRow = mouseRow;
                hoveredColumn = mouseColumn;
            }
            else if(hovered && rowInRange && local.x < rowLabelWidth)
                hoveredLabelLayer = mouseRow;
            else if(hovered && columnInRange && local.y < headerHeight)
                hoveredLabelLayer = columnLayerIndex(mouseColumn);
        }

        if(activated)
        {
            mPainting = hoveredRow >= 0; // A press on a layer name toggles that layer, dragging from it paints nothing
            if(mPainting)
                mPaintValue = !settings.ShouldCollide(layers[hoveredRow], layers[columnLayerIndex(hoveredColumn)]);
        }

        if(active && mPainting && hoveredRow >= 0)
        {
            const int a = layers[hoveredRow];
            const int b = layers[columnLayerIndex(hoveredColumn)];
            if(settings.ShouldCollide(a, b) != mPaintValue)
            {
                settings.SetCollision(a, b, mPaintValue);
                changed = true;
            }
        }

        // A layer name: every pair on (all off when they all collide already)
        if(activated && hoveredLabelLayer >= 0)
        {
            const int a = layers[hoveredLabelLayer];
            bool allOn = true;
            for(int i = 0; i < layerCount; i++)
                allOn &= settings.ShouldCollide(a, layers[i]);
            for(int i = 0; i < layerCount; i++)
                settings.SetCollision(a, layers[i], !allOn);
            changed = true;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
        const ImU32 highlightColor = ToU32(ImGuiAux::Colors::ThemeColor1);
        const ImU32 bandColor = ImGui::GetColorU32(ImGuiCol_HeaderHovered, 0.35f);
        const int focusLayer = hoveredRow >= 0 ? hoveredRow : hoveredLabelLayer;
        const int focusColumnLayer = hoveredColumn >= 0 ? columnLayerIndex(hoveredColumn) : hoveredLabelLayer;

        // Crosshair: the hovered cell's row and column, from their labels
        if(hoveredRow >= 0)
        {
            const float rowY = origin.y + headerHeight + pitch * static_cast<float>(hoveredRow);
            const float columnX = origin.x + rowLabelWidth + pitch * static_cast<float>(hoveredColumn);
            drawList->AddRectFilled(ImVec2(origin.x, rowY - 1.5f), ImVec2(columnX + cell, rowY + cell + 1.5f), bandColor, 3.0f);
            drawList->AddRectFilled(ImVec2(columnX - 1.5f, origin.y), ImVec2(columnX + cell + 1.5f, rowY + cell), bandColor, 3.0f);
        }

        const ImVec2 rotatedLayoutPos = drawList->GetClipRectMin();
        const ImU32 indexColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
        for(int r = 0; r < layerCount; r++)
        {
            const char* name = settings.LayerNames[layers[r]].c_str();
            const bool focused = r == focusLayer || r == focusColumnLayer;
            const ImU32 color = focused ? highlightColor : textColor;
            char index[4];
            std::snprintf(index, sizeof(index), "%d", layers[r]);
            const float indexOffset = indexWidth - ImGui::CalcTextSize(index).x;
            const float textHeight = ImGui::GetTextLineHeight();

            // Row label
            const float rowY = origin.y + headerHeight + pitch * static_cast<float>(r);
            const float textY = rowY + (cell - textHeight) * 0.5f;
            drawList->AddText(ImVec2(origin.x + indexOffset, textY), focused ? highlightColor : indexColor, index);
            drawList->AddText(ImVec2(origin.x + indexWidth + indexGap, textY), color, name);

            // Column label of the same layer, reading bottom to top
            const int column = layerCount - 1 - r;
            const float columnX = origin.x + rowLabelWidth + pitch * static_cast<float>(column) + (cell - textHeight) * 0.5f;
            const float bottom = origin.y + headerHeight - labelGap * 0.5f;
            AddTextRotated(drawList, rotatedLayoutPos, ImVec2(columnX, bottom - indexOffset), focused ? highlightColor : indexColor, index);
            AddTextRotated(drawList, rotatedLayoutPos, ImVec2(columnX, bottom - indexWidth - indexGap), color, name);
        }

        const ImU32 cellColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
        const ImU32 cellHoveredColor = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
        const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Border);
        const float bulletRadius = cell * 0.3f;
        for(int r = 0; r < layerCount; r++)
        {
            for(int c = 0; c <= columnLayerIndex(r); c++)
            {
                const bool isHovered = r == hoveredRow && c == hoveredColumn;
                const ImVec2 min(origin.x + rowLabelWidth + pitch * static_cast<float>(c), origin.y + headerHeight + pitch * static_cast<float>(r));
                const ImVec2 max(min.x + cell, min.y + cell);
                drawList->AddRectFilled(min, max, isHovered ? cellHoveredColor : cellColor, 3.0f);
                drawList->AddRect(min, max, borderColor, 3.0f);

                // Bullet when the pair collides, an empty cell when it does not
                if(settings.ShouldCollide(layers[r], layers[columnLayerIndex(c)]))
                    drawList->AddCircleFilled(ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f), bulletRadius, highlightColor);
            }
        }
        ImGui::EndChild();

        if(hoveredRow >= 0 && !active)
        {
            const int a = layers[hoveredRow];
            const int b = layers[columnLayerIndex(hoveredColumn)];
            ImGui::SetTooltip("%s & %s\n%s", settings.LayerNames[a].c_str(), settings.LayerNames[b].c_str(), settings.ShouldCollide(a, b) ? "Collide with each other" : "Ignore each other");
        }
        else if(hoveredLabelLayer >= 0)
            ImGui::SetTooltip("Layer %d: click to toggle all of its pairs", layers[hoveredLabelLayer]);

        if(ImGuiAux::Button("Enable All"))
        {
            settings.CollisionMatrix.fill(0xFFFF);
            changed = true;
        }
        ImGui::SameLine();
        if(ImGuiAux::Button("Disable All"))
        {
            settings.CollisionMatrix.fill(0);
            changed = true;
        }

        int pairs = 0;
        int colliding = 0;
        for(int r = 0; r < layerCount; r++)
        {
            for(int c = r; c < layerCount; c++)
            {
                pairs++;
                colliding += settings.ShouldCollide(layers[r], layers[c]) ? 1 : 0;
            }
        }
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%d / %d pairs collide", colliding, pairs);
        return changed;
    }

    void PhysicsPanel::DrawStats()
    {
        Physics* physics = Core::GetPhysics();
        int total = 0;
        int active = 0;
        physics->GetDebugStats(active, total);

        SectionTitle("SIMULATION");
        if(BeginProperties("##Stats"))
        {
            ImGuiAux::TString("Bodies", "%d", total);
            ImGuiAux::TString("Active", "%d", active);
            ImGuiAux::TString("Sleeping", "%d", total - active);
            ImGuiAux::TString("State", "%s", physics->IsPaused() ? "Paused" : "Running");
            ImGui::EndTable();
        }
        if(!static_cast<Editor*>(Core::GetClient())->IsPlaying())
            ImGui::TextDisabled("Bodies exist only in Play mode");
    }

} // namespace Surge
