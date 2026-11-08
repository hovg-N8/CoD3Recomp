#include "texture_pack.h"
#include "settings.h"
#include "kernel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <thread>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <set>
#include <unordered_map>

#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
    std::mutex g_lock;
    bool g_scanned = false;
    std::filesystem::path g_root;                                  // textures\ beside the executable
    std::unordered_map<std::string, std::filesystem::path> g_replacements;   // key -> file
    // 0.5.234: textures\replace\maps\<map>\<key>.png: used only while that
    // map is the one loaded (Kernel::CurrentMap, set when its archive
    // opens), before any other replacement of the key. The DLC maps share
    // one loading picture; a copy per map gives each its own.
    std::unordered_map<std::string, std::unordered_map<std::string, std::filesystem::path>> g_mapReplacements;   // map -> key -> file
    int g_loadingDumps = 0;
    std::set<std::string> g_dumped;
    std::atomic<int> g_dumps{0};
    int g_loads = 0;
    // 0.5.197: what the replacements in use take on the graphics card, for
    // knowing how big a pack a map can have (logged every 100 loads).
    uint64_t g_replacedBytes = 0;
    // 0.5.197: a world texture dump writes thousands of pictures; PNG
    // encoding each one on the drawing thread made the game stutter. They
    // are written by a thread of their own (up to 256 MB waiting, past that
    // the drawing thread writes the picture itself rather than holding more).
    struct DumpJob { std::filesystem::path file; std::string key; uint32_t format, width, height; std::vector<uint8_t> linear; };
    std::mutex g_jobLock;
    std::condition_variable g_jobReady;
    std::deque<DumpJob> g_jobs;
    size_t g_jobBytes = 0;
    bool g_workerStarted = false;
    constexpr size_t MaxQueuedBytes = 256u << 20;
    constexpr int MaxDumps = 30000;   // 0.5.197: was 3000, enough for the menus, not for maps

    std::filesystem::path Root()
    {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path() / L"textures";
    }

    // The replacement folder, looked at once: every .png and .dds in it (and
    // its folders, so a pack can keep its own order), by file name.
    void Scan()
    {
        if (g_scanned) return;
        g_scanned = true;
        g_root = Root();
        std::error_code ec;
        const std::filesystem::path folder = g_root / L"replace";
        std::filesystem::create_directories(folder, ec);
        for (auto it = std::filesystem::recursive_directory_iterator(folder, ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
        {
            if (!it->is_regular_file(ec)) continue;
            std::wstring ext = it->path().extension().wstring();
            for (wchar_t& c : ext) c = wchar_t(towlower(c));
            if (ext != L".png" && ext != L".dds") continue;
            const std::string key = it->path().stem().string();
            // 0.5.234: maps\<map>\<key> is that map's own (one folder deep).
            {
                std::error_code rel;
                const std::filesystem::path inside = std::filesystem::relative(it->path(), folder, rel);
                std::vector<std::string> parts;
                if (!rel) for (const auto& part : inside) parts.push_back(part.string());
                std::string first = parts.empty() ? std::string() : parts[0];
                for (char& c : first) c = char(tolower((unsigned char)c));
                if (parts.size() == 3 && first == "maps")
                {
                    std::string map = parts[1];
                    for (char& c : map) c = char(tolower((unsigned char)c));
                    auto& keys = g_mapReplacements[map];
                    auto had = keys.find(key);
                    if (had == keys.end() || ext == L".dds") keys[key] = it->path();
                    continue;
                }
            }
            // A .dds wins over a .png of the same key.
            auto found = g_replacements.find(key);
            if (found == g_replacements.end() || ext == L".dds") g_replacements[key] = it->path();
        }
        printf("textures: %zu replacements in %s\n", g_replacements.size(), folder.string().c_str());
        for (const auto& [map, keys] : g_mapReplacements)
            printf("textures: %zu for the map %s alone (replace\\maps\\%s)\n", keys.size(), map.c_str(), map.c_str());
        fflush(stdout);
    }

    void ComReady()
    {
        thread_local bool ready = false;
        if (!ready) { CoInitializeEx(nullptr, COINIT_MULTITHREADED); ready = true; }
    }

    uint64_t Fnv(const uint8_t* data, size_t size)
    {
        uint64_t hash = 0xCBF29CE484222325ull;
        // Eight bytes at a time, then the rest.
        size_t i = 0;
        for (; i + 8 <= size; i += 8)
        {
            uint64_t word;
            memcpy(&word, data + i, 8);
            hash = (hash ^ word) * 0x100000001B3ull;
            hash ^= hash >> 29;
        }
        for (; i < size; i++) hash = (hash ^ data[i]) * 0x100000001B3ull;
        return hash;
    }

    // --- block decoding, for the PNGs a dump writes ----------------------------------------------

    void Colour565(uint16_t c, uint8_t out[3])
    {
        out[0] = uint8_t(((c >> 11) & 31) * 255 / 31);
        out[1] = uint8_t(((c >> 5) & 63) * 255 / 63);
        out[2] = uint8_t((c & 31) * 255 / 31);
    }

    void ColourBlock(const uint8_t* block, uint8_t rgba[16][4], bool fourColour)
    {
        const uint16_t c0 = uint16_t(block[0] | (block[1] << 8)), c1 = uint16_t(block[2] | (block[3] << 8));
        uint8_t palette[4][4] = {};
        Colour565(c0, palette[0]); Colour565(c1, palette[1]);
        palette[0][3] = palette[1][3] = 255;
        if (fourColour || c0 > c1)
        {
            for (int k = 0; k < 3; k++)
            {
                palette[2][k] = uint8_t((2 * palette[0][k] + palette[1][k]) / 3);
                palette[3][k] = uint8_t((palette[0][k] + 2 * palette[1][k]) / 3);
            }
            palette[2][3] = palette[3][3] = 255;
        }
        else
        {
            for (int k = 0; k < 3; k++) palette[2][k] = uint8_t((palette[0][k] + palette[1][k]) / 2);
            palette[2][3] = 255;
            palette[3][0] = palette[3][1] = palette[3][2] = 0; palette[3][3] = 0;
        }
        const uint32_t bits = uint32_t(block[4]) | (uint32_t(block[5]) << 8) | (uint32_t(block[6]) << 16) | (uint32_t(block[7]) << 24);
        for (int i = 0; i < 16; i++) memcpy(rgba[i], palette[(bits >> (2 * i)) & 3], 4);
    }

    // BC1..3 to RGBA8, the size the texture says.
    std::vector<uint8_t> DecodeBlocks(uint32_t format, uint32_t width, uint32_t height, const std::vector<uint8_t>& blocks)
    {
        std::vector<uint8_t> out(size_t(width) * height * 4);
        const uint32_t blockBytes = format == 18 ? 8 : 16;
        const uint32_t across = (width + 3) / 4, down = (height + 3) / 4;
        for (uint32_t by = 0; by < down; by++)
            for (uint32_t bx = 0; bx < across; bx++)
            {
                const size_t at = (size_t(by) * across + bx) * blockBytes;
                if (at + blockBytes > blocks.size()) continue;
                const uint8_t* block = blocks.data() + at;
                uint8_t texels[16][4];
                if (format == 18) ColourBlock(block, texels, false);
                else
                {
                    ColourBlock(block + 8, texels, true);
                    if (format == 19)
                        for (int i = 0; i < 16; i++) { const uint8_t a = (block[i / 2] >> ((i & 1) * 4)) & 15; texels[i][3] = uint8_t(a * 17); }
                    else
                    {
                        uint8_t alpha[8] = { block[0], block[1] };
                        if (alpha[0] > alpha[1])
                            for (int k = 1; k < 7; k++) alpha[k + 1] = uint8_t(((7 - k) * alpha[0] + k * alpha[1]) / 7);
                        else
                        {
                            for (int k = 1; k < 5; k++) alpha[k + 1] = uint8_t(((5 - k) * alpha[0] + k * alpha[1]) / 5);
                            alpha[6] = 0; alpha[7] = 255;
                        }
                        uint64_t bits = 0;
                        for (int k = 0; k < 6; k++) bits |= uint64_t(block[2 + k]) << (8 * k);
                        for (int i = 0; i < 16; i++) texels[i][3] = alpha[(bits >> (3 * i)) & 7];
                    }
                }
                for (int i = 0; i < 16; i++)
                {
                    const uint32_t x = bx * 4 + i % 4, y = by * 4 + i / 4;
                    if (x < width && y < height) memcpy(out.data() + (size_t(y) * width + x) * 4, texels[i], 4);
                }
            }
        return out;
    }

    bool WritePng(const std::filesystem::path& file, uint32_t width, uint32_t height, const std::vector<uint8_t>& rgba)
    {
        ComReady();
        std::vector<uint8_t> bgra(rgba.size());
        for (size_t i = 0; i + 3 < rgba.size(); i += 4) { bgra[i] = rgba[i + 2]; bgra[i + 1] = rgba[i + 1]; bgra[i + 2] = rgba[i]; bgra[i + 3] = rgba[i + 3]; }
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IWICStream> stream;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) ||
            FAILED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) ||
            FAILED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) || FAILED(encoder->CreateNewFrame(&frame, nullptr)) ||
            FAILED(frame->Initialize(nullptr)) || FAILED(frame->SetSize(width, height)))
            return false;
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&format))) return false;
        return SUCCEEDED(frame->WritePixels(height, width * 4, UINT(bgra.size()), bgra.data())) && SUCCEEDED(frame->Commit()) &&
               SUCCEEDED(encoder->Commit());
    }

    bool ReadPng(const std::filesystem::path& file, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba)
    {
        ComReady();
        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame;
        ComPtr<IWICFormatConverter> converter;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
            FAILED(factory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) ||
            FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
            FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
            return false;
        UINT w = 0, h = 0;
        if (FAILED(converter->GetSize(&w, &h)) || w == 0 || h == 0 || w > 16384 || h > 16384) return false;
        width = w; height = h;
        rgba.resize(size_t(w) * h * 4);
        return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, UINT(rgba.size()), rgba.data()));
    }

    // A DDS: BC1..3 (DXT1, DXT3, DXT5), BC7 and 32-bit colour (a DX10
    // header's format, or the legacy masks), with the mips it carries.
    bool LoadDds(const std::filesystem::path& file, ID3D11Device* device, ID3D11Texture2D** texture, ID3D11ShaderResourceView** view)
    {
        FILE* in = _wfopen(file.c_str(), L"rb");
        if (!in) return false;
        std::vector<uint8_t> bytes;
        fseek(in, 0, SEEK_END);
        const long size = ftell(in);
        fseek(in, 0, SEEK_SET);
        if (size > 128) { bytes.resize(size_t(size)); if (fread(bytes.data(), 1, bytes.size(), in) != bytes.size()) bytes.clear(); }
        fclose(in);
        if (bytes.size() < 128 || memcmp(bytes.data(), "DDS ", 4) != 0) return false;
        auto word = [&](size_t at) { uint32_t v; memcpy(&v, bytes.data() + at, 4); return v; };
        const uint32_t height = word(12), width = word(16);
        uint32_t mips = std::max(word(28), 1u);
        const uint32_t pfFlags = word(80), fourCC = word(84), bits = word(88), rMask = word(92);
        size_t offset = 128;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        if (pfFlags & 0x4)
        {
            if (fourCC == 0x31545844) format = DXGI_FORMAT_BC1_UNORM;
            else if (fourCC == 0x33545844) format = DXGI_FORMAT_BC2_UNORM;
            else if (fourCC == 0x35545844) format = DXGI_FORMAT_BC3_UNORM;
            else if (fourCC == 0x30315844 && bytes.size() >= 148) { format = DXGI_FORMAT(word(128)); offset = 148; }
        }
        else if ((pfFlags & 0x40) && bits == 32)
            format = rMask == 0x000000FF ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM;
        const bool compressed = format == DXGI_FORMAT_BC1_UNORM || format == DXGI_FORMAT_BC2_UNORM || format == DXGI_FORMAT_BC3_UNORM ||
                                format == DXGI_FORMAT_BC7_UNORM || format == DXGI_FORMAT_BC1_UNORM_SRGB || format == DXGI_FORMAT_BC3_UNORM_SRGB ||
                                format == DXGI_FORMAT_BC7_UNORM_SRGB;
        const bool plain = format == DXGI_FORMAT_R8G8B8A8_UNORM || format == DXGI_FORMAT_B8G8R8A8_UNORM;
        if ((!compressed && !plain) || width == 0 || height == 0) return false;
        const uint32_t blockBytes = (format == DXGI_FORMAT_BC1_UNORM || format == DXGI_FORMAT_BC1_UNORM_SRGB) ? 8 : 16;
        std::vector<D3D11_SUBRESOURCE_DATA> levels;
        for (uint32_t level = 0; level < mips; level++)
        {
            const uint32_t w = std::max(width >> level, 1u), h = std::max(height >> level, 1u);
            const uint32_t row = compressed ? ((w + 3) / 4) * blockBytes : w * 4;
            const size_t levelBytes = size_t(row) * (compressed ? (h + 3) / 4 : h);
            if (offset + levelBytes > bytes.size()) { mips = level; break; }
            levels.push_back({ bytes.data() + offset, row, 0 });
            offset += levelBytes;
        }
        if (mips == 0) return false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width; desc.Height = height; desc.MipLevels = mips; desc.ArraySize = 1; desc.Format = format;
        desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&desc, levels.data(), texture))) return false;
        if (FAILED(device->CreateShaderResourceView(*texture, nullptr, view))) { (*texture)->Release(); *texture = nullptr; return false; }
        return true;
    }
}

