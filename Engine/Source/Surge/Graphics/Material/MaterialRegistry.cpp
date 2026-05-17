// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Material/MaterialRegistry.hpp"
#include "Surge/Graphics/Material/Material.hpp"
#include "Surge/Graphics/RHI/RHI.hpp"

namespace Surge
{
    void MaterialRegistry::Initialize(GraphicsRHI* rhi)
    {
        mRHI = rhi;

        BufferDesc desc = {};
        desc.Size = sizeof(GPUMaterial) * MAX_MATERIALS;
        desc.Usage = BufferUsage::STORAGE;
        desc.HostVisible = true;
        desc.DebugName = "MaterialRegistry";
        mBuffer = mRHI->CreateBuffer(desc);
        mBufferIndex = mRHI->GetBindlessBufferIndex(mBuffer);
    }

    void MaterialRegistry::Shutdown()
    {
        mRHI->DestroyBuffer(mBuffer);
    }

    Uint MaterialRegistry::Allocate()
    {
        if (!mFreeSlots.empty())
        {
            Uint slot = mFreeSlots.back();
            mFreeSlots.pop_back();
            return slot;
        }
        SG_ASSERT(mNextSlot < MAX_MATERIALS, "MaterialRegistry: out of slots");
        return mNextSlot++;
    }

    void MaterialRegistry::Free(Uint slot)
    {
        mFreeSlots.push_back(slot);
    }

    void MaterialRegistry::Upload(Uint slot, const GPUMaterial& data)
    {
        mRHI->UploadBuffer(mBuffer, &data, sizeof(GPUMaterial), slot * sizeof(GPUMaterial));
    }

} // namespace Surge
