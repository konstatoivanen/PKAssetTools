#include "PKMeshUtilities.h"
#include <meshoptimizer/meshoptimizer.h>
#include <corecrt_math.h>
#include <limits>
#include <cassert>
#include <unordered_map>

namespace PKAssets::Mesh
{
    uint16_t PackHalf(float v)
    {
        if (v < -65536.0f)
        {
            v = -65536.0f;
        }
    
        if (v > 65536.0f)
        {
            v = 65536.0f;
        }
    
        v *= 1.925930e-34f;
        int32_t i = *(int*)&v;
        uint32_t ui = (uint32_t)i;
        return ((i >> 16) & (int)0xffff8000) | ((int)(ui >> 13));
    }

    uint8_t PackUnorm8(float v)
    {
        auto i = (int32_t)roundf(v * 255.0f);
        if (i < 0) { i = 0; }
        if (i > 255) { i = 255; }
        return (uint8_t)(i & 0xFFu);
    }

    uint32_t PackUnorm12(float v)
    {
        auto i = (int32_t)roundf(v * 4095.0f);
        if (i < 0) { i = 0; }
        if (i > 4095) { i = 4095; }
        return (uint32_t)(i & 0xFFFu);
    }
    
    float abs(float v) { return v < 0.0f ? -v : v; }

    void OctaEncode(const float* n, float* outuv)
    {
        float t[3] = { n[0], n[1], n[2] };
        float f = abs(n[0]) + abs(n[1]) + abs(n[2]);
        t[0] /= f;
        t[1] /= f;
        t[2] /= f;

        if (t[1] >= 0.0f)
        {
            outuv[0] = t[0];
            outuv[1] = t[2];
        }
        else
        {
            outuv[0] = (1.0f - abs(t[2])) * (t[0] >= 0.0f ? 1.0f : -1.0f);
            outuv[1] = (1.0f - abs(t[0])) * (t[2] >= 0.0f ? 1.0f : -1.0f);
        }

        outuv[0] = outuv[0] * 0.5f + 0.5f;
        outuv[1] = outuv[1] * 0.5f + 0.5f;
    }

    void InitBounds(float* bmin, float* bmax)
    {
        bmin[0] = std::numeric_limits<float>().max();
        bmin[1] = std::numeric_limits<float>().max();
        bmin[2] = std::numeric_limits<float>().max();
        bmax[0] = -std::numeric_limits<float>().max();
        bmax[1] = -std::numeric_limits<float>().max();
        bmax[2] = -std::numeric_limits<float>().max();
    }

    float CalculateEdgeLength(const float* v0, const float* v1)
    {
        auto x = v0[0] - v1[0];
        auto y = v0[1] - v1[1];
        auto z = v0[2] - v1[2];
        return sqrt(x * x + y * y + z * z);
    }

    void CalculateBounds(const float* pPositions, 
        const uint32_t* indices,
        uint32_t vertex_stridef32, 
        uint32_t index_count,
        float* out_center, 
        float* out_extents)
    {
        float bbmin[3];
        float bbmax[3];
        InitBounds(bbmin, bbmax);

        for (auto i = 0u; i < index_count; ++i)
        {
            auto index = indices[i];
            auto pPosition = pPositions + index * vertex_stridef32;

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
            out_center[i] = bbmin[i] + (bbmax[i] - bbmin[i]) * 0.5f;
            out_extents[i] = (bbmax[i] - bbmin[i]) * 0.5f;
        }
    }

    size_t CalculateUniqueVertexCount(uint32_t* indices, uint32_t index_count, uint32_t vertex_count)
    {
        // Source: https://github.com/zeux/meshoptimizer
        std::vector<uint8_t> filter_data((vertex_count + 7) / 8);
        unsigned char* filter = filter_data.data();
        memset(filter, 0, (vertex_count + 7) / 8);

        size_t unique = 0;
        for (size_t i = 0; i < index_count; ++i)
        {
            unsigned int index = indices[i];
            assert(index < vertex_count);

            unique += (filter[index / 8] & (1 << (index % 8))) == 0;
            filter[index / 8] |= 1 << (index % 8);
        }

        return unique;
    }

