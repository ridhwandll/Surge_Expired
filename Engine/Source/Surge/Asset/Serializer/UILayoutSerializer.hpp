// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetSerializer.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"

namespace Surge
{
    // .sui files are human readable JSON (diffable, mergeable), read directly at runtime like scenes, no cooking step
    class UILayoutSerializer : public AssetSerializer
    {
    public:
        virtual ~UILayoutSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;

        static String ToJson(const UILayout& layout);
        static bool FromJson(const String& json, UILayout& outLayout);
    };
}
