// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidWindow.hpp"
#include "Surge/Core/Core.hpp"
#include <android/log.h>
#include <imgui.h>

namespace Surge
{
	static AndroidWindow* sAndroidWindowInstance = nullptr;

	AndroidWindow::AndroidWindow(const WindowDesc& windowData)
	{
		mWindowData = windowData;
		sAndroidWindowInstance = this;

		android_app* app = Android::GAndroidApp;
		app->userData = this;
		app->onAppCmd = HandleAppCmd;

		// Pass nullptr to disable filtering and receive every axis
		android_app_set_motion_event_filter(app, nullptr);

		// Surface may already be available if the window was created before Init
		mNativeWindow = app->window;
		if (mNativeWindow)
		{
			mWindowData.Width = static_cast<Uint>(ANativeWindow_getWidth(mNativeWindow));
			mWindowData.Height = static_cast<Uint>(ANativeWindow_getHeight(mNativeWindow));
		}

		Log<Severity::Info>("AndroidWindow created ({}x{})", mWindowData.Width, mWindowData.Height);
	}

	void AndroidWindow::Update()
	{
		android_app* app = Android::GAndroidApp;

		// Drain lifecycle / system events (non-blocking)
		int events = 0;
		android_poll_source* source = nullptr;
		while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
		{
			if (source)
				source->process(app, source);
		}

		// Check destroy request (set by glue after APP_CMD_DESTROY)
		if (app->destroyRequested && !mDestroyFired)
		{
			mDestroyFired = true;
			WindowClosedEvent e;
			if (mEventCallback)
				mEventCallback(e);
			return;
		}

		// Swap GameActivity input buffers, atomic hand-off from the input thread
		android_input_buffer* inputBuffer = android_app_swap_input_buffers(app);
		if (!inputBuffer)
			return;

		// Motion events (touch, stylus, joystick)
		for (uint64_t i = 0; i < inputBuffer->motionEventsCount; ++i)
			HandleMotionEvent(this, &inputBuffer->motionEvents[i]);

		// Key events (back button, volume keys, gamepad, etc.)
		for (uint64_t i = 0; i < inputBuffer->keyEventsCount; ++i)
			HandleKeyEvent(this, &inputBuffer->keyEvents[i]);

		// Clear both buffers must be called after processing
		android_app_clear_motion_events(inputBuffer);
		android_app_clear_key_events(inputBuffer);
	}

	glm::vec2 AndroidWindow::GetSize() const
	{
		if (mNativeWindow)
			return { static_cast<float>(ANativeWindow_getWidth(mNativeWindow)), static_cast<float>(ANativeWindow_getHeight(mNativeWindow)) };

		return { static_cast<float>(mWindowData.Width), static_cast<float>(mWindowData.Height) };
	}

	void AndroidWindow::SetSize(const glm::vec2& size) const
	{
		(void)size; // ANativeWindow dimensions are display-controlled
	}

	// Lifecycle command callback (called on the main thread by the glue)
	void AndroidWindow::HandleAppCmd(android_app* app, int32_t cmd)
	{
		auto* self = static_cast<AndroidWindow*>(app->userData);
		if (!self)
			return;

		switch (cmd)
		{
		case APP_CMD_SAVE_STATE:
			Log<Severity::Info>("APP_CMD_SAVE_STATE");
			break;

		case APP_CMD_INIT_WINDOW:
		{
			self->mNativeWindow = app->window;
			self->mWindowState = WindowState::Normal;
			if (self->mNativeWindow)
			{
				Uint w = static_cast<Uint>(ANativeWindow_getWidth(self->mNativeWindow));
				Uint h = static_cast<Uint>(ANativeWindow_getHeight(self->mNativeWindow));
				self->mWindowData.Width = w;
				self->mWindowData.Height = h;

				WindowResizeEvent e(w, h);
				if (self->mEventCallback)
					self->mEventCallback(e);
			}
			Log<Severity::Info>("APP_CMD_INIT_WINDOW ({}x{})", self->mWindowData.Width, self->mWindowData.Height);
			break;
		}

		case APP_CMD_TERM_WINDOW:
			Log<Severity::Info>("APP_CMD_TERM_WINDOW");
			self->mWindowState = WindowState::Minimized;
			self->mNativeWindow = nullptr;
			break;

		case APP_CMD_GAINED_FOCUS:
			Log<Severity::Info>("APP_CMD_GAINED_FOCUS");
			self->mWindowState = WindowState::Normal;
			break;

		case APP_CMD_LOST_FOCUS:
			Log<Severity::Info>("APP_CMD_LOST_FOCUS");
			self->mWindowState = WindowState::Minimized;
			break;

		case APP_CMD_WINDOW_RESIZED:
		case APP_CMD_CONFIG_CHANGED:
		{
			Log<Severity::Info>("APP_CMD_CONFIG_CHANGED");
			if (app->window)
			{
				Uint w = static_cast<Uint>(ANativeWindow_getWidth(app->window));
				Uint h = static_cast<Uint>(ANativeWindow_getHeight(app->window));
				self->mWindowData.Width = w;
				self->mWindowData.Height = h;

				WindowResizeEvent e(w, h);
				if (self->mEventCallback)
					self->mEventCallback(e);
			}
			break;
		}

		case APP_CMD_DESTROY:
			Log<Severity::Info>("APP_CMD_DESTROY");
			// Actual event is fired via destroyRequested in Update()
			// to avoid double-firing. Nothing to do here.
			break;

		default:
			break;
		}
	}

