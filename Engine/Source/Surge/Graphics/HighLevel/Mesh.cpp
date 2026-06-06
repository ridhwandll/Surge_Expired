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
        mMaterials = std::move(spec.Materials);
    }

    Mesh::~Mesh()
    {
        Scope<GraphicsRHI>& rhi = Core::GetRenderer()->GetRHI();

        rhi->WaitIdle();
        rhi->DestroyBuffer(mVertexBuffer);
        rhi->DestroyBuffer(mIndexBuffer);
    }

    Ref<Mesh> Mesh::Create(MeshSpecification&& spec)
    {
        return Ref<Mesh>::Create(std::move(spec));
    }

} // namespace Surge