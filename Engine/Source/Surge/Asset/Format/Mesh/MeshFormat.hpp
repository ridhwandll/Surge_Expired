// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Asset/Format/AssetFormat.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"

namespace Rapt0R
{
    class Reader;
}

namespace Surge
{
    // MeshBinary (.rMESH) payload
    // Vertices, Indices        Uint count + raw Vertex/Index blobs
    // Submeshes                Uint count + per submesh: offsets/counts, bounds, transforms, NodeName, MeshName
    // TransientMaterials       Uint count + MaterialBinary entries (the glTF materials, see MaterialFormat.hpp)
    // MaterialOverrides        Uint count + (slot index: Uint, AssetID) per overridden slot. Their source is the model's .import file (Editor)
    // HasSkeleton              bool, false = static mesh, nothing follows
    // Skeleton                 NodeNames, Parents, RestPose, PaletteNodes, InverseBind, PaletteBounds (Uint count + entries each)
    // Skin                     Uint count + raw VertexSkin blob, parallel to Vertices
    // Clips                    Uint count + per clip: Name, Duration, SampleRate, FrameCount, Tracks, Translations, Rotations, Scales
    namespace MeshBinary
    {
        inline constexpr uint16_t VERSION = 1;

        // overrides: one AssetID per material slot, INVALID = the slot keeps its glTF material
        void Write(Rapt0R::Writer& writer, const MeshSpecification& spec, const Vector<AssetID>& overrides);
        bool Read(const AssetContext& context, Rapt0R::Reader& reader, MeshSpecification& outSpec); // context creates the transient materials
    }

    class MeshFormat final : public AssetFormat
    {
    public:
        virtual AssetType GetType() const override { return AssetType::MESH; }
        virtual uint16_t GetFormatVersion() const override { return MeshBinary::VERSION; }
        virtual Ref<Asset> Read(CookedFile& file, const AssetMetadata& metadata, const AssetContext& context) const override;
        virtual Ref<Asset> CreateBuiltin(const AssetMetadata& metadata, const AssetContext& context) const override; // DefaultMesh::CUBE & co
    };

} // namespace Surge
