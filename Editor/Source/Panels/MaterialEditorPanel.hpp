// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"
#include "Surge/Graphics/HighLevel/Material.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    class MaterialEditorPanel : public IPanel
    {
    public:
        MaterialEditorPanel() = default;
        virtual ~MaterialEditorPanel() override = default;

        virtual void Init([[maybe_unused]] void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {};
        virtual void Render(bool* show) override;
        virtual void Shutdown() override;

        void SetSelectedMaterial(const Ref<Material>& material) { mSelectedMaterial = material; }
    public:
        static PanelCode GetStaticCode() { return PanelCode::MaterialEditor; }

    private:
        PanelCode mCode;
        Ref<Material> mSelectedMaterial;
    };
} // namespace Surge