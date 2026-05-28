// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Core/Core.hpp"
#include "Surge/Graphics/RenderGraph/Passes/ShadowPass.hpp"
#include <glm/gtc/type_ptr.hpp>

#define NUM_FRUSTUM_CORNERS 8

namespace Surge
{
    void ShadowPass::Setup(GraphicsRHI* rhi, FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Setup");
        mRHI = rhi;

        mShadowMapResolution = 1024.0f * 2;
        glm::vec2 size = { mShadowMapResolution, mShadowMapResolution };

        ImageDesc depthDesc = {};
        depthDesc.Width = size.x;
        depthDesc.Height = size.y;
        depthDesc.Format = ImageFormat::D16_UNORM;
        depthDesc.Usage = ImageUsage::DEPTH_ATTACHMENT | ImageUsage::SAMPLED;
        depthDesc.GenerateImGuiID = true;
        depthDesc.DebugName = "ShadowMap";
        blackBoard.ShadowPassImage = mRHI->CreateImage(depthDesc);

        FramebufferAttachment depthAttachment = {};
        depthAttachment.Handle = blackBoard.ShadowPassImage;
        depthAttachment.Load = LoadOp::CLEAR;
        depthAttachment.Store = StoreOp::STORE;

        FramebufferDesc fbDesc = {};
        fbDesc.ColorAttachmentCount = 0;
        fbDesc.DepthAttachment = depthAttachment;
        fbDesc.HasDepth = true;
        fbDesc.Width = size.x;
        fbDesc.Height = size.y;
        fbDesc.DebugName = "ShadowPass Framebuffer";
        blackBoard.ShadowPassFramebuffer = mRHI->CreateFramebuffer(fbDesc);

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
        desc.DebugName = "ShadowPass";
        desc.TargetFramebuffer = blackBoard.ShadowPassFramebuffer;
        desc.TargetSwapchain = false;
        mShadowPipeline = mRHI->CreatePipeline(desc);

        mImageWrites.push_back(blackBoard.ShadowPassImage);
    }

    void ShadowPass::Execute(const FrameContext& ctx, const FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Execute");
        if(!blackBoard.HasDirectionalLight)
            return;

        mRHI->CmdBindPipeline(ctx, mShadowPipeline);

        CalculateCascades(blackBoard);

        for(const MeshSubmitCmd& cmd : blackBoard.MeshList)
        {
            if (!cmd.DropShadow)
                continue;

            const Mesh& mesh = *cmd.Mesh_;
            mRHI->CmdBindVertexBuffer(ctx, mesh.GetVertexBuffer());
            mRHI->CmdBindIndexBuffer(ctx, mesh.GetIndexBuffer());

            const Submesh* submeshes = mesh.GetSubmeshes().data();
            for(Uint i = 0; i < mesh.GetSubmeshes().size(); i++)
            {
                const Submesh& submesh = submeshes[i];

                blackBoard.LightSpaceMatrix = mLightViewProjections[0];
                glm::mat4 pushConstants[2] = { cmd.Transform * submesh.Transform, blackBoard.LightSpaceMatrix };

                mRHI->CmdPushConstants(ctx, mShadowPipeline, ShaderType::VERTEX | ShaderType::FRAGMENT, 0, sizeof(glm::mat4) * 2, pushConstants);
                mRHI->CmdDrawIndexed(ctx, submesh.IndexCount, 1, submesh.BaseIndex, submesh.BaseVertex, 0);
            }
        }
    }

    void ShadowPass::Resize(Uint width, Uint height, FrameBlackboard& blackBoard)
    {
        //Core::AddFrameEndCallback([this, width, height, blackBoard]() { mRHI->ResizeFramebuffer(blackBoard.ShadowPassFramebuffer, width, height); });
    }

    void ShadowPass::OnImGuiRender(FrameBlackboard& blackBoard)
    {
    }

