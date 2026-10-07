// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/ExportUtils.hpp"
#include "Export/PlatformExporter.hpp"

namespace Surge
{
    struct WindowsExportSettings : ExportPlatformSettings
    {
        String IconPath; // .ico
        bool StampExecutable = true;
    };

    // Copies the prebuilt Release Player, stages the content next to it and stamps icon/version info with rcedit
    class WindowsExporter : public PlatformExporter
    {
    public:
        static constexpr const char* PLAYER_DIR = "build/Player/Release";
        static constexpr const char* RCEDIT_PATH = "Editor/Tools/rcedit-x64.exe";

        static ExportPlatform GetStaticPlatform() { return ExportPlatform::WINDOWS; }

        virtual ExportPlatform GetPlatform() const override { return GetStaticPlatform(); }
        virtual const char* GetDisplayName() const override { return "Windows"; }

        virtual ExportPlatformSettings& GetBaseSettings() override { return mSettings; }
        virtual void ResetSettings() override { mSettings = {}; }
        virtual void SerializeSettings(nlohmann::json& out) const override;
        virtual void DeserializeSettings(const nlohmann::json& in) override;

        virtual void CollectIssues(Vector<String>& issues) override;
        virtual Scope<ExportTask> CreateTask() const override;

        WindowsExportSettings& GetSettings() { return mSettings; }

    private:
        WindowsExportSettings mSettings;
        ExportUtils::CachedExists mPlayerExists;
        ExportUtils::CachedExists mRceditExists;
        ExportUtils::CachedExists mIconExists;
    };

} // namespace Surge
