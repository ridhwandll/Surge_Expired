// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Mesh.hpp"
#include "Surge/Core/Core.hpp"

namespace Surge
{
    Mesh::Mesh(MeshSpecification&& spec)
    {
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        BufferDesc vbDesc = {};
        vbDesc.Size = static_cast<Uint>(spec.Vertices.size()) * sizeof(Vertex);
        vbDesc.Usage = BufferUsage::VERTEX;
        vbDesc.HostVisible = false;
        vbDesc.InitialData = spec.Vertices.data();
        vbDesc.DebugName = "MeshVB";
        mVertexBuffer = rhi->CreateBuffer(vbDesc);
        BufferDesc ibDesc = {};
        ibDesc.Size = static_cast<Uint>(spec.Indices.size()) * sizeof(Index);
        ibDesc.Usage = BufferUsage::INDEX;
        ibDesc.HostVisible = false;
        ibDesc.InitialData = spec.Indices.data();
        ibDesc.DebugName = "MeshIB";
        mIndexBuffer = rhi->CreateBuffer(ibDesc);

        mSubmeshes = std::move(spec.Submeshes);
        mGLTFMaterials = std::move(spec.Materials);

        mMaterialOverrides.resize(mGLTFMaterials.size(), nullptr);

        // Convert stored AssetIDs to live Refs
        AssetManager* am = Core::GetAssetManager();
        for(Uint i = 0; i < (Uint)spec.MaterialOverrides.size() && i < (Uint)mGLTFMaterials.size(); i++)
        {
            const AssetID& id = spec.MaterialOverrides[i];
            if(id.IsValid())
            {
                Ref<Material> material = am->Load<Material>(id);
                if (material)
                {
                    mMaterialOverrides[i] = material;
                    mGLTFMaterials[i].Reset(); // (Rid)Release the transient material loaded from glTF, since we have a user override for this slot
                }
                else
                    Log<Severity::Warn>("[Mesh] Failed to load material override for mesh at index {}! The material is missing probably, falling back to transient material!", i);
            }
        }
    }

    Mesh::~Mesh()
    {
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();
        rhi->DestroyBuffer(mVertexBuffer);
        rhi->DestroyBuffer(mIndexBuffer);
    }

    Ref<Mesh> Mesh::Create(MeshSpecification&& spec)
    {
        return Ref<Mesh>::Create(std::move(spec));
    }

    Ref<Material> Mesh::GetMaterialAtIndex(Uint index) const
    {
        SG_ASSERT(index < mGLTFMaterials.size(), "Material index out of range!");

        if(index < mMaterialOverrides.size() && mMaterialOverrides[index])
            return mMaterialOverrides[index]; // Actual user override, owned by user, created from editor or runtime

        return mGLTFMaterials[index]; // Transient material from mesh, not owned by anyone, created from glTF
    }

} // namespace Surge