    void ShadowPass::Shutdown(FrameBlackboard& blackBoard)
    {
        SURGE_PROFILE_FUNC("ShadowPass::Shutdown");

        mRHI->DestroyFramebuffer(blackBoard.ShadowPassFramebuffer);
        mRHI->DestroyImage(blackBoard.ShadowPassImage);
        mRHI->DestroyPipeline(mShadowPipeline);
    }
    void ShadowPass::CalculateCascades(const FrameBlackboard& bb)
    {
        glm::mat4 inverseViewProjection = glm::inverse(bb.ViewProjection);
        Uint mTotalCascades = 3;

        const float clipRange = bb.CameraNearFarPlane.y - bb.CameraNearFarPlane.x;
        const float minZ = bb.CameraNearFarPlane.x;
        const float maxZ = bb.CameraNearFarPlane.x + clipRange;
        const float range = maxZ - minZ;
        const float ratio = maxZ / minZ;

        for(Uint i = 0; i < mTotalCascades; i++)
        {
            const float p = (i + 1) / static_cast<float>(mTotalCascades);
            const float log = minZ * glm::pow(ratio, p);
            const float uniform = minZ + range * p;
            const float d = mCascadeSplitLambda * (log - uniform) + uniform;
            mCascadeSplits[i] = (d - bb.CameraNearFarPlane.x) / clipRange;
        }

        float lastSplitDist = 0.0f;
        for(Uint cascade = 0; cascade < mTotalCascades; cascade++)
        {
            float splitDist = mCascadeSplits[cascade];

            glm::vec4 frustumCorners[NUM_FRUSTUM_CORNERS] =
            {
                { 1.0f,  1.0f, 0.0f, 1.0f },
                {-1.0f,  1.0f, 0.0f, 1.0f },
                { 1.0f, -1.0f, 0.0f, 1.0f },
                {-1.0f, -1.0f, 0.0f, 1.0f },

                { 1.0f,  1.0f, 1.0f, 1.0f },
                {-1.0f,  1.0f, 1.0f, 1.0f },
                { 1.0f, -1.0f, 1.0f, 1.0f },
                {-1.0f, -1.0f, 1.0f, 1.0f },
            };

            // Unproject frustum corners from NDC to world space
            for(glm::vec4& c : frustumCorners)
            {
                glm::vec4 inv = inverseViewProjection * c;
                c = inv / inv.w;
            }

            // Slice the frustum to this cascade's near/far range
            for(Uint i = 0; i < 4; i++)
            {
                glm::vec4 dist = frustumCorners[i + 4] - frustumCorners[i];
                frustumCorners[i + 4] = frustumCorners[i] + dist * splitDist;
                frustumCorners[i] = frustumCorners[i] + dist * lastSplitDist;
            }

            // Compute frustum center
            glm::vec3 frustumCenter = glm::vec3(0.0f);
            for(glm::vec4& c : frustumCorners)
                frustumCenter += glm::vec3(c);
            frustumCenter /= static_cast<float>(NUM_FRUSTUM_CORNERS);

            // Compute bounding sphere radius of this cascade slice
            float radius = 0.0f;
            for(glm::vec4& c : frustumCorners)
                radius = glm::max(radius, glm::length(glm::vec3(c) - frustumCenter));
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 maxExtents = glm::vec3(radius);
            glm::vec3 minExtents = -maxExtents;

            // Fixed world-space pullback to capture casters above/behind the frustum
            // Increase if tall objects outside the frustum slice cast shadows into it
            constexpr float zPullback = 50.0f;
            float nearPlane = -zPullback;
            float farPlane = (maxExtents.z - minExtents.z) + zPullback;

            glm::vec3 lightDir = glm::normalize(bb.DirectionalLightDir);

            glm::vec3 upVector = glm::vec3(0.0f, 1.0f, 0.0f);
            if(glm::abs(glm::dot(lightDir, upVector)) > 0.99f) // Handle degenerate up vector when light points straight down or up
                upVector = glm::vec3(0.0f, 0.0f, 1.0f);

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

            // Texel snapping to eliminate shadow edge shimmer on camera movement
            glm::mat4 shadowMatrix = lightProjectionMatrix * lightViewMatrix;
            glm::vec4 shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            shadowOrigin = shadowMatrix * shadowOrigin;
            shadowOrigin *= mShadowMapResolution / 2.0f;

            glm::vec4 roundedOrigin = glm::round(shadowOrigin);
            glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
            roundOffset *= 2.0f / mShadowMapResolution;
            roundOffset.z = 0.0f;
            roundOffset.w = 0.0f;

            lightProjectionMatrix[3] += roundOffset;

            mCascadeSplitDepths[cascade] = bb.CameraNearFarPlane.x + splitDist * clipRange;
            mLightViewProjections[cascade] = lightProjectionMatrix * lightViewMatrix;

            lastSplitDist = mCascadeSplits[cascade];
        }
    }

}
