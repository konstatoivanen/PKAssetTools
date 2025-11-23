#include <unordered_map>
#include <meshoptimizer/meshoptimizer.h>
#include <METIS/metis.h>
#include "PKMeshUtilities.h"
#include "PKMeshletWriter.h"

namespace PKAssets::Mesh
{
    static const uint32_t PK_DAG_MAX_GROUP_SIZE = 12u;
    static const uint32_t PK_DAG_TARGET_GROUP_SIZE = 6u;
    static const uint32_t PK_DAG_MAX_LEVELS = 5u;
    static const uint32_t PK_DAG_DECIMATE_FACTOR = 2u;
    static const float PK_DAG_MIN_SIMPLIFICATION_FACTOR_MESHLET = 0.9f;
    static const float PK_DAG_MIN_SIMPLIFICATION_FACTOR_LEVEL = 0.9f;

    struct MeshletGroup
    {
        uint32_t indices[PK_DAG_MAX_GROUP_SIZE]{};
        uint32_t size = 0u;
    };

    struct MeshletGroupingContext
    {
        MeshletGroup current;
        MeshletGroup best;
    };

    struct MeshletCenterError
    {
        float center[3]{};
        float error = PK_MESHLET_LOD_MAX_ERROR;
    };

    struct MeshletLodInfo
    {
        MeshletCenterError current{};
        MeshletCenterError parent{};
    };

    struct MeshletContext
    {
        meshopt_Meshlet* meshlets;
        MeshletLodInfo* meshlet_lodInfos;
        unsigned int* meshlet_vertices;
        unsigned char* meshlet_triangles;
        
        size_t meshlet_count;
        uint32_t meshlet_vertices_count;
        uint32_t meshlet_indices_count;

        float* vertex_positions;
        uint8_t* vertex_lock;
        uint32_t* vertex_remap;

        uint32_t vertex_stride;
        uint32_t vertex_count;
    };

    static std::vector<MeshletGroup> BuildMeshletGroupsMetis(meshopt_Meshlet* meshlets,
        const uint32_t* meshlet_vertices,
        const uint8_t* meshlet_triangles,
        const uint32_t* spatial_remap,
        uint32_t meshlet_count,
        uint32_t meshlet_offset)
    {
        std::unordered_map<uint64_t, uint32_t> edge_map;
        std::vector<std::vector<idx_t>> adjacencies(meshlet_count);
        auto border_count = 0ull;

        for (auto i = 0u; i < meshlet_count; ++i)
        {
            const auto& meshlet = meshlets[i];
            auto sub_vertices = meshlet_vertices + meshlet.vertex_offset;
            auto sub_triangles = meshlet_triangles + meshlet.triangle_offset;

            for (auto j = 0u; j < meshlet.triangle_count * 3u; ++j)
            {
                auto i0 = (uint64_t)spatial_remap[sub_vertices[sub_triangles[j]]];
                auto i1 = (uint64_t)spatial_remap[sub_vertices[sub_triangles[(j / 3u) * 3u + (j + 1u) % 3u]]];
                auto& edge = edge_map[std::min(i0, i1) | (std::max(i0, i1) << 32ull)];
                auto min_id = edge & 0xFFFFu;
                auto max_id = (edge >> 16u) & 0xFFFFu;
                min_id = min_id > (i + 1u) || min_id == 0u ? (i + 1u) : min_id;
                max_id = max_id < (i + 1u) || max_id == 0u ? (i + 1u) : max_id;
                edge = (min_id & 0xFFFFu) | (max_id << 16u);

                if (min_id != max_id)
                {
                    adjacencies[min_id - 1u].push_back(max_id - 1u);
                    adjacencies[max_id - 1u].push_back(min_id - 1u);
                    border_count += 2ull;
                }
            }
        }

        idx_t target_group_count = (meshlet_count + PK_DAG_TARGET_GROUP_SIZE - 1u) / PK_DAG_TARGET_GROUP_SIZE;
        std::vector<idx_t> group_indices(meshlet_count);
        
        // Only run partitioning when targeting more than one partition.
        if (target_group_count > 1)
        {
            std::vector<idx_t> adjncy;
            adjncy.reserve(border_count);
            std::vector<idx_t> xadj(meshlet_count + 1u);
            xadj.at(0) = 0;

            for (auto i = 0ull; i < meshlet_count; ++i)
            {
                adjncy.insert(adjncy.end(), adjacencies.at(i).begin(), adjacencies.at(i).end());
                xadj.at(i + 1u) = adjncy.size();
            }

            idx_t nvtxs = meshlet_count;
            idx_t ncon = 1u;
            idx_t edgecut = 0;
            idx_t options[METIS_NOPTIONS];
            METIS_SetDefaultOptions(options);
            options[METIS_OPTION_UFACTOR] = 200;
            
            auto metisResult = METIS_PartGraphKway
            (
                &nvtxs,
                &ncon,
                xadj.data(),
                adjncy.data(),
                nullptr, // vwgt,
                nullptr, // vsize,
                nullptr, //adjwgt
                &target_group_count,
                nullptr, // tpwgts
                nullptr, // ubvec
                options,
                &edgecut,
                group_indices.data()
            );

            assert(metisResult == METIS_OK);
        }

        auto group_count = 0u; 

        for (auto i = 0u; i < meshlet_count; ++i)
        {
            group_count = std::max(group_count, (uint32_t)(group_indices.at(i) + 1u));
        }

        std::vector<MeshletGroup> groups(group_count);

        for (auto i = 0u; i < meshlet_count; ++i)
        {
            auto& group = groups[group_indices.at(i)];
            assert(group.size < PK_DAG_MAX_GROUP_SIZE);
            group.indices[group.size++] = i + meshlet_offset;
        }

        return groups;
    }

