// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportSession.hpp"
#include "Export/PlatformExporter.hpp"
#include <thread>

namespace Surge
{
    // Owns the platform exporters, the per-project export settings (ExportSettings.json next to the .surgeproj) and the
    // lifetime of a run: cook on the main thread -> ExportTask on a worker thread -> result handling back on the main thread.
    // Everything here is main thread only, the worker only sees its ExportTask and the ExportSession
    class ExportManager
    {
    public:
        static constexpr const char* SETTINGS_FILE_NAME = "ExportSettings.json";

        ExportManager();
        ~ExportManager();
        SURGE_DISABLE_COPY_AND_MOVE(ExportManager);

        // Every frame, also while the panel is hidden: swaps settings when the project changes, starts cooking, finishes runs
        void Update();

        // Cancels a running export, waits for it and saves the settings
        void Shutdown();

        ExportCommonSettings& GetCommonSettings() { return mCommon; }
        const Vector<Scope<PlatformExporter>>& GetExporters() const { return mExporters; }
        PlatformExporter* GetExporter(ExportPlatform platform) const;

        template <typename T>
        T* GetExporter() const { return static_cast<T*>(GetExporter(T::GetStaticPlatform())); }

        // Common + platform problems that block the build, empty when it can start
        Vector<String> CollectIssues(ExportPlatform platform);

        // Build flow: Preflight() -> (confirm when WillCleanOutput()) -> Launch()
        bool Preflight(ExportPlatform platform); // Output folder safety
        bool WillCleanOutput(ExportPlatform platform);
        const String& GetOutputPath(ExportPlatform platform);
        void Launch(ExportPlatform platform);
        void Cancel() { mSession.RequestCancel(); }

        bool IsBusy() const { return IsExportBusy(mSession.GetState()); }
        ExportStatus GetStatus() const { return mSession.GetStatus(); }
        ExportPlatform GetRunPlatform() const { return mRunPlatform; }
        bool ConsumeFailure(); // true once after a run failed

        void SaveSettings();

    private:
        template <typename T>
        void Register();

        void StartWorker();
        void JoinWorker();
        void OnFinished(ExportState state);
        void LoadSettings(const Path& projectDir);
        String SettingsToString() const;

    private:
        Vector<Scope<PlatformExporter>> mExporters;
        ExportCommonSettings mCommon;

        ExportSession mSession;
        std::thread mWorker;
        Scope<ExportTask> mTask;
        ExportJobInfo mJob;

        ExportPlatform mRunPlatform = ExportPlatform::WINDOWS;
        uint64_t mFrame = 0;
        uint64_t mCookRequestFrame = 0;
        bool mCookScheduled = false;
        bool mFinishHandled = true;
        bool mFailurePending = false;

        Path mSettingsProjectDir;
        String mLastSavedSettings;
    };

} // namespace Surge
