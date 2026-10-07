// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Android/AndroidExporter.hpp"
#include "Export/Platforms/Android/AndroidToolParsers.hpp"
#include "Export/ExportSession.hpp"
#include "Export/ExportSettingsIO.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Utility/Platform.hpp"

#include <json/json.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>

namespace Surge
{
    namespace fs = std::filesystem;
    using ExportUtils::Quote;
    using ExportUtils::StartsWith;

    namespace
    {
        // Snapshot taken by CreateTask() on the main thread
        struct AndroidTaskInput
        {
            AndroidExportSettings Settings;
            Path AdbPath;
        };

        class AndroidExportTask : public ExportTask
        {
        public:
            explicit AndroidExportTask(AndroidTaskInput input)
                : mInput(std::move(input)) {}

            virtual bool Run(const ExportJobInfo& job, ExportSession& session) override
            {
                const AndroidExportSettings& a = mInput.Settings;
                const ExportBranding& b = job.Branding;
                const Path projectDir = AndroidExporter::PROJECT_DIR;
                const Path out = a.OutputPath;
                const bool isApk = a.Format == AndroidPackageFormat::APK;
                const bool install = isApk && a.InstallOnDevice;
                std::error_code ec;

                // Pre-flight
                if(!fs::exists(projectDir / "gradlew.bat"))
                {
                    session.Error("gradlew.bat not found in " + projectDir.string());
                    return false;
                }
                if(!fs::exists(a.KeystorePath))
                {
                    session.Error("Keystore not found: " + a.KeystorePath);
                    return false;
                }
                if(!a.IconPath.empty() && !fs::exists(a.IconPath))
                {
                    session.Error("Icon not found: " + a.IconPath);
                    return false;
                }
                if(install && mInput.AdbPath.empty())
                {
                    session.Error("adb not found in the Android SDK (platform-tools). Install it or disable 'Install on device'.");
                    return false;
                }

                session.SetStep("Preparing output folder", EXPORT_COOK_PROGRESS, 0.06f);
                ExportUtils::PrepareOutputFolder(session, out, a.CleanOutput);
                if(session.IsCancelled())
                    return false;

                session.SetStep("Staging content into APK assets", 0.06f, 0.14f);
                const Path assetsRoot = projectDir / "app/src/main/assets";
                fs::remove_all(assetsRoot, ec);
                Filesystem::CreateOrEnsureDirectories(assetsRoot);
                ExportUtils::StageRuntimeContent(assetsRoot, job);
                if(session.IsCancelled())
                    return false;

                if(!a.IconPath.empty())
                {
                    session.SetStep("Installing launcher icon", 0.14f, 0.15f);
                    const Path resDir = projectDir / "app/src/main/res/mipmap-xxxhdpi";
                    Filesystem::CreateOrEnsureDirectories(resDir);
                    for(const char* name : { "ic_launcher.png", "ic_launcher_round.png" })
                        fs::copy_file(a.IconPath, resDir / name, fs::copy_options::overwrite_existing);
                }

                // Delete stale artifacts so we can never copy an old build by accident
                const Path artifact = projectDir / (isApk ? "app/build/outputs/apk/release/app-release.apk" : "app/build/outputs/bundle/release/app-release.aab");
                fs::remove(artifact, ec);

                const float gradleEnd = install ? 0.88f : 0.96f;
                session.SetStep("Running Gradle", 0.15f, gradleEnd);
                session.SetDetail("Starting Gradle (first builds and engine changes take a while)");
                int exitCode = -1;
                {
                    // Secrets go through the environment, not the cmd
                    ExportUtils::ScopedEnv e0("SURGE_KEYSTORE_PATH", Path(a.KeystorePath).make_preferred().string());
                    ExportUtils::ScopedEnv e1("SURGE_KEYSTORE_PASS", a.KeystorePass);
                    ExportUtils::ScopedEnv e2("SURGE_KEY_ALIAS", a.KeyAlias);
                    ExportUtils::ScopedEnv e3("SURGE_KEY_PASS", a.KeyPass.empty() ? a.KeystorePass : a.KeyPass);

                    GradleOutputParser parser(a.VerboseLog);
                    exitCode = session.RunTool(BuildGradleCommand(job, isApk), projectDir, &parser);
                }

                if(session.IsCancelled())
                    return false;

                if(exitCode != 0)
                {
                    session.Error("Gradle failed with exit code " + std::to_string(exitCode));
                    return false;
                }

                if(!fs::exists(artifact))
                {
                    session.Error("Gradle succeeded but the artifact was not found at " + artifact.string());
                    return false;
                }

                session.SetStep("Copying artifact", gradleEnd, gradleEnd + 0.03f);
                const Path dest = out / (ExportUtils::SanitizeFileName(b.AppName) + (isApk ? ".apk" : ".aab"));
                fs::copy_file(artifact, dest, fs::copy_options::overwrite_existing);
                session.SetResult(dest);

                if(install)
                    InstallOnDevice(session, dest, gradleEnd + 0.03f);

                return true;
            }

