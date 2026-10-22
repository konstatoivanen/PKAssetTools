#include <unordered_map>
#include <meshoptimizer/meshoptimizer.h>
#define CLUSTERLOD_IMPLEMENTATION
#include <meshoptimizer/pkmod_clusterlod.h>
#include "PKMeshUtilities.h"
#include "PKMeshletWriter.h"

namespace PKAssets::Mesh
{
    static const uint32_t PK_DAG_MAX_GROUP_SIZE = 12u;
    static const uint32_t PK_DAG_TARGET_GROUP_SIZE = 6u;
    static const uint32_t PK_DAG_MAX_LEVELS = 5u;
    static const uint32_t PK_DAG_DECIMATE_FACTOR = 2u;
    static const float PK_DAG_MIN_SIMPLIFICATION_FACTOR_MESHLET = 0.85f;
    static const float PK_DAG_ERROR_ACUMULATION = 1.0f;
    static const float PK_DAG_EDGE_ERROR_LIMIT = 0.125f;


    static WritePtr<PKMeshletMesh> CreateMeshletMesh(PKAssetBuffer& buffer,
        const std::vector<PKSubmesh>& submeshes,
        float* vertices,
        uint32_t* indices,
        uint32_t offsetTexcoord,
        uint32_t offsetNormal,
        uint32_t offsetTangent,
        uint32_t offsetColor,
        uint32_t vertexStride,
        uint32_t vertexCount,
        uint32_t indexCount)
    {
        auto hasTexcoords = offsetTexcoord != 0xFFFFFFFFu;
        auto hasNormals = offsetNormal != 0xFFFFFFFFu;
        auto hasTangents = offsetTangent != 0xFFFFFFFFu;
        auto hasColors = offsetColor != 0xFFFFFFFFu;
        auto sm_stridef32 = vertexStride / sizeof(float);
        auto sm_texcoordsf32 = hasTexcoords ? vertices + (offsetTexcoord / sizeof(float)) : nullptr;
        auto sm_normalsf32 = hasNormals ? vertices + (offsetNormal / sizeof(float)) : nullptr;
        auto sm_tangentsf32 = hasTangents ? vertices + (offsetTangent / sizeof(float)) : nullptr;
        auto sm_colorsf32 = hasColors ? vertices + (offsetColor / sizeof(float)) : nullptr;

        std::vector<uint8_t> out_indices;
        std::vector<PKMeshletVertex> out_vertices;
        std::vector<PKMeshletSubmesh> out_submeshes;
        std::vector<PKMeshlet> out_meshlets;
        std::vector<clodGroup> groups;

        clodConfig config{};
        config.max_vertices = PK_MESHLET_MAX_VERTICES;
        config.min_triangles = PK_MESHLET_MAX_TRIANGLES / 3;
        config.max_triangles = PK_MESHLET_MAX_TRIANGLES;
        config.partition_spatial = true;
        config.partition_sort = true;
        config.partition_size = PK_DAG_TARGET_GROUP_SIZE;
        config.partition_depth = PK_DAG_MAX_LEVELS;
        config.cluster_spatial = false;
        config.cluster_fill_weight = 0.0f;
        config.cluster_split_factor = 2.0f;
        config.cluster_cone_factor = PK_MESHLET_CONE_WEIGHT;
        config.simplify_ratio = 1.0f / PK_DAG_DECIMATE_FACTOR;
        config.simplify_threshold = PK_DAG_MIN_SIMPLIFICATION_FACTOR_MESHLET;
        config.simplify_error_merge_previous = 1.0f;
        config.simplify_error_merge_additive = PK_DAG_ERROR_ACUMULATION;
        config.simplify_error_factor_sloppy = 2.0f;
        config.simplify_error_edge_limit = PK_DAG_EDGE_ERROR_LIMIT;
        config.simplify_permissive = true;
        config.simplify_fallback_permissive = false; // note: by default we run in permissive mode, but it's also possible to disable that and use it only as a fallback
        config.simplify_fallback_sloppy = true;
        config.simplify_regularize = false;
        config.optimize_bounds = false; // Do not enable. Return values from 
        config.optimize_clusters = true;

        // Maximum of 11 attributes between position and texcoord.
        const float attribute_weights[] = 
        {
            0.5f, 0.5f, 0.5f, // Normal 
            0.5f, 0.5f, 0.5f, 0.75f // Tangent
        };

        clodMesh ctx = {};
        ctx.indices = indices;
        ctx.index_count = indexCount;
        ctx.vertex_count = vertexCount;
        ctx.vertex_positions = vertices;
        ctx.vertex_positions_stride = vertexStride;
        ctx.vertex_attributes = sm_stridef32 > 3 ? vertices + 3 : nullptr; 

        if (hasNormals)
        {
            ctx.attribute_count += 3u;
        }

        if (hasTangents)
        {
            ctx.attribute_count += 4u;
        }

        if (ctx.attribute_count > 0)
        {
            ctx.attribute_weights = attribute_weights;
            ctx.vertex_attributes_stride = vertexStride;
        }

        if (hasTexcoords && (offsetTexcoord / sizeof(float)) == sm_stridef32 - 2ull)
        {
            auto maskOffset = (offsetTexcoord / sizeof(float)) - 3ull;
            ctx.attribute_protect_mask = (1u << (maskOffset + 0u)) | (1 << (maskOffset + 1u));
        }

        for (const auto& sm : submeshes)
        {
            auto first_meshlet = out_meshlets.size();
            ctx.indices = indices + sm.firstIndex;
            ctx.index_count = sm.indexCount;

            groups.clear();

            clodBuild(config, ctx, [&](clodGroup group, const clodCluster* clusters, size_t cluster_count) -> int
            {
                groups.push_back(group);

                for (auto i = 0u; i < cluster_count; ++i)
                {
                    const auto& meshlet = clusters[i];
                    const auto indices_offset = out_indices.size();
                    const auto triangle_offset = indices_offset / 3ull;
                    const auto vertices_offset = out_vertices.size();

                    uint32_t meshlet_triangle_count = meshlet.index_count / 3;
                    uint32_t meshlet_vertices[PK_MESHLET_MAX_VERTICES]{};
                    uint8_t meshlet_triangles[PK_MESHLET_MAX_TRIANGLES * 3u]{};
                    clodLocalIndices(meshlet_vertices, meshlet_triangles, meshlet.indices, meshlet.index_count);
                    
                    auto bounds = meshopt_computeMeshletBounds
                    (
                        meshlet_vertices,
                        meshlet_triangles,
                        meshlet_triangle_count,
                        ctx.vertex_positions,
                        ctx.vertex_count,
                        ctx.vertex_positions_stride
                    );

                    // meshopt does sphere bounds where the center is not equal/correct
                    float center[3];
                    float extents[3];
                    CalculateBounds(ctx.vertex_positions, meshlet_vertices, (uint32_t)sm_stridef32, meshlet.vertex_count, center, extents);

                    auto pkmeshlet = PackPKMeshlet
                    (
                        (uint32_t)vertices_offset,
                        (uint32_t)triangle_offset,
                        meshlet.vertex_count,
                        meshlet_triangle_count,
                        bounds.cone_axis_s8,
                        bounds.cone_cutoff_s8,
                        bounds.cone_apex,
                        center,
                        extents,

                        meshlet.bounds.center,
                        std::min(meshlet.bounds.error, PK_MESHLET_LOD_MAX_ERROR),
                        group.simplified.center,
                        std::min(group.simplified.error, PK_MESHLET_LOD_MAX_ERROR)
                    );

                    out_indices.resize(out_indices.size() + meshlet.index_count);
                    memcpy(out_indices.data() + indices_offset, meshlet_triangles, meshlet.index_count);

                    for (auto j = 0u; j < meshlet.vertex_count; ++j)
                    {
                        auto vertex_index = meshlet_vertices[j];
                        auto pPosition = ctx.vertex_positions + vertex_index * sm_stridef32;
                        auto pTexcoord = sm_texcoordsf32 + vertex_index * sm_stridef32;
                        auto pNormal = sm_normalsf32 + vertex_index * sm_stridef32;
                        auto pTangent = sm_tangentsf32 + vertex_index * sm_stridef32;
                        auto pColors = sm_colorsf32 + vertex_index * sm_stridef32;

                        auto vertex = PackPKMeshletVertex
                        (
                            pPosition,
                            hasTexcoords ? pTexcoord : nullptr,
                            hasNormals ? pNormal : nullptr,
                            hasTangents ? pTangent : nullptr,
                            hasColors ? pColors : nullptr,
                            sm.bbmin,
                            sm.bbmax
                        );

                        out_vertices.push_back(vertex);
                    }

                    out_meshlets.push_back(pkmeshlet);
                }

                return int(groups.size() - 1);
            });

            if (out_meshlets.size() == first_meshlet)
            {
                continue;
            }

            PKMeshletSubmesh meshletSubmesh{};
            meshletSubmesh.firstMeshlet = (uint32_t)first_meshlet;
            meshletSubmesh.meshletCount = (uint32_t)(out_meshlets.size() - first_meshlet);
            memcpy(meshletSubmesh.bbmin, sm.bbmin, sizeof(float) * 3u);
            memcpy(meshletSubmesh.bbmax, sm.bbmax, sizeof(float) * 3u);
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