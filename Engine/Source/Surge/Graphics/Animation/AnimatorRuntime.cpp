// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Animation/AnimatorRuntime.hpp"
#include "Surge/Core/Profiler.hpp"
#include "Surge/ECS/Components/Components.hpp"
#include "Surge/Graphics/HighLevel/Mesh.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "SurgeMath/Frustum.hpp"
#include <algorithm>
#include <cmath>

namespace Surge
{
    static const String sEmptyName;

    static const Ref<Asset>& GetClipSource(const AnimatorComponent& component, Uint index)
    {
        switch(index)
        {
            case 0:  return component.ClipSource1;
            case 1:  return component.ClipSource2;
            case 2:  return component.ClipSource3;
            default: return component.ClipSource4;
        }
    }

    AnimatorRuntime::AnimatorRuntime() = default;
    AnimatorRuntime::~AnimatorRuntime() = default;
    AnimatorRuntime::AnimatorRuntime(const AnimatorRuntime&) {}
    AnimatorRuntime::AnimatorRuntime(AnimatorRuntime&&) noexcept = default;
    AnimatorRuntime& AnimatorRuntime::operator=(AnimatorRuntime&&) noexcept = default;

    AnimatorRuntime& AnimatorRuntime::operator=(const AnimatorRuntime& other)
    {
        if(this != &other)
            *this = AnimatorRuntime(); // A copy starts empty, like the copy constructor
        return *this;
    }

    void AnimatorRuntime::Reset()
    {
        *this = AnimatorRuntime();
    }

    bool AnimatorRuntime::IsBoundTo(const AnimatorComponent& component, const Ref<Mesh>& mesh) const
    {
        if(mMesh.Raw() != mesh.Raw())
            return false;
        for(Uint i = 0; i < MAX_CLIP_SOURCES; i++)
        {
            if(mSourceAssets[i] != GetClipSource(component, i).Raw())
                return false;
        }
        return true;
    }

    void AnimatorRuntime::Bind(const AnimatorComponent& component, const Ref<Mesh>& mesh)
    {
        // A new mesh or new clip sources: playback starts over (a pending Play() survives)
        const String pendingClip = std::move(mPendingClip);
        const float pendingFade = mPendingFade;
        const bool paused = mPaused;
        const bool preview = mPreview;
        Reset();
        mPendingClip = pendingClip;
        mPendingFade = pendingFade;
        mPaused = paused;
        mPreview = preview;
        mStarted = !mPendingClip.empty();

        mMesh = mesh;
        const Skeleton& skeleton = mesh->GetSkeleton();
        for(const AnimationClip& clip : mesh->GetClips())
            mClips.push_back({ &clip, -1 });

        // Clips of other meshes (Mixamo style animation files), retargeted by node name. The remap is built once here
        for(Uint i = 0; i < MAX_CLIP_SOURCES; i++)
        {
            const Ref<Asset>& asset = GetClipSource(component, i);
            mSourceAssets[i] = asset.Raw();
            if(!asset || asset->GetAssetType() != AssetType::MESH || asset.Raw() == static_cast<const Asset*>(mesh.Raw()))
                continue;

            Ref<Mesh> source = asset.As<Mesh>();
            if(!source->IsSkinned() || source->GetClips().empty())
                continue;

            const Skeleton& sourceSkeleton = source->GetSkeleton();
            Vector<int> remap(sourceSkeleton.GetNodeCount(), -1);
            Uint matched = 0;
            for(Uint node = 0; node < sourceSkeleton.GetNodeCount(); node++)
            {
                remap[node] = skeleton.FindNode(sourceSkeleton.NodeNames[node]);
                if(remap[node] >= 0)
                    matched++;
            }
            if(matched == 0)
            {
                Log<Severity::Warn>("[Animator] Clip source {} shares no joint name with the mesh, its clips are skipped", i + 1);
                continue;
            }

            mRemaps.push_back(std::move(remap));
            for(const AnimationClip& clip : source->GetClips())
                mClips.push_back({ &clip, static_cast<int>(mRemaps.size() - 1) });
            mSources[i] = std::move(source);
        }

        const Uint nodeCount = skeleton.GetNodeCount();
        mPose.resize(nodeCount);
        mFadePose.resize(nodeCount);
        mWorld.resize(nodeCount);
        mPalette.resize(skeleton.GetPaletteSize());
        mDirty = true;
    }