namespace
{
    // A dump job: decoded to RGBA and written as a PNG, first under a
    // temporary name so a game closed mid-write leaves no half picture that
    // the next run would take as done.
    void WriteDump(DumpJob& job)
    {
        std::vector<uint8_t> rgba;
        const uint32_t format = job.format, width = job.width, height = job.height;
        const std::vector<uint8_t>& linear = job.linear;
        if (format == 18 || format == 19 || format == 20) rgba = DecodeBlocks(format, width, height, linear);
        else if (linear.size() >= size_t(width) * height * 4) rgba.assign(linear.begin(), linear.begin() + size_t(width) * height * 4);
        else if (linear.size() >= size_t(width) * height)
        {
            rgba.resize(size_t(width) * height * 4);
            for (size_t i = 0; i < size_t(width) * height; i++) { rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = linear[i]; rgba[i * 4 + 3] = 255; }
        }
        else return;
        std::filesystem::path temporary = job.file;
        temporary += L".part";
        std::error_code ec;
        if (!WritePng(temporary, width, height, rgba)) { std::filesystem::remove(temporary, ec); return; }
        std::filesystem::rename(temporary, job.file, ec);
        if (ec) { std::filesystem::remove(temporary, ec); return; }
        const int done = ++g_dumps;
        if (done <= 20 || done % 500 == 0) { printf("textures: dumped %s (%d so far)\n", job.key.c_str(), done); fflush(stdout); }
    }

