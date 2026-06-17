// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Asset.hpp"
#include "Surge/Asset/AssetMetadata.hpp"

namespace Surge
{
    class AssetSerializer
    {
    public:
        virtual ~AssetSerializer() = default;

        virtual void Initialize() = 0;
        virtual bool Serialize(Ref<Asset> asset) const = 0;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const = 0;
        virtual void Shutdown() = 0;

        AssetType GetSerializerType() const { return mSerializerType; }
    protected:
        AssetType mSerializerType;
    };
}