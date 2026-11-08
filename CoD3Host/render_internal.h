#pragma once

// What the PC render layer (render_commands.cpp) needs from the executor
// (render_d3d11.cpp) besides the caches: the frame log's state.

#include <cstdint>

struct ID3D11Device;
struct ID3D11ShaderResourceView;

namespace RenderInternal
{
    bool FrameLogged();       // the frame log is on for this frame
    uint64_t Swaps();         // the title's swaps so far
    int DumpNumber();         // the frame log's dump counter
    uint32_t CpuDepthAt();    // the depth resolve the title reads on the CPU (Render::CpuReadsDepthAt), 0 for none

    // The button prompts as keys (render_prompts.cpp): the device to make
    // the picture with, and the picture in place of the texture at this
    // address, when they are asked for (Render::SetPromptKeys); else none.
    void SetPromptDevice(ID3D11Device* device);
    ID3D11ShaderResourceView* PromptTexture(uint32_t physical, uint32_t width, uint32_t height);
    // 0.5.653: the texture for draws reading this vertex buffer (Render::SetMeshTexture), else none
    ID3D11ShaderResourceView* MeshTexture(uint32_t vertexBuffer, int& slot);
    ID3D11ShaderResourceView* MeshSlotTexture(uint32_t vertexBuffer, int slot);   // 0.5.664
}