        private:
            String BuildGradleCommand(const ExportJobInfo& job, bool isApk) const
            {
                const AndroidExportSettings& a = mInput.Settings;
                const ExportBranding& b = job.Branding;
                const Path gradlew = fs::absolute(Path(AndroidExporter::PROJECT_DIR) / "gradlew.bat").make_preferred();
                const int orientation = std::clamp(a.Orientation, 0, AndroidExporter::ORIENTATION_COUNT - 1);

                String cmd = Quote(gradlew.string()) + " --console=plain";
                cmd += " " + Quote("-PsurgeAppName=" + b.AppName);
                cmd += " " + Quote("-PsurgeApplicationId=" + a.PackageId);
                cmd += " " + Quote("-PsurgeVersionName=" + ExportUtils::VersionString(b, false));
                cmd += " -PsurgeVersionCode=" + std::to_string(std::max(1, b.Build));
                cmd += " -PsurgeMinSdk=" + std::to_string(a.MinApi);
                cmd += String(" -PsurgeOrientation=") + AndroidExporter::ORIENTATIONS[orientation].Value;
                cmd += String(" -PsurgeImmersive=") + (a.Immersive ? "true" : "false");
                cmd += String(" -PsurgeCutout=") + (a.UseCutout ? "true" : "false");
                cmd += String(" -PsurgeKeepScreenOn=") + (a.KeepScreenOn ? "true" : "false");
                cmd += String(" -PsurgeExitOnBack=") + (a.ExitOnBack ? "true" : "false");
                cmd += " -Pandroid.native.buildOutput=build_stdout"; // Ninja's [n/N] lines drive the progress bar
                cmd += isApk ? " assembleRelease" : " bundleRelease";
                return cmd;
            }

            // Failures here don't fail the export, the artifact is already built
            void InstallOnDevice(ExportSession& session, const Path& apk, float begin) const
            {
                const String adb = Quote(mInput.AdbPath.string());
                AdbOutputParser parser;

                session.SetStep("Installing on device", begin, 0.99f);
                session.SetDetail("adb install " + apk.filename().string());
                if(session.RunTool(adb + " install -r " + Quote(apk.string()), Path(), &parser) != 0)
                {
                    if(!session.IsCancelled())
                        session.SetResultWarning("Built, but installing on the device failed - is it connected with USB debugging enabled?");
                    return;
                }

                if(!mInput.Settings.LaunchAfterInstall || session.IsCancelled())
                    return;

                session.SetStep("Launching on device", 0.99f, 1.0f);
                const String launch = adb + " shell monkey -p " + mInput.Settings.PackageId + " -c android.intent.category.LAUNCHER 1";
                if(session.RunTool(launch, Path(), &parser) != 0 && !session.IsCancelled())
                    session.SetResultWarning("Installed, but launching the app on the device failed.");
            }

        private:
            AndroidTaskInput mInput;
        };
    } // namespace

    void AndroidExporter::SerializeSettings(nlohmann::json& out) const
    {
        const AndroidExportSettings& a = mSettings;
        out = {
            {"OutputPath", a.OutputPath},
            {"IconPath", a.IconPath},
            {"PackageId", a.PackageId},
            {"KeystorePath", a.KeystorePath},
            {"KeyAlias", a.KeyAlias},
            {"Format", static_cast<int>(a.Format)},
            {"MinApi", a.MinApi},
            {"Orientation", a.Orientation},
            {"Immersive", a.Immersive},
            {"UseCutout", a.UseCutout},
            {"KeepScreenOn", a.KeepScreenOn},
            {"ExitOnBack", a.ExitOnBack},
            {"CleanOutput", a.CleanOutput},
            {"InstallOnDevice", a.InstallOnDevice},
            {"LaunchAfterInstall", a.LaunchAfterInstall},
            {"VerboseLog", a.VerboseLog},
        };
    }

    void AndroidExporter::DeserializeSettings(const nlohmann::json& in)
    {
        AndroidExportSettings& a = mSettings;
        int format = static_cast<int>(a.Format);

        ExportSettingsIO::Read(in, "OutputPath", a.OutputPath);
        ExportSettingsIO::Read(in, "IconPath", a.IconPath);
        ExportSettingsIO::Read(in, "PackageId", a.PackageId);
        ExportSettingsIO::Read(in, "KeystorePath", a.KeystorePath);
        ExportSettingsIO::Read(in, "KeyAlias", a.KeyAlias);
        ExportSettingsIO::Read(in, "Format", format);
        ExportSettingsIO::Read(in, "MinApi", a.MinApi);
        ExportSettingsIO::Read(in, "Orientation", a.Orientation);
        ExportSettingsIO::Read(in, "Immersive", a.Immersive);
        ExportSettingsIO::Read(in, "UseCutout", a.UseCutout);
        ExportSettingsIO::Read(in, "KeepScreenOn", a.KeepScreenOn);
        ExportSettingsIO::Read(in, "ExitOnBack", a.ExitOnBack);
        ExportSettingsIO::Read(in, "CleanOutput", a.CleanOutput);
        ExportSettingsIO::Read(in, "InstallOnDevice", a.InstallOnDevice);
        ExportSettingsIO::Read(in, "LaunchAfterInstall", a.LaunchAfterInstall);
        ExportSettingsIO::Read(in, "VerboseLog", a.VerboseLog);

        a.Format = format == 1 ? AndroidPackageFormat::AAB : AndroidPackageFormat::APK;
        a.Orientation = std::clamp(a.Orientation, 0, ORIENTATION_COUNT - 1);
    }

