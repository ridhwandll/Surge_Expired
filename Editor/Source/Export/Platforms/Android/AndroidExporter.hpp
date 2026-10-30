// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportUtils.hpp"
#include "Export/PlatformExporter.hpp"
#include "Export/Platforms/Android/AndroidDevice.hpp"
#include "Export/Platforms/Android/AndroidLogcat.hpp"
#include "Export/Platforms/Android/AndroidToolchain.hpp"
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
        int TargetApi = 36; // Google Play rejects new apps/updates below its yearly minimum (API 36 since Aug 2026)
        int Orientation = 0;    // index into AndroidExporter::ORIENTATIONS
        bool Immersive = true;  // hide system bars (swipe shows them transiently)
        bool UseCutout = true;  // draw under the notch / camera cut-out
        bool KeepScreenOn = true;
        bool ExitOnBack = true;
        bool InstallOnDevice = false;   // APK only, through adb
        bool LaunchAfterInstall = true;
        bool AttachLogcat = true;       // Stream the app's log into the Console after launching it
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

    // Stages the content into the Gradle project's assets and drives Android/gradlew.bat (see Android/app/build.gradle for the -P contract)
    class AndroidExporter : public PlatformExporter
    {
    public:
        static constexpr const char* PROJECT_DIR = "Android";

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

        static constexpr AndroidApiLevel TARGET_API_LEVELS[] = {
            {35, "API 35 (Android 15)"}, {36, "API 36 (Android 16) - Google Play minimum"},
        };

        static ExportPlatform GetStaticPlatform() { return ExportPlatform::ANDROID; }

        virtual ExportPlatform GetPlatform() const override { return GetStaticPlatform(); }
        virtual const char* GetDisplayName() const override { return "Android"; }

        virtual ExportPlatformSettings& GetBaseSettings() override { return mSettings; }
        virtual void ResetSettings() override { mSettings = {}; }
        virtual void SerializeSettings(nlohmann::json& out) const override;
        virtual void DeserializeSettings(const nlohmann::json& in) override;

        virtual void CollectIssues(Vector<String>& issues) override;
        virtual Scope<ExportTask> CreateTask() override;
        virtual void Update() override;
        virtual void OnRunFinished(const ExportStatus& status) override;
        virtual void Shutdown() override;

        AndroidExportSettings& GetSettings() { return mSettings; }

        // Letters, digits and '_', at least two dot-separated segments, each starting with a letter
        static bool IsValidPackageId(const String& id);

        // JDK, SDK, NDK, ... the build uses. Re-detected every 2 seconds (it reads files), right away when the target API changes
        const AndroidToolchain& GetToolchain();

        // Retry after 'Install on device' failed: restarts adb and installs the APK of the last export again
        void ReattachAndInstall(const Path& apk);
        AndroidDeviceInstaller& GetDeviceInstaller() { return mDeviceInstaller; }

        // Device log of packageId in the Console (AndroidLogcat::LOG_TAG). Restarts when already attached
        void AttachLogcat(const String& packageId);
        void DetachLogcat() { mLogcat.Stop(); }
        AndroidLogcat& GetLogcat() { return mLogcat; }

    private:
        AndroidExportSettings mSettings;
        ExportUtils::CachedExists mKeystoreExists;
        ExportUtils::CachedExists mIconExists;

        AndroidToolchain mToolchain;
        double mToolchainTime = -100.0;
        int mToolchainTargetApi = 0;

        AndroidDeviceInstaller mDeviceInstaller;
        String mLastPackageId; // Of the last export, the retry launches what was installed
        bool mLastInstall = false;
        bool mLastLaunch = true;
        ExportState mInstallerState = ExportState::IDLE;

        AndroidLogcat mLogcat;
    };

} // namespace Surge
