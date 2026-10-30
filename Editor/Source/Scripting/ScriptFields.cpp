// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Scripting/ScriptFields.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Asset/AssetDatabase.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/ECS/Components/ScriptComponent.hpp"
#include "Surge/ScriptEngine/ScriptAsset.hpp"
#include "Surge/ScriptEngine/ScriptEngine.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include "Editor.hpp"
#include "Utility/EditorUI.hpp"
#include "Panels/ContentBrowserPanel.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <unordered_map>

namespace Surge
{
    namespace
    {
        // Recording versions of the runtime Property.* helpers (ScriptEngine.cpp): same signatures, they return descriptors instead of values
        constexpr const char* RECORDING_HELPERS = R"(
local Marker = {}
local function Field(type, default, min, max, tooltip)
    return { [Marker] = true, Type = type, Default = default, Min = min, Max = max, Tooltip = tooltip }
end
return Marker, {
    Float  = function(value, min, max, tooltip) return Field("FLOAT", value or 0.0, min, max, tooltip) end,
    Int    = function(value, min, max, tooltip) return Field("INT", value or 0, min, max, tooltip) end,
    Bool   = function(value, tooltip) return Field("BOOL", value or false, nil, nil, tooltip) end,
    String = function(value, tooltip) return Field("STRING", value or "", nil, nil, tooltip) end,
    Vec2   = function(x, y, tooltip) return Field("VEC2", Math.Vec2.new(x or 0, y or 0), nil, nil, tooltip) end,
    Vec3   = function(x, y, z, tooltip) return Field("VEC3", Math.Vec3.new(x or 0, y or 0, z or 0), nil, nil, tooltip) end,
    Vec4   = function(x, y, z, w, tooltip) return Field("VEC4", Math.Vec4.new(x or 0, y or 0, z or 0, w or 0), nil, nil, tooltip) end,
    Color  = function(r, g, b, a, tooltip) return Field("COLOR", Math.Vec4.new(r or 1, g or 1, b or 1, a or 1), nil, nil, tooltip) end,
    Entity = function(tooltip) return Field("ENTITY", nil, nil, nil, tooltip) end,
    Asset  = function(kind, path, tooltip)
        local field = Field("ASSET", path, nil, nil, tooltip); field.AssetKind = kind; return field
    end,
    Enum   = function(value, options, tooltip)
        local field = Field("ENUM", value or (options and options[1]) or "", nil, nil, tooltip); field.Options = options or {}; return field
    end,
    Array  = function(elementType, values, tooltip)
        local field = Field("ARRAY", values or {}, nil, nil, tooltip); field.ElementType = elementType; return field
    end,
}
)";

        struct CacheEntry
        {
            const Script* Source = nullptr; // A recook creates a new Script, the schema follows it
            size_t BytecodeSize = 0;
            ScriptFieldSchema Schema;
        };
        std::unordered_map<AssetID, CacheEntry> sCache;

        struct SourceWatch
        {
            uint64_t ModTime = 0;
            double NextCheck = 0.0;
        };
        std::unordered_map<AssetID, SourceWatch> sWatches;

