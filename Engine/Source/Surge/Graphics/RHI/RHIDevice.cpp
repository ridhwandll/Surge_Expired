// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/RHI/RHIDevice.hpp"

namespace Surge::RHI
{
    static RHIDevice* sDevice = nullptr;

    void SetRHIDevice(RHIDevice* device)
    {
        sDevice = device;
    }

    RHIDevice* GetRHIDevice()
    {
        SG_ASSERT(sDevice, "RHI: No device registered. Call SetRHIDevice() before using the RHI.");
        return sDevice;
    }

} // namespace Surge::RHI
