// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge::MeshBinary
{
    //[Header: 32 bytes]
    //    Magic                  Uint(4 bytes)  "RIDM"
    //    Version                Uint(4 bytes)
    //    VertexCount            Uint(4 bytes)
    //    IndexCount             Uint(4 bytes)
    //    SubmeshCount           Uint(4 bytes)
    //    TransientMaterialCount Uint(4 bytes)
    //    ValidOverrides         Uint(4 bytes)
    //    GeomSectionSize        Uint(4 bytes)  Bytes from start of BLOB:Vertices to end of BLOB:TransientMaterials section
    // 
    //    [BLOB: Vertices          ]
    //    [BLOB: Indices           ]
    //    [BLOB: Submeshes         ]
    //    [BLOB: TransientMaterials] (Look into MaterialBinaryFormat.hpp for sizes)
    //    MaterialOverrides          For each override: Uint slotIndex + RawID: Uint64

    bool Write(const String& path, const AssetStamp& stamp, const MeshSpecification& spec, const Vector<Ref<Material>>& overrides);
    bool Read(const String& path, AssetStamp& outstamp, MeshSpecification& outSpec);
}