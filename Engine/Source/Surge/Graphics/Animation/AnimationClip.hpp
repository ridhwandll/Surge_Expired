// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/Animation/Skeleton.hpp"

namespace Surge
{
    enum class AnimationPath : uint8_t
    {
        TRANSLATION,
        ROTATION,
        SCALE
    };

    // One animated channel of one node. KeyCount is 1 (constant over the clip) or the clip's FrameCount
    struct AnimationTrack
    {
        uint16_t Node;      // Into the skeleton of the mesh the clip was cooked with
        AnimationPath Path;
        uint8_t _pad = 0;
        Uint FirstKey;      // Into the pool of Path
        Uint KeyCount;
    };

    // Curves resampled at SampleRate by the cooker (STEP/LINEAR/CUBICSPLINE evaluated there): sampling is two array reads and a
    // lerp/nlerp per track, no key search. Nodes without a track keep their rest pose
    struct AnimationClip
    {
        static constexpr float DEFAULT_SAMPLE_RATE = 30.0f;

        String Name;
        float Duration = 0.0f;
        float SampleRate = DEFAULT_SAMPLE_RATE;
        Uint FrameCount = 1;

        Vector<AnimationTrack> Tracks;
        Vector<glm::vec3> Translations;
        Vector<glm::vec4> Rotations; // Quaternions, (x, y, z, w)
        Vector<glm::vec3> Scales;

        // Overwrites the nodes this clip animates in pose (poseCount nodes). nodeRemap: clip node -> pose node, -1 = not in the
        // target skeleton, nullptr = the clip was cooked with this skeleton
        void Sample(float time, JointPose* pose, Uint poseCount, const int* nodeRemap) const;
    };

    // Joint by joint: translation/scale lerp, rotation nlerp along the shorter arc
    void BlendPoses(const JointPose* from, JointPose* inOutTo, Uint count, float weight);

} // namespace Surge
