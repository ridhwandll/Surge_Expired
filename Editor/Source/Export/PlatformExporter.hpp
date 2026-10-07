// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportTypes.hpp"
#include "Surge/Core/Scope.hpp"
#include "Surge/Core/Vector.hpp"
#include <json/json_fwd.hpp>

namespace Surge
{
    class ExportSession;

    // One export run on the worker thread. Owns a snapshot of the platform's settings (taken on the main thread),
    // so it never touches state the UI can edit
    class ExportTask
    {
    public:
        virtual ~ExportTask() = default;

        // [0, EXPORT_COOK_PROGRESS] of the bar is the cooking done on the main thread, steps start after it
        virtual bool Run(const ExportJobInfo& job, ExportSession& session) = 0;
    };

    // A target platform: owns its settings (persisted per project), validates them and creates the task that builds it.
    // Everything except ExportTask::Run() is called on the main thread
    class PlatformExporter
    {
    public:
        virtual ~PlatformExporter() = default;

        virtual ExportPlatform GetPlatform() const = 0;
        virtual const char* GetDisplayName() const = 0;
        virtual bool IsSupported() const { return true; }

        virtual ExportPlatformSettings& GetBaseSettings() = 0;
        virtual void ResetSettings() = 0;
        virtual void SerializeSettings(nlohmann::json& out) const = 0;  // Never write secrets
        virtual void DeserializeSettings(const nlohmann::json& in) = 0;

        // Blocking problems shown under the build button. Called every frame: cache filesystem checks (ExportUtils::CachedExists)
        virtual void CollectIssues(Vector<String>& issues) = 0;

        // Called right before the worker starts, after cooking. nullptr: the platform can't build
        virtual Scope<ExportTask> CreateTask() const = 0;
    };

} // namespace Surge
