#pragma once
#include <cstdint>

struct meshopt_Meshlet;

namespace PKAssets::Mesh
{
    uint16_t PackHalf(float v);

    uint8_t PackUnorm8(float v);

    uint32_t PackUnorm12(float v);

    void OctaEncode(const float* n, float* outuv);

    inline uint32_t asuint(float v) { return *reinterpret_cast<uint32_t*>(&v); }
    inline float asfloat(uint32_t u) { return *reinterpret_cast<float*>(&u); }


    void InitBounds(float* bmin, float* bmax);

    float CalculateEdgeLength(const float* v0, const float* v1);

    void CalculateBounds(const float* pPositions,
        const uint32_t* indices,
        uint32_t vertex_stridef32,
        uint32_t index_count,
        float* out_center,
        float* out_extents);
    
    float CalculateMaxExtent(const float* extents);

    size_t CalculateUniqueVertexCount(uint32_t* indices, uint32_t index_count, uint32_t vertex_count);

    void CalculateVertexRemapAndWeights(float* vertices,
        float* vertex_positions,
        uint32_t vertex_count,
        uint32_t vertex_stride,
        uint32_t* out_remap,
        float* out_remap_weights);
    
    void QuantizeVerticesFloat3(float* vertices,
        uint32_t vertex_stridef32,
        uint32_t vertex_count,
        float min_delta);

    void QuantizeVerticesFloat2(float* vertices,
        uint32_t vertex_stridef32,
        uint32_t vertex_count,
        float min_delta);

    size_t LockBorderVertices(const uint32_t* indices, uint32_t index_count, const uint32_t* remap, uint8_t* vertex_lock);

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
        float cone_weight);

    size_t SimplifyCluster(
        uint32_t* indices,
        size_t index_count,
        const float* vertex_positions,
        const uint32_t* vertex_remap,
        uint8_t* vertex_lock,
        size_t vertex_count,
        size_t vertex_stride,
        size_t target_index_count,
        float* out_result_error);
}
