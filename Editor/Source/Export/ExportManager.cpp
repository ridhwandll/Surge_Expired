// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/ExportManager.hpp"
#include "Export/ExportSettingsIO.hpp"
#include "Export/ExportUtils.hpp"
#include "Export/Platforms/Android/AndroidExporter.hpp"
#include "Export/Platforms/Apple/IOSExporter.hpp"
#include "Export/Platforms/Apple/MacOSExporter.hpp"
#include "Export/Platforms/Windows/WindowsExporter.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/Platform.hpp"
#include "Editor.hpp"

#include <json/json.hpp>
#include <algorithm>
#include <fstream>

namespace Surge
{
    ExportManager::ExportManager()
    {
        Register<WindowsExporter>();
        Register<AndroidExporter>();
        Register<IOSExporter>();
        Register<MacOSExporter>();
    }

    ExportManager::~ExportManager()
    {
        Shutdown();
    }

    template <typename T>
    void ExportManager::Register()
    {
        static_assert(std::is_base_of_v<PlatformExporter, T>, "T must derive from PlatformExporter!");
        SG_ASSERT(GetExporter(T::GetStaticPlatform()) == nullptr, "Export platform registered twice!");
        mExporters.push_back(CreateScope<T>());
    }

    PlatformExporter* ExportManager::GetExporter(ExportPlatform platform) const
    {
        for(const Scope<PlatformExporter>& exporter : mExporters)
            if(exporter->GetPlatform() == platform)
                return exporter.get();
        return nullptr;
    }

    void ExportManager::Update()
    {
        mFrame++;

        const Path projectDir = ExportUtils::GetProjectDir();
        if(!IsBusy() && projectDir != mSettingsProjectDir)
        {
            SaveSettings(); // Settings are per project, swap them when the project changes
            LoadSettings(projectDir);
        }

        const ExportState state = mSession.GetState();

        // Cook at the end of a later frame, so "Cooking assets" is on screen before the main thread blocks on it
        if(state == ExportState::COOKING && !mCookScheduled && mFrame > mCookRequestFrame + 1)
        {
            mCookScheduled = true;
            Core::AddFrameEndCallback([this]() { StartWorker(); });
        }

        if(!mFinishHandled && IsExportFinished(state))
            OnFinished(state);
    }

    void ExportManager::Shutdown()
    {
        // The worker (and Gradle) must never outlive the editor
        mSession.RequestCancel();
        JoinWorker();
        mTask.reset();
        SaveSettings();
    }

    Vector<String> ExportManager::CollectIssues(ExportPlatform platform)
    {
        Vector<String> issues;
        PlatformExporter* exporter = GetExporter(platform);
        if(!exporter)
            return { "Unknown platform" };

        if(exporter->IsSupported())
        {
            if(!mCommon.StartScene.IsValid())
                issues.push_back("Select a start scene");
            if(mCommon.Branding.AppName.empty())
                issues.push_back("Enter an application name");
        }
        exporter->CollectIssues(issues);
        return issues;
    }

    bool ExportManager::WillCleanOutput(ExportPlatform platform)
    {
        PlatformExporter* exporter = GetExporter(platform);
        return exporter && exporter->GetBaseSettings().CleanOutput;
    }

    const String& ExportManager::GetOutputPath(ExportPlatform platform)
    {
        static const String sEmpty;
        PlatformExporter* exporter = GetExporter(platform);
        return exporter ? exporter->GetBaseSettings().OutputPath : sEmpty;
    }

    bool ExportManager::Preflight(ExportPlatform platform)
    {
        PlatformExporter* exporter = GetExporter(platform);
        String reason;

        bool ok = true;
        if(!exporter || !exporter->IsSupported())
        {
            reason = std::format("{} export is not supported yet.", exporter ? exporter->GetDisplayName() : "This platform's");
            ok = false;
        }
        else
        {
            const ExportPlatformSettings& settings = exporter->GetBaseSettings();
            ok = ExportUtils::ValidateOutputFolder(settings.OutputPath, settings.CleanOutput, reason);
        }

        if(!ok)
        {
            mSession.Reset();
            mSession.Error(reason);
            mSession.SetState(ExportState::FAILED);
            mRunPlatform = platform;
            mFinishHandled = true; // Shown in the panel, nothing else to do
        }
        return ok;
    }

    void ExportManager::Launch(ExportPlatform platform)
    {
        if(IsBusy())
            return;

        JoinWorker();

        if(!Preflight(platform))
            return;

        mSession.Reset();
        mRunPlatform = platform;
        mCookRequestFrame = mFrame;
        mCookScheduled = false;
        mFinishHandled = false;
        mFailurePending = false;
        mSession.Start("Cooking assets");

        mSession.Log(Severity::Info, std::format("Exporting '{}' {} for {}", mCommon.Branding.AppName, ExportUtils::VersionString(mCommon.Branding, true), GetExporter(platform)->GetDisplayName()));
        SaveSettings();
    }

    bool ExportManager::ConsumeFailure()
    {
        const bool failed = mFailurePending;
        mFailurePending = false;
        return failed;
    }

    void ExportManager::StartWorker()
    {
        if(mSession.IsCancelled())
        {
            mSession.Finish(false);
            return;
        }

        // Cooking touches editor-side state, so it stays on the main thread
        mSession.Log(Severity::Info, "> Cooking assets");
        Editor* editor = static_cast<Editor*>(Core::GetClient());
        editor->GetAssetImporter().ScanAndCookAll();

        mTask = GetExporter(mRunPlatform)->CreateTask();
        if(!mTask)
        {
            mSession.Error(std::format("{} export is not supported yet.", GetExporter(mRunPlatform)->GetDisplayName()));
            mSession.Finish(false);
            return;
        }

        mJob = {};
        mJob.Branding = mCommon.Branding;
        mJob.Proj = editor->GetCurrentProject();
        mJob.Proj.StartScene = mCommon.StartScene;
        mJob.ProjectDir = ExportUtils::GetProjectDir();

        mSession.SetState(ExportState::RUNNING);
        mWorker = std::thread([this]() {
            bool ok = false;
            try
            {
                ok = mTask->Run(mJob, mSession);
            }
            catch(const std::exception& e)
            {
                mSession.Error(String("Unhandled exception: ") + e.what());
            }
            mSession.Finish(ok);
        });
    }

