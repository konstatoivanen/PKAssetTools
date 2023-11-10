#include <meshoptimizer/meshoptimizer.h>
#include "PKMeshUtilities.h"
#include "PKMeshletWriter.h"

namespace PK::Assets::Mesh
{
    uint32_t EncodeVertexPosition(const float* pPosition, const meshopt_Bounds& bounds)
    {
        float position[3] =
        {
            ((pPosition[0] - bounds.center[0]) / bounds.radius) * 0.5f + 0.5f,
            ((pPosition[1] - bounds.center[1]) / bounds.radius) * 0.5f + 0.5f,
            ((pPosition[2] - bounds.center[2]) / bounds.radius) * 0.5f + 0.5f
        };

        int16_t qposition[3];
        qposition[0] = (int16_t)(position[0] * 2047.0f);
        qposition[1] = (int16_t)(position[1] * 2047.0f);
        qposition[2] = (int16_t)(position[2] * 1023.0f);

        if (qposition[0] < 0) { qposition[0] = 0; }
        if (qposition[1] < 0) { qposition[1] = 0; }
        if (qposition[2] < 0) { qposition[2] = 0; }
        if (qposition[0] > 2047) { qposition[0] = 2047; }
        if (qposition[1] > 2047) { qposition[1] = 2047; }
        if (qposition[2] > 1023) { qposition[2] = 1023; }

        uint32_t encoded = 0u;
        encoded = ((uint32_t)qposition[0]) & 0x7FFu;
        encoded |= (((uint32_t)qposition[1]) & 0x7FFu) << 11u;
        encoded |= (((uint32_t)qposition[1]) & 0x3FFu) << 10u;
        return encoded;
    }

    uint32_t EncodeTexcoord(const float* pTexcoord)
    {
        auto u = (uint32_t)PackHalf(pTexcoord[0]);
        auto v = (uint32_t)PackHalf(pTexcoord[1]);
        return (u & 0xFFFFu) | ((v & 0xFFFFu) << 16u);
    }

    uint32_t EncodeNormal(const float* pNormal)
    {
        float octauv[2];
        OctaEncode(pNormal, octauv);

        auto ui = (int32_t)(octauv[0] * 65535.0f);
        auto vi = (int32_t)(octauv[1] * 65535.0f);
        if (ui < 0) { ui = 0; }
        if (vi < 0) { vi = 0; }
        if (ui > 65535) { ui = 65535; }
        if (vi > 65535) { vi = 65535; }

        auto u = (uint32_t)ui;
        auto v = (uint32_t)vi;

        return (u & 0xFFFFu) | ((v & 0xFFFFu) << 16u);
    }

    uint32_t EncodeTangent(const float* pTangent)
    {
        float octauv[2];
        OctaEncode(pTangent, octauv);
        uint8_t sign = pTangent[4] < 0.0f ? 0u : 3u;

        auto ui = (int32_t)(octauv[0] * 32767.0f);
        auto vi = (int32_t)(octauv[1] * 32767.0f);
        if (ui < 0) { ui = 0; }
        if (vi < 0) { vi = 0; }
        if (ui > 32767) { ui = 32767; }
        if (vi > 32767) { vi = 32767; }

        auto u = (uint32_t)ui;
        auto v = (uint32_t)vi;

        return (u & 0x7FFFu) | ((v & 0x7FFFu) << 15u) | ((sign & 0x3) << 30u);
    }

