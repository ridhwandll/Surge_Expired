// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Serializer/IAssetSerializer.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"

namespace Surge
{
    class FontSerializer final : public AssetSerializer
    {
    public:
        virtual ~FontSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;
    };
}