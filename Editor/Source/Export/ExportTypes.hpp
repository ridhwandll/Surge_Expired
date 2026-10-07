// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Path.hpp"
#include "Surge/Core/Project.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    enum class ExportPlatform : int
    {
        WINDOWS = 0,
        ANDROID,
        IOS,
        MACOS,
        COUNT
    };

    enum class ExportState : int
    {
        IDLE = 0,
        COOKING, // Main thread, cooks the assets before the worker starts
        RUNNING, // Worker thread
        SUCCEEDED,
        FAILED,
        CANCELLED
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

    struct ExportOptions
    {
        bool OpenFolderWhenDone = true;
        bool AutoIncrementBuild = true; // After every successful export
    };

    // Settings shared by every platform, persisted per project
    struct ExportCommonSettings
    {
        AssetID StartScene = AssetID::INVALID;
        ExportPlatform SelectedPlatform = ExportPlatform::WINDOWS;
        ExportBranding Branding;
        ExportOptions Options;
    };

    // Every platform's settings start with these
    struct ExportPlatformSettings
    {
        String OutputPath;
        bool CleanOutput = false;
    };

    // Everything a run shares across platforms, snapshotted on the main thread
    struct ExportJobInfo
    {
        ExportBranding Branding;
        Project Proj; // StartScene already overridden
        Path ProjectDir;
    };

    struct ExportStatus
    {
        ExportState State = ExportState::IDLE;
        float Progress = 0.0f;
        float ElapsedSeconds = 0.0f;
        Uint WarningCount = 0;
        Uint ErrorCount = 0;
        bool CancelRequested = false;
        String StepName;
        String StepDetail;     // Current file / tool task
        String FailureSummary; // Most useful error line
        String ResultWarning;  // Export succeeded, but a post step (e.g. installing on a device) did not
        String ArtifactPath;
    };

    constexpr float EXPORT_COOK_PROGRESS = 0.05f; // Share of the progress bar taken by asset cooking

    constexpr bool IsExportBusy(ExportState state) { return state == ExportState::COOKING || state == ExportState::RUNNING; }
    constexpr bool IsExportFinished(ExportState state) { return state == ExportState::SUCCEEDED || state == ExportState::FAILED || state == ExportState::CANCELLED; }

} // namespace Surge
