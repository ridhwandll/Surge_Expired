// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    // MaterialBinary (.rMATERIAL payload: one entry, meshes embed their glTF materials as entries)
    // HasData       bool, nothing follows for a null material
    // Name          String
    // CPUBufferData Uint size + the shader's material UBO bytes
    // Textures      Uint count + (Name (name in shader, Ex: AlbedoMap), AssetID) per owned texture slot
    namespace MaterialBinary
    {
        inline constexpr uint16_t VERSION = 1;

        void Write(Rapt0R::Writer& writer, const Material* mat);
        Ref<Material> Read(const AssetContext& context, Rapt0R::Reader& reader); // Null when the material has no data or the reader failed
    }

    class MaterialFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::MATERIAL; }
        virtual uint16_t GetFormatVersion() const override { return MaterialBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual bool Write(Rapt0R::Writer& payload, const Asset& asset) const override;
    };

} // namespace Surge
