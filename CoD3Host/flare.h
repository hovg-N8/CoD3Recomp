#pragma once

#include <cstdint>
#include <string>

// 0.5.739: World at War's signal flare in CoD3's multiplayer (a copy of the
// smoke grenade, mp_fixes.cpp's Guns "signal_flare", in a custom class's
// special grenade's place). This file has its burning and what it does: its
// glow and sparks where it lies, the blinding of whoever sees it near
// (World at War's maps/mp/_flare.gsc: 8 seconds, 400 units, stronger the
// nearer, gone half a second after looking away), its sounds. mp_fixes.cpp
// starts it (the grenade's explosion, event 56) and traces the sight lines.
// The files are custom\flare\ beside the game, made from the user's own
// World at War by tools/waw/make_flare.py.
namespace Flare
{
    std::string Dir();                                 // custom\flare\ (with the slash)
    // one thrown there (0.5.746: lit after the fuse, sparks only till then); its id
    uint32_t Ignite(const float at[3], float fuse = 0.0f);
    void MoveTo(uint32_t id, const float at[3]);       // where its grenade is now (in flight)
    void Camera(const float eye[3], const float axes[9], float fov);   // the view as drawn (each frame)
    // each frame: how many are burning and where (for the sight traces)
    int Count();
    bool At(int index, float at[3], uint32_t& id);
    void Seen(uint32_t id, bool seen);                 // our eye sees it (no wall between)
    bool Lit(uint32_t id);                             // 0.5.753: burning now (past its fuse, not burnt out)
}
