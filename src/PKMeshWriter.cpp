#pragma once
#include "PKShaderWriter.h"
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include <unordered_map>
#include <map>
#include <mikktspace/mikktspace.h>
#include <tinyobjloader/tiny_obj_loader.h>
#include <meshoptimizer/meshoptimizer.h>

namespace PK::Assets::Mesh
{
	namespace MikktsInterface1
	{
		struct PKMeshData
		{
			float* vertices = nullptr;
			uint32_t stride = 0;
			uint32_t vertexOffset = 0;
			uint32_t normalOffset = 0;
			uint32_t tangentOffset = 0;
			uint32_t texcoordOffset = 0;
			const uint32_t* indices = nullptr;
			uint32_t vcount = 0;
			uint32_t icount = 0;
		};

		// Returns the number of faces (triangles/quads) on the mesh to be processed.
		int GetNumFaces(const SMikkTSpaceContext* pContext)
		{
			return reinterpret_cast<PKMeshData*>(pContext->m_pUserData)->icount / 3;
		}

		// Returns the number of vertices on face number iFace
		// iFace is a number in the range {0, 1, ..., getNumFaces()-1}
		int GetNumVerticesOfFace(const SMikkTSpaceContext* pContext, const int iFace)
		{
			return 3;
		}

		// returns the position/normal/texcoord of the referenced face of vertex number iVert.
		// iVert is in the range {0,1,2} for triangles and {0,1,2,3} for quads.
		void GetPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int iFace, const int iVert)
		{
			auto meshData = reinterpret_cast<PKMeshData*>(pContext->m_pUserData);
			auto baseIndex = meshData->indices[iFace * 3 + iVert];
			auto vertex = meshData->vertices + baseIndex * meshData->stride + meshData->vertexOffset;
			fvPosOut[0] = vertex[0];
			fvPosOut[1] = vertex[1];
			fvPosOut[2] = vertex[2];
		}

