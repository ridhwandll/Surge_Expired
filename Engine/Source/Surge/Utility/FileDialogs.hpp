// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class FileDialog
    {
    public:
        static SURGE_API String OpenFile(const char* filter);
        static SURGE_API String SaveFile(const char* filter);
        static SURGE_API String ChooseFolder();
    };

} // namespace Surge::FileDialog
