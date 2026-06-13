// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Window/Window.hpp"
#include <wtypes.h>

namespace Surge
{
    class WindowsWindow : public Window
    {
    public:
        WindowsWindow(const WindowDesc& windowData);
        virtual ~WindowsWindow() override;

        virtual void Update() override;
        virtual void Minimize() override;
        virtual void Maximize() override;
        virtual void RestoreFromMaximize() override;
        virtual void RegisterEventCallback(std::function<void(Event&)> eventCallback) override { mEventCallback = eventCallback; }
        virtual bool IsWindowMaximized() const override;
        virtual bool IsWindowMinimized() const override;

        virtual String GetTitle() const override { return mWindowData.Title; }
        virtual void SetTitle(const String& name) override;

        virtual glm::vec2 GetPos() const override;
        virtual void SetPos(const glm::vec2& pos) const override;

        virtual glm::vec2 GetSize() const override;
        virtual void SetSize(const glm::vec2& size) const override;

        virtual WindowState GetWindowState() const override { return mWindowState; }
        virtual void ShowConsole(bool show) const override;
        virtual void* GetNativeWindowHandle() override { return mWin32Window; }

    private:
        void ApplyFlags();
        static LRESULT CALLBACK WindowProcWithImgui(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
        static LRESULT CALLBACK WindowProcWithoutImGui(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    private:
        std::function<void(Event&)> mEventCallback;
        WindowState mWindowState;
        HWND mWin32Window;
    };
} // namespace Surge