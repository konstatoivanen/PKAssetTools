#include <meshoptimizer/meshoptimizer.h>
#include "PKMeshUtilities.h"
#include "PKMeshletWriter.h"

namespace PKAssets::Mesh
{
    static void CalculateMeshletCenterExtents(const float* positions,
                                              const uint32_t* vertexIndices,
                                              uint32_t vertexStridef32,
                                              uint32_t vertexFirst,
                                              uint32_t vertexCount,
                                              float* center,
                                              float* extents)
    {
        float bbmin[3];
        float bbmax[3];
        bbmin[0] = std::numeric_limits<float>().max();
        bbmin[1] = std::numeric_limits<float>().max();
        bbmin[2] = std::numeric_limits<float>().max();
        bbmax[0] = -std::numeric_limits<float>().max();
        bbmax[1] = -std::numeric_limits<float>().max();
        bbmax[2] = -std::numeric_limits<float>().max();

        for (auto i = 0u; i < vertexCount; ++i)
        {
            auto vertexIndex = vertexIndices[vertexFirst + i];
            auto pPosition = positions + vertexIndex * vertexStridef32;

            for (auto j = 0u; j < 3u; ++j)
            {
                if (pPosition[j] < bbmin[j])
                {
                    bbmin[j] = pPosition[j];
                }

                if (pPosition[j] > bbmax[j])
                {
                    bbmax[j] = pPosition[j];
                }
            }
        }

        for (auto i = 0u; i < 3; ++i)
        {
            center[i] = bbmin[i] + (bbmax[i] - bbmin[i]) * 0.5f;
            extents[i] = (bbmax[i] - bbmin[i]) * 0.5f;
        }
    }

    WritePtr<PKMeshletMesh> CreateMeshletMesh(PKAssetBuffer& buffer,
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
        auto hasTexcoords = offsetTexcoord != 0xFFFFFFFFu;
        auto hasNormals = offsetNormal != 0xFFFFFFFFu;
        auto hasTangents = offsetTangent != 0xFFFFFFFFu;

        std::vector<uint8_t> out_indices;
        std::vector<PKMeshletVertex> out_vertices;
        std::vector<PKMeshletSubmesh> out_submeshes;
        std::vector<PKMeshlet> out_meshlets;

        // no offset for attributes. @TODO maybe they should have.
        auto sm_stridef32 = vertexStride / sizeof(float);
        auto sm_positionsf32 = vertices + (offsetPosition / sizeof(float));
        auto sm_texcoordsf32 = hasTexcoords ? vertices + (offsetTexcoord / sizeof(float)) : nullptr;
        auto sm_normalsf32 = hasNormals ? vertices + (offsetNormal / sizeof(float)) : nullptr;
        auto sm_tangentsf32 = hasTangents ? vertices + (offsetTangent / sizeof(float)) : nullptr;
            
        size_t max_meshlets = meshopt_buildMeshletsBound(indexCount, PK_MESHLET_MAX_VERTICES, PK_MESHLET_MAX_TRIANGLES);
        std::vector<meshopt_Meshlet> meshlets(max_meshlets);
        std::vector<unsigned int> meshlet_vertices(max_meshlets * PK_MESHLET_MAX_VERTICES);
        std::vector<unsigned char> meshlet_triangles(max_meshlets * PK_MESHLET_MAX_TRIANGLES * 3);

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
                PK_MESHLET_MAX_VERTICES,
                PK_MESHLET_MAX_TRIANGLES,
                PK_MESHLET_CONE_WEIGHT
            );

            PKMeshletSubmesh meshletSubmesh{};
            meshletSubmesh.firstMeshlet = (uint32_t)out_meshlets.size();
            meshletSubmesh.meshletCount = (uint32_t)meshlet_count;
            
            meshletSubmesh.bbmin[0] = sm.bbmin[0];
            meshletSubmesh.bbmin[1] = sm.bbmin[1];
            meshletSubmesh.bbmin[2] = sm.bbmin[2];

            meshletSubmesh.bbmax[0] = sm.bbmax[0];
            meshletSubmesh.bbmax[1] = sm.bbmax[1];
            meshletSubmesh.bbmax[2] = sm.bbmax[2];

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

                float center[3];
                float extents[3];
                CalculateMeshletCenterExtents(sm_positionsf32, meshlet_vertices.data(), (uint32_t)sm_stridef32, meshlet.vertex_offset, meshlet.vertex_count, center, extents);
                
                PKMeshlet pkmeshlet = PackPKMeshlet
                (
                    (uint32_t)verticesOffset,
                    (uint32_t)triangleOffset,
                    meshlet.vertex_count,
                    meshlet.triangle_count,
                    bounds.cone_axis_s8,
                    bounds.cone_cutoff_s8,
                    bounds.cone_apex,
                    center,
                    extents
                );

                out_indices.resize(out_indices.size() + meshlet.triangle_count * 3);
                memcpy(out_indices.data() + indicesOffset, meshlet_triangles.data() + meshlet.triangle_offset, meshlet.triangle_count * 3u);

                for (auto j = 0u; j < meshlet.vertex_count; ++j)
                {
                    auto vertexIndex = meshlet_vertices[meshlet.vertex_offset + j];
                    auto pPosition = sm_positionsf32 + vertexIndex * sm_stridef32;
                    auto pTexcoord = sm_texcoordsf32 + vertexIndex * sm_stridef32;
                    auto pNormal = sm_normalsf32 + vertexIndex * sm_stridef32;
                    auto pTangent = sm_tangentsf32 + vertexIndex * sm_stridef32;
                    
                    PKMeshletVertex vertex = PackPKMeshletVertex
                    (
                        pPosition,
                        hasTexcoords ? pTexcoord : nullptr,
                        hasNormals ? pNormal : nullptr,
                        hasTangents ? pTangent : nullptr,
                        sm.bbmin,
                        sm.bbmax
                    );

                    out_vertices.push_back(vertex);
                }

                out_meshlets.push_back(pkmeshlet);
            }

            out_submeshes.push_back(meshletSubmesh);
        }

        // Align triangles array size to 4bytes
        {
            auto alignedIndicesSize = 12u * ((out_indices.size() + 11ull) / 12u);

            if (out_indices.size() < alignedIndicesSize)
            {
                out_indices.resize(alignedIndicesSize);
                auto padding = alignedIndicesSize - out_indices.size();
                memset(out_indices.data() + out_indices.size(), 0u, padding);
            }
        }

        auto mesh = buffer.Allocate<PKMeshletMesh>();
        
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
        printf("        Vertex Buffer Size: %i -> %i\n", (uint32_t)(vertexCount * vertexStride), (uint32_t)(out_vertices.size() * sizeof(PKMeshletVertex)));
        printf("        Triangle Buffer Size: %i -> %i\n", (uint32_t)(indexCount * sizeof(uint32_t)), (uint32_t)(out_indices.size()));
        printf("        Meshlet Count: %i\n", (uint32_t)out_meshlets.size());

        return mesh;
    }
}