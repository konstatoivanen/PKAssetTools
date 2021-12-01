#pragma once
#include "PKShaderWriter.h"
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include <unordered_map>
#include <map>
#include <tinyobjloader/tiny_obj_loader.h>

namespace PK::Assets::Mesh
{
	static struct Buffer
	{
		std::vector<char> buffer;
		size_t head = 0;

		template<typename T>
		void append(const T& v)
		{
			auto s = sizeof(T);
			auto nh = head + s;
			
			if (nh > buffer.size())
			{
				buffer.resize(nh);
			}

			memcpy(buffer.data() + head, v, s);
			head += s;
		}
	};

    int WriteMesh(const char* pathSrc, const char* pathDst)
    {
        auto filename = StringUtilities::ReadFileName(pathSrc);
        printf("Preprocessing mesh: %s \n", filename.c_str());

        auto buffer = PKAssetBuffer();
        buffer.header.get()->type = PKAssetType::Mesh;
        WriteName(buffer.header.get()->name, filename.c_str());

        auto mesh = buffer.Allocate<PKMesh>();
    
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

		if (!attrib.vertices.empty())
		{
			printf("Mesh doesn't contain vertices");
			return -1;
		}

		if (!success)
		{
			printf("Failed to load .obj");
			return -1;
		}

		auto hasNormals = attrib.normals.empty();
		auto hasUvs = attrib.texcoords.empty();
		auto ushortmax = std::numeric_limits<unsigned short>().max();
		auto indexType = attrib.vertices.size() > ushortmax ? PKElementType::Uint : PKElementType::Ushort;
		mesh.get()->indexType = indexType;

		Buffer indexBuffer;
		Buffer vertexBuffer;
		std::vector<PKIndexRange> submeshes;
		std::vector<PKVertexAttribute> attributes;

		attributes.emplace_back("in_POSITION", PKElementType::Float3, sizeof(float) * 3, 0);

		if (hasNormals)
		{
			attributes.emplace_back("in_NORMAL", PKElementType::Float3, sizeof(float) * 3, 0);
		}

		if (hasUvs)
		{
			attributes.emplace_back("in_TEXCOORD0", PKElementType::Float2, sizeof(float) * 2, 0);
		}

		float bbmax[3]{};
		float bbmin[3]{};
		bbmax[0] = -std::numeric_limits<float>().max();
		bbmax[1] = -std::numeric_limits<float>().max();
		bbmax[2] = -std::numeric_limits<float>().max();
		bbmin[0] = std::numeric_limits<float>().max();
		bbmin[1] = std::numeric_limits<float>().max();
		bbmin[2] = std::numeric_limits<float>().max();

		auto invertices = attrib.vertices.data();
		auto innormals = attrib.normals.data();
		auto inuvs = attrib.texcoords.data();

		auto index = 0u;
		auto indexCount = 0u;

		for (size_t i = 0; i < shapes.size(); ++i)
		{
			auto& tris = shapes.at(i).mesh.indices;
			auto tcount = (uint_t)tris.size();
			submeshes.emplace_back(indexCount, tcount);

			for (uint_t j = 0; j < tcount; ++j)
			{
				auto& tri = tris.at(j);

				if (indexType == PKElementType::Ushort)
				{
					indexBuffer.append((unsigned short)index++);
				}
				else
				{
					indexBuffer.append(index++);
				}

				float pos[3];
				pos[0] = invertices[tri.vertex_index * 3 + 0];
				pos[1] = invertices[tri.vertex_index * 3 + 1];
				pos[2] = invertices[tri.vertex_index * 3 + 2];

				vertexBuffer.append(pos[0]);
				vertexBuffer.append(pos[1]);
				vertexBuffer.append(pos[2]);

				if (hasNormals)
				{
					vertexBuffer.append(innormals[tri.vertex_index * 3 + 0]);
					vertexBuffer.append(innormals[tri.vertex_index * 3 + 1]);
					vertexBuffer.append(innormals[tri.vertex_index * 3 + 2]);
				}

				if (hasUvs)
				{
					vertexBuffer.append(inuvs[tri.vertex_index * 3 + 0]);
					vertexBuffer.append(inuvs[tri.vertex_index * 3 + 1]);
					vertexBuffer.append(inuvs[tri.vertex_index * 3 + 2]);
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

			indexCount += tcount;
		}

		memcpy(bbmin, mesh.get()->bbmin, sizeof(bbmin));
		memcpy(bbmax, mesh.get()->bbmax, sizeof(bbmax));

		mesh.get()->indexBufferSize = indexCount;
		mesh.get()->submeshesSize = submeshes.size();
		mesh.get()->vertexAttributesSize = attributes.size();
		mesh.get()->vertexBufferSize = vertexBuffer.buffer.size();

		auto wIndexBuffer = buffer.Write<void>(indexBuffer.buffer.data(), indexBuffer.buffer.size());
		mesh.get()->indexBuffer.Set(buffer.data(), wIndexBuffer);

		auto wVertexBuffer = buffer.Write<void>(vertexBuffer.buffer.data(), vertexBuffer.buffer.size());
		mesh.get()->vertexBuffer.Set(buffer.data(), wVertexBuffer);
		
		auto wVertexAttributes = buffer.Write(attributes.data(), attributes.size());
		mesh.get()->vertexAttributes.Set(buffer.data(), wVertexAttributes);

		auto wSubmeshes = buffer.Write(submeshes.data(), submeshes.size());
		mesh.get()->submeshes.Set(buffer.data(), wSubmeshes);
	}
}