        bool ReadValue(const sol::object& object, ScriptFieldType type, ScriptFieldType elementType, ScriptFieldValue& out)
        {
            switch(type)
            {
                case ScriptFieldType::ENUM:
                    if(object.get_type() != sol::type::string)
                        return false;
                    out.Text = object.as<String>();
                    return true;
                case ScriptFieldType::ASSET:
                    out.AssetID = 0;
                    if(object.get_type() == sol::type::string)
                    {
                        out.AssetID = Core::GetAssetManager()->GetIDFromPath(object.as<String>()).Get();
                        return true;
                    }
                    return object.get_type() == sol::type::lua_nil;
                case ScriptFieldType::ARRAY:
                {
                    out.Elements.clear();
                    if(object.get_type() != sol::type::table)
                        return object.get_type() == sol::type::lua_nil;
                    const sol::table table = object.as<sol::table>();
                    const size_t count = table.size();
                    for(size_t i = 1; i <= count; i++)
                    {
                        ScriptFieldValue element;
                        if(ReadValue(table.raw_get<sol::object>(i), elementType, ScriptFieldType::FLOAT, element))
                            out.Elements.push_back(std::move(element));
                    }
                    return true;
                }
                case ScriptFieldType::FLOAT:
                case ScriptFieldType::INT:
                    if(object.get_type() != sol::type::number)
                        return false;
                    out.Number = object.as<double>();
                    return true;
                case ScriptFieldType::BOOL:
                    if(object.get_type() != sol::type::boolean)
                        return false;
                    out.Bool = object.as<bool>();
                    return true;
                case ScriptFieldType::STRING:
                    if(object.get_type() != sol::type::string)
                        return false;
                    out.Text = object.as<String>();
                    return true;
                case ScriptFieldType::VEC2:
                    if(!object.is<glm::vec2>())
                        return false;
                    out.Vector = glm::vec4(object.as<glm::vec2>(), 0.0f, 0.0f);
                    return true;
                case ScriptFieldType::VEC3:
                    if(!object.is<glm::vec3>())
                        return false;
                    out.Vector = glm::vec4(object.as<glm::vec3>(), 0.0f);
                    return true;
                case ScriptFieldType::VEC4:
                case ScriptFieldType::COLOR:
                    if(!object.is<glm::vec4>())
                        return false;
                    out.Vector = object.as<glm::vec4>();
                    return true;
                case ScriptFieldType::ENTITY:
                {
                    out.EntityID = 0;
                    if(!object.is<Entity>())
                        return object.get_type() == sol::type::lua_nil;

                    Entity entity = object.as<Entity>();
                    if(entity && entity.HasComponent<IDComponent>())
                        out.EntityID = entity.GetComponent<IDComponent>().ID.Get();
                    return true;
                }
                default: return false;
            }
        }

        // A plain value in Properties: its type is its Lua type (integers are INT, numbers with a fraction FLOAT)
        bool InferField(const sol::object& object, ScriptFieldInfo& info)
        {
            switch(object.get_type())
            {
                case sol::type::number:
                {
                    lua_State* L = object.lua_state();
                    object.push(L);
                    info.Type = lua_isinteger(L, -1) ? ScriptFieldType::INT : ScriptFieldType::FLOAT;
                    lua_pop(L, 1);
                    break;
                }
                case sol::type::boolean: info.Type = ScriptFieldType::BOOL; break;
                case sol::type::string: info.Type = ScriptFieldType::STRING; break;
                case sol::type::userdata:
                    if(object.is<glm::vec2>())
                        info.Type = ScriptFieldType::VEC2;
                    else if(object.is<glm::vec3>())
                        info.Type = ScriptFieldType::VEC3;
                    else if(object.is<glm::vec4>())
                        info.Type = ScriptFieldType::VEC4;
                    else
                        return false;
                    break;
                default: return false; // Functions, tables... are not exposed (arrays need Property.Array)
            }
            return ReadValue(object, info.Type, info.ElementType, info.Default);
        }

        bool IsIdentifierChar(char c)
        {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        }

        // Where "<name> =" first appears after "Properties" in the source, so the Inspector follows the declaration order
        size_t FindDeclaration(const String& source, size_t propertiesPos, const String& name)
        {
            size_t pos = propertiesPos;
            while((pos = source.find(name, pos)) != String::npos)
            {
                const size_t end = pos + name.size();
                size_t next = end;
                while(next < source.size() && (source[next] == ' ' || source[next] == '\t'))
                    next++;

                const bool standalone = (pos == 0 || !IsIdentifierChar(source[pos - 1])) && (end >= source.size() || !IsIdentifierChar(source[end]));
                if(standalone && next < source.size() && source[next] == '=' && (next + 1 >= source.size() || source[next + 1] != '='))
                    return pos;
                pos = end;
            }
            return String::npos;
        }

