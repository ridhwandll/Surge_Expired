// Copyright (c) - SurgeTechnologies - All rights reserved
#include "Mesh.hpp"
#include "Surge/Utility/Filesystem.hpp"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace Surge
{
    static glm::mat4 AssimpMat4ToGlmMat4(const aiMatrix4x4& matrix)
    {
        glm::mat4 result;
        result[0][0] = matrix.a1;
        result[1][0] = matrix.a2;
        result[2][0] = matrix.a3;
        result[3][0] = matrix.a4;
        result[0][1] = matrix.b1;
        result[1][1] = matrix.b2;
        result[2][1] = matrix.b3;
        result[3][1] = matrix.b4;
        result[0][2] = matrix.c1;
        result[1][2] = matrix.c2;
        result[2][2] = matrix.c3;
        result[3][2] = matrix.c4;
        result[0][3] = matrix.d1;
        result[1][3] = matrix.d2;
        result[2][3] = matrix.d3;
        result[3][3] = matrix.d4;
        return result;
    }

    static const Uint sMeshImportFlags = aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_GenUVCoords | aiProcess_OptimizeMeshes | aiProcess_ValidateDataStructure |
                                         aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace;
    template <aiTextureType texType>
    static void LoadTexture(const Path& meshPath, aiMaterial* aiMat, Ref<Material>& material, const String& texName)
    {
        aiString aiTexPath;
        if (aiMat->GetTexture(texType, 0, &aiTexPath) == aiReturn_SUCCESS)
        {
            Path texturePath = Filesystem::GetParentPath(meshPath) / String(aiTexPath.data);
            Log<Severity::Trace>("{0} path: {1}", texName, texturePath);

            TextureSpecification spec;
            spec.UseMips = true;
            Ref<Texture2D> texture = Texture2D::Create(texturePath, spec);
            material->Set<Ref<Texture2D>>(texName, texture);
        }
        if constexpr (texType == aiTextureType_DIFFUSE)
            material->Set("Material.Albedo", glm::vec3(1.0f));
        else if constexpr (texType == aiTextureType_SHININESS)
            material->Set("Material.Roughness", 1.0f);
        else if constexpr (texType == aiTextureType_SPECULAR)
            material->Set("Material.Metalness", 1.0f);
    }

    static void SetValues(aiMaterial* aiMaterial, Ref<Material>& material)
    {
        //Color
        glm::vec3 albedoColor = {1.0f, 1.0f, 1.0f};
        aiColor3D aiColor;
        if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
            albedoColor = {aiColor.r, aiColor.g, aiColor.b};
        material->Set("Material.Albedo", albedoColor);

        //Roughness
        float shininess;
        if (aiMaterial->Get(AI_MATKEY_SHININESS, shininess) != aiReturn_SUCCESS)
            shininess = 50.0f;
        float roughness = 1.0f - glm::sqrt(shininess / 100.0f);
        material->Set<float>("Material.Roughness", roughness);

        //Metalness
        float metalness = 0.0f;
        aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness);
        material->Set<float>("Material.Metalness", metalness);
    }

    Mesh::Mesh(const Path& filepath)
        : mPath(filepath)
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filepath, sMeshImportFlags);

        if (!scene || !scene->HasMeshes())
            Log<Severity::Error>("Failed to load mesh file: {0}", filepath);

        Uint vertexCount = 0;
        Uint indexCount = 0;

        mSubmeshes.reserve(scene->mNumMeshes);
        for (size_t m = 0; m < scene->mNumMeshes; m++)
        {
            aiMesh* mesh = scene->mMeshes[m];
            SG_ASSERT(mesh->HasPositions(), "Meshes require positions.");
            SG_ASSERT(mesh->HasNormals(), "Meshes require normals.");

            Submesh& submesh = mSubmeshes.emplace_back();
            submesh.BaseVertex = vertexCount;
            submesh.BaseIndex = indexCount;
            submesh.MaterialIndex = mesh->mMaterialIndex;
            submesh.IndexCount = mesh->mNumFaces * 3;
            submesh.VertexCount = mesh->mNumVertices;
            submesh.MeshName = mesh->mName.C_Str();
            vertexCount += submesh.VertexCount;
            indexCount += submesh.IndexCount;

            GetVertexData(mesh, submesh.BoundingBox);
            GetIndexData(mesh);
        }

        TraverseNodes(scene->mRootNode);

        if (scene->HasMaterials())
        {
            mMaterials.resize(scene->mNumMaterials);
            for (Uint i = 0; i < scene->mNumMaterials; i++)
            {
                aiMaterial* assimpMaterial = scene->mMaterials[i];
                const String& materialName = assimpMaterial->GetName().C_Str();

                Ref<Material> material = Material::Create("PBR", materialName.empty() ? "NoName" : materialName);
                mMaterials[i] = material;

                SetValues(assimpMaterial, material);
                LoadTexture<aiTextureType_DIFFUSE>(mPath, assimpMaterial, mMaterials[i], "AlbedoMap");
                LoadTexture<aiTextureType_HEIGHT>(mPath, assimpMaterial, mMaterials[i], "NormalMap");
                LoadTexture<aiTextureType_SHININESS>(mPath, assimpMaterial, mMaterials[i], "RoughnessMap");
                LoadTexture<aiTextureType_SPECULAR>(mPath, assimpMaterial, mMaterials[i], "MetalnessMap");
            }
        }

        mVertexBuffer = VertexBuffer::Create(mVertices.data(), static_cast<Uint>(mVertices.size()) * sizeof(Vertex));
        mIndexBuffer = IndexBuffer::Create(mIndices.data(), static_cast<Uint>(mIndices.size() * sizeof(Index)));
    }

    void Mesh::GetVertexData(const aiMesh* mesh, AABB& outAABB)
    {
        outAABB.Reset();
        for (size_t i = 0; i < mesh->mNumVertices; i++)
        {
            Vertex vertex;
            vertex.Position = {mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z};
            vertex.Normal = {mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z};

            outAABB.Min.x = glm::min(vertex.Position.x, outAABB.Min.x);
            outAABB.Min.y = glm::min(vertex.Position.y, outAABB.Min.y);
            outAABB.Min.z = glm::min(vertex.Position.z, outAABB.Min.z);
            outAABB.Max.x = glm::max(vertex.Position.x, outAABB.Max.x);
            outAABB.Max.y = glm::max(vertex.Position.y, outAABB.Max.y);
            outAABB.Max.z = glm::max(vertex.Position.z, outAABB.Max.z);

            if (mesh->HasTangentsAndBitangents())
            {
                vertex.Tangent = {mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z};
                vertex.Bitangent = {mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z};
            }

            if (mesh->HasTextureCoords(0))
                vertex.TexCoord = {mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y};
            else
                vertex.TexCoord = {0.0f, 0.0f};

            mVertices.push_back(vertex);
        }
    }

    void Mesh::GetIndexData(const aiMesh* mesh)
    {
        for (size_t i = 0; i < mesh->mNumFaces; i++)
        {
            SG_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Mesh Must have 3 indices!");
            Index index = {mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2]};
            mIndices.push_back(index);
        }
    }

    void Mesh::TraverseNodes(aiNode* node, const glm::mat4& parentTransform, Uint level)
    {
        const glm::mat4 localTransform = AssimpMat4ToGlmMat4(node->mTransformation);
        const glm::mat4 transform = parentTransform * localTransform;

        for (Uint i = 0; i < node->mNumMeshes; i++)
        {
            const Uint mesh = node->mMeshes[i];
            auto& submesh = mSubmeshes[mesh];
            submesh.NodeName = node->mName.C_Str();
            submesh.Transform = transform;
            submesh.LocalTransform = localTransform;
        }

        for (Uint i = 0; i < node->mNumChildren; i++)
            TraverseNodes(node->mChildren[i], transform, level + 1);
    }

} // namespace Surge
