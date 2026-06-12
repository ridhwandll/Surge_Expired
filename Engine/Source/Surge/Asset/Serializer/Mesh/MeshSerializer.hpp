// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Serializer/IAssetSerializer.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"

namespace Surge
{
    class MeshSerializer : public AssetSerializer
    {
    public:
        virtual ~MeshSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;
    };
}