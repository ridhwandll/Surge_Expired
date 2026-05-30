// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Graphics/Camera/Camera.hpp"
#include <glm/glm.hpp>
#include <glm/ext/matrix_clip_space.hpp>

namespace Surge
{
    class RuntimeCamera : public Camera
    {
    public:
        enum class ProjectionType
        {
            Perspective = 0,
            Orthographic = 1
        };

    public:
        RuntimeCamera()
        {
            RecalculateProjection();
        }

        virtual ~RuntimeCamera() override = default;

        void SetPerspective(float verticalFOV, float nearClip, float farClip)
        {
            mProjectionType = ProjectionType::Perspective;
            mPerspectiveFOV = verticalFOV;
            mPerspectiveNear = nearClip;
            mPerspectiveFar = farClip;
            RecalculateProjection();
        }

        void SetOrthographic(float size, float nearClip, float farClip)
        {
            mProjectionType = ProjectionType::Orthographic;
            mOrthographicSize = size;
            mOrthographicNear = nearClip;
            mOrthographicFar = farClip;
            RecalculateProjection();
        }

        void SetViewportSize(float width, float height)
        {
            mAspectRatio = width / height;
            RecalculateProjection();
        }

        float GetAspectRatio() { return mAspectRatio; }

        //Perspective
        float GetPerspectiveVerticalFOV() const { return glm::degrees(mPerspectiveFOV); }
        void SetPerspectiveVerticalFOV(float verticalFov)
        {
            mPerspectiveFOV = glm::radians(verticalFov);
            RecalculateProjection();
        }
        float GetPerspectiveNearClip() const { return mPerspectiveNear; }
        void SetPerspectiveNearClip(float nearClip)
        {
            mPerspectiveNear = nearClip;
            RecalculateProjection();
        }
        float GetPerspectiveFarClip() const { return mPerspectiveFar; }
        void SetPerspectiveFarClip(float farClip)
        {
            mPerspectiveFar = farClip;
            RecalculateProjection();
        }

        //Orthographic
        float GetOrthographicSize() const { return mOrthographicSize; }
        void SetOrthographicSize(float size)
        {
            mOrthographicSize = size;
            RecalculateProjection();
        }
        float GetOrthographicNearClip() const { return mOrthographicNear; }
        void SetOrthographicNearClip(float nearClip)
        {
            mOrthographicNear = nearClip;
            RecalculateProjection();
        }
        float GetOrthographicFarClip() const { return mOrthographicFar; }
        void SetOrthographicFarClip(float farClip)
        {
            mOrthographicFar = farClip;
            RecalculateProjection();
        }

        ProjectionType GetProjectionType() const { return mProjectionType; }
        void SetProjectionType(ProjectionType type)
        {
            mProjectionType = type;
            RecalculateProjection();
        }

    private:
        void RecalculateProjection()
        {
            if (mProjectionType == ProjectionType::Perspective)
                mProjection = glm::perspective(mPerspectiveFOV, mAspectRatio, mPerspectiveNear, mPerspectiveFar);
            else
            {
                float orthoLeft = -mOrthographicSize * mAspectRatio * 0.5f;
                float orthoRight = mOrthographicSize * mAspectRatio * 0.5f;
                float orthoBottom = -mOrthographicSize * 0.5f;
                float orthoTop = mOrthographicSize * 0.5f;
                mProjection = glm::ortho(orthoLeft, orthoRight, orthoBottom, orthoTop, mOrthographicNear, mOrthographicFar);
            }
            mProjection[1][1] *= -1;
        }

    private:
        ProjectionType mProjectionType = ProjectionType::Perspective;

        float mPerspectiveFOV = glm::radians(45.0f);
        float mPerspectiveNear = 0.1f, mPerspectiveFar = 1024.0f;

        float mOrthographicSize = 10.0f;
        float mOrthographicNear = -1.0f, mOrthographicFar = 1.0f;
        float mAspectRatio = 0.0f;
    };

} // namespace Surge
