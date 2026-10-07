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

#include "Utility/ImGuiAux.hpp"
#include "Editor.hpp"

#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <filesystem>

namespace Surge
{
    namespace
    {
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

        void ShowLogInConsole()
        {
            static_cast<Editor*>(Core::GetClient())->GetPanelManager().GetPanel<ConsolePanel>()->FocusWithSearch(ExportSession::LOG_TAG);
        }

        // Widgets
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
            if(ImGui::InputInt(id, &value, 0, 0))
                value = std::clamp(value, minV, maxV);
        }

        bool SmallActionButton(const char* label)
        {
            ImGui::PushFont(sBold);
            ImGui::PushStyleColor(ImGuiCol_Button, kInputBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kInputHover);
            const bool pressed = ImGuiAux::Button(label);
            ImGui::PopStyleColor(2);
            ImGui::PopFont();
            return pressed;
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

            ImGui::PushFont(sBold);

            const ImVec2 trackPos = ImGui::GetCursorScreenPos();
            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(trackPos.x - 4.0f, trackPos.y - 4.0f),
                                                      ImVec2(trackPos.x + availWidth + 4.0f, trackPos.y + tabHeight + 4.0f),
                                                      ImGui::ColorConvertFloat4ToU32(kTrackBg), kRounding);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(tabSpacing, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kRounding);
            ImGui::BeginDisabled(locked);

