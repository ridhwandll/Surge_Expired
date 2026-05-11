// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#ifdef SURGE_PLATFORM_ANDROID

#include "Surge/Core/Window/Window.hpp"
#include "Surge/Platform/Android/AndroidApp.hpp"

#include <android/native_window.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <game-activity/GameActivityEvents.h>

namespace Surge
{
    namespace Android
    {
        float GetTouchX();
        float GetTouchY();
        bool IsTouchDown();
    }

    class AndroidWindow : public Window
	{
	public:
		explicit AndroidWindow(const WindowDesc& windowData);
		virtual ~AndroidWindow() override = default;

		virtual void Update() override;
		virtual void Minimize() override {}
		virtual void Maximize() override {}
		virtual void RestoreFromMaximize() override {}
		virtual void RegisterEventCallback(std::function<void(Event&)> eventCallback) override { mEventCallback = eventCallback; }

		virtual bool IsWindowMaximized() const override { return false; }
		virtual bool IsWindowMinimized() const override { return mWindowState == WindowState::Minimized; }

		virtual String GetTitle() const override { return mWindowData.Title; }
		virtual void SetTitle(const String& name) override { mWindowData.Title = name; }

		virtual glm::vec2 GetPos() const override { return { 0.0f, 0.0f }; }
		virtual void SetPos(const glm::vec2&) const override {}

		virtual glm::vec2 GetSize() const override;
		virtual void SetSize(const glm::vec2& size) const override;

		virtual WindowState GetWindowState() const override { return mWindowState; }
		virtual void ShowConsole(bool) const override {}
		virtual void* GetNativeWindowHandle() override { return static_cast<void*>(mNativeWindow); }

	private:
		static void HandleAppCmd(android_app* app, int32_t cmd);
		static void HandleMotionEvent(AndroidWindow* window, const GameActivityMotionEvent* event);
		static void HandleKeyEvent(AndroidWindow* window, const GameActivityKeyEvent* event);

	private:
		std::function<void(Event&)> mEventCallback;
		ANativeWindow* mNativeWindow = nullptr;
		WindowState mWindowState = WindowState::Normal;
		bool mDestroyFired = false;

		// Touch state for input polling

		float mTouchX = 0.0f;
		float mTouchY = 0.0f;
		bool  mTouchDown = false;

		friend float Android::GetTouchX();
		friend float Android::GetTouchY();
		friend bool  Android::IsTouchDown();
	};

	// Accessors used by AndroidInput.cpp

} // namespace Surge

#endif // SURGE_PLATFORM_ANDROID