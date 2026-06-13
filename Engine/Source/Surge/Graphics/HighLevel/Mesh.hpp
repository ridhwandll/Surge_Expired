// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Asset/Asset.hpp"
#include "SurgeMath/AABB.hpp"
#include "Material.hpp"
#include "DefaultMeshes.hpp"

namespace Surge
{
    struct Submesh
    {
        Uint BaseVertex;
        Uint BaseIndex;
        Uint MaterialIndex;

        Uint IndexCount;
        Uint VertexCount;
        AABB BoundingBox;

        glm::mat4 Transform;
        glm::mat4 LocalTransform;
        String NodeName, MeshName;
    };

    struct MeshSpecification
    {
        Vector<Submesh> Submeshes;
        Vector<Vertex> Vertices;
        Vector<Index> Indices;
        Vector<AssetID> MaterialOverrides;
        Vector<Ref<Material>> Materials;
    };

    class Mesh : public Asset
    {
    public:
        Mesh(MeshSpecification&& spec);
        ~Mesh();
        SURGE_ASSET_TYPE(AssetType::MESH);
        static Ref<Mesh> Create(MeshSpecification&& spec);

        BufferHandle GetVertexBuffer() const { return mVertexBuffer; }
        BufferHandle GetIndexBuffer() const { return mIndexBuffer; }

        const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }

        void SetMaterialOverride(Uint index, Ref<Material> material)
        {
            SG_ASSERT(index < mGLTFMaterials.size(), "Slot index out of range!");
            if(mMaterialOverrides.size() <= index)
                mMaterialOverrides.resize(mGLTFMaterials.size(), material);

            mMaterialOverrides[index] = material;
        }

        void ClearMaterialOverride(Uint index)
        {
            if(index < mMaterialOverrides.size())
                mMaterialOverrides[index].Reset();
        }

        bool HasOverride(Uint index) const
        {
            return index < mMaterialOverrides.size() && mMaterialOverrides[index];
        }

        Ref<Material> GetMaterialAtIndex(Uint index) const;

        const Vector<Ref<Material>>& GetMaterials() const { return mGLTFMaterials; }
        const Vector<Ref<Material>>& GetMaterialOverrides() const { return mMaterialOverrides; }

    private:
        Vector<Submesh> mSubmeshes;
        BufferHandle mVertexBuffer;
        BufferHandle mIndexBuffer;

        Vector<Ref<Material>> mGLTFMaterials;
        Vector<Ref<Material>> mMaterialOverrides;
    };

} // namespace Surge