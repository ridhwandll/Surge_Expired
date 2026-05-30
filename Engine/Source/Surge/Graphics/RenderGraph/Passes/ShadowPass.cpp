// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RenderGraph/Passes/ShadowPass.hpp"

namespace Surge
{
    void ShadowPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Setup");
        mRHI = rhi;

        /*
        *  (Rid) Shadow map sizs for D16_UNORM (2 bytes per pixel)
        *  1024 * 0.5 = 0512 = 0.5 MB
        *  1024 * 1   = 1024 = 2   MB
        *  1024 * 2   = 2048 = 8   MB <- Our Default for mobile
        *  1024 * 3   = 3072 = 18  MB
        *  1024 * 4   = 4096 = 32  MB <- Our Default for PC
        * 
        *  Multiply these MB values by 2 to get size for D32_SFLOAT
        */

        mShadowMapResolution = 1024.0f * 2; // Default 2048
        glm::vec2 size = { mShadowMapResolution, mShadowMapResolution };

        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D16_UNORM;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT | ImageUsage::SAMPLED;
        depthDesc.GenerateImGuiID = true;
        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
        {
            depthDesc.DebugName = std::format("ShadowMap (Cascade: {})", i);
            blackBoard.ShadowMap[i] = mRHI->CreateImage(depthDesc);
        }

        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
        {
            FramebufferAttachment depthAttachment = {};
            depthAttachment.Handle = blackBoard.ShadowMap[i];
            depthAttachment.Load = LoadOp::CLEAR;
            depthAttachment.Store = StoreOp::STORE;

            FramebufferDesc fbDesc = {};
            fbDesc.ColorAttachmentCount = 0;
            fbDesc.DepthAttachment = depthAttachment;
            fbDesc.HasDepth = true;
            fbDesc.Width = size.x;
            fbDesc.Height = size.y;

            fbDesc.DebugName = std::format("ShadowPass Framebuffer (Cascade: {})", i);
            mShadowPassFramebuffer[i] = mRHI->CreateFramebuffer(fbDesc);
        }

        PipelineDesc desc = {};
        desc.Shader_ = Core::GetRenderer()->GetShaderManager().Get("Shadow.glsl");
        desc.Raster.Topo = Topology::TRIANGLE_LIST;
        desc.Raster.Polygon = PolygonMode::FILL;
        desc.Raster.Cull = CullMode::FRONT; //Cull front to reduce Peter panning

        desc.Raster.DepthBiasEnable = false;
        desc.Raster.DepthBiasConstantFactor = 1.25f;
        desc.Raster.DepthBiasSlopeFactor = 1.75f;
        desc.Raster.DepthBiasClamp = 0.0f;

        desc.Stencil.Enable = false;
        desc.Depth.TestEnable = true;
        desc.Depth.WriteEnable = true;
        desc.Depth.Op = CompareOp::LESS_OR_EQUAL;
        desc.Blend.Enable = false;
        
