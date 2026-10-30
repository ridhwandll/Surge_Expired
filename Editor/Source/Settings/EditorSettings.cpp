// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Settings/EditorSettings.hpp"
#include "Export/ExportSettingsIO.hpp"
#include "Surge/Core/Logger/Logger.hpp"
#include "Surge/Utility/Platform.hpp"
#include <json/json.hpp>
#include <algorithm>
#include <array>

namespace Surge
{
    namespace
    {
        constexpr int VIEWPORT_TYPE_SCENE = 0; // ViewportType::SCENE
        constexpr int VIEWPORT_TYPE_GAME = 1;  // ViewportType::GAME

        void ReadCamera(const nlohmann::json& j, ViewportSettings& viewport)
        {
            using ExportSettingsIO::Read;
            Read(j, "HasCameraView", viewport.HasCameraView);
            std::array<float, 3> focalPoint = { viewport.CameraFocalPoint.x, viewport.CameraFocalPoint.y, viewport.CameraFocalPoint.z };
            Read(j, "CameraFocalPoint", focalPoint);
            viewport.CameraFocalPoint = { focalPoint[0], focalPoint[1], focalPoint[2] };
            Read(j, "CameraDistance", viewport.CameraDistance);
            Read(j, "CameraPitch", viewport.CameraPitch);
            Read(j, "CameraYaw", viewport.CameraYaw);
            Read(j, "CameraSpeed", viewport.CameraSpeed);
        }
    } // namespace

    String EditorSettings::ToString() const
    {
        nlohmann::json j;
        j["ShowAxes"] = ShowAxes;

        nlohmann::json& viewports = j["Viewports"] = nlohmann::json::array();
        for(const ViewportSettings& viewport : Viewports)
        {
            nlohmann::json& v = viewports.emplace_back();
            v["Type"] = viewport.Type == VIEWPORT_TYPE_GAME ? "Game" : "Scene";
            v["Index"] = viewport.Index;
            if(viewport.Type == VIEWPORT_TYPE_GAME)
                v["AspectMode"] = viewport.AspectMode;
            else
            {
                v["HasCameraView"] = viewport.HasCameraView;
                v["CameraFocalPoint"] = { viewport.CameraFocalPoint.x, viewport.CameraFocalPoint.y, viewport.CameraFocalPoint.z };
                v["CameraDistance"] = viewport.CameraDistance;
                v["CameraPitch"] = viewport.CameraPitch;
                v["CameraYaw"] = viewport.CameraYaw;
                v["CameraSpeed"] = viewport.CameraSpeed;
            }
        }

        j["GizmoOperation"] = GizmoOperation;
        j["GizmoWorldSpace"] = GizmoWorldSpace;
        j["SnapEnabled"] = SnapEnabled;
        j["SnapTranslate"] = SnapTranslate;
        j["SnapRotate"] = SnapRotate;
        j["SnapScale"] = SnapScale;

        nlohmann::json& browser = j["ContentBrowser"];
        browser["ThumbnailSize"] = ContentBrowser.ThumbnailSize;
        browser["ListView"] = ContentBrowser.ListView;
        browser["SortField"] = ContentBrowser.SortField;
        browser["SortDescending"] = ContentBrowser.SortDescending;
        browser["FolderTreeWidth"] = ContentBrowser.FolderTreeWidth;
        browser["DrawerHeight"] = ContentBrowser.DrawerHeight;
        browser["ShowUnimported"] = ContentBrowser.ShowUnimported;
        browser["SearchAllFolders"] = ContentBrowser.SearchAllFolders;
        browser["HiddenTypes"] = ContentBrowser.HiddenTypes;
        browser["LastDirectory"] = ContentBrowser.LastDirectory;
        browser["Favorites"] = ContentBrowser.Favorites;

        j["LastScene"] = LastScene.Get();
        j["FrameCap"] = FrameCap;
        return j.dump(4);
    }

