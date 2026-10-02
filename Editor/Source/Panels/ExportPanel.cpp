// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ExportPanel.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Serializer/Serializer.hpp"

#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include "Surge/Core/Process.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <thread>
#include "Surge/Utility/Platform.hpp"

#undef CopyDirectory // Windows.h

// NOTE: ExportPanel.hpp is unchanged. Only Init/OnEvent/Render/Shutdown/BuildWindows/BuildAndroid
// are members; everything else lives in the anonymous namespace below.

namespace Surge
{
    namespace
    {
        namespace fs = std::filesystem;

        constexpr const char* kRceditPath = "Editor/Tools/rcedit-x64.exe";
        constexpr const char* kWindowsPlayerDir = "build/Player/Release";
        constexpr const char* kAndroidProjectDir = "android";

        enum class ExportPlatform : int { WINDOWS = 0, ANDROID = 1 };
        enum class ExportAndroidFormat : int { APK = 0, AAB = 1 };
        enum class ExportState : int { IDLE = 0, RUNNING, SUCCEEDED, FAILED };
        enum class ExportLogLevel : int { INFO = 0, WARN, ERROR };

        // Value is the android:screenOrientation string, injected into the manifest through a Gradle placeholder
        struct ExportOrientation
        {
            const char* Name;
            const char* Value;
        };
        constexpr ExportOrientation kOrientations[] = {
            {"Landscape (locked)", "landscape"},
            {"Landscape (flips with sensor)", "sensorLandscape"},
            {"Portrait (locked)", "portrait"},
            {"Portrait (flips with sensor)", "sensorPortrait"},
            {"Auto-rotate (all)", "fullSensor"},
        };

        struct ExportBranding
        {
            String AppName = "Game";
            String Company = "RidT";
            String Description = "A game";
            String Copyright = "Copyright (c) 2026 RidT. All rights reserved.";
            int Major = 1;
            int Minor = 0;
            int Patch = 0;
            int Build = 1; // Windows: 4th file-version part | Android: versionCode
        };

        struct ExportWindowsSettings
        {
            String OutputPath;
            String IconPath; // .ico
            bool CleanOutput = false;
            bool StampExecutable = true;
        };

        struct ExportAndroidSettings
        {
            String OutputPath;
            String IconPath; // .png (192x192 or larger)
            String PackageId = "com.ridt.game";
            String KeystorePath;
            String KeyAlias;
            String KeystorePass; // memory only - never serialized, passed to Gradle through the environment
            String KeyPass;      // empty = same as keystore password
            ExportAndroidFormat Format = ExportAndroidFormat::APK;
            int MinApi = 30;
            int Orientation = 0;       // index into kOrientations
            bool Immersive = true;     // hide system bars (swipe shows them transiently)
            bool UseCutout = true;     // draw under the notch / camera cut-out
            bool KeepScreenOn = true;
            bool ExitOnBack = true;
            bool CleanOutput = false;
        };

        // Everything the worker thread needs. Snapshotted on the main thread so the UI can't race it.
        struct ExportJob
        {
            ExportPlatform Target = ExportPlatform::WINDOWS;
            ExportBranding Branding;
            ExportWindowsSettings Windows;
            ExportAndroidSettings Android;
            Project Proj; // StartScene already overridden
            Path ProjectDir;
        };

        struct ExportLogLine
        {
            ExportLogLevel Level;
            String Text;
        };

        // State
        int sTab = 0;
        AssetID sStartScene = AssetID::INVALID;
        ExportBranding sBranding;
        ExportWindowsSettings sWin;
        ExportAndroidSettings sAnd;

        std::thread sWorker;
        std::atomic<ExportState> sState { ExportState::IDLE };
        std::atomic<float> sProgress { 0.0f };
        std::mutex sLogMutex;
        Vector<ExportLogLine> sLog;
        String sStepName;
        String sLastOutputPath;

        ExportPlatform sPendingTab = ExportPlatform::WINDOWS;

        // Logging / progress (thread-safe)
        void ExportLog(ExportLogLevel level, const String& text)
        {
            std::scoped_lock lock(sLogMutex);
            sLog.push_back({ level, text });
        }

        void ClearLog()
        {
            std::scoped_lock lock(sLogMutex);
            sLog.clear();
            sStepName.clear();
            sProgress = 0.0f;
        }

        void SetStep(const String& name, float progress)
        {
            {
                std::scoped_lock lock(sLogMutex);
                sStepName = name;
            }
            sProgress = progress;
            ExportLog(ExportLogLevel::INFO, "> " + name);
        }

        // Helpers
        String Quote(const String& s)
        {
            String r = "\"";
            for(char c : s)
                r += (c == '"') ? '\'' : c;
            r += "\"";
            return r;
        }

        String SanitizeFileName(const String& name)
        {
            String r;
            for(char c : name)
                r += (std::strchr("<>:\"/\\|?*", c) != nullptr || static_cast<unsigned char>(c) < 32) ? '_' : c;
            while(!r.empty() && (r.back() == ' ' || r.back() == '.'))
                r.pop_back();
            return r.empty() ? String("Game") : r;
        }