		void GetNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int iFace, const int iVert)
		{
			auto meshData = reinterpret_cast<PKMeshData*>(pContext->m_pUserData);
			auto baseIndex = meshData->indices[iFace * 3 + iVert];
			auto normal = meshData->vertices + baseIndex * meshData->stride + meshData->normalOffset;
			fvNormOut[0] = normal[0];
			fvNormOut[1] = normal[1];
			fvNormOut[2] = normal[2];
		}

		void GetTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int iFace, const int iVert)
		{
			auto meshData = reinterpret_cast<PKMeshData*>(pContext->m_pUserData);
			auto baseIndex = meshData->indices[iFace * 3 + iVert];
			auto texcoord = meshData->vertices + baseIndex * meshData->stride + meshData->texcoordOffset;
			fvTexcOut[0] = texcoord[0];
			fvTexcOut[1] = texcoord[1];
		}

		// either (or both) of the two setTSpace callbacks can be set.
		// The call-back m_setTSpaceBasic() is sufficient for basic normal mapping.

		// This function is used to return the tangent and fSign to the application.
		// fvTangent is a unit length vector.
		// For normal maps it is sufficient to use the following simplified version of the bitangent which is generated at pixel/vertex level.
		// bitangent = fSign * cross(vN, tangent);
		// Note that the results are returned unindexed. It is possible to generate a new index list
		// But averaging/overwriting tangent spaces by using an already existing index list WILL produce INCRORRECT results.
		// DO NOT! use an already existing index list.
		void SetTSpaceBasic(const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert)
		{
			auto meshData = reinterpret_cast<PKMeshData*>(pContext->m_pUserData);
			auto baseIndex = meshData->indices[iFace * 3 + iVert];
			auto tangent = meshData->vertices + baseIndex * meshData->stride + meshData->tangentOffset;
			tangent[0] = fvTangent[0];
			tangent[1] = fvTangent[1];
			tangent[2] = fvTangent[2];
			tangent[3] = fSign;
		}
	}

	int CalculateTangents(void* vertices, 
						  uint32_t stride, 
					      uint32_t vertexOffset, 
						  uint32_t normalOffset, 
						  uint32_t tangentOffset, 
						  uint32_t texcoordOffset, 
						  const uint32_t* indices, 
						  uint32_t vcount, 
						  uint32_t icount)
	{
		MikktsInterface1::PKMeshData data;
		data.vertices = reinterpret_cast<float*>(vertices);
		data.stride = stride;
		data.vertexOffset = vertexOffset;
		data.normalOffset = normalOffset;
		data.tangentOffset = tangentOffset;
		data.texcoordOffset = texcoordOffset;
		data.indices = indices;
		data.vcount = vcount;
		data.icount = icount;

		SMikkTSpaceInterface mikttInterface;
		mikttInterface.m_getNumFaces = MikktsInterface1::GetNumFaces;
		mikttInterface.m_getNumVerticesOfFace = MikktsInterface1::GetNumVerticesOfFace;
		mikttInterface.m_getPosition = MikktsInterface1::GetPosition;
		mikttInterface.m_getNormal = MikktsInterface1::GetNormal;
		mikttInterface.m_getTexCoord = MikktsInterface1::GetTexCoord;
		mikttInterface.m_setTSpaceBasic = MikktsInterface1::SetTSpaceBasic;
		mikttInterface.m_setTSpace = nullptr;

		SMikkTSpaceContext context;
		context.m_pInterface = &mikttInterface;
		context.m_pUserData = &data;

		auto result = genTangSpaceDefault(&context);
		
		if (!result)
		{
			printf("Failed to calculate tangents");
			return -1;
		}

		return 0;
	}


	struct Buffer : public std::vector<char>
	{
		size_t head = 0;
		
		template<typename T>
		void append(const T* src, uint32_t count)
		{
			auto s = sizeof(T) * count;
			auto nh = head + s;
			
			if (nh > size())
			{
				resize(nh);
			}

			memcpy(data() + head, src, s);
			head += s;
		}
	};

	struct IndexSet
	{
		uint32_t position = 0u;
		uint32_t normal = 0u;
		uint32_t uv = 0u;

		inline bool operator < (const IndexSet& r) const noexcept
		{
			return memcmp(reinterpret_cast<const void*>(this), reinterpret_cast<const void*>(&r), sizeof(IndexSet)) < 0;
		}
	};

	void OptimizeMesh(Buffer& vertices, size_t stride, std::vector<uint32_t>& indices, const std::vector<PKSubmesh>& submeshes)
	{
		auto vcount = vertices.size() / stride;

		std::vector<uint32_t> remap(indices.size());
		size_t total_vertices = meshopt_generateVertexRemap(&remap[0], indices.data(), indices.size(), vertices.data(), vcount, stride);
		meshopt_remapIndexBuffer(indices.data(), indices.data(), indices.size(), &remap[0]);
		meshopt_remapVertexBuffer(vertices.data(), vertices.data(), vcount, stride, &remap[0]);

		for (const auto& sm : submeshes)
		{
			auto pSmIndices = &indices[sm.firstIndex];
			meshopt_optimizeVertexCache(pSmIndices, pSmIndices, sm.indexCount, total_vertices);
		
			// Assumes that positions are the first attribute in a vertex.
			meshopt_optimizeOverdraw(pSmIndices, pSmIndices, sm.indexCount, reinterpret_cast<float*>(vertices.data()), total_vertices, stride, 1.05f);
		}
		
		meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), total_vertices, stride);
	}

    int WriteMesh(const char* pathSrc, const char* pathDst)
    {
        auto filename = StringUtilities::ReadFileName(pathSrc);
        printf("Preprocessing mesh: %s \n", filename.c_str());

		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string err;

		bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, pathSrc, StringUtilities::ReadDirectory(pathSrc).c_str(), true);

		if (!err.empty())
		{
			printf(err.c_str());
			return -1;
		}

		if (attrib.vertices.empty())
		{
			printf("Mesh doesn't contain vertices");
			return -1;
		}

		if (!success)
		{
			printf("Failed to load .obj");
			return -1;
		}

		auto hasNormals = !attrib.normals.empty();
		auto hasUvs = !attrib.texcoords.empty();

		Buffer vertexBuffer;
		std::map<IndexSet, uint32_t> indexmap;
		std::vector<uint32_t> indices;
		std::vector<PKSubmesh> submeshes;
		std::vector<PKVertexAttribute> attributes;

		PKVertexAttribute attribute;
		WriteName(attribute.name, PK_VS_POSITION);
		attribute.type = PKElementType::Float3;
		attribute.size = sizeof(float) * 3;
		attribute.offset = 0;
		attributes.push_back(attribute);
		auto stride = attribute.size;

		if (hasNormals)
		{
			WriteName(attribute.name, PK_VS_NORMAL);
			attribute.type = PKElementType::Float3;
			attribute.size = sizeof(float) * 3;
			attribute.offset = stride;
			attributes.push_back(attribute);
			stride += attribute.size;

			if (hasUvs)
			{
				WriteName(attribute.name, PK_VS_TANGENT);
				attribute.type = PKElementType::Float4;
				attribute.size = sizeof(float) * 4;
				attribute.offset = stride;
				attributes.push_back(attribute);
				stride += attribute.size;
			}
		}

		if (hasUvs)
		{
			WriteName(attribute.name, PK_VS_TEXCOORD0);
			attribute.type = PKElementType::Float2;
			attribute.size = sizeof(float) * 2;
			attribute.offset = stride;
			attributes.push_back(attribute);
			stride += attribute.size;
		}

		float float4_zero[4]{};

		auto invertices = attrib.vertices.data();
		auto innormals = attrib.normals.data();
		auto inuvs = attrib.texcoords.data();

		auto index = 0u;

		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& tris = shapes.at(i).mesh.indices;
			auto tcount = (uint32_t)tris.size();
			PKSubmesh submesh{};
			submesh.firstIndex = (uint32_t)indices.size();
			submesh.indexCount = tcount;
			submesh.bbmax[0] = submesh.bbmax[1] = submesh.bbmax[2] = -std::numeric_limits<float>().max();
			submesh.bbmin[0] = submesh.bbmin[1] = submesh.bbmin[2] = std::numeric_limits<float>().max();

			for (uint32_t j = 0; j < tcount; ++j)
			{
				auto& tri = tris.at(j);
				IndexSet triKey = { (uint32_t)tri.vertex_index, (uint32_t)tri.normal_index, (uint32_t)tri.texcoord_index };
				
				auto iter = indexmap.find(triKey);
				
				if (iter != indexmap.end())
				{
					indices.push_back(iter->second);
					continue;
				}

				indices.push_back(index);
				indexmap[triKey] = index++;

				float pos[3]{};
				memcpy(pos, invertices + tri.vertex_index * 3, sizeof(float) * 3);

				vertexBuffer.append(pos, 3);

				if (hasNormals)
				{
					vertexBuffer.append(innormals + tri.normal_index * 3, 3);

					// tangent
					if (hasUvs)
					{
						vertexBuffer.append(float4_zero, 4);
					}
				}

				if (hasUvs)
				{
					vertexBuffer.append(inuvs + tri.texcoord_index * 2, 2);
				}

				for (auto k = 0; k < 3; ++k)
				{
					if (submesh.bbmax[k] < pos[k])
					{
						submesh.bbmax[k] = pos[k];
					}

					if (submesh.bbmin[k] > pos[k])
					{
						submesh.bbmin[k] = pos[k];
					}
				}
			}

			submeshes.push_back(submesh);
		}

		OptimizeMesh(vertexBuffer, stride, indices, submeshes);

		auto vcount = (uint32_t)(vertexBuffer.size() / stride);
		auto ushortmax = std::numeric_limits<unsigned short>().max();
		auto indexType = vcount > ushortmax ? PKElementType::Uint : PKElementType::Ushort;
		auto indexSize = indexType == PKElementType::Uint ? sizeof(uint32_t) : sizeof(unsigned short);

		if (hasNormals && hasUvs)
		{
			auto vfloats = reinterpret_cast<float*>(vertexBuffer.data());
			auto fstride = (uint32_t)(stride / sizeof(float));
			CalculateTangents(vfloats, fstride, 0, 3, 6, 10, indices.data(), vcount, (uint32_t)indices.size());
		}

		auto buffer = PKAssetBuffer();
        buffer.header.get()->type = PKAssetType::Mesh;
        WriteName(buffer.header.get()->name, filename.c_str());
        
		auto mesh = buffer.Allocate<PKMesh>();

		mesh.get()->indexType = indexType;
		mesh.get()->submeshCount = (uint32_t)submeshes.size();
		mesh.get()->vertexAttributeCount = (uint32_t)attributes.size();
		mesh.get()->vertexCount = (uint32_t)(vertexBuffer.size() / stride);
		mesh.get()->indexCount = (uint32_t)indices.size();

		auto pVertexAttributes = buffer.Write(attributes.data(), attributes.size());
		mesh.get()->vertexAttributes.Set(buffer.data(), pVertexAttributes);

		auto pSubmeshes = buffer.Write(submeshes.data(), submeshes.size());
		mesh.get()->submeshes.Set(buffer.data(), pSubmeshes);

		auto pVertexBuffer = buffer.Write(vertexBuffer.data(), vertexBuffer.size());
		mesh.get()->vertexBuffer.Set(buffer.data(), pVertexBuffer);

		if (indexType == PKElementType::Uint)
		{
			auto pIndexBuffer = buffer.Write(indices.data(), indices.size());
			mesh.get()->indexBuffer.Set(buffer.data(), pIndexBuffer);
		}
		else
		{
			auto pIndexBuffer = buffer.Allocate<unsigned short>(indices.size());
			auto pIndices = pIndexBuffer.get();
			mesh.get()->indexBuffer.Set(buffer.data(), pIndexBuffer);

			for (auto i = 0; i < indices.size(); ++i)
			{
				pIndices[i] = (unsigned short)indices.at(i);
			}
		}

		return WriteAsset(pathDst, buffer);
	}
}