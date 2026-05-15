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
				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				for (Uint s = 0; s <= segments; ++s)
				{
					float theta = 2.0f * glm::pi<float>() * (float)s / (float)segments;
					float sinTheta = glm::sin(theta);
					float cosTheta = glm::cos(theta);

					Vertex v;
					// Position and Normals (Y is Up)
					v.Normal = glm::vec3(-sinPhi * cosTheta, -cosPhi, -sinPhi * sinTheta);
					v.Position = v.Normal * radius;
					v.TexCoord = glm::vec2((float)s / segments, (float)r / rings);

					// Tangent Vector (Derivative with respect to Theta)
					// This is the direction of increasing 's' texture coordinate.
					v.Tangent = glm::normalize(glm::vec3(-sinPhi * sinTheta, 0.0f, sinPhi * cosTheta));

					// Handle the poles where sinPhi == 0 to avoid NaNs
					if (sinPhi == 0.0f)
						v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
					
					// Bitangent Vector
					// Cross product direction dictates whether lighting calculations 
					// read your surfaces inside-out or outside-in!
					v.Bitangent = glm::normalize(glm::cross(v.Normal, v.Tangent));

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
			MeshData meshData;
			float halfHeight = height * 0.5f;
			float thetaStep = (2.0f * glm::pi<float>()) / (float)segments;

			// Generate Side Vertices
			for (Uint i = 0; i <= segments; i++)
			{
				float theta = i * thetaStep;
				float cosTheta = cos(theta);
				float sinTheta = sin(theta);

				glm::vec3 basePos = glm::vec3(radius * cosTheta, -halfHeight, radius * sinTheta);

				float slantAngle = atan2(radius, height);
				float nx = cosTheta * cos(slantAngle);
				float ny = sin(slantAngle);
				float nz = sinTheta * cos(slantAngle);
				glm::vec3 sideNormal = glm::normalize(glm::vec3(nx, ny, nz));

				glm::vec3 tangent = glm::normalize(glm::vec3(-sinTheta, 0.0f, cosTheta));
				glm::vec3 bitangent = glm::cross(sideNormal, tangent);

				// Base Vertex
				Vertex baseVertex;
				baseVertex.Position = basePos;
				baseVertex.Normal = sideNormal;
				baseVertex.Tangent = tangent;
				baseVertex.Bitangent = bitangent;
				baseVertex.TexCoord = glm::vec2((float)i / segments, 0.0f);
				meshData.Vertices.push_back(baseVertex);

				// Tip Vertex
				Vertex tipVertex;
				tipVertex.Position = glm::vec3(0.0f, halfHeight, 0.0f);
				tipVertex.Normal = sideNormal;
				tipVertex.Tangent = tangent;
				tipVertex.Bitangent = bitangent;
				tipVertex.TexCoord = glm::vec2((float)i / segments, 1.0f);
				meshData.Vertices.push_back(tipVertex);
			}

			// Side Indices: Corrected for CCW Front Face
			for (Uint i = 0; i < segments; i++)
			{
				Uint baseIndex = i * 2;
				Index idx;
				idx.V1 = baseIndex;
				idx.V2 = baseIndex + 1;
				idx.V3 = baseIndex + 2;
				meshData.Indices.push_back(idx);
			}

			// Generate Bottom Cap
			Uint capCenterIndex = (Uint)meshData.Vertices.size();

			Vertex centerVertex;
			centerVertex.Position = glm::vec3(0.0f, -halfHeight, 0.0f);
			centerVertex.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
			centerVertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
			centerVertex.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
			centerVertex.TexCoord = glm::vec2(0.5f, 0.5f);
			meshData.Vertices.push_back(centerVertex);

			Uint capEdgeStart = (Uint)meshData.Vertices.size();
			for (Uint i = 0; i <= segments; i++)
			{
				float theta = i * thetaStep;
				Vertex v;
				v.Position = glm::vec3(radius * cos(theta), -halfHeight, radius * sin(theta));
				v.Normal = glm::vec3(0.0f, -1.0f, 0.0f);
				v.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
				v.Bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
				v.TexCoord = glm::vec2(cos(theta) * 0.5f + 0.5f, sin(theta) * 0.5f + 0.5f);
				meshData.Vertices.push_back(v);
			}

			// Bottom Cap Indices: Corrected for CCW Front Face (looking from bottom)
			for (Uint i = 0; i < segments; i++)
			{
				Index idx;
				idx.V1 = capCenterIndex;
				idx.V2 = capEdgeStart + i;
				idx.V3 = capEdgeStart + i + 1;
				meshData.Indices.push_back(idx);
			}

			return meshData;
		}

		// CYLINDER	
		inline MeshData GenerateCylinder(float radius = 0.5f, float height = 1.0f, int sectors = 32)
		{
			MeshData meshData;

			float halfHeight = height / 2.0f;
			float angleStep = 2.0f * glm::pi<float>() / static_cast<float>(sectors);

			// Bottom cap
			Vertex bottomCenter;
			bottomCenter.Position = { 0.0f, -halfHeight, 0.0f };
			bottomCenter.Normal = { 0.0f, -1.0f, 0.0f };
			bottomCenter.Tangent = { 1.0f, 0.0f, 0.0f };
			bottomCenter.Bitangent = { 0.0f, 0.0f, 1.0f };
			bottomCenter.TexCoord = { 0.5f, 0.5f };
			meshData.Vertices.push_back(bottomCenter);
			Uint bottomCenterIndex = 0;

			Uint bottomRimStart = static_cast<Uint>(meshData.Vertices.size());
			for (int i = 0; i <= sectors; i++)
			{
				float angle = i * angleStep;
				float cosA = glm::cos(angle);
				float sinA = glm::sin(angle);

				Vertex v;
				v.Position = { radius * cosA, -halfHeight, radius * sinA };
				v.Normal = { 0.0f, -1.0f, 0.0f };
				v.Tangent = { 1.0f, 0.0f, 0.0f };
				v.Bitangent = { 0.0f, 0.0f, 1.0f };
				v.TexCoord = { cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f };
				meshData.Vertices.push_back(v);
			}

			// Bottom cap triangles
			for (int i = 0; i < sectors; i++)			
				meshData.Indices.push_back({ bottomCenterIndex, static_cast<Uint>(bottomRimStart + i), static_cast<Uint>(bottomRimStart + i + 1) });			

			// Top cap
			Uint topCenterIndex = static_cast<Uint>(meshData.Vertices.size());
			Vertex topCenter;
			topCenter.Position = { 0.0f, halfHeight, 0.0f };
			topCenter.Normal = { 0.0f, 1.0f, 0.0f };
			topCenter.Tangent = { 1.0f, 0.0f, 0.0f };
			topCenter.Bitangent = { 0.0f, 0.0f, -1.0f };
			topCenter.TexCoord = { 0.5f, 0.5f };
			meshData.Vertices.push_back(topCenter);

			Uint topRimStart = static_cast<Uint>(meshData.Vertices.size());
			for (int i = 0; i <= sectors; i++)
			{
				float angle = i * angleStep;
				float cosA = glm::cos(angle);
				float sinA = glm::sin(angle);

				Vertex v;
				v.Position = { radius * cosA, halfHeight, radius * sinA };
				v.Normal = { 0.0f, 1.0f, 0.0f };
				v.Tangent = { 1.0f, 0.0f, 0.0f };
				v.Bitangent = { 0.0f, 0.0f, -1.0f };
				v.TexCoord = { cosA * 0.5f + 0.5f, sinA * 0.5f + 0.5f };
				meshData.Vertices.push_back(v);
			}

			// Top cap triangles
			for (int i = 0; i < sectors; i++)			
				meshData.Indices.push_back({ topCenterIndex, static_cast<Uint>(topRimStart + i + 1), static_cast<Uint>(topRimStart + i) });
			
			// Side surface
			Uint sideStart = static_cast<Uint>(meshData.Vertices.size());
			for (int i = 0; i <= sectors; i++)
			{
				float angle = i * angleStep;
				float cosA = glm::cos(angle);
				float sinA = glm::sin(angle);
				float u = static_cast<float>(i) / static_cast<float>(sectors);

				glm::vec3 sideNormal = glm::normalize(glm::vec3(cosA, 0.0f, sinA));
				glm::vec3 sideTangent = glm::normalize(glm::vec3(-sinA, 0.0f, cosA));
				glm::vec3 sideBitangent = { 0.0f, 1.0f, 0.0f };

				Vertex vBottom;
				vBottom.Position = { radius * cosA, -halfHeight, radius * sinA };
				vBottom.Normal = sideNormal;
				vBottom.Tangent = sideTangent;
				vBottom.Bitangent = sideBitangent;
				vBottom.TexCoord = { u, 1.0f };
				meshData.Vertices.push_back(vBottom);

				Vertex vTop;
				vTop.Position = { radius * cosA, halfHeight, radius * sinA };
				vTop.Normal = sideNormal;
				vTop.Tangent = sideTangent;
				vTop.Bitangent = sideBitangent;
				vTop.TexCoord = { u, 0.0f };
				meshData.Vertices.push_back(vTop);
			}

			// Side quads as two CCW triangles
			for (int i = 0; i < sectors; i++)
			{
				Uint currentBottom = sideStart + i * 2;
				Uint currentTop = sideStart + i * 2 + 1;
				Uint nextBottom = sideStart + (i + 1) * 2;
				Uint nextTop = sideStart + (i + 1) * 2 + 1;

				// Triangle 1
				meshData.Indices.push_back({ currentBottom, currentTop, nextTop });
				// Triangle 2
				meshData.Indices.push_back({ currentBottom, nextTop, nextBottom });
			}

			return meshData;
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

				float sinPhi = glm::sin(phi);
				float cosPhi = glm::cos(phi);

				for (Uint s = 0; s <= segments; ++s)
				{
					float theta = 2.0f * glm::pi<float>() * (float)s / segments;
					float sinTheta = glm::sin(theta);
					float cosTheta = glm::cos(theta);

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
				float cosMain = glm::cos(mainAngle);
				float sinMain = glm::sin(mainAngle);

				for (Uint t = 0; t <= tubeSegments; ++t)
				{
					float tubeAngle = ((float)t / tubeSegments) * 2.0f * glm::pi<float>();
					float cosTube = glm::cos(tubeAngle);
					float sinTube = glm::sin(tubeAngle);

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