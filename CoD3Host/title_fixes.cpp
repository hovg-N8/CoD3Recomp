// Fixes to the title's own code: faults of the game itself that the
// console survives by the luck of its memory layout and the port does not.
// Each replaces one recompiled function through its weak alias, which the
// stronger definition here takes the place of, and changes as little as
// the fault needs.

#include "heap_trace.h"
#include "kernel.h"
#include "render.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>

// --- a destructor that runs twice ------------------------------------------------------------------
//
// sub_823F0828 tears down a list of objects at the end of a level (and at a
// mission's restart after "MISSION FAILED", which unloads the level too).
// For each object it calls the destructor, sub_823F04B0, and then deletes
// the object through its vtable; the deleting destructor behind that slot,
// sub_820C8478, calls sub_823F04B0 again. The destructor frees six fields
// and releases their handles without clearing them, so the second run
// frees the same blocks a second time.
//
// The title's allocator is dlmalloc, and a block freed twice goes onto a
// bin's list twice: the list closes into a cycle, and the next allocation
// that walks it (sub_820CCF00) never returns. That was one of the hangs at
// the load of a second level and after a mission's restart. (The console
// runs the same code and does not hang; presumably its memory is laid out
// so that the second free finds the chunk already merged away and is
// refused. That has not been checked.)
//
// The second run is left out: when sub_823F04B0 is called from inside the
// deleting destructor for the very object the teardown loop has just
// destroyed, it returns at once. Every other call runs as it is.
namespace
{
    constexpr uint32_t TeardownLoopCall = 0x823F0880;       // return address of the call in sub_823F0828
    constexpr uint32_t DeletingDestructorCall = 0x820C84A4;  // return address of the call in sub_820C8478
    thread_local uint32_t t_justDestroyed = 0;
}

extern "C" PPC_FUNC(__imp__sub_823F04B0);
PPC_FUNC(sub_823F04B0)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t from = uint32_t(ctx.lr);
    // COD3_NODTORFIX=1 lets the second run happen, to see whether it still
    // does harm.
    static const bool leaveIt = getenv("COD3_NODTORFIX") != nullptr;
    if (!leaveIt && from == DeletingDestructorCall && object != 0 && object == t_justDestroyed)
    {
        t_justDestroyed = 0;
        return;
    }
    t_justDestroyed = from == TeardownLoopCall ? object : 0;
    HeapTrace::Destroying(ctx, base);
    __imp__sub_823F04B0(ctx, base);
}

// --- the view made for the screen's shape (a PC addition) -------------------------------------------
//
// The player's view setup stores, last, its field of view across and up and
// down in degrees, in the client game's array of local players: campaign
// sub_82443A98 at cg + 4464 / +4468 (cg 0x82A98D90, 5840 bytes a player, so
// 0x82A99F00 for the first), multiplayer sub_8245CDC0 at cg + 4944 / +4948
// (cg 0x82C7D290, 6368 a player); the player the view is for is the word
// each reads first (0x82A2A1C8, 0x82BCBE9C). Found by their values (cg_fov
// 65 gives 80.69 by 51.08), a write watch, and the same code in both. The
// title makes the one across for 16:9 from the one up and down. Filling a
// screen of another shape (render_d3d11.cpp), the one across is made again
// for that shape from the one up and down, so the title itself draws, and
// culls, the wider (or narrower) view, aiming and scopes included; the
// renderer draws it into a target of the screen's shape.
namespace
{
    void FovForScreen(uint8_t* base, uint32_t cg, uint32_t across, uint32_t players, uint32_t stride, uint32_t playerWord)
    {
        const float aspect = Render::FillAspect();
        if (aspect <= 0.0f) return;
        const uint32_t player = Guest::Read32(base, playerWord);
        if (player >= players) return;
        const uint32_t at = cg + across + player * stride;
        auto readFloat = [&](uint32_t a) { const uint32_t v = Guest::Read32(base, a); float f; memcpy(&f, &v, 4); return f; };
        const float upDown = readFloat(at + 4);
        if (!(upDown > 1.0f && upDown < 170.0f)) return;
        const float rad = 3.14159265f / 180.0f;
        const float wide = 2.0f * std::atan(std::tan(upDown * 0.5f * rad) * aspect) / rad;
        uint32_t v;
        memcpy(&v, &wide, 4);
        Guest::Write32(base, at, v);
    }
}

#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_8245CDC0);
PPC_FUNC(sub_8245CDC0)
{
    __imp__sub_8245CDC0(ctx, base);
    FovForScreen(base, 0x82C7D290u, 4944, 4, 6368, 0x82BCBE9Cu);
}
#else
extern "C" PPC_FUNC(__imp__sub_82443A98);
PPC_FUNC(sub_82443A98)
{
    __imp__sub_82443A98(ctx, base);
    FovForScreen(base, 0x82A98D90u, 4464, 4, 5840, 0x82A2A1C8u);
}
#endif

#ifndef COD3_MP   // missions and their films are single player's
// --- the mission's film, skippable once the level has loaded (a PC addition) ---------------------
//
// A mission's film plays while its level loads (sub_824636D0 opens it and
// the loading screen's callback, sub_824EECF8, draws it), and once the load
// is done the level's start (sub_82517A00) plays the rest to its end in
// sub_824EEBA0. That loop asks sub_824D0470 whether to stop: any button
// pressed, once the film's clock (0x82A2A254, from nought in the loop) has
// reached the threshold at 0x82A2A258. The intros set the threshold low and
// can be skipped with A (Space); a mission's film has it set to the
// largest float when it opens, so on the console it plays to its end. Here
// the threshold goes to nought when the loop starts: the load is finished
// by then, and a press skips the rest. COD3_NOFILMSKIP=1 keeps the
// console's way.
// In co-op (COD3_COOP) the film is shared (coop.cpp, 0.5.231): the host's
// A skips it only once the joining copy has loaded too, and the joining
// copy's film ends when the host's does (its own A does nothing).
// COD3_SKIPFILMS=1, outside co-op: the film stops by itself once loaded.
bool CoopFilmActive();
bool CoopFilmStop(bool pressed);
void CoopFilmEnded();
namespace { thread_local bool t_inFilm = false; }
extern "C" PPC_FUNC(__imp__sub_824EEBA0);
PPC_FUNC(sub_824EEBA0)
{
    static const bool consoleWay = getenv("COD3_NOFILMSKIP") != nullptr;
    if (!consoleWay) Guest::Write32(base, 0x82A2A258u, 0);   // 0.0f
    t_inFilm = true;
    __imp__sub_824EEBA0(ctx, base);
    t_inFilm = false;
    if (CoopFilmActive()) CoopFilmEnded();
}
extern "C" PPC_FUNC(__imp__sub_824D0470);
PPC_FUNC(sub_824D0470)
{
    if (!t_inFilm) { __imp__sub_824D0470(ctx, base); return; }
    static const bool coop = CoopFilmActive();
    static const bool autoSkip = [] { const char* t = getenv("COD3_SKIPFILMS"); return t != nullptr && t[0] != '0'; }();
    if (coop)
    {
        __imp__sub_824D0470(ctx, base);
        const bool pressed = (ctx.r3.u32 & 0xFF) == 1;
        ctx.r3.u64 = CoopFilmStop(pressed) ? 1 : 0;
        return;
    }
    if (autoSkip) { ctx.r3.u64 = 1; return; }
    __imp__sub_824D0470(ctx, base);
}
#endif
