#pragma once

// Texture packs: the game's textures saved out, and replacements loaded in
// their place (a PC addition, as Dolphin and Xenia have it).
//
// Each texture is known by its kind, size and a hash of its first level's
// texels, as the game uploads it: "<width>x<height>_f<format>_<hash>". With
// texture_dump on, every texture the game uploads is saved once as
// textures\dump\<key>.png beside the executable (the block compressed ones
// decoded). A file textures\replace\<key>.png (or .dds) is then used instead
// of the game's texture, at whatever size it is: a menu picture or a font
// four times as big is drawn four times as sharp, the game none the wiser
// (it samples by the texture's corners, not its texels).

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace TexturePack
{
    // Either is on (the folder looked at once, at the first texture).
    bool Active();

    // The texture's key, from its first level as the host has it (untiled,
    // the console's byte order undone: BC1..3 blocks, or RGBA8 / R8 texels).
    std::string Key(uint32_t format, uint32_t width, uint32_t height, const std::vector<uint8_t>& linear);

    // 0.5.197: with texture_dump on, the key listed once a map in
    // textures\maps\<map>.txt (which textures each map uses).
    void NoteMap(const std::string& key);

    // Saved once, when texture_dump is on.
    void Dump(const std::string& key, uint32_t format, uint32_t width, uint32_t height, const std::vector<uint8_t>& linear);

    // The replacement for this key, made into a texture, when there is one.
    // 0.5.234: textures\replace\maps\<map>\<key>.png wins while that map
    // is loaded; forMap then says which map (empty for any other).
    bool Load(const std::string& key, ID3D11Device* device, ID3D11DeviceContext* context,
              ID3D11Texture2D** texture, ID3D11ShaderResourceView** view, std::string* forMap = nullptr);

    // 0.5.234: the map a per-map replacement goes with, lower case, as Load
    // compares it ("" in the menus).
    // 0.5.653: a PNG file read as RGBA8 (the Webley's texture, render_prompts.cpp).
    bool ReadPngFile(const std::filesystem::path& file, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba);
    std::string CurrentMapKey();

    // 0.5.432: whether that map has its own replacement for this key
    // (textures\replace\maps\<map>\<key>.png).
    bool HasMapReplacement(const std::string& key, const std::string& map);
}
