#pragma once

// A draw's state, read once from the register file into plain fields, and
// the PC render command it becomes.
//
// The title's driver leaves the register file describing the draw it is
// about to make: the surface, the depth and blend control, the scissor,
// the fetch constants of the programs' inputs. Snapshot is those words,
// nothing decoded from them but the fields the backend keys its caches by.
// Reading them is thirty relaxed loads; everything after works on the copy.
//
// DrawCommand is the other side: what the PC renderer draws, in its own
// terms. Programs and pipeline states by integer handle into the caches,
// resources by the views the caches own, the scissor and the viewport in
// host pixels, the constants already in the ring at offsets, the indices
// already in the index ring. Nothing in it is a console register, and the
// executor that runs it (render_d3d11.cpp) touches nothing but the
// Direct3D context and what it remembers having set on it.

#include <cstdint>
#include "blood_render.h"

struct ID3D11ShaderResourceView;
struct ID3D11SamplerState;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11Texture2D;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11Buffer;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;

namespace RenderState
{
    // The register indices, by name, as the title's driver writes them.
    enum Register : uint32_t
    {
        SurfaceInfo = 0x2000,          // pitch in the low bits
        ColorInfo0 = 0x2001,           // base tile, format
        DepthInfo = 0x2002,
        ColorInfo1 = 0x2003, ColorInfo2 = 0x2004, ColorInfo3 = 0x2005,
        WindowOffset = 0x2080,
        ScissorTopLeft = 0x2081,
        ScissorBottomRight = 0x2082,
        MaxVertexIndex = 0x2100, MinVertexIndex = 0x2101,
        IndexOffset = 0x2102,
        ResetIndex = 0x2103,
        ColorMask = 0x2104,
        BlendRed = 0x2105,             // the constant colour, four floats
        StencilRefMask = 0x210D,
        AlphaReference = 0x210E,
        ViewportXScale = 0x210F,       // then offset x, scale y, offset y, scale z, offset z
        ProgramControl = 0x2180,       // bit 18: the pixel parameters are generated
        ContextMisc = 0x2181,          // bits 8-15: the pixel program register they go to
        DepthControl = 0x2200,
        BlendControl0 = 0x2201,
        ColorControl = 0x2202,         // the alpha test
        ClipControl = 0x2204,
        SuScModeControl = 0x2205,      // cull, facing, strip reset
        VteControl = 0x2206,           // which viewport terms apply
        ModeControl = 0x2208,          // the render backend's mode: 4 draws, 6 copies
        BlendControl1 = 0x2209, BlendControl2 = 0x220A, BlendControl3 = 0x220B,
        PointSize = 0x2280,
        PolyOffsetFrontScale = 0x2380, // then front offset, back scale, back offset: floats
        VertexControl = 0x2302,        // PA_SU_VTX_CNTL: bit 0 where pixel centres are
        CopyControl = 0x2318,          // the resolve
        CopyDestBase = 0x2319, CopyDestPitch = 0x231A, CopyDestInfo = 0x231B,
        CopyDepthClear = 0x231D, CopyColorClear = 0x231E,
        VertexConstants = 0x4000,      // 256 float4
        PixelConstants = 0x4400,       // 256 float4
        FetchConstants = 0x4800,       // 32 texture fetches of 6 words, or 96 vertex fetches of 2
        BoolConstants = 0x4900,        // 8 words of booleans, then 32 loop constants
    };

    enum Primitive : uint32_t
    {
        Points = 1, Lines = 2, LineStrip = 3, Triangles = 4, TriangleFan = 5, TriangleStrip = 6,
        Rectangles = 8, Quads = 13,
    };

    struct Snapshot
    {
        // The initiator word, decoded.
        uint32_t primitive = 0;
        uint32_t sourceSelect = 0;     // 0 indexed from memory, 2 from the vertex id
        uint32_t indexCount = 0;
        bool wideIndices = false;      // 32 bit indices, else 16
        uint32_t indexBase = 0;        // physical, when indexed

        // The surface.
        uint32_t pitch = 0;
        uint32_t colorInfo[4] = {};
        uint32_t depthInfo = 0;
        uint32_t modeControl = 0;      // 0x2208 & 7
        uint32_t colorMask = 0;
        uint32_t depthControl = 0;
        uint32_t stencilRefMask = 0;

