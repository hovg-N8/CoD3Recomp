#pragma once
#include <cstdint>
struct PPCContext;
namespace Blood {
inline constexpr int MaxDecals=4096, MaxParticles=96, VerticesPerSplat=36;
inline constexpr int MaxVertices=(MaxDecals+MaxParticles)*VerticesPerSplat;
struct Options {
    bool enabled=false, decals=true, physics=true;
    int amount=100, density=1, persistence=30, maxDecals=64;
    float cloudSize=1.0f, cloudOpacity=1.0f;
    float size=1.0f, color[3]={0.30f,0.018f,0.012f};
};
struct Impact { float point[3]{}, direction[3]{}; uint32_t target=0; int health=0, damage=0; bool valid=false, playerFired=false; uint64_t event=0; };
void Clamp(Options& options);
void Configure(const Options& options);
void Clear();
void Update(PPCContext& context,uint8_t* base);
void Before(PPCContext& context,uint8_t* base,Impact& impact);
void After(uint8_t* base,const Impact& impact);
bool Active();
struct WorldVertex {float x,y,z;uint32_t color;};
int BuildWorld(WorldVertex* output,int capacity);
struct Stats { int particles=0, decals=0, queued=0, traces=0; };
Stats Status();
}
