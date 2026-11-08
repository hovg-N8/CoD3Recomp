#pragma once

// The GPU resources behind the title's memory: its textures and vertex
// buffers uploaded once and kept, its render targets as textures, the
// surfaces its resolves make, and the rings its indices and constants
// stream through.
//
// Everything is found by an integer handle from a small table keyed by
// what the title names it by: a texture by its address, format and size,
// a vertex buffer by its address, a render target by its surface words
// and the scale. What the title changes under a resource (its dynamic
// vertex buffers, the HUD's textures) is caught by a fingerprint taken
// once a frame, not once a draw.

#include "render_state.h"

#include <cstdint>
#include <vector>

#include <dxgiformat.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;

namespace RenderResources
{
    using RenderState::Handle;

    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context);

    // The frame's number, for the once a frame checks, and the release of
    // what has not been used for a long while. Returns true when the
    // render targets were made again (the scale changed): what was bound
    // is gone.
    bool BeginFrame(uint64_t frame);

    // Guest memory the title freed: every texture, vertex buffer and
    // resolved surface uploaded from the range is dropped, so the address
    // holding something else later is not served the old thing. True when
    // anything went, since it may have been bound.
    bool Forget(uint32_t address, uint32_t size);

    // --- the render scale --------------------------------------------------------------

    // The title draws 1040 by 624; the targets here are that times the
    // scale, which is a fraction when the window's height asks for one.
    // A change takes effect at the next frame, when the targets are made
    // again at the new size: no frame is drawn half at each.
    void RequestScale(float scale);
    // The 560 pitch surfaces (smoke, soft particles, shadow cascades) at half
    // size, as the title has them, or at full size. Applied at the frame's edge.
    void RequestEffectsScale(double scale);   // 1 the title's half size, 1.5, 2 the frame's size
    float Scale();
    // The width drawn wider than the height's scale by this much (the
    // screen's shape over the frame's, render_d3d11.cpp's fill), 1 none.
    void RequestWiden(float widen);
    // The shadow maps (the title's square, power of two targets: its sun's
    // 512 by 512) drawn this many times finer again, 1 .. 4.
    void RequestShadowDetail(uint32_t times);
    // The MSAA samples the targets of a pitch are made with.
    uint32_t TargetSamples(uint32_t pitch);

    // Samples a pixel of the render targets: 1, 2, 4 or 8. A change takes
    // effect at the next frame like the scale; the count the device does
    // not support falls back to what it does.
    void RequestMultisample(uint32_t samples);
    uint32_t Multisample();

    // The HDR mode: the title's 8 bit colour targets (formats 0 and 1) made
    // 16 bit float, so what its drawing puts past one is kept. The targets
    // are made again at the next frame's start when this changes.
    void RequestHdrTargets(bool upgraded);
    bool HdrTargets();

    // The texture quality: how many of a texture's top mip levels are left
    // out on upload (0 all of them kept). A change takes effect at the
    // next frame, when every texture is dropped to be uploaded again.
    void RequestMipSkip(uint32_t levels);
    uint32_t MipSkip();

    // --- render targets ---------------------------------------------------------------

    struct ColorTarget
    {
        ID3D11Texture2D* texture = nullptr;
        ID3D11RenderTargetView* view = nullptr;
        ID3D11ShaderResourceView* resource = nullptr;
        uint32_t width = 0, height = 0;      // host pixels
        uint32_t pitch = 0, rows = 0;        // title pixels
        uint32_t samples = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool upgraded = false;               // an 8 bit target made 16 bit float for the HDR mode
    };
    struct DepthTarget
    {
        ID3D11Texture2D* texture = nullptr;
        ID3D11DepthStencilView* view = nullptr;
        ID3D11ShaderResourceView* resource = nullptr;
        uint32_t width = 0, height = 0;
        uint32_t pitch = 0, rows = 0;
        uint32_t samples = 1;
    };
    Handle ColorTargetFor(uint32_t colorInfo, uint32_t pitch);
    Handle DepthTargetFor(uint32_t depthInfo, uint32_t pitch);

    // The surfaces of a pitch must reach this many of the title's rows: a
    // draw or a resolve is about to use them. The targets of that pitch
    // are made again taller when they do not, colour and depth alike so
    // they stay the same size, and true says so: what was bound is gone.
    bool EnsureRows(uint32_t pitch, uint32_t rows);
    const ColorTarget* ColorTargetOf(Handle handle);
    const DepthTarget* DepthTargetOf(Handle handle);

    // A target's host size for a pitch, and the scales that map the title's
    // pixels to it: the size is whole pixels, so they are not quite Scale().
    void TargetSize(uint32_t pitch, uint32_t bytesPerSample, uint32_t& width, uint32_t& height, float& scaleX, float& scaleY);

    // --- resolved surfaces ---------------------------------------------------------------

    // What a resolve made, under the physical address the title will sample
    // it by, at the host size of the frame.
    struct Resolved
    {
        ID3D11Texture2D* texture = nullptr;
        ID3D11ShaderResourceView* resource = nullptr;
        ID3D11RenderTargetView* view = nullptr;
        uint32_t width = 0, height = 0;          // host pixels
        uint32_t guestWidth = 0, guestHeight = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool depth = false;
        uint64_t serial = 0;
    };
    // The surface at the address, made or resized to the size and format.
    Resolved* ResolvedFor(uint32_t physical, uint32_t width, uint32_t height, DXGI_FORMAT format, bool depth);
    // The surface at the address, if a resolve made one.
    const Resolved* ResolvedAt(uint32_t physical);
    // The view a fetch of that size binds: the whole surface, or a copy of
    // its top left corner when the fetch reads less than the resolve wrote.
    ID3D11ShaderResourceView* ResolvedView(uint32_t physical, uint32_t guestWidth, uint32_t guestHeight);

    // --- textures -----------------------------------------------------------------------------

    // The texture a fetch constant names, uploaded with its mip levels if
    // it is new or changed. A surface a resolve made comes back as that.
    // Returns the white texture for what cannot be uploaded.
    Handle TextureFor(const uint32_t fetch[6], uint32_t& width, uint32_t& height, bool& resolved);
    // The view of the kind the program samples it as (the fetch
    // instruction's dimension: 1 flat, 2 volume, 3 cube): a program that
    // reads a cube gets a cube (a white one when the texture is flat), one
    // that reads a flat texture gets the first face of a cube, and a
    // volume is white.
    ID3D11ShaderResourceView* TextureView(Handle handle, uint32_t dimension);
    Handle WhiteTexture();
    // Why the last TextureFor gave the white texture, for the frame log.
    const char* WhiteReason();

    // --- vertex buffers ---------------------------------------------------------------------

    // The buffer at the physical address, of at least the size, as a raw
    // buffer in the console's byte order: the programs swap the words.
    Handle VertexBufferFor(uint32_t physical, uint32_t bytes, uint32_t endian);
    ID3D11ShaderResourceView* VertexBufferView(Handle handle);

    // With "Watch memory writes" off, a big buffer is looked at whole once
    // a frame, by its first draw. NeedsRangeLook: the buffer VertexBufferFor
    // just gave was not looked at by that call, and has been changing (the
    // title's pool of dynamic geometry, written a piece at a time through
    // the frame). LookRange: the part a draw reads, compared with what went
    // up and sent again where it differs.
    bool NeedsRangeLook(Handle handle);
    // 0.5.374: a small buffer the title rewrites within the frame, kept by
    // a copy: its part a draw reads is compared, or all of it when the
    // draw's vertices can't be told.
    bool MidFrameRangeLook(Handle handle);
    void LookRange(Handle handle, uint32_t physical, uint32_t firstByte, uint32_t endByte);
    // Verify mode (verify.h): the part a draw reads held against what was
    // sent of the buffer; a difference is said and sent again.
    void VerifyRange(Handle handle, uint32_t physical, uint32_t firstByte, uint32_t endByte);

    // --- the rings ---------------------------------------------------------------------------

    // Indices for a draw, appended: where they start in the index ring.
    // The ring is discarded and started over when full, which is safe: an
    // offset is used by the draw that made it and never again.
    uint32_t IndexAppend(const void* data, uint32_t bytes);
    // Or the room mapped, to write into in place: null when it could not be.
    uint8_t* IndexMap(uint32_t bytes, uint32_t& offset);
    void IndexUnmap();
    ID3D11Buffer* IndexRing();

    // A draw's indices kept converted (COD3_WRITEWATCH): a buffer of them
    // from its start, 32 bit or not in `indices32`, or null to stream them
    // through the ring. `reset` is the strip reset index when strips are
    // reset (PA_SU_SC_MODE_CNTL bit 21), else ~0.
    ID3D11Buffer* IndexBufferFor(uint32_t physical, uint32_t count, bool wide, uint32_t reset, bool& indices32);

    // Constants, the same way, in whole 256 byte units for the offset
    // binding. Reserve first for everything one draw will append: a wrap
    // in the middle of a draw's appends would leave the earlier ones in
    // the discarded buffer. Returns true when the reserve wrapped, and
    // then every offset remembered from before is stale.
    bool ConstantReserve(uint32_t bytes);
    uint32_t ConstantAppend(const void* data, uint32_t bytes);
    // Or one map for a draw's blocks together: where to write them, and
    // the offset of the first; the blocks inside are for the caller to
    // lay out in 256 byte units.
    uint8_t* ConstantMap(uint32_t bytes, uint32_t& offset);
    void ConstantUnmap();
    ID3D11Buffer* ConstantRing();
    bool ConstantOffsetsAvailable();

    struct Statistics
    {
        uint64_t textureUploads = 0, textureBytes = 0;
        uint64_t bufferUploads = 0, bufferBytes = 0;
        uint64_t indexBytes = 0, constantBytes = 0;
        uint64_t resourceBytes = 0;     // what the caches hold now, an estimate
        uint32_t textures = 0, buffers = 0, colorTargets = 0, depthTargets = 0, resolved = 0;
        uint32_t released = 0;
    };
    Statistics Stats();
}
