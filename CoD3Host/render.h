#pragma once

// The renderer: the title's frames drawn on the host GPU through Direct3D 11.
//
// The title's own graphics driver writes a Xenos command stream, and the
// command processor in gpu.cpp decodes that into register writes and the
// calls below. Nothing past this point is a model of the console's GPU:
// the registers of a draw are read once into a plain render state
// (render_state.h), the state is mapped to cached Direct3D objects
// (render_pipeline.h), the title's programs are translated to HLSL once
// and kept (render_shaders.h), its vertex buffers and textures are uploaded
// once and kept (render_resources.h), and the draw is one Direct3D call.
// The layers, from the title down:
//
//   game logic (recompiled)  ->  PM4 packets  ->  register file (gpu.cpp)
//     -> Render::Draw / Resolve / Swap             (this interface)
//     -> DrawState, the registers read once         (render_state.h)
//     -> DrawCommand: handles, not descriptions     (render_state.h)
//     -> caches: pipeline, shaders, resources       (render_*.h)
//     -> Direct3D 11, one context, one thread       (render_d3d11.cpp)
//
// The command thread draws; the window thread only pumps messages. The
// swap packet ends the title's frame, and that is where the frame is put on
// the window, with or without vertical sync as the settings say.

#include <cstdint>

namespace Render
{
    // Whether the host GPU is used at all. The device is made on first use.
    bool Enabled();

    // A draw from the command stream: the initiator word, and for an indexed
    // draw the guest address of the indices and their size word.
    void Draw(uint32_t initiator, uint32_t indexBase, uint32_t indexWord);

    // A resolve: the render target the registers describe into the texture
    // at the destination they name, with the clears they ask for.
    void Resolve();

    // A shader upload from the stream: the stage, and where the microcode
    // lies in guest memory. The program becomes the stage's current one.
    void ShaderLoaded(bool pixel, uint32_t guestAddress, uint32_t sizeDwords, uint32_t start = 0);

    // A draw's program from its native record (native_state.cpp): as a
    // load from the stream, the microcode's hash known already.
    void UseProgram(bool pixel, uint32_t guestAddress, uint32_t sizeDwords, uint64_t hash);

    // VdSwap reached in the stream: the front buffer the title finished, by
    // physical address. The frame goes on the window from here.
    void Swap(uint32_t frontBufferPhysical, uint32_t width, uint32_t height);

    // The window thread: the window the frames go on, and its client size
    // whenever it changes. Presenting is not the window thread's business.
    void SetWindow(void* hwnd, int clientWidth, int clientHeight);

    // The window thread, when nothing has been swapped for a while: the
    // last frame again, so the window is not stale under a resize or the
    // overlay. Does nothing when a frame went out recently.
    void PresentIdle();

    // Guest memory the title freed, from any thread: what was uploaded from
    // it (textures, vertex buffers, resolved surfaces) is dropped by the
    // command thread before its next draw, since the address will hold
    // something else soon.
    void MemoryFreed(uint32_t address, uint32_t size);

    // The title reads the depth resolve at this physical address on the CPU,
    // from any thread (cpu_depth.cpp): each resolve to it is written back
    // to guest memory, as the console's resolve wrote it, a frame or two
    // late. COD3_DEPTHREADBACK=0 leaves it on the host.
    void CpuReadsDepthAt(uint32_t physical);

