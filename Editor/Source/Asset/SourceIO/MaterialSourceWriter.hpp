// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include <Surge/Graphics/HighLevel/Material.hpp>

namespace Surge::MaterialSourceWriter
{
    // Writes human-editable .smat JSON version-controllable file
    bool Write(const Ref<Material>& mat, const String& absolutePath);

    // Reads .smat JSON back into a Material used by MaterialCooker
    bool Read(Ref<Material>& mat, const String& absolutePath);
}
