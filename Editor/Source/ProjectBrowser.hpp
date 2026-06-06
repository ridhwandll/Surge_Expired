// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/String.hpp"

namespace Surge
{
    struct RecentProject
    {
        String Name;
        String Filepath;
        String LastOpened;
    };

    class AssetManager;
    class ProjectBrowser
    {
    public:
        void Init();
        void Render();
    private:
        void CreateProject();
        void OpenProject();
        void SerializeRecentProject(const String& name, const String& filepath);
        void SerializeRecentProjects();
        void DeserializeRecentProjects();
        String GetTimeString();
    private:
        AssetManager* mAssetManager = nullptr;
    };

} // namespace Surge