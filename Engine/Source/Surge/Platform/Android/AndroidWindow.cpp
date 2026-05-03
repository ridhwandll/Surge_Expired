// Copyright (c) - SurgeTechnologies - All rights reserved
#ifdef SURGE_PLATFORM_ANDROID
#include "Surge/Platform/Android/AndroidWindow.hpp"
#include "Surge/Core/Core.hpp"
#include <android/input.h>

namespace Surge
{
    // Single instance pointer – set on construction, used by input helpers
    static AndroidWindow* sAndroidWindowInstance = nullptr;

    AndroidWindow::AndroidWindow(const WindowDesc& windowData)
    {
        mWindowData = windowData;
        sAndroidWindowInstance = this;

        android_app* app = Android::GAndroidApp;
        app->userData = this;
        app->onAppCmd = HandleAppCmd;
        app->onInputEvent = HandleInputEvent;

        // The native window may already be available (window created before Init)
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

        int events = 0;
        android_poll_source* source = nullptr;

        // Non-blocking poll – drain all pending events
        while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
        {
            if (source != nullptr)
                source->process(app, source);

            if (app->destroyRequested)
            {
                AppClosedEvent e;
                if (mEventCallback)
                    mEventCallback(e);
                return;
            }
        }
    }

    glm::vec2 AndroidWindow::GetSize() const
    {
        if (mNativeWindow)
        {
            return {static_cast<float>(ANativeWindow_getWidth(mNativeWindow)),
                    static_cast<float>(ANativeWindow_getHeight(mNativeWindow))};
        }
        return {static_cast<float>(mWindowData.Width), static_cast<float>(mWindowData.Height)};
    }

    void AndroidWindow::SetSize(const glm::vec2& size) const
    {
        // ANativeWindow size is determined by the display; cannot be set programmatically
        (void)size;
    }

    // Static callback invoked by android_native_app_glue for lifecycle commands
    void AndroidWindow::HandleAppCmd(android_app* app, int32_t cmd)
    {
        AndroidWindow* self = static_cast<AndroidWindow*>(app->userData);
        if (!self)
            return;

        switch (cmd)
        {
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
                break;
            }
            case APP_CMD_TERM_WINDOW:
            {
                self->mWindowState = WindowState::Minimized;
                self->mNativeWindow = nullptr;
                break;
            }
            case APP_CMD_GAINED_FOCUS:
            {
                self->mWindowState = WindowState::Normal;
                break;
            }
            case APP_CMD_LOST_FOCUS:
            {
                self->mWindowState = WindowState::Minimized;
                break;
            }
            case APP_CMD_WINDOW_RESIZED:
            case APP_CMD_CONFIG_CHANGED:
            {
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
            {
                AppClosedEvent e;
                if (self->mEventCallback)
                    self->mEventCallback(e);
                break;
            }
            default:
                break;
        }
    }

    // Static callback for touch/key input events
    int32_t AndroidWindow::HandleInputEvent(android_app* app, AInputEvent* event)
    {
        AndroidWindow* self = static_cast<AndroidWindow*>(app->userData);
        if (!self)
            return 0;

        int32_t eventType = AInputEvent_getType(event);

        if (eventType == AINPUT_EVENT_TYPE_MOTION)
        {
            int32_t action = AMotionEvent_getAction(event) & AMOTION_EVENT_ACTION_MASK;
            float x = AMotionEvent_getX(event, 0);
            float y = AMotionEvent_getY(event, 0);

            self->mTouchX = x;
            self->mTouchY = y;

            switch (action)
            {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                {
                    self->mTouchDown = true;
                    MouseMovedEvent moveEvent(x, y);
                    if (self->mEventCallback)
                        self->mEventCallback(moveEvent);
                    MouseButtonPressedEvent pressEvent(static_cast<MouseCode>(Mouse::ButtonLeft));
                    if (self->mEventCallback)
                        self->mEventCallback(pressEvent);
                    return 1;
                }
                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP:
                case AMOTION_EVENT_ACTION_CANCEL:
                {
                    self->mTouchDown = false;
                    MouseButtonReleasedEvent releaseEvent(static_cast<MouseCode>(Mouse::ButtonLeft));
                    if (self->mEventCallback)
                        self->mEventCallback(releaseEvent);
                    return 1;
                }
                case AMOTION_EVENT_ACTION_MOVE:
                {
                    MouseMovedEvent moveEvent(x, y);
                    if (self->mEventCallback)
                        self->mEventCallback(moveEvent);
                    return 1;
                }
                default:
                    break;
            }
        }

        return 0;
    }

    // Input helper
    float AndroidGetTouchX()
    {
        return sAndroidWindowInstance ? sAndroidWindowInstance->mTouchX : 0.0f;
    }

    float AndroidGetTouchY()
    {
        return sAndroidWindowInstance ? sAndroidWindowInstance->mTouchY : 0.0f;
    }

    bool AndroidIsTouchDown()
    {
        return sAndroidWindowInstance ? sAndroidWindowInstance->mTouchDown : false;
    }

} // namespace Surge

#endif // SURGE_ANDROID