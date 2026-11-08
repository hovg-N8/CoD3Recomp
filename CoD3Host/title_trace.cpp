// Traces of a few of the title's own functions, for the times the picture
// says nothing was drawn and the question is whether the title ever asked.
// The recompiled functions are reached through weak aliases, which these
// stronger definitions replace; each prints and then runs the original.
// All of it is behind COD3_TRACETITLE=1 and costs nothing otherwise.

#include "kernel.h"
#include "overlay.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#include <Windows.h>

namespace
{
    bool Wanted()
    {
        static const bool wanted = getenv("COD3_TRACETITLE") != nullptr;
        return wanted;
    }

    const char* Text(uint8_t* base, uint32_t address)
    {
        if (address < 0x10000 || address >= 0xFFFF0000u) return "?";
        return reinterpret_cast<const char*>(base + address);
    }
}

// The front end manager's panel dispatcher, sub_824E7998(manager, name,
// argument): the level's chapter title screen is one of its names.
extern "C" PPC_FUNC(__imp__sub_824E7998);
PPC_FUNC(sub_824E7998)
{
    if (Wanted()) { printf("title: panel \"%s\" (argument %08X) on manager %08X\n", Text(base, ctx.r4.u32), ctx.r5.u32, ctx.r3.u32); fflush(stdout); }
    __imp__sub_824E7998(ctx, base);
}

// The chapter title screen, sub_824DFF30(screen, panel): finds its two
// images and four text lines in the panel and starts their fades.
extern "C" PPC_FUNC(__imp__sub_824DFF30);
PPC_FUNC(sub_824DFF30)
{
    const uint32_t screen = ctx.r3.u32;
    if (Wanted()) { printf("title: chapter title screen %08X opens on panel %08X\n", screen, ctx.r4.u32); fflush(stdout); }
    __imp__sub_824DFF30(ctx, base);
    if (Wanted())
    {
        for (uint32_t i = 0; i < 6; i++)
            printf("title:   element %u: kind %u at %08X\n", i, Guest::Read32(base, screen + i * 8), Guest::Read32(base, screen + i * 8 + 4));
        fflush(stdout);
    }
}

// The text element lookup, sub_824D07D0(panel, name).
extern "C" PPC_FUNC(__imp__sub_824D07D0);
PPC_FUNC(sub_824D07D0)
{
    const uint32_t name = ctx.r4.u32;
    __imp__sub_824D07D0(ctx, base);
    if (Wanted()) { printf("title: text element \"%s\" -> %08X\n", Text(base, name), ctx.r3.u32); fflush(stdout); }
}

// The image element lookup, sub_824DFC70(panel, name).
extern "C" PPC_FUNC(__imp__sub_824DFC70);
PPC_FUNC(sub_824DFC70)
{
    const uint32_t name = ctx.r4.u32;
    __imp__sub_824DFC70(ctx, base);
    if (Wanted()) { printf("title: image element \"%s\" -> %08X\n", Text(base, name), ctx.r3.u32); fflush(stdout); }
}

// The chapter title screen's text, sub_824C0CD8(screen, line, text): the
// script's settext on a hud element of types ten to thirteen lands here.
extern "C" PPC_FUNC(__imp__sub_824C0CD8);
PPC_FUNC(sub_824C0CD8)
{
    if (Wanted()) { printf("title: screen %08X line %u text \"%s\"\n", ctx.r3.u32, ctx.r4.u32, Text(base, ctx.r5.u32)); fflush(stdout); }
    __imp__sub_824C0CD8(ctx, base);
}

// A hud element's text, sub_824ACC90(element, string): every settext.
extern "C" PPC_FUNC(__imp__sub_824ACC90);
PPC_FUNC(sub_824ACC90)
{
    if (Wanted())
    {
        const uint32_t element = ctx.r3.u32, string = ctx.r4.u32;
        const uint32_t text = string != 0 ? Guest::Read32(base, string) : 0;
        printf("title: hud element %08X (index %u) settext \"%s\"\n", element, Guest::Read32(base, element), text != 0 ? Text(base, text + 12) : "");
        fflush(stdout);
    }
    __imp__sub_824ACC90(ctx, base);
}

// The front end's level launcher, sub_82517840: runs "spmap <name>" for
// the name in the pending map buffer, once its gates let it.
extern "C" PPC_FUNC(__imp__sub_82517840);
PPC_FUNC(sub_82517840)
{
    static int shown = 0;
    const char* pending = Text(base, 0x829C3A48);
    const bool interesting = pending[0] != 0;
    __imp__sub_82517840(ctx, base);
    if (Wanted() && interesting && (shown < 20 && shown++ < 20))
    {
        printf("title: launcher saw \"%s\", returned %u, gate object %08X (+40 %08X vs %08X), loading flag %08X\n", pending, ctx.r3.u32,
            Guest::Read32(base, 0x829C39E8), Guest::Read32(base, 0x829C39E8) ? Guest::Read32(base, Guest::Read32(base, 0x829C39E8) + 40) : 0,
            Guest::Read32(base, 0x82A56CF0), Guest::Read32(base, 0x829C3A24));
        fflush(stdout);
    }
}

