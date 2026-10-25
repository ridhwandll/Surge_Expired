// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Project.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <json/json.hpp>
#include <algorithm>

namespace Surge
{
    static nlohmann::json PhysicsToJson(const PhysicsSettings& settings)
    {
        nlohmann::json out;
        out["Gravity"] = { settings.Gravity.x, settings.Gravity.y, settings.Gravity.z };
        out["FixedRate"] = settings.FixedRate;
        out["CollisionSubsteps"] = settings.CollisionSubsteps;
        out["Layers"] = settings.LayerNames;
        out["CollisionMatrix"] = settings.CollisionMatrix;
        return out;
    }

    // Missing or mistyped entries keep their defaults: projects saved before physics settings existed load unchanged
    static void PhysicsFromJson(const nlohmann::json& in, PhysicsSettings& settings)
    {
        const auto gravity = in.find("Gravity");
        if(gravity != in.end() && gravity->is_array() && gravity->size() == 3)
            settings.Gravity = { (*gravity)[0].get<float>(), (*gravity)[1].get<float>(), (*gravity)[2].get<float>() };

        settings.FixedRate = std::clamp(in.value("FixedRate", settings.FixedRate), PhysicsSettings::MIN_FIXED_RATE, PhysicsSettings::MAX_FIXED_RATE);
        settings.CollisionSubsteps = std::clamp(in.value("CollisionSubsteps", settings.CollisionSubsteps), 1, PhysicsSettings::MAX_SUBSTEPS);

        const auto layers = in.find("Layers");
        if(layers != in.end() && layers->is_array())
            for(size_t i = 1; i < std::min(layers->size(), settings.LayerNames.size()); i++) // Layer 0 stays "Default"
                if((*layers)[i].is_string())
                    settings.LayerNames[i] = (*layers)[i].get<String>();

        const auto matrix = in.find("CollisionMatrix");
        if(matrix != in.end() && matrix->is_array())
            for(size_t i = 0; i < std::min(matrix->size(), settings.CollisionMatrix.size()); i++)
                if((*matrix)[i].is_number_unsigned())
                    settings.CollisionMatrix[i] = (*matrix)[i].get<uint16_t>();
    }

    bool Project::Save([[maybe_unused]] const Path& path) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[Project] Save is unsupported on Android runtime. APK assets are readonly!");
        return false;
#else
        nlohmann::json outJson;
        outJson["Project"]["Name"] = Name;
        outJson["Project"]["Version"] = Version;
        outJson["Project"]["StartScene"] = StartScene.Get(); // Not static_cast: on a const UUID it goes through operator bool and writes 1
        outJson["Project"]["Physics"] = PhysicsToJson(Physics);

        if(!Filesystem::WriteTextFile(path, outJson.dump(4)))
        {
            Log<Severity::Error>("[Project] Failed to write to '{}'", path.string());
            return false;
        }
        return true;
#endif
    }

    bool Project::Load(const Path& path)
    {
        String jsonContents;
        if(!Filesystem::ReadTextFile(path, jsonContents))
        {
            Log<Severity::Error>("[Project] Failed to read file '{}'", path.string());
            return false;
        }

        nlohmann::json parsedJson = nlohmann::json::parse(jsonContents);
        if(parsedJson.is_discarded())
        {
            Log<Severity::Error>("[Project] Corrupt or invalid JSON at '{}'", path.string());
            return false;
        }

        if(!parsedJson.contains("Project"))
        {
            Log<Severity::Error>("[Project] Corrupted file '{}'", path.string());
            return false;
        }

        const auto& projNode = parsedJson["Project"];
        Name = projNode.value("Name", "");
        Version = projNode.value("Version", "1.0");
        StartScene = static_cast<AssetID>(projNode.value("StartScene", 0ULL));
        Physics = {};
        if(projNode.contains("Physics") && projNode["Physics"].is_object())
            PhysicsFromJson(projNode["Physics"], Physics);
        return true;
    }
}
