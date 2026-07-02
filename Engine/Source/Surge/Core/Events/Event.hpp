// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/String.hpp"
#include "Surge/Core/Input/KeyCodes.hpp"
#include "Surge/Core/Input/MouseCodes.hpp"
#include <sstream>

namespace Surge
{
    enum class EventType
    {
        None = 0,
        WindowClose,
        WindowResize,
        WindowFocus,
        WindowLostFocus,
        WindowMoved,
        KeyPressed,
        KeyReleased,
        KeyTyped,
        MouseButtonPressed,
        MouseButtonReleased,
        MouseMoved,
        MouseScrolled
    };

    class Event
    {
    public:
        Event() = default;
        virtual ~Event() = default;
        virtual String ToString() const = 0;
        virtual const char* GetName() const = 0;
        virtual EventType GetEventType() const = 0;
    };

#define EVENT_CLASS_TYPE(type)                                                  \
    static EventType GetStaticType() { return EventType::type; }                \
    virtual EventType GetEventType() const override { return GetStaticType(); } \
    virtual const char* GetName() const override { return #type; }

    // Key Events
    class KeyEvent : public Event
    {
    public:
        Key GetKeyCode() const { return mKeyCode; }

    protected:
        KeyEvent(const Key keycode) : mKeyCode(keycode) {}

        Key mKeyCode;
    };

    class KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(const Key keycode, const uint16_t repeatCount) : KeyEvent(keycode), mRepeatCount(repeatCount) {}

        uint16_t GetRepeatCount() const { return mRepeatCount; }

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << (KeyCode)mKeyCode << " (" << mRepeatCount << " repeats)";
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyPressed);

    private:
        uint16_t mRepeatCount;
    };

    class KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(const KeyCode keycode) : KeyEvent((Key)keycode) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyReleasedEvent: " << (KeyCode)mKeyCode;
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyReleased);
    };

    class KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(const KeyCode keycode) : KeyEvent((Key)keycode) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyTypedEvent: " << (KeyCode)mKeyCode;
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyTyped);
    };

    // Mouse Events
    class MouseMovedEvent : public Event
    {
    public:
        MouseMovedEvent(const float x, const float y) : mMouseX(x), mMouseY(y) {}

        float GetX() const { return mMouseX; }
        float GetY() const { return mMouseY; }

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseMovedEvent: " << mMouseX << ", " << mMouseY;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseMoved);

    private:
        float mMouseX, mMouseY;
    };

    class MouseScrolledEvent : public Event
    {
    public:
        MouseScrolledEvent(const float delta) : mDelta(delta) {}

        float GetDelta() const { return mDelta; }

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseScrolledEvent: " << mDelta;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseScrolled);

    private:
        float mDelta;
    };

    class MouseButtonEvent : public Event
    {
    public:
        MouseCode GetMouseButton() const { return mButton; }

        float GetX() const { return mMouseX; }
        float GetY() const { return mMouseY; }

    protected:
        MouseButtonEvent(float mouseX, float mouseY, const MouseCode button) : mButton(button), mMouseX(mouseX), mMouseY(mouseY) {}

        MouseCode mButton;
        float mMouseX, mMouseY;
    };

    class MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(float mouseX, float mouseY, const MouseCode button)
            : MouseButtonEvent(mouseX, mouseY, button) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << mButton;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonPressed);
    };

    class MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(float mouseX, float mouseY, const MouseCode button)
            : MouseButtonEvent(mouseX, mouseY, button) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << mButton;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonReleased);
    };

    // App Events
    class WindowResizeEvent : public Event
    {
    public:
        WindowResizeEvent(Uint width, Uint height) : mWidth(width), mHeight(height) {}

        Uint GetWidth() const { return mWidth; }
        Uint GetHeight() const { return mHeight; }

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "WindowResizeEvent: " << mWidth << ", " << mHeight;
            return ss.str();
        }
        EVENT_CLASS_TYPE(WindowResize);

    private:
        Uint mWidth, mHeight;
    };

    class WindowClosedEvent : public Event
    {
    public:
        WindowClosedEvent() {}

        virtual String ToString() const override { return "WindowCloseEvent"; }
        EVENT_CLASS_TYPE(WindowClose);
    };

    class EventDispatcher
    {
    public:
        EventDispatcher(Event& event)
            : mEvent(event) {}

        template <typename T, typename F>
        void Dispatch(const F& func)
        {
            if (mEvent.GetEventType() == T::GetStaticType())
                func(static_cast<T&>(mEvent));
        }

    private:
        Event& mEvent;
    };

} // namespace Surge