// The XMA voice's decode step, sub_823B8C60(voice, destination, ?, &count):
// one context job and the copy of what it produced.
extern "C" PPC_FUNC(__imp__sub_823B8C60);
PPC_FUNC(sub_823B8C60)
{
    static int shown = 0;
    const uint32_t voice = ctx.r3.u32, dest = ctx.r4.u32, arg5 = ctx.r5.u32, countPtr = ctx.r6.u32;
    const uint32_t before = countPtr ? Guest::Read32(base, countPtr) : 0;
    __imp__sub_823B8C60(ctx, base);
    if (getenv("COD3_XMATRACE") != nullptr && (shown < 60 && shown++ < 60))
    {
        printf("title: xma step voice %08X dest %08X r5 %08X count %u -> returned %u, count now %u, dest words %08X %08X\n",
            voice, dest, arg5, before, ctx.r3.u32, countPtr ? Guest::Read32(base, countPtr) : 0,
            Guest::Read32(base, dest), Guest::Read32(base, dest + 4));
        fflush(stdout);
    }
}

// --- the mixer ------------------------------------------------------------------
// The audio callback sub_822C1108(mixer) runs on the render driver's
// frame; with a mixer thread it wakes it and waits for it, without one it
// mixes inline. The thread's loop is sub_822C0F90 and the mix itself
// sub_822C0990(mixer, flag). Behind COD3_XMATRACE: how often each runs.

namespace
{
    bool MixTrace()
    {
        static const bool wanted = getenv("COD3_XMATRACE") != nullptr;
        return wanted;
    }
    uint64_t Now()
    {
        return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    }
}

extern "C" PPC_FUNC(__imp__sub_822C1108);
PPC_FUNC(sub_822C1108)
{
    static int shown = 0;
    static uint64_t lastSecond = 0, calls = 0;
    const uint32_t mixer = ctx.r3.u32;
    const uint32_t thread = Guest::Read32(base, mixer + 304);
    calls++;
    if (MixTrace() && (shown < 8 && shown++ < 8))
        printf("title: audio callback mixer %08X thread %08X at %llu ms\n", mixer, thread, (unsigned long long)Now());
    __imp__sub_822C1108(ctx, base);
    if (MixTrace() && Now() - lastSecond >= 1000)
    {
        lastSecond = Now();
        printf("title: audio callback %llu calls so far, thread %08X\n", (unsigned long long)calls, thread);
        fflush(stdout);
    }
}

extern "C" PPC_FUNC(__imp__sub_822C0990);
PPC_FUNC(sub_822C0990)
{
    static uint64_t calls = 0, lastSecond = 0;
    calls++;
    const uint64_t started = Now();
    __imp__sub_822C0990(ctx, base);
    if (MixTrace() && Now() - lastSecond >= 1000)
    {
        lastSecond = Now();
        printf("title: mix %llu calls so far, the last took %llu ms, flag %u\n", (unsigned long long)calls, (unsigned long long)(Now() - started), ctx.r4.u32);
        fflush(stdout);
    }
}

// The mixer thread's function, sub_822C0F90(a, b): loops while b is zero.
extern "C" PPC_FUNC(__imp__sub_822C0F90);
PPC_FUNC(sub_822C0F90)
{
    static int shown = 0;
    const uint32_t a = ctx.r3.u32, b = ctx.r4.u32;
    if (MixTrace() && shown < 20) printf("title: mixer thread function (%08X, %08X) enters on thread %lu at %llu ms\n", a, b, GetCurrentThreadId(), (unsigned long long)Now());
    __imp__sub_822C0F90(ctx, base);
    if (MixTrace() && (shown < 20 && shown++ < 20)) printf("title: mixer thread function (%08X, %08X) returns %u at %llu ms\n", a, b, ctx.r3.u32, (unsigned long long)Now());
    fflush(stdout);
}

// Cbuf_AddText, sub_824644D0(text): every command the title puts on its own
// buffer, which is how its menus act.
extern "C" PPC_FUNC(__imp__sub_824644D0);
PPC_FUNC(sub_824644D0)
{
    if (Wanted())
    {
        std::string text = Text(base, ctx.r3.u32);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        printf("title: command \"%s\"\n", text.c_str());
        fflush(stdout);
    }
    __imp__sub_824644D0(ctx, base);
}

// A text element's draw, sub_824CFBE8(element, ...): what the front end
// shows, each frame. The element's text object is at word seven, its
// characters twelve bytes in. Two of the texts say which page is up: the
// main menu's first entry and the options page's title, and the overlay
// opens its own settings when the one gives way to the other, which is
// the player pressing OPTIONS.
extern "C" PPC_FUNC(__imp__sub_824CFBE8);
PPC_FUNC(sub_824CFBE8)
{
    const uint32_t element = ctx.r3.u32;
    if (element >= 0x10000 && element < 0xFFFF0000u)
    {
        const uint32_t text = Guest::Read32(base, element + 28);
        if (text >= 0x10000 && text < 0xFFFF0000u)
        {
            const char* characters = Text(base, text + 12);
            if (strcmp(characters, "OPTIONS MENU") == 0) Overlay::NoteFrontEndText(Overlay::FrontEndText::OptionsMenu);
            else if (strcmp(characters, "SINGLE PLAYER") == 0) Overlay::NoteFrontEndText(Overlay::FrontEndText::MainMenu);
            else if (strcmp(characters, "PRESS START") == 0) Overlay::NoteFrontEndText(Overlay::FrontEndText::MainMenu);   // (co-op v46: the title screen shows the co-op entry too)
        }
    }
    __imp__sub_824CFBE8(ctx, base);
}

