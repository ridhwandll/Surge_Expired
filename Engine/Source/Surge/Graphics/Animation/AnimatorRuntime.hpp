// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Vector.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/Animation/Skeleton.hpp"
#include <array>

namespace Surge
{
    class Asset;
    class Mesh;
    class Renderer;
    struct Frustum;
    struct AnimationClip;
    struct AnimatorComponent;

    // Live state of an AnimatorComponent: the playing clip (+ the one fading out), the pose buffers and the palette the mesh is drawn with.
    // Bound to the entity's mesh (and the component's clip sources) on the first update, sized once from the skeleton, rebound when they
    // change. A copy starts empty (entering Play mode, duplicating an entity), so poses are never shared between scenes
    class AnimatorRuntime
    {
    public:
        static constexpr Uint MAX_CLIP_SOURCES = 4;

        AnimatorRuntime();
        ~AnimatorRuntime();
        AnimatorRuntime(const AnimatorRuntime&);
        AnimatorRuntime& operator=(const AnimatorRuntime&);
        AnimatorRuntime(AnimatorRuntime&&) noexcept;
        AnimatorRuntime& operator=(AnimatorRuntime&&) noexcept;

        // Advances the clips by deltaTime (scaled scene time, 0 while paused) and evaluates the pose. running = false (Editor edit mode):
        // shows the component's clip at its current time, advancing only while previewing. frustum = null: never culled, otherwise an
        // animator outside it (last pose's bounds) only advances its time unless AnimateOffscreen
        void Update(const AnimatorComponent& component, const Ref<Mesh>& mesh, const glm::mat4& transform, float deltaTime, const Frustum* frustum, bool running);

        // Draws the mesh skinned with the current pose (after Update, same frame)
        void Submit(Renderer& renderer, const glm::mat4& transform, bool dropShadow);

        bool HasPose() const { return mHasPose; }
        bool IsCulled() const { return mCulled; }
        const Vector<SkinMatrix>& GetPalette() const { return mPalette; } // The current pose (valid with HasPose)
        const AABB& GetBounds() const { return mBounds; }                  // Model space bounds of the current pose

        // Playback. Play() before the first update is applied once the animator is bound
        bool Play(const String& clipName, float fadeDuration = 0.0f); // false: no clip with that name
        void Stop();                                                    // Back to the rest pose
        bool IsPlaying() const { return mCurrent.Clip >= 0 && !mFinished; }
        bool IsPlaying(const String& clipName) const;
        const String& GetClipName() const; // Empty when nothing plays
        float GetTime() const { return mCurrent.Time; }
        void SetTime(float time);
        float GetDuration() const; // Of the current clip
        float GetDuration(const String& clipName) const;
        void SetPaused(bool paused) { mPaused = paused; }
        bool IsPaused() const { return mPaused; }
        void SetPreview(bool preview) { mPreview = preview; } // Editor edit mode: advance time like the running scene would
        bool IsPreviewing() const { return mPreview; }

        // Every clip the animator can play: the mesh's own, then the clip sources'
        Uint GetClipCount() const { return static_cast<Uint>(mClips.size()); }
        const String& GetClipNameAt(Uint index) const;
        bool HasClip(const String& clipName) const { return FindClip(clipName) >= 0; }

        // Joints (skeleton nodes) for attaching things: look one up once, read its transform every frame. -1 until bound / not found
        int FindJoint(const String& name) const;
        Uint GetJointCount() const { return static_cast<Uint>(mWorld.size()); }
        int GetJointParent(int joint) const;
        glm::mat4 GetJointModelTransform(int joint) const; // Model space (the mesh's), identity for an invalid joint
        glm::mat4 GetJointWorldTransform(int joint) const; // The entity transform of the last update applied
        const Mesh* GetMesh() const { return mMesh.Raw(); }

    private:
        struct ClipEntry
        {
            const AnimationClip* Clip;
            int Remap; // Into mRemaps, -1 = cooked with this skeleton
        };

        struct PlayState
        {
            int Clip = -1; // Into mClips
            float Time = 0.0f;
        };

        bool IsBoundTo(const AnimatorComponent& component, const Ref<Mesh>& mesh) const;
        void Bind(const AnimatorComponent& component, const Ref<Mesh>& mesh);
        void Reset();
        int FindClip(const String& name) const;
        bool Advance(PlayState& state, float deltaTime, bool loop) const; // True when a non looping clip reached its end
        void Evaluate();

    private:
        Ref<Mesh> mMesh;
        std::array<Ref<Mesh>, MAX_CLIP_SOURCES> mSources;
        std::array<const Asset*, MAX_CLIP_SOURCES> mSourceAssets = {}; // What the component pointed to when bound
        Vector<ClipEntry> mClips;
        Vector<Vector<int>> mRemaps; // Clip source node -> this skeleton's node, matched by name

        PlayState mCurrent;
        PlayState mPrevious; // Fading out
        float mFadeTime = 0.0f;
        float mFadeDuration = 0.0f;
        String mDefaultClip;  // Component::Clip when the animator started (edit mode follows changes)
        String mPendingClip;  // Play() before the animator was bound
        float mPendingFade = 0.0f;
        bool mStarted = false;
        bool mFinished = false;
        bool mPaused = false;
        bool mPreview = false;
        bool mDirty = true;   // The pose must be evaluated again even without time passing

        Vector<JointPose> mPose;
        Vector<JointPose> mFadePose;
        Vector<glm::mat4> mWorld;
        Vector<SkinMatrix> mPalette;
        AABB mBounds;
        glm::mat4 mEntityTransform = glm::mat4(1.0f);
        bool mHasPose = false;
        bool mCulled = false;
    };

} // namespace Surge
