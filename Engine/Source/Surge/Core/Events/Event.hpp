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
        KeyCode GetKeyCode() const { return mKeyCode; }

    protected:
        KeyEvent(const KeyCode keycode) : mKeyCode(keycode) {}

        KeyCode mKeyCode;
    };

    class SURGE_API KeyPressedEvent : public KeyEvent
    {
    public:
        KeyPressedEvent(const KeyCode keycode, const uint16_t repeatCount) : KeyEvent(keycode), mRepeatCount(repeatCount) {}

        uint16_t GetRepeatCount() const { return mRepeatCount; }

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyPressedEvent: " << mKeyCode << " (" << mRepeatCount << " repeats)";
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyPressed);

    private:
        uint16_t mRepeatCount;
    };

    class SURGE_API KeyReleasedEvent : public KeyEvent
    {
    public:
        KeyReleasedEvent(const KeyCode keycode) : KeyEvent(keycode) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyReleasedEvent: " << mKeyCode;
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyReleased);
    };

    class SURGE_API KeyTypedEvent : public KeyEvent
    {
    public:
        KeyTypedEvent(const KeyCode keycode) : KeyEvent(keycode) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "KeyTypedEvent: " << mKeyCode;
            return ss.str();
        }
        EVENT_CLASS_TYPE(KeyTyped);
    };

    // Mouse Events
    class SURGE_API MouseMovedEvent : public Event
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

    class SURGE_API MouseScrolledEvent : public Event
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

    class SURGE_API MouseButtonEvent : public Event
    {
    public:
        MouseCode GetMouseButton() const { return mButton; }

    protected:
        MouseButtonEvent(const MouseCode button) : mButton(button) {}

        MouseCode mButton;
    };

    class SURGE_API MouseButtonPressedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonPressedEvent(const MouseCode button) : MouseButtonEvent(button) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonPressedEvent: " << mButton;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonPressed);
    };

    class SURGE_API MouseButtonReleasedEvent : public MouseButtonEvent
    {
    public:
        MouseButtonReleasedEvent(const MouseCode button) : MouseButtonEvent(button) {}

        virtual String ToString() const override
        {
            std::stringstream ss;
            ss << "MouseButtonReleasedEvent: " << mButton;
            return ss.str();
        }
        EVENT_CLASS_TYPE(MouseButtonReleased);
    };

    // App Events
    class SURGE_API WindowResizeEvent : public Event
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

    class SURGE_API WindowClosedEvent : public Event
    {
    public:
        WindowClosedEvent() {}

        virtual String ToString() const override { return "WindowCloseEvent"; }
        EVENT_CLASS_TYPE(WindowClose);
    };

    class SURGE_API EventDispatcher
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
