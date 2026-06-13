// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetSerializer.hpp"

namespace Surge
{
    class SceneSerializer : public AssetSerializer
    {
    public:
        virtual ~SceneSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;

    };
}