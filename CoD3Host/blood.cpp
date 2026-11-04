#include "blood_diag.h"
#include "blood.h"
#include "blood_math.h"
#include "kernel.h"
#include "overlay.h"
#include <Windows.h>
#include <array>
#include <atomic>
#include <mutex>
#include <cstring>
#include <cstdio>
#include <cstdlib>
using namespace BloodMath;
namespace {
constexpr int ParticleLimit=Blood::MaxParticles, DecalLimit=Blood::MaxDecals, QueueLimit=32, TraceLimit=ParticleLimit;
std::mutex mutex;
std::atomic<bool> enabled{false};
Blood::Options options;
struct Particle {V p,v;float life=0,size=0;uint64_t event=0;};
struct Decal {V p,n;std::array<V,12> offsets{};float size=0,life=0;uint64_t order=0,event=0;};
std::array<Particle,ParticleLimit> particles;
std::array<Decal,DecalLimit> decals;
std::array<Blood::Impact,QueueLimit> queue;
int queued=0,nextParticle=0,nextDecal=0,traces=0;
uint64_t decalOrder=0;
Camera camera;
uint32_t rng=0x93A71B2Du;
uint64_t frameAt=0;
bool Controlled(){static bool on=getenv("COD3_BLOOD_CONTROLLED")!=nullptr;return on&&BloodDiag::Test();}
bool controlledCaptured=false,controlledFrozen=false;
uint32_t controlledTarget=0;uint64_t controlledFreezeAt=0;
Camera controlledCamera;V controlledActor;

bool TraceOn(){static const bool on=getenv("COD3_BLOOD_TRACE")!=nullptr;return on;}
float Random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return (rng&65535)/65535.0f;}
bool Pointer(uint32_t p){return p>=0x40000000u&&p<0xC0000000u-2048;}
float F(uint8_t* b,uint32_t a){uint32_t w=Guest::Read32(b,a);float f;memcpy(&f,&w,4);return f;}
V Read(uint8_t* b,uint32_t a){return{F(b,a),F(b,a+4),F(b,a+8)};}
void Put(uint8_t* b,uint32_t a,V p){float v[3]={p.x,p.y,p.z};for(int k=0;k<3;k++){uint32_t w;memcpy(&w,v+k,4);Guest::Write32(b,a+4*k,w);}}
void Reset(const char* reason){int live=0;for(const auto& d:decals)live+=d.size>0;BloodDiag::Log("reset all reason=%s decals=%d queued=%d",reason,live,queued);particles={};decals={};queued=nextParticle=nextDecal=traces=0;frameAt=0;decalOrder=0;}
struct Hit {V p,n;float fraction=1;bool valid=false;};
Hit Trace(PPCContext& ctx,uint8_t* base,V from,V to){
 Hit hit;if(traces>=TraceLimit||!Finite(from)||!Finite(to))return hit;
 ++traces;
 // One reusable native stack/result buffer, only used on the game thread.
 static uint32_t memory=0;if(!memory)memory=Guest::AllocatePhysical(0x10000);
 if(!memory)return hit;
 const uint32_t stack=memory+0xE000,scratch=memory+0xF000;
 memset(base+scratch,0,256);memset(base+stack,0,128);
 Put(base,scratch+96,from);Put(base,scratch+112,to);
 PPCContext call=ctx;call.r1.u32=stack;call.lr=0;
 call.r3.u32=scratch;call.r4.u32=scratch+96;call.r5.u32=scratch+112;
 call.r6.u32=scratch+128;call.r7.u32=scratch+144;
 call.r8.u32=0;call.r9.u32=1;call.r10.u32=0;
 // SP CM_BoxTrace: world model 0, CONTENTS_SOLID, point trace. No entities.
 Guest::Lookup(0x82485D38)(call,base);
 hit.fraction=F(base,scratch+32);hit.n=Read(base,scratch+16);
 hit.p=from+(to-from)*hit.fraction;
 hit.valid=std::isfinite(hit.fraction)&&hit.fraction>=0&&hit.fraction<=1&&Finite(hit.n)&&Finite(hit.p)&&!base[scratch+60];
 return hit;
}
void AddDecal(V p,V normal,float size,uint64_t event=0){
 if(!options.decals)return;
 if(!Finite(p)||!Finite(normal)||Dot(normal,normal)<.25f)return;
 // Geometry and surface basis are fixed at collision, never camera-facing.
 normal=Unit(normal);auto& d=decals[nextDecal];if(BloodDiag::Test()&&d.size>0)return; if(d.size>0)BloodDiag::Log("destroy decal=%llu reason=pool_replacement",d.order);nextDecal=(nextDecal+1)%options.maxDecals;
 d={};d.p=p+normal*.04f;d.n=normal;d.size=size;d.life=float(options.persistence);d.order=++decalOrder;d.event=event;BloodDiag::Log("link decal=%llu player_event=%llu",d.order,event);BloodDiag::Log("create decal=%llu pos=%.4f,%.4f,%.4f normal=%.4f,%.4f,%.4f attachment=static_world expires=%d test_no_expiry=%d",d.order,d.p.x,d.p.y,d.p.z,normal.x,normal.y,normal.z,options.persistence,BloodDiag::Test());
 V u=Unit(Cross(normal,std::fabs(normal.z)<.9f?V{0,0,1}:V{1,0,0})),v=Cross(normal,u);
 uint32_t seed=rng;
 for(int k=0;k<12;k++){seed=seed*1664525u+1013904223u;float r=.65f+(seed&255)/255.0f*.35f;float a=k*6.2831853f/12;d.offsets[k]=u*(cosf(a)*r)+v*(sinf(a)*r);}
}
void Spawn(const Blood::Impact& e){
 BloodDiag::Log("spawn player_event=%llu",e.event);
 V p{e.point[0],e.point[1],e.point[2]},d=Unit({e.direction[0],e.direction[1],e.direction[2]});
 const float distance=std::sqrt(Dot(p-camera.eye,p-camera.eye));
 int count=int((3+options.density*3)*options.amount/100.0f*std::clamp(e.damage/35.0f,0.5f,2.0f));
 if(distance>2000)count/=4;else if(distance>1000)count/=2;
 count=std::clamp(count,1,32);
 for(int i=0;i<count;i++){
  V scatter{Random()-0.5f,Random()-0.5f,Random()*0.8f};
  float speed=std::clamp(80.0f+e.damage*1.5f,90.0f,280.0f)*(0.5f+Random());
  particles[nextParticle++%ParticleLimit]={p,Unit(d+scatter*1.2f)*speed,0.55f+Random()*0.8f,(0.4f+Random()*1.2f),e.event};
 }
 if(TraceOn())printf("blood: confirmed damage %d, spawned %d droplets at %.1f %.1f %.1f\n",e.damage,count,p.x,p.y,p.z);
}
void Frame(PPCContext& ctx,uint8_t* base){
 if(!enabled.load(std::memory_order_relaxed))return;
 std::lock_guard<std::mutex> lock(mutex);
 if(frameAt && GetTickCount64()-frameAt<33)return;
 if(!options.enabled||!options.amount){Reset("master_off_or_zero_amount");return;}
 const uint32_t index=Guest::Read32(base,0x82A2A1C8);if(index>3){static uint64_t lastInvalid=0;uint64_t now=GetTickCount64();if(now-lastInvalid>=1000){lastInvalid=now;BloodDiag::Log("skip update invalid_view_index=%u persistence=%d",index,options.persistence);}return;}
 const uint32_t view=0x82A98D90+5840*index;
 Camera c{Read(base,view+4480),Read(base,view+4496),Read(base,view+4508),Read(base,view+4520),tanf(F(base,view+4464)*0.00872664626f),tanf(F(base,view+4468)*0.00872664626f),true};
 c.valid=Finite(c.eye)&&Finite(c.forward)&&std::fabs(Dot(c.forward,c.forward)-1)<0.05f&&c.tx>0.05f&&c.tx<5&&c.ty>0.05f&&c.ty<5;
 if(!c.valid){frameAt=GetTickCount64();if(TraceOn()){static int invalid=0;if(invalid++<3)printf("blood: camera not ready, tan %.3f %.3f forward %.3f %.3f %.3f\n",c.tx,c.ty,c.forward.x,c.forward.y,c.forward.z);}return;}camera=c;
 uint64_t now=GetTickCount64();float dt=frameAt?std::clamp((now-frameAt)/1000.0f,0.0f,0.1f):0;frameAt=now;
 traces=0;
 for(int i=0;i<queued;i++)Spawn(queue[i]);queued=0;
 int liveDecals=0;
 for(int i=0;i<options.maxDecals;i++){
  auto& d=decals[i];if(d.size<=0)continue;
  ++liveDecals;
  if(options.persistence&&!BloodDiag::Test()){d.life-=dt;if(d.life<=0){BloodDiag::Log("destroy decal=%llu reason=lifetime",d.order);d={};continue;}}

 }
 for(auto& p:particles){
  if(p.life<=0)continue;
  p.life-=dt;if(p.life<=0){BloodDiag::Log("particle slot=%zu removed reason=lifetime",size_t(&p-particles.data()));continue;}
  V before=p.p;Step(p.p,p.v,dt,options.physics);
  // Collision remains active without gravity so the decal toggle is independent.
  auto h=Trace(ctx,base,before,p.p);
  if(!h.valid){BloodDiag::Log("particle slot=%zu removed reason=invalid_or_startsolid_trace",size_t(&p-particles.data()));p.life=0;continue;}
  if(h.fraction<1){BloodDiag::Log("particle slot=%zu converts_to_decal at=%.4f,%.4f,%.4f",size_t(&p-particles.data()),h.p.x,h.p.y,h.p.z);AddDecal(h.p,h.n,p.size*(2+Random()*3),p.event);p.life=0;continue;}

 }
 if(Controlled()&&controlledCaptured){
  bool moving=false;for(const auto& p:particles)moving|=p.life>0;
  if(!controlledFrozen&&!moving&&liveDecals>0){
   controlledFrozen=true;controlledFreezeAt=now;controlledCamera=c;controlledActor=Read(base,controlledTarget+336);
   Kernel::QueueConsoleCommand("timescale 0");
   BloodDiag::Log("controlled phase=stationary freeze_requested=timescale_0 target=%08X decals=%d eye=%.6f,%.6f,%.6f forward=%.6f,%.6f,%.6f actor_origin=%.6f,%.6f,%.6f",controlledTarget,liveDecals,c.eye.x,c.eye.y,c.eye.z,c.forward.x,c.forward.y,c.forward.z,controlledActor.x,controlledActor.y,controlledActor.z);
  }
  static uint64_t last=0;if(controlledFrozen&&now-last>=1000){last=now;auto actor=Read(base,controlledTarget+336);V e=c.eye-controlledCamera.eye,f=c.forward-controlledCamera.forward,a=actor-controlledActor;
   BloodDiag::Log("controlled verify elapsed_ms=%llu eye_drift=%.8f forward_drift=%.8f actor_drift=%.8f decals=%d particles_active=%d",now-controlledFreezeAt,sqrtf(Dot(e,e)),sqrtf(Dot(f,f)),sqrtf(Dot(a,a)),liveDecals,moving);
  }
 }
 static uint64_t report=0;if(BloodDiag::On()&&now-report>=1000){report=now;BloodDiag::Log("runtime persistence=%d dt=%.3f decals=%d view_index=%u",options.persistence,dt,liveDecals,index);}if(TraceOn()&&now-report>2000){printf("blood: camera tan %.3f %.3f eye %.1f %.1f %.1f traces %d\n",c.tx,c.ty,c.eye.x,c.eye.y,c.eye.z,traces);fflush(stdout);}
}
}
void Blood::Clamp(Options& o){
 if(!std::isfinite(o.cloudSize))o.cloudSize=1; o.cloudSize=std::clamp(o.cloudSize,.25f,4.f);
 if(!std::isfinite(o.cloudOpacity))o.cloudOpacity=1; o.cloudOpacity=std::clamp(o.cloudOpacity,0.f,3.f);
 o.amount=std::clamp(o.amount,0,200);o.density=std::clamp(o.density,0,3);o.persistence=std::clamp(o.persistence,0,600);o.maxDecals=std::clamp(o.maxDecals,8,DecalLimit);
 if(!std::isfinite(o.size))o.size=1;o.size=std::clamp(o.size,0.25f,3.0f);
 for(auto& c:o.color){if(!std::isfinite(c))c=0;c=std::clamp(c,0.0f,1.0f);}
}
void Blood::Configure(const Options& input){
 std::lock_guard<std::mutex> lock(mutex);auto next=input;Clamp(next);
 BloodDiag::Log("configure enabled=%d amount=%d decals=%d persistence=%d max_decals=%d",next.enabled,next.amount,next.decals,next.persistence,next.maxDecals);
 if(!next.enabled||!next.amount)Reset("master_off_or_zero_amount");
 if(!next.decals){BloodDiag::Log("destroy decals reason=decal_toggle_off");decals={};}
 if(next.persistence!=options.persistence)for(auto& d:decals)d.life=float(next.persistence);
 if(next.maxDecals!=options.maxDecals){
  // Only a limit change sorts; preserve the newest live decals, in FIFO order.
  std::sort(decals.begin(),decals.end(),[](const Decal& a,const Decal& b){return (a.size>0?a.order:0)>(b.size>0?b.order:0);});
  int live=0;while(live<next.maxDecals&&decals[live].size>0)++live;
  for(int i=live;i<DecalLimit;i++){if(decals[i].size>0)BloodDiag::Log("destroy decal=%llu reason=limit_reduction",decals[i].order);if(!BloodDiag::Test())decals[i]={};}
  std::reverse(decals.begin(),decals.begin()+live);nextDecal=live%next.maxDecals;
 }
 // Discard render packets immediately; they are rebuilt with the new settings.
 options=next;enabled.store(next.enabled&&next.amount>0,std::memory_order_relaxed);
}
void Blood::Clear(){std::lock_guard<std::mutex> lock(mutex);Reset("map_or_checkpoint_restore");camera.valid=false;}
void Blood::Before(PPCContext& ctx,uint8_t* base,Impact& e){
 if(!enabled.load(std::memory_order_relaxed))return;
 uint32_t target=ctx.r3.u32,point=ctx.r7.u32,dir=ctx.r6.u32;
 if(!Pointer(target)||!Pointer(point)||!Pointer(dir)||!Guest::Read32(base,target+532))return;
 uint32_t attacker=ctx.r5.u32;e.playerFired=Pointer(attacker)&&Guest::Read32(base,attacker+528)!=0;
 e.target=target;e.health=int32_t(Guest::Read32(base,target+776));e.damage=int32_t(ctx.r8.u32);
 V p=Read(base,point),d=Read(base,dir);
 e.valid=e.health>0&&e.damage>0&&Finite(p)&&Finite(d);
 e.point[0]=p.x;e.point[1]=p.y;e.point[2]=p.z;e.direction[0]=d.x;e.direction[1]=d.y;e.direction[2]=d.z;
}
void Blood::After(uint8_t* base,const Impact& e){
 if(!e.valid||!enabled.load(std::memory_order_relaxed)||int32_t(Guest::Read32(base,e.target+776))>=e.health)return;
 std::lock_guard<std::mutex> lock(mutex);
 if(options.enabled&&options.amount&&queued<QueueLimit){
  if(BloodDiag::Test()&&(e.health>=1000000||e.damage<=1)){BloodDiag::Log("ignore diagnostic non-enemy impact health=%d damage=%d",e.health,e.damage);return;}
  if(BloodDiag::Test()&&!e.playerFired){BloodDiag::Log("ignore impact reason=not_player_fired target=%08X",e.target);return;}
  if(Controlled()&&controlledCaptured){BloodDiag::Log("controlled ignore additional player hit reason=single_event_only");return;}
  if(Controlled()){controlledCaptured=true;controlledTarget=e.target;}
  static uint64_t eventSequence=0;auto captured=e;captured.event=++eventSequence;
  BloodDiag::Log("player_impact event=%llu player_fired=%d target=%08X confirmed_health=%d->%d damage=%d",captured.event,e.playerFired,e.target,e.health,int32_t(Guest::Read32(base,e.target+776)),e.damage);
  BloodDiag::Log("impact target=%08X health=%d->%d damage=%d point=%.4f,%.4f,%.4f target_origin=%.4f,%.4f,%.4f attachment=none (added effects use static-world collisions)",e.target,e.health,int32_t(Guest::Read32(base,e.target+776)),e.damage,e.point[0],e.point[1],e.point[2],F(base,e.target+340),F(base,e.target+344),F(base,e.target+348));
  queue[queued++]=captured;
 }
}
bool Blood::Active(){if(!enabled.load(std::memory_order_relaxed))return false;std::lock_guard<std::mutex> lock(mutex);for(auto p:particles)if(p.life>0)return true;for(const auto& d:decals)if(d.size>0)return true;return false;}
int Blood::BuildWorld(WorldVertex* output,int capacity){
 if(!enabled.load(std::memory_order_relaxed)||Overlay::IsOpen()){static int logged=0;if(logged++<3)BloodDiag::Log("hidden reason=master_disabled_or_F11_open");return 0;}
 std::lock_guard<std::mutex> lock(mutex);if(!camera.valid)return 0;
 int count=0;
 auto polygon=[&](V center,V u,V v,float size,float alpha,uint32_t seed){
  if(count+36>capacity)return;
  V points[12];
  for(int k=0;k<12;k++){seed=seed*1664525u+1013904223u;float radius=size*(0.65f+(seed&255)/255.0f*.35f);float angle=k*6.2831853f/12;points[k]=center+u*(cosf(angle)*radius)+v*(sinf(angle)*radius);}
  uint32_t color=uint32_t(options.color[0]*255)|(uint32_t(options.color[1]*255)<<8)|(uint32_t(options.color[2]*255)<<16)|(uint32_t(std::clamp(alpha,0.0f,1.0f)*255)<<24);
  auto vertex=[&](V p){output[count++]={p.x,p.y,p.z,color};};
  for(int k=0;k<12;k++){vertex(center);vertex(points[k]);vertex(points[(k+1)%12]);}
 };
 for(const auto& d:decals)if(d.size>0){
  if(count+36>capacity)break;
  float fade=options.persistence?std::min(1.0f,d.life/2):1;
  uint32_t color=uint32_t(options.color[0]*255)|(uint32_t(options.color[1]*255)<<8)|(uint32_t(options.color[2]*255)<<16)|(uint32_t(.72f*fade*255)<<24);
  auto vertex=[&](V v){output[count++]={v.x,v.y,v.z,color};};
  for(int k=0;k<12;k++){vertex(d.p);vertex(d.p+d.offsets[k]*(d.size*options.size));vertex(d.p+d.offsets[(k+1)%12]*(d.size*options.size));}
 }
 if(BloodDiag::On()){static uint64_t last=0;auto now=GetTickCount64();if(now-last>500){last=now;uint64_t hash=1469598103934665603ull;int live=0;for(const auto& d:decals)if(d.size>0){++live;const auto* bytes=reinterpret_cast<const unsigned char*>(&d.p);for(size_t k=0;k<sizeof(d.p);k++)hash=(hash^bytes[k])*1099511628211ull;}BloodDiag::Log("geometry decals=%d vertices=%d positions_hash=%016llX first_id=%llu first_pos=%.4f,%.4f,%.4f",live,count,hash,decals[0].order,decals[0].p.x,decals[0].p.y,decals[0].p.z);int slot=0;for(const auto& d:decals)if(d.size>0){if(slot<8)BloodDiag::Log("render_link slot=%d decal=%llu player_event=%llu alpha=%.6f rgb=%.4f,%.4f,%.4f",slot,d.order,d.event,.72f*(options.persistence?std::min(1.0f,d.life/2):1),options.color[0],options.color[1],options.color[2]);++slot;}}}
 // Predict only the remaining fraction of the simulation step; camera motion
 // is handled by the frame's native GPU matrix and cannot drag cached sprites.
 float dt=frameAt?std::clamp((GetTickCount64()-frameAt)/1000.0f,0.0f,.033f):0;
 for(auto p:particles)if(p.life>0){V pos=p.p,vel=p.v;Step(pos,vel,dt,options.physics);polygon(pos,camera.left,camera.up,p.size*options.size,std::min(.9f,p.life*3),0x23AB71);}
 return count;
}
Blood::Stats Blood::Status(){std::lock_guard<std::mutex> lock(mutex);Stats s;s.queued=queued;s.traces=traces;for(auto p:particles)s.particles+=p.life>0;for(const auto& d:decals)s.decals+=d.size>0;return s;}
void Blood::Update(PPCContext& ctx,uint8_t* base){Frame(ctx,base);}

void BloodDiagnosticEye(float* eye){std::lock_guard<std::mutex> lock(mutex);eye[0]=camera.eye.x;eye[1]=camera.eye.y;eye[2]=camera.eye.z;}
