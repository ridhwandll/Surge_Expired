// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "SurgeMath/AABB.hpp"
#include "Surge/Graphics/Interface/GraphicsPipeline.hpp"
#include "Surge/Graphics/Interface/IndexBuffer.hpp"
#include "Surge/Graphics/Interface/VertexBuffer.hpp"
#include "Surge/Graphics/Material.hpp"
#include <glm/glm.hpp>

namespace Surge
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
        glm::vec2 TexCoord;
    };

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

    struct Index
    {
        Uint V1, V2, V3;
    };

    class SURGE_API Mesh : public RefCounted
    {
    public:
        Mesh(const Path& filepath);

        // Returns the path from which the Mesh was loaded
        FORCEINLINE const Path& GetPath() const { return mPath; }

        // Returns the vertex buffer of the mesh
        FORCEINLINE const Ref<VertexBuffer>& GetVertexBuffer() const { return mVertexBuffer; }

        // Returns the index buffer of the mesh
        FORCEINLINE const Ref<IndexBuffer>& GetIndexBuffer() const { return mIndexBuffer; }

        // Returns the submeshes of the mesh/model
        FORCEINLINE const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }

        FORCEINLINE Vector<Ref<Material>>& GetMaterials() { return mMaterials; }

    private:
        Path mPath;
        Vector<Submesh> mSubmeshes;

        Ref<VertexBuffer> mVertexBuffer;
        Ref<IndexBuffer> mIndexBuffer;
        Vector<Ref<Material>> mMaterials;

        Vector<Vertex> mVertices;
        Vector<Index> mIndices;
    };

} // namespace Surge