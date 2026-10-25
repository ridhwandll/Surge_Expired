// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge
{
    class Scene;
}

namespace Surge::SceneBinary
{
    // SceneBinary (.rSCENE), the only scene format the runtime loads (the .srg JSON is the Editor's source)
    // AssetStamp
    // Header        Magic, Version, SchemaHash, EntityCount
    // Per entity    ComponentMask (bit i = i-th entry of SERIALIZABLE_COMPONENTS), then every present component in list order:
    //               its reflected variables in declaration order + the special parts (relationship links as UUIDs, script lists, script fields)
    // Little endian like every cooked format (x64, arm64)

    // VERSION mixed with the hash of the serializable component layout. It is also the SceneCooker version
    Uint GetFormatVersion();

    bool Write(const String& path, const AssetStamp& stamp, Scene* scene);
    bool Read(const String& path, AssetStamp& outStamp, Scene* outScene);
}
