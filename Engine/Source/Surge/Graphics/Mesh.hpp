// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "SurgeMath/AABB.hpp"
#include "Surge/Graphics/Material.hpp"
#include "Surge/Graphics/RHI/RHIDevice.hpp"
#include <glm/glm.hpp>

struct aiMesh;
struct aiNode;

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
        ~Mesh();

        FORCEINLINE const Path& GetPath() const { return mPath; }
        FORCEINLINE RHI::BufferHandle GetVertexBufferHandle() const { return mVertexBuffer; }
        FORCEINLINE RHI::BufferHandle GetIndexBufferHandle() const { return mIndexBuffer; }
        FORCEINLINE const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }
        FORCEINLINE Vector<Ref<Material>>& GetMaterials() { return mMaterials; }

    private:
        void GetVertexData(const aiMesh* mesh, AABB& outAABB);
        void GetIndexData(const aiMesh* mesh);
        void TraverseNodes(aiNode* node, const glm::mat4& parentTransform = glm::mat4(1.0f), Uint level = 0);

    private:
        Path mPath;
        Vector<Submesh> mSubmeshes;

        RHI::BufferHandle mVertexBuffer;
        RHI::BufferHandle mIndexBuffer;
        Vector<Ref<Material>> mMaterials;

        Vector<Vertex> mVertices;
        Vector<Index>  mIndices;
    };

} // namespace Surge