    void CalculateVertexRemapAndWeights(
        float* vertices, 
        float* vertex_positions, 
        uint32_t vertex_count,
        uint32_t vertex_stride,
        uint32_t* out_remap, 
        float* out_remap_weights)
    {
        meshopt_Stream stream;
        stream.data = vertex_positions;
        stream.size = sizeof(float) * 3u;
        stream.stride = vertex_stride;
        meshopt_generateVertexRemapMulti(out_remap, nullptr, vertex_count, vertex_count, &stream, 1u);

        auto stride_f32 = vertex_stride / sizeof(float);

        for (auto i = 0u; i < vertex_count; ++i)
        {
            float weight = 0.0f;

            if (out_remap[i] != i)
            {
                auto v0 = vertices + i * stride_f32;
                auto v1 = vertices + out_remap[i] * stride_f32;

                // Naive float attribute error weight.
                // @TODO if this is bad refactor to use per attribute weights.
                for (auto j = 0u; j < stride_f32; ++j)
                {
                    weight += (v0[j] - v1[j]) * (v0[j] - v1[j]);
                }

                if (weight > 0.0f)
                {
                    weight = sqrt(weight);
                }
            }

            out_remap_weights[i] = weight;
        }
    }

    struct QuantizedVertex
    {
        int64_t x = 0;
        int64_t y = 0;
        int64_t z = 0;

        constexpr bool operator == (const QuantizedVertex& other) const { return x == other.x && y == other.y && z == other.z; }
    };

    struct QuantizedVertexHash
    {
        size_t operator()(const QuantizedVertex& k) const noexcept
        {
            auto x = k.x ^ (k.x >> 17);
            auto y = k.y ^ (k.y >> 17);
            auto z = k.z ^ (k.z >> 17);
            return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
        }
    };

    void QuantizeVerticesFloat3(float* vertices, uint32_t vertex_stridef32, uint32_t vertex_count, float min_delta)
    {
        // Forgive me for this...
        std::unordered_map<QuantizedVertex, uint32_t, QuantizedVertexHash> unique_vertices(vertex_count);

        for (auto i = 0u; i < vertex_count; ++i)
        {
            auto v = vertices + vertex_stridef32 * i;
            QuantizedVertex q;
            q.x = (int64_t)(v[0] / min_delta);
            q.y = (int64_t)(v[1] / min_delta);
            q.z = (int64_t)(v[2] / min_delta);

            auto iter = unique_vertices.find(q);

            if (iter != unique_vertices.end())
            {
                v[0] = (vertices + vertex_stridef32 * iter->second)[0];
                v[1] = (vertices + vertex_stridef32 * iter->second)[1];
                v[2] = (vertices + vertex_stridef32 * iter->second)[2];
                continue;
            }

            unique_vertices[q] = i;
        }
    }

    void QuantizeVerticesFloat2(float* vertices, uint32_t vertex_stridef32, uint32_t vertex_count, float min_delta)
    {
        // Forgive me for this...
        std::unordered_map<QuantizedVertex, uint32_t, QuantizedVertexHash> unique_vertices(vertex_count);

        for (auto i = 0u; i < vertex_count; ++i)
        {
            auto v = vertices + vertex_stridef32 * i;
            QuantizedVertex q;
            q.x = (int64_t)(v[0] / min_delta);
            q.y = (int64_t)(v[1] / min_delta);
            q.z = 0;

            auto iter = unique_vertices.find(q);

            if (iter != unique_vertices.end())
            {
                v[0] = (vertices + vertex_stridef32 * iter->second)[0];
                v[1] = (vertices + vertex_stridef32 * iter->second)[1];
                continue;
            }

            unique_vertices[q] = i;
        }
    }

    size_t LockBorderVertices(const uint32_t* indices, uint32_t index_count, const uint32_t* remap, uint8_t* vertex_lock)
    {
        std::unordered_map<uint64_t, uint8_t> edge_map(index_count);

        for (auto i = 0u; i < index_count; ++i)
        {
            auto i0 = indices[i];
            auto i1 = indices[(i / 3u) * 3u + (i + 1u) % 3u];
            vertex_lock[i0] = 0u;

            auto r0 = (uint64_t)(remap ? remap[i0] : i0);
            auto r1 = (uint64_t)(remap ? remap[i1] : i1);
            edge_map[std::min(r0, r1) | (std::max(r0, r1) << 32ull)]++;
        }

        size_t edge_count = 0ull;

        for (auto i = 0u; i < index_count; ++i)
        {
            auto i0 = indices[i];
            auto i1 = indices[(i / 3u) * 3u + (i + 1u) % 3u];

            auto r0 = (uint64_t)(remap ? remap[i0] : i0);
            auto r1 = (uint64_t)(remap ? remap[i1] : i1);
            auto& count = edge_map.at(std::min(r0, r1) | (std::max(r0, r1) << 32ull));

            if (count <= 1u)
            {
                vertex_lock[i0] = 1u;
                vertex_lock[i1] = 1u;
                edge_count++;
            }
        }

        return edge_count;
    }