        // The rasteriser.
        uint32_t suScModeControl = 0;
        float polyOffset[4] = {};      // front scale, front offset, back scale, back offset
        uint32_t scissorTopLeft = 0, scissorBottomRight = 0, windowOffset = 0;
        int32_t indexOffset = 0;
        uint32_t minVertexIndex = 0, maxVertexIndex = 0;   // VGT_MIN/MAX_VTX_INDX: the hardware clamps indices into these
        uint32_t resetIndex = 0;
        uint32_t pointSize = 0;
        uint32_t vertexControl = 0;    // PA_SU_VTX_CNTL: bit 0 clear, pixel centres at whole coordinates (Direct3D 9's)

        // Blending.
        uint32_t blendControl[4] = {};
        float blendFactor[4] = {};

        // What the vertex program's output goes through, and the alpha test.
        uint32_t vteControl = 0;
        uint32_t colorControl = 0;
        uint32_t alphaReference = 0;
        uint32_t programControl = 0, contextMisc = 0;
        float viewport[6] = {};        // x scale, x offset, y scale, y offset, z scale, z offset

        // The programs.
        uint64_t vertexProgram = 0;    // microcode hashes
        uint64_t pixelProgram = 0;
    };

    // The state from the register file, for a draw with this initiator.
    void Read(uint32_t initiator, uint32_t indexBase, Snapshot& out);

    // Where the draw's registers are read from, on this thread: the register
    // file (null), or a draw's own registers by register number (the native
    // path's record, native_state.cpp; only the draw's registers, 0x2000..
    // 0x2387 and the constants, are in it). Register and Registers read
    // from there.
    void UseRegisters(const uint32_t* registers);
    bool UsingRecord();
    uint32_t Register(uint32_t index);
    const uint32_t* Registers();

    // A fetch constant's words, straight from the file.
    void ReadTextureFetch(uint32_t slot, uint32_t words[6]);
    void ReadVertexFetch(uint32_t slot, uint32_t& word0, uint32_t& word1);

    // The registers of a resolve: the copy that ends a pass.
    struct ResolveSnapshot
    {
        uint32_t control = 0;          // source, command, clears
        uint32_t destBase = 0;         // physical, where the title will sample it
        uint32_t destInfo = 0;
        uint32_t destPitch = 0;        // RB_COPY_DEST_PITCH: pitch, and height from bit 16
        uint32_t pitch = 0;
        uint32_t scissorTopLeft = 0, scissorBottomRight = 0;
        uint32_t colorInfo[4] = {};
        uint32_t depthInfo = 0;
        uint32_t colorClearValue = 0, depthClearValue = 0;
    };
    void ReadResolve(ResolveSnapshot& out);

    // --- the commands -------------------------------------------------------------

    using Handle = uint32_t;

    // The programs' own constants (b2 in the translated HLSL): the viewport
    // the vertex program folds in, the alpha test, and each texture's size
    // and swizzle. The same layout the programs declare.
    struct DrawConstants
    {
        float viewportScale[4];
        float viewportOffset[4];
        float targetSize[4];
        uint32_t flags[4];
        float textureSize[32][4];
        uint32_t textureAdjustment[32][4];
        float pixelGen[4];             // guest pixels per host pixel x and y, the register, on or off
        uint32_t coverage[4];          // x: the target's samples when an alpha tested edge goes to coverage, else 0; y: the HDR mode's float targets by slot, whose output is kept in range
    };

    // One draw, ready to run. Handles are indices into the caches; the
    // views are the ones the caches own and keep, compared by identity
    // with what the context has. Everything the draw streams (indices,
    // constants) is in a ring already, at the offsets given here.
    struct DrawCommand
    {
        // The programs.
        Handle vertexShader = 0, pixelShader = 0;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        bool rectangles = false;       // through the rectangle list geometry program
        bool bloodWorldStage=false, bloodProjectionValid=false, bloodScene=false;
        uint64_t bloodFrame=0;
        BloodRender::Projection bloodProjection{};
        bool opaque = false;           // no blending: the flat debug view may replace the pixel program

        // The pipeline.
        Handle blend = 0, depth = 0, rasterizer = 0;
        float blendFactor[4] = {};
        uint32_t stencilReference = 0;
        uint32_t topology = 0;         // D3D11_PRIMITIVE_TOPOLOGY

        // The targets, and their own resource views so the executor can
        // unbind a surface it is about to draw into.
        ID3D11RenderTargetView* targets[4] = {};
        ID3D11ShaderResourceView* targetResources[4] = {};
        ID3D11DepthStencilView* depthView = nullptr;
        ID3D11ShaderResourceView* depthResource = nullptr;
        ID3D11Texture2D* colorTexture = nullptr;   // for the frame log's dump
        ID3D11Texture2D* depthTexture = nullptr;
        uint32_t targetCount = 0;
        uint32_t targetWidth = 0, targetHeight = 0;   // the viewport
        int32_t scissor[4] = {};       // left, top, right, bottom, host pixels

