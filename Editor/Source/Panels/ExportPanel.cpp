// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Panels/ExportPanel.hpp"
#include "Panels/ConsolePanel.hpp"
#include "Export/ExportManager.hpp"
#include "Export/ExportUtils.hpp"
#include "Export/Platforms/Android/AndroidExporter.hpp"
#include "Export/Platforms/Windows/WindowsExporter.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Surge/Core/Core.hpp"
#include "Surge/Utility/FileDialogs.hpp"
#include "Surge/Utility/Platform.hpp"

#include "Utility/EditorUI.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>

namespace Surge
{
    namespace
    {
        namespace Colors = EditorUI::Colors;
        constexpr float kRounding = 2.0f;
        float sLabelWidth = 0.0f;

        void ShowLogInConsole(const char* tag = ExportSession::LOG_TAG)
        {
            static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ConsolePanel>()->FocusWithSearch(tag);
        }

        // One tab per registered exporter
        void DrawPlatformTabs(ExportManager& manager, float availWidth, bool locked)
        {
            constexpr float tabHeight = 45.0f;
            constexpr float tabSpacing = 8.0f;
            ExportCommonSettings& common = manager.GetCommonSettings();
            const auto& exporters = manager.GetExporters();
            const float count = static_cast<float>(exporters.size());
            const float tabWidth = (availWidth - tabSpacing * (count - 1.0f)) / count;

            ImGui::PushFont(EditorUI::BoldFont());

            const ImVec2 trackPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(trackPos.x - 4.0f, trackPos.y - 4.0f),
                                                      ImVec2(trackPos.x + availWidth + 4.0f, trackPos.y + tabHeight + 4.0f),
                                                      EditorUI::ToU32(Colors::Tone06), kRounding);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(tabSpacing, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
            ImGui::BeginDisabled(locked);

            for(size_t i = 0; i < exporters.size(); ++i)
            {
                const PlatformExporter& exporter = *exporters[i];
                const bool active = common.SelectedPlatform == exporter.GetPlatform();
                const ImVec4 idleText = exporter.IsSupported() ? Colors::Tone67 : EditorUI::WithAlpha(Colors::Tone67, 0.45f);

                if(i > 0)
                    ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, active ? Colors::Accent : Colors::Transparent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? Colors::Selection : Colors::Tone07);
                ImGui::PushStyleColor(ImGuiCol_Text, active ? Colors::Black : idleText);
                if(ImGui::Button(exporter.GetDisplayName(), ImVec2(tabWidth, tabHeight)))
                    common.SelectedPlatform = exporter.GetPlatform();
                ImGui::PopStyleColor(3);

                if(!exporter.IsSupported() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s export is not supported yet", exporter.GetDisplayName());
            }

            ImGui::EndDisabled();
            ImGui::PopStyleVar(2);
            ImGui::PopFont();
        }

