// Remastered films. See film_hd.h.
//
// The file (written by tools\remaster-films.py), little endian:
//
//   "C3FILM1\0"
//   u32 codec (0 HEVC, 1 H.264, 2 AV1), width, height
//   u32 rate numerator, denominator (its frames a second, 60000/1001)
//   u32 frames
//   u32 the original's frames, its rate's numerator and denominator (30000/1001)
//   u32 a thumbnail's width and height (32 by 18)
//   u32 the codec's extra data's size, and that data
//   the original's thumbnails, one a frame: the brightness, a byte a pixel
//   the frames' index: u64 where, u32 size, u32 1 for a key frame
//   the frames, each a packet of the codec, with no frames reordered
//
// In step: the title swaps once for each of its film's frames. Each swap's
// picture is made into a thumbnail on the GPU and read back at once;
// matched against the original's thumbnails it says which of the film's
// frames that swap showed. The frame shown at a swap is then the
// one at the same time in the remastered film, and the frames between at
// their times, so a film that stalls, skips or starts late stays in step.

#include "film_hd.h"
#include "render.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <Windows.h>
#include <wrl/client.h>

#ifdef COD3_HAVE_FFMPEG_HEADERS
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/pixfmt.h>
#include <libavutil/version.h>
}
#endif

using Microsoft::WRL::ComPtr;

namespace
{
#ifdef COD3_HAVE_FFMPEG_HEADERS
    struct Ffmpeg
    {
        decltype(&avcodec_find_decoder) find_decoder = nullptr;
        decltype(&avcodec_alloc_context3) alloc_context = nullptr;
        decltype(&avcodec_open2) open = nullptr;
        decltype(&avcodec_send_packet) send_packet = nullptr;
        decltype(&avcodec_receive_frame) receive_frame = nullptr;
        decltype(&avcodec_free_context) free_context = nullptr;
        decltype(&avcodec_flush_buffers) flush = nullptr;
        decltype(&av_packet_alloc) packet_alloc = nullptr;
        decltype(&av_packet_free) packet_free = nullptr;
        decltype(&av_frame_alloc) frame_alloc = nullptr;
        decltype(&av_frame_free) frame_free = nullptr;
        decltype(&av_frame_unref) frame_unref = nullptr;
        decltype(&av_mallocz) mallocz = nullptr;
        bool loaded = false;
    };

    // The same libraries as the XMA decoding's (xma.cpp), beside the
    // executable or on the path.
    const Ffmpeg& Library()
    {
        static Ffmpeg ffmpeg;
        static std::once_flag once;
        std::call_once(once, []() {
            auto load = [](const std::wstring& name) -> HMODULE {
                wchar_t exe[MAX_PATH];
                if (GetModuleFileNameW(nullptr, exe, MAX_PATH) != 0)
                {
                    std::wstring beside(exe);
                    beside.resize(beside.find_last_of(L"\\/") + 1);
                    if (HMODULE module = LoadLibraryW((beside + name).c_str())) return module;
                }
                return LoadLibraryW(name.c_str());
            };
            HMODULE util = load(L"avutil-" + std::to_wstring(LIBAVUTIL_VERSION_MAJOR) + L".dll");
            HMODULE codec = util ? load(L"avcodec-" + std::to_wstring(LIBAVCODEC_VERSION_MAJOR) + L".dll") : nullptr;
            if (util == nullptr || codec == nullptr) return;
            bool ok = true;
            auto get = [&](HMODULE module, const char* name, auto& slot) {
                slot = reinterpret_cast<std::remove_reference_t<decltype(slot)>>(GetProcAddress(module, name));
                if (slot == nullptr) ok = false;
            };
            get(codec, "avcodec_find_decoder", ffmpeg.find_decoder);
            get(codec, "avcodec_alloc_context3", ffmpeg.alloc_context);
            get(codec, "avcodec_open2", ffmpeg.open);
            get(codec, "avcodec_send_packet", ffmpeg.send_packet);
            get(codec, "avcodec_receive_frame", ffmpeg.receive_frame);
            get(codec, "avcodec_free_context", ffmpeg.free_context);
            get(codec, "avcodec_flush_buffers", ffmpeg.flush);
            get(codec, "av_packet_alloc", ffmpeg.packet_alloc);
            get(codec, "av_packet_free", ffmpeg.packet_free);
            get(util, "av_frame_alloc", ffmpeg.frame_alloc);
            get(util, "av_frame_free", ffmpeg.frame_free);
            get(util, "av_frame_unref", ffmpeg.frame_unref);
            get(util, "av_mallocz", ffmpeg.mallocz);
            ffmpeg.loaded = ok;
        });
        return ffmpeg;
    }
#endif

    struct Entry { uint64_t where; uint32_t size, key; };

