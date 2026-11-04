#include "kernel.h"
#include "settings.h"
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifndef COD3_MP
namespace {
std::array<std::atomic<uint64_t>,4096> actions{};
void Tag(uint32_t key,bool blood){
 for(size_t i=0;i<actions.size();i++){
  auto& slot=actions[((key>>4)+i)%actions.size()];auto old=slot.load(std::memory_order_relaxed);
  if(old&&uint32_t(old>>32)!=key)continue;
  if(slot.compare_exchange_strong(old,(uint64_t(key)<<32)|blood,std::memory_order_relaxed))return;
  --i;
 }
}
bool Tagged(uint32_t key){
 for(size_t i=0;i<actions.size();i++){
  auto v=actions[((key>>4)+i)%actions.size()].load(std::memory_order_relaxed);
  if(!v)return false;if(uint32_t(v>>32)==key)return v&1;
 }return false;
}
bool Ptr(uint32_t p){return p>=0x10000&&p<0xC0000000-4096;}
void Register(uint8_t* base,uint32_t def){
 if(!Ptr(def))return;char name[97]{};memcpy(name,base+def+116,96);
 bool blood=strstr(name,"_flesh")&&!strstr(name,"dust");
 uint32_t list=Guest::Read32(base,def+80);int count=Guest::Read16(base,def+86);
 if(!Ptr(list)||count>128)return;
 for(int i=0;i<count;i++){
  // The emitter's renderer is separate from its update-action list.
  uint32_t renderer=Guest::Read32(base,list+i*72+24);
  if(Ptr(renderer))Tag(renderer,blood);
 }
}
}
extern "C" PPC_FUNC(__imp__sub_820EB270);
PPC_FUNC(sub_820EB270){Register(base,ctx.r3.u32);__imp__sub_820EB270(ctx,base);}
extern "C" PPC_FUNC(__imp__sub_820FD788);
PPC_FUNC(sub_820FD788){
 uint32_t action=ctx.r3.u32,node=ctx.r4.u32;__imp__sub_820FD788(ctx,base);
 if(ctx.r3.u32&&Tagged(action)){
  const auto o=Settings::Get().blood;
  // Change the private render node, never shared assets or simulation data.
  // The ColorUVA billboard copy stores radius at +12 and RGBA at +16.
  // Its transform and particle origins must remain untouched.
  auto scale=[&](uint32_t at,float gain){uint32_t bits=Guest::Read32(base,at);float f;memcpy(&f,&bits,4);f*=gain;memcpy(&bits,&f,4);Guest::Write32(base,at,bits);};
  if(!o.enabled||!o.amount||o.cloudOpacity==0)Guest::Write32(base,node+116,0);
  else if(Guest::Read32(base,node)==0x820152E0u){
   const uint32_t data=Guest::Read32(base,node+112),count=Guest::Read32(base,node+116),stride=Guest::Read32(base,node+120);
   if(Ptr(data)&&count<=4096&&stride>=40&&stride<=512&&uint64_t(data)+uint64_t(count)*stride<0xC0000000ull){
    for(uint32_t i=0;i<count;i++){scale(data+i*stride+12,o.cloudSize);scale(data+i*stride+28,o.cloudOpacity);}
    scale(node+92,o.cloudSize);
   }
  }
  static int reports=0;if(getenv("COD3_BLOOD_CLOUD_TRACE")&&reports++<16)printf("cloud-node: renderer %08X vtable %08X count %u size %.2f opacity %.2f\n",action,Guest::Read32(base,node),Guest::Read32(base,node+116),o.cloudSize,o.cloudOpacity);
 }
}
#endif