    size_t BuildAndOptimizeMeshlets(meshopt_Meshlet* meshlets,
        unsigned int* meshlet_vertices,
        unsigned char* meshlet_triangles,
        const unsigned int* indices,
        size_t index_count,
        const float* vertex_positions,
        size_t vertex_count,
        size_t vertex_positions_stride,
        size_t max_vertices,
        size_t max_triangles,
        float cone_weight)
    {
        auto count = meshopt_buildMeshlets
        (
            meshlets,
            meshlet_vertices,
            meshlet_triangles,
            indices,
            index_count,
            vertex_positions,
            vertex_count,
            vertex_positions_stride,
            max_vertices,
            max_triangles,
            cone_weight
        );

        for (auto i = 0u; i < count; ++i)
        {
            meshopt_optimizeMeshlet
            (
                meshlet_vertices + meshlets[i].vertex_offset,
                meshlet_triangles + meshlets[i].triangle_offset,
                meshlets[i].triangle_count,
                meshlets[i].vertex_count
            );
        }

        return count;
    }

    struct IndexMergeInfo
    {
        uint32_t from;
        uint32_t to;
        float error;
    };

    static int IndexMergeInfoCompare(const void* a, const void* b)
    {
        auto i0 = reinterpret_cast<const IndexMergeInfo*>(a);
        auto i1 = reinterpret_cast<const IndexMergeInfo*>(b);

        if (i0->error < i1->error)
        {
            return 1;
        }

        if (i0->error > i1->error)
        {
            return -1;
        }

        return 0;
    }

    // Super horrible iterative simplify that merges spatially exact vertices when failing to simplify to target.
    // @TODO make a custom version of zeux simplify that allows for remap vertex weights.
    size_t SimplifyCluster(
        uint32_t* indices,
        size_t index_count, 
        const float* vertex_positions, 
        const uint32_t* vertex_remap,
        const float* vertex_remap_weights,
        uint8_t* vertex_lock, 
        size_t vertex_count, 
        size_t vertex_stride, 
        size_t target_index_count, 
        size_t target_vertex_count, 
        float* out_result_error)
    {
        auto remaining_merges = 0;
        std::vector<IndexMergeInfo> index_merges(index_count);
        std::vector<uint32_t> simplified_indices(index_count);

        for (auto i = 0u; i < index_count; ++i)
        {
            if (vertex_remap[indices[i]] != indices[i])
            {
                index_merges.at(remaining_merges++) = { i, vertex_remap[indices[i]], vertex_remap_weights[indices[i]] };
            }
        }

        qsort(index_merges.data(), remaining_merges, sizeof(IndexMergeInfo), IndexMergeInfoCompare);

        LockBorderVertices(indices, index_count, vertex_remap, vertex_lock);
       
        auto simplified_index_count = 0u;

        while (true)
        {
            simplified_index_count = meshopt_simplifyWithAttributes
            (
                simplified_indices.data(),
                indices,
                index_count,
                vertex_positions,
                vertex_count,
                vertex_stride,
                nullptr,
                0u,
                nullptr,
                0u,
                vertex_lock,
                target_index_count,
                1.0f,
                meshopt_SimplifySparse,
                out_result_error
            );

            auto unique_vertex_count = CalculateUniqueVertexCount(simplified_indices.data(), simplified_index_count, vertex_count);

            if (unique_vertex_count <= target_vertex_count && simplified_index_count <= target_index_count)
            {
                break;
            }

           // if (remaining_merges == 0)
            {
                // Mission failed, we'll get em next time.
                break;
            }

            for (auto i = (int32_t)remaining_merges - 1; i >= remaining_merges / 2; i--)
            {
                indices[index_merges.at(i).from] = index_merges.at(i).to;
            }

            remaining_merges /= 2;
        }

        memcpy(indices, simplified_indices.data(), simplified_index_count * sizeof(uint32_t));

        return simplified_index_count;
    }
}