        // "MESH" -> AssetType::MESH, empty -> NONE (any asset)
        bool ParseAssetKind(const String& text, AssetType& out)
        {
            out = AssetType::NONE;
            if(text.empty())
                return true;
            String upper = text;
            std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            out = AssetTypeFromString(upper.c_str());
            return out != AssetType::NONE;
        }

        // ASSET kind, ENUM options, ARRAY element type ("Float", "Entity", "Asset:MESH", ...)
        bool ReadDescriptorExtras(const sol::table& descriptor, ScriptFieldInfo& info)
        {
            switch(info.Type)
            {
                case ScriptFieldType::ASSET:
                {
                    const sol::object kind = descriptor.raw_get<sol::object>("AssetKind");
                    return ParseAssetKind(kind.get_type() == sol::type::string ? kind.as<String>() : String(), info.AssetKind);
                }
                case ScriptFieldType::ENUM:
                {
                    const sol::object options = descriptor.raw_get<sol::object>("Options");
                    if(options.get_type() != sol::type::table)
                        return false;
                    for(const auto& [key, option] : options.as<sol::table>())
                    {
                        if(option.get_type() == sol::type::string)
                            info.Options.push_back(option.as<String>());
                    }
                    return !info.Options.empty();
                }
                case ScriptFieldType::ARRAY:
                {
                    const sol::object element = descriptor.raw_get<sol::object>("ElementType");
                    if(element.get_type() != sol::type::string)
                        return false;
                    String text = element.as<String>();
                    String kind;
                    if(const size_t colon = text.find(':'); colon != String::npos)
                    {
                        kind = text.substr(colon + 1);
                        text = text.substr(0, colon);
                    }
                    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                    if(!ScriptFieldTypeFromString(text.c_str(), info.ElementType) || info.ElementType == ScriptFieldType::ENUM || info.ElementType == ScriptFieldType::ARRAY)
                        return false;
                    return ParseAssetKind(kind, info.AssetKind);
                }
                default: return true;
            }
        }

        ScriptFieldSchema Extract(const Script& script, AssetID id)
        {
            ScriptFieldSchema schema;
            sol::state_view& lua = *static_cast<sol::state_view*>(Core::GetScriptEngine()->GetSOLState());

            sol::protected_function_result helpers = lua.safe_script(RECORDING_HELPERS, sol::script_pass_on_error, "=ScriptFieldRecorder");
            if(!helpers.valid())
            {
                sol::error err = helpers;
                schema.Error = err.what();
                return schema;
            }
            const sol::table marker = helpers.get<sol::table>(0);
            const sol::table recording = helpers.get<sol::table>(1);

            const Vector<Byte>& bytecode = script.GetBytecode();
            sol::load_result loaded = lua.load(std::string_view(reinterpret_cast<const char*>(bytecode.data()), bytecode.size()), "=script", sol::load_mode::binary);
            if(!loaded.valid())
            {
                sol::error err = loaded;
                schema.Error = err.what();
                return schema;
            }

            sol::protected_function chunk = loaded;
            sol::environment env(lua, sol::create, lua.globals());
            env["Property"] = recording;
            sol::set_environment(env, chunk);
            sol::protected_function_result result = chunk();
            if(!result.valid())
            {
                sol::error err = result;
                schema.Error = err.what();
                return schema;
            }

            const sol::object propertiesObject = env.raw_get<sol::object>("Properties");
            if(propertiesObject.get_type() != sol::type::table)
                return schema;

            for(const auto& [key, value] : propertiesObject.as<sol::table>())
            {
                if(key.get_type() != sol::type::string)
                    continue;

                ScriptFieldInfo info;
                info.Name = key.as<String>();
                if(value.get_type() == sol::type::table && value.as<sol::table>().raw_get<sol::object>(marker).valid())
                {
                    const sol::table descriptor = value.as<sol::table>();
                    const sol::object type = descriptor.raw_get<sol::object>("Type");
                    if(type.get_type() != sol::type::string || !ScriptFieldTypeFromString(type.as<String>().c_str(), info.Type))
                        continue;
                    if(!ReadDescriptorExtras(descriptor, info))
                        continue;
                    if(!ReadValue(descriptor.raw_get<sol::object>("Default"), info.Type, info.ElementType, info.Default))
                        continue;
                    info.Min = descriptor.get_or("Min", 0.0f);
                    info.Max = descriptor.get_or("Max", 0.0f);
                    const sol::object tooltip = descriptor.raw_get<sol::object>("Tooltip");
                    if(tooltip.get_type() == sol::type::string)
                        info.Tooltip = tooltip.as<String>();
                }
                else if(!InferField(value, info))
                    continue;

                schema.Fields.push_back(std::move(info));
            }

            // Declaration order from the source, alphabetical for anything not found there
            String source;
            AssetManager* am = Core::GetAssetManager();
            Filesystem::ReadTextFile(AssetDatabase::Get().GetAbsolutePath(am->GetMetadata(id).RelativePath), source);
            const size_t propertiesPos = source.find("Properties");
            std::unordered_map<String, size_t> order;
            for(const ScriptFieldInfo& field : schema.Fields)
                order[field.Name] = propertiesPos == String::npos ? String::npos : FindDeclaration(source, propertiesPos, field.Name);

            std::sort(schema.Fields.begin(), schema.Fields.end(), [&order](const ScriptFieldInfo& a, const ScriptFieldInfo& b) {
                const size_t orderA = order[a.Name];
                const size_t orderB = order[b.Name];
                return orderA != orderB ? orderA < orderB : a.Name < b.Name;
            });
            return schema;
        }

