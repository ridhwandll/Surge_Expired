// Copyright (c) - SurgeTechnologies - All rights reserved
#pragma once
#include "Surge/Core/Defines.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

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

	struct Index
	{
		Uint V1, V2, V3;
	};

	enum class DefaultMesh
	{
		CUBE = 0,
		SPHERE,
		PLANE,
		CONE,
		CYLINDER,
		BEAN, // Totally not Capsule
		TORUS
	};

	namespace MeshGenerator
	{
		inline String DefaultMeshToString(DefaultMesh type)
		{
			switch (type)
			{
			case DefaultMesh::CUBE: return "Cube";
			case DefaultMesh::SPHERE: return "Sphere";
			case DefaultMesh::PLANE: return "Plane";
			case DefaultMesh::CONE: return "Cone";
			case DefaultMesh::CYLINDER: return "Cylinder";
			case DefaultMesh::BEAN: return "Bean";
			case DefaultMesh::TORUS: return "Torus";
			default: return "Unknown";
			}
		}

		struct MeshData
		{
			Vector<Vertex> Vertices;
			Vector<Index> Indices;
		};

		inline void ComputeTangents(Vertex & v)
		{
			glm::vec3 c1 = glm::cross(v.Normal, glm::vec3(0.0f, 0.0f, 1.0f));
			glm::vec3 c2 = glm::cross(v.Normal, glm::vec3(0.0f, 1.0f, 0.0f));
			v.Tangent = glm::length(c1) > glm::length(c2) ? glm::normalize(c1) : glm::normalize(c2);
			v.Bitangent = glm::normalize(glm::cross(v.Normal, v.Tangent));
		}

		// CUBE
		inline MeshData GenerateCube(float size = 1.0f)
		{
			MeshData mesh;
			float h = size * 0.5f;

			// 6 faces, 4 vertices per face
			struct Face { glm::vec3 n, t, b; };
			Face faces[6] =
			{
				{ { 0, 0, 1}, { 1, 0, 0}, {0, 1, 0} }, // Front
				{ { 0, 0,-1}, {-1, 0, 0}, {0, 1, 0} }, // Back
				{ { 1, 0, 0}, { 0, 0,-1}, {0, 1, 0} }, // Right
				{ {-1, 0, 0}, { 0, 0, 1}, {0, 1, 0} }, // Left
				{ { 0, 1, 0}, { 1, 0, 0}, {0, 0,-1} }, // Top
				{ { 0,-1, 0}, { 1, 0, 0}, {0, 0, 1} }  // Bottom
			};

			glm::vec2 uvs[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

			for (int i = 0; i < 6; ++i)
			{
				Uint startIdx = (Uint)mesh.Vertices.size();
				for (int j = 0; j < 4; ++j)
				{
					Vertex v;
					v.Normal = faces[i].n;
					v.Tangent = faces[i].t;
					v.Bitangent = faces[i].b;
					v.TexCoord = uvs[j];

					// Derive corner position from basis vectors
					float signX = (j == 1 || j == 2) ? 1.0f : -1.0f;
					float signY = (j == 2 || j == 3) ? 1.0f : -1.0f;
					v.Position = faces[i].n * h + faces[i].t * signX * h + faces[i].b * signY * h;

					mesh.Vertices.push_back(v);
				}
				mesh.Indices.push_back({ startIdx, startIdx + 1, startIdx + 2 });
				mesh.Indices.push_back({ startIdx, startIdx + 2, startIdx + 3 });
			}
			return mesh;
		}

		// PLANE
		inline MeshData GeneratePlane(float width = 1.0f, float depth = 1.0f, Uint subdivisions = 1)
		{
			MeshData mesh;
			Uint xSegments = subdivisions;
			Uint zSegments = subdivisions;

			for (Uint z = 0; z <= zSegments; ++z)
			{
				float zPos = ((float)z / zSegments - 0.5f) * depth;
				float vCoord = (float)z / zSegments;
				for (Uint x = 0; x <= xSegments; ++x)
				{
					float xPos = ((float)x / xSegments - 0.5f) * width;
					float uCoord = (float)x / xSegments;

					Vertex v;
					v.Position = glm::vec3(xPos, 0.0f, zPos);
					v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
					v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
					v.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
					v.TexCoord = glm::vec2(uCoord, vCoord);
					mesh.Vertices.push_back(v);
				}
			}

			for (Uint z = 0; z < zSegments; ++z)
			{
				for (Uint x = 0; x < xSegments; ++x)
				{
					Uint row1 = z * (xSegments + 1);
					Uint row2 = (z + 1) * (xSegments + 1);

					mesh.Indices.push_back({ row1 + x, row2 + x, row1 + x + 1 });
					mesh.Indices.push_back({ row1 + x + 1, row2 + x, row2 + x + 1 });
				}
			}
			return mesh;
		}

		// SPHERE
		inline MeshData GenerateSphere(float radius = 0.5f, Uint segments = 32, Uint rings = 16)
		{
			MeshData mesh;

			for (Uint r = 0; r <= rings; ++r)
			{
				float phi = glm::pi<float>() * (float)r / (float)rings;
				float sinPhi = std::sin(phi);
				float cosPhi = std::cos(phi);

				for (Uint s = 0; s <= segments; ++s)
				{
					float theta = 2.0f * glm::pi<float>() * (float)s / (float)segments;
					float sinTheta = std::sin(theta);
					float cosTheta = std::cos(theta);

					Vertex v;
					v.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
					v.Position = v.Normal * radius;
					v.TexCoord = glm::vec2((float)s / segments, (float)r / rings);
					v.Tangent = glm::vec3(-sinTheta, 0.0f, cosTheta);
					v.Bitangent = glm::cross(v.Normal, v.Tangent);

					mesh.Vertices.push_back(v);
				}
			}

			for (Uint r = 0; r < rings; ++r)
			{
				for (Uint s = 0; s < segments; ++s)
				{
					Uint current = r * (segments + 1) + s;
					Uint next = current + segments + 1;

					mesh.Indices.push_back({ current, next, current + 1 });
					mesh.Indices.push_back({ current + 1, next, next + 1 });
				}
			}
			return mesh;
		}

		// CONE
		inline MeshData GenerateCone(float radius = 0.5f, float height = 1.0f, Uint segments = 32)
		{
			MeshData mesh;
			float halfH = height * 0.5f;

			// Body vertices + Apex duplicates for smooth texture mapping seam
			for (Uint i = 0; i <= segments; ++i)
			{
				float ratio = (float)i / segments;
				float angle = ratio * 2.0f * glm::pi<float>();
				float c = std::cos(angle);
				float s = std::sin(angle);

				// Base perimeter vertex
				Vertex vBase;
				vBase.Position = glm::vec3(c * radius, -halfH, s * radius);
				vBase.Normal = glm::normalize(glm::vec3(c * height / radius, radius / height, s * height / radius));
				vBase.TexCoord = glm::vec2(ratio, 0.0f);
				ComputeTangents(vBase);
				mesh.Vertices.push_back(vBase);

				// Matching Tip vertex (unique UVs per segment slice)
				Vertex vTip;
				vTip.Position = glm::vec3(0.0f, halfH, 0.0f);
				vTip.Normal = vBase.Normal;
				vTip.TexCoord = glm::vec2(ratio, 1.0f);
				ComputeTangents(vTip);
				mesh.Vertices.push_back(vTip);
			}

			// Body indices
			for (Uint i = 0; i < segments; ++i)
			{
				Uint baseIdx = i * 2;
				mesh.Indices.push_back({ baseIdx, baseIdx + 1, baseIdx + 2 });
			}

			// Bottom Cap Center
			Uint centerIdx = (Uint)mesh.Vertices.size();
			Vertex capCenter;
			capCenter.Position = glm::vec3(0.0f, -halfH, 0.0f);
			capCenter.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
			capCenter.TexCoord = glm::vec2(0.5f, 0.5f);
			ComputeTangents(capCenter);
			mesh.Vertices.push_back(capCenter);

			// Bottom Cap Perimeter
			Uint capStart = (Uint)mesh.Vertices.size();
			for (Uint i = 0; i <= segments; ++i)
			{
				float ratio = (float)i / segments;
				float angle = ratio * 2.0f * glm::pi<float>();
				Vertex v;
				v.Position = glm::vec3(std::cos(angle) * radius, -halfH, std::sin(angle) * radius);
				v.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
				v.TexCoord = glm::vec2(std::cos(angle) * 0.5f + 0.5f, std::sin(angle) * 0.5f + 0.5f);
				ComputeTangents(v);
				mesh.Vertices.push_back(v);
			}

			for (Uint i = 0; i < segments; ++i)
				mesh.Indices.push_back({ centerIdx, capStart + i + 1, capStart + i });

			return mesh;
		}

		// CYLINDER	
		inline MeshData GenerateCylinder(float radius = 0.5f, float height = 1.0f, int sectors = 32)
		{
			MeshData mesh;
			float halfHeight = height / 2.0f;

			// Side wall vertices and indices
			Uint sideStartIdx = 0;
			for (int i = 0; i <= sectors; ++i)
			{
				float ratio = static_cast<float>(i) / sectors;
				float angle = 2.0f * glm::pi<float>() * ratio;
				float x = radius * cos(angle);
				float z = radius * sin(angle); // Using Z as depth (Y-up convention)

				// Normal, Tangent, and Bitangent for the sides
				glm::vec3 normal = glm::normalize(glm::vec3(x, 0.0f, z));
				glm::vec3 tangent = glm::normalize(glm::vec3(-sin(angle), 0.0f, cos(angle)));
				glm::vec3 bitangent = glm::cross(normal, tangent);

				// Bottom side vertex
				Vertex bottomVertex;
				bottomVertex.Position = glm::vec3(x, -halfHeight, z);
				bottomVertex.Normal = normal;
				bottomVertex.Tangent = tangent;
				bottomVertex.Bitangent = bitangent;
				bottomVertex.TexCoord = glm::vec2(ratio, 0.0f);
				mesh.Vertices.push_back(bottomVertex);

				// Top side vertex
				Vertex topVertex;
				topVertex.Position = glm::vec3(x, halfHeight, z);
				topVertex.Normal = normal;
				topVertex.Tangent = tangent;
				topVertex.Bitangent = bitangent;
				topVertex.TexCoord = glm::vec2(ratio, 1.0f);
				mesh.Vertices.push_back(topVertex);
			}

			// Side indices
			for (int i = 0; i < sectors; ++i)
			{
				Uint b1 = sideStartIdx + (i * 2);
				Uint t1 = b1 + 1;
				Uint b2 = sideStartIdx + ((i + 1) * 2);
				Uint t2 = b2 + 1;

				mesh.Indices.push_back({ b1, b2, t1 });
				mesh.Indices.push_back({ t1, b2, t2 });
			}

			// Bottom cap
			Uint bottomCenterIdx = mesh.Vertices.size();
			Vertex bottomCenter;
			bottomCenter.Position = glm::vec3(0.0f, -halfHeight, 0.0f);
			bottomCenter.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
			bottomCenter.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			bottomCenter.Bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
			bottomCenter.TexCoord = glm::vec2(0.5f, 0.5f);
			mesh.Vertices.push_back(bottomCenter);

			Uint bottomCapStartIdx = mesh.Vertices.size();
			for (int i = 0; i < sectors; ++i)
			{
				float angle = 2.0f * glm::pi<float>() * i / sectors;
				float x = radius * cos(angle);
				float z = radius * sin(angle);

				Vertex v;
				v.Position = glm::vec3(x, -halfHeight, z);
				v.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
				v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				v.Bitangent = glm::vec3(0.0f, 0.0f, -1.0f);
				v.TexCoord = glm::vec2(0.5f + 0.5f * cos(angle), 0.5f + 0.5f * sin(angle));
				mesh.Vertices.push_back(v);
			}

			// Bottom cap indices (facing downwards: clockwise order from bottom look)
			for (int i = 0; i < sectors; ++i)
			{
				Uint current = bottomCapStartIdx + i;
				Uint next = bottomCapStartIdx + ((i + 1) % sectors);
				mesh.Indices.push_back({ bottomCenterIdx, next, current });
			}

			// Top cap
			Uint topCenterIdx = mesh.Vertices.size();
			Vertex topCenter;
			topCenter.Position = glm::vec3(0.0f, halfHeight, 0.0f);
			topCenter.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
			topCenter.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			topCenter.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			topCenter.TexCoord = glm::vec2(0.5f, 0.5f);
			mesh.Vertices.push_back(topCenter);

			Uint topCapStartIdx = mesh.Vertices.size();
			for (int i = 0; i < sectors; ++i)
			{
				float angle = 2.0f * glm::pi<float>() * i / sectors;
				float x = radius * cos(angle);
				float z = radius * sin(angle);

				Vertex v;
				v.Position = glm::vec3(x, halfHeight, z);
				v.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
				v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				v.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
				v.TexCoord = glm::vec2(0.5f + 0.5f * cos(angle), 0.5f + 0.5f * sin(angle));
				mesh.Vertices.push_back(v);
			}

			// Top cap indices (facing upwards: counter-clockwise order)
			for (int i = 0; i < sectors; ++i)
			{
				Uint current = topCapStartIdx + i;
				Uint next = topCapStartIdx + ((i + 1) % sectors);
				mesh.Indices.push_back({ topCenterIdx, current, next });
			}

			return mesh;
		}

		// BEAN
		inline MeshData GenerateBean(float radius = 0.3f, float height = 1.0f, Uint segments = 32, Uint rings = 8)
		{
			MeshData mesh;
			float cylHeight = height - (radius * 2.0f);
			if (cylHeight < 0) cylHeight = 0; // Fallback to safe sphere layout if height is too low
			float halfCylH = cylHeight * 0.5f;

			// Generate Profile from Top to Bottom
			for (Uint r = 0; r <= rings * 2; ++r)
			{
				float yOffset = 0.0f;
				float phi = 0.0f;

				if (r <= rings)
				{ // Top Hemisphere
					phi = (glm::pi<float>() * 0.5f) * (float)r / rings;
					yOffset = halfCylH;
				}
				else
				{ // Bottom Hemisphere
					phi = (glm::pi<float>() * 0.5f) + (glm::pi<float>() * 0.5f) * (float)(r - rings) / rings;
					yOffset = -halfCylH;
				}

				float sinPhi = std::sin(phi);
				float cosPhi = std::cos(phi);

				for (Uint s = 0; s <= segments; ++s)
				{
					float theta = 2.0f * glm::pi<float>() * (float)s / segments;
					float sinTheta = std::sin(theta);
					float cosTheta = std::cos(theta);

					Vertex v;
					v.Normal = glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta);
					v.Position = glm::vec3(v.Normal.x * radius, (v.Normal.y * radius) + yOffset, v.Normal.z * radius);
					v.TexCoord = glm::vec2((float)s / segments, (float)r / (rings * 2));
					v.Tangent = glm::vec3(-sinTheta, 0.0f, cosTheta);
					v.Bitangent = glm::cross(v.Normal, v.Tangent);
					mesh.Vertices.push_back(v);
				}
			}

			for (Uint r = 0; r < rings * 2; ++r)
			{
				for (Uint s = 0; s < segments; ++s)
				{
					Uint current = r * (segments + 1) + s;
					Uint next = current + segments + 1;
					mesh.Indices.push_back({ current, next, current + 1 });
					mesh.Indices.push_back({ current + 1, next, next + 1 });
				}
			}
			return mesh;
		}

		// TORUS
		inline MeshData GenerateTorus(float mainRadius = 0.5f, float tubeRadius = 0.15f, Uint mainSegments = 32, Uint tubeSegments = 16)
		{
			MeshData mesh;

			for (Uint m = 0; m <= mainSegments; ++m)
			{
				float mainAngle = ((float)m / mainSegments) * 2.0f * glm::pi<float>();
				float cosMain = std::cos(mainAngle);
				float sinMain = std::sin(mainAngle);

				for (Uint t = 0; t <= tubeSegments; ++t)
				{
					float tubeAngle = ((float)t / tubeSegments) * 2.0f * glm::pi<float>();
					float cosTube = std::cos(tubeAngle);
					float sinTube = std::sin(tubeAngle);

					Vertex v;
					// Position
					v.Position.x = (mainRadius + tubeRadius * cosTube) * cosMain;
					v.Position.y = tubeRadius * sinTube;
					v.Position.z = (mainRadius + tubeRadius * cosTube) * sinMain;

					// Normal vector pointing outwards from the local core ring center
					glm::vec3 coreRingCenter = glm::vec3(mainRadius * cosMain, 0.0f, mainRadius * sinMain);
					v.Normal = glm::normalize(v.Position - coreRingCenter);

					v.TexCoord = glm::vec2((float)m / mainSegments, (float)t / tubeSegments);

					// Tangent along main ring path
					v.Tangent = glm::vec3(-sinMain, 0.0f, cosMain);
					v.Bitangent = glm::cross(v.Normal, v.Tangent);

					mesh.Vertices.push_back(v);
				}
			}

			for (Uint m = 0; m < mainSegments; ++m)
			{
				for (Uint t = 0; t < tubeSegments; ++t)
				{
					Uint current = m * (tubeSegments + 1) + t;
					Uint next = current + tubeSegments + 1;

					mesh.Indices.push_back({ current, next, current + 1 });
					mesh.Indices.push_back({ current + 1, next, next + 1 });
				}
			}
			return mesh;
		}

		inline MeshData GenerateDefaultMesh(DefaultMesh type)
		{
			switch (type)
			{
			case DefaultMesh::CUBE:     return GenerateCube();
			case DefaultMesh::SPHERE:   return GenerateSphere();
			case DefaultMesh::PLANE:    return GeneratePlane();
			case DefaultMesh::CONE:     return GenerateCone();
			case DefaultMesh::CYLINDER: return GenerateCylinder();
			case DefaultMesh::BEAN:     return GenerateBean();
			case DefaultMesh::TORUS:    return GenerateTorus();
			default:
				SG_ASSERT_INTERNAL("Tf yo doin?");
				return GenerateCube();
			}
		}
	}

}