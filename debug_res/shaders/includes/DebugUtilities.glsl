#pragma once
#ifndef PK_UTILITIES
#define PK_UTILITIES

float pow2(float x) { return x * x; }
float pow3(float x) { return x * x * x; }
float pow4(float x) { return x * x * x * x; }
float pow5(float x) { return x * x * x * x * x; }
#define unlerp(a,b,value) (((value) - (a)) / ((b) - (a)))
#define unlerp_sat(a,b,value) saturate(((value) - (a)) / ((b) - (a)))
#define saturate(v) clamp(v, 0.0, 1.0)
#define lerp_true(x,y,s) ((x) + (s) * ((y) - (x)))
#define POW2(x) ((x) * (x))
#define POW3(x) ((x) * (x) * (x))
#define POW4(x) ((x) * (x) * (x) * (x))
#define POW5(x) ((x) * (x) * (x) * (x) * (x))
#define mod(x,y) ((x) - (y) * floor((x) / (y)))
#define mul(a,b) (a * b)

#define Any_GEqual(a, b) any(greaterThanEqual(a,b))
#define Any_Equal(a, b) any(equal(a,b))
#define Any_NotEqual(a, b) any(notEqual(a,b))
#define Any_LEqual(a, b) any(lessThanEqual(a,b))
#define Any_Less(a,b) any(lessThan(a,b))
#define Any_Greater(a,b) any(greaterThan(a,b))

#define All_GEqual(a, b) all(greaterThanEqual(a,b))
#define All_Equal(a, b) all(equal(a,b))
#define All_NotEqual(a, b) all(notEqual(a,b))
#define All_LEqual(a, b) all(lessThanEqual(a,b))
#define All_Less(a,b) all(lessThan(a,b))
#define All_Greater(a,b) all(greaterThan(a,b))

#define PK_SET_GLOBAL 0
#define PK_SET_PASS 1
#define PK_SET_SHADER 2
#define PK_SET_DRAW 3
#define PK_DECLARE_SET_GLOBAL layout(set = 0)
#define PK_DECLARE_SET_PASS layout(set = 1)
#define PK_DECLARE_SET_SHADER layout(set = 2)
#define PK_DECLARE_SET_DRAW layout(set = 3)

#define PK_DECLARE_LOCAL_CBUFFER(BufferName) layout(push_constant) uniform BufferName
#define PK_DECLARE_CBUFFER(BufferName, Set) layout(std140, set = Set) uniform BufferName

#define PK_DECLARE_BUFFER(ValueType, BufferName, Set) layout(std430, set = Set) buffer BufferName { ValueType BufferName##_Data[]; }
#define PK_DECLARE_READONLY_BUFFER(ValueType, BufferName, Set) layout(std430, set = Set) readonly buffer BufferName { ValueType BufferName##_Data[]; }
#define PK_DECLARE_WRITEONLY_BUFFER(ValueType, BufferName, Set) layout(std430, set = Set) writeonly buffer BufferName { ValueType BufferName##_Data[]; }
#define PK_DECLARE_RESTRICTED_BUFFER(ValueType, BufferName, Set) layout(std430, set = Set) restrict buffer BufferName { ValueType BufferName##_Data[]; }
#define PK_DECLARE_RESTRICTED_READONLY_BUFFER(ValueType, BufferName, Set) layout(std430, set = Set) restrict readonly buffer BufferName { ValueType BufferName##_Data[]; }

#define PK_DECLARE_ATOMIC_VARIABLE(ValueType, BufferName, Set) layout(std430, set = Set) buffer BufferName { ValueType BufferName##_Data; }
#define PK_DECLARE_ATOMIC_READONLY_VARIABLE(ValueType, BufferName, Set) layout(std430, set = Set) readonly buffer BufferName { ValueType BufferName##_Data; }
#define PK_DECLARE_ATOMIC_WRITEONLY_VARIABLE(ValueType, BufferName, Set) layout(std430, set = Set) writeonly buffer BufferName { ValueType BufferName##_Data; }
#define PK_DECLARE_ATOMIC_RESTRICTED_VARIABLE(ValueType, BufferName, Set) layout(std430, set = Set) restrict buffer BufferName { ValueType BufferName##_Data; }
#define PK_DECLARE_ATOMIC_RESTRICTED_READONLY_VARIABLE(ValueType, BufferName, Set) layout(std430, set = Set) restrict readonly buffer BufferName { ValueType BufferName##_Data; }

#define PK_BUFFER_DATA(BufferName, index) BufferName##_Data[index]
#define PK_ATOMIC_DATA(BufferName) BufferName##_Data

// Ray tracing utilities
#define PK_GET_RAY_HIT_POINT (gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT)
#define PK_GET_RAY_HIT_DISTANCE gl_HitTEXT
#define PK_DECLARE_RT_BARYCOORDS(name) hitAttributeEXT float2 name
#define PK_DECLARE_RT_PAYLOAD_IN(type, name, index) layout(location = index) rayPayloadInEXT type name
#define PK_DECLARE_RT_PAYLOAD_OUT(type, name, index) layout(location = index) rayPayloadEXT type name

#if defined(SHADER_STAGE_RAY_GENERATION) || defined(SHADER_STAGE_RAY_MISS) || defined(SHADER_STAGE_RAY_CLOSEST_HIT) || defined(SHADER_STAGE_RAY_ANY_HIT) || defined(SHADER_STAGE_RAY_INTERSECTION)
    #define PK_DECLARE_ACCELERATION_STRUCTURE(Set, Name) layout(set = Set) uniform accelerationStructureEXT Name;
#else
    #define PK_DECLARE_ACCELERATION_STRUCTURE(Set, Name)
#endif

#endif