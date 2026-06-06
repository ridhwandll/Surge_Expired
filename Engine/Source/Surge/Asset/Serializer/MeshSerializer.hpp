// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "IAssetSerializer.hpp"
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
    private:
        bool CheckAndGenerateDefaultMesh(const String& filepath, MeshSpecification& outSpec) const;
        MeshSpecification LoadMesh(const String& filepath) const;
    };
}