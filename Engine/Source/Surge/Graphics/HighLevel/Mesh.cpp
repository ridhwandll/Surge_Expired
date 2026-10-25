// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Mesh.hpp"
#include "Surge/Graphics/Renderer/Renderer.hpp"
#include "Surge/Asset/AssetManager.hpp"
#include "Material.hpp"

namespace Surge
{
    Mesh::Mesh(Renderer& renderer, AssetManager& assetManager, MeshSpecification&& spec)
        : mRHI(renderer.GetRHI().get())
    {
        // Culling relies on per-submesh bounds, rebuild them from the vertices (meshes created in code never fill them)
        for(Submesh& submesh : spec.Submeshes)
        {
            submesh.BoundingBox.Reset();
            const Uint end = std::min(submesh.BaseVertex + submesh.VertexCount, static_cast<Uint>(spec.Vertices.size()));
            for(Uint v = submesh.BaseVertex; v < end; v++)
            {
                submesh.BoundingBox.Min = glm::min(submesh.BoundingBox.Min, spec.Vertices[v].Position);
                submesh.BoundingBox.Max = glm::max(submesh.BoundingBox.Max, spec.Vertices[v].Position);
            }

            if(submesh.BoundingBox.Min.x > submesh.BoundingBox.Max.x) // No vertices, never culled
                submesh.BoundingBox = AABB(glm::vec3(-FLT_MAX * 0.5f), glm::vec3(FLT_MAX * 0.5f));
        }

        // A skeleton without one skin entry per vertex can't be drawn skinned (code built meshes, corrupted data): drop it
        if(spec.Skeleton_.IsValid() && spec.Skin.size() != spec.Vertices.size())
        {
            Log<Severity::Warn>("[Mesh] Skin data doesn't match the vertices ({} vs {}), the mesh is drawn static", spec.Skin.size(), spec.Vertices.size());
            spec.Skeleton_ = {};
            spec.Clips.clear();
        }

        // Skinned meshes interleave the skin influences behind each vertex (SkinnedVertex, Skinning.glsli)
        Vector<SkinnedVertex> skinnedVertices;
        if(spec.Skeleton_.IsValid())
        {
            skinnedVertices.resize(spec.Vertices.size());
            for(size_t i = 0; i < spec.Vertices.size(); i++)
                skinnedVertices[i] = { spec.Vertices[i], spec.Skin[i] };
        }

        GraphicsRHI* rhi = mRHI;
        BufferDesc vbDesc = {};
        vbDesc.Size = skinnedVertices.empty() ? static_cast<Uint>(spec.Vertices.size()) * sizeof(Vertex) : static_cast<Uint>(skinnedVertices.size()) * sizeof(SkinnedVertex);
        vbDesc.Usage = BufferUsage::VERTEX;
        vbDesc.HostVisible = false;
        vbDesc.InitialData = skinnedVertices.empty() ? static_cast<const void*>(spec.Vertices.data()) : static_cast<const void*>(skinnedVertices.data());
        vbDesc.DebugName = skinnedVertices.empty() ? "MeshVB" : "SkinnedMeshVB";
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

        mSkeleton = std::move(spec.Skeleton_);
        mClips = std::move(spec.Clips);
        if(mSkeleton.IsValid())
        {
            // Drawn with this palette while no animator drives the mesh
            Vector<glm::mat4> world(mSkeleton.GetNodeCount());
            mRestPalette.resize(mSkeleton.GetPaletteSize());
            mSkeleton.BuildPalette(mSkeleton.RestPose.data(), world.data(), mRestPalette.data());
            mRestBounds = mSkeleton.ComputeBounds(mRestPalette.data());
        }

        mMaterialOverrides.resize(mGLTFMaterials.size(), nullptr);

        // Convert stored AssetIDs to live Refs
        for(Uint i = 0; i < (Uint)spec.MaterialOverrides.size() && i < (Uint)mGLTFMaterials.size(); i++)
        {
            const AssetID& id = spec.MaterialOverrides[i];
            if(id.IsValid())
            {
                Ref<Material> material = assetManager.Load<Material>(id);
                if (material)
                {
                    mMaterialOverrides[i] = material;
                    mGLTFMaterials[i].Reset(); // (Rid)Release the transient material loaded from glTF, since we have a user override for this slot
                }
                else
                    Log<Severity::Warn>("[Mesh] Failed to load material override for mesh at index {}! The material is missing probably, falling back to transient material!", i);
            }
        }

        //TODO: Remove
        mVertices = std::move(spec.Vertices);
        mIndices = std::move(spec.Indices);
        if(mSkeleton.IsValid())
            mSkin = std::move(spec.Skin);
    }

    Mesh::~Mesh()
    {
        mRHI->DestroyBuffer(mVertexBuffer);
        mRHI->DestroyBuffer(mIndexBuffer);
    }

    Ref<Mesh> Mesh::Create(Renderer& renderer, AssetManager& assetManager, MeshSpecification&& spec)
    {
        return Ref<Mesh>::Create(renderer, assetManager, std::move(spec));
    }

    glm::vec3 Mesh::GetRestPosition(const Submesh& submesh, Uint vertexIndex) const
    {
        const glm::vec3& position = mVertices[vertexIndex].Position;
        if(mSkeleton.IsValid() && vertexIndex < mSkin.size())
            return SkinPosition(mRestPalette.data(), mSkin[vertexIndex], position);
        return glm::vec3(submesh.Transform * glm::vec4(position, 1.0f));
    }

    int Mesh::FindClip(const String& name) const
    {
        for(Uint i = 0; i < static_cast<Uint>(mClips.size()); i++)
        {
            if(mClips[i].Name == name)
                return static_cast<int>(i);
        }
        return -1;
    }

    const Material* Mesh::GetMaterialPtrAtIndex(Uint index) const
    {
        SG_ASSERT(index < mGLTFMaterials.size(), "Material index out of range!");

        if(index < mMaterialOverrides.size() && mMaterialOverrides[index])
            return mMaterialOverrides[index].Raw();

        return mGLTFMaterials[index].Raw();
    }

    Ref<Material> Mesh::GetMaterialAtIndex(Uint index) const
    {
        SG_ASSERT(index < mGLTFMaterials.size(), "Material index out of range!");

        if(index < mMaterialOverrides.size() && mMaterialOverrides[index])
            return mMaterialOverrides[index]; // Actual user override, owned by user, created from editor or runtime

        return mGLTFMaterials[index]; // Transient material from mesh, not owned by anyone, created from glTF
    }

} // namespace Surge