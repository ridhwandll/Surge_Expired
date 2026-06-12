// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Serializer/IAssetSerializer.hpp"

namespace Surge
{
    class Material;
    class MaterialSerializer : public AssetSerializer
    {
    public:
        virtual ~MaterialSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;
    };
}