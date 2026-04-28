// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Abstraction/Vulkan/VulkanFramebuffer.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanUtils.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanImage.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanRenderCommandBuffer.hpp"

namespace Surge
{
    VulkanFramebuffer::VulkanFramebuffer(const FramebufferSpecification& spec)
        : mSpecification(spec)
    {
        Invalidate();
    }

    VulkanFramebuffer::~VulkanFramebuffer()
    {
        Clear();
    }

    void VulkanFramebuffer::Resize(Uint width, Uint height)
    {
        if (mSpecification.NoResize)
            return;

        SG_ASSERT(width != 0 && height != 0, "Invalid size!");
        mSpecification.Width = width;
        mSpecification.Height = height;
        Invalidate();
    }

    void VulkanFramebuffer::BeginRenderPass(const Ref<RenderCommandBuffer>& cmdBuffer) const
    {
        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(renderContext->GetFrameIndex());

        // HACK; TODO: FIX
        Vector<VkClearValue> clearValues;
        if (mSpecification.AttachmentSpecs.size() == 1 && VulkanUtils::IsDepthFormat(mSpecification.AttachmentSpecs[0].Format))
        {
            clearValues.resize(1);
            clearValues[0].depthStencil = {1.0f, 0};
        }
        else
        {
            clearValues.resize(2);
            clearValues[0].color = {mSpecification.ClearColor.r, mSpecification.ClearColor.g, mSpecification.ClearColor.b, mSpecification.ClearColor.a};
            clearValues[1].depthStencil = {1.0f, 0};
        }

        VkViewport viewport = {};
        viewport.width = float(mSpecification.Width);
        viewport.height = float(mSpecification.Height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.extent = {mSpecification.Width, mSpecification.Height};
        scissor.offset = {0, 0};

        VkRenderPassBeginInfo renderPassBeginInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassBeginInfo.renderPass = mRenderPass;
        renderPassBeginInfo.renderArea.offset = {0, 0};
        renderPassBeginInfo.renderArea.extent = {mSpecification.Width, mSpecification.Height};
        renderPassBeginInfo.framebuffer = mFramebuffer;
        renderPassBeginInfo.clearValueCount = static_cast<Uint>(clearValues.size());
        renderPassBeginInfo.pClearValues = clearValues.data();
        vkCmdBeginRenderPass(vulkanCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(vulkanCmdBuffer, 0, 1, &viewport);
        vkCmdSetScissor(vulkanCmdBuffer, 0, 1, &scissor);
    }

    void VulkanFramebuffer::EndRenderPass(const Ref<RenderCommandBuffer>& cmdBuffer) const
    {
        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkCommandBuffer vulkanCmdBuffer = cmdBuffer.As<VulkanRenderCommandBuffer>()->GetVulkanCommandBuffer(renderContext->GetFrameIndex());
        vkCmdEndRenderPass(vulkanCmdBuffer);
    }

    void VulkanFramebuffer::Invalidate()
    {
        Clear();
        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice logicalDevice = renderContext->GetDevice()->GetLogicalDevice();

        Vector<VkAttachmentDescription> attachmentDescriptions;

        Vector<VkAttachmentReference> colorAttachmentReferences;
        VkAttachmentReference depthAttachmentReference;

        Uint attachmentIndex = 0;
        for (FramebufferAttachmentSpec& spec : mSpecification.AttachmentSpecs)
        {
            ImageSpecification imageSpec;
            imageSpec.Format = spec.Format;
            imageSpec.Width = mSpecification.Width;
            imageSpec.Height = mSpecification.Height;
            imageSpec.Usage = ImageUsage::Attachment;
            imageSpec.Mips = 1;
            imageSpec.SamplerProps = spec.AttachmentSamplerProps;
            Ref<Image2D> image = Image2D::Create(imageSpec);

            if (VulkanUtils::IsDepthFormat(spec.Format))
            {
                SG_ASSERT(!mDepthAttachmentImage, "Depth Attachment already exists!");
                mDepthAttachmentImage = image;
                attachmentDescriptions.emplace_back();
                VkAttachmentDescription& depthAttachment = attachmentDescriptions.back();
                depthAttachment = {};
                depthAttachment.format = VulkanUtils::GetImageFormat(spec.Format);
                depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                depthAttachmentReference.attachment = attachmentIndex;
                depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
            else
            {
                mColorAttachmentImages.push_back(image);
                attachmentDescriptions.emplace_back();
                VkAttachmentDescription& colorAttachment = attachmentDescriptions.back();
                colorAttachment = {};
                colorAttachment.format = VulkanUtils::GetImageFormat(spec.Format);
                colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
                colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;   // We don't care about stencil
                colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // ^^
                colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;         // We don't care what previous layout the image was in
                colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                VkAttachmentReference colorAttachmentReference {};
                colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                colorAttachmentReference.attachment = attachmentIndex; // Specifies which attachment to reference by it's index in the attachment descriptions array
                colorAttachmentReferences.push_back(colorAttachmentReference);
            }
            attachmentIndex++;
        }

        VkSubpassDescription subpassDescription = {};
        subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpassDescription.colorAttachmentCount = static_cast<Uint>(colorAttachmentReferences.size());
        subpassDescription.pColorAttachments = colorAttachmentReferences.data();
        if (mDepthAttachmentImage)
            subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        // Create the Framebuffer
        VkRenderPassCreateInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        renderPassInfo.attachmentCount = static_cast<Uint>(attachmentDescriptions.size());
        renderPassInfo.pAttachments = attachmentDescriptions.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpassDescription;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        VK_CALL(vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &mRenderPass));
        SET_VK_OBJECT_DEBUGNAME(mRenderPass, VK_OBJECT_TYPE_RENDER_PASS, "RenderPass");

        Uint colorAttachmentImagesSize = static_cast<Uint>(mColorAttachmentImages.size());
        Vector<VkImageView> attachments(colorAttachmentImagesSize);

        for (Uint i = 0; i < colorAttachmentImagesSize; i++)
        {
            Ref<VulkanImage2D> image = mColorAttachmentImages[i].As<VulkanImage2D>();
            attachments[i] = image->GetVulkanImageView();
            SG_ASSERT(attachments[i], "Invalid Vulkan ImageView!");
        }
        if (mDepthAttachmentImage)
        {
            Ref<VulkanImage2D> image = mDepthAttachmentImage.As<VulkanImage2D>();
            attachments.emplace_back(image->GetVulkanImageView());
            SG_ASSERT(attachments.back(), "Invalid Vulkan ImageView!");
        }

        VkFramebufferCreateInfo framebufferCreateInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferCreateInfo.renderPass = mRenderPass;
        framebufferCreateInfo.attachmentCount = Uint(attachments.size());
        framebufferCreateInfo.pAttachments = attachments.data();
        framebufferCreateInfo.width = mSpecification.Width;
        framebufferCreateInfo.height = mSpecification.Height;
        framebufferCreateInfo.layers = 1;
        VK_CALL(vkCreateFramebuffer(logicalDevice, &framebufferCreateInfo, nullptr, &mFramebuffer));
        SET_VK_OBJECT_DEBUGNAME(mFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER, "Framebuffer");
    }

    void VulkanFramebuffer::Clear()
    {
        VulkanRenderContext* renderContext;
        SURGE_GET_VULKAN_CONTEXT(renderContext);
        VkDevice logicalDevice = renderContext->GetDevice()->GetLogicalDevice();

        mColorAttachmentImages.clear();

        if (mDepthAttachmentImage)
        {
            mDepthAttachmentImage = nullptr;
        }

        if (mRenderPass)
        {
            vkDestroyRenderPass(logicalDevice, mRenderPass, nullptr);
            mRenderPass = VK_NULL_HANDLE;
        }
        if (mFramebuffer)
        {
            vkDestroyFramebuffer(logicalDevice, mFramebuffer, nullptr);
            mFramebuffer = VK_NULL_HANDLE;
        }
    }

} // namespace Surge
