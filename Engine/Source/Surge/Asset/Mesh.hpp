// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Material/Material.hpp"
#include "SurgeMath/AABB.hpp"
#include "Asset.hpp"

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

    class Mesh : public Asset
    {
    public:
        // Mesh Construction
        // @param filepath Pass actual filepath to generate the mesh from disk
        //                 Pass DefaultMesh::X to generate a default mesh from memory
        Mesh(const String& filepath);

        ~Mesh();
        AssetType GetAssetType() const override { return AssetType::MESH; }

        BufferHandle GetVertexBuffer() const { return mVertexBuffer; }
        BufferHandle GetIndexBuffer() const { return mIndexBuffer; }

        const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }
        const Vector<Ref<Material>>& GetMaterials() const { return mMaterials; }
        Ref<Material>& GetMaterialAtIndex(Uint index) { return mMaterials[index]; }

        static Ref<Mesh> Create(const String& filepath);
    private:
        bool CheckAndGenerateDefaultMesh(const String& filepath);
    private:
        Vector<Submesh> mSubmeshes;
        BufferHandle mVertexBuffer;
        BufferHandle mIndexBuffer;

        // TODO
        // Materials associated with this mesh
        Vector<Ref<Material>> mMaterials;
    };

} // namespace Surge