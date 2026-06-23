// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/HighLevel/Font.hpp"
#include "Surge/Asset/Serializer/AssetStamp.hpp"

namespace Surge::FontBinary
{
    bool Write(const String& path, const AssetStamp& stamp, const FontSpecification& spec);
    bool Read(const String& path, AssetStamp& outstamp, FontSpecification& outSpec);
}