        // The textures and samplers, by the slots the programs name.
        struct Texture              // plain, so the array is not zeroed for every draw
        {
            uint8_t stage;             // 0 pixel, 1 vertex
            uint8_t slot, samplerSlot;
            ID3D11ShaderResourceView* view;
            ID3D11SamplerState* sampler;
        };
        static constexpr uint32_t MaxTextures = 64;
        Texture textures[MaxTextures];
        uint32_t textureCount = 0;

        // The vertex buffers, raw, by fetch slot (t32 upwards in the vertex program).
        struct Buffer
        {
            uint8_t slot;
            ID3D11ShaderResourceView* view;
        };
        static constexpr uint32_t MaxBuffers = 32;
        Buffer buffers[MaxBuffers];
        uint32_t bufferCount = 0;

        // The constants. With the ring: the float files' and the draw
        // block's offsets in it, in bytes, to bind. Without (no offsetting
        // on the device): the whole files and the block, to upload when
        // marked changed.
        bool ring = false;
        uint32_t floatAt[2] = {}, floatCount[2] = {};   // bytes, float4s
        uint32_t drawAt = 0, drawBytes = 0;
        const uint32_t* floatFile[2] = {};              // 1024 words each, when !ring and changed
        bool floatChanged[2] = {};
        const DrawConstants* drawConstants = nullptr;   // when !ring and changed
        bool drawChanged = false;
        const uint32_t* bools = nullptr;                // 40 words, when changed
        bool boolsChanged = false;

        // The geometry: from the index ring, or a plain draw of indexCount.
        bool indexed = false;
        bool indices32 = false;
        uint32_t indexCount = 0;
        uint32_t indexRingOffset = 0;   // bytes
        ID3D11Buffer* indexBuffer = nullptr;   // the draw's indices kept converted (from 0), else the ring
        int32_t baseVertex = 0;
        uint32_t triangles = 0;         // for the counters

        // The HDR mode (render_d3d11.cpp): the targets written that are 8 bit
        // ones made float, by slot; and the bloom's composite, after which
        // the frame is tone mapped before the HUD goes over it.
        uint32_t upgradedTargets = 0;
        bool hdrScene = false;
        // Verify mode: a layer over a surface (the decals), counted on the GPU
        // with an occlusion query and logged with the pool its vertices came from.
        bool layerQuery = false;
        uint32_t layerVertices = 0;

        // Housekeeping.
        bool bindingsLost = false;      // a cache dropped objects: forget what is bound first
        bool logged = false;            // the frame log wants the target after the draw

        // 0.5.391: the state objects for the handles, looked up on the
        // command thread (the tables grow there) for the drawing thread.
        ID3D11BlendState* blendObject = nullptr;
        ID3D11DepthStencilState* depthObject = nullptr;
        ID3D11RasterizerState* rasterizerObject = nullptr;
    };

    // A resolve, ready to run: a copy of a target into the surface the
    // title will sample, and the clears that come with it.
    struct ResolveCommand
    {
        bool copy = false;
        ID3D11ShaderResourceView* source = nullptr;
        uint32_t sourceWidth = 0, sourceHeight = 0, sourceSamples = 1;
        bool fromDepth = false;
        ID3D11RenderTargetView* destination = nullptr;
        ID3D11ShaderResourceView* destinationResource = nullptr;
        float x0 = 0, y0 = 0;           // the source rectangle, host pixels
        uint32_t width = 0, height = 0; // and its size, the destination's

        bool colorClear = false;
        ID3D11RenderTargetView* clearTarget = nullptr;
        float clearColor[4] = {};
        int32_t clearRect[4] = {};
        bool depthClear = false;
        ID3D11DepthStencilView* clearDepth = nullptr;
        float clearDepthValue = 1.0f;
        uint8_t clearStencilValue = 0;

        bool bindingsLost = false;

        // A depth copy also written into the title's memory, as the
        // console's resolve would (debug.txt depth_writeback): the tiled
        // surface at guestBase, guestPitch texels across.
        bool writeBack = false;
        uint32_t guestBase = 0, guestPitch = 0, guestWidth = 0, guestHeight = 0;
        uint32_t guestEndian = 0;
        bool guestFloatDepth = false;
    };
}
