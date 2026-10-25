// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Panels/IPanel.hpp"

namespace Surge
{
    // Project physics settings (world, layers, collision matrix) + live simulation stats.
    // Edits apply to the simulation immediately and are saved to the .surgeproj once the interaction ends
    class PhysicsPanel : public IPanel
    {
    public:
        PhysicsPanel() = default;
        virtual ~PhysicsPanel() override = default;

        virtual void Init(void* panelInitArgs) override;
        virtual void OnEvent([[maybe_unused]] Event& e) override {}
        virtual void Render(bool* show) override;
        virtual void Shutdown() override {}

        static PanelCode GetStaticCode() { return PanelCode::Physics; }

    private:
        bool DrawWorld();
        bool DrawLayers();
        bool DrawCollisionMatrix();
        void DrawStats();

    private:
        PanelCode mCode;
        bool mPendingSave = false;
        bool mPainting = false;
        bool mPaintValue = false; // Collision matrix drag: the state the first clicked cell got, applied to every cell dragged over
    };

} // namespace Surge