        // Sets an environment variable for child processes, removes it again on scope exit.
        struct ScopedEnv
        {
            String Name;
            ScopedEnv(const String& name, const String& value) : Name(name) { Platform::SetEnvVariableForCurrentProcess(name, value); }
            ~ScopedEnv() { Platform::SetEnvVariableForCurrentProcess(Name, ""); }
            ScopedEnv(const ScopedEnv&) = delete;
            ScopedEnv& operator=(const ScopedEnv&) = delete;
        };

        String VersionString(const ExportBranding& b, bool withBuild)
        {
            String v = std::to_string(b.Major) + "." + std::to_string(b.Minor) + "." + std::to_string(b.Patch);
            if(withBuild)
                v += "." + std::to_string(b.Build);
            return v;
        }

        bool IsValidPackageId(const String& id)
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

        ExportLogLevel ClassifyLine(const String& line)
        {
            String l = line;
            std::transform(l.begin(), l.end(), l.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if(l.find("error") != String::npos || l.find("failed") != String::npos || l.rfind("e: ", 0) == 0)
                return ExportLogLevel::ERROR;
            if(l.find("warning") != String::npos || l.rfind("w: ", 0) == 0)
                return ExportLogLevel::WARN;
            return ExportLogLevel::INFO;
        }

        // Runs a shell command, tails its output into the build log while it runs, returns the exit code.
        int RunLogged(const String& command, const Path& workDir)
        {
            const Path logFile = fs::temp_directory_path() / "surge_export_cmd.log";
            std::error_code ec;
            fs::remove(logFile, ec);

            // cmd /C strips the outer quote pair when the string has more than two quotes
            String full = "\"";
            if(!workDir.empty())
                full += "cd /d " + Quote(workDir.string()) + " && ";
            full += command + " > " + Quote(logFile.string()) + " 2>&1\"";

            std::atomic<bool> done { false };
            int exitCode = -1;
            std::thread runner([&]() {
                exitCode = std::system(full.c_str());
                done = true;
                               });

            String pending;
            std::streamoff offset = 0;
            auto pump = [&](bool flush) {
                std::ifstream in(logFile, std::ios::binary);
                if(in)
                {
                    in.seekg(offset);
                    String chunk((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    offset += static_cast<std::streamoff>(chunk.size());
                    pending += chunk;
                }

                size_t nl;
                while((nl = pending.find('\n')) != String::npos)
                {
                    String line = pending.substr(0, nl);
                    if(!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if(!line.empty())
                        ExportLog(ClassifyLine(line), line);
                    pending.erase(0, nl + 1);
                }
                if(flush && !pending.empty())
                {
                    ExportLog(ClassifyLine(pending), pending);
                    pending.clear();
                }
                };

            while(!done)
            {
                pump(false);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
            runner.join();
            pump(true);
            return exitCode;
        }

        // Output folder safety
        bool PathContains(const Path& outer, const Path& inner) // true if inner == outer or inside it
        {
            std::error_code ec;
            const Path rel = fs::relative(inner, outer, ec);
            return !ec && !rel.empty() && *rel.begin() != Path("..");
        }

        Path GetProjectDir()
        {
            return std::filesystem::path(Core::GetAssetManager()->GetAssetsDirectory()).parent_path();
        }

        bool ValidateOutputFolder(const String& rawPath, bool clean, String& reason)
        {
            std::error_code ec;
            const Path out = fs::weakly_canonical(Path(rawPath), ec);
            if(ec || out.empty())
            {
                reason = "Output folder could not be resolved.";
                return false;
            }

            const Path proj = fs::weakly_canonical(GetProjectDir(), ec);
            if(PathContains(proj, out))
            {
                reason = "Output folder must not be inside the project folder.";
                return false;
            }

            if(clean)
            {
                if(out == out.root_path())
                {
                    reason = "Refusing to clean a drive root.";
                    return false;
                }

                const Path rel = out.relative_path();
                if(std::distance(rel.begin(), rel.end()) < 2)
                {
                    reason = "Refusing to clean a top-level folder. Pick a dedicated subfolder (e.g. D:/Dev/Builds/MyGame).";
                    return false;
                }

                if(PathContains(out, proj))
                {
                    reason = "Cleaning would delete the project folder.";
                    return false;
                }

                if(PathContains(out, fs::weakly_canonical(fs::current_path(ec), ec)))
                {
                    reason = "Cleaning would delete the editor's working directory.";
                    return false;
                }
            }
            return true;
        }

        void CleanDirectory(const Path& dir)
        {
            std::error_code ec;
            if(!fs::exists(dir, ec))
                return;

            Vector<Path> entries;
            for(const auto& entry : fs::directory_iterator(dir, ec))
                entries.push_back(entry.path());

            for(const Path& p : entries)
            {
                std::error_code rec;
                fs::remove_all(p, rec);
                if(rec)
                    ExportLog(ExportLogLevel::WARN, "Could not remove " + p.string() + ": " + rec.message());
            }
            ExportLog(ExportLogLevel::INFO, "Cleaned " + dir.string());
        }

        void PrepareOutputFolder(const Path& out, bool clean)
        {
            if(clean)
                CleanDirectory(out);
            Filesystem::CreateOrEnsureDirectories(out);
        }

        // Runtime content staging
        void StageRuntimeContent(const Path& root, const ExportJob& job)
        {
            const Path fontsPath = root / "Engine/Assets/Fonts";
            Filesystem::CreateOrEnsureDirectories(fontsPath);
            Filesystem::CopyDirectory("Engine/Assets/Fonts", fontsPath);

            const Path shadersPath = root / "Engine/Assets/Shaders";
            Filesystem::CreateOrEnsureDirectories(shadersPath);
            Filesystem::CopyDirectory("Engine/Assets/Shaders", shadersPath);

            // Cooked project content
            Project project = job.Proj;
            const Path projectFilePath = root / "Engine" / (project.Name + ".surgeproj");

            // COPY THE WHOLE ASSETS FOLDER
            Filesystem::CopyDirectory(job.ProjectDir / "Assets", root / "Engine/Assets");
            fs::remove_all(root / "Engine/Assets/Meshes");
            fs::remove_all(root / "Engine/Assets/Scripts");
            fs::remove_all(root / "Engine/Assets/Audio");
            fs::remove_all(root / "Engine/Assets/Fonts");
            fs::remove_all(root / "Engine/Assets/Materials");

            Filesystem::CopyFile(job.ProjectDir / (project.Name + ".surgeproj"), root / "Engine" / (project.Name + ".surgeproj"));

            // Rewrite the .surgeproj so it points at the chosen start scene
            Serializer::SerializeProject(projectFilePath, &project);
        }

        // Windows
        bool StampWindowsExecutable(const ExportJob& job, const Path& exe)
        {
            const ExportBranding& b = job.Branding;
            const ExportWindowsSettings& w = job.Windows;

            if(!fs::exists(kRceditPath))
            {
                ExportLog(ExportLogLevel::ERROR, String("rcedit not found at ") + kRceditPath);
                return false;
            }

            String args = Quote(Path(exe).make_preferred().string());

            if(!w.IconPath.empty())
                args += " --set-icon " + Quote(w.IconPath);

            if(w.StampExecutable)
            {
                auto addString = [&](const char* key, const String& value) {
                    if(!value.empty())
                        args += String(" --set-version-string ") + key + " " + Quote(value);
                    };

                addString("ProductName", b.AppName);
                addString("CompanyName", b.Company);
                addString("FileDescription", b.Description.empty() ? b.AppName : b.Description);
                addString("LegalCopyright", b.Copyright);
                addString("InternalName", SanitizeFileName(b.AppName));
                addString("OriginalFilename", exe.filename().string());

                const String version = VersionString(b, true);
                args += " --set-file-version " + Quote(version);
                args += " --set-product-version " + Quote(version);
            }

            if(w.IconPath.empty() && !w.StampExecutable)
                return true; // nothing to do

            const int code = RunLogged(Quote(kRceditPath) + " " + args, Path());
            if(code != 0)
            {
                ExportLog(ExportLogLevel::ERROR, "rcedit failed with exit code " + std::to_string(code));
                return false;
            }
            return true;
        }

        bool ExecuteWindowsBuild(const ExportJob& job)
        {
            const ExportWindowsSettings& w = job.Windows;
            const Path out = w.OutputPath;
            const Path playerDir = kWindowsPlayerDir;

            if(!fs::exists(playerDir / "Player.exe"))
            {
                ExportLog(ExportLogLevel::ERROR, "Player.exe not found in " + playerDir.string() + " - build the Player target (Release) first.");
                return false;
            }

            SetStep("Preparing output folder", 0.2f);
            PrepareOutputFolder(out, w.CleanOutput);

            SetStep("Copying player", 0.4f);
            bool copied = Filesystem::CopyFile(playerDir / "Player.exe", out / "Player.exe");
            if(fs::exists(playerDir / "shaderc_shared.dll"))
                Filesystem::CopyFile(playerDir / "shaderc_shared.dll", out / "shaderc_shared.dll"); // TODO: Remove

            SetStep("Staging content", 0.70f);
            StageRuntimeContent(out, job);

            if(!w.IconPath.empty() || w.StampExecutable)
            {
                SetStep("Stamping executable", 0.85f);
                if(!StampWindowsExecutable(job, out / "Player.exe"))
                    return false;
            }

            if(copied)
                fs::rename(out / "Player.exe", out / (job.Branding.AppName + String(".exe")));
            else
            {
                ExportLog(ExportLogLevel::ERROR, "Failed to copy Player.exe to " + out.string());
                return false;
            }

            sLastOutputPath = out.string();
            return true;
        }

        // Android (Gradle)
        bool ExecuteAndroidBuild(const ExportJob& job)
        {
            const ExportAndroidSettings& a = job.Android;
            const ExportBranding& b = job.Branding;
            const Path projectDir = kAndroidProjectDir;
            const Path out = a.OutputPath;
            std::error_code ec;

            // Pre-flight
            if(!fs::exists(projectDir / "gradlew.bat"))
            {
                ExportLog(ExportLogLevel::ERROR, "gradlew.bat not found in " + projectDir.string());
                return false;
            }
            if(!fs::exists(a.KeystorePath))
            {
                ExportLog(ExportLogLevel::ERROR, "Keystore not found: " + a.KeystorePath);
                return false;
            }
            if(!a.IconPath.empty() && !fs::exists(a.IconPath))
            {
                ExportLog(ExportLogLevel::ERROR, "Icon not found: " + a.IconPath);
                return false;
            }
            if(!Platform::HasEnvVariable("ANDROID_HOME") && !Platform::HasEnvVariable("ANDROID_SDK_ROOT") && !fs::exists(projectDir / "local.properties"))
            {
                ExportLog(ExportLogLevel::ERROR, "Android SDK not found. Set ANDROID_HOME or add sdk.dir to local.properties.");
                return false;
            }

            SetStep("Preparing output folder", 0.1f);
            PrepareOutputFolder(out, a.CleanOutput);

            SetStep("Staging content into APK assets", 0.25f);
            const Path assetsRoot = projectDir / "app/src/main/assets";
            fs::remove_all(assetsRoot, ec);
            Filesystem::CreateOrEnsureDirectories(assetsRoot);
            StageRuntimeContent(assetsRoot, job);

            if(!a.IconPath.empty())
            {
                SetStep("Installing launcher icon", 0.3f);
                const Path resDir = projectDir / "app/src/main/res/mipmap-xxxhdpi";
                Filesystem::CreateOrEnsureDirectories(resDir);
                for(const char* name : { "ic_launcher.png", "ic_launcher_round.png" })
                    fs::copy_file(a.IconPath, resDir / name, fs::copy_options::overwrite_existing);
            }

            // Delete stale artifacts so we can never copy an old build by accident
            const bool isApk = a.Format == ExportAndroidFormat::APK;
            const Path artifact = projectDir / (isApk ? "app/build/outputs/apk/release/app-release.apk" : "app/build/outputs/bundle/release/app-release.aab");
            fs::remove(artifact, ec);

            String cmd = "call gradlew.bat --console=plain";
            cmd += " " + Quote("-PsurgeAppName=" + b.AppName);
            cmd += " " + Quote("-PsurgeApplicationId=" + a.PackageId);
            cmd += " " + Quote("-PsurgeVersionName=" + VersionString(b, false));
            cmd += " -PsurgeVersionCode=" + std::to_string(std::max(1, b.Build));
            cmd += " -PsurgeMinSdk=" + std::to_string(a.MinApi);
            cmd += String(" -PsurgeOrientation=") + kOrientations[std::clamp(a.Orientation, 0, static_cast<int>(std::size(kOrientations)) - 1)].Value;
            cmd += String(" -PsurgeImmersive=") + (a.Immersive ? "true" : "false");
            cmd += String(" -PsurgeCutout=") + (a.UseCutout ? "true" : "false");
            cmd += String(" -PsurgeKeepScreenOn=") + (a.KeepScreenOn ? "true" : "false");
            cmd += String(" -PsurgeExitOnBack=") + (a.ExitOnBack ? "true" : "false");
            cmd += isApk ? " assembleRelease" : " bundleRelease";

            SetStep("Running Gradle (this might take a while if running for the first time/modified engine source, be patient)", 0.4f);
            int exitCode = -1;
            {
                // Secrets go through the environment, not the cmd
                ScopedEnv e0("SURGE_KEYSTORE_PATH", Path(a.KeystorePath).make_preferred().string());
                ScopedEnv e1("SURGE_KEYSTORE_PASS", a.KeystorePass);
                ScopedEnv e2("SURGE_KEY_ALIAS", a.KeyAlias);
                ScopedEnv e3("SURGE_KEY_PASS", a.KeyPass.empty() ? a.KeystorePass : a.KeyPass);
                exitCode = RunLogged(cmd, projectDir);
            }

            if(exitCode != 0)
            {
                ExportLog(ExportLogLevel::ERROR, "Gradle failed with exit code " + std::to_string(exitCode));
                return false;
            }

            if(!fs::exists(artifact))
            {
                ExportLog(ExportLogLevel::ERROR, "Gradle succeeded but the artifact was not found at " + artifact.string());
                return false;
            }

            SetStep("Copying artifact", 0.8f);
            const Path dest = out / (SanitizeFileName(b.AppName) + (isApk ? ".apk" : ".aab"));
            fs::copy_file(artifact, dest, fs::copy_options::overwrite_existing);
            ExportLog(ExportLogLevel::INFO, "Output: " + dest.string());

            sLastOutputPath = out.string();
            return true;
        }

        // Job launch
        void RunJob(ExportJob job)
        {
            bool ok = false;
            try
            {
                ok = job.Target == ExportPlatform::WINDOWS ? ExecuteWindowsBuild(job) : ExecuteAndroidBuild(job);
            }
            catch(const std::exception& e)
            {
                ExportLog(ExportLogLevel::ERROR, String("Unhandled exception: ") + e.what());
            }

            if(ok)
            {
                sProgress = 1.0f;
                Platform::OpenFolderInExplorer(sLastOutputPath);
            }

            ExportLog(ok ? ExportLogLevel::INFO : ExportLogLevel::ERROR, ok ? "Build finished successfully." : "Build FAILED.");
            sState = ok ? ExportState::SUCCEEDED : ExportState::FAILED;
        }

        const String& OutputPathOf(ExportPlatform p)
        {
            return p == ExportPlatform::WINDOWS ? sWin.OutputPath : sAnd.OutputPath;
        }

        bool PreflightOutput(ExportPlatform target)
        {
            String reason;
            if(!ValidateOutputFolder(OutputPathOf(target), target == ExportPlatform::WINDOWS ? sWin.CleanOutput : sAnd.CleanOutput, reason))
            {
                ClearLog();
                ExportLog(ExportLogLevel::ERROR, reason);
                sState = ExportState::FAILED;
                return false;
            }
            return true;
        }

        void LaunchBuild(ExportPlatform target)
        {
            if(sState == ExportState::RUNNING)
                return;
            if(sWorker.joinable())
                sWorker.join();

            if(!PreflightOutput(target))
                return;

            ClearLog();
            sState = ExportState::RUNNING;

            // Cooking touches editor-side state, so it stays on the main thread.
            SetStep("Cooking assets", 0.0f);
            Editor* editor = static_cast<Editor*>(Core::GetClient());
            editor->GetAssetImporter().ScanAndCookAll();

            ExportJob job;
            job.Target = target;
            job.Branding = sBranding;
            job.Windows = sWin;
            job.Android = sAnd;
            job.Proj = editor->GetCurrentProject();
            job.Proj.StartScene = sStartScene;
            job.ProjectDir = GetProjectDir();

            sWorker = std::thread(RunJob, std::move(job));
        }

        void RequestBuild(ExportPlatform target)
        {
            if(!PreflightOutput(target))
                return;

            if(target == ExportPlatform::WINDOWS ? sWin.CleanOutput : sAnd.CleanOutput)
            {
                sPendingTab = target;
                String msg = std::format("All the files in the output folder:\n\"{}\" will be DELETED PERMANANTLY.\n\nAre you sure you want to continue?", OutputPathOf(target));
                ImGuiAux::ShowConfirmationBox("WARNING", msg.c_str(), [&]() { LaunchBuild(sPendingTab); });
            }
            else
                LaunchBuild(target);
        }

        // UI
        constexpr ImVec4 kAccent = ImGuiAux::Colors::ThemeColor1;
        constexpr ImVec4 kAccentHover = ImGuiAux::Colors::ThemeColor2;
        constexpr ImVec4 kAccentActive = ImGuiAux::Colors::ThemeColor1;
        constexpr ImVec4 kInputBg = ImVec4(0.067f, 0.067f, 0.067f, 1.0f);
        constexpr ImVec4 kInputHover = ImVec4(0.1f, 0.1f, 0.1f, 1.0f);
        constexpr ImVec4 kTextMuted = ImVec4(0.67f, 0.67f, 0.67f, 1.0f);
        constexpr ImVec4 kTrackBg = ImVec4(0.06f, 0.06f, 0.06f, 1.0f);
        constexpr ImVec4 kCardBg = ImVec4(0.09f, 0.09f, 0.09f, 1.0f);
        constexpr ImVec4 kOk = ImVec4(0.35f, 0.80f, 0.45f, 1.0f);
        constexpr ImVec4 kWarn = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
        constexpr ImVec4 kErr = ImVec4(0.95f, 0.3f, 0.3f, 1.0f);
        constexpr float kRounding = 2.5f;

        ImFont* sBold = nullptr;
        float sLabelWidth = 0.0f;

        struct ApiLevel
        {
            int Level;
            const char* Name;
        };
        constexpr ApiLevel kApiLevels[] = {
            {29, "API 29 (Android 10)"}, {30, "API 30 (Android 11)"},
            {31, "API 31 (Android 12)"}, {33, "API 33 (Android 13)"}, {34, "API 34 (Android 14)"},
        };

        int StringResizeCallback(ImGuiInputTextCallbackData* data)
        {
            if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
            {
                String* str = static_cast<String*>(data->UserData);
                str->resize(static_cast<size_t>(data->BufTextLen));
                data->Buf = str->data();
            }
            return 0;
        }

        bool InputString(const char* id, String& value, const char* hint = "", ImGuiInputTextFlags flags = 0)
        {
            flags |= ImGuiInputTextFlags_CallbackResize;
            return ImGui::InputTextWithHint(id, hint, value.data(), value.capacity() + 1, flags, StringResizeCallback, &value);
        }

        void Section(const char* title, bool first = false)
        {
            if(!first)
                ImGui::Dummy(ImVec2(0.0f, 14.0f));
            ImGui::PushFont(sBold);
            ImGui::TextColored(kAccent, "%s", title);
            ImGui::PopFont();
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0.0f, 4.0f));
        }

        void RowLabel(const char* label)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(kTextMuted, "%s", label);
            ImGui::SameLine(sLabelWidth);
        }

        void HelpMarker(const char* text)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if(ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", text);
        }

        void TextRow(const char* label, const char* id, String& value, const char* hint = "", ImGuiInputTextFlags flags = 0)
        {
            RowLabel(label);
            ImGui::SetNextItemWidth(-FLT_MIN);
            InputString(id, value, hint, flags);
        }

        template <typename BrowseFn>
        void PathRow(const char* label, const char* id, String& value, const char* hint, bool clearable, BrowseFn&& browse)
        {
            RowLabel(label);
            ImGui::PushID(id);

            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            ImGui::PushFont(sBold);
            const float browseW = ImGui::CalcTextSize("BROWSE").x + 28.0f;
            const float clearW = ImGui::CalcTextSize("X").x + 28.0f;
            ImGui::PopFont();

            const bool showClear = clearable && !value.empty();
            ImGui::SetNextItemWidth(-(browseW + spacing + (showClear ? clearW + spacing : 0.0f)));
            InputString("##path", value, hint, ImGuiInputTextFlags_ReadOnly);

            ImGui::PushFont(sBold);
            ImGui::PushStyleColor(ImGuiCol_Button, kInputBg);
            ImGui::SameLine();
            if(ImGuiAux::Button("BROWSE", ImVec2(browseW, 0.0f)))
            {
                String selected = browse();
                if(!selected.empty())
                    value = selected;
            }
            if(showClear)
            {
                ImGui::SameLine();
                if(ImGuiAux::Button("X", ImVec2(clearW, 0.0f)))
                    value.clear();
            }
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PopID();
        }

        void CheckRow(const char* label, const char* id, bool& value, const char* tooltip)
        {
            RowLabel(label);
            ImGui::Checkbox(id, &value);
            HelpMarker(tooltip);
        }

        void ClampedIntField(const char* id, int& value, int minV, int maxV, float width)
        {
            ImGui::SetNextItemWidth(width);
            if (ImGui::InputInt(id, &value, 0, 0))
            value = std::clamp(value, minV, maxV);
        }

        void DrawPlatformTabs(float availWidth, bool locked)
        {
            const float tabHeight = 45.0f;
            const float tabWidth = (availWidth - 8.0f) * 0.5f;

            ImGui::PushFont(sBold);

            const ImVec2 trackPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(trackPos.x - 4.0f, trackPos.y - 4.0f),
                                                      ImVec2(trackPos.x + availWidth + 4.0f, trackPos.y + tabHeight + 4.0f),
                                                      ImGui::ColorConvertFloat4ToU32(kTrackBg), kRounding);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
            ImGui::BeginDisabled(locked);

            auto tab = [&](const char* label, int index) {
                const bool active = sTab == index;
                ImGui::PushStyleColor(ImGuiCol_Button, active ? kAccent : ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kAccentHover : kInputBg);
                ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0, 0, 0, 1) : kTextMuted);
                if(ImGui::Button(label, ImVec2(tabWidth, tabHeight)))
                    sTab = index;
                ImGui::PopStyleColor(3);
                };
            tab("Windows", 0);
            ImGui::SameLine();
            tab("Android", 1);

