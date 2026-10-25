// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"
#include "Surge/Graphics/RenderGraph/RenderGraph.hpp"
#include "Surge/Graphics/Shader/ShaderManager.hpp"
#include "Surge/Graphics/Renderer/Text.hpp"
#include "Surge/Graphics/Renderer/RenderView.hpp"
#include "Surge/Graphics/UISystem/UIManager.hpp"
#include "Surge/Graphics/HighLevel/Font.hpp"

namespace Surge
{
    class Window;
    struct ClientOptions;

    // Space for one emitter's particles in the frame's lists, see Renderer::SubmitParticles
    struct ParticleSubmission
    {
        ParticleRecord* Records = nullptr; // RecordCount entries to fill right away: the next SubmitParticles call may move them
        Uint EmitterIndex = 0;             // Goes into every record (ParticleRecord::VelocityZEmitter)
    };

    // Submissions of the last finished frame (main graph, captured before its lists are cleared), shown by the performance overlay
    struct RendererFrameCounts
    {
        Uint Meshes = 0;
        Uint Lights = 0;
        Uint Sprites = 0;
        Uint Texts = 0;
        Uint UIItems = 0; // UI sprites + text blocks
    };

    class Renderer
    {
    public:
        void Initialize(Window* window, const ClientOptions& clientOptions);
        void Shutdown();
        void OnEvent(Event& e);

        void BeginFrame(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar);
        void EndFrame();

        FORCEINLINE void SubmitQuad(const glm::mat4& transform, const glm::vec4& color, bool billboard = false, ImageHandle texture = ImageHandle::Invalid())
        {
            mMainRenderGraph.GetBlackboard().QuadList.emplace_back(QuadSubmitCmd { .Transform = transform, .Color = color, .Texture = texture, .Billboard = billboard });
        }
        FORCEINLINE void SubmitLine(const glm::vec3& point0, const glm::vec3& point1, const glm::vec4& color)
        {
            mMainRenderGraph.GetBlackboard().LineList.emplace_back(LineSubmitCmd { .P0 = point0, .P1 = point1, .Color = color, });
        }
        FORCEINLINE void SubmitText(const glm::mat4& transform, const String& txt, const glm::vec4& color, float maxWidth, float letterSpacing,
                        float lineSpacing, TextAlignment alignment, TextVerticalAlignment vAlignment, bool italic, bool underline,
                        bool enableShadow, const glm::vec2& shadowOffset, const glm::vec4& shadowColor, const Font* font, bool billboard = false)
        {
            mMainRenderGraph.GetBlackboard().TextList.emplace_back(TextSubmitCmd { .Transform = transform, .Text = txt, .Color = color, .MaxWidth = maxWidth, .LetterSpacing = letterSpacing,
                                                         .LineSpacing = lineSpacing, .Alignment = alignment, .VerticalAlignment = vAlignment, .Italic = italic, .Underline = underline, .Billboard = billboard,
                                                          .EnableShadow = enableShadow, .ShadowOffset = shadowOffset, .ShadowColor = shadowColor, .FontAsset = font });
        }
        FORCEINLINE void SubmitMesh(const glm::mat4& transform, const Mesh* mesh, bool dropShadow)
        {
            if(mesh->IsSkinned()) // Skinned vertex layout: without an animator it shows its rest pose
                SubmitSkinnedMesh(transform, mesh, dropShadow, mesh->GetRestPalette().data(), static_cast<Uint>(mesh->GetRestPalette().size()), mesh->GetRestBounds());
            else
                mMainRenderGraph.GetBlackboard().MeshList.emplace_back(MeshSubmitCmd { transform, mesh, dropShadow });
        }
        // A skinned mesh drawn with a palette (one SkinMatrix per palette entry of its skeleton) and the model space bounds of that pose.
        // Returns the palette's offset in the frame's skin matrices, INVALID_SKIN_OFFSET when the frame's budget (FrameBlackboard::Animation)
        // is used up: nothing is drawn then
        Uint SubmitSkinnedMesh(const glm::mat4& transform, const Mesh* mesh, bool dropShadow, const SkinMatrix* palette, Uint paletteSize, const AABB& bounds);
        FORCEINLINE void SubmitLight(const Light& light)
        {
            FrameBlackboard& bb = mMainRenderGraph.GetBlackboard();
            if(light.PositionType.w == (float)LightType::DIRECTIONAL)
            {
                bb.HasDirectionalLight = true;
                bb.DirectionalLightDir = glm::vec3(light.PositionType);
            }
            bb.LightList.emplace_back(light);
        }
        // Reserves 'count' records for one emitter (ParticleRuntime::Submit packs into them). view: an isolated Editor view (previews),
        // INVALID_RENDER_VIEW = the frame's scene. Records = null when nothing could be reserved
        ParticleSubmission SubmitParticles(const ParticleEmitterCmd& cmd, Uint count, RenderViewID view = INVALID_RENDER_VIEW);