    void DumpWorker()
    {
        for (;;)
        {
            DumpJob job;
            {
                std::unique_lock<std::mutex> lock(g_jobLock);
                g_jobReady.wait(lock, [] { return !g_jobs.empty(); });
                job = std::move(g_jobs.front());
                g_jobs.pop_front();
                g_jobBytes -= job.linear.size();
            }
            WriteDump(job);
        }
    }

    // 0.5.234: the map being loaded or played, lower case, as the
    // replace\maps folders name it ("" in the menus).
    std::string MapKey()
    {
        std::string map = Kernel::CurrentMap();
        for (char& c : map) c = char(tolower((unsigned char)c));
        return map;
    }

    // The map being played, as the dump lists it (textures\maps\<map>.txt).
    std::string MapForList()
    {
        std::string map = Kernel::CurrentMap();
        for (char& c : map) if (!isalnum((unsigned char)c) && c != '_' && c != '-') c = '_';
        return map.empty() ? std::string("menus") : map;
    }
}

bool TexturePack::Active()
{
    const Settings::Values values = Settings::Get();
    if (!values.textureDump && !values.textureReplace) return false;
    std::lock_guard<std::mutex> lock(g_lock);
    Scan();
    return values.textureDump || !g_replacements.empty() || !g_mapReplacements.empty();
}

