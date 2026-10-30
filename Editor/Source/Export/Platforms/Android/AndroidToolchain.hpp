// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Path.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Vector.hpp"

namespace Surge
{
    enum class AndroidToolState
    {
        OK = 0,
        WARN,    // Missing, but the build still works (Gradle downloads it, or only 'Install on device' needs it)
        BLOCKING // The build can't run without it
    };

    // One path the Android build uses, shown in the Export panel
    struct AndroidToolItem
    {
        String Name;     // "NDK 27.3.13750724"
        String Location; // Resolved path, or where it is expected when missing. Empty: unknown
        String Note;     // Where it came from, or how to get it
        AndroidToolState State = AndroidToolState::OK;
    };

    // What Android/gradlew.bat will use, resolved the way gradlew.bat and the Android Gradle Plugin do.
    // Versions come from Android/app/build.gradle and the Gradle wrapper. Main thread, reads a few small files
    struct AndroidToolchain
    {
        Path Sdk; // Empty when not found
        Path Jdk;
        Path Adb;
        Path Ndk; // The version build.gradle asks for, empty when not installed
        Vector<AndroidToolItem> Items;
        Vector<String> Issues; // Block the build

        // Found only in the user's registry environment: set after the Editor started, so child processes don't inherit them
        String JdkEnvToApply; // "JAVA_HOME" or empty
        String SdkEnvToApply; // "ANDROID_HOME" / "ANDROID_SDK_ROOT" or empty

        static AndroidToolchain Detect(int targetApi);

        // Passes the variables above to child processes (Gradle, adb), so the Editor doesn't need a restart after running Scripts/AndroidSetup.bat
        void ApplyToProcessEnvironment() const;
    };

} // namespace Surge
