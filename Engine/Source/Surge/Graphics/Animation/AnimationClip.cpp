// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Animation/AnimationClip.hpp"
#include <algorithm>

namespace Surge
{
    static glm::vec4 Nlerp(const glm::vec4& a, glm::vec4 b, float t)
    {
        if(glm::dot(a, b) < 0.0f)
            b = -b; // q and -q are the same rotation, take the shorter way
        const glm::vec4 q = a + (b - a) * t;
        const float lengthSquared = glm::dot(q, q);
        return lengthSquared > 1e-12f ? q * (1.0f / glm::sqrt(lengthSquared)) : a;
    }

    void AnimationClip::Sample(float time, JointPose* pose, Uint poseCount, const int* nodeRemap) const
    {
        const float frame = glm::clamp(time, 0.0f, Duration) * SampleRate;
        const Uint lastFrame = FrameCount > 0 ? FrameCount - 1 : 0;
        const Uint frame0 = std::min(static_cast<Uint>(frame), lastFrame);
        const Uint frame1 = std::min(frame0 + 1, lastFrame);
        const float t = glm::clamp(frame - static_cast<float>(frame0), 0.0f, 1.0f);

        for(const AnimationTrack& track : Tracks)
        {
            const int node = nodeRemap ? nodeRemap[track.Node] : static_cast<int>(track.Node);
            if(node < 0 || static_cast<Uint>(node) >= poseCount)
                continue;

            const Uint key0 = track.FirstKey + (track.KeyCount > 1 ? frame0 : 0);
            const Uint key1 = track.FirstKey + (track.KeyCount > 1 ? frame1 : 0);
            JointPose& target = pose[node];
            switch(track.Path)
            {
                case AnimationPath::TRANSLATION:
                    target.Translation = glm::mix(Translations[key0], Translations[key1], t);
                    break;
                case AnimationPath::ROTATION:
                    target.Rotation = Nlerp(Rotations[key0], Rotations[key1], t);
                    break;
                case AnimationPath::SCALE:
                    target.Scale = glm::mix(Scales[key0], Scales[key1], t);
                    break;
            }
        }
    }

    void BlendPoses(const JointPose* from, JointPose* inOutTo, Uint count, float weight)
    {
        for(Uint i = 0; i < count; i++)
        {
            JointPose& to = inOutTo[i];
            to.Translation = glm::mix(from[i].Translation, to.Translation, weight);
            to.Rotation = Nlerp(from[i].Rotation, to.Rotation, weight);
            to.Scale = glm::mix(from[i].Scale, to.Scale, weight);
        }
    }

} // namespace Surge