std::string TexturePack::Key(uint32_t format, uint32_t width, uint32_t height, const std::vector<uint8_t>& linear)
{
    char key[64];
    snprintf(key, sizeof(key), "%ux%u_f%u_%016llX", width, height, format, (unsigned long long)Fnv(linear.data(), linear.size()));
    return key;
}

void TexturePack::Dump(const std::string& key, uint32_t format, uint32_t width, uint32_t height, const std::vector<uint8_t>& linear)
{
    // 0.5.234: a 1024x512 picture with no replacement is saved even with
    // texture_dump off (twelve a run at most): the DLC maps' loading picture
    // is one (1024x512_f18_05F0F9261D8E1859 first used as Marseille loaded,
    // 0.5.233's used.txt), and seeing it is how its replacement is made.
    bool loadingCandidate = false;
    if (!Settings::Get().textureDump)
    {
        if (width != 1024 || height != 512) return;
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_loadingDumps >= 12) return;
        loadingCandidate = true;
    }
    if (width < 8 || height < 8 || width > 4096 || height > 4096) return;
    // 0.5.87: a playing movie gives a new texture every frame (640x360 and
    // 1280x720, format 2), and writing each one out held up the drawing
    // thread long enough to stall the campaign's start-up movies. Those are
    // never dumped, and no more than 3000 textures are dumped in one run.
    if (format == 2 && ((width == 640 && height == 360) || (width == 1280 && height == 720))) return;
    if (g_dumps.load() >= MaxDumps) return;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Scan();
        if (!g_dumped.insert(key).second) return;
    }
    std::error_code ec;
    const std::filesystem::path folder = g_root / L"dump";
    std::filesystem::create_directories(folder, ec);
    const std::filesystem::path file = folder / (key + ".png");
    if (loadingCandidate)
    {
        {
            std::lock_guard<std::mutex> lock(g_lock);
            g_loadingDumps++;
        }
        printf("textures: a 1024x512 picture with no replacement, %s, while the map is \"%s\" (saved in textures\\dump: a loading picture?)\n",
               key.c_str(), Kernel::CurrentMap().c_str());
        fflush(stdout);
    }
    if (std::filesystem::exists(file, ec)) return;
    DumpJob job{ file, key, format, width, height, linear };
    {
        std::lock_guard<std::mutex> lock(g_jobLock);
        if (g_jobBytes + linear.size() <= MaxQueuedBytes)
        {
            if (!g_workerStarted) { g_workerStarted = true; std::thread(DumpWorker).detach(); }
            g_jobBytes += linear.size();
            g_jobs.push_back(std::move(job));
            g_jobReady.notify_one();
            return;
        }
    }
    WriteDump(job);   // the queue is full: written here, as before 0.5.197
}

