// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    class Scene;

    // SceneBinary (.rSCENE) payload, cooked from (or saved next to) the Editor's .srg JSON source
    // SchemaHash, EntityCount
    // Per entity    ComponentMask (bit i = i-th entry of SERIALIZABLE_COMPONENTS), then every present component in list order:
    //               its reflected variables in declaration order + the special parts (relationship links as UUIDs, script lists, script fields)
    namespace SceneBinary
    {
        inline constexpr uint16_t VERSION = 1;

        // VERSION mixed with the hash of the serializable component layout, the SceneCooker version: a component change re-cooks every scene
        Uint GetSchemaVersion();

        void Write(Rapt0R::Writer& writer, Scene* scene);
        bool Read(Rapt0R::Reader& reader, Scene* outScene); // Fails on a payload written with another component layout
    }

    class SceneFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::SCENE; }
        virtual uint16_t GetFormatVersion() const override { return SceneBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