    WritePtr<Meshlet::PKMesh> CreateMeshletMesh(PKAssetBuffer& buffer,
                                                const std::vector<PKSubmesh>& submeshes,
                                                float* vertices,
                                                uint32_t* indices,
                                                uint32_t offsetPosition,
                                                uint32_t offsetTexcoord,
                                                uint32_t offsetNormal,
                                                uint32_t offsetTangent,
                                                uint32_t vertexStride,
                                                uint32_t vertexCount,
                                                uint32_t indexCount)
    {
        const size_t max_vertices = 64ull;
        const size_t max_triangles = 124ull;
        const float cone_weight = 0.5f;

        auto hasTexcoords = offsetTexcoord != 0xFFFFFFFFu;
        auto hasNormals = offsetNormal != 0xFFFFFFFFu;
        auto hasTangents = offsetTangent != 0xFFFFFFFFu;

        std::vector<uint8_t> out_indices;
        std::vector<Meshlet::PKVertex> out_vertices;
        std::vector<Meshlet::PKSubmesh> out_submeshes;
        std::vector<Meshlet::PKMeshlet> out_meshlets;

        auto totalVertices = 0ull;
        auto totalTriangles = 0ull;

        // no offset for attributes. @TODO maybe they should have.
        auto sm_stridef32 = vertexStride / sizeof(float);
        auto sm_positionsf32 = vertices + (offsetPosition / sizeof(float));
        auto sm_texcoordsf32 = hasTexcoords ? vertices + (offsetTexcoord / sizeof(float)) : nullptr;
        auto sm_normalsf32 = hasNormals ? vertices + (offsetNormal / sizeof(float)) : nullptr;
        auto sm_tangentsf32 = hasTangents ? vertices + (offsetTangent / sizeof(float)) : nullptr;
            
        size_t max_meshlets = meshopt_buildMeshletsBound(indexCount, max_vertices, max_triangles);
        std::vector<meshopt_Meshlet> meshlets(max_meshlets);
        std::vector<unsigned int> meshlet_vertices(max_meshlets * max_vertices);
        std::vector<unsigned char> meshlet_triangles(max_meshlets * max_triangles * 3);

        for (const auto& sm : submeshes)
        {
            auto sm_indices = indices + sm.firstIndex;

            size_t meshlet_count = meshopt_buildMeshlets
            (
                meshlets.data(), 
                meshlet_vertices.data(), 
                meshlet_triangles.data(), 
                sm_indices,
                sm.indexCount, 
                sm_positionsf32,
                vertexCount, 
                vertexStride, 
                max_vertices, 
                max_triangles, 
                cone_weight
            );

            Meshlet::PKSubmesh meshletSubmesh{};
            meshletSubmesh.firstTriangle = (uint32_t)(out_indices.size() / 3ull);
            meshletSubmesh.firstVertex = (uint32_t)out_vertices.size();
            meshletSubmesh.firstMeshlet = (uint32_t)out_meshlets.size();
            meshletSubmesh.triangleCount = 0u;
            meshletSubmesh.vertexCount = 0u;
            meshletSubmesh.meshletCount = (uint32_t)meshlet_count;
            
            meshletSubmesh.bbmin[0] = sm.bbmin[0];
            meshletSubmesh.bbmin[1] = sm.bbmin[1];
            meshletSubmesh.bbmin[2] = sm.bbmin[2];

            meshletSubmesh.bbmax[0] = sm.bbmax[0];
            meshletSubmesh.bbmax[1] = sm.bbmax[1];
            meshletSubmesh.bbmax[2] = sm.bbmax[2];

            //float center[3] =
            //{
            //    (sm.bbmin[0] + sm.bbmax[0]) / 2.0f,
            //    (sm.bbmin[1] + sm.bbmax[1]) / 2.0f,
            //    (sm.bbmin[2] + sm.bbmax[2]) / 2.0f,
            //};

            for (auto i = 0u; i < meshlet_count; ++i)
            {
                const auto& meshlet = meshlets.at(i);

                auto bounds = meshopt_computeMeshletBounds
                (
                    meshlet_vertices.data() + meshlet.vertex_offset,
                    meshlet_triangles.data() + meshlet.triangle_offset,
                    meshlet.triangle_count,
                    sm_positionsf32,
                    vertexCount,
                    vertexStride
                );

                auto indicesOffset = out_indices.size();
                auto triangleOffset = indicesOffset / 3ull;
                auto verticesOffset = out_vertices.size();
                
                meshletSubmesh.triangleCount += meshlet.triangle_count;
                meshletSubmesh.vertexCount += meshlet.vertex_count;

                Meshlet::PKMeshlet pkmeshlet;
                pkmeshlet.firstTriangle = (uint32_t)triangleOffset;
                pkmeshlet.firstVertex = (uint32_t)verticesOffset;
                pkmeshlet.triangleCount = meshlet.triangle_count;
                pkmeshlet.vertexCount = meshlet.vertex_count;

                float octauv[2];
                OctaEncode(bounds.cone_axis, octauv);
                
                pkmeshlet.coneAxis[0] = PackHalf(octauv[0]);
                pkmeshlet.coneAxis[1] = PackHalf(octauv[1]);
                pkmeshlet.center[0] = PackHalf(bounds.center[0]);// - center[0]);
                pkmeshlet.center[1] = PackHalf(bounds.center[1]);// - center[1]);
                pkmeshlet.center[2] = PackHalf(bounds.center[2]);// - center[2]);
                pkmeshlet.radius = PackHalf(bounds.radius);
                pkmeshlet.coneApex[0] = PackHalf(bounds.cone_apex[0]);// - center[0]);
                pkmeshlet.coneApex[1] = PackHalf(bounds.cone_apex[1]);// - center[1]);
                pkmeshlet.coneApex[2] = PackHalf(bounds.cone_apex[2]);// - center[2]);
                pkmeshlet.coneangle = PackHalf(bounds.radius);

                out_indices.resize(out_indices.size() + meshlet.triangle_count * 3);
                memcpy(out_indices.data() + indicesOffset, meshlet_triangles.data() + meshlet.triangle_offset * 3u, meshlet.triangle_count * 3u);

                for (auto j = 0u; j < meshlet.vertex_count; ++j)
                {
                    auto vertexIndex = meshlet_vertices[meshlet.vertex_offset + j];
                    auto pPosition = sm_positionsf32 + vertexIndex * sm_stridef32;
                    auto pTexcoord = sm_texcoordsf32 + vertexIndex * sm_stridef32;
                    auto pNormal = sm_normalsf32 + vertexIndex * sm_stridef32;
                    auto pTangent = sm_tangentsf32 + vertexIndex * sm_stridef32;
                    
                    Meshlet::PKVertex vertex = { 0u, 0u, 0u };
                    vertex.position = EncodeVertexPosition(pPosition, bounds);
                    vertex.texcoord = hasTexcoords ? EncodeTexcoord(pTexcoord) : 0u;
                    vertex.normal = hasNormals ? EncodeNormal(pNormal) : 0u;
                    vertex.tangent = hasTangents ? EncodeTangent(pTangent) : 0u;
                    out_vertices.push_back(vertex);
                }

                out_meshlets.push_back(pkmeshlet);
            }

            out_submeshes.push_back(meshletSubmesh);
        }

        // Align indices array size to 4bytes
        auto alignedIndicesSize = 4u * ((out_indices.size() + 3ull) / 4u);

        if (out_indices.size() < alignedIndicesSize)
        {
            out_indices.resize(alignedIndicesSize);
            auto padding = alignedIndicesSize - out_indices.size();
            memset(out_indices.data() + out_indices.size(), 0u, padding);
        }

        auto mesh = buffer.Allocate<Meshlet::PKMesh>();
        
        mesh->triangleCount = (uint32_t)(out_indices.size() / 3ull);
        mesh->vertexCount = (uint32_t)out_vertices.size();
        mesh->submeshCount = (uint32_t)out_submeshes.size();
        mesh->meshletCount = (uint32_t)out_meshlets.size();
        
        auto pMeshlets = buffer.Write(out_meshlets.data(), out_meshlets.size());
        mesh->meshlets.Set(buffer.data(), pMeshlets.get());

        auto pSubmeshes = buffer.Write(out_submeshes.data(), out_submeshes.size());
        mesh->submeshes.Set(buffer.data(), pSubmeshes.get());

        auto pVertices = buffer.Write(out_vertices.data(), out_vertices.size());
        mesh->vertices.Set(buffer.data(), pVertices.get());

        auto pIndices = buffer.Write(out_indices.data(), out_indices.size());
        mesh->indices.Set(buffer.data(), pIndices.get());

        printf("    Meshlet Statistics:\n");
        printf("        Vertex Count: %i -> %i\n", vertexCount, (uint32_t)out_vertices.size());
        printf("        Triangle Count: %i -> %i\n", indexCount / 3u, (uint32_t)(out_indices.size() / 3ull));
        printf("        Vertex Buffer Size: %i -> %i\n", (uint32_t)(vertexCount * vertexStride), (uint32_t)(out_vertices.size() * sizeof(Meshlet::PKVertex)));
        printf("        Triangle Buffer Size: %i -> %i\n", (uint32_t)(indexCount * sizeof(uint32_t)), (uint32_t)(out_indices.size()));
        printf("        Meshlet Count: %i\n", (uint32_t)out_meshlets.size());

        return mesh;
    }
}