void TexturePack::NoteMap(const std::string& key)
{
    // 0.5.197: with texture_dump on, every texture each map uses, once a
    // map, in textures\maps\<map>.txt, so UPSCALE-TEXTURES can do one map's
    // textures (its bushes, trees, walls, vehicles and the guns in hand).
    if (!Settings::Get().textureDump) return;
    unsigned w = 0, h = 0;
    if (sscanf(key.c_str(), "%ux%u", &w, &h) != 2 || w < 32 || h < 32) return;
    const std::string map = MapForList();
    static std::set<std::string> listed;
    std::lock_guard<std::mutex> lock(g_lock);
    Scan();
    if (!listed.insert(map + "/" + key).second) return;
    std::error_code ec;
    const std::filesystem::path folder = g_root / L"maps";
    std::filesystem::create_directories(folder, ec);
    if (FILE* f = _wfopen((folder / (map + ".txt")).c_str(), L"a")) { fprintf(f, "%s\n", key.c_str()); fclose(f); }
}

bool TexturePack::Load(const std::string& key, ID3D11Device* device, ID3D11DeviceContext* context,
                       ID3D11Texture2D** texture, ID3D11ShaderResourceView** view, std::string* forMap)
{
    if (forMap != nullptr) forMap->clear();
    if (!Settings::Get().textureReplace) return false;
    std::filesystem::path file;
    {
        std::lock_guard<std::mutex> lock(g_lock);
        Scan();
        // 0.5.234: the map's own copy first, then the one for every map.
        const std::filesystem::path* chosen = nullptr;
        std::string mapUsed;
        if (!g_mapReplacements.empty())
        {
            const std::string map = MapKey();
            const auto keys = g_mapReplacements.find(map);
            if (keys != g_mapReplacements.end())
            {
                const auto own = keys->second.find(key);
                if (own != keys->second.end()) { chosen = &own->second; mapUsed = map; }
            }
        }
        if (chosen == nullptr)
        {
            const auto found = g_replacements.find(key);
            if (found != g_replacements.end()) chosen = &found->second;
        }
        // 0.5.168: every texture of 128 or more across that the game
        // uploads, once, into textures\used.txt: when (seconds since the
        // start), its key, and whether a replacement was there. Visiting the
        // menus then shows which of their pictures (banners, backgrounds)
        // still have no upscaled copy (Justin: check the Create Lobby and
        // System Link screens).
        {
            static std::set<std::string> seen;
            static const ULONGLONG start = GetTickCount64();
            unsigned w = 0, h = 0;
            if (seen.insert(key).second && sscanf(key.c_str(), "%ux%u", &w, &h) == 2 && (w >= 128 || h >= 128))
            {
                static bool started = false;
                // 0.5.215: the campaign keeps its own list (used-campaign.txt),
                // so a multiplayer run no longer overwrites the campaign's
                // menus (Justin: upscale the campaign's menu pictures next).
#ifdef COD3_MP
                const wchar_t* const usedName = L"used.txt";
#else
                const wchar_t* const usedName = L"used-campaign.txt";
#endif
                if (FILE* f = _wfopen((g_root / usedName).c_str(), started ? L"a" : L"w"))
                {
                    started = true;
                    fprintf(f, "%6.1f %s %s%s%s\n", double(GetTickCount64() - start) / 1000.0, key.c_str(),
                            chosen == nullptr ? "NOT-REPLACED" : "replaced",
                            mapUsed.empty() ? "" : " for ", mapUsed.c_str());
                    fclose(f);
                }
            }
        }
        if (chosen == nullptr) return false;
        file = *chosen;
        if (!mapUsed.empty())
        {
            if (forMap != nullptr) *forMap = mapUsed;
            printf("textures: %s replaced from maps\\%s\\%s\n", key.c_str(), mapUsed.c_str(), file.filename().string().c_str());
            fflush(stdout);
        }
    }
    std::wstring ext = file.extension().wstring();
    for (wchar_t& c : ext) c = wchar_t(towlower(c));
    bool ok = false;
    if (ext == L".dds") ok = LoadDds(file, device, texture, view);
    else
    {
        // A PNG: RGBA8, its mips made by the GPU.
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> rgba;
        if (ReadPng(file, width, height, rgba))
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = width; desc.Height = height; desc.MipLevels = 0; desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1; desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
            if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, texture)))
            {
                context->UpdateSubresource(*texture, 0, nullptr, rgba.data(), width * 4, 0);
                if (SUCCEEDED(device->CreateShaderResourceView(*texture, nullptr, view)))
                {
                    context->GenerateMips(*view);
                    ok = true;
                }
                else { (*texture)->Release(); *texture = nullptr; }
            }
        }
    }
    if (ok)
    {
        // What it takes on the card: every level, block compressed or not.
        D3D11_TEXTURE2D_DESC desc{};
        (*texture)->GetDesc(&desc);
        const bool bc1 = desc.Format == DXGI_FORMAT_BC1_UNORM || desc.Format == DXGI_FORMAT_BC1_UNORM_SRGB;
        const bool blocks = bc1 || desc.Format == DXGI_FORMAT_BC2_UNORM || desc.Format == DXGI_FORMAT_BC3_UNORM ||
                            desc.Format == DXGI_FORMAT_BC3_UNORM_SRGB || desc.Format == DXGI_FORMAT_BC7_UNORM || desc.Format == DXGI_FORMAT_BC7_UNORM_SRGB;
        uint64_t bytes = 0;
        for (uint32_t level = 0; level < std::max(desc.MipLevels, 1u); level++)
        {
            const uint64_t w = std::max(desc.Width >> level, 1u), h = std::max(desc.Height >> level, 1u);
            bytes += blocks ? ((w + 3) / 4) * ((h + 3) / 4) * (bc1 ? 8 : 16) : w * h * 4;
        }
        std::lock_guard<std::mutex> lock(g_lock);
        g_replacedBytes += bytes;
        ++g_loads;
        if (g_loads <= 50) { printf("textures: %s replaced from %s\n", key.c_str(), file.filename().string().c_str()); fflush(stdout); }
        if (g_loads % 100 == 0) { printf("textures: %d replacements loaded so far, %.0f MB on the graphics card\n", g_loads, double(g_replacedBytes) / 1048576.0); fflush(stdout); }
    }
    if (!ok) { printf("textures: %s could not be read\n", file.string().c_str()); fflush(stdout); }
    return ok;
}

std::string TexturePack::CurrentMapKey() { return MapKey(); }
bool TexturePack::ReadPngFile(const std::filesystem::path& file, uint32_t& width, uint32_t& height, std::vector<uint8_t>& rgba) { return ReadPng(file, width, height, rgba); }

bool TexturePack::HasMapReplacement(const std::string& key, const std::string& map)
{
    if (key.empty() || map.empty() || !Settings::Get().textureReplace) return false;
    std::lock_guard<std::mutex> lock(g_lock);
    Scan();
    const auto keys = g_mapReplacements.find(map);
    return keys != g_mapReplacements.end() && keys->second.find(key) != keys->second.end();
}