            for(size_t i = 0; i < exporters.size(); ++i)
            {
                const PlatformExporter& exporter = *exporters[i];
                const bool active = common.SelectedPlatform == exporter.GetPlatform();
                const ImVec4 idleText = exporter.IsSupported() ? kTextMuted : ImVec4(kTextMuted.x, kTextMuted.y, kTextMuted.z, 0.45f);

                if(i > 0)
                    ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, active ? kAccent : ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? kAccentHover : kInputBg);
                ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0, 0, 0, 1) : idleText);
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
            Section("PROJECT", true);

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

            RowLabel("Start scene");
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
            Section("APPLICATION");

            TextRow("Name", "##AppName", b.AppName, "Product / display name");
            TextRow("Company", "##Company", b.Company, "e.g. Surge Technologies");
            TextRow("Description", "##Desc", b.Description, "Shown as the executable's file description");
            TextRow("Copyright", "##Copyright", b.Copyright, "e.g. Copyright (C) 2026 Surge Technologies");

            RowLabel("Version");
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float avail = ImGui::GetContentRegionAvail().x;
            const float w = (avail - spacing * 2.0f) / 3.0f;
            ClampedIntField("##Major", b.Major, 0, 65535, w);
            ImGui::SameLine();
            ClampedIntField("##Minor", b.Minor, 0, 65535, w);
            ImGui::SameLine();
            ClampedIntField("##Patch", b.Patch, 0, 65535, w);

            RowLabel("Build number");
            ClampedIntField("##Build", b.Build, 1, 65535, ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("(?)").x - spacing);
            HelpMarker("Windows: 4th part of the file/product version (M.m.p.build).\nAndroid: versionCode - must increase for every Play Store upload.");

            CheckRow("Auto-increment build", "##AutoBuild", common.Options.AutoIncrementBuild, "Bump the build number after every successful export.");
        }

        // Platform sections
        void DrawWindowsSettings(WindowsExportSettings& w, ExportOptions& options)
        {
            Section("WINDOWS");

            PathRow("Build destination", "##WinOut", w.OutputPath, "Folder to export into", false,
                    []() { return FileDialog::ChooseFolder(); });
            PathRow("Icon (.ico)", "##WinIcon", w.IconPath, "Optional - embedded into the executable", true,
                    []() { return FileDialog::OpenFile("Icon Files (*.ico)\0*.ico\0All Files (*.*)\0*.*\0"); });

            RowLabel("Architecture");
            ImGui::TextDisabled("Windows x86_64");

            CheckRow("Clean destination", "##WinClean", w.CleanOutput, "Delete everything inside the destination folder before exporting.");
            CheckRow("Stamp executable info", "##WinStamp", w.StampExecutable,
                     "Write product name, company, description, copyright and version into the exe via rcedit.");
            CheckRow("Open folder when done", "##WinOpen", options.OpenFolderWhenDone, "Show the exported executable in Explorer after a successful export.");
        }

        void DrawAndroidSettings(AndroidExportSettings& a, ExportOptions& options)
        {
            Section("ANDROID");

            PathRow("Build destination", "##AndOut", a.OutputPath, "Folder to export into", false,
                    []() { return FileDialog::ChooseFolder(); });
            PathRow("Icon (.png)", "##AndIcon", a.IconPath, "Optional - 192x192 or larger", true,
                    []() { return FileDialog::OpenFile("PNG Images (*.png)\0*.png\0All Files (*.*)\0*.*\0"); });

            TextRow("Package ID", "##PkgId", a.PackageId, "com.company.game");
            if(!AndroidExporter::IsValidPackageId(a.PackageId))
            {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sLabelWidth);
                ImGui::TextColored(kWarn, "Invalid package ID (letters, digits, '_' - at least two dot-separated parts)");
            }

            // Format
            RowLabel("Format");
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
            RowLabel("Minimum API level");
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

            RowLabel("Architecture");
            ImGui::TextDisabled("ARM64-v8a");

            CheckRow("Clean destination", "##AndClean", a.CleanOutput, "Delete everything inside the destination folder before exporting.");
            CheckRow("Verbose build log", "##AndVerbose", a.VerboseLog, "Print every compiled C++ file to the Console. Warnings and errors are always printed.");

            Section("DISPLAY");

            RowLabel("Orientation");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if(ImGui::BeginCombo("##AndOrient", AndroidExporter::ORIENTATIONS[a.Orientation].Name))
            {
                for(int i = 0; i < AndroidExporter::ORIENTATION_COUNT; ++i)
                    if(ImGui::Selectable(AndroidExporter::ORIENTATIONS[i].Name, i == a.Orientation))
                        a.Orientation = i;
                ImGui::EndCombo();
            }

            CheckRow("Immersive fullscreen", "##AndImmersive", a.Immersive, "Hide the status and navigation bars. Swiping from a screen edge shows them briefly, then they hide again.");
            CheckRow("Draw into display cutout", "##AndCutout", a.UseCutout, "Use the full screen including the notch / camera cut-out. Your UI must respect the safe-area insets.");
            CheckRow("Keep screen on", "##AndKeepOn", a.KeepScreenOn, "Prevent the screen from dimming or locking while the game is running.");
            CheckRow("Exit on back (TODO)", "##AndExitOnBack", a.ExitOnBack, "Exit the app when the back button is pressed.");

            Section("SIGNING");
            PathRow("Keystore", "##AndKey", a.KeystorePath, "Path to .keystore / .jks", false,
                    []() { return FileDialog::OpenFile("Android Keystore (*.keystore;*.jks)\0*.keystore;*.jks\0All Files (*.*)\0*.*\0"); });
            TextRow("Key alias", "##KeyAlias", a.KeyAlias);
            TextRow("Keystore password", "##StorePass", a.KeystorePass, "", ImGuiInputTextFlags_Password);
            TextRow("Key password", "##KeyPass", a.KeyPass, "Same as keystore password if empty", ImGuiInputTextFlags_Password);
            ImGuiAux::TextCentered("NOTE: Passwords are kept in memory only and NEVER written to disk or passed to command line");

            Section("AFTER EXPORT");
            const bool isApk = a.Format == AndroidPackageFormat::APK;
            CheckRow("Open folder when done", "##AndOpen", options.OpenFolderWhenDone, "Show the exported package in Explorer after a successful export.");
            ImGui::BeginDisabled(!isApk);
            CheckRow("Install on device", "##AndInstall", a.InstallOnDevice,
                     isApk ? "Install the APK on the connected device with adb (USB debugging must be enabled)." : "Only available for APK exports.");
            ImGui::BeginDisabled(!a.InstallOnDevice);
            CheckRow("Launch after install", "##AndLaunch", a.LaunchAfterInstall, "Start the app on the device once it is installed.");
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
                ImGuiAux::ScopedBoldFont font(22.0f);
                const String title = std::format("{} export is not supported yet", exporter.GetDisplayName());
                ImGui::PushStyleColor(ImGuiCol_Text, kWarn);
                ImGuiAux::TextCentered(title.c_str());
                ImGui::PopStyleColor();
            }
            ImGui::Dummy(ImVec2(0.0f, 8.0f));
            ImGuiAux::TextCentered(reason);
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
            ImGui::PushFont(sBold, 22.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccentActive);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            const bool pressed = ImGuiAux::Button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
            ImGui::PopStyleColor(4);
            ImGui::PopFont();
            ImGui::EndDisabled();

            if(!busy)
                for(const String& issue : issues)
                    ImGui::TextColored(kWarn, "- %s", issue.c_str());

            return pressed && canBuild;
        }

        void DrawProgressBar(float fraction, bool indeterminate)
        {
            constexpr float height = 10.0f;
            const float rounding = height * 0.5f;
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float width = ImGui::GetContentRegionAvail().x;
            const ImVec2 p1(p0.x + width, p0.y + height);
            const float time = static_cast<float>(ImGui::GetTime());
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            drawList->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(kTrackBg), rounding);
            drawList->PushClipRect(p0, p1, true);

            if(indeterminate)
            {
                // A segment sweeping across the track
                const float segment = width * 0.25f;
                const float x = p0.x - segment + std::fmod(time * width * 0.6f, width + segment);
                drawList->AddRectFilled(ImVec2(x, p0.y), ImVec2(x + segment, p1.y), ImGui::ColorConvertFloat4ToU32(kAccent), rounding);
            }
            else
            {
                const float fillEnd = p0.x + std::max(height, width * std::clamp(fraction, 0.0f, 1.0f));
                drawList->AddRectFilled(p0, ImVec2(fillEnd, p1.y), ImGui::ColorConvertFloat4ToU32(kAccent), rounding);

                // Sheen running over the filled part, so long steps still look alive
                const float sheenWidth = 60.0f;
                const float sheenX = p0.x - sheenWidth + std::fmod(time * 220.0f, (fillEnd - p0.x) + sheenWidth * 2.0f);
                drawList->PushClipRect(p0, ImVec2(fillEnd, p1.y), true);
                drawList->AddRectFilledMultiColor(ImVec2(sheenX, p0.y), ImVec2(sheenX + sheenWidth * 0.5f, p1.y),
                                                  IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 70), IM_COL32(255, 255, 255, 70), IM_COL32(255, 255, 255, 0));
                drawList->AddRectFilledMultiColor(ImVec2(sheenX + sheenWidth * 0.5f, p0.y), ImVec2(sheenX + sheenWidth, p1.y),
                                                  IM_COL32(255, 255, 255, 70), IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 70));
                drawList->PopClipRect();
            }

            drawList->PopClipRect();
            ImGui::Dummy(ImVec2(width, height));
        }

        // Text on the left, right-aligned text on the same line
        void SplitLine(const ImVec4& leftColor, const String& left, const String& right)
        {
            ImGui::PushFont(sBold, 16.0f);
            ImGui::TextColored(leftColor, "%s", left.c_str());
            ImGui::PopFont();
            if(!right.empty())
            {
                ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize(right.c_str()).x);
                ImGui::TextColored(kTextMuted, "%s", right.c_str());
            }
        }

        void DrawStatus(ExportManager& manager, const ExportStatus& status, float displayProgress)
        {
            if(status.State == ExportState::IDLE)
                return;

            ImGui::Dummy(ImVec2(0.0f, 14.0f));

            const String elapsed = ExportUtils::FormatDuration(status.ElapsedSeconds);
            if(IsExportBusy(status.State))
            {
                SplitLine(ImVec4(1.0f, 1.0f, 1.0f, 0.85f), status.StepName, std::format("{}%  |  {}", static_cast<int>(displayProgress * 100.0f), elapsed));
                ImGui::Dummy(ImVec2(0.0f, 2.0f));
                DrawProgressBar(displayProgress, status.State == ExportState::COOKING);
                ImGui::TextColored(kTextMuted, "%s", status.StepDetail.empty() ? " " : status.StepDetail.c_str());

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                ImGui::BeginDisabled(status.CancelRequested);
                if(SmallActionButton(status.CancelRequested ? "CANCELLING..." : "CANCEL"))
                    manager.Cancel();
                ImGui::EndDisabled();
                ImGui::SameLine();
                if(SmallActionButton("SHOW LOG"))
                    ShowLogInConsole();
                return;
            }

            const String duration = status.ElapsedSeconds > 0.0f ? elapsed : String();

            if(status.State == ExportState::SUCCEEDED)
            {
                SplitLine(kOk, "EXPORT SUCCEEDED", duration.empty() ? String() : "in " + duration);
                DrawProgressBar(1.0f, false);

                std::error_code ec;
                const uintmax_t size = std::filesystem::file_size(status.ArtifactPath, ec);
                const String sizeText = ec ? String() : "  (" + ExportUtils::FormatSize(size) + ")";
                ImGui::TextWrapped("%s%s", status.ArtifactPath.c_str(), sizeText.c_str());
                if(status.WarningCount > 0)
                    ImGui::TextColored(kWarn, "%u warning%s", status.WarningCount, status.WarningCount == 1 ? "" : "s");
                if(!status.ResultWarning.empty())
                    ImGui::TextColored(kWarn, "%s", status.ResultWarning.c_str());

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if(SmallActionButton("SHOW FILE"))
                    Platform::OpenInExplorer(status.ArtifactPath);
                ImGui::SameLine();
                if(SmallActionButton("COPY PATH"))
                    ImGui::SetClipboardText(status.ArtifactPath.c_str());
                ImGui::SameLine();
                if(SmallActionButton("SHOW LOG"))
                    ShowLogInConsole();
            }
            else if(status.State == ExportState::FAILED)
            {
                SplitLine(kErr, "EXPORT FAILED", duration.empty() ? String() : "after " + duration);

                ImGui::PushStyleColor(ImGuiCol_Text, kErr);
                ImGui::PushTextWrapPos(0.0f);
                ImGui::TextUnformatted(status.FailureSummary.empty() ? "Unknown error. See the Console." : status.FailureSummary.c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();

                ImGui::Dummy(ImVec2(0.0f, 4.0f));
                if(SmallActionButton("SHOW LOG"))
                    ShowLogInConsole();
                if(!status.FailureSummary.empty())
                {
                    ImGui::SameLine();
                    if(SmallActionButton("COPY ERROR"))
                        ImGui::SetClipboardText(status.FailureSummary.c_str());
                }
            }
            else if(status.State == ExportState::CANCELLED)
            {
                SplitLine(kWarn, "EXPORT CANCELLED", duration.empty() ? String() : "after " + duration);
                if(SmallActionButton("SHOW LOG"))
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
                ImGuiAux::ShowConfirmationBox("WARNING", msg, [&manager, platform]() { manager.Launch(platform); });
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
            sBold = ImGui::GetIO().Fonts->Fonts[1];
            sLabelWidth = ImGui::GetFontSize() * 11.0f;

            ExportCommonSettings& common = manager.GetCommonSettings();
            const float availWidth = ImGui::GetContentRegionAvail().x;

            DrawPlatformTabs(manager, availWidth, busy);
            ImGui::Dummy(ImVec2(0.0f, 16.0f));

            // Settings card
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, kRounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(25.0f, 25.0f));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kCardBg);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));

            bool buildPressed = false;
            const ExportPlatform platform = common.SelectedPlatform;
            PlatformExporter* exporter = manager.GetExporter(platform);

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
                        case ExportPlatform::ANDROID: DrawAndroidSettings(manager.GetExporter<AndroidExporter>()->GetSettings(), common.Options); break;
                        default: break;
                    }
                    ImGui::EndDisabled();

                    const Vector<String> issues = manager.CollectIssues(platform);
                    buildPressed = DrawBuildButton(*exporter, busy, mDisplayProgress, issues);

                    // The result belongs to the platform it was built for
                    if(busy || manager.GetRunPlatform() == platform)
                        DrawStatus(manager, status, busy ? mDisplayProgress : status.Progress);
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
