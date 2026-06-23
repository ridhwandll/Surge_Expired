// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetSerializer.hpp"
#include "Surge/Graphics/HighLevel/Texture2D.hpp"

namespace Surge
{
    class Texture2DSerializer : public AssetSerializer
    {
    public:
        virtual ~Texture2DSerializer() = default;

        virtual void Initialize() override;
        virtual bool Serialize(Ref<Asset> asset) const override;
        virtual Ref<Asset> Deserialize(const AssetMetadata& metadata) const override;
        virtual void Shutdown() override;

        // Returns raw GPU Data transcoded from KTX2
        // The caller is responsible for freeing the returned data buffer after upload to GPU
        // @param ktx2Data:     Raw KTX2 data blob
        // @param debugStr:     Optional string to identify the source of the KTX2 data in error logs
        // @param raw:          If true, ktx2Data is expected to start directly with the KTX2 header. If false, ktx2Data is expected to have an AssetStamp at the beginning, and the actual KTX2 data starts after the AssetStamp.
        // @returns:            TextureSpecification
        static TextureSpecification LoadFromKTX2(const Vector<Byte>& ktx2Data, const String& debugStr = "", bool raw = true);

        static Ref<Texture2D> LoadFromKTX2(const String& ktx2Path);
    };
}