    int AnimatorRuntime::FindClip(const String& name) const
    {
        for(Uint i = 0; i < static_cast<Uint>(mClips.size()); i++)
        {
            if(mClips[i].Clip->Name == name)
                return static_cast<int>(i);
        }
        return -1;
    }

    bool AnimatorRuntime::Advance(PlayState& state, float deltaTime, bool loop) const
    {
        if(state.Clip < 0)
            return false;

        const float duration = mClips[state.Clip].Clip->Duration;
        if(duration <= 0.0f)
        {
            state.Time = 0.0f;
            return !loop;
        }

        state.Time += deltaTime;
        if(loop)
        {
            state.Time = std::fmod(state.Time, duration);
            if(state.Time < 0.0f)
                state.Time += duration; // Negative speed plays backwards
            return false;
        }

        const bool finished = state.Time >= duration || state.Time <= 0.0f;
        state.Time = glm::clamp(state.Time, 0.0f, duration);
        return finished && deltaTime != 0.0f;
    }

    void AnimatorRuntime::Update(const AnimatorComponent& component, const Ref<Mesh>& mesh, const glm::mat4& transform, float deltaTime, const Frustum* frustum, bool running)
    {
        SURGE_PROFILE_FUNC("AnimatorRuntime::Update");
        mEntityTransform = transform;

        if(!mesh || !mesh->IsSkinned())
        {
            if(mMesh)
                Reset();
            return;
        }

        if(!IsBoundTo(component, mesh))
            Bind(component, mesh);

        if(!mPendingClip.empty())
        {
            const String clip = std::move(mPendingClip);
            mPendingClip.clear();
            Play(clip, 0.0f);
        }

        // The component's clip: edit mode always shows it (and follows Inspector changes), the running scene plays it when PlayOnAwake
        if(!mStarted || (!running && component.Clip != mDefaultClip))
        {
            mStarted = true;
            mDefaultClip = component.Clip;
            mPrevious = {};
            mCurrent = {};
            mFinished = false;
            mDirty = true;
            if(!running || component.PlayOnAwake)
                mCurrent.Clip = FindClip(component.Clip);
        }

        float advance = deltaTime * component.Speed;
        if(mPaused || (!running && !mPreview))
            advance = 0.0f;

        if(advance != 0.0f)
        {
            if(Advance(mCurrent, advance, component.Loop))
                mFinished = true;
            if(mPrevious.Clip >= 0)
            {
                Advance(mPrevious, advance, component.Loop);
                mFadeTime += glm::abs(advance);
                if(mFadeTime >= mFadeDuration)
                    mPrevious = {};
            }
            mDirty = true;
        }

        // Off screen (last pose's bounds): only the time moves, the last palette keeps being drawn (shadows)
        mCulled = frustum && !component.AnimateOffscreen && mHasPose && !frustum->IntersectsAABB(mBounds, transform);
        if(mCulled || (!mDirty && mHasPose))
            return;

        Evaluate();
    }

    void AnimatorRuntime::Evaluate()
    {
        const Skeleton& skeleton = mMesh->GetSkeleton();
        const Uint nodeCount = skeleton.GetNodeCount();

        std::copy(skeleton.RestPose.begin(), skeleton.RestPose.end(), mPose.begin());
        if(mCurrent.Clip >= 0)
        {
            const ClipEntry& entry = mClips[mCurrent.Clip];
            entry.Clip->Sample(mCurrent.Time, mPose.data(), nodeCount, entry.Remap >= 0 ? mRemaps[entry.Remap].data() : nullptr);
        }

        if(mPrevious.Clip >= 0 && mFadeDuration > 0.0f)
        {
            const ClipEntry& entry = mClips[mPrevious.Clip];
            std::copy(skeleton.RestPose.begin(), skeleton.RestPose.end(), mFadePose.begin());
            entry.Clip->Sample(mPrevious.Time, mFadePose.data(), nodeCount, entry.Remap >= 0 ? mRemaps[entry.Remap].data() : nullptr);
            BlendPoses(mFadePose.data(), mPose.data(), nodeCount, glm::clamp(mFadeTime / mFadeDuration, 0.0f, 1.0f));
        }

        skeleton.BuildPalette(mPose.data(), mWorld.data(), mPalette.data());
        mBounds = skeleton.ComputeBounds(mPalette.data());
        mHasPose = true;
        mDirty = false;
    }

