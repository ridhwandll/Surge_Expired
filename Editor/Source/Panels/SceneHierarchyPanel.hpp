// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/ECS/Scene.hpp"
#include "Utility/ImGUIAux.hpp"

namespace Surge
{
    class SceneHierarchyPanel : public IPanel
    {
    public:
        SceneHierarchyPanel() = default;
        ~SceneHierarchyPanel() = default;

        virtual void Init(void* panelInitArgs);
        virtual void OnEvent(Event& e) override {};
        virtual void Render(bool* show);
        virtual void Shutdown();

        static PanelCode GetStaticCode() { return PanelCode::SceneHierarchy; }

        void SetSceneContext(Scene* scene)
        {
            SG_ASSERT_NOMSG(scene);
            mSelectedEntity = {};
            mSceneContext = scene;
        }
        Scene* GetSceneContext() { return mSceneContext; }

        void SetSelectedEntity(Entity& e) { mSelectedEntity = e; }
        Entity& GetSelectedEntity() { return mSelectedEntity; }

    private:
        void DrawEntityNode(Entity& e);

    private:
        PanelCode mCode;
        Scene* mSceneContext;
        Entity mSelectedEntity; // TODO: Make It a vector when we allow multiple selection
    };

} // namespace Surge