#pragma once
#include "PKShaderWriter.h"
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include <unordered_map>
#include <map>
#include <mikktspace/mikktspace.h>
#include <tinyobjloader/tiny_obj_loader.h>

namespace PK::Assets::Mesh
{
	namespace MikktsInterface1
	{
		struct PKMeshData
		{
			float* vertices = nullptr;
			unsigned int stride = 0;
			unsigned int vertexOffset = 0;
			unsigned int normalOffset = 0;
			unsigned int tangentOffset = 0;
			unsigned int texcoordOffset = 0;
			const unsigned int* indices = nullptr;
			unsigned int vcount = 0;
			unsigned int icount = 0;
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
						  uint_t stride, 
					      uint_t vertexOffset, 
						  uint_t normalOffset, 
						  uint_t tangentOffset, 
						  uint_t texcoordOffset, 
						  const uint_t* indices, 
						  uint_t vcount, 
						  uint_t icount)
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
		void append(const T* src, uint_t count)
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
		uint_t position = 0u;
		uint_t normal = 0u;
		uint_t uv = 0u;

		inline bool operator < (const IndexSet& r) const noexcept
		{
			return memcmp(reinterpret_cast<const void*>(this), reinterpret_cast<const void*>(&r), sizeof(IndexSet)) < 0;
		}
	};

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
		std::map<IndexSet, uint_t> indexmap;
		std::vector<uint_t> indices;
		std::vector<PKIndexRange> submeshes;
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
		float bbmax[3]{}, bbmin[3]{};
		bbmax[0] = bbmax[1] = bbmax[2] = -std::numeric_limits<float>().max();
		bbmin[0] = bbmin[1] = bbmin[2] =  std::numeric_limits<float>().max();

		auto invertices = attrib.vertices.data();
		auto innormals = attrib.normals.data();
		auto inuvs = attrib.texcoords.data();

		auto index = 0u;

		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& tris = shapes.at(i).mesh.indices;
			auto tcount = (uint_t)tris.size();
			submeshes.push_back({ (uint_t)indices.size(), tcount });

			for (uint_t j = 0; j < tcount; ++j)
			{
				auto& tri = tris.at(j);
				IndexSet triKey = { (uint_t)tri.vertex_index, (uint_t)tri.normal_index, (uint_t)tri.texcoord_index };
				
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
					if (bbmax[k] < pos[k])
					{
						bbmax[k] = pos[k];
					}

					if (bbmin[k] > pos[k])
					{
						bbmin[k] = pos[k];
					}
				}
			}
		}

		auto vcount = (uint_t)(vertexBuffer.size() / stride);
		auto ushortmax = std::numeric_limits<unsigned short>().max();
		auto indexType = vcount > ushortmax ? PKElementType::Uint : PKElementType::Ushort;
		auto indexSize = indexType == PKElementType::Uint ? sizeof(uint_t) : sizeof(unsigned short);

		if (hasNormals && hasUvs)
		{
			auto vfloats = reinterpret_cast<float*>(vertexBuffer.data());
			auto fstride = (uint_t)(stride / sizeof(float));
			CalculateTangents(vfloats, fstride, 0, 3, 6, 10, indices.data(), vcount, (uint_t)indices.size());
		}

		auto buffer = PKAssetBuffer();
        buffer.header.get()->type = PKAssetType::Mesh;
        WriteName(buffer.header.get()->name, filename.c_str());
        
		auto mesh = buffer.Allocate<PKMesh>();
		
		memcpy(mesh.get()->bbmin, bbmin, sizeof(bbmin));
		memcpy(mesh.get()->bbmax, bbmax, sizeof(bbmax));

		mesh.get()->indexType = indexType;
		mesh.get()->submeshCount = (uint_t)submeshes.size();
		mesh.get()->vertexAttributeCount = (uint_t)attributes.size();
		mesh.get()->vertexCount = (uint_t)(vertexBuffer.size() / stride);
		mesh.get()->indexCount = (uint_t)indices.size();

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