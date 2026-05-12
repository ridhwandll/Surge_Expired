// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once

namespace Surge
{
    class Editor;
    class Titlebar
    {
    public:
        Titlebar() = default;
        ~Titlebar() = default;

        void OnInit();
        void Render();
        float GetHeight() { return 60.0f; }

    private:
        Editor* mEditor;
    };
} // namespace Surge