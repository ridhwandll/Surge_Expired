// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetSerializer.hpp"

namespace Surge
{
    class Texture2DSerializer : public AssetSerializer
    {
    public:
        virtual ~Texture2DSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize([[maybe_unused]] Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;
    private:
        Ref<Asset> LoadFromKTX2(const String& ktx2Path) const;
    };
}