// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Events/Event.hpp"
#include <functional>
#include <glm/glm.hpp>

namespace Surge
{
    enum class SURGE_API WindowFlags : Uint
    {
        // TODO: Add more flags
        Minimized = BIT(1),
        Maximized = BIT(2),
        CreateDefault = BIT(3),
        NoTitlebar = BIT(4)
    };
    MAKE_BIT_ENUM(WindowFlags, Uint);

    enum class SURGE_API WindowState
    {
        Normal = 0,
        Minimized
    };

    struct WindowDesc
    {
        WindowDesc(Uint width, Uint height, const String& title, WindowFlags flags = WindowFlags::CreateDefault) : Width(width), Height(height), Title(title), Flags(flags) {}
        WindowDesc() : Width(1280), Height(720), Title("Surge Window"), Flags(WindowFlags::CreateDefault) {}
        ~WindowDesc() = default;

        Uint Width;
        Uint Height;
        String Title;
        WindowFlags Flags;
    };

    class SURGE_API Window
    {
    public:
        virtual ~Window() = default;

        virtual void Update() = 0;
        virtual void Minimize() = 0;
        virtual void Maximize() = 0;
        virtual void RestoreFromMaximize() = 0;
        virtual void RegisterEventCallback(std::function<void(Event&)> eventCallback) = 0;

        virtual bool IsWindowMaximized() const = 0;
        virtual bool IsWindowMinimized() const = 0;

        // Get/Set the title Window(in pixels)
        virtual String GetTitle() const = 0;
        virtual void SetTitle(const String& name) = 0;

        // Get/Set the position of the Window(in pixels)
        virtual glm::vec2 GetPos() const = 0;
        virtual void SetPos(const glm::vec2& pos) const = 0;

        // Get/Set the size of the Window(in pixels)
        virtual glm::vec2 GetSize() const = 0;
        virtual void SetSize(const glm::vec2& size) const = 0;

        virtual WindowState GetWindowState() const = 0;
        virtual void ShowConsole(bool show) const = 0;
        virtual void* GetNativeWindowHandle() = 0;

        const WindowDesc& GetData() const { return mWindowData; }

    protected:
        WindowDesc mWindowData;
    };
} // namespace Surge