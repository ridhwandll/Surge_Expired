// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"

namespace Surge::MaterialBinary
{
    Uint CalculateSize(const Ref<Material>& mat);
    void WriteBuffer(Vector<Byte>& buffer, const Ref<Material>& mat);
    Ref<Material> ReadBuffer(const Byte*& ptr);

    bool Write(const String& path, const Ref<Material>& mat);
    bool Read(const String& path, Ref<Material>& outMat);
}