        // Sections shared by every platform
        void DrawProjectSection(ExportCommonSettings& common)
        {
            EditorUI::SectionHeading("PROJECT", true);

            AssetManager* am = Core::GetAssetManager();
            Vector<AssetID> sceneIDs;
            bool found = false;
            for(const auto& [id, meta] : am->GetRegistryMap())
            {
                if(meta.Type == AssetType::SCENE)
                {
                    sceneIDs.push_back(id);
                    found |= (id == common.StartScene);
                }
            }
            if(!found)
                common.StartScene = AssetID::INVALID; // scene was deleted / project switched

            EditorUI::FormLabel("Start scene", sLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const String preview = common.StartScene.IsValid() ? am->GetMetadata(common.StartScene).RelativePath : String("No scene selected...");
            if(ImGui::BeginCombo("##StartScene", preview.c_str()))
            {
                for(const AssetID& id : sceneIDs)
                {
                    const AssetMetadata& meta = am->GetMetadata(id);
                    const bool selected = (common.StartScene == id);
                    if(ImGui::Selectable(meta.RelativePath.c_str(), selected))
                        common.StartScene = id;
                    if(selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
        }

        void DrawApplicationSection(ExportCommonSettings& common)
        {
            ExportBranding& b = common.Branding;
            EditorUI::SectionHeading("APPLICATION");

            EditorUI::FormInputText("Name", "##AppName", b.AppName, sLabelWidth, "Product / display name");
            EditorUI::FormInputText("Company", "##Company", b.Company, sLabelWidth, "e.g. Surge Technologies");
            EditorUI::FormInputText("Description", "##Desc", b.Description, sLabelWidth, "Shown as the executable's file description");
            EditorUI::FormInputText("Copyright", "##Copyright", b.Copyright, sLabelWidth, "e.g. Copyright (C) 2026 Surge Technologies");

            EditorUI::FormLabel("Version", sLabelWidth);
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = ImGui::GetContentRegionAvail().x;
            const float w = (avail - spacing * 2.0f) / 3.0f;
            EditorUI::InputIntClamped("##Major", b.Major, 0, 65535, w);
            ImGui::SameLine();
            EditorUI::InputIntClamped("##Minor", b.Minor, 0, 65535, w);
            ImGui::SameLine();
            EditorUI::InputIntClamped("##Patch", b.Patch, 0, 65535, w);

            EditorUI::FormLabel("Build number", sLabelWidth);
            EditorUI::InputIntClamped("##Build", b.Build, 1, 65535, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)").x - spacing);
            EditorUI::HelpMarker("Windows: 4th part of the file/product version (M.m.p.build).\nAndroid: versionCode - must increase for every Play Store upload.");

            EditorUI::FormCheckbox("Auto-increment build", "##AutoBuild", common.Options.AutoIncrementBuild, sLabelWidth, "Bump the build number after every successful export.");
        }

        // Platform sections
        void DrawWindowsSettings(WindowsExportSettings& w, ExportOptions& options)
        {
            EditorUI::SectionHeading("WINDOWS");

            EditorUI::FormLabel("Build destination", sLabelWidth);
            EditorUI::PathField("##WinOut", w.OutputPath, "Folder to export into", false, []() { return FileDialog::ChooseFolder(); });
            EditorUI::FormLabel("Icon (.ico)", sLabelWidth);
            EditorUI::PathField("##WinIcon", w.IconPath, "Optional - embedded into the executable", true, []() { return FileDialog::OpenFile("Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0"); });

            EditorUI::FormLabel("Architecture", sLabelWidth);
            ImGui::TextDisabled("Windows x86_64");

            EditorUI::FormCheckbox("Clean destination", "##WinClean", w.CleanOutput, sLabelWidth, "Delete everything inside the destination folder before exporting.");
            EditorUI::FormCheckbox("Stamp executable info", "##WinStamp", w.StampExecutable, sLabelWidth, "Write product name, company, description, copyright and version into the exe via rcedit.");
            EditorUI::FormCheckbox("Open folder when done", "##WinOpen", options.OpenFolderWhenDone, sLabelWidth, "Show the exported executable in Explorer after a successful export.");
        }

        // Every path the Android build uses, missing ones in their expected location. Click a path to copy it
        void DrawAndroidToolchain(AndroidExporter& exporter)
        {
            EditorUI::SectionHeading("ANDROID TOOLCHAIN");

            ImGui::PushFont(EditorUI::RegularFont(), 14.0f);
            bool incomplete = false;
            for(const AndroidToolItem& item : exporter.GetToolchain().Items)
            {
                incomplete |= item.State != AndroidToolState::OK;
                EditorUI::FormLabel(item.Name.c_str(), sLabelWidth);

                const ImVec4 color = item.State == AndroidToolState::OK ? ImGui::GetStyleColorVec4(ImGuiCol_Text) : item.State == AndroidToolState::WARN ? Colors::Warning : Colors::Error;
                ImGui::PushStyleColor(ImGuiCol_Text, color);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(item.Location.empty() ? "Not found" : item.Location.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();
                if(!item.Location.empty() && ImGui::IsItemHovered() && GImGui->HoveredIdTimer > 0.8f)
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                    ImGui::TextUnformatted("Click to copy the path");
                    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        ImGui::SetClipboardText(item.Location.c_str());
                }

                if(!item.Note.empty() && item.Location.empty())
                {
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sLabelWidth);
                    ImGui::PushStyleColor(ImGuiCol_Text, Colors::Tone67);
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextUnformatted(item.Note.c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::PopStyleColor();
                }
            }
            ImGui::PopFont();
            if(incomplete)
                EditorUI::TextCentered("Scripts/AndroidSetup.bat installs everything the Android build needs! Just run it and restart the editor!");
        }

        void DrawAndroidSettings(AndroidExporter& exporter, ExportOptions& options)
        {
            AndroidExportSettings& a = exporter.GetSettings();
            DrawAndroidToolchain(exporter);

            EditorUI::SectionHeading("ANDROID");
            EditorUI::FormLabel("Build destination", sLabelWidth);
            EditorUI::PathField("##AndOut", a.OutputPath, "Folder to export into", false, []() { return FileDialog::ChooseFolder(); });
            EditorUI::FormLabel("Icon (.png)", sLabelWidth);
            EditorUI::PathField("##AndIcon", a.IconPath, "Optional - 192x192 or larger", true, []() { return FileDialog::OpenFile("PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0"); });

            EditorUI::FormInputText("Package ID", "##PkgId", a.PackageId, sLabelWidth, "com.company.game");
            if(!AndroidExporter::IsValidPackageId(a.PackageId))
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sLabelWidth);
                ImGui::TextColored(Colors::Warning, "Invalid package ID (letters, digits, '_' - at least two dot-separated parts)");
            }

            // Format
            EditorUI::FormLabel("Format", sLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const char* formatPreview = a.Format == AndroidPackageFormat::APK ? "APK (.apk) - sideload / testing" : "AAB (.aab) - Google Play";
            if(ImGui::BeginCombo("##AndFormat", formatPreview))
            {
                if(ImGui::Selectable("APK (.apk) - sideload / testing", a.Format == AndroidPackageFormat::APK))
                    a.Format = AndroidPackageFormat::APK;
                if(ImGui::Selectable("AAB (.aab) - Google Play", a.Format == AndroidPackageFormat::AAB))
                    a.Format = AndroidPackageFormat::AAB;
                ImGui::EndCombo();
            }

            // Min API
            EditorUI::FormLabel("Minimum API level", sLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const char* apiPreview = "Custom";
            for(const AndroidApiLevel& api : AndroidExporter::API_LEVELS)
                if(api.Level == a.MinApi)
                    apiPreview = api.Name;
            if(ImGui::BeginCombo("##AndApi", apiPreview))
            {
                for(const AndroidApiLevel& api : AndroidExporter::API_LEVELS)
                    if(ImGui::Selectable(api.Name, api.Level == a.MinApi))
                        a.MinApi = api.Level;
                ImGui::EndCombo();
            }

            // Target API
            EditorUI::FormLabel("Target API level", sLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            const char* targetPreview = "Custom";
            for(const AndroidApiLevel& api : AndroidExporter::TARGET_API_LEVELS)
                if(api.Level == a.TargetApi)
                    targetPreview = api.Name;
            if(ImGui::BeginCombo("##AndTargetApi", targetPreview))
            {
                for(const AndroidApiLevel& api : AndroidExporter::TARGET_API_LEVELS)
                    if(ImGui::Selectable(api.Name, api.Level == a.TargetApi))
                        a.TargetApi = api.Level;
                ImGui::EndCombo();
            }
            if(a.TargetApi < 36)
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sLabelWidth);
                ImGui::TextColored(Colors::Warning, "Google Play rejects new apps and updates below API 36");
            }

            EditorUI::FormLabel("Architecture", sLabelWidth);
            ImGui::TextDisabled("ARM64-v8a");

            EditorUI::FormCheckbox("Clean destination", "##AndClean", a.CleanOutput, sLabelWidth, "Delete everything inside the destination folder before exporting.");
            EditorUI::FormCheckbox("Verbose build log", "##AndVerbose", a.VerboseLog, sLabelWidth, "Print every compiled C++ file to the Console. Warnings and errors are always printed.");

            EditorUI::SectionHeading("DISPLAY");
            EditorUI::FormLabel("Orientation", sLabelWidth);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::BeginCombo("##AndOrient", AndroidExporter::ORIENTATIONS[a.Orientation].Name))
            {
                for(int i = 0; i < AndroidExporter::ORIENTATION_COUNT; ++i)
                    if(ImGui::Selectable(AndroidExporter::ORIENTATIONS[i].Name, i == a.Orientation))
                        a.Orientation = i;
                ImGui::EndCombo();
            }

            EditorUI::FormCheckbox("Immersive fullscreen", "##AndImmersive", a.Immersive, sLabelWidth, "Hide the status and navigation bars. Swiping from a screen edge shows them briefly, then they hide again.");
            EditorUI::FormCheckbox("Draw into display cutout", "##AndCutout", a.UseCutout, sLabelWidth, "Use the full screen including the notch / camera cut-out. Your UI must respect the safe-area insets.");
            EditorUI::FormCheckbox("Keep screen on", "##AndKeepOn", a.KeepScreenOn, sLabelWidth, "Prevent the screen from dimming or locking while the game is running.");
            EditorUI::FormCheckbox("Exit on back (TODO)", "##AndExitOnBack", a.ExitOnBack, sLabelWidth, "Exit the app when the back button is pressed.");

            EditorUI::SectionHeading("SIGNING");
            EditorUI::FormLabel("Keystore", sLabelWidth);
            EditorUI::PathField("##AndKey", a.KeystorePath, "Path to .keystore / .jks", false, []() { return FileDialog::OpenFile("Android Keystore (*.keystore;*.jks)\0*.keystore;*.jks\0All Files (*.*)\0*.*\0"); });
            EditorUI::FormInputText("Key alias", "##KeyAlias", a.KeyAlias, sLabelWidth);
            EditorUI::FormInputText("Keystore password", "##StorePass", a.KeystorePass, sLabelWidth, "", ImGuiInputTextFlags_Password);
            EditorUI::FormInputText("Key password", "##KeyPass", a.KeyPass, sLabelWidth, "Same as keystore password if empty", ImGuiInputTextFlags_Password);
            EditorUI::TextCentered("NOTE: Passwords are kept in memory only and NEVER written to disk or passed to command line");

            EditorUI::SectionHeading("AFTER EXPORT");
            const bool isApk = a.Format == AndroidPackageFormat::APK;
            EditorUI::FormCheckbox("Open folder when done", "##AndOpen", options.OpenFolderWhenDone, sLabelWidth, "Show the exported package in Explorer after a successful export.");
            ImGui::BeginDisabled(!isApk);
            EditorUI::FormCheckbox("Install on device", "##AndInstall", a.InstallOnDevice, sLabelWidth, isApk ? "Install the APK on the connected device with adb (USB debugging must be enabled)." : "Only available for APK exports.");
            ImGui::BeginDisabled(!a.InstallOnDevice);
            EditorUI::FormCheckbox("Launch after install", "##AndLaunch", a.LaunchAfterInstall, sLabelWidth, "Start the app on the device once it is installed.");
            ImGui::BeginDisabled(!a.LaunchAfterInstall);
            EditorUI::FormCheckbox("Attach device log", "##AndLogcat", a.AttachLogcat, sLabelWidth, "After launching, stream the app's log and native crash reports into the Console (search [Device]).\nCrash backtraces are symbolized against the last build.");
            ImGui::EndDisabled();
            ImGui::EndDisabled();
            ImGui::EndDisabled();
        }

        void DrawUnsupportedPlatform(const PlatformExporter& exporter)
        {
            const char* reason = "This platform has no exporter implementation yet.";
            switch(exporter.GetPlatform())
            {
                case ExportPlatform::IOS:
                    reason = "iOS (Metal) support is planned, but the engine has no iOS runtime yet. Exporting will also need a Mac with Xcode to build and code sign the app.";
                    break;
                case ExportPlatform::MACOS:
                    reason = "macOS support is planned, but the engine has no macOS runtime yet. Exporting would also need a Mac with Xcode to build and sign the .app bundle.";
                    break;
                default:
                    break;
            }

            ImGui::Dummy(ImVec2(0.0f, 30.0f));
            {
                EditorUI::ScopedBoldFont font(22.0f);
                const String title = std::format("{} export is not supported yet", exporter.GetDisplayName());
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Warning);
                EditorUI::TextCentered(title.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            EditorUI::TextCentered(reason);
            ImGui::Dummy(ImVec2(0.0f, 30.0f));
        }

        // Returns true when the build button was pressed
        bool DrawBuildButton(const PlatformExporter& exporter, bool busy, float displayProgress, const Vector<String>& issues)
        {
            ImGui::Dummy(ImVec2(0.0f, 24.0f));

            const bool canBuild = issues.empty() && !busy;
            String label = busy ? std::format("EXPORTING... {}%###BuildButton", static_cast<int>(displayProgress * 100.0f))
                                : std::format("BUILD FOR {}###BuildButton", exporter.GetDisplayName());
            std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

            ImGui::BeginDisabled(!canBuild);
            ImGui::PushFont(EditorUI::BoldFont(), 22.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, Colors::Accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Colors::Selection);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, Colors::Accent);
            ImGui::PushStyleColor(ImGuiCol_Text, Colors::Black);
            const bool pressed = EditorUI::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            ImGui::EndDisabled();

            if(!busy)
                for(const String& issue : issues)
                    ImGui::TextColored(Colors::Warning, "- %s", issue.c_str());

            return pressed && canBuild;
        }

        // Attach by hand: an app started from the launcher, or after a crash (the recent log is included)
        void DrawAndroidDeviceLog(AndroidExporter& exporter, bool busy)
        {
            AndroidLogcat& logcat = exporter.GetLogcat();
            const String& packageId = exporter.GetSettings().PackageId;
            const bool hasAdb = !exporter.GetToolchain().Adb.empty();

            EditorUI::FormLabel("Device log", sLabelWidth);
            ImGui::BeginDisabled(busy);
            if(logcat.IsRunning())
            {
                if(EditorUI::FlatButton("DETACH"))
                    exporter.DetachLogcat();
            }
            else
            {
                ImGui::BeginDisabled(!hasAdb || !AndroidExporter::IsValidPackageId(packageId));
                if(EditorUI::FlatButton("ATTACH"))
                    exporter.AttachLogcat(packageId);
                ImGui::EndDisabled();
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip(hasAdb ? "Stream the log of %s from the connected device into the Console" : "adb not found in the Android SDK (platform-tools)", packageId.c_str());
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if(EditorUI::FlatButton("SHOW LOG##DeviceLog"))
                ShowLogInConsole(AndroidLogcat::LOG_TAG);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(Colors::Tone67, "%s", logcat.GetStatusText().c_str());
        }

        // The last exported APK, else the one an earlier export left in the build destination
        String FindInstallableApk(ExportManager& manager, const ExportStatus& exportStatus)
        {
            if(manager.GetRunPlatform() == ExportPlatform::ANDROID && exportStatus.State == ExportState::SUCCEEDED && exportStatus.ArtifactPath.ends_with(".apk"))
                return exportStatus.ArtifactPath;

            static ExportUtils::CachedExists sApkExists;
            const String& outputPath = manager.GetExporter<AndroidExporter>()->GetSettings().OutputPath;
            if(outputPath.empty())
                return String();
            const String apk = (Path(outputPath) / (ExportUtils::SanitizeFileName(manager.GetCommonSettings().Branding.AppName) + ".apk")).string();
            return sApkExists.Check(apk) ? apk : String();
        }

        // Installs (restarting adb first) without a new export, e.g. when 'Install on device' failed or the device was plugged in later
        void DrawAndroidDeviceInstall(AndroidExporter& exporter, const String& apk, const ExportStatus& exportStatus, bool busy)
        {
            AndroidDeviceInstaller& installer = exporter.GetDeviceInstaller();
            const ExportStatus status = installer.GetStatus();
            const bool ownsResult = status.State != ExportState::IDLE && !apk.empty() && installer.GetApk() == Path(apk);

            EditorUI::FormLabel("Install", sLabelWidth);
            ImGui::BeginGroup();
            if(IsExportBusy(status.State))
            {
                EditorUI::SplitText(EditorUI::WithAlpha(Colors::Ink, 0.85f), status.StepName, ExportUtils::FormatDuration(status.ElapsedSeconds));
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                EditorUI::ProgressBar(status.Progress, 10.0f, Colors::Accent, Colors::Tone06, true);
                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Tone67);
                ImGui::TextWrapped("%s", status.StepDetail.empty() ? " " : status.StepDetail.c_str());
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::BeginDisabled(status.CancelRequested);
                if(EditorUI::FlatButton(status.CancelRequested ? "CANCELLING...##Device" : "CANCEL##Device"))
                    installer.Cancel();
                ImGui::EndDisabled();
                ImGui::EndGroup();
                return;
            }

            const bool hasAdb = !exporter.GetToolchain().Adb.empty();
            const bool lastFailed = ownsResult ? status.State != ExportState::SUCCEEDED : apk == exportStatus.ArtifactPath && !exportStatus.ResultWarning.empty();
            const char* label = lastFailed ? "REATTACH ADB & INSTALL" : ownsResult ? "INSTALL AGAIN" : "INSTALL";
            ImGui::BeginDisabled(!hasAdb || apk.empty() || busy);
            if(EditorUI::FlatButton(label))
                exporter.ReattachAndInstall(apk);
            ImGui::EndDisabled();
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", !hasAdb ? "adb not found in the Android SDK (platform-tools)"
                                        : "Restarts the adb server, waits for an authorized device (up to 30 s) and installs the APK.");
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            if(apk.empty())
                ImGui::TextColored(Colors::Tone67, "No APK yet - export one first");
            else
                ImGui::TextColored(Colors::Tone67, "%s", Path(apk).filename().string().c_str());
            if(!apk.empty() && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", apk.c_str());

            if(ownsResult)
            {
                ImGui::PushTextWrapPos(0.0f);
                if(status.State == ExportState::SUCCEEDED)
                {
                    ImGui::TextColored(Colors::Success, "Installed on the device");
                    if(!status.ResultWarning.empty())
                        ImGui::TextColored(Colors::Warning, "%s", status.ResultWarning.c_str());
                }
                else if(status.State == ExportState::FAILED)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
                    ImGui::TextUnformatted(status.FailureSummary.empty() ? "Installing on the device failed. See the Console." : status.FailureSummary.c_str());
                    ImGui::PopStyleColor();
                }
                else if(status.State == ExportState::CANCELLED)
                    ImGui::TextColored(Colors::Warning, "Device install cancelled");
                ImGui::PopTextWrapPos();
            }
            ImGui::EndGroup();
        }

        // Below the build button and its status: everything that talks to the connected device
        void DrawAndroidDevice(ExportManager& manager, const ExportStatus& exportStatus, bool busy)
        {
            AndroidExporter& exporter = *manager.GetExporter<AndroidExporter>();
            EditorUI::SectionHeading("DEVICE");
            DrawAndroidDeviceLog(exporter, busy);
            DrawAndroidDeviceInstall(exporter, FindInstallableApk(manager, exportStatus), exportStatus, busy);
        }

        void DrawStatus(ExportManager& manager, const ExportStatus& status, float displayProgress)
        {
            if(status.State == ExportState::IDLE)
                return;

            ImGui::Dummy(ImVec2(0.0f, 14.0f));

            const String elapsed = ExportUtils::FormatDuration(status.ElapsedSeconds);
            if(IsExportBusy(status.State))
            {
                EditorUI::SplitText(EditorUI::WithAlpha(Colors::Ink, 0.85f), status.StepName, std::format("{}%  |  {}", static_cast<int>(displayProgress * 100.0f), elapsed));
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                EditorUI::ProgressBar(status.State == ExportState::COOKING ? -1.0f : displayProgress, 10.0f, Colors::Accent, Colors::Tone06, true);
                ImGui::TextColored(Colors::Tone67, "%s", status.StepDetail.empty() ? " " : status.StepDetail.c_str());

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::BeginDisabled(status.CancelRequested);
                if(EditorUI::FlatButton(status.CancelRequested ? "CANCELLING..." : "CANCEL"))
                    manager.Cancel();
                ImGui::EndDisabled();
                ImGui::SameLine();
                if(EditorUI::FlatButton("SHOW LOG"))
                    ShowLogInConsole();
                return;
            }

            const String duration = status.ElapsedSeconds > 0.0f ? elapsed : String();

            if(status.State == ExportState::SUCCEEDED)
            {
                EditorUI::SplitText(Colors::Success, "EXPORT SUCCEEDED", duration.empty() ? String() : "in " + duration);
                EditorUI::ProgressBar(1.0f, 10.0f, Colors::Accent, Colors::Tone06, true);

                std::error_code ec;
                const uintmax_t size = std::filesystem::file_size(status.ArtifactPath, ec);
                const String sizeText = ec ? String() : "  (" + ExportUtils::FormatSize(size) + ")";
                ImGui::TextWrapped("%s%s", status.ArtifactPath.c_str(), sizeText.c_str());
                if(status.WarningCount > 0)
                    ImGui::TextColored(Colors::Warning, "%u warning%s", status.WarningCount, status.WarningCount == 1 ? "" : "s");
                if(!status.ResultWarning.empty())
                    ImGui::TextColored(Colors::Warning, "%s", status.ResultWarning.c_str());

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if(EditorUI::FlatButton("SHOW FILE"))
                    Platform::OpenInExplorer(status.ArtifactPath);
                ImGui::SameLine();
                if(EditorUI::FlatButton("COPY PATH"))
                    ImGui::SetClipboardText(status.ArtifactPath.c_str());
                ImGui::SameLine();
                if(EditorUI::FlatButton("SHOW LOG"))
                    ShowLogInConsole();
            }
            else if(status.State == ExportState::FAILED)
            {
                EditorUI::SplitText(Colors::Error, "EXPORT FAILED", duration.empty() ? String() : "after " + duration);

                ImGui::PushStyleColor(ImGuiCol_Text, Colors::Error);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(status.FailureSummary.empty() ? "Unknown error. See the Console." : status.FailureSummary.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if(EditorUI::FlatButton("SHOW LOG"))
                    ShowLogInConsole();
                if(!status.FailureSummary.empty())
                {
                    ImGui::SameLine();
                    if(EditorUI::FlatButton("COPY ERROR"))
                        ImGui::SetClipboardText(status.FailureSummary.c_str());
                }
            }
            else if(status.State == ExportState::CANCELLED)
            {
                EditorUI::SplitText(Colors::Warning, "EXPORT CANCELLED", duration.empty() ? String() : "after " + duration);
                if(EditorUI::FlatButton("SHOW LOG"))
                    ShowLogInConsole();
            }
        }

        void RequestBuild(ExportManager& manager, ExportPlatform platform)
        {
            if(!manager.Preflight(platform))
                return;

            if(manager.WillCleanOutput(platform))
            {
                const String msg = std::format("All the files in the output folder:\n\"{}\" will be DELETED PERMANENTLY.\n\nAre you sure you want to continue?",
                                               manager.GetOutputPath(platform));
                EditorUI::ShowConfirmationBox("WARNING", msg, [&manager, platform]() { manager.Launch(platform); });
            }
            else
                manager.Launch(platform);
        }

    } // namespace

    // ExportPanel
    ExportPanel::ExportPanel() = default;
    ExportPanel::~ExportPanel() = default;

    void ExportPanel::Init(void*)
    {
        mCode = GetStaticCode();
        mManager = CreateScope<ExportManager>();
    }

    void ExportPanel::OnEvent(Event&)
    {}

    void ExportPanel::Render(bool* show)
    {
        // Runs even while hidden: cooking, finishing a run and project switches don't depend on the panel being open
        ExportManager& manager = *mManager;
        manager.Update();
        if(manager.ConsumeFailure())
            ShowLogInConsole();

        if(!*show)
            return;

        const ExportStatus status = manager.GetStatus();
        const bool busy = IsExportBusy(status.State);

        // Ease towards the worker's target so jumps between steps don't snap
        if(status.State == ExportState::COOKING)
            mDisplayProgress = status.Progress;
        else if(busy)
            mDisplayProgress += (status.Progress - mDisplayProgress) * std::min(1.0f, ImGui::GetIO().DeltaTime * 6.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

        if(ImGui::Begin("Export Project", show))
        {
            sLabelWidth = ImGui::GetFontSize() * 11.0f;

            ExportCommonSettings& common = manager.GetCommonSettings();
            const float availWidth = ImGui::GetContentRegionAvail().x;

            DrawPlatformTabs(manager, availWidth, busy);
            ImGui::Dummy(ImVec2(0.0f, 16.0f));

            // Settings card
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 25.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, Colors::Tone09);
            ImGui::PushStyleColor(ImGuiCol_Border, Colors::Tone18);

            bool buildPressed = false;
            const ExportPlatform platform = common.SelectedPlatform;
            PlatformExporter* exporter = manager.GetExporter(platform);

            if(ImGui::BeginChild("ExportSettingsCard", ImVec2(availWidth, 0), ImGuiChildFlags_AlwaysUseWindowPadding))
            {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 9.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
                ImGui::PushStyleColor(ImGuiCol_FrameBg, Colors::Tone07);
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, Colors::Tone10);
                ImGui::PushStyleColor(ImGuiCol_Header, Colors::Tone10);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, Colors::Selection);
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, Colors::Accent);
                ImGui::PushStyleColor(ImGuiCol_CheckMark, Colors::Accent);

                if(exporter && !exporter->IsSupported())
                    DrawUnsupportedPlatform(*exporter);
                else if(exporter)
                {
                    ImGui::BeginDisabled(busy);
                    DrawProjectSection(common);
                    DrawApplicationSection(common);
                    switch(platform)
                    {
                        case ExportPlatform::WINDOWS: DrawWindowsSettings(manager.GetExporter<WindowsExporter>()->GetSettings(), common.Options); break;
                        case ExportPlatform::ANDROID: DrawAndroidSettings(*manager.GetExporter<AndroidExporter>(), common.Options); break;
                        default: break;
                    }
                    ImGui::EndDisabled();

                    const Vector<String> issues = manager.CollectIssues(platform);
                    buildPressed = DrawBuildButton(*exporter, busy, mDisplayProgress, issues);

                    // The result belongs to the platform it was built for
                    if(busy || manager.GetRunPlatform() == platform)
                        DrawStatus(manager, status, busy ? mDisplayProgress : status.Progress);

                    if(platform == ExportPlatform::ANDROID)
                        DrawAndroidDevice(manager, status, busy);
                }

                ImGui::PopStyleColor(6);
                ImGui::PopStyleVar(2);
            }
            ImGui::EndChild();

            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);

            if(buildPressed)
                RequestBuild(manager, platform);
        }
        ImGui::End();

        ImGui::PopStyleVar();
    }

    void ExportPanel::Shutdown()
    {
        // Cancels a running export and waits for it, so the worker (and Gradle) never outlive the editor
        if(mManager)
            mManager->Shutdown();
    }

} // namespace Surge
