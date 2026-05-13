// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include "Surge/Core/Memory.hpp"
#include "SurgeMath/AABB.hpp"
#include "Surge/Graphics/RHI/RHIHandle.hpp"
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

	class Mesh : public RefCounted
	{
	public:
		// Relative path to Engine/Assets directory
		Mesh(const String& filepath);
		~Mesh();

		const String& GetPath() const { return mPath; }

		BufferHandle GetVertexBuffer() const { return mVertexBuffer; }
		BufferHandle GetIndexBuffer() const { return mIndexBuffer; }

		const Vector<Submesh>& GetSubmeshes() const { return mSubmeshes; }		

	private:
		String mPath;
		Vector<Submesh> mSubmeshes;

		BufferHandle mVertexBuffer;
		BufferHandle mIndexBuffer;

		Vector<Vertex> mVertices;
		Vector<Index> mIndices;
	};

} // namespace Surge