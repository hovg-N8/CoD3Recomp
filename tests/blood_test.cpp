// Deterministic host simulation test. The world query is a controlled floor;
// actual SP collision/camera behavior is checked separately in the game.
#include "../CoD3Host/blood.cpp"
#include "settings.h"
#include <filesystem>
#include <fstream>
namespace Guest {
uint8_t* Base=nullptr;
uint32_t AllocatePhysical(uint32_t){return 0x40010000;}
PPCFunc* LookupModule(uint32_t){return nullptr;}
}
extern "C" PPC_FUNC(__imp__sub_82448268){}
static int queries=0,checks=0;bool testMenuOpen=false;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x);return 1;}}while(0)
PPC_FUNC(FloorTrace){
 ++queries;V a=Read(base,ctx.r4.u32),b=Read(base,ctx.r5.u32);float f=1;
 if(a.z>=0&&b.z<0)f=a.z/(a.z-b.z);
 uint32_t w;memcpy(&w,&f,4);Guest::Write32(base,ctx.r3.u32+32,w);
 Put(base,ctx.r3.u32+16,{0,0,1});
}
void Commit(uint8_t* base,uint64_t at,size_t size){VirtualAlloc(base+at,size,MEM_COMMIT,PAGE_READWRITE);}
int main(){
 uint8_t* base=(uint8_t*)VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_NOACCESS);CHECK(base);Guest::Base=base;
 Commit(base,0x40000000,0x40000);Commit(base,0x82A20000,0x10000);Commit(base,0x82A90000,0x10000);
 uint64_t slot=Guest::FuncTableBase+(0x82485D38-PPC_CODE_BASE)*2;Commit(base,slot&~0xFFFFull,0x20000);
 *reinterpret_cast<PPCFunc**>(base+slot)=FloorTrace;
 Guest::Write32(base,0x82A2A1C8,0);uint32_t view=0x82A98D90;
 Put(base,view+4480,{-100,0,50});Put(base,view+4496,{1,0,0});Put(base,view+4508,{0,1,0});Put(base,view+4520,{0,0,1});
 Put(base,view+4464,{90,61.9275f,0});
 PPCContext ctx{};Blood::Options o;o.enabled=true;o.persistence=1;Blood::Configure(o);
 Blood::Impact hit;hit.target=0x40000000;hit.valid=true;hit.health=100;hit.damage=30;
 hit.point[0]=0;hit.point[2]=12;hit.direction[0]=1;Guest::Write32(base,hit.target+776,70);
 Blood::After(base,hit);CHECK(Blood::Status().queued==1);
 frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);CHECK(camera.valid);CHECK(Blood::Active());
 auto initial=particles;
 o.physics=false;Blood::Configure(o);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);
 V p{0,0,1},v{10,0,0};Step(p,v,0.1f,false);CHECK(v.z==0&&std::fabs(p.x-1)<0.0001f);
 Step(p,v,0.1f,true);CHECK(v.z<0&&v.x<10);
 o.physics=true;Blood::Configure(o);
 for(int i=0;i<40;i++){frameAt=GetTickCount64()-50;frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(traces<=TraceLimit);}
 CHECK(Blood::Status().particles==0);
 // Create persistent splatters and verify expiration is independent of particles.
 o.persistence=0;Blood::Configure(o);AddDecal({0,0,0},{0,0,1},5);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().decals>0);
 frameAt=GetTickCount64()-100;Frame(ctx,base);CHECK(Blood::Status().decals>0);
 o.persistence=1;Blood::Configure(o);for(int i=0;i<12;i++){frameAt=GetTickCount64()-100;Frame(ctx,base);}CHECK(Blood::Status().decals==0);
 o.maxDecals=8;Blood::Configure(o);for(int i=0;i<100;i++)AddDecal({0,0,0},{0,0,1},5);CHECK(Blood::Status().decals==8);
 o.decals=false;Blood::Configure(o);CHECK(Blood::Status().decals==0);Blood::After(base,hit);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles>0);
 o.amount=0;Blood::Configure(o);CHECK(Blood::Status().particles==0&&Blood::Status().queued==0&&!Blood::Active());
 int beforeQueries=queries;frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(queries==beforeQueries);
 o.amount=200;o.density=3;o.decals=true;Blood::Configure(o);
 for(int i=0;i<1000;i++)Blood::After(base,hit);CHECK(Blood::Status().queued==QueueLimit);frameAt=GetTickCount64()-50;Frame(ctx,base);CHECK(Blood::Status().particles<=ParticleLimit);
 o.enabled=false;Blood::Configure(o);CHECK(Blood::Status().particles==0&&Blood::Status().decals==0&&Blood::Status().queued==0);
 beforeQueries=queries;frameAt=GetTickCount64()-50;Frame(ctx,base);Blood::After(base,hit);CHECK(queries==beforeQueries&&Blood::Status().queued==0);
 // Each persisted control, with non-default values and independent booleans.
 std::filesystem::path dir="blood-test-config";std::filesystem::create_directories(dir);Settings::Load(dir);auto s=Settings::Get();
 s.blood={};s.blood.enabled=true;s.blood.amount=173;s.blood.density=3;s.blood.size=2.25f;s.blood.decals=false;s.blood.physics=false;s.blood.persistence=0;s.blood.maxDecals=47;
 s.blood.color[0]=0.18f;s.blood.color[1]=0.04f;s.blood.color[2]=0.02f;Settings::Set(s);Settings::Load(dir);auto loaded=Settings::Get().blood;
 CHECK(loaded.enabled&&loaded.amount==173&&loaded.density==3);CHECK(loaded.size==2.25f&&!loaded.decals&&!loaded.physics);
 CHECK(loaded.persistence==0&&loaded.maxDecals==47);CHECK(std::fabs(loaded.color[0]-.18f)<.001f&&std::fabs(loaded.color[1]-.04f)<.001f&&std::fabs(loaded.color[2]-.02f)<.001f);
 loaded.amount=999;loaded.density=-4;loaded.maxDecals=900;loaded.size=NAN;loaded.color[0]=NAN;Blood::Clamp(loaded);
 CHECK(loaded.amount==200&&loaded.density==0&&loaded.maxDecals==128&&loaded.size==1&&loaded.color[0]==0);
 // World vertices are generated from positions, not cached screen points.
 o.enabled=true;o.amount=100;o.size=1;Blood::Configure(o);Blood::After(base,hit);frameAt=GetTickCount64()-50;Frame(ctx,base);
 std::array<Blood::WorldVertex,8192> vertices;
 CHECK(Blood::BuildWorld(vertices.data(),8192)>0);testMenuOpen=true;CHECK(Blood::BuildWorld(vertices.data(),8192)==0);testMenuOpen=false;
 o.enabled=false;Blood::Configure(o);CHECK(Blood::BuildWorld(vertices.data(),8192)==0);
 VirtualFree(base,0,MEM_RELEASE);
 printf("PASS %d blood simulation/settings/world-geometry checks. Native collision stubbed.\n",checks);
}
namespace Overlay { bool IsOpen(){return testMenuOpen;} }