    void AndroidExporter::CollectIssues(Vector<String>& issues)
    {
        const AndroidExportSettings& a = mSettings;

        if(a.OutputPath.empty())
            issues.push_back("Choose a build destination");
        if(!IsValidPackageId(a.PackageId))
            issues.push_back("Enter a valid package ID");
        if(a.KeystorePath.empty())
            issues.push_back("Choose a signing keystore");
        else if(!mKeystoreExists.Check(a.KeystorePath))
            issues.push_back("Keystore file not found");
        if(a.KeyAlias.empty())
            issues.push_back("Enter the key alias");
        if(a.KeystorePass.empty())
            issues.push_back("Enter the keystore password");
        if(!a.IconPath.empty() && !mIconExists.Check(a.IconPath))
            issues.push_back("Icon file not found");
        if(!mGradlewExists.Check(String(PROJECT_DIR) + "/gradlew.bat"))
            issues.push_back(std::format("Gradle wrapper not found ({}/gradlew.bat)", PROJECT_DIR));

        // The SDK lookup reads a file, so it is refreshed every 2 seconds
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        if(now - mSdkCheckTime > 2.0)
        {
            mCachedSdk = FindSdk();
            mCachedAdb = FindAdb(mCachedSdk);
            mSdkCheckTime = now;
        }

        if(mCachedSdk.empty())
            issues.push_back(std::format("Android SDK not found - set ANDROID_HOME or sdk.dir in {}/local.properties", PROJECT_DIR));
        else if(a.InstallOnDevice && a.Format == AndroidPackageFormat::APK && mCachedAdb.empty())
            issues.push_back("adb not found in the Android SDK (platform-tools) - needed for 'Install on device'");
    }

    Scope<ExportTask> AndroidExporter::CreateTask() const
    {
        // Run on main thread
        AndroidTaskInput input;
        input.Settings = mSettings;
        input.AdbPath = FindAdb(FindSdk());
        return CreateScope<AndroidExportTask>(std::move(input));
    }

    bool AndroidExporter::IsValidPackageId(const String& id)
    {
        int segments = 0;
        size_t i = 0;
        while(i <= id.size())
        {
            size_t dot = id.find('.', i);
            if(dot == String::npos)
                dot = id.size();
            if(dot == i)
                return false; // empty segment
            if(!std::isalpha(static_cast<unsigned char>(id[i])))
                return false;
            for(size_t k = i; k < dot; ++k)
            {
                const unsigned char c = static_cast<unsigned char>(id[k]);
                if(!std::isalnum(c) && c != '_')
                    return false;
            }
            ++segments;
            if(dot == id.size())
                break;
            i = dot + 1;
        }
        return segments >= 2;
    }

    Path AndroidExporter::FindSdk()
    {
        for(const char* var : { "ANDROID_HOME", "ANDROID_SDK_ROOT" })
        {
            String value = Platform::GetEnvVariableForCurrentProcess(var);
            if(value.empty() && Platform::HasEnvVariable(var))
                value = Platform::GetEnvVariable(var);
            if(!value.empty())
                return value;
        }

        // Java properties escaping: sdk.dir=C\:\\Android\\Sdk
        std::ifstream file(Path(PROJECT_DIR) / "local.properties");
        String line;
        while(std::getline(file, line))
        {
            if(!StartsWith(line, "sdk.dir"))
                continue;
            const size_t eq = line.find('=');
            if(eq == String::npos)
                continue;

            String value;
            for(size_t i = eq + 1; i < line.size(); ++i)
            {
                if(line[i] == '\\' && i + 1 < line.size())
                    ++i;
                value += line[i];
            }
            while(!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
                value.pop_back();
            while(!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
                value.erase(0, 1);
            if(!value.empty())
                return value;
        }
        return {};
    }

    Path AndroidExporter::FindAdb(const Path& sdk)
    {
        if(sdk.empty())
            return {};
        std::error_code ec;
        const Path adb = sdk / "platform-tools" / "adb.exe";
        return fs::exists(adb, ec) ? adb : Path();
    }

} // namespace Surge
