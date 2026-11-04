#include "kernel.h"
#include "settings.h"
#include "hitmarkers.h"
#include "hitmarker_rules.h"
#include "imgui.h"
#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <vector>

static int tests=0;
#define CHECK(x) do { ++tests; if(!(x)){printf("FAIL line %d: %s\n",__LINE__,#x); return 1;} }while(0)
static bool applyDamage=true;
static int calls=0;
// Controlled original-function substitute tests attribution around the hook;
// this does not claim to test the native game's implementation of damage.
extern "C" PPC_FUNC(__imp__sub_82562DD8) {
    ++calls;
    if(applyDamage) Guest::Write32(base,ctx.r3.u32+776,
        Guest::Read32(base,ctx.r3.u32+776)-ctx.r8.u32);
}
int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const std::filesystem::path dir="hitmarker-test-config";
    std::filesystem::create_directories(dir);
    Settings::Load(dir);
    auto v=Settings::Get();
    CHECK(v.hitmarkers && v.hitmarkerSound && v.hitmarkerRedKill);
    v.hitmarkerSize=83; v.hitmarkerOpacity=62; v.hitmarkerDuration=50; v.hitmarkerVolume=39;
    v.hitmarkerRedKill=false; v.hitmarkerSound=false; v.hitmarkers=false;
    Settings::Set(v); Settings::Load(dir); auto read=Settings::Get();
    CHECK(read.hitmarkerSize==83 && read.hitmarkerOpacity==62 && read.hitmarkerDuration==50 && read.hitmarkerVolume==39);
    CHECK(!read.hitmarkers && !read.hitmarkerSound && !read.hitmarkerRedKill);
    { std::ofstream file(dir/"CoD3Recomp.ini"); file<<"hitmarker_size = -1\nhitmarker_opacity = 200\nhitmarker_duration = 0\nhitmarker_volume = -10\n"; }
    Settings::Load(dir); read=Settings::Get();
    CHECK(read.hitmarkerSize==16 && read.hitmarkerOpacity==100 && read.hitmarkerDuration==50 && read.hitmarkerVolume==0);
    CHECK(HitmarkerRules::Alpha(0,200,100)==1);
    CHECK(HitmarkerRules::Alpha(150,200,100)==0.5f);
    CHECK(HitmarkerRules::Alpha(200,200,100)==0);
    CHECK(HitmarkerRules::Alpha(0,200,0)==0);
    v=Settings::Defaults(); v.hitmarkerDuration=50; Settings::Preview(v);
    uint8_t* base=(uint8_t*)VirtualAlloc(nullptr,0x100000000ull,MEM_RESERVE,PAGE_NOACCESS);
    CHECK(base);
    CHECK(VirtualAlloc(base+0x40000000,0x10000,MEM_COMMIT,PAGE_READWRITE));
    constexpr uint32_t target=0x40000000, attacker=0x40001000, ts=0x40002000, as=0x40003000;
    Guest::Write32(base,target+532,0x40004000); Guest::Write32(base,target+536,ts);
    Guest::Write32(base,attacker+528,0x40005000); Guest::Write32(base,attacker+536,as);
    Guest::Write32(base,ts+4,1); Guest::Write32(base,as+4,2);
    auto damage=[&](int health,int amount) {
        Guest::Write32(base,target+776,health);
        PPCContext call{}; call.r3.u32=target; call.r5.u32=attacker; call.r8.u32=amount;
        sub_82562DD8(call,base);
    };
    damage(100,20); CHECK(Hitmarkers::Active(v)); CHECK(calls==1);
    CHECK(Guest::Read32(base,target+776)==80);
    Sleep(60); CHECK(!Hitmarkers::Active(v));
    Guest::Write32(base,ts+4,2); damage(100,20); CHECK(!Hitmarkers::Active(v)); // friendly
    Guest::Write32(base,ts+4,1); Guest::Write32(base,attacker+528,0);
    damage(100,20); CHECK(!Hitmarkers::Active(v)); // AI attacker
    Guest::Write32(base,attacker+528,0x40005000); damage(0,20); CHECK(!Hitmarkers::Active(v)); // corpse
    damage(100,0); CHECK(!Hitmarkers::Active(v)); // no damage
    applyDamage=false; damage(100,20); CHECK(!Hitmarkers::Active(v)); // blocked damage
    applyDamage=true; v.hitmarkers=false; Settings::Preview(v);
    damage(100,20); CHECK(!Hitmarkers::Active(v));
    v.hitmarkers=true; Settings::Preview(v);
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,nullptr)));
    Hitmarkers::Initialize(device.Get());
    ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.DisplaySize=ImVec2(1920,1080); io.IniFilename=nullptr;
    unsigned char* pixels; int w,h; io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto renderColor=[&]() {
        ImGui::NewFrame(); Hitmarkers::Draw(1920,1080,v); ImGui::Render();
        auto* d=ImGui::GetDrawData();
        return d->CmdListsCount ? d->CmdLists[0]->VtxBuffer[0].col : 0u;
    };
    damage(100,200); CHECK(Hitmarkers::Active(v));
    CHECK(renderColor()==IM_COL32(255,45,45,255)); // lethal damage
    v.hitmarkerRedKill=false; CHECK(renderColor()==IM_COL32(255,255,255,255));
    std::vector<float> audio(4096*6,0);
    Hitmarkers::Mix(audio.data(),4096,6,v);
    float peak=0;
    for(int i=0;i<4096;i++) { peak=std::max(peak,std::fabs(audio[i*6])); CHECK(audio[i*6]==audio[i*6+1]); CHECK(audio[i*6+2]==0); }
    CHECK(peak>0 && peak<=0.7f);
    std::fill(audio.begin(),audio.end(),0); Hitmarkers::Mix(audio.data(),4096,6,v);
    CHECK(audio==std::vector<float>(4096*6,0)); // clip ends, no loop
    Sleep(90); Hitmarkers::Preview(false); v.hitmarkerSound=false;
    Hitmarkers::Mix(audio.data(),4096,6,v); CHECK(audio==std::vector<float>(4096*6,0));
    v.hitmarkerSound=true; v.hitmarkerVolume=0; Hitmarkers::Preview(false);
    Hitmarkers::Mix(audio.data(),4096,6,v); CHECK(audio==std::vector<float>(4096*6,0));
    ImGui::DestroyContext(); VirtualFree(base,0,MEM_RELEASE);
    printf("PASS: %d checks (including per-sample channel checks). Attribution wrapper, lethal tint, real PNG/PCM, fade, audio and settings persistence.\n",tests);
}

namespace Guest { uint8_t* Base=nullptr; uint32_t AllocatePhysical(uint32_t){return 0;} PPCFunc* LookupModule(uint32_t){return nullptr;} }
extern "C" PPC_FUNC(__imp__sub_82448268){}

namespace Overlay { bool IsOpen(){return false;} }
