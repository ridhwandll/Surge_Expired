// Copyright (c) - SurgeTechnologies - All rights reserved
#include "MaterialCooker.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/Material/MaterialFormat.hpp"

#include "Asset/SourceIO/MaterialSourceWriter.hpp"

namespace Surge
{
    bool MaterialCooker::Cook(const CookContext& context) const
    {
        Ref<Material> material = Material::Create(*Core::GetRenderer(), "MaterialCooker::Cook");
        if(!MaterialSourceWriter::Read(material, context.SourcePath.generic_string()))
            Log<Severity::Warn>("[MaterialCooker] Could not read {}, cooking the default material", context.SourcePath.string());

        Rapt0R::Writer payload(1024);
        MaterialBinary::Write(payload, material.Raw());
        return context.Write(MaterialBinary::VERSION, payload);
    }
}