// A print the release build formats and throws away, sub_82539518(format,
// ...): fourteen of the title's own warnings go through it, "Could not find
// animation tree '%s'" among them. Printed here as "title says:", always:
// they are rare and each one is a fault of the run.
extern "C" PPC_FUNC(__imp__sub_82539518);
PPC_FUNC(sub_82539518)
{
    static int said = 0;
    if ((said < 200 && said++ < 200))
    {
        std::string text = Kernel::FormatGuestCall(ctx, base, ctx.r3.u32, 4);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        printf("title says: %s (from %08X)\n", text.c_str(), uint32_t(ctx.lr));
        fflush(stdout);
    }
    __imp__sub_82539518(ctx, base);
}

// The AI's animation tree, sub_8253C490(): "generic_human" looked up in the
// loaded assets. What it found, each time, with COD3_TRACETITLE.
extern "C" PPC_FUNC(__imp__sub_8253C490);
PPC_FUNC(sub_8253C490)
{
    const uint32_t from = uint32_t(ctx.lr);
    __imp__sub_8253C490(ctx, base);
    static int told = 0;
    if (Wanted() && (told < 40 && told++ < 40)) { printf("title: animation tree generic_human is %08X (from %08X)\n", ctx.r3.u32, from); fflush(stdout); }
}

// Three more of the title's own reports the release build keeps quiet,
// printed as "title says:" always, since each one is a fault of the run:
// sub_821279C0(format, ...) formats and hands the text to a printer the
// build left empty; sub_820FBDB8(log, level, format, ...) keeps the last
// eight messages in a ring ("apsMemory : pool(size=%d, count=%d) is
// empty" among them); sub_820CB568() is the break after a failed check
// ("stream_alloc: out of memory!" and fifty others), which does nothing
// unless a debug flag is set: the call site says which check failed.
extern "C" PPC_FUNC(__imp__sub_821279C0);
PPC_FUNC(sub_821279C0)
{
    static int said = 0;
    if ((said < 300 && said++ < 300))
    {
        std::string text = Kernel::FormatGuestCall(ctx, base, ctx.r3.u32, 4);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        printf("title says: %s (from %08X)\n", text.c_str(), uint32_t(ctx.lr));
        fflush(stdout);
    }
    __imp__sub_821279C0(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_820FBDB8);
PPC_FUNC(sub_820FBDB8)
{
    static int said = 0;
    if ((said < 300 && said++ < 300))
    {
        std::string text = Kernel::FormatGuestCall(ctx, base, ctx.r5.u32, 6);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        printf("title says (level %u): %s (from %08X)\n", ctx.r4.u32, text.c_str(), uint32_t(ctx.lr));
        fflush(stdout);
    }
    __imp__sub_820FBDB8(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_820CB568);
PPC_FUNC(sub_820CB568)
{
    static std::map<uint32_t, uint32_t> seen;
    const uint32_t from = uint32_t(ctx.lr);
    if (++seen[from] <= 3)
    {
        printf("title: a check failed before the break at %08X (time %u there)\n", from, seen[from]);
        fflush(stdout);
    }
    __imp__sub_820CB568(ctx, base);
}

// sub_82144CA8 is a bare return the release build left where the animation
// library prints its complaints ("Couldn't find skeleton \"%s\" while loading
// animfile \"%s\".", "Duplicate anim %s found.", "Attempt to load already
// loaded AnimFile %s"). The same return also fills empty slots in virtual
// tables, so only a first argument that is a string in the read-only data
// is taken for a format. They are many (a line per shader registered, the
// streamer's decompression), so only with COD3_TRACETITLE=1.
extern "C" PPC_FUNC(__imp__sub_82144CA8);
PPC_FUNC(sub_82144CA8)
{
    const uint32_t format = ctx.r3.u32;
    if (Wanted() && format >= 0x82000000u && format < 0x82090000u)
    {
        bool text = true;
        int length = 0;
        for (; length < 200; length++)
        {
            const uint8_t c = base[format + length];
            if (c == 0) break;
            if ((c < 32 || c >= 127) && c != '\n') { text = false; break; }
        }
        // The streamer's per-chunk decompression line is not a complaint.
        const bool chatter = memcmp(base + format, "LZO ", 4) == 0;
        static int said = 0;
        if (text && length >= 4 && !chatter && (said < 2000 && said++ < 2000))
        {
            std::string message = Kernel::FormatGuestCall(ctx, base, format, 4);
            while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();
            printf("title says: %s (from %08X)\n", message.c_str(), uint32_t(ctx.lr));
            fflush(stdout);
        }
    }
    __imp__sub_82144CA8(ctx, base);
}
