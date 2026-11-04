#pragma once
#include <algorithm>
#include <cmath>
namespace BloodMath {
struct V { float x=0,y=0,z=0; V operator+(V b)const{return{x+b.x,y+b.y,z+b.z};} V operator-(V b)const{return{x-b.x,y-b.y,z-b.z};} V operator*(float b)const{return{x*b,y*b,z*b};} };
inline float Dot(V a,V b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V Cross(V a,V b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Finite(V a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline V Unit(V a){float n=std::sqrt(Dot(a,a));return n>0.001f&&std::isfinite(n)?a*(1/n):V{0,0,1};}
inline void Step(V& p,V& v,float dt,bool physics){if(physics){v=v*std::exp(-1.5f*dt);v.z-=650*dt;}p=p+v*dt;}
struct Camera {V eye,forward,left,up;float tx=0,ty=0;bool valid=false;};
inline bool Project(const Camera& c,V p,float& x,float& y){
 V d=p-c.eye;float z=Dot(d,c.forward);
 if(!c.valid||z<2||!Finite(d))return false;
 x=0.5f-Dot(d,c.left)/(2*z*c.tx);y=0.5f-Dot(d,c.up)/(2*z*c.ty);
 return std::isfinite(x)&&std::isfinite(y)&&x>-1&&x<2&&y>-1&&y<2;
}
}