            ImGui::EndDisabled();
            ImGui::PopStyleVar(2);
            ImGui::PopFont();
        }

        void DrawProjectSection()
        {
            Section("PROJECT", true);

            AssetManager* am = Core::GetAssetManager();
            Vector<AssetID> sceneIDs;
            bool found = false;
            for(const auto& [id, meta] : am->GetRegistryMap())
            {
                if(meta.Type == AssetType::SCENE)
                {
                    sceneIDs.push_back(id);
                    found |= (id == sStartScene);
                }
            }
            if(!found)
                sStartScene = AssetID::INVALID; // scene was deleted / project switched

            RowLabel("Start scene");
            ImGui::SetNextItemWidth(-FLT_MIN);
            const String preview = sStartScene.IsValid() ? am->GetMetadata(sStartScene).RelativePath : String("No scene selected...");
            if(ImGui::BeginCombo("##StartScene", preview.c_str()))
            {
                for(const AssetID& id : sceneIDs)
                {
                    const AssetMetadata& meta = am->GetMetadata(id);
                    const bool selected = (sStartScene == id);
                    if(ImGui::Selectable(meta.RelativePath.c_str(), selected))
                        sStartScene = id;
                    if(selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        void DrawApplicationSection()
        {
            Section("APPLICATION");

            TextRow("Name", "##AppName", sBranding.AppName, "Product / display name");
            TextRow("Company", "##Company", sBranding.Company, "e.g. Surge Technologies");
            TextRow("Description", "##Desc", sBranding.Description, "Shown as the executable's file description");
            TextRow("Copyright", "##Copyright", sBranding.Copyright, "e.g. Copyright (C) 2026 Surge Technologies");

            RowLabel("Version");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = ImGui::GetContentRegionAvail().x;
            const float w = (avail - spacing * 2.0f) / 3.0f;
            ClampedIntField("##Major", sBranding.Major, 0, 65535, w);
            ImGui::SameLine();
            ClampedIntField("##Minor", sBranding.Minor, 0, 65535, w);
            ImGui::SameLine();
            ClampedIntField("##Patch", sBranding.Patch, 0, 65535, w);

            RowLabel("Build number");
            ClampedIntField("##Build", sBranding.Build, 1, 65535, -FLT_MIN);
            ImGui::SameLine();
            HelpMarker("Windows: 4th part of the file/product version (M.m.p.build).\nAndroid: versionCode - must increase for every Play Store upload.");
        }

        void DrawWindowsSection()
        {
            Section("WINDOWS");

            PathRow("Build destination", "##WinOut", sWin.OutputPath, "Folder to export into", false,
                    []() { return FileDialog::ChooseFolder(); });
            PathRow("Icon (.ico)", "##WinIcon", sWin.IconPath, "Optional - embedded into the executable", true,
                    []() { return FileDialog::OpenFile("Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0"); });

            RowLabel("Architecture");
            ImGui::TextDisabled("Windows x86_64");

            CheckRow("Clean destination", "##WinClean", sWin.CleanOutput, "Delete everything inside the destination folder before exporting.");
            CheckRow("Stamp executable info", "##WinStamp", sWin.StampExecutable,
                     "Write product name, company, description, copyright and version into the exe via rcedit.");
        }

        void DrawAndroidSection()
        {
            Section("ANDROID");

            PathRow("Build destination", "##AndOut", sAnd.OutputPath, "Folder to export into", false,
                    []() { return FileDialog::ChooseFolder(); });
            PathRow("Icon (.png)", "##AndIcon", sAnd.IconPath, "Optional - 192x192 or larger", true,
                    []() { return FileDialog::OpenFile("PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0"); });

            TextRow("Package ID", "##PkgId", sAnd.PackageId, "com.company.game");
            if(!IsValidPackageId(sAnd.PackageId))
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sLabelWidth);
                ImGui::TextColored(kWarn, "Invalid package ID (letters, digits, '_' - at least two dot-separated parts)");
            }

            // Format
            RowLabel("Format");
            ImGui::SetNextItemWidth(-FLT_MIN);
            const char* formatPreview = sAnd.Format == ExportAndroidFormat::APK ? "APK (.apk) - sideload / testing" : "AAB (.aab) - Google Play";
            if(ImGui::BeginCombo("##AndFormat", formatPreview))
            {
                if(ImGui::Selectable("APK (.apk) - sideload / testing", sAnd.Format == ExportAndroidFormat::APK))
                    sAnd.Format = ExportAndroidFormat::APK;
                if(ImGui::Selectable("AAB (.aab) - Google Play", sAnd.Format == ExportAndroidFormat::AAB))
                    sAnd.Format = ExportAndroidFormat::AAB;
                ImGui::EndCombo();
            }

            // Min API
            RowLabel("Minimum API level");
            ImGui::SetNextItemWidth(-FLT_MIN);
            const char* apiPreview = "Custom";
            for(const ApiLevel& api : kApiLevels)
                if(api.Level == sAnd.MinApi)
                    apiPreview = api.Name;
            if(ImGui::BeginCombo("##AndApi", apiPreview))
            {
                for(const ApiLevel& api : kApiLevels)
                    if(ImGui::Selectable(api.Name, api.Level == sAnd.MinApi))
                        sAnd.MinApi = api.Level;
                ImGui::EndCombo();
            }

            RowLabel("Architecture");
            ImGui::TextDisabled("ARM64-v8a");

            CheckRow("Clean destination", "##AndClean", sAnd.CleanOutput, "Delete everything inside the destination folder before exporting.");

            Section("DISPLAY");

            RowLabel("Orientation");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::BeginCombo("##AndOrient", kOrientations[sAnd.Orientation].Name))
            {
                for(int i = 0; i < static_cast<int>(std::size(kOrientations)); ++i)
                    if(ImGui::Selectable(kOrientations[i].Name, i == sAnd.Orientation))
                        sAnd.Orientation = i;
                ImGui::EndCombo();
            }

            CheckRow("Immersive fullscreen", "##AndImmersive", sAnd.Immersive, "Hide the status and navigation bars. Swiping from a screen edge shows them briefly, then they hide again.");
            CheckRow("Draw into display cutout", "##AndCutout", sAnd.UseCutout, "Use the full screen including the notch / camera cut-out. Your UI must respect the safe-area insets.");
            CheckRow("Keep screen on", "##AndKeepOn", sAnd.KeepScreenOn, "Prevent the screen from dimming or locking while the game is running.");
            CheckRow("Exit on back (TODO)", "##AndExitOnBack", sAnd.ExitOnBack, "Exit the app when the back button is pressed.");

            Section("SIGNING");
            PathRow("Keystore", "##AndKey", sAnd.KeystorePath, "Path to .keystore / .jks", false,
                    []() { return FileDialog::OpenFile("Android Keystore (*.keystore;*.jks)\0*.keystore;*.jks\0All Files (*.*)\0*.*\0"); });
            TextRow("Key alias", "##KeyAlias", sAnd.KeyAlias);
            TextRow("Keystore password", "##StorePass", sAnd.KeystorePass, "", ImGuiInputTextFlags_Password);
            TextRow("Key password", "##KeyPass", sAnd.KeyPass, "Same as keystore password if empty", ImGuiInputTextFlags_Password);
            ImGuiAux::TextCentered("NOTE: Passwords are kept in memory only and NEVER written to disk or passed to command line");
        }

        Vector<String> CollectIssues(ExportPlatform tab)
        {
            Vector<String> issues;
            if(!sStartScene.IsValid())
                issues.push_back("Select a start scene");
            if(sBranding.AppName.empty())
                issues.push_back("Enter an application name");

            if(tab == ExportPlatform::WINDOWS)
            {
                if(sWin.OutputPath.empty())
                    issues.push_back("Choose a build destination");
            }
            else
            {
                if(sAnd.OutputPath.empty())
                    issues.push_back("Choose a build destination");
                if(!IsValidPackageId(sAnd.PackageId))
                    issues.push_back("Enter a valid package ID");
                if(sAnd.KeystorePath.empty())
                    issues.push_back("Choose a signing keystore");
                if(sAnd.KeyAlias.empty())
                    issues.push_back("Enter the key alias");
                if(sAnd.KeystorePass.empty())
                    issues.push_back("Enter the keystore password");
            }
            return issues;
        }

        // Returns true when the build button was pressed
        bool DrawBuildButton(ExportPlatform tab, bool running, const Vector<String>& issues)
        {
            ImGui::Dummy(ImVec2(0.0f, 24.0f));

            const bool canBuild = issues.empty() && !running;
            const char* label = running ? "BUILDING..." : (tab == ExportPlatform::WINDOWS ? "BUILD FOR WINDOWS" : "BUILD FOR ANDROID");

            ImGui::BeginDisabled(!canBuild);
            ImGui::PushFont(sBold, 22.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            const bool pressed = ImGuiAux::Button(label, ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            ImGui::EndDisabled();

            if(!running)
                for(const String& issue : issues)
                    ImGui::TextColored(kWarn, "- %s", issue.c_str());

            return pressed && canBuild;
        }

        void DrawStatusAndLog()
        {
            const ExportState state = sState;
            {
                std::scoped_lock lock(sLogMutex);
                if(state == ExportState::IDLE && sLog.empty())
                    return;
            }

            ImGui::Dummy(ImVec2(0.0f, 14.0f));

            if(state == ExportState::RUNNING)
            {
                String step;
                {
                    std::scoped_lock lock(sLogMutex);
                    step = sStepName;
                }
                ImGui::ProgressBar(sProgress, ImVec2(-FLT_MIN, 6.0f), "");
                ImGui::PushFont(sBold, 16.0f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.8f), "%s", step.c_str());
                ImGui::PopFont();
            }
            else if(state == ExportState::SUCCEEDED)
            {
                ImGui::TextColored(kOk, "Export succeeded");
                if(!sLastOutputPath.empty())
                {
                    ImGui::SameLine();
                    if(ImGui::SmallButton("OPEN FOLDER"))
                        Platform::OpenFolderInExplorer(sLastOutputPath);
                }
            }
            else if(state == ExportState::FAILED)
            {
                ImGui::PushFont(sBold, 16.0f);
                ImGui::TextColored(kErr, "BUILD FAILED // See log below");
                ImGui::PopFont();
            }

            ImGui::SameLine(ImGui::GetContentRegionMax().x - 90.0f);
            if(ImGui::SmallButton("COPY LOG"))
            {
                String all;
                std::scoped_lock lock(sLogMutex);
                for(const ExportLogLine& line : sLog)
                    all += line.Text + "\n";
                ImGui::SetClipboardText(all.c_str());
            }

            if(ImGui::BeginChild("##ExportLog", ImVec2(0.0f, 220.0f), ImGuiChildFlags_Borders))
            {
                const bool wasAtBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f;
                {
                    std::scoped_lock lock(sLogMutex);
                    ImGuiListClipper clipper;
                    clipper.Begin(static_cast<int>(sLog.size()));
                    while(clipper.Step())
                    {
                        for(int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
                        {
                            const ExportLogLine& line = sLog[static_cast<size_t>(i)];
                            const ImVec4 color = line.Level == ExportLogLevel::ERROR ? kErr
                                : line.Level == ExportLogLevel::WARN ? kWarn
                                : ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
                            ImGui::TextColored(color, "%s", line.Text.c_str());
                        }
                    }
                }
                if(wasAtBottom && state == ExportState::RUNNING)
                    ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }

    } // namespace

    // ExportPanel
    void ExportPanel::Init(void*)
    {
        mCode = GetStaticCode();
    }

    void ExportPanel::OnEvent(Event&)
    {}

    void ExportPanel::Render(bool* show)
    {
        if(!*show)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

        if(ImGui::Begin("Export Project", show))
        {
            sBold = ImGui::GetIO().Fonts->Fonts[1];
            sLabelWidth = ImGui::GetFontSize() * 11.0f;

            const bool running = sState == ExportState::RUNNING;
            const float availWidth = ImGui::GetContentRegionAvail().x;

            DrawPlatformTabs(availWidth, running);
            ImGui::Dummy(ImVec2(0.0f, 16.0f));

            // Settings card
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 25.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));

            bool buildPressed = false;
            const ExportPlatform tab = static_cast<ExportPlatform>(sTab);

            if(ImGui::BeginChild("ExportSettingsCard", ImVec2(availWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 9.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, kInputBg);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, kInputHover);
                ImGui::PushStyleColor(ImGuiCol_Header, kInputHover);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kAccentHover);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, kAccentActive);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, kAccent);

                ImGui::BeginDisabled(running);
                DrawProjectSection();
                DrawApplicationSection();
                if(tab == ExportPlatform::WINDOWS)
                    DrawWindowsSection();
                else
                    DrawAndroidSection();
                ImGui::EndDisabled();

                const Vector<String> issues = CollectIssues(tab);
                buildPressed = DrawBuildButton(tab, running, issues);
                DrawStatusAndLog();

                ImGui::PopStyleColor(6);
                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);

            if(buildPressed)
            {
                if(tab == ExportPlatform::WINDOWS)
                    BuildWindows();
                else
                    BuildAndroid();
            }
        }
        ImGui::End();

        ImGui::PopStyleVar();
    }

    void ExportPanel::Shutdown()
    {
        // Blocks until a running export finishes so the worker never outlives the editor
        if(sWorker.joinable())
            sWorker.join();
    }

    void ExportPanel::BuildWindows()
    {
        RequestBuild(ExportPlatform::WINDOWS);
    }

    void ExportPanel::BuildAndroid()
    {
        RequestBuild(ExportPlatform::ANDROID);
    }

} // namespace Surge