#pragma once
#include <cmath>
#include <cstdint>
namespace JarcMenu {
// (0.5.869) the campaign's title and main menu classes: campaign only (they have no multiplayer twin in
// mp_addresses.txt, and with them named for both programs make_mp_host.py stopped every build)
#ifndef COD3_MP
inline bool IsMenuClass(uint32_t menuClass) { return menuClass==0x82071DDCu || menuClass==0x82071724u; }
#else
inline bool IsMenuClass(uint32_t) { return false; }
#endif
struct CoverRect { float x,y,width,height; };
inline CoverRect Cover(float width,float height) {
    const float aspect=16.0f/9.0f;
    float w=width,h=w/aspect;
    if(h<height){h=height;w=h*aspect;}
    return {(width-w)*0.5f,(height-h)*0.5f,w,h};
}
struct Vertex { float x, y; uint32_t color; };
inline bool IsMainDivider(uint32_t menuClass, const Vertex (&v)[4]) {
    // Campaign title/main menu only, excluding dialogs and gameplay HUD.
    if (!IsMenuClass(menuClass)) return false;
    float x0=v[0].x,x1=x0,y0=v[0].y,y1=y0;
    for (const auto& p:v) {
        if (!std::isfinite(p.x)||!std::isfinite(p.y)) return false;
        x0=p.x<x0?p.x:x0;x1=p.x>x1?p.x:x1;
        y0=p.y<y0?p.y:y0;y1=p.y>y1?p.y:y1;
        const unsigned a=p.color>>24,r=(p.color>>16)&255,g=(p.color>>8)&255,b=p.color&255;
        if (a==0 || r<g || g<=b || r-b<16) return false;
    }
    // Live trace: the rule is two gradient halves, -4..320 and 320..644,
    // at y=270..272 in the menu's 640x480 coordinates.
    const bool left=std::fabs(x0+4)<=0.1f && std::fabs(x1-320)<=0.1f;
    const bool right=std::fabs(x0-320)<=0.1f && std::fabs(x1-644)<=0.1f;
    return (left||right) && std::fabs(y0-270)<=0.1f && std::fabs(y1-272)<=0.1f;
}
}
