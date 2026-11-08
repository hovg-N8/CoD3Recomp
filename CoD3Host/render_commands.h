#pragma once

// The PC render layer: the draw and the resolve the title's registers
// describe, turned into commands the Direct3D executor runs.
//
// This is where the console's terms end. A draw comes in as the register
// file (RenderState::Snapshot) and goes out as a DrawCommand: programs and
// pipeline states as handles from the caches, textures and buffers as the
// views the caches keep, the scissor in host pixels, the constants and the
// indices already streamed into their rings. What is not needed to draw
// the same picture on a PC is decided here and does not reach the
// executor: the EDRAM tile a surface is named by becomes a render target
// handle, a fetch constant becomes a view and a sampler, a byte swapped
// index list becomes a range of the index ring.

#include "render_state.h"

namespace RenderCommands
{
    // The draw the registers describe, as a command. False when there is
    // nothing to draw: the skip has been counted with its reason.
    bool PrepareDraw(uint32_t initiator, uint32_t indexBase, RenderState::DrawCommand& out);

    // 0.5.809: the draw just prepared, again, mirrored left to right (its
    // vertex program's c12, the clip's x row, negated; no culling), when its
    // vertices are one of Render::SetMirrorBuffers' (the dual pistols' left
    // gun and arm). Called right after PrepareDraw, for the same draw.
    // 0.5.811: in may be changed (moved right); skipOriginal: in is not to be
    // drawn (the second part of the dual reload: only the left gun shows)
    bool PrepareMirror(RenderState::DrawCommand& in, RenderState::DrawCommand& out, bool& skipOriginal);

    // The resolve the registers describe, as a command. False when it
    // amounts to nothing.
    bool PrepareResolve(RenderState::ResolveCommand& out);

    // The draws that were not made, by reason, for the report.
    void ReportSkips();

    // Whether the title's blur while aiming down the sights is drawn: its
    // depth of field pass, three draws the pixel programs name.
    void SetAimBlur(bool drawn);

    // Surface light (F11): how much the world's solid surfaces are
    // brightened in their own pass, 0 (none) .. 127; set once a frame.
    void SetSurfaceLight(uint32_t level);

    // Filling a screen of another shape than 16:9 (render_d3d11.cpp): the
    // flat (2D) draws into the frame squeezed across by `squeeze` (or, when
    // negative, up and down by its size) so the HUD keeps its shape in the
    // middle. Called at each frame's end; it also turns over whether the
    // frame drew the world, which leaves full screen 2D (a film, a menu's
    // picture) squeezed too when it did not.
    void SetFill(float squeeze);

    // 0.5.234: whether the host's player is down (dead, waiting to respawn).
    // While he is, the depth of field pass is drawn even with the blur while
    // aiming off: the death view's "double vision" is built on it, and
    // without it the two copies of the frame came out sharp.
    void SetPlayerDown(bool down);

    // 0.5.616: in the spectator's seat (spectator.cpp): the blur the game
    // keeps behind the team choice left out of its last full screen pass.
    void SetSpectatorSeat(bool seated);

    // 0.5.248: whether the host's player has the binoculars at his eyes
    // (called every frame). While he has, the depth of field pass is drawn,
    // and the draws left out are said when they come down.
    void SetBinocularsUp(bool up);

    // 0.5.249: the next count draws said one line each (programs, state,
    // and drawn or why not), headed by why.
    void DumpDraws(const char* why, int count);
    // The HDR mode's scene pass after the bloom's composite, and the bloom's
    // strength (one the game's own), from the settings at each frame's edge.
    void SetHdr(bool scenePass, float bloom, float hudGain);
    // F11's "Steady decals": the layers over a surface a little nearer.
    void SetSteadyDecals(bool on);
    // Verify mode: the vertices and indices the frame's draws read, hashed
    // again at its swap; a change is the title writing them after the draw
    // (counted with the vertex buffers' checks). Clears the list.
    void CheckLateWrites();

    // Whether the shell shock's full screen pass is drawn (F11's "Shell
    // shock screen blur"), set once a frame with the other settings.
    void SetShellShockBlur(bool drawn);

    // F11's "Sharp shadows", "Fast shadow maps" and "Smooth foliage edges",
    // set once a frame with the other settings.
    void SetRendering(bool sharpShadows, bool fastShadowMaps, bool foliageCoverage);
}
