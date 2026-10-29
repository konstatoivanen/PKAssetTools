#pragma once
#ifndef PK_UTILITIES
#define PK_UTILITIES

float pow2(float x) { return x * x; }
float pow3(float x) { return x * x * x; }
float pow4(float x) { return x * x * x * x; }
float pow5(float x) { return x * x * x * x * x; }

#define unlerp(a, b, value) (((value) - (a)) / ((b) - (a)))
#define unlerp_sat(a, b, value) saturate(((value) - (a)) / ((b) - (a)))
#define lerp_true(x, y, s) ((x) + (s) * ((y) - (x)))
#define POW2(x) ((x) * (x))
#define POW3(x) ((x) * (x) * (x))
#define POW4(x) ((x) * (x) * (x) * (x))
#define POW5(x) ((x) * (x) * (x) * (x) * (x))
#define mod(x, y) ((x) - (y) * floor((x) / (y)))

#define Any_GEqual(a, b)   any((a) >= (b))
#define Any_Equal(a, b)    any((a) == (b))
#define Any_NotEqual(a, b) any((a) != (b))
#define Any_LEqual(a, b)   any((a) <= (b))
#define Any_Less(a, b)     any((a) <  (b))
#define Any_Greater(a, b)  any((a) >  (b))

#define All_GEqual(a, b)   all((a) >= (b))
#define All_Equal(a, b)    all((a) == (b))
#define All_NotEqual(a, b) all((a) != (b))
#define All_LEqual(a, b)   all((a) <= (b))
#define All_Less(a, b)     all((a) <  (b))
#define All_Greater(a, b)  all((a) >  (b))

// Ray tracing utilities
#define PK_GET_RAY_HIT_POINT (WorldRayOrigin() + WorldRayDirection() * RayTCurrent())
#define PK_GET_RAY_HIT_DISTANCE RayTCurrent()

#endif