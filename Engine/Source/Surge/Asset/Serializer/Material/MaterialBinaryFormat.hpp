// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge::MaterialBinary
{
    // MaterialBinary
    // HasData (1 byte)
    // Material Name (Uint length + String chars)
    // CPUBufferData (Uint length + Buffer bytes)
    // Texture Count (Uint count)
    // Textures      (For each texture: Name: (Uint length + String chars) + RawID: Uint64)

    Uint CalculateSize(const Ref<Material>& mat);
    void WriteBuffer(Vector<Byte>& buffer, const Ref<Material>& mat);
    Ref<Material> ReadBuffer(const Byte*& ptr);

    bool Write(const String& path, const AssetStamp& stamp, const Ref<Material>& mat);
    bool Read(const String& path, AssetStamp& outStamp, Ref<Material>& outMat);
}