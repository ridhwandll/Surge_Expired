// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "RenderCommandBuffer.hpp"
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Core/String.hpp"

namespace Surge
{
    class Shader;
    class SURGE_API ComputePipeline : public RefCounted
    {
    public:
        ComputePipeline() = default;
        virtual ~ComputePipeline() = default;

        virtual void Bind(const Ref<RenderCommandBuffer>& renderCmdBuffer) = 0;
        virtual void SetPushConstantData(const Ref<RenderCommandBuffer>& cmdBuffer, const String& bufferName, void* data) const = 0;
        virtual void Dispatch(const Ref<RenderCommandBuffer>& renderCmdBuffer, Uint groupCountX, Uint groupCountY, Uint groupCountZ) = 0;
        virtual const Ref<Shader>& GetShader() const = 0;

        static Ref<ComputePipeline> Create(Ref<Shader>& computeShader);
    };

} // namespace Surge
