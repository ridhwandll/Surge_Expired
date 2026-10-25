// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Asset/Asset.hpp"
#include "SurgeMath/AABB.hpp"
#include "Surge/Graphics/Animation/AnimationClip.hpp"
#include "Material.hpp"
#include "DefaultMeshes.hpp"

namespace Surge
{
    class Renderer;
    class GraphicsRHI;
    class AssetManager;

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

        // Animated meshes only (empty otherwise): every vertex is skinned, Skin runs parallel to Vertices
        Skeleton Skeleton_;
        Vector<VertexSkin> Skin;
        Vector<AnimationClip> Clips;
    };

    class Mesh : public Asset
    {
    public:
        // assetManager resolves the spec's material overrides
        Mesh(Renderer& renderer, AssetManager& assetManager, MeshSpecification&& spec);
        ~Mesh();
        SURGE_ASSET_TYPE(AssetType::MESH);
        static Ref<Mesh> Create(Renderer& renderer, AssetManager& assetManager, MeshSpecification&& spec);

        const Vector<Vertex>& GetVertices() const { return mVertices; }
        const Vector<Index>& GetIndices() const { return mIndices; }

        BufferHandle GetVertexBuffer() const { return mVertexBuffer; }
        BufferHandle GetIndexBuffer() const { return mIndexBuffer; }

        const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }

        // Skinned meshes (a skeleton came with the glTF) always draw with the skinned pipelines: their vertex buffer holds SkinnedVertex.
        // Without an animator they show the rest pose
        bool IsSkinned() const { return mSkeleton.IsValid(); }
        const Skeleton& GetSkeleton() const { return mSkeleton; }
        const Vector<AnimationClip>& GetClips() const { return mClips; }
        int FindClip(const String& name) const;
        const Vector<SkinMatrix>& GetRestPalette() const { return mRestPalette; }
        const AABB& GetRestBounds() const { return mRestBounds; } // Model space
        const Vector<VertexSkin>& GetSkin() const { return mSkin; } // Parallel to GetVertices(), skinned meshes only

        // Model space position of a vertex as drawn without an animator: skinned with the rest palette, or through its submesh node
        glm::vec3 GetRestPosition(const Submesh& submesh, Uint vertexIndex) const;

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

        void SetGLTFMaterial(Uint index, const Ref<Material>& material)
        {
            SG_ASSERT(index < mGLTFMaterials.size(), "Slot index out of range!");
            mGLTFMaterials[index] = material;
        }

        Ref<Material> GetMaterialAtIndex(Uint index) const;
        const Material* GetMaterialPtrAtIndex(Uint index) const; // Render loops: no refcount traffic

        const Vector<Ref<Material>>& GetMaterials() const { return mGLTFMaterials; }
        const Vector<Ref<Material>>& GetMaterialOverrides() const { return mMaterialOverrides; }

    private:
        Vector<Vertex> mVertices; //TODO: Remove, currently used for physics convex hull generation
        Vector<Index> mIndices;
        Vector<VertexSkin> mSkin; // CPU copy like mVertices: picking and colliders skin positions with it

        Vector<Submesh> mSubmeshes;
        Skeleton mSkeleton;
        Vector<AnimationClip> mClips;
        Vector<SkinMatrix> mRestPalette;
        AABB mRestBounds;
        BufferHandle mVertexBuffer;
        BufferHandle mIndexBuffer;
        GraphicsRHI* mRHI;

        Vector<Ref<Material>> mGLTFMaterials;
        Vector<Ref<Material>> mMaterialOverrides;
    };

} // namespace Surge