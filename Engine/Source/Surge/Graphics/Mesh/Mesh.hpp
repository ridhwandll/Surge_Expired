// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "SurgeMath/AABB.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
#include "Surge/Graphics/Material/Material.hpp"
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

    class Mesh : public RefCounted
    {
    public:
        Mesh(DefaultMesh type);
        Mesh(const String& filepath); // Relative path to Engine/Assets directory
        ~Mesh();

        const String& GetPath() const { return mPath; }

        BufferHandle GetVertexBuffer() const { return mVertexBuffer; }
        BufferHandle GetIndexBuffer() const { return mIndexBuffer; }

        const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }
        const Vector<Ref<Material>>& GetMaterials() const { return mMaterials; }
        Ref<Material>& GetMaterialAtIndex(Uint index) { return mMaterials[index]; }
    private:
        void CreateRHIObjects();

    private:
        String mPath;
        Vector<Submesh> mSubmeshes;

        BufferHandle mVertexBuffer;
        BufferHandle mIndexBuffer;

        Vector<Vertex> mVertices;
        Vector<Index> mIndices;

        // TODO
        // Materials associated with this mesh
        Vector<Ref<Material>> mMaterials;
    };

} // namespace Surge