    // The button prompts drawn as keys (pc_prompts.cpp): while cells are
    // given, the texture at `physical` (the button icon font's) is drawn as
    // a picture made here, a key cap in each cell (u0 v0 u1 v1 over the
    // texture) with its label (UTF-8); none, the texture as it is. From any
    // thread; the picture is made again only when the cells change.
    struct PromptCell { float u0, v0, u1, v1; char label[24]; };
    void SetPromptKeys(uint32_t physical, const PromptCell* cells, uint32_t count);
    // 0.5.653: draws reading the vertex buffer at this guest address are given
    // this PNG's texture in their pixel texture slot (COD3_MESHTEX_SLOT, 0 by
    // default): CoD2's Webley on the Colt's body (mp_fixes.cpp). 0 clears it.
    // 0.5.661: several (one a buffer); slot -1: COD3_MESHTEX_SLOT's (5), -2:
    // the draw's lowest texture slot (a mesh whose colour map's is not known)
    void SetMeshTexture(uint32_t vertexBuffer, const char* pngPath, int slot = -1);
    // 0.5.664: a PNG for another of those draws' pixel texture slots (their
    // normal and specular maps); empty clears it
    void SetMeshSlotTexture(uint32_t vertexBuffer, int slot, const char* pngPath);
    // 0.5.714: DIAGNOSTIC: the vertex program's 256 constants of draws reading
    // this vertex buffer written to <dir>vsconst_<n>.bin, three times 8 s apart
    // (the M2's view mesh: what puts its nozzle on the screen)
    void ProbeVertexBuffer(uint32_t vertexBuffer, const char* dir);
    // 0.5.717: a point of the mesh in this vertex buffer followed onto the
    // screen each time it is drawn, as its own draw puts it (its rigid bone,
    // constants 16-18, then the view and projection, 12-15: 0.5.716's dumps):
    // the M2's nozzle. TrackedPoint: its place on the picture (fractions,
    // y down) if drawn within the last 0.15 s.
    void TrackMeshPoint(uint32_t vertexBuffer, float x, float y, float z);
    bool TrackedPoint(float& x, float& y);
    // 0.5.718: that draw's own transforms (its bone, 3 rows of 4; its view and
    // projection, 4 rows of 4), if drawn within the last 0.15 s: the whole mesh
    // put on the screen as it was drawn (the gun over its flame: overlay.cpp)
    bool TrackedTransform(float bone[12], float viewProjection[16]);
    // 0.5.809: the vertex buffers (guest addresses) whose draws are drawn a
    // second time mirrored left to right (the dual pistols: the right arm and
    // its Colt again as the left); none (count 0) turns it off
    void SetMirrorBuffers(const uint32_t* buffers, int count);
    // 0.5.811: a reload of the dual pistols begun now, taking this long (0:
    // none): first the right gun is reloaded (the left not drawn), then for as
    // long the left (the right not drawn, the right's reload replayed mirrored)
    void SetMirrorReload(uint32_t milliseconds);
    // 0.5.811: how far apart the two guns are moved (fractions of the screen's
    // half width each way; 0: as mirrored)
    void SetMirrorSpread(float spread);
    // 0.5.821: DIAGNOSTIC: every draw for this many milliseconds logged (the
    // dual Colts' muzzle flash: which draws it is)
    void LogDrawsFor(uint32_t milliseconds);
    // 0.5.824: the dual Colts fired: for this long their muzzle flash (the
    // effect's draws near the eye) is moved with the right gun and drawn again
    // mirrored at the left one (RenderCommands::PrepareMirror)
    void DualFlashFor(uint32_t milliseconds);
    // 0.5.837: Seelow Night: the arms' light-grid terms (pixel program
    // 9a14f83313a256a4, c18-c26) scaled by this rgb while on
    void SetNightLight(bool on, const float scale[3]);
    // 0.5.677: a PNG for any texture whose base address (its fetch constant's)
    // is this guest place, used when such a texture is made; empty clears it
    void SetTextureImage(uint32_t address, const char* pngPath);
    // Filling the screen (F11, "Fill the screen"): the window's shape, width
    // over height, that the title's view is to be made for; 0 when the
    // picture is the console's 16:9. Read by the title's view setup
    // (title_fixes.cpp) from its own thread.
    float FillAspect();

    // The hash of the stage's current program, for the traces.
    uint64_t CurrentProgramHash(bool pixel);

    // The screenshot key: the next frame's draws go to the log in full, as
    // COD3_D3DFRAME logs one, so a fault seen while playing can be read.
    void LogNextFrame();

    // The ten second report.
    void Report();

    // For the HDR tab: whether Windows has the window's display in HDR, and
    // the peak it reports for it (nought when it reports none); whether the
    // swap chain is scRGB now; and how many of the HDR programs are still
    // being compiled.
    void DisplayHdr(bool& hdr, float& peakNits);
    bool HdrActive();
    int HdrCompiling();
}
