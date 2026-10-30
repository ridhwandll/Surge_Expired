// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "String.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Asset/Asset.hpp"
#include "Surge/Physics/PhysicsSettings.hpp"

namespace Surge
{
    struct Project
    {
        // Exported games: Internal/Project.rPROJ in the assets directory, written by the Editor's export. The Editor's source is <Name>.surgeproj (JSON)
        static constexpr const char* COOKED_FILE_NAME = "Internal/Project.rPROJ";
        static constexpr uint16_t COOKED_VERSION = 1;

        String Name;
        String Version;
        AssetID StartScene = AssetID::INVALID;
        PhysicsSettings Physics;

        bool IsValid() const { return !Name.empty(); }
        void Clear() { Name.clear(); Version.clear(); StartScene = AssetID(AssetID::INVALID); Physics = {}; }

        bool LoadCooked(const Path& path);
        bool SaveCooked(const Path& path) const;
    };
}
