// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/UUID.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    struct ContentBrowserSettings
    {
        float ThumbnailSize = 96.0f;
        bool ListView = false;
        int SortField = 0; // ContentBrowserPanel::SortField
        bool SortDescending = false;
        float FolderTreeWidth = 220.0f;
        float DrawerHeight = 0.4f; // Content Drawer (Ctrl+Space), fraction of the editor area
        bool ShowUnimported = true;
        bool SearchAllFolders = true;
        Uint HiddenTypes = 0;     // Bit per AssetType hidden by the filter
        String LastDirectory;     // Relative to the Assets folder
        Vector<String> Favorites; // Folders relative to the Assets folder
    };

    // An open Scene/Game view, restored with the project
    struct ViewportSettings
    {
        int Type = 0;   // ViewportType
        Uint Index = 1; // Window title: "Scene", "Scene (2)", ...

        // Scene view camera (only applied when HasCameraView)
        bool HasCameraView = false;
        glm::vec3 CameraFocalPoint = glm::vec3(0.0f);
        float CameraDistance = 10.0f;
        float CameraPitch = 0.0f;
        float CameraYaw = 0.0f;
        float CameraSpeed = 0.003f;

        // Game view
        int AspectMode = 0; // GameViewport aspect presets
    };

    // Per project editor state, restored when the project is opened again (<ProjectDir>/EditorSettings.json)
    struct EditorSettings
    {
        static constexpr const char* FILE_NAME = "EditorSettings.json";

        // Viewports, empty: one Scene and one Game view
        bool ShowAxes = true;
        Vector<ViewportSettings> Viewports;

        // Transform gizmo
        int GizmoOperation = -1; // ImGuizmo::OPERATION, -1 = none
        bool GizmoWorldSpace = false;
        bool SnapEnabled = false; // Holding Ctrl inverts it
        float SnapTranslate = 0.5f;
        float SnapRotate = 45.0f;
        float SnapScale = 0.5f;

        ContentBrowserSettings ContentBrowser;

        // Session
        UUID LastScene = UUID::INVALID;
        float FrameCap = 144.0f;

        String ToString() const;
        void FromString(const String& text); // Unknown/missing/mistyped keys keep their defaults
    };

    // Editor wide preferences, shared by every project (<PersistentStorage>/EditorPreferences.json)
    struct EditorPreferences
    {
        static constexpr const char* FILE_NAME = "EditorPreferences.json";

        bool LightTheme = false;

        static EditorPreferences Load(); // Defaults when the file is missing or broken
        void Save() const;
    };

    // Reads/writes EditorSettings next to the project file, the file is only touched when something changed
    class EditorSettingsStore
    {
    public:
        void Load(const Path& projectDir, EditorSettings& outSettings);
        void Save(const EditorSettings& settings);
        void Reset() { mProjectDir.clear(); mLastSaved.clear(); }

    private:
        Path mProjectDir;
        String mLastSaved;
    };

} // namespace Surge
