// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Project.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include "Surge/Serializer/Serializer.hpp"

namespace Surge
{
    ProjectMetadata::ProjectMetadata(const String& name, const Path& path)
    {
        Name = name;
        ProjPath = path;
        ProjectID = UUID();
        ActiveSceneIndex = 0;
        InternalDirectory = fmt::format("{0}/{1}", path, ".surge");
        ProjectMetadataPath = fmt::format("{0}/{1}.surgeProj", InternalDirectory, name);

        bool success = Filesystem::CreateOrEnsureDirectory(InternalDirectory);
        SG_ASSERT(success, "Cannot create directory!");
        Filesystem::CreateOrEnsureFile(ProjectMetadataPath);

        Serializer::Serialize<ProjectMetadata>(ProjectMetadataPath, this);
    }

    Project::Project()
    {
        mIsValid = false;
    }

    Project::~Project()
    {
        Destroy();
    }

    void Project::Invalidate(const String& name, const Path& path)
    {
        Destroy();
        mMetadata = ProjectMetadata(name, path);
        // Add a default scene
        Ref<Scene> scene = AddScene("Default", fmt::format("{0}", fmt::format("{0}/Default.surge", mMetadata.ProjPath)));
        Serializer::Deserialize<Scene>("Engine/Assets/Scenes/Default.surge", scene.Raw());
        Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());
        mIsValid = true;
    }

    void Project::Invalidate(const ProjectMetadata& metadata)
    {
        Destroy();
        mMetadata = metadata;
        bool success = Filesystem::CreateOrEnsureDirectory(mMetadata.InternalDirectory);
        SG_ASSERT(success, "Cannot create directory!");
        Filesystem::CreateOrEnsureFile(mMetadata.ProjectMetadataPath);

        if (metadata.SceneMetadatas.empty())
        {
            // Add a default scene if there is none
            Ref<Scene> scene = AddScene("Default", fmt::format("{0}", fmt::format("{0}/Default.surge", mMetadata.ProjPath)));
            Serializer::Deserialize<Scene>("Engine/Assets/Scenes/Default.surge", scene.Raw()); // Load the default scene to the new scene
            Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());         // Save the new scene in the project path
        }
        else
        {
            for (const SceneMetadata& sceneMetadata : metadata.SceneMetadatas)
            {
                // We don't use Project::AddScene here, as that adds the scene to the metadata
                // As in this scope we only read from the already filled metadata
                Ref<Scene> scene = Ref<Scene>::Create(this, sceneMetadata, false);
                mScenes.push_back(scene);
                Serializer::Deserialize<Scene>(sceneMetadata.ScenePath, scene.Raw());
            }
        }

        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata); // Write the project metadata to file
        mIsValid = true;
    }

    void Project::OnRuntimeStart()
    {
        mRuntimeSceneStorage.resize(mScenes.size());
        for (Uint i = 0; i < mScenes.size(); i++)
        {
            // Create the runtime scene
            auto& runtimeScene = mRuntimeSceneStorage[i];
            runtimeScene = Ref<Scene>::Create(this, mScenes[i]->GetMetadata().Name, mScenes[i]->GetMetadata().ScenePath, true);
            mScenes[i]->CopyTo(runtimeScene.Raw()); // Copy the scene
            runtimeScene->OnRuntimeStart();
        }
        Core::GetRenderer()->SetSceneContext(mRuntimeSceneStorage[mMetadata.ActiveSceneIndex]);
    }

    void Project::Update(EditorCamera& camera)
    {
        switch (mProjectState)
        {
            case Surge::ProjectState::Edit:
                mScenes[mMetadata.ActiveSceneIndex]->Update(camera);
                break;
            case Surge::ProjectState::Play:
                mRuntimeSceneStorage[mMetadata.ActiveSceneIndex]->Update();
                break;
        }
    }

    void Project::OnRuntimeEnd()
    {
        for (auto& scene : mRuntimeSceneStorage)
        {
            scene->OnRuntimeEnd();
        }
        mRuntimeSceneStorage.clear();
        Core::GetRenderer()->SetSceneContext(mScenes[mMetadata.ActiveSceneIndex]);
    }

    Ref<Scene> Project::AddScene(const SceneMetadata& metadata)
    {
        Ref<Scene> newScene = Ref<Scene>::Create(this, metadata, false);
        mScenes.push_back(newScene);
        mMetadata.SceneMetadatas.push_back(newScene->GetMetadata());
        Serializer::Serialize<Scene>(metadata.ScenePath, newScene.Raw());
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        return newScene;
    }

    Ref<Scene> Project::AddScene(const String& name, const Path& path)
    {
        Ref<Scene> newScene = Ref<Scene>::Create(this, name, path, false);
        mScenes.push_back(newScene);
        mMetadata.SceneMetadatas.push_back(newScene->GetMetadata());
        Serializer::Serialize<Scene>(path, newScene.Raw());
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        return newScene;
    }

    void Project::RenameScene(Uint index, const String& newName)
    {
        Ref<Scene>& sceneToRename = mScenes[index];
        SceneMetadata& metadata = sceneToRename->GetMetadata();

        Filesystem::RemoveFile(metadata.ScenePath);
        metadata.Name = newName;
        metadata.ScenePath = fmt::format("{0}/{1}.surge", Filesystem::GetParentPath(metadata.ScenePath), newName);

        mMetadata.SceneMetadatas[index].Name = newName;
        mMetadata.SceneMetadatas[index].ScenePath = metadata.ScenePath;

        Serializer::Serialize<Scene>(metadata.ScenePath, sceneToRename.Raw());
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
    }

    void Project::RemoveScene(Uint arrayIndex)
    {
        Ref<Scene>& sceneToRemove = mScenes[arrayIndex];
        Filesystem::RemoveFile(sceneToRemove->GetMetadata().ScenePath);
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        mScenes.erase(mScenes.begin() + arrayIndex);
    }

    Ref<Scene>& Project::GetScene(Uint arrayIndex)
    {
        return mScenes[arrayIndex];
    }

    void Project::SetActiveScene(Uint sceneindex)
    {
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);

        Ref<Scene>& activatedScene = GetScene(sceneindex);

        for (auto& func : mOnActiveSceneChangeCallbacks)
            func(activatedScene);

        mMetadata.ActiveSceneIndex = sceneindex;
    }

    void Project::Save()
    {
        Serializer::Serialize<ProjectMetadata>(mMetadata.ProjectMetadataPath, &mMetadata);
        for (auto& scene : mScenes)
            Serializer::Serialize<Scene>(scene->GetMetadata().ScenePath, scene.Raw());
    }

    void Project::Destroy()
    {
        mScenes.clear();
        mRuntimeSceneStorage.clear();
        mOnActiveSceneChangeCallbacks.clear();
        mIsValid = false;
        mMetadata = {};
    }

} // namespace Surge
