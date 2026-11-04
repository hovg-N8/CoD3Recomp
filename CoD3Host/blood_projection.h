#pragma once
#include <cmath>
namespace BloodRender {
// Reversed-depth perspective Z = a*W+b. Recover the near plane without
// depending on camera position. COD3's scene uses a six-unit near plane;
// its short-range/sky projections must not project static world decals.
inline float NearPlane(const float* m){
 float ww=m[12]*m[12]+m[13]*m[13]+m[14]*m[14];
 if(!std::isfinite(ww)||ww<.5f||ww>2.f)return 0;
 float a=(m[8]*m[12]+m[9]*m[13]+m[10]*m[14])/ww;
 float b=m[11]-a*m[15];
 float n=b/(1-a);return std::isfinite(n)?n:0;
}
}
