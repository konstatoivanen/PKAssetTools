#include <unordered_map>
#include <map>
#include <mikktspace/mikktspace.h>
#include <tinyobjloader/tiny_obj_loader.h>
#include <meshoptimizer/meshoptimizer.h>
#include <PKAssetLoader.h>
#include "PKAssetWriter.h"
#include "PKStringUtilities.h"
#include "PKFileVersionUtilities.h"
#include "PKMeshUtilities.h"
#include "PKMeshletWriter.h"

namespace PKAssets::Mesh
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
        int GetNumVerticesOfFace([[maybe_unused]] const SMikkTSpaceContext* pContext, [[maybe_unused]] const int iFace)
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

        auto submeshIndex = 0u;

        printf("    Statistics:\n");

        for (const auto& sm : submeshes)
        {
            auto pSmIndices = &indices[sm.firstIndex];
            meshopt_optimizeVertexCache(pSmIndices, pSmIndices, sm.indexCount, total_vertices);

            // Assumes that positions are the first attribute in a vertex.
            meshopt_optimizeOverdraw(pSmIndices, pSmIndices, sm.indexCount, reinterpret_cast<float*>(vertices.data()), total_vertices, stride, 1.05f);
            
            auto statisticsOverdraw = meshopt_analyzeOverdraw(pSmIndices, sm.indexCount, reinterpret_cast<float*>(vertices.data()), total_vertices, stride);
            printf("        Submesh: %i Overdraw: %4.2f, Covered: %ipx, Shared: %ipx\n", submeshIndex++, statisticsOverdraw.overdraw, statisticsOverdraw.pixels_covered, statisticsOverdraw.pixels_shaded);
        }

        meshopt_optimizeVertexFetch(vertices.data(), indices.data(), indices.size(), vertices.data(), total_vertices, stride);

        auto statisticsVertexFetch = meshopt_analyzeVertexFetch(indices.data(), indices.size(), vcount, stride);
        printf("        OverFetch: %4.2f, Fetched: %ibytes\n", statisticsVertexFetch.overfetch, statisticsVertexFetch.bytes_fetched);
    }

    void SplitPositionStream(Buffer& vertices, size_t stride, size_t vertexCount)
    {
        auto positionSize = sizeof(float) * 3;
        Buffer newVertices;
        newVertices.resize(vertices.size());

        auto newStride = stride - positionSize;
        auto stream0 = newVertices.data();
        auto stream1 = newVertices.data() + newStride * vertexCount;

        for (auto i = 0u; i < vertexCount; ++i)
        {
            memcpy(stream0 + newStride * i, vertices.data() + positionSize + stride * i, newStride);
            memcpy(stream1 + positionSize * i, vertices.data() + stride * i, positionSize);
        }

        vertices.swap(newVertices);
    }

    int32_t ConvertFloatToHalfAttribute(Buffer& vertices, size_t stride, size_t offset, size_t elementCount, size_t vertexCount)
    {
        // 4 byte alignment
        auto alignedElements = 2u * ((elementCount + 1u) / 2u);

        auto fullSize = sizeof(float) * elementCount;
        auto halfSize = sizeof(uint16_t) * alignedElements;
        auto newStride = stride - (fullSize - halfSize);
        auto dstTrailOffset = offset + halfSize;
        auto srcTrailOffset = offset + fullSize;

        Buffer newVertices;
        newVertices.resize(newStride * vertexCount);

        for (auto i = 0u; i < vertexCount; ++i)
        {
            if (offset > 0)
            {
                memcpy(newVertices.data() + newStride * i, vertices.data() + stride * i, offset);
            }

            if (dstTrailOffset <= newStride)
            {
                memcpy(newVertices.data() + newStride * i + dstTrailOffset, vertices.data() + stride * i + srcTrailOffset, newStride - dstTrailOffset);
            }

            for (auto j = 0u; j < elementCount; ++j)
            {
                auto vec = (float*)(vertices.data() + stride * i + offset);
                auto half = PackHalf(vec[j]);
                memcpy(newVertices.data() + newStride * i + offset + sizeof(uint16_t) * j, &half, sizeof(uint16_t));
            }

            // Fill possible trailing uint16 to fullfill 4 byte alignment.
            for (auto j = elementCount; j < alignedElements; ++j)
            {
                memset(newVertices.data() + newStride * i + offset * sizeof(uint16_t) * j, 0u, sizeof(uint16_t));
            }
        }

        vertices.swap(newVertices);

        return (int32_t)newStride - (int32_t)stride;
    }

    int WriteMesh(const char* pathSrc, const char* pathDst)
    {
        if (!PKVersionUtilities::IsFileOutOfDate(pathSrc, pathDst))
        {
            return 1;
        }

        auto filename = StringUtilities::ReadFileName(pathSrc);
        printf("Preprocessing mesh: %s \n", filename.c_str());

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, pathSrc, StringUtilities::ReadDirectory(pathSrc).c_str(), true);

        if (!err.empty())
        {
            printf("%s", err.c_str());
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

        auto splitPositionStream = false;
        auto useHalfPrecisionNormals = false;
        auto useHalfPrecisionTangents = false;
        auto useHalfPrecisionUVs = false;
        auto hasNormals = !attrib.normals.empty();
        auto hasUvs = !attrib.texcoords.empty();
        auto hasTangents = hasNormals && hasUvs;
        auto meta = OpenAssetMeta((std::string(pathSrc) + std::string(".pkmeta")).c_str());

        GetAssetMetaOption(meta, "mesh_splitPositionStream", &splitPositionStream);
        GetAssetMetaOption(meta, "mesh_useHalfPrecisionNormals", &useHalfPrecisionNormals);
        GetAssetMetaOption(meta, "mesh_useHalfPrecisionTangents", &useHalfPrecisionTangents);
        GetAssetMetaOption(meta, "mesh_useHalfPrecisionUVs", &useHalfPrecisionUVs);

        CloseAssetMeta(&meta);

        Buffer vertices;
        std::map<IndexSet, uint32_t> indexmap;
        std::vector<uint32_t> indices;
        std::vector<PKSubmesh> submeshes;
        std::vector<PKVertexAttribute> attributes;

        PKVertexAttribute attribute;
        WriteName(attribute.name, PK_MESH_VS_POSITION);
        attribute.type = PKElementType::Float3;
        attribute.size = sizeof(float) * 3;
        attribute.offset = 0;
        attribute.stream = splitPositionStream ? 1 : 0;
        attributes.push_back(attribute);

        auto attributeOffset = splitPositionStream ? 0 : attribute.size;
        auto stride = attribute.size;
        auto offsetNormals = 0u;
        auto offsetTangents = 0u;
        auto offsetUVs = 0u;

        if (hasNormals)
        {
            WriteName(attribute.name, PK_MESH_VS_NORMAL);
            attribute.type = useHalfPrecisionNormals ? PKElementType::Half4 : PKElementType::Float3;
            attribute.size = (useHalfPrecisionNormals ? sizeof(uint16_t) * 4 : sizeof(float)) * 3;
            attribute.offset = attributeOffset;
            attribute.stream = 0;
            attributes.push_back(attribute);

            offsetNormals = stride;
            attributeOffset += attribute.size;
            stride += sizeof(float) * 3;
        }

        if (hasTangents)
        {
            WriteName(attribute.name, PK_MESH_VS_TANGENT);
            attribute.type = useHalfPrecisionTangents ? PKElementType::Half4 : PKElementType::Float4;
            attribute.size = (useHalfPrecisionTangents ? sizeof(uint16_t) * 4 : sizeof(float)) * 4;
            attribute.offset = attributeOffset;
            attribute.stream = 0;
            attributes.push_back(attribute);

            offsetTangents = stride;
            attributeOffset += attribute.size;
            stride += sizeof(float) * 4;
        }

        if (hasUvs)
        {
            WriteName(attribute.name, PK_MESH_VS_TEXCOORD0);
            attribute.type = useHalfPrecisionUVs ? PKElementType::Half2 : PKElementType::Float2;
            attribute.size = (useHalfPrecisionUVs ? sizeof(uint16_t) * 2 : sizeof(float)) * 2;
            attribute.offset = attributeOffset;
            attribute.stream = 0;
            attributes.push_back(attribute);

            offsetUVs = stride;
            attributeOffset += attribute.size;
            stride += sizeof(float) * 2;
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

                vertices.append(pos, 3);

                if (hasNormals)
                {
                    vertices.append(innormals + tri.normal_index * 3, 3);
                }

                if (hasTangents)
                {
                    vertices.append(float4_zero, 4);
                }

                if (hasUvs)
                {
                    vertices.append(inuvs + tri.texcoord_index * 2, 2);
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

        OptimizeMesh(vertices, stride, indices, submeshes);

        auto vcount = (uint32_t)(vertices.size() / stride);
        constexpr auto ushortmax = std::numeric_limits<uint16_t>().max();
        auto indexType = vcount > ushortmax ? PKElementType::Uint : PKElementType::Ushort;

        if (hasTangents)
        {
            auto vfloats = reinterpret_cast<float*>(vertices.data());
            auto fstride = (uint32_t)(stride / sizeof(float));
            CalculateTangents(vfloats, fstride, 0, 3, 6, 10, indices.data(), vcount, (uint32_t)indices.size());
        }

        // Hack: Create these here so that meshlet creation can use them correctly withot reduced precision.
        auto offsetNormalsMeshlet = hasNormals ? offsetNormals : 0xFFFFFFFFu;
        auto offsetTangentsMeshlet = hasTangents ? offsetTangents : 0xFFFFFFFFu;
        auto offsetUvsMeshlet = hasUvs ? offsetUVs : 0xFFFFFFFFu;
        auto strideMeshlet = stride;
        std::vector<float> verticesMeshlet;
        std::vector<uint32_t> indicesMeshlet;
        verticesMeshlet.resize(vertices.size() / sizeof(float));
        memcpy(verticesMeshlet.data(), vertices.data(), vertices.size());
        indicesMeshlet.resize(indices.size());
        memcpy(indicesMeshlet.data(), indices.data(), indices.size() * sizeof(uint32_t));

        if (hasNormals && useHalfPrecisionNormals)
        {
            // Upon stride reduction we need to correct the read strides of other attributes.
            auto strideDelta = ConvertFloatToHalfAttribute(vertices, stride, offsetNormals, 3, vcount);
            stride += strideDelta;
            offsetTangents += strideDelta;
            offsetUVs += strideDelta;
        }

        if (hasTangents && useHalfPrecisionTangents)
        {
            auto strideDelta = ConvertFloatToHalfAttribute(vertices, stride, offsetTangents, 4, vcount);
            stride += strideDelta;
            offsetUVs += strideDelta;
        }

        if (hasUvs && useHalfPrecisionUVs)
        {
            auto strideDelta = ConvertFloatToHalfAttribute(vertices, stride, offsetUVs, 2, vcount);
            stride += strideDelta;
        }

        if (splitPositionStream)
        {
            SplitPositionStream(vertices, stride, vcount);
        }

        auto buffer = PKAssetBuffer();
        buffer.header->type = PKAssetType::Mesh;
        WriteName(buffer.header->name, filename.c_str());
        auto mesh = buffer.Allocate<PKMesh>();

        mesh->indexType = indexType;
        mesh->submeshCount = (uint32_t)submeshes.size();
        mesh->vertexAttributeCount = (uint32_t)attributes.size();
        mesh->vertexCount = vcount;
        mesh->indexCount = (uint32_t)indices.size();

        auto pVertexAttributes = buffer.Write(attributes.data(), attributes.size());
        mesh->vertexAttributes.Set(buffer.data(), pVertexAttributes.get());

        auto pSubmeshes = buffer.Write(submeshes.data(), submeshes.size());
        mesh->submeshes.Set(buffer.data(), pSubmeshes.get());

        auto pVertexBuffer = buffer.Write(vertices.data(), vertices.size());
        mesh->vertexBuffer.Set(buffer.data(), pVertexBuffer.get());

        if (indexType == PKElementType::Uint)
        {
            auto pIndexBuffer = buffer.Write(indices.data(), indices.size());
            mesh->indexBuffer.Set(buffer.data(), pIndexBuffer.get());
        }
        else
        {
            // Align uint16 index array to 4 byte size
            auto alignedIndexCount = 2u * (indices.size() + 1u) / 2u;

            auto pIndexBuffer = buffer.Allocate<unsigned short>(alignedIndexCount);
            auto pIndices = pIndexBuffer.get();
            mesh->indexBuffer.Set(buffer.data(), pIndexBuffer.get());

            for (auto i = 0u; i < indices.size(); ++i)
            {
                pIndices[i] = (unsigned short)indices.at(i);
            }

            for (auto i = indices.size(); i < alignedIndexCount; ++i)
            {
                pIndices[i] = 0u;
            }
        }

        // Create meshlets last to ensure better read coherency
        auto meshletMesh = CreateMeshletMesh
        (
            buffer,
            submeshes,
            verticesMeshlet.data(),
            indicesMeshlet.data(),
            0u,
            offsetUvsMeshlet,
            offsetNormalsMeshlet,
            offsetTangentsMeshlet,
            strideMeshlet,
            vcount,
            (uint32_t)indices.size()
        );

        mesh->meshletMesh.Set(buffer.data(), meshletMesh.get());

        return WriteAsset(pathDst, buffer, false);
    }
}