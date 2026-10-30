// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Project.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/CookedFile.hpp"
#include <algorithm>

namespace Surge
{
    SURGE_RAPTOR_FIELDS(PhysicsSettings, v.Gravity, v.FixedRate, v.CollisionSubsteps, v.LayerNames, v.CollisionMatrix)

    bool Project::LoadCooked(const Path& path)
    {
        CookedFile file;
        const CookedFileError error = CookedFileIO::Read(path, AssetType::NONE, COOKED_VERSION, file);
        if(error != CookedFileError::NONE)
        {
            Log<Severity::Error>("[Project] '{}' can't be loaded: {}", path.string(), CookedFileErrorToString(error));
            return false;
        }

        Project project;
        Rapt0R::Reader reader = file.GetPayload();
        if(!reader.Read(project.Name, project.Version, project.StartScene, project.Physics) || project.Name.empty())
        {
            Log<Severity::Error>("[Project] '{}' is truncated or corrupted", path.string());
            return false;
        }

        project.Physics.FixedRate = std::clamp(project.Physics.FixedRate, PhysicsSettings::MIN_FIXED_RATE, PhysicsSettings::MAX_FIXED_RATE);
        project.Physics.CollisionSubsteps = std::clamp(project.Physics.CollisionSubsteps, 1, PhysicsSettings::MAX_SUBSTEPS);
        *this = std::move(project);
        return true;
    }

    bool Project::SaveCooked([[maybe_unused]] const Path& path) const
    {
#ifdef SURGE_PLATFORM_ANDROID
        Log<Severity::Error>("[Project] SaveCooked is unsupported on Android runtime. APK assets are readonly!");
        return false;
#else
        Rapt0R::Writer writer(1024);
        writer.Write(Name, Version, StartScene, Physics);

        CookedHeader header;
        header.Type = AssetType::NONE;
        if(writer.HasError() || !CookedFileIO::Write(path, header, COOKED_VERSION, writer.WrittenSpan()))
        {
            Log<Severity::Error>("[Project] Failed to write '{}'", path.string());
            return false;
        }
        return true;
#endif
    }
}
