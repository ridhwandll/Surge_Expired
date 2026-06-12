// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"

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
    //    [BLOB: TransientMaterials]
    //    MaterialOverrides         For each override: Uint slotIndex + RawID: Uint64

    // Each [TransientMaterials] (Look into MaterialSerializer.cpp)
    // HasData (1 byte)
    // Material Name (Uint length + String chars)
    // CPUBufferData (Uint length + Buffer bytes)
    // Texture Count (Uint count)
    // Textures      (For each texture: Name: (Uint length + String chars) + RawID: Uint64)

    bool Write(const String& path, const MeshSpecification& spec, const Vector<Ref<Material>>& overrides);
    bool Read(const String& path, MeshSpecification& outSpec);
}