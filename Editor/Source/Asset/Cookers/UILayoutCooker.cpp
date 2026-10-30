// Copyright (c) - SurgeTechnologies - All rights reserved
#include "UILayoutCooker.hpp"
#include "Surge/Asset/Format/BinarySerialization.hpp"
#include "Surge/Asset/Format/UILayout/UILayoutFormat.hpp"
#include "Asset/SourceIO/UILayoutSourceIO.hpp"

namespace Surge
{
    bool UILayoutCooker::Cook(const CookContext& context) const
    {
        Ref<UILayout> layout = UILayout::Create();
        if(!UILayoutSourceIO::Read(context.SourcePath, *layout))
        {
            Log<Severity::Error>("[UILayoutCooker] Failed to read UI layout: {}", context.SourcePath.string());
            return false;
        }

        Rapt0R::Writer payload(16 * 1024);
        UILayoutBinary::Write(payload, *layout);
        return context.Write(UILayoutBinary::VERSION, payload);
    }
}
