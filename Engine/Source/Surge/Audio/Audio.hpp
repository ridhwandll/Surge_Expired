// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"

namespace Surge
{
    class Audio : public Asset
    {
    public:
        Audio(const String& path)
            : mFilepath(path) {}

        SURGE_ASSET_TYPE(AssetType::AUDIO);

        const String& GetFilepath() const { return mFilepath; }
    private:
        String mFilepath;
    };
}
