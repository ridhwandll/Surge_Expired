// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Export/Platforms/Windows/WindowsExporter.hpp"
#include "Export/ExportSession.hpp"
#include "Export/ExportSettingsIO.hpp"
#include "Surge/Utility/Filesystem.hpp"

#include <json/json.hpp>

namespace Surge
{
    namespace fs = std::filesystem;
    using ExportUtils::Quote;
    using ExportUtils::SanitizeFileName;

    namespace
    {
        class WindowsExportTask : public ExportTask
        {
        public:
            explicit WindowsExportTask(const WindowsExportSettings& settings)
                : mSettings(settings) {}

            virtual bool Run(const ExportJobInfo& job, ExportSession& session) override
            {
                const Path out = mSettings.OutputPath;
                const Path playerDir = WindowsExporter::PLAYER_DIR;
                const bool stamp = !mSettings.IconPath.empty() || mSettings.StampExecutable;

                if(!fs::exists(playerDir / "Player.exe"))
                {
                    session.Error("Player.exe not found in " + playerDir.string() + " - build the Player target (Release) first.");
                    return false;
                }

                session.SetStep("Preparing output folder", EXPORT_COOK_PROGRESS, 0.10f);
                ExportUtils::PrepareOutputFolder(session, out, mSettings.CleanOutput);
                if(session.IsCancelled())
                    return false;

                session.SetStep("Copying player", 0.10f, 0.20f);
                const Path stagedExe = out / "Player.exe";
                if(!Filesystem::CopyFile(playerDir / "Player.exe", stagedExe))
                {
                    session.Error("Failed to copy Player.exe to " + out.string());
                    return false;
                }
                if(fs::exists(playerDir / "shaderc_shared.dll"))
                    Filesystem::CopyFile(playerDir / "shaderc_shared.dll", out / "shaderc_shared.dll"); // TODO: Remove
                if(session.IsCancelled())
                    return false;

                session.SetStep("Staging content", 0.20f, stamp ? 0.80f : 0.95f);
                ExportUtils::StageRuntimeContent(out, job);
                if(session.IsCancelled())
                    return false;

                if(stamp)
                {
                    session.SetStep("Stamping executable", 0.80f, 0.95f);
                    if(!Stamp(job, session, stagedExe))
                        return false;
                }

                session.SetStep("Finalizing", 0.95f, 1.0f);
                const Path exe = out / (SanitizeFileName(job.Branding.AppName) + ".exe");
                std::error_code ec;
                fs::rename(stagedExe, exe, ec);
                if(ec)
                {
                    session.Error("Could not rename Player.exe to " + exe.filename().string() + ": " + ec.message());
                    return false;
                }

                session.SetResult(exe);
                return true;
            }

        private:
            bool Stamp(const ExportJobInfo& job, ExportSession& session, const Path& exe) const
            {
                const ExportBranding& b = job.Branding;

                if(!fs::exists(WindowsExporter::RCEDIT_PATH))
                {
                    session.Error(String("rcedit not found at ") + WindowsExporter::RCEDIT_PATH);
                    return false;
                }

                String args = Quote(Path(exe).make_preferred().string());

                if(!mSettings.IconPath.empty())
                    args += " --set-icon " + Quote(mSettings.IconPath);

                if(mSettings.StampExecutable)
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
                    addString("OriginalFilename", SanitizeFileName(b.AppName) + ".exe");

                    const String version = ExportUtils::VersionString(b, true);
                    args += " --set-file-version " + Quote(version);
                    args += " --set-product-version " + Quote(version);
                }

                const String rcedit = Quote(fs::absolute(WindowsExporter::RCEDIT_PATH).make_preferred().string());
                const int code = session.RunTool(rcedit + " " + args, Path());
                if(code != 0)
                {
                    if(!session.IsCancelled())
                        session.Error("rcedit failed with exit code " + std::to_string(code));
                    return false;
                }
                return true;
            }

        private:
            WindowsExportSettings mSettings;
        };
    } // namespace

    void WindowsExporter::SerializeSettings(nlohmann::json& out) const
    {
        out = {
            {"OutputPath", mSettings.OutputPath},
            {"IconPath", mSettings.IconPath},
            {"CleanOutput", mSettings.CleanOutput},
            {"StampExecutable", mSettings.StampExecutable},
        };
    }

    void WindowsExporter::DeserializeSettings(const nlohmann::json& in)
    {
        ExportSettingsIO::Read(in, "OutputPath", mSettings.OutputPath);
        ExportSettingsIO::Read(in, "IconPath", mSettings.IconPath);
        ExportSettingsIO::Read(in, "CleanOutput", mSettings.CleanOutput);
        ExportSettingsIO::Read(in, "StampExecutable", mSettings.StampExecutable);
    }

    void WindowsExporter::CollectIssues(Vector<String>& issues)
    {
        if(mSettings.OutputPath.empty())
            issues.push_back("Choose a build destination");
        if(!mPlayerExists.Check(String(PLAYER_DIR) + "/Player.exe"))
            issues.push_back(std::format("Build the Player target (Release) first - {}/Player.exe not found", PLAYER_DIR));
        if((mSettings.StampExecutable || !mSettings.IconPath.empty()) && !mRceditExists.Check(RCEDIT_PATH))
            issues.push_back(std::format("rcedit not found at {}", RCEDIT_PATH));
        if(!mSettings.IconPath.empty() && !mIconExists.Check(mSettings.IconPath))
            issues.push_back("Icon file not found");
    }

    Scope<ExportTask> WindowsExporter::CreateTask() const
    {
        return CreateScope<WindowsExportTask>(mSettings);
    }

} // namespace Surge
