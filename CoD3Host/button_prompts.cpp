// Translate controller-font runs, keeping the title's original layout cells.
#include "button_prompts.h"
#include "kernel.h"
#include "input.h"
#include "input.h"
#include "settings.h"
#include "overlay.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
std::atomic<bool> keyboard{true};
std::atomic<uint32_t> regularFont{0};
thread_local std::string contextText;
std::string Text(uint8_t* base,uint32_t address) {
    if(address<0x10000 || address>0xFFFFF000u) return {};
    char buffer[1024]{}; SIZE_T got=0;
    ReadProcessMemory(GetCurrentProcess(),base+address,buffer,sizeof(buffer)-1,&got);
    for(size_t i=0;i<got;++i) if(!buffer[i]) return std::string(buffer,i);
    return {};
}
bool IconFont(uint8_t* base,uint32_t font) {
    if(font<0x10000 || font>0xFFFFF000u) return false;
    uint32_t words[11]; SIZE_T got=0;
    if(!ReadProcessMemory(GetCurrentProcess(),base+font,words,sizeof(words),&got) || got!=sizeof(words)) return false;
    const uint32_t first=_byteswap_ulong(words[9]),count=_byteswap_ulong(words[10]);
    if(first==32 && count==224) regularFont.store(font,std::memory_order_relaxed);
    return first==32 && count==28;
}
std::string Key(int key) {
    if((key>='A' && key<='Z') || (key>='0' && key<='9')) return std::string(1,char(key));
    switch(key) {
    case 0:return "-";
    case VK_LBUTTON:return "M1";case VK_RBUTTON:return "M2";
    case VK_MBUTTON:return "M3";case VK_XBUTTON1:return "M4";case VK_XBUTTON2:return "M5";
    case VK_SPACE:return "Space";case VK_RETURN:return "Enter";case VK_ESCAPE:return "Esc";
    case VK_SHIFT:case VK_LSHIFT:return "Shift";case VK_RSHIFT:return "RShift";
    case VK_CONTROL:case VK_LCONTROL:return "Ctrl";case VK_RCONTROL:return "RCtrl";
    case VK_MENU:case VK_LMENU:return "Alt";case VK_RMENU:return "RAlt";
    case VK_TAB:return "Tab";case VK_BACK:return "Bksp";case VK_CAPITAL:return "Caps";
    case VK_UP:return "Up";case VK_DOWN:return "Down";case VK_LEFT:return "Left";case VK_RIGHT:return "Right";
    }
    if(key>=VK_F1 && key<=VK_F24) return "F"+std::to_string(key-VK_F1+1);
    if(key>=VK_NUMPAD0 && key<=VK_NUMPAD9) return "Num"+std::to_string(key-VK_NUMPAD0);
    char name[64]{};UINT scan=MapVirtualKeyA(UINT(key),MAPVK_VK_TO_VSC);
    if(key==VK_INSERT || key==VK_DELETE || key==VK_HOME || key==VK_END || key==VK_PRIOR || key==VK_NEXT || key==VK_DIVIDE) scan|=0x100;
    if(scan && GetKeyNameTextA(LONG(scan<<16),name,sizeof(name))) return name;
    return "Key"+std::to_string(key);
}
bool Has(const std::string& text,const char* word) { return text.find(word)!=std::string::npos; }
std::string Label(unsigned char glyph,const Settings::Values& v,const std::string& context) {
    auto action=[&](Settings::Action a){return Key(v.keys[a]);};
    switch(glyph) {
    case 0x21:return action(Settings::Fire);
    case 0x22:return action(Settings::Jump);
    case 0x23:return action(Has(context,"reload") ? Settings::Reload : Settings::Use);
    case 0x24:return action(Settings::Weapon);
    case 0x25:return Has(context,"back") || Has(context,"cancel") ? "Esc" : action(Settings::Crouch);
    case 0x26:return action(Settings::Aim);
    case 0x27:return action(Settings::Smoke);
    case 0x28:return action(Settings::Grenade);
    case 0x29:return Has(context,"binocular") ? action(Settings::BinocularsOut) : "Up";
    case 0x2A:return "Right";
    case 0x2B:return Has(context,"objective") ? action(Settings::Objectives) : "Left";
    case 0x2C:return Has(context,"binocular") ? action(Settings::BinocularsAway) : "Down";
    case 0x2D:return Has(context,"continue") || Has(context,"start") || Kernel::CurrentMap().empty() ? "Enter" : action(Settings::Pause);
    case 0x2E:case 0x2F:case 0x30:return "Arrows";
    case 0x31:return action(Settings::Objectives);
    case 0x32:return action(Has(context,"binocular") ? Settings::BinocularsOut : Settings::Sprint);
    case 0x33:return action(Settings::Melee);
    case 0x34:return action(Settings::Forward)+"/"+action(Settings::Left)+"/"+action(Settings::Backward)+"/"+action(Settings::Right);
    case 0x35:return "Mouse";
    default:return {};
    }
}
uint32_t Scratch(uint8_t* base,const std::string& value) {
    thread_local uint32_t address=0;
    if(!address) address=Guest::AllocateKernelObject(4096);
    if(address && value.size()<4096) { memcpy(base+address,value.c_str(),value.size()+1);return address; }
    return 0;
}
float Advance(uint8_t* base,uint32_t font,const std::string& text) {
    const uint32_t first=Guest::Read32(base,font+36),count=Guest::Read32(base,font+40),glyphs=Guest::Read32(base,font+8);
    float width=0;
    for(unsigned char c:text) if(c>=first && c-first<count) width+=Guest::Read32(base,glyphs+(c-first)*28+24);
    return width;
}
using Function=void(*)(PPCContext&,uint8_t*);
std::string PlainText(const std::string& text) {
    if(text=="PRESS START") return "PRESS ENTER";
    if(text=="Quad Speed" || text=="QUAD SPEED") return "Sprinting";
    const std::string phrase="right and left sticks";
    const size_t at=text.find(phrase);
    if(at!=std::string::npos) {
        const auto v=Settings::Get();
        const std::string movement=Key(v.keys[Settings::Forward])+"/"+Key(v.keys[Settings::Left])+"/"+
            Key(v.keys[Settings::Backward])+"/"+Key(v.keys[Settings::Right]);
        return text.substr(0,at)+"["+movement+"] and the mouse"+text.substr(at+phrase.size());
    }
    return {};
}
void Measure(PPCContext& ctx,uint8_t* base,Function original) {
    const bool icons=IconFont(base,ctx.r3.u32);
    if(!icons && keyboard.load() && ctx.r4.u32>=0x10000 && (base[ctx.r4.u32]=='P' || base[ctx.r4.u32]=='U' || base[ctx.r4.u32]=='Q' || base[ctx.r4.u32]=='Q' || base[ctx.r4.u32]=='Q')) {
        const auto replacement=PlainText(Text(base,ctx.r4.u32));
        if(!replacement.empty()) { const uint32_t scratch=Scratch(base,replacement);if(scratch) ctx.r4.u32=scratch; }
    }
    original(ctx,base);
}
void Draw(PPCContext& ctx,uint8_t* base,Function original) {
    static const char* shot=getenv("COD3_PROMPT_SHOT");
    if(shot) {
        static const ULONGLONG start=GetTickCount64();static bool taken=false;
        if(!taken && GetTickCount64()-start>=ULONGLONG(atof(shot)*1000)) { taken=true;Overlay::RequestScreenshot(); }
    }
    const bool icons=IconFont(base,ctx.r3.u32);
    if(!keyboard.load(std::memory_order_relaxed)) { original(ctx,base);return; }
    const uint32_t font=regularFont.load(std::memory_order_relaxed);
    if(!icons && (ctx.r4.u32<0x10000 || (base[ctx.r4.u32]!='P' && base[ctx.r4.u32]!='U' && base[ctx.r4.u32]!='Q' && base[ctx.r4.u32]!='Q' && base[ctx.r4.u32]!='Q'))) { original(ctx,base);return; }
    std::string text=Text(base,ctx.r4.u32),replacement;
    if(icons && font) {
        const auto settings=Settings::Get();
        for(unsigned char c:text) {
            const auto label=Label(c,settings,contextText);
            if(label.empty()) { original(ctx,base);return; }
            replacement+='['+label+']';
        }
    } else replacement=PlainText(text);
    if(replacement.empty()) { original(ctx,base);return; }
    const uint32_t scratch=Scratch(base,replacement);
    if(!scratch) { original(ctx,base);return; }
    if(icons) {
        const float before=Advance(base,ctx.r3.u32,text),after=Advance(base,font,replacement);
        const double fit=after>0 ? std::min(0.68,double(before/after)) : 0.68;
        ctx.f2.f64+=Guest::Read32(base,font+28)*(1.0-fit)*ctx.f5.f64*0.5;
        ctx.r3.u32=font;ctx.f4.f64*=fit;ctx.f5.f64*=fit;
    }
    if(replacement=="Sprinting") { ctx.f4.f64*=0.65;ctx.f5.f64*=0.65; }
    ctx.r4.u32=scratch;
    if(getenv("COD3_PROMPT_TRACE")) {
        static thread_local std::string last;
        const std::string line=text+" -> "+replacement+" context "+contextText;
        if(line!=last) { last=line;printf("keyboard prompt: %s\n",line.c_str());fflush(stdout); }
    }
    original(ctx,base);
}
void Rich(PPCContext& ctx,uint8_t* base,Function original) {
    std::string previous=std::move(contextText);
    const uint32_t string=Guest::Read32(base,ctx.r3.u32+20);
    contextText=string ? Text(base,string+12) : "";
    std::transform(contextText.begin(),contextText.end(),contextText.begin(),[](unsigned char c){return char(std::tolower(c));});
    original(ctx,base);
    contextText=std::move(previous);
}
// Optional developer verification sheet; never displayed during normal play.
void Sheet(PPCContext& ctx,uint8_t* base,Function original) {
    if(!getenv("COD3_PROMPT_SHEET") || !IconFont(base,ctx.r3.u32) || base[ctx.r4.u32]!=0x22) return;
    const uint32_t font=regularFont.load();if(!font) return;
    struct Row { const char* name; unsigned char glyph; };
    const Row rows[]={{"FIRE",0x21},{"AIM",0x26},{"RELOAD",0x23},{"USE",0x23},{"JUMP",0x22},
        {"CROUCH",0x25},{"WEAPON",0x24},{"MELEE",0x33},{"SPRINT",0x32},{"GRENADE",0x28},{"SMOKE",0x27}};
    const std::string previous=contextText;
    for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);++i) {
        PPCContext call=ctx;call.r3.u32=font;call.r4.u32=Scratch(base,rows[i].name);
        call.f1.f64=60;call.f2.f64=35+i*27;call.f4.f64=0.5;call.f5.f64=0.5;call.r8.u32=0xFFFFFFFFu;
        original(call,base);
        contextText=rows[i].name;
        std::transform(contextText.begin(),contextText.end(),contextText.begin(),[](unsigned char c){return char(std::tolower(c));});
        call=ctx;call.r4.u32=Scratch(base,std::string(1,char(rows[i].glyph)));
        call.f1.f64=220;call.f2.f64=35+i*27;
        Draw(call,base,original);
        call=ctx;call.r4.u32=Scratch(base,std::string(1,char(rows[i].glyph)));
        call.f1.f64=330;call.f2.f64=35+i*27;
        original(call,base);
    }
    contextText=previous;
}
}
void ButtonPrompts::KeyboardUsed() { keyboard.store(true,std::memory_order_relaxed); }
void ButtonPrompts::ControllerUsed() { keyboard.store(false,std::memory_order_relaxed); }
bool ButtonPrompts::KeyboardActive() { return keyboard.load(std::memory_order_relaxed); }
std::string ButtonPrompts::ActionKey(Settings::Action action) { return "["+Key(Settings::Get().keys[action])+"]"; }

#ifndef BUTTON_PROMPTS_UNIT_TEST
#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_8214CA50);
extern "C" PPC_FUNC(__imp__sub_8214CE30);
extern "C" PPC_FUNC(__imp__sub_824EC430);
PPC_FUNC(sub_8214CA50) { Measure(ctx,base,__imp__sub_8214CA50); }
PPC_FUNC(sub_8214CE30) { Sheet(ctx,base,__imp__sub_8214CE30);Draw(ctx,base,__imp__sub_8214CE30); }
PPC_FUNC(sub_824EC430) { Rich(ctx,base,__imp__sub_824EC430); }
#else
extern "C" PPC_FUNC(__imp__sub_82134B90);
extern "C" PPC_FUNC(__imp__sub_82134F70);
extern "C" PPC_FUNC(__imp__sub_824D7D88);
PPC_FUNC(sub_82134B90) { Measure(ctx,base,__imp__sub_82134B90); }
PPC_FUNC(sub_82134F70) { Sheet(ctx,base,__imp__sub_82134F70);Draw(ctx,base,__imp__sub_82134F70); }
PPC_FUNC(sub_824D7D88) { Rich(ctx,base,__imp__sub_824D7D88); }
#endif
#endif


