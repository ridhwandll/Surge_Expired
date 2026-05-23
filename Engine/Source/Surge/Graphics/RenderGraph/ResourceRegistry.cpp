// Copyright (c) - SurgeTechnologies - All rights reserved
#include "ResourceRegistry.hpp"
#include "../RHI/RHI.hpp"

namespace Surge
{
    void ResourceRegistry::Register(const String& name, ImageHandle handle, const ImageDesc& desc, ImageSizePolicy policy)
    {
        SG_ASSERT(mImages.find(name) == mImages.end(), "ResourceRegistry: image already registered!");

        RegisteredImage entry = {};
        entry.Handle = handle;
        entry.Desc = desc;
        entry.SizePolicy = policy;
        mImages[name] = entry;
    }

    HashMap<String, ImageHandle> ResourceRegistry::ResizeAll(GraphicsRHI* rhi, Uint width, Uint height)
    {
        HashMap<String, ImageHandle> updatedHandles;
        for(auto& [name, entry] : mImages)
        {
            if(entry.SizePolicy != ImageSizePolicy::FULLSCREEN)
                continue;

            rhi->ResizeImage(entry.Handle, width, height);
            updatedHandles[name] = entry.Handle;
        }

        return updatedHandles;
    }

    void ResourceRegistry::Shutdown(GraphicsRHI* rhi)
    {
        for(auto& [name, entry] : mImages)
            rhi->DestroyImage(entry.Handle);

        mImages.clear();
    }
}