    // A remastered film's file, its header, thumbnails and index read.
    struct File
    {
        std::filesystem::path path;
        HANDLE handle = INVALID_HANDLE_VALUE;
        uint32_t codec = 0, width = 0, height = 0, rateNum = 0, rateDen = 1, frames = 0;
        uint32_t sourceFrames = 0, sourceNum = 30000, sourceDen = 1001, thumbWidth = 0, thumbHeight = 0;
        std::vector<uint8_t> extra, thumbs;
        std::vector<Entry> index;

        bool Read(void* out, uint32_t size, uint64_t where) const
        {
            OVERLAPPED at{};
            at.Offset = uint32_t(where);
            at.OffsetHigh = uint32_t(where >> 32);
            DWORD read = 0;
            return ReadFile(handle, out, size, &read, &at) && read == size;
        }
        ~File() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
    };

    std::unique_ptr<File> OpenFile(const std::filesystem::path& path)
    {
        auto file = std::make_unique<File>();
        file->path = path;
        file->handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file->handle == INVALID_HANDLE_VALUE) return nullptr;
        char magic[8];
        uint32_t h[12];
        uint64_t at = 0;
        if (!file->Read(magic, 8, at) || memcmp(magic, "C3FILM1", 8) != 0 || !file->Read(h, sizeof(h), at + 8)) return nullptr;
        at += 8 + sizeof(h);
        file->codec = h[0]; file->width = h[1]; file->height = h[2]; file->rateNum = h[3]; file->rateDen = h[4]; file->frames = h[5];
        file->sourceFrames = h[6]; file->sourceNum = h[7]; file->sourceDen = h[8]; file->thumbWidth = h[9]; file->thumbHeight = h[10];
        const uint32_t extraSize = h[11];
        if (file->width == 0 || file->height == 0 || file->width > 8192 || file->height > 8192 || file->frames == 0 || file->rateNum == 0 ||
            file->rateDen == 0 || file->sourceNum == 0 || file->sourceDen == 0 || extraSize > (1u << 20) ||
            file->thumbWidth * file->thumbHeight == 0 || file->thumbWidth * file->thumbHeight > 4096) return nullptr;
        file->extra.resize(extraSize);
        if (extraSize != 0 && !file->Read(file->extra.data(), extraSize, at)) return nullptr;
        at += extraSize;
        file->thumbs.resize(size_t(file->sourceFrames) * file->thumbWidth * file->thumbHeight);
        if (!file->thumbs.empty() && !file->Read(file->thumbs.data(), uint32_t(file->thumbs.size()), at)) return nullptr;
        at += file->thumbs.size();
        file->index.resize(file->frames);
        if (!file->Read(file->index.data(), uint32_t(file->index.size() * sizeof(Entry)), at)) return nullptr;
        return file;
    }

    // A frame decoded: its three planes as they came, Y at full size and
    // U and V at half, a byte or two a sample.
    struct Picture
    {
        int index = -1;
        uint32_t width = 0, height = 0, bytes = 1;
        std::vector<uint8_t> planes[3];
        uint32_t pitch[3] = {};
    };

    // The decoder, on a thread of its own: the frames from `wanted` on,
    // a second or so of them ahead, kept for the render thread.
    class Decoder
    {
    public:
        explicit Decoder(std::unique_ptr<File> file) : file_(std::move(file)) { thread_ = std::thread([this]() { Run(); }); }
        ~Decoder()
        {
            { std::lock_guard<std::mutex> lock(mutex_); stop_ = true; }
            wake_.notify_all();
            thread_.join();
        }
        const File& file() const { return *file_; }

        void Want(int index)
        {
            { std::lock_guard<std::mutex> lock(mutex_); wanted_ = std::clamp(index, 0, int(file_->frames) - 1); }
            wake_.notify_all();
        }

        // The frame, if it is decoded; `out` keeps its own copy.
        bool Take(int index, Picture& out)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (Picture& p : ready_)
                if (p.index == index) { out = p; return true; }
            return false;
        }

    private:
        static constexpr size_t Ahead = 48;

        void Run()
        {
#ifdef COD3_HAVE_FFMPEG_HEADERS
            const Ffmpeg& f = Library();
            if (!f.loaded) { printf("film: no FFmpeg for the remastered films\n"); return; }
            const AVCodecID id = file_->codec == 1 ? AV_CODEC_ID_H264 : file_->codec == 2 ? AV_CODEC_ID_AV1 : AV_CODEC_ID_HEVC;
            const AVCodec* codec = f.find_decoder(id);
            AVCodecContext* context = codec ? f.alloc_context(codec) : nullptr;
            if (context == nullptr) { printf("film: no decoder for %s\n", file_->path.string().c_str()); return; }
            context->thread_count = 0;
            context->thread_type = FF_THREAD_FRAME | FF_THREAD_SLICE;
            if (!file_->extra.empty())
            {
                context->extradata = static_cast<uint8_t*>(f.mallocz(file_->extra.size() + AV_INPUT_BUFFER_PADDING_SIZE));
                memcpy(context->extradata, file_->extra.data(), file_->extra.size());
                context->extradata_size = int(file_->extra.size());
            }
            if (f.open(context, codec, nullptr) < 0) { printf("film: the decoder did not open\n"); f.free_context(&context); return; }
            AVPacket* packet = f.packet_alloc();
            AVFrame* frame = f.frame_alloc();
            std::vector<uint8_t> data;
            int next = 0, out = 0;           // the next packet to send, the index of the next frame out
            bool drained = false;
            for (;;)
            {
                int wanted;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    wake_.wait(lock, [&]() {
                        if (stop_) return true;
                        const bool behind = wanted_ < out - int(ready_.size()) || wanted_ > out + int(Ahead) * 2;
                        const int last = ready_.empty() ? out - 1 : ready_.back().index;
                        return behind || (last < wanted_ + int(Ahead) && !drained);
                    });
                    if (stop_) break;
                    wanted = wanted_;
                    // Far from what is decoded (a seek, a film started
                    // again): from the key frame at or before it.
                    const int first = ready_.empty() ? out : ready_.front().index;
                    if (wanted < first || wanted > out + int(Ahead) * 2)
                    {
                        int key = wanted;
                        while (key > 0 && file_->index[key].key == 0) key--;
                        ready_.clear();
                        f.flush(context);
                        next = out = key;
                        drained = false;
                    }
                    while (!ready_.empty() && ready_.front().index < wanted - 4) ready_.pop_front();
                }
                // One packet in, what comes out kept.
                if (next < int(file_->frames))
                {
                    const Entry& e = file_->index[next];
                    data.assign(e.size + AV_INPUT_BUFFER_PADDING_SIZE, 0);
                    if (!file_->Read(data.data(), e.size, e.where)) { printf("film: a read failed at frame %d\n", next); break; }
                    packet->data = data.data();
                    packet->size = int(e.size);
                    f.send_packet(context, packet);
                    next++;
                }
                else if (!drained)
                {
                    f.send_packet(context, nullptr);
                    drained = true;
                }
                while (f.receive_frame(context, frame) == 0)
                {
                    Picture p;
                    p.index = out++;
                    p.width = uint32_t(frame->width);
                    p.height = uint32_t(frame->height);
                    p.bytes = frame->format == AV_PIX_FMT_YUV420P10LE ? 2 : 1;
                    if (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUVJ420P || frame->format == AV_PIX_FMT_YUV420P10LE)
                        for (int c = 0; c < 3; c++)
                        {
                            const uint32_t w = (c == 0 ? p.width : (p.width + 1) / 2) * p.bytes, h = c == 0 ? p.height : (p.height + 1) / 2;
                            p.pitch[c] = w;
                            p.planes[c].resize(size_t(w) * h);
                            for (uint32_t y = 0; y < h; y++)
                                memcpy(p.planes[c].data() + size_t(y) * w, frame->data[c] + size_t(y) * frame->linesize[c], w);
                        }
                    else
                    {
                        static bool told = false;
                        if (!told) { told = true; printf("film: frames of pixel format %d are not shown\n", frame->format); }
                    }
                    f.frame_unref(frame);
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (p.index >= wanted_ - 4) ready_.push_back(std::move(p));
                }
            }
            f.frame_free(&frame);
            f.packet_free(&packet);
            f.free_context(&context);
