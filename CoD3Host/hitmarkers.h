#pragma once
#include <cstdint>
struct ID3D11Device;
namespace Settings { struct Values; }
namespace Hitmarkers {
void Initialize(ID3D11Device* device);
bool Active(const Settings::Values& settings);
void Draw(int width, int height, const Settings::Values& settings);
void Mix(float* samples, uint32_t frames, uint32_t channels, const Settings::Values& settings);
void Preview(bool kill);
}