    static std::vector<uint32_t> GetMeshletGroupTriangleIndices(const meshopt_Meshlet* meshlets, const uint32_t* meshlet_vertices, const uint8_t* meshlet_triangles, const MeshletGroup& group)
    {
        auto count = 0u;

        for (auto i = 0u; i < group.size; ++i)
        {
            count += meshlets[group.indices[i]].triangle_count * 3u;
        }

        std::vector<uint32_t> indices(count);
        count = 0u;

        for (auto i = 0u; i < group.size; ++i)
        {
            const auto& meshlet = meshlets[group.indices[i]];

            for (auto j = meshlet.triangle_offset; j < (meshlet.triangle_offset + meshlet.triangle_count * 3u); ++j)
            {
                indices.at(count++) = meshlet_vertices[meshlet.vertex_offset + meshlet_triangles[j]];
            }
        }

        return indices;
    }

    static void BuildMeshletDAG(MeshletContext* ctx)
    {
        uint32_t stats_initial_triangle_count = ctx->meshlet_indices_count / 3u;
        uint32_t stats_final_triangle_count = 0u;
        uint32_t stats_new_meshlet_count = 0u;
        uint32_t stats_level_count = 0u;

        auto total_index_count = 0u;
        auto total_simplified_index_count = 0u;
        auto meshlet_offset = 0u;
        auto meshlet_count = ctx->meshlet_count;

        for (auto lodLevel = 0u; lodLevel < PK_DAG_MAX_LEVELS; ++lodLevel)
        {
            auto groups = BuildMeshletGroupsMetis(ctx->meshlets + meshlet_offset, ctx->meshlet_vertices, ctx->meshlet_triangles, ctx->vertex_remap, meshlet_count, meshlet_offset);
            
            total_index_count = 0u;
            total_simplified_index_count = 0u;
            meshlet_offset = ctx->meshlet_count;
            meshlet_count = 0u;

            for (auto& group : groups)
            {
                if (group.size <= 1u)
                {
                    continue;
                }

                auto indices = GetMeshletGroupTriangleIndices(ctx->meshlets, ctx->meshlet_vertices, ctx->meshlet_triangles, group);
                auto target_index_count = 3u * ((indices.size() / PK_DAG_DECIMATE_FACTOR) / 3u);

                MeshletCenterError error{};
                auto simplified_index_count = SimplifyCluster
                (
                    indices.data(),
                    indices.size(),
                    ctx->vertex_positions,
                    ctx->vertex_remap,
                    ctx->vertex_lock,
                    ctx->vertex_count,
                    ctx->vertex_stride,
                    target_index_count,
                    &error.error
                );

                auto simplifaction_factor = (float)simplified_index_count / (float)indices.size();

                if (simplifaction_factor > PK_DAG_MIN_SIMPLIFICATION_FACTOR_MESHLET)
                {
                    stats_final_triangle_count += indices.size() / 3u;
                    continue;
                }

                total_index_count += indices.size();
                total_simplified_index_count += simplified_index_count;

                float extents[3];
                CalculateBounds(ctx->vertex_positions, indices.data(), (uint32_t)(ctx->vertex_stride / sizeof(float)), simplified_index_count, error.center, extents);
                
                error.error *= CalculateMaxExtent(extents);

                float max_child_error = 0.0f;

                for (auto i = 0u; i < group.size; ++i)
                {
                    max_child_error = std::max(max_child_error, ctx->meshlet_lodInfos[group.indices[i]].current.error);
                }

                error.error += max_child_error;

                for (auto i = 0u; i < group.size; ++i)
                {
                    ctx->meshlet_lodInfos[group.indices[i]].parent = error;
                }

                auto simplified_count = BuildAndOptimizeMeshlets
                (
                    ctx->meshlets + ctx->meshlet_count,
                    ctx->meshlet_vertices + ctx->meshlet_vertices_count,
                    ctx->meshlet_triangles + ctx->meshlet_indices_count,
                    indices.data(),
                    simplified_index_count,
                    ctx->vertex_positions,
                    ctx->vertex_count,
                    ctx->vertex_stride,
                    PK_MESHLET_MAX_VERTICES,
                    PK_MESHLET_MAX_TRIANGLES,
                    PK_MESHLET_CONE_WEIGHT
                );

                meshlet_count += simplified_count;

                for (auto j = ctx->meshlet_count; j < (ctx->meshlet_count + simplified_count); ++j)
                {
                    ctx->meshlet_lodInfos[j].current = error;
                    ctx->meshlet_lodInfos[j].parent = MeshletCenterError();
                    ctx->meshlets[j].vertex_offset += ctx->meshlet_vertices_count;
                    ctx->meshlets[j].triangle_offset += ctx->meshlet_indices_count;
                }

                auto& last_meshlet = ctx->meshlets[ctx->meshlet_count + simplified_count - 1u];
                ctx->meshlet_vertices_count = last_meshlet.vertex_offset + last_meshlet.vertex_count;
                ctx->meshlet_indices_count = last_meshlet.triangle_offset + last_meshlet.triangle_count * 3u;
                ctx->meshlet_count += simplified_count;
            }

            stats_new_meshlet_count += meshlet_count;
            stats_level_count++;

            auto total_simplification_factor = (float)total_simplified_index_count / (float)std::max(1u, total_index_count);

            if (total_simplification_factor > PK_DAG_MIN_SIMPLIFICATION_FACTOR_LEVEL || total_index_count == 0u || meshlet_count <= 1u)
            {
                break;
            }
        }

        stats_final_triangle_count += total_simplified_index_count / 3u;

        printf("        Submesh DAG: Tris: %u -> %u, Meshlets: %u, Levels: %u\n", 
            stats_initial_triangle_count, 
            stats_final_triangle_count, 
            stats_new_meshlet_count, 
            stats_level_count);
    }

