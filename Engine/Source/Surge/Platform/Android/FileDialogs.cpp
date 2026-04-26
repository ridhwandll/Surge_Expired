// Copyright (c) - SurgeTechnologies - All rights reserved
// File dialogs are not supported on Android; all functions return empty strings.
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Utility/FileDialogs.hpp"

namespace Surge
{
    String FileDialog::OpenFile(const char* filter)
    {
        return String();
    }

    String FileDialog::SaveFile(const char* filter)
    {
        return String();
    }

    Surge::String FileDialog::ChooseFolder()
    {
        return "";
    }

} // namespace Surge
#endif // SURGE_PLATFORM_ANDROID