	// Motion event handler (touch / multi-touch)
	void AndroidWindow::HandleMotionEvent(AndroidWindow* window, const GameActivityMotionEvent* event)
	{
		if (!window || !event || event->pointerCount == 0)
			return;

		const int32_t action = event->action;
		const int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;

		// For POINTER_DOWN / POINTER_UP the acting pointer index is encoded in the action
		uint32_t pointerIndex = 0;
		if (actionMasked == AMOTION_EVENT_ACTION_POINTER_DOWN || actionMasked == AMOTION_EVENT_ACTION_POINTER_UP)
		{
			pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
			if (pointerIndex >= event->pointerCount)
				pointerIndex = 0;
		}

		const GameActivityPointerAxes* pointer = &event->pointers[pointerIndex];
		const float x = GameActivityPointerAxes_getX(pointer);
		const float y = GameActivityPointerAxes_getY(pointer);

		window->mTouchX = x;
		window->mTouchY = y;

		// Feed ImGui directly, GameActivity has no AInputEvent*, so we drive
		// ImGui IO manually instead of calling ImGui_ImplAndroid_HandleInputEvent
		ImGuiIO& io = ImGui::GetIO();

		switch (actionMasked)
		{
		case AMOTION_EVENT_ACTION_DOWN:
		case AMOTION_EVENT_ACTION_POINTER_DOWN:
		{
			window->mTouchDown = true;
			io.AddMousePosEvent(x, y);
			io.AddMouseButtonEvent(0, true);

			MouseMovedEvent moveEvent(x, y);
			MouseButtonPressedEvent pressEvent(static_cast<MouseCode>(Mouse::ButtonLeft));
			if (window->mEventCallback)
			{
				window->mEventCallback(moveEvent);
				window->mEventCallback(pressEvent);
			}
			break;
		}

		case AMOTION_EVENT_ACTION_UP:
		case AMOTION_EVENT_ACTION_POINTER_UP:
		case AMOTION_EVENT_ACTION_CANCEL:
		{
			window->mTouchDown = false;
			io.AddMouseButtonEvent(0, false);

			MouseButtonReleasedEvent releaseEvent(static_cast<MouseCode>(Mouse::ButtonLeft));
			if (window->mEventCallback)
				window->mEventCallback(releaseEvent);
			break;
		}

		case AMOTION_EVENT_ACTION_MOVE:
		{
			// MOVE carries ALL active pointers, iterate every one
			for (Uint p = 0; p < event->pointerCount; ++p)
			{
				const float px = GameActivityPointerAxes_getX(&event->pointers[p]);
				const float py = GameActivityPointerAxes_getY(&event->pointers[p]);

				// Primary pointer drives ImGui and the window's tracked position
				if (p == 0)
				{
					window->mTouchX = px;
					window->mTouchY = py;
					io.AddMousePosEvent(px, py);
				}

				MouseMovedEvent moveEvent(px, py);
				if (window->mEventCallback)
					window->mEventCallback(moveEvent);
			}
			break;
		}
		case AMOTION_EVENT_ACTION_SCROLL:
		{
			// ALL active pointers, iterate every one
			for (Uint p = 0; p < event->pointerCount; ++p)
			{
				float xScroll = GameActivityPointerAxes_getAxisValue(&event->pointers[p], AMOTION_EVENT_AXIS_HSCROLL);
				float yScroll = GameActivityPointerAxes_getAxisValue(&event->pointers[p], AMOTION_EVENT_AXIS_VSCROLL);
				io.AddMouseWheelEvent(xScroll, yScroll);
			}
		}
		default:
			break;
		}
	}

	// Key event handler (back button, volume, gamepad buttons, etc.)
	void AndroidWindow::HandleKeyEvent(AndroidWindow* window, const GameActivityKeyEvent* event)
	{
		if (!window || !event)
			return;

		// Back button, treat as app close request on android
		if (event->keyCode == AKEYCODE_BACK && event->action == AKEY_EVENT_ACTION_UP)
		{
            //window->mWindowState = WindowState::Minimized;
            // TODO: Have a proper way to store and manage states in Android
            // Currently we destroy the window and be done with it
			WindowClosedEvent e;
			if (window->mEventCallback)
                window->mEventCallback(e);

			return;
		}

		// Extend here for gamepad / keyboard key binding
	}

	// Input polling helpers used by AndroidInput.cpp
	float Android::GetTouchX() { return sAndroidWindowInstance ? sAndroidWindowInstance->mTouchX : 0.0f; }
	float Android::GetTouchY() { return sAndroidWindowInstance ? sAndroidWindowInstance->mTouchY : 0.0f; }
	bool Android::IsTouchDown() { return sAndroidWindowInstance != nullptr && sAndroidWindowInstance->mTouchDown; }

} // namespace Surge

#endif // SURGE_PLATFORM_ANDROID