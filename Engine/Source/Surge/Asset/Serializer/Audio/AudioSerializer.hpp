// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Serializer/IAssetSerializer.hpp"

namespace Surge
{
    class AudioSerializer : public AssetSerializer
    {
    public:
        virtual ~AudioSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;
    };
}