    void ExportManager::JoinWorker()
    {
        if(mWorker.joinable())
            mWorker.join();
    }

    void ExportManager::OnFinished(ExportState state)
    {
        mFinishHandled = true;
        JoinWorker(); // Already done, the state is its last write
        mTask.reset();

        if(state == ExportState::SUCCEEDED)
        {
            if(mCommon.Options.AutoIncrementBuild)
                mCommon.Branding.Build = std::clamp(std::max(mCommon.Branding.Build, mJob.Branding.Build + 1), 1, 65535);
            if(mCommon.Options.OpenFolderWhenDone)
                Platform::OpenInExplorer(mSession.GetStatus().ArtifactPath); // Opens the folder with the artifact selected
        }
        else if(state == ExportState::FAILED)
            mFailurePending = true;

        SaveSettings();
    }

    // Settings persistence (per project, secrets are never written)
    String ExportManager::SettingsToString() const
    {
        const ExportBranding& b = mCommon.Branding;

        nlohmann::json j;
        j["INFO"] = "Export settings, generated by the Surge Editor. Passwords are never stored.";
        j["StartScene"] = mCommon.StartScene.Get();
        j["Platform"] = static_cast<int>(mCommon.SelectedPlatform);
        j["Branding"] = {
            {"AppName", b.AppName}, {"Company", b.Company}, {"Description", b.Description}, {"Copyright", b.Copyright},
            {"Major", b.Major}, {"Minor", b.Minor}, {"Patch", b.Patch}, {"Build", b.Build},
        };
        j["Options"] = {
            {"OpenFolderWhenDone", mCommon.Options.OpenFolderWhenDone}, {"AutoIncrementBuild", mCommon.Options.AutoIncrementBuild},
        };

        for(const Scope<PlatformExporter>& exporter : mExporters)
        {
            nlohmann::json section;
            exporter->SerializeSettings(section);
            if(!section.empty())
                j[exporter->GetDisplayName()] = std::move(section);
        }
        return j.dump(4);
    }

    void ExportManager::SaveSettings()
    {
        if(mSettingsProjectDir.empty())
            return;

        const String text = SettingsToString();
        if(text == mLastSavedSettings)
            return; // Don't touch the file (or its VCS status) when nothing changed

        std::ofstream file(mSettingsProjectDir / SETTINGS_FILE_NAME);
        if(file)
        {
            file << text;
            mLastSavedSettings = text;
        }
    }

    void ExportManager::LoadSettings(const Path& projectDir)
    {
        // Fresh defaults per project, this also drops the previous project's passwords
        const Project& project = static_cast<Editor*>(Core::GetClient())->GetCurrentProject();
        mCommon = {};
        mCommon.Branding.AppName = project.Name;
        mCommon.StartScene = project.StartScene;
        for(const Scope<PlatformExporter>& exporter : mExporters)
            exporter->ResetSettings();
        mSettingsProjectDir = projectDir;

        std::ifstream file(projectDir / SETTINGS_FILE_NAME);
        if(file)
        {
            try
            {
                nlohmann::json j;
                file >> j;

                uint64_t startScene = mCommon.StartScene.Get();
                ExportSettingsIO::Read(j, "StartScene", startScene);
                mCommon.StartScene = AssetID(startScene);

                int platform = static_cast<int>(mCommon.SelectedPlatform);
                ExportSettingsIO::Read(j, "Platform", platform);
                mCommon.SelectedPlatform = static_cast<ExportPlatform>(std::clamp(platform, 0, static_cast<int>(ExportPlatform::COUNT) - 1));

                if(const auto it = j.find("Branding"); it != j.end())
                {
                    ExportBranding& b = mCommon.Branding;
                    ExportSettingsIO::Read(*it, "AppName", b.AppName);
                    ExportSettingsIO::Read(*it, "Company", b.Company);
                    ExportSettingsIO::Read(*it, "Description", b.Description);
                    ExportSettingsIO::Read(*it, "Copyright", b.Copyright);
                    ExportSettingsIO::Read(*it, "Major", b.Major);
                    ExportSettingsIO::Read(*it, "Minor", b.Minor);
                    ExportSettingsIO::Read(*it, "Patch", b.Patch);
                    ExportSettingsIO::Read(*it, "Build", b.Build);
                }
                if(const auto it = j.find("Options"); it != j.end())
                {
                    ExportSettingsIO::Read(*it, "OpenFolderWhenDone", mCommon.Options.OpenFolderWhenDone);
                    ExportSettingsIO::Read(*it, "AutoIncrementBuild", mCommon.Options.AutoIncrementBuild);
                }

                for(const Scope<PlatformExporter>& exporter : mExporters)
                    if(const auto it = j.find(exporter->GetDisplayName()); it != j.end())
                        exporter->DeserializeSettings(*it);
            }
            catch(const nlohmann::json::exception& e)
            {
                Log<Severity::Warn>("{} Could not read {}: {}", ExportSession::LOG_TAG, SETTINGS_FILE_NAME, e.what());
            }
        }

        mCommon.Branding.Build = std::clamp(mCommon.Branding.Build, 1, 65535);
        mLastSavedSettings = SettingsToString();
    }

} // namespace Surge
