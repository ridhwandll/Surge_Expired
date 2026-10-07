// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Export/PlatformExporter.hpp"

namespace Surge
{
    // TODO: Bundle ID, signing identity, notarization, universal (arm64 + x86_64) binary... once the runtime exists
    struct MacOSExportSettings : ExportPlatformSettings
    {
    };

    // STUB: macOS
    // A real implementation needs a Mac with Xcode to build and sign the .app bundle, the editor itself stays Windows-only
    class MacOSExporter : public PlatformExporter
    {
    public:
        static ExportPlatform GetStaticPlatform() { return ExportPlatform::MACOS; }

        virtual ExportPlatform GetPlatform() const override { return GetStaticPlatform(); }
        virtual const char* GetDisplayName() const override { return "macOS"; }
        virtual bool IsSupported() const override { return false; }

        virtual ExportPlatformSettings& GetBaseSettings() override { return mSettings; }
        virtual void ResetSettings() override { mSettings = {}; }
        virtual void SerializeSettings(nlohmann::json& out) const override;
        virtual void DeserializeSettings(const nlohmann::json& in) override;

        virtual void CollectIssues(Vector<String>& issues) override;
        virtual Scope<ExportTask> CreateTask() const override { return nullptr; }

        MacOSExportSettings& GetSettings() { return mSettings; }

    private:
        MacOSExportSettings mSettings;
    };

} // namespace Surge
