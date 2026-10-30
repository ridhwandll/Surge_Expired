// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Android/AndroidToolchain.hpp"
#include "Export/Platforms/Android/AndroidExporter.hpp"
#include "Surge/Utility/Platform.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <format>
#include <fstream>
#include <initializer_list>
#include <sstream>

namespace Surge
{
    namespace fs = std::filesystem;

    namespace
    {
        constexpr int MIN_JDK_VERSION = 17; // Android Gradle Plugin 8.x
        constexpr const char* SETUP_HINT = "run Scripts/AndroidSetup.bat";

        bool Exists(const Path& path)
        {
            std::error_code ec;
            return !path.empty() && fs::exists(path, ec);
        }

        String ReadFile(const Path& path)
        {
            std::ifstream file(path, std::ios::binary);
            std::ostringstream text;
            if(file)
                text << file.rdbuf();
            return text.str();
        }

        // The quoted value on the same line as key: ndkVersion = "27.3.13750724" -> 27.3.13750724
        String QuotedAfter(const String& text, const char* key)
        {
            const size_t at = text.find(key);
            if(at == String::npos)
                return {};
            const size_t lineEnd = std::min(text.find('\n', at), text.size());
            const size_t open = text.find('"', at);
            if(open == String::npos || open > lineEnd)
                return {};
            const size_t close = text.find('"', open + 1);
            if(close == String::npos || close > lineEnd)
                return {};
            return text.substr(open + 1, close - open - 1);
        }

        // Process environment first (what Gradle inherits), then the user's registry environment (set after the Editor started)
        String FindEnv(std::initializer_list<const char*> names, String& outName, bool& outFromRegistry)
        {
            outFromRegistry = false;
            for(const char* name : names)
            {
                String value = Platform::GetEnvVariableForCurrentProcess(name);
                if(!value.empty())
                {
                    outName = name;
                    return value;
                }
            }
            for(const char* name : names)
            {
                if(!Platform::HasEnvVariable(name))
                    continue;
                String value = Platform::GetEnvVariable(name);
                if(!value.empty())
                {
                    outName = name;
                    outFromRegistry = true;
                    return value;
                }
            }
            return {};
        }

