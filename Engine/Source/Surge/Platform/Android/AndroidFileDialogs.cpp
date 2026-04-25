// Copyright (c) - SurgeTechnologies - All rights reserved
// File dialogs are not supported on Android; all functions return empty strings.
#ifdef SURGE_ANDROID
#include "Surge/Utility/FileDialogs.hpp"

namespace Surge
{
    String FileDialog::OpenFile(const char*) { return String(); }
    String FileDialog::SaveFile(const char*) { return String(); }
    String FileDialog::ChooseFolder() { return String(); }

} // namespace Surge

#endif // SURGE_ANDROID
