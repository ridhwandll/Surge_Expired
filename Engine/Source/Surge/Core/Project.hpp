// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "String.hpp"
#include "Surge/Asset/Asset.hpp"

namespace Surge
{
    struct Project
    {
        String Name;
        String Version;
        AssetID StartScene = AssetID::INVALID;

        bool IsValid() const { return !Name.empty(); }
        void Clear() { Name.clear(); Version.clear(); StartScene = AssetID(AssetID::INVALID); }
    };
}