    void AnimatorRuntime::Submit(Renderer& renderer, const glm::mat4& transform, bool dropShadow)
    {
        if(!mHasPose || !mMesh)
            return;
        renderer.SubmitSkinnedMesh(transform, mMesh.Raw(), dropShadow, mPalette.data(), static_cast<Uint>(mPalette.size()), mBounds);
    }

    bool AnimatorRuntime::Play(const String& clipName, float fadeDuration)
    {
        if(!mMesh)
        {
            // Not bound yet (OnCreate runs before the first animation update)
            mPendingClip = clipName;
            mPendingFade = fadeDuration;
            mStarted = true;
            return true;
        }

        const int clip = FindClip(clipName);
        if(clip < 0)
        {
            Log<Severity::Warn>("[Animator] No clip named '{}' on this animator", clipName);
            return false;
        }

        mStarted = true;
        if(clip == mCurrent.Clip && !mFinished)
            return true; // Already playing it

        if(fadeDuration > 0.0f && mCurrent.Clip >= 0 && mHasPose)
        {
            mPrevious = mCurrent;
            mFadeTime = 0.0f;
            mFadeDuration = fadeDuration;
        }
        else
            mPrevious = {};

        mCurrent = { clip, 0.0f };
        mFinished = false;
        mDirty = true;
        return true;
    }

    void AnimatorRuntime::Stop()
    {
        mPendingClip.clear();
        mCurrent = {};
        mPrevious = {};
        mFinished = false;
        mStarted = true; // Stays in the rest pose instead of starting the component's clip again
        mDirty = true;
    }

    bool AnimatorRuntime::IsPlaying(const String& clipName) const
    {
        return IsPlaying() && mClips[mCurrent.Clip].Clip->Name == clipName;
    }

    const String& AnimatorRuntime::GetClipName() const
    {
        return mCurrent.Clip >= 0 ? mClips[mCurrent.Clip].Clip->Name : sEmptyName;
    }

    void AnimatorRuntime::SetTime(float time)
    {
        if(mCurrent.Clip < 0)
            return;
        mCurrent.Time = glm::clamp(time, 0.0f, mClips[mCurrent.Clip].Clip->Duration);
        mFinished = false;
        mDirty = true;
    }

    float AnimatorRuntime::GetDuration() const
    {
        return mCurrent.Clip >= 0 ? mClips[mCurrent.Clip].Clip->Duration : 0.0f;
    }

    float AnimatorRuntime::GetDuration(const String& clipName) const
    {
        const int clip = FindClip(clipName);
        return clip >= 0 ? mClips[clip].Clip->Duration : 0.0f;
    }

    const String& AnimatorRuntime::GetClipNameAt(Uint index) const
    {
        return index < mClips.size() ? mClips[index].Clip->Name : sEmptyName;
    }

    int AnimatorRuntime::FindJoint(const String& name) const
    {
        return mMesh ? mMesh->GetSkeleton().FindNode(name) : -1;
    }

    int AnimatorRuntime::GetJointParent(int joint) const
    {
        return mMesh && joint >= 0 && static_cast<Uint>(joint) < GetJointCount() ? mMesh->GetSkeleton().Parents[joint] : -1;
    }

    glm::mat4 AnimatorRuntime::GetJointModelTransform(int joint) const
    {
        if(!mHasPose || joint < 0 || static_cast<Uint>(joint) >= GetJointCount())
            return glm::mat4(1.0f);
        return mWorld[joint];
    }

    glm::mat4 AnimatorRuntime::GetJointWorldTransform(int joint) const
    {
        return mEntityTransform * GetJointModelTransform(joint);
    }

} // namespace Surge
