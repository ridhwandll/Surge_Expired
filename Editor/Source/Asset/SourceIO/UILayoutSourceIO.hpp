// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Graphics/UISystem/UILayout.hpp"

namespace Surge
{
    // .sui files are human readable JSON (diffable, mergeable), the Editor's source of UI layouts. Every build loads the cooked layout
    namespace UILayoutSourceIO
    {
        String ToJson(const UILayout& layout);
        bool FromJson(const String& json, UILayout& outLayout);

        bool Write(const UILayout& layout, const Path& path);
        bool Read(const Path& path, UILayout& outLayout);
    }
}