        // Java properties escaping: sdk.dir=C\:\\Android\\Sdk
        String ReadSdkDir(const Path& localProperties)
        {
            std::ifstream file(localProperties);
            String line;
            while(std::getline(file, line))
            {
                if(line.rfind("sdk.dir", 0) != 0)
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

        // gradlew.bat falls back to java.exe on PATH when JAVA_HOME is not set
        Path FindJavaOnPath()
        {
            const String path = Platform::GetEnvVariableForCurrentProcess("PATH");
            size_t start = 0;
            while(start < path.size())
            {
                const size_t end = std::min(path.find(';', start), path.size());
                if(end > start)
                {
                    const Path java = Path(path.substr(start, end - start)) / "java.exe";
                    if(Exists(java))
                        return java;
                }
                start = end + 1;
            }
            return {};
        }

        // JAVA_VERSION="17.0.20.1" (or "1.8.0_x") in <jdk>/release. 0 if unknown
        int ReadJdkMajorVersion(const Path& jdk)
        {
            const String version = QuotedAfter(ReadFile(jdk / "release"), "JAVA_VERSION=");
            const int first = std::atoi(version.c_str());
            if(first != 1)
                return first;
            const size_t dot = version.find('.');
            return dot == String::npos ? first : std::atoi(version.c_str() + dot + 1);
        }

        void DetectJdk(AndroidToolchain& t)
        {
            AndroidToolItem& item = t.Items.emplace_back();
            item.Name = "JDK";

            String varName;
            bool fromRegistry = false;
            const String javaHome = FindEnv({ "JAVA_HOME" }, varName, fromRegistry);

            Path jdk;
            if(!javaHome.empty())
            {
                item.Location = javaHome;
                if(!Exists(Path(javaHome) / "bin" / "java.exe"))
                {
                    item.State = AndroidToolState::BLOCKING;
                    item.Note = std::format("JAVA_HOME has no bin\\java.exe: fix it or {}", SETUP_HINT);
                    t.Issues.push_back("JAVA_HOME points to a folder without bin\\java.exe");
                    return;
                }
                jdk = javaHome;
                item.Note = fromRegistry ? "JAVA_HOME (set after the Editor started, passed to Gradle)" : "JAVA_HOME";
                if(fromRegistry)
                    t.JdkEnvToApply = varName;
            }
            else
            {
                const Path java = FindJavaOnPath();
                if(java.empty())
                {
                    item.State = AndroidToolState::BLOCKING;
                    item.Note = std::format("Set JAVA_HOME to a JDK {}+ or {}", MIN_JDK_VERSION, SETUP_HINT);
                    t.Issues.push_back("JDK not found: set JAVA_HOME or run Scripts/AndroidSetup.bat");
                    return;
                }
                jdk = java.parent_path().parent_path(); // <jdk>/bin/java.exe
                item.Location = java.string();
                item.Note = "java.exe on PATH (JAVA_HOME is not set)";
            }

            t.Jdk = jdk;
            const int major = ReadJdkMajorVersion(jdk);
            if(major == 0)
            {
                item.Note += ", version unknown";
                return;
            }

            item.Name = std::format("JDK {}", major);
            if(major < MIN_JDK_VERSION)
            {
                item.State = AndroidToolState::BLOCKING;
                item.Note = std::format("Gradle needs JDK {} or newer: {}", MIN_JDK_VERSION, SETUP_HINT);
                t.Issues.push_back(std::format("JDK {} is too old, the Android build needs JDK {}+", major, MIN_JDK_VERSION));
            }
        }

        void DetectSdk(AndroidToolchain& t, const Path& projectDir)
        {
            AndroidToolItem& item = t.Items.emplace_back();
            item.Name = "Android SDK";

            // Same order as the Android Gradle Plugin: local.properties wins over the environment
            String sdk = ReadSdkDir(projectDir / "local.properties");
            String source = "sdk.dir in Android/local.properties";
            if(sdk.empty())
            {
                String varName;
                bool fromRegistry = false;
                sdk = FindEnv({ "ANDROID_HOME", "ANDROID_SDK_ROOT" }, varName, fromRegistry);
                source = fromRegistry ? varName + " (set after the Editor started, passed to Gradle)" : varName;
                if(fromRegistry)
                    t.SdkEnvToApply = varName;
            }

            if(sdk.empty())
            {
                item.State = AndroidToolState::BLOCKING;
                item.Note = std::format("Set ANDROID_HOME or {}", SETUP_HINT);
                t.Issues.push_back("Android SDK not found: set ANDROID_HOME or run Scripts/AndroidSetup.bat");
                t.SdkEnvToApply.clear();
                return;
            }

            item.Location = sdk;
            if(!Exists(sdk))
            {
                item.State = AndroidToolState::BLOCKING;
                item.Note = std::format("{} points to a missing folder: {}", source, SETUP_HINT);
                t.Issues.push_back("The Android SDK folder does not exist (" + sdk + ")");
                t.SdkEnvToApply.clear();
                return;
            }

            t.Sdk = sdk;
            item.Note = source;
        }

        void DetectSdkComponents(AndroidToolchain& t, const Path& projectDir, int targetApi)
        {
            const String gradle = ReadFile(projectDir / "app" / "build.gradle");
            const bool licensesAccepted = Exists(t.Sdk / "licenses" / "android-sdk-license");

            // AGP downloads a missing platform / NDK / CMake on the first build, but only with the SDK licenses accepted
            auto addComponent = [&](const String& name, const Path& location, const Path& probe)
            {
                AndroidToolItem& item = t.Items.emplace_back();
                item.Name = name;
                item.Location = location.string();
                if(Exists(probe))
                    return;

                if(licensesAccepted)
                {
                    item.State = AndroidToolState::WARN;
                    item.Note = "Not installed: Gradle downloads it on the first build";
                }
                else
                {
                    item.State = AndroidToolState::BLOCKING;
                    item.Note = std::format("Not installed, and the SDK licenses are not accepted: {}", SETUP_HINT);
                    t.Issues.push_back(name + " is not installed: run Scripts/AndroidSetup.bat");
                }
            };

            // compileSdk Math.max(36, surgeTargetSdk)
            int compileSdk = targetApi;
            if(const size_t at = gradle.find("compileSdk Math.max("); at != String::npos)
                compileSdk = std::max(compileSdk, std::atoi(gradle.c_str() + at + std::char_traits<char>::length("compileSdk Math.max(")));
            const String platform = std::format("android-{}", compileSdk);
            addComponent("SDK Platform " + std::to_string(compileSdk), t.Sdk / "platforms" / platform, t.Sdk / "platforms" / platform / "android.jar");

            if(const String ndk = QuotedAfter(gradle, "ndkVersion"); !ndk.empty())
            {
                addComponent("NDK " + ndk, t.Sdk / "ndk" / ndk, t.Sdk / "ndk" / ndk / "source.properties");
                if(t.Items.back().State == AndroidToolState::OK)
                    t.Ndk = t.Sdk / "ndk" / ndk;
            }

            // externalNativeBuild.cmake.version (ndkVersion is "Version" with a capital V, versionCode/Name have no " = ")
            if(const String cmake = QuotedAfter(gradle, "version = "); !cmake.empty())
                addComponent("CMake " + cmake, t.Sdk / "cmake" / cmake, t.Sdk / "cmake" / cmake / "bin" / "cmake.exe");

            AndroidToolItem& adb = t.Items.emplace_back();
            adb.Name = "Platform tools (adb)";
            const Path adbPath = t.Sdk / "platform-tools" / "adb.exe";
            adb.Location = adbPath.string();
            if(Exists(adbPath))
                t.Adb = adbPath;
            else
            {
                adb.State = AndroidToolState::WARN;
                adb.Note = std::format("Not installed, only 'Install on device' needs it: {}", SETUP_HINT);
            }
        }

        void DetectGradle(AndroidToolchain& t, const Path& projectDir)
        {
            AndroidToolItem& project = t.Items.emplace_back();
            project.Name = "Gradle project";
            project.Location = (projectDir / "gradlew.bat").string();
            if(!Exists(projectDir / "gradlew.bat"))
            {
                project.State = AndroidToolState::BLOCKING;
                project.Note = "gradlew.bat is missing, restore the Android/ folder from the repository";
                t.Issues.push_back(std::format("Gradle wrapper not found ({}/gradlew.bat)", AndroidExporter::PROJECT_DIR));
                return;
            }

            // distributionUrl=https\://services.gradle.org/distributions/gradle-8.9-bin.zip -> <gradle home>/wrapper/dists/gradle-8.9-bin
            const String properties = ReadFile(projectDir / "gradle" / "wrapper" / "gradle-wrapper.properties");
            const size_t urlAt = properties.find("distributionUrl");
            if(urlAt == String::npos)
                return;
            const size_t lineEnd = std::min(properties.find_first_of("\r\n", urlAt), properties.size());
            const String url = properties.substr(urlAt, lineEnd - urlAt);
            String dist = url.substr(url.find_last_of('/') + 1);
            if(dist.size() > 4 && dist.ends_with(".zip"))
                dist.resize(dist.size() - 4);

            String version = dist;
            if(version.starts_with("gradle-"))
                version.erase(0, 7);
            if(const size_t dash = version.rfind('-'); dash != String::npos)
                version.resize(dash);

            String gradleHome = Platform::GetEnvVariableForCurrentProcess("GRADLE_USER_HOME");
            if(gradleHome.empty())
                gradleHome = (Path(Platform::GetEnvVariableForCurrentProcess("USERPROFILE")) / ".gradle").string();

            AndroidToolItem& wrapper = t.Items.emplace_back();
            wrapper.Name = "Gradle " + version;
            wrapper.Location = (Path(gradleHome) / "wrapper" / "dists" / dist).string();
            if(!Exists(wrapper.Location))
            {
                wrapper.State = AndroidToolState::WARN;
                wrapper.Note = "Not downloaded yet: gradlew downloads it on the first build";
            }
        }

    } // namespace

    AndroidToolchain AndroidToolchain::Detect(int targetApi)
    {
        AndroidToolchain t;
        std::error_code ec;
        const Path projectDir = fs::absolute(AndroidExporter::PROJECT_DIR, ec);

        DetectJdk(t);
        DetectSdk(t, projectDir);
        if(!t.Sdk.empty())
            DetectSdkComponents(t, projectDir, targetApi);
        DetectGradle(t, projectDir);
        return t;
    }

    void AndroidToolchain::ApplyToProcessEnvironment() const
    {
        if(!JdkEnvToApply.empty())
            Platform::SetEnvVariableForCurrentProcess(JdkEnvToApply, Jdk.string());
        if(!SdkEnvToApply.empty())
            Platform::SetEnvVariableForCurrentProcess(SdkEnvToApply, Sdk.string());
    }

} // namespace Surge
