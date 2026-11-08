// The depth the title reads on the CPU (0.5.477, from FrankHUN88453's
// CoD3Recomp d35101ce, "render: the depth the title reads on the CPU written
// back to guest memory").
//
// The light coronas (the oil lamps and the level's lights, a glow sprite
// each) are drawn at the near plane, in front of everything, and fade in and
// out by what the CPU finds: sub_82420F48(x, y) reads the depth at that point
// of the screen from a depth resolve the title keeps a texture for, turns it
// into a distance, and a light farther away than that fades out. On the
// console the resolve wrote the surface to memory. Here the resolves stay on
// the host, the title read zeros (the far plane) and every corona shone
// through the walls.
//
// So the renderer is told which resolve the title reads, and writes that one
// back to guest memory (Render::CpuReadsDepthAt), through the depth write
// back render_d3d11.cpp already has (WriteDepthBack, until now only for the
// debug probe's depth_writeback). The texture: the object at 0x82A2ABDC, +0
// its width, +4 its height, +20 the texture, whose fetch constant's second
// word (+32) holds the base address. Their multiplayer twins are in
// mp_addresses.txt, matched by hand: the same code, scheduled otherwise.

#include "kernel.h"
#include "render.h"

extern "C" PPC_FUNC(__imp__sub_82420F48);

namespace
{
    constexpr uint32_t DepthLookupTexture = 0x82A2ABDC;
}

PPC_FUNC(sub_82420F48)
{
    if (const uint32_t object = Guest::Read32(base, DepthLookupTexture); object != 0)
        if (const uint32_t texture = Guest::Read32(base, object + 20); texture != 0)
            if (const uint32_t at = Guest::Read32(base, texture + 32) & 0xFFFFF000u; at != 0)
                Render::CpuReadsDepthAt(at);
    __imp__sub_82420F48(ctx, base);
}