#endif
        }

        std::unique_ptr<File> file_;
        std::thread thread_;
        std::mutex mutex_;
        std::condition_variable wake_;
        bool stop_ = false;
        int wanted_ = 0;
        std::deque<Picture> ready_;
    };

    const char* const Source =
        "Texture2D picture : register(t0); SamplerState smp : register(s0);\n"
        "Texture2D<float> planeY : register(t1); Texture2D<float> planeU : register(t2); Texture2D<float> planeV : register(t3);\n"
        "Texture1D gammaTable : register(t4); Texture2D game : register(t5); Texture2D<float> overlay : register(t6);\n"
        // values: the thumbnail's mip, the planes' scale to 0..1, the colour table on, the overlay known;
        // rect: where the film is in the frame (uv corner and size: the 16:9 middle when filling the screen).
        "cbuffer Film : register(b0) { float4 values; float4 rect; };\n"
        "struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
        "V vsmain(uint id : SV_VertexID) { V v; float2 t = float2((id << 1) & 2, id & 2);"
        " v.position = float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); v.uv = t; return v; }\n"
        // The thumbnail: the brightness, from a mip near its size.
        "float4 thumb(V v) : SV_Target { float3 c = picture.SampleLevel(smp, rect.xy + v.uv * rect.zw, values.x).rgb;"
        " return dot(c, float3(0.299, 0.587, 0.114)).xxxx; }\n"
        // A frame: the video's BT.709 at its limited range, to RGB, and
        // through the display's colour table, as the title's picture is.
        "float3 gamma(float3 c) { float3 at = saturate(c) * (255.0 / 256.0) + (0.5 / 256.0);"
        " return float3(gammaTable.SampleLevel(smp, at.r, 0).r, gammaTable.SampleLevel(smp, at.g, 0).g, gammaTable.SampleLevel(smp, at.b, 0).b); }\n"
        "float3 frameAt(float2 uv) {\n"
        "    float y = (planeY.SampleLevel(smp, uv, 0) * values.y - 16.0 / 255.0) * (255.0 / 219.0);\n"
        "    float u = (planeU.SampleLevel(smp, uv, 0) * values.y - 128.0 / 255.0) * (255.0 / 224.0);\n"
        "    float w = (planeV.SampleLevel(smp, uv, 0) * values.y - 128.0 / 255.0) * (255.0 / 224.0);\n"
        "    float3 c = float3(y + 1.5748 * w, y - 0.1873 * u - 0.4681 * w, y + 1.8556 * u);\n"
        "    return values.z > 0.5 ? gamma(c) : saturate(c); }\n"
        // What the title drew over its film (a level's date and place, a
        // subtitle): light text, so only where the title's picture is
        // plainly lighter than the remastered frame of the same moment and
        // light itself. Both are compared blurred over about three of the
        // film's own pixels, so the remaster's finer detail, its sharper
        // edges and a frame's small differences of colour or motion do not
        // count: compared pixel by pixel, those let patches of the title's
        // frame through all over the picture.
        "float4 overlayMask(V v) : SV_Target { uint w, h; game.GetDimensions(w, h); float2 px = 1.5 * max(1.0, float(w) * rect.z / 1280.0) / float2(w, h);\n"
        "    float2 f = (v.uv - rect.xy) / rect.zw; if (any(f < 0.0) || any(f > 1.0)) return 0.0;\n"
        "    float3 g = 0.0, r = 0.0; float gHere = dot(game.SampleLevel(smp, v.uv, 0).rgb, float3(0.299, 0.587, 0.114));\n"
        "    [unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) { float2 o = float2(x, y) * px;"
        " g += game.SampleLevel(smp, v.uv + o, 0).rgb; r += frameAt(f + o / rect.zw); }\n"
        "    float lighter = dot(g - r, float3(0.299, 0.587, 0.114)) / 9.0;\n"
        "    return (smoothstep(0.15, 0.3, lighter) * smoothstep(0.45, 0.65, gHere)).xxxx; }\n"
        // The remastered frame, with what the title drew over its film kept
        // (the mask widened by two pixels, so its edges are kept whole).
        "float4 convert(V v) : SV_Target { float3 c = frameAt(v.uv); float2 guv = rect.xy + v.uv * rect.zw;\n"
        "    if (values.w > 0.5) { uint w, h; game.GetDimensions(w, h); float2 px = 2.0 / float2(w, h); float m = overlay.SampleLevel(smp, guv, 0);\n"
        "        m = max(m, max(overlay.SampleLevel(smp, guv + float2(px.x, 0), 0), overlay.SampleLevel(smp, guv - float2(px.x, 0), 0)));\n"
        "        m = max(m, max(overlay.SampleLevel(smp, guv + float2(0, px.y), 0), overlay.SampleLevel(smp, guv - float2(0, px.y), 0)));\n"
        "        c = lerp(c, game.SampleLevel(smp, guv, 0).rgb, m); }\n"
        "    return float4(c, 1.0); }\n";

    struct State
    {
        ID3D11Device* device = nullptr;
        bool failed = false;
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> thumb, convert, overlayMask;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11RasterizerState> rasterizer;
        ComPtr<ID3D11Buffer> constants;
        ComPtr<ID3D11Texture2D> thumbTexture, staging[6];
        ComPtr<ID3D11RenderTargetView> thumbTarget;
        int64_t stagingSwap[6] = { -1, -1, -1, -1, -1, -1 };
        ComPtr<ID3D11Texture2D> planes[3];
        ComPtr<ID3D11ShaderResourceView> planeViews[3];
        uint32_t planeWidth = 0, planeHeight = 0, planeBytes = 0;
        int uploaded = -1;
        // The title's picture at the last swap, and where it drew over its film.
        ComPtr<ID3D11Texture2D> game, overlay;
        ComPtr<ID3D11ShaderResourceView> gameView, overlayView;
        ComPtr<ID3D11RenderTargetView> overlayTarget;
        uint32_t gameWidth = 0, gameHeight = 0;
        bool overlayKnown = false;

        std::unique_ptr<Decoder> decoder;
        std::filesystem::path playing;   // the file the decoder has
        int64_t swaps = 0;               // the swaps of this film
        bool inStep = false;
        int64_t offset = 0;              // the original's frame at a swap: swap + offset
        int64_t moves = 0;               // the times a swap's frame was not the one expected
        bool shown = false;
        int agreed = 0;                  // this swap's frame was found (or was flat after one that was)
        bool active = false;
    };
    State g;

    std::mutex g_openedMutex;
    std::filesystem::path g_opened;      // the remastered file of the film the title opened last

    bool Programs(ID3D11Device* device, RenderPost::CompileFunction compile)
    {
        if (g.device != device) { g = State(); g.device = device; }
        if (g.convert) return true;
        if (g.failed || compile == nullptr) return false;
        g.failed = true;
        auto build = [&](const char* entry, const char* profile) -> ComPtr<ID3DBlob> {
            ComPtr<ID3DBlob> code, errors;
            if (FAILED(compile(Source, strlen(Source), "film program", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors)))
            {
                printf("film: the program (%s) did not compile: %s\n", entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
                return nullptr;
            }
            return code;
        };
        ComPtr<ID3DBlob> vs = build("vsmain", "vs_5_0"), thumb = build("thumb", "ps_5_0"), convert = build("convert", "ps_5_0"),
                         overlayMask = build("overlayMask", "ps_5_0");
        if (!vs || !thumb || !convert || !overlayMask) return false;
        if (FAILED(device->CreatePixelShader(overlayMask->GetBufferPointer(), overlayMask->GetBufferSize(), nullptr, &g.overlayMask))) return false;
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth = 32;
        buffer.Usage = D3D11_USAGE_DEFAULT;
        buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g.vertexShader)) ||
            FAILED(device->CreatePixelShader(thumb->GetBufferPointer(), thumb->GetBufferSize(), nullptr, &g.thumb)) ||
            FAILED(device->CreatePixelShader(convert->GetBufferPointer(), convert->GetBufferSize(), nullptr, &g.convert)) ||
            FAILED(device->CreateSamplerState(&sampler, &g.sampler)) || FAILED(device->CreateRasterizerState(&rasterizer, &g.rasterizer)) ||
            FAILED(device->CreateBuffer(&buffer, nullptr, &g.constants))) { g.convert.Reset(); return false; }
        g.failed = false;
        return true;
    }

    // Where the title draws its film in the frame, as uv: all of it, or
    // when filling a screen of another shape the 16:9 middle
    // (render_commands.cpp squeezes a film's draw into it).
    void FilmRect(float rect[4])
    {
        const float a = Render::FillAspect(), wide = 16.0f / 9.0f;
        rect[0] = 0.0f; rect[1] = 0.0f; rect[2] = 1.0f; rect[3] = 1.0f;
        if (a > wide) { rect[2] = wide / a; rect[0] = (1.0f - rect[2]) * 0.5f; }
        else if (a > 0.0f && a < wide) { rect[3] = a / wide; rect[1] = (1.0f - rect[3]) * 0.5f; }
    }

    void Pass(ID3D11DeviceContext* context, ID3D11RenderTargetView* target, uint32_t width, uint32_t height, ID3D11PixelShader* program,
              ID3D11ShaderResourceView* const* sources, UINT count, const float values4[4], bool inRect = false)
    {
        float values[8] = { values4[0], values4[1], values4[2], values4[3] };
        FilmRect(values + 4);
        context->UpdateSubresource(g.constants.Get(), 0, nullptr, values, 0, 0);
        ID3D11ShaderResourceView* none[8] = {};
        context->PSSetShaderResources(0, 8, none);
        context->VSSetShaderResources(0, 8, none);
        context->OMSetRenderTargets(1, &target, nullptr);
        // The film's own frame drawn into its rectangle of the target only.
        const D3D11_VIEWPORT viewport = inRect ? D3D11_VIEWPORT{ values[4] * width, values[5] * height, values[6] * width, values[7] * height, 0.0f, 1.0f }
                                               : D3D11_VIEWPORT{ 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
        context->RSSetViewports(1, &viewport);
        context->RSSetState(g.rasterizer.Get());
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(nullptr, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(nullptr);
        context->VSSetShader(g.vertexShader.Get(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(program, nullptr, 0);
        ID3D11Buffer* buffers[1] = { g.constants.Get() };
        context->PSSetConstantBuffers(0, 1, buffers);
        ID3D11SamplerState* samplers[1] = { g.sampler.Get() };
        context->PSSetSamplers(0, 1, samplers);
        context->PSSetShaderResources(0, count, sources);
        context->Draw(3, 0);
        context->PSSetShaderResources(0, 8, none);
        ID3D11RenderTargetView* noTarget = nullptr;
        context->OMSetRenderTargets(1, &noTarget, nullptr);
    }

    // How alike two thumbnails are: their correlation, each less its own
    // mean and over its own spread, so a curve on the brightness (the
    // display's colour table) does not matter. Flat ones say nothing.
    float Likeness(const uint8_t* a, const float* b, float bSpread, size_t n)
    {
        float mean = 0.0f;
        for (size_t i = 0; i < n; i++) mean += a[i];
        mean /= float(n);
        float dot = 0.0f, spread = 0.0f;
        for (size_t i = 0; i < n; i++) { const float d = a[i] - mean; dot += d * b[i]; spread += d * d; }
        return spread > 0.0f && bSpread > 0.0f ? dot / std::sqrt(spread * bSpread) : 0.0f;
    }

    // A thumbnail read back from the swap it was taken at: the original's
    // frame it is found to be, and so the offset from the swaps.
    void Match(const uint8_t* thumb, int64_t swap)
    {
        const File& file = g.decoder->file();
        const size_t n = size_t(file.thumbWidth) * file.thumbHeight;
        std::vector<float> b(n);
        float mean = 0.0f;
        for (size_t i = 0; i < n; i++) mean += thumb[i];
        mean /= float(n);
        float spread = 0.0f;
        for (size_t i = 0; i < n; i++) { b[i] = thumb[i] - mean; spread += b[i] * b[i]; }
        if (spread / float(n) < 36.0f) return;   // flat (black, a fade): nothing to go by
        const int64_t predicted = swap + g.offset;
        const int64_t from = g.inStep ? std::max<int64_t>(0, predicted - 8) : 0;
        const int64_t to = g.inStep ? std::min<int64_t>(file.sourceFrames - 1, predicted + 8) : int64_t(file.sourceFrames) - 1;
        float best = -1.0f, atPredicted = -1.0f, otherBest = -1.0f;
        int64_t bestAt = -1;
        std::vector<float> score(size_t(std::max<int64_t>(0, to - from + 1)));
        for (int64_t j = from; j <= to; j++)
        {
            const float s = Likeness(file.thumbs.data() + size_t(j) * n, b.data(), spread, n);
            score[size_t(j - from)] = s;
            if (s > best) { best = s; bestAt = j; }
            if (j == predicted) atPredicted = s;
        }
        // In step but nothing near the frame expected is like it: the title
        // skipped further (a level loading behind its film), so the whole
        // film is looked through, as before it was in step.
        if (g.inStep && best < 0.85f && (to - from + 1) < int64_t(file.sourceFrames))
        {
            g.inStep = false;
            Match(thumb, swap);
            if (g.inStep) return;
            g.inStep = true;
            // Nothing stands out anywhere: a dim or soft frame; the count
            // has been right, so the frame expected is kept.
            g.agreed = best > 0.6f ? 1 : 0;
            return;
        }
        if (bestAt < 0 || best < 0.85f) { g.agreed = 0; return; }   // not a frame of this film: the title's own shown
        // Not in step yet: only a match that stands out (not one of many
        // frames of a still shot) puts it in step.
        for (int64_t j = from; j <= to; j++)
            if (std::llabs(j - bestAt) > 3) otherBest = std::max(otherBest, score[size_t(j - from)]);
        if (!g.inStep)
        {
            // COD3_FILMDEBUG=1: why a film is not put in step.
            static const bool debug = getenv("COD3_FILMDEBUG") != nullptr;
            if (debug && swap % 30 == 0) { printf("film: swap %lld best %.3f at %lld, others %.3f\n", (long long)swap, best, (long long)bestAt, otherBest); fflush(stdout); }
            if (best - otherBest < 0.02f) return;
            g.offset = bestAt - swap;
            g.inStep = true;
            g.agreed = 1;
            printf("film: the remastered film in step at swap %lld, its frame %lld (likeness %.3f)\n", (long long)swap, (long long)bestAt, best);
            fflush(stdout);
            return;
        }
        // In step: the frame expected unless another is clearly more like
        // it (a still shot's frames are all much alike); the title skips
        // and repeats frames of its film while a level loads behind it.
        g.agreed = 1;
        if (bestAt == predicted || best - atPredicted < 0.02f) return;
        g.offset = bestAt - swap;
        g.moves++;
    }

    // The remastered film's frame at a time given in the original's frames.
    int FrameAt(double sourceFrame)
    {
        const File& file = g.decoder->file();
        const double seconds = sourceFrame * file.sourceDen / file.sourceNum;
        return int(std::lround(seconds * file.rateNum / file.rateDen));
    }

    void Stop()
    {
        g.decoder.reset();
        g.playing.clear();
        g.swaps = 0;
        g.inStep = false;
        g.offset = 0;
        g.agreed = 0;
        g.active = false;
        g.uploaded = -1;
        for (int64_t& s : g.stagingSwap) s = -1;
    }
}

void FilmHd::Opened(const std::filesystem::path& hostPath)
{
    std::string extension = hostPath.extension().string();
    for (char& c : extension) c = char(tolower(uint8_t(c)));
    if (extension != ".wmv") return;
    std::filesystem::path remastered = hostPath;
    remastered.replace_extension(".c3film");
    std::error_code error;
    const bool there = std::filesystem::exists(remastered, error);
    std::lock_guard<std::mutex> lock(g_openedMutex);
    g_opened = there ? remastered : std::filesystem::path();
    if (there) { printf("film: %s has remastered frames\n", hostPath.filename().string().c_str()); fflush(stdout); }
}

bool FilmHd::Active()
{
    return g.active;
}

bool FilmHd::Swap(ID3D11Device* device, ID3D11DeviceContext* context, RenderPost::CompileFunction compile,
                  ID3D11ShaderResourceView* picture, uint32_t width, uint32_t height, bool playing)
{
    g.active = false;
    std::filesystem::path opened;
    { std::lock_guard<std::mutex> lock(g_openedMutex); opened = g_opened; }
    static const bool off = getenv("COD3_NOREMASTERED") != nullptr;
    if (!playing || opened.empty() || off || picture == nullptr) { if (g.decoder) Stop(); return false; }
    if (!Programs(device, compile)) return false;
    if (g.playing != opened)
    {
        Stop();
        std::unique_ptr<File> file = OpenFile(opened);
        g.playing = opened;
        if (!file) { printf("film: %s could not be read\n", opened.string().c_str()); fflush(stdout); return false; }
        printf("film: %s, %ux%u at %.2f a second, %u frames\n", opened.filename().string().c_str(), file->width, file->height,
               double(file->rateNum) / file->rateDen, file->frames);
        fflush(stdout);
        g.decoder = std::make_unique<Decoder>(std::move(file));
    }
    if (!g.decoder) return false;
    const File& file = g.decoder->file();

    // This swap's thumbnail, and the ones read back from before.
    if (!g.thumbTarget)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = file.thumbWidth;
        desc.Height = file.thumbHeight;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.thumbTexture)) ||
            FAILED(device->CreateRenderTargetView(g.thumbTexture.Get(), nullptr, &g.thumbTarget))) return false;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        for (auto& s : g.staging) if (FAILED(device->CreateTexture2D(&desc, nullptr, &s))) { g.thumbTarget.Reset(); return false; }
    }
    const int64_t swap = g.swaps++;
    {
        const float values[4] = { std::max(0.0f, std::log2(float(width) / file.thumbWidth) - 0.5f), 1.0f, 0.0f, 0.0f };
        ID3D11ShaderResourceView* sources[1] = { picture };
        context->GenerateMips(picture);
        Pass(context, g.thumbTarget.Get(), file.thumbWidth, file.thumbHeight, g.thumb.Get(), sources, 1, values);
        // Read back at once (a film's frame is a few draws; the wait is
        // short): this swap's own frame of the film, found exactly.
        context->CopyResource(g.staging[0].Get(), g.thumbTexture.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(g.staging[0].Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        {
            std::vector<uint8_t> thumb(size_t(file.thumbWidth) * file.thumbHeight);
            for (uint32_t y = 0; y < file.thumbHeight; y++)
                memcpy(thumb.data() + size_t(y) * file.thumbWidth, static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch, file.thumbWidth);
            context->Unmap(g.staging[0].Get(), 0);
            Match(thumb.data(), swap);
        }
    }
    // Shown only for a swap whose frame was found (or is flat, black or a
    // fade, after one that was): what is not a frame of the film (a menu,
    // a frame it could not tell) is the title's own.
    const bool shown = g.inStep && g.agreed > 0;
    if (shown != g.shown)
    {
        g.shown = shown;
        printf("film: the remastered frames %s at swap %lld (%lld frames found elsewhere than expected)\n", shown ? "shown" : "left out",
               (long long)swap, (long long)g.moves);
        fflush(stdout);
    }
    if (!shown) return false;
    const int64_t source = swap + g.offset;
    if (source < 0 || source >= int64_t(file.sourceFrames) + 2) return false;
    g.decoder->Want(FrameAt(double(source - 1)));
    // The title's picture kept, for what it drew over the film.
    ComPtr<ID3D11Resource> resource;
    picture->GetResource(&resource);
    if (!g.game || g.gameWidth != width || g.gameHeight != height)
    {
        g.game.Reset(); g.gameView.Reset(); g.overlay.Reset(); g.overlayView.Reset(); g.overlayTarget.Reset();
        ComPtr<ID3D11Texture2D> texture;
        resource.As(&texture);
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        desc.MipLevels = 1;
        desc.MiscFlags = 0;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_TEXTURE2D_DESC mask = desc;
        mask.Format = DXGI_FORMAT_R8_UNORM;
        mask.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.game)) || FAILED(device->CreateShaderResourceView(g.game.Get(), nullptr, &g.gameView)) ||
            FAILED(device->CreateTexture2D(&mask, nullptr, &g.overlay)) || FAILED(device->CreateShaderResourceView(g.overlay.Get(), nullptr, &g.overlayView)) ||
            FAILED(device->CreateRenderTargetView(g.overlay.Get(), nullptr, &g.overlayTarget)))
        {
            g.game.Reset(); g.gameView.Reset(); g.overlayTarget.Reset();
        }
        g.gameWidth = width;
        g.gameHeight = height;
    }
    if (g.game) context->CopySubresourceRegion(g.game.Get(), 0, 0, 0, 0, resource.Get(), 0, nullptr);
    g.overlayKnown = false;
    g.active = true;
    return true;
}

