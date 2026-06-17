// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge
{
    class Script; // ScriptAsset.hpp contains the fat ass sol
}
namespace Surge::ScriptBinary
{
    // ScriptBinary
    bool Write(const String& path, const AssetStamp& stamp, const Ref<Script>& script);
    bool Read(const String& path, AssetStamp& outStamp, Ref<Script>& outScript);
}