    WritePtr<PKMeshletMesh> CreateMeshletMesh(PKAssetBuffer& buffer,
                                                const std::vector<PKSubmesh>& submeshes,
                                                float* vertices,
                                                uint32_t* indices,
                                                uint32_t offsetPosition,
                                                uint32_t offsetTexcoord,
                                                uint32_t offsetNormal,
                                                uint32_t offsetTangent,
                                                uint32_t offsetColor,
                                                uint32_t vertexStride,
                                                uint32_t vertexCount,
                                                uint32_t indexCount)
    {
        auto max_meshlets = meshopt_buildMeshletsBound(indexCount, PK_MESHLET_MAX_VERTICES, PK_MESHLET_MAX_TRIANGLES) * 2u;
        std::vector<meshopt_Meshlet> meshlets(max_meshlets);
        std::vector<MeshletLodInfo> meshlet_lodInfos(max_meshlets);
        std::vector<unsigned int> meshlet_vertices(max_meshlets * PK_MESHLET_MAX_VERTICES);
        std::vector<unsigned char> meshlet_triangles(max_meshlets * PK_MESHLET_MAX_TRIANGLES * 3);
        std::vector<uint32_t> vertex_remap(vertexCount);
        std::vector<float> vertex_remap_weights(vertexCount);
        std::vector<uint8_t> vertex_lock(vertexCount);

        auto hasTexcoords = offsetTexcoord != 0xFFFFFFFFu;
        auto hasNormals = offsetNormal != 0xFFFFFFFFu;
        auto hasTangents = offsetTangent != 0xFFFFFFFFu;
        auto hasColors = offsetColor != 0xFFFFFFFFu;
        auto sm_stridef32 = vertexStride / sizeof(float);
        auto sm_texcoordsf32 = hasTexcoords ? vertices + (offsetTexcoord / sizeof(float)) : nullptr;
        auto sm_normalsf32 = hasNormals ? vertices + (offsetNormal / sizeof(float)) : nullptr;
        auto sm_tangentsf32 = hasTangents ? vertices + (offsetTangent / sizeof(float)) : nullptr;
        auto sm_colorsf32 = hasColors ? vertices + (offsetColor / sizeof(float)) : nullptr;

        MeshletContext ctx{};
        ctx.meshlets = meshlets.data();
        ctx.meshlet_lodInfos = meshlet_lodInfos.data();
        ctx.meshlet_vertices = meshlet_vertices.data();
        ctx.meshlet_triangles = meshlet_triangles.data();
        ctx.meshlet_count = 0u;
        ctx.meshlet_vertices_count = 0u;
        ctx.meshlet_indices_count = 0u;

        ctx.vertex_positions = vertices + (offsetPosition / sizeof(float));
        ctx.vertex_lock = vertex_lock.data();
        ctx.vertex_remap = vertex_remap.data();
        ctx.vertex_stride = vertexStride;
        ctx.vertex_count = vertexCount;
            
        std::vector<uint8_t> out_indices;
        std::vector<PKMeshletVertex> out_vertices;
        std::vector<PKMeshletSubmesh> out_submeshes;
        std::vector<PKMeshlet> out_meshlets;

        meshopt_Stream stream;
        stream.data = ctx.vertex_positions;
        stream.size = sizeof(float) * 3u;
        stream.stride = ctx.vertex_stride;
        meshopt_generateVertexRemapMulti(ctx.vertex_remap, nullptr, ctx.vertex_count, ctx.vertex_count, &stream, 1u);

        for (const auto& sm : submeshes)
        {
            auto sm_indices = indices + sm.firstIndex;
            auto first_meshlet = out_meshlets.size();

            ctx.meshlet_count = BuildAndOptimizeMeshlets
            (
                ctx.meshlets, 
                ctx.meshlet_vertices, 
                ctx.meshlet_triangles, 
                sm_indices,
                sm.indexCount, 
                ctx.vertex_positions,
                ctx.vertex_count, 
                ctx.vertex_stride, 
                PK_MESHLET_MAX_VERTICES,
                PK_MESHLET_MAX_TRIANGLES,
                PK_MESHLET_CONE_WEIGHT
            );

            if (ctx.meshlet_count == 0)
            {
                continue;
            }

            for (auto i = 0u; i < ctx.meshlet_count; ++i)
            {
                ctx.meshlet_lodInfos[i].current.error = -1.0f;
            }

            ctx.meshlet_indices_count = ctx.meshlets[ctx.meshlet_count - 1u].triangle_offset + ctx.meshlets[ctx.meshlet_count - 1u].triangle_count * 3u;
            ctx.meshlet_vertices_count = ctx.meshlets[ctx.meshlet_count - 1u].vertex_offset + ctx.meshlets[ctx.meshlet_count - 1u].vertex_count;
            auto first_meshlet_lod_index = ctx.meshlet_count;

            BuildMeshletDAG(&ctx);
            
            PKMeshletSubmesh meshletSubmesh{};
            meshletSubmesh.firstMeshlet = (uint32_t)first_meshlet;
            meshletSubmesh.meshletCount = (uint32_t)ctx.meshlet_count;
            memcpy(meshletSubmesh.bbmin, sm.bbmin, sizeof(float) * 3u);
            memcpy(meshletSubmesh.bbmax, sm.bbmax, sizeof(float) * 3u);
            out_submeshes.push_back(meshletSubmesh);

            for (auto i = 0u; i < ctx.meshlet_count; ++i)
            {
                const auto& meshlet = ctx.meshlets[i];

                auto bounds = meshopt_computeMeshletBounds
                (
                    ctx.meshlet_vertices + meshlet.vertex_offset,
                    ctx.meshlet_triangles + meshlet.triangle_offset,
                    meshlet.triangle_count,
                    ctx.vertex_positions,
                    ctx.vertex_count,
                    ctx.vertex_stride
                );

                auto indices_offset = out_indices.size();
                auto triangle_offset = indices_offset / 3ull;
                auto vertices_offset = out_vertices.size();

                float center[3];
                float extents[3];

                CalculateBounds
                (   
                    ctx.vertex_positions,
                    ctx.meshlet_vertices + meshlet.vertex_offset,
                    (uint32_t)sm_stridef32,
                    meshlet.vertex_count,
                    center,
                    extents
                );

                if (i < first_meshlet_lod_index)
                {
                    ctx.meshlet_lodInfos[i].current.error = -1.0f;
                    memcpy(ctx.meshlet_lodInfos[i].current.center, center, sizeof(float) * 3u);
                }

                auto pkmeshlet = PackPKMeshlet
                (
                    (uint32_t)vertices_offset,
                    (uint32_t)triangle_offset,
                    meshlet.vertex_count,
                    meshlet.triangle_count,
                    bounds.cone_axis_s8,
                    bounds.cone_cutoff_s8,
                    bounds.cone_apex,
                    center,
                    extents,

                    ctx.meshlet_lodInfos[i].current.center,
                    ctx.meshlet_lodInfos[i].current.error,
                    ctx.meshlet_lodInfos[i].parent.center,
                    ctx.meshlet_lodInfos[i].parent.error
                );

                out_indices.resize(out_indices.size() + meshlet.triangle_count * 3);
                memcpy(out_indices.data() + indices_offset, ctx.meshlet_triangles + meshlet.triangle_offset, meshlet.triangle_count * 3u);

                for (auto j = 0u; j < meshlet.vertex_count; ++j)
                {
                    auto vertex_index = ctx.meshlet_vertices[meshlet.vertex_offset + j];
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