        // Isolated Editor views only (previews), ignored for any other view. The scene path keeps the inline overloads above
        void SubmitMesh(const glm::mat4& transform, const Mesh* mesh, bool dropShadow, RenderViewID view);
        void SubmitLight(const Light& light, RenderViewID view);
        void SubmitEnvironment(const Environnment& env, RenderViewID view);
        FORCEINLINE void SubmitEnvironment(Environnment&& env)
        {
            env.HasEnvironment = true;
            mMainRenderGraph.GetBlackboard().Env = std::move(env);
        }

        void OnWindowResize(Uint width, Uint height);
        void OnSurfaceDestroyed() { mRHI->OnSurfaceDestroyed(); }
        void OnSurfaceCreated(Window* window) { mRHI->OnSurfaceCreated(window); }
        void ForceResize(Uint width, Uint height);

        // Scene render settings (RendererSettingsComponent), submitted every frame, written to the main blackboard only when they changed
        // Views copy them from there. The profile is the platform's, the Editor previews the other one by switching it
        void SubmitRenderSettings(const RenderSettings& settings);
        RenderProfile GetRenderProfile() const { return mRenderProfile; }
        void SetRenderProfile(RenderProfile profile) { mRenderProfile = profile; }

        // Scene resolution relative to the output (0.5 - 1.0), UI and post-processing stay at full resolution
        void SetRenderScale(float scale);
        float GetRenderScale() const { return mMainRenderGraph.GetBlackboard().RenderScale; }
        void ShowUI(bool show) { mMainRenderGraph.GetBlackboard().ShowUI = show; }

        const UI::Manager& GetUIManager() const { return mUIManager; }
        UI::Manager& GetUIManager() { return mUIManager; }

        const FrameBlackboard& GetRenderGraphBlackBoard() const { return mMainRenderGraph.GetBlackboard(); }
        FrameBlackboard& GetRenderGraphBlackBoard() { return mMainRenderGraph.GetBlackboard(); }

        ImageHandle GetWhiteTexture() const { return mMainRenderGraph.GetBlackboard().WhiteImage; }
        ImageHandle GetFinalImage() const { return mMainRenderGraph.GetBlackboard().FinalImage; }
        // Output resolution framebuffer (the main pass may be smaller with a render scale < 1)
        FramebufferHandle GetFinalFramebuffer() const
        {
            const FrameBlackboard& bb = mMainRenderGraph.GetBlackboard();
            return bb.PostProcessFramebuffer.IsNull() ? bb.MainPassFramebuffer : bb.PostProcessFramebuffer;
        }
        uint64_t GetFinalImageImGuiID() const
        {
            SG_ASSERT(!RHISettings::RENDER_TO_SWAPCHAIN, "Renderer is rendering to swapchain, cannot get Renderer's final image for ImGui rendering! Set ClientOptions::RenderFinalImageToSwapchain to false");
            return mRHI->GetImGuiImage(mMainRenderGraph.GetBlackboard().FinalImage);
        }

        ShaderManager& GetShaderManager() { return mShaderManager; }
        const RendererFrameCounts& GetLastFrameCounts() const { return mLastFrameCounts; }

        // Engine UI font (EngineFont::COOKED_PATH), loaded on first use. nullptr when the cooked file is missing (warned once)
        const Ref<Font>& GetEngineFont();

        SamplerHandle GetDefaultSampler() const { return mMainRenderGraph.GetBlackboard().DefaultSampler; }
        SamplerHandle GetTextSampler() const { return mMainRenderGraph.GetBlackboard().TextSampler; }

        const Scope<GraphicsRHI>& GetRHI() const { return mRHI; }
        Scope<GraphicsRHI>& GetRHI() { return mRHI; }

        void AddImGuiRenderCallback(std::function<void()>&& callback) { mMainRenderGraph.AddImGuiRenderCallback(std::move(callback)); }

