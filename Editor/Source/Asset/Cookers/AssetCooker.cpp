// Copyright (c) - SurgeTechnologies - All rights reserved
#include "AssetCooker.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"

namespace Surge
{
    bool CookContext::Write(uint16_t formatVersion, const Rapt0R::Writer& payload) const
    {
        if(payload.HasError())
        {
            Log<Severity::Error>("[AssetCooker] Could not build the cooked data of {}", Metadata.RelativePath);
            return false;
        }
        if(!CookedFileIO::Write(OutputPath, Header, formatVersion, payload.WrittenSpan()))
        {
            Log<Severity::Error>("[AssetCooker] Failed to write {}", OutputPath.string());
            return false;
        }
        return true;
    }
}