        desc.TargetSwapchain = false;
        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
        {
            desc.DebugName = std::format("ShadowPass (Cascade: {})", i);
            desc.TargetFramebuffer = mShadowPassFramebuffer[i];
            mShadowPipelines[i] = mRHI->CreatePipeline(desc);
        }

        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
            mImageWrites.push_back(blackBoard.ShadowMap[i]);
    }

    void ShadowPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Execute");
        if(!blackBoard.HasDirectionalLight)
            return;

        CalculateCascades(blackBoard);
        const Uint& cascadeCount = blackBoard.ShadowSettings_.CascadeCount;

        ShadowUBO shadowData = {};
        shadowData.CascadeCount = cascadeCount;
        shadowData.ShowCascades = blackBoard.ShadowSettings_.ShowCascades;
        for(Uint i = 0; i < shadowData.CascadeCount; i++)
        {
            shadowData.CascadeEnds[i] = mCascadeEnds[i];
            shadowData.LightSpaceMatrix[i] = mLightViewProjections[i];
        }
        mRHI->UploadBuffer(blackBoard.ShadowUBOs[ctx.FrameIndex], &shadowData, sizeof(ShadowUBO));

        for(Uint cascade = 0; cascade < cascadeCount; cascade++)
        {
            // ShadowPass manages its own renderpass unlike other passes
            mRHI->CmdBeginRenderPass(ctx, mShadowPassFramebuffer[cascade]);
            mRHI->CmdBindPipeline(ctx, mShadowPipelines[cascade]);

            for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
            {
                if(!cmd.DropShadow)
                    continue;

                const Mesh& mesh = *cmd.Mesh_;
                mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
                mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());

                const Submesh* submeshes = mesh.GetSubmeshes().data();
                for(Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
                {
                    const Submesh& submesh = submeshes[i];
                    glm::mat4 pushConstants[2] = { cmd.Transform * submesh.Transform, mLightViewProjections[cascade] };
                    mRHI->CmdPushConstants(ctx, mShadowPipelines[cascade], ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(glm::mat4) * 2, pushConstants);
                    mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
                }
            }
            mRHI->CmdEndRenderPass(ctx, mShadowPassFramebuffer[cascade]);
        }
    }

    void ShadowPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard) {}
    void ShadowPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
        ImFont* boldFont = ImGui::GetIO().Fonts->Fonts[1];
        ImGui::PushFont(boldFont, 25.0f);
        ImGui::TextUnformatted("Shadow Pass");
        ImGui::Separator();
        ImGui::PopFont();

        ImGui::Checkbox("Visualize Cascades", &blackBoard.ShadowSettings_.ShowCascades);
        ImGui::SliderInt("Cascade Count", &blackBoard.ShadowSettings_.CascadeCount, 2, MAX_SHADOW_CASCADE_COUNT);
        ImGui::SliderFloat("Cascade Split Lambda", &blackBoard.ShadowSettings_.CascadeSplitLambda, 0.0f, 1.0f, "%.3f");
        ImGui::Text("CascadeSplits: %f | %f | %f", mCascadeEnds[0], mCascadeEnds[1], mCascadeEnds[2]);
    }

    void ShadowPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Shutdown");

        for(Uint i = 0; i < MAX_SHADOW_CASCADE_COUNT; i++)
        {
            mRHI->DestroyFramebuffer(mShadowPassFramebuffer[i]);
            mRHI->DestroyImage(blackBoard.ShadowMap[i]);
            mRHI->DestroyPipeline(mShadowPipelines[i]);
        }
    }

    // (Rid)Visualization: https://gemini.google.com/share/13d9f8dfbf32
    void ShadowPass::CalculateCascades(const FrameBlackboard& bb)
    {
        SURGE_PROFILE_FUNC("ShadowPass::CalculateCascades");

        constexpr Uint NUM_FRUSTUM_CORNERS = 8;
        const int totalCascades = bb.ShadowSettings_.CascadeCount;
        const float cascadeSplitLambda = bb.ShadowSettings_.CascadeSplitLambda;

        const float minZ = bb.CameraNearFarPlane.x;
        const float maxZ = bb.CameraNearFarPlane.y;
        const float clipRange = maxZ - minZ;
        const float ratio = maxZ / minZ;

        // Calculate split intervals
        for(Uint i = 0; i < totalCascades; i++)
        {
            const float p = (i + 1.0f) / static_cast<float>(totalCascades);
            const float log = minZ * glm::pow(ratio, p);
            const float uniform = maxZ * p;
            const float d = cascadeSplitLambda * (log - uniform) + uniform;
            mCascadeSplits[i] = (d - minZ) / clipRange;
        }

        // Reconstruct master view frustum in world space
        glm::mat4 inverseViewProjection = glm::inverse(bb.ViewProjection);
        glm::vec4 frustumCorners[NUM_FRUSTUM_CORNERS] =
        {
            // Near Plane
            { 1.0f,  1.0f, 0.0f, 1.0f },
            {-1.0f,  1.0f, 0.0f, 1.0f },
            { 1.0f, -1.0f, 0.0f, 1.0f },
            {-1.0f, -1.0f, 0.0f, 1.0f },

            // Far Plane
            { 1.0f,  1.0f, 1.0f, 1.0f },
            {-1.0f,  1.0f, 1.0f, 1.0f },
            { 1.0f, -1.0f, 1.0f, 1.0f },
            {-1.0f, -1.0f, 1.0f, 1.0f },
        };

        for(glm::vec4& c : frustumCorners)
        {
            glm::vec4 inv = inverseViewProjection * c;
            c = inv / inv.w;
        }

        // Setup light orientation properties
        glm::vec3 lightDir = glm::normalize(bb.DirectionalLightDir);
        glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
        if(glm::abs(lightDir.y) > 0.99f)
            upVector = glm::vec3(0.0f, 0.0f, 1.0f);

        // Compute orthographic projections per cascade layer
        float lastSplitDist = 0.0f;
        for(Uint cascade = 0; cascade < totalCascades; cascade++)
        {
            float splitDist = mCascadeSplits[cascade];
            glm::vec4 sliceCorners[NUM_FRUSTUM_CORNERS]; //Temporary buffer to protect master frustum

            // Slice the frustum to this cascade's near/far range
            for(Uint i = 0; i < 4; i++)
            {
                glm::vec4 dist = frustumCorners[i + 4] - frustumCorners[i];
                sliceCorners[i + 4] = frustumCorners[i] + dist * splitDist;
                sliceCorners[i] = frustumCorners[i] + dist * lastSplitDist;
            }

            // Compute frustum center
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for(const glm::vec4& c : sliceCorners)
                frustumCenter += glm::vec3(c);
            frustumCenter /= static_cast<float>(NUM_FRUSTUM_CORNERS);

            // Compute bounding sphere radius
            float radius = 0.0f;
            for(const glm::vec4& c : sliceCorners)
                radius = glm::max(radius, glm::length(glm::vec3(c) - frustumCenter));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            constexpr float zPullback = 50.0f;
            float nearPlane = -zPullback;
            float farPlane = (maxExtents.z - minExtents.z) + zPullback;

            // View & Ortho Matrices
            glm::mat4 lightViewMatrix = glm::lookAt(
                frustumCenter - lightDir * maxExtents.z,
                frustumCenter,
                upVector
            );
            glm::mat4 lightProjectionMatrix = glm::ortho(
                minExtents.x, maxExtents.x,
                minExtents.y, maxExtents.y,
                nearPlane, farPlane
            );
            lightProjectionMatrix[1][1] *= -1; // Vulkan NDC Y flip

            // Texel snapping to eliminate shadow edge shimmer
            glm::mat4 shadowMatrix = lightProjectionMatrix * lightViewMatrix;
            glm::vec4 shadowOrigin = shadowMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin *= mShadowMapResolution / 2.0f;
            glm::vec4 roundOffset = glm::round(shadowOrigin) - shadowOrigin;
            roundOffset *= 2.0f / mShadowMapResolution;
            roundOffset.z = 0.0f;
            roundOffset.w = 0.0f;
            shadowMatrix[3] += roundOffset;

            // Store final parameters
            mCascadeEnds[cascade] = minZ + splitDist * clipRange;
            mLightViewProjections[cascade] = shadowMatrix;

            lastSplitDist = splitDist;
        }
    }
}