        // Offscreen views (Editor Scene/Game viewports): each one draws the frame's submissions with its own camera, size and render targets.
        // They execute right before the main graph, whose ImGui pass shows their final images. Nothing creates one at runtime, the Player
        // (and the mobile path) stays a single graph
        RenderViewID CreateView(const RenderViewDesc& desc);
        void DestroyView(RenderViewID id);
        void ResizeView(RenderViewID id, Uint width, Uint height); // Applied at frame end, the view is not drawn before its first size
        void SetViewCamera(RenderViewID id, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, const glm::vec2& cameraNearFar);
        void RequestViewRender(RenderViewID id);                   // Every frame the view is on screen, draws it in the next EndFrame (ImGui runs after the views)
        uint64_t GetViewImGuiID(RenderViewID id) const;
        bool HasViewOutput(RenderViewID id) const; // The current targets were drawn at least once: the image can be shown (sampled)
        glm::uvec2 GetViewSize(RenderViewID id) const;
        void SetUIView(RenderViewID id) { mUIView = id; }         // The in-game UI is laid out at this view's size (main scene disabled)

        // Editor: the scene is only drawn through views, the main graph just runs the swapchain pass (ImGui)
        void SetMainSceneEnabled(bool enabled);
        // Submissions after this call (until EndFrame) are editor-only debug geometry, only views with EditorOverlay draw them
        void BeginEditorOverlay();

        // Debug tools (Editor Render Graph / Renderer panels). Settings live on the main graph's blackboard, views copy them every frame
        RenderGraph& GetMainRenderGraph() { return mMainRenderGraph; }
        RenderGraph* GetViewGraph(RenderViewID id) const; // nullptr once the view is destroyed
        template<typename Fn>
        void ForEachView(Fn&& fn) const // fn(RenderViewID, const RenderViewDesc&), live views in creation order
        {
            for(const Scope<RenderView>& view : mViews)
            {
                if(!view->Destroyed)
                    fn(view->ID, view->Desc);
            }
        }

    private:
        struct RenderView
        {
            RenderViewID ID = INVALID_RENDER_VIEW;
            RenderViewDesc Desc;
            Scope<RenderGraph> Graph;
            glm::uvec2 Size = { 0, 0 };          // Size of the render targets, 0 until the first resize landed
            glm::uvec2 RequestedSize = { 0, 0 };
            glm::mat4 ViewMatrix = glm::mat4(1.0f);
            glm::mat4 ProjectionMatrix = glm::mat4(1.0f);
            glm::vec2 NearFar = { 0.1f, 1000.0f };
            bool HasCamera = false;
            bool HasOutput = false; // Cleared when the targets are (re)created, they hold garbage in an undefined layout until drawn
            bool RenderRequested = false;
            bool Destroyed = false;
        };

        // Submissions before BeginEditorOverlay(), set = false: everything is scene content
        struct OverlayMark
        {
            size_t Meshes, Lights, Quads, Lines, Texts, ParticleEmitters, ParticleRecords;
            bool Set = false;
        };

        void AddScenePasses(RenderGraph& graph, bool withUI);
        RenderGraphContext MakeGraphContext(); // What every graph's passes get instead of reaching into Core
        RenderView* FindView(RenderViewID id) const;
        FrameBlackboard* FindIsolatedBlackboard(RenderViewID id) const; // nullptr unless it is a live isolated view
        void QueueViewResize(RenderViewID id, glm::uvec2 size);
        void ExecuteViews();
        void DestroyViewImmediate(RenderView& view);

        // Skin matrices: one storage buffer per frame in flight on every graph's blackboard, uploaded right before the graph executes
        void CreateSkinBuffers(FrameBlackboard& blackBoard, const String& debugName);
        void DestroySkinBuffers(FrameBlackboard& blackBoard);
        void UploadSkinMatrices(FrameBlackboard& blackBoard);

    private:
        FrameContext mCurrentFrameCtx;

        Vector<Scope<RenderView>> mViews;
        RenderViewID mNextViewID = 1;
        RenderViewID mUIView = INVALID_RENDER_VIEW;
        OverlayMark mOverlayMark;

        glm::uvec2 mScreenSize = { 0, 0 };
        RenderGraph mMainRenderGraph;

        RenderProfile mRenderProfile = PLATFORM_RENDER_PROFILE;
        RenderSettings mAppliedSettings;
        bool mHasAppliedSettings = false;

        UI::Manager mUIManager;
        ShaderManager mShaderManager;
        RendererFrameCounts mLastFrameCounts;
        Ref<Font> mEngineFont;
        bool mEngineFontLoaded = false;

        Scope<GraphicsRHI> mRHI;
        Window* mWindow = nullptr;
        bool mImGuiEnabled = false;
    };
} // namespace Surge