        // Reloads (and so recooks) the script when its source was saved since the last check, once per second
        void RefreshIfSourceChanged(Ref<Asset>& scriptAsset)
        {
            const double now = ImGui::GetTime();
            SourceWatch& watch = sWatches[scriptAsset->GetID()];
            if(now < watch.NextCheck)
                return;
            watch.NextCheck = now + 1.0;

            AssetManager* am = Core::GetAssetManager();
            const uint64_t modTime = AssetDatabase::GetFileModTime(AssetDatabase::Get().GetAbsolutePath(am->GetMetadata(scriptAsset->GetID()).RelativePath));
            if(watch.ModTime != 0 && modTime != watch.ModTime)
            {
                Ref<Script> reloaded = am->Load<Script>(scriptAsset->GetID());
                if(reloaded)
                    scriptAsset = reloaded;
            }
            watch.ModTime = modTime;
        }

        bool AreEqual(const ScriptFieldValue& a, const ScriptFieldValue& b, ScriptFieldType type, ScriptFieldType elementType)
        {
            switch(type)
            {
                case ScriptFieldType::FLOAT:
                case ScriptFieldType::INT: return a.Number == b.Number;
                case ScriptFieldType::BOOL: return a.Bool == b.Bool;
                case ScriptFieldType::STRING:
                case ScriptFieldType::ENUM: return a.Text == b.Text;
                case ScriptFieldType::ASSET: return a.AssetID == b.AssetID;
                case ScriptFieldType::ARRAY:
                {
                    if(a.Elements.size() != b.Elements.size())
                        return false;
                    for(size_t i = 0; i < a.Elements.size(); i++)
                    {
                        if(!AreEqual(a.Elements[i], b.Elements[i], elementType, ScriptFieldType::FLOAT))
                            return false;
                    }
                    return true;
                }
                case ScriptFieldType::VEC2:
                case ScriptFieldType::VEC3:
                case ScriptFieldType::VEC4:
                case ScriptFieldType::COLOR: return a.Vector == b.Vector;
                case ScriptFieldType::ENTITY: return a.EntityID == b.EntityID;
                default: return false;
            }
        }