namespace
{
    // The remastered frame into the planes' textures, if it is decoded.
    bool Upload(ID3D11DeviceContext* context, int index)
    {
        if (index != g.uploaded)
        {
            Picture p;
            if (!g.decoder->Take(index, p) || p.planes[0].empty()) return false;
            if (!g.planes[0] || g.planeWidth != p.width || g.planeHeight != p.height || g.planeBytes != p.bytes)
            {
                ID3D11Device* device = nullptr;
                context->GetDevice(&device);
                if (device == nullptr) return false;
                bool made = true;
                for (int c = 0; c < 3; c++)
                {
                    D3D11_TEXTURE2D_DESC desc{};
                    desc.Width = c == 0 ? p.width : (p.width + 1) / 2;
                    desc.Height = c == 0 ? p.height : (p.height + 1) / 2;
                    desc.MipLevels = desc.ArraySize = 1;
                    desc.Format = p.bytes == 2 ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
                    desc.SampleDesc.Count = 1;
                    desc.Usage = D3D11_USAGE_DEFAULT;
                    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                    g.planes[c].Reset(); g.planeViews[c].Reset();
                    made = made && SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &g.planes[c])) &&
                           SUCCEEDED(device->CreateShaderResourceView(g.planes[c].Get(), nullptr, &g.planeViews[c]));
                }
                device->Release();
                if (!made) { g.planes[0].Reset(); return false; }
                g.planeWidth = p.width;
                g.planeHeight = p.height;
                g.planeBytes = p.bytes;
            }
            for (int c = 0; c < 3; c++) context->UpdateSubresource(g.planes[c].Get(), 0, nullptr, p.planes[c].data(), p.pitch[c], 0);
            g.uploaded = index;
        }
        return true;
    }
}

