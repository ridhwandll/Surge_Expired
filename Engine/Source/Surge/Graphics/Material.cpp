// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Surge/Graphics/Material.hpp"
#include "Surge/Graphics/Abstraction/Vulkan/VulkanMaterial.hpp"
#include "Renderer/Renderer.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    Ref<Texture2D> Material::mDummyTexture = nullptr;

    Ref<Material> Material::Create(const String& shaderName, const String& materialName)
    {
        return Ref<VulkanMaterial>::Create(Core::GetRenderer()->GetShader(shaderName), materialName);
    }

    void Material::RemoveTexture(const String& name)
    {
        Ref<Texture2D>& whiteTex = Core::GetRenderer()->GetData()->WhiteTexture;
        this->Set<Ref<Texture2D>>(name, whiteTex);
    }

} // namespace Surge