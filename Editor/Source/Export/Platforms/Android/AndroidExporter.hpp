// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportUtils.hpp"
#include "Export/PlatformExporter.hpp"
#include <iterator>

namespace Surge
{
    enum class AndroidPackageFormat : int { APK = 0, AAB = 1 };

    struct AndroidExportSettings : ExportPlatformSettings
    {
        String IconPath; // .png (192x192 or larger)
        String PackageId = "com.ridt.game";
        String KeystorePath;
        String KeyAlias;
        String KeystorePass; // memory only - never serialized, passed to Gradle through the environment
        String KeyPass;      // empty = same as keystore password
        AndroidPackageFormat Format = AndroidPackageFormat::APK;
        int MinApi = 29;
        int Orientation = 0;    // index into AndroidExporter::ORIENTATIONS
        bool Immersive = true;  // hide system bars (swipe shows them transiently)
        bool UseCutout = true;  // draw under the notch / camera cut-out
        bool KeepScreenOn = true;
        bool ExitOnBack = true;
        bool InstallOnDevice = false;   // APK only, through adb
        bool LaunchAfterInstall = true;
        bool VerboseLog = true;         // Every compiled file goes to the Console
    };

    // Value is the android:screenOrientation string, injected into the manifest through a Gradle placeholder
    struct AndroidOrientation
    {
        const char* Name;
        const char* Value;
    };

    struct AndroidApiLevel
    {
        int Level;
        const char* Name;
    };

    // Stages the content into the Gradle project's assets and drives android/gradlew.bat (see android/app/build.gradle for the -P contract)
    class AndroidExporter : public PlatformExporter
    {
    public:
        static constexpr const char* PROJECT_DIR = "android";

        static constexpr AndroidOrientation ORIENTATIONS[] = {
            {"Landscape (Locked)", "landscape"},
            {"Landscape (Flips with sensor)", "sensorLandscape"},
            {"Portrait (Locked)", "portrait"},
            {"Portrait (Flips with sensor)", "sensorPortrait"},
            {"Auto-rotate (All)", "fullSensor"},
        };
        static constexpr int ORIENTATION_COUNT = static_cast<int>(std::size(ORIENTATIONS));

        static constexpr AndroidApiLevel API_LEVELS[] = {
            {29, "API 29 (Android 10)"}, {30, "API 30 (Android 11)"},
            {31, "API 31 (Android 12)"}, {33, "API 33 (Android 13)"}, {34, "API 34 (Android 14)"},
        };

        static ExportPlatform GetStaticPlatform() { return ExportPlatform::ANDROID; }

        virtual ExportPlatform GetPlatform() const override { return GetStaticPlatform(); }
        virtual const char* GetDisplayName() const override { return "Android"; }

        virtual ExportPlatformSettings& GetBaseSettings() override { return mSettings; }
        virtual void ResetSettings() override { mSettings = {}; }
        virtual void SerializeSettings(nlohmann::json& out) const override;
        virtual void DeserializeSettings(const nlohmann::json& in) override;

        virtual void CollectIssues(Vector<String>& issues) override;
        virtual Scope<ExportTask> CreateTask() const override;

        AndroidExportSettings& GetSettings() { return mSettings; }

        // Letters, digits and '_', at least two dot-separated segments, each starting with a letter
        static bool IsValidPackageId(const String& id);

        // ANDROID_HOME / ANDROID_SDK_ROOT, else sdk.dir from android/local.properties. Empty if not found
        static Path FindSdk();
        static Path FindAdb(const Path& sdk); // Empty if the SDK has no platform-tools

    private:
        AndroidExportSettings mSettings;
        ExportUtils::CachedExists mGradlewExists;
        ExportUtils::CachedExists mKeystoreExists;
        ExportUtils::CachedExists mIconExists;

        Path mCachedSdk;
        Path mCachedAdb;
        double mSdkCheckTime = -100.0;
    };

} // namespace Surge
