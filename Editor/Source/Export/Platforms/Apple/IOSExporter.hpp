// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/PlatformExporter.hpp"

namespace Surge
{
    // TODO: Bundle ID, team ID, provisioning profile, device family, orientation... once the runtime exists
    struct IOSExportSettings : ExportPlatformSettings
    {
    };

    // STUB: iOS (Metal)
    // A real implementation needs a Mac with Xcode (xcodebuild + codesign), the editor itself stays Windows-only
    class IOSExporter : public PlatformExporter
    {
    public:
        static ExportPlatform GetStaticPlatform() { return ExportPlatform::IOS; }

        virtual ExportPlatform GetPlatform() const override { return GetStaticPlatform(); }
        virtual const char* GetDisplayName() const override { return "iOS"; }
        virtual bool IsSupported() const override { return false; }

        virtual ExportPlatformSettings& GetBaseSettings() override { return mSettings; }
        virtual void ResetSettings() override { mSettings = {}; }
        virtual void SerializeSettings(nlohmann::json& out) const override;
        virtual void DeserializeSettings(const nlohmann::json& in) override;

        virtual void CollectIssues(Vector<String>& issues) override;
        virtual Scope<ExportTask> CreateTask() const override { return nullptr; }

        IOSExportSettings& GetSettings() { return mSettings; }

    private:
        IOSExportSettings mSettings;
    };

} // namespace Surge
