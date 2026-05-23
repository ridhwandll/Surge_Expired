// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/RHI/RHIDescs.hpp"

namespace Surge
{
    class GraphicsRHI;

    // Controls whether an image is recreated on window resize
    enum class ImageSizePolicy : uint8_t
    {
        FULLSCREEN, // Matches window dimensions, OffscreenColor, DepthImage etc.
        FIXED,      // Fixed size regardless of window, ShadowMap, LUTs etc.
    };

    struct RegisteredImage
    {
        ImageHandle Handle = {};
        ImageDesc Desc = {};   // Stored so we can recreate on resize
        ImageSizePolicy SizePolicy = ImageSizePolicy::FULLSCREEN;
    };

    class ResourceRegistry
    {
    public:
        ResourceRegistry() = default;
        ~ResourceRegistry() = default;

        // Called by a RenderPass in Setup() to register an image it created and shared via blackboard
        void Register(const String& name, ImageHandle handle, const ImageDesc& desc, ImageSizePolicy policy);

        // Called by the graph compiler to iterate all shared images (for barrier derivation)
        const HashMap<String, RegisteredImage>& GetAll() const { return mImages; }

        // Called by RenderGraph::Resize() — recreates FULLSCREEN images and returns updated handles
        // Caller (the graph) is responsible for writing new handles back to the blackboard
        HashMap<String, ImageHandle> ResizeAll(GraphicsRHI* rhi, Uint width, Uint height);

        // Destroys all registered images, called by RenderGraph::Shutdown()
        void Shutdown(GraphicsRHI* rhi);

    private:
        HashMap<String, RegisteredImage> mImages;
    };
}