bool FilmHd::Draw(ID3D11DeviceContext* context, float phase, ID3D11RenderTargetView* target, uint32_t width, uint32_t height,
                  ID3D11ShaderResourceView* gammaTable)
{
    if (!g.active || !g.decoder || !g.convert) return false;
    // At the last swap, where the title drew over its film, from its
    // picture against this swap's own remastered frame.
    static const bool noOverlay = getenv("COD3_NOFILMOVERLAY") != nullptr;
    if (!noOverlay && !g.overlayKnown && g.gameView && g.overlayTarget && Upload(context, FrameAt(double(g.swaps - 1 + g.offset))))
    {
        const float values[4] = { 0.0f, g.planeBytes == 2 ? 65535.0f / 1023.0f : 1.0f, gammaTable != nullptr ? 1.0f : 0.0f, 0.0f };
        ID3D11ShaderResourceView* sources[6] = { nullptr, g.planeViews[0].Get(), g.planeViews[1].Get(), g.planeViews[2].Get(), gammaTable, g.gameView.Get() };
        Pass(context, g.overlayTarget.Get(), g.gameWidth, g.gameHeight, g.overlayMask.Get(), sources, 6, values);
        g.overlayKnown = true;
    }
    if (!Upload(context, FrameAt(double(g.swaps - 1 + g.offset) - 1.0 + phase))) return false;
    // Ten bits come in the low bits of sixteen.
    const float values[4] = { 0.0f, g.planeBytes == 2 ? 65535.0f / 1023.0f : 1.0f, gammaTable != nullptr ? 1.0f : 0.0f, g.overlayKnown ? 1.0f : 0.0f };
    ID3D11ShaderResourceView* sources[7] = { nullptr, g.planeViews[0].Get(), g.planeViews[1].Get(), g.planeViews[2].Get(), gammaTable,
                                             g.gameView.Get(), g.overlayView.Get() };
    Pass(context, target, width, height, g.convert.Get(), sources, 7, values, true);
    return true;
}
