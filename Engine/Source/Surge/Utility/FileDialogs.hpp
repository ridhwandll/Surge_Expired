// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class FileDialog
    {
    public:
        static String OpenFile(const char* filter);
        static String SaveFile(const char* filter, const char* defaultName = "");
        static String ChooseFolder();
    };

} // namespace Surge::FileDialog