    void EditorSettings::FromString(const String& text)
    {
        const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        if(j.is_discarded() || !j.is_object())
            return;

        using ExportSettingsIO::Read;
        Read(j, "ShowAxes", ShowAxes);

        Viewports.clear();
        if(const auto it = j.find("Viewports"); it != j.end() && it->is_array())
        {
            for(const nlohmann::json& v : *it)
            {
                if(!v.is_object())
                    continue;

                ViewportSettings viewport;
                String type;
                Read(v, "Type", type);
                viewport.Type = type == "Game" ? VIEWPORT_TYPE_GAME : VIEWPORT_TYPE_SCENE;
                Read(v, "Index", viewport.Index);
                Read(v, "AspectMode", viewport.AspectMode);
                ReadCamera(v, viewport);

                viewport.Index = glm::clamp(viewport.Index, 1u, 64u);
                viewport.AspectMode = glm::clamp(viewport.AspectMode, 0, 4);
                const bool duplicate = std::any_of(Viewports.begin(), Viewports.end(), [&](const ViewportSettings& other) { return other.Type == viewport.Type && other.Index == viewport.Index; });
                if(!duplicate)
                    Viewports.push_back(viewport);
            }
        }
        else if(j.contains("HasCameraView") || j.contains("AspectMode"))
        {
            // Files from the single viewport era: its camera becomes the Scene view's, its aspect the Game view's
            ViewportSettings sceneView;
            sceneView.Type = VIEWPORT_TYPE_SCENE;
            ReadCamera(j, sceneView);

            ViewportSettings gameView;
            gameView.Type = VIEWPORT_TYPE_GAME;
            Read(j, "AspectMode", gameView.AspectMode);
            gameView.AspectMode = glm::clamp(gameView.AspectMode, 0, 4);

            Viewports = { sceneView, gameView };
        }

        Read(j, "GizmoOperation", GizmoOperation);
        Read(j, "GizmoWorldSpace", GizmoWorldSpace);
        Read(j, "SnapEnabled", SnapEnabled);
        Read(j, "SnapTranslate", SnapTranslate);
        Read(j, "SnapRotate", SnapRotate);
        Read(j, "SnapScale", SnapScale);

        if(const auto it = j.find("ContentBrowser"); it != j.end())
        {
            const nlohmann::json& browser = *it;
            Read(browser, "ThumbnailSize", ContentBrowser.ThumbnailSize);
            Read(browser, "ListView", ContentBrowser.ListView);
            Read(browser, "SortField", ContentBrowser.SortField);
            Read(browser, "SortDescending", ContentBrowser.SortDescending);
            Read(browser, "FolderTreeWidth", ContentBrowser.FolderTreeWidth);
            Read(browser, "DrawerHeight", ContentBrowser.DrawerHeight);
            Read(browser, "ShowUnimported", ContentBrowser.ShowUnimported);
            Read(browser, "SearchAllFolders", ContentBrowser.SearchAllFolders);
            Read(browser, "HiddenTypes", ContentBrowser.HiddenTypes);
            Read(browser, "LastDirectory", ContentBrowser.LastDirectory);
            Read(browser, "Favorites", ContentBrowser.Favorites);
        }

        uint64_t lastScene = LastScene.Get();
        Read(j, "LastScene", lastScene);
        LastScene = UUID(lastScene);
        Read(j, "FrameCap", FrameCap);

        // Hand edited files must not break the editor
        SnapTranslate = glm::max(SnapTranslate, 0.001f);
        SnapRotate = glm::max(SnapRotate, 0.01f);
        SnapScale = glm::max(SnapScale, 0.001f);
        FrameCap = glm::clamp(FrameCap, 0.0f, 1000.0f);
        ContentBrowser.ThumbnailSize = glm::clamp(ContentBrowser.ThumbnailSize, 48.0f, 256.0f);
        ContentBrowser.SortField = glm::clamp(ContentBrowser.SortField, 0, 3);
        ContentBrowser.FolderTreeWidth = glm::clamp(ContentBrowser.FolderTreeWidth, 120.0f, 800.0f);
        ContentBrowser.DrawerHeight = glm::clamp(ContentBrowser.DrawerHeight, 0.2f, 0.9f);
    }

    EditorPreferences EditorPreferences::Load()
    {
        EditorPreferences preferences;
        const Path path = Path(Platform::GetPersistantStoragePath()) / FILE_NAME;
        String text;
        if(!Filesystem::Exists(path) || !Filesystem::ReadTextFile(path, text))
            return preferences;

        const nlohmann::json j = nlohmann::json::parse(text, nullptr, false);
        String theme;
        ExportSettingsIO::Read(j, "Theme", theme);
        preferences.LightTheme = theme == "Light";
        return preferences;
    }

    void EditorPreferences::Save() const
    {
        nlohmann::json j;
        j["Theme"] = LightTheme ? "Light" : "Dark";
        const Path path = Path(Platform::GetPersistantStoragePath()) / FILE_NAME;
        if(!Filesystem::WriteTextFile(path, j.dump(4)))
            Log<Severity::Warn>("[Editor] Could not write {}", path.string());
    }

    void EditorSettingsStore::Load(const Path& projectDir, EditorSettings& outSettings)
    {
        outSettings = {};
        mProjectDir = projectDir;
        mLastSaved.clear();

        String text;
        if(Filesystem::Exists(projectDir / EditorSettings::FILE_NAME) && Filesystem::ReadTextFile(projectDir / EditorSettings::FILE_NAME, text))
        {
            outSettings.FromString(text);
            mLastSaved = std::move(text); // The file as it is: older formats get rewritten once, by the first save
        }
    }

    void EditorSettingsStore::Save(const EditorSettings& settings)
    {
        if(mProjectDir.empty())
            return;

        String text = settings.ToString();
        if(text == mLastSaved)
            return; // Don't touch the file (or its VCS status) when nothing changed

        if(Filesystem::WriteTextFile(mProjectDir / EditorSettings::FILE_NAME, text))
            mLastSaved = std::move(text);
        else
            Log<Severity::Warn>("[Editor] Could not write {}", (mProjectDir / EditorSettings::FILE_NAME).string());
    }

} // namespace Surge
