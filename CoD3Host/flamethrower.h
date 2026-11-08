#pragma once

#include <cstdint>
#include <string>

// 0.5.681: World at War's M2 flamethrower in CoD3's multiplayer (a copy of
// the Thompson, mp_fixes.cpp's Guns "flamethrower"). This file has its flame
// and its sounds; the view model's swap is mp_fixes.cpp's (MpFixesFlameView).
// The files are custom\flamethrower\ beside the game, made from the user's own
// World at War by tools/waw/make_flamethrower.py.
namespace Flame
{
    std::string Dir();                       // custom\flamethrower\ (with the slash)
    void Held(bool held);                    // our player has it in hand (each frame)
    void OwnShot();                          // our player's shot (a flame's moment)
    void OtherShot(uint32_t entity, float gain);   // another's, heard at this gain
    void Tick();                             // each frame: the loop and the cool-down
    // 0.5.703: others' flames drawn in the world, every flame stopped by walls
    void Camera(const float eye[3], const float axes[9], float fov);   // the view as drawn (each frame)
    bool Eye(float eye[3]);                  // the last view's place (false: none yet)
    void OwnReach(float units);              // how far our aim runs before a wall
    void OtherStream(uint32_t entity, const float muzzle[3], const float dir[3], float reach, bool visible);
    // 0.5.709: World at War's arms (mp_fixes.cpp plays them): is ours burning,
    // the view's fov, and the nozzle (World at War's tag_flash) in the view's
    // space (x ahead, y left, z up) this frame
    bool Firing();
    float Fov();
    void Nozzle(const float at[3], float fov);   // (fov: the arms', 4:3 degrees)
    void Heat(float heat, bool overheated);     // 0.5.720: ours, 0..100 (World at War's overheating; its bar)
}
