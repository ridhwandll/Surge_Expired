// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Camera/Camera.hpp"
#include "Surge/Core/Events/Event.hpp"

namespace Surge
{
    // 
    // TODO MOVE THIS FILE IN Editor Project, this should not be used in runtime code, only editor code
    // 

    class EditorCamera : public Camera
    {
    public:
        EditorCamera() = default;
        EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);

        void Focus(const glm::vec3& focusPoint);
        void OnUpdate();
        void OnEvent(Event& e);

        bool IsActive() const { return mIsActive; }
        void SetActive(bool active) { mIsActive = active; }

        float GetDistance() const { return mDistance; }
        void SetDistance(float distance) { mDistance = distance; }

        void SetViewportSize(const glm::vec2& size)
        {
            mViewportWidth = size.x;
            mViewportHeight = size.y;
            UpdateProjection();
        }
        glm::vec2 GetViewportSize() const { return glm::vec2(mViewportWidth, mViewportHeight); }

        glm::mat4 GetViewMatrix() const { return mViewMatrix; }
        glm::mat4& GetViewMatrix() { return mViewMatrix; }
        glm::mat4 GetViewProjection() const { return mProjection * mViewMatrix; }
        glm::vec3 GetUpDirection() const;
        glm::vec3 GetRightDirection() const;
        glm::vec3 GetForwardDirection() const;
        glm::quat GetOrientation() const;

        const glm::vec3& GetFocalPoint() const { return mFocalPoint; }
        const glm::vec3& GetPosition() const { return mPosition; }

        float GetPitch() const { return mPitch; }
        float GetYaw() const { return mYaw; }
        float GetCameraSpeed() const { return mSpeed; }
        glm::vec2 GetNearAndFarPlane() const { return { mNearClip, mFarClip }; };

    private:
        void UpdateCameraView();
        void UpdateProjection();

        bool OnMouseScroll(MouseScrolledEvent& e);
        bool OnKeyPressed(KeyPressedEvent& e);
        bool OnKeyReleased(KeyReleasedEvent& e);

        void MousePan(const glm::vec2& delta);
        void MouseRotate(const glm::vec2& delta);
        void MouseZoom(float delta);

        glm::vec3 CalculatePosition() const;
        Pair<float, float> PanSpeed() const;
        float RotationSpeed() const;
        float ZoomSpeed() const;

    private:
        bool mIsActive = false;

        glm::mat4 mViewMatrix;
        glm::vec3 mPosition, mWorldRotation, mFocalPoint;
        glm::vec2 mInitialMousePosition {};
        glm::vec3 mPositionDelta {};
        glm::vec3 mRightDirection {};

        float mDistance;
        float mSpeed = 0.003f;
        float mLastSpeed = 0.0f;
        float mPitch, mYaw;
        float mPitchDelta = 0, mYawDelta = 0;
        float mMinFocusDistance = 100.0f;
        float mViewportWidth = 1280.0f, mViewportHeight = 720.0f;
        float mFOV = 45.0f, mAspectRatio = 1.778f, mNearClip = 0.1f, mFarClip = 1000.0f;
        CameraMode mCameraMode {CameraMode::Arcball};
    };

} // namespace Surge