        ScriptFieldOverride* FindOverride(Vector<ScriptFieldOverride>& overrides, const ScriptFieldInfo& field)
        {
            for(ScriptFieldOverride& entry : overrides)
            {
                if(entry.Name == field.Name && entry.Type == field.Type && (field.Type != ScriptFieldType::ARRAY || entry.ElementType == field.ElementType))
                    return &entry;
            }
            return nullptr;
        }

        void RemoveOverride(Vector<ScriptFieldOverride>& overrides, const String& name)
        {
            std::erase_if(overrides, [&name](const ScriptFieldOverride& entry) { return entry.Name == name; });
        }

        bool DrawEntityField(ScriptFieldValue& value, Scene* scene)
        {
            bool changed = false;
            Entity target = (scene && value.EntityID) ? scene->FindEntityByUUID(UUID(value.EntityID)) : Entity();

            String label = "None (drop an entity)";
            if(value.EntityID)
                label = target ? (target.HasComponent<NameComponent>() ? target.GetComponent<NameComponent>().Name : String("Entity")) : String("Missing entity");

            const float clearWidth = value.EntityID ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
            if(ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clearWidth, 0.0f)) && target)
                static_cast<Editor*>(Core::GetClient())->SelectEntity(target);
            EditorUI::ItemTooltip(target ? "Click to select it, drop another entity from the Hierarchy to replace it" : "Drop an entity from the Hierarchy");

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(IMGUI_ENTITY_PAYLOAD))
                {
                    Entity dropped(*static_cast<const entt::entity*>(payload->Data), scene);
                    if(dropped && dropped.HasComponent<IDComponent>())
                    {
                        value.EntityID = dropped.GetComponent<IDComponent>().ID.Get();
                        changed = true;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if(value.EntityID)
            {
                ImGui::SameLine();
                if(ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                {
                    value.EntityID = 0;
                    changed = true;
                }
            }
            return changed;
        }

        bool DrawAssetField(ScriptFieldValue& value, AssetType kind)
        {
            bool changed = false;
            AssetManager* am = Core::GetAssetManager();
            const AssetMetadata& meta = am->GetMetadata(AssetID(value.AssetID));
            const String kindName = kind == AssetType::NONE ? String("ASSET") : String(SurgeReflect::EnumToString(kind));

            String label = "None (drop a " + kindName + ")";
            if(value.AssetID)
                label = meta.IsValid() ? Filesystem::GetFilenameWithExt(meta.RelativePath) : String("Missing asset");

            const float clearWidth = value.AssetID ? ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x : 0.0f;
            ImGui::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clearWidth, 0.0f));
            EditorUI::ItemTooltip(value.AssetID && meta.IsValid() ? meta.RelativePath.c_str() : "Drop an asset from the Content Browser");

            if(ImGui::BeginDragDropTarget())
            {
                if(const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CONTENT_BROWSER_PAYLOAD))
                {
                    const AssetID dropped = *static_cast<const AssetID*>(payload->Data);
                    const AssetType droppedType = am->GetMetadata(dropped).Type;
                    if(kind == AssetType::NONE || droppedType == kind)
                    {
                        value.AssetID = dropped.Get();
                        changed = true;
                    }
                    else
                        Log<Severity::Warn>("[Inspector] This property takes a {}, not a {}", kindName, SurgeReflect::EnumToString(droppedType).data());
                }
                ImGui::EndDragDropTarget();
            }

            if(value.AssetID)
            {
                ImGui::SameLine();
                if(ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                {
                    value.AssetID = 0;
                    changed = true;
                }
            }
            return changed;
        }

        bool DrawEnumField(const ScriptFieldInfo& field, ScriptFieldValue& value)
        {
            bool changed = false;
            if(ImGui::BeginCombo("##Value", value.Text.c_str()))
            {
                for(const String& option : field.Options)
                {
                    const bool selected = option == value.Text;
                    if(ImGui::Selectable(option.c_str(), selected) && !selected)
                    {
                        value.Text = option;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool DrawValue(const ScriptFieldInfo& field, ScriptFieldValue& value, Scene* scene);

        // Header in the current row ("3 items" +), then one row per element with a remove button
        bool DrawArrayField(const ScriptFieldInfo& field, ScriptFieldValue& value, Scene* scene)
        {
            bool changed = false;
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%zu item%s", value.Elements.size(), value.Elements.size() == 1 ? "" : "s");
            ImGui::SameLine();
            if(ImGui::SmallButton("+"))
            {
                ScriptFieldValue& element = value.Elements.emplace_back();
                if(field.ElementType == ScriptFieldType::COLOR)
                    element.Vector = glm::vec4(1.0f);
                changed = true;
            }

            ScriptFieldInfo elementInfo;
            elementInfo.Type = field.ElementType;
            elementInfo.AssetKind = field.AssetKind;

            int removeIndex = -1;
            for(size_t i = 0; i < value.Elements.size(); i++)
            {
                ImGui::PushID(static_cast<int>(i));
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("    [%zu]", i + 1);
                ImGui::TableNextColumn();
                const float removeWidth = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x;
                ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - removeWidth);
                changed |= DrawValue(elementInfo, value.Elements[i], scene);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if(ImGui::Button("-", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
                    removeIndex = static_cast<int>(i);
                ImGui::PopID();
            }
            if(removeIndex >= 0)
            {
                value.Elements.erase(value.Elements.begin() + removeIndex);
                changed = true;
            }
            return changed;
        }

        bool DrawValue(const ScriptFieldInfo& field, ScriptFieldValue& value, Scene* scene)
        {
            const bool hasRange = field.Min < field.Max;
            const ImGuiSliderFlags clamp = hasRange ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
            switch(field.Type)
            {
                case ScriptFieldType::FLOAT:
                {
                    float number = static_cast<float>(value.Number);
                    const float speed = hasRange ? (field.Max - field.Min) / 200.0f : 0.05f;
                    if(!ImGui::DragFloat("##Value", &number, speed, field.Min, field.Max, "%.3f", clamp))
                        return false;
                    value.Number = number;
                    return true;
                }
                case ScriptFieldType::INT:
                {
                    int number = static_cast<int>(std::llround(value.Number));
                    if(!ImGui::DragInt("##Value", &number, 0.2f, static_cast<int>(field.Min), static_cast<int>(field.Max), "%d", clamp))
                        return false;
                    value.Number = number;
                    return true;
                }
                case ScriptFieldType::BOOL: return ImGui::Checkbox("##Value", &value.Bool);
                case ScriptFieldType::STRING: return ImGui::InputText("##Value", &value.Text);
                case ScriptFieldType::VEC2: return ImGui::DragFloat2("##Value", glm::value_ptr(value.Vector), 0.05f, 0.0f, 0.0f, "%.2f");
                case ScriptFieldType::VEC3: return ImGui::DragFloat3("##Value", glm::value_ptr(value.Vector), 0.05f, 0.0f, 0.0f, "%.2f");
                case ScriptFieldType::VEC4: return ImGui::DragFloat4("##Value", glm::value_ptr(value.Vector), 0.05f, 0.0f, 0.0f, "%.2f");
                case ScriptFieldType::COLOR: return ImGui::ColorEdit4("##Value", glm::value_ptr(value.Vector), ImGuiColorEditFlags_AlphaBar);
                case ScriptFieldType::ENTITY: return DrawEntityField(value, scene);
                case ScriptFieldType::ASSET: return DrawAssetField(value, field.AssetKind);
                case ScriptFieldType::ENUM: return DrawEnumField(field, value);
                case ScriptFieldType::ARRAY: return DrawArrayField(field, value, scene);
                default: return false;
            }
        }
    } // namespace

    const ScriptFieldSchema& ScriptFields::GetSchema(const Ref<Asset>& scriptAsset)
    {
        const Script* script = scriptAsset.As<Script>().Raw();
        CacheEntry& entry = sCache[scriptAsset->GetID()];
        if(entry.Source != script || entry.BytecodeSize != script->GetBytecode().size())
        {
            entry.Source = script;
            entry.BytecodeSize = script->GetBytecode().size();
            entry.Schema = Extract(*script, scriptAsset->GetID());
        }
        return entry.Schema;
    }

    bool ScriptFields::DrawInspector(Ref<Asset>& scriptAsset, Vector<ScriptFieldOverride>& overrides, ScriptInstance* liveInstance, Scene* scene)
    {
        if(!scriptAsset)
            return false;

        const bool live = liveInstance && liveInstance->IsValid();
        if(!static_cast<Editor*>(Core::GetClient())->IsPlaying())
        {
            RefreshIfSourceChanged(scriptAsset);

            // A reload only replaces the AssetManager's cached Script (and the reference of the component that triggered it):
            // every other component using the same script catches up here, so all of them show the new fields
            if(Ref<Script> latest = Core::GetAssetManager()->GetLoaded<Script>(scriptAsset->GetID()); latest && latest.Raw() != scriptAsset.Raw())
                scriptAsset = latest;
        }

        const ScriptFieldSchema& schema = GetSchema(scriptAsset);
        if(!schema.Error.empty())
        {
            //EditorUI::TSeparator("Properties", true);
            const String firstLine = schema.Error.substr(0, schema.Error.find('\n'));
            EditorUI::THint(("Script error: " + firstLine).c_str(), EditorUI::Colors::Error, 0.0f);
            EditorUI::ItemTooltip(schema.Error.c_str());
            return false;
        }
        if(schema.Fields.empty())
            return false;

        //EditorUI::TSeparator(live ? "Properties (LIVE)" : "Properties", true);
        bool modified = false;
        for(const ScriptFieldInfo& field : schema.Fields)
        {
            ImGui::PushID(field.Name.c_str());
            ScriptFieldOverride* fieldOverride = live ? nullptr : FindOverride(overrides, field);

            ScriptFieldValue value = fieldOverride ? fieldOverride->Value : field.Default;
            if(live)
            {
                const sol::object properties = liveInstance->Env.raw_get<sol::object>("Properties");
                if(properties.get_type() == sol::type::table)
                    ReadValue(properties.as<sol::table>().raw_get<sol::object>(field.Name), field.Type, field.ElementType, value);
            }

            // Label: highlighted when this entity overrides the script's default, right click to reset
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            if(fieldOverride)
                ImGui::PushStyleColor(ImGuiCol_Text, EditorUI::Colors::Accent);
            ImGui::TextUnformatted(field.Name.c_str());
            if(fieldOverride)
                ImGui::PopStyleColor();
            if(!field.Tooltip.empty())
                EditorUI::ItemTooltip(field.Tooltip.c_str());
            if(fieldOverride && EditorUI::BeginStyledPopupContextItem("##FieldMenu"))
            {
                if(EditorUI::StyledMenuItem("Reset to default"))
                {
                    RemoveOverride(overrides, field.Name);
                    fieldOverride = nullptr;
                    modified = true;
                }
                EditorUI::EndStyledPopup();
            }

            ImGui::TableNextColumn();
            ImGui::PushItemWidth(-1);
            if(DrawValue(field, value, scene))
            {
                modified = true;
                if(live)
                    Core::GetScriptEngine()->SetFields(*liveInstance, { ScriptFieldOverride { field.Name, field.Type, field.ElementType, value } }, scene);
                else if(AreEqual(value, field.Default, field.Type, field.ElementType))
                    RemoveOverride(overrides, field.Name);
                else if(fieldOverride)
                    fieldOverride->Value = value;
                else
                    overrides.push_back({ field.Name, field.Type, field.ElementType, value });
            }
            ImGui::PopItemWidth();
            ImGui::PopID();
        }
        return modified;
    }

} // namespace Surge
