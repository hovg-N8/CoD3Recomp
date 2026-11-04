// Small fixes to the multiplayer program alone, with its own addresses
// (make_mp_host.py copies this file unchanged; the single player build
// compiles it to nothing).

#ifdef COD3_MP

#include <windows.h>
#include <emmintrin.h>
#include "debug_probe.h"
#include "kernel.h"
#include "bot_loadouts.h"
#include "input.h"
#include "button_prompts.h"
#include "tactical_point_input.h"
#include "overlay.h"
#include "render.h"
#include "render_stats.h"
#include "render_commands.h"
#include "render_commands.h"
#include "settings.h"
#include "campaign_animation.h"
#include "campaign_sound.h"
#include "world_model_fit.h"
#include "session_player_storage.h"
#include "player_capacity.h"
#include "server_client_scan.h"
#include "sampler.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <array>
#include "net.h"   // 0.5.275: the side channel (the UAV plane on every machine)
#include <deque>
#include <map>
#include <atomic>
#include <mutex>
#include <vector>
#include <string>
#include <set>
#include <mutex>
#include <chrono>
#include <cmath>
#include <string>
#include <cstdio>
#include <cstring>
#include "env_cache.h"   // 0.5.370: getenv read once, log flushed at most every 100 ms

// 0.5.240: the players in play: 24 (the game's own), or 32 with
// COD3_PLAYERS=32 (BIG-TEST-32.bat). Read by the generated code's reviewed
// sites (patch_cap32) and by PlayerCapacity::Active(). Both PCs of a game must
// use the same number: the session's messages carry player numbers ranged by it.
uint32_t MpCapClients = [] {
    const char* text = getenv("COD3_PLAYERS");
    if (text && text[0]) return atoi(text) >= 32 ? 32u : 24u;
    // 0.5.247: or the setting "max_players = 32" (F11, Multiplayer: "32
    // players"), read straight from CoD3Recomp.ini beside the program: this
    // is set before anything else runs, so before the settings are loaded.
    wchar_t path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return 24u;
    std::filesystem::path ini = std::filesystem::path(path).parent_path() / L"CoD3Recomp.ini";
    uint32_t players = 24u;
    if (FILE* file = _wfopen(ini.c_str(), L"rb"))
    {
        char line[256];
        while (fgets(line, sizeof(line), file))
        {
            const char* p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncmp(p, "max_players", 11) != 0) continue;
            p += 11;
            while (*p == ' ' || *p == '\t' || *p == '=') p++;
            players = atoi(p) >= 32 ? 32u : 24u;
        }
        fclose(file);
    }
    return players;
}();

// 0.5.208: phase one of 16v16. Relocate storage only; every gameplay,
// session and UI limit remains 24 until the other arrays are enlarged.
namespace {
    constexpr uint32_t PlayerRecordBytes = 2992, PlayerStorageCount = 32;
    constexpr uint32_t PlayerStorageBytes = PlayerRecordBytes * PlayerStorageCount;
    std::atomic<uint32_t> g_playerStorage{0};
}
// 0.5.214: replace the join path's six-wide scan before raising the cap.
void MpFindFreeServerClient(PPCContext& ctx, uint8_t* base)
{
    const uint32_t clients = ctx.r31.u32;
    uint32_t state = ctx.r10.u32;
    const uint32_t slot = clients ? ServerClientScan::FindFree(PlayerCapacity::Active(), [&](uint32_t index) {
        state = Guest::Read32(base, clients + index * PlayerCapacity::ServerClientBytes);
        return state;
    }) : PlayerCapacity::Active();
    ctx.r11.u64 = slot;
    ctx.r31.u64 = uint32_t(clients + slot * PlayerCapacity::ServerClientBytes);
    ctx.r10.u64 = state; // last loaded state, as in the original scan
}

// 0.5.239: the player-cap probe. scripts/patch_recomp.py puts a call to
// MpCapProbe before each of the retail code's 911 instructions that compare
// with or load 23, 24 or 25 (and the EntityList functions' compares with 92,
// 96, 100, 128), listed in docs/player-limit/cap-probe-sites.csv (its id is
// the row). For each: how often it ran and, for a compare, the values it
// compared (lowest, highest, which of 0..63 were seen). A loop over the
// players runs every frame and compares 0..24; a "no player" test sees 24;
// unrelated code shows other numbers. That sorts out which of the 911 must
// become 32, and which never run at all. Written to cap-probe.csv beside
// the exe every 30 s and when the game ends (COD3_NO_CAP_PROBE: off).
namespace
{
    constexpr uint32_t CapProbeSites = 1024;
    struct CapProbeSite { uint64_t hits; int32_t low, high; uint64_t seen; uint64_t big; };
    CapProbeSite g_capProbe[CapProbeSites];
    uint64_t g_capProbeFrames = 0;
    const bool g_capProbeOff = getenv("COD3_NO_CAP_PROBE") != nullptr;
}
void MpCapProbe(uint32_t id, uint32_t value)
{
    if (g_capProbeOff || id >= CapProbeSites) return;
    CapProbeSite& site = g_capProbe[id];
    const int32_t v = int32_t(value);
    if (site.hits++ == 0) site.low = site.high = v;
    else { if (v < site.low) site.low = v; if (v > site.high) site.high = v; }
    if (value < 64) site.seen |= 1ull << value; else site.big++;
}
void MpCapProbeWrite(const char* why)
{
    if (g_capProbeOff) return;
    static std::mutex lock;
    std::lock_guard<std::mutex> hold(lock);
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string file(path);
    const size_t slash = file.find_last_of("\\/");
    file = (slash == std::string::npos ? std::string() : file.substr(0, slash + 1)) + "cap-probe.csv";
    FILE* f = fopen(file.c_str(), "wb");
    if (!f) return;
    fprintf(f, "# cap probe 0.5.239 (%s), %llu server frames\nid,hits,low,high,seen,big\n", why, (unsigned long long)g_capProbeFrames);
    for (uint32_t id = 0; id < CapProbeSites; id++)
    {
        const CapProbeSite site = g_capProbe[id];
        if (site.hits)
            fprintf(f, "%u,%llu,%d,%d,%016llX,%llu\n", id, (unsigned long long)site.hits, site.low, site.high,
                    (unsigned long long)site.seen, (unsigned long long)site.big);
    }
    fclose(f);
}
void RosterTrapReport();
void MpCapProbeTick()
{
    if (g_capProbeOff) return;
    g_capProbeFrames++;
    static uint32_t writtenAt = GetTickCount();
    static bool exitHook = [] { atexit([] { MpCapProbeWrite("exit"); }); return true; }();
    (void)exitHook;
    if (GetTickCount() - writtenAt >= 30000) { writtenAt = GetTickCount(); MpCapProbeWrite("every 30 s"); RosterTrapReport(); }
}

int64_t MpPlayerRecords(uint8_t* base)
{
    static const uint32_t address = [base]() -> uint32_t {
        if (getenv("COD3_NO_PLAYER_RELOCATION")) return 0x82BF21D0u;
        const uint32_t memory = Guest::AllocateExtended(PlayerStorageBytes + 64);
        if (!memory) {
            printf("player-storage: allocation failed; keeping original 24 records\n");
            fflush(stdout);
            return 0x82BF21D0u;
        }
        memset(base + memory, 0, PlayerStorageBytes);
        memcpy(base + memory, base + 0x82BF21D0u, 24 * PlayerRecordBytes);
        memset(base + memory + PlayerStorageBytes, 0xA7, 64);
        g_playerStorage.store(memory);
        printf("player-storage: 32 x 2992 bytes at %08X; active limit remains 24\n", memory);
        fflush(stdout);
        return memory;
    }();
    return int64_t(int32_t(address));
}
static void CheckPlayerStorage(uint8_t* base)
{
    const uint32_t address = g_playerStorage.load();
    if (!address) return;
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 10000) return;
    last = now;
    bool guard = true, spare = true;
    for (uint32_t i = 0; i < 64; ++i)
        if (base[address + PlayerStorageBytes + i] != 0xA7) guard = false;
    for (uint32_t i = PlayerCapacity::Active() * PlayerRecordBytes; i < PlayerStorageBytes; ++i)   // 0.5.241
        if (base[address + i] != 0) spare = false;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    unsigned records = 0, stale = 0;
    for (uint32_t slot = 0; entities && slot < PlayerCapacity::Active(); ++slot) {
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        if (record >= address && record < address + PlayerCapacity::Active() * PlayerRecordBytes &&
            (record - address) % PlayerRecordBytes == 0) ++records;
        if (record >= 0x82BF21D0u && record < 0x82C03A50u) ++stale;
    }
    printf("player-storage: relocated %u, old pointers %u, spare %s, guard %s\n",
           records, stale, spare ? "clean" : "WRITTEN", guard ? "clean" : "DAMAGED");
    fflush(stdout);
}

// 0.5.210: reserve the next eight server-client records without making them
// usable. The generated sub_825336F0 allocates and clears 32 * 4992 bytes;
// mark slots 24-31 and report any write into them during a match.
static void CheckServerClientSpare(uint8_t* base)
{
    constexpr uint32_t ClientsPointer = 0x82B3FDE8;
    constexpr uint32_t ClientBytes = 4992, ActiveCount = 24, StorageCount = 32;
    static uint32_t clients = 0, lastCheck = 0;
    const uint32_t current = Guest::Read32(base, ClientsPointer);
    if (!current) return;
    if (PlayerCapacity::Active() > ActiveCount)
    {
        // 0.5.240: 32 in play: slots 24-31 are real (zero until used).
        if (current != clients)
        {
            clients = current;
            printf("server-client-storage: 32 x 4992 at %08X; all 32 slots in play (COD3_PLAYERS=32)\n", clients);
            fflush(stdout);
        }
        return;
    }
    if (current != clients)
    {
        clients = current;
        memset(base + clients + ActiveCount * ClientBytes, 0xA7, (StorageCount - ActiveCount) * ClientBytes);
        lastCheck = GetTickCount();
        printf("server-client-storage: 32 x 4992 allocated at %08X; spare slots 24-31 marked; active limit remains 24\n", clients);
        fflush(stdout);
        return;
    }
    const uint32_t now = GetTickCount();
    if (now - lastCheck < 10000) return;
    lastCheck = now;
    unsigned changed = 0;
    for (uint32_t i = ActiveCount * ClientBytes; i < StorageCount * ClientBytes; ++i)
    {
        const uint8_t value = base[clients + i];
        if (value == 0xA7) continue;
        if (changed < 8) printf("server-client-storage: spare byte +%u changed to %02X\n", i, value);
        ++changed;
    }
    printf("server-client-storage: spare slots 24-31 %s (%u bytes changed)\n",
           changed ? "WRITTEN" : "clean", changed);
    fflush(stdout);
}

// The shell shock's screen blur. An explosion close by (a bazooka fired at
// the ground at one's feet) shocks the player, and the title then smears the
// screen with a blurred copy of an earlier frame that fades out over the
// shock. Here that copy comes out dark, so the shock is a black, smeared
// screen over everything, the HUD included, for a moment.
//
// This function (the title's, beside cg_shellshockblur, the variable at
// 0x82B9F680) is called once a frame with the local client's shock: r3, when
// it began (r4) and how long it lasts (r5). While the shock lasts it starts
// the blur and notes it for the client (the word at +6256 of the client's
// 6368 bytes at 0x82C7D490), and the frame is drawn through the blur; its
// answer in r3 is not used. Asked with no time left, it notes no blur and
// the frame is drawn as it is. The shock's sound, its view kick and its
// slowed aim are elsewhere and stay. The setting: "Shell shock screen blur"
// in the F11 menu (off by default).
//
// (A washed out, then a grey and doubled picture, seen in the first tests
// of this, were "Native command lists" on Merville, not this.)
extern "C" PPC_FUNC(__imp__sub_8244BB00);
PPC_FUNC(sub_8244BB00)
{
    // What the shock is, each time it changes, into the log; and with
    // debug.txt's shock_capture, F12's captures of its first frames.
    {
        static uint32_t lastStart = 0, lastLength = 0;
        static int framesSince = -1, said = 0;
        const uint32_t start = ctx.r4.u32, length = ctx.r5.u32;
        if (start != lastStart || length != lastLength)
        {
            if ((said < 60 && said++ < 60))
            {
                printf("shock: client %08X, began %u, lasts %u\n", ctx.r3.u32, start, length);
                fflush(stdout);
            }
            if (start != 0 && length != 0) framesSince = 0;
            lastStart = start;
            lastLength = length;
        }
        if (framesSince >= 0)
        {
            framesSince++;
            if (DebugProbe::ShockCapture() && (framesSince == 1 || framesSince == 8 || framesSince == 20 || framesSince == 40))
            {
                Overlay::RequestScreenshot();
                Render::LogNextFrame();
                printf("shock: capture %d frames in\n", framesSince);
                fflush(stdout);
            }
            if (framesSince > 40) framesSince = -1;
        }
    }
    if (!Settings::Get().shellShockBlur) ctx.r5.u64 = 0;
    __imp__sub_8244BB00(ctx, base);
}

// The melee lunge, as World at War's bayonet and Modern Warfare 2's
// commando knife have it (a PC addition).
//
// Call of Duty 3 has a melee assist already: a melee swung with an enemy
// close in front (bg_meleeassistrange, 50 units, inside bg_meleeassistfov)
// takes him as its target (the entity handle at +1480 of the player's
// state) and, every move while it lasts, turns the view onto him
// (bg_meleeassistaspeed). This is that turn (sub_824A1150, with the player
// state in r3 and the move's milliseconds in r4, from Pmove for every move
// on the host and in each player's prediction alike). After it, the player
// is carried towards the target as well: the flat part of his velocity
// (+16 of the state; the origin is at +0) set at the lunge's speed straight
// at him until he is within arm's reach. And the assist takes its target
// from further away (122 units: 128 until 0.5.57, 5% shorter since), so the
// lunge has a gap to close.
//
// Movement is predicted and decided on the host, so in a match everyone
// wants the same build and setting. "Melee lunge" in the F11 menu's
// multiplayer tab (on by default).
namespace
{
    constexpr uint32_t EntityTable = 0x82BD1DC0 + 512;   // [entity, serial] by number
    constexpr uint32_t MeleeTarget = 1480, Velocity = 16;
    constexpr uint32_t EntityOrigin = 336;
    constexpr float LungeSpeed = 520.0f;       // units a second; sprinting is about 280
    constexpr float ArmsReach = 36.0f;
    constexpr uint32_t NameAssistRange = 0x82077FD8;   // "bg_meleeassistrange"
    constexpr uint32_t CvarSet = 0x8254F0C8;

    float GetFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t word = Guest::Read32(base, address);
        float value;
        memcpy(&value, &word, 4);
        return value;
    }
    void PutFloat(uint8_t* base, uint32_t address, float value)
    {
        uint32_t word;
        memcpy(&word, &value, 4);
        Guest::Write32(base, address, word);
    }
}

extern "C" PPC_FUNC(__imp__sub_824A1150);
PPC_FUNC(sub_824A1150)
{
    const uint32_t ps = ctx.r3.u32;
    const bool lunge = Settings::Get().meleeLunge;
    {
        // The assist's reach, once, the first time a move is made: "122"
        // and "50" are written where the title keeps its own strings.
        static int reach = -1;
        if (reach != (lunge ? 1 : 0))
        {
            reach = lunge ? 1 : 0;
            static uint32_t text = 0;
            if (text == 0) text = Guest::AllocatePhysical(0x1000);
            strcpy(reinterpret_cast<char*>(base + text), lunge ? "122" : "50");
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
            call.r3.u32 = NameAssistRange;
            call.r4.u32 = text;
            call.r5.u32 = 1;
            call.lr = 0;
            Guest::Lookup(CvarSet)(call, base);
        }
    }
    __imp__sub_824A1150(ctx, base);
    if (!lunge || ps == 0) return;

    const uint32_t handle = Guest::Read32(base, ps + MeleeTarget);
    if (handle == 0) return;
    const uint32_t number = handle & 0x3FFF, serial = handle >> 14;
    if (number >= 4096) return;
    const uint32_t slot = EntityTable + number * 8;
    if (Guest::Read32(base, slot + 4) != serial) return;
    const uint32_t entity = Guest::Read32(base, slot);
    if (entity == 0) return;

    const float dx = GetFloat(base, entity + EntityOrigin) - GetFloat(base, ps);
    const float dy = GetFloat(base, entity + EntityOrigin + 4) - GetFloat(base, ps + 4);
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (!(distance > ArmsReach) || distance > 400.0f) return;
    PutFloat(base, ps + Velocity, dx / distance * LungeSpeed);
    PutFloat(base, ps + Velocity + 4, dy / distance * LungeSpeed);
}

// A heavy landing (sub_8248ED40, the player's crash landing in his
// movement): the title stuns the player for 500 to 2000 ms (35 a per cent of
// the fall's damage), the player's pm_time (+48 of his record, the movement's
// record at +0 of 0x82B922C8's) with the land flag (0x100 of +44), and cuts his
// speed. With Quick landing the stun is a quarter of that, at most 150 ms; the
// speed cut stays.
extern "C" PPC_FUNC(__imp__sub_8248ED40);
PPC_FUNC(sub_8248ED40)
{
    __imp__sub_8248ED40(ctx, base);
    if (!Settings::Get().quickLanding) return;
    const uint32_t move = Guest::Read32(base, 0x82B922C8);
    const uint32_t record = move ? Guest::Read32(base, move) : 0;
    if (!record) return;
    const uint32_t flags = Guest::Read32(base, record + 44);
    if (!(flags & 0x100)) return;
    const int32_t stun = int32_t(Guest::Read32(base, record + 48));
    if (stun <= 0) return;
    const int32_t quick = std::min(stun / 4, 150);
    Guest::Write32(base, record + 48, uint32_t(quick));
    static int said = 0;
    if ((said < 20 && said++ < 20)) { printf("moves: landing stun %d ms cut to %d\n", stun, quick); fflush(stdout); }
}

// Sprint: the player's stamina is the float at +1168 of his record (1 full),
// and +1172 counts up while he sprints. When a sprint ends the title waits
// about a second before the stamina comes back, then refills it over another
// second or more, and sprinting again waits on that. With Quick sprint
// recovery the stamina starts coming back a quarter second after the sprint
// ends and is full a second and a half from empty.
namespace
{
    float RecordFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t bits = Guest::Read32(base, address);
        float f; memcpy(&f, &bits, 4); return f;
    }
}

namespace { std::string ReadText(uint8_t* base, uint32_t at, size_t most); }
// 0.5.93: bullet holes. Justin's bullet-hole dossier: each weapon's own
// impact records, one for each of 23 surfaces (none, bark, brick, carpet,
// cloth, concrete, dirt, flesh, foliage, glass, grass, gravel, ice, metal, mud,
// paper, plaster, rock, sand, snow, water, wood, asphalt), are pointers at
// +2284 of the weapon (sub_8249A380 finds "<weapon>_<SURFACE>" for each and
// keeps what it found; +4, +16 and +28 of a record are its resources: the
// effect and the mark). A weapon with none never leaves a mark. A map's
// weapons are looked over once after it loads: each weapon's surfaces with no
// record are given the record of a weapon that has one for that surface (an
// original rifle's first), and every weapon's count logged. Then, every five
// seconds, what the weapon in hand has for concrete, metal and wood.
namespace
{
    constexpr uint32_t DecalSlots = 2284, Surfaces = 23;
    const char* const SurfaceNames[Surfaces] = { "none", "bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh",
                                                  "foliage", "glass", "grass", "gravel", "ice", "metal", "mud", "paper",
                                                  "plaster", "rock", "sand", "snow", "water", "wood", "asphalt" };
    void WeaponMarks(uint8_t* base, uint32_t record)
    {
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        // 0.5.98: the look over every weapon, and the filling in, are gone.
        // It walked 255 places of the weapon list, past the list's end, took
        // whatever lay there for weapons and wrote impact records into it:
        // on Justin's Merville "91 weapons looked over, 466 given", and the
        // game's memory was wrecked (the crashes at the side menu and the
        // spawn in 0.5.95 to 0.5.97). Every real weapon has all 22 anyway.
        const uint32_t now = GetTickCount();
        static uint32_t lastLog = 0;
        if (!record || now - lastLog < 5000) return;
        lastLog = now;
        const uint32_t held = Guest::Read32(base, record + 164);
        const uint32_t def = (list && held && held < 256) ? Guest::Read32(base, list + 4 * held) : 0;
        if (def < 0x40000000u || def >= 0xA0000000u) return;
        printf("marks: in hand %s:", ReadText(base, Guest::Read32(base, def + 8), 32).c_str());
        for (const uint32_t k : { 5u, 13u, 21u, 16u, 2u })
        {
            const uint32_t slot = Guest::Read32(base, def + DecalSlots + 4 * k);
            const bool ok = slot >= 0x40000000u && slot < 0xA0000000u;
            printf(" %s %08X (%08X %08X %08X)", SurfaceNames[k], slot, ok ? Guest::Read32(base, slot + 4) : 0,
                   ok ? Guest::Read32(base, slot + 16) : 0, ok ? Guest::Read32(base, slot + 28) : 0);
        }
        printf("\n"); fflush(stdout);
    }
}

// 0.5.240: the roster trap (COD3_ROSTER_TRAP=1). Since 0.5.213 the session's
// 24 x 784 roster lives in its own block and the game's catalogued roster
// instructions are pointed there; an instruction that was missed still uses
// the inline copy (object + 0x1010), which is stale from then on. Its pages
// are made unreadable: each access faults, the function that made it is
// logged ("roster trap:"), the page is opened for that one instruction
// (single step) and closed again. A list of every missed access, for the
// 24 -> 32 work: those must be pointed at the block too.
namespace RosterTrap
{
    std::mutex g_lock;
    uint32_t g_first = 0, g_end = 0;            // the protected guest pages
    uint32_t g_rosterFirst = 0;                 // object + 0x1010
    thread_local uintptr_t t_rearm = 0;         // host page to close after the step
    std::map<uint64_t, uint32_t> g_seen;        // (function << 2 | write) -> count
    std::atomic<uint32_t> g_faults{0};

    bool Wanted()
    {
        static const bool on = [] { const char* t = getenv("COD3_ROSTER_TRAP"); return t && t[0] && t[0] != '0'; }();
        return on;
    }

    bool Hook(void* pointers)
    {
        auto* info = static_cast<EXCEPTION_POINTERS*>(pointers);
        const auto* record = info->ExceptionRecord;
        if (record->ExceptionCode == EXCEPTION_SINGLE_STEP && t_rearm)
        {
            DWORD was = 0;
            VirtualProtect(reinterpret_cast<void*>(t_rearm), 4096, PAGE_NOACCESS, &was);
            t_rearm = 0;
            return true;
        }
        if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2) return false;
        const uintptr_t at = uintptr_t(record->ExceptionInformation[1]);
        const uintptr_t first = uintptr_t(Guest::Base) + g_first, end = uintptr_t(Guest::Base) + g_end;
        if (g_first == 0 || at < first || at >= end) return false;
        const uint32_t guest = uint32_t(at - uintptr_t(Guest::Base));
        const bool write = record->ExceptionInformation[0] == 1;
        const uint32_t function = Sampler::FunctionAt(info->ContextRecord->Rip);
        g_faults.fetch_add(1);
        {
            std::lock_guard<std::mutex> hold(g_lock);
            uint32_t& count = g_seen[(uint64_t(function) << 2) | (write ? 1 : 0)];
            if (count++ == 0)
            {
                const PPCContext* context = Kernel::CurrentContext();
                const uint32_t offset = guest - g_rosterFirst;
                printf("roster trap: sub_%08X %s the stale inline roster: entry %u +%u (lr %08X)\n",
                       function, write ? "WROTE" : "read", offset / 784, offset % 784,
                       context ? uint32_t(context->lr) : 0);
                fflush(stdout);
            }
        }
        const uintptr_t page = at & ~uintptr_t(4095);
        DWORD was = 0;
        VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &was);
        t_rearm = page;
        info->ContextRecord->EFlags |= 0x100;   // trap after this one instruction
        return true;
    }

    void Report()
    {
        if (!Wanted()) return;
        std::lock_guard<std::mutex> hold(g_lock);
        printf("roster trap: %u faults, %zu functions\n", g_faults.load(), g_seen.size());
        for (const auto& [key, count] : g_seen)
            printf("roster trap:   sub_%08X %s x%u\n", uint32_t(key >> 2), (key & 1) ? "write" : "read", count);
        fflush(stdout);
    }
}

static void RosterTrapArm(uint8_t* base, uint32_t object)
{
    (void)base;
    if (!RosterTrap::Wanted()) return;
    std::lock_guard<std::mutex> hold(RosterTrap::g_lock);
    DWORD was = 0;
    if (RosterTrap::g_first)
        VirtualProtect(Guest::Base + RosterTrap::g_first, RosterTrap::g_end - RosterTrap::g_first, PAGE_READWRITE, &was);
    const uint32_t start = object + SessionPlayerStorage::ObjectOffset;
    const uint32_t stop = start + 24 * SessionPlayerStorage::RecordBytes;
    const uint32_t first = (start + 4095) & ~4095u, end = stop & ~4095u;   // only pages wholly inside the roster
    RosterTrap::g_rosterFirst = start;
    RosterTrap::g_first = RosterTrap::g_end = 0;
    if (end <= first) return;
    Kernel::FaultHook = RosterTrap::Hook;
    if (VirtualProtect(Guest::Base + first, end - first, PAGE_NOACCESS, &was))
    {
        RosterTrap::g_first = first; RosterTrap::g_end = end;
        printf("roster trap: inline roster %08X-%08X closed (pages %08X-%08X)\n", start, stop, first, end);
    }
    else printf("roster trap: could not close %08X-%08X (error %lu)\n", first, end, GetLastError());
    fflush(stdout);
}

namespace
{
    constexpr uint32_t SessionStorageBytes = SessionPlayerStorage::StorageCount * SessionPlayerStorage::RecordBytes;
    std::mutex g_sessionPlayerStorageMutex;
    std::map<uint32_t, uint32_t> g_sessionPlayerStorage;

    uint32_t CurrentSessionObject(uint8_t* base)
    {
        const uint32_t pointer = Guest::Read32(base, 0x82BCCAC0);
        const uint32_t mode = pointer ? Guest::Read32(base, pointer) : 0;
        if (mode == 0 || mode > 0xA0000000u - 0xAEB0u) return 0;
        return mode + 0xAEB0u; // 0xBEC0 session offset == object + 0x1010
    }

    uint32_t EnsureSessionPlayerStorage(uint8_t* base, uint32_t object)
    {
        std::lock_guard<std::mutex> hold(g_sessionPlayerStorageMutex);
        const auto found = g_sessionPlayerStorage.find(object);
        if (found != g_sessionPlayerStorage.end()) return found->second;
        const uint32_t memory = Guest::AllocateExtended(SessionStorageBytes + SessionPlayerStorage::GuardBytes);
        if (!memory)
        {
            printf("session-player-storage: allocation failed for object %08X; using inline 24-entry roster\n", object);
            fflush(stdout);
            return 0;
        }
        const uint32_t original = object + SessionPlayerStorage::ObjectOffset;
        memset(base + memory, 0, SessionStorageBytes);
        memcpy(base + memory, base + original,
               SessionPlayerStorage::ActiveCount * SessionPlayerStorage::RecordBytes);
        memset(base + memory + SessionStorageBytes, 0xA7, SessionPlayerStorage::GuardBytes);
        g_sessionPlayerStorage.emplace(object, memory);
        printf("session-player-storage: %u x 784 at %08X for object %08X; %u in play\n",
               SessionPlayerStorage::StorageCount, memory, object, PlayerCapacity::Active());
        RosterTrapArm(base, object);
        fflush(stdout);
        return memory;
    }
}

uint32_t MpSessionPlayerAddress(uint8_t* base, uint32_t source, uint32_t offset)
{
    const uint32_t address = source + offset;
    if (getenv("COD3_NO_SESSION_RELOCATION")) return address;
    const uint32_t object = CurrentSessionObject(base);
    if (!object) return address;
    // Explicit roster addressing supports the reserved block, but keep the
    // legacy sentinel 24 outside it until coordinated player-cap activation.
    if (SessionPlayerStorage::RedirectRosterAddress(address, object, 1, PlayerCapacity::Active()) == address) return address;
    const uint32_t memory = EnsureSessionPlayerStorage(base, object);
    return SessionPlayerStorage::RedirectRosterAddress(address, object, memory, PlayerCapacity::Active());
}

// 0.5.241: the game's pool of 24 per-player objects (1392 bytes each, held
// at entity +504; sub_82427170 hands one out, sub_820D5BD0 takes it back,
// sub_820D6580 fills the pool). Its struct at 0x82607890 is packed: the 24
// objects, then 24 pointers (+0x8280), 24 indices (+0x82E0), the objects'
// base (+0x8340) and the count (+0x8344). With 32 in play the 25th player
// got none and the game wrote through a null pointer (guest address 0 held
// entity handles, then "call through 0x40xx"). Now the objects (36: the
// fill goes six at a time) and both arrays live in a block of our own; the
// base word points the game at it and the generated code's pointer/index
// addresses come here (patch_cap32 P504/P504d). Unchanged with 24.
uint32_t MpPool504(uint8_t* base, uint32_t address)
{
    constexpr uint32_t Struct = 0x82607890, Pointers = Struct + 0x8280, Indices = Struct + 0x82E0, ObjectsBase = Struct + 0x8340;
    constexpr uint32_t Count = 36, ObjectBytes = 1392;
    if (MpCapClients <= 24) return address;
    static std::atomic<uint32_t> block{0};
    uint32_t memory = block.load();
    if (!memory)
    {
        static std::mutex lock;
        std::lock_guard<std::mutex> hold(lock);
        memory = block.load();
        if (!memory)
        {
            memory = Guest::AllocateExtended(Count * ObjectBytes + 2 * Count * 4 + 64);
            if (!memory) { printf("player-object-pool: allocation failed; the pool stays at 24\n"); fflush(stdout); return address; }
            memset(base + memory, 0, Count * ObjectBytes + 2 * Count * 4 + 64);
            Guest::Write32(base, ObjectsBase, memory);
            block.store(memory);
            printf("player-object-pool: %u x %u at %08X (was 24 at %08X); count now %u\n", Count, ObjectBytes, memory, Struct,
                   Guest::Read32(base, Struct + 0x8344));
            fflush(stdout);
        }
    }
    if (address == Pointers) return memory + Count * ObjectBytes;
    if (address == Indices) return memory + Count * ObjectBytes + Count * 4;
    return address;
}

// 0.5.240: the end of the roster in play, for sub_823EC678's walk from the
// last entry back to the first (it computed the inline end, object + 0x5990,
// and so walked the stale inline copy once the roster had moved).
uint32_t MpSessionRosterEnd(uint8_t* base, uint32_t object)
{
    const uint32_t inlineEnd = object + SessionPlayerStorage::ObjectOffset + 24 * SessionPlayerStorage::RecordBytes;
    if (getenv("COD3_NO_SESSION_RELOCATION") || object == 0 || object != CurrentSessionObject(base)) return inlineEnd;
    const uint32_t memory = EnsureSessionPlayerStorage(base, object);
    return memory ? memory + PlayerCapacity::Active() * SessionPlayerStorage::RecordBytes : inlineEnd;
}

static void CheckSessionPlayerStorage(uint8_t* base)
{
    if (getenv("COD3_NO_SESSION_RELOCATION")) return;
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 10000) return;
    last = now;
    const uint32_t object = CurrentSessionObject(base);
    if (!object) return;
    uint32_t memory = 0;
    {
        std::lock_guard<std::mutex> hold(g_sessionPlayerStorageMutex);
        const auto found = g_sessionPlayerStorage.find(object);
        if (found != g_sessionPlayerStorage.end()) memory = found->second;
    }
    if (!memory) return;
    bool guard = true, spare = true;
    for (uint32_t i = 0; i < SessionPlayerStorage::GuardBytes; ++i)
        if (base[memory + SessionStorageBytes + i] != 0xA7) guard = false;
    for (uint32_t i = PlayerCapacity::Active() * SessionPlayerStorage::RecordBytes;   // 0.5.241: past the players in play
         i < SessionStorageBytes; ++i)
        if (base[memory + i] != 0) { spare = false; break; }
    printf("session-player-storage: %u in play; spare entries %s, guard %s\n", PlayerCapacity::Active(),
           spare ? "clean" : "WRITTEN", guard ? "clean" : "DAMAGED");
    fflush(stdout);
}

void MpFixesFrame(uint8_t* base)
{
    CheckPlayerStorage(base);
    CheckServerClientSpare(base);
    CheckSessionPlayerStorage(base);
    const uint32_t now = GetTickCount();
    {
        const uint32_t entities0 = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t self0 = entities0 ? Guest::Read32(base, entities0 + 4) : 0;
        WeaponMarks(base, self0 ? Guest::Read32(base, self0 + 512) : 0);
    }
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    if (!record) return;
    const bool playing = Guest::Read32(base, record + 1924) == 3;

    if (Settings::Get().quickSprint && playing)
    {
        static uint32_t lastRun = 0, lastCount = 0, lastFrame = 0;
        const uint32_t count = Guest::Read32(base, record + 1172);
        if (count != lastCount) { lastCount = count; lastRun = now; }
        const float seconds = std::min(float(now - lastFrame) / 1000.0f, 0.1f);
        lastFrame = now;
        const float stamina = RecordFloat(base, record + 1168);
        // 0.5.197: Justin: sprint felt close to unlimited. 0.5.42 brought a
        // dry bar back to half in a third of a second, and any stop refilled
        // it a quarter second later (full in 1.5 s), so tapping sprint again
        // ran forever. Now running dry winds you: the bar is held empty for
        // 1.5 s (you walk), then comes back from a quarter; after a sprint
        // you stop yourself it starts filling after 0.75 s, full from empty
        // in 3 s.
        static bool winded = false;
        static uint32_t windedAt = 0;
        auto put = [&](float value) { uint32_t bits; memcpy(&bits, &value, 4); Guest::Write32(base, record + 1168, bits); };
        if (!winded && stamina >= 0.0f && stamina < 0.02f)
        {
            winded = true; windedAt = now;
            static int said = 0;
            if ((said < 5 && said++ < 5)) { printf("moves: out of breath: no sprint for 1.5 s\n"); fflush(stdout); }
        }
        if (winded)
        {
            if (now - windedAt < 1500) put(0.0f);
            else { put(std::max(stamina, 0.25f)); winded = false; lastRun = now; }
        }
        else if (now - lastRun > 750 && stamina >= 0.0f && stamina < 1.0f)
            put(std::min(1.0f, stamina + seconds / 3.0f));
    }

    // 0.5.198: 0.5.197 took every weapon for a scoped one (the overlay field
    // is set on all of them, most to an empty name): every gun walked at the
    // scoped speed and swayed half. Now by name (bots.cpp BotsScopedWeapon).
    // 0.5.197: scoped rifles sway less while aimed (Justin). Every weapon
    // with a scope overlay (+0x650, the sniper rifles) has its aimed sway
    // (+0x6D0..+0x6E4: max angle, pitch, yaw, side and up scales) and its
    // aimed idle drift (+0x69C) set to the Scoped sway share of what the
    // game gave it; the game's own values are kept, so changing the setting
    // or a new map's weapons (a new definition) start from them again.
    {
        static uint32_t lastSway = 0;
        if (now - lastSway >= 2000)
        {
            lastSway = now;
            struct Kept { uint32_t def; float original[7]; float written[7]; };
            static std::vector<Kept> kept;
            static const uint32_t fields[7] = { 0x6D0, 0x6D8, 0x6DC, 0x6E0, 0x6E4, 0x69C, 0x6D4 };
            const float share = std::clamp(Settings::Get().sniperSway, 0, 100) / 100.0f;
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            for (uint32_t w = 1; list && w < 128; w++)
            {
                const uint32_t def = Guest::Read32(base, list + 4 * w);
                if (!BotsScopedWeapon(base, def)) continue;   // 0.5.198: by name (every weapon has an overlay field)
                Kept* k = nullptr;
                for (Kept& e : kept) if (e.def == def) k = &e;
                bool fresh = !k;
                if (k) for (int f = 0; f < 6; f++) if (RecordFloat(base, def + fields[f]) != k->written[f]) fresh = true;
                if (fresh)
                {
                    if (!k) { kept.push_back({}); k = &kept.back(); k->def = def; }
                    for (int f = 0; f < 7; f++) k->original[f] = RecordFloat(base, def + fields[f]);
                    static int said = 0;
                    if ((said < 12 && said++ < 12)) { printf("weapons: scoped weapon %u: aimed sway %.2f, drift %.2f, cut to %.0f%%\n", w, k->original[0], k->original[5], share * 100.0f); fflush(stdout); }
                }
                for (int f = 0; f < 6; f++)   // +0x6D4 (the sway's speed) is left as it is
                {
                    const float value = k->original[f] * share;
                    uint32_t bits; memcpy(&bits, &value, 4);
                    Guest::Write32(base, def + fields[f], bits);
                    k->written[f] = value;
                }
            }
            if (kept.size() > 256) kept.erase(kept.begin(), kept.begin() + 128);
        }
    }

    // Aiming down the sights, for finding its fields: the record's words
    // that change between the sights down and a moment after they come up
    // (the left trigger, or the right mouse button standing in for it), the
    // first ten times.
    {
        static uint32_t snapshot[600] = {};
        static bool haveSnapshot = false, reported = false;
        static uint32_t heldSince = 0;
        static int reports = 0;
        // The sights: bit 8 of the buttons in this machine's player's newest
        // command (client 0's ring of 64, 48 bytes each, at +1688 of his 6352
        // bytes at 0x82C2ECF0, the newest's number at +4760).
        const uint32_t player = 0x82C2ECF0;
        const uint32_t newest = Guest::Read32(base, player + 4760);
        const uint32_t command = player + 1688 + 48 * (newest & 63);
        const bool aiming = (Guest::Read32(base, command + 4) & 8) != 0 || Input::State().leftTrigger > 100;
        if (!aiming)
        {
            for (uint32_t i = 0; i < 600; i++) snapshot[i] = Guest::Read32(base, record + 4 * i);
            haveSnapshot = true;
            reported = false;
            heldSince = 0;
        }
        else if (haveSnapshot && !reported && reports < 10)
        {
            if (!heldSince) heldSince = now;
            if (now - heldSince > 600)
            {
                reported = true;
                reports++;
                std::string line;
                for (uint32_t i = 8; i < 600; i++)
                {
                    const uint32_t word = Guest::Read32(base, record + 4 * i);
                    if (word == snapshot[i]) continue;
                    float f; memcpy(&f, &word, 4);
                    char item[64];
                    snprintf(item, sizeof(item), " +%u=%08X(%.3f)", 4 * i, word, double(f));
                    line += item;
                }
                printf("moves: sights up, changed:%s\n", line.c_str());
                fflush(stdout);
            }
        }
    }

    // move_watch (debug.txt): the sprint's and the landing's words, ten
    // times a second, for 90 seconds of play.
    static uint32_t last = 0, began = 0;
    if (!DebugProbe::MoveWatch() || !playing) return;
    if (!began) began = now;
    if (now - began > 90000 || now - last < 100) return;
    last = now;
    static uint32_t before[64] = {};
    static const uint32_t watched[] = { 40, 44, 48, 52, 1160, 1164, 1168, 1172, 1176, 1180, 1184, 1188, 1192, 1196, 1200 };
    std::string line;
    for (size_t i = 0; i < sizeof(watched) / sizeof(watched[0]); i++)
    {
        const uint32_t word = Guest::Read32(base, record + watched[i]);
        if (word != before[i])
        {
            char item[48];
            if (watched[i] == 1168) snprintf(item, sizeof(item), " +%u=%.3f", watched[i], RecordFloat(base, record + watched[i]));
            else snprintf(item, sizeof(item), " +%u=%d", watched[i], int32_t(word));
            line += item;
            before[i] = word;
        }
    }
    if (!line.empty()) { printf("moves: t %u%s\n", now - began, line.c_str()); fflush(stdout); }
}

// A custom class's main weapon. A player's kit at his spawn (sub_82206028:
// the player, his class 0 to 6, ...) takes his weapons, then gives the
// class's main weapon (by name, from sub_821E6118 for the Americans,
// sub_821E61A8 for the Germans: heavy assault, light assault, rifleman,
// medic, support, anti-armour, scout) and his side's pistol, then the
// class's grenades and ability, and switches to the main weapon. For this
// machine's own player (entity 0) the name asked for is the one chosen in
// the settings; the rest of the kit is the class's.
const char* BotsSecondaryName(uint8_t* base,uint32_t entity);   // bots.cpp (0.5.284)
bool BotsKitSticky(uint8_t* base,uint32_t entity);
bool BotsKitMills(uint8_t* base,uint32_t entity);   // bots.cpp (0.5.329: Rusher 2)
extern "C" PPC_FUNC(__imp__sub_82206028);
extern "C" PPC_FUNC(__imp__sub_821E6118);
extern "C" PPC_FUNC(__imp__sub_821E61A8);
namespace
{
    thread_local const char* g_botSpawnPrimary="";
    thread_local const char* g_botSpawnSecondary="";   // 0.5.284: a bot kit's second main weapon (the Demolitionist's MG42)
    thread_local bool g_botSpawnSticky = false;          // and his sticky bombs
    thread_local bool g_botSpawnMills = false;           // 0.5.329: or Mills bombs (Rusher 2)
    // 0.5.198: a bot's kit grenades (bot_kits.txt): set while his kit is made.
    thread_local bool g_botSpawn = false, g_botHadFrag = false, g_botHadSmoke = false;
    thread_local int g_botFrags = -1, g_botSmokes = -1;
    thread_local bool g_ownAmericanBot = false;
    struct BotGrenadeGift { uint32_t handle; int frags, smokes; bool american, giveFrag, giveSmoke; };
    std::vector<BotGrenadeGift> g_botGifts;
    bool g_ownSpawn = false;
    bool g_ownAmerican = true;       // this machine's player's side at his last kit
    char g_ownPrimary[32] = {};      // and the main weapon it gave him
    bool g_launcherDue = false;      // the launcher still to give, after his kit
    bool g_binocularsDue = false;    // and the binoculars
    std::vector<uint32_t> g_binocularsOthers;   // the other players (and bots) whose kits this machine gave: binoculars for them too
    uint32_t g_ownHandle = 0;   // this machine's player's script handle, from his last kit
    const char* const CustomWeapons[] = { "", "m1garand", "thompson", "bar", "springfield", "mg30cal", "bazooka", "shotgun",
                                          "kar98", "mp40", "mp44", "kar98_sniper", "mg34", "panzerschreck",
                                          "lee_enfield", "bren", "sten", "fg42", "fg42_scoped", "gewehr43", "mg42", "lee_enfield_sniper" };
    // (The campaign's Sten and MG42 have models in the multiplayer archives
    // but no weapon file a map loads: given one, a player gets nothing.)

    // The pistols' names the kit gives (sub_82206028): the Germans' "p38"
    // at 0x8203AFD8, the Americans' "colt" at 0x8203AFDC, each made into a
    // script string (sub_821DF378: the string at r3, the text at r4) and
    // given. For one's own spawn the text asked for is the custom class's
    // second weapon: the other side's pistol, or a main weapon.
    constexpr uint32_t GermanPistol = 0x8203AFD8, AmericanPistol = 0x8203AFDC;

    uint32_t WeaponName(uint8_t* base, int chosen);
    const char* CampaignStandIn(const char* name);   // below, with the campaign guns

    // The custom class's second weapon's name (guest text), or 0: the side's own.
    uint32_t SecondWeapon(uint8_t* base)
    {
        const Settings::Values values = Settings::Get();
        const int slot = values.activeClass - 1;
        if (slot < 0 || slot >= Settings::Values::CustomClasses) return 0;
        const int sidearm = values.classSidearm[slot];
        if (sidearm == 1) return AmericanPistol;
        if (sidearm == 2) return GermanPistol;
        if (sidearm >= 3) return WeaponName(base, sidearm - 2);
        return 0;
    }

    // A weapon of the list by its number (1 on), as guest text.
    uint32_t WeaponName(uint8_t* base, int chosen)
    {
        constexpr int count = int(sizeof(CustomWeapons) / sizeof(CustomWeapons[0]));
        if (chosen <= 0 || chosen >= count) return 0;
        static uint32_t names = 0;
        if (!names)
        {
            names = Guest::AllocatePhysical(0x400);
            uint32_t at = 0;
            for (const char* name : CustomWeapons) { memcpy(base + names + at, name, strlen(name) + 1); at += 32; }
        }
        // A campaign gun not set up on this map: the gun it is a copy of.
        if (const char* standIn = CampaignStandIn(CustomWeapons[chosen]))
            for (int i = 1; i < count; i++)
                if (strcmp(CustomWeapons[i], standIn) == 0) return names + 32 * uint32_t(i);
        return names + 32 * uint32_t(chosen);
    }

    bool CustomWeapon(uint8_t* base, PPCContext& ctx, bool american)
    {
        if(g_botSpawnPrimary[0]) {
            for(int choice=1;choice<int(sizeof(CustomWeapons)/sizeof(CustomWeapons[0]));++choice)
                if(strcmp(g_botSpawnPrimary,CustomWeapons[choice])==0) {
                    ctx.r3.u64=WeaponName(base,choice);
                    printf("bots: native kit requests custom primary %s\n",g_botSpawnPrimary);fflush(stdout);
                    return true;
                }
        }
        if (!g_ownSpawn) return false;
        (void)american;
        const Settings::Values values = Settings::Get();
        const int slot = values.activeClass - 1;
        if (slot < 0 || slot >= Settings::Values::CustomClasses) return false;
        const int chosen = values.classPrimary[slot];
        if (chosen <= 0 || chosen >= int(sizeof(CustomWeapons) / sizeof(CustomWeapons[0]))) return false;
        ctx.r3.u64 = WeaponName(base, chosen);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("class: own spawn with %s\n", CustomWeapons[chosen]); fflush(stdout); }
        return true;
    }
}

PPC_FUNC(sub_82206028)
{
    // A script entity handle: its index in the low 14 bits, its entry (the
    // entity, then its generation) 8 bytes each at 0x82BD1FC0 (as the
    // script's SetModel reads it, sub_824CE130). This machine's own player:
    // the entity of client 0 (0x82BCC7FC's list, +4).
    const uint32_t handle = Guest::Read32(base, ctx.r3.u32);
    const uint32_t entity = Guest::Read32(base, 0x82BD1FC0 + 8 * (handle & 0x3FFF));
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
    const char* previousBotPrimary=g_botSpawnPrimary;
    g_botSpawnPrimary=BotsPrimaryName(base,entity);
    const char* previousBotSecondary=g_botSpawnSecondary;
    const bool previousBotSticky=g_botSpawnSticky;
    const bool previousBotMills=g_botSpawnMills;
    g_botSpawnSecondary=BotsSecondaryName(base,entity);
    g_botSpawnSticky=BotsKitSticky(base,entity);
    g_botSpawnMills=BotsKitMills(base,entity);
    g_ownSpawn = entity != 0 && entity == own;
    if (g_ownSpawn) g_ownHandle = handle;
    // A custom class's base class: the kit as that class's (its items,
    // ability and grenade count).
    if (g_ownSpawn)
    {
        const Settings::Values values = Settings::Get();
        const int slot = values.activeClass - 1;
        if (slot >= 0 && slot < Settings::Values::CustomClasses && values.classBase[slot] > 0)
            ctx.r4.s64 = values.classBase[slot] - 1;
    }
    if(!g_ownSpawn) {
        const int botBase=BotsBaseClass(base,entity);
        if(botBase>=0 && botBase<7)ctx.r4.s64=botBase;
        bool american = false;
        g_botFrags = g_botSmokes = -1; g_botHadFrag = g_botHadSmoke = false;
        g_botSpawn = BotsKitGrenades(base, entity, g_botFrags, g_botSmokes, american);
        g_ownAmericanBot = american;
    }
    static int said = 0;
    if ((said < 20 && said++ < 20))
    {
        printf("class: kit for handle %08X (entity %08X, ours %08X), class %d%s\n", handle, entity, own, ctx.r4.s32,
               g_ownSpawn ? ": this machine's player" : "");
        fflush(stdout);
    }
    __imp__sub_82206028(ctx, base);
    // 0.5.198: a bot kit without frags or smokes it should have: given just
    // after (MpFixesBinoculars hands them out, as it does the binoculars).
    if (g_botSpawn)
    {
        const bool giveFrag = g_botFrags > 0 && !g_botHadFrag, giveSmoke = g_botSmokes > 0 && !g_botHadSmoke;
        if ((giveFrag || giveSmoke) && g_botGifts.size() < 64) g_botGifts.push_back({ handle, g_botFrags, g_botSmokes, g_ownAmericanBot, giveFrag, giveSmoke });
        g_botSpawn = false;
    }
    if (g_ownSpawn && Settings::Get().launcher) g_launcherDue = true;
    if (g_ownSpawn && Settings::Get().binoculars) g_binocularsDue = true;
    else if (!g_ownSpawn && Settings::Get().binoculars && g_binocularsOthers.size() < 64) g_binocularsOthers.push_back(handle);
    g_ownSpawn = false;
    g_botSpawnPrimary=previousBotPrimary;
    g_botSpawnSecondary=previousBotSecondary;
    g_botSpawnSticky=previousBotSticky;
    g_botSpawnMills=previousBotMills;
}

// A script string made from text (sub_821DF378: the string at r3, the text
// at r4). In one's own kit, the side's pistol's name becomes the custom
// class's second weapon.
extern "C" PPC_FUNC(__imp__sub_821DF378);
PPC_FUNC(sub_821DF378)
{
    // 0.5.284: a bot kit's second main weapon in his pistol's place.
    if (!g_ownSpawn && g_botSpawnSecondary[0] && (ctx.r4.u32 == GermanPistol || ctx.r4.u32 == AmericanPistol))
    {
        for (int choice = 1; choice < int(sizeof(CustomWeapons) / sizeof(CustomWeapons[0])); ++choice)
            if (strcmp(g_botSpawnSecondary, CustomWeapons[choice]) == 0)
            {
                ctx.r4.u64 = WeaponName(base, choice);
                static int said = 0;
                if ((said < 20 && said++ < 20)) { printf("bots: kit second weapon %s (for the pistol)\n", g_botSpawnSecondary); fflush(stdout); }
                break;
            }
    }
    if (g_ownSpawn && (ctx.r4.u32 == GermanPistol || ctx.r4.u32 == AmericanPistol))
    {
        if (const uint32_t second = SecondWeapon(base))
        {
            static int said = 0;
            if ((said < 20 && said++ < 20))
            {
                char name[32] = {};
                for (int i = 0; i < 31; i++) { name[i] = char(Guest::Read8(base, second + i)); if (!name[i]) break; }
                printf("class: own spawn, second weapon %s\n", name);
                fflush(stdout);
            }
            ctx.r4.u64 = second;
        }
    }
    __imp__sub_821DF378(ctx, base);
}

namespace
{
    // The main weapon's name a kit asked for (r3, guest text), kept for the
    // launcher's way back.
    void KeepPrimary(uint8_t* base, uint32_t name, bool american)
    {
        if (!g_ownSpawn) return;
        g_ownAmerican = american;
        g_ownPrimary[0] = 0;
        for (int i = 0; name && i < 31; i++) { g_ownPrimary[i] = char(Guest::Read8(base, name + i)); g_ownPrimary[i + 1] = 0; if (!g_ownPrimary[i]) break; }
    }
}

PPC_FUNC(sub_821E6118)
{
    if (!CustomWeapon(base, ctx, true)) __imp__sub_821E6118(ctx, base);
    KeepPrimary(base, ctx.r3.u32, true);
}

PPC_FUNC(sub_821E61A8)
{
    if (!CustomWeapon(base, ctx, false)) __imp__sub_821E61A8(ctx, base);
    KeepPrimary(base, ctx.r3.u32, false);
}

// The kit's grenades: their kind by side (sub_821E6238: the name) and how
// many by class and rank (sub_821E6850: into the word at r3; one, two, three
// as the player is promoted). For one's own spawn: the custom class's kind
// and count, and with "two grenades" at least two.
extern "C" PPC_FUNC(__imp__sub_821E6238);
PPC_FUNC(sub_821E6238)
{
    // 0.5.284: the Demolitionist bot's sticky bombs (multiplayer's own
    // stickygrenade, every map; its name at 0x82039438).
    if (!g_ownSpawn && g_botSpawnSticky)
    {
        ctx.r3.u64 = 0x82039438u;
        static int said = 0;
        if ((said < 10 && said++ < 10)) { printf("bots: kit grenades: sticky bombs\n"); fflush(stdout); }
        return;
    }
    // 0.5.329: Rusher 2's Mills bombs (the campaign's; the frag where they
    // are not set up on the map)
    if (!g_ownSpawn && g_botSpawnMills)
    {
        static uint32_t names = 0;
        if (!names) { names = Guest::AllocatePhysical(0x40); memcpy(base + names, "mills_bomb", 11); memcpy(base + names + 32, "fraggrenade", 12); }
        const bool standIn = CampaignStandIn("mills_bomb") != nullptr;
        ctx.r3.u64 = names + (standIn ? 32 : 0);
        static int said = 0;
        if ((said < 10 && said++ < 10)) { printf("bots: kit grenades: %s\n", standIn ? "Mills bombs not on this map: frags" : "Mills bombs"); fflush(stdout); }
        return;
    }
    if (g_ownSpawn)
    {
        const Settings::Values values = Settings::Get();
        const int slot = values.activeClass - 1;
        const int kind = slot >= 0 && slot < Settings::Values::CustomClasses ? values.classGrenadeType[slot] : 0;
        if (kind > 0)
        {
            static uint32_t names = 0;
            if (!names)
            {
                names = Guest::AllocatePhysical(0x100);
                memcpy(base + names, "fraggrenade", 12);
                memcpy(base + names + 32, "stielhandgranate", 17);
                memcpy(base + names + 64, "mills_bomb", 11);
                memcpy(base + names + 96, "stickygrenade", 14);
            }
            // 3, the Mills bomb: the campaign's, or the frag where it is
            // not set up (campaign_guns off, Poissons). 4, the sticky bomb
            // (the British No. 74): multiplayer's own, loaded on every map
            // (weapon_stickygrenade, item 30: weapusge.cod's
            // WEAPON_STICKYGRENADE, sticks where it lands (noBounce), 3 s
            // fuse, 900 damage in a 147 radius) but given to no class.
            const char* const standIn = kind == 3 ? CampaignStandIn("mills_bomb") : nullptr;
            ctx.r3.u64 = names + (kind == 1 || standIn ? 0 : kind == 2 ? 32 : kind == 4 ? 96 : 64);
            return;
        }
    }
    __imp__sub_821E6238(ctx, base);
}

// How many of a slot's ammo a kit gives (sub_821F28C8: into r3, the slot by
// name in r4 (a script string: its text at +12 of what r4 points at), the
// class, the rank): "primary", "primaryb", "grenade", "smokegrenade" (the
// second grenade slot) and "special" (a mine or a satchel: one). For one's
// own spawn: the custom class's grenade count, two grenades at least, and
// four of the second slot and the special.
// 0.5.220: four mines and four rifle grenades (Justin: 0.5.219 still gave
// "only the normal amount"). They are not counted in sub_821F28C8: the
// class's special is given by sub_821EE6C0 (mp_mine) and sub_821EE7C8
// (m1garand_rg / k98_rg): the weapon (+2664 of the builtin table
// 0x82A98030), then the "special" slot's clip set to 0 (+2864) and its
// spare to 1, 2 or 3 by rank (+2872). After either, the spare is set again,
// to 4 (COD3_SPECIAL_AMMO), the same way. Everyone's: bots' too.
namespace
{
    void SpecialToFour(PPCContext& ctx, uint8_t* base, uint32_t holder, const char* what)
    {
        int want = 4;
        if (const char* e = getenv("COD3_SPECIAL_AMMO")) want = std::clamp(atoi(e), 1, 20);
        const uint32_t handle = holder ? Guest::Read32(base, holder) : 0;
        const uint32_t setSpare = Guest::Read32(base, 0x82A98030 + 2872);
        if (!handle || !setSpare) return;
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        const uint32_t name = call.r1.u32 + 0x100;   // the script string "special"
        Guest::Write32(base, name, 0);
        call.r3.u64 = name; call.r4.u64 = 0x82074D80; call.lr = 0;   // "special"
        Guest::Lookup(0x821E0E48)(call, base);
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = handle; call.r4.u64 = name; call.r5.u64 = uint32_t(want); call.lr = 0;
        Guest::Lookup(setSpare)(call, base);
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = name; call.lr = 0;
        Guest::Lookup(0x821DD938)(call, base);
        static int said = 0;
        if ((said < 40 && said++ < 40)) { printf("class: %s special set to %d (handle %08X)\n", what, want, handle); fflush(stdout); }
    }
}
extern "C" PPC_FUNC(__imp__sub_821EE6C0);
PPC_FUNC(sub_821EE6C0)
{
    const uint32_t holder = ctx.r3.u32;
    __imp__sub_821EE6C0(ctx, base);
    SpecialToFour(ctx, base, holder, "mines");
}
// 0.5.278: mines for a bot (bots.cpp BotLayMine): the class special's own
// giving, sub_821EE6C0 (holder: the script handle at +0, r4 the rank: the
// spare, made 4 by the hook above), for the bot's entity (+484 its handle).
// 277's bots never had mp_mine: no bot class's special is a mine.
void MpFixesGiveMines(PPCContext& ctx, uint8_t* base, uint32_t entity)
{
    const uint32_t handle = entity ? Guest::Read32(base, entity + 484) : 0;
    if (!handle) return;
    static uint32_t holder = 0;
    if (!holder) holder = Guest::AllocatePhysical(0x20);
    memset(base + holder, 0, 0x20);
    Guest::Write32(base, holder, handle);
    PPCContext call = ctx;
    call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    call.r3.u64 = holder; call.r4.u64 = 1; call.lr = 0;
    Guest::Lookup(0x821EE6C0)(call, base);
}
// 0.5.329: rifle grenades for a bot (bots.cpp BotRifleGrenades), the same
// way: the class special's own giving, sub_821EE7C8 (m1garand_rg for the
// Americans, k98_rg for the Germans). 328's riflemen bots owned none.
void MpFixesGiveRifleGrenades(PPCContext& ctx, uint8_t* base, uint32_t entity)
{
    const uint32_t handle = entity ? Guest::Read32(base, entity + 484) : 0;
    if (!handle) return;
    static uint32_t holder = 0;
    if (!holder) holder = Guest::AllocatePhysical(0x20);
    memset(base + holder, 0, 0x20);
    Guest::Write32(base, holder, handle);
    PPCContext call = ctx;
    call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    call.r3.u64 = holder; call.r4.u64 = 1; call.lr = 0;
    Guest::Lookup(0x821EE7C8)(call, base);
}
extern "C" PPC_FUNC(__imp__sub_821EE7C8);
PPC_FUNC(sub_821EE7C8)
{
    const uint32_t holder = ctx.r3.u32;
    __imp__sub_821EE7C8(ctx, base);
    SpecialToFour(ctx, base, holder, "rifle grenades");
}

extern "C" PPC_FUNC(__imp__sub_821F28C8);
PPC_FUNC(sub_821F28C8)
{
    const uint32_t out = ctx.r3.u32, holder = ctx.r4.u32;
    __imp__sub_821F28C8(ctx, base);
    // 0.5.198: a bot's kit: its frags and smokes from bot_kits.txt.
    if (g_botSpawn && out && holder)
    {
        const uint32_t text = Guest::Read32(base, holder);
        char name[24] = {};
        for (int i = 0; text && i < 23; i++) { name[i] = char(Guest::Read8(base, text + 12 + i)); if (!name[i]) break; }
        int want = -1;
        if (strcmp(name, "grenade") == 0) { g_botHadFrag = true; want = g_botFrags; }
        else if (strcmp(name, "smokegrenade") == 0) { g_botHadSmoke = true; want = g_botSmokes; }
        if (want >= 0)
        {
            const int before = int32_t(Guest::Read32(base, out));
            Guest::Write32(base, out, uint32_t(want));
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("bots: kit %s %d (the class's %d)\n", name, want, before); fflush(stdout); }
        }
        return;
    }
    if (!g_ownSpawn || !out || !holder) return;
    const uint32_t string = Guest::Read32(base, holder);
    if (!string) return;
    char slot[24] = {};
    for (int i = 0; i < 23; i++) { slot[i] = char(Guest::Read8(base, string + 12 + i)); if (!slot[i]) break; }
    const Settings::Values values = Settings::Get();
    const int custom = values.activeClass - 1;
    int count = int32_t(Guest::Read32(base, out));
    const int before = count;
    if (strcmp(slot, "grenade") == 0)
    {
        const int chosen = custom >= 0 && custom < Settings::Values::CustomClasses ? values.classGrenades[custom] : 0;
        if (chosen > 0) count = chosen;
        else if (values.twoGrenades && count < 2) count = 2;
    }
    else if ((strcmp(slot, "smokegrenade") == 0 || strcmp(slot, "special") == 0) && values.fourSecondary && count > 0 && count < 4)
        count = 4;
    if (count != before) Guest::Write32(base, out, uint32_t(count));
    static int said = 0;
    if ((said < 30 && said++ < 30)) { printf("class: own spawn, %s %d (the game's %d)\n", slot, count, before); fflush(stdout); }
}

// The classes' abilities: the scout's artillery, the medic's health packs
// and the support's ammo come back on a timer (sub_821F1E28 for the
// artillery, sub_821F23E0 for the others): the time it comes back is kept
// in the player's script variable 0x14D6461A, set to the game time plus the
// wait (three minutes for the artillery). With a shorter recharge, each new
// wait is cut to its share as soon as it is set.
namespace
{
    uint32_t CallGame(PPCContext& ctx, uint8_t* base, uint32_t function, uint32_t r3 = 0, uint32_t r4 = 0)
    {
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = r3;
        call.r4.u64 = r4;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
        return call.r3.u32;
    }
}

bool MpFixesStreakArmed(uint32_t handle);
void MpFixesAbilities(PPCContext& ctx, uint8_t* base)
{
    const int percent = Settings::Get().abilityRecharge;
    if (percent >= 100 || percent <= 0 || !g_ownHandle) return;
    if (MpFixesStreakArmed(g_ownHandle)) return;   // the killstreak's artillery keeps its own clock
    static uint32_t last = 0;
    const uint32_t ticks = GetTickCount();
    if (ticks - last < 250) return;
    last = ticks;
    const uint32_t gameTime = Guest::Read32(base, 0x82A98030 + 1400);   // the script's GetTime
    if (gameTime < 0x82000000u || gameTime >= 0x82600000u) return;
    const uint32_t vars = CallGame(ctx, base, 0x821DE680, g_ownHandle);
    if (!vars) return;
    const uint32_t slot = CallGame(ctx, base, 0x821DE958, vars, 0x14D6461A);
    if (!slot) return;
    static uint32_t adjusted = 0;
    const uint32_t ready = Guest::Read32(base, slot);
    if (ready == adjusted) return;
    const uint32_t now = CallGame(ctx, base, gameTime);
    if (int32_t(ready - now) <= 0 || int32_t(ready - now) > 600000) { adjusted = ready; return; }
    const uint32_t wait = ready - now;
    const uint32_t shorter = now + uint32_t(uint64_t(wait) * uint32_t(percent) / 100);
    Guest::Write32(base, slot, shorter);
    adjusted = shorter;
    printf("class: ability back in %u s, not %u s\n", (shorter - now) / 1000, wait / 1000);
    fflush(stdout);
}

#endif

// A rocket launcher for everyone, as equipment (the Launcher setting): at
// one's own spawn, after the kit, the side's launcher (the Americans'
// bazooka, the Germans' Panzerschreck) is given besides the class's weapons,
// with some rockets; d-pad left takes it out, and again puts it away for the
// main weapon. The script's own calls (the table at 0x82A98030): give a
// weapon at +2664, switch to one at +2692, a weapon's clip and its spare
// ammunition at +2864 and +2872 (the player, the weapon's name, how many).
#ifdef COD3_MP
namespace
{
    constexpr uint32_t Calls = 0x82A98030, MakeText = 0x821E0E48, FreeText = 0x821DD938;
    bool g_launcherOut = false;
    std::string g_launcherReturnWeapon;
    uint32_t g_launcherReturnId = 0;

    uint32_t Call3(PPCContext& ctx, uint8_t* base, uint32_t function, uint32_t r3, uint32_t r4, uint32_t r5)
    {
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = r3;
        call.r4.u64 = r4;
        call.r5.u64 = r5;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
        return call.r3.u32;
    }

    // A script call on a player (his script handle) with a weapon's name (and a count).
    void OnWeaponFor(PPCContext& ctx, uint8_t* base, uint32_t handle, uint32_t slot, const char* weapon, uint32_t count = 0)
    {
        const uint32_t function = Guest::Read32(base, Calls + slot);
        if (function < 0x82000000u || function >= 0x82600000u || !handle) return;
        static uint32_t scratch = 0;
        if (!scratch) scratch = Guest::AllocatePhysical(0x100);
        memset(base + scratch, 0, 0x100);
        memcpy(base + scratch + 0x40, weapon, strlen(weapon) + 1);
        Call3(ctx, base, MakeText, scratch, scratch + 0x40, 0);
        Call3(ctx, base, function, handle, scratch, count);
        Call3(ctx, base, FreeText, scratch, 0, 0);
    }

    // A script call on this machine's player with a weapon's name (and a count).
    void OnWeapon(PPCContext& ctx, uint8_t* base, uint32_t slot, const char* weapon, uint32_t count = 0)
    {
        const uint32_t function = Guest::Read32(base, Calls + slot);
        if (function < 0x82000000u || function >= 0x82600000u || !g_ownHandle) return;
        static uint32_t scratch = 0;
        if (!scratch) scratch = Guest::AllocatePhysical(0x100);
        memset(base + scratch, 0, 0x100);
        memcpy(base + scratch + 0x40, weapon, strlen(weapon) + 1);
        Call3(ctx, base, MakeText, scratch, scratch + 0x40, 0);
        Call3(ctx, base, function, g_ownHandle, scratch, count);
        Call3(ctx, base, FreeText, scratch, 0, 0);
    }

    const char* Launcher() { return g_ownAmerican ? "bazooka" : "panzerschreck"; }
}

// The sights in the air: the aim-down-sights flag's update (sub_824A7770,
// from the movement's PmoveSingle) puts the sights up only on the ground
// (the movement's locals at 0x82C74880, +48 the ground under the player:
// none in the air, so it went to the sights-down path). With ads_in_air
// it is told there is ground for that check alone, and set back after.
extern "C" PPC_FUNC(__imp__sub_824A7770);
PPC_FUNC(sub_824A7770)
{
    constexpr uint32_t Ground = 0x82C74880u + 48u;
    const bool lift = Settings::Get().adsInAir && Guest::Read32(base, Ground) == 0;
    if (lift) Guest::Write32(base, Ground, 1);
    __imp__sub_824A7770(ctx, base);
    if (lift) Guest::Write32(base, Ground, 0);
}

// A dying player's gun dropped (sub_82580E38: the entity in r3; it drops
// the first slot's weapon, +1065). The gun in hand is kept in the second
// slot for pick-ups (below), so the pistol fell instead of the gun held:
// the two are put back the other way first, and the gun held falls.
extern "C" PPC_FUNC(__imp__sub_82580E38);
PPC_FUNC(sub_82580E38)
{
    const uint32_t entity = ctx.r3.u32;
    const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
    if (ps >= 0x40000000u && ps < 0x90000000u)
    {
        uint32_t held = Guest::Read8(base, entity + 3);
        if (held == 0) held = Guest::Read32(base, ps + 164);
        const uint8_t first = Guest::Read8(base, ps + 1065), second = Guest::Read8(base, ps + 1066);
        if (held != 0 && held == second && first != 0 && first != held)
        {
            Guest::Write8(base, ps + 1065, second);
            Guest::Write8(base, ps + 1066, first);
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("slots: dying with %u in hand: it is the one dropped (was the slot's %u)\n", held, first); fflush(stdout); }
        }
    }
    __imp__sub_82580E38(ctx, base);
}

void MpFixesCampaignGuns(PPCContext& ctx, uint8_t* base);
void MpFixesMg42View(uint8_t* base, uint32_t held);
namespace { std::string ReadText(uint8_t* base, uint32_t at, size_t most); }
extern bool g_mortarRoundArms;   // 0.5.310 (below)

// 0.5.199: a weapon taken out for a bot (the script's switch, +2692, on his
// script handle: generation << 14 | his index in the entity table at
// 0x82BD1FC0, found by looking for his entity there). For the Support's and
// Medic's packs (bots.cpp BotDispense). False when he has no handle.
bool MpFixesSwitchWeaponFor(PPCContext& ctx, uint8_t* base, uint32_t entity, const char* weapon)
{
    if (!entity || !weapon || !weapon[0]) return false;
    static uint32_t cachedEntity[64] = {}, cachedHandle[64] = {};
    uint32_t handle = 0;
    for (int i = 0; i < 64; i++)
        if (cachedEntity[i] == entity)
        {
            const uint32_t index = cachedHandle[i] & 0x3FFF;
            if (Guest::Read32(base, 0x82BD1FC0 + 8 * index) == entity && (cachedHandle[i] >> 14) == Guest::Read32(base, 0x82BD1FC0 + 8 * index + 4))
                handle = cachedHandle[i];
        }
    if (!handle)
        for (uint32_t index = 0; index < 4096; index++)
            if (Guest::Read32(base, 0x82BD1FC0 + 8 * index) == entity)
            {
                handle = (Guest::Read32(base, 0x82BD1FC0 + 8 * index + 4) << 14) | index;
                static int next = 0;
                cachedEntity[next % 64] = entity; cachedHandle[next % 64] = handle; next++;
                break;
            }
    if (!handle) return false;
    OnWeaponFor(ctx, base, handle, 2692, weapon);
    return true;
}

// Binoculars for every class (the Binoculars setting). Only the scout's kit
// has them (the loadout's sub_821F3C60: "mp_binoculars_nospot", by rank
// "_rank2" or "_rank3", given as his "special"); the others' specials are
// mines, rifle grenades, revives and ammo (sub_821EE6C0, sub_821EE7C8,
// sub_821F3DF0, sub_821F3EC0, chosen by class in sub_821FB7E0). At one's own
// spawn, after the kit, the plain binoculars are given besides, every slot
// put back as it was, as the launcher is; the host does the same for every
// other player and bot whose kit it gives. D-pad up takes them out (the
// script's switch, +2692), d-pad down goes back to the main weapon (on the
// keyboard: B and N, the Binoculars out and away keys). While they are out
// aim is held for the title (they are at the eyes), the left trigger's
// presses turn an x2 zoom on and off instead, and the right trigger goes
// through only with an artillery strike ready (input.cpp).
namespace
{
    bool g_binocularsOut = false;
    constexpr const char* Binoculars = "mp_binoculars_nospot";
}

bool MpFixesOwnStrikeReady();
bool MpFixesStreakPad(PPCContext& ctx, uint8_t* base, bool down, int rights);
namespace { bool g_heldSpotter = false; }   // 0.5.86: the spotter's binoculars in this machine's player's hands
void MpFixesBinoculars(PPCContext& ctx, uint8_t* base)
{
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
    if (g_binocularsDue)
    {
        g_binocularsDue = false;
        g_binocularsOut = false;
        Input::SetBinoculars(false, false);
        if (ps)
        {
            uint8_t slots[10];
            for (int i = 0; i < 10; i++) slots[i] = Guest::Read8(base, ps + 1064 + i);
            OnWeapon(ctx, base, 2664, Binoculars);
            for (int i = 0; i < 10; i++)
                if (Guest::Read8(base, ps + 1064 + i) != slots[i])
                {
                    printf("class: the binoculars took slot %d (weapon %u, was %u): put back\n", i, Guest::Read8(base, ps + 1064 + i), slots[i]);
                    Guest::Write8(base, ps + 1064 + i, slots[i]);
                }
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("class: own spawn, binoculars given (main weapon %s)\n", g_ownPrimary); fflush(stdout); }
        }
    }
    // The others' (the host gives every player's kit): the same, with their
    // slots put back.
    for (const uint32_t handle : g_binocularsOthers)
    {
        const uint32_t entity = Guest::Read32(base, 0x82BD1FC0 + 8 * (handle & 0x3FFF));
        const uint32_t theirs = entity ? Guest::Read32(base, entity + 512) : 0;
        if (theirs < 0x40000000u || theirs >= 0x90000000u) continue;
        uint8_t slots[10];
        for (int i = 0; i < 10; i++) slots[i] = Guest::Read8(base, theirs + 1064 + i);
        OnWeaponFor(ctx, base, handle, 2664, Binoculars);
        for (int i = 0; i < 10; i++)
            if (Guest::Read8(base, theirs + 1064 + i) != slots[i]) Guest::Write8(base, theirs + 1064 + i, slots[i]);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("class: binoculars given to handle %08X\n", handle); fflush(stdout); }
    }
    g_binocularsOthers.clear();
    // 0.5.198: the bots' grenades their class's kit has no slot for: the
    // side's frag and the smoke given, the count set (the script's clip
    // call, +2864), their weapon slots put back as with the binoculars.
    for (const BotGrenadeGift& gift : g_botGifts)
    {
        const uint32_t entity = Guest::Read32(base, 0x82BD1FC0 + 8 * (gift.handle & 0x3FFF));
        const uint32_t theirs = entity ? Guest::Read32(base, entity + 512) : 0;
        if (theirs < 0x40000000u || theirs >= 0x90000000u) continue;
        uint8_t slots[10];
        for (int i = 0; i < 10; i++) slots[i] = Guest::Read8(base, theirs + 1064 + i);
        const char* frag = gift.american ? "fraggrenade" : "stielhandgranate";
        if (gift.giveFrag) { OnWeaponFor(ctx, base, gift.handle, 2664, frag); OnWeaponFor(ctx, base, gift.handle, 2864, frag, uint32_t(gift.frags)); }
        // 0.5.199: the Germans' smoke is its own weapon (smokegrenade_axis,
        // as the 0.5.198 kit log shows on a German rifleman).
        const char* smoke = gift.american ? "smokegrenade" : "smokegrenade_axis";
        if (gift.giveSmoke) { OnWeaponFor(ctx, base, gift.handle, 2664, smoke); OnWeaponFor(ctx, base, gift.handle, 2864, smoke, uint32_t(gift.smokes)); }
        // 0.5.244: 0.5.243's "smoke count look": the bots own their side's
        // smoke (owned bit set) but its rounds (record +4*(172 + def+1468))
        // stay 0, so they never threw one; the 2864 count call does not set
        // a smoke's. Their grenades' rounds are set directly if still 0.
        {
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            auto setRounds = [&](const char* name, int count) {
                if (!list || count <= 0) return;
                for (uint32_t w = 1; w < 128; w++)
                {
                    const uint32_t def = Guest::Read32(base, list + 4 * w);
                    if (def < 0x40000000u || def >= 0xA0000000u) continue;
                    const uint32_t text = Guest::Read32(base, def + 8);
                    if (text < 0x40000000u || text >= 0xA0000000u) continue;
                    bool same = true;
                    for (size_t c = 0; c <= strlen(name); c++) if (char(Guest::Read8(base, text + uint32_t(c))) != name[c]) { same = false; break; }
                    if (!same) continue;
                    if (!((Guest::Read32(base, theirs + 1056 + 4 * (w >> 5)) >> (w & 31)) & 1)) return;   // not his
                    const uint32_t clip = Guest::Read32(base, def + 1468);
                    if (clip > 128) return;
                    const uint32_t at = theirs + 4 * (172 + clip);
                    if (Guest::Read32(base, at) == 0)
                    {
                        Guest::Write32(base, at, uint32_t(count));
                        static int set = 0;
                        if ((set < 30 && set++ < 30)) { printf("bots: kit extras: %s rounds set to %d (handle %08X)\n", name, count, gift.handle); fflush(stdout); }
                    }
                    return;
                }
            };
            if (gift.giveSmoke) setRounds(smoke, gift.smokes);
            if (gift.giveFrag) setRounds(frag, gift.frags);
        }
        for (int i = 0; i < 10; i++)
            if (Guest::Read8(base, theirs + 1064 + i) != slots[i]) Guest::Write8(base, theirs + 1064 + i, slots[i]);
        static int said = 0;
        if ((said < 30 && said++ < 30))
        {
            printf("bots: kit extras for handle %08X:%s%s\n", gift.handle, gift.giveFrag ? " frags" : "", gift.giveSmoke ? " smokes" : "");
            fflush(stdout);
        }
    }
    g_botGifts.clear();
    // Whether they are out is read from the weapon in hand (+164 of the
    // player's state) each frame, never kept apart from it: the binoculars'
    // weapon number found by name, as the script's give finds it (the
    // name's hash, sub_820E10E0, then the weapon, sub_82480428). (0.5.59's
    // first try kept its own "out" and took the weapon's kind +180 for 7;
    // the binoculars' is not, so it lost track a moment after they came out,
    // held aim on whatever was in hand and would not put them away.)
    static uint8_t binocular = 0;
    static uint32_t binocularMap = 0;
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    if (binocularMap != list) { binocular = 0; binocularMap = list; }
    if (!binocular && list)
    {
        static uint32_t text = 0;
        if (!text) text = Guest::AllocatePhysical(0x40);
        memset(base + text, 0, 0x40);
        memcpy(base + text, Binoculars, strlen(Binoculars) + 1);
        const uint32_t hash = Call3(ctx, base, 0x820E10E0, text, 0, 0);
        binocular = uint8_t(Call3(ctx, base, 0x82480428, hash, 0, 0));
        const uint32_t weapon = binocular ? Guest::Read32(base, list + 4 * binocular) : 0;
        printf("class: binoculars are weapon %u (kind %u, field of view word %08X)\n", binocular,
               weapon ? Guest::Read32(base, weapon + 180) : 0, weapon ? Guest::Read32(base, weapon + 1600) : 0);
        fflush(stdout);
    }
    const uint32_t held = ps ? Guest::Read32(base, ps + 164) : 0;
    const bool out = binocular != 0 && held == binocular && ps != 0;
    {
        // The spotter's binoculars (mp_binoculars, a strike armed): found by
        // name as the class's are.
        static uint8_t spotter = 0;
        static uint32_t spotterMap = 0;
        if (spotterMap != list) { spotter = 0; spotterMap = list; }
        if (!spotter && list)
        {
            static uint32_t text = 0;
            if (!text) text = Guest::AllocatePhysical(0x40);
            memset(base + text, 0, 0x40);
            memcpy(base + text, "mp_binoculars", 14);
            spotter = uint8_t(Call3(ctx, base, 0x82480428, Call3(ctx, base, 0x820E10E0, text, 0, 0), 0, 0));
        }
        g_heldSpotter = spotter != 0 && ps != 0 && held == spotter;
    }
    const uint32_t binocularWeapon = (binocular && list) ? Guest::Read32(base, list + 4 * binocular) : 0;

    // The view through them: the binoculars' own sights' field of view
    // (+1600 of their weapon, 0.5.59's first try left the game's, far too
    // close) set to a light zoom, 1.5 times the player's own field of view
    // (the F11 menu's, cg_fov), and twice that while the left trigger's zoom
    // is on. The game's own value is put back whenever they are not in hand.
    static uint32_t fovWeapon = 0, fovWas = 0;
    static bool zoomed = false;
    const auto view = [&](int level) {   // 0 the game's own, 1 light, 2 the x2 zoom
        if (level == 0)
        {
            if (fovWeapon) Guest::Write32(base, fovWeapon + 1600, fovWas);
            fovWeapon = 0;
            return;
        }
        if (!binocularWeapon) return;
        if (fovWeapon != binocularWeapon) { fovWeapon = binocularWeapon; fovWas = Guest::Read32(base, binocularWeapon + 1600); }
        const float own = float(std::clamp(Settings::Get().fov, 50, 120));
        const float magnify = level == 2 ? 3.0f : 1.5f;
        const float half = own * 0.5f * 3.14159265f / 180.0f;
        const float fov = 2.0f * atanf(tanf(half) / magnify) * 180.0f / 3.14159265f;
        uint32_t word; memcpy(&word, &fov, 4);
        if (Guest::Read32(base, binocularWeapon + 1600) != word)
        {
            Guest::Write32(base, binocularWeapon + 1600, word);
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("class: binoculars' view %.1f degrees (x%.1f of %.0f)\n", fov, magnify, own); fflush(stdout); }
        }
    };
    if (!Settings::Get().binoculars || !g_ownHandle || g_ownPrimary[0] == 0 || !out)
    {
        view(0);
        zoomed = false;
        Input::SetBinoculars(false, false);
        Input::TakeAimPresses();
    }
    else
    {
        // In hand: at the eyes, the right trigger only with a strike ready,
        // the left one's presses turning the x2 zoom on and off.
        Input::SetBinoculars(true, MpFixesOwnStrikeReady());
        if (Input::TakeAimPresses() % 2 == 1) zoomed = !zoomed;
        view(zoomed ? 2 : 1);
        // 0.5.249: one list of the draws at each zoom, a second after it is
        // reached (the sides seen through at the x2 zoom): CoD3MP.log
        // "render: draw list".
        static int level = 0, frames = 0;
        static bool listed[3] = {};
        const int now = zoomed ? 2 : 1;
        if (now != level) { level = now; frames = 0; }
        if (++frames == 60 && !listed[now])
        {
            listed[now] = true;
            RenderCommands::DumpDraws(now == 2 ? "binoculars, x2 zoom" : "binoculars, no zoom", -300);   // 0.5.252: flat draws only
        }
    }
    g_binocularsOut = out;
    RenderCommands::SetBinocularsUp(g_heldSpotter || (Settings::Get().binoculars && g_ownHandle && g_ownPrimary[0] != 0 && out));   // 0.5.248
    RenderCommands::SetBinocularsUp(out || g_heldSpotter);   // 0.5.248: the depth of field pass drawn while they are up
    if (!Settings::Get().binoculars || !g_ownHandle || g_ownPrimary[0] == 0) return;
    const Input::Pad pad = Input::State();
    int starts = 0, ups = 0, downs = 0;
    Input::TakePresses(starts, ups, downs);
    int rights = Input::TakeRightPresses();
    // Presses from before this frame's last second (the menus, a load) are
    // stale: no game frame was there to take them.
    static uint32_t lastFrame = 0;
    const uint32_t tick = GetTickCount();
    if (tick - lastFrame > 1000) starts = ups = downs = rights = 0;
    lastFrame = tick;
    const bool up = ups > 0, down = downs > 0, start = starts > 0;
    // The pause menu: the d-pad walks it, and must not take the binoculars
    // out or put them away. The game's own flag for it: Start's command,
    // togglepaused (sub_82527280), flips a byte per local player at
    // 0x82C73024 (the one PmoveSingle stops the player's moves by), this
    // machine's player's first. (0.5.63 to 0.5.68 guessed at it from the
    // buttons and never saw it.) Presses made while it is set are dropped.
    const bool menu = Guest::Read8(base, 0x82C73024) != 0;
    static bool saidMenu = false;
    if (menu != saidMenu) { saidMenu = menu; printf("class: pause menu %s (binoculars' d-pad %s)\n", menu ? "open" : "closed", menu ? "off" : "on"); fflush(stdout); }
    (void)start;
    if (menu) return;
    if (RenderStats::Current().triangles < 20000.0f || !binocular) return;   // not in the menus
    // 0.5.82: d-pad down picks a killstreak when there is one to pick; d-pad
    // up then puts the binoculars away as well as taking them out.
    const bool streakDown = MpFixesStreakPad(ctx, base, down, rights);
    if (up && out)
    {
        zoomed = false;
        Input::SetBinoculars(false, false);
        g_launcherOut = false;
        OnWeapon(ctx, base, 2692, g_ownPrimary);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("class: d-pad up, binoculars away, to %s\n", g_ownPrimary); fflush(stdout); }
    }
    else if (up && !out)
    {
        OnWeapon(ctx, base, 2692, Binoculars);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("class: d-pad up, binoculars out (in hand %u)\n", held); fflush(stdout); }
    }
    else if (down && out && !streakDown)
    {
        zoomed = false;
        Input::SetBinoculars(false, false);
        g_launcherOut = false;
        OnWeapon(ctx, base, 2692, g_ownPrimary);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("class: d-pad down, binoculars away, to %s\n", g_ownPrimary); fflush(stdout); }
    }
}
// The artillery as a killstreak (the Artillery killstreak setting, on the
// host, who runs every player's game). Each player's kills in a row are
// counted where damage is dealt (G_Damage, sub_825862F8: the target r3,
// the attacker r5; the target's health at +776, a player's state at +512):
// a player whose health goes from above nothing to nothing has died, which
// ends his own run and adds one to his attacker's. Every killstreak_kills
// in a row earns a strike.
//
// The strike is the scout's own: his kit starts the script thread
// "ArtilleryDispenser" (sub_821F1E28, started by sub_821F3C60 through the
// script's thread call at +60 of 0x82A98030: the .bro's name, line 620, the
// function's name, the player and a 12-byte closure from sub_8221D2F0), which
// ends with the player's death, keeps the time the strike is ready in the
// player's variable 0x14D6461A (0: ready) and, once one is called, sets it to
// the game time plus three minutes. For a streak the thread is started if
// the player has none running, or the time set to now; once he has called
// it, it is set an hour away until his next streak, so it does not come back
// by itself. Earned strikes not yet called wait for his next life.
namespace
{
    struct Streak
    {
        uint32_t handle = 0;
        int kills = 0;        // in a row, this life
        int charges = 0;      // strikes earned, not yet called
        int typhoons = 0;     // of those, Typhoon runs
        int uavs = 0;         // UAVs earned, not yet called (0.5.82: kept until called)
        int mortars = 0;      // 0.5.285: mortars earned, not yet set up
        bool armed = false;   // a strike called from the killstreak box, waiting for the binoculars' mark
        bool typhoonCalled = false;   // that strike is a Typhoon run
        bool ours = false;    // the dispenser thread started by us, this life
        bool fired = false;   // 0.5.84: the armed strike's barrage came (the binoculars were fired)
        bool alive = true;
    };
    Streak g_streaks[32];   // 0.5.253: 32 players (was 24)
    bool g_streakBackToGun = false;   // this machine's strike was called: back to the main weapon
    bool g_streakScout[32] = {};   // the class's own dispenser (a scout): his own clock

    int ClientOf(uint8_t* base, uint32_t entity)
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        if (!entities || !entity) return -1;
        for (int i = 0; i < int(MpCapClients); i++) if (Guest::Read32(base, entities + 4 * uint32_t(i + 1)) == entity) return i;
        return -1;
    }

    uint32_t CallArgs(PPCContext& ctx, uint8_t* base, uint32_t function, uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0,
                      uint32_t r6 = 0, uint32_t r7 = 0, uint32_t r8 = 0)
    {
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = r3; call.r4.u64 = r4; call.r5.u64 = r5; call.r6.u64 = r6; call.r7.u64 = r7; call.r8.u64 = r8;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
        return call.r3.u32;
    }

    // The player's ready time (variable 0x14D6461A), or 0 when he has none.
    uint32_t ReadySlot(PPCContext& ctx, uint8_t* base, uint32_t handle)
    {
        const uint32_t vars = CallArgs(ctx, base, 0x821DE680, handle);
        return vars ? CallArgs(ctx, base, 0x821DE958, vars, 0x14D6461A) : 0;
    }

    uint32_t GameTime(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t function = Guest::Read32(base, 0x82A98030 + 1400);
        if (function < 0x82000000u || function >= 0x82600000u) return 0;
        return CallArgs(ctx, base, function, 0);
    }

    void StartDispenser(PPCContext& ctx, uint8_t* base, uint32_t handle)
    {
        const uint32_t thread = Guest::Read32(base, 0x82A98030 + 60);
        if (thread < 0x82000000u || thread >= 0x82600000u) return;
        static uint32_t word = 0;
        if (!word) word = Guest::AllocatePhysical(0x10);
        Guest::Write32(base, word, handle);
        const uint32_t closure = CallArgs(ctx, base, 0x8221D2F0, word);
        if (!closure) return;
        CallArgs(ctx, base, thread, 0x820396B4, 620, 0x8203A130, handle, closure, 0);
    }
}

// 0.5.330: Rusher 2's artillery (bots.cpp BotRusher2): the scout's own,
// his class's artillery dispenser (the script thread StartDispenser starts,
// its ready time the player's variable 0x14D6461A; 329's Rusher 2 bots had
// no spotter's binoculars: 18 not owned). Started once a life; when its
// ready time has come, the spotter's binoculars (mp_binoculars) are given
// with a round in them, as an armed strike's are (his other slots kept).
// Returns 1 ready (binoculars with a round), 0 waiting, -1 no dispenser.
int MpFixesRusherArtillery(PPCContext& ctx, uint8_t* base, uint32_t entity)
{
    const uint32_t handle = entity ? Guest::Read32(base, entity + 484) : 0;
    const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!handle || ps < 0x40000000u || ps >= 0x90000000u) return -1;
    uint32_t slot = ReadySlot(ctx, base, handle);
    if (!slot)
    {
        StartDispenser(ctx, base, handle);
        slot = ReadySlot(ctx, base, handle);
        printf("bots: rusher 2: artillery dispenser %s for handle %08X (ready word %08X)\n", slot ? "started" : "did not start", handle, slot);
        fflush(stdout);
        if (!slot) return -1;
    }
    const uint32_t now = GameTime(ctx, base), ready = Guest::Read32(base, slot);
    if (ready != 0 && now && int32_t(ready - now) > 0) return 0;
    static uint32_t text = 0;
    static uint8_t spotter = 0;
    if (!text) text = Guest::AllocatePhysical(0x40);
    if (!spotter)
    {
        memset(base + text, 0, 0x40);
        memcpy(base + text, "mp_binoculars", 14);
        spotter = uint8_t(Call3(ctx, base, 0x82480428, Call3(ctx, base, 0x820E10E0, text, 0, 0), 0, 0));
    }
    if (!spotter) return -1;
    const bool owned = (Guest::Read32(base, ps + 1056 + 4 * (spotter >> 5)) & (1u << (spotter & 31))) != 0;
    if (!owned)
    {
        uint8_t slots[10];
        for (int n = 0; n < 10; n++) slots[n] = Guest::Read8(base, ps + 1064 + n);
        OnWeaponFor(ctx, base, handle, 2664, "mp_binoculars");
        for (int n = 0; n < 10; n++) if (Guest::Read8(base, ps + 1064 + n) != slots[n]) Guest::Write8(base, ps + 1064 + n, slots[n]);
        printf("bots: rusher 2: artillery ready: the spotter's binoculars (weapon %u) given to handle %08X\n", spotter, handle);
        fflush(stdout);
    }
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t weapon = list ? Guest::Read32(base, list + 4 * spotter) : 0;
    if (weapon)
    {
        const uint32_t clip = Guest::Read32(base, weapon + 1468), ammo = Guest::Read32(base, weapon + 1460);
        if (clip > 0 && clip < 128 && Guest::Read32(base, ps + 4 * (clip + 172)) == 0) Guest::Write32(base, ps + 4 * (clip + 172), 1);
        if (ammo > 0 && ammo < 128 && Guest::Read32(base, ps + 4 * (ammo + 80)) == 0) Guest::Write32(base, ps + 4 * (ammo + 80), 1);
    }
    return 1;
}

// Whether this machine's player may fire his binoculars: a strike waiting
// (on the host, who counts); a joiner's machine does not count kills and
// leaves the trigger to the host's game.
namespace { bool g_damageSeen = false; }
bool MpFixesOwnStrikeReady()
{
    if (!g_damageSeen || !Settings::Get().killstreakArtillery) return true;
    const uint32_t entities = Guest::Read32(Guest::Base, 0x82BCC7FC);
    const uint32_t own = entities ? Guest::Read32(Guest::Base, entities + 4) : 0;
    const int i = ClientOf(Guest::Base, own);
    return i >= 0 && g_streaks[i].armed && g_streaks[i].charges > 0;
}

// Whether a barrage of this player's is a killstreak's (a strike we made
// ready, not a scout's own clock).
bool MpFixesStreakBarrage(uint32_t handle)
{
    for (int i = 0; i < int(MpCapClients); i++)
        if (g_streaks[i].handle == handle) return g_streaks[i].ours && !g_streakScout[i];
    return false;
}

bool MpFixesStreakArmed(uint32_t handle)
{
    for (const Streak& s : g_streaks) if (s.handle == handle && (s.ours || s.charges > 0)) return true;
    return false;
}

// The UAV: every enemy on the compass for a while. The compass draws the
// enemies itself (sub_825049C8 and the rest of its widgets) while the
// enemy_radar setting is 1 (its record at 0x82B2CF90, the value at +12);
// the game never sets it. It is this machine's own drawing, so the UAV
// shows on the earner's machine (the host's player, for now).
namespace
{
    uint64_t g_uavUntil = 0;
    constexpr uint32_t EnemyRadar = 0x82B2CF90;
}
void MpFixesUav(int seconds)
{
    g_uavUntil = GetTickCount64() + uint64_t(seconds) * 1000;
    Guest::Write32(Guest::Base, EnemyRadar + 12, 1);
    Overlay::Notice("UAV ONLINE  -  enemies on your compass for " + std::to_string(seconds) + " seconds", 4000);
    printf("killstreak: UAV on for %d seconds\n", seconds);
    fflush(stdout);
}
void MpFixesUavFrame()
{
    if (g_uavUntil && GetTickCount64() >= g_uavUntil)
    {
        g_uavUntil = 0;
        Guest::Write32(Guest::Base, EnemyRadar + 12, 0);
        Overlay::Notice("UAV offline", 2000);
        printf("killstreak: UAV off\n");
        fflush(stdout);
    }
}

// 0.5.253: the UAV's plane. While a UAV is up a plane of the caller's side
// (Allies: the campaign's P-47 "v_rep_P47" from sp\chambois\veh.cod; Axis:
// the Stuka "stuka_flying" from sp\crssrds\vehicles.cod, both loaded beside
// the map: MpFixesLoadVehicleArchive) flies in, circles over the players for
// the UAV's time and flies off. It is a script_model made the way the
// script's spawn() makes one (sub_824CEAD8: class, origin, spawn flags,
// spawner), its model set by the script's SetModel (sub_824CE130) and moved
// by the script's mover calls, found by the movers' check sub_824BB360
// ("entity is not a script_brushmodel, script_model, or script_origin"):
// moveto sub_824C7FC0 (entity, point, time, acceleration, deceleration;
// table +0xA5C) and rotateto sub_824C80A8 (entity, angles in degrees, the
// same times; +0xA4C). Not deleted after: parked far under the map and used
// again for the next UAV of that side on the same map. Host only (the
// host's game runs every player's); the mover's path goes to the joiners
// with the entity. Shooting it down: 0.5.254 (Build B).
bool MpFixesStukaFound(PPCContext& ctx, uint8_t* base);   // 0.5.271 (below)
namespace
{
    struct UavPlane
    {
        uint32_t handle = 0;      // its script handle (+484), 0 none yet on this map
        uint32_t sound = 0;       // 0.5.259: a script_origin near the host's player, toward the plane: the flybys play there
        int phase = 0;            // 0 parked, 1 flying in, 2 circling, 3 flying off, 4 shot down (0.5.257)
        int health = 0;           // 0.5.257: hit points left (our own count; G_Damage hook)
        uint64_t until = 0;       // the UAV's end (circling)
        uint64_t next = 0;        // the next mover step
        float cx = 0, cy = 0, cz = 0, radius = 2000, angle = 0;
        float dist = 1e9f;        // 0.5.259: its distance from the host's player
        float ground = 0;         // 0.5.260: the highest player's height when it was called
    };
    UavPlane g_uavPlanes[2];      // by side: [0] side 1 (Axis), [1] side 2 (Allies)
    // 0.5.257: the ground the players have covered on this map (sampled every
    // second): the circle goes round all of it, not just where they are now.
    float g_playMinX = 1e9f, g_playMinY = 1e9f, g_playMaxX = -1e9f, g_playMaxY = -1e9f, g_playTop = -1e9f;
    constexpr int kPlaneHealth = 600;        // about 12 rifle or 20 MG hits; a rocket (explosive) counts x3
    constexpr float kPlaneSpeed = 993.0f;    // units a second round the circle (0.5.257: 1045; 0.5.258: 5% slower again)
    std::atomic<uint64_t> g_planeHitMarker{0};   // 0.5.258: until when this machine's player sees a hit marker
    // 0.5.264: the plane's sound, chosen in the console (plane_sound): where
    // (0 the plane, 1 the sound origin, 2 the host's player, 3 the nearest
    // bot) and which (0 flybys, 1 the engine ARPL_P47_1, 2 FLAG_FLAPPING, a
    // sound known to be heard); g_planeSoundNow: play once at the next frame.
    std::atomic<int> g_planeSoundWhere{1}, g_planeSoundWhich{0};   // 0.5.265: at the host's player by default; which 3: a rifle shot (known heard)
    std::atomic<bool> g_planeSoundNow{false};
    std::atomic<const char*> g_planeSoundCustom{nullptr};   // 0.5.267: plane_sound name <ALIAS>
    std::atomic<const char*> g_planeEngineCue{nullptr};     // 0.5.272: plane_engine <cue> <ms>
    std::atomic<int> g_planeEngineEvery{5450};   // 0.5.273: Justin's pick, ARPL_P47_2 every 5450 ms
    uint64_t g_planeCrashAt[2] = {};             // 0.5.273: when a shot-down plane hits the ground (its crash sound)
    std::atomic<bool> g_planeEngineNow{false};
    std::atomic<uint64_t> g_planeSoundQueued{0};   // 0.5.266: when queued (no event in 300 ms: played from the frame)
    std::atomic<bool> g_planeSoundDirect{false};   // 0.5.266: plane_sound ... now: from the frame at once
    std::string g_uavPlaneMap;
    struct UavPlaneAsk { int side = 0; int seconds = 0; float cx = 0, cy = 0, cz = 0, radius = 0; float angle = -1.0f; bool remote = false; };
    std::mutex g_uavPlaneLock;
    std::vector<UavPlaneAsk> g_uavPlaneAsks;
    constexpr float kPlaneYawOffset = 0.0f;   // the model's nose against +X (tune from a screenshot)
    constexpr float kPlaneBank = 25.0f;       // degrees of roll while circling
    constexpr uint64_t kPlaneStepMs = 400;

    float PlaneReadF(uint8_t* base, uint32_t at) { const uint32_t w = Guest::Read32(base, at); float f; memcpy(&f, &w, 4); return f; }
    void PlaneWriteF(uint8_t* base, uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); }

    uint32_t PlaneEntity(uint8_t* base, uint32_t handle)
    {
        const uint32_t index = handle & 0x3FFF;
        if (!handle || index >= 4096) return 0;
        const uint32_t entry = 0x82BD1FC0 + 8 * index;
        return (handle >> 14) == Guest::Read32(base, entry + 4) ? Guest::Read32(base, entry) : 0;
    }

    uint32_t PlaneScratch()
    {
        static uint32_t scratch = 0;
        if (!scratch) scratch = Guest::AllocatePhysical(0x200);
        return scratch;
    }

    // A mover call: entity handle, a vector, the time and no easing.
    void PlaneMove(PPCContext& ctx, uint8_t* base, uint32_t function, uint32_t handle, float x, float y, float z, float seconds)
    {
        const uint32_t vec = PlaneScratch() + 0x180;
        PlaneWriteF(base, vec, x); PlaneWriteF(base, vec + 4, y); PlaneWriteF(base, vec + 8, z);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = handle; call.r4.u64 = vec;
        call.f1.f64 = seconds; call.f2.f64 = 0; call.f3.f64 = 0;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
    }
    void PlaneMoveTo(PPCContext& ctx, uint8_t* base, uint32_t handle, float x, float y, float z, float seconds)
    { PlaneMove(ctx, base, 0x824C8288, handle, x, y, z, seconds); }   // 0.5.254: 824C8288 is moveto (824C5C30: origin +336, trajectory +20); 824C7FC0 was a rotation
    void PlaneRotateTo(PPCContext& ctx, uint8_t* base, uint32_t handle, float pitch, float yaw, float roll, float seconds)
    { PlaneMove(ctx, base, 0x824C80A8, handle, pitch, yaw, roll, seconds); }

    // 0.5.259: a script_origin (no model) for the plane's sound.
    uint32_t PlaneSpawnOrigin(PPCContext& ctx, uint8_t* base, float x, float y, float z)
    {
        static uint32_t scratch = 0;
        if (!scratch) scratch = Guest::AllocatePhysical(0x100);
        memset(base + scratch, 0, 0x100);
        const uint32_t classText = scratch + 0x40, origin = scratch + 0x80;
        memcpy(base + classText, "script_origin", 14);
        PlaneWriteF(base, origin, x); PlaneWriteF(base, origin + 4, y); PlaneWriteF(base, origin + 8, z);
        Call3(ctx, base, MakeText, scratch, classText, 0);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = scratch; call.r4.u64 = origin; call.r5.u64 = 0; call.r6.u64 = 0; call.lr = 0;
        Guest::Lookup(0x824CEAD8)(call, base);
        Call3(ctx, base, FreeText, scratch, 0, 0);
        printf("uav plane: sound origin made: handle %08X\n", call.r3.u32);
        fflush(stdout);
        return call.r3.u32;
    }

    uint32_t PlaneSpawn(PPCContext& ctx, uint8_t* base, int side, float x, float y, float z, const char* testModel = nullptr)
    {
        const uint32_t scratch = PlaneScratch();
        memset(base + scratch, 0, 0x180);
        const uint32_t classText = scratch + 0x40, modelText = scratch + 0x80, origin = scratch + 0xC0;
        // 0.5.254: side 2 is the Allies (253: Justin, American on Merville, got the Stuka).
        // 0.5.266: the Typhoon for both sides: the Stuka was never seen (265,
        // Justin American on Merville twice: the plane flew its circle unseen;
        // its entity's +16 had 0x40000000, the Typhoon's 0).
        const char* model = testModel ? testModel : (side == 2 || !MpFixesStukaFound(ctx, base) ? "v_Typhoon" : "stuka_flying");   // 0.5.271: the Stuka again when it is found   // 0.5.255: Typhoon (P-47's file too big); 0.5.267: plane_model
        (void)side;
        memcpy(base + classText, "script_model", 13);
        memcpy(base + modelText, model, strlen(model) + 1);
        PlaneWriteF(base, origin, x); PlaneWriteF(base, origin + 4, y); PlaneWriteF(base, origin + 8, z);
        Call3(ctx, base, MakeText, scratch, classText, 0);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = scratch; call.r4.u64 = origin; call.r5.u64 = 0; call.r6.u64 = 0; call.lr = 0;
        Guest::Lookup(0x824CEAD8)(call, base);
        const uint32_t handle = call.r3.u32;
        Call3(ctx, base, FreeText, scratch, 0, 0);
        if (!handle) { printf("uav plane: the game would not make a script_model\n"); fflush(stdout); return 0; }
        Call3(ctx, base, MakeText, scratch + 0x20, modelText, 0);
        Call3(ctx, base, 0x824CE130, handle, scratch + 0x20, 0);
        Call3(ctx, base, FreeText, scratch + 0x20, 0, 0);
        // 0.5.258: no looped sound (257's playloopsound was silent: its third
        // word is the sound itself, not a flag). PlaneSound plays the flybys.
        const uint32_t ent = PlaneEntity(base, handle);
        printf("uav plane: %s made for side %d: handle %08X entity %08X at %.0f %.0f %.0f\n", model, side, handle, ent, x, y, z);
        fflush(stdout);
        return handle;
    }
}

// 0.5.267: plane_model <name> [clear] in the console: that model made as a
// script_model in front of the host's player, at eye height, to see whether
// it is drawn (the Stuka, stuka_flying, never was as the UAV); logged: its
// entity's +16 (0x40000000 was set on the Stuka, not on the Typhoon), its
// model (+540) and that model's +32 / +32->+44->+0 / +76 (what the model
// setter sub_8256FD68 checks), and the model looked up by name directly
// (sub_82446EA0, as that setter does). "clear": +16's 0x40000000 kept off.
namespace
{
    std::mutex g_planeModelLock;
    std::string g_planeModelAsk;
    uint32_t g_planeModelHandle = 0;
    bool g_planeModelClear = false;
}
void MpFixesPlaneModelTest(const std::string& args)
{
    std::lock_guard<std::mutex> hold(g_planeModelLock);
    g_planeModelAsk = args.empty() ? std::string("stuka_flying") : args;
    Overlay::ConsolePrint("plane_model: " + g_planeModelAsk + " (in front of you)");
}
namespace
{
    void PlaneModelReport(uint8_t* base, uint32_t ent, const char* when)
    {
        if (!ent) return;
        const uint32_t model = Guest::Read32(base, ent + 540);
        const bool ok = model >= 0x40000000u && model < 0x90000000u;
        const uint32_t m32 = ok ? Guest::Read32(base, model + 32) : 0;
        const uint32_t m44 = m32 >= 0x40000000u && m32 < 0x90000000u ? Guest::Read32(base, m32 + 44) : 0;
        const uint32_t m44v = m44 >= 0x40000000u && m44 < 0x90000000u ? Guest::Read32(base, m44) : 0;
        printf("plane_model: %s: entity %08X +16 %08X +20 %08X +620 %08X +968 %08X model +540 %08X (+32 %08X ->+44 %08X ->%08X, +76 %08X)\n", when, ent,
               Guest::Read32(base, ent + 16), Guest::Read32(base, ent + 20), Guest::Read32(base, ent + 620), Guest::Read32(base, ent + 968), model, m32, m44, m44v,
               ok ? Guest::Read32(base, model + 76) : 0);
        fflush(stdout);
    }

    void PlaneModelFrame(PPCContext& ctx, uint8_t* base)
    {
        std::string ask;
        {
            std::lock_guard<std::mutex> hold(g_planeModelLock);
            ask.swap(g_planeModelAsk);
        }
        static uint64_t reportAt = 0;
        const uint32_t ent = g_planeModelHandle ? PlaneEntity(base, g_planeModelHandle) : 0;
        if (ent && g_planeModelClear) Guest::Write32(base, ent + 16, Guest::Read32(base, ent + 16) & ~0x40000000u);
        if (ent && reportAt && GetTickCount64() > reportAt) { reportAt = 0; PlaneModelReport(base, ent, "1 s later"); }
        if (ask.empty()) return;
        g_planeModelClear = ask.find(" clear") != std::string::npos;
        std::string name = ask.substr(0, ask.find(' '));
        const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        if (ps < 0x40000000u || ps >= 0x90000000u) { printf("plane_model: no player\n"); fflush(stdout); return; }
        const float yaw = PlaneReadF(base, ps + 208) * 3.14159265f / 180.0f;
        const float x = PlaneReadF(base, ps) + cosf(yaw) * 450.0f, y = PlaneReadF(base, ps + 4) + sinf(yaw) * 450.0f, z = PlaneReadF(base, ps + 8) + 120.0f;
        // The model by name, as the setter looks it up: (out, *(0x82BCC88C),
        // the global bank (*(0x82BCC828) +40), the name).
        {
            const uint32_t scratch = PlaneScratch();
            memset(base + scratch + 0x100, 0, 0x60);
            memcpy(base + scratch + 0x110, name.c_str(), std::min<size_t>(name.size(), 0x40));
            const uint32_t globals = Guest::Read32(base, 0x82BCC828);
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
            call.r3.u64 = scratch + 0x100; call.r4.u64 = Guest::Read32(base, 0x82BCC88C);
            call.r5.u64 = globals ? Guest::Read32(base, globals + 40) : 0; call.r6.u64 = scratch + 0x110; call.lr = 0;
            Guest::Lookup(0x82446EA0)(call, base);
            const uint32_t found = Guest::Read32(base, scratch + 0x100);
            printf("plane_model: %s looked up by name: %08X (+32 %08X)\n", name.c_str(), found,
                   found >= 0x40000000u && found < 0x90000000u ? Guest::Read32(base, found + 32) : 0);
            fflush(stdout);
        }
        // 0.5.270: which of Crossroads' vehicles.cod models are found at all
        // (stuka_flying was not, 268/269, with its archive loaded and in the
        // lists): the others in it, and the Typhoon (found).
        for (const char* probe : { "stuka_flying", "stuka", "v_a20", "v_shermanM4A3_d1", "v_gunturret", "tankshell", "v_Typhoon", "v_shermanM4A3" })
        {
            const uint32_t scratch = PlaneScratch();
            memset(base + scratch + 0x100, 0, 0x60);
            memcpy(base + scratch + 0x110, probe, strlen(probe));
            const uint32_t globals = Guest::Read32(base, 0x82BCC828);
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
            call.r3.u64 = scratch + 0x100; call.r4.u64 = Guest::Read32(base, 0x82BCC88C);
            call.r5.u64 = globals ? Guest::Read32(base, globals + 40) : 0; call.r6.u64 = scratch + 0x110; call.lr = 0;
            Guest::Lookup(0x82446EA0)(call, base);
            printf("plane_model: probe %s -> %08X\n", probe, Guest::Read32(base, scratch + 0x100));
        }
        fflush(stdout);
        if (!g_planeModelHandle || !PlaneEntity(base, g_planeModelHandle))
            g_planeModelHandle = PlaneSpawn(ctx, base, 1, x, y, z, name.c_str());
        else
        {
            PlaneMoveTo(ctx, base, g_planeModelHandle, x, y, z, 0.05f);
            const uint32_t scratch = PlaneScratch();
            memset(base + scratch + 0x80, 0, 0x60);
            memcpy(base + scratch + 0x80, name.c_str(), std::min<size_t>(name.size(), 0x40));
            Call3(ctx, base, MakeText, scratch + 0x20, scratch + 0x80, 0);
            Call3(ctx, base, 0x824CE130, g_planeModelHandle, scratch + 0x20, 0);
            Call3(ctx, base, FreeText, scratch + 0x20, 0, 0);
        }
        PlaneModelReport(base, PlaneEntity(base, g_planeModelHandle), (name + " just made").c_str());
        reportAt = GetTickCount64() + 1000;
        Overlay::Notice("PLANE MODEL TEST: " + name + (g_planeModelClear ? " (flag cleared)" : ""), 2500);
    }
}

// 0.5.271: whether stuka_flying is found by name (sub_82446EA0, the global
// bank), as the model setter looks it up; else the Axis get the Typhoon.
bool MpFixesStukaFound(PPCContext& ctx, uint8_t* base)
{
    const uint32_t scratch = PlaneScratch();
    memset(base + scratch + 0x100, 0, 0x60);
    memcpy(base + scratch + 0x110, "stuka_flying", 12);
    const uint32_t globals = Guest::Read32(base, 0x82BCC828);
    PPCContext call = ctx;
    call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    call.r3.u64 = scratch + 0x100; call.r4.u64 = Guest::Read32(base, 0x82BCC88C);
    call.r5.u64 = globals ? Guest::Read32(base, globals + 40) : 0; call.r6.u64 = scratch + 0x110; call.lr = 0;
    Guest::Lookup(0x82446EA0)(call, base);
    const uint32_t found = Guest::Read32(base, scratch + 0x100);
    static int said = 0;
    if ((said < 4 && said++ < 4)) { printf("uav plane: stuka_flying %s by name (%08X)\n", found ? "found" : "NOT found", found); fflush(stdout); }
    return found != 0;
}
// 0.5.285: any model by name, the same way (the mortar's v_81mm, its shell).
bool MpFixesModelFound(PPCContext& ctx, uint8_t* base, const char* name)
{
    const uint32_t scratch = PlaneScratch();
    memset(base + scratch + 0x100, 0, 0x60);
    memcpy(base + scratch + 0x110, name, std::min<size_t>(strlen(name), 0x40));
    const uint32_t globals = Guest::Read32(base, 0x82BCC828);
    PPCContext call = ctx;
    call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    call.r3.u64 = scratch + 0x100; call.r4.u64 = Guest::Read32(base, 0x82BCC88C);
    call.r5.u64 = globals ? Guest::Read32(base, globals + 40) : 0; call.r6.u64 = scratch + 0x110; call.lr = 0;
    Guest::Lookup(0x82446EA0)(call, base);
    return Guest::Read32(base, scratch + 0x100) != 0;
}

// 0.5.257: a hit on a UAV plane (from the G_Damage hook): true when the
// target was one (the game's own damage is then not done). At no hit points
// left it is shot down: it dives and crashes, and the UAV it flew for ends.
namespace { void PlaneSound(PPCContext& ctx, uint8_t* base, uint32_t ent, const char* alias); }   // 0.5.273 (below)

// 0.5.274: effects on the plane: a weapon's effect event played at a point,
// the way a projectile's explosion is (sub_82583310: the effect entity's
// origin +336..344 set, then sub_82556180(out, entity, the weapon's name,
// 56)). Event 56 = that weapon's explosion (its effect and sound); event 1 =
// its muzzle flash (a bullet hit's spark). On a script_origin of its own.
namespace
{
    uint32_t g_planeFxHandle = 0;
    float g_planeCrashX[2] = {}, g_planeCrashY[2] = {}, g_planeCrashZ[2] = {};
    void PlaneFx(PPCContext& ctx, uint8_t* base, float x, float y, float z, const char* weapon, uint32_t event)
    {
        if (!g_planeFxHandle || !PlaneEntity(base, g_planeFxHandle)) g_planeFxHandle = PlaneSpawnOrigin(ctx, base, x, y, z);
        const uint32_t ent = g_planeFxHandle ? PlaneEntity(base, g_planeFxHandle) : 0;
        if (!ent) return;
        static std::map<std::string, uint32_t> names;
        uint32_t& name = names[weapon];
        if (!name)
        {
            name = Guest::AllocatePhysical(0x40);
            memset(base + name, 0, 0x40);
            memcpy(base + name, weapon, std::min<size_t>(strlen(weapon), 0x3F));
        }
        PlaneWriteF(base, ent + 336, x); PlaneWriteF(base, ent + 340, y); PlaneWriteF(base, ent + 344, z);
        PlaneWriteF(base, ent + 352, -90.0f); PlaneWriteF(base, ent + 356, 0.0f); PlaneWriteF(base, ent + 360, 0.0f);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        const uint32_t out = call.r1.u32 + 0x200;
        Guest::Write32(base, out, 0);
        call.r3.u64 = out; call.r4.u64 = ent; call.r5.u64 = name; call.r6.u64 = event; call.r7.u64 = 0; call.r8.u64 = 0; call.lr = 0;
        Guest::Lookup(0x82556180)(call, base);
        static int said = 0;
        if ((said < 30 && said++ < 30)) { printf("uav plane: effect %s event %u at %.0f %.0f %.0f\n", weapon, event, x, y, z); fflush(stdout); }
    }
}

bool BotsIsBotEntity(uint8_t* base, uint32_t entity);   // bots.cpp (0.5.275)
void BotsLearnKill(uint8_t* base, uint32_t attacker, uint32_t victim);   // bots.cpp (0.5.282): lookouts
void MpFixesSendPlaneHit(int side, int hit, bool rocket, bool down);   // 0.5.275 (below)
bool BotsThisMachineHostsBots(uint8_t* base);          // bots.cpp (0.5.275)
bool MpFixesPlaneHit(PPCContext& ctx, uint8_t* base, uint32_t target, uint32_t attacker, int32_t damage, bool remote = false)
{
    int k = -1;
    for (int i = 0; i < 2; i++)
        if (g_uavPlanes[i].handle && PlaneEntity(base, g_uavPlanes[i].handle) == target) k = i;
    if (k < 0) return false;
    UavPlane& p = g_uavPlanes[k];
    if (p.phase != 1 && p.phase != 2 && p.phase != 3) return true;   // parked or already falling
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    if (!remote)
    {
        // Its own side cannot hurt it (as Call of Duty's UAV).
        const uint32_t team = attacker ? Guest::Read32(base, attacker + 520) : 0;
        if (team && int(Guest::Read32(base, team + 4)) == k + 1) return true;
        // 0.5.275: only this machine's own shooters (the others send theirs).
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        if (attacker != own && !(BotsThisMachineHostsBots(base) && BotsIsBotEntity(base, attacker))) return true;
    }
    const int hit = remote ? damage : damage >= 100 ? damage * 2 : std::max(damage, 1);   // rockets and grenades hit harder
    if (!remote) MpFixesSendPlaneHit(k + 1, hit, damage >= 100, false);   // 0.5.275
    // 0.5.274: the hit seen on the plane: a rocket's explosion, a bullet's
    // spark (a muzzle flash, at most one each 60 ms), a little off its middle.
    {
        static uint64_t lastSpark = 0;
        const uint64_t t = GetTickCount64();
        const float jx = float(int(t % 7) - 3) * 30.0f, jy = float(int(t % 5) - 2) * 30.0f;
        const float x = PlaneReadF(base, target + 336) + jx, y = PlaneReadF(base, target + 340) + jy, z = PlaneReadF(base, target + 344);
        if (damage >= 100) PlaneFx(ctx, base, x, y, z, "bazooka", 56);
        else if (t > lastSpark + 60) { lastSpark = t; PlaneFx(ctx, base, x, y, z, "thompson", 1); }
    }
    p.health -= hit;
    int by = -1;
    if (entities && attacker && attacker == Guest::Read32(base, entities + 4)) g_planeHitMarker.store(GetTickCount64() + 220);   // 0.5.258
    for (uint32_t i = 0; entities && attacker && i < MpCapClients; i++)
        if (Guest::Read32(base, entities + 4 * (i + 1)) == attacker) by = int(i);
    printf("uav plane: side %d hit for %d (game damage %d) by client %d, %d left\n", k + 1, hit, damage, by, p.health);
    fflush(stdout);
    if (p.health > 0) return true;
    if (!remote) MpFixesSendPlaneHit(k + 1, 0, false, true);   // 0.5.275: down, for every copy
    // Shot down: nose down, rolling, into the ground ahead of it in 2.5 s.
    constexpr float Pi = 3.14159265f;
    const float dx = -sinf(p.angle), dy = cosf(p.angle);
    const float x = PlaneReadF(base, target + 336) + dx * 1800.0f, y = PlaneReadF(base, target + 340) + dy * 1800.0f;
    const float ground = p.ground - 100.0f;   // 0.5.264: the players' mean height (not the highest seen)
    PlaneRotateTo(ctx, base, p.handle, 55.0f, atan2f(dy, dx) * 180.0f / Pi + kPlaneYawOffset, 120.0f, 2.5f);
    PlaneMoveTo(ctx, base, p.handle, x, y, ground, 2.5f);
    p.phase = 4;
    p.next = GetTickCount64() + 3000;
    g_planeCrashAt[k] = GetTickCount64() + 2500;   // 0.5.273: its crash sound
    // 0.5.274: it blows up where it is hit, trails smoke, and the crash
    // blows up again where it comes down.
    PlaneFx(ctx, base, PlaneReadF(base, target + 336), PlaneReadF(base, target + 340), PlaneReadF(base, target + 344), "panzerschreck", 56);
    PlaneFx(ctx, base, PlaneReadF(base, target + 336), PlaneReadF(base, target + 340), PlaneReadF(base, target + 344), "smokegrenade", 56);
    g_planeCrashX[k] = x; g_planeCrashY[k] = y; g_planeCrashZ[k] = ground;
    if (const uint32_t at = p.sound ? PlaneEntity(base, p.sound) : 0) PlaneSound(ctx, base, at, "UAV:Mortar_Explo");   // and the hit that downed it
    // The UAV it flew for: this machine's (the host's) when it is his side's.
    const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t team = own ? Guest::Read32(base, own + 520) : 0;
    const int ownSide = team ? int(Guest::Read32(base, team + 4)) : 0;
    const bool ours = ownSide == k + 1;
    if (ours && g_uavUntil) g_uavUntil = 1;   // MpFixesUavFrame turns it off next frame
    const bool mine = by >= 0 && entities && Guest::Read32(base, entities + 4 * uint32_t(by + 1)) == own;
    Overlay::Notice(ours ? "YOUR UAV WAS SHOT DOWN" : mine ? "ENEMY UAV SHOT DOWN  -  nice shot" : "ENEMY UAV SHOT DOWN", 4000);
    printf("uav plane: side %d SHOT DOWN by client %d%s\n", k + 1, by, ours ? " (the host's UAV ends)" : "");
    fflush(stdout);
    return true;
}

namespace
{
    // 0.5.258: a flyby sound on the plane (it plays where the plane is):
    // the call playsound/playloopsound end in, sub_82556868 (out word, the
    // entity, the alias' text, 0, -1). Multiplayer's own bank has the P-47's
    // flybys ARPL_P47_By01..07 (no 05) and engines ARPL_P47_1..4.
    void PlaneSound(PPCContext& ctx, uint8_t* base, uint32_t ent, const char* alias)
    {
        // 0.5.261: played at an entity pointer the way the port's own audible
        // sounds are (the scoped Lee-Enfield's shot: the shooter's EntityList
        // entity, r8 = 0); 259/260 on a script_origin were never heard.
        if (!ent) return;
        static uint32_t text = 0;
        if (!text) text = Guest::AllocatePhysical(0x40);
        memset(base + text, 0, 0x40);
        memcpy(base + text + 8, alias, std::min<size_t>(strlen(alias), 0x30));
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = text; call.r4.u64 = ent; call.r5.u64 = text + 8; call.r6.u64 = 0; call.r7.u64 = uint64_t(int64_t(-1)); call.r8.u64 = 0; call.lr = 0;
        Guest::Lookup(0x82556868)(call, base);
        static int said = 0;
        if ((said < 6 && said++ < 6)) { printf("uav plane: sound %s on it (%08X)\n", alias, Guest::Read32(base, text)); fflush(stdout); }
    }
}

bool MpFixesPlaneModelLoaded(int side);   // 0.5.259 (below, with the archives)

void MpFixesPlaneSoundTest(const std::string& args)
{
    int where = g_planeSoundWhere.load(), which = g_planeSoundWhich.load();
    if (args.find("plane") != std::string::npos) where = 0;
    if (args.find("origin") != std::string::npos) where = 1;
    if (args.find("me") != std::string::npos) where = 2;
    if (args.find("bot") != std::string::npos) where = 3;
    if (args.find("flyby") != std::string::npos) which = 0;
    if (args.find("engine") != std::string::npos) which = 1;
    if (args.find("flag") != std::string::npos) which = 2;
    if (args.find("shot") != std::string::npos) which = 3;
    // 0.5.267: plane_sound name <ALIAS> ...: any sound by its name (the
    // P-47 flybys are mute, the rifle shot is heard: try the plane's others).
    {
        const size_t at = args.find("name ");
        if (at != std::string::npos)
        {
            std::string alias = args.substr(at + 5);
            alias = alias.substr(0, alias.find(' '));
            static std::set<std::string> kept;   // the text stays alive for the queue
            static std::mutex keptLock;
            std::lock_guard<std::mutex> hold(keptLock);
            g_planeSoundCustom.store(kept.insert(alias).first->c_str());
            which = 4;
        }
    }
    g_planeSoundDirect.store(args.find("now") != std::string::npos);
    g_planeSoundWhere.store(where); g_planeSoundWhich.store(which);
    g_planeSoundNow.store(true);
    static const char* const W[] = { "plane", "origin", "me", "bot" }, * const S[] = { "flyby", "engine", "flag", "shot", "by name" };
    Overlay::ConsolePrint(std::string("plane_sound: ") + W[where] + ", " + S[which] + " (played at the next weapon sound; the plane's flybys use it too)");
}

// 0.5.272: plane_engine <cue> <ms> in the console: the plane's steady engine
// sound and how often it is started again (0: once; for a looping cue).
void MpFixesPlaneEngine(const std::string& args)
{
    std::string cue = args.substr(0, args.find(' '));
    const size_t space = args.find(' ');
    const int every = space == std::string::npos ? g_planeEngineEvery.load() : atoi(args.c_str() + space + 1);
    if (!cue.empty())
    {
        static std::set<std::string> kept;
        static std::mutex keptLock;
        std::lock_guard<std::mutex> hold(keptLock);
        g_planeEngineCue.store(kept.insert(cue).first->c_str());
    }
    g_planeEngineEvery.store(std::max(0, every));
    g_planeEngineNow.store(true);
    Overlay::ConsolePrint("plane_engine: " + (cue.empty() ? std::string("ARPL_P47_1") : cue) + ", again every " + std::to_string(std::max(0, every)) + " ms");
}

// 0.5.275: the UAV plane on every machine. System Link is peer to peer:
// each machine runs its own server, so a script_model made on one is in
// that one's world only (Justin's friend would never have seen the plane).
// The machine whose player calls a UAV tells the others over the side
// channel (lobby_rules.cpp, as the bots' "C3BR"): "C3UP" (side, seconds,
// the circle, its first angle). Each flies its own copy on that path, so its
// engine, flybys and effects are its own, heard from where its player is.
// Hits: a machine counts only its own shooters' (its player; the host also
// its bots), and sends each as "C3UH" (side, hit points, rocket); the others
// take them off their copy too. "C3UD": shot down, for anyone who missed one.
namespace
{
    // 0.5.276: version 2: every packet ends with this machine's id and a
    // count (8 bytes): 275's arrived twice on the friend's machine (sent to
    // both his addresses), so every hit counted double there (his copy of
    // the second plane went down to one rocket).
    constexpr uint8_t PlaneNetVersion = 3;   // 0.5.285: C3KS entries 4 bytes (the mortars)
    struct PlanePacket { char kind; std::vector<uint8_t> data; };
    std::mutex g_planeNetLock;
    std::vector<PlanePacket> g_planeNetIn;
    void PutF(std::vector<uint8_t>& v, float f) { uint8_t b[4]; memcpy(b, &f, 4); v.insert(v.end(), b, b + 4); }
    float GetF(const uint8_t* p) { float f; memcpy(&f, p, 4); return f; }
    uint32_t PlaneNetId()
    {
        static const uint32_t id = uint32_t(GetTickCount64() * 2654435761ull) ^ GetCurrentProcessId() * 40503u ^ 0x5A17C3D1u;
        return id;
    }
    void PlaneNetSend(std::vector<uint8_t> v)
    {
        static std::atomic<uint32_t> count{0};
        const uint32_t id = PlaneNetId(), n = ++count;
        for (int i = 0; i < 4; i++) v.push_back(uint8_t(id >> (24 - 8 * i)));
        for (int i = 0; i < 4; i++) v.push_back(uint8_t(n >> (24 - 8 * i)));
        Net::SideSend(v.data(), int(v.size()));
    }
}
void MpFixesSendPlane(const UavPlaneAsk& ask)
{
    std::vector<uint8_t> v = { 'C', '3', 'U', 'P', PlaneNetVersion, uint8_t(ask.side), uint8_t(ask.seconds >> 8), uint8_t(ask.seconds) };
    PutF(v, ask.cx); PutF(v, ask.cy); PutF(v, ask.cz); PutF(v, ask.radius); PutF(v, ask.angle);
    PlaneNetSend(v);
    printf("uav plane: told the other machines (side %d, %d s)\n", ask.side, ask.seconds);
    fflush(stdout);
}
void MpFixesSendPlaneHit(int side, int hit, bool rocket, bool down)
{
    PlaneNetSend({ 'C', '3', 'U', uint8_t(down ? 'D' : 'H'), PlaneNetVersion, uint8_t(side), uint8_t(uint16_t(hit) >> 8), uint8_t(hit), uint8_t(rocket ? 1 : 0), 0 });
}
// The side channel's thread: queued for the frame, each packet once.
// "C3U?" (the plane) and, 0.5.276, "C3K?" (killstreaks: kinds 's', 'u').
void MpFixesOnPlanePacket(const uint8_t* packet, int size, unsigned int from)
{
    (void)from;
    if (size < 14 || packet[4] != PlaneNetVersion) return;
    const uint32_t id = uint32_t(packet[size - 8]) << 24 | uint32_t(packet[size - 7]) << 16 | uint32_t(packet[size - 6]) << 8 | packet[size - 5];
    const uint32_t n = uint32_t(packet[size - 4]) << 24 | uint32_t(packet[size - 3]) << 16 | uint32_t(packet[size - 2]) << 8 | packet[size - 1];
    if (id == PlaneNetId()) return;   // our own, back to us
    static std::deque<uint64_t> seen;
    const uint64_t key = uint64_t(id) << 32 | n;
    std::lock_guard<std::mutex> hold(g_planeNetLock);
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) return;
    seen.push_back(key);
    if (seen.size() > 512) seen.pop_front();
    const char kind = packet[2] == 'K' || packet[2] == 'M' ? char(tolower(packet[3])) : char(packet[3]);   // 0.5.285: C3M? the mortar ('f' a round, 'p' its tube), C3KM 'm'
    if (g_planeNetIn.size() < 64) g_planeNetIn.push_back({ kind, std::vector<uint8_t>(packet, packet + size - 8) });
}

// The UAV called by a player (his entity): where the plane will circle, from
// where the players are.
void MpFixesUavPlane(uint8_t* base, uint32_t caller, int seconds, bool enemy = false)
{
    const uint32_t team = caller ? Guest::Read32(base, caller + 520) : 0;
    int side = team ? int(Guest::Read32(base, team + 4)) : 1;
    if (side != 1 && side != 2) side = 1;
    if (enemy) side = 3 - side;   // 0.5.257: streak_give enemyuav (a plane to shoot at)
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    float sx = 0, sy = 0, sz = 0, top = -1e9f;
    int n = 0;
    std::vector<std::pair<float, float>> at;
    for (uint32_t i = 0; entities && i < MpCapClients; i++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
        const uint32_t ps = e ? Guest::Read32(base, e + 512) : 0;
        if (ps < 0x40000000u || ps >= 0x90000000u) continue;
        // 0.5.254: a player's own origin is his state's (+512 +0..+8, as the
        // set-origin call writes it); +336 read 0 (253 circled 0 -104).
        const float x = PlaneReadF(base, ps), y = PlaneReadF(base, ps + 4), z = PlaneReadF(base, ps + 8);
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || fabsf(x) > 60000 || fabsf(y) > 60000) continue;
        sx += x; sy += y; top = std::max(top, z); sz += z; n++;   // 0.5.264: sz (mean height; the top was a noclip flyer's)
        at.push_back({ x, y });
    }
    if (n == 0) { printf("uav plane: no players to circle over\n"); fflush(stdout); return; }
    UavPlaneAsk ask;
    ask.side = side; ask.seconds = seconds;
    ask.cx = sx / n; ask.cy = sy / n;
    float spread = 0;
    for (const auto& p : at) spread = std::max(spread, sqrtf((p.first - ask.cx) * (p.first - ask.cx) + (p.second - ask.cy) * (p.second - ask.cy)));
    ask.radius = std::clamp(spread * 0.7f, 1500.0f, 4000.0f);
    ask.cz = sz / float(n) + 1400.0f;   // 0.5.264: the players' mean height, not the highest (Justin in noclip: higher each call)
    (void)top;
    // 0.5.257: round the whole ground covered on this map (Justin: it only
    // circled part of the map), its middle and 90% of its larger half-width.
    // 0.5.264: round the whole map: the box of the map's spawn points (every
    // entity whose class name has "spawn" in it: they cover where play can
    // go), else the ground the players have covered.
    {
        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        int spawns = 0;
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = Guest::Read32(base, 0x82BD1FC0 + 8 * i);
            if (e < 0x40000000u || e >= 0x90000000u) continue;
            const uint32_t cls = Guest::Read32(base, e + 548);
            if (cls < 0x40000000u || cls >= 0x90000000u) continue;
            char name[40] = {};
            for (int c = 0; c < 39; c++) { name[c] = char(base[cls + 12 + c]); if (!name[c]) break; }
            if (!strstr(name, "spawn")) continue;
            const float x = PlaneReadF(base, e + 336), y = PlaneReadF(base, e + 340);
            if (!std::isfinite(x) || !std::isfinite(y) || fabsf(x) > 60000 || fabsf(y) > 60000) continue;
            minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y);
            spawns++;
        }
        if (spawns >= 4 && maxX > minX + 500.0f && maxY > minY + 500.0f)
        {
            ask.cx = (minX + maxX) * 0.5f;
            ask.cy = (minY + maxY) * 0.5f;
            ask.radius = std::clamp(std::max(maxX - minX, maxY - minY) * 0.5f * 0.98f, 2500.0f, 12000.0f);   // 0.5.280: 2% in from the edge (Justin)
            printf("uav plane: %d spawn points span %.0f..%.0f x %.0f..%.0f: circle %.0f\n", spawns, minX, maxX, minY, maxY, ask.radius);
        }
        else if (g_playMaxX > g_playMinX + 500.0f && g_playMaxY > g_playMinY + 500.0f)
        {
            ask.cx = (g_playMinX + g_playMaxX) * 0.5f;
            ask.cy = (g_playMinY + g_playMaxY) * 0.5f;
            ask.radius = std::clamp(std::max(g_playMaxX - g_playMinX, g_playMaxY - g_playMinY) * 0.45f, 2500.0f, 9000.0f);
        }
        else ask.radius = std::max(ask.radius, 2500.0f);
    }
    ask.angle = float(GetTickCount64() % 6283) / 1000.0f;   // 0.5.275: chosen here, sent to the others
    MpFixesSendPlane(ask);
    std::lock_guard<std::mutex> hold(g_uavPlaneLock);
    g_uavPlaneAsks.push_back(ask);
}

// 0.5.265: a plane sound waiting for the game's next weapon sound event.
std::atomic<uint32_t> g_planeSoundEnt{0};
std::atomic<const char*> g_planeSoundAlias{nullptr};
void MpFixesQueuePlaneSound(uint32_t ent, const char* alias)
{
    g_planeSoundEnt.store(ent);
    g_planeSoundQueued.store(GetTickCount64());
    g_planeSoundAlias.store(alias);
}

// 0.5.264: the plane's sound where plane_sound says.
namespace
{
    void PlaneSoundChosen(PPCContext& ctx, uint8_t* base, const UavPlane& p, const char* flyby)
    {
        const int where = g_planeSoundWhere.load(), which = g_planeSoundWhich.load();
        const char* alias = which == 1 ? "ARPL_P47_1" : which == 2 ? "FLAG_FLAPPING" : which == 3 ? "LEE_ENFIELD_FIRE_3RD" : which == 4 && g_planeSoundCustom.load() ? g_planeSoundCustom.load() : flyby;
        const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
        uint32_t ent = 0;
        if (where == 0) ent = PlaneEntity(base, p.handle);
        if (where == 1) ent = PlaneEntity(base, p.sound);
        if (where == 2) ent = own;
        if (where == 3 && list && own)
        {
            const uint32_t ps = Guest::Read32(base, own + 512);
            float best = 1e18f;
            for (uint32_t i = 1; i < MpCapClients; i++)
            {
                const uint32_t e = Guest::Read32(base, list + 4 * (i + 1));
                const uint32_t eps = e ? Guest::Read32(base, e + 512) : 0;
                if (!ps || eps < 0x40000000u || eps >= 0x90000000u) continue;
                const float dx = PlaneReadF(base, eps) - PlaneReadF(base, ps), dy = PlaneReadF(base, eps + 4) - PlaneReadF(base, ps + 4);
                if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; ent = e; }
            }
        }
        // 0.5.265: not played from here (the server frame: every way tried
        // from here was silent, 258-264) but from inside the game's own
        // weapon sound events (CampaignEventName), the way the Lee-Enfield's
        // scoped shot is played and heard: the next event plays it.
        MpFixesQueuePlaneSound(ent, alias);
        static int told = 0;
        if ((told < 30 && told++ < 30)) { printf("uav plane: sound %s at %s entity %08X\n", alias, where == 0 ? "the plane" : where == 1 ? "the origin" : where == 2 ? "my player" : "the nearest bot", ent); fflush(stdout); }
    }
}

// 0.5.258: shots at the plane. 257's plane was never hit (no "hit for"
// lines: bullets do not reach a script_model this way), so each player's
// shots are seen here, by his clip going down (the weapon in hand +164, its
// def from 0x82BCBF9C, the clip at record +4*(172 + def+1468)), and each
// shot is a line from his eyes along his view (record +204 pitch, +208 yaw,
// degrees, pitch down positive) tested against a ball round the plane.
// Rockets (bazooka, panzerschreck) count as 150 (x2 in MpFixesPlaneHit),
// other shots 40.
void MpFixesPlaneShots(PPCContext& ctx, uint8_t* base)
{
    static uint32_t lastHeld[32] = {}, lastClip[32] = {}, lastRecord[32] = {};
    // 0.5.262: a rocket's hit lands when the rocket would reach the plane
    // (about 1500 units a second), not as it is fired (Justin: no hit
    // marker seen for the panzerschreck: it showed at the moment of firing).
    struct RocketHit { int k; uint32_t attacker; uint64_t at; };
    static std::vector<RocketHit> rockets;
    {
        const uint64_t t = GetTickCount64();
        for (size_t n = 0; n < rockets.size();)
        {
            if (t < rockets[n].at) { n++; continue; }
            const RocketHit r = rockets[n];
            rockets.erase(rockets.begin() + long(n));
            const uint32_t plane = PlaneEntity(base, g_uavPlanes[r.k].handle);
            if (plane) MpFixesPlaneHit(ctx, base, plane, r.attacker, 150);
        }
    }
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    if (!entities || !list) return;
    constexpr float Rad = 0.01745329252f, HitRadius = 380.0f;
    for (uint32_t i = 0; i < MpCapClients && i < 32; i++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
        const uint32_t ps = e ? Guest::Read32(base, e + 512) : 0;
        if (ps < 0x40000000u || ps >= 0x90000000u) { lastRecord[i] = 0; continue; }
        const uint32_t held = Guest::Read32(base, ps + 164);
        const uint32_t def = (held && held < 256) ? Guest::Read32(base, list + 4 * held) : 0;
        if (def < 0x40000000u || def >= 0xA0000000u) { lastRecord[i] = 0; continue; }
        const uint32_t index = Guest::Read32(base, def + 1468);
        if (index >= 128) { lastRecord[i] = 0; continue; }
        const uint32_t clip = Guest::Read32(base, ps + 4 * (172 + index));
        const bool same = lastRecord[i] == ps && lastHeld[i] == held;
        const uint32_t before = lastClip[i];
        lastRecord[i] = ps; lastHeld[i] = held; lastClip[i] = clip;
        if (!same || clip >= before || before - clip > 5) continue;
        const int shots = int(before - clip);
        const uint32_t team = Guest::Read32(base, e + 520);
        const int side = team ? int(Guest::Read32(base, team + 4)) : 0;
        float pitch = PlaneReadF(base, ps + 204), yaw = PlaneReadF(base, ps + 208);
        if (pitch > 180.0f) pitch -= 360.0f;
        const float dx = cosf(pitch * Rad) * cosf(yaw * Rad), dy = cosf(pitch * Rad) * sinf(yaw * Rad), dz = -sinf(pitch * Rad);
        const float ex = PlaneReadF(base, ps), ey = PlaneReadF(base, ps + 4), ez = PlaneReadF(base, ps + 8) + 60.0f;
        const std::string name = ReadText(base, Guest::Read32(base, def + 8), 32);
        const bool rocket = name.find("bazooka") != std::string::npos || name.find("panzerschreck") != std::string::npos;
        for (int k = 0; k < 2; k++)
        {
            UavPlane& p = g_uavPlanes[k];
            if (p.phase < 1 || p.phase > 3 || side == k + 1) continue;
            const uint32_t plane = PlaneEntity(base, p.handle);
            if (!plane) continue;
            const float vx = PlaneReadF(base, plane + 336) - ex, vy = PlaneReadF(base, plane + 340) - ey, vz = PlaneReadF(base, plane + 344) - ez;
            const float t = vx * dx + vy * dy + vz * dz;
            if (t < 0 || t > 20000.0f) continue;
            const float miss2 = vx * vx + vy * vy + vz * vz - t * t;
            static int told = 0;
            if ((told < 40 && told++ < 40)) { printf("uav plane: client %u shot %d (%s) at side %d's plane: %.0f away, missed by %.0f\n", i, shots, name.c_str(), k + 1, t, sqrtf(std::max(miss2, 0.0f))); fflush(stdout); }
            if (miss2 > HitRadius * HitRadius) continue;
            if (rocket) rockets.push_back({ k, e, GetTickCount64() + uint64_t(t / 1.5f) });   // 0.5.262
            else for (int n = 0; n < shots; n++) MpFixesPlaneHit(ctx, base, plane, e, 40);
        }
    }
}

// 0.5.276: killstreaks for a joiner. Kills are counted where they happen:
// the host's world runs the bots, so the host knows when a joiner earns a
// UAV (275: "client 1 earned the UAV" on the host only; d-pad right did
// nothing on the friend's machine). The host sends "C3KS" every second:
// each other person's session number, his kills in a row and UAVs waiting;
// that person's machine takes them as its own player's. Calling one there
// sends "C3KU" (his number) and the host takes it off his count.
bool BotsIsBotEntity(uint8_t* base, uint32_t entity);
bool BotsThisMachineHostsBots(uint8_t* base);
int BotsSessionNumberOfSlot(uint8_t* base, uint32_t slot);   // bots.cpp (0.5.276)
int BotsLocalSessionNumber(uint8_t* base);
namespace { uint64_t g_streakUsedAt = 0; }
void MpFixesStreakSyncOut(uint8_t* base)
{
    static uint64_t last = 0;
    const uint64_t now = GetTickCount64();
    if (now - last < 1000) return;
    last = now;
    if (!BotsThisMachineHostsBots(base) || BotsLocalSessionNumber(base) == 255) return;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    if (!entities) return;
    std::vector<uint8_t> v = { 'C', '3', 'K', 'S', PlaneNetVersion, 0 };
    int count = 0;
    for (uint32_t i = 1; i < MpCapClients && count < 8; i++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
        if (!e || BotsIsBotEntity(base, e)) continue;
        const int number = BotsSessionNumberOfSlot(base, i);
        if (number < 0 || number >= 255) continue;
        v.push_back(uint8_t(number)); v.push_back(uint8_t(std::clamp(g_streaks[i].kills, 0, 255))); v.push_back(uint8_t(std::clamp(g_streaks[i].uavs, 0, 255)));
        v.push_back(uint8_t(std::clamp(g_streaks[i].mortars, 0, 255)));   // 0.5.285
        count++;
    }
    if (count == 0) return;
    v[5] = uint8_t(count);
    PlaneNetSend(v);
}
void MpFixesStreakSyncIn(uint8_t* base, const uint8_t* d, size_t n)
{
    const int mine = BotsLocalSessionNumber(base);
    if (mine == 255 || BotsThisMachineHostsBots(base)) return;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const int own = entities ? ClientOf(base, Guest::Read32(base, entities + 4)) : -1;
    if (own < 0) return;
    for (size_t k = 0; k < d[5] && 6 + 4 * k + 3 < n; k++)
    {
        const uint8_t* e = d + 6 + 4 * k;
        if (e[0] != mine) continue;
        Streak& s = g_streaks[own];
        s.kills = e[1];
        if (GetTickCount64() < g_streakUsedAt + 2500) return;   // our call is still on its way to the host
        if (e[2] > s.uavs) { Overlay::Notice("UAV EARNED  -  d-pad right to call it", 3000); printf("killstreak: the host says we earned a UAV (%d waiting)\n", e[2]); fflush(stdout); }
        s.uavs = e[2];
        if (e[3] > s.mortars) { Overlay::Notice("MORTAR EARNED  -  d-pad down to pick, d-pad right to set it up", 3000); printf("killstreak: the host says we earned a mortar (%d waiting)\n", e[3]); fflush(stdout); }
        s.mortars = e[3];   // 0.5.285
        return;
    }
}
void MpFixesStreakUsedIn(uint8_t* base, int number)
{
    if (!BotsThisMachineHostsBots(base)) return;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    for (uint32_t i = 1; entities && i < MpCapClients; i++)
        if (BotsSessionNumberOfSlot(base, i) == number)
        {
            if (g_streaks[i].uavs > 0) g_streaks[i].uavs--;
            printf("killstreak: client %u (number %d) called his UAV on his machine (%d left)\n", i, number, g_streaks[i].uavs);
            fflush(stdout);
            return;
        }
}
// A joiner's own UAV called: the host takes it off his count.
void MpFixesStreakUsedOut(uint8_t* base)
{
    if (BotsThisMachineHostsBots(base)) return;
    const int mine = BotsLocalSessionNumber(base);
    if (mine == 255) return;
    g_streakUsedAt = GetTickCount64();
    PlaneNetSend({ 'C', '3', 'K', 'U', PlaneNetVersion, uint8_t(mine) });
}

void MpFixesMortarPacket(PPCContext& ctx, uint8_t* base, char kind, const uint8_t* d, size_t n);   // 0.5.285 (with the killstreak box)
void MpFixesMortarFrame(PPCContext& ctx, uint8_t* base);
// Each server frame on the host (MpFixesLauncher).
void MpFixesPlaneFrame(PPCContext& ctx, uint8_t* base)
{
    const std::string map = Kernel::CurrentMap();
    if (map != g_uavPlaneMap)
    {
        g_uavPlaneMap = map;
        for (UavPlane& p : g_uavPlanes) p = UavPlane{};
        g_playMinX = g_playMinY = g_playTop = 1e9f; g_playMaxX = g_playMaxY = -1e9f; g_playTop = -1e9f;
    }
    if (map.empty()) return;
    {
        static uint64_t sampled = 0;
        const uint64_t t = GetTickCount64();
        if (t >= sampled)
        {
            sampled = t + 1000;
            const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
            for (uint32_t i = 0; entities && i < MpCapClients; i++)
            {
                const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
                const uint32_t ps = e ? Guest::Read32(base, e + 512) : 0;
                if (ps < 0x40000000u || ps >= 0x90000000u) continue;
                const float x = PlaneReadF(base, ps), y = PlaneReadF(base, ps + 4), z = PlaneReadF(base, ps + 8);
                if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || fabsf(x) > 60000 || fabsf(y) > 60000 || (x == 0 && y == 0)) continue;
                g_playMinX = std::min(g_playMinX, x); g_playMaxX = std::max(g_playMaxX, x);
                g_playMinY = std::min(g_playMinY, y); g_playMaxY = std::max(g_playMaxY, y);
                g_playTop = std::max(g_playTop, z);
            }
        }
    }
    MpFixesStreakSyncOut(base);   // 0.5.276: the host's count of each joiner's UAVs
    // 0.5.275: what the other machines sent (MpFixesOnPlanePacket).
    {
        std::vector<PlanePacket> in;
        {
            std::lock_guard<std::mutex> hold(g_planeNetLock);
            in.swap(g_planeNetIn);
        }
        for (const PlanePacket& pk : in)
        {
            const uint8_t* d = pk.data.data();
            const size_t n = pk.data.size();
            if (pk.kind == 'P' && n >= 28)
            {
                UavPlaneAsk a;
                a.side = d[5]; a.seconds = d[6] << 8 | d[7];
                a.cx = GetF(d + 8); a.cy = GetF(d + 12); a.cz = GetF(d + 16); a.radius = GetF(d + 20); a.angle = GetF(d + 24);
                a.remote = true;
                if ((a.side == 1 || a.side == 2) && std::isfinite(a.cx) && std::isfinite(a.radius))
                {
                    printf("uav plane: side %d's plane from another machine (%d s)\n", a.side, a.seconds);
                    fflush(stdout);
                    std::lock_guard<std::mutex> hold(g_uavPlaneLock);
                    g_uavPlaneAsks.push_back(a);
                }
            }
            else if (pk.kind == 's' && n >= 6)
                MpFixesStreakSyncIn(base, d, n);   // 0.5.276
            else if (pk.kind == 'u' && n >= 6)
                MpFixesStreakUsedIn(base, d[5]);   // 0.5.276
            else if (pk.kind == 'f' || pk.kind == 'p' || pk.kind == 'm')
                MpFixesMortarPacket(ctx, base, pk.kind, d, n);   // 0.5.285
            else if ((pk.kind == 'H' || pk.kind == 'D') && n >= 9 && (d[5] == 1 || d[5] == 2))
            {
                const UavPlane& q = g_uavPlanes[d[5] - 1];
                const uint32_t plane = q.handle ? PlaneEntity(base, q.handle) : 0;
                const int hit = pk.kind == 'D' ? 99999 : (d[6] << 8 | d[7]);
                if (plane) MpFixesPlaneHit(ctx, base, plane, 0, hit, true);
            }
        }
    }
    MpFixesMortarFrame(ctx, base);   // 0.5.285
    std::vector<UavPlaneAsk> asks;
    {
        std::lock_guard<std::mutex> hold(g_uavPlaneLock);
        asks.swap(g_uavPlaneAsks);
    }
    const uint64_t now = GetTickCount64();
    constexpr float Pi = 3.14159265f;
    for (const UavPlaneAsk& ask : asks)
    {
        UavPlane& p = g_uavPlanes[ask.side == 2 ? 1 : 0];
        p.cx = ask.cx; p.cy = ask.cy; p.cz = ask.cz; p.radius = ask.radius;
        p.ground = ask.cz - 1400.0f;   // 0.5.260
        // 0.5.263: Poissons' sky is low (a town): seen only when it came
        // down to ~300 over the highest player (260); 1400 everywhere else.
        // 0.5.279: 900 (Justin: very low at 400). 259's unseen plane there
        // was the Stuka's missing model (found 268-271), not its height.
        if (map == "poissons") p.cz = p.ground + 1600.0f;   // 0.5.280: +700 more (Justin: still low at 900)
        if (map == "mp_fuel") p.cz = p.ground + 1400.0f * 1.11f;   // 0.5.290: Fuel Plant (Justin): 11% higher (289's 7% + 4%); circle back to the whole map (289's x0.92 off)
        p.until = now + 4000 + uint64_t(ask.seconds) * 1000;
        if ((p.phase == 1 || p.phase == 2) && PlaneEntity(base, p.handle)) { printf("uav plane: side %d's plane is up already: its time is renewed\n", ask.side); fflush(stdout); continue; }
        p.angle = ask.angle >= 0.0f ? ask.angle : float(now % 6283) / 1000.0f;   // a different way in each time (0.5.275: the caller's)
        const float ex = p.cx + cosf(p.angle - 0.6f) * (p.radius + 9000.0f), ey = p.cy + sinf(p.angle - 0.6f) * (p.radius + 9000.0f);
        if (!PlaneEntity(base, p.handle))
        {
            if (!MpFixesPlaneModelLoaded(ask.side)) { printf("uav plane: side %d's plane is not loaded on this map\n", ask.side); fflush(stdout); continue; }
            p.handle = PlaneSpawn(ctx, base, ask.side, ex, ey, p.cz + 600.0f);
        }
        if (!p.handle) continue;
        // From far off to the circle's first point in 4 seconds.
        const uint32_t ent = PlaneEntity(base, p.handle);
        // 0.5.257: it can be hit: G_Damage (sub_825862F8) returns at once
        // unless the target's +608 (takedamage) is set; the hits are counted
        // by MpFixesPlaneHit, not the game.
        if (ent) Guest::Write32(base, ent + 608, 1);
        p.health = kPlaneHealth;
        if (ent) { PlaneWriteF(base, ent + 336, ex); PlaneWriteF(base, ent + 340, ey); PlaneWriteF(base, ent + 344, p.cz + 600.0f); }
        const float tx = p.cx + cosf(p.angle) * p.radius, ty = p.cy + sinf(p.angle) * p.radius;
        const float yaw = atan2f(ty - ey, tx - ex) * 180.0f / Pi;
        PlaneRotateTo(ctx, base, p.handle, 0.0f, yaw + kPlaneYawOffset, 0.0f, 0.2f);
        PlaneMoveTo(ctx, base, p.handle, tx, ty, p.cz, 4.0f);
        p.phase = 1;
        p.next = now + 4000;
        printf("uav plane: side %d flying in over %.0f %.0f at height %.0f, circle %.0f, for %d s\n", ask.side, p.cx, p.cy, p.cz, p.radius, ask.seconds);
        fflush(stdout);
    }
    if (g_uavPlanes[0].phase || g_uavPlanes[1].phase) MpFixesPlaneShots(ctx, base);   // 0.5.258
    if (g_planeSoundNow.exchange(false))   // 0.5.264: plane_sound in the console
    {
        const UavPlane& any = g_uavPlanes[g_uavPlanes[1].phase ? 1 : 0];
        PlaneSoundChosen(ctx, base, any, "ARPL_P47_By01");
    }
    PlaneModelFrame(ctx, base);   // 0.5.267: plane_model
    // 0.5.266: a queued sound no weapon event has taken in 300 ms (no bots,
    // nobody shooting: 265's test) or asked for "now": played from here.
    if (g_planeSoundAlias.load() && (g_planeSoundDirect.exchange(false) || GetTickCount64() > g_planeSoundQueued.load() + 300))
        if (const char* alias = g_planeSoundAlias.exchange(nullptr))
        {
            PlaneSound(ctx, base, g_planeSoundEnt.load(), alias);
            static int said = 0;
            if ((said < 40 && said++ < 40)) { printf("uav plane: %s played from the frame at entity %08X\n", alias, g_planeSoundEnt.load()); fflush(stdout); }
        }
    for (int k = 0; k < 2; k++)
    {
        UavPlane& p = g_uavPlanes[k];
        // 0.5.258: a flyby every 3 s while it is up (257's loop was silent).
        {
            static uint64_t nextSound[2] = {};
            static int which[2] = {};
            if (p.phase >= 1 && p.phase <= 3 && now >= nextSound[k])
            {
                static const char* const flybys[] = { "ARPL_P47_By01", "ARPL_P47_By02", "ARPL_P47_By03", "ARPL_P47_By04", "ARPL_P47_By06", "ARPL_P47_By07" };
                // 0.5.259: on the sound origin (258's on the plane itself were
                // never heard: the plane is thousands of units off, past the
                // sounds' reach); within 9000 of the player only.
                // 0.5.261: on the host's own player while the plane is within
                // 6000 of him (the way the port's heard sounds are played).
                // 0.5.263: as the map's flags play theirs (the script's
                // playsound, sub_824CF5F8 with r5 0: sub_82556868 at the
                // entity, r6 0, r7 -1, r8 0): on the sound origin, a script
                // entity like a flag (261 on the host's own player: silent).
                // 0.5.264: where and which from plane_sound (default: the
                // sound origin, flybys).
                // 0.5.272: a flyby only as it passes near (within 3500), at
                // most one each 9 s; the engine below the rest of the time
                // (Justin: the flybys alone sounded like many planes).
                if (p.dist < 3500.0f)
                {
                    const uint32_t at = PlaneEntity(base, p.sound);
                    if (at) PlaneSound(ctx, base, at, flybys[which[k]++ % 6]);
                    nextSound[k] = now + 9000;
                }
                else nextSound[k] = now + 250;
            }
            // 0.5.273: shot down: the crash, as it hits the ground (2.5 s
            // into its dive), a mortar round's explosion (Mortar_Explo, the
            // campaign's Forest/Laison banks, loaded beside every map) and a
            // shell's, on the sound origin toward it.
            if (g_planeCrashAt[k] && now >= g_planeCrashAt[k])
            {
                g_planeCrashAt[k] = 0;
                const uint32_t at = p.sound ? PlaneEntity(base, p.sound) : 0;
                if (at)
                {
                    PlaneSound(ctx, base, at, "UAV:Mortar_Explo");
                    PlaneSound(ctx, base, at, "UAV:SHELL_EXPLO_MAINIMP");
                }
                PlaneFx(ctx, base, g_planeCrashX[k], g_planeCrashY[k], g_planeCrashZ[k], "panzerschreck", 56);   // 0.5.274
                PlaneFx(ctx, base, g_planeCrashX[k], g_planeCrashY[k], g_planeCrashZ[k], "fraggrenade", 56);
                PlaneFx(ctx, base, g_planeCrashX[k], g_planeCrashY[k], g_planeCrashZ[k] + 50.0f, "smokegrenade", 56);
                printf("uav plane: side %d crashed (sound at %08X)\n", k + 1, at);
                fflush(stdout);
            }
            // 0.5.272: the engine, steady: plane_engine <cue> <ms> (default
            // ARPL_P47_1, again every g_planeEngineEvery ms; 0 = once, for a
            // cue that loops by itself), on the sound origin, which follows
            // toward the plane (and is parked with it after).
            {
                static uint64_t nextEngine[2] = {};
                static bool on[2] = {};
                const uint32_t at = p.sound ? PlaneEntity(base, p.sound) : 0;
                if (p.phase >= 1 && p.phase <= 3 && at)
                {
                    const int every = g_planeEngineEvery.load();
                    if (!on[k] || (every > 0 && now >= nextEngine[k]) || g_planeEngineNow.exchange(false))
                    {
                        on[k] = true;
                        nextEngine[k] = now + uint64_t(std::max(every, 500));
                        const char* cue = g_planeEngineCue.load();
                        PlaneSound(ctx, base, at, cue ? cue : "ARPL_P47_2");
                        static int said = 0;
                        if ((said < 20 && said++ < 20)) { printf("uav plane: engine %s (again every %d ms)\n", cue ? cue : "ARPL_P47_2", every); fflush(stdout); }
                    }
                }
                else if (p.phase == 0) on[k] = false;
            }
            // 0.5.259: the sound origin: from the host's player's eyes toward
            // the plane, at most 900 units out, so the flyby is heard from
            // the plane's direction and grows as it comes near.
            static uint64_t nextMove[2] = {};
            if (p.phase >= 1 && p.phase <= 3 && now >= nextMove[k])
            {
                nextMove[k] = now + 100;
                const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
                const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
                const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
                const uint32_t plane = PlaneEntity(base, p.handle);
                if (ps >= 0x40000000u && ps < 0x90000000u && plane)
                {
                    const float ex = PlaneReadF(base, ps), ey = PlaneReadF(base, ps + 4), ez = PlaneReadF(base, ps + 8) + 60.0f;
                    const float vx = PlaneReadF(base, plane + 336) - ex, vy = PlaneReadF(base, plane + 340) - ey, vz = PlaneReadF(base, plane + 344) - ez;
                    const float d = sqrtf(vx * vx + vy * vy + vz * vz);
                    p.dist = d;
                    // 0.5.262: 900 out while the plane is within 3000; past that
                    // further out with it, so the engine fades with distance.
                    const float out = d < 3000.0f ? std::min(d, 900.0f) : 900.0f + (d - 3000.0f) * 0.6f;
                    const float f = d > 1.0f ? out / d : 0.0f;
                    if (!p.sound || !PlaneEntity(base, p.sound))
                    {
                        p.sound = PlaneSpawnOrigin(ctx, base, ex + vx * f, ey + vy * f, ez + vz * f);
                        // 0.5.262: the engine as a looped sound on it, the way
                        // the map's flags flap (FLAG_FLAPPING: the script's
                        // playloopsound sub_824CF5F8 from the server frame,
                        // heard): entity, alias, a non-zero word (kept at
                        // +888 while it loops). The origin stays with the
                        // player, toward the plane; parked far below after.
                        if (false && p.sound)   // 0.5.263: off: r5 is not a flag (the flags' script passes 0); 262's 1 silenced other sounds
                        {
                            const uint32_t slot = PlaneScratch() + 0x1A0, text = PlaneScratch() + 0x1C0;
                            memset(base + text, 0, 0x20);
                            memcpy(base + text, "ARPL_P47_1", 11);
                            Call3(ctx, base, MakeText, slot, text, 0);
                            PPCContext call = ctx;
                            call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
                            call.r3.u64 = p.sound; call.r4.u64 = slot; call.r5.u64 = 1; call.r6.u64 = 0; call.r7.u64 = 0; call.lr = 0;
                            Guest::Lookup(0x824CF5F8)(call, base);
                            Call3(ctx, base, FreeText, slot, 0, 0);
                            printf("uav plane: engine loop ARPL_P47_1 on the sound origin (%d)\n", int32_t(call.r3.u32));
                            fflush(stdout);
                        }
                    }
                    else PlaneMoveTo(ctx, base, p.sound, ex + vx * f, ey + vy * f, ez + vz * f, 0.1f);
                }
            }
        }
        if (p.phase == 0 || now < p.next) continue;
        if (!PlaneEntity(base, p.handle)) { printf("uav plane: side %d's plane is gone\n", k + 1); fflush(stdout); p = UavPlane{}; continue; }
        {
            // 0.5.256: where the entity really is (255: Justin saw no plane;
            // the moves were sent, the result never looked at).
            static uint64_t told[2] = {};
            if (now >= told[k])
            {
                told[k] = now + 2000;
                const uint32_t e = PlaneEntity(base, p.handle);
                // 0.5.260: Poissons (259): the plane flew its circle but was
                // never seen; its +16 had 0x40000000 there (0 on Merville,
                // where it is seen): taken as "outside the map's world" (the
                // town's sky is low). Brought down 250 at a time, to 300
                // above the highest player at least, till the bit clears.
                // 0.5.263: off: the bit is set on Merville too since 259 (the
                // plane was then brought down into the trees and lost there).
                if (false && p.phase == 2 && (Guest::Read32(base, e + 16) & 0x40000000u) && p.cz > p.ground + 300.0f)
                {
                    p.cz = std::max(p.ground + 300.0f, p.cz - 250.0f);
                    printf("uav plane: side %d out of the world at that height: down to %.0f\n", k + 1, p.cz);
                }
                printf("uav plane: side %d phase %d at %.0f %.0f %.0f (angles %.0f %.0f %.0f; +16 %08X +20 %08X +620 %08X), aiming at the circle %.0f %.0f %.0f\n", k + 1, p.phase,
                       PlaneReadF(base, e + 336), PlaneReadF(base, e + 340), PlaneReadF(base, e + 344),
                       PlaneReadF(base, e + 352), PlaneReadF(base, e + 356), PlaneReadF(base, e + 360),
                       Guest::Read32(base, e + 16), Guest::Read32(base, e + 20), Guest::Read32(base, e + 620),
                       p.cx + cosf(p.angle) * p.radius, p.cy + sinf(p.angle) * p.radius, p.cz);
                fflush(stdout);
            }
        }
        if (p.phase == 1 || p.phase == 2)
        {
            if (now >= p.until)
            {
                // Off along the circle's tangent, far away, in 6 seconds.
                const float dx = -sinf(p.angle), dy = cosf(p.angle);
                const float x = p.cx + cosf(p.angle) * p.radius + dx * 14000.0f, y = p.cy + sinf(p.angle) * p.radius + dy * 14000.0f;
                PlaneRotateTo(ctx, base, p.handle, -8.0f, atan2f(dy, dx) * 180.0f / Pi + kPlaneYawOffset, 0.0f, 1.0f);
                PlaneMoveTo(ctx, base, p.handle, x, y, p.cz + 1500.0f, 6.0f);
                p.phase = 3;
                p.next = now + 6000;
                printf("uav plane: side %d flying off\n", k + 1);
                fflush(stdout);
                continue;
            }
            // About 1100 units a second round the circle, counter-clockwise.
            const float step = float(kPlaneStepMs) / 1000.0f;
            p.angle += kPlaneSpeed * step / std::max(p.radius, 500.0f);
            const float x = p.cx + cosf(p.angle) * p.radius, y = p.cy + sinf(p.angle) * p.radius;
            const float yaw = (p.angle + Pi * 0.5f) * 180.0f / Pi;
            PlaneMoveTo(ctx, base, p.handle, x, y, p.cz, step);
            PlaneRotateTo(ctx, base, p.handle, 0.0f, fmodf(yaw, 360.0f) + kPlaneYawOffset, -kPlaneBank, p.phase == 1 ? 0.8f : step);
            p.phase = 2;
            p.next = now + kPlaneStepMs;
            continue;
        }
        // Flown off: parked far under the map till the next UAV.
        PlaneMoveTo(ctx, base, p.handle, p.cx, p.cy, -60000.0f, 0.05f);
        if (p.sound && PlaneEntity(base, p.sound)) PlaneMoveTo(ctx, base, p.sound, p.cx, p.cy, -60000.0f, 0.05f);   // 0.5.262: its engine with it
        p.phase = 0;
        printf("uav plane: side %d parked\n", k + 1);
        fflush(stdout);
    }
}

// The artillery barrage twice as long for a killstreak's. The scout's
// barrage (sub_821F70A8: the caller's handle at r3, then two values passed
// on to each shell's thread) starts its shells' threads and returns; for a
// player whose barrage is a killstreak's, the same barrage is started again
// once the first has fallen (barrage_repeat_ms after, 8 seconds).
namespace
{
    struct Repeat { uint32_t handle = 0; uint64_t r4 = 0, r5 = 0, at = 0; };
    Repeat g_repeat;
    bool g_repeating = false;
    // The Typhoon's run (0.5.77): the barrage's target is a point passed by
    // value (r4 its x and y, the high half of r5 its z). A run is five
    // strikes one after another along the line from the caller to the spot
    // he marked, 300 units apart and centred on it, 0.9 seconds between.
    std::vector<Repeat> g_strafe;

    float BitsF(uint32_t w) { float f; memcpy(&f, &w, 4); return f; }
    uint32_t FBits(float f) { uint32_t w; memcpy(&w, &f, 4); return w; }
}
bool MpFixesStreakBarrage(uint32_t handle);
extern "C" PPC_FUNC(__imp__sub_821F70A8);
PPC_FUNC(sub_821F70A8)
{
    const uint32_t handle = ctx.r3.u32 ? Guest::Read32(base, ctx.r3.u32) : 0;
    static int said = 0;
    if ((said < 40 && said++ < 40))
    {
        printf("killstreak: artillery barrage for handle %08X (r4 %016llX r5 %016llX: %.0f %.0f %.0f)%s\n", handle,
               (unsigned long long)ctx.r4.u64, (unsigned long long)ctx.r5.u64, BitsF(uint32_t(ctx.r4.u64 >> 32)),
               BitsF(uint32_t(ctx.r4.u64)), BitsF(uint32_t(ctx.r5.u64 >> 32)), g_repeating ? ", one we started" : "");
        fflush(stdout);
    }
    if (!g_repeating && handle)
    {
        int i = 0;
        while (i < int(MpCapClients) && g_streaks[i].handle != handle) i++;
        Streak* s = i < int(MpCapClients) ? &g_streaks[i] : nullptr;
        if (s && s->armed) s->fired = true;   // the call is spent when its barrage comes, not before
        if (s && s->typhoonCalled && s->typhoons > 0)
        {
            // A Typhoon run: the strikes along the line, no second half.
            s->typhoons--;
            s->typhoonCalled = false;
            const float tx = BitsF(uint32_t(ctx.r4.u64 >> 32)), ty = BitsF(uint32_t(ctx.r4.u64)), tz = BitsF(uint32_t(ctx.r5.u64 >> 32));
            const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
            const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * uint32_t(i + 1)) : 0;
            float dx = 1, dy = 0;
            if (entity)
            {
                dx = tx - BitsF(Guest::Read32(base, entity + 336));
                dy = ty - BitsF(Guest::Read32(base, entity + 340));
                const float length = sqrtf(dx * dx + dy * dy);
                if (length > 1) { dx /= length; dy /= length; } else { dx = 1; dy = 0; }
            }
            const uint64_t now = GetTickCount64();
            for (int k = 1; k < 5; k++)
            {
                const float along = (float(k) - 2.0f) * 300.0f;
                const uint64_t r4 = (uint64_t(FBits(tx + dx * along)) << 32) | FBits(ty + dy * along);
                g_strafe.push_back({ handle, r4, ctx.r5.u64, now + uint64_t(k) * 900 });
            }
            // The first strike is this call itself, moved to the start of the line.
            ctx.r4.u64 = (uint64_t(FBits(tx - dx * 600.0f)) << 32) | FBits(ty - dy * 600.0f);
            printf("killstreak: client %d's Typhoon run at %.0f %.0f %.0f, heading %.2f %.2f\n", i, tx, ty, tz, dx, dy);
            fflush(stdout);
            const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
            if (own && own == entity) Overlay::Notice("TYPHOON INBOUND  -  rockets along your mark", 4000);
        }
        else if (MpFixesStreakBarrage(handle))
            g_repeat = { handle, ctx.r4.u64, ctx.r5.u64, GetTickCount64() + 8000 };
    }
    __imp__sub_821F70A8(ctx, base);
}
void MpFixesBarrageFrame(PPCContext& ctx, uint8_t* base)
{
    static uint32_t word = 0;
    if (!word) word = Guest::AllocatePhysical(0x10);
    auto start = [&](const Repeat& r)
    {
        Guest::Write32(base, word, r.handle);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = word; call.r4.u64 = r.r4; call.r5.u64 = r.r5; call.lr = 0;
        g_repeating = true;
        Guest::Lookup(0x821F70A8)(call, base);
        g_repeating = false;
    };
    const uint64_t now = GetTickCount64();
    for (size_t k = 0; k < g_strafe.size();)
    {
        if (now < g_strafe[k].at) { k++; continue; }
        const Repeat r = g_strafe[k];
        g_strafe.erase(g_strafe.begin() + k);
        start(r);
        printf("killstreak: a Typhoon strike at %.0f %.0f\n", BitsF(uint32_t(r.r4 >> 32)), BitsF(uint32_t(r.r4)));
        fflush(stdout);
    }
    if (!g_repeat.handle || now < g_repeat.at) return;
    const Repeat r = g_repeat;
    g_repeat = Repeat{};
    start(r);
    printf("killstreak: the barrage's second half started\n");
    fflush(stdout);
}

namespace { int MortarKills(); }   // 0.5.285 (with the killstreak box)
extern "C" PPC_FUNC(__imp__sub_825862F8);
PPC_FUNC(sub_825862F8)
{
    const uint32_t target = ctx.r3.u32, attacker = ctx.r5.u32;
    if (target && MpFixesPlaneHit(ctx, base, target, attacker, int32_t(ctx.r8.u32))) return;   // 0.5.257: the UAV plane
    const int32_t before = target ? int32_t(Guest::Read32(base, target + 776)) : 0;
    g_damageSeen = true;
    __imp__sub_825862F8(ctx, base);
    // 0.5.282: a kill teaches the bots a lookout when it was a long one.
    if (target && before > 0 && int32_t(Guest::Read32(base, target + 776)) <= 0) BotsLearnKill(base, attacker, target);
    // 0.5.331 (Justin): the class's artillery (the scout's, Rusher 2's; about
    // 130 s to recharge) comes faster with every kill: each kill takes time
    // off the killer's wait, more for each kill in a row this life: 10 s,
    // then 15, 20, 25, 30 s at most. Anyone's: people's and bots'. A wait
    // set by the killstreaks (an hour, parked) is left alone.
    if (target && before > 0 && int32_t(Guest::Read32(base, target + 776)) <= 0)
    {
        static int inARow[32] = {};
        const int victim = ClientOf(base, target), killer = ClientOf(base, attacker);
        if (victim >= 0 && victim < 32) inARow[victim] = 0;
        const uint32_t handle = (killer >= 0 && killer != victim) ? Guest::Read32(base, attacker + 484) : 0;
        const uint32_t ready = handle ? ReadySlot(ctx, base, handle) : 0;
        const uint32_t now = ready ? GameTime(ctx, base) : 0;
        if (killer >= 0 && killer < 32 && killer != victim && !ready)
        {
            static int told = 0;
            if ((told < 20 && told++ < 20)) { printf("artillery: client %d's kill: he has no class artillery (no dispenser)\n", killer); fflush(stdout); }
        }
        if (ready && now && killer < 32)
        {
            inARow[killer]++;
            const uint32_t at = Guest::Read32(base, ready);
            const int32_t left = int32_t(at - now);
            {
                static int told = 0;   // 0.5.333: why a kill took nothing off (Justin played a scout in 332: none logged)
                if (!(at != 0 && left > 0 && left < 600000) && told++ < 40) { printf("artillery: client %d's kill (%d in a row): nothing to take off (ready time %08X, now %08X: %s)\n", killer, inARow[killer], at, now, at == 0 || left <= 0 ? "already ready" : "parked by a killstreak"); fflush(stdout); }
            }
            if (at != 0 && left > 0 && left < 600000)
            {
                const int32_t cut = std::min(left, std::min(30000, 10000 + 5000 * (inARow[killer] - 1)));
                Guest::Write32(base, ready, at - uint32_t(cut));
                printf("artillery: client %d's kill (%d in a row) takes %d s off his artillery's wait: %d s left\n", killer, inARow[killer], cut / 1000, (left - cut) / 1000);
                fflush(stdout);
                if (killer == ClientOf(base, Guest::Read32(base, Guest::Read32(base, 0x82BCC7FC) + 4)))
                    Overlay::Notice(left - cut <= 0 ? "ARTILLERY READY" : ("ARTILLERY  -" + std::to_string(cut / 1000) + " s  (" + std::to_string((left - cut) / 1000) + " s left)").c_str(), 2500);
            }
        }
    }
    if (!target || before <= 0 || !Settings::Get().killstreakArtillery) return;
    const int32_t after = int32_t(Guest::Read32(base, target + 776));
    if (after > 0) return;
    const int victim = ClientOf(base, target);
    if (victim < 0) return;
    Streak& dead = g_streaks[victim];
    dead.kills = 0;
    dead.alive = false;
    dead.ours = false;   // his dispenser ends with him
    const int killer = ClientOf(base, attacker);
    if (killer < 0 || killer == victim) return;
    Streak& s = g_streaks[killer];
    s.kills++;
    const Settings::Values values = Settings::Get();
    const bool mine = killer == ClientOf(base, Guest::Read32(base, Guest::Read32(base, 0x82BCC7FC) + 4));
    printf("killstreak: client %d killed client %d, %d in a row\n", killer, victim, s.kills);
    // The ladder: the UAV, then the artillery, then the Typhoon, each once a
    // life at its count (as Call of Duty 4's).
    if (s.kills == values.killstreakUav)
    {
        s.uavs++;
        printf("killstreak: client %d earned the UAV\n", killer);
        if (mine) Overlay::Notice("UAV EARNED  -  d-pad right to call it", 3000);
    }
    if (s.kills == MortarKills())   // 0.5.285
    {
        s.mortars++;
        printf("killstreak: client %d earned the mortar\n", killer);
        if (mine) Overlay::Notice("MORTAR EARNED  -  d-pad down to pick, d-pad right to set it up", 3000);
    }
    if (s.kills == values.killstreakKills)
    {
        s.charges++;
        printf("killstreak: client %d earned an artillery barrage (%d waiting)\n", killer, s.charges);
        if (mine) Overlay::Notice("ARTILLERY EARNED  -  d-pad down to pick, d-pad right to call", 3000);
    }
    if (s.kills == values.killstreakTyphoon)
    {
        s.charges++;
        s.typhoons++;
        printf("killstreak: client %d earned the Typhoon (%d strikes waiting, %d of them runs)\n", killer, s.charges, s.typhoons);
        if (mine) Overlay::Notice("TYPHOON EARNED  -  d-pad down to pick, d-pad right to call", 3000);
    }
    if (mine) Overlay::ConsolePrint("killstreak: " + std::to_string(s.kills) + " in a row");
    fflush(stdout);
}

// streak_give <uav|artillery|typhoon> in the console (0.5.79, for testing on
// the host): the host's own player is given that killstreak as if earned.
namespace { std::atomic<int> g_streakAsked{0}; }
void MpFixesGiveStreak(const std::string& kind)
{
    const int what = kind == "uav" ? 1 : kind == "artillery" ? 2 : kind == "typhoon" ? 3 : kind == "enemyuav" ? 4 : kind == "mortar" ? 5 : 0;
    if (!what) { Overlay::ConsolePrint("streak_give: uav, artillery, typhoon, mortar or enemyuav"); return; }
    g_streakAsked.store(what);
}
void MpFixesGiveStreakFrame(uint8_t* base)
{
    const int what = g_streakAsked.exchange(0);
    if (!what) return;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const int i = entities ? ClientOf(base, Guest::Read32(base, entities + 4)) : -1;
    if (i < 0) { Overlay::ConsolePrint("streak_give: you are not in a game"); return; }
    if (what == 4)
    {
        // 0.5.257: the other side's UAV plane over the map now, to shoot at.
        MpFixesUavPlane(base, Guest::Read32(base, entities + 4), Settings::Get().uavSeconds, true);
        Overlay::Notice("ENEMY UAV INBOUND (test)", 2000);
        printf("killstreak: streak_give enemyuav\n");
        fflush(stdout);
        return;
    }
    Streak& s = g_streaks[i];
    if (what == 5)   // 0.5.285
    {
        s.mortars++;
        printf("killstreak: streak_give mortar for client %d (%d waiting)\n", i, s.mortars);
        fflush(stdout);
        Overlay::Notice("MORTAR GIVEN", 2000);
        return;
    }
    if (what == 1) s.uavs++;
    if (what >= 2) s.charges++;
    if (what == 3) s.typhoons++;
    printf("killstreak: streak_give %d for client %d (%d strikes waiting, %d runs)\n", what, i, s.charges, s.typhoons);
    fflush(stdout);
    Overlay::Notice(what == 3 ? "TYPHOON GIVEN" : what == 2 ? "ARTILLERY GIVEN" : "UAV GIVEN", 2000);
}

// Each frame on the host: strikes earned are made ready, called ones taken off.
void MpFixesKangarooTreads(uint8_t* base);
void MpFixesKillstreaks(PPCContext& ctx, uint8_t* base)
{
    MpFixesKangarooTreads(base);
    MpFixesUavFrame();
    MpFixesGiveStreakFrame(base);
    MpFixesBarrageFrame(ctx, base);
    if (!Settings::Get().killstreakArtillery) return;
    static uint32_t last = 0;
    const uint32_t ticks = GetTickCount();
    if (ticks - last < 250) return;
    last = ticks;
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    if (!entities) return;
    const uint32_t now = GameTime(ctx, base);
    if (!now) return;
    for (int i = 0; i < int(MpCapClients); i++)
    {
        Streak& s = g_streaks[i];
        const uint32_t entity = Guest::Read32(base, entities + 4 * uint32_t(i + 1));
        const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
        if (ps < 0x40000000u || ps >= 0x90000000u) { s = Streak{}; continue; }
        const uint32_t handle = Guest::Read32(base, entity + 484);
        if (handle != s.handle) { s = Streak{}; s.handle = handle; }
        if (int32_t(Guest::Read32(base, entity + 776)) > 0) s.alive = true;
        // 0.5.86: the other players' strikes (the host counts everyone's kills,
        // but only the host has the killstreak box): one earned is armed at
        // once, the spotter's binoculars given and put in his hands (every
        // other slot kept), and fired as the host's are.
        if (s.alive && !s.armed && s.charges > 0 && entity != Guest::Read32(base, entities + 4))
        {
            s.armed = true;
            s.typhoonCalled = s.typhoons > 0;
            uint8_t slots[10];
            for (int n = 0; n < 10; n++) slots[n] = Guest::Read8(base, ps + 1064 + n);
            OnWeaponFor(ctx, base, handle, 2664, "mp_binoculars");
            for (int n = 0; n < 10; n++) if (Guest::Read8(base, ps + 1064 + n) != slots[n]) Guest::Write8(base, ps + 1064 + n, slots[n]);
            OnWeaponFor(ctx, base, handle, 2692, "mp_binoculars");
            printf("killstreak: client %d's %s armed for him: the spotter's binoculars in his hands\n", i, s.typhoonCalled ? "Typhoon" : "artillery");
            fflush(stdout);
        }
        if (!s.alive || (!s.armed && !s.ours)) continue;
        uint32_t slot = ReadySlot(ctx, base, handle);
        if (s.armed && s.charges > 0 && !s.ours)
        {
            // 0.5.83: the dispenser started every time a strike is called from
            // the box. (Its ready time, the player's variable, outlives the
            // thread: 0.5.82 took a variable left from before for a scout's
            // running dispenser, started none, and the spotter's binoculars
            // came out with nothing to fire.)
            g_streakScout[i] = false;
            StartDispenser(ctx, base, handle);
            slot = ReadySlot(ctx, base, handle);
            s.ours = true;
            if (slot) Guest::Write32(base, slot, 0);
            printf("killstreak: client %d's artillery ready (%s, ready word %08X)\n", i, g_streakScout[i] ? "his own dispenser" : "dispenser started", slot);
            fflush(stdout);
        }
        if (!slot) continue;
        const uint32_t ready = Guest::Read32(base, slot);
        const bool waiting = ready != 0 && int32_t(ready - now) > 0;
        // 0.5.84: a call is spent only when its barrage comes (the barrage
        // hook sets fired). 0.5.83 took the dispenser's own first clock,
        // set as it starts, for the call: the charge went and the main
        // weapon came back before the binoculars were up.
        if (s.armed && s.fired)
        {
            s.charges--;
            s.armed = false;
            s.fired = false;
            printf("killstreak: client %d called his strike (%d more waiting)\n", i, s.charges);
            fflush(stdout);
            Guest::Write32(base, slot, now + 3600000u);
            if (entity == Guest::Read32(base, entities + 4)) g_streakBackToGun = true;
        }
        else if (s.armed && s.charges > 0)
        {
            // Armed: kept ready until fired, and a round kept in the spotter's
            // binoculars (0.5.83 set it once, as the switch began: the
            // binoculars came up with 0 and would not fire).
            if (ready != 0) Guest::Write32(base, slot, 0);
            // 0.5.85: written straight into his ammunition (the script's
            // clip call did not take for the binoculars): the weapon by its
            // name's hash (sub_820E10E0, sub_82480428), its clip's number
            // (+1468; the clips from +688 of the player's state) and its
            // ammunition's (+1460; from +320), as the launcher's rockets are.
            static uint32_t text = 0;
            static uint8_t spotter = 0;
            if (!text) text = Guest::AllocatePhysical(0x40);
            if (!spotter)
            {
                memset(base + text, 0, 0x40);
                memcpy(base + text, "mp_binoculars", 14);
                const uint32_t hash = Call3(ctx, base, 0x820E10E0, text, 0, 0);
                spotter = uint8_t(Call3(ctx, base, 0x82480428, hash, 0, 0));
                printf("killstreak: the spotter's binoculars are weapon %u\n", spotter);
                fflush(stdout);
            }
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t weapon = (spotter && list) ? Guest::Read32(base, list + 4 * spotter) : 0;
            if (weapon)
            {
                const uint32_t clip = Guest::Read32(base, weapon + 1468), ammo = Guest::Read32(base, weapon + 1460);
                if (clip > 0 && clip < 128 && Guest::Read32(base, ps + 4 * (clip + 172)) == 0) Guest::Write32(base, ps + 4 * (clip + 172), 1);
                if (ammo > 0 && ammo < 128 && Guest::Read32(base, ps + 4 * (ammo + 80)) == 0) Guest::Write32(base, ps + 4 * (ammo + 80), 1);
            }
        }
        else if (!s.armed && !g_streakScout[i] && !waiting) Guest::Write32(base, slot, now + 3600000u);
    }
}

// The killstreak box (0.5.82, as Call of Duty 4's and later): streaks earned
// are kept, not used at once. A box at the bottom right lists this machine's
// player's (the host's: the host counts), the one picked framed: d-pad down
// picks the next, d-pad right calls it. The UAV goes up at once; the
// artillery and the Typhoon take the spotter's binoculars out
// (mp_binoculars, which can mark a spot: the class binoculars,
// mp_binoculars_nospot, cannot, so a strike earned as a rifleman could not
// be called) and the strike falls where they are fired; then back to the
// main weapon.
// 0.5.285: THE MORTAR KILLSTREAK (Justin: "if you get 4 kills you get the
// mortar killstreak and you pull it out on dpad with the mortar model in
// front of you and using it as intended ... blast radius and impact damage
// wider and stronger ... the max amount of ammo they can have safely").
// Earned at MortarKills() in a row (4; COD3_MORTAR_KILLS), picked in the
// killstreak box and set up with d-pad right: the campaign's 81mm (v_81mm,
// Laison's vehicles.cod, loaded beside the map: MpFixesLoadMortarArchive)
// is put down in front of the player, who is held there (Input::SetMortar:
// no moving, jumping, crouching, reloading, grenades or triggers for the
// title). The tube turns with his view; looking up sends the round further
// (kMortarMinRange looking down, kMortarMaxRange looking up), a ring on the
// screen shows where it will land (the ground there, traced). RT fires a
// round (one each kMortarReloadMs, kMortarRounds in all: kept when put away
// with d-pad right, until used): Mortar_Slide and Mortar_Fire at the tube
// (the campaign's Forest/Laison banks, loaded beside every map: cue swap
// "UAV:"), the shell (v_81mm_shell, the campaign weapons archives) along
// its arc, Mortar_Miss as it comes down, then the bazooka's/panzerschreck's
// and the frag's explosions, Mortar_Explo, and the blast: full damage
// (kMortarDamage, a kill) within kMortarKillRadius, down to
// kMortarEdgeDamage at kMortarRadius (the frag's is about 256), a third
// through a wall; never the shooter or his side. MOD_MORTAR_SPLASH (14).
// Peer to peer, as the UAV plane: every shot goes to the other machines
// ("C3MF": shooter's session number, tube, landing point, flight time) and
// the tube's place while it is up ("C3MP"); each machine flies its own
// copy, effects and sounds heard from where its own player is; the damage
// is dealt by the host alone (his G_Damage counts the kills and streaks).
// A joiner's mortars are counted on the host (C3KS 4th byte) and taken off
// there when he sets one up (C3KM).
int BotsSideOf(uint8_t* base, uint32_t entity);   // bots.cpp (0.5.285)
bool BotsGroundZ(PPCContext& ctx, uint8_t* base, uint32_t ignore, float x, float y, float top, float& z);
bool BotsLineClear(PPCContext& ctx, uint8_t* base, uint32_t ignore, float ax, float ay, float az, float bx, float by, float bz);
bool MpFixesModelFound(PPCContext& ctx, uint8_t* base, const char* name);   // 0.5.285 (with the Stuka's)
int32_t MpFixesCampaignWeaponNumber(const char* name);   // 0.5.288 (below, the campaign guns)
namespace
{
    constexpr int kMortarRounds = 10;               // a round each 1.6 s: at most ~3 of one tube in the air
    constexpr float kMortarRadius = 560.0f;         // the blast's reach (a frag's is about 256)
    constexpr float kMortarKillRadius = 170.0f;     // full damage within
    constexpr int kMortarDamage = 400, kMortarEdgeDamage = 45;
    constexpr uint64_t kMortarReloadMs = 1600;
    constexpr float kMortarMinRange = 600.0f, kMortarMaxRange = 5000.0f;
    // 0.5.303: the model's facing against the aim, set live (mortar_yaw <deg>
    // in the console; 300 at 0 and 301 at -90 both stood sideways for Justin).
    std::atomic<float> g_mortarYawOffset{0.0f};   // 0.5.307: 0. 0.5.317: measured: the campaign's tube (e5_mortar) faces exactly the player's yaw
    float MortarYawOffset() { return g_mortarYawOffset.load(); }
    // 0.5.306: where the tube stands from the player (forward, right, up),
    // live: mortar_yaw <deg> <forward> <right> <up>. The campaign's turret
    // gives forward 0 right 0 (305's log: the player stands on the tube's
    // own spot, the eye 45 up), and the interaction draws the arms in the
    // tube's frame (sub_825198C0: the entity's matrix), so the tube goes
    // where the player stands.
    std::atomic<float> g_mortarOffF{48.2f}, g_mortarOffR{0.0f}, g_mortarOffU{0.0f};   // 0.5.317: MEASURED in the campaign (316's probe, Laison, e5_mortar): the tube 48.2 ahead of the player, 0 to the side; there it stands on a ledge 16.8 above his feet, here on the ground (his eye 16.8 higher over it: more of the tube's top, as Justin asked)
    // 0.5.307: crouched at the mortar (the campaign's eye is 45 up: the
    // turret's playerUpOffset), a tap of crouch at set-up and one at put-away.
    std::atomic<bool> g_mortarCrouch{false};   // 0.5.317: measured: the campaign player stands at the mortar (view height 66)
    // 0.5.309: the camera's offset at the mortar: the interaction's own
    // view offset (controller +100..+108, eased into +88..+96 each frame,
    // sub_8251A950; the state's def +952: 2 0 3), set by us each frame:
    // back and down a little (Justin: "come back and down a bit"). Live:
    // mortar_view <forward> <right> <up>.
    std::atomic<float> g_mortarViewF{37.8f}, g_mortarViewR{0.0f}, g_mortarViewU{-23.1f};   // 0.5.319: MEASURED from the screens: the campaign camera is not his eye: 318's screen (tube 3.9 deg wide, its top 10.8 deg down from his eye 48 behind the tube) against the campaign's (13.3 deg wide, 14.2 deg down): 3.4 times nearer, 15 behind the muzzle and 4 over it = 37.8 ahead of him and 42.9 over the ground (his eye is 66)   // 0.5.318: EXACTLY the campaign's eye over the tube: there the tube stands 16.8 above his feet (316's probe), here on the ground, so the eye comes down 16.8 (eye 49.2 over the tube's base, 48.2 behind it)   // 0.5.315: as the campaign (Justin's video of Laison): crouched eye about 45, looking level over the muzzle   // 0.5.314: lower still (Justin: more of the tube's top)   // 0.5.312: lower (Justin: more of the tube)
    // 0.5.311: the muzzle from the tube's spot (forward along its facing,
    // up), where the round is fed in and leaves: mortar_view's 4th and 5th.
    std::atomic<float> g_mortarMuzzleF{4.8f}, g_mortarMuzzleU{39.0f};   // 0.5.319: measured from 318's screen (the tube 3.7 across): the muzzle 4.8 ahead of the tube's spot, 39 up   // 0.5.317: from the campaign's view of it (centred, 14 deg below level at 48 ahead): about 10 further and 35 up from the tube's spot
    bool g_mortarCrouched = false;
    void MortarTubeSpot(float x, float y, float yaw, float& px, float& py)
    {
        const float rad = yaw * 3.14159265f / 180.0f;
        const float f = g_mortarOffF.load(), r = g_mortarOffR.load();
        px = x + cosf(rad) * f + sinf(rad) * r;
        py = y + sinf(rad) * f - cosf(rad) * r;
    }
    int MortarKills()
    {
        static const int kills = []() { const char* t = getenv("COD3_MORTAR_KILLS"); const int v = t ? atoi(t) : 4; return v > 0 ? v : 4; }();
        return kills;
    }

    struct MortarUse
    {
        bool active = false;
        int rounds = 0;            // left in this machine's player's mortar
        uint32_t prop = 0;         // the v_81mm script_model's handle (kept for the map)
        float ox = 0, oy = 0;      // where the player stood
        float px = 0, py = 0, pz = 0, propYaw = 1e9f, ground = 0;
        // 0.5.311: the round being fed into the tube (RT), launched when in.
        uint32_t feed = 0;          // its v_81mm_shell model (kept for the map)
        uint64_t feedAt = 0;        // 0: none being fed
        bool feedSliding = false;
        uint64_t reloopAt = 0;      // 0.5.313: the interaction set back to mortar_idle then
        float fx = 0, fy = 0, fz = 0, fyaw = 0, ftx = 0, fty = 0, ftz = 0;
        int fms = 0;
        uint64_t nextLook = 0;
        float tx = 0, ty = 0, tz = 0, range = 0;
        bool targetOk = false;
        uint64_t nextShot = 0, nextTrace = 0, nextTurn = 0, nextTell = 0, stopAt = 0;
    };
    MortarUse g_mortar;
    std::atomic<int> g_mortarAskStart{0};      // from the box (frame): 1 the rounds left, 2 a new mortar's
    std::atomic<bool> g_mortarAskStop{false};
    std::atomic<int> g_mortarLeft{0};          // rounds left while put away (the box)
    std::atomic<bool> g_mortarActive{false};
    struct MortarView { bool on = false; bool ahead = false; float fwd = 0, right = 0, up = 0; int rounds = 0, range = 0; bool ready = false; };
    std::mutex g_mortarViewLock;
    MortarView g_mortarView;

    struct MortarShell
    {
        bool on = false;
        uint32_t handle = 0;     // v_81mm_shell (kept, parked after)
        float sx = 0, sy = 0, sz = 0, tx = 0, ty = 0, tz = 0, apex = 0;
        uint64_t t0 = 0, t1 = 0, nextMove = 0;
        uint32_t attacker = 0;   // the shooter's entity on this machine (0: found by number at the blast)
        int number = 255;
        bool whistled = false;
    };
    MortarShell g_mortarShells[16];
    struct MortarProp { int number = -1; uint32_t handle = 0; uint64_t seen = 0; bool up = false; };
    MortarProp g_mortarProps[4];
    uint32_t g_mortarSounds[3] = {};
    int g_mortarSoundNext = 0;
    std::string g_mortarMap;
    int g_mortarModel = -1, g_mortarShellModel = -1;   // found by name on this map (-1 not looked yet)

    uint32_t MortarOwn(uint8_t* base) { const uint32_t e = Guest::Read32(base, 0x82BCC7FC); return e ? Guest::Read32(base, e + 4) : 0; }
    uint32_t MortarState(uint8_t* base, uint32_t e)
    {
        const uint32_t ps = e ? Guest::Read32(base, e + 512) : 0;
        return ps >= 0x40000000u && ps < 0x90000000u ? ps : 0;
    }
    bool MortarModel(PPCContext& ctx, uint8_t* base, bool shell)
    {
        int& known = shell ? g_mortarShellModel : g_mortarModel;
        if (known < 0)
        {
            known = MpFixesModelFound(ctx, base, shell ? "v_81mm_shell" : "v_81mm") ? 1 : 0;
            printf("mortar: %s %s by name\n", shell ? "v_81mm_shell" : "v_81mm", known ? "found" : "NOT found (not drawn)");
            fflush(stdout);
        }
        return known == 1;
    }

    // A sound at a point, as the plane's: on a script_origin from this
    // machine's player toward the point (900 out, further with distance).
    void MortarSoundAt(PPCContext& ctx, uint8_t* base, float x, float y, float z, const char* alias)
    {
        const uint32_t ps = MortarState(base, MortarOwn(base));
        if (!ps) return;
        const float ex = PlaneReadF(base, ps), ey = PlaneReadF(base, ps + 4), ez = PlaneReadF(base, ps + 8) + 60.0f;
        const float vx = x - ex, vy = y - ey, vz = z - ez;
        const float d = sqrtf(vx * vx + vy * vy + vz * vz);
        if (!std::isfinite(d) || d > 14000.0f) return;
        const float out = d < 3000.0f ? std::min(d, 900.0f) : 900.0f + (d - 3000.0f) * 0.6f;
        const float f = d > 1.0f ? out / d : 0.0f;
        const float sx = ex + vx * f, sy = ey + vy * f, sz = ez + vz * f;
        uint32_t& handle = g_mortarSounds[g_mortarSoundNext++ % 3];
        if (!handle || !PlaneEntity(base, handle)) handle = PlaneSpawnOrigin(ctx, base, sx, sy, sz);
        const uint32_t ent = handle ? PlaneEntity(base, handle) : 0;
        if (!ent) return;
        PlaneWriteF(base, ent + 336, sx); PlaneWriteF(base, ent + 340, sy); PlaneWriteF(base, ent + 344, sz);
        PlaneMoveTo(ctx, base, handle, sx, sy, sz, 0.05f);
        PlaneSound(ctx, base, ent, alias);
        static int said = 0;
        if ((said < 60 && said++ < 60)) { printf("mortar: sound %s, %.0f away\n", alias, d); fflush(stdout); }
    }

    void MortarPlace(PPCContext& ctx, uint8_t* base, uint32_t& handle, bool shell, float x, float y, float z, float pitch, float yaw)
    {
        if (!MortarModel(ctx, base, shell)) return;
        if (!handle || !PlaneEntity(base, handle)) handle = PlaneSpawn(ctx, base, 0, x, y, z, shell ? "v_81mm_shell" : "v_81mm");
        const uint32_t ent = handle ? PlaneEntity(base, handle) : 0;
        if (!ent) return;
        PlaneWriteF(base, ent + 336, x); PlaneWriteF(base, ent + 340, y); PlaneWriteF(base, ent + 344, z);
        PlaneMoveTo(ctx, base, handle, x, y, z, 0.05f);
        PlaneRotateTo(ctx, base, handle, pitch, yaw, 0.0f, 0.05f);
    }
    void MortarPark(PPCContext& ctx, uint8_t* base, uint32_t handle)
    {
        const uint32_t ent = handle ? PlaneEntity(base, handle) : 0;
        if (!ent) return;
        PlaneMoveTo(ctx, base, handle, PlaneReadF(base, ent + 336), PlaneReadF(base, ent + 340), PlaneReadF(base, ent + 344) - 9000.0f, 0.05f);
    }

    void MortarShellAt(const MortarShell& s, float f, float& x, float& y, float& z)
    {
        x = s.sx + (s.tx - s.sx) * f;
        y = s.sy + (s.ty - s.sy) * f;
        z = s.sz + (s.tz - s.sz) * f + 4.0f * s.apex * f * (1.0f - f);
    }

    // 0.5.291: whether the campaign mortar's weapons (Laison's) are set up on this map.
    bool MortarCampaignWeapons(uint8_t* base)
    {
        static uint32_t listSeen = 0;
        static int32_t countSeen = -1;
        static bool found = false;
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        if (list == listSeen && count == countSeen) return found;
        listSeen = list;
        countSeen = count;
        found = false;
        for (int32_t k = 1; list && k <= count && k < 64; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            const uint32_t text = def ? Guest::Read32(base, def + 8) : 0;
            if (text < 0x40000000u || text >= 0x90000000u) continue;
            char name[24] = {};
            for (int c = 0; c < 23; c++) { name[c] = char(base[text + c]); if (!name[c]) break; }
            if (strcmp(name, "mortar_shell") == 0) found = true;
        }
        printf("mortar: the campaign round (mortar_shell) %s on this map\n", found ? "is set up" : "is not set up: the bazooka's flash and blast stand in");
        fflush(stdout);
        return found;
    }

    // 0.5.303: the round's drawing set up (its weapon's info +124, as
    // MpFixesCampaignGuns does it): never held or played on before.
    bool MortarRoundReady(uint8_t* base)
    {
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        for (int32_t k = 1; list && k <= count && k < 64; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            const uint32_t text = def ? Guest::Read32(base, def + 8) : 0;
            if (text < 0x40000000u || text >= 0x90000000u) continue;
            char name[16] = {};
            for (int c = 0; c < 15; c++) { name[c] = char(base[text + c]); if (!name[c]) break; }
            if (strcmp(name, "mortar_shell") == 0) return Guest::Read8(base, 0x82C83810 + 192 * uint32_t(k) + 124) != 0;
        }
        return false;
    }

    uint32_t MortarShooter(uint8_t* base, int number)
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        if (!entities || number < 0 || number >= 255) return 0;
        for (uint32_t i = 0; i < MpCapClients; i++)
            if (BotsSessionNumberOfSlot(base, i) == number) return Guest::Read32(base, entities + 4 * (i + 1));
        return 0;
    }

    // The blast (the host only): every living player of the other side (or
    // anyone, with no sides) within kMortarRadius, the shooter never.
    void MortarBlast(PPCContext& ctx, uint8_t* base, float x, float y, float z, uint32_t attacker)
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        if (!entities) return;
        const uint32_t side = attacker ? uint32_t(BotsSideOf(base, attacker)) : 0;
        static uint32_t point = 0;
        if (!point) point = Guest::AllocatePhysical(0x20);
        int hits = 0;
        for (uint32_t i = 0; i < MpCapClients; i++)
        {
            const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
            const uint32_t ps = MortarState(base, e);
            if (!ps || e == attacker || int32_t(Guest::Read32(base, e + 776)) <= 0) continue;
            if (side && uint32_t(BotsSideOf(base, e)) == side) continue;
            const float ex = PlaneReadF(base, ps), ey = PlaneReadF(base, ps + 4), ez = PlaneReadF(base, ps + 8) + 36.0f;
            const float d = sqrtf((ex - x) * (ex - x) + (ey - y) * (ey - y) + (ez - z) * (ez - z));
            if (!std::isfinite(d) || d > kMortarRadius) continue;
            float damage = d <= kMortarKillRadius ? float(kMortarDamage)
                : float(kMortarDamage) - (d - kMortarKillRadius) / (kMortarRadius - kMortarKillRadius) * float(kMortarDamage - kMortarEdgeDamage);
            const bool clear = BotsLineClear(ctx, base, e, x, y, z + 24.0f, ex, ey, ez);
            if (!clear) damage *= 0.35f;
            const int amount = int(damage);
            if (amount < 1) continue;
            PlaneWriteF(base, point, x); PlaneWriteF(base, point + 4, y); PlaneWriteF(base, point + 8, z);
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
            Guest::Write32(base, call.r1.u32 + 84, 0);
            Guest::Write32(base, call.r1.u32 + 92, 0xFFFFFFFFu);
            // G_Damage: target, inflictor, attacker, direction, point, damage,
            // flags, means of death (MOD_MORTAR_SPLASH), then +84 0, +92 -1
            // (as the game's own callers, sub_8258A040).
            call.r3.u64 = e; call.r4.u64 = attacker; call.r5.u64 = attacker; call.r6.u64 = 0; call.r7.u64 = point;
            call.r8.u64 = uint32_t(amount); call.r9.u64 = 0; call.r10.u64 = 14; call.lr = 0;
            Guest::Lookup(0x825862F8)(call, base);
            hits++;
            printf("mortar: blast hit client %u, %.0f away%s: %d damage (health now %d)\n", i, d, clear ? "" : " (through cover)", amount,
                   int32_t(Guest::Read32(base, e + 776)));
        }
        printf("mortar: blast at %.0f %.0f %.0f by %08X: %d hit\n", x, y, z, attacker, hits);
        fflush(stdout);
    }

    uint32_t g_mortarFxPark = 0;      // 0.5.319: the launch effect's origin, put away at g_mortarFxParkAt
    uint64_t g_mortarFxParkAt = 0;
    void MortarLaunch(PPCContext& ctx, uint8_t* base, int number, uint32_t attacker, float sx, float sy, float sz, float tx, float ty, float tz, int ms)
    {
        MortarShell* s = nullptr;
        for (MortarShell& c : g_mortarShells) if (!c.on) { s = &c; break; }
        MortarSoundAt(ctx, base, sx, sy, sz, "UAV:Mortar_Slide");
        MortarSoundAt(ctx, base, sx, sy, sz, "UAV:Mortar_Fire");
        // 0.5.319: on an origin of its own, put away 1.5 s later: on the
        // shared one (PlaneFx) the campaign's launch glow and smoke stayed
        // at the muzzle until something else moved it (Justin, 318).
        if (MortarCampaignWeapons(base))
        {
            static uint32_t fx = 0;
            if (!fx || !PlaneEntity(base, fx)) fx = PlaneSpawnOrigin(ctx, base, sx, sy, sz + 10.0f);
            const uint32_t keep = g_planeFxHandle;
            g_planeFxHandle = fx;
            PlaneFx(ctx, base, sx, sy, sz + 10.0f, "mortar_81mm_turret", 1);   // 0.5.291: the campaign tube's flash (mortar_launch)
            g_planeFxHandle = keep;
            g_mortarFxPark = fx;
            g_mortarFxParkAt = GetTickCount64() + 1500;
        }
        else PlaneFx(ctx, base, sx, sy, sz + 10.0f, "bazooka", 1);   // the tube's flash
        if (!s) { printf("mortar: 16 rounds in the air already: this one left out\n"); fflush(stdout); return; }
        const uint32_t kept = s->handle;
        *s = MortarShell{};
        s->handle = kept;
        s->on = true;
        s->sx = sx; s->sy = sy; s->sz = sz; s->tx = tx; s->ty = ty; s->tz = tz;
        const float d = sqrtf((tx - sx) * (tx - sx) + (ty - sy) * (ty - sy));
        s->apex = 500.0f + d * 0.3f;
        s->t0 = GetTickCount64();
        s->t1 = s->t0 + uint64_t(std::clamp(ms, 800, 8000));
        s->attacker = attacker;
        s->number = number;
        const float yaw = atan2f(ty - sy, tx - sx) * 180.0f / 3.14159265f;
        MortarPlace(ctx, base, s->handle, true, sx, sy, sz, -70.0f, yaw);
        printf("mortar: round from %.0f %.0f %.0f to %.0f %.0f %.0f (%.0f out, %d ms, shooter %d %08X)\n", sx, sy, sz, tx, ty, tz, d, ms, number, attacker);
        fflush(stdout);
    }

    // 0.5.288: the gun put away at the mortar: the mortar's hands (a campaign
    // gun, "mortar_hands": the mortar's arms holding a round) given and taken
    // out, the weapon slots put back as the binoculars' are; the main weapon
    // back when it is put away.
    // 0.5.305: where the campaign stands the player at the mortar: the
    // turret weapon's (mortar_81mm_turret) playerForwardOffset/RightOffset/
    // UpOffset (def +1344/+1348/+1352, the weapon field table), the player's
    // place in the tube's frame. The arms' moves (w_arms_ei_Mortar*) are made
    // for that spot.
    bool MortarTurretOffsets(uint8_t* base, float& fwd, float& right, float& up, float& dist)
    {
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        for (int32_t k = 1; list && k <= count && k < 92; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            const uint32_t text = def ? Guest::Read32(base, def + 8) : 0;
            if (text < 0x40000000u || text >= 0x90000000u) continue;
            char name[24] = {};
            for (int c = 0; c < 23; c++) { name[c] = char(base[text + c]); if (!name[c]) break; }
            if (strcmp(name, "mortar_81mm_turret") != 0) continue;
            fwd = PlaneReadF(base, def + 1344);
            right = PlaneReadF(base, def + 1348);
            up = PlaneReadF(base, def + 1352);
            dist = PlaneReadF(base, def + 2196);
            return true;
        }
        return false;
    }

    bool g_laisonInteraction = false;   // 0.5.304
    bool MortarRoundReady(uint8_t* base);   // 0.5.310 (and above: the mortar interaction registered and set to mortar_idle on this map)
    void MortarHands(PPCContext& ctx, uint8_t* base, uint32_t ps, bool on)
    {
        if (!ps) return;
        // 0.5.302: the campaign's own: the round (mortar_shell, Laison's
        // weapon: its arms are the ones the mortar's moves are made for; the
        // campaign's mortar_start gives it, 300's log). Without it on this
        // map, the gun stays in hand.
        // 0.5.304: the multiplayer's own interaction arms, "interact" (what
        // planting a charge takes out: bare arms, its animation tree made by
        // the map like any of its weapons). 302/303 held the campaign's
        // mortar_shell: its arms' tree is the campaign's and crashed in the
        // animation code even set up for drawing (303).
        if (!g_laisonInteraction)
        {
            static int said = 0;
            if (on && said++ < 3) { printf("mortar: no campaign mortar interaction on this map: the gun stays in hand\n"); fflush(stdout); }
            return;
        }
        if (on)
        {
            uint8_t slots[10];
            for (int i = 0; i < 10; i++) slots[i] = Guest::Read8(base, ps + 1064 + i);
            // 0.5.310: the round in the multiplayer's arms when set up.
            const char* hands = g_mortarRoundArms && MortarRoundReady(base) ? "mortar_shell" : "interact";
            OnWeapon(ctx, base, 2664, hands);
            for (int i = 0; i < 10; i++) if (Guest::Read8(base, ps + 1064 + i) != slots[i]) Guest::Write8(base, ps + 1064 + i, slots[i]);
            OnWeapon(ctx, base, 2692, hands);
            printf("mortar: %s arms out (in hand %u)\n", hands, Guest::Read32(base, ps + 164));
        }
        else if (g_ownPrimary[0])
        {
            OnWeapon(ctx, base, 2692, g_ownPrimary);
            printf("mortar: hands away, back to %s\n", g_ownPrimary);
        }
        fflush(stdout);
    }

    // 0.5.300: the campaign mortar's own interaction (INTERACTIONFILE mortar
    // from Laison: states mortar_start -> mortar_idle (w_arms_ei_MortarIdle)
    // -> mortar_load (MortarLoad: w_arms_ei_Mortar, Mortar_Slide)), started
    // on this machine's player the way the game starts a vehicle's
    // (sub_82462C68 at a seat: sub_8251AE38(the local player's interaction
    // controller, *(0x82C37070 + 4 * local index); the entity; the
    // interaction's name; the global archive *(*(0x82BCC828) + 40))). The
    // entity: our v_81mm. COD3_MORTAR_INTERACT=0 leaves it out.
    bool g_mortarInteracting = false;
    void MortarInteraction(PPCContext& ctx, uint8_t* base, bool on)
    {
        // 0.5.315: OFF unless COD3_MORTAR_INTERACT=1. Justin's video of 314 (Merville):
        // at each shot the camera went somewhere else on the map (the round's
        // landing place, out under the map in white fog) for the whole loading
        // step, and the interaction never showed any arms in multiplayer (304-314).
        // Without it: the plain crouched view over the tube (as the campaign's
        // video), the round fed in by us, no camera jumps.
        static const bool off = []() { const char* t = getenv("COD3_MORTAR_INTERACT"); return t && t[0] == '0'; }();   // 0.5.318: on again, for its view offset only (controller +100): RT is never given to it, so its loading step (the camera jumps) never runs
        const uint32_t ctrl = Guest::Read32(base, 0x82C37070);
        if (off || ctrl < 0x40000000u || ctrl >= 0x90000000u)
        {
            if (on) { printf("mortar: no interaction (%s, controller %08X)\n", off ? "off: COD3_MORTAR_INTERACT=1 turns it on" : "no controller", ctrl); fflush(stdout); }
            return;
        }
        if (!on)
        {
            const uint32_t state = Guest::Read32(base, ctrl + 4), interaction = Guest::Read32(base, ctrl + 8);
            // 0.5.302: ended as the game ends one (sub_82519178(controller, 1):
            // the state's exit, +4/+8 cleared, the view and weapon put back;
            // 301 left it running: the camera went into the void after).
            if (state || interaction)
            {
                PPCContext call = ctx;
                call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
                call.r3.u64 = ctrl; call.r4.u64 = 1; call.lr = 0;
                Guest::Lookup(0x82519178)(call, base);
            }
            printf("mortar: interaction ended at put away (was state %08X, interaction %08X; now %08X %08X)\n", state, interaction,
                   Guest::Read32(base, ctrl + 4), Guest::Read32(base, ctrl + 8));
            fflush(stdout);
            g_mortarInteracting = false;
            Input::SetMortarPassFire(false);
            return;
        }
        if (!g_laisonInteraction) { printf("mortar: no campaign mortar interaction on this map\n"); fflush(stdout); return; }
        static uint32_t name = 0;
        if (!name) { name = Guest::AllocatePhysical(0x40); memset(base + name, 0, 0x40); memcpy(base + name, "mortar", 7); }
        const uint32_t ent = g_mortar.prop ? PlaneEntity(base, g_mortar.prop) : 0;
        const uint32_t globals = Guest::Read32(base, 0x82BCC828);
        const uint32_t archive = globals ? Guest::Read32(base, globals + 40) : 0;
        printf("mortar: interaction before: state %08X, interaction %08X\n", Guest::Read32(base, ctrl + 4), Guest::Read32(base, ctrl + 8));
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = ctrl; call.r4.u64 = ent; call.r5.u64 = name; call.r6.u64 = archive; call.lr = 0;
        Guest::Lookup(0x8251AE38)(call, base);
        const uint32_t state = Guest::Read32(base, ctrl + 4), interaction = Guest::Read32(base, ctrl + 8);
        g_mortarInteracting = state != 0;
        Input::SetMortarPassFire(false);   // 0.5.318: RT stays ours: the interaction stays in mortar_idle (its mortar_load sent the camera across the map, 314's video)
        printf("mortar: campaign interaction 'mortar' started on the tube %08X: state %08X, interaction %08X (%s)\n", ent, state, interaction,
               g_mortarInteracting ? "running: RT goes to it" : "did not start");
        fflush(stdout);
    }

    void MortarSendProp(bool on, float x, float y, float z, float yaw)
    {
        std::vector<uint8_t> v = { 'C', '3', 'M', 'P', PlaneNetVersion, uint8_t(BotsLocalSessionNumber(Guest::Base)), uint8_t(on ? 1 : 0) };
        PutF(v, x); PutF(v, y); PutF(v, z); PutF(v, yaw);
        PlaneNetSend(v);
    }

    void MortarStop(PPCContext& ctx, uint8_t* base, const char* why)
    {
        if (!g_mortar.active) return;
        g_mortar.active = false;
        g_mortarActive.store(false);
        if (g_mortar.feedAt) { g_mortar.feedAt = 0; g_mortar.rounds++; MortarPark(ctx, base, g_mortar.feed); }   // 0.5.311: a round being fed kept
        g_mortar.reloopAt = 0;   // 0.5.314: 313 restarted it right after the next set-up
        Input::SetMortar(false);
        if (g_mortarCrouched && strcmp(why, "died") != 0) Input::PressCrouch();   // 0.5.307: stood up again
        Kernel::QueueConsoleCommand("set cg_drawGun 1");   // 0.5.319
        g_mortarCrouched = false;
        MortarInteraction(ctx, base, false);   // 0.5.300 (0.5.302: ended first)
        MortarHands(ctx, base, MortarState(base, MortarOwn(base)), false);   // 0.5.288
        MortarPark(ctx, base, g_mortar.prop);
        MortarSendProp(false, 0, 0, 0, 0);
        g_mortarLeft.store(g_mortar.rounds);
        {
            std::lock_guard<std::mutex> hold(g_mortarViewLock);
            g_mortarView = MortarView{};
        }
        if (g_mortar.rounds <= 0) Overlay::Notice("MORTAR EMPTY", 2000);
        else Overlay::Notice("MORTAR PUT AWAY  -  " + std::to_string(g_mortar.rounds) + " rounds kept", 2000);
        printf("mortar: put away (%s), %d rounds left\n", why, g_mortar.rounds);
        fflush(stdout);
    }

    void MortarStart(PPCContext& ctx, uint8_t* base, uint32_t own, uint32_t ps, uint64_t now, bool fresh)
    {
        if (fresh || g_mortar.rounds <= 0) g_mortar.rounds = kMortarRounds;
        const float x = PlaneReadF(base, ps), y = PlaneReadF(base, ps + 4), z = PlaneReadF(base, ps + 8);
        const float yaw = PlaneReadF(base, ps + 208);
        const float rad = yaw * 3.14159265f / 180.0f;
        g_mortar.ox = x; g_mortar.oy = y;
        (void)rad;
        MortarTubeSpot(x, y, yaw, g_mortar.px, g_mortar.py);   // 0.5.306
        // 0.5.305: the tube put where the campaign's player stands from it:
        // the player at tube + fwd*forward + right*right (the tube's frame,
        // right = yaw - 90), so the tube at player - that.
        {
            float fwd = 0, right = 0, up = 0, dist = 0;
            const bool have = MortarTurretOffsets(base, fwd, right, up, dist);
            printf("mortar: turret player offsets %s: forward %.1f right %.1f up %.1f, positionDist %.1f\n", have ? "read" : "not found", fwd, right, up, dist);
            fflush(stdout);
        }
        float ground = z;
        // 0.5.333 (Justin, 332: "I can see my arms again and I'm standing to
        // the left of the mortar"): 332's log had the tube put down 58 above his
        // feet (on a ledge in front of him: the ground search took anything up
        // to 60 above), so his view sat low and off under it. Now only ground
        // within 20 above or 60 below his feet; else the tube at his feet' height.
        g_mortar.pz = BotsGroundZ(ctx, base, own, g_mortar.px, g_mortar.py, z + 20.0f, ground) && ground > z - 60.0f && ground < z + 20.0f ? ground : z;
        printf("mortar: ground under the tube %.1f, his feet %.1f: tube at %.1f\n", ground, z, g_mortar.pz); fflush(stdout);
        g_mortar.ground = g_mortar.pz;
        g_mortar.pz += g_mortarOffU.load();
        MortarPlace(ctx, base, g_mortar.prop, false, g_mortar.px, g_mortar.py, g_mortar.pz, 0.0f, yaw + MortarYawOffset());
        g_mortar.propYaw = yaw;
        MortarHands(ctx, base, ps, true);   // 0.5.288
        if (g_mortarCrouch.load()) { Input::PressCrouch(); g_mortarCrouched = true; }   // 0.5.307
        Kernel::QueueConsoleCommand("set cg_drawGun 0");   // 0.5.319: no arms at the mortar (318: the interaction's bare arms hung in view; the campaign shows none until a round goes in)
        MortarInteraction(ctx, base, true);   // 0.5.300
        g_mortar.active = true;
        g_mortar.stopAt = 0;
        g_mortar.nextShot = now + 700;
        g_mortar.nextTrace = 0;
        g_mortarActive.store(true);
        Input::SetMortar(true);
        Input::TakeMortarFires();
        MortarSendProp(true, g_mortar.px, g_mortar.py, g_mortar.pz, yaw);
        g_mortar.nextTell = now + 500;
        Overlay::Notice("MORTAR  -  look up/down for range, RT to fire, D-PAD RIGHT to put it away", 4000);
        printf("mortar: set up at %.0f %.0f %.0f facing %.0f (%d rounds, tube %08X)\n", g_mortar.px, g_mortar.py, g_mortar.pz, yaw, g_mortar.rounds, g_mortar.prop);
        fflush(stdout);
    }

    // This machine's player at his mortar: aim, fire, the screen's ring.
    void MortarUseFrame(PPCContext& ctx, uint8_t* base, uint64_t now)
    {
        const uint32_t own = MortarOwn(base);
        const uint32_t ps = MortarState(base, own);
        if (g_mortarAskStop.exchange(false)) MortarStop(ctx, base, "d-pad right");
        if (const int ask = g_mortarAskStart.exchange(0))
            if (!g_mortar.active && ps && int32_t(Guest::Read32(base, own + 776)) > 0) MortarStart(ctx, base, own, ps, now, ask == 2);
        if (!g_mortar.active) { Input::TakeMortarFires(); return; }
        if (!ps || int32_t(Guest::Read32(base, own + 776)) <= 0) { MortarStop(ctx, base, "died"); return; }
        const float x = PlaneReadF(base, ps), y = PlaneReadF(base, ps + 4), z = PlaneReadF(base, ps + 8);
        if ((x - g_mortar.ox) * (x - g_mortar.ox) + (y - g_mortar.oy) * (y - g_mortar.oy) > 150.0f * 150.0f) { MortarStop(ctx, base, "moved off it"); return; }
        float pitch = PlaneReadF(base, ps + 204);
        const float yaw = PlaneReadF(base, ps + 208);
        if (pitch > 180.0f) pitch -= 360.0f;
        constexpr float Rad = 3.14159265f / 180.0f;
        // Looking 30 down: the nearest; 50 up: the furthest.
        const float t = std::clamp((30.0f - pitch) / 80.0f, 0.0f, 1.0f);
        g_mortar.range = kMortarMinRange + (kMortarMaxRange - kMortarMinRange) * t;
        g_mortar.tx = g_mortar.px + cosf(yaw * Rad) * g_mortar.range;
        g_mortar.ty = g_mortar.py + sinf(yaw * Rad) * g_mortar.range;
        if (now >= g_mortar.nextTrace)
        {
            g_mortar.nextTrace = now + 120;
            float ground = z;
            g_mortar.targetOk = BotsGroundZ(ctx, base, own, g_mortar.tx, g_mortar.ty, std::max(z, g_playTop) + 2500.0f, ground);
            g_mortar.tz = g_mortar.targetOk ? ground : z;
        }
        if (now >= g_mortar.nextTurn && fabsf(yaw - g_mortar.propYaw) > 2.0f)
        {
            g_mortar.nextTurn = now + 150;
            g_mortar.propYaw = yaw;
            // 0.5.306: the tube turns about the player (its spot from him).
            MortarTubeSpot(g_mortar.ox, g_mortar.oy, yaw, g_mortar.px, g_mortar.py);
            g_mortar.pz = g_mortar.ground + g_mortarOffU.load();
            if (g_mortar.prop) PlaneMoveTo(ctx, base, g_mortar.prop, g_mortar.px, g_mortar.py, g_mortar.pz, 0.15f);
            if (g_mortar.prop) PlaneRotateTo(ctx, base, g_mortar.prop, 0.0f, yaw + MortarYawOffset(), 0.0f, 0.15f);
        }
        if (now >= g_mortar.nextTell)
        {
            g_mortar.nextTell = now + 500;
            MortarSendProp(true, g_mortar.px, g_mortar.py, g_mortar.pz, g_mortar.propYaw);
        }
        // 0.5.309: our camera offset, over the state's own, each frame.
        if (g_mortarInteracting)
            if (const uint32_t c = Guest::Read32(base, 0x82C37070); c && Guest::Read32(base, c + 4))
            {
                PlaneWriteF(base, c + 100, g_mortarViewF.load());
                PlaneWriteF(base, c + 104, g_mortarViewR.load());
                PlaneWriteF(base, c + 108, g_mortarViewU.load());
            }
        // 0.5.306: what the interaction does with the view and the arms
        // (controller +88 the view offset eased to +100; +304 the frame's
        // place when flag 64; the state def's +904/+940/+944/+948/+952).
        if (now >= g_mortar.nextLook)
        {
            g_mortar.nextLook = now + 1000;
            const uint32_t c = Guest::Read32(base, 0x82C37070);
            const uint32_t st = c ? Guest::Read32(base, c + 4) : 0;
            const uint32_t def = st >= 0x40000000u && st < 0x90000000u ? Guest::Read32(base, st + 28) : 0;
            const bool defOk = def >= 0x40000000u && def < 0x90000000u;
            if (c)
                printf("mortar look: flags %08X state %08X (%s) view %.1f %.1f %.1f -> %.1f %.1f %.1f, frame %.1f %.1f %.1f | def +848 '%s' +904 %d +940 %d +944 %.1f +948 %.1f +952 %.1f %.1f %.1f | eye %.1f %.1f %.1f tube %.1f %.1f %.1f\n",
                       Guest::Read32(base, c), st, defOk ? ReadText(base, def, 32).c_str() : "-",
                       PlaneReadF(base, c + 88), PlaneReadF(base, c + 92), PlaneReadF(base, c + 96),
                       PlaneReadF(base, c + 100), PlaneReadF(base, c + 104), PlaneReadF(base, c + 108),
                       PlaneReadF(base, c + 304), PlaneReadF(base, c + 308), PlaneReadF(base, c + 312),
                       defOk ? ReadText(base, def + 848, 32).c_str() : "-", defOk ? int32_t(Guest::Read32(base, def + 904)) : 0, defOk ? int32_t(Guest::Read32(base, def + 940)) : 0,
                       defOk ? PlaneReadF(base, def + 944) : 0.0f, defOk ? PlaneReadF(base, def + 948) : 0.0f,
                       defOk ? PlaneReadF(base, def + 952) : 0.0f, defOk ? PlaneReadF(base, def + 956) : 0.0f, defOk ? PlaneReadF(base, def + 960) : 0.0f,
                       x, y, z, g_mortar.px, g_mortar.py, g_mortar.pz);
            printf("mortar look: aim pitch %.1f yaw %.1f range %.0f target %.0f %.0f %.0f\n", pitch, yaw, g_mortar.range, g_mortar.tx, g_mortar.ty, g_mortar.tz);
            fflush(stdout);
        }
        const int fires = Input::TakeMortarFires();
        if (fires > 0 && g_mortar.rounds > 0 && now >= g_mortar.nextShot && !g_mortar.feedAt)
        {
            // 0.5.311: the round fed in first (held over the muzzle, then
            // slid in: Mortar_Slide), launched once in (MortarFeedFrame).
            g_mortar.rounds--;
            g_mortar.nextShot = now + kMortarReloadMs;
            const float a = g_mortar.propYaw * Rad;
            const float mf = g_mortarMuzzleF.load(), mu = g_mortarMuzzleU.load();
            g_mortar.fx = g_mortar.px + cosf(a) * mf; g_mortar.fy = g_mortar.py + sinf(a) * mf; g_mortar.fz = g_mortar.pz + mu;
            g_mortar.fyaw = g_mortar.propYaw;
            g_mortar.ftx = g_mortar.tx; g_mortar.fty = g_mortar.ty; g_mortar.ftz = g_mortar.tz;
            g_mortar.fms = int(2000.0f + g_mortar.range * 0.5f);
            g_mortar.feedAt = now;
            g_mortar.feedSliding = false;
            // 0.5.312: from the hands (low, in front of the eye, a little
            // right) up over the muzzle, then in: fed, not just appearing.
            const float hx = x + cosf(a) * 46.0f + sinf(a) * 14.0f, hy = y + sinf(a) * 46.0f - cosf(a) * 14.0f, hz = z + 33.0f;   // 0.5.319: from the right, below the camera (now 37.8 ahead, 42.9 up)   // 0.5.317: from the right, at hand height of a standing player (the campaign's arm comes in from the right)
            MortarPlace(ctx, base, g_mortar.feed, true, hx, hy, hz, -70.0f, g_mortar.fyaw);
            if (g_mortar.feed) PlaneMoveTo(ctx, base, g_mortar.feed, g_mortar.fx, g_mortar.fy, g_mortar.fz + 16.0f, 0.35f);
        }
        if (g_mortar.feedAt)
        {
            const uint64_t t = now - g_mortar.feedAt;
            if (!g_mortar.feedSliding && t >= 520)
            {
                g_mortar.feedSliding = true;
                if (g_mortar.feed) PlaneMoveTo(ctx, base, g_mortar.feed, g_mortar.fx, g_mortar.fy, g_mortar.fz - 14.0f, 0.3f);
            }
            if (t >= 850)
            {
                g_mortar.feedAt = 0;
                MortarPark(ctx, base, g_mortar.feed);
                const float sx = g_mortar.fx, sy = g_mortar.fy, sz = g_mortar.fz;
                const int ms = g_mortar.fms;
                const int number = BotsLocalSessionNumber(base);
                MortarLaunch(ctx, base, number, own, sx, sy, sz, g_mortar.ftx, g_mortar.fty, g_mortar.ftz, ms);
                std::vector<uint8_t> v = { 'C', '3', 'M', 'F', PlaneNetVersion, uint8_t(number) };
                PutF(v, sx); PutF(v, sy); PutF(v, sz); PutF(v, g_mortar.ftx); PutF(v, g_mortar.fty); PutF(v, g_mortar.ftz);
                v.push_back(uint8_t(ms >> 8)); v.push_back(uint8_t(ms));
                PlaneNetSend(v);
                if (g_mortar.rounds == 0) g_mortar.stopAt = now + 1500;
                // 0.5.319: no restart (318: the restarted interaction never took the view offset again)
            }
        }
        // 0.5.313: 312's log: after the first RT the interaction stays in
        // mortar_load for good (the campaign's MortarLoad waits for its own
        // round, never fired here; flags 4), and from then on the aim read
        // differently (Justin: the red ring flips after the first shot), and
        // later RTs play no loading. Once the round is out, the interaction
        // is ended and started again: back at mortar_idle, the next RT loads.
        if (g_mortar.reloopAt && now >= g_mortar.reloopAt)
        {
            g_mortar.reloopAt = 0;
            if (g_mortarInteracting && g_mortar.rounds > 0)
            {
                MortarInteraction(ctx, base, false);
                MortarInteraction(ctx, base, true);
            }
        }
        if (g_mortar.stopAt && now >= g_mortar.stopAt) { MortarStop(ctx, base, "empty"); return; }
        // The ring: the landing point in the view's own axes (pitch down
        // positive), for the overlay to put on the screen.
        const float ex = x, ey = y, ez = z + 60.0f;
        const float vx = g_mortar.tx - ex, vy = g_mortar.ty - ey, vz = g_mortar.tz - ez;
        const float cp = cosf(pitch * Rad), sp = sinf(pitch * Rad), cy = cosf(yaw * Rad), sy = sinf(yaw * Rad);
        MortarView view;
        view.on = true;
        view.fwd = vx * cp * cy + vy * cp * sy - vz * sp;
        view.right = vx * sy - vy * cy;
        view.up = vx * sp * cy + vy * sp * sy + vz * cp;
        view.ahead = view.fwd > 20.0f;
        view.rounds = g_mortar.rounds;
        view.range = int(g_mortar.range);
        view.ready = now >= g_mortar.nextShot && g_mortar.rounds > 0;
        std::lock_guard<std::mutex> hold(g_mortarViewLock);
        g_mortarView = view;
    }
}

// 0.5.303: mortar_yaw <degrees> in the console: the tube's facing against
// the aim, turned at once (Justin finds the right one).
void MpFixesMortarYaw(const std::string& args)
{
    // 0.5.306: mortar_yaw <deg> [forward right up]: the tube's spot too.
    float v = 0, f = g_mortarOffF.load(), r = g_mortarOffR.load(), u = g_mortarOffU.load(), c = g_mortarCrouch.load() ? 1.0f : 0.0f;
    const int got = sscanf(args.c_str(), "%f %f %f %f %f", &v, &f, &r, &u, &c);
    if (got >= 5) g_mortarCrouch.store(c != 0.0f);   // 0.5.307: crouch at the next set-up
    if (got >= 1) g_mortarYawOffset.store(v);
    if (got >= 2) g_mortarOffF.store(f);
    if (got >= 3) g_mortarOffR.store(r);
    if (got >= 4) g_mortarOffU.store(u);
    g_mortar.propYaw = 1e9f;
    char line[160];
    snprintf(line, sizeof line, "mortar_yaw: facing %.0f, tube forward %.0f right %.0f up %.0f, crouch %d (tell Claude these when it looks right)", g_mortarYawOffset.load(), g_mortarOffF.load(), g_mortarOffR.load(), g_mortarOffU.load(), g_mortarCrouch.load() ? 1 : 0);
    Overlay::ConsolePrint(line);
    printf("mortar: %s\n", line);
    fflush(stdout);
}

// 0.5.309: mortar_view <forward> <right> <up>: the camera at the mortar.
void MpFixesMortarView(const std::string& args)
{
    float f = g_mortarViewF.load(), r = g_mortarViewR.load(), u = g_mortarViewU.load(), mf = g_mortarMuzzleF.load(), mu = g_mortarMuzzleU.load();
    sscanf(args.c_str(), "%f %f %f %f %f", &f, &r, &u, &mf, &mu);
    g_mortarViewF.store(f); g_mortarViewR.store(r); g_mortarViewU.store(u);
    g_mortarMuzzleF.store(mf); g_mortarMuzzleU.store(mu);   // 0.5.311
    char line[200];
    snprintf(line, sizeof line, "mortar_view: forward %.0f right %.0f up %.0f, muzzle forward %.0f up %.0f (tell Claude these when it looks right)", f, r, u, mf, mu);
    Overlay::ConsolePrint(line);
    printf("mortar: %s\n", line);
    fflush(stdout);
}

void MpFixesMortarPacket(PPCContext& ctx, uint8_t* base, char kind, const uint8_t* d, size_t n)
{
    if (kind == 'f' && n >= 32)
    {
        const int ms = d[30] << 8 | d[31];
        const float sx = GetF(d + 6), sy = GetF(d + 10), sz = GetF(d + 14), tx = GetF(d + 18), ty = GetF(d + 22), tz = GetF(d + 26);
        if (!std::isfinite(sx) || !std::isfinite(tx) || !std::isfinite(tz) || fabsf(tx) > 60000 || fabsf(ty) > 60000) return;
        printf("mortar: a round from another machine (shooter %d)\n", d[5]);
        MortarLaunch(ctx, base, d[5], 0, sx, sy, sz, tx, ty, tz, ms);
    }
    else if (kind == 'p' && n >= 23)
    {
        const int number = d[5];
        const bool on = d[6] != 0;
        const float x = GetF(d + 7), y = GetF(d + 11), z = GetF(d + 15), yaw = GetF(d + 19);
        MortarProp* p = nullptr;
        for (MortarProp& c : g_mortarProps) if (c.number == number) { p = &c; break; }
        if (!p && on) for (MortarProp& c : g_mortarProps) if (!c.up) { p = &c; break; }
        if (!p) return;
        p->number = number;
        if (!on || !std::isfinite(x) || !std::isfinite(yaw))
        {
            if (p->up) { MortarPark(ctx, base, p->handle); printf("mortar: shooter %d's tube put away\n", number); fflush(stdout); }
            p->up = false;
            return;
        }
        if (!p->up) { printf("mortar: shooter %d set his tube up at %.0f %.0f %.0f\n", number, x, y, z); fflush(stdout); }
        MortarPlace(ctx, base, p->handle, false, x, y, z, 0.0f, yaw + MortarYawOffset());
        p->up = true;
        p->seen = GetTickCount64();
    }
    else if (kind == 'm' && n >= 6 && BotsThisMachineHostsBots(base))
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        for (uint32_t i = 1; entities && i < MpCapClients; i++)
            if (BotsSessionNumberOfSlot(base, i) == d[5])
            {
                if (g_streaks[i].mortars > 0) g_streaks[i].mortars--;
                printf("killstreak: client %u (number %d) set up his mortar on his machine (%d left)\n", i, d[5], g_streaks[i].mortars);
                fflush(stdout);
                return;
            }
    }
}

// A joiner's mortar set up: the host takes it off his count.
void MpFixesMortarUsedOut(uint8_t* base)
{
    if (BotsThisMachineHostsBots(base)) return;
    const int mine = BotsLocalSessionNumber(base);
    if (mine == 255) return;
    g_streakUsedAt = GetTickCount64();
    PlaneNetSend({ 'C', '3', 'K', 'M', PlaneNetVersion, uint8_t(mine) });
}

// Each server frame (MpFixesPlaneFrame), on every machine.
void MpFixesMortarFrame(PPCContext& ctx, uint8_t* base)
{
    const std::string map = Kernel::CurrentMap();
    if (map != g_mortarMap)
    {
        g_mortarMap = map;
        if (g_mortar.active) Input::SetMortar(false);
        g_mortar = MortarUse{};
        g_mortarActive.store(false);
        g_mortarLeft.store(0);
        for (MortarShell& s : g_mortarShells) s = MortarShell{};
        for (MortarProp& p : g_mortarProps) p = MortarProp{};
        for (uint32_t& h : g_mortarSounds) h = 0;
        g_mortarModel = g_mortarShellModel = -1;
        std::lock_guard<std::mutex> hold(g_mortarViewLock);
        g_mortarView = MortarView{};
    }
    if (map.empty()) return;
    const uint64_t now = GetTickCount64();
    if (g_mortarFxParkAt && now >= g_mortarFxParkAt) { g_mortarFxParkAt = 0; MortarPark(ctx, base, g_mortarFxPark); }   // 0.5.319
    MortarUseFrame(ctx, base, now);
    if (!g_mortar.active) g_mortarLeft.store(g_mortar.rounds);
    for (MortarProp& p : g_mortarProps)
        if (p.up && now > p.seen + 3000) { MortarPark(ctx, base, p.handle); p.up = false; }
    for (MortarShell& s : g_mortarShells)
    {
        if (!s.on) continue;
        if (!s.whistled && now + 1100 >= s.t1)
        {
            s.whistled = true;
            MortarSoundAt(ctx, base, s.tx, s.ty, s.tz + 250.0f, "UAV:Mortar_Miss");
        }
        if (now >= s.t1)
        {
            s.on = false;
            // 0.5.291: the campaign round's own explosion (mortar_shell:
            // mortar_dirt, SHELL_EXPLO_DIRT) when Laison's weapons are in;
            // else the panzerschreck's and the frag's.
            // 0.5.308: 307 TESTED: the campaign round's event made no
            // explosion that Justin could see, so the panzerschreck's and
            // the frag's (seen before 298) always go off too; the round's
            // own explosion fields are logged once to find why.
            if (MortarCampaignWeapons(base))
            {
                PlaneFx(ctx, base, s.tx, s.ty, s.tz + 8.0f, "mortar_shell", 56);
                static bool told = false;
                if (!told)
                {
                    told = true;
                    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                    const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
                    for (int32_t k = 1; list && k <= count && k < 92; k++)
                    {
                        const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
                        const uint32_t text = def ? Guest::Read32(base, def + 8) : 0;
                        if (text < 0x40000000u || text >= 0x90000000u || ReadText(base, text, 16) != "mortar_shell") continue;
                        const uint32_t fx = Guest::Read32(base, def + 1940), snd = Guest::Read32(base, def + 1944), model = Guest::Read32(base, def + 1932);
                        printf("mortar: mortar_shell (weapon %d) explosion effect %08X sound %08X projectile model %08X\n", k, fx, snd, model);
                        if (fx >= 0x40000000u && fx < 0x90000000u) printf("mortar:   effect points at '%s' / +0 %08X +4 %08X +8 %08X\n", ReadText(base, fx, 32).c_str(), Guest::Read32(base, fx), Guest::Read32(base, fx + 4), Guest::Read32(base, fx + 8));
                    }
                    fflush(stdout);
                }
            }
            PlaneFx(ctx, base, s.tx, s.ty, s.tz + 8.0f, "panzerschreck", 56);
            PlaneFx(ctx, base, s.tx, s.ty, s.tz + 8.0f, "fraggrenade", 56);
            MortarSoundAt(ctx, base, s.tx, s.ty, s.tz + 20.0f, "UAV:Mortar_Explo");
            MortarPark(ctx, base, s.handle);
            if (BotsThisMachineHostsBots(base))
            {
                const uint32_t attacker = s.attacker ? s.attacker : MortarShooter(base, s.number);
                MortarBlast(ctx, base, s.tx, s.ty, s.tz + 16.0f, attacker);
            }
            continue;
        }
        if (!s.handle || now < s.nextMove) continue;
        s.nextMove = now + 50;
        const float f = float(now - s.t0) / float(std::max<uint64_t>(1, s.t1 - s.t0));
        const float f2 = std::min(1.0f, f + 0.04f);
        float x, y, z, x2, y2, z2;
        MortarShellAt(s, f2, x, y, z);
        MortarShellAt(s, std::min(1.0f, f2 + 0.02f), x2, y2, z2);
        PlaneMoveTo(ctx, base, s.handle, x, y, z, 0.1f);
        const float flat = sqrtf((x2 - x) * (x2 - x) + (y2 - y) * (y2 - y));
        const float pitch = -atan2f(z2 - z, std::max(flat, 0.01f)) * 180.0f / 3.14159265f;
        const float yaw = atan2f(s.ty - s.sy, s.tx - s.sx) * 180.0f / 3.14159265f;
        PlaneRotateTo(ctx, base, s.handle, pitch - 90.0f, yaw, 0.0f, 0.1f);   // the shell's nose up its length (tune)
    }
}

namespace
{
    enum StreakKind { StreakUav, StreakArtillery, StreakTyphoon, StreakMortar, StreakKinds };   // 0.5.285: the mortar
    struct StreakBox { bool active = false; int kills = 0; int counts[StreakKinds] = {}; int picked = 0; bool armed = false; bool typhoon = false; bool mortar = false; };
    std::mutex g_streakBoxLock;
    StreakBox g_streakBox;
    int g_streakPicked = 0;
    constexpr const char* Spotter = "mp_binoculars";

    int StreakCount(const Streak& s, int kind)
    {
        if (kind == StreakMortar) return s.mortars + (g_mortarLeft.load() > 0 && !g_mortarActive.load() ? 1 : 0);   // 0.5.285: one kept part-used counts too
        return kind == StreakUav ? s.uavs : kind == StreakArtillery ? std::max(0, s.charges - s.typhoons) : s.typhoons;
    }

    bool StreakHudActive()
    {
        if (GetTickCount64() < g_planeHitMarker.load()) return true;   // 0.5.258: a hit marker on the UAV plane
        if (g_mortarActive.load()) return true;   // 0.5.285
        std::lock_guard<std::mutex> lock(g_streakBoxLock);
        return g_streakBox.active;
    }

    void StreakHudDraw(int width, int height)
    {
        StreakBox box;
        {
            std::lock_guard<std::mutex> lock(g_streakBoxLock);
            box = g_streakBox;
        }
        if (GetTickCount64() < g_planeHitMarker.load())
        {
            // 0.5.258: a hit marker (the four short white strokes of an X) at
            // the screen's middle: a shot hit the enemy's UAV plane.
            ImDrawList* marks = ImGui::GetForegroundDrawList();
            const float cx = width * 0.5f, cy = height * 0.5f, a = 7.0f * height / 1080.0f, b = 16.0f * height / 1080.0f;
            const ImU32 colour = IM_COL32(255, 255, 255, 235);
            const float thick = std::max(2.0f, 2.5f * height / 1080.0f);
            marks->AddLine(ImVec2(cx - b, cy - b), ImVec2(cx - a, cy - a), colour, thick);
            marks->AddLine(ImVec2(cx + b, cy - b), ImVec2(cx + a, cy - a), colour, thick);
            marks->AddLine(ImVec2(cx - b, cy + b), ImVec2(cx - a, cy + a), colour, thick);
            marks->AddLine(ImVec2(cx + b, cy + b), ImVec2(cx + a, cy + a), colour, thick);
        }
        // 0.5.285: at the mortar: the landing point's ring and its panel.
        {
            MortarView view;
            {
                std::lock_guard<std::mutex> hold(g_mortarViewLock);
                view = g_mortarView;
            }
            if (view.on)
            {
                ImDrawList* draw = ImGui::GetForegroundDrawList();
                ImFont* font = ImGui::GetFont();
                const float scale = height / 1080.0f;
                const ImU32 gold = IM_COL32(240, 200, 90, 255), red = IM_COL32(230, 70, 50, 240), white = IM_COL32(255, 255, 255, 240), back = IM_COL32(0, 0, 0, 150);
                if (view.ahead)
                {
                    // The view's field: the setting's (cg_fov, 4:3 across) as
                    // the game's: up and down from it, across by the window.
                    const float fov = float(std::clamp(Settings::Get().fov, 50, 120));
                    const float tanV = tanf(fov * 0.5f * 3.14159265f / 180.0f) * 0.75f;
                    const float tanH = tanV * float(width) / float(std::max(1, height));
                    const float sx = width * 0.5f + (view.right / view.fwd) / tanH * width * 0.5f;
                    const float sy = height * 0.5f - (view.up / view.fwd) / tanV * height * 0.5f;
                    const float r = std::clamp(9000.0f / view.fwd, 6.0f, 60.0f) * scale;
                    const ImU32 ring = view.ready ? red : IM_COL32(230, 140, 50, 200);
                    draw->AddCircle(ImVec2(sx, sy), r, ring, 32, 2.5f * scale);
                    draw->AddCircle(ImVec2(sx, sy), r * 0.35f, ring, 16, 2.0f * scale);
                    draw->AddLine(ImVec2(sx - r * 1.4f, sy), ImVec2(sx - r * 0.6f, sy), ring, 2.0f * scale);
                    draw->AddLine(ImVec2(sx + r * 0.6f, sy), ImVec2(sx + r * 1.4f, sy), ring, 2.0f * scale);
                }
                char text[96];
                snprintf(text, sizeof(text), "MORTAR   %d ROUNDS   RANGE %d", view.rounds, view.range);
                const float big = 24.0f * scale, small = 17.0f * scale;
                const float w = 470.0f * scale, h = 62.0f * scale, x = width * 0.5f - w * 0.5f, y = height - h - 150.0f * scale;
                draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), back, 6.0f * scale);
                draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), gold, 6.0f * scale, 0, 2.0f * scale);
                draw->AddText(font, big, ImVec2(x + 14 * scale, y + 6 * scale), view.ready ? gold : white, text);
                draw->AddText(font, small, ImVec2(x + 14 * scale, y + 36 * scale), white,
                              (ButtonPrompts::KeyboardActive() ? (view.ready ? (ButtonPrompts::ActionKey(Settings::Fire) + " FIRE    MOUSE: RANGE    [RIGHT]: PUT AWAY").c_str() : "LOADING...    [RIGHT]: PUT AWAY") : (view.ready ? "RT FIRE    LOOK UP/DOWN: RANGE    D-PAD RIGHT: PUT AWAY" : "LOADING...    D-PAD RIGHT: PUT AWAY")));
            }
        }
        if (!box.active) return;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float scale = height / 1080.0f;
        const float size = 22.0f * scale, small = 17.0f * scale, row = 34.0f * scale, w = 250.0f * scale;
        static const char* const names[StreakKinds] = { "UAV", "ARTILLERY", "TYPHOON", "MORTAR" };
        int rows = 0;
        for (int k = 0; k < StreakKinds; k++) if (box.counts[k] > 0) rows++;
        const float h = (rows + 1) * row + 14.0f * scale;
        const float x = width - w - 40.0f * scale, y = height - h - 230.0f * scale;
        const ImU32 back = IM_COL32(0, 0, 0, 150), edge = IM_COL32(210, 180, 110, 220), white = IM_COL32(255, 255, 255, 240),
                    dim = IM_COL32(200, 200, 200, 170), gold = IM_COL32(240, 200, 90, 255);
        draw->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), back, 6.0f * scale);
        draw->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), edge, 6.0f * scale, 0, 2.0f * scale);
        char line[64];
        snprintf(line, sizeof(line), "KILLSTREAK  %d", box.kills);
        draw->AddText(font, small, ImVec2(x + 12 * scale, y + 8 * scale), dim, line);
        float ry = y + row;
        for (int k = 0; k < StreakKinds; k++)
        {
            if (box.counts[k] <= 0) continue;
            const bool picked = k == box.picked;
            if (picked) draw->AddRect(ImVec2(x + 6 * scale, ry - 3 * scale), ImVec2(x + w - 6 * scale, ry + row - 7 * scale), gold, 4.0f * scale, 0, 2.0f * scale);
            snprintf(line, sizeof(line), "%s%s  x%d", picked ? "> " : "  ", names[k], box.counts[k]);
            draw->AddText(font, size, ImVec2(x + 14 * scale, ry), picked ? gold : white, line);
            ry += row;
        }
        const char* hint = box.mortar ? "D-PAD RIGHT put the mortar away"
                         : box.armed ? (box.typhoon ? "MARK THE RUN: fire the binoculars" : "MARK THE SPOT: fire the binoculars")
                                     : "D-PAD DOWN pick   D-PAD RIGHT call";
        std::string keyboardHint;
        if (ButtonPrompts::KeyboardActive()) {
            keyboardHint = box.mortar ? "[Right] put the mortar away" : box.armed ?
                (ButtonPrompts::ActionKey(Settings::Fire) + (box.typhoon ? " mark the run" : " mark the spot")) :
                (ButtonPrompts::ActionKey(Settings::BinocularsAway) + " pick   [Right] call");
            hint = keyboardHint.c_str();
        }
        draw->AddText(font, small, ImVec2(x + 12 * scale, y + h + 4 * scale), dim, hint);
    }
}

// Each game frame (from the binoculars' d-pad code): the picks and calls,
// and the box's picture. True when d-pad down was the box's (a streak to
// pick from), so it does not also put the binoculars away.
bool MpFixesStreakPad(PPCContext& ctx, uint8_t* base, bool down, int rights)
{
    static bool registered = false;
    if (!registered) { registered = true; Overlay::SetStreakHud(StreakHudActive, StreakHudDraw); }
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
    const int i = own ? ClientOf(base, own) : -1;
    StreakBox box;
    bool usedDown = false;
    if (i >= 0 && Settings::Get().killstreakArtillery)
    {
        Streak& s = g_streaks[i];
        if (g_streakBackToGun)
        {
            g_streakBackToGun = false;
            OnWeapon(ctx, base, 2692, g_ownPrimary);
        }
        int total = 0;
        for (int k = 0; k < StreakKinds; k++) total += StreakCount(s, k);
        if (total > 0 && StreakCount(s, g_streakPicked) == 0)
            for (int k = 0; k < StreakKinds; k++) if (StreakCount(s, k) > 0) { g_streakPicked = k; break; }
        const bool atMortar = g_mortarActive.load();   // 0.5.285
        if (down && total > 0 && !s.armed && !atMortar)
        {
            usedDown = true;
            for (int step = 1; step <= StreakKinds; step++)
            {
                const int k = (g_streakPicked + step) % StreakKinds;
                if (StreakCount(s, k) > 0) { g_streakPicked = k; break; }
            }
        }
        if (down && atMortar) usedDown = true;
        if (rights > 0 && atMortar)
        {
            g_mortarAskStop.store(true);   // 0.5.285: d-pad right at the mortar puts it away
            rights = 0;
        }
        if (rights > 0 && total > 0 && !s.armed && s.alive)
        {
            const int k = g_streakPicked;
            if (k == StreakMortar && StreakCount(s, k) > 0)   // 0.5.285
            {
                if (g_mortarLeft.load() > 0) g_mortarAskStart.store(1);
                else
                {
                    s.mortars--;
                    MpFixesMortarUsedOut(base);
                    g_mortarAskStart.store(2);
                }
                printf("killstreak: own mortar set up from the box (%d more waiting)\n", s.mortars);
            }
            else if (k == StreakUav && s.uavs > 0)
            {
                s.uavs--;
                MpFixesStreakUsedOut(base);   // 0.5.276: a joiner tells the host
                MpFixesUav(Settings::Get().uavSeconds);
                if (Settings::Get().uavPlane) MpFixesUavPlane(base, own, Settings::Get().uavSeconds);   // 0.5.253: the plane over the map
                printf("killstreak: own UAV called from the box\n");
            }
            else if ((k == StreakArtillery || k == StreakTyphoon) && StreakCount(s, k) > 0)
            {
                s.armed = true;
                s.typhoonCalled = k == StreakTyphoon;
                const uint32_t ps = Guest::Read32(base, own + 512);
                uint8_t slots[10];
                for (int n = 0; n < 10; n++) slots[n] = Guest::Read8(base, ps + 1064 + n);
                OnWeapon(ctx, base, 2664, Spotter);
                for (int n = 0; n < 10; n++) if (Guest::Read8(base, ps + 1064 + n) != slots[n]) Guest::Write8(base, ps + 1064 + n, slots[n]);
                OnWeapon(ctx, base, 2692, Spotter);
                OnWeapon(ctx, base, 2864, Spotter, 1);   // one round in them: the mark
                Overlay::Notice(k == StreakTyphoon ? "TYPHOON  -  fire the binoculars at the line's centre" : "ARTILLERY  -  fire the binoculars at the spot", 3000);
                printf("killstreak: own %s called from the box: the spotter's binoculars out\n", k == StreakTyphoon ? "Typhoon" : "artillery");
            }
            fflush(stdout);
        }
        box.active = total > 0 || s.armed || s.kills > 0 || atMortar;
        box.kills = s.kills;
        box.mortar = atMortar;
        for (int k = 0; k < StreakKinds; k++) box.counts[k] = StreakCount(s, k);
        box.picked = g_streakPicked;
        box.armed = s.armed;
        box.typhoon = s.typhoonCalled;
    }
    // 0.5.86: a player who joined (his kills are counted on the host, which
    // arms his strike and puts the spotter's binoculars in his hands): while
    // he holds them, his box shows the strike waiting to be marked.
    if (!box.active && !g_damageSeen && g_heldSpotter)
    {
        box.active = true;
        box.counts[StreakArtillery] = 1;
        box.picked = StreakArtillery;
        box.armed = true;
    }
    {
        std::lock_guard<std::mutex> lock(g_streakBoxLock);
        g_streakBox = box;
    }
    return usedDown;
}

// Vehicles put down by hand (the spawnvehicle console command, the first
// step of the vehicle placement and the Kangaroo).
//
// A map's vehicles are entities of the class "script_vehicle": the entity is
// made (G_Spawn, sub_825638A0), its class name set (+548 the script string,
// sub_821DD8C8; +552 its id, sub_82548BC8), its origin (+336) and angles
// (+352), and the class's spawn function called through G_CallSpawnEntity
// (sub_8257DE38: the 53 class names' ids at 0x82BCCC68, their functions at
// 0x825FE818), as the script's spawn() does (sub_824CEAD8). The vehicle's
// (sub_82581190) reads its kind from the spawn's key/values ("vehicletype",
// level.spawnVars at 0x82C281D0: +176 the count, +180 pairs of the key's
// hash and the text) and sets it up (sub_8257F950) from the map's
// VEHICLEFILE entries: Merville and Poissons have mp_sherman(tank),
// mp_panzeriv, mp_horch, mp_wc51, mp_motorcycle_bmw and _harley. A map
// without that entry has nothing to set it up from (said in the log).
//
// The console's command is taken before the game's buffer (kernel_video.cpp)
// and done here, in the server's frame, on the host only.
namespace
{
    std::mutex g_vehicleMutex;
    bool g_inMapStart = false;   // vehicles made in the map's start: the script's own setup pass comes after

    const char* VehicleType(const std::string& asked)
    {
        static const struct { const char* name; const char* type; } names[] = {
            { "sherman", "mp_shermantank" }, { "tank", "mp_shermantank" }, { "mp_sherman", "mp_shermantank" },
            { "panzer", "mp_panzeriv" }, { "panzeriv", "mp_panzeriv" }, { "horch", "mp_horch" },
            { "wc51", "mp_wc51" }, { "jeep", "mp_wc51" }, { "truck", "mp_wc51" },
            { "bmw", "mp_motorcycle_bmw" }, { "harley", "mp_motorcycle_harley" }, { "motorcycle", "mp_motorcycle_harley" },
            { "kangaroo", "mp_kangaroo" }, { "apc", "mp_kangaroo" },
            { "mortar", "mortar_81mm" }, { "81mm", "mortar_81mm" }, { "mortar_81mm", "mortar_81mm" },   // 0.5.299
        };
        for (const auto& n : names) if (_stricmp(asked.c_str(), n.name) == 0) return n.type;
        // A vehicle file's name without its "mp_" (motorcycle_harley, wc51).
        static std::string prefixed;
        prefixed = "mp_" + asked;
        for (const auto& n : names) if (_stricmp(prefixed.c_str(), n.type) == 0) return n.type;
        return nullptr;
    }

    // The model a kind's map entity carries (Merville's script_vehicle
    // entities: their "model"). The entity needs it set before its spawn
    // function, as the map's loading does: 0.5.59's had none, and were
    // made but never seen.
    const char* VehicleModel(const std::string& type)
    {
        static const struct { const char* type; const char* model; } models[] = {
            { "mp_shermantank", "v_shermanm4a3" }, { "mp_panzeriv", "v_panzer_iv" }, { "mp_horch", "v_horch1a" },
            { "mp_wc51", "v_wc51" }, { "mp_motorcycle_bmw", "v_bmw" }, { "mp_motorcycle_harley", "v_harley" },
            { "mp_kangaroo", "v_Kangaroo" },
            { "mortar_81mm", "v_81mm" },   // 0.5.299: the campaign mortar (Laison's vehicle file; its model in Laison's vehicles.cod)
        };
        for (const auto& m : models) if (_stricmp(type.c_str(), m.type) == 0) return m.model;
        return nullptr;
    }

    float ReadF(uint8_t* base, uint32_t at) { const uint32_t w = Guest::Read32(base, at); float f; memcpy(&f, &w, 4); return f; }
    void WriteF(uint8_t* base, uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); }

    // A vehicle of a kind (its vehicle file's name) at a place, facing yaw;
    // true when it set up (its own vehicle record).
    bool SpawnVehicleAt(PPCContext& ctx, uint8_t* base, const std::string& type, float x, float y, float z, float yaw)
    {

        // Text for the game: the class name, the kind, the key's name.
        static uint32_t scratch = 0;
        if (!scratch) scratch = Guest::AllocatePhysical(0x200);
        memset(base + scratch, 0, 0x200);
        const uint32_t classText = scratch + 0x40, typeText = scratch + 0x80, keyText = scratch + 0xC0;
        memcpy(base + classText, "script_vehicle", 15);
        memcpy(base + typeText, type.c_str(), std::min<size_t>(type.size(), 0x3E) + 1);
        memcpy(base + keyText, "vehicletype", 12);

        // Is the kind set up on this map (its vehicle file read: sub_8255A310,
        // the kind's number, -1 none; the names at 0x82B2DC90, their count
        // at 0x82B34594)? And is a vehicle record free (the map's pool,
        // level +3088 of them, 1872 bytes each at 0x82BCBDB4; one is in use
        // while its +368 is a live entity's handle)? 0.5.61's vehicles all
        // took the same record: the kind was not found, the record was
        // cleared and left free, and the entity stayed a model only.
        // The Harley's model (v_harley) is only in Poissons' archive: on
        // Merville only its wreck is, and a Harley put down there was
        // nothing to see (0.5.66).
        if (_stricmp(type.c_str(), "mp_motorcycle_harley") == 0 && Kernel::CurrentMap() != "poissons")
        {
            Overlay::ConsolePrint("spawnvehicle: the Harley's model is only on Poissons; here try bmw (the German motorcycle)");
            printf("vehicles: no Harley model on %s\n", Kernel::CurrentMap().c_str());
            fflush(stdout);
            return false;
        }
        const int32_t kind = int32_t(Call3(ctx, base, 0x8255A310, typeText, 0, 0));
        if (kind < 0)
        {
            std::string known;
            const int count = int(int16_t(Guest::Read16(base, 0x82B34594)));
            for (int i = 0; i < count && i < 32; i++)
            {
                const uint32_t name = Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(i));
                known += (known.empty() ? "" : ", ") + (name ? ReadText(base, name, 32) : std::string("?"));
            }
            printf("vehicles: spawnvehicle %s: not set up on this map (%d kinds: %s)\n", type.c_str(), count, known.c_str());
            fflush(stdout);
            Overlay::ConsolePrint("spawnvehicle: " + type + " is not on this map. Here: " + (known.empty() ? std::string("none") : known));
            return false;
        }
        {
            const uint32_t pool = Guest::Read32(base, 0x82BCBDB4);
            const int records = int(Guest::Read16(base, 0x82C281D0 + 3088));
            int used = 0;
            for (int i = 0; pool && i < records; i++)
            {
                const uint32_t handle = Guest::Read32(base, pool + 1872 * uint32_t(i) + 368);
                const uint32_t index = handle & 0x3FFF;
                if (index >= 4096) continue;
                const uint32_t entry = 0x82BD1FC0 + 8 * index;
                if ((handle >> 14) == Guest::Read32(base, entry + 4) && Guest::Read32(base, entry) != 0) used++;
            }
            if (used >= records)
            {
                printf("vehicles: spawnvehicle %s: all %d vehicle records in use\n", type.c_str(), records);
                fflush(stdout);
                Overlay::ConsolePrint("spawnvehicle: the maximum number of vehicles (" + std::to_string(records) + ") is reached");
                return false;
            }
        }

        // The spawn's key/values: only the kind, for this one spawn.
        constexpr uint32_t Vars = 0x82C281D0;
        const uint32_t savedCount = Guest::Read32(base, Vars + 176);
        const uint32_t savedKey = Guest::Read32(base, Vars + 180), savedValue = Guest::Read32(base, Vars + 184);
        const uint32_t keyHash = Call3(ctx, base, 0x820E10E0, keyText, 0, 0);
        Guest::Write32(base, Vars + 176, 1);
        Guest::Write32(base, Vars + 180, keyHash);
        Guest::Write32(base, Vars + 184, typeText);

        const uint32_t ent = Call3(ctx, base, 0x825638A0, 0xFFFFFFFFu, 0, 0);
        uint32_t made = 0;
        if (ent)
        {
            Call3(ctx, base, MakeText, scratch, classText, 0);
            Call3(ctx, base, 0x821DD8C8, ent + 548, scratch, 0);
            Call3(ctx, base, FreeText, scratch, 0, 0);
            const uint32_t id = Call3(ctx, base, 0x82548BC8, scratch + 0x10, ent + 548, 0);
            Guest::Write32(base, ent + 552, id ? Guest::Read32(base, id) : 0);
            WriteF(base, ent + 336, x); WriteF(base, ent + 340, y); WriteF(base, ent + 344, z);
            WriteF(base, ent + 352, 0.0f); WriteF(base, ent + 356, yaw); WriteF(base, ent + 360, 0.0f);
            Guest::Write32(base, ent + 616, 0);
            // Its model, through the script's SetModel (sub_824CE130: the
            // entity's script handle +484, a script string of the name).
            if (const char* model = VehicleModel(type))
            {
                const uint32_t modelText = scratch + 0x100;
                memcpy(base + modelText, model, strlen(model) + 1);
                Call3(ctx, base, MakeText, scratch + 0x20, modelText, 0);
                Call3(ctx, base, 0x824CE130, Guest::Read32(base, ent + 484), scratch + 0x20, 0);
                Call3(ctx, base, FreeText, scratch + 0x20, 0, 0);
                printf("vehicles: model %s set\n", model);
            }
            made = Call3(ctx, base, 0x8257DE38, ent, 0, 0);
        }
        Guest::Write32(base, Vars + 176, savedCount);
        Guest::Write32(base, Vars + 180, savedKey);
        Guest::Write32(base, Vars + 184, savedValue);
        const uint32_t record = ent ? Guest::Read32(base, ent + 524) : 0;
        const bool bound = record && Guest::Read32(base, record + 368) == Guest::Read32(base, ent + 484);
        printf("vehicles: spawnvehicle %s (kind %d) at %.0f %.0f %.0f (yaw %.0f): entity %08X, %s, vehicle record %08X %s\n",
               type.c_str(), kind, x, y, z, yaw, ent, made ? "spawned" : "the game refused it", record, bound ? "its own" : "NOT bound to it");
        fflush(stdout);
        // The multiplayer script's own setup of each map vehicle
        // (sub_821F2B40, from _mp_tankdrive.bro: run by sub_821FD058 over
        // getentarray("script_vehicle") at the map's start): its damage
        // effects, blowing up when flipped or under the world, and what
        // lets players get in and man its gun. A vehicle put down later
        // missed it: it stood there, not to be driven.
        // A map vehicle beside ours, word by word where they differ (the
        // entity, then the vehicle record): what a vehicle put down here
        // still lacks to be driven, manned and to sit on its wheels (0.5.70:
        // they hover and cannot be used, the map's own can).
        if (bound)
        {
            const uint32_t pool = Guest::Read32(base, 0x82BCBDB4);
            const int records = int(Guest::Read16(base, 0x82C281D0 + 3088));
            for (int i = 0; pool && i < records; i++)
            {
                const uint32_t theirs = pool + 1872 * uint32_t(i);
                if (theirs == record) continue;
                const uint32_t handle = Guest::Read32(base, theirs + 368), index = handle & 0x3FFF;
                if (index >= 4096) continue;
                const uint32_t entry = 0x82BD1FC0 + 8 * index;
                if ((handle >> 14) != Guest::Read32(base, entry + 4)) continue;
                const uint32_t other = Guest::Read32(base, entry);
                if (!other) continue;
                printf("vehicles: ours %08X (record %08X) against the map's %08X (record %08X, kind %d):\n", ent, record, other, theirs,
                       int(int16_t(Guest::Read16(base, theirs + 376))));
                int lines = 0;
                for (uint32_t w = 0; w < 1000 && lines < 120; w += 4)
                {
                    const uint32_t a = Guest::Read32(base, ent + w), b = Guest::Read32(base, other + w);
                    if (a != b) { printf("vehicles:   entity +%u ours %08X map's %08X\n", w, a, b); lines++; }
                }
                for (uint32_t w = 0; w < 1872 && lines < 260; w += 4)
                {
                    const uint32_t a = Guest::Read32(base, record + w), b = Guest::Read32(base, theirs + w);
                    if (a != b) { printf("vehicles:   record +%u ours %08X map's %08X\n", w, a, b); lines++; }
                }
                fflush(stdout);
                break;
            }
        }
        if (bound && !g_inMapStart)
        {
            static uint32_t word = 0;
            if (!word) word = Guest::AllocatePhysical(0x10);
            Guest::Write32(base, word, Guest::Read32(base, ent + 484));
            Call3(ctx, base, 0x821F2B40, word, 0, 0);
            printf("vehicles: the script's vehicle setup run for it\n");
            fflush(stdout);
        }
        Overlay::ConsolePrint("spawnvehicle: " + type + (bound ? " put down" : " was made but did not set up (see the log)"));
        return bound;
    }
}

// The vehicle placement file (vehicles.txt beside the executable): one
// vehicle a line, "<map> <kind> <x> <y> <z> <yaw>". The host puts each of a
// map's down a few seconds into every game on it; vehicle_mark <kind> puts
// one down in front of you and adds it, vehicle_clear forgets the map's,
// vehicle_list says them. Only the machine running the game (the host,
// svs's clients at 0x82B3FDE8 there) makes vehicles: the others are told by
// the game itself, as for any entity.
namespace
{
    struct VehicleOrder { int what = 0; std::string kind; };   // 1 spawn, 2 mark, 3 clear, 4 list
    std::vector<VehicleOrder> g_vehicleOrders;
    bool g_placementDue = false;

    std::string PlacementFile()
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string file(path);
        const size_t slash = file.find_last_of("\\/");
        return (slash == std::string::npos ? std::string() : file.substr(0, slash + 1)) + "vehicles.txt";
    }

    struct Placed { std::string map, kind; float x, y, z, yaw; };
    std::vector<Placed> ReadPlacements()
    {
        std::vector<Placed> all;
        if (FILE* file = fopen(PlacementFile().c_str(), "r"))
        {
            char line[256];
            while (fgets(line, sizeof(line), file))
            {
                char map[64] = {}, kind[64] = {};
                Placed p{};
                if (line[0] == '#') continue;
                if (sscanf(line, "%63s %63s %f %f %f %f", map, kind, &p.x, &p.y, &p.z, &p.yaw) == 6) { p.map = map; p.kind = kind; all.push_back(p); }
            }
            fclose(file);
        }
        return all;
    }

    void WritePlacements(const std::vector<Placed>& all)
    {
        if (FILE* file = fopen(PlacementFile().c_str(), "w"))
        {
            fprintf(file, "# Vehicles put down at the start of each game on a map: map kind x y z yaw (vehicle_mark <kind> in the console adds one)\n");
            for (const Placed& p : all) fprintf(file, "%s %s %.0f %.0f %.0f %.0f\n", p.map.c_str(), p.kind.c_str(), p.x, p.y, p.z, p.yaw);
            fclose(file);
        }
    }

    bool HostHere(uint8_t* base) { return Guest::Read32(base, 0x82B3FDE0 + 8) != 0; }

    std::string MapNow()
    {
        std::string map = Kernel::CurrentMap();
        for (char& c : map) c = char(tolower(static_cast<unsigned char>(c)));
        return map;
    }

    // In front of this machine's player: 220 units along his facing.
    bool InFront(uint8_t* base, float& x, float& y, float& z, float& yaw)
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        if (!own || !g_ownHandle) return false;
        // The view's yaw: the player state's +208 (as the bots aim by it).
        // (0.5.64 took the entity's +356, which stayed 0 in a devmap: every
        // vehicle went east of the player, out of sight.)
        const uint32_t ps = Guest::Read32(base, own + 512);
        yaw = ps ? ReadF(base, ps + 208) : ReadF(base, own + 356);
        printf("vehicles: facing: state +204 %.1f +208 %.1f, entity +352 %.1f +356 %.1f\n", ps ? ReadF(base, ps + 204) : 0.0f,
               yaw, ReadF(base, own + 352), ReadF(base, own + 356));
        const float rad = yaw * 3.14159265f / 180.0f;
        x = ReadF(base, own + 336) + 220.0f * cosf(rad);
        y = ReadF(base, own + 340) + 220.0f * sinf(rad);
        z = ReadF(base, own + 344) + 16.0f;
        return true;
    }

    std::string KindOf(const std::string& asked)
    {
        if (const char* known = VehicleType(asked)) return known;
        return asked;
    }
}

// Called by the console (kernel_video.cpp): spawnvehicle, vehicle_mark,
// vehicle_clear, vehicle_list.
void MpFixesPlacementDue() { g_placementDue = true; }
void MpFixesAskVehicle(const std::string& kind)
{
    std::lock_guard<std::mutex> lock(g_vehicleMutex);
    g_vehicleOrders.push_back({ 1, kind.empty() ? std::string("sherman") : kind });
}
void MpFixesVehicleOrder(int what, const std::string& kind)
{
    std::lock_guard<std::mutex> lock(g_vehicleMutex);
    g_vehicleOrders.push_back({ what, kind });
}

void MpFixesKangarooProbe(PPCContext& ctx, uint8_t* base);
void MpFixesKangarooWhere(uint8_t* base);
void MpFixesVehicles(PPCContext& ctx, uint8_t* base)
{
    std::vector<VehicleOrder> orders;
    {
        std::lock_guard<std::mutex> lock(g_vehicleMutex);
        orders.swap(g_vehicleOrders);
    }
    for (const VehicleOrder& order : orders)
    {
        const std::string map = MapNow();
        if (order.what == 3 || order.what == 4)
        {
            std::vector<Placed> all = ReadPlacements(), kept;
            int mine = 0;
            for (const Placed& p : all)
            {
                if (p.map == map)
                {
                    mine++;
                    if (order.what == 4)
                        Overlay::ConsolePrint("  " + p.kind + " at " + std::to_string(int(p.x)) + " " + std::to_string(int(p.y)) + " " + std::to_string(int(p.z)));
                    if (order.what == 3) continue;
                }
                kept.push_back(p);
            }
            if (order.what == 3) { WritePlacements(kept); Overlay::ConsolePrint("vehicle_clear: " + std::to_string(mine) + " placed vehicles of " + map + " forgotten (from its next game)"); }
            else Overlay::ConsolePrint("vehicle_list: " + std::to_string(mine) + " placed on " + map + " (" + PlacementFile() + ")");
            continue;
        }
        if (!HostHere(base))
        {
            Overlay::ConsolePrint("spawnvehicle: only the host can put vehicles down");
            continue;
        }
        float x, y, z, yaw;
        if (!InFront(base, x, y, z, yaw))
        {
            Overlay::ConsolePrint("spawnvehicle: you are not in a game");
            continue;
        }
        const std::string kind = KindOf(order.kind);
        const bool made = SpawnVehicleAt(ctx, base, kind, x, y, z, yaw);
        if (order.what == 2 && made)
        {
            std::vector<Placed> all = ReadPlacements();
            all.push_back({ map, kind, x, y, z, yaw });
            WritePlacements(all);
            Overlay::ConsolePrint("vehicle_mark: " + kind + " saved for " + map + ". Type map_restart: the saved ones are made with the map's own, drivable");
        }
    }
    // (The map's placed vehicles are put down in the map's start itself,
    // sub_82564520 below.)
    MpFixesKangarooProbe(ctx, base);
    MpFixesKangarooWhere(base);
}

// 0.5.128: native binding dispatched only the first Y despite subsequent
// raw press/release edges (126 trace). Bridge each physical edge to the same
// native cycle command, retaining native ownership/ammo/animation checks.
static void MpFixesWeaponCycle(PPCContext& ctx,uint8_t* base)
{
    const unsigned presses=Input::TakeWeaponCyclePresses();
    const uint32_t entities=Guest::Read32(base,0x82BCC7FC);
    const uint32_t own=entities?Guest::Read32(base,entities+4):0;
    const uint32_t ps=own?Guest::Read32(base,own+512):0;
    const bool enabled=ps && Guest::Read32(base,0x82BCBE9C)==0 &&
        Guest::Read32(base,ps+1932)==2 && int32_t(Guest::Read32(base,ps+304))>0 &&
        (Guest::Read32(base,ps+44)&0x80000u)!=0 &&
        (Guest::Read32(base,ps+44)&0x4000u)==0 &&
        (Guest::Read32(base,ps+240)&0x200000u)==0 &&
        !Guest::Read32(base,own+900) && !Guest::Read8(base,0x82C73024) &&
        !Overlay::IsOpen() && RenderStats::Current().triangles>=20000.0f;
    Input::SetWeaponCycleEnabled(enabled);
    // Drain above even in menus: no deferred switch when gameplay resumes.
    if (!enabled) return;
    for (unsigned i=0;i<presses;++i)
    {
        const uint32_t before=Guest::Read32(base,0x82B9F310);
        Call3(ctx,base,0x82457A60,1,0,0);
        printf("weapons: Y/wheel bridge selected %u -> %u held %u\n",before,
               Guest::Read32(base,0x82B9F310),Guest::Read32(base,ps+164));
        fflush(stdout);
    }
}

static void TacticalInsertionFrame(PPCContext& ctx, uint8_t* base);
void MpFixesLauncher(PPCContext& ctx, uint8_t* base)
{
    TacticalInsertionFrame(ctx, base);
    MpFixesVehicles(ctx, base);
    MpFixesKillstreaks(ctx, base);
    MpFixesPlaneFrame(ctx, base);   // 0.5.253: the UAV's plane
    MpFixesCampaignGuns(ctx, base);
    MpFixesWeaponCycle(ctx, base);
    // Picking up a gun puts down the weapon in the second slot ("primaryb",
    // +1066: sub_82502850 and the pick-up itself), whatever is in hand; a
    // custom class's second weapon (the pistol, a main weapon kind here)
    // sits there, so the pistol always went. The weapon in hand (+164 of
    // the player's state) is kept in that slot instead: whenever it is the
    // first slot's (+1065), the two swap places. What is picked up then
    // takes the place of the weapon held, for every player on this machine.
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        for (uint32_t client = 0; entities && client < 24; client++)
        {
            const uint32_t entity = Guest::Read32(base, entities + 4 * (client + 1));
            const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
            // (The players' states are in the title's own data, 0x82..., not
            // the heap: 0.5.37 skipped them all.)
            if (ps < 0x40000000u || ps >= 0x90000000u) continue;
            // In hand: the entity's byte +3 (the host's own carries it there,
            // bots.cpp), else its state's +164 (0.5.36 used only that, and
            // nothing ever swapped).
            uint32_t held = Guest::Read8(base, entity + 3);
            if (held == 0) held = Guest::Read32(base, ps + 164);
            {
                static uint32_t lastHeld[24] = {}, lastState[24] = {};
                const uint32_t state = Guest::Read32(base, ps + 164);
                static int said = 0;
                if ((held != lastHeld[client] || state != lastState[client]) && said < 80)
                {
                    said++;
                    lastHeld[client] = held; lastState[client] = state;
                    printf("slots: client %u in hand %u (entity +3 %u, state +164 %u, +196 %u), slots %u and %u\n", client, held,
                           Guest::Read8(base, entity + 3), state, Guest::Read32(base, ps + 196), Guest::Read8(base, ps + 1065), Guest::Read8(base, ps + 1066));
                    fflush(stdout);
                }
            }
            const uint8_t first = Guest::Read8(base, ps + 1065), second = Guest::Read8(base, ps + 1066);
            if (held != 0 && held < 128 && first == held && second != 0 && second < 128 && second != held)
            {
                Guest::Write8(base, ps + 1065, second);
                Guest::Write8(base, ps + 1066, first);
            }
        }
    }
    // The player's weapon slots (+1064: none, primary, primaryb, pistol,
    // ...) whenever they change, each weapon with its own slot kind (+180):
    // to see why picking up a Sten put down the pistol and not the FG42.
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        static uint8_t last[8] = {};
        static int said = 0;
        if (ps && said < 60)
        {
            uint8_t now[8];
            for (int i = 0; i < 8; i++) now[i] = Guest::Read8(base, ps + 1064 + i);
            if (memcmp(now, last, 8) != 0)
            {
                memcpy(last, now, 8);
                said++;
                const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                printf("slots:");
                for (int i = 0; i < 8; i++)
                {
                    const uint32_t weapon = (now[i] && list) ? Guest::Read32(base, list + 4 * now[i]) : 0;
                    printf(" [%d] %u %s (kind %u)", i, now[i], weapon ? ReadText(base, Guest::Read32(base, weapon + 8), 24).c_str() : "-",
                           weapon ? Guest::Read32(base, weapon + 180) : 0);
                }
                printf("\n");
                fflush(stdout);
            }
        }
    }
    if (g_launcherDue)
    {
        g_launcherDue = false;
        g_launcherOut = false;
        g_launcherReturnWeapon.clear();
        g_launcherReturnId = 0;
        // Not again for an anti-armour class, whose main weapon it is.
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        if (ps && strcmp(g_ownPrimary, Launcher()) != 0)
        {
            // The player's weapon by slot: a byte each at +1064 (slot 1 the
            // "primary", the table of slot names at 0x825D7804). Giving the
            // launcher puts it in the "primary" slot; the main weapon is put
            // back there, so the launcher is carried besides, in no slot.
            // Every slot as it was before (the launcher's own slot may be
            // the pistol's: given, it took the pistol's place).
            uint8_t slots[10];
            for (int i = 0; i < 10; i++) slots[i] = Guest::Read8(base, ps + 1064 + i);
            const uint8_t mainInSlot = slots[1];
            OnWeapon(ctx, base, 2664, Launcher());
            for (int i = 0; i < 10; i++)
                if (Guest::Read8(base, ps + 1064 + i) != slots[i])
                {
                    printf("class: the launcher took slot %d (weapon %u, was %u): put back\n", i, Guest::Read8(base, ps + 1064 + i), slots[i]);
                    Guest::Write8(base, ps + 1064 + i, slots[i]);
                }
            // The launcher's weapon number by its name, as the script's give
            // finds it (sub_824BA800): the name's hash (sub_820E10E0), then
            // the weapon with that hash (sub_82480428).
            static uint32_t text = 0;
            if (!text) text = Guest::AllocatePhysical(0x40);
            memset(base + text, 0, 0x40);
            memcpy(base + text, Launcher(), strlen(Launcher()) + 1);
            const uint32_t hash = Call3(ctx, base, 0x820E10E0, text, 0, 0);
            const uint8_t launcher = uint8_t(Call3(ctx, base, 0x82480428, hash, 0, 0));
            // Its rockets, written as the script's ammunition calls do
            // (sub_824B8A88, sub_824B8CC8) but for the launcher itself: its
            // weapon (the list at 0x82BCBF9C) gives its clip's number (+1468)
            // and its ammunition's (+1460); the clips from +688 of the
            // player's state, the ammunition from +320, each up to its most
            // (0x82B93680, 0x82B8C988).
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t weapon = (launcher && list) ? Guest::Read32(base, list + 4 * launcher) : 0;
            int clipGiven = -1, spareGiven = -1;
            if (weapon && launcher != mainInSlot)
            {
                const uint32_t clip = Guest::Read32(base, weapon + 1468), ammo = Guest::Read32(base, weapon + 1460);
                if (clip > 0 && clip < 128)
                {
                    clipGiven = std::min<int>(int32_t(Guest::Read32(base, 0x82B93680 + 4 * clip)), 1);
                    Guest::Write32(base, ps + 4 * (clip + 172), uint32_t(clipGiven));
                }
                if (Guest::Read32(base, weapon + 1820) != 0 && clip > 0 && clip < 128)
                {
                    // A weapon whose rockets are all in its "clip" (+1820 set:
                    // the spare call writes the clip, up to 0x82B8C988 +27896).
                    spareGiven = std::min<int>(int32_t(Guest::Read32(base, 0x82B8C988 + 27896 + 4 * clip)), 5);
                    Guest::Write32(base, ps + 4 * (clip + 172), uint32_t(spareGiven));
                }
                else if (ammo > 0 && ammo < 128)
                {
                    spareGiven = std::min<int>(int32_t(Guest::Read32(base, 0x82B8C988 + 4 * ammo)), 4);
                    Guest::Write32(base, ps + 4 * (ammo + 80), uint32_t(spareGiven));
                }
            }
            static int said = 0;
            if ((said < 20 && said++ < 20))
            {
                printf("class: own spawn, %s given as equipment (main weapon %s, slot %u kept, launcher %u, clip %d, spare %d)\n",
                       Launcher(), g_ownPrimary, mainInSlot, launcher, clipGiven, spareGiven);
                fflush(stdout);
            }
        }
    }
    MpFixesBinoculars(ctx, base);
    // D-pad left: the launcher out, or back to the main weapon.
    static uint16_t last = 0;
    const uint16_t buttons = Input::Buttons();
    const bool pressed = (buttons & 0x0004) && !(last & 0x0004);
    last = buttons;
    // Drain the edge above even while paused: a menu press must not equip
    // the launcher immediately, or become a deferred press after closing.
    if (Guest::Read8(base, 0x82C73024) != 0) return;
    if (!pressed || !Settings::Get().launcher || !g_ownHandle || g_ownPrimary[0] == 0) return;
    if (RenderStats::Current().triangles < 20000.0f) return;   // not in the menus
    // 0.5.129: return to the weapon actually held before taking out the
    // launcher. Read current equipment so cycling away from the launcher
    // does not leave its toggle latched. Spawn clears the saved return.
    const uint32_t entities=Guest::Read32(base,0x82BCC7FC);
    const uint32_t own=entities?Guest::Read32(base,entities+4):0;
    const uint32_t ps=own?Guest::Read32(base,own+512):0;
    const uint32_t held=ps?Guest::Read32(base,ps+164):0;
    const uint32_t list=Guest::Read32(base,0x82BCBF9C);
    const uint32_t weapon=(list && held>0 && held<128)?Guest::Read32(base,list+held*4):0;
    const std::string heldName=weapon?ReadText(base,Guest::Read32(base,weapon+8),64):std::string();
    g_launcherOut=heldName!=Launcher();
    std::string target=Launcher();
    if (g_launcherOut)
    {
        g_launcherReturnWeapon=heldName;
        g_launcherReturnId=held;
    }
    else
    {
        const bool owned=ps && g_launcherReturnId>0 && g_launcherReturnId<128 &&
            (Guest::Read32(base,ps+1056+4*(g_launcherReturnId>>5)) &
             (1u<<(g_launcherReturnId&31)))!=0;
        target=owned && !g_launcherReturnWeapon.empty()?g_launcherReturnWeapon:g_ownPrimary;
    }
    OnWeapon(ctx,base,2692,target.c_str());
    static int said = 0;
    if ((said < 20 && said++ < 20)) { printf("class: d-pad left, to %s (previous %s)\n", target.c_str(),heldName.c_str()); fflush(stdout); }
}
#endif

// The campaign's guns in multiplayer.
//
// A weapon is set up from its WEAPONFILE entry in the map's archive
// (sub_8249A380: its name, the entry), into a record that names its models
// and animations as text: the view model at +20, the arms' animations from
// +72 to +164, the world model at +1432, the HUD icon at +1440. A campaign
// gun is made as a copy of a like multiplayer one (the Lee-Enfield of the
// Kar98, set up again under its own name from the Kar98's entry), with the
// campaign's model and animation names put in. Their assets come from a
// campaign level's weapons archive, loaded with the multiplayer's own
// (sub_8247DD38, below).
#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_8249A380);
namespace
{
    // Guest text, kept for good.
    uint32_t GuestText(uint8_t* base, const std::string& text)
    {
        static uint32_t pool = 0, used = 0;
        if (!pool || used + text.size() + 1 > 0x4000) { pool = Guest::AllocatePhysical(0x4000); used = 0; }
        const uint32_t at = pool + used;
        memcpy(base + at, text.c_str(), text.size() + 1);
        used += uint32_t(text.size() + 1 + 3) & ~3u;
        return at;
    }

    std::string ReadText(uint8_t* base, uint32_t at, size_t most = 64)
    {
        std::string text;
        for (size_t i = 0; at && i < most; i++) { const char c = char(Guest::Read8(base, at + uint32_t(i))); if (!c) break; text += c; }
        return text;
    }

    struct CampaignGun
    {
        const char* name;       // the new weapon's
        const char* from;       // the multiplayer weapon it is a copy of
        const char* title;      // its name's text key
        const char* viewModel;
        const char* worldModel;
        const char* hud;
        const char* armsFrom;   // in the copy's animation names, this
        const char* armsTo;     // becomes this
        int clipSize;           // its own clip, if not the copy's (0)
        int reloadAdd;          // with a reload in parts: rounds put in by each
        float reloadStart, reloadEach, reloadEnd;   // the parts' animations' lengths, in seconds
        float reloadWhole;      // a reload in one: its animation's length, in seconds (0: the copy's time)
        int dropPercent;        // the copy's time to put it away, in percent (100: as it is)
        int raisePercent;       // the copy's time to take it out, in percent
        int archive;            // which campaign weapons archive has it (CampaignArchives)
        const char* shot = nullptr; // its gunshot sounds' prefix in the campaign banks (<shot>_FIRE_1ST, _3RD)
        const char* scopeFrom = nullptr; // a scoped gun whose scope (its overlay and zoom) it takes
    };
    // The campaign weapons archives the guns come from: St Lambert's (the
    // Lee-Enfield, the Bren) and Fuel Plant's (the Lee-Enfield, the Sten, the
    // FG42; 11 banks of 3 MB), plus Forest's G43 (both require big_memory).
    // Laison's: the scoped Lee-Enfield (its scope model, its arms' scoped reload).
    const char* const CampaignArchives[] = { "sp\\mace2\\weapons.cod", "sp\\fuelplnt\\weapons.cod", "sp\\forest\\weapons.cod", "sp\\laison\\weapons.cod" };
    constexpr int ArchiveCount = int(sizeof(CampaignArchives) / sizeof(CampaignArchives[0]));
    const CampaignGun Guns[] = {
        { "lee_enfield", "kar98", "WEAPON_LEE_ENFIELD", "lee_enfield", "w_lee_Enfield_WORLD", "hud_Lee_enfield", "geRFL", "brRFL",
          // Ten rounds, put in as two stripper clips of five: the bolt opened
          // (w_brRFL_reload_start2, 1.3 s), a clip pushed in (w_brRFL_reload,
          // 0.8 s) for each, the bolt closed (w_brRFL_reload_end2, 1.13 s).
          // Its drop hides the rifle early and its raise shows it late: 30%
          // and 70% of the Kar98's times.
          10, 5, 1.3f, 0.8f, 1.1333f, 0.0f, 30, 70, 0, "LEE_ENFIELD" },
        // The Bren: a copy of the BAR (automatic, the same handling), its
        // animations the campaign's "bren" family (the BAR's are "usBAR",
        // the same moves, no bolt), a 30-round magazine. Its reload takes as
        // long as its campaign sound (GR_BREN in stbert.wbk, 3.73 s; the
        // game's reloads match their sounds: the BAR's 4.0 s to GR_BAR's
        // 3.9, the Thompson's 2.3 to GR_THOMPSON's 2.33): 3.8 s, its
        // animation (3.53 s) a little slower.
        { "bren", "bar", "WEAPON_BREN", "w_bren", "w_bren_WORLD", "hud_bren", "usBAR", "bren",
          30, 0, 0.0f, 0.0f, 0.0f, 3.8f, 30, 70, 0, "BREN" },
        // The Sten: a copy of the Thompson (the "usSMG" family; the Sten's is
        // "brsmg"), a 32-round magazine. Its reload 3.4 s: GR_STEN
        // (fuelplnt.wbk, 2.53 s) has the magazine out at 0.95 s, in at 1.6 s
        // and the bolt at 2.8 s from the reload's start; the animation's are
        // spaced differently (at 3.2 s, 0.5.33's video: 0.7, 1.7 and 2.3 s),
        // so no length fits all three. 3.4 s keeps each within about 0.35 s
        // (2.6 s had the bolt 0.7 s early, 3.7 s ran long after the sound).
        { "sten", "thompson", "WEAPON_STEN", "w_brSMG", "w_sten_WORLD", "hud_sten_smg", "usSMG", "brsmg",
          32, 0, 0.0f, 0.0f, 0.0f, 3.4f, 30, 70, 1, "STEN" },
        // The FG42: a copy of the BAR (automatic), the "FG42" family, a
        // 20-round magazine. Its reload 2.8 s, between GR_fg42's 2.33 s
        // and its animation's 3.27 s (as the Sten's, which was out of step
        // at its sound's length). Fuel Plant's.
        { "fg42", "bar", "WEAPON_FG42", "w_FG42", "w_fg42_WORLD_noscope", "hud_fg_42", "usBAR", "FG42",
          20, 0, 0.0f, 0.0f, 0.0f, 2.8f, 30, 70, 1, "fg42" },
        // The FG42 (scoped): the same gun with the German scope view the
        // scoped Kar98 has (its overlay, reticle and zoom; the campaign
        // FG42's scope is the same German kind, w_ge_scope). The campaign
        // has no scoped FG42 model (w_fg42_world holds only the noscope one),
        // so its models are the FG42's; its name is its own, "FG42 Scoped"
        // (WEAPON_FG42_SCOPED, below). In the class list beside the FG42.
        // (0.5.54: Mace 2's, the campaign's scoped FG42: its scoped world
        // model w_fg42_WORLD, its icon, its own shots fg42sniper_FIRE.)
        { "fg42_scoped", "bar", "WEAPON_FG42_SCOPED", "w_FG42", "w_fg42_WORLD", "hud_fg_42_scoped", "usBAR", "FG42",
          20, 0, 0.0f, 0.0f, 0.0f, 2.8f, 30, 70, 0, "fg42sniper", "kar98_sniper" },
        // The Mills bomb, the British grenade (Fuel Plant's world model): a
        // copy of the American frag, its own model when thrown and lying.
        // Its view model and moves are the frag's (the campaign has none of
        // its own); a custom class's grenade kind 3.
        { "mills_bomb", "fraggrenade", "WEAPON_MILLS_BOMB", nullptr, "w_mills_bomb_WORLD", "hud_us_grenade", nullptr, nullptr,
          0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 100, 100, 1, nullptr, nullptr },
        // The Gewehr 43 (Forest's): a copy of the M1 Garand (semi-automatic),
        // the "geg43" family, a 10-round magazine. Appended after the others
        // so their weapon numbers stay. Its reload 3.45 s (its campaign file
        // has 3.95; 3.45 lines its animation up with its sound, GR_G43: see
        // G43Fields' reloadTime), in place of the Garand's 2.9 s; its other
        // numbers are G43Fields, below. The ini's g43_reload, when set, in its place.
        { "gewehr43", "m1garand", "WEAPON_GEWEHR43", "w_g43_VM", "g43_world", "hud_gewehr43", "usRFL", "geg43",
          10, 0, 0.0f, 0.0f, 0.0f, 3.45f, 100, 100, 2, "GEWEHR43" },
        // The MG42: the campaign has it only mounted and in AI soldiers'
        // hands (w_ge_MG42_nobipod, no first-person model or arm animations),
        // so this is a copy of the MG34 with the MG42 others see in your
        // hands and on the ground, the MG34's view in your own (the AI model
        // there sits above the view with only a hand on it: tried, 0.5.900),
        // the MG34's icon (the MG42 has none), its own gunfire from Forest's
        // bank (MG42_FIRE_1ST/3RD_LASTSHOT; see CampaignShotSuffix) and its own fire rate
        // (below MakeGun's Reload). Appended last so the others keep their numbers.
        { "mg42", "mg34", "WEAPON_MG42", nullptr, "w_ge_MG42_nobipod", "hud_mg34", nullptr, nullptr,
          0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 100, 100, 2, "MG42" },
        // The scoped Lee-Enfield (Laison's, the campaign's WEAPON_LEE_ENFIELD_SNIPER):
        // a copy of the scoped Kar98 (a sniper's: the scope's view, a reload
        // a round at a time between a start and an end), the Lee-Enfield's
        // view model with Laison's scope model on it (attach1Model
        // lee_enfield_scope at tag w_lee_enfield, LeeScopedFields' strings),
        // the "brRFL" moves (reloadSCOPE, reload_start, reload_end, as the
        // Kar98's "geRFL"), its own icon and its numbers (LeeScopedFields,
        // its campaign file's: 10 rounds, 1.4 s start, 0.7 s a round, 1.0 s
        // end, zoom 15). No scoped world model exists: others see the
        // Lee-Enfield. Its shot Lee_Enfield_Sniper_FIRE_1ST (laison.wbk).
        { "lee_enfield_sniper", "kar98_sniper", "WEAPON_LEE_ENFIELD_SNIPER", "lee_enfield", "w_lee_Enfield_WORLD", "hud_Lee_enfield_scoped",
          "geRFL", "brRFL", 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, 100, 100, 3, "Lee_Enfield_Sniper" },
        // 0.5.288: the mortar killstreak's hands (Justin: the gun stayed in
        // them at the mortar): a copy of "interact" (the bare arms the
        // game's interactions use, w_arms_interact_idle_a) with the
        // campaign mortar's arms (w_arms_ei_MortarIdle, Forest's and
        // Laison's weapons archives) and an 81mm round in them
        // (v_81mm_shell). Given and taken out at the mortar, the main
        // weapon back after. Not a class's weapon (the interact class).
        // (0.5.288-0.5.291: "mortar_hands", a copy of interact, then of the
        // binoculars, for the mortar's arms: taken out in 0.5.292. Neither
        // showed the campaign's moves, and on Fuel Plant it was weapon 64,
        // past the game's limit: a player owns weapons by a 64-bit mask,
        // state +1056..+1063, the slots right after it at +1064.)
    };

    int32_t g_campaignWeapon[sizeof(Guns) / sizeof(Guns[0])] = {};   // each campaign gun's weapon number, once made
    // 0.5.206: each campaign gun's copy's world model (as the copy had it,
    // before MakeGun put the campaign's in), for fitting the campaign's to it
    // (WorldFitFrame, world_model_fit.h).
    std::string g_copyWorld[sizeof(Guns) / sizeof(Guns[0])];

    // The Lee-Enfields keep the Kar98's idle (w_arms_geRFL_idle_a), not the
    // campaign's brRFL one: on multiplayer's arms the British idle raised
    // the right hand into view and into the butt, at spawn and after each
    // reload, while after a shot (the rack's last pose) it was right (hand
    // test, session 4). The sights' idle stays the British one.
    // COD3_ENFIELD_BRITISH_IDLE=1 brings the British idle back.
    bool KeepsCopyIdle(const char* gun, const std::string& anim)
    {
        if (strcmp(gun, "lee_enfield") != 0 && strcmp(gun, "lee_enfield_sniper") != 0) return false;
        if (getenv("COD3_ENFIELD_BRITISH_IDLE")) return false;
        const std::string end = "_idle_a";
        return anim.size() > end.size() && _stricmp(anim.c_str() + anim.size() - end.size(), end.c_str()) == 0;
    }

    // Shooters' sound handles and the campaign gun each last fired (none
    // for any other gun), for the gunshot sounds (sub_82554BA0).
    std::map<uint32_t, const CampaignGun*> g_shooters;
    std::mutex g_shootersLock;
    void RememberShooter(uint32_t handle, const CampaignGun* gun)
    {
        std::lock_guard<std::mutex> guard(g_shootersLock);
        if (g_shooters.size() > 256) g_shooters.clear();
        g_shooters[handle] = gun;
    }
    const CampaignGun* ShooterGun(uint32_t handle)
    {
        std::lock_guard<std::mutex> guard(g_shootersLock);
        const auto found = g_shooters.find(handle);
        return found == g_shooters.end() ? nullptr : found->second;
    }

    // A campaign gun's name: the gun it is a copy of when it is not set up on
    // this map (campaign_guns off, or a map without room for its archive);
    // else, or for any other name, null.
    const char* CampaignStandIn(const char* name)
    {
        for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
            if (strcmp(name, Guns[i].name) == 0) return g_campaignWeapon[i] > 0 ? nullptr : Guns[i].from;
        return nullptr;
    }

    // Maps whose memory is too full for the campaign weapons archive (about
    // 27 MB): with it, part of Poissons (its zn01) is never loaded. On these
    // the campaign guns are left out, and a class with one gets the gun it
    // is a copy of.
    const char* const NoCampaignGuns[] = { "poissons" };
    std::string g_mapName;   // the map's, from its archive's path (D:\mp\<map>\<map>.cod)

    // A weapon's reload settings (the weapon file's fields, by the field
    // table at 0x825D7D64): +1464 clipName and +1468 its number, +1476
    // clipSize, +1548 reloadTime, +1552 reloadEmptyTime, +1556 reloadAddTime,
    // +1560 reloadStartTime, +1564 reloadStartAddTime, +1568 reloadEndTime,
    // +1872 segmentedReload, +1876 reloadAmmoAdd, +1880 reloadStartAdd. The
    // clips are numbered after every weapon is set up (sub_82486890), by
    // clipName, each with its clipSize as its most: a clip named for the gun
    // gets a number, and a most, of its own. The animations play stretched to
    // these times, so they are set to the animations' own lengths.

    // The Gewehr 43's own numbers, from its campaign weapon file (read from
    // memory in single player Forest, 2026-10-17: COD3_WEAPONDUMP). Every
    // field where its file differs from the Garand's, the file's MP value
    // where it has one, at the multiplayer weapon record's offset (from the
    // game's own field table at 0x825D77D0: offset, kind 4 int, 5 bool,
    // 6 float, 7 time in seconds kept as milliseconds). Damage is left the
    // Garand's (84, 53%, 1000 / 3000): the campaign file's MP damage (100,
    // to 8000 / 10000) was never the multiplayer balance (its Garand says 50
    // where multiplayer's is 84).
    struct G43Field { uint32_t offset; int kind; float value; const char* name; };
    const G43Field G43Fields[] = {
        { 1632, 6, 0.0f, "adsBobFactor" },   // Garand 0.6
        { 2232, 6, 0.6f, "adsBulletConeAngle" },   // Garand 0.3
        { 1996, 6, 0.1f, "adsCrosshairInFrac" },   // Garand 1
        { 2000, 6, 0.2f, "adsCrosshairOutFrac" },   // Garand 0.1
        { 2020, 6, 0.0f, "adsGunKickAccel" },   // Garand 260
        { 2008, 6, 0.0f, "adsGunKickPitchMax" },   // Garand -27
        { 2004, 6, 0.0f, "adsGunKickPitchMin" },   // Garand -25
        { 2028, 6, 0.0f, "adsGunKickSpeedDecay" },   // Garand 50
        { 2024, 6, 0.0f, "adsGunKickSpeedMax" },   // Garand 600
        { 2032, 6, 0.0f, "adsGunKickStaticDecay" },   // Garand 10
        { 2016, 6, 0.0f, "adsGunKickYawMax" },   // Garand 45
        { 2012, 6, 0.0f, "adsGunKickYawMin" },   // Garand -35
        { 2148, 7, 0.6f, "adsReloadTransTime" },   // Garand 0.2
        { 1604, 6, 1.0f, "adsSensitivityScale" },   // Garand 2
        { 2064, 6, 0.25f, "adsSpread" },   // Garand 0
        { 2068, 6, 0.25f, "adsSpreadDucked" },   // Garand 0
        { 2072, 6, 0.25f, "adsSpreadProne" },   // Garand 0
        { 1760, 6, 0.1f, "adsSwayHorizScale" },   // Garand 0.05
        { 1748, 6, 9.0f, "adsSwayLerpSpeed" },   // Garand 8
        { 1744, 6, 5.0f, "adsSwayMaxAngle" },   // Garand 1
        { 1752, 6, 0.3f, "adsSwayPitchScale" },   // Garand 0.05
        { 1764, 6, 0.1f, "adsSwayVertScale" },   // Garand 0.05
        { 1756, 6, 0.3f, "adsSwayYawScale" },   // Garand 0.05
        { 2152, 7, 0.1f, "adsTransBlendTime" },   // Garand 0.4
        { 1636, 6, 1.2f, "adsViewBobMult" },   // Garand 0.4
        { 2052, 6, 1200.0f, "adsViewKickCenterSpeed" },   // Garand 1480
        { 2040, 6, 70.0f, "adsViewKickPitchMax" },   // Garand 78
        { 2036, 6, 50.0f, "adsViewKickPitchMin" },   // Garand 57
        { 2048, 6, -70.0f, "adsViewKickYawMax" },   // Garand 20
        { 2044, 6, -40.0f, "adsViewKickYawMin" },   // Garand -90
        { 1600, 6, 65.0f, "adsZoomFov" },   // Garand 40
        { 1608, 6, 0.3f, "adsZoomInFrac" },   // Garand 0.4
        { 1612, 6, 0.5f, "adsZoomOutFrac" },   // Garand 0.2
        { 2136, 6, 800.0f, "aiEffectiveRange" },   // Garand 700
        { 2140, 6, 1800.0f, "aiMissRange" },   // Garand 2000
        { 1844, 5, 1.0f, "animateCamFire" },   // Garand 0
        { 2228, 6, 0.3f, "bulletConeAngle" },   // Garand 0.2
        { 1476, 4, 10.0f, "clipSize" },   // Garand 8
        { 1904, 4, 100.0f, "dropAmmoMax" },   // Garand 104
        { 1900, 4, 90.0f, "dropAmmoMin" },   // Garand 96
        { 1572, 7, 0.4f, "dropTime" },   // Garand 0.5
        { 1396, 6, 0.0f, "duckedMoveMinSpeed" },   // Garand 10
        { 1324, 6, 1.0f, "duckedMoveR" },   // Garand 0.1
        { 1328, 6, -1.8f, "duckedMoveU" },   // Garand -1
        { 1308, 6, -1.1f, "duckedOfsF" },   // Garand -0.6
        { 1312, 6, -0.42f, "duckedOfsR" },   // Garand 0.12
        { 1316, 6, -0.72f, "duckedOfsU" },   // Garand -0.495
        { 1420, 6, 0.0f, "duckedRotMinSpeed" },   // Garand 10
        { 1332, 6, 0.0f, "duckedRotP" },   // Garand 0.05
        { 1340, 6, 0.0f, "duckedRotR" },   // Garand 0.05
        { 1336, 6, 0.0f, "duckedRotY" },   // Garand 0.05
        { 1520, 7, 0.1f, "fireDelay" },   // Garand 0
        { 1528, 7, 0.1f, "fireTime" },   // Garand 0.135
        { 1708, 6, 5.0f, "gunMaxPitch" },   // Garand 6
        { 2088, 6, 18.0f, "hipGunKickYawMax" },   // Garand 24
        { 2084, 6, -2.0f, "hipGunKickYawMin" },   // Garand -4
        { 1644, 6, 2.0f, "hipSpreadDuckedMin" },   // Garand 1.5
        { 1648, 6, 1.3f, "hipSpreadProneMin" },   // Garand 1
        { 1640, 6, 2.4f, "hipSpreadStandMin" },   // Garand 2
        { 2124, 6, 800.0f, "hipViewKickCenterSpeed" },   // Garand 1000
        { 2112, 6, 70.0f, "hipViewKickPitchMax" },   // Garand 65
        { 2120, 6, 20.0f, "hipViewKickYawMax" },   // Garand 35
        { 2116, 6, -20.0f, "hipViewKickYawMin" },   // Garand -35
        { 1472, 4, 120.0f, "maxAmmo" },   // Garand 128
        { 1512, 4, 150.0f, "meleeDamage" },   // Garand 90
        { 1524, 7, 0.25f, "meleeDelay" },   // Garand 0.1
        { 1868, 5, 0.0f, "noPartialReload" },   // Garand 1
        { 1380, 6, 6.0f, "posMoveRate" },   // Garand 5
        { 1384, 6, 10.0f, "posProneMoveRate" },   // Garand 7
        { 1404, 6, 6.0f, "posRotRate" },   // Garand 8
        { 1356, 6, -200.0f, "proneMoveF" },   // Garand -160
        { 1400, 6, 0.0f, "proneMoveMinSpeed" },   // Garand 10
        { 1364, 6, -105.0f, "proneMoveU" },   // Garand -120
        { 1348, 6, -0.28f, "proneOfsR" },   // Garand -0.19
        { 1352, 6, 0.2f, "proneOfsU" },   // Garand 0.43
        { 1424, 6, 0.0f, "proneRotMinSpeed" },   // Garand 10
        { 1576, 7, 0.4f, "raiseTime" },   // Garand 0.5
        { 1556, 7, 2.5f, "reloadAddTime" },   // Garand 0.83
        { 1552, 7, 3.45f, "reloadEmptyTime" },   // Garand 1.9; campaign 3.95, see reloadTime
        // Campaign 3.95; 3.45 here: in multiplayer the reload's last part
        // (seating the magazine, then the bolt) ends with the reload time, and
        // its one sound, GR_G43, started with the reload, has the magazine
        // seated at 2.4 s and the bolt at 3.0 s; at 3.95 the hands reached the
        // bolt about 0.5 s after its sound (0.5.900's timed screenshots).
        { 1548, 7, 3.45f, "reloadTime" },   // Garand 1.9
        { 1284, 6, 0.0f, "standMoveF" },   // Garand 0.2
        { 1392, 6, 0.0f, "standMoveMinSpeed" },   // Garand 10
        { 1288, 6, 0.4f, "standMoveR" },   // Garand 0.05
        { 1416, 6, 0.0f, "standRotMinSpeed" },   // Garand 10
        { 1296, 6, 0.0f, "standRotP" },   // Garand 0.1
        { 1304, 6, 0.0f, "standRotR" },   // Garand 0.1
        { 1300, 6, 0.0f, "standRotY" },   // Garand 0.1
        { 1452, 4, 120.0f, "startAmmo" },   // Garand 128
        { 1720, 6, 5.0f, "swayLerpSpeed" },   // Garand 6
        { 1716, 6, 4.0f, "swayMaxAngle" },   // Garand 3
        { 1724, 6, 0.6f, "swayPitchScale" },   // Garand 0.3
        { 1736, 6, 0.5f, "swayVertScale" },   // Garand 0.35
        { 1728, 6, 0.15f, "swayYawScale" },   // Garand 0.3
    };

    // The scoped Lee-Enfield's own numbers: where its campaign weapon file
    // (Laison, WEAPON_LEE_ENFIELD_SNIPER) differs from the scoped Kar98's,
    // its multiplayer value where it has one; damage kept the Kar98's
    // (balanced, as the G43's). Recorded by the 0.5.900 weapon dump.
    const G43Field LeeScopedFields[] = {
        { 1288, 6, 0.6f, "standMoveR" },   // scoped Kar98 0
        { 1292, 6, -1.0f, "standMoveU" },   // scoped Kar98 -1.2
        { 1308, 6, -1.34f, "duckedOfsF" },   // scoped Kar98 0
        { 1312, 6, -0.26f, "duckedOfsR" },   // scoped Kar98 0.3
        { 1316, 6, -0.26f, "duckedOfsU" },   // scoped Kar98 -0.7
        { 1324, 6, 1.0f, "duckedMoveR" },   // scoped Kar98 0.5
        { 1328, 6, -1.0f, "duckedMoveU" },   // scoped Kar98 -1.8
        { 1348, 6, -0.2f, "proneOfsR" },   // scoped Kar98 0.5
        { 1352, 6, 0.58f, "proneOfsU" },   // scoped Kar98 -1
        { 1364, 6, -80.0f, "proneMoveU" },   // scoped Kar98 -145
        { 1372, 6, 220.0f, "proneRotY" },   // scoped Kar98 300
        { 1376, 6, -300.0f, "proneRotR" },   // scoped Kar98 -220
        { 1380, 6, 4.0f, "posMoveRate" },   // scoped Kar98 5
        { 1384, 6, 20.0f, "posProneMoveRate" },   // scoped Kar98 8
        { 1392, 6, 80.0f, "standMoveMinSpeed" },   // scoped Kar98 0
        { 1396, 6, 20.0f, "duckedMoveMinSpeed" },   // scoped Kar98 0
        { 1404, 6, 5.0f, "posRotRate" },   // scoped Kar98 4
        { 1408, 6, 30.0f, "posProneRotRate" },   // scoped Kar98 10
        { 1416, 6, 80.0f, "standRotMinSpeed" },   // scoped Kar98 0
        { 1420, 6, 20.0f, "duckedRotMinSpeed" },   // scoped Kar98 0
        { 1452, 4, 100.0f, "startAmmo" },   // scoped Kar98 125
        { 1472, 4, 125.0f, "maxAmmoMP" },   // scoped Kar98 None
        { 1476, 4, 10.0f, "clipSize" },   // scoped Kar98 5
        // fireDelayMP: the campaign file's 0.1 s; in multiplayer a shot after
        // a delay never took a round from the clip (endless rounds, no
        // reload, hand test): the scoped Kar98's 0.
        { 1520, 7, 0.0f, "fireDelayMP" },   // campaign 0.1, scoped Kar98 0
        { 1524, 7, 0.25f, "meleeDelayMP" },   // scoped Kar98 None
        { 1532, 7, 1.07f, "rechamberTime" },   // scoped Kar98 1.1
        { 1536, 7, 0.65f, "rechamberBoltTime" },   // scoped Kar98 .1
        { 1548, 7, 0.7f, "reloadTime" },   // scoped Kar98 0.6
        { 1552, 7, 0.7f, "reloadEmptyTime" },   // scoped Kar98 0.6
        { 1556, 7, 0.6f, "reloadAddTime" },   // scoped Kar98 0.4
        { 1560, 7, 1.4f, "reloadStartTime" },   // scoped Kar98 1.8
        { 1568, 7, 1.0f, "reloadEndTime" },   // scoped Kar98 0.77
        { 1600, 6, 15.0f, "adsZoomFovMP" },   // scoped Kar98 10
        { 1604, 6, 1.0f, "adsSensitivityScaleMP" },   // scoped Kar98 1.5
        { 1608, 6, 0.42f, "adsZoomInFrac" },   // scoped Kar98 0.5
        { 1636, 6, 0.2f, "adsViewBobMult" },   // scoped Kar98 1
        { 1640, 6, 1.5f, "hipSpreadStandMinMP" },   // scoped Kar98 3
        { 1644, 6, 4.5f, "hipSpreadDuckedMinMP" },   // scoped Kar98 2.75
        { 1648, 6, 3.5f, "hipSpreadProneMinMP" },   // scoped Kar98 2.5
        { 1652, 6, 5.5f, "hipSpreadMaxMP" },   // scoped Kar98 6
        { 1656, 6, 1.5f, "hipSpreadDecayRateMP" },   // scoped Kar98 5
        { 1660, 6, 0.25f, "hipSpreadFireAddMP" },   // scoped Kar98 1
        { 1668, 6, 0.0f, "hipSpreadMoveAddMP" },   // scoped Kar98 10
        { 1672, 6, 1.375f, "hipSpreadDuckedDecayMP" },   // scoped Kar98 1
        { 1692, 6, 40.0f, "adsIdleAmount" },   // scoped Kar98 50
        { 1700, 6, 0.5f, "idleCrouchFactor" },   // scoped Kar98 0.75
        { 1704, 6, 0.2f, "idleProneFactor" },   // scoped Kar98 0.4
        { 1708, 6, 80.0f, "gunMaxPitch" },   // scoped Kar98 4
        { 1712, 6, 15.0f, "gunMaxYaw" },   // scoped Kar98 5
        { 1716, 6, 3.0f, "swayMaxAngle" },   // scoped Kar98 4
        { 1720, 6, 6.0f, "swayLerpSpeed" },   // scoped Kar98 5
        { 1724, 6, 0.3f, "swayPitchScale" },   // scoped Kar98 0.12
        { 1728, 6, 0.175f, "swayYawScale" },   // scoped Kar98 0.1
        { 1732, 6, 0.3f, "swayHorizScale" },   // scoped Kar98 0.24
        { 1736, 6, 0.6f, "swayVertScale" },   // scoped Kar98 0.5
        { 1748, 6, 5.0f, "adsSwayLerpSpeed" },   // scoped Kar98 6
        // rechamberWhileAds: the campaign file's 1 kept the bolt work behind
        // the scope (the rack seen only after the first shot, hand test):
        // the scoped Kar98's 0, out of the scope for each round, as in
        // multiplayer.
        { 1788, 5, 0.0f, "rechamberWhileAds" },   // campaign 1, scoped Kar98 0
        { 1900, 4, 50.0f, "dropAmmoMin" },   // scoped Kar98 10
        { 1904, 4, 75.0f, "dropAmmoMax" },   // scoped Kar98 30
        { 2004, 6, -85.0f, "adsGunKickPitchMin" },   // scoped Kar98 -10
        { 2008, 6, -90.0f, "adsGunKickPitchMax" },   // scoped Kar98 -15
        { 2012, 6, 16.0f, "adsGunKickYawMin" },   // scoped Kar98 -5
        { 2016, 6, 18.0f, "adsGunKickYawMax" },   // scoped Kar98 5
        { 2020, 6, 400.0f, "adsGunKickAccel" },   // scoped Kar98 600
        { 2024, 6, 300.0f, "adsGunKickSpeedMax" },   // scoped Kar98 600
        { 2036, 6, 60.0f, "adsViewKickPitchMin" },   // scoped Kar98 50
        { 2040, 6, 67.0f, "adsViewKickPitchMax" },   // scoped Kar98 55
        { 2044, 6, 15.0f, "adsViewKickYawMin" },   // scoped Kar98 -15
        { 2048, 6, 18.0f, "adsViewKickYawMax" },   // scoped Kar98 -20
        { 2052, 6, 1200.0f, "adsViewKickCenterSpeed" },   // scoped Kar98 650
        { 2064, 6, 0.05f, "adsSpreadMP" },   // scoped Kar98 0.03
        { 2068, 6, 0.05f, "adsSpreadDuckedMP" },   // scoped Kar98 0.02
        { 2072, 6, 0.05f, "adsSpreadProneMP" },   // scoped Kar98 0.01
        { 2116, 6, -20.0f, "hipViewKickYawMin" },   // scoped Kar98 100
        { 2148, 7, 0.6f, "adsReloadTransTime" },   // scoped Kar98 0.8
        { 2228, 6, 0.2f, "bulletConeAngle" },   // scoped Kar98 0.1
        { 2232, 6, 0.4f, "adsBulletConeAngle" },   // scoped Kar98 0.2
    };

    void Reload(uint8_t* base, const CampaignGun& gun, uint32_t weapon)
    {
        const uint32_t was = Guest::Read32(base, weapon + 1548);
        // The times are kept in milliseconds; should they be floats in
        // seconds instead (a float's bits are far above any time), as that.
        const bool floats = was > 0x10000000u;
        auto time = [&](uint32_t at, float seconds)
        {
            if (floats) { uint32_t bits; memcpy(&bits, &seconds, 4); Guest::Write32(base, weapon + at, bits); }
            else Guest::Write32(base, weapon + at, uint32_t(seconds * 1000.0f + 0.5f));
        };
        printf("weapons: %s's copy had clip %s of %u, reload %08X (empty %08X, add %08X, start %08X, start add %08X, end %08X), in parts %u, add %u, start add %u\n",
               gun.name, ReadText(base, Guest::Read32(base, weapon + 1464)).c_str(), Guest::Read32(base, weapon + 1476), was,
               Guest::Read32(base, weapon + 1552), Guest::Read32(base, weapon + 1556), Guest::Read32(base, weapon + 1560),
               Guest::Read32(base, weapon + 1564), Guest::Read32(base, weapon + 1568), Guest::Read32(base, weapon + 1872),
               Guest::Read32(base, weapon + 1876), Guest::Read32(base, weapon + 1880));
        // The scoped Lee-Enfield's rounds in the Lee-Enfield's clip (both
        // 10 rounds; tested with it: the rounds count down and it reloads).
        Guest::Write32(base, weapon + 1464, GuestText(base, strcmp(gun.name, "lee_enfield_sniper") == 0 ? "lee_enfield" : gun.name));
        Guest::Write32(base, weapon + 1476, uint32_t(gun.clipSize));
        if (gun.reloadAdd > 0)
        {
            Guest::Write32(base, weapon + 1872, 1);
            Guest::Write32(base, weapon + 1876, uint32_t(gun.reloadAdd));
            Guest::Write32(base, weapon + 1880, 0);
            time(1560, gun.reloadStart);
            time(1564, gun.reloadStart);
            time(1548, gun.reloadEach);
            time(1552, gun.reloadEach);
            time(1556, gun.reloadEach * 0.75f);
            time(1568, gun.reloadEnd);
            // The start's and end's animations' names (+112, +116), after
            // the reload's (+104).
            const std::string reload = ReadText(base, Guest::Read32(base, weapon + 104));
            if (!reload.empty())
            {
                Guest::Write32(base, weapon + 112, GuestText(base, reload + "_start2"));
                Guest::Write32(base, weapon + 116, GuestText(base, reload + "_end2"));
            }
        }
        // A reload in one animation: its times set to the animation's length
        // (the ammunition put in at 80% of it).
        // The ini's sten_reload, fg42_reload and bren_reload, when set, in
        // place of the times here (tuning without a build).
        float whole = gun.reloadWhole;
        {
            const Settings::Values& v = Settings::Get();
            const float asked = strcmp(gun.name, "sten") == 0 ? v.stenReload : strcmp(gun.name, "fg42") == 0 ? v.fg42Reload
                              : strcmp(gun.name, "bren") == 0 ? v.brenReload : strcmp(gun.name, "gewehr43") == 0 ? v.g43Reload : 0.0f;
            if (asked > 0.3f) { whole = asked; printf("weapons: %s's reload %.2f s, from the ini\n", gun.name, asked); }
        }
        // The sights' field of view (+1600, adsZoomFov, a float): the copy's
        // (the BAR's for the FG42), or the ini's (fg42_ads_fov, ...).
        {
            const Settings::Values& v = Settings::Get();
            const float fov = strcmp(gun.name, "fg42") == 0 ? v.fg42AdsFov : strcmp(gun.name, "sten") == 0 ? v.stenAdsFov
                            : strcmp(gun.name, "bren") == 0 ? v.brenAdsFov : strcmp(gun.name, "lee_enfield") == 0 ? v.enfieldAdsFov : 0.0f;
            const uint32_t bits = Guest::Read32(base, weapon + 1600);
            float was; memcpy(&was, &bits, 4);
            if (fov >= 10.0f) { uint32_t put; memcpy(&put, &fov, 4); Guest::Write32(base, weapon + 1600, put); }
            printf("weapons: %s's sights field of view %.1f (its copy's %.1f)\n", gun.name, fov >= 10.0f ? fov : was, was);
        }
        if (gun.reloadAdd <= 0 && whole > 0.0f)
        {
            time(1548, whole);
            time(1552, whole);
            time(1556, whole * 0.8f);
        }
        // Putting it away (+1572 dropTime): the Lee-Enfield's own drop
        // (w_brRFL_drop) takes the rifle out of sight in its first part and
        // shows nothing after, and its raise (w_brRFL_raise) shows it only
        // late: at the Kar98's times (400 and 500 ms) a switch sat on empty
        // hands for about half a second either way (video of 0.5.17: 60%
        // of the drop still left 0.4 s). dropPercent and raisePercent of
        // the copy's: 30% and 70%.
        {
            const uint32_t drop = Guest::Read32(base, weapon + 1572), raise = Guest::Read32(base, weapon + 1576);
            if (!floats && drop > 0 && drop < 5000 && gun.dropPercent != 100) Guest::Write32(base, weapon + 1572, drop * uint32_t(gun.dropPercent) / 100);
            if (!floats && raise > 0 && raise < 5000 && gun.raisePercent != 100) Guest::Write32(base, weapon + 1576, raise * uint32_t(gun.raisePercent) / 100);
            printf("weapons: %s's drop time %u ms (was %u), raise time %u ms (was %u)\n", gun.name, Guest::Read32(base, weapon + 1572), drop,
                   Guest::Read32(base, weapon + 1576), raise);
        }
        // The G43's own handling, ammunition and timings (G43Fields, above);
        // the ini's g43_reload, when set, keeps its reload times.
        if (strcmp(gun.name, "gewehr43") == 0)
        {
            const bool iniReload = Settings::Get().g43Reload > 0.3f;
            int set = 0;
            for (const G43Field& f : G43Fields)
            {
                if (iniReload && (f.offset == 1548 || f.offset == 1552 || f.offset == 1556)) continue;
                if (f.kind == 6) { uint32_t bits; memcpy(&bits, &f.value, 4); Guest::Write32(base, weapon + f.offset, bits); }
                else if (f.kind == 7) time(f.offset, f.value);
                else Guest::Write32(base, weapon + f.offset, uint32_t(int32_t(f.value)));
                set++;
            }
            printf("weapons: %s's own numbers: %d fields from its campaign weapon file\n", gun.name, set);
        }
        if (strcmp(gun.name, "lee_enfield_sniper") == 0)
        {
            int set = 0;
            for (const G43Field& f : LeeScopedFields)
            {
                if (f.kind == 6) { uint32_t bits; memcpy(&bits, &f.value, 4); Guest::Write32(base, weapon + f.offset, bits); }
                else if (f.kind == 7) time(f.offset, f.value);
                else Guest::Write32(base, weapon + f.offset, uint32_t(int32_t(f.value)));
                set++;
            }
            // its scope (attach1Model +28, attach1Tag +48), its sights'
            // bolt move (adsRechamberAnim +144) and its kill icon (+1860)
            Guest::Write32(base, weapon + 28, GuestText(base, "lee_enfield_scope"));
            Guest::Write32(base, weapon + 48, GuestText(base, "w_lee_enfield"));
            Guest::Write32(base, weapon + 144, GuestText(base, "w_arms_brRFL_ADSrechamber"));
            Guest::Write32(base, weapon + 1860, GuestText(base, gun.hud));
            printf("weapons: %s's own numbers: %d fields from its campaign weapon file, its scope lee_enfield_scope\n", gun.name, set);
        }
        printf("weapons: %s gets a clip of %d%s (times in %s)\n", gun.name, gun.clipSize,
               gun.reloadAdd > 0 ? ", reloaded in parts" : gun.reloadWhole > 0.0f ? ", its own reload time" : "", floats ? "seconds" : "milliseconds");
        fflush(stdout);
    }

    void MakeGun(PPCContext& ctx, uint8_t* base, const CampaignGun& gun, uint32_t entry)
    {
        // 0.5.292: weapons 1..63 only: a player's weapons are a 64-bit mask
        // (his state's +1056..+1063, the weapon slots right after, +1064), so
        // weapon 64 and on would be written over his slots. Fuel Plant's map
        // has 54: with the 9 campaign guns it is full (63).
        if (int32_t(Guest::Read32(base, 0x82BCBF94)) >= 63)
        {
            printf("weapons: %s left out: the game's 63 weapons are all in use on this map\n", gun.name);
            fflush(stdout);
            return;
        }
        // Set up under the copied gun's own name, so that what the set-up
        // finds by the weapon's name (its effects on each surface, "kar98_
        // METAL" and the rest, by sub_82490220) is found; then renamed: its
        // name at +8 and that name's hash at +4.
        const uint32_t name = GuestText(base, gun.name);
        PPCContext call = ctx;
        call.r3.u64 = GuestText(base, gun.from);
        call.r4.u64 = entry;
        __imp__sub_8249A380(call, base);
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t weapon = list && count > 0 ? Guest::Read32(base, list + 4 * uint32_t(count)) : 0;
        if (weapon)
        {
            Guest::Write32(base, weapon + 8, name);
            PPCContext hash = ctx;
            hash.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
            hash.r3.u64 = name;
            hash.lr = 0;
            Guest::Lookup(0x820E10E0)(hash, base);
            Guest::Write32(base, weapon + 4, hash.r3.u32);
        }
        if (!weapon || ReadText(base, Guest::Read32(base, weapon + 8)) != gun.name)
        {
            printf("weapons: %s could not be made (weapon %d %08X)\n", gun.name, count, weapon);
            fflush(stdout);
            return;
        }
        Guest::Write32(base, weapon + 12, GuestText(base, gun.title));
        const int test = Settings::Get().campaignTest;
        if (test != 2 && gun.viewModel) Guest::Write32(base, weapon + 20, GuestText(base, gun.viewModel));
        // The world models: +1432 the one in a player's hands, +1436 the one
        // lying on the ground when dropped (sub_82486BF8, the items: +1436
        // when it is set, else +1432). The BAR's and Kar98's are set to their
        // own (the Sten's copy, the Thompson, has none, so the Sten was the
        // only one dropped right): both are the campaign gun's world model.
        g_copyWorld[&gun - Guns] = ReadText(base, Guest::Read32(base, weapon + 1432));
        printf("weapons: %s's copy had item %s, world models %s and %s, +1444 %s, +1448 %s\n", gun.name,
               ReadText(base, Guest::Read32(base, weapon + 1428)).c_str(), ReadText(base, Guest::Read32(base, weapon + 1432)).c_str(),
               ReadText(base, Guest::Read32(base, weapon + 1436)).c_str(), ReadText(base, Guest::Read32(base, weapon + 1444)).c_str(),
               ReadText(base, Guest::Read32(base, weapon + 1448)).c_str());
        // A grenade (no animations of its own): its view model, when it is
        // the copy's world model (the frag's first-person grenade is its
        // world model in the hands, w_us_nade_world), is its own world
        // model too: the Mills bomb in the hand.
        if (!gun.armsFrom)
        {
            const std::string view = ReadText(base, Guest::Read32(base, weapon + 20));
            const std::string world = ReadText(base, Guest::Read32(base, weapon + 1432));
            auto lower = [](std::string t) { for (char& c : t) c = char(tolower(static_cast<unsigned char>(c))); return t; };
            const bool same = !view.empty() && lower(view) == lower(world);
            printf("weapons: %s's copy's view model %s, world model %s%s\n", gun.name, view.c_str(), world.c_str(),
                   same ? ": its own world model in the hands too" : "");
            if (same) Guest::Write32(base, weapon + 20, GuestText(base, gun.worldModel));
        }
        Guest::Write32(base, weapon + 1432, GuestText(base, gun.worldModel));
        // Its item's own name (+1428, hashed for the items list,
        // sub_82486BF8): the copy's ("weapon_thompson", "weapon_bar") was
        // shared, so a dropped campaign gun's item was the copy's too.
        Guest::Write32(base, weapon + 1428, GuestText(base, std::string("weapon_") + gun.name));
        if (Guest::Read32(base, weapon + 1436) != 0) Guest::Write32(base, weapon + 1436, GuestText(base, gun.worldModel));
        // A grenade's (no animations of its own): its thrown model too
        // (+1932, projectileModel), when it has one.
        if (!gun.armsFrom && !ReadText(base, Guest::Read32(base, weapon + 1932)).empty())
        {
            printf("weapons: %s's thrown model %s -> %s\n", gun.name, ReadText(base, Guest::Read32(base, weapon + 1932)).c_str(), gun.worldModel);
            Guest::Write32(base, weapon + 1932, GuestText(base, gun.worldModel));
        }
        // The HUD icon (+1440, the pick-up prompt's): the campaign's own are
        // found (0.5.35) but drew as a white box (the Sten's) or a hand
        // (the others'); the copy's own icon is kept until they draw.
        if (Settings::Get().campaignIcons) Guest::Write32(base, weapon + 1440, GuestText(base, gun.hud));
        // Its gunshots: the copy's sound names (THOMPSON_FIRE_1ST and _3RD,
        // ...) in the weapon's fields become the campaign gun's own, which
        // the campaign banks loaded beside the map's have (STEN_FIRE_1ST in
        // fuelplnt.wbk, BREN_FIRE_1ST in stbert.wbk, ...). The shots' events
        // keep the copy's name, so its muzzle flash stays (0.5.31 renamed
        // the events, and lost the flashes).
        if (Settings::Get().campaignSounds && Settings::Get().campaignShotSounds && gun.shot)
        {
            std::string from = CampaignCopyShotPrefix(gun.from) + "_FIRE_";
            int changed = 0;
            for (uint32_t at = 0; at < 2400; at += 4)
            {
                const uint32_t text = Guest::Read32(base, weapon + at);
                if (text < 0x40000000u || text >= 0x90000000u) continue;
                std::string value = ReadText(base, text, 48), upper = value;
                for (char& c : upper) c = char(toupper(static_cast<unsigned char>(c)));
                if (upper != from + "1ST" && upper != from + "3RD") continue;
                const std::string own = CampaignShotCue(gun.name, gun.shot, upper.substr(from.size()));
                Guest::Write32(base, weapon + at, GuestText(base, own));
                printf("weapons: %s's gunshot +%u: %s -> %s\n", gun.name, at, value.c_str(), own.c_str());
                changed++;
            }
            if (!changed)
            {
                printf("weapons: %s: no %s1ST/3RD sound field found; its sound-like texts:", gun.name, from.c_str());
                for (uint32_t at = 0; at < 2400; at += 4)
                {
                    const uint32_t text = Guest::Read32(base, weapon + at);
                    if (text < 0x40000000u || text >= 0x90000000u) continue;
                    std::string value = ReadText(base, text, 48), upper = value;
                    for (char& c : upper) c = char(toupper(static_cast<unsigned char>(c)));
                    if (upper.find("FIRE") != std::string::npos || upper.find("RINGOFF") != std::string::npos || upper.find("SNIP") != std::string::npos ||
                        upper.find("SHOT") != std::string::npos)
                        printf(" +%u %s", at, value.c_str());
                }
                printf("\n");
            }
        }
        int renamed = 0;
        for (uint32_t at = 72; at <= 164 && test != 1 && gun.armsFrom; at += 4)
        {
            std::string anim = ReadText(base, Guest::Read32(base, weapon + at));
            printf("weapons:   %s field +%u: %s\n", gun.name, at, anim.c_str());
            const size_t found = anim.find(gun.armsFrom);
            if (found == std::string::npos) continue;
            if (KeepsCopyIdle(gun.name, anim)) continue;
            anim.replace(found, strlen(gun.armsFrom), gun.armsTo);
            anim = CampaignAnimationName(gun.name, anim);
            // The Kar98's reload's start and end are the Lee-Enfield's "2".
            // (Only for a reload in parts made here; the scoped Lee-Enfield's
            // are its own reload_start and reload_end, as the scoped Kar98's.)
            if (gun.reloadAdd > 0 && anim.size() > 6 && (anim.compare(anim.size() - 6, 6, "_start") == 0 || anim.compare(anim.size() - 4, 4, "_end") == 0)) anim += "2";
            Guest::Write32(base, weapon + at, GuestText(base, anim));
            renamed++;
        }
        if (gun.clipSize > 0 && test != 1) Reload(base, gun, weapon);
        // The MG42's fire rate: its famous ~1200 rounds a minute, 0.05 s a
        // shot (+1528 fireTime; the MG34's clip and the rest kept, so no
        // Reload() for it). Milliseconds, or a float in seconds where the
        // copy keeps them so (as Reload() tells by the reload time).
        // The MG42 held, not deployed: the MG34's aim is its bipod's (aiming
        // deploys it, on a ledge or by lying down, "Prone blocked"); with the
        // deployed animations (lmgDeployedAnim +148, lmgDeployAnim +152,
        // lmgBreakdownAnim +156) left out it aims from the shoulder, as the
        // BAR does. Its class (+176) logged beside the BAR's to compare;
        // COD3_MG42_CLASS=n sets it (testing).
        if (strcmp(gun.name, "mg42") == 0 && test != 1 && !getenv("COD3_MG42_DEPLOY"))
        {
            const uint32_t none = GuestText(base, "");
            printf("weapons: mg42's bipod animations %s / %s / %s left out; class %u\n", ReadText(base, Guest::Read32(base, weapon + 148)).c_str(),
                   ReadText(base, Guest::Read32(base, weapon + 152)).c_str(), ReadText(base, Guest::Read32(base, weapon + 156)).c_str(), Guest::Read32(base, weapon + 176));
            for (uint32_t at : { 148u, 152u, 156u }) Guest::Write32(base, weapon + at, none);
            {   // every weapon's class, to choose the MG42's
                const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
                const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                std::string all;
                for (int32_t k = 1; list && k <= count && k < 64; k++)
                {
                    const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
                    if (!def) continue;
                    char one[96]; snprintf(one, sizeof(one), " %s=%u", ReadText(base, Guest::Read32(base, def + 8), 32).c_str(), Guest::Read32(base, def + 176));
                    all += one;
                }
                printf("weapons: classes:%s\n", all.c_str());
            }
            // the BAR's class (a rifle's: aimed from the shoulder); the MG34's (3) deploys
            uint32_t barClass = 0;
            {
                const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
                const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                for (int32_t k = 1; list && k <= count && k < 64; k++)
                {
                    const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
                    if (def && ReadText(base, Guest::Read32(base, def + 8), 32) == "bar") barClass = Guest::Read32(base, def + 176);
                }
            }
            Guest::Write32(base, weapon + 176, barClass);
            if (const char* c = getenv("COD3_MG42_CLASS")) Guest::Write32(base, weapon + 176, uint32_t(atoi(c)));
            printf("weapons: mg42's class now %u (the BAR's)\n", Guest::Read32(base, weapon + 176));
        }
        if (strcmp(gun.name, "mg42") == 0 && test != 1)
        {
            const uint32_t was = Guest::Read32(base, weapon + 1528);
            if (Guest::Read32(base, weapon + 1548) > 0x10000000u) { const float seconds = 0.05f; uint32_t bits; memcpy(&bits, &seconds, 4); Guest::Write32(base, weapon + 1528, bits); }
            else Guest::Write32(base, weapon + 1528, 50);
            printf("weapons: mg42's fireTime %08X -> 50 ms\n", was);
        }
        g_campaignWeapon[&gun - Guns] = count;
        printf("weapons: %s made as weapon %d (%08X) from %s, %d animations renamed (test %d)\n", gun.name, count, weapon, gun.from, renamed, test);
        fflush(stdout);
    }
}

// Each weapon's set-up: the entries of the guns the campaign ones are
// copies of are kept, and the copies made after the last weapon is set up
// (sub_8249E970, below), so they come at the end of the list and no
// multiplayer weapon's number moves.
namespace { uint32_t g_copyFrom[sizeof(Guns) / sizeof(Guns[0])] = {}; }
namespace
{
    // Each campaign weapons archive: its number for this map (-1: not used
    // here), and the number and object it was loaded as (kept: archives are
    // never let go, and are used again by the maps after).
    int32_t g_archiveActive[ArchiveCount] = { -1, -1, -1, -1 };
    // Merville's archive, loaded beside a map without vehicles of its own
    // (the console's vehicles_everywhere 1, before the map): its vehicle
    // files, models and effects. -1: not on this map.
    int32_t g_vehicleArchive = -1;
    int32_t g_mapArchive = -1;   // the map's own archive's number (kind 3), 0.5.85
    bool g_kangarooWaiting = false;   // its archive put off till the map's is in
    bool g_kangarooPendingMap() { return g_kangarooWaiting; }
    int32_t g_kangarooArchive = -1;
    bool g_stukaEarly = false;   // 0.5.271
    int32_t g_mortarArchive = -1;   // 0.5.285: Laison's vehicles (the mortar's v_81mm)
    int32_t g_laisonArchive = -1, g_laisonFxArchive = -1, g_laisonZoneArchive = -1;   // 0.5.291: Laison's own archive and effects (the campaign mortar); 0.5.298 zone 8's arms
    bool g_laisonWeaponFilter = false, g_laisonVehicleFilter = false;   // 0.5.291: reading only the mortar's files from it
    int8_t g_archiveKind[64] = {};   // 0.5.294: each archive number's kind as opened (3 a map, 4 its zones; sub_82477B50)
    bool g_laisonPending = false;     // 0.5.296: Laison's archive waits for the map's (MpFixesLoadLaisonAfterMap)
    int32_t g_laisonWeapon[4] = {};   // 0.5.303: the weapon numbers Laison's two were given (set up for drawing like the campaign guns)
    uint32_t g_laisonManager = 0, g_laisonBank = 0;
    int32_t g_planeArchiveP47 = -1, g_planeArchiveStuka = -1;   // 0.5.253: the UAV's planes (MpFixesPlaneFrame)   // the Kangaroo's (sp\mace2\kangaroo.cod), when asked for
    int32_t g_archiveLoaded[ArchiveCount] = { -1, -1, -1, -1 };
    uint32_t g_archiveObject[ArchiveCount] = {};
}
PPC_FUNC(sub_8249A380)
{
    const std::string name = ReadText(base, ctx.r3.u32, 40);
    const uint32_t entry = ctx.r4.u32;
    // 0.5.291: from Laison's own archive only the mortar's weapons (its
    // turret, mortar_81mm_turret, and its round, mortar_shell); the other
    // 17 (mp40, gewehr43, the Shermans' guns ...) left out.
    if (g_laisonWeaponFilter)
    {
        const bool wanted = name == "mortar_81mm_turret" || name == "mortar_shell";
        printf("mortar: Laison's weapon file %s %s\n", name.c_str(), wanted ? "set up" : "left out");
        fflush(stdout);
        if (!wanted) return;
        __imp__sub_8249A380(ctx, base);
        return;
    }
    __imp__sub_8249A380(ctx, base);
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
        if (name == Guns[i].from) g_copyFrom[i] = entry;
}

// The weapons set up from the map's WEAPONFILE entries (sub_8249E970), then
// the campaign guns after them.
void MpFixesLaisonWeapons(PPCContext& saved, uint8_t* base);   // 0.5.296 (below)
bool MakeMortarArchives();   // 0.5.298 (below)
extern "C" PPC_FUNC(__imp__sub_8249E970);
PPC_FUNC(sub_8249E970)
{
    memset(g_copyFrom, 0, sizeof(g_copyFrom));
    memset(g_campaignWeapon, 0, sizeof(g_campaignWeapon));
    PPCContext saved = ctx;
    __imp__sub_8249E970(ctx, base);
    const int32_t before = int32_t(Guest::Read32(base, 0x82BCBF94));
    if (Settings::Get().campaignGuns)
        for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
            if (g_copyFrom[i] && g_archiveActive[Guns[i].archive] >= 0) MakeGun(saved, base, Guns[i], g_copyFrom[i]);
    // 0.5.291: the campaign mortar's weapons, from Laison's own archive (its
    // WEAPONFILE entries read as the map's are: sub_8254B318 with the
    // archives, the archive's number, "WEAPONFILE", the set-up sub_8249A380),
    // only those two (g_laisonWeaponFilter), last in the list.
    // 0.5.293: the game's own weapon tables have room for 91 (the list, 368
    // bytes at 0x82BCBF9C, sub_824A1058; the drawing info at 0x82C83810,
    // 92 x 192); only what a player owns is held to 63 (his 64-bit mask,
    // state +1056). The mortar's turret and round are a vehicle's and a
    // projectile's, never a player's: with COD3_WEAPONS_PAST_63=1 (test)
    // they may be 64 and 65 on a full map (Fuel Plant).
    printf("weapons: %d set up from the map, %d with the campaign's\n", before, int32_t(Guest::Read32(base, 0x82BCBF94)));
    fflush(stdout);
}
// 0.5.310: the campaign round in the multiplayer's arms. 303 crashed in the
// state's player anim holding mortar_shell (its arms are the campaign's);
// 304's "interact" (the multiplayer's interaction arms) plays the moves but
// holds nothing. So mortar_shell keeps its own model (the round, +20) and
// takes interact's arms (+24) and arms' moves (+72..+164) before it is set
// up for drawing (MpFixesCampaignGuns, later). COD3_MORTAR_ROUND=0: off
// (304's bare arms).
bool g_mortarRoundArms = false;
uint32_t WeaponDefNamed(uint8_t* base, const char* want, int32_t* index = nullptr)
{
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
    for (int32_t k = 1; list && k <= count && k < 92; k++)
    {
        const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
        const uint32_t text = def ? Guest::Read32(base, def + 8) : 0;
        if (text < 0x40000000u || text >= 0x90000000u) continue;
        if (ReadText(base, text, 40) != want) continue;
        if (index) *index = k;
        return def;
    }
    return 0;
}
void MortarRoundOnMpArms(uint8_t* base)
{
    g_mortarRoundArms = false;
    static const bool off = []() { const char* t = getenv("COD3_MORTAR_ROUND"); return t && t[0] == '0'; }();
    int32_t ri = 0, ii = 0;
    const uint32_t round = WeaponDefNamed(base, "mortar_shell", &ri), arms = WeaponDefNamed(base, "interact", &ii);
    auto text = [&](uint32_t def, uint32_t at) -> std::string
    {
        const uint32_t t = Guest::Read32(base, def + at);
        return t >= 0x40000000u && t < 0x90000000u ? ReadText(base, t, 40) : std::string("-");
    };
    if (!round || !arms) { printf("mortar: round on the MP arms: mortar_shell %08X interact %08X: not both here\n", round, arms); return; }
    printf("mortar: mortar_shell (%d) gun '%s' hands '%s'; interact (%d) gun '%s' hands '%s'\n", ri, text(round, 20).c_str(), text(round, 24).c_str(), ii, text(arms, 20).c_str(), text(arms, 24).c_str());
    for (uint32_t at = 72; at <= 164; at += 4)
    {
        const std::string a = text(round, at), b = text(arms, at);
        if (a != "-" || b != "-") printf("mortar:   anim +%u: mortar_shell '%s' interact '%s'\n", at, a.c_str(), b.c_str());
    }
    // 0.5.311: 310 TESTED: crashed the moment it came out (the same chain
    // as 302/303: sub_82515408 -> 820B85C8 -> 820B6570 -> 8218A290 ->
    // 82189E68, a call through garbage). Both use the same hands
    // (viewmodel_hands_us), so it is the round's own model (v_81mm_shell)
    // in the arms' animation tree that breaks the mortar's moves. The round
    // is not put in the hands any more; it is fed into the tube as a model
    // in the world instead (MortarFeedFrame), which everyone sees.
    if (!off || off) { printf("mortar: the round stays out of the hands (310's crash): fed into the tube in the world\n"); return; }
    if (Guest::Read8(base, 0x82C83810 + 192 * uint32_t(ri) + 124)) { printf("mortar: mortar_shell already set up for drawing: its arms left as they are\n"); return; }
    printf("mortar: class (+176) mortar_shell %u interact %u; +172 mortar_shell %u interact %u: the round takes interact's class (no firing)\n",
           Guest::Read32(base, round + 176), Guest::Read32(base, arms + 176), Guest::Read32(base, round + 172), Guest::Read32(base, arms + 172));
    Guest::Write32(base, round + 176, Guest::Read32(base, arms + 176));
    Guest::Write32(base, round + 24, Guest::Read32(base, arms + 24));
    for (uint32_t at = 72; at <= 164; at += 4) Guest::Write32(base, round + at, Guest::Read32(base, arms + at));
    g_mortarRoundArms = true;
    printf("mortar: mortar_shell now on the multiplayer's arms (hands '%s'), the round its model\n", text(round, 24).c_str());
}

// 0.5.296: from MpFixesLoadLaisonAfterMap (Laison's archive now comes after
// the map, so after the weapons' set-up: these two are appended then).
void MpFixesLaisonWeapons(PPCContext& saved, uint8_t* base)
{
    static const bool past63 = []() { const char* t = getenv("COD3_WEAPONS_PAST_63"); return t && t[0] == '1'; }();
    const int32_t laisonRoom = past63 ? 91 : 63;
    if (g_laisonArchive >= 0 && int32_t(Guest::Read32(base, 0x82BCBF94)) + 2 > laisonRoom)
        printf("mortar: no room for Laison's 2 weapons (%d of %d in use): the campaign mortar's turret and round left out on this map\n",
               int32_t(Guest::Read32(base, 0x82BCBF94)), laisonRoom);
    else if (g_laisonArchive >= 0)
    {
        const int32_t was = int32_t(Guest::Read32(base, 0x82BCBF94));
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = Guest::Read32(base, 0x82BCBE1C); call.r4.u64 = uint32_t(g_laisonArchive); call.r5.u64 = 0x82076044; call.r6.u64 = 0x8249A380; call.lr = 0;
        g_laisonWeaponFilter = true;
        Guest::Lookup(0x8254B318)(call, base);
        g_laisonWeaponFilter = false;
        const int32_t now = int32_t(Guest::Read32(base, 0x82BCBF94));
        for (int32_t k = 0; k < 4; k++) g_laisonWeapon[k] = was + 1 + k <= now ? was + 1 + k : 0;
        printf("mortar: Laison's weapons read: %d -> %d weapons\n", was, now);
        MortarRoundOnMpArms(base);   // 0.5.310
    }
    fflush(stdout);
}


// The archives an asset is looked for in (sub_82474CB8: a bank's number,
// then a list of archive numbers filled in, their count at +128, at most
// 32): the bank's own and what it needs, then the two global ones. The
// script's SetModel, and the rest, find a model only in these (sub_820D71E8),
// so the campaign weapons archive is put at the end of every list.
// 0.5.314: the campaign mortar's loading step (interaction type 0x10,
// MortarLoad: vtable 0x8206AF40, its update sub_82521950) presses one of the
// player's buttons once the loading moves are done (sub_82543290(the
// button, 1): the campaign fires the round in the hands that way) and holds
// it every frame after (sub_825430A0) until its round is gone. In
// multiplayer no round goes, so it held that button for good: 313's camera
// "shooting off" while firing, the black screen after a put-away mid-load,
// 312's aim ring flipping after the first shot. While this machine's
// player is on the mortar in that step, those two calls are left out (the
// mortar is fired by us).
namespace
{
    bool MortarInLoadStep(uint8_t* base)
    {
        if (!g_mortarInteracting) return false;
        const uint32_t c = Guest::Read32(base, 0x82C37070);
        const uint32_t st = c ? Guest::Read32(base, c + 4) : 0;
        const uint32_t def = st >= 0x40000000u && st < 0x90000000u ? Guest::Read32(base, st + 28) : 0;
        return def >= 0x40000000u && def < 0x90000000u && ReadText(base, def, 32) == "mortar_load";
    }
}
extern "C" PPC_FUNC(__imp__sub_82543290);
PPC_FUNC(sub_82543290)
{
    if (MortarInLoadStep(base))
    {
        static int said = 0;
        if ((said < 5 && said++ < 5)) { printf("mortar: the loading step's button press left out (button %u at %08X)\n", Guest::Read8(base, ctx.r3.u32), ctx.r3.u32); fflush(stdout); }
        return;
    }
    __imp__sub_82543290(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_825430A0);
PPC_FUNC(sub_825430A0)
{
    if (MortarInLoadStep(base)) return;
    __imp__sub_825430A0(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82474CB8);
PPC_FUNC(sub_82474CB8)
{
    const uint32_t list = ctx.r4.u32;
    const uint32_t owner = ctx.r3.u32;   // 0.5.294: the bank (archive) the list is for
    __imp__sub_82474CB8(ctx, base);
    if (!list) return;
    // 0.5.294: Laison's own archive and its effects not in a map's or its
    // zones' lists (291/292: Fuel Plant's world drew invisible, floors and
    // walls, with them there: the same game's level shares names with the
    // map's, its textures and shaders found in their place). Every other
    // list (the weapons', the vehicles', the globals') still has them, for
    // the mortar's arms, models and effects. COD3_LAISON_LISTS=all: as 291.
    static const bool laisonEverywhere = []() { const char* t = getenv("COD3_LAISON_LISTS"); return t && strcmp(t, "all") == 0; }();
    const int ownerKind = owner < 64 ? g_archiveKind[owner] : 0;
    const bool mapList = ownerKind == 3 || ownerKind == 4;
    {
        static int said = 0;
        if ((said < 40 && said++ < 40)) { printf("archives: list for archive %u (kind %d)%s\n", owner, ownerKind, mapList && !laisonEverywhere ? ": Laison's left out" : ""); fflush(stdout); }
    }
    // 0.5.85: and the map's own archive last, for the Kangaroo's archive:
    // its treads and wheels are drawn with v_tracked_tread and
    // v_tracked_wheel, which Merville's and Poissons' archives have and its
    // own does not (Mace 2 has them in its vehicles.cod). Not in its list,
    // they were never found, and the treads and wheels were not drawn.
    int32_t extra[ArchiveCount + 9];
    for (int i = 0; i < ArchiveCount; i++) extra[i] = g_archiveActive[i];
    extra[ArchiveCount] = g_vehicleArchive;
    extra[ArchiveCount + 1] = g_kangarooArchive;
    extra[ArchiveCount + 2] = g_kangarooArchive >= 0 || g_kangarooPendingMap() ? g_mapArchive : -1;
    extra[ArchiveCount + 3] = g_planeArchiveP47;     // 0.5.253: the UAV's P-47
    extra[ArchiveCount + 4] = g_planeArchiveStuka;   // and Stuka
    extra[ArchiveCount + 5] = g_mortarArchive;       // 0.5.285: the mortar
    extra[ArchiveCount + 6] = g_laisonArchive;       // 0.5.291: the campaign mortar's own (arms, effects, interactions)
    extra[ArchiveCount + 7] = g_laisonFxArchive;
    extra[ArchiveCount + 8] = g_laisonZoneArchive;   // 0.5.298
    // 0.5.269: the planes' archives right after the campaign weapons' (a
    // full list keeps the first ones; the Kangaroo's and the map's last).
    std::rotate(extra + ArchiveCount, extra + ArchiveCount + 3, extra + ArchiveCount + 9);
    for (const int32_t archive : extra)
    {
        if (archive < 0) continue;
        if (mapList && !laisonEverywhere && (archive == g_laisonArchive || archive == g_laisonFxArchive)) continue;   // 0.5.294
        const int32_t count = int32_t(Guest::Read32(base, list + 128));
        if (count < 0 || count >= 32)
        {
            // 0.5.269: a full list (32) leaves the rest out (the Stuka's
            // archive is the last one: its model was not found, 268).
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("archives: list %08X full (%d): archive %d and after left out\n", list, count, archive); fflush(stdout); }
            return;
        }
        bool there = false;
        for (int32_t i = 0; i < count; i++)
            if (int32_t(Guest::Read32(base, list + 4 * uint32_t(i))) == archive) there = true;
        if (there) continue;
        Guest::Write32(base, list + 4 * uint32_t(count), uint32_t(archive));
        Guest::Write32(base, list + 128, uint32_t(count + 1));
    }
}

// Vehicles on any map (the console's vehicles_everywhere 1, typed before the
// map is loaded). Only Merville and Poissons carry vehicle files; on another
// map Merville's archive is loaded beside the map's, as the campaign weapons
// archives are (with the weapons', below), and its VEHICLEFILE and
// VEHICLEPHYSICSFILE entries read the way the map's own are (sub_8256CE08:
// sub_8254B318 over an archive's entries with a name's prefix, the
// vehicle's reader sub_825683D8 and the physics' sub_8255F3A8).
namespace { std::atomic<bool> g_vehiclesEverywhere{false}; }
void MpFixesVehiclesEverywhere(bool on) { g_vehiclesEverywhere.store(on); }

namespace
{
    // An archive loaded beside the map's, with the weapons' (as the campaign
    // weapons archives are): kept loaded for the maps after, used again.
    struct ExtraArchive { const char* path; const char* label; int32_t active = -1, loaded = -1; uint32_t object = 0, guestPath = 0; };
    ExtraArchive g_merville{ "mp\\mp_merv\\mp_merv.cod", "Merville's archive" };
    ExtraArchive g_kangarooFile{ "sp\\mace2\\kangaroo.cod", "the Kangaroo's archive" };
    // 0.5.253: the UAV's planes: the P-47 (v_rep_P47; Chambois' vehicles,
    // 13 MB, the smallest with it) and the Stuka (stuka_flying; Crossroads'
    // vehicles, 10 MB). Loaded on every map (g_uavPlaneArchives).
    // 0.5.255: the Allies' plane is the Typhoon (sp\\forest\\vehicles.cod, 2.6 MB,
    // only the Typhoon): 254 hung loading Merville with the 13 MB P-47 file
    // (the fourth campaign weapons archive never finished: the map block full).
    ExtraArchive g_p47File{ "sp\\forest\\vehicles.cod", "the Typhoon's archive" };
    ExtraArchive g_stukaFile{ "sp\\crssrds\\vehicles.cod", "the Stuka's archive" };
    ExtraArchive g_mortarFile{ "sp\\laison\\vehicles.cod", "the mortar's archive (v_81mm)" };   // 0.5.285: 8 MB (with Laison's Shermans)
    // 0.5.298: filtered copies (MakeMortarArchives): the same blocks, their
    // file lists holding only the mortar's files, so none of Laison's level
    // data (its scene, collision, physics, paths, colour tables, level heap)
    // is loaded over the map's (291-296's invisible world).
    ExtraArchive g_laisonFile{ "sp\\laison\\laison_mortar.cod", "Laison's mortar files (definitions)" };
    ExtraArchive g_laisonFxFile{ "sp\\laison\\laison_mortar_fx.cod", "Laison's mortar effects" };
    ExtraArchive g_laisonZoneFile{ "sp\\laison\\laison_mortar_zn08.cod", "Laison's mortar arms (w_arms_ei_Mortar)" };
    std::atomic<bool> g_uavPlaneArchives{true};
    std::atomic<int> g_kangarooType{1};   // 0.5.101: back to 1 (Justin: 3 drove broken, 2 could not be got into)   // the vehicle type the Kangaroo is given: 1 four wheel, 2 tank,
                                          // 3 (0.5.100, the default) four wheel kind with a tracked vehicle's physics
    std::atomic<bool> g_kangaroo{true};   // on unless the console says kangaroo 0: its archive is small (5 MB)

    void LoadExtra(PPCContext& saved, uint8_t* base, uint32_t manager, uint32_t bank, ExtraArchive& archive, const std::string& map)
    {
        archive.active = -1;
        if (archive.object && archive.loaded >= 0 && Guest::Read32(base, manager + 4 * uint32_t(archive.loaded + 11)) == archive.object)
        {
            archive.active = archive.loaded;
            printf("vehicles: %s still loaded as %d\n", archive.label, archive.active);
            fflush(stdout);
            return;
        }
        if (!archive.guestPath) archive.guestPath = GuestText(base, archive.path);
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = manager;
        call.r4.u64 = Guest::Read32(base, bank + 0);
        call.r5.u64 = archive.guestPath;
        for (int i = 0; i < 4; i++)
        {
            const uint64_t value = (uint64_t(Guest::Read32(base, bank + 12 + 8 * i)) << 32) | Guest::Read32(base, bank + 16 + 8 * i);
            (i == 0 ? call.r6 : i == 1 ? call.r7 : i == 2 ? call.r8 : call.r9).u64 = value;
        }
        if (Settings::Get().bigMemory) call.r8.u64 = (uint64_t(0xBF800000u) << 32) | (call.r8.u64 & 0xFFFFFFFFull);
        call.lr = 0;
        Guest::Lookup(0x82477B50)(call, base);
        const int32_t index = int32_t(call.r3.u32);
        printf("vehicles: %s loading as %d beside %s\n", archive.label, index, map.c_str());
        fflush(stdout);
        if (index == -1) return;
        for (int spins = 0; spins < 100000 && Guest::Read32(base, manager) == 0; spins++)
        {
            PPCContext pump = call;
            pump.r3.u64 = manager; pump.r4.u64 = 0;
            Guest::Lookup(0x8247D700)(pump, base);
            Guest::Lookup(0x820F9CA8)(pump, base);
            Guest::Lookup(0x820F9D00)(pump, base);
        }
        archive.active = archive.loaded = index;
        archive.object = Guest::Read32(base, manager + 4 * uint32_t(index + 11));
        printf("vehicles: %s loaded (%08X)\n", archive.label, archive.object);
        fflush(stdout);
    }
}
void MpFixesKangaroo(bool on) { g_kangaroo.store(on); }
void MpFixesKangarooType(int type) { const int t = type == 2 ? 2 : type == 1 ? 1 : 3; g_kangarooType.store(t); printf("kangaroo: type %d from the next map start\n", t); fflush(stdout); }

namespace { uint32_t g_kangarooManager = 0, g_kangarooBank = 0; bool& g_kangarooPending = g_kangarooWaiting; }
void MpFixesLoadVehicleArchive(PPCContext& saved, uint8_t* base, uint32_t manager, uint32_t bank)
{
    const std::string map = Kernel::CurrentMap();
    const bool ownVehicles = map == "mp_merv" || map == "poissons";
    // Merville's, for vehicles on a map without (vehicles_everywhere 1: big,
    // 133 MB). The Kangaroo needs a WC51 to be copied from: on Merville and
    // Poissons, or with vehicles_everywhere.
    g_merville.active = -1;
    if (!map.empty() && !ownVehicles && g_vehiclesEverywhere.load()) LoadExtra(saved, base, manager, bank, g_merville, map);
    g_kangarooFile.active = -1;
    // 0.5.85: on Merville and Poissons the Kangaroo's archive waits for the
    // map's own (MpFixesLoadKangarooAfterMap): its treads and wheels are
    // drawn by v_tracked_tread and v_tracked_wheel, which only the map's
    // archive has (Mace 2 loads them from its vehicles.cod). Loaded before
    // the map, the Kangaroo found neither and drew without both.
    if (!map.empty() && g_kangaroo.load() && !ownVehicles && g_merville.active >= 0) LoadExtra(saved, base, manager, bank, g_kangarooFile, map);
    g_kangarooPending = false;
    if (!map.empty() && g_kangaroo.load() && ownVehicles)
    {
        if (!g_kangarooBank) g_kangarooBank = Guest::AllocatePhysical(0x40);
        memcpy(base + g_kangarooBank, base + bank, 0x30);
        g_kangarooManager = manager;
        g_kangarooPending = true;
    }
    g_vehicleArchive = g_merville.active;
    g_kangarooArchive = g_kangarooFile.active;
}
// 0.5.253: the UAV's planes, on every map. 0.5.255: loaded last, after the
// campaign weapons archives (the guns keep the room they had before).
void MpFixesLoadPlaneArchives(PPCContext& saved, uint8_t* base, uint32_t manager, uint32_t bank)
{
    const std::string map = Kernel::CurrentMap();
    g_p47File.active = -1;
    const bool stukaEarly = g_stukaEarly;   // 0.5.271: loaded before the guns this map (MpFixesLoadStukaEarly)
    g_stukaEarly = false;
    if (!stukaEarly) g_stukaFile.active = -1;
    // 0.5.259: not on Poissons (Justin's 258: crashed there, a call through
    // 0xCDCDCDCD from sub_82291160, with the planes' files in; it is the map
    // too full for the campaign guns without big_memory): the UAV still works,
    // without its plane.
    // (0.5.259: tried, then taken back the same hour: Justin wants the plane
    // there too. Under test: Poissons as the first map, not after Merville.)
    if (!map.empty() && g_uavPlaneArchives.load() && Settings::Get().uavPlane)
    {
        LoadExtra(saved, base, manager, bank, g_p47File, map);
        if (!stukaEarly) LoadExtra(saved, base, manager, bank, g_stukaFile, map);
    }
    g_planeArchiveP47 = g_p47File.active;
    g_planeArchiveStuka = g_stukaFile.active;
}
// 0.5.271: the Stuka's archive before the campaign guns', as 253 had it
// (its plane was seen then). Loaded after them (254-270) its stuka_flying
// was never found by name (268-270: Crossroads' tankshell and Shermans
// were found, but those are Merville's own too; stuka, v_a20 and
// v_gunturret, only in Crossroads, were not).
void MpFixesLoadStukaEarly(PPCContext& saved, uint8_t* base, uint32_t manager, uint32_t bank)
{
    const std::string map = Kernel::CurrentMap();
    g_stukaEarly = false;
    g_stukaFile.active = -1;
    if (map.empty() || !g_uavPlaneArchives.load() || !Settings::Get().uavPlane) return;
    LoadExtra(saved, base, manager, bank, g_stukaFile, map);
    g_stukaEarly = true;
    printf("vehicles: the Stuka's archive loaded before the campaign guns (as %d)\n", g_stukaFile.active);
    fflush(stdout);
}
// 0.5.285: the mortar's (Laison's vehicles.cod: v_81mm), before the guns as
// the Stuka's (models in an archive loaded after them were not found, 268).
// 0.5.286: OFF by default: 285 on Eder Dam (big_memory, 4 campaign gun
// archives, the Stuka's) ran the map block out: the 4th gun archive
// (sp\laison\weapons.cod) never finished loading, the game hung reading
// unmapped memory. COD3_MORTAR_MODEL=1 loads it (the mortar works without
// it, its tube not drawn).
void MpFixesLoadMortarArchive(PPCContext& saved, uint8_t* base, uint32_t manager, uint32_t bank)
{
    const std::string map = Kernel::CurrentMap();
    g_mortarFile.active = -1;
    g_mortarArchive = -1;
    // 0.5.287: on again with big_memory (the map block 608 MB); COD3_MORTAR_MODEL=0 leaves it out.
    static const bool off = []() { const char* t = getenv("COD3_MORTAR_MODEL"); return t && t[0] == '0'; }();
    if (map.empty() || off || !Settings::Get().killstreakArtillery || !Settings::Get().bigMemory)
    {
        if (!map.empty()) { printf("vehicles: the mortar's archive left out%s\n", off ? " (COD3_MORTAR_MODEL=0)" : " (needs big_memory)"); fflush(stdout); }
        return;
    }
    LoadExtra(saved, base, manager, bank, g_mortarFile, map);
    g_mortarArchive = g_mortarFile.active;
    printf("vehicles: the mortar's archive as %d\n", g_mortarArchive);
    fflush(stdout);
    // 0.5.291: the campaign mortar itself: Laison's own archive (its
    // VEHICLEFILE mortar_81mm, WEAPONFILEs mortar_81mm_turret and
    // mortar_shell, INTERACTIONFILE mortar and its states mortar_start,
    // mortar_idle, mortar_load, the arms' w_arms_ei_Mortar, the effects) and
    // its effects archive. Only the mortar's files are read from it (the
    // weapons' and vehicles' readers filtered); the level's world is in its
    // zones, not loaded. COD3_CAMPAIGN_MORTAR=0 leaves both out.
    g_laisonInteraction = false;   // 0.5.304
    g_laisonFile.active = g_laisonFxFile.active = g_laisonZoneFile.active = -1;
    g_laisonArchive = g_laisonFxArchive = g_laisonZoneArchive = -1;
    // 0.5.295: OFF unless COD3_CAMPAIGN_MORTAR=1: with Laison's archive in
    // (291, 292, 294) Fuel Plant's floors and walls drew invisible, and
    // keeping it out of the map's asset lists (294) did not help: its load
    // itself changes something the world needs. The way on: a small archive
    // of the mortar's files alone, made from it (NEXT-SESSION).
    // 0.5.296: on again, but loaded AFTER the map (MpFixesLoadLaisonAfterMap,
    // from the vehicles' set-up, as the Kangaroo's on Merville): Laison's
    // archive carries its own copies of textures the maps share by name
    // (common\ng\blackish_c and the rest of its 368 streamed files): loaded
    // before the map, its copies were taken for the map's own. After it, the
    // map's are there first. COD3_CAMPAIGN_MORTAR=0 leaves it out.
    // 0.5.297: off (the full archive drew the world invisible). 0.5.298: on,
    // from the filtered copies (MakeMortarArchives); COD3_CAMPAIGN_MORTAR=0 off.
    static const bool campaignOff = []() { const char* t = getenv("COD3_CAMPAIGN_MORTAR"); return t && t[0] == '0'; }();
    if (!campaignOff && !MakeMortarArchives()) { printf("mortar: the filtered Laison archives could not be made: the campaign mortar left out\n"); fflush(stdout); return; }
    g_laisonPending = false;
    if (campaignOff) return;
    if (!g_laisonBank) g_laisonBank = Guest::AllocatePhysical(0x40);
    memcpy(base + g_laisonBank, base + bank, 0x30);
    g_laisonManager = manager;
    g_laisonPending = true;
}
void MpFixesLoadLaisonAfterMap(PPCContext& saved, uint8_t* base)
{
    if (!g_laisonPending) return;
    g_laisonPending = false;
    const std::string map = Kernel::CurrentMap();
    const uint32_t manager = g_laisonManager, bank = g_laisonBank;
    const int32_t interactionsBefore = int32_t(Guest::Read32(base, 0x82BCBEB4));
    LoadExtra(saved, base, manager, bank, g_laisonFile, map);
    g_laisonArchive = g_laisonFile.active;
    LoadExtra(saved, base, manager, bank, g_laisonFxFile, map);
    g_laisonFxArchive = g_laisonFxFile.active;
    LoadExtra(saved, base, manager, bank, g_laisonZoneFile, map);   // 0.5.298
    g_laisonZoneArchive = g_laisonZoneFile.active;
    // The interactions (82515CB8 registers an archive's INTERACTIONFILE and
    // INTERACTSTATEFILE readers, sub_82511CA0 / sub_82511BA0: a table at
    // 0x82B778C4, its count at 0x82BCBEB4): read from Laison's if loading
    // it did not.
    auto interactionNames = [&]() {
        std::string all;
        const int32_t n = int32_t(Guest::Read32(base, 0x82BCBEB4));
        for (int32_t i = 0; i < n && i < 64; i++)
        {
            const uint32_t e = Guest::Read32(base, 0x82B778C4 + 4 * uint32_t(i));
            if (e >= 0x40000000u && e < 0x90000000u) all += " " + ReadText(base, e, 32);
        }
        return all;
    };
    if (g_laisonArchive >= 0 && interactionNames().find(" mortar") == std::string::npos)
    {
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = uint32_t(g_laisonArchive); call.lr = 0;
        Guest::Lookup(0x82515CB8)(call, base);
    }
    printf("mortar: Laison's archive as %d, its effects as %d (after the map); interactions %d -> %d:%s\n", g_laisonArchive, g_laisonFxArchive,
           interactionsBefore, int32_t(Guest::Read32(base, 0x82BCBEB4)), interactionNames().c_str());
    // 0.5.301: the mortar interaction begins at mortar_idle, not mortar_start
    // (300 crashed in mortar_start: its type, "MortarStart", is the campaign's
    // only; multiplayer has MortarLoad but not it, so the state was built as
    // something else). The interaction (72 bytes, table 0x82B778C4, count
    // 0x82BCBEB4): +0 its name, +32 initialInteractState (32 bytes, text).
    // The states (1432 bytes, table 0x82B791E8, count 0x82BCBEB8): +0 the
    // name, +32 interactType (an enum), logged for the mortar's.
    {
        const int32_t n = int32_t(Guest::Read32(base, 0x82BCBEB4));
        for (int32_t i = 0; i < n && i < 64; i++)
        {
            const uint32_t e = Guest::Read32(base, 0x82B778C4 + 4 * uint32_t(i));
            if (e < 0x40000000u || e >= 0x90000000u || ReadText(base, e, 32) != "mortar") continue;
            const std::string was = ReadText(base, e + 32, 32);
            memset(base + e + 32, 0, 32);
            memcpy(base + e + 32, "mortar_idle", 12);
            g_laisonInteraction = true;   // 0.5.304
            printf("mortar: interaction 'mortar' starts at mortar_idle now (was %s)\n", was.c_str());
        }
        const int32_t states = int32_t(Guest::Read32(base, 0x82BCBEB8));
        for (int32_t i = 0; i < states && i < 256; i++)
        {
            const uint32_t s = Guest::Read32(base, 0x82B791E8 + 4 * uint32_t(i));
            if (s < 0x40000000u || s >= 0x90000000u) continue;
            const std::string name = ReadText(base, s, 32);
            printf("mortar: interaction state %d %s: type %08X input %08X success %s playerAnim %s\n", i, name.c_str(), Guest::Read32(base, s + 32),
                   Guest::Read32(base, s + 36), ReadText(base, s + 40, 32).c_str(), ReadText(base, s + 208, 40).c_str());
        }
        fflush(stdout);
    }
    fflush(stdout);
    MpFixesLaisonWeapons(saved, base);
}
// 0.5.298: the mortar-only archives, made once from the player's own
// campaign files (nothing is downloaded or shipped): a copy of each KAPF
// archive with its file lists cut down to the files kept. KAPF (little
// endian): +0x0C the number of sections; the main section's record at +0x3C
// (u16 blocks, u16 files, block table, file list), the others' 24 bytes from
// +0x48 (name[12], u16 blocks, u16 files, table, list); a file list entry is
// 20 bytes (name, path, size, block, index). The game loads an archive's
// files from these lists (sub_8247CF70 walks them), so a file left out is
// never loaded; the blocks themselves are kept as they are.
namespace
{
    bool KapfFilter(const std::filesystem::path& from, const std::filesystem::path& to, bool (*keep)(const std::string&))
    {
        std::error_code error;
        if (std::filesystem::exists(to, error) && std::filesystem::exists(from, error) &&
            std::filesystem::last_write_time(to, error) >= std::filesystem::last_write_time(from, error) &&
            std::filesystem::file_size(to, error) == std::filesystem::file_size(from, error))
            return true;   // made before
        FILE* in = _wfopen(from.wstring().c_str(), L"rb");
        if (!in) { printf("mortar: %s not found\n", from.string().c_str()); return false; }
        std::vector<uint8_t> d;
        fseek(in, 0, SEEK_END); d.resize(size_t(ftell(in))); fseek(in, 0, SEEK_SET);
        const size_t got = fread(d.data(), 1, d.size(), in);
        fclose(in);
        if (got != d.size() || d.size() < 0x100 || memcmp(d.data(), "KAPF", 4) != 0) { printf("mortar: %s is not a KAPF archive\n", from.string().c_str()); return false; }
        auto u16 = [&](size_t o) { return uint32_t(d[o] | (d[o + 1] << 8)); };
        auto u32 = [&](size_t o) { return uint32_t(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (uint32_t(d[o + 3]) << 24)); };
        auto put16 = [&](size_t o, uint32_t v) { d[o] = uint8_t(v); d[o + 1] = uint8_t(v >> 8); };
        auto text = [&](uint32_t o) { std::string t; while (o < d.size() && d[o]) t += char(d[o++]); return t; };
        const uint32_t sections = u32(0x0C);
        int kept = 0, dropped = 0;
        for (uint32_t s = 0; s < sections && s < 16; s++)
        {
            const size_t rec = s == 0 ? 0x3C : 0x48 + 24 * size_t(s - 1) + 12;
            const uint32_t count = u16(rec + 2), list = u32(rec + 8);
            if (list + 20ull * count > d.size()) { printf("mortar: %s: section %u's list out of the file\n", from.string().c_str(), s); return false; }
            std::vector<uint8_t> keepList;
            for (uint32_t i = 0; i < count; i++)
            {
                const size_t e = list + 20ull * i;
                const std::string name = text(u32(e));
                if (s == 0 && keep(name)) { keepList.insert(keepList.end(), d.begin() + e, d.begin() + e + 20); kept++; printf("mortar:   kept %s\n", name.c_str()); }
                else dropped++;
            }
            std::fill(d.begin() + list, d.begin() + list + 20ull * count, uint8_t(0));
            std::copy(keepList.begin(), keepList.end(), d.begin() + list);
            put16(rec + 2, uint32_t(keepList.size() / 20));
        }
        FILE* out = _wfopen(to.wstring().c_str(), L"wb");
        if (!out) { printf("mortar: %s could not be written\n", to.string().c_str()); return false; }
        const bool ok = fwrite(d.data(), 1, d.size(), out) == d.size();
        fclose(out);
        printf("mortar: made %s from %s: %d files kept, %d left out\n", to.filename().string().c_str(), from.filename().string().c_str(), kept, dropped);
        fflush(stdout);
        return ok;
    }
    bool EndsWith(const std::string& t, const char* e) { const size_t n = strlen(e); return t.size() >= n && _stricmp(t.c_str() + t.size() - n, e) == 0; }
    // Laison's own: only its game database files (the weapons', vehicles'
    // and interactions' definitions and the level's file list they are read by).
    bool KeepLaisonDefinitions(const std::string& n) { return EndsWith(n, ".gdbb") || EndsWith(n, ".dbb") || EndsWith(n, ".cfgstrb"); }
    // Its effects archive: the effects, their textures and meshes (not its
    // two level files, .dcgb/.xmbb/.xmpbb, nor its index buffer).
    // 0.5.299: not Laison's sky either (sky.xemesh and its laison_* textures).
    bool KeepLaisonEffects(const std::string& n)
    {
        if (_stricmp(n.c_str(), "sky.xemesh") == 0 || _stricmp(n.substr(0, 7).c_str(), "laison_") == 0) return false;
        return EndsWith(n, ".bpeb") || EndsWith(n, ".xetex") || EndsWith(n, ".xemesh");
    }
    // Zone 8: the mortar's loading arms alone.
    bool KeepMortarArms(const std::string& n) { return _stricmp(n.c_str(), "w_arms_ei_Mortar.xeanim") == 0; }
}
bool MakeMortarArchives()
{
    static int done = 0;   // 1 made, -1 failed
    if (done) return done > 0;
    const std::filesystem::path dir = Guest::GameRoot / "sp" / "laison";
    // 0.5.299: the effects copy remade (its filter changed): the old one removed once.
    {
        std::error_code error;
        const std::filesystem::path marker = dir / "laison_mortar_fx.299";
        if (!std::filesystem::exists(marker, error))
        {
            std::filesystem::remove(dir / "laison_mortar_fx.cod", error);
            if (FILE* m = _wfopen(marker.wstring().c_str(), L"wb")) fclose(m);
        }
    }
    const bool ok = KapfFilter(dir / "laison.cod", dir / "laison_mortar.cod", KeepLaisonDefinitions) &&
                    KapfFilter(dir / "fx.cod", dir / "laison_mortar_fx.cod", KeepLaisonEffects) &&
                    KapfFilter(dir / "zn08.cod", dir / "laison_mortar_zn08.cod", KeepMortarArms);
    done = ok ? 1 : -1;
    return ok;
}
void MpFixesUavPlaneArchives(bool on) { g_uavPlaneArchives.store(on); }
bool MpFixesPlaneModelLoaded(int side) { (void)side; return g_planeArchiveP47 >= 0 || g_planeArchiveStuka >= 0; }   // 0.5.266: the Typhoon for both

// The Kangaroo's archive on Merville and Poissons, put off till the map's
// own archive is in (from the vehicles' setup, sub_82561838, which comes
// after it): with the weapons' bank's manager and a copy of that bank (its
// name word and bounds), kept from then.
void MpFixesLoadKangarooAfterMap(PPCContext& ctx, uint8_t* base)
{
    if (!g_kangarooPending) return;
    const std::string map = Kernel::CurrentMap();
    printf("vehicles: %s's own archive (%d) is in: the Kangaroo's after it, looking in the map's too (the treads' and wheels' shaders)\n", map.c_str(), g_mapArchive);
    fflush(stdout);
    LoadExtra(ctx, base, g_kangarooManager, g_kangarooBank, g_kangarooFile, map);
    g_kangarooArchive = g_kangarooFile.active;
    g_kangarooPending = false;
}

// The Kangaroo (the campaign's Mace 2 APC, v_Kangaroo in sp\mace2\kangaroo.cod,
// with a driver, a gunner and six passengers' tags) as a vehicle kind of its
// own, "mp_kangaroo": a copy of the WC51 truck's vehicle file (784 bytes,
// set up by sub_825683D8: its name first, the rest its fields; the kinds'
// table at 0x82B2DC90, their count at 0x82B34594), under the new name. Its
// model is set when it is put down.
namespace
{
    void MakeKangarooKind(PPCContext& ctx, uint8_t* base)
    {
        const int count = int(int16_t(Guest::Read16(base, 0x82B34594)));
        uint32_t truck = 0;
        for (int i = 0; i < count; i++)
        {
            const uint32_t def = Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(i));
            const std::string name = def ? ReadText(base, def, 32) : std::string();
            if (_stricmp(name.c_str(), "mp_kangaroo") == 0) return;
            if (_stricmp(name.c_str(), "mp_horch") == 0 || (!truck && _stricmp(name.c_str(), "mp_wc51") == 0)) truck = def;
        }
        if (!truck || count >= 32)
        {
            printf("vehicles: no WC51 to make the Kangaroo from (%d kinds)\n", count);
            fflush(stdout);
            return;
        }
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        call.r3.u64 = 16; call.r4.u64 = 784; call.lr = 0;
        Guest::Lookup(0x820DE750)(call, base);
        const uint32_t kangaroo = call.r3.u32;
        if (!kangaroo) return;
        memcpy(base + kangaroo, base + truck, 784);
        memset(base + kangaroo, 0, 32);
        memcpy(base + kangaroo, "mp_kangaroo", 12);
        Guest::Write32(base, 0x82B2DC90 + 4 * uint32_t(count), kangaroo);
        Guest::Write16(base, 0x82B34594, uint16_t(count + 1));
        printf("vehicles: the Kangaroo made as kind %d (from the %s's vehicle file)\n", count, ReadText(base, truck, 32).c_str());
        fflush(stdout);
    }
}

namespace
{
    // 0.5.82: the Kangaroo its own kind (mp_kangaroo, a copy of the Horch's
    // vehicle file made with the map's files, so the Horch's seats' animations
    // stay), not the map's real Horch's: its driving block (+80 to +127: the
    // Horch's speeds, too fast for a tank's hull, Justin) taken from the
    // Sherman's file, and its seats (+72: the Horch 3, the WC51 2) as many as
    // its model has places: the driver, the gunner and six passengers.
    // 0.5.99: how the Kangaroo is made (kangaroo_build): 1 a Kangaroo from
    // the start, with the tanks' physics (mp_tankdefault, the Sherman's: the
    // Kangaroo is a Sherman's hull), 2 the same keeping the Horch's physics,
    // 0 the Horch made over at the map's start as up to 0.5.98.
    std::atomic<int> g_kangarooBuild{2};   // 0.5.102: the Horch's physics (Justin: the tank preset slid back and forth on hills)
    int KangarooPrepareKind(uint8_t* base);
    void KangarooOwnKind(uint8_t* base, uint32_t record)
    {
        const int own = KangarooPrepareKind(base);
        if (own >= 0) Guest::Write16(base, record + 376, uint16_t(own));
    }
    int KangarooPrepareKind(uint8_t* base)
    {
        // 0.5.83: the vehicle file's fields by the game's own table of them
        // (0x825FE3C8: name, offset, kind): type +32, subtype +34 (the
        // game's subtypes: Jeep 1, Motorcycle, Truck, Wagon, Halftrack,
        // Horch, Sherman, Kangaroo 8), numSeats +68 (0.5.82 wrote 8 to +72,
        // which is hudIndex), texureScroll +40 and its scale +76, maxSpeed
        // +80 to boundsLength +124, health +128, the cameras +432 to +499.
        // The campaign's "Kangaroo" file (its archive's, read in the map's
        // start) gives those to mp_kangaroo when it is there; else the
        // Sherman's driving and bounds.
        const int count = int(int16_t(Guest::Read16(base, 0x82B34594)));
        int own = -1;
        uint32_t ownDef = 0, sherman = 0, campaign = 0;
        for (int i = 0; i < count && i < 32; i++)
        {
            const uint32_t def = Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(i));
            const std::string name = def ? ReadText(base, def, 32) : std::string();
            if (_stricmp(name.c_str(), "mp_kangaroo") == 0) { own = i; ownDef = def; }
            else if (_stricmp(name.c_str(), "mp_shermanTank") == 0) sherman = def;
            else if (name.find("angaroo") != std::string::npos) campaign = def;
        }
        if (own < 0) { printf("kangaroo: no kind of its own on this map\n"); return -1; }
        const uint32_t from = campaign ? campaign : sherman;
        if (from)
        {
            memcpy(base + ownDef + 80, base + from + 80, 52);     // maxSpeed .. boundsLength, health
            if (campaign)
            {
                memcpy(base + ownDef + 40, base + from + 40, 4);   // texureScroll
                memcpy(base + ownDef + 76, base + from + 76, 4);   // texureScrollScale
                memcpy(base + ownDef + 68, base + from + 68, 4);   // numSeats
                memcpy(base + ownDef + 392, base + from + 392, 4); // engineSndSpeed
                memcpy(base + ownDef + 432, base + from + 432, 68); // the cameras
            }
        }
        if (!campaign) Guest::Write32(base, ownDef + 68, 8);       // numSeats: driver, gunner, six passengers
        // 0.5.99: its treads' scroll as the tanks' (texureScroll +40 1.0 and
        // texureScrollScale +76 5.5, the Sherman's and the Panzer's). Without the
        // campaign's file the Horch's 1.0 was kept: the distance each tread is
        // moved by as it drives (sub_82568600, both sides from +1308 and +1312)
        // is scaled by it, so its treads ran at a fifth of a tank's going ahead.
        if (!campaign && sherman)
        {
            memcpy(base + ownDef + 40, base + sherman + 40, 4);
            memcpy(base + ownDef + 76, base + sherman + 76, 4);
        }
        // Its subtype the game's own Kangaroo (8), its driving curve the
        // Kangaroo's (vehicleCurveFile +508: Kangaroo.bvce is in mp\global.cod),
        // and its type 4 wheel (1) or tank (2: tracked, the treads' scroll)
        // as the kangaroo_type console command last said.
        Guest::Write16(base, ownDef + 34, 8);
        memset(base + ownDef + 508, 0, 32);
        memcpy(base + ownDef + 508, "Kangaroo", 9);
        Guest::Write16(base, ownDef + 32, uint16_t(g_kangarooType.load() == 2 ? 2 : 1));
        // 0.5.99: its physics (vehiclePhysics +540, vehiclePhysicsThird +572:
        // the preset its physics body and wheels are made from) the Sherman's
        // (mp_tankdefault) instead of the Horch's (mp_horsch, a small car's),
        // and a tank's +708/+712 (read for tracked kinds as it is made).
        if (sherman && g_kangarooBuild.load() == 1)
        {
            memcpy(base + ownDef + 540, base + sherman + 540, 32);
            memcpy(base + ownDef + 572, base + sherman + 572, 32);
            memcpy(base + ownDef + 708, base + sherman + 708, 8);
        }
        printf("kangaroo: its own kind %d (type %d, subtype 8, curve Kangaroo) from %s: seats %d, max speed %.0f, bounds %.0f %.0f %.0f, tread scroll %.2f x %.2f\n", own, g_kangarooType.load(),
               campaign ? "the campaign's Kangaroo file" : sherman ? "the Sherman's" : "the Horch's", int(Guest::Read32(base, ownDef + 68)),
               ReadF(base, ownDef + 80), ReadF(base, ownDef + 116), ReadF(base, ownDef + 120), ReadF(base, ownDef + 124),
               ReadF(base, ownDef + 40), ReadF(base, ownDef + 76));
        printf("kangaroo: its physics '%s' (third person '%s')\n", ReadText(base, ownDef + 540, 32).c_str(), ReadText(base, ownDef + 572, 32).c_str());
        fflush(stdout);
        return own;
    }

    // One of the map's own vehicles moved to a saved place, in the map's
    // start (its physics not yet begun): every copy of its origin and yaw
    // the comparison found (the entity's +32, +304, +336, +832 and the
    // yaw at +76 and +356; the record's +192, +208, +448 and its yaw at
    // +228, +244, +468). It keeps everything that makes it drivable.
    // The map's vehicles made Kangaroos at the map's start (their entities).
    std::set<uint32_t> g_kangarooEntities;
    struct Moved { float x, y, z; uint32_t entity, record; std::string kind; };
    std::vector<Moved> g_movedFrom;
    void ScanForPlace(uint8_t* base, const Moved& m)
    {
        int hits = 0;
        static const struct { uint32_t from, to; } ranges[] = {
            { 0x40000000u, 0x70000000u }, { 0x82000000u, 0x84000000u }, { 0xA0000000u, 0xC0000000u } };
        for (const auto& r : ranges)
        {
            for (uint64_t at = r.from; at < r.to && hits < 80;)
            {
                MEMORY_BASIC_INFORMATION info{};
                if (!VirtualQuery(base + at, &info, sizeof(info))) break;
                const uint64_t regionEnd = std::min<uint64_t>(r.to, uint64_t(reinterpret_cast<uint8_t*>(info.BaseAddress) - base) + info.RegionSize);
                if (info.State == MEM_COMMIT && !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
                {
                    for (uint64_t a = at & ~3ull; a + 12 <= regionEnd && hits < 80; a += 4)
                    {
                        const uint32_t g = uint32_t(a);
                        const float x = ReadF(base, g);
                        if (!(fabsf(x - m.x) < 200.0f)) continue;
                        const float y = ReadF(base, g + 4);
                        if (!(fabsf(y - m.y) < 200.0f)) continue;
                        const float z = ReadF(base, g + 8);
                        if (!(fabsf(z - m.z) < 250.0f)) continue;
                        if ((g >= m.entity && g < m.entity + 876) || (g >= m.record && g < m.record + 1872)) continue;
                        hits++;
                        std::string around;
                        char word[12];
                        for (int k = -8; k < 8; k++) { snprintf(word, sizeof(word), " %08X", Guest::Read32(base, g + 4 * k)); around += word; }
                        printf("vehicles: %s's old place still at %08X: %.1f %.1f %.1f |%s\n", m.kind.c_str(), g, x, y, z, around.c_str());
                    }
                }
                at = regionEnd;
            }
        }
        printf("vehicles: %s's old place (%.0f %.0f %.0f): %d copies left in memory\n", m.kind.c_str(), m.x, m.y, m.z, hits);
        fflush(stdout);
    }
    int g_mapStarts = 0;   // the map starts seen (sub_82564520), for the probe below

    void MoveVehicle(uint8_t* base, uint32_t entity, uint32_t record, float x, float y, float z, float yaw)
    {
        // 0.5.80: back to 0.5.73's copies only (the entity's +32, +304, +336,
        // +832 and its yaw at +76, +356; the record's +192, +208, +448 and its
        // yaw at +228, +244, +468). 0.5.76 carried every position near the old
        // place in both, turned: after it the moved jeep could not be got
        // into at all (Justin's test, 0.5.79).
        const float ox = ReadF(base, entity + 336), oy = ReadF(base, entity + 340), oz = ReadF(base, entity + 344);
        auto put = [&](uint32_t at) { WriteF(base, at, x); WriteF(base, at + 4, y); WriteF(base, at + 8, z); };
        for (const uint32_t at : { 32u, 304u, 336u, 832u }) put(entity + at);
        for (const uint32_t at : { 192u, 208u, 448u }) put(record + at);
        WriteF(base, entity + 76, yaw); WriteF(base, entity + 356, yaw);
        WriteF(base, record + 228, yaw); WriteF(base, record + 244, yaw); WriteF(base, record + 468, yaw);
        printf("vehicles: moved from %.0f %.0f %.0f to %.0f %.0f %.0f (0.5.73's copies)\n", ox, oy, oz, x, y, z);
    }
    // A vehicle kind's file (its def, 0x82B2DC90 by the record's +376): every
    // word, and the text a word points at when it is text: its seats, their
    // tags and animations, to give the Kangaroo its six passengers.
    void DumpVehicleKind(uint8_t* base, uint32_t record)
    {
        const int kind = int(int16_t(Guest::Read16(base, record + 376)));
        const uint32_t def = kind >= 0 && kind < 32 ? Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(kind)) : 0;
        if (!def) return;
        std::string line;
        char word[64];
        for (uint32_t at = 0; at < 784; at += 4)
        {
            const uint32_t w = Guest::Read32(base, def + at);
            if (!w) continue;
            float f; memcpy(&f, &w, 4);
            std::string text;
            if (w >= 0x40000000u && w < 0x90000000u)
            {
                text = ReadText(base, w, 32);
                bool printable = text.size() >= 3;
                for (char c : text) if (c < 0x20 || c > 0x7E) printable = false;
                if (!printable) text.clear();
            }
            if (!text.empty()) snprintf(word, sizeof(word), " +%u='%s'", at, text.c_str());
            else if (std::isfinite(f) && fabsf(f) > 0.001f && fabsf(f) < 100000.0f) snprintf(word, sizeof(word), " +%u=%.2f", at, f);
            else snprintf(word, sizeof(word), " +%u=%08X", at, w);
            line += word;
        }
        printf("vehicles: kind %d's file (%s):%s\n", kind, ReadText(base, def, 32).c_str(), line.c_str());
    }
    // The entity's and the record's words, for finding the box a vehicle is
    // hit by (its mins and maxs) next to the Horch's and the WC51's.
    void DumpEntity(uint8_t* base, uint32_t entity, uint32_t record, const std::string& kind)
    {
        std::string line;
        char word[24];
        for (uint32_t at = 0; at < 900; at += 4)
        {
            const uint32_t w = Guest::Read32(base, entity + at);
            float f; memcpy(&f, &w, 4);
            if (w == 0) continue;
            if (std::isfinite(f) && fabsf(f) > 0.5f && fabsf(f) < 20000.0f) snprintf(word, sizeof(word), " +%u=%.1f", at, f);
            else snprintf(word, sizeof(word), " +%u=%08X", at, w);
            line += word;
        }
        printf("vehicles: %s's entity:%s\n", kind.c_str(), line.c_str());
        (void)record;
    }
}

// The map's saved vehicles (vehicles.txt), in the map's start: its last
// pass over the vehicles (sub_82564520: each one's entity unlinked and
// linked again) runs right after the map's own are made, before the game
// begins. 0.5.72 made new ones here: made as the map's are, they still
// could not be driven (they lack what the map's vehicles are given from
// the map itself, their collision brush at +878 among it). So a saved
// vehicle is now one of the map's own of that kind, moved to its place
// (the Kangaroo: a WC51 given the Kangaroo's model); a new one is made only
// when the map has none of that kind left, and says so.
extern "C" PPC_FUNC(__imp__sub_82564520);
PPC_FUNC(sub_82564520)
{
    if (Guest::Read32(base, 0x82B3FDE0 + 8) != 0)
    {
        const std::string map = MapNow();
        const uint32_t pool = Guest::Read32(base, 0x82BCBDB4);
        const int records = int(Guest::Read16(base, 0x82C281D0 + 3088));
        // The map's vehicles: entity, record, kind's name.
        struct Own { uint32_t entity, record; std::string kind; bool taken; };
        std::vector<Own> own;
        for (int i = 0; pool && i < records; i++)
        {
            const uint32_t record = pool + 1872 * uint32_t(i);
            const uint32_t handle = Guest::Read32(base, record + 368), index = handle & 0x3FFF;
            if (index >= 4096) continue;
            const uint32_t entry = 0x82BD1FC0 + 8 * index;
            if ((handle >> 14) != Guest::Read32(base, entry + 4)) continue;
            const uint32_t entity = Guest::Read32(base, entry);
            if (!entity) continue;
            const int kind = int(int16_t(Guest::Read16(base, record + 376)));
            const uint32_t def = kind >= 0 && kind < 32 ? Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(kind)) : 0;
            own.push_back({ entity, record, def ? ReadText(base, def, 32) : std::string("?"), false });
        }
        std::string have;
        for (const Own& o : own) have += (have.empty() ? "" : ", ") + o.kind;
        printf("vehicles: %s's own at its start: %s\n", map.c_str(), have.empty() ? "none" : have.c_str());
        int moved = 0, made = 0, wanted = 0;
        g_kangarooEntities.clear();
        g_mapStarts++;
        g_inMapStart = true;
        for (const Placed& p : ReadPlacements())
        {
            if (p.map != map) continue;
            wanted++;
            const bool kangaroo = _stricmp(p.kind.c_str(), "mp_kangaroo") == 0;
            // Which of the map's vehicles can stand in, best first: the same
            // kind; for the Kangaroo the Sherman (the Kangaroo is a Sherman's
            // hull: tracks, its size, its collision), then a WC51, then a
            // Horch; for the WC51 a Horch. One that is not the kind asked for
            // takes the asked for one's model.
            std::vector<std::string> kinds;
            if (kangaroo) kinds = { "mp_kangaroo", "mp_horch", "mp_wc51" };   // 0.5.99: made one from the start first
            else if (_stricmp(p.kind.c_str(), "mp_wc51") == 0) kinds = { "mp_wc51", "mp_horch" };
            else kinds = { p.kind };
            Own* use = nullptr;
            std::string kind = kinds.front();
            for (const std::string& k : kinds)
            {
                for (Own& o : own) if (!o.taken && _stricmp(o.kind.c_str(), k.c_str()) == 0) { use = &o; break; }
                if (use) { kind = k; break; }
            }
            if (use)
            {
                use->taken = true;
                g_movedFrom.push_back({ ReadF(base, use->entity + 336), ReadF(base, use->entity + 340), ReadF(base, use->entity + 344),
                                        use->entity, use->record, p.kind });
                MoveVehicle(base, use->entity, use->record, p.x, p.y, p.z, p.yaw);
                DumpEntity(base, use->entity, use->record, kind);
                DumpVehicleKind(base, use->record);
                const bool already = kangaroo && _stricmp(kind.c_str(), "mp_kangaroo") == 0;   // 0.5.99: made a Kangaroo as it spawned
                const char* model = already ? nullptr : kangaroo ? "v_Kangaroo" : (_stricmp(kind.c_str(), p.kind.c_str()) != 0 ? "v_wc51" : nullptr);
                if (kangaroo) g_kangarooEntities.insert(use->entity);
                if (model)
                {
                    static uint32_t scratch = 0;
                    if (!scratch) scratch = Guest::AllocatePhysical(0x80);
                    memset(base + scratch, 0, 0x80);
                    memcpy(base + scratch + 0x40, model, strlen(model) + 1);
                    Call3(ctx, base, MakeText, scratch, scratch + 0x40, 0);
                    Call3(ctx, base, 0x824CE130, Guest::Read32(base, use->entity + 484), scratch, 0);
                    Call3(ctx, base, FreeText, scratch, 0, 0);
                }
                if (kangaroo)
                {
                    // 0.5.82: the Kangaroo's own box, the size of its hull (a
                    // Sherman's: about 250 long, 112 wide, 95 high), set after
                    // its model (setting the model sets the box from the model:
                    // 0.5.81 set it before, and Justin still walked through
                    // parts of it). The entity's mins at +240, maxs at +256.
                    // 0.5.99: not for one made a Kangaroo as it spawned: its box is
                    // its model's own (-141 -60 0 to 141 68 124), set with it.
                    if (!already)
                    {
                        const float mins[3] = { -125.0f, -56.0f, 0.0f }, maxs[3] = { 125.0f, 56.0f, 95.0f };
                        for (int k = 0; k < 3; k++) { WriteF(base, use->entity + 240 + 4 * k, mins[k]); WriteF(base, use->entity + 256 + 4 * k, maxs[k]); }
                    }
                    KangarooOwnKind(base, use->record);
                }
                moved++;
                printf("vehicles: the map's %s moved to %.0f %.0f %.0f for a saved %s\n", kind.c_str(), p.x, p.y, p.z, p.kind.c_str());
            }
            else if (SpawnVehicleAt(ctx, base, p.kind, p.x, p.y, p.z, p.yaw))
            {
                made++;
                printf("vehicles: no %s of the map's left: a new one made (not drivable yet)\n", kind.c_str());
            }
        }
        g_inMapStart = false;
        if (wanted)
        {
            printf("vehicles: %d saved: %d of the map's own moved there, %d made new\n", wanted, moved, made);
            Overlay::ConsolePrint("vehicles: " + std::to_string(moved) + " saved vehicles are the map's own, moved (drivable); " +
                                  std::to_string(made) + " made new. This map has: " + (have.empty() ? std::string("none") : have));
        }
        fflush(stdout);
    }
    g_placementDue = false;
    __imp__sub_82564520(ctx, base);
    // 0.5.79: what still stands where a moved vehicle stood (its collision
    // stays there: walking into the Kangaroo goes through it, and at the
    // Sherman's old place something unseen blocks the way). Every copy of
    // the old place left in memory, outside the entity and the record
    // already carried, is logged with the words around it.
    for (const Moved& m : g_movedFrom) ScanForPlace(base, m);
    // (Scanned again a few seconds in, by the probe, once the game is going.)
}

// 0.5.99: the Kangaroo made a Kangaroo as the map's vehicles spawn, not a
// Horch made over afterwards. A vehicle is set up by sub_8257E3A8 (its entity
// r3, its record r4, its kind r5): its seats' tags, then its physics object
// (record +1304, from a pool of 15) from the kind's vehiclePhysics preset
// (+540: the Horch's mp_horsch, the Sherman's mp_tankdefault) and its model's
// bones and size (sub_82420998, sub_8241BF00, the body sub_8241BB90). Made
// over at the map's start, the Kangaroo kept all of that from the Horch: a
// Horch's physics body, car physics and the Horch's wheel bones (0.5.93's
// spinning tread). Now, for each Kangaroo the map's saved vehicles want, one
// of the map's Horches (or WC51s) is given the Kangaroo's model and kind just
// before it is set up, so it is set up as the Kangaroo throughout; it is then
// moved to its place at the map's start as before.
namespace
{
    std::string g_spawnKey;
    int g_kangaroosToMake = 0;
}
void MpFixesKangarooBuild(int mode)
{
    g_kangarooBuild.store(mode < 0 ? 0 : mode > 2 ? 2 : mode);
    printf("kangaroo: build %d from the next map start (1 a Kangaroo from the start with a tank's physics, 2 with the Horch's, 0 the Horch made over)\n", g_kangarooBuild.load());
    fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_8257E3A8);
PPC_FUNC(sub_8257E3A8)
{
    const uint32_t entity = ctx.r3.u32;
    const int kind = int(int16_t(ctx.r5.u16));
    if (!g_inMapStart && g_kangaroo.load() && g_kangarooBuild.load() != 0 && entity && kind >= 0 && kind < 32)
    {
        const std::string map = MapNow();
        const std::string key = map + "#" + std::to_string(g_mapStarts);
        if (key != g_spawnKey)
        {
            g_spawnKey = key;
            g_kangaroosToMake = 0;
            for (const Placed& p : ReadPlacements())
                if (p.map == map && _stricmp(p.kind.c_str(), "mp_kangaroo") == 0) g_kangaroosToMake++;
        }
        const uint32_t def = Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(kind));
        const std::string name = def ? ReadText(base, def, 32) : std::string();
        if (g_kangaroosToMake > 0 && (_stricmp(name.c_str(), "mp_horch") == 0 || _stricmp(name.c_str(), "mp_wc51") == 0))
        {
            const int own = KangarooPrepareKind(base);
            if (own >= 0)
            {
                g_kangaroosToMake--;
                static uint32_t scratch = 0;
                if (!scratch) scratch = Guest::AllocatePhysical(0x80);
                memset(base + scratch, 0, 0x80);
                memcpy(base + scratch + 0x40, "v_Kangaroo", 11);
                Call3(ctx, base, MakeText, scratch, scratch + 0x40, 0);
                Call3(ctx, base, 0x824CE130, Guest::Read32(base, entity + 484), scratch, 0);
                Call3(ctx, base, FreeText, scratch, 0, 0);
                printf("kangaroo: the map's %s made a Kangaroo as it is set up (kind %d, its model first; box %.0f %.0f %.0f to %.0f %.0f %.0f)\n",
                       name.c_str(), own, ReadF(base, entity + 240), ReadF(base, entity + 244), ReadF(base, entity + 248),
                       ReadF(base, entity + 256), ReadF(base, entity + 260), ReadF(base, entity + 264));
                fflush(stdout);
                ctx.r5.u64 = uint64_t(own);
                const uint32_t record = ctx.r4.u32;
                // 0.5.100: known as a Kangaroo before it is set up, so its seats'
                // tags (looked up here, once: tag_enter_left/right and the rest)
                // get their stand-ins (sub_82434E60's hook only answers for
                // the Kangaroos' entities). 0.5.99 put it in only at the map's
                // start, after: the side ways in found none, and Justin could
                // get in only at the gun at the back.
                g_kangarooEntities.insert(entity);
                __imp__sub_8257E3A8(ctx, base);
                const uint32_t physics = Guest::Read32(base, record + 1304);
                // 0.5.100: its physics driven as a tracked vehicle's (the flag
                // 0x20 its physics object is given for a type 2 kind, read by the
                // driving, sub_82423660, and the posing, sub_8242B938). Justin, on
                // 0.5.99: with the tanks' preset driven as a four wheeler it was
                // slow and stuck going up out of a ravine. Its kind stays a four
                // wheeler's, so its seats and ways in stay as they were.
                if (physics && g_kangarooType.load() == 3)
                    Guest::Write32(base, physics + 640, Guest::Read32(base, physics + 640) | 0x20u);
                const uint32_t preset = physics ? Guest::Read32(base, physics + 0x250) : 0;
                printf("kangaroo: set up: physics object %08X, preset '%s', flags %08X, wheel bones", physics,
                       preset ? ReadText(base, preset + 0x70, 32).c_str() : "none", physics ? Guest::Read32(base, physics + 640) : 0);
                for (int i = 0; physics && i < 8; i++) printf(" %d", int32_t(Guest::Read32(base, physics + 36 + 4 * uint32_t(i))));
                printf("\n");
                fflush(stdout);
                return;
            }
        }
    }
    __imp__sub_8257E3A8(ctx, base);
}

// 0.5.112: the Horch's contact wheels hold this hull a little high.
// Native physics fields at 825FEAF0 explicitly name wheel_radius at +20.
// Copy only the verified preset span (0..200 inclusive), and change the
// Kangaroo's private copy BEFORE the native vehicle physics is initialized.
namespace {
    std::atomic<float> g_kangarooDrop{5.0f};
    std::map<uint32_t,uint32_t> g_kangarooPhysicsCopies;
}
void MpFixesKangarooDrop(float amount)
{
    if (!std::isfinite(amount)) return;
    g_kangarooDrop.store(std::clamp(amount,0.0f,8.0f));
    printf("kangaroo: contact height drop %.1f from next map start (0 original)\n",g_kangarooDrop.load());
    fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_82420998);
PPC_FUNC(sub_82420998)
{
    const uint32_t object=ctx.r3.u32,entity=ctx.r4.u32,preset=ctx.r5.u32;
    const float drop=g_kangarooDrop.load();
    if (object && preset && drop>0 && g_kangarooEntities.count(entity) &&
        ReadText(base,preset+112,32)=="mp_horsch")
    {
        const float radius=ReadF(base,preset+20);
        if (std::isfinite(radius) && radius>=8.0f && radius<=64.0f)
        {
            uint32_t& copy=g_kangarooPhysicsCopies[object];
            if (!copy) copy=Guest::AllocatePhysical(208);
            if (copy)
            {
                memset(base+copy,0,208);
                memcpy(base+copy,base+preset,204); // constructor's last field is float +200
                const float adjusted=std::max(8.0f,radius-drop);
                WriteF(base,copy+20,adjusted);
                ctx.r5.u64=copy;
                printf("kangaroo: private contact radius %.1f -> %.1f (requested height drop %.1f)\n",radius,adjusted,drop);
                fflush(stdout);
            }
        }
    }
    __imp__sub_82420998(ctx,base);
}

// 0.5.76: a tank's tags each frame (sub_82570558, its entity in r3): its
// body, turret, barrel, gunner's turret and barrel, hatches and recoil, found
// on its model by their names' hashes, hashed once into 0x82CA1AC8.. (the
// flags at +6888). Where they are is where the tank is got into (the
// hatches) and aimed from. The Kangaroo (a Sherman given the Kangaroo's
// model) has none of a tank's turret, barrel or hatches, so it could not be
// got into. For a Kangaroo the names asked for are its own for the while:
// the hatches its driver's and gunner's places, the turret and barrel its
// gun's mount and barrel.
extern "C" PPC_FUNC(__imp__sub_82570558);
PPC_FUNC(sub_82570558)
{
    constexpr uint32_t Hashes = 0x82CA0000u, Flags = Hashes + 6888;
    const bool kangaroo = !g_kangarooEntities.empty() && g_kangarooEntities.count(ctx.r3.u32) != 0 &&
                          (Guest::Read32(base, Flags) & 0xFF) == 0xFF;
    if (!kangaroo) { __imp__sub_82570558(ctx, base); return; }
    struct Swap { uint32_t at; const char* name; };
    static const Swap swaps[] = {
        { 6880, "tag_gunner_turret" }, { 6876, "tag_gunner_barrel" },
        { 6864, "tag_gunner" }, { 6860, "tag_driver" }, { 6856, "tag_gunner_barrel" },
    };
    static uint32_t hashes[5] = {};
    if (!hashes[0])
        for (int i = 0; i < 5; i++) hashes[i] = Call3(ctx, base, 0x820E10E0, GuestText(base, swaps[i].name), 0, 0);
    uint32_t saved[5];
    for (int i = 0; i < 5; i++) { saved[i] = Guest::Read32(base, Hashes + swaps[i].at); Guest::Write32(base, Hashes + swaps[i].at, hashes[i]); }
    __imp__sub_82570558(ctx, base);
    for (int i = 0; i < 5; i++) Guest::Write32(base, Hashes + swaps[i].at, saved[i]);
}

// 0.5.77 (finding what the Kangaroo lacks): every tag the game looks up on
// a model (sub_82435668: the DObj r3, the name's hash r4; -1 when the model
// has none) is logged once per tag and caller when the model is a
// Kangaroo's, with the name when it is one of the names below. And once a
// map, a few seconds in, every vehicle's tags are listed (the DObj's models
// at +128, their count at +170; a model's skeleton is its first non-zero
// word from +36, its bone table at +8: the count at +16, 12-byte entries
// from +20 with the name's hash at +4), and the names the tank's own tag
// code hashes (sub_82570558's, from 0x820655BC on) printed.
namespace
{
    const char* const KnownTags[] = {
        "tag_origin", "tag_body", "tag_body2", "tag_turret", "tag_turret2", "tag_barrel", "tag_barrel2", "tag_flash", "tag_flash2",
        "tag_recoil", "tag_driver", "tag_passenger", "tag_passenger1", "tag_passenger2", "tag_passenger3", "tag_passenger4",
        "tag_passenger5", "tag_passenger6", "tag_gunner", "tag_gunner1", "tag_gunner2", "tag_gunner_turret", "tag_gunner_barrel",
        "tag_gunner_flash", "tag_gunner_recoil", "tag_hatch", "tag_hatch1", "tag_hatch2", "tag_hatch_open", "tag_driver_hatch",
        "tag_gunner_hatch", "tag_commander", "tag_commander_hatch", "tag_loader", "tag_enter_driver", "tag_enter_gunner",
        "tag_enter_passenger", "tag_enter_back", "tag_enter", "tag_exit", "tag_tread_left", "tag_tread_right", "tag_gear_left",
        "tag_gear_right", "tag_wheel_front_left", "tag_wheel_front_right", "tag_wheel_back_left", "tag_wheel_back_right",
        "tag_wheel_middle_left", "tag_wheel_middle_right", "tag_wheel_left", "tag_wheel_right", "tag_wheel1", "tag_wheel2",
        "tag_engine", "tag_engine_left", "tag_engine_right", "tag_detach", "tag_player", "tag_camera", "tag_flag", "tag_mg",
        "tag_mg_flash", "tag_mg_turret", "tag_mg_barrel", "tag_antenna", "tag_headlight_left", "tag_headlight_right",
        "tag_exhaust", "tag_smoke", "tag_fire", "tag_death_fx", "tag_seat", "tag_seat1", "tag_seat2", "tag_gun", "gun_mount",
        "gun_mount1", "tag_gun_mount", "tag_steering", "tag_steeringwheel", "tag_steering_wheel", "tag_wheel_steering",
        "tag_cargo", "tag_door_left", "tag_door_right", "tag_light", "tag_sideplate", "tag_sideplate_left", "tag_sideplate_right",
        "tag_handlebars", "tag_handlebar", "tag_rider", "tag_sidecar", "tag_sidecar_gunner", "tag_fender", "tag_spare",
        "tag_window", "tag_glass", "tag_windshield", "tag_hood", "tag_trunk", "tag_left", "tag_right", "tag_front", "tag_back",
        "tag_rear", "tag_top", "tag_bottom", "tag_hull", "tag_chassis", "tag_tracks", "tag_track_left", "tag_track_right",
        "tag_treads", "tag_tread", "tag_wheels", "tag_bogie", "tag_shield", "tag_ammo", "tag_weapon", "tag_weapon_right",
        "tag_weapon_left", "tag_inhand", "tag_eye", "tag_view", "tag_aim", "tag_aim_animated", "tag_pivot", "tag_mount",
        "tag_mantle", "tag_propeller", "tag_prop", "tag_canopy", "tag_wing_left", "tag_wing_right", "tag_rocket", "tag_rocket1",
        "tag_rocket2", "tag_rocket3", "tag_rocket4", "tag_bomb", "tag_pilot", "tag_tail", "j_wheel", "j_body", "j_turret",
        "j_barrel", "ng_tread", "ng_wheel1", "ng_wheel2", "ng_sideplate", "ng_body", "ng_body3", "ng_main", "tread", "body",
        "turret", "barrel", "hatch", "driver", "gunner", "passenger", "wheel", "origin",
        "tag_enter_left", "tag_enter_right", "tag_enter_back_left", "tag_enter_back_right", "tag_enter_turret",
        "tag_hatch_left", "tag_hatch_right", "tag_gunner_player", "tag_flash_2", "tag_flash_11", "tag_flash_22",
        "tag_gunhull", "tag_hand_left", "tag_hand_right", "tag_gunner_hand_left", "tag_gunner_hand_right",
    };
    std::map<uint32_t, std::string> g_tagNames;
    std::set<uint64_t> g_tagAsked;
    std::string g_probedMap;
    uint64_t g_probeAt = 0;

    uint32_t TagHash(const char* text)
    {
        uint32_t h = 0;
        for (const char* p = text; *p; p++) h = h * 33 + uint32_t(tolower(uint8_t(*p)));
        return h;
    }
    std::string TagName(uint32_t hash)
    {
        if (g_tagNames.empty()) for (const char* n : KnownTags) g_tagNames[TagHash(n)] = n;
        const auto it = g_tagNames.find(hash);
        char text[16];
        snprintf(text, sizeof(text), "%08X", hash);
        return it != g_tagNames.end() ? it->second : std::string("?") + text;
    }
    bool IsKangarooDObj(uint8_t* base, uint32_t dobj)
    {
        if (!dobj) return false;
        for (const uint32_t entity : g_kangarooEntities) if (Guest::Read32(base, entity + 492) == dobj) return true;
        return false;
    }
    std::string BoneList(uint8_t* base, uint32_t dobj)
    {
        std::string out;
        if (!dobj) return "no model";
        const int models = base[dobj + 170];
        for (int m = 0; m < models && m < 8; m++)
        {
            const uint32_t model = Guest::Read32(base, dobj + 128 + 4 * uint32_t(m));
            if (!model) continue;
            uint32_t skeleton = 0;
            for (int k = 0; k < 8 && !skeleton; k++) skeleton = Guest::Read32(base, model + 36 + 4 * uint32_t(k));
            const uint32_t table = skeleton ? Guest::Read32(base, skeleton + 8) : 0;
            const uint32_t count = table ? Guest::Read32(base, table + 16) : 0;
            const uint32_t entries = table ? Guest::Read32(base, table + 20) : 0;
            out += "[model " + std::to_string(m) + ": " + std::to_string(count) + " tags]";
            for (uint32_t i = 0; entries && i < count && i < 160; i++) out += " " + TagName(Guest::Read32(base, entries + 12 * i + 4));
        }
        return out;
    }
}

// 0.5.78: the tags a vehicle is got into by, which the Kangaroo's model
// lacks (0.5.77's log, the names found from their hashes): the Panzer, the
// Horch, the BMW and the WC51 all have tag_enter_left and tag_enter_right
// (the WC51 and the Horch tag_enter_back_left/right too), the tanks
// tag_hatch_left/right, tag_enter_turret and tag_gunhull. The Kangaroo has
// tag_enter_back, its six wheels, tag_driver, tag_gunner and its gun's
// mount. Asked on a Kangaroo, each missing one is answered with the nearest
// it has. This is the lookup under every tag lookup (sub_82434E60: the
// DObj r3, the name's hash r4; the tag's index, or -1).
namespace
{
    struct TagSwap { const char* missing; const char* instead; };
    const TagSwap KangarooTags[] = {
        { "tag_enter_left", "tag_wheel_middle_left" }, { "tag_enter_right", "tag_wheel_middle_right" },
        { "tag_enter_back_left", "tag_enter_back" }, { "tag_enter_back_right", "tag_enter_back" },
        { "tag_hatch_left", "tag_driver" }, { "tag_hatch_right", "tag_gunner" },
        { "tag_enter_turret", "tag_gunner_turret" }, { "tag_gunhull", "gun_mount" },
        { "tag_gunner_player", "tag_gunner" }, { "tag_steeringwheel", "tag_driver" },
        { "tag_recoil", "tag_barrel" },
    };
    uint32_t KangarooInstead(uint32_t hash)
    {
        static std::map<uint32_t, uint32_t> swaps;
        TagName(0);   // the known names first
        if (swaps.empty())
            for (const TagSwap& s : KangarooTags)
            {
                swaps[TagHash(s.missing)] = TagHash(s.instead);
                g_tagNames[TagHash(s.missing)] = s.missing;
            }
        const auto it = swaps.find(hash);
        return it != swaps.end() ? it->second : 0;
    }
}

extern "C" PPC_FUNC(__imp__sub_82434E60);
uint32_t KillcamRigModelsOk(uint8_t* base, uint32_t rig);   // bots.cpp, 0.5.365
bool KillcamReplayingNow();
PPC_FUNC(sub_82434E60)
{
    const uint32_t dobj = ctx.r3.u32, hash = ctx.r4.u32, from = uint32_t(ctx.lr);
    // 0.5.365 (Justin 00:03, crash in a killcam after a vehicle death: this
    // lookup read 0x96170000 through a model of the rig): only the rig's
    // models that can be read are looked through; the rest are skipped for
    // this call (the count put back after) and the first such rig is logged.
    uint8_t countWas = 0; bool cut = false;
    if (dobj >= 0x40000000u && dobj < 0xC0000000u)
    {
        countWas = Guest::Read8(base, dobj + 0xAA);
        const uint32_t ok = KillcamRigModelsOk(base, dobj);
        if (ok < countWas)
        {
            cut = true;
            Guest::Write8(base, dobj + 0xAA, uint8_t(ok));
            static int told = 0;
            if (told < 12)
            {
                told++;
                printf("tags: rig %08X has a model that can't be read (%u of %u fine; models %08X %08X %08X; killcam %s; asked from %08X): skipped\n",
                       dobj, ok, countWas, Guest::Read32(base, dobj + 0x80), Guest::Read32(base, dobj + 0x84), Guest::Read32(base, dobj + 0x88),
                       KillcamReplayingNow() ? "playing" : "off", from);
                fflush(stdout);
            }
        }
    }
    PPCContext saved = ctx;
    __imp__sub_82434E60(ctx, base);
    if (cut) Guest::Write8(base, dobj + 0xAA, countWas);
    if (g_kangarooEntities.empty() || !IsKangarooDObj(base, dobj)) return;
    const int32_t found = int32_t(ctx.r3.u32);
    int32_t instead = -1;
    if (found < 0)
        if (const uint32_t other = KangarooInstead(hash))
        {
            PPCContext again = saved;
            again.r4.u64 = other;
            __imp__sub_82434E60(again, base);
            instead = int32_t(again.r3.u32);
            if (instead >= 0) ctx.r3.u64 = again.r3.u64;
        }
    (void)found;
    if (instead >= 0 && g_tagAsked.size() < 600 && g_tagAsked.insert((uint64_t(hash) << 32) | from).second)
    {
        printf("kangaroo: tag %s asked: none, its stand-in's %d\n", TagName(hash).c_str(), instead);
        fflush(stdout);
    }
}

// 0.5.104: a side-entry stand-in is a rotating road wheel. Keep its
// position, but use the hull's orientation when it is the player's link.
// sub_82571A58 returns a 48-byte matrix (rotation +0..32, origin +36..44),
// verified against the generated implementation; never alter wheel poses.
namespace {
    std::atomic<bool> g_kangarooEntry{true};
    std::mutex g_kangarooApproachMutex;
    struct KangarooApproach { uint32_t vehicle; int direction; };
    std::map<uint32_t, KangarooApproach> g_kangarooApproaches;
}
void MpFixesKangarooEntry(int enabled)
{
    g_kangarooEntry.store(enabled != 0);
    printf("kangaroo: stable entry orientation %s\n", enabled ? "on" : "off");
    fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_82571A58);

PPC_FUNC(sub_82571A58)
{
    const uint32_t player = ctx.r3.u32, out = ctx.r4.u32;
    const uint32_t link = player ? Guest::Read32(base, player + 900) : 0;
    const uint32_t vehicle = link ? Guest::Read32(base, link) : 0;
    const uint32_t hash = link ? Guest::Read32(base, link + 8) : 0;
    if (g_kangarooEntities.count(vehicle))
    {
        int direction = 0;
        if (hash == TagHash("tag_enter_left")) direction = 1;
        if (hash == TagHash("tag_enter_right")) direction = -1;
        if (hash == TagHash("tag_enter_back") || hash == TagHash("tag_enter_back_left") ||
            hash == TagHash("tag_enter_back_right") || hash == TagHash("tag_enter_turret")) direction = 3;
        if (direction) {
            std::lock_guard<std::mutex> guard(g_kangarooApproachMutex);
            g_kangarooApproaches[player] = {vehicle, direction};
        }
    }
    const bool side = hash == TagHash("tag_enter_left") || hash == TagHash("tag_enter_right");
    __imp__sub_82571A58(ctx, base);
    if (!out || !side || !g_kangarooEntry.load() || !g_kangarooEntities.count(vehicle)) return;
    // The game's own angle-to-axis routine writes exactly nine floats.
    // Use a copied context so the original function's return is preserved.
    PPCContext axis = ctx;
    axis.r3.u64 = vehicle + 352;
    axis.r4.u64 = out;
    sub_82551D60(axis, base);
    static std::set<uint32_t> logged;
    if (logged.insert(hash).second)
    {
        printf("kangaroo: %s link uses hull orientation, retaining entry position\n", TagName(hash).c_str());
        fflush(stdout);
    }
}
// 0.5.106: finish a dismount outside the hull, regardless of the seat's
// inherited Horch animation. Only the two verified exit-completion callers
// of Unlink are intercepted; seat changes and attachment setup are untouched.
namespace
{
    std::atomic<int> g_kangarooExit{1}; // 0 native, 1 prefer left, 2 prefer right
    std::mutex g_kangarooExitMutex;

    bool KangarooExitPoint(PPCContext& ctx, uint8_t* base, uint32_t player,
                           uint32_t vehicle, int preferred, uint32_t& position, int& side)
    {
        // Private scratch and a separate guest stack. Vectors are 16-byte
        // aligned: the native traces load four words even for a vec3.
        static uint32_t memory = 0;
        if (!memory) memory = Guest::AllocatePhysical(0x21000);
        if (!memory) return false;
        const uint32_t sp = memory + 0x1FF00, scratch = memory + 0x20000;
        const uint32_t result = scratch, start = scratch + 96, end = scratch + 112;
        const uint32_t mins = scratch + 128, maxs = scratch + 144, filter = scratch + 160;
        position = scratch + 192;
        memset(base + scratch, 0, 256);
        float lower[3], upper[3], hullMin[3], hullMax[3], origin[3];
        for (int k = 0; k < 3; k++)
        {
            lower[k] = ReadF(base, player + 240 + 4 * k);
            upper[k] = ReadF(base, player + 256 + 4 * k);
            hullMin[k] = ReadF(base, vehicle + 240 + 4 * k);
            hullMax[k] = ReadF(base, vehicle + 256 + 4 * k);
            origin[k] = ReadF(base, vehicle + 336 + 4 * k);
            if (!std::isfinite(lower[k]) || !std::isfinite(upper[k]) ||
                !std::isfinite(hullMin[k]) || !std::isfinite(hullMax[k]) ||
                !std::isfinite(origin[k]) || lower[k] > upper[k] || hullMin[k] > hullMax[k] ||
                fabsf(lower[k]) > 128 || fabsf(upper[k]) > 128 ||
                fabsf(hullMin[k]) > 512 || fabsf(hullMax[k]) > 512) return false;
            WriteF(base, mins + 4 * k, lower[k]);
            WriteF(base, maxs + 4 * k, upper[k]);
        }
        const float yaw = ReadF(base, vehicle + 356);
        if (!std::isfinite(yaw)) return false;
        const float radians = yaw * 0.01745329252f;
        const float fx = cosf(radians), fy = sinf(radians), lx = -fy, ly = fx;
        const float radius = sqrtf(std::max(lower[0] * lower[0], upper[0] * upper[0]) +
                                   std::max(lower[1] * lower[1], upper[1] * upper[1]));
        if (radius < 4.0f || upper[2] - lower[2] < 24.0f) return false;
        float ahead = (ReadF(base, player + 336) - origin[0]) * fx +
                      (ReadF(base, player + 340) - origin[1]) * fy;
        if (!std::isfinite(ahead) || hullMax[0] - hullMin[0] < 2 * radius) return false;
        ahead = std::clamp(ahead, hullMin[0] + radius, hullMax[0] - radius);

        // Ordinary native trace filter from sub_82583Cxx: 24 bytes, vtable
        // +0, ignored entity's handle +4, contents mask +20. Trace arguments
        // are result/start/mins/maxs/end/filter/r9/r10, then stack +84/+92.
        // Result is 80 bytes: hit position +0, normal +16, fraction +32, solid +60/+61,
        // hit handle +48. These layouts were checked in the generated code.
        Guest::Write32(base, filter, 0x82065DCCu);
        Guest::Write32(base, filter + 4, Guest::Read32(base, player + 484));
        Guest::Write32(base, filter + 20, 0x00210011u);
        auto trace = [&](float sx, float sy, float sz, float ex, float ey, float ez)
        {
            memset(base + result, 0, 80);
            WriteF(base, result + 32, 1.0f);
            WriteF(base, start, sx); WriteF(base, start + 4, sy); WriteF(base, start + 8, sz);
            WriteF(base, end, ex); WriteF(base, end + 4, ey); WriteF(base, end + 8, ez);
            Guest::Write32(base, sp, 0);
            Guest::Write32(base, sp + 84, 0);
            Guest::Write32(base, sp + 92, 0);
            PPCContext call = ctx;
            call.r1.u64 = sp; call.lr = 0;
            call.r3.u64 = result; call.r4.u64 = start; call.r5.u64 = mins;
            call.r6.u64 = maxs; call.r7.u64 = end; call.r8.u64 = filter;
            call.r9.u64 = 0; call.r10.u64 = 0; call.f1.f64 = 0;
            sub_82534200(call, base);
            return std::isfinite(ReadF(base, result + 32)) &&
                   !base[result + 60] && !base[result + 61];
        };
        const float cx = origin[0] + fx * ahead, cy = origin[1] + fy * ahead;
        const float top = origin[2] + hullMax[2] - lower[2] + 24.0f;
        const float bottom = origin[2] + hullMin[2] - 192.0f;
        for (int attempt = 0; attempt < 2; attempt++)
        {
            side = preferred == 3 ? (attempt == 0 ? 3 : 1) : (attempt == 0 ? preferred : -preferred);
            for (float extra : { 24.0f, 48.0f, 72.0f })
            {
                const float across = side > 0 ? hullMax[1] + radius + extra : hullMin[1] - radius - extra;
                const float behind = hullMin[0] - radius - extra;
                const float x = side == 3 ? origin[0] + fx * behind : cx + lx * across;
                const float y = side == 3 ? origin[1] + fy * behind : cy + ly * across;
                // Sweep over the hull, then down to the ground with the full
                // player box. Reject walls, solid starts, steep faces, void,
                // and a landing on this vehicle itself.
                if (!trace(cx, cy, top, x, y, top) || ReadF(base, result + 32) < 1.0f) continue;
                if (!trace(x, y, top, x, y, bottom)) continue;
                const float fraction = ReadF(base, result + 32);
                const float normalZ = ReadF(base, result + 24); // +0 is hit position, +16 is normal
                printf("kangaroo: exit trace: direction %d, clearance %.0f, fraction %.3f, ground Z %.1f, normal Z %.3f\n", side, extra, fraction, ReadF(base, result + 8), normalZ);
                if (fraction < 0 || fraction >= 1 || !std::isfinite(normalZ) || normalZ < 0.6f ||
                    Guest::Read32(base, result + 48) == Guest::Read32(base, vehicle + 484)) continue;
                WriteF(base, position, x); WriteF(base, position + 4, y);
                WriteF(base, position + 8, top + (bottom - top) * fraction + 1.0f);
                return true;
            }
        }
        return false;
    }
}
void MpFixesKangarooExit(int mode)
{
    g_kangarooExit.store(std::clamp(mode, 0, 2));
    printf("kangaroo: exit mode %d (0 native, 1 prefer left, 2 prefer right)\n", g_kangarooExit.load());
    fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_8255AFF0);
PPC_FUNC(sub_8255AFF0)
{
    const uint32_t player = ctx.r3.u32, caller = uint32_t(ctx.lr);
    const uint32_t link = player ? Guest::Read32(base, player + 900) : 0;
    const uint32_t vehicle = link ? Guest::Read32(base, link) : 0;
    const uint32_t client = player ? Guest::Read32(base, player + 512) : 0;
    const bool kangaroo = client && g_kangarooEntities.count(vehicle) != 0;
    __imp__sub_8255AFF0(ctx, base);
    if (!kangaroo || Guest::Read32(base, player + 900) != 0) return;
    printf("kangaroo: unlink player %08X from %08X at %08X, seat %d\n", player, vehicle, caller,
           int32_t(Guest::Read32(base, client + 1300)));
    fflush(stdout);
}

// 0.5.107: the live game's dismount path is 8257ABF0. It unlinks at
// 8257AD34, chooses an exit, then sets origin at 8257B180. Apply AFTER
// the whole function, only when its in-vehicle bit and link are cleared.
extern "C" PPC_FUNC(__imp__sub_8257ABF0);
PPC_FUNC(sub_8257ABF0)
{
    const uint32_t player = ctx.r3.u32;
    const uint32_t client = player ? Guest::Read32(base, player + 512) : 0;
    const uint32_t link = player ? Guest::Read32(base, player + 900) : 0;
    const uint32_t vehicle = link ? Guest::Read32(base, link) : 0;
    const bool wasMounted = client && (Guest::Read32(base, client + 240) & 0x100000u);
    int preferred = g_kangarooExit.load() == 2 ? -1 : 1;
    {
        std::lock_guard<std::mutex> guard(g_kangarooApproachMutex);
        auto found = g_kangarooApproaches.find(player);
        if (found != g_kangarooApproaches.end() && found->second.vehicle == vehicle)
            preferred = found->second.direction;
    }
    __imp__sub_8257ABF0(ctx, base);
    if (!wasMounted || !g_kangarooEntities.count(vehicle) || !g_kangarooExit.load() ||
        Guest::Read32(base, player + 900) || (Guest::Read32(base, client + 240) & 0x100000u)) return;
    std::lock_guard<std::mutex> lock(g_kangarooExitMutex);
    uint32_t position = 0;
    int side = 0;
    if (!KangarooExitPoint(ctx, base, player, vehicle, preferred, position, side))
    {
        printf("kangaroo: exit: no clear grounded landing for direction %d; native position retained\n", preferred);
        fflush(stdout);
        return;
    }
    PPCContext place = ctx;
    place.r1.u64 = position - 448; place.lr = 0;
    place.r3.u64 = player; place.r4.u64 = position;
    sub_8255E118(place, base);
    printf("kangaroo: exit: placed on %s at %.1f %.1f %.1f\n", side == 3 ? "rear" : side > 0 ? "left" : "right",
           ReadF(base, player + 336), ReadF(base, player + 340), ReadF(base, player + 344));
    fflush(stdout);
}

// 0.5.78: who asks which tag of which vehicle (sub_82435668, the caller
// its return address), every vehicle's model, not only the Kangaroo's: with
// Use pressed next to the jeep and next to the Kangaroo, what getting in
// asks shows. The wheels' and the turret's every-frame asks are left out.
namespace { std::map<uint32_t, std::string> g_vehicleDObjs; std::set<uint64_t> g_vehicleAsked; }
extern "C" PPC_FUNC(__imp__sub_82435668);
PPC_FUNC(sub_82435668)
{
    const uint32_t dobj = ctx.r3.u32, hash = ctx.r4.u32, from = uint32_t(ctx.lr);
    __imp__sub_82435668(ctx, base);
    if (g_vehicleDObjs.empty() || from == 0x8258D72C || from == 0x8256FD30) return;
    const auto it = g_vehicleDObjs.find(dobj);
    if (it == g_vehicleDObjs.end()) return;
    if (g_vehicleAsked.size() < 800 && g_vehicleAsked.insert((uint64_t(hash) << 32) ^ from ^ (uint64_t(dobj) << 8)).second)
    {
        printf("vehicles: %s's tag %s asked from %08X: %d\n", it->second.c_str(), TagName(hash).c_str(), from, int32_t(ctx.r3.u32));
        fflush(stdout);
    }
}

// 0.5.81: where each Kangaroo is, every five seconds while it moves.
void MpFixesKangarooWhere(uint8_t* base)
{
    static uint64_t next = 0;
    static float last[3] = {};
    if (g_kangarooEntities.empty() || GetTickCount64() < next) return;
    next = GetTickCount64() + 5000;
    for (const uint32_t entity : g_kangarooEntities)
    {
        const float x = ReadF(base, entity + 336), y = ReadF(base, entity + 340), z = ReadF(base, entity + 344);
        if (fabsf(x - last[0]) + fabsf(y - last[1]) + fabsf(z - last[2]) < 2.0f) continue;
        last[0] = x; last[1] = y; last[2] = z;
        printf("kangaroo: at %.0f %.0f %.0f, facing %.0f\n", x, y, z, ReadF(base, entity + 356));
        fflush(stdout);
    }
    // 0.5.92: both treads' bone matrices as last worked out (the model
    // object's matrices, +112, 64 bytes a bone, as sub_8241E908 writes them),
    // and the four wheels next to them, to see what sets the bad tread apart.
    for (const uint32_t entity : g_kangarooEntities)
    {
        const uint32_t dobj = Guest::Read32(base, entity + 492);
        const uint32_t model = dobj ? Guest::Read32(base, dobj + 128) : 0;
        uint32_t skeleton = 0;
        for (int k = 0; model && k < 8 && !skeleton; k++) skeleton = Guest::Read32(base, model + 36 + 4 * uint32_t(k));
        const uint32_t table = skeleton ? Guest::Read32(base, skeleton + 8) : 0;
        const uint32_t count = table ? Guest::Read32(base, table + 16) : 0;
        const uint32_t names = table ? Guest::Read32(base, table + 20) : 0;
        const uint32_t matrices = dobj ? Guest::Read32(base, dobj + 112) : 0;
        if (!names || !matrices || count > 160) continue;
        static const char* const wanted[] = { "tag_tread_left", "tag_tread_right", "tag_gear_left", "tag_gear_right",
                                              "tag_wheel_middle_left", "tag_wheel_middle_right" };
        for (const char* name : wanted)
            for (uint32_t i = 0; i < count; i++)
            {
                if (Guest::Read32(base, names + 12 * i + 4) != TagHash(name)) continue;
                printf("kangaroo: bone %s (%u) speed %.1f:", name, i, ReadF(base, entity + 716));
                for (uint32_t w = 0; w < 16; w++) printf(" %.2f", ReadF(base, matrices + i * 64 + 4 * w));
                printf("\n");
                break;
            }
        fflush(stdout);
        break;
    }
}

void MpFixesKangarooProbe(PPCContext& ctx, uint8_t* base)
{
    const std::string map = MapNow() + "#" + std::to_string(g_mapStarts);
    if (MapNow().empty() || map == g_probedMap) return;
    if (!g_probeAt) { g_probeAt = GetTickCount64() + 5000; g_vehicleDObjs.clear(); g_vehicleAsked.clear(); return; }
    if (GetTickCount64() < g_probeAt) return;
    g_probeAt = 0;
    g_probedMap = map;
    printf("vehicles: a few seconds in:\n");
    for (const Moved& m : g_movedFrom) ScanForPlace(base, m);
    g_movedFrom.clear();
    printf("kangaroo: tag hash check: tag_origin %08X here, %08X the game's\n", TagHash("tag_origin"),
           Call3(ctx, base, 0x820E10E0, GuestText(base, "tag_origin"), 0, 0));
    static const uint32_t names[] = { 0x820655BC, 0x820655D0, 0x820655EC, 0x820655F8, 0x82065604, 0x82065614, 0x82065624, 0x8206544C, 0x820676BC };
    for (const uint32_t at : names) printf("kangaroo: the tank code's name at %08X: %s\n", at, ReadText(base, at, 40).c_str());
    const uint32_t pool = Guest::Read32(base, 0x82BCBDB4);
    const int records = int(Guest::Read16(base, 0x82C281D0 + 3088));
    std::set<std::string> listed;
    for (int i = 0; pool && i < records; i++)
    {
        const uint32_t record = pool + 1872 * uint32_t(i);
        const uint32_t handle = Guest::Read32(base, record + 368), index = handle & 0x3FFF;
        if (index >= 4096) continue;
        const uint32_t entry = 0x82BD1FC0 + 8 * index;
        if ((handle >> 14) != Guest::Read32(base, entry + 4)) continue;
        const uint32_t entity = Guest::Read32(base, entry);
        if (!entity) continue;
        const int kind = int(int16_t(Guest::Read16(base, record + 376)));
        const uint32_t def = kind >= 0 && kind < 32 ? Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(kind)) : 0;
        std::string name = def ? ReadText(base, def, 32) : std::string("?");
        const bool kangaroo = g_kangarooEntities.count(entity) != 0;
        if (kangaroo) name += " (the Kangaroo)";
        g_vehicleDObjs[Guest::Read32(base, entity + 492)] = name + " " + std::to_string(i);
        if (!listed.insert(name).second) continue;
        printf("kangaroo: %s's tags: %s\n", name.c_str(), BoneList(base, Guest::Read32(base, entity + 492)).c_str());
        // 0.5.82: its box as linked (mins +240, maxs +256, the world's +272 and
        // +284), its model object's first 256 bytes (which parts are hidden:
        // the Kangaroo's treads and wheels are never drawn) and its kind's file.
        if (kangaroo || name.rfind("mp_panzeriv", 0) == 0 || name.rfind("mp_shermanTank", 0) == 0)
        {
            printf("kangaroo: %s's box %.0f %.0f %.0f to %.0f %.0f %.0f, in the world %.0f %.0f %.0f to %.0f %.0f %.0f\n", name.c_str(),
                   ReadF(base, entity + 240), ReadF(base, entity + 244), ReadF(base, entity + 248), ReadF(base, entity + 256),
                   ReadF(base, entity + 260), ReadF(base, entity + 264), ReadF(base, entity + 272), ReadF(base, entity + 276),
                   ReadF(base, entity + 280), ReadF(base, entity + 284), ReadF(base, entity + 288), ReadF(base, entity + 292));
            const uint32_t dobj = Guest::Read32(base, entity + 492);
            std::string words;
            char word[12];
            for (uint32_t at = 0; dobj && at < 256; at += 4) { snprintf(word, sizeof(word), " %08X", Guest::Read32(base, dobj + at)); words += word; }
            printf("kangaroo: %s's model object:%s\n", name.c_str(), words.c_str());
            // 0.5.88: the entity's own words around its box, to find what
            // makes a player bump into a vehicle (Justin walks into the
            // Kangaroo's hull but not the tanks').
            words.clear();
            for (uint32_t at = 200; at < 340; at += 4) { snprintf(word, sizeof(word), " %08X", Guest::Read32(base, entity + at)); words += word; }
            printf("kangaroo: %s's entity +200..+336:%s\n", name.c_str(), words.c_str());
            DumpVehicleKind(base, record);
        }
    }
    fflush(stdout);
}

// 0.5.291: a vehicle file read (sub_825683D8: its name, its data): from
// Laison's archive only the mortar's.
extern "C" PPC_FUNC(__imp__sub_825683D8);
PPC_FUNC(sub_825683D8)
{
    if (g_laisonVehicleFilter)
    {
        const std::string name = ReadText(base, ctx.r3.u32, 40);
        const bool wanted = name == "mortar_81mm";
        printf("mortar: Laison's vehicle file %s %s\n", name.c_str(), wanted ? "read" : "left out");
        fflush(stdout);
        if (!wanted || Guest::Read16(base, 0x82B34594) >= 31) return;
    }
    __imp__sub_825683D8(ctx, base);
}

// Room for more vehicles: the map's vehicle records (sub_82561838: how
// many, 1872 bytes each, the ones in use copied over and their entities'
// +524 pointed at the copies) are as many as the map asked for (Merville
// in a devmap: 5, all taken by its own vehicles, so spawnvehicle had none).
// Sixteen more each time, for vehicles put down by hand: 30 KB.
void MpFixesPlacementDue();
extern "C" PPC_FUNC(__imp__sub_82561838);
PPC_FUNC(sub_82561838)
{
    const uint32_t asked = ctx.r3.u32 & 0xFFFF;
    // The map's vehicle files are read just before (in the map's start,
    // sub_82593FA8, which then makes the records here): Merville's are read
    // after them when its archive is here, then the Kangaroo made.
    // (0.5.62 waited for sub_8256CE08 to do it, which this map start does
    // not call.)
    if (g_vehicleArchive >= 0)
    {
        const uint32_t archives = Guest::Read32(base, 0x82BCBE1C);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        call.r3.u64 = archives; call.r4.u64 = uint32_t(g_vehicleArchive); call.r5.u64 = 0x820658F0; call.r6.u64 = 0x825683D8; call.lr = 0;
        Guest::Lookup(0x8254B318)(call, base);
        call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        call.r3.u64 = archives; call.r4.u64 = uint32_t(g_vehicleArchive); call.r5.u64 = 0x820658FC; call.r6.u64 = 0x8255F3A8; call.lr = 0;
        Guest::Lookup(0x8254B318)(call, base);
        printf("vehicles: Merville's vehicle files read (%d kinds now)\n", int(int16_t(Guest::Read16(base, 0x82B34594))));
        fflush(stdout);
    }
    MpFixesLoadKangarooAfterMap(ctx, base);
    // 0.5.83: the Kangaroo's own archive's vehicle files too (the campaign's
    // "Kangaroo": its speed, seats, bounds, cameras and treads' scroll), read
    // as Merville's are; then mp_kangaroo is made (from the Horch's).
    if (g_kangarooArchive >= 0)
    {
        const uint32_t archives = Guest::Read32(base, 0x82BCBE1C);
        for (const auto& reader : { std::pair<uint32_t, uint32_t>{ 0x820658F0, 0x825683D8 }, std::pair<uint32_t, uint32_t>{ 0x820658FC, 0x8255F3A8 } })
        {
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
            call.r3.u64 = archives; call.r4.u64 = uint32_t(g_kangarooArchive); call.r5.u64 = reader.first; call.r6.u64 = reader.second; call.lr = 0;
            Guest::Lookup(0x8254B318)(call, base);
        }
        printf("vehicles: the Kangaroo's archive's vehicle files read (%d kinds now)\n", int(int16_t(Guest::Read16(base, 0x82B34594))));
        fflush(stdout);
    }
    if (g_kangarooArchive >= 0) MakeKangarooKind(ctx, base);
    MpFixesLoadLaisonAfterMap(ctx, base);   // 0.5.296: Laison's archive after the map's
    // 0.5.291: the campaign mortar as a vehicle kind (Laison's
    // VEHICLEFILE:mortar_81mm, its turret mortar_81mm_turret), the other six
    // of Laison's vehicles left out (g_laisonVehicleFilter, sub_825683D8).
    static std::string laisonVehiclesMap;   // 0.5.297: once a map (296 read it at each of the hook's calls: 3, 4, 5 kinds)
    if (g_laisonArchive >= 0 && laisonVehiclesMap != Kernel::CurrentMap())
    {
        laisonVehiclesMap = Kernel::CurrentMap();
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        call.r3.u64 = Guest::Read32(base, 0x82BCBE1C); call.r4.u64 = uint32_t(g_laisonArchive); call.r5.u64 = 0x820658F0; call.r6.u64 = 0x825683D8; call.lr = 0;
        g_laisonVehicleFilter = true;
        Guest::Lookup(0x8254B318)(call, base);
        g_laisonVehicleFilter = false;
        printf("mortar: Laison's vehicle files read (%d kinds now)\n", int(int16_t(Guest::Read16(base, 0x82B34594))));
        fflush(stdout);
    }
    ctx.r3.u64 = std::min<uint32_t>(asked + 16, 64);
    printf("vehicles: room for %u vehicles (the map asked for %u)\n", ctx.r3.u32, asked);
    fflush(stdout);
    __imp__sub_82561838(ctx, base);
    MpFixesPlacementDue();
}


// A bank (an archive) loaded with what it needs first (sub_8247DD38: the
// manager, the bank: its name at +0, its path at +8, its bounds from +12,
// its archive's number at +180), by sub_82477B50 (the manager, the name, the
// path, the bounds), then waited for. With the multiplayer's weapons
// (mp\weapusge.cod), a campaign level's weapons archive is loaded too, the
// same way: the campaign guns' models and animations.
extern "C" PPC_FUNC(__imp__sub_8247DD38);
PPC_FUNC(sub_8247DD38)
{
    const uint32_t manager = ctx.r3.u32, bank = ctx.r4.u32;
    PPCContext saved = ctx;
    std::string path = bank ? ReadText(base, Guest::Read32(base, bank + 8)) : std::string();
    for (char& c : path) c = char(tolower(static_cast<unsigned char>(c)));
    const bool weapons = path.find("weapusge") != std::string::npos;
    // (Letting the campaign archive of the map before go here, by
    // sub_8247D820, did free the room, but the next map then read the
    // game's tables of that archive after it was gone, and crashed: the
    // game's own banks are let go by more than that. Left loaded for now:
    // a map the guns are skipped on is best played first, and the archive
    // is loaded once and used again by the maps after.)
    __imp__sub_8247DD38(ctx, base);
    const uint32_t result = ctx.r3.u32;
    if (weapons) MpFixesLoadVehicleArchive(saved, base, manager, bank);
    ctx.r3.u64 = result;
    if (weapons && !Settings::Get().campaignGuns) MpFixesLoadMortarArchive(saved, base, manager, bank);   // 0.5.285
    if (weapons && !Settings::Get().campaignGuns) MpFixesLoadPlaneArchives(saved, base, manager, bank);   // 0.5.255
    if (!weapons || !Settings::Get().campaignGuns) return;
    // The map: its own archive is opened before the weapons' (kernel_file.cpp).
    g_mapName = Kernel::CurrentMap();
    const bool big = Settings::Get().bigMemory;
    MpFixesLoadStukaEarly(saved, base, manager, bank);   // 0.5.271
    MpFixesLoadMortarArchive(saved, base, manager, bank);   // 0.5.285
    for (int which = 0; which < ArchiveCount; which++)
    {
        g_archiveActive[which] = -1;
        // Without big_memory: only the first, and not on the maps too full
        // for it. (With it, the banks are enough, sub_82479448 above.)
        if (which > 0 && !big) continue;
        // DIAGNOSTIC: COD3_SKIP_ARCHIVE=n leaves campaign archive n out
        if (const char* t = getenv("COD3_SKIP_ARCHIVE")) if (atoi(t) == which) { printf("weapons: archive %d left out (COD3_SKIP_ARCHIVE)\n", which); continue; }
        bool skip = false;
        for (const char* map : NoCampaignGuns)
            if (g_mapName == map && !big) skip = true;
        if (skip)
        {
            printf("weapons: no campaign guns on %s (not enough memory for their archive)\n", g_mapName.c_str());
            fflush(stdout);
            continue;
        }
        // Still loaded from a map before (its slot, manager + 4 * (number +
        // 11), holding the same archive): used again, not loaded again.
        const int32_t loaded = g_archiveLoaded[which];
        if (g_archiveObject[which] && loaded >= 0 && Guest::Read32(base, manager + 4 * uint32_t(loaded + 11)) == g_archiveObject[which])
        {
            g_archiveActive[which] = loaded;
            printf("weapons: campaign weapons archive %s still loaded as %d\n", CampaignArchives[which], loaded);
            fflush(stdout);
            continue;
        }
        static uint32_t paths[ArchiveCount] = {};
        if (!paths[which]) paths[which] = GuestText(base, CampaignArchives[which]);
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = manager;
        call.r4.u64 = Guest::Read32(base, bank + 0);
        call.r5.u64 = paths[which];
        for (int i = 0; i < 4; i++)
        {
            const uint64_t value = (uint64_t(Guest::Read32(base, bank + 12 + 8 * i)) << 32) | Guest::Read32(base, bank + 16 + 8 * i);
            (i == 0 ? call.r6 : i == 1 ? call.r7 : i == 2 ? call.r8 : call.r9).u64 = value;
        }
        // No banks: the third bound's high word (the bank's +28) is how many
        // banks the archive reserves from the pool when it is made
        // (sub_82476870, through sub_824731E0). With none reserved, its load
        // (sub_82475E18) skips the planning and takes every block from heap
        // 0 instead (the map block, 480 MB with big_memory), marked so
        // (0x20000) to be given back there. The map's own zones keep all 96
        // banks: Poissons loads whole beside both campaign archives.
        if (big) call.r8.u64 = (uint64_t(0xBF800000u) << 32) | (call.r8.u64 & 0xFFFFFFFFull);
        call.lr = 0;
        Guest::Lookup(0x82477B50)(call, base);
        const int32_t index = int32_t(call.r3.u32);
        printf("weapons: campaign weapons archive %s loading as %d (beside %s, bank %08X)\n", CampaignArchives[which], index, path.c_str(), bank);
        fflush(stdout);
        // Waited for as the bank's own is (sub_8247D700 then the two pumps,
        // till the manager's first word is set).
        if (index == -1) continue;
        for (int spins = 0; spins < 100000 && Guest::Read32(base, manager) == 0; spins++)
        {
            PPCContext pump = call;
            pump.r3.u64 = manager; pump.r4.u64 = 0;
            Guest::Lookup(0x8247D700)(pump, base);
            Guest::Lookup(0x820F9CA8)(pump, base);
            Guest::Lookup(0x820F9D00)(pump, base);
        }
        g_archiveActive[which] = index;
        g_archiveLoaded[which] = index;
        g_archiveObject[which] = Guest::Read32(base, manager + 4 * uint32_t(index + 11));
        printf("weapons: campaign weapons archive loaded (%08X)\n", g_archiveObject[which]);
        fflush(stdout);
    }
    MpFixesLoadPlaneArchives(saved, base, manager, bank);   // 0.5.255: after the guns
    ctx.r3.u64 = result;
}
#endif

// Diagnostics for Poissons' missing zone (zn01) while the campaign guns'
// archive is loaded, which more memory (big_memory) did not cure: every
// archive opened (sub_82477B50: the manager, a kind, the path; the number it
// gets) and every streaming allocation that fails (sub_8246EA50: the size;
// null when the stream heap has no room).
#ifdef COD3_MP
namespace { uint32_t g_banks = 0; }   // the bank pool's object (sub_82479448's, below), for counting
extern "C" PPC_FUNC(__imp__sub_82477B50);
PPC_FUNC(sub_82477B50)
{
    const uint32_t kind = ctx.r4.u32, path = ctx.r5.u32;
    __imp__sub_82477B50(ctx, base);
    if (kind == 3) g_mapArchive = int32_t(ctx.r3.u32);   // the map's own archive (kind 3)
    if (ctx.r3.u32 < 64) g_archiveKind[ctx.r3.u32] = int8_t(kind);   // 0.5.294
    static int said = 0;
    if ((said < 200 && said++ < 200))
    {
        // The banks' bitmaps (three words each, at +0 and +12): their set
        // bits counted, to see how full the pool is.
        int first = 0, second = 0;
        for (uint32_t w = 0; g_banks && w < 3; w++)
        {
            first += __builtin_popcount(Guest::Read32(base, g_banks + 4 * w));
            second += __builtin_popcount(Guest::Read32(base, g_banks + 12 + 4 * w));
        }
        printf("archives: %s opened as %d (kind %u); bank bitmaps %d and %d set\n", ReadText(base, path, 80).c_str(), int32_t(ctx.r3.u32), kind, first, second);
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_8246EA50);
PPC_FUNC(sub_8246EA50)
{
    const uint32_t size = ctx.r3.u32;
    __imp__sub_8246EA50(ctx, base);
    static int said = 0;
    if (ctx.r3.u32 == 0 && size != 0 && said++ < 50)
    {
        printf("archives: stream allocation of %.1f MB failed (no room in the stream heap)\n", size / 1048576.0);
        fflush(stdout);
    }
}
#endif

// The archive manager's banks (sub_82479448, its set-up): every archive
// loaded takes one bank of 3 MB a block, from a pool of num_banks banks made
// once from the map block (num_banks * 3 MB). num_banks is a setting whose
// default, 88, is the text of the double at 0x82085908 (used only there); its
// two bitmaps hold at most 96. On Poissons the map, its zones and the
// weapons fill the 88; the campaign guns' archive (9 banks) leaves no room
// for its zone 1 (10). With big_memory the map block has the room, so the
// pool is made of all 96.
#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_82479448);
PPC_FUNC(sub_82479448)
{
    const uint32_t manager = ctx.r3.u32;
    g_banks = manager;
    if (Settings::Get().bigMemory)
    {
        const double banks = 96.0;
        uint64_t bits; memcpy(&bits, &banks, 8);
        Guest::Write32(base, 0x82085908, uint32_t(bits >> 32));
        Guest::Write32(base, 0x8208590C, uint32_t(bits));
    }
    __imp__sub_82479448(ctx, base);
    const uint32_t count = Guest::Read32(base, manager + 24);
    float banks; memcpy(&banks, &count, 4);
    printf("archives: %.0f banks of %.1f MB for archives (%s)\n", banks, Guest::Read32(base, manager + 28) / 1048576.0,
           Settings::Get().bigMemory ? "big_memory: the most there can be" : "the game's own");
    fflush(stdout);
}
#endif

// The game's own memory (sub_820E0158, at start): a block of 0x171F0000
// bytes (369.9 MB) for maps and everything loaded with them, and one of
// 0x900000, both physical memory (sub_823142B0: size, highest address,
// alignment, protection); the block's start, end and size kept at
// 0x82A8665C (+0, +4, +8), the second's at +12, +16, +20. Graphics memory
// is 511 MB and full at about 492 MB with a big map, so with the campaign
// guns (their archive is about 27 MB more) the map block is made a little
// bigger out of the rest: extra_map_memory MB, 16 by default.
#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_820E0158);
PPC_FUNC(sub_820E0158)
{
    const Settings::Values& settings = Settings::Get();
    const bool big = settings.bigMemory;
    if (!big && (!settings.campaignGuns || settings.extraMapMemory <= 0)) { __imp__sub_820E0158(ctx, base); return; }
    const uint32_t state = 0x82A8665C;
    // With big_memory: the map block in the extended window (kernel.h), 480
    // MB of it instead of 370, and the 370 MB of the 512 left to the
    // graphics library's own textures and buffers.
    // 0.5.287: 608 MB with big_memory (was 495): the extended window is
    // 624 MB now; 16 left for this port's own (player storage, the MG42 view).
    const uint32_t size = big ? 0x26000000u : 0x171F0000u + uint32_t(settings.extraMapMemory) * 0x100000u;
    PPCContext call = ctx;
    call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    call.lr = 0;
    auto allocate = [&](uint32_t bytes, uint32_t protect)
    {
        call.r3.u64 = bytes; call.r4.u64 = 0xFFFFFFFFu; call.r5.u64 = 0x10000; call.r6.u64 = protect;
        Guest::Lookup(0x823142B0)(call, base);
        return call.r3.u32;
    };
    Guest::Write32(base, state + 8, size);
    uint32_t at = big ? Guest::AllocateExtended(size) : allocate(size, 0x20000004u);
    uint32_t got = size;
    if (!at)
    {
        // Not that much left: the game's own size.
        got = 0x171F0000u;
        Guest::Write32(base, state + 8, got);
        at = allocate(got, 0x20000004u);
    }
    Guest::Write32(base, state + 0, at);
    Guest::Write32(base, state + 4, at + got);
    Guest::Write32(base, state + 20, 0x900000u);
    const uint32_t second = allocate(0x900000u, 0x20000404u);
    Guest::Write32(base, state + 12, second);
    Guest::Write32(base, state + 16, second + 0x900000u);
    printf("memory: the game's map block is %.1f MB (%d MB more than the game's own) at %08X%s\n", got / 1048576.0, int((got - 0x171F0000u) / 0x100000u), at,
           Guest::IsExtendedCpu(at) ? ", past the 512 MB of graphics memory (big_memory)" : "");
    fflush(stdout);
}
#endif

// The drawing side's set-up of a weapon (sub_8245E720: its number): its
// models, animations, name and crosshair, kept at 0x82C83810, 192 bytes a
// weapon, +124 set once done. The game does it for the weapons it knows of
// as the level starts; a campaign gun, made after, is set up here once the
// player is in the game.
#ifdef COD3_MP
void MpFixesCampaignGuns(PPCContext& ctx, uint8_t* base)
{
    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
    const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
    if (!own || !Guest::Read32(base, own + 512)) return;
    MpFixesMg42View(base, Guest::Read32(base, Guest::Read32(base, own + 512) + 164));
    const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
    // 0.5.303: and Laison's mortar weapons (302's crash: mortar_shell held
    // with no drawing set-up, its arms' animation tree never made: garbage
    // calls in the animation code as the mortar's moves played).
    std::vector<int32_t> weapons(std::begin(g_campaignWeapon), std::end(g_campaignWeapon));
    for (int32_t w : g_laisonWeapon) if (w > 0) weapons.push_back(w);
    for (int32_t weapon : weapons)
    {
        if (weapon <= 0 || weapon > count || weapon >= 64) continue;
        const uint32_t info = 0x82C83810 + 192 * uint32_t(weapon);
        if (Guest::Read8(base, info + 124)) continue;
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u64 = uint32_t(weapon);
        call.lr = 0;
        Guest::Lookup(0x8245E720)(call, base);
        printf("weapons: weapon %d set up for drawing (%s)\n", weapon, Guest::Read8(base, info + 124) ? "done" : "not done");
        fflush(stdout);
    }
}
#endif

// A table looked up by a weapon's name (sub_820D1320: the table, the name;
// a list of entries, each named at +0, compared as text): the drawing
// set-up (sub_8245E720) finds its weapon's entry there, and with none it
// stops early, leaving the weapon without models, name or crosshair. A
// campaign gun has no entry of its own, so it takes the entry of the gun it
// is a copy of.
#ifdef COD3_MP
// The game's hash of a name (sub_820B00C0): h * 33 + the lowercased
// character, from 0.
static uint32_t NameHash(const std::string& name)
{
    uint32_t h = 0;
    for (char c : name) h = h * 33 + uint32_t(uint8_t(tolower(static_cast<unsigned char>(c))));
    return h;
}

// The borrowed entry lists the copied gun's animations by their names'
// hashes (+4 the count, +8 the items, 28 bytes each, the hash first): a copy
// of it is made with each animation of the copied gun's family ("geRFL")
// swapped for the campaign gun's ("brRFL"), so its own arms' moves play.
static uint32_t CampaignAnimEntry(uint8_t* base, const CampaignGun& gun, const std::string& key, uint32_t entry)
{
    if (!entry) return entry;
    static std::map<std::string, std::pair<uint32_t, uint32_t>> made;   // the gun's: the entry copied, the copy
    auto& slot = made[key];
    if (slot.first == entry && slot.second) return slot.second;
    const uint32_t count = Guest::Read32(base, entry + 4), items = Guest::Read32(base, entry + 8);
    if (!items || count == 0 || count > 256) return entry;
    // The family's moves, each with the campaign gun's name for it.
    static const char* const Moves[] = { "idle_a", "idle_ads_a", "fire", "fireADS", "rechamber", "ADSrechamber", "melee", "reload",
                                         "raise", "drop", "ads_up", "ads_down", "reload_start", "reload_end", "idle_ads", "fireLAST", "reloadSCOPE" };
    std::map<uint32_t, uint32_t> renamed;
    // 0.5.288: the mortar's hands: the interact arms' one idle -> the mortar's.
    if (strcmp(gun.name, "mortar_hands") == 0)
    {
        renamed[NameHash("w_arms_bino_idle_a")] = NameHash("w_arms_ei_MortarIdle");   // 0.5.289: from the binoculars
        renamed[NameHash("w_arms_bino_idle_ads_a")] = NameHash("w_arms_ei_MortarIdle");
    }
    for (const char* move : Moves)
        for (const char* prefix : { "w_arms_", "w_" })
        {
            // These two donor names are needed by the G43 only; preserve
            // the existing guns' animation hash maps.
            if (std::string(gun.name) != "gewehr43" &&
                (std::string(move) == "idle_ads" || std::string(move) == "fireLAST")) continue;
            if (KeepsCopyIdle(gun.name, std::string(prefix) + gun.armsFrom + "_" + move)) continue;
            std::string to = move;
            if (gun.reloadAdd > 0 && (to == "reload_start" || to == "reload_end")) to += "2";
            const std::string target = CampaignAnimationName(gun.name, std::string(prefix) + gun.armsTo + "_" + to);
            renamed[NameHash(std::string(prefix) + gun.armsFrom + "_" + move)] = NameHash(target);
        }
    const uint32_t copy = Guest::AllocatePhysical(16 + 28 * count);
    memcpy(base + copy, base + entry, 12);
    memcpy(base + copy + 16, base + items, 28 * count);
    Guest::Write32(base, copy + 8, copy + 16);
    // A reload in parts: the entry's items are the weapon's animations by
    // slot (9 the reload, 11 its start, 12 its end; the copied gun has its
    // idle in 11 and 12): the start's and end's made from the reload's item,
    // named for the copied family's start and end, swapped below.
    if (gun.reloadAdd > 0 && count > 12)
        for (uint32_t i : { 11u, 12u })
        {
            memcpy(base + copy + 16 + 28 * i, base + copy + 16 + 28 * 9, 28);
            for (const char* prefix : { "w_arms_", "w_" })
            {
                const uint32_t reload = NameHash(std::string(prefix) + gun.armsFrom + "_reload");
                const uint32_t part = NameHash(std::string(prefix) + gun.armsFrom + (i == 11 ? "_reload_start" : "_reload_end"));
                for (uint32_t word = 0; word < 7; word++)
                    if (Guest::Read32(base, copy + 16 + 28 * i + 4 * word) == reload) Guest::Write32(base, copy + 16 + 28 * i + 4 * word, part);
            }
        }
    int swapped = 0;
    // Every word of an item is looked at: besides the arms' animation (the
    // first), an item can name the gun's own (the "w_" family: the rifle's
    // moves, which carry it up to the hands while the bolt is worked).
    int gunSwapped = 0;
    for (uint32_t i = 0; i < count; i++)
    {
        std::string words;
        for (uint32_t word = 0; word < 7; word++)
        {
            const uint32_t at = copy + 16 + 28 * i + 4 * word, hash = Guest::Read32(base, at);
            char text[12]; snprintf(text, sizeof(text), " %08X", hash); words += text;
            const auto found = renamed.find(hash);
            if (found == renamed.end()) continue;
            Guest::Write32(base, at, found->second);
            if (word == 0) swapped++; else gunSwapped++;
        }
        printf("weapons:   %s item %u:%s\n", key.c_str(), i, words.c_str());
        // DIAGNOSTIC: COD3_ANIM_ITEMS=1: what an item's third word points at
        if (getenv("COD3_ANIM_ITEMS") && (i == 1 || i == 7 || i == 12 || i == 13))
        {
            const uint32_t to = Guest::Read32(base, copy + 16 + 28 * i + 8);
            std::string dump;
            for (uint32_t w = 0; to >= 0x40000000u && to < 0x90000000u && w < 24; w++)
            {
                const uint32_t v = Guest::Read32(base, to + 4 * w);
                char t[80];
                const std::string text = (v >= 0x40000000u && v < 0x90000000u) ? ReadText(base, v, 40) : std::string();
                snprintf(t, sizeof(t), " %08X%s%s", v, text.size() > 2 && isprint(static_cast<unsigned char>(text[0])) ? "=" : "", text.size() > 2 && isprint(static_cast<unsigned char>(text[0])) ? text.c_str() : "");
                dump += t;
            }
            printf("weapons:   %s item %u word 2 -> %08X:%s\n", key.c_str(), i, to, dump.c_str());
        }
    }
    printf("weapons: %s's other words: %d swapped\n", key.c_str(), gunSwapped);
    printf("weapons: %s's animations: %d of %u swapped for its own\n", key.c_str(), swapped, count);
    fflush(stdout);
    slot = { entry, copy };
    return copy;
}

extern "C" PPC_FUNC(__imp__sub_820D1320);
PPC_FUNC(sub_820D1320)
{
    const uint32_t table = ctx.r3.u32, name = ctx.r4.u32;
    PPCContext saved = ctx;
    __imp__sub_820D1320(ctx, base);
    if (ctx.r3.u32 || !name) return;
    const std::string text = ReadText(base, name, 40);
    // Two entries a weapon: by its name, and by its name with "_arms" (the
    // drawing set-up, sub_82465920, makes that one with "%s%s"); each taken
    // from the copied gun's, under the same ending.
    for (const CampaignGun& gun : Guns)
        if (text == gun.name || text == std::string(gun.name) + "_arms")
        {
            const bool arms = text != gun.name;
            static uint32_t from[sizeof(Guns) / sizeof(Guns[0])][2] = {};
            uint32_t& alias = from[&gun - Guns][arms];
            if (!alias) alias = GuestText(base, arms ? std::string(gun.from) + "_arms" : std::string(gun.from));
            PPCContext again = saved;
            again.r3.u64 = table;
            again.r4.u64 = alias;
            __imp__sub_820D1320(again, base);
            static int said = 0;
            if ((said < 10 && said++ < 10))
            {
                printf("weapons: %s has no entry of its own in table %08X; %s's (%08X) taken\n", text.c_str(), table, ReadText(base, alias).c_str(), again.r3.u32);
                // What the entry lists (+4 a count, +8 the items, 28 bytes
                // each, the first word of each an item's name or handle).
                const uint32_t entry = again.r3.u32;
                const uint32_t items = entry ? Guest::Read32(base, entry + 4) : 0, at = entry ? Guest::Read32(base, entry + 8) : 0;
                for (uint32_t i = 0; i < items && i < 40 && at; i++)
                {
                    const uint32_t first = Guest::Read32(base, at + 28 * i);
                    const std::string text = (first >= 0x40000000u && first < 0xC0000000u) ? ReadText(base, first, 48) : std::string();
                    printf("weapons:   item %u: %08X %s\n", i, first, text.c_str());
                }
                fflush(stdout);
            }
            ctx.r3.u64 = gun.armsFrom ? CampaignAnimEntry(base, gun, text, again.r3.u32) : again.r3.u32;
            return;
        }
}
#endif

// A weapon's effect events (its shot's sound and flash, the reload's, ...)
// are played by the weapon's name (sub_82556180, sub_82556508 and
// sub_825563D0: the name in r5, the event in r6; CG_FireWeapon, sub_8246A228,
// passes its weapon's +8). A campaign gun has no events of its own, so it
// plays the ones of the gun it is a copy of.
#ifdef COD3_MP
namespace
{
    // 0.5.265: the queued plane sound (MpFixesQueuePlaneSound), played the
    // way the Lee-Enfield's scoped shot is (heard): sub_82556868 from inside
    // the game's own sound event, its context, the stack 0x400 down, the out
    // word on that stack. With no entity asked for (0), the event's own.
    void PlaneSoundFromEvent(uint8_t* base, PPCContext& ctx, uint32_t function)
    {
        const char* alias = g_planeSoundAlias.exchange(nullptr);
        if (!alias) return;
        uint32_t ent = g_planeSoundEnt.load();
        if (ent < 0x40000000u || ent >= 0x90000000u) ent = ctx.r4.u32;
        if (ent < 0x40000000u || ent >= 0x90000000u) { g_planeSoundAlias.store(alias); return; }
        static std::map<std::string, uint32_t> names;
        uint32_t& name = names[alias];
        if (!name) name = GuestText(base, alias);
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x400) & ~0xFu;
        const uint32_t out = call.r1.u32 + 0x200;
        Guest::Write32(base, out, 0);
        call.r3.u64 = out; call.r4.u64 = ent; call.r5.u64 = name;
        call.r6.u64 = 0; call.r7.u64 = uint64_t(int64_t(-1)); call.r8.u64 = 0; call.lr = 0;
        Guest::Lookup(0x82556868)(call, base);
        static int said = 0;
        if ((said < 40 && said++ < 40))
        {
            printf("uav plane: %s played in event %u (sub_%08X) at entity %08X (+484 %08X; the event's %08X +484 %08X): %08X\n", alias, ctx.r6.u32, function, ent,
                   Guest::Read32(base, ent + 484), ctx.r4.u32, ctx.r4.u32 >= 0x40000000u && ctx.r4.u32 < 0x90000000u ? Guest::Read32(base, ctx.r4.u32 + 484) : 0, Guest::Read32(base, out));
            fflush(stdout);
        }
    }

    void CampaignEventName(uint8_t* base, PPCContext& ctx, uint32_t function)
    {
        PlaneSoundFromEvent(base, ctx, function);
        // 0.5.269: whose entity an animation's sound (a reload's) is played
        // at: Justin hears the bots' reloads as if his own (12v12 too).
        if ((function == 0x82555E90u || function == 0x82555F90u) && ctx.r4.u32 >= 0x40000000u && ctx.r4.u32 < 0x90000000u)
        {
            static int said = 0;
            if ((said < 150 && said++ < 150))
            {
                const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
                const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
                const uint32_t e = ctx.r4.u32;
                int index = -1;
                for (uint32_t i = 0; list && i < MpCapClients; i++) if (Guest::Read32(base, list + 4 * (i + 1)) == e) index = int(i);
                const uint32_t eps = Guest::Read32(base, e + 512), ops = own ? Guest::Read32(base, own + 512) : 0;
                float d = -1;
                if (eps >= 0x40000000u && eps < 0x90000000u && ops >= 0x40000000u && ops < 0x90000000u)
                {
                    const float dx = PlaneReadF(base, eps) - PlaneReadF(base, ops), dy = PlaneReadF(base, eps + 4) - PlaneReadF(base, ops + 4);
                    d = sqrtf(dx * dx + dy * dy);
                }
                printf("bot sounds: %s (sub_%08X) at entity %08X (player %d%s, %.0f away) +484 %08X +480 %08X; mine +484 %08X +480 %08X; r3 %08X r5 %08X r6 %08X\n",
                       ReadText(base, ctx.r5.u32, 40).c_str(), function, e, index, e == own ? " = me" : "", d, Guest::Read32(base, e + 484), Guest::Read32(base, e + 480),
                       own ? Guest::Read32(base, own + 484) : 0, own ? Guest::Read32(base, own + 480) : 0, ctx.r3.u32, ctx.r5.u32, ctx.r6.u32);
                fflush(stdout);
            }
        }
        if (!ctx.r5.u32) return;
        const std::string text = ReadText(base, ctx.r5.u32, 40);
        // Every weapon event but a shot's (1, 3, 7, 53: thousands), with the
        // time in milliseconds: when a reload's sounds start against the
        // reload itself (the Sten's still feels out of step, 0.5.31).
        {
            const bool shot = (function == 0x82556180u || function == 0x82556508u) &&
                              (ctx.r6.u32 == 1 || ctx.r6.u32 == 3 || ctx.r6.u32 == 7 || ctx.r6.u32 == 53);
            static int said = 0;
            if (!shot && said++ < 1500)
            {
                printf("weapons: event %u for %s (sub_%08X, from %08X) at %llu ms\n", ctx.r6.u32, text.c_str(), function, uint32_t(ctx.lr),
                       (unsigned long long)GetTickCount64());
                fflush(stdout);
            }
        }
        bool any = false;
        for (int32_t weapon : g_campaignWeapon) any = any || weapon > 0;
        if (!any) return;
        // Who fired what (0.5.49): a shot's events reach the sound code
        // (sub_82554BA0) later, as messages carrying the shooter's sound
        // handle (+484 of his entity, the message's +496). Each shot event
        // keeps the handle's gun: a campaign gun's, or none.
        if ((function == 0x82556180u || function == 0x82556508u) && ctx.r4.u32 >= 0x40000000u && ctx.r4.u32 < 0x90000000u &&
            (ctx.r6.u32 == 1 || ctx.r6.u32 == 3 || ctx.r6.u32 == 7 || ctx.r6.u32 == 53))
        {
            const CampaignGun* fired = nullptr;
            for (const CampaignGun& gun : Guns) if (text == gun.name) fired = &gun;
            RememberShooter(Guest::Read32(base, ctx.r4.u32 + 484), fired);
        }
        // The scoped Lee-Enfield's shot: a sniper rifle's shot is event 13
        // (the scoped Kar98's: SPRINGFIELD_FIRE_1ST and SniperRingoff_1p),
        // which under the copy's name found nothing for the copy (silent,
        // 0.5.900's first test). Its own shot is played at the shooter by
        // name (sub_82556868): Lee_Enfield_Sniper_FIRE_1ST (laison.wbk) for
        // this machine's player, LEE_ENFIELD_FIRE_3RD for the others, and the
        // sniper's ring-off after it.
        if (Settings::Get().campaignSounds && Settings::Get().campaignShotSounds && ctx.r6.u32 == 13 &&
            (function == 0x82556180u || function == 0x82556508u) && text == "lee_enfield_sniper" && ctx.r4.u32 >= 0x40000000u && ctx.r4.u32 < 0x90000000u)
        {
            const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
            const uint32_t self = entities ? Guest::Read32(base, entities + 4) : 0;
            const bool mine = self != 0 && ctx.r4.u32 == self;
            static uint32_t names[2][2] = {};
            const char* const Shot[2][2] = { { "Lee_Enfield_Sniper_FIRE_1ST", "SniperRingoff_1p" }, { "LEE_ENFIELD_FIRE_3RD", "SniperRingoff_3p" } };
            for (int k = 0; k < 1; k++)   // the shot (its ring-off comes with it, sub_82554BA0)
            {
                uint32_t& name = names[mine ? 0 : 1][k];
                if (!name) name = GuestText(base, Shot[mine ? 0 : 1][k]);
                PPCContext call = ctx;
                call.r1.u32 = (ctx.r1.u32 - 0x400) & ~0xFu;
                const uint32_t out = call.r1.u32 + 0x200;
                Guest::Write32(base, out, 0);
                call.r3.u64 = out;
                call.r4.u64 = ctx.r4.u32;
                call.r5.u64 = name;
                call.r6.u64 = 0;
                call.r7.u64 = uint64_t(int64_t(-1));
                call.r8.u64 = 0;
                call.lr = 0;
                Guest::Lookup(0x82556868)(call, base);
            }
            RememberShooter(Guest::Read32(base, ctx.r4.u32 + 484), &Guns[0] + (std::find_if(std::begin(Guns), std::end(Guns), [](const CampaignGun& g) { return strcmp(g.name, "lee_enfield_sniper") == 0; }) - std::begin(Guns)));
            static int said = 0;
            if ((said < 12 && said++ < 12)) { printf("weapons: lee_enfield_sniper's own shot (%s)\n", mine ? "first person" : "another's"); fflush(stdout); }
        }
        // Its own gunshot (0.5.46): the copy's shot sound comes with one of
        // events 3, 7 and 53, which keep the campaign name below (they find
        // no sound by it: silent), and the muzzle flash with event 1, which
        // keeps the copy's. With event 1 the campaign gun's own shot is
        // played by name (sub_82556868, the way the game plays a sound at an
        // entity), from the campaign banks: <SHOT>_FIRE_1ST for this
        // machine's player, _3RD for the others.
        if (false && Settings::Get().campaignSounds && Settings::Get().campaignShotSounds && ctx.r6.u32 == 1 &&
            (function == 0x82556180u || function == 0x82556508u))
            for (const CampaignGun& gun : Guns)
                if (text == gun.name && gun.shot)
                {
                    const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
                    const uint32_t self = entities ? Guest::Read32(base, entities + 4) : 0;
                    const bool mine = self != 0 && ctx.r4.u32 == self;
                    static uint32_t names[sizeof(Guns) / sizeof(Guns[0])][2] = {};
                    uint32_t& name = names[&gun - Guns][mine ? 0 : 1];
                    if (!name) name = GuestText(base, std::string(gun.shot) + (mine ? "_FIRE_1ST" : "_FIRE_3RD"));
                    PPCContext call = ctx;
                    call.r1.u32 = (ctx.r1.u32 - 0x400) & ~0xFu;
                    const uint32_t out = call.r1.u32 + 0x200;
                    Guest::Write32(base, out, 0);
                    call.r3.u64 = out;
                    call.r4.u64 = ctx.r4.u32;
                    call.r5.u64 = name;
                    call.r6.u64 = 0;
                    call.r7.u64 = uint64_t(int64_t(-1));
                    call.r8.u64 = 0;
                    call.lr = 0;
                    Guest::Lookup(0x82556868)(call, base);
                    static int said = 0;
                    if ((said < 12 && said++ < 12)) { printf("weapons: %s's own shot %s\n", gun.name, ReadText(base, name).c_str()); fflush(stdout); }
                    break;
                }
        // With the campaign levels' sound banks loaded (campaign_sounds,
        // sub_824964E8 below), the sounds an animation plays (sub_82555E90
        // and sub_82555F90, from its markers: the reloads) keep the gun's
        // own name, for the campaign bank's own sounds (GR_STEN, ...).
        // A gun with no campaign sounds of its own (the Mills bomb) is the
        // copy's for these too: its explosion is one (silent and unseen
        // under its own name, 0.5.54). So is one with no animations of its
        // own (the MG42: the MG34's reload and bipod sounds).
        if (Settings::Get().campaignSounds && (function == 0x82555E90u || function == 0x82555F90u))
        {
            bool own = true;
            for (const CampaignGun& gun : Guns) if (text == gun.name && (!gun.shot || !gun.armsFrom)) own = false;
            // the scoped Lee-Enfield's reload and bolt: the scoped Kar98's
            // (GR_K98Scope_*, GF_KAR98_Rechamber), swapped for its own by
            // CampaignReloadCue (its own name finds nothing in multiplayer)
            if (text == "lee_enfield_sniper") own = false;
            if (own) return;
        }
        // And the shots (sub_82556180 and sub_82556508: events 1, 3, 7 and
        // 53 a shot), for the campaign banks' gunshots (STEN_FIRE_1ST,
        // fg42_FIRE_1ST, BREN_FIRE_1ST, LEE_ENFIELD_FIRE_1ST):
        // campaign_gunshots, 0 for the copy's.
        if (Settings::Get().campaignSounds && (function == 0x82556180u || function == 0x82556508u))
        {
            // campaign_shot_events: which shot events keep the campaign
            // gun's own name (1: event 1, 2: event 3, 4: event 7, 8: event
            // 53; campaign_gunshots: all four), to find which carries the
            // sound and which the muzzle flash.
            const uint32_t event = ctx.r6.u32;
            const int mask = Settings::Get().campaignGunshots ? 15 : Settings::Get().campaignShotEvents;
            if ((event == 1 && (mask & 1)) || (event == 3 && (mask & 2)) || (event == 7 && (mask & 4)) || (event == 53 && (mask & 8))) return;
        }
        for (const CampaignGun& gun : Guns)
            if (text == gun.name)
            {
                static uint32_t from[sizeof(Guns) / sizeof(Guns[0])] = {};
                uint32_t& alias = from[&gun - Guns];
                if (!alias) alias = GuestText(base, gun.from);
                ctx.r5.u64 = alias;
                return;
            }
    }
}
extern "C" PPC_FUNC(__imp__sub_82556180);
PPC_FUNC(sub_82556180) { CampaignEventName(base, ctx, 0x82556180); __imp__sub_82556180(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82556508);
PPC_FUNC(sub_82556508) { CampaignEventName(base, ctx, 0x82556508); __imp__sub_82556508(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_825563D0);
PPC_FUNC(sub_825563D0) { CampaignEventName(base, ctx, 0x825563D0); __imp__sub_825563D0(ctx, base); }
// The other senders that take a weapon's name (r5), the same way.
extern "C" PPC_FUNC(__imp__sub_82555C00);
PPC_FUNC(sub_82555C00) { CampaignEventName(base, ctx, 0x82555C00); __imp__sub_82555C00(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82555D80);
PPC_FUNC(sub_82555D80) { CampaignEventName(base, ctx, 0x82555D80); __imp__sub_82555D80(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82555E90);
PPC_FUNC(sub_82555E90) { CampaignEventName(base, ctx, 0x82555E90); __imp__sub_82555E90(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82555F90);
PPC_FUNC(sub_82555F90) { CampaignEventName(base, ctx, 0x82555F90); __imp__sub_82555F90(ctx, base); }
extern "C" PPC_FUNC(__imp__sub_82556298);
PPC_FUNC(sub_82556298) { CampaignEventName(base, ctx, 0x82556298); __imp__sub_82556298(ctx, base); }
// DIAGNOSTIC: COD3_RELOAD_TRACE=1: this machine's player's weapon sounds
// by kind (sub_824B13A0: r3 the kind, 14 a round, 16 the start, 17 the
// end; the weapon the one at 0x82C74880 +176), with the handle (+180) and
// flag (+184) that can make it play nothing.
namespace { long long SoundMs(); }
extern "C" PPC_FUNC(__imp__sub_824B13A0);
PPC_FUNC(sub_824B13A0)
{
    if (getenv("COD3_RELOAD_TRACE"))
    {
        const uint32_t def = Guest::Read32(base, 0x82C74880u + 176);
        const uint32_t holder = Guest::Read32(base, 0x82B922C8u);   // lis -32071, 8904
        const uint32_t state = holder ? Guest::Read32(base, holder) : 0;
        printf("sound times: %lld reload trace kind %u weapon %s handle %08X flag %u\n", SoundMs(), ctx.r3.u32,
               def ? ReadText(base, Guest::Read32(base, def + 8), 32).c_str() : "?", state ? Guest::Read32(base, state + 180) : 0, state ? uint32_t(Guest::Read8(base, state + 184)) : 0);
        fflush(stdout);
    }
    __imp__sub_824B13A0(ctx, base);
}

// The campaign levels' sound banks (sp\<level>\<level>.wbk: the campaign
// guns' own reloads and shots, GR_STEN, STEN_FIRE_1ST, ...), loaded beside
// the map's. A map's archive names its bank; sub_824964E8 (the audio bank
// manager, a key: the bank's name made by sub_820B5170, the path, the kind,
// the archive's +136) loads it, once for each key. Each time a map's bank
// is loaded (its path has \mp\ in it), the campaign archives' levels'
// banks are loaded the same way after it.
// Entries correspond to CampaignArchives. Mace 2 covers the existing guns;
// Fuel Plant needs no separate sound bank. The G43's sounds are in Forest.
namespace
{
    const char* const CampaignBanks[] = { "mace2", nullptr, "forest", "laison" };
    // The sound engine was given bank memory of its own (sub_821297E8):
    // the campaign banks are then loaded, and their own cues played.
    bool g_campaignBanksLoaded = false;
    // The engine's slots the campaign banks were queued into (sub_8212A410),
    // searched after the others (OrderedCueLookup).
    bool g_queueingCampaign = false;
    std::atomic<uint64_t> g_campaignSlots{ 0 };
}
static_assert(sizeof(CampaignBanks) / sizeof(CampaignBanks[0]) == ArchiveCount);
extern "C" PPC_FUNC(__imp__sub_824964E8);
PPC_FUNC(sub_824964E8)
{
    PPCContext saved = ctx;
    const std::string path = ctx.r5.u32 ? ReadText(base, ctx.r5.u32, 200) : std::string();
    __imp__sub_824964E8(ctx, base);
    const uint32_t result = ctx.r3.u32;
    {
        static int said = 0;
        if ((said < 40 && said++ < 40))
        {
            printf("sounds: bank %s registered (kind %u, archive %08X) -> %08X\n", path.c_str(), saved.r6.u32, saved.r7.u32, result);
            fflush(stdout);
        }
    }
    std::string lower = path;
    for (char& c : lower) c = char(tolower(static_cast<unsigned char>(c)));
    const size_t at = lower.find("mp\\");
    if (!Settings::Get().campaignGuns || !Settings::Get().campaignSounds || at == std::string::npos || lower.find("global") != std::string::npos ||
        lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".wbk") != 0)
    {
        ctx.r3.u64 = result;
        return;
    }
    for (int which = 0; which < ArchiveCount && which < int(sizeof(CampaignBanks) / sizeof(CampaignBanks[0])); which++)
    {
        if (g_archiveActive[which] < 0 || !CampaignBanks[which]) continue;
        const std::string level = CampaignBanks[which];
        const std::string name = level + ".wbk";
        // (A DLC map's bank is on its own drive, dlcs:\mp\...: the
        // campaign's are always on the game's, D:.)
        std::string prefix = path.substr(0, at);
        if (prefix.find(':') != std::string::npos && lower.compare(0, 2, "d:") != 0) prefix = "D:\\";
        const std::string full = prefix + "sp\\" + level + "\\" + name;
        // The key, as the game makes it: the name in a 128-byte buffer (its
        // length in the last byte), through sub_820B5170.
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x800) & ~0xFu;
        const uint32_t nameBuffer = call.r1.u32 + 0x200, keyBuffer = call.r1.u32 + 0x300, pathBuffer = call.r1.u32 + 0x400;
        for (uint32_t i = 0; i < 0x300; i += 4) Guest::Write32(base, nameBuffer + i, 0);
        for (size_t i = 0; i < name.size() && i < 120; i++) Guest::Write8(base, nameBuffer + uint32_t(i), uint8_t(name[i]));
        Guest::Write8(base, nameBuffer + 127, uint8_t(name.size()));
        for (size_t i = 0; i <= full.size() && i < 250; i++) Guest::Write8(base, pathBuffer + uint32_t(i), i < full.size() ? uint8_t(full[i]) : 0);
        call.r3.u64 = keyBuffer; call.r4.u64 = nameBuffer; call.r5.u64 = 1; call.lr = 0;
        Guest::Lookup(0x820B5170)(call, base);
        call.r3.u64 = saved.r3.u64; call.r4.u64 = keyBuffer; call.r5.u64 = pathBuffer; call.r6.u64 = saved.r6.u64; call.r7.u64 = saved.r7.u64; call.lr = 0;
        __imp__sub_824964E8(call, base);
        printf("sounds: campaign bank %s loaded beside %s -> %08X\n", full.c_str(), path.c_str(), call.r3.u32);
        // Registering a bank only gives it a record in the bank manager; the
        // sound engine loads a bank when the game asks for its key
        // (sub_824AE270(manager [0x82BCBF7C], key, 1), as it does for the
        // map's bank). Without this the campaign banks were never loaded
        // (0.5.900: only global.wbk and the map's were ever queued) and no
        // campaign cue was found by name.
        if (g_campaignBanksLoaded)
        {
            const uint32_t manager = Guest::Read32(base, 0x82BCBF7Cu);
            PPCContext load = call;
            load.r3.u64 = manager; load.r4.u64 = keyBuffer; load.r5.u64 = 1; load.lr = 0;
            g_queueingCampaign = true;
            if (manager) Guest::Lookup(0x824AE270)(load, base);
            g_queueingCampaign = false;
            printf("sounds: campaign bank %s asked to load (manager %08X, register's r3 %08X)\n", full.c_str(), manager, saved.r3.u32);
        }
        fflush(stdout);
    }
    ctx.r3.u64 = result;
}

// A campaign gun's names (its world model, HUD icon), for the look-ups below.
namespace
{
    bool CampaignName(const std::string& name)
    {
        std::string lower = name;
        for (char& c : lower) c = char(tolower(static_cast<unsigned char>(c)));
        for (const CampaignGun& gun : Guns)
        {
            std::string world = gun.worldModel, hud = gun.hud;
            for (char& c : world) c = char(tolower(static_cast<unsigned char>(c)));
            for (char& c : hud) c = char(tolower(static_cast<unsigned char>(c)));
            if (lower == world || lower == hud) return true;
        }
        // models a campaign gun's file names besides its own: the scoped
        // Lee-Enfield's scope (attach1Model), only in Laison's archive
        return lower == "lee_enfield_scope";
    }
}

// Materials asked for by name (sub_824408E0: the name in r3; looked for in
// one archive, the bank's or the global one, then the level's, else the
// default material at *0x82A8C398, drawn as a white box). A campaign gun's
// HUD icon (the pick-up prompt's) is only in its campaign archive: looked for
// there too (sub_82477E70: an archive's number, the name's key made by
// sub_820B00C0).
// A material's first words, and the first words of what each points at
// (its textures, their fetch constants: formats and addresses), logged to
// compare the campaign icons with the multiplayer ones.
static void DumpMaterial(uint8_t* base, const std::string& name, uint32_t material)
{
    if (material < 0x40000000u || material >= 0x90000000u) return;
    auto words = [&](uint32_t at, int count)
    {
        std::string line;
        char text[16];
        for (int i = 0; i < count; i++) { snprintf(text, sizeof(text), " %08X", Guest::Read32(base, at + 4u * uint32_t(i))); line += text; }
        return line;
    };
    printf("materials:   %s at %08X:%s\n", name.c_str(), material, words(material, 24).c_str());
    for (int i = 0; i < 24; i++)
    {
        const uint32_t to = Guest::Read32(base, material + 4u * uint32_t(i));
        if (to < 0x40000000u || to >= 0x90000000u || to == material) continue;
        printf("materials:     +%d -> %08X:%s \"%s\"\n", i * 4, to, words(to, 16).c_str(), ReadText(base, to, 24).c_str());
    }
}
extern "C" PPC_FUNC(__imp__sub_824408E0);
PPC_FUNC(sub_824408E0)
{
    const std::string name = ctx.r3.u32 ? ReadText(base, ctx.r3.u32, 64) : std::string();
    PPCContext saved = ctx;
    __imp__sub_824408E0(ctx, base);
    if (name == "hud_thompson" || name == "hud_bar_fullauto" || name == "hud_kar98")
    {
        static std::set<std::string> dumped;
        if (dumped.insert(name).second) { printf("materials: %s (a copy's, to compare) -> %08X\n", name.c_str(), ctx.r3.u32); DumpMaterial(base, name, ctx.r3.u32); fflush(stdout); }
    }
    if (name.empty() || !CampaignName(name)) return;
    const uint32_t fallback = Guest::Read32(base, 0x82A8C398);
    const uint32_t first = ctx.r3.u32;
    if (ctx.r3.u32 == fallback || ctx.r3.u32 == 0)
        for (const int32_t archive : g_archiveActive)
        {
            if (archive < 0) continue;
            PPCContext call = saved;
            call.r1.u32 = (saved.r1.u32 - 0x800) & ~0xFu;
            const uint32_t nameBuffer = call.r1.u32 + 0x200, key = call.r1.u32 + 0x300;
            for (uint32_t i = 0; i < 0x200; i += 4) Guest::Write32(base, nameBuffer + i, 0);
            for (size_t i = 0; i < name.size() && i < 120; i++) Guest::Write8(base, nameBuffer + uint32_t(i), uint8_t(name[i]));
            call.r3.u64 = key; call.r4.u64 = nameBuffer; call.lr = 0;
            Guest::Lookup(0x820B00C0)(call, base);
            call.r3.u64 = uint32_t(archive); call.r4.u64 = key; call.lr = 0;
            Guest::Lookup(0x82477E70)(call, base);
            if (call.r3.u32 != 0) { ctx.r3.u64 = call.r3.u32; break; }
        }
    static int said = 0;
    if ((said < 60 && said++ < 60))
    {
        printf("materials: %s -> %08X (first %08X, the default %08X)%s\n", name.c_str(), ctx.r3.u32, first, fallback,
               ctx.r3.u32 == fallback ? " NOT FOUND" : "");
        DumpMaterial(base, name, ctx.r3.u32);
        fflush(stdout);
    }
}

// Models asked for by name (sub_82446EA0: the result's holder r3, the name
// r6, looked for in the archives of the bank number r5's list). A campaign
// gun's world model not found that way: looked for with each campaign
// archive's own list too. Logged either way.
extern "C" PPC_FUNC(__imp__sub_82446EA0);
PPC_FUNC(sub_82446EA0)
{
    const std::string name = ctx.r6.u32 ? ReadText(base, ctx.r6.u32, 64) : std::string();
    PPCContext saved = ctx;
    const uint32_t holder = ctx.r3.u32;
    __imp__sub_82446EA0(ctx, base);
    // First-person meshes can also be absent from the MP map's own banks.
    // The G43 is wholly in Forest; resolving only its world model leaves
    // the player holding empty hands despite owning a valid weapon.
    bool campaignView = false;
    for (const CampaignGun& gun : Guns)
        if (gun.viewModel && _stricmp(name.c_str(), gun.viewModel) == 0) campaignView = true;
    if (name.empty() || (!CampaignName(name) && !campaignView) || !holder) return;
    const uint32_t first = Guest::Read32(base, holder);
    if (first == 0)
        for (const int32_t archive : g_archiveActive)
        {
            if (archive < 0) continue;
            PPCContext call = saved;
            call.r5.u64 = uint32_t(archive);
            call.lr = 0;
            __imp__sub_82446EA0(call, base);
            if (Guest::Read32(base, holder) != 0) break;
        }
    static int said = 0;
    if ((said < 60 && said++ < 60))
    {
        printf("models: %s (bank %d) -> %08X (first %08X)\n", name.c_str(), saved.r5.s32, Guest::Read32(base, holder), first);
        fflush(stdout);
    }
    ctx.r3.u64 = holder;
}

namespace
{
    // COD3_SOUND_TIMES=1: every sound asked for by name and every cue looked
    // up, with the milliseconds since start (timing reload sounds).
    bool SoundTimes() { static const bool on = getenv("COD3_SOUND_TIMES") != nullptr; return on; }
    // COD3_RELOAD_SHOTS=1: screenshots every quarter second for 4.5 s from
    // when a reload sound (GR_*) is looked up (lining sound up with motion).
    std::atomic<long long> g_reloadShotsFrom{ -1 };
    long long SoundMs() { static const auto start = std::chrono::steady_clock::now(); return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(); }
}
// A model's parts and their meshes (sub_82446AB8: r25 a list, +16 count,
// +20 items; each model +52 its name, +16/+20 its parts, 12 bytes each, the
// part's name first; +32/+36 the meshes' names, "<part>.<model>", found as
// assets of kind 3 into +40/+44). The MG42 has no first-person model: in its
// view, the MG34's ("w_geLMG_VM") gun body, mesh "mg34.w_geLMG_VM", is drawn
// with the AI soldiers' MG42 body, "mg42.w_ge_MG42_nobipod", in its place,
// while the MG34's bipod, drum and handle and every animation stay. The slot
// is swapped only while the MG42 is in hand (MpFixesMg42View, each frame).
namespace
{
    uint32_t g_mg34BodySlot = 0, g_mg34Body = 0, g_mg42Body = 0;
    std::vector<uint32_t> g_mg34Bodies;   // every MG34 view body seen (one per bank that loads it)
    // The MG34 view parts not drawn while the MG42 is in hand (the bipod,
    // which the handheld MG42 has none of): their triangle and index counts
    // (+28, +0x40+104) set to none, and put back.
    std::vector<uint32_t> g_mg34Hidden;
    // The Lee-Enfield's first-person body (both Lee-Enfields use it), for
    // its stock's bend (MpFixesEnfieldStock).
    std::vector<uint32_t> g_enfieldBodies;
    bool HiddenPart(const std::string& mesh)
    {
        static const bool drum = getenv("COD3_MG42_HIDE_DRUM") != nullptr;
        for (const char* part : { "mg34_bipod_base.w_geLMG_VM", "mg34_bipod_L.w_geLMG_VM", "mg34_bipod_R.w_geLMG_VM" })
            if (_stricmp(mesh.c_str(), part) == 0) return true;
        return drum && _stricmp(mesh.c_str(), "mg34_drum.w_geLMG_VM") == 0;
    }
    std::mutex g_bodyLock;
}
static void WorldFitSeeMesh(uint8_t* base, const std::string& mesh, uint32_t at);   // below MakeMg42Body
extern "C" PPC_FUNC(__imp__sub_82446AB8);
PPC_FUNC(sub_82446AB8)
{
    const uint32_t list = ctx.r3.u32 ? ctx.r3.u32 : 0;
    const uint32_t models = ctx.r4.u32;
    __imp__sub_82446AB8(ctx, base);
    (void)list;
    if (!models) return;
    const uint32_t count = std::min<uint32_t>(Guest::Read32(base, models + 16), 256);
    const uint32_t items = Guest::Read32(base, models + 20);
    static bool tell = getenv("COD3_MG42_PARTS") != nullptr;
    for (uint32_t m = 0; items && m < count; m++)
    {
        const uint32_t model = Guest::Read32(base, items + 4 * m);
        if (!model) continue;
        const uint32_t meshes = std::min<uint32_t>(Guest::Read32(base, model + 32), 64);
        const uint32_t names = Guest::Read32(base, model + 36);
        const uint32_t found = Guest::Read32(base, model + 44);
        const uint32_t found_count = Guest::Read32(base, model + 40);
        for (uint32_t i = 0; names && found && i < meshes && i < found_count; i++)
        {
            const std::string mesh = ReadText(base, Guest::Read32(base, names + 4 * i), 64);
            const uint32_t slot = found + 4 * i;
            WorldFitSeeMesh(base, mesh, Guest::Read32(base, slot));
            if (HiddenPart(mesh))
            {
                std::lock_guard<std::mutex> guard(g_bodyLock);
                const uint32_t part = Guest::Read32(base, slot);
                if (part && std::find(g_mg34Hidden.begin(), g_mg34Hidden.end(), part) == g_mg34Hidden.end()) g_mg34Hidden.push_back(part);
            }
            if (_stricmp(mesh.c_str(), "w_lee_enfield.lee_enfield") == 0)
            {
                std::lock_guard<std::mutex> guard(g_bodyLock);
                const uint32_t body = Guest::Read32(base, slot);
                if (body && std::find(g_enfieldBodies.begin(), g_enfieldBodies.end(), body) == g_enfieldBodies.end()) g_enfieldBodies.push_back(body);
                printf("enfield view: body %s at %08X\n", mesh.c_str(), body); fflush(stdout);
            }
            if (_stricmp(mesh.c_str(), "mg34.w_geLMG_VM") == 0 || _stricmp(mesh.c_str(), "mg42.w_ge_MG42_nobipod") == 0)
            {
                std::lock_guard<std::mutex> guard(g_bodyLock);
                if (mesh[3] == '4')
                {
                    g_mg34BodySlot = slot; g_mg34Body = Guest::Read32(base, slot);
                    if (std::find(g_mg34Bodies.begin(), g_mg34Bodies.end(), g_mg34Body) == g_mg34Bodies.end()) g_mg34Bodies.push_back(g_mg34Body);
                }
                else g_mg42Body = Guest::Read32(base, slot);
                printf("mg42 view: %s at %08X -> %08X (model %08X \"%s\")\n", mesh.c_str(), slot, Guest::Read32(base, slot), model,
                       ReadText(base, Guest::Read32(base, model + 52), 64).c_str());
                fflush(stdout);
                if (tell)
                {   // the mesh, and two levels of what it points at
                    auto line = [&](const char* what, uint32_t at, int count)
                    {
                        std::string words; char text[16];
                        for (int w = 0; w < count; w++) { snprintf(text, sizeof(text), " %08X", Guest::Read32(base, at + 4u * uint32_t(w))); words += text; }
                        printf("mg42 mesh: %s %08X:%s\n", what, at, words.c_str());
                    };
                    const uint32_t at = Guest::Read32(base, slot);
                    line(mesh.c_str(), at, 64);
                    std::set<uint32_t> seen;
                    for (int w = 0; w < 64; w++)
                    {
                        const uint32_t to = Guest::Read32(base, at + 4u * uint32_t(w));
                        if (to < 0x40000000u || to >= 0x90000000u || !seen.insert(to).second) continue;
                        char what[48]; snprintf(what, sizeof(what), "  +%d ->", w * 4);
                        line(what, to, 32);
                        for (int v = 0; v < 32; v++)
                        {
                            const uint32_t two = Guest::Read32(base, to + 4u * uint32_t(v));
                            if (two < 0x40000000u || two >= 0x90000000u || !seen.insert(two).second) continue;
                            char deeper[48]; snprintf(deeper, sizeof(deeper), "    +%d+%d ->", w * 4, v * 4);
                            line(deeper, two, 24);
                        }
                    }
                    fflush(stdout);
                }
            }
            if (tell && (mesh.find("w_geLMG_VM") != std::string::npos || mesh.find("MG42") != std::string::npos || mesh.find("mg42") != std::string::npos))
            {
                const uint32_t at = Guest::Read32(base, slot);
                std::string words;
                char text[16];
                for (int w = 0; at >= 0x40000000u && at < 0x90000000u && w < 24; w++) { snprintf(text, sizeof(text), " %08X", Guest::Read32(base, at + 4u * uint32_t(w))); words += text; }
                printf("mg42 parts: %s -> %08X:%s\n", mesh.c_str(), at, words.c_str());
                fflush(stdout);
            }
        }
        if (tell)
        {
            const std::string name = ReadText(base, Guest::Read32(base, model + 52), 64);
            const uint32_t parts = std::min<uint32_t>(Guest::Read32(base, model + 16), 64), table = Guest::Read32(base, model + 20);
            if (name.find("mg") != std::string::npos || name.find("MG") != std::string::npos)
                for (uint32_t i = 0; table && i < parts; i++)
                    printf("mg42 parts: model %s part %u %s %08X %08X\n", name.c_str(), i, ReadText(base, Guest::Read32(base, table + 12 * i), 64).c_str(),
                           Guest::Read32(base, table + 12 * i + 4), Guest::Read32(base, table + 12 * i + 8));
            fflush(stdout);
        }
    }
}
// The MG42's body made to sit where the MG34's does: a copy of the mesh
// (0x180 bytes: the part +0x00, its piece +0x40, the vertex layout +0xC0,
// the vertex and index buffers' descriptions +0x138/+0x158, pointers within
// it moved with it) and of its vertices (52 bytes each: position, three
// floats, then the normal +12, tangent +44 and binormal +48 as packed
// 10:10:10 words), turned a quarter about the up axis (the AI model lies
// along x, barrel to -x; the MG34's along y, barrel to +y: seen in 0.5.900's
// screenshots, the other way round put the barrel at the eye), scaled and moved
// onto the MG34 body's box. The vertex description's +24 is the buffer
// (| 3), +28 its size in words << 2; the bounds are the MG34's.
namespace
{
    // The fit: the scale, and where the turned body's box goes: its rear
    // end on the MG34 body's rear end (rear) or its barrel end on the MG34's
    // barrel end, centred across, its top on the MG34's top; then moved by
    // x, y, z (y forward, z up). COD3_MG42_BODY="scale x y z rear(0/1)".
    // pitch: degrees the body is then tipped, muzzle up, about the MG34's
    // rear sight (y -6.57, z 3.75 of its body), so the MG42's sight line
    // (its rear sight's notch and its front sight's tip) is the eye's.
    // yaw: degrees turned about the same point (x 0.31), muzzle to +x.
    struct BodyFit { float scale = 0.40f, x = 0.4f, y = -3.0f, z = 0.0f; int rear = 0; float pitch = 0.0f, yaw = 0.0f; };
    BodyFit ParseFit(const char* text) { BodyFit fit; if (text) sscanf(text, "%f %f %f %f %d %f %f", &fit.scale, &fit.x, &fit.y, &fit.z, &fit.rear, &fit.pitch, &fit.yaw); return fit; }
    BodyFit g_fit = ParseFit(getenv("COD3_MG42_BODY"));
    // aiming: COD3_MG42_ADS_BODY="scale x y z rear"
    // (0.5.900, session 4: the MG42's rear sight's notch put on the MG34's,
    // y -6.57 z 3.75 of its body, found from the two meshes; tipped 0.5
    // degrees and turned -0.7 so its front sight's tip sits in the notch,
    // on the screen's centre: screenshots, COD3_MG42_ADS_CYCLE)
    BodyFit g_adsFit = []() { BodyFit f; f.x = -0.22f; f.y = -4.72f; f.z = -0.97f; f.pitch = 0.5f; f.yaw = -0.7f; if (const char* t = getenv("COD3_MG42_ADS_BODY")) f = ParseFit(t); return f; }();
    bool g_cycleMg34 = false;
    float ReadFloat(uint8_t* base, uint32_t at) { const uint32_t v = Guest::Read32(base, at); float f; memcpy(&f, &v, 4); return f; }
    void WriteFloat(uint8_t* base, uint32_t at, float f) { uint32_t v; memcpy(&v, &f, 4); Guest::Write32(base, at, v); }
    int Field10(uint32_t word, int shift) { int v = int((word >> shift) & 0x3FF); return v >= 512 ? v - 1024 : v; }
    // Turns a packed direction a quarter about the up axis; reversed: z in
    // the low bits, x in the high.
    uint32_t TurnPacked(uint32_t word, bool reversed)
    {
        const int xs = reversed ? 20 : 0, zs = reversed ? 0 : 20;
        const int x = Field10(word, xs), y = Field10(word, 10), z = Field10(word, zs);
        const int nx = y, ny = -x;
        return (word & 0xC0000000u) | (uint32_t(nx & 0x3FF) << xs) | (uint32_t(ny & 0x3FF) << 10) | (uint32_t(z & 0x3FF) << zs);
    }
    // 0.5.206: the MG42 body's vertices as loaded, when the world-model fit
    // has moved them in place (the third-person MG42 is the same mesh).
    const std::vector<uint32_t>* WorldFitOriginal(uint32_t buffer, uint32_t vertices);
    const uint32_t* WorldFitOriginalBox(uint32_t mesh);   // its box as loaded (min xyz, max xyz), when moved
    uint32_t MakeMg42Body(uint8_t* base, uint32_t mg42, uint32_t mg34, const BodyFit& fit, int slot, bool quiet = false)
    {
        const uint32_t size = 0x180, piece = mg42 + 0x40;
        const uint32_t vdesc = Guest::Read32(base, piece + 80), idesc = Guest::Read32(base, piece + 96), decl = Guest::Read32(base, piece + 112);
        if (vdesc != mg42 + 0x138 || idesc != mg42 + 0x158 || decl != mg42 + 0xC0 || Guest::Read32(base, decl) != 0x34)
        { printf("mg42 view: the MG42 body's layout is not the one expected (%08X %08X %08X); not made\n", vdesc, idesc, decl); return 0; }
        const uint32_t vertices = Guest::Read32(base, piece + 88);
        const uint32_t from = Guest::Read32(base, vdesc + 24) & ~3u;
        const uint32_t words = (Guest::Read32(base, vdesc + 28) >> 2) & 0xFFFFFFu;
        if (words * 4 != vertices * 52 || !from) { printf("mg42 view: vertices %u, buffer %08X of %u words: not made\n", vertices, from, words); return 0; }
        const uint32_t indices = Guest::Read32(base, idesc + 24), indexBytes = Guest::Read32(base, idesc + 28);
        // The vertices read from the copy kept as loaded, if the world fit
        // has moved the buffer's (else from the buffer itself).
        const std::vector<uint32_t>* kept = WorldFitOriginal(from, vertices);
        if (kept && kept->size() != size_t(vertices) * 13) kept = nullptr;
        auto Word = [&](uint32_t at) -> uint32_t { return kept && at >= from && at < from + vertices * 52 ? (*kept)[(at - from) / 4] : Guest::Read32(base, at); };
        auto Float = [&](uint32_t at) -> float { const uint32_t v = Word(at); float f; memcpy(&f, &v, 4); return f; };
        // Which way round the packed normals are: the one that agrees with
        // the triangles' own facing.
        double agree[2] = {};
        for (uint32_t t = 0; t + 6 <= indexBytes && t < 6 * 400; t += 6)
        {
            uint32_t v[3]; float p[3][3];
            for (int k = 0; k < 3; k++) { v[k] = Guest::Read16(base, indices + t + 2 * k); if (v[k] >= vertices) goto next; for (int c = 0; c < 3; c++) p[k][c] = Float(from + 52 * v[k] + 4 * c); }
            {
                const float a[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, b2[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
                float n[3] = { a[1] * b2[2] - a[2] * b2[1], a[2] * b2[0] - a[0] * b2[2], a[0] * b2[1] - a[1] * b2[0] };
                const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (len < 1e-6f) goto next;
                const uint32_t w = Word(from + 52 * v[0] + 12);
                for (int r = 0; r < 2; r++)
                {
                    const int xs = r ? 20 : 0, zs = r ? 0 : 20;
                    const float m[3] = { float(Field10(w, xs)), float(Field10(w, 10)), float(Field10(w, zs)) };
                    const float ml = sqrtf(m[0] * m[0] + m[1] * m[1] + m[2] * m[2]);
                    if (ml > 0) agree[r] += fabs((n[0] * m[0] + n[1] * m[1] + n[2] * m[2]) / (len * ml));
                }
            }
            next:;
        }
        const bool reversed = agree[1] > agree[0];
        // made once, filled again on each map (the window is not given back)
        // slot 0 the hip's, 1 the sights' (made again as they come up)
        static uint32_t buffers[2] = {}, bufferSizes[2] = {}, meshes[2] = {};
        const int at = slot & 1;
        uint32_t& buffer = buffers[at]; uint32_t& bufferSize = bufferSizes[at]; uint32_t& mesh = meshes[at];
        if (vertices * 52 > bufferSize) { bufferSize = (vertices * 52 + 0xFFFFu) & ~0xFFFFu; buffer = Guest::AllocateExtended(bufferSize); }
        if (!mesh) mesh = Guest::AllocateExtended(0x1000);
        if (!buffer || !mesh) { printf("mg42 view: no memory for the MG42 body\n"); return 0; }

        auto box = [&](uint32_t mesh, int axis, bool most)
        {
            if (const uint32_t* loaded = WorldFitOriginalBox(mesh)) { const uint32_t v = loaded[(most ? 3 : 0) + axis]; float f; memcpy(&f, &v, 4); return f; }
            return ReadFloat(base, mesh + 0x40 + (most ? 48 : 32) + 4 * axis);
        };
        const float sc = fit.scale;
        const float offX = (box(mg34, 0, false) + box(mg34, 0, true)) / 2 - (box(mg42, 1, false) + box(mg42, 1, true)) * sc / 2 + fit.x;
        const float offY = (fit.rear ? box(mg34, 1, false) + box(mg42, 0, true) * sc : box(mg34, 1, true) + box(mg42, 0, false) * sc) + fit.y;
        const float offZ = box(mg34, 2, true) - box(mg42, 2, true) * sc + fit.z;
        for (uint32_t i = 0; i < vertices; i++)
        {
            const uint32_t src = from + 52 * i, dst = buffer + 52 * i;
            for (uint32_t w = 0; w < 52; w += 4) Guest::Write32(base, dst + w, Word(src + w));
            const float x = Float(src), y = Float(src + 4), z = Float(src + 8);
            float nx = y * sc + offX, ny = -x * sc + offY, nz = z * sc + offZ;
            if (fit.pitch != 0.0f)
            {
                const float a = fit.pitch * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
                const float ahead = -6.57f - ny, up = nz - 3.75f;   // from the pivot: forward (-y) and up
                ny = -6.57f - (ahead * c - up * s);
                nz = 3.75f + (up * c + ahead * s);
            }
            if (fit.yaw != 0.0f)
            {
                const float a = fit.yaw * 3.14159265f / 180.0f, c = cosf(a), s = sinf(a);
                const float ahead = -6.57f - ny, side = nx - 0.31f;
                nx = 0.31f + (side * c + ahead * s);
                ny = -6.57f - (ahead * c - side * s);
            }
            WriteFloat(base, dst, nx);
            WriteFloat(base, dst + 4, ny);
            WriteFloat(base, dst + 8, nz);
            for (uint32_t at : { 12u, 44u, 48u }) Guest::Write32(base, dst + at, TurnPacked(Word(src + at), reversed));
        }
        for (uint32_t w = 0; w < size; w += 4)
        {
            const uint32_t v = Guest::Read32(base, mg42 + w);
            Guest::Write32(base, mesh + w, (v >= mg42 && v < mg42 + size) ? v - mg42 + mesh : v);
        }
        Guest::Write32(base, mesh + 0x138 + 24, buffer | (Guest::Read32(base, vdesc + 24) & 3u));
        // the MG34 body's bounds: its sphere (+32, +0x40+16) and box (+0x40+32)
        for (uint32_t w = 32; w < 48; w += 4) Guest::Write32(base, mesh + w, Guest::Read32(base, mg34 + w));
        for (uint32_t w = 16; w < 64; w += 4) Guest::Write32(base, mesh + 0x40 + w, Guest::Read32(base, mg34 + 0x40 + w));
        if (!quiet) printf("mg42 view: MG42 body made at %08X (vertices %u at %08X, normals %s, agreement %.0f/%.0f; scale %.2f, %s end, by %.2f %.2f %.2f: offset %.2f %.2f %.2f, pitch %.2f, yaw %.2f)\n",
               mesh, vertices, buffer, reversed ? "z-low" : "x-low", agree[0], agree[1], sc, fit.rear ? "rear" : "barrel", fit.x, fit.y, fit.z, offX, offY, offZ, fit.pitch, fit.yaw);
        fflush(stdout);
        return mesh;
    }
}
// 0.5.206: the campaign guns' third-person models fitted to the hand
// (world_model_fit.h says why and how). Each mesh the game loads is seen by
// sub_82446AB8 above ("<part>.<model>"); the meshes of each campaign gun's
// world model and of its copy's are kept by model name. Once a frame, in a
// match, each campaign model's vertices are written again from a copy of
// them as loaded, through the gun's fit, whenever the fit or loaded data
// changed. Its original vertex bounds are cached when that copy is captured;
// rebuilding them every frame needlessly walked every vertex. A new map may
// load meshes again at the same address: the first vertex detects that.
// world_models.txt beside the executable, read again when it is saved.
namespace
{
    std::mutex g_worldLock;
    std::map<std::string, std::vector<uint32_t>> g_modelMeshes;   // lower-case model name: its meshes
    std::map<uint32_t, std::string> g_meshPart;                    // each mesh's part (lower case, before the dot)
    struct WorldMesh
    {
        uint32_t buffer = 0, vertices = 0, stride = 0;
        std::vector<uint32_t> original;   // the buffer's words as loaded
        WorldFit::Box originalBox;        // cached bounds; rebuilding these every frame walked every vertex
        uint32_t box[6] = {};             // the mesh's box as loaded: min xyz (+0x40+32), max xyz (+0x40+48; +44 is padding)
        uint32_t sphere[2][4] = {};       // +32 and +0x40+16, as loaded (moved only when they look like a centre and radius)
        bool reversed = false;            // the packed normals' layout (z in the low bits)
        std::string applied;              // the transform last written
        uint32_t wrote[3] = {};           // the first vertex's position as written
    };
    std::map<uint32_t, WorldMesh> g_worldMeshes;   // by mesh
    std::multimap<uint32_t, uint32_t> g_worldBufferMesh; // the buffer's meshes, for WorldFitOriginal (a model's parts may share one)
    std::map<uint32_t, std::array<uint32_t, 4>> g_worldHidden;   // hidden parts: their counts as loaded

    std::string ModelOf(const std::string& mesh)
    {
        const size_t dot = mesh.find('.');
        return WorldFit::Lower(dot == std::string::npos ? mesh : mesh.substr(dot + 1));
    }
    // The mesh's vertex buffer (the layout the MG42 body has: the piece at
    // +0x40, its vertex description +80 and declaration +112 inside the mesh).
    bool MeshBuffer(uint8_t* base, uint32_t mesh, uint32_t& buffer, uint32_t& vertices, uint32_t& stride)
    {
        if (mesh < 0x40000000u || mesh >= 0xA0000000u) return false;
        const uint32_t piece = mesh + 0x40;
        const uint32_t vdesc = Guest::Read32(base, piece + 80), decl = Guest::Read32(base, piece + 112);
        if (vdesc < mesh || vdesc > mesh + 0x400 || decl < mesh || decl > mesh + 0x400) return false;
        vertices = Guest::Read32(base, piece + 88);
        stride = Guest::Read32(base, decl);
        buffer = Guest::Read32(base, vdesc + 24) & ~3u;
        const uint32_t words = (Guest::Read32(base, vdesc + 28) >> 2) & 0xFFFFFFu;
        if (!buffer || stride < 12 || stride > 128 || (stride & 3) || vertices == 0 || vertices > 65535) return false;
        if (words * 4 != vertices * stride) return false;
        return true;
    }
    float AsFloat(uint32_t v) { float f; memcpy(&f, &v, 4); return f; }
    uint32_t AsWord(float f) { uint32_t v; memcpy(&v, &f, 4); return v; }
    // The box of a mesh's vertices (its own copy if kept, else as they are).
    WorldFit::Box VertexBox(uint8_t* base, uint32_t mesh)
    {
        WorldFit::Box box;
        auto kept = g_worldMeshes.find(mesh);
        uint32_t buffer, vertices, stride;
        if (kept != g_worldMeshes.end() && !kept->second.original.empty())
        {
            return kept->second.originalBox;
        }
        if (!MeshBuffer(base, mesh, buffer, vertices, stride)) return box;
        for (uint32_t i = 0; i < vertices; i++)
        {
            const float p[3] = { ReadFloat(base, buffer + stride * i), ReadFloat(base, buffer + stride * i + 4), ReadFloat(base, buffer + stride * i + 8) };
            if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2])) continue;
            box.Add(p);
        }
        return box;
    }
    bool Filled(uint8_t* base, uint32_t buffer, uint32_t vertices, uint32_t stride)
    {
        for (uint32_t i = 0; i < vertices && i < 64; i++)
            if (Guest::Read32(base, buffer + stride * i) != 0 || Guest::Read32(base, buffer + stride * i + 8) != 0) return true;
        return false;
    }
    // Keeps a mesh's data as loaded (again, when it was loaded again).
    WorldMesh* Keep(uint8_t* base, uint32_t mesh)
    {
        uint32_t buffer, vertices, stride;
        if (!MeshBuffer(base, mesh, buffer, vertices, stride) || !Filled(base, buffer, vertices, stride)) return nullptr;
        WorldMesh& m = g_worldMeshes[mesh];
        const bool same = m.buffer == buffer && m.vertices == vertices && m.stride == stride && !m.original.empty() &&
                          (m.applied.empty() ? Guest::Read32(base, buffer) == m.original[0] && Guest::Read32(base, buffer + 4) == m.original[1] && Guest::Read32(base, buffer + 8) == m.original[2]
                                             : Guest::Read32(base, buffer) == m.wrote[0] && Guest::Read32(base, buffer + 4) == m.wrote[1] && Guest::Read32(base, buffer + 8) == m.wrote[2]);
        if (same) return &m;
        for (auto it = g_worldBufferMesh.begin(); it != g_worldBufferMesh.end();) it = it->second == mesh ? g_worldBufferMesh.erase(it) : std::next(it);
        m = WorldMesh{};
        m.buffer = buffer; m.vertices = vertices; m.stride = stride;
        m.original.resize(size_t(vertices) * stride / 4);
        const uint32_t step = stride / 4;
        for (uint32_t i = 0; i < vertices; i++)
        {
            const size_t at = size_t(i) * step;
            for (uint32_t w = 0; w < step; w++) m.original[at + w] = Guest::Read32(base, buffer + uint32_t(4 * (at + w)));
            const float p[3] = { AsFloat(m.original[at]), AsFloat(m.original[at + 1]), AsFloat(m.original[at + 2]) };
            if (std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2])) m.originalBox.Add(p);
        }
        for (int c = 0; c < 3; c++) { m.box[c] = Guest::Read32(base, mesh + 0x40 + 32 + 4 * c); m.box[3 + c] = Guest::Read32(base, mesh + 0x40 + 48 + 4 * c); }
        for (int c = 0; c < 4; c++) { m.sphere[0][c] = Guest::Read32(base, mesh + 32 + 4 * c); m.sphere[1][c] = Guest::Read32(base, mesh + 0x40 + 16 + 4 * c); }
        g_worldBufferMesh.emplace(buffer, mesh);
        // Which way round the packed normals are (52-byte vertices: the
        // normal at +12), by agreement with the triangles' own facing.
        if (stride == 52)
        {
            const uint32_t idesc = Guest::Read32(base, mesh + 0x40 + 96);
            const uint32_t indices = idesc ? Guest::Read32(base, idesc + 24) : 0, indexBytes = idesc ? Guest::Read32(base, idesc + 28) : 0;
            double agree[2] = {};
            for (uint32_t t = 0; indices && t + 6 <= indexBytes && t < 6 * 400; t += 6)
            {
                uint32_t v[3]; float p[3][3]; bool ok = true;
                for (int k = 0; k < 3 && ok; k++) { v[k] = Guest::Read16(base, indices + t + 2 * k); if (v[k] >= vertices) ok = false; else for (int c = 0; c < 3; c++) p[k][c] = AsFloat(m.original[v[k] * 13 + uint32_t(c)]); }
                if (!ok) continue;
                const float a[3] = { p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2] }, b2[3] = { p[2][0] - p[0][0], p[2][1] - p[0][1], p[2][2] - p[0][2] };
                const float n[3] = { a[1] * b2[2] - a[2] * b2[1], a[2] * b2[0] - a[0] * b2[2], a[0] * b2[1] - a[1] * b2[0] };
                const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
                if (len < 1e-6f) continue;
                const uint32_t w = m.original[v[0] * 13 + 3];
                for (int r = 0; r < 2; r++)
                {
                    const int xs = r ? 20 : 0, zs = r ? 0 : 20;
                    const float q[3] = { float(WorldFit::Field10(w, xs)), float(WorldFit::Field10(w, 10)), float(WorldFit::Field10(w, zs)) };
                    const float ql = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2]);
                    if (ql > 0) agree[r] += fabs((n[0] * q[0] + n[1] * q[1] + n[2] * q[2]) / (len * ql));
                }
            }
            m.reversed = agree[1] > agree[0];
        }
        return &m;
    }
    // Writes a mesh again through t (from its data as loaded).
    void WriteMesh(uint8_t* base, uint32_t mesh, WorldMesh& m, const WorldFit::Transform& t)
    {
        const std::string key = WorldFit::Identity(t) ? std::string("as loaded") : WorldFit::Describe(t);
        if (m.applied == key || (m.applied.empty() && key == "as loaded")) return;
        const uint32_t step = m.stride / 4;
        WorldFit::Box box;
        for (uint32_t i = 0; i < m.vertices; i++)
        {
            const uint32_t* src = &m.original[size_t(i) * step];
            const uint32_t dst = m.buffer + m.stride * i;
            const float p[3] = { AsFloat(src[0]), AsFloat(src[1]), AsFloat(src[2]) };
            float q[3];
            WorldFit::Point(t, p, q);
            box.Add(q);
            Guest::Write32(base, dst, AsWord(q[0]));
            Guest::Write32(base, dst + 4, AsWord(q[1]));
            Guest::Write32(base, dst + 8, AsWord(q[2]));
            if (m.stride == 52)
                for (uint32_t at : { 3u, 11u, 12u }) Guest::Write32(base, dst + 4 * at, WorldFit::TurnPacked(src[at], m.reversed, t));
        }
        for (uint32_t w = 0; w < 3; w++) m.wrote[w] = Guest::Read32(base, m.buffer + 4 * w);
        // The box (+0x40+32 min, +48 max), so the model is not culled where it now is.
        for (int c = 0; c < 3; c++)
        {
            Guest::Write32(base, mesh + 0x40 + 32 + 4 * c, key == "as loaded" ? m.box[c] : AsWord(box.lo[c] - 1.0f));
            Guest::Write32(base, mesh + 0x40 + 48 + 4 * c, key == "as loaded" ? m.box[3 + c] : AsWord(box.hi[c] + 1.0f));
        }
        // The spheres (+32, +0x40+16), when they look like one (a centre
        // inside the box as loaded and a radius about the box's size): the
        // centre moved, the radius grown to hold the new box.
        const float lo[3] = { AsFloat(m.box[0]), AsFloat(m.box[1]), AsFloat(m.box[2]) }, hi[3] = { AsFloat(m.box[3]), AsFloat(m.box[4]), AsFloat(m.box[5]) };
        const float diagonal = sqrtf((hi[0] - lo[0]) * (hi[0] - lo[0]) + (hi[1] - lo[1]) * (hi[1] - lo[1]) + (hi[2] - lo[2]) * (hi[2] - lo[2]));
        const uint32_t sphereAt[2] = { mesh + 32, mesh + 0x40 + 16 };
        for (int k = 0; k < 2; k++)
        {
            const float c[3] = { AsFloat(m.sphere[k][0]), AsFloat(m.sphere[k][1]), AsFloat(m.sphere[k][2]) }, r = AsFloat(m.sphere[k][3]);
            bool looks = std::isfinite(r) && r > 0.2f * diagonal && r < 2.0f * diagonal;
            for (int a = 0; a < 3 && looks; a++) looks = c[a] >= lo[a] - 1.0f && c[a] <= hi[a] + 1.0f;
            if (!looks) continue;
            if (key == "as loaded") { for (int a = 0; a < 4; a++) Guest::Write32(base, sphereAt[k] + 4 * uint32_t(a), m.sphere[k][a]); continue; }
            float q[3];
            WorldFit::Point(t, c, q);
            float most = r * t.scale;
            for (int corner = 0; corner < 8; corner++)
            {
                const float p[3] = { (corner & 1) ? box.hi[0] : box.lo[0], (corner & 2) ? box.hi[1] : box.lo[1], (corner & 4) ? box.hi[2] : box.lo[2] };
                most = std::max(most, sqrtf((p[0] - q[0]) * (p[0] - q[0]) + (p[1] - q[1]) * (p[1] - q[1]) + (p[2] - q[2]) * (p[2] - q[2])));
            }
            for (int a = 0; a < 3; a++) Guest::Write32(base, sphereAt[k] + 4 * uint32_t(a), AsWord(q[a]));
            Guest::Write32(base, sphereAt[k] + 12, AsWord(most));
        }
        printf("world fit: mesh %08X (buffer %08X, %u vertices of %u bytes%s) written %s\n", mesh, m.buffer, m.vertices, m.stride,
               m.stride == 52 ? (m.reversed ? ", normals z-low" : ", normals x-low") : ", normals left (not 52-byte vertices)", key.c_str());
        fflush(stdout);
        m.applied = key;
    }
    // world_models.txt: written with the defaults when missing, read again
    // when saved since.
    std::map<std::string, WorldFit::Fit> g_worldFits = WorldFit::Parse(WorldFit::DefaultText());
    unsigned g_worldFitsVersion = 1;
    void ReadWorldFits()
    {
        static auto lastCheck = std::chrono::steady_clock::now() - std::chrono::seconds(10);
        const auto now = std::chrono::steady_clock::now();
        if (now - lastCheck < std::chrono::milliseconds(500)) return;
        lastCheck = now;
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        const std::filesystem::path file = std::filesystem::path(path).parent_path() / "world_models.txt";
        static std::filesystem::file_time_type seen{};
        std::error_code ec;
        if (!std::filesystem::exists(file, ec))
        {
            if (FILE* f = fopen(file.string().c_str(), "wb")) { fputs(WorldFit::DefaultText(), f); fclose(f); }
            printf("world fit: wrote the default world_models.txt\n"); fflush(stdout);
        }
        const auto when = std::filesystem::last_write_time(file, ec);
        if (ec || when == seen) return;
        seen = when;
        std::string text;
        if (FILE* f = fopen(file.string().c_str(), "rb")) { char b[4096]; size_t got; while ((got = fread(b, 1, sizeof(b), f)) > 0) text.append(b, got); fclose(f); }
        std::string problems;
        g_worldFits = WorldFit::Parse(text, &problems);
        g_worldFitsVersion++;
        printf("world fit: world_models.txt read: %zu lines%s%s\n", g_worldFits.size(), problems.empty() ? "" : "; ", problems.c_str());
        fflush(stdout);
    }
    std::string BoxText(const WorldFit::Box& b)
    {
        char t[128];
        if (!b.valid) return "none";
        snprintf(t, sizeof(t), "x %.1f..%.1f y %.1f..%.1f z %.1f..%.1f", b.lo[0], b.hi[0], b.lo[1], b.hi[1], b.lo[2], b.hi[2]);
        return t;
    }
    const std::vector<uint32_t>* WorldFitOriginal(uint32_t buffer, uint32_t vertices)
    {
        std::lock_guard<std::mutex> guard(g_worldLock);
        const auto range = g_worldBufferMesh.equal_range(buffer);
        for (auto it = range.first; it != range.second; ++it)
        {
            const auto m = g_worldMeshes.find(it->second);
            if (m == g_worldMeshes.end() || m->second.vertices != vertices) continue;
            if (m->second.applied.empty() || m->second.applied == "as loaded") return nullptr;
            return &m->second.original;
        }
        return nullptr;
    }
    const uint32_t* WorldFitOriginalBox(uint32_t mesh)
    {
        std::lock_guard<std::mutex> guard(g_worldLock);
        const auto m = g_worldMeshes.find(mesh);
        if (m == g_worldMeshes.end() || m->second.applied.empty() || m->second.applied == "as loaded") return nullptr;
        return m->second.box;
    }
}
static void WorldFitSeeMesh(uint8_t* base, const std::string& mesh, uint32_t at)
{
    (void)base;
    if (!at || mesh.empty()) return;
    const std::string model = ModelOf(mesh);
    std::lock_guard<std::mutex> guard(g_worldLock);
    std::vector<uint32_t>& list = g_modelMeshes[model];
    if (std::find(list.begin(), list.end(), at) != list.end()) return;
    list.push_back(at);
    {
        const size_t dot = mesh.find('.');
        g_meshPart[at] = WorldFit::Lower(dot == std::string::npos ? std::string() : mesh.substr(0, dot));
    }
    if (getenv("COD3_WORLD_MESHES") || model.find("world") != std::string::npos || model.find("mg42") != std::string::npos)
    {
        static int said = 0;
        if ((said < 400 && said++ < 400)) { printf("world fit: mesh %s at %08X\n", mesh.c_str(), at); fflush(stdout); }
    }
}
// Once a frame in a match (from MpFixesMg42View).
void WorldFitFrame(uint8_t* base)
{
    ReadWorldFits();
    std::lock_guard<std::mutex> guard(g_worldLock);
    static std::map<std::string, std::string> told;   // gun: what was said of it
    std::set<uint32_t> done;                           // a mesh two guns share (the Lee-Enfields): the first gun's fit
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
    {
        const CampaignGun& gun = Guns[i];
        if (g_campaignWeapon[i] <= 0 || !gun.worldModel || g_copyWorld[i].empty()) continue;
        const std::string own = WorldFit::Lower(gun.worldModel), copy = WorldFit::Lower(g_copyWorld[i]);
        if (own == copy) continue;
        const auto ownMeshes = g_modelMeshes.find(own), copyMeshes = g_modelMeshes.find(copy);
        if (ownMeshes == g_modelMeshes.end()) continue;
        // its meshes kept as loaded, and the boxes
        WorldFit::Box ownBox, copyBox;
        std::vector<std::pair<uint32_t, WorldMesh*>> meshes;
        const WorldFit::Fit fit = WorldFit::For(g_worldFits, gun.name);
        auto hidden = [&](uint32_t mesh) { const auto part = g_meshPart.find(mesh); return part != g_meshPart.end() && std::find(fit.hide.begin(), fit.hide.end(), part->second) != fit.hide.end(); };
        // A hidden part: its triangle, vertex and index counts and its index
        // buffer's size none (as the MG34's bipod in the MG42's view: the
        // counts alone did not hide it), kept to be put back. Its vertices
        // are not touched (a model's parts may share one buffer).
        static const uint32_t HideFields[] = { 28, 0x40 + 88, 0x40 + 104, 0x158 + 28 };
        for (uint32_t mesh : ownMeshes->second)
        {
            const bool hide = hidden(mesh);
            const auto saved = g_worldHidden.find(mesh);
            std::array<uint32_t, 4> now{};
            for (int f = 0; f < 4; f++) now[f] = Guest::Read32(base, mesh + HideFields[f]);
            const std::array<uint32_t, 4> none{};
            if (hide)
            {
                if (Guest::Read32(base, mesh + 0x40 + 80) < mesh || Guest::Read32(base, mesh + 0x40 + 80) > mesh + 0x400) continue;
                if (now != none)
                {
                    g_worldHidden[mesh] = now;
                    for (int f = 0; f < 4; f++) Guest::Write32(base, mesh + HideFields[f], 0);
                    printf("world fit: %s's part %s (mesh %08X) hidden\n", gun.name, g_meshPart[mesh].c_str(), mesh); fflush(stdout);
                }
                continue;
            }
            if (saved != g_worldHidden.end())
            {
                if (now == none) for (int f = 0; f < 4; f++) Guest::Write32(base, mesh + HideFields[f], saved->second[f]);
                printf("world fit: %s's part %s (mesh %08X) shown again\n", gun.name, g_meshPart[mesh].c_str(), mesh); fflush(stdout);
                g_worldHidden.erase(saved);
            }
            if (WorldMesh* m = Keep(base, mesh)) { meshes.emplace_back(mesh, m); ownBox.Add(VertexBox(base, mesh)); }
        }
        if (copyMeshes != g_modelMeshes.end())
            for (uint32_t mesh : copyMeshes->second)
            {   // kept by the buffer's first words (a new map's load changes them)
                static std::map<uint32_t, std::pair<std::array<uint32_t, 4>, WorldFit::Box>> boxes;
                uint32_t buffer, vertices, stride;
                if (!MeshBuffer(base, mesh, buffer, vertices, stride) || !Filled(base, buffer, vertices, stride)) continue;
                const std::array<uint32_t, 4> key = { buffer, Guest::Read32(base, buffer), Guest::Read32(base, buffer + 4), vertices };
                auto& cached = boxes[mesh];
                if (cached.first != key || !cached.second.valid) cached = { key, VertexBox(base, mesh) };
                copyBox.Add(cached.second);
            }
        if (meshes.empty()) continue;
        if (fit.mode == WorldFit::Mode::Auto && !copyBox.valid) continue;   // the copy's model not seen yet
        std::string why;
        const WorldFit::Transform t = WorldFit::Make(fit, ownBox, copyBox, &why);
        char line[1024];
        snprintf(line, sizeof(line), "world fit: %s's %s (%zu mesh%s, box %s) to %s's %s (box %s): %s -> %s", gun.name, gun.worldModel, meshes.size(),
                 meshes.size() == 1 ? "" : "es", BoxText(ownBox).c_str(), gun.from, g_copyWorld[i].c_str(), BoxText(copyBox).c_str(), why.c_str(),
                 WorldFit::Identity(t) ? "left as loaded" : WorldFit::Describe(t).c_str());
        if (told[gun.name] != line)
        {
            told[gun.name] = line;
            printf("%s\n", line);
            if (meshes.size() > 1) printf("world fit: %s's model has %zu meshes: each is moved in its own part's frame\n", gun.name, meshes.size());
            fflush(stdout);
        }
        for (auto& [mesh, m] : meshes)
            if (done.insert(mesh).second) WriteMesh(base, mesh, *m, t);
    }
}

// The Lee-Enfields' stock bent a little away from the right hand: the
// British arms' moves on multiplayer's arms put the hand into the butt
// (hand test, session 4). Only the body's vertices past the grip (along
// its length) move, more the further back, so the sights, bolt and scope
// stay where they were. Bend: "axis sign from dx dy dz" (axis 0-2 the
// length, sign +1 if the butt lies toward +axis, from the point where the
// bend starts; dx dy dz the butt end's move). COD3_ENFIELD_BEND sets it,
// COD3_ENFIELD_BEND_CYCLE="bend;bend;..." tries each for 4 s with a
// screenshot once a Lee-Enfield is in hand (tuning).
namespace
{
    struct StockBend { int axis = 1; float sign = 1.0f, from = 0.0f, dx = 0.0f, dy = 0.0f, dz = 0.0f; bool on = false; };
    StockBend ParseBend(const char* text)
    {
        StockBend b; if (!text) return b;
        b.on = sscanf(text, "%d %f %f %f %f %f", &b.axis, &b.sign, &b.from, &b.dx, &b.dy, &b.dz) == 6 && b.axis >= 0 && b.axis < 3;
        return b;
    }
    StockBend g_bend = []() { const char* t = getenv("COD3_ENFIELD_BEND"); return t ? ParseBend(t) : StockBend{}; }();
    std::map<uint32_t, std::vector<float>> g_enfieldOriginal;   // each body's buffer: its positions as loaded
    std::map<uint32_t, std::string> g_enfieldApplied;            // and the bend it has now
    std::string BendKey(const StockBend& b) { char t[96]; snprintf(t, sizeof(t), "%d %.2f %.2f %.3f %.3f %.3f %d", b.axis, b.sign, b.from, b.dx, b.dy, b.dz, b.on); return t; }
    void ApplyBend(uint8_t* base, uint32_t body, const StockBend& bend)
    {
        const uint32_t piece = body + 0x40;
        const uint32_t vdesc = Guest::Read32(base, piece + 80), decl = Guest::Read32(base, piece + 112);
        if (vdesc != body + 0x138 || !decl) return;
        const uint32_t stride = Guest::Read32(base, decl), vertices = Guest::Read32(base, piece + 88);
        const uint32_t from = Guest::Read32(base, vdesc + 24) & ~3u;
        if (!from || stride < 12 || stride > 128 || vertices == 0 || vertices > 60000) return;
        auto& original = g_enfieldOriginal[from];
        if (original.size() != size_t(vertices) * 3)
        {
            // not yet filled (all zero so far): later
            bool any = false;
            for (uint32_t i = 0; i < vertices && i < 64 && !any; i++) any = Guest::Read32(base, from + stride * i) != 0 || Guest::Read32(base, from + stride * i + 8) != 0;
            if (!any) { g_enfieldOriginal.erase(from); return; }
            original.resize(size_t(vertices) * 3);
            for (uint32_t i = 0; i < vertices; i++) for (uint32_t c = 0; c < 3; c++) original[3 * i + c] = ReadFloat(base, from + stride * i + 4 * c);
        }
        const std::string key = BendKey(bend);
        if (g_enfieldApplied[from] == key) return;
        g_enfieldApplied[from] = key;
        // how far the butt reaches past the bend's start
        float reach = 0.0f;
        for (uint32_t i = 0; i < vertices; i++) reach = std::max(reach, (original[3 * i + bend.axis] - bend.from) * bend.sign);
        int moved = 0;
        for (uint32_t i = 0; i < vertices; i++)
        {
            float p[3] = { original[3 * i], original[3 * i + 1], original[3 * i + 2] };
            const float past = (p[bend.axis] - bend.from) * bend.sign;
            if (bend.on && past > 0.0f && reach > 0.0f)
            {
                const float k = past / reach;
                p[0] += bend.dx * k; p[1] += bend.dy * k; p[2] += bend.dz * k;
                moved++;
            }
            for (uint32_t c = 0; c < 3; c++) WriteFloat(base, from + stride * i + 4 * c, p[c]);
        }
        printf("enfield view: body %08X stock bend %s: %d of %u vertices moved (reach %.2f)\n", body, key.c_str(), moved, vertices, reach);
        fflush(stdout);
    }
}
void MpFixesEnfieldStock(uint8_t* base, uint32_t held)
{
    if (g_enfieldBodies.empty()) return;
    if (const char* cycle = getenv("COD3_ENFIELD_BEND_CYCLE"))
    {
        static std::vector<std::string> bends;
        static bool parsed = false;
        if (!parsed) { parsed = true; std::string all(cycle); size_t at = 0; while (at <= all.size()) { size_t semi = all.find(';', at); bends.push_back(all.substr(at, semi == std::string::npos ? std::string::npos : semi - at)); if (semi == std::string::npos) break; at = semi + 1; } }
        static bool started = false; static std::chrono::steady_clock::time_point since; static int shown = -1; static bool pictured = true;
        bool holding = false;
        for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
            if ((strcmp(Guns[i].name, "lee_enfield") == 0 || strcmp(Guns[i].name, "lee_enfield_sniper") == 0) && g_campaignWeapon[i] > 0 && held == uint32_t(g_campaignWeapon[i])) holding = true;
        if (holding && !started) { started = true; since = std::chrono::steady_clock::now(); }
        if (started)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
            const int index = int(t / 4.0);
            if (index != shown && index < int(bends.size()))
            {
                shown = index; pictured = false;
                g_bend = ParseBend(bends[size_t(index)].c_str());
                printf("enfield view: bend %d: %s\n", index, bends[size_t(index)].c_str()); fflush(stdout);
            }
            if (!pictured && t - shown * 4.0 >= 2.0) { pictured = true; Overlay::RequestScreenshot(); printf("enfield view: screenshot of bend %d\n", shown); fflush(stdout); }
        }
    }
    std::lock_guard<std::mutex> guard(g_bodyLock);
    for (size_t b = 0; b < g_enfieldBodies.size();)
    {
        const uint32_t body = g_enfieldBodies[b];
        if (Guest::Read32(base, body + 0x40 + 80) != body + 0x138) { g_enfieldBodies.erase(g_enfieldBodies.begin() + long(b)); continue; }
        ApplyBend(base, body, g_bend);
        b++;
    }
}
// 0.5.206: a campaign gun's weapon number (0: not one, or not made on this
// map), for bots.cpp's pose test (a campaign gun's record keeps its copy's
// name, so the weapon list can't be searched by name for it).
int32_t MpFixesCampaignWeaponNumber(const char* name)
{
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
        if (_stricmp(name, Guns[i].name) == 0) return g_campaignWeapon[i] > 0 ? g_campaignWeapon[i] : 0;
    return 0;
}
void MpFixesMg42View(uint8_t* base, uint32_t held)
{
    WorldFitFrame(base);   // 0.5.206: the campaign guns' third-person models
    MpFixesEnfieldStock(base, held);
    // The new scoped Lee-Enfield intermittently blacks out on ADS. Keep the
    // transition trace lightweight; a forced full-frame renderer dump here
    // can itself stall the frame we are trying to diagnose.
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t count = Guest::Read32(base, 0x82BCBF94);
        const uint32_t def = list && held && held <= count ? Guest::Read32(base, list + 4 * held) : 0;
        static bool wasAimed = false;
        static unsigned traces = 0;
        float ads = 0;
        if (ps) { const uint32_t bits = Guest::Read32(base, ps + 172); memcpy(&ads, &bits, 4); }
        const bool aimed = def && ReadText(base, Guest::Read32(base, def + 8)) == "lee_enfield_sniper" && ads > 0.01f;
        if (aimed != wasAimed && traces < 48) {
            ++traces;
            printf("scope trace: Lee-Enfield ADS %s %.4f, def %08X, overlay %08X reticle %08X, zoom %08X in %08X out %08X, blur %u shock %u\n",
                aimed ? "enter" : "leave", ads, def,
                def ? Guest::Read32(base, def + 1616) : 0, def ? Guest::Read32(base, def + 1620) : 0,
                def ? Guest::Read32(base, def + 1600) : 0, def ? Guest::Read32(base, def + 1608) : 0,
                def ? Guest::Read32(base, def + 1612) : 0, unsigned(Settings::Get().aimBlur), unsigned(Settings::Get().shellShockBlur));
            fflush(stdout);
        }
        wasAimed = aimed;
    }

    // DIAGNOSTIC: COD3_PS_WATCH=1: the local player's state, its words that
    // change, twice a second (to find the aim-down-sights fraction).
    if (getenv("COD3_PS_WATCH"))
    {
        static auto last = std::chrono::steady_clock::now();
        static std::vector<uint32_t> before;
        const auto now = std::chrono::steady_clock::now();
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        if (ps && now - last > std::chrono::milliseconds(500))
        {
            last = now;
            std::vector<uint32_t> words(1024);
            for (uint32_t i = 0; i < 1024; i++) words[i] = Guest::Read32(base, ps + 4 * i);
            if (before.size() == words.size())
            {
                std::string line;
                for (uint32_t i = 0; i < 1024; i++)
                    if (words[i] != before[i])
                    {
                        float f; memcpy(&f, &words[i], 4);
                        char one[48];
                        if (getenv("COD3_PS_WATCH")[0] != '2' && f >= -0.01f && f <= 1.01f && words[i] != 0) snprintf(one, sizeof(one), " +%u=%.3f", 4 * i, f);
                        else snprintf(one, sizeof(one), " +%u=%08X", 4 * i, words[i]);
                        line += one;
                        if (line.size() > 1500) break;
                    }
                printf("sound times: %lld ps%s\n", SoundMs(), line.c_str());
                fflush(stdout);
            }
            before = words;
        }
    }
    {
        const long long from = g_reloadShotsFrom.load();
        static long long last = -1; static int next = 0;
        if (from >= 0)
        {
            if (from != last) { last = from; next = 0; }
            const long long since = SoundMs() - from;
            if (next <= 18 && since >= next * 250) { Overlay::RequestScreenshot(); printf("sound times: %lld reload screenshot %d (+%lld ms)\n", SoundMs(), next, since); fflush(stdout); next++; }
        }
    }
    // COD3_SHOTS="12 20 30": screenshots that many seconds after the first
    // frame in a match (testing the MG42's look unattended).
    if (const char* shots = getenv("COD3_SHOTS"))
    {
        static const auto start = std::chrono::steady_clock::now();
        static std::vector<double> at;
        static bool parsed = false;
        if (!parsed) { parsed = true; for (const char* t = shots; *t;) { char* end; const double v = strtod(t, &end); if (end == t) break; at.push_back(v); t = end; } }
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (size_t i = 0; i < at.size(); i++)
            if (now >= at[i]) { Overlay::RequestScreenshot(); printf("mg42 view: screenshot at %.1f s (held %u)\n", now, held); fflush(stdout); at.erase(at.begin() + long(i)); break; }
    }
    static int32_t mg42 = -1;
    if (mg42 < 0)
        for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
            if (strcmp(Guns[i].name, "mg42") == 0) mg42 = int32_t(i);
    if (mg42 < 0 || getenv("COD3_MG42_NO_BODY")) return;
    std::lock_guard<std::mutex> guard(g_bodyLock);
    if (!g_mg34Body || !g_mg42Body) return;
    static uint32_t madeFrom = 0, made = 0;
    static uint32_t madeAds = 0;
    if (madeFrom != g_mg42Body)
    {
        madeFrom = g_mg42Body;
        made = getenv("COD3_MG42_RAW_BODY") ? g_mg42Body : MakeMg42Body(base, g_mg42Body, g_mg34Body, g_fit, 0);
        madeAds = made ? MakeMg42Body(base, g_mg42Body, g_mg34Body, g_adsFit, 1) : 0;
    }
    if (!made) return;
    // DIAGNOSTIC: COD3_MESH_DUMP=folder: the MG34 and MG42 bodies' vertex
    // positions and triangles as .obj files, once (to find their sights).
    if (const char* folder = getenv("COD3_MESH_DUMP"))
    {
        static bool dumped = false;
        if (!dumped)
        {
            dumped = true;
            auto dump = [&](uint32_t mesh, const char* name)
            {
                const uint32_t piece = mesh + 0x40;
                const uint32_t vdesc = Guest::Read32(base, piece + 80), idesc = Guest::Read32(base, piece + 96), decl = Guest::Read32(base, piece + 112);
                const uint32_t stride = Guest::Read32(base, decl), vertices = Guest::Read32(base, piece + 88);
                const uint32_t from = Guest::Read32(base, vdesc + 24) & ~3u, indices = Guest::Read32(base, idesc + 24), indexBytes = Guest::Read32(base, idesc + 28);
                const std::string path = std::string(folder) + "\\" + name + ".obj";
                FILE* f = fopen(path.c_str(), "w");
                if (!f || stride < 12 || stride > 256 || vertices > 200000) { if (f) fclose(f); printf("mesh dump: %s not written (stride %u, vertices %u)\n", name, stride, vertices); return; }
                fprintf(f, "# %s stride %u vertices %u indexBytes %u\n", name, stride, vertices, indexBytes);
                for (uint32_t i = 0; i < vertices; i++)
                    fprintf(f, "v %f %f %f\n", ReadFloat(base, from + stride * i), ReadFloat(base, from + stride * i + 4), ReadFloat(base, from + stride * i + 8));
                for (uint32_t t = 0; t + 6 <= indexBytes && indices; t += 6)
                    fprintf(f, "f %u %u %u\n", Guest::Read16(base, indices + t) + 1u, Guest::Read16(base, indices + t + 2) + 1u, Guest::Read16(base, indices + t + 4) + 1u);
                fclose(f);
                printf("mesh dump: %s written (stride %u, vertices %u)\n", path.c_str(), stride, vertices);
                fflush(stdout);
            };
            dump(g_mg34Body, "mg34-body");
            if (!g_enfieldBodies.empty()) dump(g_enfieldBodies.back(), "enfield-body");
            dump(g_mg42Body, "mg42-body");
            if (madeAds) dump(madeAds, "mg42-ads-made");
            dump(made, "mg42-hip-made");
        }
    }
    // COD3_MG42_ADS_CYCLE="fit;fit;...": the aiming body's fit, 4 s each
    // from when the MG42 is first aimed, a screenshot 2.5 s into each.
    if (const char* cycle = getenv("COD3_MG42_ADS_CYCLE"))
    {
        static std::vector<std::string> fits;
        static bool parsed = false;
        if (!parsed) { parsed = true; std::string all(cycle); size_t at = 0; while (at <= all.size()) { size_t semi = all.find(';', at); fits.push_back(all.substr(at, semi == std::string::npos ? std::string::npos : semi - at)); if (semi == std::string::npos) break; at = semi + 1; } }
        static bool started = false; static std::chrono::steady_clock::time_point since; static int shown = -1; static bool pictured = true;
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        float frac = 0; if (ps) { const uint32_t v = Guest::Read32(base, ps + 172); memcpy(&frac, &v, 4); }
        if (frac > 0.9f && !started) { started = true; since = std::chrono::steady_clock::now(); }
        if (started)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
            const int index = int(t / 4.0);
            if (index != shown && index < int(fits.size()))
            {
                shown = index; pictured = false;
                g_adsFit = ParseFit(fits[size_t(index)].c_str());
                printf("mg42 view: aiming fit %d: %s\n", index, fits[size_t(index)].c_str());
                madeAds = MakeMg42Body(base, g_mg42Body, g_mg34Body, g_adsFit, 1);
            }
            if (!pictured && t - shown * 4.0 >= 2.5) { pictured = true; Overlay::RequestScreenshot(); printf("mg42 view: screenshot of aiming fit %d (aim %.2f)\n", shown, frac); fflush(stdout); }
        }
    }
    // COD3_MG42_BODY_CYCLE="fit;fit;...": each fit for 4 s from when the
    // MG42 is first in hand, with a screenshot 2 s into each (tuning).
    if (const char* cycle = getenv("COD3_MG42_BODY_CYCLE"))
    {
        static std::vector<std::string> fits;
        static bool parsed = false;
        if (!parsed) { parsed = true; std::string all(cycle); size_t at = 0; while (at <= all.size()) { size_t semi = all.find(';', at); fits.push_back(all.substr(at, semi == std::string::npos ? std::string::npos : semi - at)); if (semi == std::string::npos) break; at = semi + 1; } }
        static bool started = false; static std::chrono::steady_clock::time_point since; static int shown = -1; static bool pictured = true;
        // a fit of scale 0: the MG34's own body, to compare
        const bool holding = g_campaignWeapon[mg42] > 0 && held == uint32_t(g_campaignWeapon[mg42]);
        if (holding && !started) { started = true; since = std::chrono::steady_clock::now(); }
        if (started)
        {
            const double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - since).count();
            const int index = int(t / 4.0);
            if (index != shown && index < int(fits.size()))
            {
                shown = index; pictured = false;
                g_fit = ParseFit(fits[size_t(index)].c_str());
                printf("mg42 view: fit %d: %s\n", index, fits[size_t(index)].c_str());
                g_cycleMg34 = g_fit.scale <= 0;
                if (!g_cycleMg34) made = MakeMg42Body(base, g_mg42Body, g_mg34Body, g_fit, 0);
            }
            if (!pictured && t - shown * 4.0 >= 2.0) { pictured = true; Overlay::RequestScreenshot(); printf("mg42 view: screenshot of fit %d\n", shown); fflush(stdout); }
        }
    }
    // The slot alone is not what is drawn (0.5.900's first tries: the MG34
    // still in view): the drawn body's own fields are changed in place, the
    // triangle count (+28), and of its piece (+0x40) the material (+68), the
    // vertex count (+88), the index count (+104), the vertex and index
    // buffers (+24/+28 of the descriptions at +0x138 and +0x158).
    static const uint32_t Fields[] = { 28, 0x40 + 68, 0x40 + 88, 0x40 + 104, 0x138 + 24, 0x138 + 28, 0x158 + 24, 0x158 + 28 };
    constexpr size_t FieldCount = sizeof(Fields) / sizeof(Fields[0]);
    static std::map<uint32_t, std::array<uint32_t, FieldCount>> saved;
    // Aiming (the player's state +172, the sights' fraction, 0 to 1): the
    // body made for the aim, moved so the MG34's sight line, where the eye
    // sits, has the MG42's sights on it and not its receiver.
    float aim = 0;
    {
        const uint32_t entities = Guest::Read32(base, 0x82BCC7FC);
        const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t ps = own ? Guest::Read32(base, own + 512) : 0;
        if (ps) { const uint32_t v = Guest::Read32(base, ps + 172); memcpy(&aim, &v, 4); }
    }
    // The sights coming up (or going down): the body moved step by step
    // from the hip's fit to the sights' (one jump halfway let the gun
    // slide through the view, 0.5.900's hand test). The fraction +172 may
    // jump straight to 1: it is followed at most as fast as the MG42's own
    // adsTransInTime (+1684) and adsTransOutTime (+1688) allow, so the body
    // moves with the arms' raise.
    {
        static float shown = 0.0f;
        static auto last = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::min(0.1f, std::chrono::duration<float>(now - last).count());
        last = now;
        const bool holding = g_campaignWeapon[mg42] > 0 && held == uint32_t(g_campaignWeapon[mg42]);
        const float target = std::clamp(aim, 0.0f, 1.0f);
        if (!holding) shown = target;
        else
        {
            float inTime = 0.3f, outTime = 0.4f;
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t def = list ? Guest::Read32(base, list + 4 * uint32_t(g_campaignWeapon[mg42])) : 0;
            auto seconds = [&](uint32_t at, float fallback)
            {
                const uint32_t v = def ? Guest::Read32(base, def + at) : 0;
                if (v > 0x10000000u) { float f; memcpy(&f, &v, 4); return f > 0.01f && f < 5.0f ? f : fallback; }
                return v > 10 && v < 5000 ? float(v) / 1000.0f : fallback;
            };
            inTime = seconds(1684, inTime); outTime = seconds(1688, outTime);
            if (target > shown) shown = std::min(target, shown + dt / inTime);
            else shown = std::max(target, shown - dt / outTime);
        }
        static float said = -1.0f;
        if (getenv("COD3_MG42_BLEND_SHOTS") && holding && fabsf(aim - said) > 0.001f) { said = aim; printf("mg42 view: aim fraction %.3f, followed %.3f\n", aim, shown); fflush(stdout); }
        aim = shown;
    }
    // The aiming body (slot 1) is made again on the way, from the hip's fit
    // to the sights' (t 0 to 1), and dipped lower in between (the gun rode
    // a little high while coming up, hand test): COD3_MG42_DIP, default 1.0
    // (1.6 left the hands above the gun; screenshots at each step).
    static float betweenAt = -1.0f;
    static const float dip = getenv("COD3_MG42_DIP") ? float(atof(getenv("COD3_MG42_DIP"))) : 1.0f;
    uint32_t drawn = made;
    const bool holdingNow = g_campaignWeapon[mg42] > 0 && held == uint32_t(g_campaignWeapon[mg42]);
    if (madeAds && aim > 0.03f && holdingNow && !getenv("COD3_MG42_ADS_CYCLE"))
    {
        const float step = aim >= 0.97f ? 1.0f : aim;
        if (fabsf(step - betweenAt) > 0.04f || (step == 1.0f && betweenAt != 1.0f))
        {
            const float t = step * step * (3.0f - 2.0f * step);   // eased
            BodyFit mix = g_adsFit;
            if (step < 1.0f)
            {
                mix.scale = g_fit.scale + (g_adsFit.scale - g_fit.scale) * t;
                mix.x = g_fit.x + (g_adsFit.x - g_fit.x) * t;
                mix.y = g_fit.y + (g_adsFit.y - g_fit.y) * t;
                mix.z = g_fit.z + (g_adsFit.z - g_fit.z) * t - dip * sinf(3.14159265f * t);
                mix.pitch = g_fit.pitch + (g_adsFit.pitch - g_fit.pitch) * t;
                mix.yaw = g_fit.yaw + (g_adsFit.yaw - g_fit.yaw) * t;
            }
            static int quiet = 0;
            const bool tell = step == 1.0f && quiet < 4;
            if (tell) quiet++;
            const uint32_t m = MakeMg42Body(base, g_mg42Body, g_mg34Body, mix, 1, !tell);
            if (m) { madeAds = m; betweenAt = step; }
            // DIAGNOSTIC: COD3_MG42_BLEND_SHOTS=1: a screenshot at each step
            if (m && getenv("COD3_MG42_BLEND_SHOTS")) { Overlay::RequestScreenshot(); printf("mg42 view: aiming body at %.2f (t %.2f), screenshot\n", step, t); fflush(stdout); }
        }
        drawn = madeAds;
    }
    else if (madeAds && aim >= 0.97f && getenv("COD3_MG42_ADS_CYCLE")) drawn = madeAds;
    std::array<uint32_t, FieldCount> theirs{};
    for (size_t f = 0; f < FieldCount; f++) theirs[f] = Guest::Read32(base, drawn + Fields[f]);
    const bool on = g_campaignWeapon[mg42] > 0 && held == uint32_t(g_campaignWeapon[mg42]) && !g_cycleMg34;
    for (size_t b = 0; b < g_mg34Bodies.size();)
    {
        const uint32_t body = g_mg34Bodies[b];
        // the MG34 body's own layout, checked as MakeMg42Body checks the MG42's
        if (Guest::Read32(base, body + 0x40 + 80) != body + 0x138 || Guest::Read32(base, body + 0x40 + 96) != body + 0x158)
        { saved.erase(body); g_mg34Bodies.erase(g_mg34Bodies.begin() + long(b)); continue; }
        std::array<uint32_t, FieldCount> now{};
        for (size_t f = 0; f < FieldCount; f++) now[f] = Guest::Read32(base, body + Fields[f]);
        if (!saved.count(body)) saved[body] = now;
        bool ours = now == theirs;
        for (uint32_t m : { made, madeAds })
            if (m) { bool same = true; for (size_t f = 0; f < FieldCount; f++) if (now[f] != Guest::Read32(base, m + Fields[f])) same = false; ours = ours || same; }
        if (now != saved[body] && !ours) { saved[body] = now; }   // something new loaded there
        const auto& want = on ? theirs : saved[body];
        if (now != want)
        {
            for (size_t f = 0; f < FieldCount; f++) Guest::Write32(base, body + Fields[f], want[f]);
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("mg42 view: the gun body %08X in view is now the %s\n", body, on ? "MG42's" : "MG34's"); fflush(stdout); }
        }
        b++;
    }
    // hidden: the triangle count (+28), the vertex and index counts
    // (+0x40+88, +0x40+104) and the index buffer's size (+0x158+28) all none
    static const uint32_t HideFields[] = { 28, 0x40 + 88, 0x40 + 104, 0x158 + 28 };
    static std::map<uint32_t, std::array<uint32_t, 4>> counts;
    for (size_t h = 0; h < g_mg34Hidden.size();)
    {
        const uint32_t part = g_mg34Hidden[h];
        if (Guest::Read32(base, part + 0x40 + 80) != part + 0x138) { counts.erase(part); g_mg34Hidden.erase(g_mg34Hidden.begin() + long(h)); continue; }
        std::array<uint32_t, 4> now{};
        for (int f = 0; f < 4; f++) now[f] = Guest::Read32(base, part + HideFields[f]);
        const std::array<uint32_t, 4> none{};
        if (!counts.count(part) || (now != none && now != counts[part])) counts[part] = now;
        const std::array<uint32_t, 4> want = on ? none : counts[part];
        if (now != want)
        {
            for (int f = 0; f < 4; f++) Guest::Write32(base, part + HideFields[f], want[f]);
            static int said = 0;
            if ((said < 20 && said++ < 20)) { printf("mg42 view: part %08X %s\n", part, on ? "hidden" : "shown again"); fflush(stdout); }
        }
        h++;
    }
}

// The items list (sub_82486BF8, 52 bytes each from 0x82C75A48 + 52, item i
// for weapon i: the name's hash, the name, the model, the HUD icon, ...):
// the campaign guns' items logged once it is made.
extern "C" PPC_FUNC(__imp__sub_82486BF8);
void ResolveCampaignIconsLater(PPCContext& ctx, uint8_t* base);
PPC_FUNC(sub_82486BF8)
{
    __imp__sub_82486BF8(ctx, base);
    ResolveCampaignIconsLater(ctx, base);
    // The MG34 held, not deployed (ini mg34_no_deploy, on by default), as
    // the MG42: aiming with its class (+176) 3 deploys the bipod and forces
    // the player prone ("Prone blocked"); with the BAR's class and the
    // deployed animations (+148 lmgDeployedAnim, +152 lmgDeployAnim, +156
    // lmgBreakdownAnim) left out it aims from the shoulder.
    if (Settings::Get().mg34NoDeploy && !getenv("COD3_MG42_DEPLOY"))
    {
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        uint32_t mg34 = 0, barClass = 0; bool bar = false;
        for (int32_t k = 1; list && k <= count && k < 64; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            if (!def) continue;
            const std::string name = ReadText(base, Guest::Read32(base, def + 8), 32);
            if (name == "mg34") mg34 = def;
            else if (name == "bar") { barClass = Guest::Read32(base, def + 176); bar = true; }
        }
        if (mg34 && bar && Guest::Read32(base, mg34 + 176) != barClass)
        {
            const uint32_t none = GuestText(base, "");
            printf("weapons: mg34 held, not deployed: class %u -> %u (the BAR's), bipod animations %s / %s / %s left out\n", Guest::Read32(base, mg34 + 176), barClass,
                   ReadText(base, Guest::Read32(base, mg34 + 148)).c_str(), ReadText(base, Guest::Read32(base, mg34 + 152)).c_str(), ReadText(base, Guest::Read32(base, mg34 + 156)).c_str());
            for (uint32_t at : { 148u, 152u, 156u }) Guest::Write32(base, mg34 + at, none);
            Guest::Write32(base, mg34 + 176, barClass);
        }
    }
    // Scoped campaign guns: the scope of the gun named (its sights' zoom
    // +1600, zoom in/out fractions +1608/+1612, overlay +1616, reticle
    // +1620, overlay width and height +1624/+1628), once every weapon is set up.
    {
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
        {
            const int32_t weapon = g_campaignWeapon[i];
            if (weapon <= 0 || !Guns[i].scopeFrom || !list || weapon > count) continue;
            const uint32_t own = Guest::Read32(base, list + 4 * uint32_t(weapon));
            uint32_t scope = 0;
            for (int32_t k = 1; k <= count && !scope; k++)
            {
                const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
                if (def && ReadText(base, Guest::Read32(base, def + 8)) == Guns[i].scopeFrom) scope = def;
            }
            if (!own || !scope) { printf("weapons: %s: no %s to take the scope of\n", Guns[i].name, Guns[i].scopeFrom); continue; }
            for (uint32_t at = 1600; at <= 1628; at += 4)
                if (at != 1604) Guest::Write32(base, own + at, Guest::Read32(base, scope + at));
            printf("weapons: %s takes %s's scope (overlay %s)\n", Guns[i].name, Guns[i].scopeFrom, ReadText(base, Guest::Read32(base, own + 1616)).c_str());
        }
        fflush(stdout);
    }
    {   // 0.5.219: Justin: carry four mines and four rifle grenades in all,
        // and get them back from an ammo crate once used. Each weapon's
        // clipSize (+1476), startAmmo (+1452) and maxAmmo (+1472): the
        // mines' and the rifle grenades' (names with "mine" or ending "_rg")
        // reserve set so clip + reserve = 4, start and most alike (a crate
        // fills up to maxAmmo). COD3_SPECIAL_AMMO=n changes the 4.
        int want = 4;
        if (const char* e = getenv("COD3_SPECIAL_AMMO")) want = std::clamp(atoi(e), 1, 20);
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        for (int32_t k = 1; list && k <= count && k < 128; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            if (!def) continue;
            const std::string name = ReadText(base, Guest::Read32(base, def + 8), 32);
            const bool rifleGrenade = name.size() > 3 && name.compare(name.size() - 3, 3, "_rg") == 0;
            const bool mine = name.find("mine") != std::string::npos;
            if (!rifleGrenade && !mine) continue;
            const uint32_t clip = Guest::Read32(base, def + 1476), start = Guest::Read32(base, def + 1452), most = Guest::Read32(base, def + 1472);
            // 0.5.220: clip and spare share one pool here (same index) and the kit
            // fills the spare only, so the most an ammo crate refills to is want.
            (void)clip;
            Guest::Write32(base, def + 1472, uint32_t(want));
            Guest::Write32(base, def + 1452, uint32_t(want));
            printf("weapons: %d %s (class %u): clip %u, start %u -> %u, max %u -> %u (clip ammo #%u, reserve #%u)\n", k, name.c_str(), Guest::Read32(base, def + 176), clip,
                   start, Guest::Read32(base, def + 1452), most, Guest::Read32(base, def + 1472), Guest::Read32(base, def + 1468), Guest::Read32(base, def + 1460));
        }
        fflush(stdout);
    }
    {   // each campaign gun's clip (+1468) and ammunition (+1460) numbers, beside its copy's
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        for (int32_t k = 1; list && k <= count && k < 64; k++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
            if (!def) continue;
            const std::string name = ReadText(base, Guest::Read32(base, def + 8), 32);
            bool show = name == "kar98_sniper" || name == "kar98" || name == "m1garand";
            for (const CampaignGun& g : Guns) show = show || name == g.name;
            if (show) printf("weapons: %d %s: clip %u (%s) ammo %u, clip size %u\n", k, name.c_str(), Guest::Read32(base, def + 1468),
                             ReadText(base, Guest::Read32(base, def + 1464), 32).c_str(), Guest::Read32(base, def + 1460), Guest::Read32(base, def + 1476));
        }
        // DIAGNOSTIC: COD3_WEAPON_DIFF="a b": every word where two weapons differ
        if (const char* pair = getenv("COD3_WEAPON_DIFF"))
        {
            char an[32] = {}, bn[32] = {};
            sscanf(pair, "%31s %31s", an, bn);
            uint32_t a = 0, b = 0;
            for (int32_t k = 1; list && k <= count && k < 64; k++)
            {
                const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(k));
                if (!def) continue;
                const std::string name = ReadText(base, Guest::Read32(base, def + 8), 32);
                if (name == an) a = def; else if (name == bn) b = def;
            }
            for (uint32_t at = 0; a && b && at < 2400; at += 4)
            {
                const uint32_t x = Guest::Read32(base, a + at), y = Guest::Read32(base, b + at);
                if (x == y) continue;
                auto show = [&](uint32_t v) { return (v >= 0x40000000u && v < 0x90000000u) ? ReadText(base, v, 40) : std::string(); };
                printf("weapon diff: +%u %s %08X %s | %s %08X %s\n", at, an, x, show(x).c_str(), bn, y, show(y).c_str());
            }
        }
    }
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
    {
        const int32_t weapon = g_campaignWeapon[i];
        if (weapon <= 0) continue;
        const uint32_t item = 0x82C75A48u + 52u * uint32_t(weapon);
        printf("items: %d %s: hash %08X name %s model %s icon %s weapon %u\n", weapon, Guns[i].name, Guest::Read32(base, item),
               ReadText(base, Guest::Read32(base, item + 4)).c_str(), ReadText(base, Guest::Read32(base, item + 12)).c_str(),
               ReadText(base, Guest::Read32(base, item + 20)).c_str(), Guest::Read32(base, item + 40));
    }
    fflush(stdout);
}

// Sounds played by name (sub_82556868: the sound's name in r5, from the
// map's sound bank, mp\<map>\<map>.wbk). The campaign guns' reload
// animations ask for their campaign sounds (GR_STEN, GR_fg42, ...), which
// are only in the campaign levels' banks: those play the reload sound of the
// gun each is a copy of instead. Every name asked for is logged once.
namespace
{
    // The third name: the campaign bank's own, played instead when the
    // campaign banks are loaded into the sound engine (g_campaignBanksLoaded).
    struct SoundAlias { const char* campaign; const char* multiplayer; const char* own = nullptr; };
    const SoundAlias SoundAliases[] = {
        { "gr_g43", "GR_M1G", "GR_G43" },
        { "gewehr43_fire_1st", "M1GARAND_FIRE_1ST" },
        { "gewehr43_fire_3rd", "M1GARAND_FIRE_3RD" },
        { "gr_sten", "GR_THOMPSON", "GR_STEN" },
        { "gr_fg42", "GR_BAR", "GR_fg42" },
        { "gr_bren", "GR_BAR", "GR_BREN" },
        { "gr_enfield_reload", "GR_KAR98", "GR_enfield_reload" },
        { "gr_enfield_rechamber", "GF_KAR98_Rechamber", "GR_Enfield_rechamber" },
        { "sten_fire_1st", "THOMPSON_FIRE_1ST" },
        { "sten_fire_3rd", "THOMPSON_FIRE_3RD" },
        { "fg42_fire_1st", "BAR_FIRE_1ST" },
        { "fg42_fire_3rd", "BAR_FIRE_3RD" },
        { "bren_fire_1st", "BAR_FIRE_1ST" },
        { "bren_fire_3rd", "BAR_FIRE_3RD" },
        { "lee_enfield_fire_1st", "KAR98_FIRE_1ST" },
        { "lee_enfield_fire_3rd", "KAR98_FIRE_3RD" },
    };
}
extern "C" PPC_FUNC(__imp__sub_82556868);
PPC_FUNC(sub_82556868)
{
    if (ctx.r5.u32)
    {
        std::string text = ReadText(base, ctx.r5.u32, 64);
        if (SoundTimes()) { printf("sound times: %lld asked %s (from %08X)\n", SoundMs(), text.c_str(), uint32_t(ctx.lr)); fflush(stdout); }
        // 0.5.277: Justin hears the bots' reloads and gun handling as if his
        // own (12v12 and 32, solo and two-PC): every reload-like sound with
        // whose entity it is played at, the player it belongs to, and how
        // far from this machine's player (300 lines).
        {
            std::string up = text;
            for (char& c : up) c = char(toupper(static_cast<unsigned char>(c)));
            if (up.rfind("GR_", 0) == 0 || up.rfind("GF_", 0) == 0 || up.find("RELOAD") != std::string::npos || up.find("RECHAMBER") != std::string::npos || up.find("BOLT") != std::string::npos)
            {
                static int said = 0;
                if (said < 300)
                {
                    said++;
                    const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
                    const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
                    const uint32_t e = ctx.r4.u32;
                    int index = -1;
                    for (uint32_t i = 0; list && i < MpCapClients; i++) if (Guest::Read32(base, list + 4 * (i + 1)) == e) index = int(i);
                    float d = -1.0f;
                    const uint32_t eps = e >= 0x40000000u && e < 0x90000000u ? Guest::Read32(base, e + 512) : 0;
                    const uint32_t ops = own ? Guest::Read32(base, own + 512) : 0;
                    if (eps >= 0x40000000u && eps < 0x90000000u && ops >= 0x40000000u && ops < 0x90000000u)
                    {
                        float a[3], b[3];
                        for (int k = 0; k < 3; k++) { uint32_t w = Guest::Read32(base, eps + 4 * k); memcpy(&a[k], &w, 4); w = Guest::Read32(base, ops + 4 * k); memcpy(&b[k], &w, 4); }
                        d = sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
                    }
                    printf("reload sounds: %s at entity %08X (player %d%s, %.0f away; +484 %08X +480 %08X; mine +484 %08X) r6 %u r7 %d r8 %u from %08X\n",
                           text.c_str(), e, index, e == own ? " = me" : "", d,
                           e >= 0x40000000u && e < 0x90000000u ? Guest::Read32(base, e + 484) : 0, e >= 0x40000000u && e < 0x90000000u ? Guest::Read32(base, e + 480) : 0,
                           own ? Guest::Read32(base, own + 484) : 0, ctx.r6.u32, int32_t(ctx.r7.u32), ctx.r8.u32, uint32_t(ctx.lr));
                    fflush(stdout);
                }
            }
        }
        static std::set<std::string> seen;
        static std::mutex lock;
        {
            std::lock_guard<std::mutex> guard(lock);
            if (seen.size() < 400 && seen.insert(text).second)
            {
                printf("sounds: %s asked for (from %08X)\n", text.c_str(), uint32_t(ctx.lr));
                fflush(stdout);
            }
        }
        for (char& c : text) c = char(tolower(static_cast<unsigned char>(c)));
        const bool ownShots = Settings::Get().campaignSounds && Settings::Get().campaignShotSounds;
        for (const SoundAlias& alias : SoundAliases)
            if (text == alias.campaign && !(ownShots && strstr(alias.campaign, "_fire_")))
            {
                const bool own = alias.own && g_campaignBanksLoaded;
                static uint32_t names[2][sizeof(SoundAliases) / sizeof(SoundAliases[0])] = {};
                uint32_t& name = names[own ? 1 : 0][&alias - SoundAliases];
                if (!name) name = GuestText(base, own ? alias.own : alias.multiplayer);
                ctx.r5.u64 = name;
                break;
            }
    }
    __imp__sub_82556868(ctx, base);
}

// Diagnostics (0.5.44): every asset name looked up by name (sub_820B00C0
// makes an archive key from a name: models, effects, materials, ...), once
// each, and the HUD's image lookups (sub_824EE308: a list of the menus'
// image objects searched by name; not found, the default's), with the
// objects found for hud_ names described.
extern "C" PPC_FUNC(__imp__sub_820B00C0);
PPC_FUNC(sub_820B00C0)
{
    if (ctx.r4.u32)
    {
        static std::set<std::string> seen;
        static std::mutex lock;
        const std::string name = ReadText(base, ctx.r4.u32, 64);
        std::lock_guard<std::mutex> guard(lock);
        if (seen.size() < 4000 && seen.insert(name).second) printf("assets: %s (from %08X)\n", name.c_str(), uint32_t(ctx.lr));
    }
    __imp__sub_820B00C0(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_824EE308);
PPC_FUNC(sub_824EE308)
{
    const uint32_t list = ctx.r3.u32;
    const std::string name = ctx.r4.u32 ? ReadText(base, ctx.r4.u32, 64) : std::string();
    __imp__sub_824EE308(ctx, base);
    static std::set<std::string> seen;
    static std::mutex lock;
    std::lock_guard<std::mutex> guard(lock);
    if (seen.size() > 400 || !seen.insert(name + "@" + std::to_string(list)).second) return;
    const uint32_t found = ctx.r3.u32;
    const uint32_t fallback = Guest::Read32(base, 0x82C6B1ECu + 32u);
    printf("hud images: %s in list %08X (%u images) -> %08X%s\n", name.c_str(), list, list ? Guest::Read32(base, list + 8) : 0, found,
           found == fallback ? " (the default)" : "");
    if (name.rfind("hud_", 0) == 0 && found >= 0x40000000u && found < 0x90000000u)
    {
        auto words = [&](uint32_t at, int count)
        {
            std::string line; char text[16];
            for (int i = 0; i < count; i++) { snprintf(text, sizeof(text), " %08X", Guest::Read32(base, at + 4u * uint32_t(i))); line += text; }
            return line;
        };
        printf("hud images:   object%s\n", words(found, 48).c_str());
        const uint32_t record = Guest::Read32(base, found + 72);
        if (record >= 0x40000000u && record < 0x90000000u)
            printf("hud images:   +72 record %08X%s \"%s\"\n", record, words(record, 40).c_str(), ReadText(base, record + 12, 40).c_str());
        for (int i = 0; i < 48; i++)
        {
            const uint32_t to = Guest::Read32(base, found + 4u * uint32_t(i));
            if (i == 18 || to < 0x40000000u || to >= 0x90000000u || to == found) continue;
            printf("hud images:   +%d -> %08X%s\n", i * 4, to, words(to, 16).c_str());
        }
    }
    fflush(stdout);
}

// The HUD's images (sub_824784B8: an archive key r3, the asset's type r4,
// "TEX" 0x00584554 for an image): looked for in one archive only (the
// current one, sub_8246F1A0), else the default image. The pick-up prompt's
// gun icons are images by the weapon's icon name (sub_825026A8): the
// campaign guns' (hud_sten_smg, ...) are in the campaign weapons archives,
// so they came back as the default (a hand, or a white box). Not found,
// an image is looked for in the campaign archives too.
extern "C" PPC_FUNC(__imp__sub_824784B8);
PPC_FUNC(sub_824784B8)
{
    PPCContext saved = ctx;
    __imp__sub_824784B8(ctx, base);
    if (saved.r4.u32 != 0x00584554u || !Settings::Get().campaignIcons) return;
    const uint32_t fallback = Guest::Read32(base, 0x82A8C390);
    if (ctx.r3.u32 != fallback && ctx.r3.u32 != 0) return;
    for (const int32_t archive : g_archiveActive)
    {
        if (archive < 0) continue;
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x400) & ~0xFu;
        call.r3.u64 = uint32_t(archive);
        call.r4.u64 = saved.r3.u32;
        call.lr = 0;
        Guest::Lookup(0x82477E70)(call, base);
        if (call.r3.u32 != 0)
        {
            static int said = 0;
            if ((said < 40 && said++ < 40))
            {
                std::string key;
                for (uint32_t i = 4; i < 32; i++) { const char c = char(Guest::Read8(base, saved.r3.u32 + i)); if (!c) break; key += c; }
                printf("hud images: image %s found in campaign archive %d -> %08X\n", key.c_str(), archive, call.r3.u32);
                fflush(stdout);
            }
            ctx.r3.u64 = call.r3.u32;
            return;
        }
    }
}

// The HUD's images made from the menus (sub_82503EB8: an image entry r30,
// its name at +4, its image at +0 from sub_825026A8). These are made at
// the front end, before the campaign archives are loaded: the Sten's
// (hud_sten_smg, in the list) got the default image (the white box). Each
// entry is logged once; the campaign icons' are kept, and given their
// images once the campaign archives are in (after the items are made).
namespace
{
    std::vector<std::pair<uint32_t, std::string>> g_iconEntries;
    std::mutex g_iconLock;
    std::string Lower(std::string text) { for (char& c : text) c = char(tolower(static_cast<unsigned char>(c))); return text; }
    bool IsCampaignIcon(const std::string& name)
    {
        std::string bare = Lower(name);
        if (bare.size() > 4 && bare.compare(bare.size() - 4, 4, ".tga") == 0) bare.resize(bare.size() - 4);
        for (const CampaignGun& gun : Guns) if (bare == Lower(gun.hud)) return true;
        return false;
    }
    void ResolveCampaignIcons(PPCContext& ctx, uint8_t* base)
    {
        std::lock_guard<std::mutex> guard(g_iconLock);
        for (const auto& [entry, name] : g_iconEntries)
        {
            std::string bare = name;
            if (bare.size() > 4 && Lower(bare).compare(bare.size() - 4, 4, ".tga") == 0) bare.resize(bare.size() - 4);
            for (const int32_t archive : g_archiveActive)
            {
                if (archive < 0) continue;
                PPCContext call = ctx;
                call.r1.u32 = (ctx.r1.u32 - 0x800) & ~0xFu;
                const uint32_t text = call.r1.u32 + 0x200, key = call.r1.u32 + 0x300;
                for (uint32_t i = 0; i < 0x100; i += 4) Guest::Write32(base, text + i, 0);
                for (size_t i = 0; i < bare.size() && i < 120; i++) Guest::Write8(base, text + uint32_t(i), uint8_t(bare[i]));
                call.r3.u64 = key; call.r4.u64 = text; call.lr = 0;
                Guest::Lookup(0x820B00C0)(call, base);
                call.r3.u64 = uint32_t(archive); call.r4.u64 = key; call.lr = 0;
                Guest::Lookup(0x82477E70)(call, base);
                if (call.r3.u32 != 0)
                {
                    printf("hud images: %s's entry %08X: image %08X (was %08X), campaign archive %d\n", name.c_str(), entry, call.r3.u32,
                           Guest::Read32(base, entry), archive);
                    Guest::Write32(base, entry, call.r3.u32);
                    break;
                }
            }
        }
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82503EB8);
PPC_FUNC(sub_82503EB8)
{
    const uint32_t entry = ctx.r4.u32;
    __imp__sub_82503EB8(ctx, base);
    const uint32_t namePointer = Guest::Read32(base, entry + 4);
    if (namePointer < 0x40000000u || namePointer >= 0x90000000u) return;
    const std::string name = ReadText(base, namePointer, 64);
    static std::set<std::string> seen;
    std::lock_guard<std::mutex> guard(g_iconLock);
    if (seen.size() < 600 && seen.insert(name).second && name.rfind("hud_", 0) == 0)
    { printf("hud images: menu image %s entry %08X image %08X\n", name.c_str(), entry, Guest::Read32(base, entry)); fflush(stdout); }
    if (IsCampaignIcon(name)) g_iconEntries.emplace_back(entry, name);
}

// The HUD's weapon icons (sub_824EF868: per weapon i, its icon's image
// object from the menus' image lists at +52 + 4i of r3, 0 when none): the
// campaign guns' logged.
extern "C" PPC_FUNC(__imp__sub_824EF868);
extern "C" PPC_FUNC(__imp__sub_824EE308);
// The HUD's weapon icons (sub_824EF868: per weapon w, the image object of
// its icon from the menus' image lists, at +52 + 4w of r3; looked for by
// name in r5's list, then by name.tga in r4's; none, 0: the hand). The
// menus' list has the Sten's object (hud_sten_smg, with no image: the white
// box) but none for the others. Each campaign gun with no object gets one
// of the list's objects no multiplayer gun uses (the M1 Carbine's, the
// G43's, the MG34's (ge_), the Thompson's full auto), and each gets its own
// image (the campaign archives' hud_..., set on the object's parts at +100,
// as the object's own image setting does, sub_824E13F0).
PPC_FUNC(sub_824EF868)
{
    PPCContext saved = ctx;
    const uint32_t table = ctx.r3.u32, nameList = ctx.r5.u32, tgaList = ctx.r4.u32;
    __imp__sub_824EF868(ctx, base);
    if (!Settings::Get().campaignIcons) return;
    const uint32_t fallback = Guest::Read32(base, 0x82C6B1ECu + 32u);
    auto find = [&](uint32_t list, const std::string& name) -> uint32_t
    {
        if (!list) return 0;
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x800) & ~0xFu;
        const uint32_t text = call.r1.u32 + 0x200;
        for (uint32_t i = 0; i < 0x80; i += 4) Guest::Write32(base, text + i, 0);
        for (size_t i = 0; i < name.size() && i < 120; i++) Guest::Write8(base, text + uint32_t(i), uint8_t(name[i]));
        call.r3.u64 = list; call.r4.u64 = text; call.lr = 0;
        __imp__sub_824EE308(call, base);
        return call.r3.u32 == fallback ? 0 : call.r3.u32;
    };
    auto image = [&](const std::string& name) -> uint32_t
    {
        for (const int32_t archive : g_archiveActive)
        {
            if (archive < 0) continue;
            PPCContext call = saved;
            call.r1.u32 = (saved.r1.u32 - 0x800) & ~0xFu;
            const uint32_t text = call.r1.u32 + 0x200, key = call.r1.u32 + 0x300;
            for (uint32_t i = 0; i < 0x80; i += 4) Guest::Write32(base, text + i, 0);
            for (size_t i = 0; i < name.size() && i < 120; i++) Guest::Write8(base, text + uint32_t(i), uint8_t(name[i]));
            call.r3.u64 = key; call.r4.u64 = text; call.lr = 0;
            Guest::Lookup(0x820B00C0)(call, base);
            call.r3.u64 = uint32_t(archive); call.r4.u64 = key; call.lr = 0;
            Guest::Lookup(0x82477E70)(call, base);
            if (call.r3.u32) return call.r3.u32;
        }
        return 0;
    };
    // The Sten's icon object (hud_sten_smg, in the menus' list) is the
    // pattern: a gun with none gets its own copy of it (the object, its
    // parts list and its parts, 104 bytes each, from the game's allocator
    // sub_820DE6D8), with its own image on its parts. Not found, the list
    // gives its default object (the hand), which is never changed.
    const uint32_t hand = find(nameList, "\x01none");
    auto allocate = [&](uint32_t bytes) -> uint32_t
    {
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x800) & ~0xFu;
        call.r3.u64 = bytes; call.lr = 0;
        Guest::Lookup(0x820DE6D8)(call, base);
        return call.r3.u32;
    };
    uint32_t pattern = 0;
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
    {
        const int32_t weapon = g_campaignWeapon[i];
        const uint32_t object = weapon > 0 ? Guest::Read32(base, table + 52 + 4 * uint32_t(weapon)) : 0;
        if (object && object != hand && Guest::Read32(base, object + 40) > 0) { pattern = object; break; }
    }
    for (size_t i = 0; i < sizeof(Guns) / sizeof(Guns[0]); i++)
    {
        const int32_t weapon = g_campaignWeapon[i];
        if (weapon <= 0) continue;
        const uint32_t slot = table + 52 + 4 * uint32_t(weapon);
        uint32_t object = Guest::Read32(base, slot);
        const uint32_t picture = image(Guns[i].hud);
        const char* given = "its own";
        if ((!object || object == hand) && pattern && picture)
        {
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, pattern + 40), 8);
            const uint32_t copy = allocate(0x100), list = allocate(4 * count + 4);
            if (copy && list)
            {
                for (uint32_t k = 0; k < 0x100; k += 4) Guest::Write32(base, copy + k, Guest::Read32(base, pattern + k));
                const uint32_t from = Guest::Read32(base, pattern + 32);
                for (uint32_t k = 0; k < count; k++)
                {
                    const uint32_t part = allocate(104), was = Guest::Read32(base, from + 4 * k);
                    if (part && was) for (uint32_t b = 0; b < 104; b += 4) Guest::Write32(base, part + b, Guest::Read32(base, was + b));
                    Guest::Write32(base, list + 4 * k, part);
                }
                Guest::Write32(base, copy + 32, list);
                object = copy;
                Guest::Write32(base, slot, object);
                given = "a copy of the Sten's";
            }
        }
        uint32_t parts = 0;
        if (object && object != hand && picture)
        {
            parts = Guest::Read32(base, object + 40);
            const uint32_t array = Guest::Read32(base, object + 32);
            for (uint32_t k = 0; k < parts && k < 8 && array; k++)
            {
                const uint32_t part = Guest::Read32(base, array + 4 * k);
                if (part) Guest::Write32(base, part + 100, picture);
            }
        }
        printf("hud images: weapon %d %s: icon object %08X (%s; the hand %08X), image %s %08X on %u part(s)\n", weapon, Guns[i].name, object,
               given, hand, Guns[i].hud, picture, parts);
    }
    fflush(stdout);
}
void ResolveCampaignIconsLater(PPCContext& ctx, uint8_t* base) { if (Settings::Get().campaignIcons) ResolveCampaignIcons(ctx, base); }

namespace
{   // the campaign gun whose event is being made (sub_82554BA0, below)
    thread_local const CampaignGun* t_eventGun = nullptr;
    thread_local const char* t_cueOverride = nullptr;   // every cue of the event made this one
}
// The sound engine's cue lookups (0.5.47 diagnostics): by name in every
// bank (sub_8212B268, the name r3), by hash (sub_8212B300, r3), by a hash
// at r3 (sub_8212AF78): each asked-for one once, with who asked.
namespace
{
    void LogCue(const char* how, const std::string& what, uint32_t from, int32_t result)
    {
        if (SoundTimes()) { printf("sound times: %lld cue %s %s -> %d\n", SoundMs(), how, what.c_str(), result); fflush(stdout); }
        static std::set<std::string> seen;
        static std::mutex lock;
        std::lock_guard<std::mutex> guard(lock);
        if (seen.size() < 1500 && seen.insert(what).second)
        { printf("cues: %s %s (from %08X) -> %d\n", how, what.c_str(), from, result); fflush(stdout); }
    }
}
extern "C" PPC_FUNC(__imp__sub_8212B268);
// The cue look-up by name (sub_8212B268) searches the slots in order, and the
// campaign banks, queued before the map's, would answer first for the names
// both have (THOMPSON_FIRE_1ST is in forest.wbk too): multiplayer's own guns
// would play the campaign's sounds. With campaign banks loaded, the same
// search in two passes: every other slot first, then the campaign banks'
// (what only they have: STEN_FIRE_1ST, GEWEHR43_FIRE_1ST, ...).
namespace
{
    void OrderedCueLookup(PPCContext& ctx, uint8_t* base)
    {
        const uint64_t campaign = g_campaignSlots.load();
        if (!g_campaignBanksLoaded || campaign == 0 || ctx.r3.u32 == 0) { __imp__sub_8212B268(ctx, base); return; }
        const uint32_t name = ctx.r3.u32;
        const uint32_t count = std::min<uint32_t>(Guest::Read32(base, 0x825B1838u), 64);
        const uint32_t table = Guest::Read32(base, 0x82A89918u);
        for (int pass = 0; pass < 2 && table; pass++)
            for (uint32_t i = 0; i < count; i++)
            {
                if (((campaign >> i) & 1) != uint64_t(pass)) continue;
                const uint32_t e = table + 52 * i;
                if (Guest::Read32(base, e + 4) != 3) continue;
                PPCContext call = ctx;
                call.r3.u64 = Guest::Read32(base, e + 24);
                call.r4.u64 = name;
                Guest::Lookup(0x8212B170)(call, base);
                if (int32_t(call.r3.u32) == -1) continue;
                ctx.r3.u64 = (Guest::Read32(base, e) & 0xFFFF0000u) | call.r3.u32;
                return;
            }
        ctx.r3.u64 = uint64_t(int64_t(-1));
    }
}
PPC_FUNC(sub_8212B268)
{
    // The copy's own cue for a campaign gun's swapped one, played when the
    // campaign cue is not found: the campaign banks loaded beside a map do not
    // answer these name look-ups (0.5.900's probe: STEN_FIRE_1ST, GR_STEN,
    // GEWEHR43_FIRE_1ST and GR_G43 all -1; the map's M1GARAND_FIRE_1ST found),
    // and a gun whose shot is not found plays only the rifle ring-off: quiet,
    // far off, and nothing at all on the last shot.
    uint32_t fallback = 0, fallback2 = 0;
    if (t_cueOverride && ctx.r3.u32)
    {
        static std::map<std::string, uint32_t> texts;
        static std::mutex lock;
        std::lock_guard<std::mutex> guard(lock);
        uint32_t& t = texts[t_cueOverride];
        if (!t) t = GuestText(base, t_cueOverride);
        fallback = ctx.r3.u32;   // the event's own cue, should the override be missing
        ctx.r3.u64 = t;
    }
    else if (t_eventGun && ctx.r3.u32)
    {
        const CampaignGun& gun = *t_eventGun;
        const std::string copy = CampaignCopyShotPrefix(gun.from);
        const std::string cue = ReadText(base, ctx.r3.u32, 64);
        std::string upper = cue;
        for (char& c : upper) c = char(toupper(static_cast<unsigned char>(c)));
        const std::string prefix = copy + "_FIRE_";
        if (upper.rfind(prefix, 0) == 0)
        {
            static std::map<std::string, uint32_t> swapped;
            static std::mutex lock;
            const std::string own = CampaignShotCue(gun.name, gun.shot, CampaignShotSuffix(gun.name, upper.substr(prefix.size())));
            uint32_t text;
            {
                std::lock_guard<std::mutex> guard(lock);
                uint32_t& t = swapped[own];
                if (!t) t = GuestText(base, own);
                text = t;
            }
            static int said = 0;
            if ((said < 30 && said++ < 30)) { printf("cues: %s's shot %s -> %s\n", gun.name, cue.c_str(), own.c_str()); fflush(stdout); }
            // The copy's, with the campaign gun's suffix (the G43's last shot:
            // M1GARAND_FIRE_1ST, not the Garand's clip-ping _1ST_CLIP).
            // The MG42's: the multiplayer Horch turret's same sound first
            // (MG42_HORCH_FIRE_1ST, in some maps' banks), then the MG34's own.
            const bool mg42 = strcmp(gun.name, "mg42") == 0;
            const std::string copyCue = copy + "_FIRE_" + (mg42 ? upper.substr(prefix.size()) : CampaignShotSuffix(gun.name, upper.substr(prefix.size())));
            {
                std::lock_guard<std::mutex> guard(lock);
                uint32_t& t = swapped[copyCue];
                if (!t) t = GuestText(base, copyCue);
                fallback = t;
                if (mg42)
                {
                    const std::string horch = "MG42_HORCH_FIRE_" + upper.substr(prefix.size());
                    uint32_t& h = swapped[horch];
                    if (!h) h = GuestText(base, horch);
                    fallback2 = h;
                }
            }
            ctx.r3.u64 = text;
        }
        // Its reload: the copy's reload cue becomes the gun's own.
        const std::string ownReload = CampaignReloadCue(gun.name, upper);
        if (!ownReload.empty())
        {
            static std::map<std::string, uint32_t> texts;
            static std::mutex textLock;
            uint32_t text;
            {
                std::lock_guard<std::mutex> guard(textLock);
                uint32_t& t = texts[ownReload];
                if (!t) t = GuestText(base, ownReload);
                text = t;
            }
            static int said = 0;
            if ((said < 10 && said++ < 10)) { printf("cues: %s's reload %s -> %s\n", gun.name, cue.c_str(), ownReload.c_str()); fflush(stdout); }
            fallback = ctx.r3.u32;   // the copy's reload cue, as asked
            ctx.r3.u64 = text;
        }
    }
    const std::string name = ctx.r3.u32 ? ReadText(base, ctx.r3.u32, 64) : std::string();
    const uint32_t from = uint32_t(ctx.lr);
    const PPCContext before = ctx;
    OrderedCueLookup(ctx, base);
    LogCue("name", name, from, int32_t(ctx.r3.u32));
    if (getenv("COD3_RELOAD_SHOTS") && name.rfind("GR_", 0) == 0) g_reloadShotsFrom = SoundMs();
    if (int32_t(ctx.r3.u32) == -1 && fallback2 != 0)
    {
        PPCContext again = before;
        again.r3.u64 = fallback2;
        OrderedCueLookup(again, base);
        if (int32_t(again.r3.u32) != -1) { ctx.r3.u64 = again.r3.u64; return; }
    }
    if (int32_t(ctx.r3.u32) == -1 && fallback != 0)
    {
        PPCContext again = before;
        again.r3.u64 = fallback;
        OrderedCueLookup(again, base);
        static int said = 0;
        if ((said < 20 && said++ < 20)) { printf("cues: %s not found; the copy's %s instead -> %d\n", name.c_str(), ReadText(base, fallback, 64).c_str(), int32_t(again.r3.u32)); fflush(stdout); }
        ctx.r3.u64 = again.r3.u64;
    }
}
extern "C" PPC_FUNC(__imp__sub_8212B300);
PPC_FUNC(sub_8212B300)
{
    char text[16]; snprintf(text, sizeof(text), "%08X", ctx.r3.u32);
    const uint32_t from = uint32_t(ctx.lr);
    __imp__sub_8212B300(ctx, base);
    LogCue("hash", text, from, int32_t(ctx.r3.u32));
}
extern "C" PPC_FUNC(__imp__sub_8212AF78);
PPC_FUNC(sub_8212AF78)
{
    char text[16]; snprintf(text, sizeof(text), "%08X", ctx.r3.u32 ? Guest::Read32(base, ctx.r3.u32) : 0);
    const uint32_t from = uint32_t(ctx.lr);
    __imp__sub_8212AF78(ctx, base);
    LogCue("key", text, from, int32_t(ctx.r3.u32));
}

// The sounds of a weapon's events (sub_82554BA0: the event's name r4,
// looked up in the map archive's "event" table (sub_82490220), whose
// entries give the cue names, THOMPSON_FIRE_1ST, ...; the cue then found
// by name in the banks, sub_8212B268). The table has only the multiplayer
// guns: a campaign gun's event (its shots keep its own name, events 3, 7
// and 53) found nothing, silent. Its name is looked up as the copy's
// (sten as thompson), and while that event is made the copy's gunshot
// cues become the campaign gun's own (THOMPSON_FIRE_1ST: STEN_FIRE_1ST, in
// the campaign banks).
extern "C" PPC_FUNC(__imp__sub_82554BA0);
PPC_FUNC(sub_82554BA0)
{
    // The event's name r4 (the copy's: shots keep it, for the table and
    // the muzzle flash), the message r5 (+496 the shooter's sound handle).
    if (SoundTimes() && ctx.r4.u32) { printf("sound times: %lld event %s (r5 %08X)\n", SoundMs(), ReadText(base, ctx.r4.u32, 64).c_str(), ctx.r5.u32); fflush(stdout); }
    // 0.5.278: the reloads Justin hears as his own: each reload-like event
    // here (the sound code: the event's name r4, its message r5, +496 the
    // sound handle of whose it is) with the player it belongs to and how
    // far, and the message's words that say how it is played (+0, +8, +432,
    // +500, +504, +524, +528). 277's trace at sub_82556868 caught none:
    // reloads come from the animations' events, not by name.
    if (ctx.r4.u32 && ctx.r5.u32)
    {
        const std::string event = ReadText(base, ctx.r4.u32, 64);
        std::string low = event;
        for (char& c : low) c = char(tolower(static_cast<unsigned char>(c)));
        static int said = 0;
        if (said < 300 && (low.find("reload") != std::string::npos || low.find("rechamber") != std::string::npos || low.find("_gr_") != std::string::npos || low.rfind("gr_", 0) == 0))
        {
            said++;
            const uint32_t m = ctx.r5.u32;
            const uint32_t handle = Guest::Read32(base, m + 496);
            const uint32_t slotIndex = handle & 0x3FFF;
            uint32_t ent = 0;
            if (slotIndex < 4096 && Guest::Read32(base, 0x82BD1FC0 + 8 * slotIndex + 4) == (handle >> 14)) ent = Guest::Read32(base, 0x82BD1FC0 + 8 * slotIndex);
            const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
            const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
            int index = -1;
            for (uint32_t i = 0; list && ent && i < MpCapClients; i++) if (Guest::Read32(base, list + 4 * (i + 1)) == ent) index = int(i);
            float d = -1.0f;
            const uint32_t eps = ent ? Guest::Read32(base, ent + 512) : 0, ops = own ? Guest::Read32(base, own + 512) : 0;
            if (eps >= 0x40000000u && eps < 0x90000000u && ops >= 0x40000000u && ops < 0x90000000u)
            {
                float a[2], b[2];
                for (int k = 0; k < 2; k++) { uint32_t w = Guest::Read32(base, eps + 4 * k); memcpy(&a[k], &w, 4); w = Guest::Read32(base, ops + 4 * k); memcpy(&b[k], &w, 4); }
                d = sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]));
            }
            printf("reload sounds: event %s for handle %08X: entity %08X (player %d%s, %.0f away) | msg +0 %08X +8 %08X +432 %08X +500 %08X +504 %08X +524 %08X +528 %04X | from %08X\n",
                   event.c_str(), handle, ent, index, ent && ent == own ? " = me" : "", d,
                   Guest::Read32(base, m), Guest::Read32(base, m + 8), Guest::Read32(base, m + 432), Guest::Read32(base, m + 500), Guest::Read32(base, m + 504),
                   Guest::Read32(base, m + 524), Guest::Read16(base, m + 528), uint32_t(ctx.lr));
            fflush(stdout);
        }
    }
    // 0.5.279: the fix. 278's trace: a bot's reload is the very same event
    // as this machine's player's (weapon_shotgun_weapon_rechamber for player
    // 7, 1600 away: the same message, +0 00000501, +432 4), played as a
    // first-person sound: the bots are local players here (local index 3),
    // so their guns' handling is played as the player's own. A gun-handling
    // event (reload, rechamber, raise, drop, bolt, aim) of any other
    // player is let go: not heard at all, instead of in your own ears.
    if (ctx.r4.u32 && ctx.r5.u32 && !getenv("COD3_OTHERS_RELOADS"))
    {
        const std::string event = ReadText(base, ctx.r4.u32, 64);
        if (event.rfind("weapon_", 0) == 0 && event.find("_fire") == std::string::npos &&
            (event.find("reload") != std::string::npos || event.find("rechamber") != std::string::npos || event.find("raise") != std::string::npos ||
             event.find("drop") != std::string::npos || event.find("bolt") != std::string::npos || event.find("_ads") != std::string::npos ||
             event.find("pullout") != std::string::npos || event.find("putaway") != std::string::npos || event.find("clip") != std::string::npos))
        {
            const uint32_t handle = Guest::Read32(base, ctx.r5.u32 + 496);
            const uint32_t slotIndex = handle & 0x3FFF;
            const uint32_t ent = slotIndex < 4096 && Guest::Read32(base, 0x82BD1FC0 + 8 * slotIndex + 4) == (handle >> 14) ? Guest::Read32(base, 0x82BD1FC0 + 8 * slotIndex) : 0;
            const uint32_t list = Guest::Read32(base, 0x82BCC7FC);
            const uint32_t own = list ? Guest::Read32(base, list + 4) : 0;
            bool player = false;
            for (uint32_t i = 1; list && ent && i < MpCapClients; i++) if (Guest::Read32(base, list + 4 * (i + 1)) == ent) player = true;
            if (player && ent != own)
            {
                static int said = 0;
                if ((said < 20 && said++ < 20)) { printf("reload sounds: %s of another player not played (first person)\n", event.c_str()); fflush(stdout); }
                ctx.r3.u64 = 0;
                return;
            }
        }
    }
    const CampaignGun* gun = nullptr;
    // The scoped Lee-Enfield's own shot, sent by name at its shot (event 13,
    // CampaignEventName): no event has its name, so it is made as the
    // Kar98's shot or ring-off event, and the cue swapped for its own:
    // KAR98_FIRE_1ST becomes Lee_Enfield_Sniper_FIRE_1ST (CampaignShotCue),
    // the ring-off the sniper's (SniperRingoff_1p / _3p).
    const char* cueOverride = nullptr;
    // 0.5.270: the UAV plane's sounds (ARPL_P47_*, the campaign's P-47, in
    // sp\mace2\mace2.wbk; and any other campaign plane cue): a name sent to
    // sub_82556868 is an EVENT name, looked up in the map's event table,
    // which has no plane: 258-269's flybys were dropped there (no cue was
    // ever looked up, 268's log). Made as the Kar98's third-person shot
    // event (in every map's table, played where the entity is) with every
    // cue of it the plane's, as the Lee-Enfield's shot is (heard, 266).
    if (ctx.r4.u32)
    {
        const std::string sent = ReadText(base, ctx.r4.u32, 64);
        if (sent.rfind("ARPL_", 0) == 0 || sent.rfind("UAV:", 0) == 0)
        {
            static std::set<std::string> kept;
            static std::mutex keptLock;
            static uint32_t eventText = 0;
            {
                std::lock_guard<std::mutex> guard(keptLock);
                cueOverride = kept.insert(sent.rfind("UAV:", 0) == 0 ? sent.substr(4) : sent).first->c_str();
                if (!eventText) eventText = GuestText(base, "weapon_kar98_weapon_fire_3rd");
            }
            ctx.r4.u64 = eventText;
            static int said = 0;
            if ((said < 30 && said++ < 30)) { printf("uav plane: sound %s made as the Kar98's shot event, its cue swapped\n", cueOverride); fflush(stdout); }
            const CampaignGun* outer = t_eventGun;
            const char* outerCue = t_cueOverride;
            t_eventGun = nullptr;
            t_cueOverride = cueOverride;
            __imp__sub_82554BA0(ctx, base);
            t_eventGun = outer;
            t_cueOverride = outerCue;
            return;
        }
    }
    if (ctx.r4.u32 && Settings::Get().campaignSounds && Settings::Get().campaignShotSounds)
    {
        const std::string sent = ReadText(base, ctx.r4.u32, 64);
        const char* as = nullptr;
        if (sent == "Lee_Enfield_Sniper_FIRE_1ST") as = "weapon_kar98_weapon_fire_1st";
        else if (sent == "LEE_ENFIELD_FIRE_3RD") as = "weapon_kar98_weapon_fire_3rd";
        // its ring-off: the Kar98's shot event's own (Rifle_Ringoff) made the sniper's
        else if ((sent == "weapon_kar98_weapon_fire_1st_ringoff" || sent == "weapon_kar98_weapon_fire_3rd_ringoff") && ctx.r5.u32)
        {
            const CampaignGun* fired = ShooterGun(Guest::Read32(base, ctx.r5.u32 + 496));
            // None (session 4): the campaign shot already rings off, and the
            // sniper's ring-off on top of it was heard as a second shot (hand
            // test). COD3_ENFIELD_RINGOFF=1 brings it back.
            if (fired && strcmp(fired->name, "lee_enfield_sniper") == 0)
            {
                if (!getenv("COD3_ENFIELD_RINGOFF")) { ctx.r3.u64 = 0; return; }
                cueOverride = sent.find("1st") != std::string::npos ? "SniperRingoff_1p" : "SniperRingoff_3p";
            }
        }
        if (as)
        {
            static std::map<std::string, uint32_t> texts;
            static std::mutex lock;
            uint32_t text;
            { std::lock_guard<std::mutex> guard(lock); uint32_t& t = texts[as]; if (!t) t = GuestText(base, as); text = t; }
            ctx.r4.u64 = text;
            for (const CampaignGun& g : Guns) if (strcmp(g.name, "lee_enfield_sniper") == 0) gun = &g;
        }
    }
    if (!gun && ctx.r4.u32 && ctx.r5.u32 && Settings::Get().campaignSounds && Settings::Get().campaignShotSounds)
    {
        const CampaignGun* fired = ShooterGun(Guest::Read32(base, ctx.r5.u32 + 496));
        if (fired)
        {
            const std::string name = Lower(ReadText(base, ctx.r4.u32, 64));
            const std::string copy = fired->from;
            // The G43 has no Garand en-bloc clip to eject or ping.
            if (G43ClipEjectEvent(fired->name, name)) { ctx.r3.u64 = 0; return; }
            // The scoped Lee-Enfield's shot is its own (event 13, above): the
            // scoped Kar98's gunshot and ring-off events, which its shots also
            // send since its fire delay is 0 (session 4), would play the
            // Springfield's shot 0.1 s before it: the double shot heard.
            if (strcmp(fired->name, "lee_enfield_sniper") == 0 && name.rfind("weapon_kar98_sniper_weapon_fire_", 0) == 0) { ctx.r3.u64 = 0; return; }
            if ((name.rfind(copy, 0) == 0 && (name.size() == copy.size() || !isalnum(static_cast<unsigned char>(name[copy.size()])))) ||
                CampaignDonorEvent(copy, name))
                gun = fired;
            static std::set<std::string> seen;
            static std::mutex lock;
            std::lock_guard<std::mutex> guard(lock);
            if (seen.size() < 60 && seen.insert(name).second) { printf("cues: event %s by a %s shooter%s\n", name.c_str(), fired->name, gun ? "" : " (not its gun's)"); fflush(stdout); }
        }
    }
    const CampaignGun* outer = t_eventGun;
    const char* outerCue = t_cueOverride;
    t_eventGun = gun;
    t_cueOverride = cueOverride;
    __imp__sub_82554BA0(ctx, base);
    t_eventGun = outer;
    t_cueOverride = outerCue;
}

// Text keys the game has no text for (sub_8254D180: the string table r3, the
// key r4, the text back): the campaign guns' own, given here.
extern "C" PPC_FUNC(__imp__sub_8254D180);
PPC_FUNC(sub_8254D180)
{
    if (ctx.r4.u32)
    {
        static const std::pair<const char*, const char*> Texts[] = { { "WEAPON_GEWEHR43", "Gewehr 43" }, { "WEAPON_MG42", "MG42" }, { "WEAPON_LEE_ENFIELD_SNIPER", "Lee-Enfield Scoped" }, { "WEAPON_FG42_SCOPED", "FG42 Scoped" }, { "WEAPON_MILLS_BOMB", "Mills Bomb" } };
        const std::string key = ReadText(base, ctx.r4.u32, 40);
        static uint32_t made[sizeof(Texts) / sizeof(Texts[0])] = {};
        for (size_t i = 0; i < sizeof(Texts) / sizeof(Texts[0]); i++)
            if (key == Texts[i].first)
            {
                uint32_t& guest = made[i];
                if (!guest) guest = GuestText(base, Texts[i].second);
                ctx.r3.u64 = guest;
                return;
            }
    }
    __imp__sub_8254D180(ctx, base);
}
// 0.5.86: the Kangaroo's treads in line. The tread bones are moved by how
// far each side has gone (sub_82434918, from the model's drawing: the left
// tread by the vehicle record's +1312, the right by its +1308, times 0.0045;
// only while it moves). The Kangaroo's record is the Horch's, and a four
// wheeler keeps something else at +1308: the right tread stood out of
// place. Each frame its right side's distance is kept the left side's.
void MpFixesKangarooTreads(uint8_t* base)
{
    (void)base;   // 0.5.87: the right side's distance is left alone again
}

// 0.5.87: 0.5.86's copy did nothing for it: the right tread still stood out
// of the hull, and the more so the further it drove. sub_82434918 (the
// entity r3, the tag r4, the tag list r5) gives a tread or gear tag a moved
// pose of its own while the vehicle drives; it returns 1 when it gave one.
// The Kangaroo's tag_tread_right isn't posed as the tanks' is, and a moved
// pose throws it out. For a Kangaroo that tag keeps the model's own pose
// (the call answered 0, as when standing still). Its left tread, gears and
// wheels go on as before. The tag's id is the game's own, kept at 82C9E59C.
// 0.5.88: the console's kangaroo_tread N picks how the right tread is moved,
// for finding the way that rolls it in place: 0 kept in place (0.5.87's),
// 1 the game's own, 2 to 5 the game's movement along -X, +Y, -Y and +Z of the
// tread's own bone instead of +X, 6 the left tread's own movement given to
// the right tread too.
// 0.5.99: 10 (the game's own tread and sprocket poses, with the wheel list
// put right) is the default: 0.5.98's real crash was the weapon look, not
// these poses, and Justin's test of 10 on 0.5.98 ran and rolled the treads.
namespace { std::atomic<int> g_treadMode{10}; }
void MpFixesKangarooTread(int mode)
{
    g_treadMode.store(mode < 0 ? 0 : mode > 10 ? 10 : mode);
    printf("kangaroo: right tread mode %d\n", g_treadMode.load()); fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_82434918);
PPC_FUNC(sub_82434918)
{
    const uint32_t entity = ctx.r3.u32, tag = ctx.r4.u32, list = ctx.r5.u32;
    const uint32_t right = Guest::Read32(base, 0x82C9E59Cu);
    const bool kangaroo = !g_kangarooEntities.empty() && g_kangarooEntities.count(entity) != 0;
    const bool kangarooRight = kangaroo && right != 0 && tag == right;
    const int mode = g_treadMode.load();
    // 0.5.88: Justin's video of 0.5.87: with the right tread held, the left
    // tread was thrown off whole, out past the front and over the top, and a
    // drive sprocket stood out of the hull at the back. The moved poses this
    // gives (made for the tanks' models) carry the Kangaroo's tread and gear
    // parts bodily. So in mode 0 none of its four tags (both treads, both
    // gears: the ids at 82C9E594..82C9E5A0) is given one.
    // 0.5.90: what the tests add up to. The left tread sits right only with
    // the game's pose: Justin's 0.5.85 (both posed: left right, right off),
    // 0.5.87's video (left posed, but only while moving: parked, it lay thrown
    // off), 0.5.88 and 0.5.89 (left never posed: always thrown off). The
    // right tread sits right only without one. The pose is given only while
    // the vehicle moves (its speed, entity +716, above nought), so the left
    // tread is now always given it: a parked Kangaroo's speed is made a hair
    // above nought for that one call.
    const uint32_t leftTread = Guest::Read32(base, 0x82C9E5A0u);
    if (kangaroo && mode == 8 && tag != 0 && tag == leftTread)
    {
        const uint32_t speedBits = Guest::Read32(base, entity + 716);
        float speed; memcpy(&speed, &speedBits, 4);
        if (!(speed > 0.0f)) { const float tiny = 0.001f; uint32_t bits; memcpy(&bits, &tiny, 4); Guest::Write32(base, entity + 716, bits); }
        __imp__sub_82434918(ctx, base);
        Guest::Write32(base, entity + 716, speedBits);
        static bool said = false;
        if (!said) { said = true; printf("kangaroo: left tread always posed, right tread and gears kept in place\n"); fflush(stdout); }
        return;
    }
    // 0.5.95: with the wheel list put right (0.5.93), the game's own tread
    // and sprocket poses are given again in mode 0: the treads' texture runs
    // with how far each side has gone (the v_tracked_tread shader reads the
    // tread bone's pose) and the sprockets turn, as on the campaign's tanks.
    // They were only ever thrown off by the Horch's wheel at bone 4. Mode 9
    // keeps all four still, as 0.5.93 did.
    // 0.5.97: 0.5.95's giving them in mode 0 crashed the game at the spawn
    // (Justin, 0.5.95 and 0.5.96), so mode 0 holds all four still again, as
    // 0.5.94 did, and mode 10 gives the game's poses with the wheel list put
    // right, for trying.
    if (kangaroo && mode != 10 && (mode == 0 || mode == 9))
    {
        const bool part = tag != 0 && (tag == right || tag == leftTread ||
                                       tag == Guest::Read32(base, 0x82C9E598u) || tag == Guest::Read32(base, 0x82C9E594u));
        if (part)
        {
            static bool said = false;
            if (!said) { said = true; printf("kangaroo: treads and gears kept in the model's own place\n"); fflush(stdout); }
            ctx.r3.u64 = 0;
            return;
        }
    }
    if (kangarooRight && mode == 6)
    {
        const uint32_t left = Guest::Read32(base, 0x82C9E5A0u);   // tag_tread_left's id
        if (left != 0) ctx.r4.u64 = left;
    }
    __imp__sub_82434918(ctx, base);
    if (!kangarooRight || mode < 2 || mode > 5 || ctx.r3.u32 == 0) return;
    // The pose it gave: an identity turn moved by (d, 0, 0), its pointer in
    // the tag list's slot (bone + 2) (the bone at 82A86B80, as the call had it).
    const uint32_t slots = list ? Guest::Read32(base, list) : 0;
    const uint32_t bone = Guest::Read32(base, 0x82A86B80u);
    const uint32_t matrix = slots ? Guest::Read32(base, slots + (bone + 2) * 4) : 0;
    if (matrix < 0x40000000u || matrix >= 0xA0000000u) return;
    uint32_t bits = Guest::Read32(base, matrix + 48);
    float d; memcpy(&d, &bits, 4);
    float moved[3] = { 0, 0, 0 };
    if (mode == 2) moved[0] = -d;
    else if (mode == 3) moved[1] = d;
    else if (mode == 4) moved[1] = -d;
    else moved[2] = d;
    for (int i = 0; i < 3; i++) { memcpy(&bits, &moved[i], 4); Guest::Write32(base, matrix + 48 + 4 * i, bits); }
    static int said = 0;
    if ((said < 3 && said++ < 3)) { printf("kangaroo: right tread (mode %d) moved %.2f, bone %u, pose at %08X\n", mode, d, bone, matrix); fflush(stdout); }
}

// 0.5.89: the other thing that throws the Kangaroo's tread off. Justin, on
// 0.5.88: with the tread mover off, the left tread is still thrown off the
// same way, and the wheels never turn. A vehicle's parts are posed each frame
// by sub_8242B938 (its physics object r3: the entity at +624, the flags at
// +640). With the flags' 0x10 (a tracked vehicle, the Sherman's kind the
// Kangaroo's is copied from) it calls sub_8241E908, which aims each tread
// between three of the vehicle's wheel points (+800..+820) and writes the
// tread's bone outright: on the Kangaroo, built from the Horch's record, those
// points aren't its own, and the tread is laid along the wrong line. Without
// 0x10 it poses each wheel instead (the bones at +36.., sub_8241D168: the
// turning and the springs), as for the jeeps.
// kangaroo_tread 0 (the default): the Kangaroo posed as a wheeled vehicle:
// its wheels turn, its treads stay where its model has them. 1: the game's
// own. 7: the tank's way, with only the tread aiming left out.
// 0.5.93: the left tread spun because it was turned as a wheel. Justin's
// Kangaroo dossier: the Kangaroo is a Horch made over (its model swapped in
// on the map's Horch) and posed as a four-wheeler. A vehicle's wheel bones
// are kept as bone numbers at +36 of its physics object (up to eight, -1 for
// none; sub_8242B938 turns and springs each), found once by name in the order
// of the game's wheel-name table (0x825D5C6C: front left, front right, back
// left, back right, middle left, middle right). They were found on the
// Horch's model: on the Kangaroo's skeleton bone 4 is tag_tread_left, so the
// Horch's wheel there turned the tread. Each frame the Kangaroo's list is put
// right by the names on its own skeleton.
namespace
{
    int32_t KangarooBone(uint8_t* base, uint32_t dobj, const char* name)
    {
        const uint32_t model = dobj ? Guest::Read32(base, dobj + 128) : 0;
        uint32_t skeleton = 0;
        for (int k = 0; model && k < 8 && !skeleton; k++) skeleton = Guest::Read32(base, model + 36 + 4 * uint32_t(k));
        const uint32_t table = skeleton ? Guest::Read32(base, skeleton + 8) : 0;
        const uint32_t count = table ? Guest::Read32(base, table + 16) : 0;
        const uint32_t names = table ? Guest::Read32(base, table + 20) : 0;
        if (!names || count > 160) return -1;
        const uint32_t hash = TagHash(name);
        for (uint32_t i = 0; i < count; i++) if (Guest::Read32(base, names + 12 * i + 4) == hash) return int32_t(i);
        return -1;
    }
    void FixKangarooWheels(uint8_t* base, uint32_t object, uint32_t entity)
    {
        static const char* const order[6] = { "tag_wheel_front_left", "tag_wheel_front_right", "tag_wheel_back_left",
                                              "tag_wheel_back_right", "tag_wheel_middle_left", "tag_wheel_middle_right" };
        const uint32_t dobj = Guest::Read32(base, entity + 492);
        static std::set<uint32_t> told;
        const bool tell = told.insert(object).second;
        std::string before, after;
        char text[16];
        for (uint32_t i = 0; i < 8; i++)
        {
            const int32_t was = int32_t(Guest::Read32(base, object + 36 + 4 * i));
            snprintf(text, sizeof(text), " %d", was); before += text;
            if (was < 0 || i >= 6) { after += text; continue; }   // only the slots the vehicle uses
            const int32_t bone = KangarooBone(base, dobj, order[i]);
            if (bone >= 0 && bone != was) Guest::Write32(base, object + 36 + 4 * i, uint32_t(bone));
            snprintf(text, sizeof(text), " %d", bone >= 0 ? bone : was); after += text;
        }
        if (tell) { printf("kangaroo: wheel bones were%s, now%s (its own by name)\n", before.c_str(), after.c_str()); fflush(stdout); }
    }
}
// 0.5.103: the Kangaroo's treads going straight. Each frame sub_82568600
// (the entity r3; its record at +524) adds the distance driven to the treads'
// distances (record +1308 right, +1312 left), which sub_82434918 turns into
// the treads' and sprockets' poses. On the Kangaroo (the Horch's physics)
// that only moved them while turning (Justin, 0.5.102): going straight, the
// treads stood still. So after it, the distance the Kangaroo moved along its
// facing since the last frame is added to both sides (backwards takes away).
namespace { struct TreadLast { float x, y; bool set; }; std::map<uint32_t, TreadLast> g_treadLast; }
extern "C" PPC_FUNC(__imp__sub_82568600);
PPC_FUNC(sub_82568600)
{
    const uint32_t entity = ctx.r3.u32;
    __imp__sub_82568600(ctx, base);
    if (!entity || g_kangarooEntities.empty() || g_kangarooEntities.count(entity) == 0) return;
    const uint32_t record = Guest::Read32(base, entity + 524);
    if (record < 0x40000000u || record >= 0xA0000000u) return;
    const float x = ReadF(base, entity + 336), y = ReadF(base, entity + 340);
    TreadLast& last = g_treadLast[entity];
    if (!last.set) { last = { x, y, true }; return; }
    const float dx = x - last.x, dy = y - last.y;
    last.x = x; last.y = y;
    if (fabsf(dx) + fabsf(dy) > 200.0f) return;   // a jump (moved or respawned), not driving
    const float yaw = ReadF(base, entity + 356) * 3.14159265f / 180.0f;
    const float ahead = dx * cosf(yaw) + dy * sinf(yaw);
    if (ahead == 0.0f) return;
    WriteF(base, record + 1308, ReadF(base, record + 1308) + ahead);
    WriteF(base, record + 1312, ReadF(base, record + 1312) + ahead);
}

extern "C" PPC_FUNC(__imp__sub_8242B938);
PPC_FUNC(sub_8242B938)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t entity = object ? Guest::Read32(base, object + 624) : 0;
    const bool kangaroo = entity && !g_kangarooEntities.empty() && g_kangarooEntities.count(entity) != 0;
    if (kangaroo && g_treadMode.load() != 1) FixKangarooWheels(base, object, entity);
    const uint32_t flags = kangaroo ? Guest::Read32(base, object + 640) : 0;
    if (kangaroo && g_treadMode.load() == 0 && (flags & 0x10))
    {
        static bool said = false;
        if (!said)
        {
            said = true;
            printf("kangaroo: posed as a wheeled vehicle (flags %08X): wheels turn, treads in place; its wheel bones", flags);
            for (uint32_t i = 0; i < 8; i++) printf(" %d", int32_t(Guest::Read32(base, object + 36 + 4 * i)));
            printf("\n"); fflush(stdout);
        }
        Guest::Write32(base, object + 640, flags & ~0x10u);
        __imp__sub_8242B938(ctx, base);
        Guest::Write32(base, object + 640, Guest::Read32(base, object + 640) | 0x10u);
        return;
    }
    __imp__sub_8242B938(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8241E908);
PPC_FUNC(sub_8241E908)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t entity = object ? Guest::Read32(base, object + 624) : 0;
    if (entity && g_treadMode.load() == 7 && g_kangarooEntities.count(entity) != 0) return;
    __imp__sub_8241E908(ctx, base);
}
#endif

#ifdef COD3_MP
namespace {
    // Full player-box clearance and grounded landing using the same native
    // trace layout as the verified Kangaroo exits, on a private guest stack.
    bool TacticalLanding(PPCContext& ctx,uint8_t* base,uint32_t player,const float saved[3],uint32_t& position) {
        static uint32_t memory=0;
        if(!memory) memory=Guest::AllocatePhysical(0x21000);
        if(!memory) return false;
        const uint32_t sp=memory+0x1FF00, scratch=memory+0x20000;
        const uint32_t start=scratch+96,end=scratch+112,mins=scratch+128,maxs=scratch+144,filter=scratch+160;
        position=scratch+192;
        memset(base+scratch,0,256);
        for(int k=0;k<3;++k) {
            float lo=ReadF(base,player+240+4*k),hi=ReadF(base,player+256+4*k);
            if(!std::isfinite(saved[k])||!std::isfinite(lo)||!std::isfinite(hi)||lo>hi||fabsf(lo)>128||fabsf(hi)>128) return false;
            WriteF(base,mins+4*k,lo);WriteF(base,maxs+4*k,hi);
            WriteF(base,start+4*k,saved[k]);WriteF(base,end+4*k,saved[k]);
        }
        WriteF(base,start+8,saved[2]+8);WriteF(base,end+8,saved[2]-16);
        Guest::Write32(base,filter,0x82065DCC);
        Guest::Write32(base,filter+4,Guest::Read32(base,player+484));
        Guest::Write32(base,filter+20,0x00210011);
        Guest::Write32(base,sp,0);Guest::Write32(base,sp+84,0);Guest::Write32(base,sp+92,0);
        PPCContext call=ctx;call.r1.u64=sp;call.lr=0;
        call.r3.u64=scratch;call.r4.u64=start;call.r5.u64=mins;call.r6.u64=maxs;
        call.r7.u64=end;call.r8.u64=filter;call.r9.u64=0;call.r10.u64=0;call.f1.f64=0;
        sub_82534200(call,base);
        const float fraction=ReadF(base,scratch+32),nz=ReadF(base,scratch+24);
        if(!std::isfinite(fraction)||fraction<0||fraction>=1||!std::isfinite(nz)||nz<0.6f||base[scratch+60]||base[scratch+61]) return false;
        for(int k=0;k<3;++k) {float v=ReadF(base,scratch+4*k);if(!std::isfinite(v))return false;WriteF(base,position+4*k,v);}
        return true;
    }
}
static void TacticalInsertionFrame(PPCContext& ctx,uint8_t* base) {
    static TacticalPointInput::Hold hold,returnHold;
    static bool saved=false;
    static float point[3]={};
    static uint32_t hostBefore=0,levelBefore=0;
    const uint32_t list=Guest::Read32(base,0x82BCC7FC);
    const uint32_t host=list?Guest::Read32(base,list+4):0;
    const uint32_t ps=host?Guest::Read32(base,host+512):0;
    const uint32_t levelTime=Guest::Read32(base,0x82C2826C);
    if(host!=hostBefore || levelTime<levelBefore) {saved=false;hold={};returnHold={};}
    hostBefore=host;levelBefore=levelTime;
    bool eligible=host>=0x40000000u && host<0xA0000000u && ps>=0x40000000u && ps<0xA0000000u &&
        Guest::Read32(base,0x82BCBE9C)==0 && !Guest::Read8(base,0x82C73024) && !Overlay::IsOpen() &&
        Guest::Read32(base,0x82C7A270+0xB8)==2;
    if(eligible) {
        eligible=int32_t(Guest::Read32(base,ps+304))>0 && Guest::Read32(base,ps+1924)==3 &&
            !(Guest::Read32(base,ps+240)&0x100000u) &&
            !Guest::Read32(base,host+900) && ReadF(base,ps+172)<0.01f;
        // Horizontal stillness; ordinary vertical gravity samples must not
        // restart the timer every frame while standing on the floor.
        for(int k=0;k<3;++k) {const float v=ReadF(base,ps+16+4*k);eligible=eligible&&std::isfinite(v)&&fabsf(v)<(k==2?40.0f:5.0f);}
    }
    // 0.5.181: (the return is held X now, input.cpp.) Justin: R3 said "RETURN POINT BLOCKED" (and often the hold was
    // rejected): he wants to go back to wherever he saved, from wherever he
    // is. Returning needs only: alive and in play, out of menus and out of a
    // vehicle; moving, ADS or odd ground no longer stop it. (The strict
    // stillness above still decides the L3 save and when L3/R3 are kept
    // from the game, so sprint and melee work as normal while moving.)
    bool canReturn=host>=0x40000000u && host<0xA0000000u && ps>=0x40000000u && ps<0xA0000000u &&
        Guest::Read32(base,0x82BCBE9C)==0 && !Guest::Read8(base,0x82C73024) && !Overlay::IsOpen() &&
        Guest::Read32(base,0x82C7A270+0xB8)==2;
    if(canReturn) canReturn=int32_t(Guest::Read32(base,ps+304))>0 && Guest::Read32(base,ps+1924)==3 &&
        !(Guest::Read32(base,ps+240)&0x100000u) && !Guest::Read32(base,host+900);
    Input::SetTacticalPointInput(eligible,saved);
    const bool down=Input::TacticalPointSaveHeld();
    const bool back=Input::TacticalPointReturnHeld();
    const bool starting=eligible&&down&&!hold.timing;
    if(starting)Overlay::Notice("SAVING RETURN POINT - hold L3",1100);
    if(TacticalPointInput::Save(hold,eligible,down,GetTickCount())) {
        bool finite=true;
        for(int k=0;k<3;++k)finite=finite&&std::isfinite(ReadF(base,ps+4*k));
        if(finite) {
            for(int k=0;k<3;++k)point[k]=ReadF(base,ps+4*k);
            saved=true;Overlay::Notice("RETURN POINT SAVED - hold X for 2 seconds to return",4000);
            printf("tactical point: saved %.1f %.1f %.1f\n",point[0],point[1],point[2]);fflush(stdout);
        }
    }
    const bool returning=TacticalPointInput::Save(returnHold,canReturn,back,GetTickCount(),2000);
    // X is also reload/use: the notice only once X has been held half a
    // second, so an ordinary reload shows nothing.
    {
        static bool told=false;
        if(!returnHold.timing) told=false;
        else if(saved&&!told&&uint32_t(GetTickCount()-returnHold.began)>=500) {told=true;Overlay::Notice("RETURNING - keep holding X",1800);}
    }
    if(down&&!eligible) {
        static uint32_t lastRejected=0;
        const uint32_t now=GetTickCount();
        if(now-lastRejected>1000) {
            lastRejected=now;
            printf("tactical point: hold rejected; local state %u pause %u current %u host %08X ps %08X\n",Guest::Read32(base,0x82C7A270+0xB8),Guest::Read8(base,0x82C73024),Guest::Read32(base,0x82BCBE9C),host,ps);fflush(stdout);
            Overlay::Notice("RETURN POINT: stop moving, leave ADS and menus",1500);
        }
    }
    if(!canReturn||!saved||!returning)return;
    uint32_t position=0;
    // 0.5.181: the grounded landing trace is only a refinement now. If it
    // finds no clear floor (odd geometry), he goes to the exact spot he
    // saved: it is where he stood when he saved it.
    if(!TacticalLanding(ctx,base,host,point,position)) {
        if(!position) {Overlay::Notice("RETURN POINT: no memory",2500);return;}
        for(int k=0;k<3;++k)WriteF(base,position+4*k,point[k]);
        WriteF(base,position+8,point[2]+1.0f);
        printf("tactical point: landing trace found no floor; returning to the exact saved spot\n");fflush(stdout);
    }
    PPCContext place=ctx;place.r1.u64=position-448;place.lr=0;place.r3.u64=host;place.r4.u64=position;
    sub_8255E118(place,base);
    for(int k=0;k<3;++k)WriteF(base,ps+16+4*k,0);
    Overlay::Notice("RETURNED TO SAVED POINT",2000);
    printf("tactical point: returned %.1f %.1f %.1f\n",point[0],point[1],point[2]);fflush(stdout);
}

// --- Sound bank slots (G43 investigation) -----------------------------------
// The sound engine keeps its banks in a table of slots (52 bytes each, at
// [0x82A89918], their number at 0x825B1838, from the 68-byte configuration
// its start-up, sub_821297E8, copies from its caller). A bank is queued into
// the first empty slot (sub_8212A410: state 0 -> 1, -1 when none is empty),
// loaded by the pump (sub_8212B3B8: 1 -> 2 -> 3), and the cue look-up by name
// (sub_8212B268) searches only slots in state 3. The campaign banks loaded
// beside a map (forest.wbk for the G43) are loaded below (sub_824964E8) and
// need the bigger bank memory set at start-up here; a bank that finds no
// empty slot is logged with the slots.
namespace
{
    void LogSoundSlots(uint8_t* base, const char* why)
    {
        const uint32_t count = Guest::Read32(base, 0x825B1838u);
        const uint32_t table = Guest::Read32(base, 0x82A89918u);
        printf("soundslots: %s: %u slots at %08X\n", why, count, table);
        for (uint32_t i = 0; table && i < count && i < 64; i++)
        {
            const uint32_t e = table + 52 * i;
            printf("soundslots:   %2u id %08X state %u file %08X size %u offset %08X order %u data %08X\n", i,
                   Guest::Read32(base, e), Guest::Read32(base, e + 4), Guest::Read32(base, e + 12), Guest::Read32(base, e + 16),
                   Guest::Read32(base, e + 8), Guest::Read32(base, e + 20), Guest::Read32(base, e + 24));
        }
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_821297E8);
PPC_FUNC(sub_821297E8)
{
    if (ctx.r3.u32)
    {
        const uint32_t config = ctx.r3.u32;
        printf("soundslots: start-up configuration at %08X:", config);
        for (uint32_t i = 0; i < 68; i += 4) printf(" %08X", Guest::Read32(base, config + i));
        printf("\n");
        // +24/+28: the bank memory, the banks' in-memory parts. Multiplayer
        // gives none of its own (+24 0) and a limit of 0x880000 (8.5 MB)
        // taken from the game's heap, the map block: global.wbk 0.4 MB and a
        // map's 6-7 MB fit; the campaign banks loaded beside them (forest.wbk
        // 8.8 MB, mace2.wbk 13.8 MB) did not, and the loader let them go, so
        // no campaign cue was found. 36 MB more of the map block hung the map
        // load. With the campaign guns' sounds on, the engine is given its
        // own 36 MB of physical memory instead (the engine makes its heap in
        // memory it is given, sub_8212E9A8), outside the map block.
        if (Settings::Get().campaignGuns && Settings::Get().campaignSounds && Guest::Read32(base, config + 24) == 0)
        {
            // 0.5.338: 64 MB (was 36). Laison's bank (the mortar's sounds,
            // 0.5.29x) on top of Mace 2's and Forest's left too little for
            // the map's own bank, queued last: on Merville its cues were not
            // found (BAZOOKA_FIRE_3RD, the shell explosions, STG44 3rd person,
            // Player_Dying, the ambience; Justin: no Panzerschreck explosion).
            const uint32_t bytes = 0x4000000u;
            const uint32_t memory = Guest::AllocatePhysical(bytes);
            if (memory)
            {
                Guest::Write32(base, config + 24, memory);
                Guest::Write32(base, config + 28, bytes);
                g_campaignBanksLoaded = true;
            }
            printf("soundslots: bank memory: %s %u bytes at %08X\n", memory ? "its own" : "NOT GIVEN (campaign banks stay unloaded)", bytes, memory);
        }
        fflush(stdout);
    }
    __imp__sub_821297E8(ctx, base);
    printf("soundslots: start-up -> %d\n", int32_t(ctx.r3.u32));
    fflush(stdout);
}
extern "C" PPC_FUNC(__imp__sub_8212A410);
PPC_FUNC(sub_8212A410)
{
    const uint32_t file = ctx.r3.u32, size = ctx.r4.u32, offset = ctx.r5.u32;
    __imp__sub_8212A410(ctx, base);
    if (g_queueingCampaign && int32_t(ctx.r3.u32) != -1)
    {
        const uint32_t count = Guest::Read32(base, 0x825B1838u);
        if (count) g_campaignSlots |= uint64_t(1) << (((ctx.r3.u32 >> 16) % count) & 63);
        printf("soundslots: campaign bank in slot %u\n", count ? (ctx.r3.u32 >> 16) % count : 0);
    }
    static int said = 0;
    if ((said < 60 && said++ < 60))
    {
        printf("soundslots: bank queued: file %08X size %u offset %08X -> id %08X%s\n", file, size, offset, ctx.r3.u32,
               int32_t(ctx.r3.u32) == -1 ? " (NO EMPTY SLOT)" : "");
        if (int32_t(ctx.r3.u32) == -1) LogSoundSlots(base, "no empty slot");
        fflush(stdout);
    }
}



// 0.5.204: the freeze a little after bots were added (0.5.203, Justin: "the
// game crashed" seconds after bot_add 24, the picture frozen, fps 0).
//
// The game's main thread was stuck for good in sub_824C0AE0, the search of
// a script thread's list of records for an endon (the endon built-in,
// sub_824C9258: offset 0x58 of the script built-ins, docs/builtins-table.md).
// That search can only go round forever when the list is broken, and the
// list is broken when a record is put on it that was never given memory:
// the endon, wait and waitframes built-ins (sub_824C9258, sub_824C9350,
// sub_824C4840, sub_824C4910) take a block from the game's small-block pool
// (the pool at 0x82BCBF5C, also kept at eight other places: most of the
// game's small allocations come from it) and do not check the answer; with
// none, they put "block 0 + 4" on the list.
//
// The pool (sub_820E0D40; made in sub_8254A758 with bins of 20, 32, 64, 128
// and 256 bytes, 4096, 2048, 4096, 1024 and 450 blocks) answers 0 when
// every bin a block fits is empty: the setting that lets it go on to the
// game's heap instead (bit 1 of +56) is off for this pool. With 24 bots,
// and the campaign weapons, sounds and maps now loaded beside the game's own,
// it ran out. So here, when the pool answers 0, the block comes from the
// game's heap the way the pool's own setting would take it (sub_820DE750
// with 16 and the size), and its free (sub_820E0E60) already gives a block
// outside every bin back to the heap (sub_820DE980). And the search counts
// its steps, so that a broken list is told and left rather than frozen on.
//
// "pool:" lines say how full the bins are (every 30 s, and when one runs out).
namespace
{
    constexpr uint32_t SmallPool = 0x82BCBF5C;   // the pool's pointer
    constexpr uint32_t HeapAllocate = 0x820DE750;
    std::atomic<uint32_t> g_poolFallbacks{0}, g_poolFailures{0};
    std::atomic<uint32_t> g_poolCalls{0};
    uint32_t g_endonLongest = 0;

    void LogPool(uint8_t* base, uint32_t pool, const char* why)
    {
        if (pool == 0) return;
        const uint32_t count = Guest::Read32(base, pool + 52);
        printf("pool: %s; pool %08X, %u bins:", why, pool, count);
        for (uint32_t i = 0; i < count && i < 8; i++)
        {
            const uint32_t bin = Guest::Read32(base, pool + 4 * i);
            if (bin == 0) continue;
            const uint32_t size = (Guest::Read32(base, pool + 36 + 2 * (i & ~1u)) >> ((i & 1) ? 0 : 16)) & 0xFFFF;
            const uint32_t total = Guest::Read32(base, bin + 4), free = Guest::Read32(base, bin + 8), peak = Guest::Read32(base, bin + 20);
            printf(" %u bytes %u of %u used (most %u)", size, total - free, total, peak);
        }
        printf(" | %u taken from the heap instead, %u not given at all; longest endon list %u",
               g_poolFallbacks.load(), g_poolFailures.load(), g_endonLongest);
        // 0.5.205: the particle system's effects and nodes too (count, in
        // use, most at once): see sub_82109F58 below.
        for (const auto& fx : { std::make_pair("effects", 0x82A8786Cu), std::make_pair("effect nodes", 0x82A89474u) })
        {
            const uint32_t holder = Guest::Read32(base, fx.second);
            if (holder < 0x40000000u || holder >= 0xA0000000u) continue;
            const uint32_t at = holder + 8;
            printf("; %s %u of %u (most %u)", fx.first, Guest::Read32(base, at + 4), Guest::Read32(base, at), Guest::Read32(base, at + 8));
        }
        printf("\n");
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_820E0D40);
PPC_FUNC(sub_820E0D40)
{
    const uint32_t pool = ctx.r3.u32, size = ctx.r4.u32 & 0xFFFF;
    PPCContext call = ctx;
    __imp__sub_820E0D40(ctx, base);
    if (ctx.r3.u32 == 0 && size != 0)
    {
        call.r1.u32 = (call.r1.u32 - 0x200) & ~0xFu;
        call.r3.u64 = 16;
        call.r4.u64 = size;
        call.lr = 0;
        Guest::Lookup(HeapAllocate)(call, base);
        ctx.r3.u64 = call.r3.u32;
        const uint32_t n = call.r3.u32 ? ++g_poolFallbacks : ++g_poolFailures;
        if (n <= 5 || (n & (n - 1)) == 0)
        {
            char why[96];
            snprintf(why, sizeof(why), "a %u byte block found no room; %s %08X", size, call.r3.u32 ? "the heap gave" : "THE HEAP HAD NONE EITHER,", call.r3.u32);
            LogPool(base, pool, why);
        }
    }
    if ((++g_poolCalls & 0x3FFF) == 0)
    {
        static DWORD last = 0;
        const DWORD now = GetTickCount();
        if (now - last >= 30000)
        {
            last = now;
            LogPool(base, Guest::Read32(base, SmallPool), "every 30 s");
        }
    }
}
extern "C" PPC_FUNC(__imp__sub_824C0AE0);
PPC_FUNC(sub_824C0AE0)
{
    // As the game's (an endon already on the running thread's list for this
    // entity and event?), with a count of its steps.
    const uint32_t head = ctx.r3.u32 + 24, key = ctx.r4.u32;
    uint32_t link = Guest::Read32(base, head + 4);
    uint32_t next = link ? Guest::Read32(base, link) : 0;
    if (link == head + 8) { link = 0; next = 0; }
    uint32_t steps = 0;
    while (next != 0)
    {
        if (link < 0x10000 || ++steps > 100000)
        {
            static int said = 0;
            if ((said < 10 && said++ < 10))
            {
                printf("pool: a script thread's endon list at %08X is broken (step %u at %08X); the endon is left off\n", head, steps, link);
                fflush(stdout);
            }
            ctx.r3.u64 = 1;
            return;
        }
        if (Guest::Read32(base, link + 12) == 3 && Guest::Read32(base, link + 20) == key)
        {
            ctx.r3.u64 = 1;
            if (steps > g_endonLongest) g_endonLongest = steps;
            return;
        }
        link = next;
        next = Guest::Read32(base, next);
    }
    if (steps > g_endonLongest) g_endonLongest = steps;
    ctx.r3.u64 = 0;
}


// 0.5.205: the crash after a while with 22 bots (0.5.204, Justin hosting,
// his friend joined, Merville War, ~8 minutes in). From 353 s the host's log
// fills with the particle system's own complaints, "apsMemory : pool(size=
// 128, count=4096) is empty", "apsEffect : pool is empty" and "apsGroup :
// alloc failed", thousands of them; at 434 s its drawing (sub_82110FD0 under
// sub_820FC850, from nglListSend) calls through a broken object, and at
// 472 s through garbage, and the game goes down. The joiner, with the same
// fights on screen but no bots of his own, never ran out.
//
// The particle system sets its sizes once, at start-up (sub_82430A58): group
// memory 2 MB (sub_82109F38), 2048 effect nodes of 224 bytes (sub_82109F48),
// 4096 effects of 128 bytes (sub_82109F58) and 1024 of a third kind
// (sub_82109F68), into the table at 0x825B0FD4 that sub_8210ACB8 then makes
// the pools from (sub_8210A378; free lists of 16-bit indexes, so up to
// 32767). Twice each now (group memory 4 MB, 4096 nodes, 8192 effects, 2048):
// about 5 MB more, taken from the same memory the game takes them from. The
// "pool:" line every 30 s says how many effects and nodes are in use and the
// most at once, to see whether they still fill (a leak) or not.
namespace
{
    int32_t Doubled(int32_t asked, int32_t most)
    {
        const int32_t twice = asked > 0 ? asked * 2 : asked;
        return twice > most ? most : twice;
    }
}
extern "C" PPC_FUNC(__imp__sub_82109F38);
PPC_FUNC(sub_82109F38)
{
    const uint32_t asked = ctx.r3.u32;
    if (asked > 0 && asked <= 0x1000000u) ctx.r3.u64 = asked * 2;
    printf("effects: group memory %u bytes (the game asked for %u)\n", ctx.r3.u32, asked);
    fflush(stdout);
    __imp__sub_82109F38(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82109F48);
PPC_FUNC(sub_82109F48)
{
    const int32_t asked = int32_t(ctx.r3.u32);
    ctx.r3.s64 = Doubled(asked, 16000);
    printf("effects: %d effect nodes (the game asked for %d)\n", int32_t(ctx.r3.u32), asked);
    fflush(stdout);
    __imp__sub_82109F48(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82109F58);
PPC_FUNC(sub_82109F58)
{
    const int32_t asked = int32_t(ctx.r3.u32);
    ctx.r3.s64 = Doubled(asked, 16000);
    printf("effects: %d effects (the game asked for %d)\n", int32_t(ctx.r3.u32), asked);
    fflush(stdout);
    __imp__sub_82109F58(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82109F68);
PPC_FUNC(sub_82109F68)
{
    const int32_t asked = int32_t(ctx.r3.u32);
    ctx.r3.s64 = Doubled(asked, 16000);
    printf("effects: %d of the third kind (the game asked for %d)\n", int32_t(ctx.r3.u32), asked);
    fflush(stdout);
    __imp__sub_82109F68(ctx, base);
}
// 0.5.242: the 32-player test (0.5.241, rimling, 28 bots + host) crashed
// 2 min in: an effect started by a sound event (sub_82554BA0 -> 82552BE8 ->
// 82442840 -> 82440118 -> 8243B878 spawn effect -> 820FBE20 -> 82108EA8)
// read a garbage child pointer. Just before, the particle system said 22
// times "apsMemory : pool(size=1024, count=512) is empty": its general
// memory, nine pools of 512 blocks each (16 bytes .. 8 KB) added at start-up
// by sub_82430A58 through sub_8210ACA0 (count r3, block size r4; the
// best-fit allocator sub_8210A638 takes from them), was never enlarged
// (0.5.205 doubled only the effects, nodes, group memory and the third
// kind). The 24-player run had none of those messages. With 32 players in
// play every pool added there gets twice the blocks (about 8 MB more, the
// same memory the game takes them from); 24 is unchanged.
// 0.5.242: the spare word for 82580B40's damage-from table (see
// scripts/patch_recomp.py SINK337): damage by players 24-31 lands here, not on
// the victim's record +1344.
uint32_t MpDamageSink(uint8_t* base);
// 0.5.251: the side block for the overhead icons (rank, medic, spotted)
// of players 24-31. 824F9410 fills 24 entries of 12 bytes at HUD object +76
// (one per EntityList slot) and 82507D28 draws them; the object's next
// fields follow at once, so entries 24..31 of each HUD object (up to 16
// objects, one per local player view) live here (patch_cap32 SIDE12W/R).
uint32_t MpHudSide(uint8_t* base, uint32_t object)
{
    constexpr uint32_t Slots = 16, Bytes = 8 * 12 + 32;
    static std::mutex lock;
    static uint32_t objects[Slots] = {}, blocks[Slots] = {};
    std::lock_guard<std::mutex> hold(lock);
    for (uint32_t i = 0; i < Slots; ++i)
        if (objects[i] == object && blocks[i]) return blocks[i];
    for (uint32_t i = 0; i < Slots; ++i)
    {
        if (objects[i]) continue;
        const uint32_t memory = Guest::AllocatePhysical(Bytes);
        if (!memory) break;
        memset(base + memory, 0, Bytes);
        objects[i] = object;
        blocks[i] = memory;
        printf("overhead icons: players 24-31 of HUD object %08X at %08X\n", object, memory);
        fflush(stdout);
        return memory;
    }
    static uint32_t fallback = 0;   // (never expected: 16 objects or no memory)
    if (!fallback) { fallback = Guest::AllocatePhysical(Bytes); if (fallback) memset(base + fallback, 0, Bytes); }
    return fallback ? fallback : MpDamageSink(base);
}
uint32_t MpDamageSink(uint8_t* base)
{
    static uint32_t sink = 0;
    if (!sink) { sink = Guest::AllocatePhysical(64); if (sink) memset(base + sink, 0, 64); }
    return sink ? sink : 0x82C281D0u + 0x200u;   // (fallback never expected)
}
extern "C" PPC_FUNC(__imp__sub_8210ACA0);
PPC_FUNC(sub_8210ACA0)
{
    const int32_t asked = int32_t(ctx.r3.u32);
    // 0.5.243: for 24 players too: 0.5.242's 24-player Rimling run (20 bots)
    // logged the same 1 KB pool empty 46 times (it did not crash that time).
    // 0.5.244: with 24, only the 1 KB pool (the one that ran dry), so the
    // memory taken at start-up stays close to the game's own (0.5.243's
    // 24-player Rimling run crashed loading the map, in the session's
    // message reading, an unrelated-looking place; kept minimal to be safe).
    if (asked > 0 && asked <= 8192 && (MpCapClients > 24 || ctx.r4.u32 == 1024)) ctx.r3.s64 = asked * 2;
    static int told = 0;
    if ((told < 40 && told++ < 40)) { printf("effects: memory pool of %u-byte blocks, %d of them (the game asked for %d)\n", ctx.r4.u32, int32_t(ctx.r3.u32), asked); fflush(stdout); }
    __imp__sub_8210ACA0(ctx, base);
}
// And whether the pools were made: a pool the game could not get the
// memory for is left out (sub_820FD090 checks), which the log should say.
extern "C" PPC_FUNC(__imp__sub_8210ACB8);
PPC_FUNC(sub_8210ACB8)
{
    __imp__sub_8210ACB8(ctx, base);
    const uint32_t effects = Guest::Read32(base, 0x82A8786Cu), nodes = Guest::Read32(base, 0x82A89474u);
    printf("effects: pools made: effects %s (%u), nodes %s (%u)\n",
           effects ? "yes" : "NO (no memory for them)", effects ? Guest::Read32(base, effects + 8) : 0,
           nodes ? "yes" : "NO (no memory for them)", nodes ? Guest::Read32(base, nodes + 8) : 0);
    fflush(stdout);
}


// 0.5.372: the game's 4x4 matrix inverse (sub_820B6260), native. Marseille
// at 32 players (0.5.371 profile): the inverse and its 3x3 minor
// (sub_820B6128, 20 calls an inverse, every float through the stack in
// guest memory) were 5% of the main thread, from 24 places in the server's
// code. Same steps, same order, same rounding as the recompiled code: each
// float step is done in double and rounded to float, as it does. The first
// 2000 calls also run the game's own and compare (logged). Off with
// COD3_NATIVE_INVERSE=0.
namespace NativeInverse
{
    inline float LoadF(uint8_t* base, uint32_t at) { const uint32_t v = Guest::Read32(base, at); float f; memcpy(&f, &v, 4); return f; }
    inline void StoreF(uint8_t* base, uint32_t at, float f) { uint32_t v; memcpy(&v, &f, 4); Guest::Write32(base, at, v); }
    inline float Mul(float a, float b) { return float(double(a) * double(b)); }
    inline float MulAdd(float a, float b, float c) { return float(double(a) * double(b) + double(c)); }
    inline float MulSubFrom(float a, float b, float c) { return float(-(double(a) * double(b) - double(c))); }

    // sub_820B6128: the signed minor of m without row r and column c.
    float Minor(const float m[4][4], int r, int c)
    {
        float A[3][3]; int oi = 0;
        for (int i = 0; i < 4; i++)
        {
            if (i == r) continue;
            int oj = 0;
            for (int j = 0; j < 4; j++) { if (j == c) continue; A[oi][oj++] = m[i][j]; }
            oi++;
        }
        const float f6 = Mul(A[2][2], A[1][1]), f3 = Mul(A[1][1], A[0][2]), f0 = Mul(A[2][2], A[1][0]);
        const float f5 = Mul(A[2][1], A[1][0]), f12 = Mul(A[2][1], A[1][2]), f4 = Mul(A[1][2], A[2][0]);
        float f11 = Mul(f6, A[0][0]);
        f11 = MulAdd(f5, A[0][2], f11);
        f11 = MulAdd(f4, A[0][1], f11);
        f11 = MulSubFrom(f3, A[2][0], f11);
        const float f13 = MulSubFrom(f12, A[0][0], f11);
        const float d = MulSubFrom(f0, A[0][1], f13);
        return Mul(d, ((r ^ c) & 1) ? -1.0f : 1.0f);
    }

    // Into out (16 floats); returns what the game leaves in f1.
    float Inverse(uint8_t* base, uint32_t in, float out[16], bool& identity)
    {
        float m[4][4];
        for (int i = 0; i < 16; i++) m[i >> 2][i & 3] = LoadF(base, in + 4 * i);
        const float zero = LoadF(base, 0x820641A0u);
        float minors[4][4];
        for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) minors[i][j] = Minor(m, i, j);
        float det = zero;
        for (int j = 0; j < 4; j++) det = MulAdd(m[0][j], minors[0][j], det);
        identity = det == zero;
        if (identity)
        {
            for (int i = 0; i < 16; i++) out[i] = LoadF(base, 0x82A94660u + 4 * i);
            return minors[0][3];
        }
        const float scale = float(double(LoadF(base, 0x8206178Cu)) / double(det));
        for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) out[r * 4 + c] = minors[c][r] * scale;
        return det;
    }

    bool On()
    {
        static const bool on = []() { const char* t = getenv("COD3_NATIVE_INVERSE"); return !(t && t[0] == '0'); }();
        return on;
    }
    uint32_t g_checked = 0, g_same = 0;
}

extern "C" PPC_FUNC(__imp__sub_820B6260);
PPC_FUNC(sub_820B6260)
{
    using namespace NativeInverse;
    if (!On()) { __imp__sub_820B6260(ctx, base); return; }
    const uint32_t outReg = ctx.r3.u32, outAt = outReg & ~15u, inAt = ctx.r4.u32;   // the game stores it with vector stores: aligned down
    float out[16]; bool identity = false;
    const float f1 = Inverse(base, inAt, out, identity);
    if (g_checked < 2000)
    {
        __imp__sub_820B6260(ctx, base);
        bool same = true;
        for (int i = 0; i < 16; i++)
        {
            const float game = LoadF(base, outAt + 4 * i);
            if (memcmp(&game, &out[i], 4) != 0 && !(std::fabs(game - out[i]) <= 1e-6f * std::max(1.0f, std::fabs(game)))) same = false;
        }
        g_checked++;
        if (same) g_same++;
        else if (g_checked - g_same <= 4)
        {
            printf("inverse: native differs from the game's at call %u (det path %s): game %g %g %g %g, native %g %g %g %g\n", g_checked,
                identity ? "identity" : "normal", LoadF(base, outAt), LoadF(base, outAt + 4), LoadF(base, outAt + 20), LoadF(base, outAt + 60),
                out[0], out[1], out[5], out[15]);
        }
        if (g_checked == 2000 || (g_checked % 500) == 0)
        {
            printf("inverse: native matched the game's on %u of %u calls\n", g_same, g_checked);
            fflush(stdout);
        }
        return;   // the game's own result stands while checking
    }
    for (int i = 0; i < 16; i++) StoreF(base, outAt + 4 * i, out[i]);
    ctx.r3.u64 = outReg;
    ctx.f1.f64 = double(f1);
}


// 0.5.375: the world's box tree query (sub_824193E0), native. Marseille at
// 32 players: 6.5% of the main thread, the top function after 0.5.374. A
// four-way tree of boxes in 16 bit numbers; the query box is turned into
// the tree's numbers, the tree walked, and for each piece it reaches at the
// tree's depth the caller's object is called (its first virtual) with the
// piece. Same steps, same order. The first 3000 queries also run the game's
// own walk, its calls caught by a stand-in object, and the two lists of
// pieces compared (logged); one difference and the game's own is used from
// then on. COD3_NATIVE_CULL=0: the game's own.
namespace NativeCull
{
    thread_local std::vector<uint32_t>* t_record = nullptr;
    std::atomic<uint32_t> g_checked{ 0 }, g_same{ 0 };
    std::atomic<bool> g_failed{ false };

    struct Vec { uint8_t b[16]; };
    inline Vec LoadVec(uint8_t* base, uint32_t at) { Vec v; memcpy(v.b, base + (at & ~15u), 16); return v; }
    inline float Lane(const Vec& v, int i) { uint32_t w; memcpy(&w, v.b + 4 * i, 4); w = _byteswap_ulong(w); float f; memcpy(&f, &w, 4); return f; }
    inline Vec Perm(const Vec& a, const Vec& b, const Vec& m)
    {
        Vec r;
        for (int i = 0; i < 16; i++) { const int k = m.b[i] & 31; r.b[i] = k < 16 ? a.b[k] : b.b[k - 16]; }
        return r;
    }
    inline float FMax(float a, float b) { return a > b ? a : b; }
    inline float FMin(float a, float b) { return a < b ? a : b; }
    inline int16_t Trunc16(float f)
    {
        int32_t i;
        if (f != f) i = INT32_MIN;
        else if (f >= 2147483648.0f) i = INT32_MAX;
        else if (f < -2147483648.0f) i = INT32_MIN;
        else i = int32_t(f);
        return int16_t(uint16_t(uint32_t(i) & 0xFFFF));
    }
    inline uint32_t R32(uint8_t* base, uint32_t at) { return Guest::Read32(base, at); }
    inline int32_t R16s(uint8_t* base, uint32_t at) { uint16_t v; memcpy(&v, base + at, 2); return int16_t(_byteswap_ushort(v)); }

    // The walk. With `out`, the pieces are listed instead of called.
    void Walk(PPCContext& ctx, uint8_t* base, std::vector<uint32_t>* out)
    {
        const uint32_t minsAt = ctx.r3.u32, maxsAt = ctx.r4.u32, tree = ctx.r5.u32, object = ctx.r6.u32, sp = ctx.r1.u32;
        // The constants' first use sets them up, as the game does.
        constexpr uint32_t flagAt = 0x82C9E560u, clampAt = 0x82C9E550u;
        const uint32_t flag = R32(base, flagAt);
        if ((flag & 1) == 0)
        {
            Guest::Write32(base, flagAt, flag | 1);
            const uint32_t c = R32(base, 0x82085574u), w = R32(base, 0x820641A0u);
            Guest::Write32(base, clampAt, c); Guest::Write32(base, clampAt + 4, c); Guest::Write32(base, clampAt + 8, c); Guest::Write32(base, clampAt + 12, w);
        }
        const Vec origin = LoadVec(base, tree), scale = LoadVec(base, tree + 16);
        const Vec mask = LoadVec(base, 0x820640A0u), fill = LoadVec(base, 0x82063FF0u), sign = LoadVec(base, 0x820641B0u);
        const Vec clamp = LoadVec(base, clampAt);
        Vec negClamp; for (int i = 0; i < 16; i++) negClamp.b[i] = clamp.b[i] ^ sign.b[i];
        const Vec a = Perm(LoadVec(base, minsAt), fill, mask), b = Perm(LoadVec(base, maxsAt), fill, mask);
        float lo[4], hi[4];
        for (int i = 0; i < 4; i++)
        {
            const float o = Lane(origin, i), s = Lane(scale, i), c = Lane(clamp, i), n = Lane(negClamp, i);
            const float t1 = (Lane(a, i) - o) * s, t2 = (Lane(b, i) - o) * s;
            const float n1 = FMin(FMax(t1, n), c), n2 = FMin(FMax(t2, n), c);
            lo[i] = FMin(n1, n2); hi[i] = FMax(n1, n2);
        }
        const int32_t loX = Trunc16(lo[0]), loY = Trunc16(lo[1]), loZ = Trunc16(lo[2]);
        const int32_t nhX = Trunc16(-hi[0]), nhY = Trunc16(-hi[1]), nhZ = Trunc16(-hi[2]);
        const uint32_t nodes = R32(base, tree + 32), roots = R32(base, tree + 40), depthLimit = R32(base, tree + 44);
        // Lane by lane against the node's words: +0 nhX, +2 loX, +4 nhY,
        // +6 loY, +8 nhZ, +10 loZ; the child's two lanes never fail.
        const __m128i query = _mm_setr_epi16(int16_t(nhX), int16_t(loX), int16_t(nhY), int16_t(loY), int16_t(nhZ), int16_t(loZ), INT16_MIN, INT16_MIN);
        struct Item { uint32_t offset, depth; };
        // A stack a level: a piece's call can ask for another walk.
        static thread_local std::vector<Item> pool[8];
        static thread_local int level = 0;
        std::vector<Item> local;
        std::vector<Item>& stack = level < 8 ? pool[level] : local;
        level++;
        // 0.5.415: the stack as a plain array with its own top, sized once
        // for the deepest it can get (each step down leaves at most three
        // brothers waiting, so the roots and three a level), instead of
        // push_back's capacity check each time (1.1% of the main thread on
        // Marseille besides the walk's own 3.3%). A tree whose numbers say
        // more than 65536 goes the old way. COD3_CULL_VECTORSTACK=1: always
        // the old way.
        static const bool vectorStack = []() { const char* t = getenv("COD3_CULL_VECTORSTACK"); return t != nullptr && t[0] == '1'; }();
        const uint64_t bound = uint64_t(roots) + 3ull * uint64_t(depthLimit) + 4ull;
        if (!vectorStack && bound <= 65536)
        {
            if (stack.size() < bound) stack.resize(size_t(bound));
            Item* const items = stack.data();
            size_t top = 0;
            for (uint32_t i = roots; i > 0; i--) items[top++] = Item{ (i - 1) * 16, 1 };
            while (top != 0)
            {
                const Item item = items[top - 1];
                const uint32_t node = nodes + item.offset;
                const __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + node));
                const __m128i words = _mm_or_si128(_mm_slli_epi16(raw, 8), _mm_srli_epi16(raw, 8));
                if (_mm_movemask_epi8(_mm_cmpgt_epi16(query, words)) != 0) { top--; continue; }
                const uint32_t child = R32(base, node + 12);
                if (item.depth >= depthLimit)
                {
                    top--;
                    if (out != nullptr) { out->push_back(child); continue; }
                    ctx.r3.u64 = object; ctx.r4.u64 = child; ctx.r5.u64 = tree; ctx.r6.u64 = object;
                    ctx.r1.u64 = sp - 2304;
                    ctx.lr = 0x8241966C;
                    Guest::Lookup(R32(base, R32(base, object)))(ctx, base);
                    ctx.r1.u64 = sp;
                    continue;
                }
                const uint32_t depth = item.depth + 1;
                _mm_prefetch(reinterpret_cast<const char*>(base + nodes + child), _MM_HINT_T0);
                _mm_prefetch(reinterpret_cast<const char*>(base + nodes + child + 48), _MM_HINT_T0);
                items[top - 1] = Item{ child, depth };
                items[top++] = Item{ child + 16, depth };
                items[top++] = Item{ child + 32, depth };
                items[top++] = Item{ child + 48, depth };
            }
            level--;
            return;
        }
        stack.clear();
        for (uint32_t i = roots; i > 0; i--) stack.push_back(Item{ (i - 1) * 16, 1 });
        while (!stack.empty())
        {
            const Item item = stack.back();
            const uint32_t node = nodes + item.offset;
            // 0.5.398: the six tests at once (SSE2): the node's six big
            // endian 16 bit numbers turned round, compared with the query's.
            const __m128i raw = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + node));
            const __m128i words = _mm_or_si128(_mm_slli_epi16(raw, 8), _mm_srli_epi16(raw, 8));
            if (_mm_movemask_epi8(_mm_cmpgt_epi16(query, words)) != 0)
            {
                stack.pop_back();
                continue;
            }
            const uint32_t child = R32(base, node + 12);
            if (item.depth >= depthLimit)
            {
                stack.pop_back();
                if (out != nullptr) { out->push_back(child); continue; }
                ctx.r3.u64 = object; ctx.r4.u64 = child; ctx.r5.u64 = tree; ctx.r6.u64 = object;
                ctx.r1.u64 = sp - 2304;
                ctx.lr = 0x8241966C;
                Guest::Lookup(R32(base, R32(base, object)))(ctx, base);
                ctx.r1.u64 = sp;
                continue;
            }
            const uint32_t depth = item.depth + 1;
            // 0.5.404: the four children's bounds asked for now, so they
            // are in the cache when they are popped (two lines: the four
            // nodes are 64 bytes, not always on one).
            _mm_prefetch(reinterpret_cast<const char*>(base + nodes + child), _MM_HINT_T0);
            _mm_prefetch(reinterpret_cast<const char*>(base + nodes + child + 48), _MM_HINT_T0);
            stack.back() = Item{ child, depth };
            stack.push_back(Item{ child + 16, depth });
            stack.push_back(Item{ child + 32, depth });
            stack.push_back(Item{ child + 48, depth });
        }
        level--;
    }

    bool On()
    {
        static const bool on = []() { const char* t = getenv("COD3_NATIVE_CULL"); return !(t && t[0] == '0'); }();
        return on && !g_failed.load(std::memory_order_relaxed);
    }
}

// 0.5.400: the title's memcpy (sub_8231E8C0), native. 3% of the main
// thread on Marseille at 32 players (and used by every thread): the
// recompiled copy moves eight bytes a step through the guest's registers.
// Guest memory is the console's bytes in the same order, so a copy is a
// copy; r3 comes back as the destination, as the title's does. A copy
// whose destination starts inside its source (the title's forward copy
// repeats the pattern there, memcpy would not) is left to the title's own.
// COD3_NATIVE_MEMCPY=0: the title's own always.
extern "C" PPC_FUNC(__imp__sub_8231E8C0);
PPC_FUNC(sub_8231E8C0)
{
    static const bool on = []() { const char* t = getenv("COD3_NATIVE_MEMCPY"); return !(t && t[0] == '0'); }();
    const uint32_t dst = ctx.r3.u32, src = ctx.r4.u32, n = ctx.r5.u32;
    // (And anything near the device registers, 0x7F000000 up, the title's own.)
    if (!on || (dst > src && dst - src < n) || n > (64u << 20) || (dst >> 24) == 0x7F || (src >> 24) == 0x7F)
    { __imp__sub_8231E8C0(ctx, base); return; }
    if (n != 0) memmove(base + dst, base + src, n);
}

// 0.5.403: the effects update's guard. A job worker's effect update
// (sub_824472C0 -> sub_820FC850, each effect's emitters) now and then finds
// an emitter whose attached object (+200) is not an object at all: a
// vtable's own address, or an odd number, and calls through it. The bad
// call is caught, but what follows has crashed the game (0.5.384's
// "index off" run, 0.5.397, 0.5.398: every one with this same chain). Where
// the bad pointer comes from is not known yet; until it is, an emitter's
// attached object that is not a plausible object (an aligned heap address
// whose first word is a vtable in the image, whose entries are code) is
// treated as none (cleared), an emitter that is not itself plausible is
// left out of the update, and the first of each are logged with what
// surrounds them ("fxguard:"). COD3_FXGUARD=0: off.
namespace FxGuard
{
    inline bool InImage(uint32_t a) { return a >= 0x82000000u && a < 0x82E20000u; }
    inline bool InCode(uint32_t a) { return a >= 0x82000000u && a < 0x82E20000u && (a & 3u) == 0; }
    inline bool HeapAligned(uint32_t a) { return (a & 3u) == 0 && a >= 0x40000000u && a < 0x80000000u; }
    bool PlausibleObject(uint8_t* base, uint32_t object)
    {
        if (!HeapAligned(object)) return false;
        const uint32_t vtable = Guest::Read32(base, object);
        if (!InImage(vtable) || (vtable & 3u) != 0) return false;
        return InCode(Guest::Read32(base, vtable + 16)) && InCode(Guest::Read32(base, vtable + 20));
    }
    std::atomic<int> g_told{ 0 };
    void Tell(uint8_t* base, const char* what, uint32_t effect, uint32_t index, uint32_t child, uint32_t value)
    {
        if (g_told.fetch_add(1) >= 12) return;
        printf("fxguard: %s: effect %08X (def %08X, +76 %08X, %d emitters at %08X) emitter %u at %08X, value %08X", what, effect,
            Guest::Read32(base, effect + 72), Guest::Read32(base, effect + 76), int(int16_t(Guest::Read32(base, effect + 88) & 0xFFFF)),
            Guest::Read32(base, effect + 84), index, child, value);
        if (HeapAligned(value) || InImage(value)) printf(" -> %08X %08X %08X %08X", Guest::Read32(base, value), Guest::Read32(base, value + 4), Guest::Read32(base, value + 8), Guest::Read32(base, value + 12));
        printf("\n");
        fflush(stdout);
    }
}

extern "C" PPC_FUNC(__imp__sub_820FC850);
PPC_FUNC(sub_820FC850)
{
    using namespace FxGuard;
    static const bool on = []() { const char* t = getenv("COD3_FXGUARD"); return !(t && t[0] == '0'); }();
    const uint32_t effect = ctx.r3.u32;
    if (on && HeapAligned(effect))
    {
        const int count = int(int16_t(Guest::Read32(base, effect + 88) & 0xFFFF));   // lhz +90
        const uint32_t list = Guest::Read32(base, effect + 84);
        if (count > 0 && count <= 1024 && HeapAligned(list))
            for (int i = 0; i < count; i++)
            {
                const uint32_t child = Guest::Read32(base, list + 4 * uint32_t(i));
                if (child == 0) continue;
                if (!HeapAligned(child))
                {
                    Tell(base, "an emitter that is not one, left out", effect, uint32_t(i), list + 4 * uint32_t(i), child);
                    Guest::Write32(base, list + 4 * uint32_t(i), 0);
                    continue;
                }
                const uint32_t attached = Guest::Read32(base, child + 200);
                if (attached != 0 && !PlausibleObject(base, attached))
                {
                    Tell(base, "an attached object that is not one, cleared", effect, uint32_t(i), child, attached);
                    Guest::Write32(base, child + 200, 0);
                }
            }
    }
    __imp__sub_820FC850(ctx, base);
}

// 0.5.407: a freeze on Rimling at 32 players (0.5.406, 30 s in): the main
// thread in sub_82185D18, which asks the game's heap (sub_820DE7B8 through
// the cache's allocator) for a block and, refused, lets go of the cache's
// oldest entry (sub_82185B40) and asks again, for ever: the cache was
// already empty, so nothing was let go and the heap stayed full. The first
// time it happens the request and the cache are logged ("heapwait:"),
// and from then on the loop sleeps a millisecond a turn, so other threads
// that hold the heap's memory can give it back instead of the game
// spinning flat out.
namespace HeapWait
{
    thread_local uint32_t t_cache = 0, t_size = 0, t_empty = 0;
    std::atomic<int> g_told{ 0 };
    std::atomic<uint32_t> g_audits{ 0 };

    // 0.5.408: diagnostics must not demand-commit memory while inspecting
    // a possibly broken cache. These reads happen only on empty reclaims.
    bool Peek(uint8_t* base, uint32_t at, uint32_t& value)
    {
        uint32_t raw = 0;
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), base + uint64_t(at), &raw, sizeof(raw), &got) || got != sizeof(raw)) return false;
        value = _byteswap_ulong(raw);
        return true;
    }
    void Audit(PPCContext& ctx, uint8_t* base, uint32_t empty)
    {
        static const bool enabled = []() { const char* v = getenv("COD3_HEAPWAIT_AUDIT"); return !(v && v[0] == '0'); }();
        if (!enabled || (empty != 1 && empty != 100) || g_audits.fetch_add(1) >= 12) return;
        const uint32_t caller = uint32_t(ctx.lr);
        uint32_t count = 0, sizes = 0, units = 0;
        const bool phase = caller == 0x82185D80u || caller == 0x82185E34u;
        const uint32_t header = caller == 0x82185D80u ? 32u : 16u;
        const bool readable = phase && Peek(base, ctx.r28.u32, count) && Peek(base, ctx.r28.u32 + 4, sizes)
            && uint64_t(sizes) + ctx.r31.u32 <= 0xFFFFFFFCull && Peek(base, sizes + ctx.r31.u32, units);
        const uint64_t bytes = uint64_t(units) * ctx.r27.u32 + header;
        printf("heapwait: audit cache %08X empty-reclaim %u caller %08X list %08X count %u index-bytes %08X stride %u; ",
            t_cache, empty, caller, ctx.r28.u32, count, ctx.r31.u32, ctx.r27.u32);
        if (readable) printf("EXACT request %llu bytes (%u units + %u header), low32 %u, overflow %s\n",
            (unsigned long long)bytes, units, header, uint32_t(bytes), bytes > 0xFFFFFFFFull ? "YES" : "no");
        else printf("exact request unavailable (unrecognized caller or unreadable list); entry maximum %u\n", t_size);
        uint32_t allocator = 0, table = 0, allocFn = 0, freeFn = 0, active = 0;
        Peek(base, t_cache, allocator); Peek(base, allocator, table);
        Peek(base, table + 4, allocFn); Peek(base, table + 8, freeFn);
        constexpr uint32_t heaps = 0x82A84120u;  // sub_820DE188's eight 1172-byte heap descriptors
        Peek(base, heaps + 9392, active);
        printf("heapwait: allocator %08X vtable %08X alloc %08X free %08X active-game-heap %08X\n", allocator, table, allocFn, freeFn, active);
        for (uint32_t i = 0; i < 8; i++)
        {
            const uint32_t heap = heaps + i * 1172;
            uint32_t capacity = 0, used = 0, peak = 0, fallback = 0;
            if (!Peek(base, heap + 1156, capacity) || !Peek(base, heap + 1160, used)
                || !Peek(base, heap + 1164, peak) || !Peek(base, heap + 1168, fallback))
            { printf("heapwait: heap %u metadata unreadable\n", i); continue; }
            printf("heapwait: heap %u at %08X capacity %u used %u peak %u fallback %08X\n", i, heap, capacity, used, peak, fallback);
        }
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82185D18);
PPC_FUNC(sub_82185D18)
{
    const uint32_t outerCache = HeapWait::t_cache, outerSize = HeapWait::t_size, outerEmpty = HeapWait::t_empty;
    HeapWait::t_cache = ctx.r3.u32;
    const uint32_t list = ctx.r4.u32;
    const uint32_t count = Guest::Read32(base, list);
    HeapWait::t_size = count != 0 ? Guest::Read32(base, Guest::Read32(base, list + 4) + (count - 1) * 4) * ctx.r6.u32 + 32 : 0;
    HeapWait::t_empty = 0;
    __imp__sub_82185D18(ctx, base);
    HeapWait::t_cache = outerCache; HeapWait::t_size = outerSize; HeapWait::t_empty = outerEmpty;
}
extern "C" PPC_FUNC(__imp__sub_82185B40);
PPC_FUNC(sub_82185B40)
{
    if (ctx.r4.u32 == 0 && HeapWait::t_cache != 0)
    {
        const uint32_t empty = HeapWait::t_empty == UINT32_MAX ? UINT32_MAX : ++HeapWait::t_empty;
        HeapWait::Audit(ctx, base, empty);
        if (empty == 100 && HeapWait::g_told.fetch_add(1) < 4)
        {
            const uint32_t cache = HeapWait::t_cache;
            printf("heapwait: the heap refused %u bytes 100 times for cache %08X (allocator %08X, head %08X, tail %08X) with nothing left in the cache to let go; sleeping between tries from now on\n",
                HeapWait::t_size, cache, Guest::Read32(base, cache), Guest::Read32(base, cache + 4), Guest::Read32(base, cache + 8));
            fflush(stdout);
        }
        if (empty >= 100) Sleep(1);
    }
    __imp__sub_82185B40(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_824193E0);
PPC_FUNC(sub_824193E0)
{
    using namespace NativeCull;
    // The stand-in object's call, while the game's own walk is checked.
    if (t_record != nullptr) { t_record->push_back(ctx.r4.u32); return; }
    if (!On()) { __imp__sub_824193E0(ctx, base); return; }
    if (g_checked.load(std::memory_order_relaxed) < 3000)
    {
        std::vector<uint32_t> mine, game;
        Walk(ctx, base, &mine);
        // The original walk reserves 2304 bytes and calls only this intercepted
        // virtual, which returns above without using the guest stack. Keep the
        // 32-byte object below that frame, within its 224-byte ABI red zone,
        // instead of writing 24 KB below a possibly small guest stack.
        static const bool nearStack = []() { const char* v = getenv("COD3_CULL_NEARSTACK"); return !(v && v[0] == '0'); }();
        const uint32_t standIn = (ctx.r1.u32 - (nearStack ? 2304u + 128u : 0x6000u)) & ~15u;
        Guest::Write32(base, standIn, standIn + 16);
        Guest::Write32(base, standIn + 16, 0x824193E0u);
        const PPCContext saved = ctx;
        ctx.r6.u64 = standIn;
        t_record = &game;
        __imp__sub_824193E0(ctx, base);
        t_record = nullptr;
        ctx = saved;
        const uint32_t n = g_checked.fetch_add(1) + 1;
        if (mine == game) g_same.fetch_add(1);
        else
        {
            g_failed.store(true);
            printf("cull: native differs from the game's at query %u (%zu pieces, the game %zu): the game's own from now on\n", n, mine.size(), game.size());
            fflush(stdout);
            __imp__sub_824193E0(ctx, base);
            return;
        }
        if (n == 3000 || (n % 1000) == 0)
        {
            printf("cull: native matched the game's on %u of %u queries\n", g_same.load(), n);
            fflush(stdout);
        }
    }
    Walk(ctx, base, nullptr);
}



// 0.5.417: two of the world trace's busiest pieces, native (Marseille at 32
// players, 0.5.414: 1.8% and 1.6% of the main thread, the trace itself
// sub_824A0940 2.3% more). sub_824851F8 tests a segment against a box's two
// sets of faces (a slab test; 1 when it misses) and sub_824850D0 puts the
// pieces of a list that match a contents mask into three lists of 128 by
// range. Both copied step for step from the recompiled code (every value
// rounded to single precision where the original's fsubs/fmuls/fdivs are;
// the faces copied below sp as the original's vector store does), and
// checked against a transcription of it off line on 6 million segments
// and 1500 lists. The first 3000 calls of each also run the game's own and
// compare: r3 and f1 for the first, the three lists for the second; one
// difference and the game's own is used from then on ("trace native").
// COD3_NATIVE_SLAB=0 / COD3_NATIVE_SORT=0 turn them off.
namespace NativeTrace
{
    inline uint32_t R32(uint8_t* base, uint32_t at) { uint32_t v; memcpy(&v, base + at, 4); return _byteswap_ulong(v); }
    inline uint32_t R16(uint8_t* base, uint32_t at) { uint16_t v; memcpy(&v, base + at, 2); return _byteswap_ushort(v); }
    inline void W32(uint8_t* base, uint32_t at, uint32_t v) { v = _byteswap_ulong(v); memcpy(base + at, &v, 4); }
    inline double F(uint8_t* base, uint32_t at) { const uint32_t v = R32(base, at); float f; memcpy(&f, &v, 4); return double(f); }
    inline double Round(double d) { return double(float(d)); }

    // sub_824851F8: a segment against a box's two sets of faces, as the
    // recompiled code does it, step for step: every value a double held
    // at single precision (the fsubs/fmuls/fdivs), the faces copied to the
    // guest stack below sp as the original's vector store does. Returns
    // r3 (1: misses); `exitFraction` is f1 in and out.
    uint32_t Slab(uint8_t* base, uint32_t sp, uint32_t start, uint32_t end, uint32_t facesA, uint32_t facesB, double& exitFraction)
    {
        const double zero = F(base, 0x820641A0u);      // f11
        const double second = F(base, 0x8206178Cu);    // f8
        double sign = F(base, 0x825DA49Cu);            // f10
        const double margin = F(base, 0x820853E0u);    // f9
        double enter = zero;                           // f12
        const uint32_t slot = sp - 16;
        uint32_t faces = facesA;
        for (;;)
        {
            memcpy(base + (slot & ~15u), base + (faces & ~15u), 16);
            for (uint32_t i = 0; i < 3; i++)
            {
                const double face = F(base, slot + 4 * i);
                const double fromStart = Round(Round(F(base, start + 4 * i) - face) * sign);
                const double fromEnd = Round(Round(F(base, end + 4 * i) - face) * sign);
                if (fromStart > zero)
                {
                    if (fromEnd > zero) return 1;
                    const double span = Round(fromStart - fromEnd);
                    const double reach = Round(fromStart - margin);
                    if (reach > Round(span * enter))
                    {
                        enter = Round(reach / span);
                        if (!(enter < exitFraction)) return 1;
                    }
                }
                else if (fromEnd > zero)
                {
                    const double span = Round(fromStart - fromEnd);
                    if (fromStart > Round(span * exitFraction))
                    {
                        exitFraction = Round(fromStart / span);
                        if (!(enter < exitFraction)) return 1;
                    }
                }
            }
            if (sign == second) return 0;
            sign = second;
            faces = facesB;
        }
    }

    // sub_824850D0: the pieces of a list whose contents match the mask,
    // put in one of three lists (before, inside or after a range), 128 at
    // most each, as the original does.
    void Sort(uint8_t* base, uint32_t self, uint32_t mask)
    {
        const uint32_t count = R32(base, self + 1044);
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t list = R32(base, self + 1040);
            const uint32_t world = R32(base, self + 2640);
            const uint32_t table = R32(base, world + 64);
            const uint32_t item = R32(base, list + i * 4);
            if ((R32(base, table + (item & 0xFFFFu) * 36u) & mask) == 0) continue;
            const uint32_t low = R16(base, world + 56);
            uint32_t into;
            if (item < low) into = self + 1056;
            else if (item >= low + R16(base, world + 58)) into = self + 2112;
            else into = self + 1584;
            const uint32_t n = R32(base, into + 516);
            if (n == 128) continue;
            W32(base, into + 516, n + 1);
            W32(base, R32(base, into + 512) + n * 4u, item);
        }
    }
}

namespace NativeTrace
{
    std::atomic<uint32_t> g_slabChecked{ 0 }, g_sortChecked{ 0 };
    std::atomic<bool> g_slabFailed{ false }, g_sortFailed{ false };
    bool SlabOn() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_SLAB"); return !(t && t[0] == '0'); }(); return on && !g_slabFailed.load(std::memory_order_relaxed); }
    bool SortOn() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_SORT"); return !(t && t[0] == '0'); }(); return on && !g_sortFailed.load(std::memory_order_relaxed); }
}

// 0.5.419: the tree walk's (sub_824A0940's) calls, written down while the
// game's own walk runs, for the native walk to be checked against (below).
namespace NativeWalk
{
    enum : uint8_t { SiteSlabA, SiteLeafA, SiteSlabB, SiteLeafB };
    // What the walk passed (r3-r7, f1), what came back (r3, r4, f1) and the
    // trace as the call left it.
    struct Call { uint8_t site; uint32_t in[5]; uint64_t f1In; uint32_t r3, r4; uint64_t f1Out; uint8_t trace[128]; };
    thread_local std::vector<Call>* t_rec = nullptr;
    thread_local uint32_t t_trace = 0;
    thread_local int t_inGame = 0;

    int SiteOf(uint32_t lr)
    {
        switch (lr)
        {
        case 0x824A0AE0u: return SiteSlabA;
        case 0x824A0B00u: return SiteLeafA;
        case 0x824A0B58u: return SiteSlabB;
        case 0x824A0B78u: return SiteLeafB;
        default: return -1;
        }
    }
    // Calls straight from the game's walk only (a leaf test's own slab
    // tests come from elsewhere).
    // An index, not a pointer: the record can grow while the call runs.
    constexpr size_t None = ~size_t(0);
    size_t Begin(PPCContext& ctx)
    {
        if (!t_rec) return None;
        const int site = SiteOf(uint32_t(ctx.lr));
        if (site < 0) return None;
        Call& c = t_rec->emplace_back();
        c.site = uint8_t(site);
        c.in[0] = ctx.r3.u32; c.in[1] = ctx.r4.u32; c.in[2] = ctx.r5.u32; c.in[3] = ctx.r6.u32; c.in[4] = ctx.r7.u32;
        memcpy(&c.f1In, &ctx.f1.f64, 8);
        return t_rec->size() - 1;
    }
    void End(PPCContext& ctx, uint8_t* base, size_t at)
    {
        if (at == None || !t_rec || at >= t_rec->size()) return;
        Call& c = (*t_rec)[at];
        c.r3 = ctx.r3.u32; c.r4 = ctx.r4.u32;
        memcpy(&c.f1Out, &ctx.f1.f64, 8);
        memcpy(c.trace, base + t_trace, 128);
    }
}

extern "C" PPC_FUNC(__imp__sub_824851F8);
static void SlabHook(PPCContext& ctx, uint8_t* base)
{
    namespace T = NativeTrace;
    if (!T::SlabOn()) { __imp__sub_824851F8(ctx, base); return; }
    ctx.fpscr.disableFlushMode();
    double exitFraction = ctx.f1.f64;
    const uint32_t mine = T::Slab(base, ctx.r1.u32, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, exitFraction);
    if (T::g_slabChecked.load(std::memory_order_relaxed) < 3000)
    {
        __imp__sub_824851F8(ctx, base);
        const uint32_t n = T::g_slabChecked.fetch_add(1) + 1;
        if (ctx.r3.u32 != mine || memcmp(&ctx.f1.f64, &exitFraction, 8) != 0)
        {
            T::g_slabFailed.store(true);
            printf("trace native: the slab test differs from the game's at call %u (%u / %u, %.9g / %.9g): the game's own from now on\n",
                n, mine, ctx.r3.u32, exitFraction, ctx.f1.f64);
            fflush(stdout);
        }
        else if (n == 3000) { printf("trace native: the slab test matched the game's on 3000 of 3000 calls\n"); fflush(stdout); }
        return;
    }
    ctx.r3.u64 = mine;
    ctx.f1.f64 = exitFraction;
}
PPC_FUNC(sub_824851F8)
{
    const size_t c = NativeWalk::Begin(ctx);   // 0.5.419
    SlabHook(ctx, base);
    NativeWalk::End(ctx, base, c);
}

extern "C" PPC_FUNC(__imp__sub_824850D0);
PPC_FUNC(sub_824850D0)
{
    namespace T = NativeTrace;
    if (!T::SortOn()) { __imp__sub_824850D0(ctx, base); return; }
    const uint32_t self = ctx.r3.u32, mask = ctx.r4.u32;
    if (T::g_sortChecked.load(std::memory_order_relaxed) < 3000)
    {
        // The three lists before, as the game's own leaves them, and as
        // this leaves them (the game's own runs last; its result stays).
        static constexpr uint32_t Lists[3] = { 1056, 1584, 2112 };
        uint32_t before[3][129], game[3][129], mine[3][129];
        auto take = [&](uint32_t (&into)[3][129]) {
            for (int l = 0; l < 3; l++)
            {
                into[l][0] = T::R32(base, self + Lists[l] + 516);
                const uint32_t array = T::R32(base, self + Lists[l] + 512);
                for (uint32_t i = 0; i < 128; i++) into[l][i + 1] = array ? T::R32(base, array + i * 4) : 0;
            }
        };
        auto put = [&](const uint32_t (&from)[3][129]) {
            for (int l = 0; l < 3; l++)
            {
                T::W32(base, self + Lists[l] + 516, from[l][0]);
                const uint32_t array = T::R32(base, self + Lists[l] + 512);
                if (array) for (uint32_t i = 0; i < 128; i++) T::W32(base, array + i * 4, from[l][i + 1]);
            }
        };
        take(before);
        bool sane = true;
        for (int l = 0; l < 3; l++) sane = sane && before[l][0] <= 128 && T::R32(base, self + Lists[l] + 512) != 0;
        if (sane)
        {
            T::Sort(base, self, mask);
            take(mine);
            put(before);
            __imp__sub_824850D0(ctx, base);
            take(game);
            const uint32_t n = T::g_sortChecked.fetch_add(1) + 1;
            if (memcmp(mine, game, sizeof(mine)) != 0)
            {
                T::g_sortFailed.store(true);
                printf("trace native: the piece sort differs from the game's at call %u: the game's own from now on\n", n);
                fflush(stdout);
            }
            else if (n == 3000) { printf("trace native: the piece sort matched the game's on 3000 of 3000 calls\n"); fflush(stdout); }
            return;
        }
        __imp__sub_824850D0(ctx, base);
        return;
    }
    T::Sort(base, self, mask);
}

// 0.5.419: the world trace's tree walk (sub_824A0940), native: 2.3-2.6% of
// the main thread on Marseille at 32 players (0.5.418's run), the busiest
// of the trace after the world search. A node's plane splits the segment:
// one side or both are walked (the nearer first), then the node's two lists
// of pieces each go through the slab test and, if it does not miss, a leaf
// test (sub_82499F58 / sub_8249DDC8, still the game's). Same steps as the
// recompiled code: the same guest stack frames (208 bytes a level) with the
// midpoints and boxes in them where the original keeps them, values held at
// single precision where it rounds, the vector part with denormals flushed
// as the vector unit does.
// Checked: the first 3000 walks of a match run the game's own walk, which
// writes down every call it makes (its arguments, what came back and the
// trace after). The native walk then runs from the same trace against that
// record: each call it would make must be the next one written down (the
// recorded result is put back instead of calling again, so nothing runs
// twice), and it must end with the same calls and the same trace. One
// difference and the game's own from then on ("walk native:" in the log).
// COD3_NATIVE_WALK=0 turns it off.
extern "C" PPC_FUNC(__imp__sub_82499F58);
PPC_FUNC(sub_82499F58)
{
    const size_t c = NativeWalk::Begin(ctx);
    __imp__sub_82499F58(ctx, base);
    NativeWalk::End(ctx, base, c);
}
extern "C" PPC_FUNC(__imp__sub_8249DDC8);
PPC_FUNC(sub_8249DDC8)
{
    const size_t c = NativeWalk::Begin(ctx);
    __imp__sub_8249DDC8(ctx, base);
    NativeWalk::End(ctx, base, c);
}

namespace NativeWalk
{
    using NativeTrace::R32;
    using NativeTrace::W32;
    using NativeTrace::F;
    using NativeTrace::Round;
    inline void WF(uint8_t* base, uint32_t at, float f) { uint32_t v; memcpy(&v, &f, 4); W32(base, at, v); }
    // lfs then stfs: the word through a double and back, as the original moves it
    inline void MoveF(uint8_t* base, uint32_t to, uint32_t from) { WF(base, to, float(F(base, from))); }

    thread_local const std::vector<Call>* t_play = nullptr;
    thread_local size_t t_at = 0;
    thread_local bool t_bad = false;
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_WALK"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }

    // A call the walk makes: the game's function, or (checking) the next
    // call written down, compared and its result put back.
    void Make(PPCContext& ctx, uint8_t* base, uint8_t site, uint32_t trace)
    {
        if (t_play)
        {
            if (t_bad) { ctx.r3.u64 = 1; return; }
            if (t_at >= t_play->size()) { t_bad = true; ctx.r3.u64 = 1; return; }
            const Call& c = (*t_play)[t_at++];
            const bool slab = site == SiteSlabA || site == SiteSlabB;
            bool same = c.site == site && c.in[0] == ctx.r3.u32 && c.in[1] == ctx.r4.u32 && c.in[2] == ctx.r5.u32 && c.in[3] == ctx.r6.u32;
            if (!slab) same = same && c.in[4] == ctx.r7.u32;
            else same = same && memcmp(&c.f1In, &ctx.f1.f64, 8) == 0;
            if (!same) { t_bad = true; ctx.r3.u64 = 1; return; }
            ctx.r3.u64 = c.r3; ctx.r4.u64 = c.r4;
            memcpy(&ctx.f1.f64, &c.f1Out, 8);
            memcpy(base + trace, c.trace, 128);
            return;
        }
        ctx.lr = site == SiteSlabA ? 0x824A0AE0u : site == SiteLeafA ? 0x824A0B00u : site == SiteSlabB ? 0x824A0B58u : 0x824A0B78u;
        if (site == SiteLeafA) sub_82499F58(ctx, base);
        else if (site == SiteLeafB) sub_8249DDC8(ctx, base);
        else sub_824851F8(ctx, base);
    }

    // sub_824A0940(r3 trace, r4 node, f1 / f2 the piece's fractions, r7 / r8
    // its start and end), sp the caller's r1.
    void Walk(PPCContext& ctx, uint8_t* base, uint32_t trace, uint32_t node, double f1, double f2, uint32_t start, uint32_t end, uint32_t sp)
    {
        const uint32_t frame = sp - 208;
        ctx.fpscr.disableFlushMode();
        if (!(F(base, trace + 32) > f1)) return;
        if ((R32(base, node + 4) & R32(base, trace + 80)) == 0) return;
        const uint32_t axis = R32(base, node) << 2;
        const double dist = F(base, node + 12);
        const double t1 = Round(F(base, start + axis) - dist);
        const double t2 = Round(F(base, end + axis) - dist);
        const double zero = F(base, 0x820641A0u);
        uint32_t child;
        double childF1 = f1;
        uint32_t childStart = start;
        if (!(t1 < zero) && !(t2 < zero)) child = R32(base, node + 32);
        else if (!(t1 > zero) && !(t2 > zero)) child = R32(base, node + 36);
        else
        {
            const double span = Round(t1 - t2);
            const double reach = Round(f2 - f1);
            const double frac = Round(t1 / span);
            WF(base, frame + 80, float(frac));
            const double mid = Round(reach * frac + f1);
            // The midpoint, four lanes: start + (end - start) * frac, the
            // lanes as raw words (the vector loads are 16-byte aligned, as
            // the store to the frame is)
            uint32_t s[4], e[4], m[4];
            for (int i = 0; i < 4; i++) { s[i] = R32(base, (start & ~15u) + 4 * i); e[i] = R32(base, (end & ~15u) + 4 * i); }
            const float fracF = float(frac);
            ctx.fpscr.enableFlushModeUnconditional();
            const __m128 vs = _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(s)));
            const __m128 ve = _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(e)));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(m), _mm_castps_si128(_mm_add_ps(vs, _mm_mul_ps(_mm_sub_ps(ve, vs), _mm_set1_ps(fracF)))));
            ctx.fpscr.disableFlushModeUnconditional();
            for (int i = 0; i < 4; i++) W32(base, ((frame + 96) & ~15u) + 4 * i, m[i]);
            const uint32_t side = t1 < zero ? 1u : 0u;
            Walk(ctx, base, trace, R32(base, node + 4 * (8 + side)), f1, mid, start, frame + 96, frame);
            child = R32(base, node + 4 * (9 - side));
            childF1 = mid;
            childStart = frame + 96;
        }
        Walk(ctx, base, trace, child, childF1, f2, childStart, end, frame);

        for (uint32_t piece = R32(base, node + 20); piece; piece = R32(base, piece + 220))
        {
            const uint32_t mask = R32(base, trace + 80);
            if ((R32(base, R32(base, piece + 104) + 64) & mask) == 0) continue;
            ctx.fpscr.disableFlushMode();
            for (uint32_t i = 0; i < 3; i++) MoveF(base, frame + 112 + 4 * i, piece + 172 + 4 * i);
            for (uint32_t i = 0; i < 3; i++) MoveF(base, frame + 128 + 4 * i, piece + 160 + 4 * i);
            ctx.r1.u64 = frame;
            ctx.r3.u64 = trace + 96; ctx.r4.u64 = trace + 112; ctx.r5.u64 = frame + 128; ctx.r6.u64 = frame + 112;
            ctx.f1.f64 = F(base, trace + 32);
            Make(ctx, base, SiteSlabA, trace);
            if (ctx.r3.s32 != 0) continue;
            ctx.r6.u64 = ctx.r4.u32; ctx.r7.u64 = mask; ctx.r5.u64 = trace + 96; ctx.r4.u64 = trace; ctx.r3.u64 = piece;
            ctx.r1.u64 = frame;
            Make(ctx, base, SiteLeafA, trace);
        }
        for (uint32_t piece = R32(base, node + 24); piece; piece = R32(base, piece + 156))
        {
            ctx.fpscr.disableFlushMode();
            memcpy(base + ((frame + 128) & ~15u), base + ((piece + 176) & ~15u), 16);
            memcpy(base + ((frame + 112) & ~15u), base + ((piece + 192) & ~15u), 16);
            ctx.r1.u64 = frame;
            ctx.r3.u64 = trace + 96; ctx.r4.u64 = trace + 112; ctx.r5.u64 = frame + 128; ctx.r6.u64 = frame + 112;
            ctx.f1.f64 = F(base, trace + 32);
            Make(ctx, base, SiteSlabB, trace);
            if (ctx.r3.s32 != 0) continue;
            ctx.r6.u64 = trace + 112; ctx.r7.u64 = R32(base, trace + 80); ctx.r5.u64 = trace + 96; ctx.r4.u64 = trace; ctx.r3.u64 = piece;
            ctx.r1.u64 = frame;
            Make(ctx, base, SiteLeafB, trace);
        }
    }
}

extern "C" PPC_FUNC(__imp__sub_824A0940);
PPC_FUNC(sub_824A0940)
{
    namespace W = NativeWalk;
    if (W::t_inGame || W::t_play || !W::On()) { __imp__sub_824A0940(ctx, base); return; }
    const uint32_t trace = ctx.r3.u32, node = ctx.r4.u32, start = ctx.r7.u32, end = ctx.r8.u32, sp = ctx.r1.u32;
    const double f1 = ctx.f1.f64, f2 = ctx.f2.f64;
    if (W::g_checked.load(std::memory_order_relaxed) < 3000)
    {
        static thread_local std::vector<W::Call> record;
        record.clear();
        uint8_t before[128], game[128];
        memcpy(before, base + trace, 128);
        // The game's own walk, its calls written down
        W::t_rec = &record; W::t_trace = trace; W::t_inGame++;
        __imp__sub_824A0940(ctx, base);
        W::t_inGame--; W::t_rec = nullptr;
        memcpy(game, base + trace, 128);
        static thread_local PPCContext after;
        after = ctx;
        // The native walk from the same trace, against the record
        memcpy(base + trace, before, 128);
        W::t_play = &record; W::t_at = 0; W::t_bad = false;
        W::Walk(ctx, base, trace, node, f1, f2, start, end, sp);
        W::t_play = nullptr;
        const bool same = !W::t_bad && W::t_at == record.size() && memcmp(game, base + trace, 128) == 0;
        memcpy(base + trace, game, 128);
        ctx = after;
        ctx.fpscr.setcsr(ctx.fpscr.csr);   // the float modes as the game's walk left them
        const uint32_t n = W::g_checked.fetch_add(1) + 1;
        if (!same)
        {
            W::g_failed.store(true);
            printf("walk native: the tree walk differs from the game's at walk %u (%zu of %zu calls the same%s): the game's own from now on\n",
                n, W::t_at, record.size(), W::t_bad ? ", then a different one" : "");
            fflush(stdout);
        }
        else if (n == 3000) { printf("walk native: the tree walk matched the game's on 3000 of 3000 walks (%zu calls in the last)\n", record.size()); fflush(stdout); }
        return;
    }
    W::Walk(ctx, base, trace, node, f1, f2, start, end, sp);
    ctx.r1.u64 = sp;
}

// 0.5.420: a segment against a box (sub_82490A30), native: 0.9% of the main
// thread on Marseille at 32 players (0.5.419's run; called by the trace's
// pieces, sub_82497958 and sub_824972D0). r3 / r4 the segment's ends, r5 /
// r6 and r7 / r8 two boxes' corners. First, with the vector unit (denormals
// flushed): if any of x, y, z of max(r7 - r6, r5 - r8) reaches the limit at
// 0x820641C0, 0 (apart). Then the segment's enter and exit fractions
// against the faces of r7 (z, y, x) and of r8 (z, y, x), single precision
// as the original rounds: 0 as soon as enter is not below exit, else 1.
// It writes nothing. The first 5000 calls of a match also run the game's
// own and compare ("box native:" in the log); one difference and the
// game's own from then on. COD3_NATIVE_BOX=0 turns it off.
namespace NativeBox
{
    using NativeTrace::R32;
    using NativeTrace::F;
    using NativeTrace::Round;
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_BOX"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }

    inline __m128 Load(uint8_t* base, uint32_t at)
    {
        uint32_t w[4];
        for (int i = 0; i < 4; i++) w[i] = R32(base, (at & ~15u) + 4 * i);
        return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(w)));
    }

    uint32_t Test(PPCContext& ctx, uint8_t* base, uint32_t s, uint32_t e, uint32_t a, uint32_t b, uint32_t lo, uint32_t hi)
    {
        ctx.fpscr.enableFlushMode();
        const __m128 gap = _mm_max_ps(_mm_sub_ps(Load(base, lo), Load(base, b)), _mm_sub_ps(Load(base, a), Load(base, hi)));
        const __m128 reach = _mm_cmpge_ps(gap, Load(base, 0x820641C0u));
        if ((_mm_movemask_ps(reach) & 7) != 0) return 0;   // lanes x, y, z (w is z again in the original)
        ctx.fpscr.disableFlushMode();
        const double zero = F(base, 0x820641A0u), margin = F(base, 0x820853E0u);
        double enter = zero, exit = F(base, 0x8206178Cu);
        for (int i = 2; i >= 0; i--)
        {
            const double face = F(base, lo + 4 * i);
            const double fromStart = Round(face - F(base, s + 4 * i));
            const double fromEnd = Round(face - F(base, e + 4 * i));
            if (fromStart > zero)
            {
                const double span = Round(fromStart - fromEnd);
                if (!(fromStart > Round(span * enter + margin))) continue;
                enter = Round(Round(fromStart - margin) / span);
            }
            else
            {
                if (!(fromEnd > zero)) continue;
                const double span = Round(fromStart - fromEnd);
                if (!(fromStart > Round(span * exit))) continue;
                exit = Round(fromStart / span);
            }
            if (!(enter < exit)) return 0;
        }
        for (int i = 2; i >= 0; i--)
        {
            const double face = F(base, hi + 4 * i);
            const double fromStart = Round(F(base, s + 4 * i) - face);
            const double fromEnd = Round(F(base, e + 4 * i) - face);
            if (fromStart > zero)
            {
                const double span = Round(fromStart - fromEnd);
                if (!(fromStart > Round(span * enter))) continue;
                enter = Round(fromStart / span);
            }
            else
            {
                if (!(fromEnd > zero)) continue;
                const double span = Round(fromStart - fromEnd);
                if (!(fromStart > Round(span * exit))) continue;
                exit = Round(fromStart / span);
            }
            if (!(enter < exit)) return 0;
        }
        return 1;
    }
}

extern "C" PPC_FUNC(__imp__sub_82490A30);
PPC_FUNC(sub_82490A30)
{
    namespace B = NativeBox;
    if (!B::On()) { __imp__sub_82490A30(ctx, base); return; }
    const uint32_t mine = B::Test(ctx, base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    if (B::g_checked.load(std::memory_order_relaxed) < 5000)
    {
        __imp__sub_82490A30(ctx, base);
        const uint32_t n = B::g_checked.fetch_add(1) + 1;
        if (ctx.r3.u32 != mine)
        {
            B::g_failed.store(true);
            printf("box native: the box test differs from the game's at call %u (%u / %u): the game's own from now on\n", n, mine, ctx.r3.u32);
            fflush(stdout);
        }
        else if (n == 5000) { printf("box native: the box test matched the game's on 5000 of 5000 calls\n"); fflush(stdout); }
        return;
    }
    ctx.r3.u64 = mine;
}

// 0.5.421: a moving box against a box's faces (sub_8248BB48), native: 1.1%
// of the main thread on Marseille at 32 players (0.5.420's run; called from
// sub_824972D0 and sub_82497958, the trace's pieces). r3 the trace (start
// +0, end +16, the moving box's half sizes +80, the fraction so far +288),
// r4 and r5 the two sets of faces: the first with the sign at 0x825DA49C,
// the second with 1. Per face the segment's distances (less the half size),
// then the enter and exit fractions as in the slab test; 0 as soon as they
// cross (or both ends are beyond a face and the end too near). On the way out: a
// hit writes the enter fraction at +288 and the face's normal at +272;
// none, +317 = 1 and, if no face had the end beyond it, +288 = 0 and
// +316 = 1.
// Same steps and rounding as the recompiled code (its scratch below r1 is
// not kept: nothing reads it after the return). The first 5000 calls of a
// match also run the game's own from the same trace and compare r3 and
// what was written ("box sweep native:" in the log); one difference and the
// game's own from then on. COD3_NATIVE_SWEEP=0 turns it off.
namespace NativeSweep
{
    using NativeTrace::R32;
    using NativeTrace::W32;
    using NativeTrace::F;
    using NativeTrace::Round;
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_SWEEP"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }
    inline void WF(uint8_t* base, uint32_t at, float f) { uint32_t v; memcpy(&v, &f, 4); W32(base, at, v); }
    inline double Lane(uint8_t* base, uint32_t vec, int i) { return F(base, (vec & ~15u) + 4 * uint32_t(i)); }

    uint32_t Sweep(PPCContext& ctx, uint8_t* base, uint32_t trace, uint32_t facesA, uint32_t facesB, uint32_t sp)
    {
        ctx.fpscr.disableFlushMode();
        // the original's stores into the caller's parameter area
        W32(base, sp + 20, trace); W32(base, sp + 28, facesA); W32(base, sp + 36, facesB);
        const double zero = F(base, 0x820641A0u), margin = F(base, 0x820853E0u);
        double exit = Round(F(base, trace + 288));
        double enter = zero;
        double sign = F(base, 0x825DA49Cu);
        bool hit = false, endInside = true;   // r7: no face had the end beyond it
        uint32_t normal[4];
        for (int i = 0; i < 4; i++) normal[i] = R32(base, 0x820641C0u + 4 * i);
        uint32_t faces = facesA;
        for (int pass = 0; pass < 2; pass++)
        {
            for (int i = 0; i < 3; i++)
            {
                const double half = F(base, trace + 80 + 4 * i);
                const double face = Lane(base, faces, i);
                const double toStart = Round(Lane(base, trace, i) - face);
                const double toEnd = Round(Lane(base, trace + 16, i) - face);
                const double d1 = Round(toStart * sign - half);
                const double d2 = Round(toEnd * sign - half);
                if (d1 > zero)
                {
                    const double span = Round(d1 - d2);
                    if (d2 > zero)
                    {
                        if (!(span > zero)) return 0;
                        if (!(d2 < margin)) return 0;
                        endInside = false;
                    }
                    const double t = Round(span * enter);
                    const double reach = Round(d1 - margin);
                    if (reach > t)
                    {
                        enter = Round(reach / span);
                        if (!(enter < exit)) return 0;
                    }
                    else if (hit) continue;
                    for (int k = 0; k < 4; k++) normal[k] = R32(base, 0x820641C0u + 4 * k);
                    float s = float(sign); memcpy(&normal[i], &s, 4);
                    hit = true;
                }
                else
                {
                    if (!(d2 > zero)) continue;
                    const double span = Round(d1 - d2);
                    endInside = false;
                    const double t = Round(span * exit);
                    if (!(d1 > t)) continue;
                    exit = Round(d1 / span);
                    if (!(enter < exit)) return 0;
                }
            }
            sign = F(base, 0x8206178Cu);
            faces = facesB;
        }
        if (hit)
        {
            WF(base, trace + 288, float(enter));
            for (int k = 0; k < 4; k++) W32(base, ((trace + 272) & ~15u) + 4 * k, normal[k]);
            return 1;
        }
        base[trace + 317] = 1;
        if (endInside)
        {
            WF(base, trace + 288, float(zero));
            base[trace + 316] = 1;
        }
        return 1;
    }
}

extern "C" PPC_FUNC(__imp__sub_8248BB48);
PPC_FUNC(sub_8248BB48)
{
    namespace S = NativeSweep;
    if (!S::On()) { __imp__sub_8248BB48(ctx, base); return; }
    const uint32_t trace = ctx.r3.u32, a = ctx.r4.u32, b = ctx.r5.u32, sp = ctx.r1.u32;
    if (S::g_checked.load(std::memory_order_relaxed) < 5000)
    {
        uint8_t before[48], mine[48], game[48];
        const uint32_t area = (trace + 272) & ~15u;
        memcpy(before, base + area, 48);
        const uint32_t r3 = S::Sweep(ctx, base, trace, a, b, sp);
        memcpy(mine, base + area, 48);
        memcpy(base + area, before, 48);
        __imp__sub_8248BB48(ctx, base);
        memcpy(game, base + area, 48);
        const uint32_t n = S::g_checked.fetch_add(1) + 1;
        if (ctx.r3.u32 != r3 || memcmp(mine, game, 48) != 0)
        {
            S::g_failed.store(true);
            printf("box sweep native: differs from the game's at call %u (r3 %u / %u, written %s): the game's own from now on\n",
                n, r3, ctx.r3.u32, memcmp(mine, game, 48) ? "different" : "the same");
            fflush(stdout);
        }
        else if (n == 5000) { printf("box sweep native: matched the game's on 5000 of 5000 calls\n"); fflush(stdout); }
        return;
    }
    ctx.r3.u64 = S::Sweep(ctx, base, trace, a, b, sp);
}

// 0.5.422: a stream of packed vectors unpacked (sub_82181520), native: 1.65%
// of the main thread on Marseille at 32 players (0.5.421's run; called from
// sub_82189380). Three stream positions are rounded up (*r6 to 2 bytes,
// *(r4+8) to 4, *(r4+12) to 4 and then 4 more); then for each of the
// *(*(r4+4)+40) entries: four big-endian 16-bit values from the stream at
// *r6, each made a float as the recompiled vupkd3d128 does (the bits of 3.0
// plus the value), times the vector at 0x82028C50 plus the one at
// 0x82028C60 (denormals flushed, as the vector unit does), stored at the
// entry's address from the table (*(r8 + 4 * (*(*(r4+4)+36) + i)) + r5).
// The positions are written back and r3 is the last count. The generated
// code put together each 8-byte read from two partial vector loads and
// the unpack lane by lane.
// Checked: the first 3000 calls of a match first work out, without writing
// anything, what they would store; then the game's own runs and each store,
// the three positions and r3 must be the same ("unpack native:" in the
// log); one difference and the game's own from then on. COD3_NATIVE_UNPACK=0
// turns it off.
namespace NativeUnpack
{
    using NativeTrace::R32;
    using NativeTrace::W32;
    using NativeTrace::R16;
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_UNPACK"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }
    struct Store { uint32_t at; uint32_t w[4]; };

    inline __m128 Vec(uint8_t* base, uint32_t at)
    {
        uint32_t w[4];
        for (int i = 0; i < 4; i++) w[i] = R32(base, (at & ~15u) + 4 * i);
        return _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(w)));
    }

    // dry: nothing written, the stores and the end positions given back
    void Unpack(PPCContext& ctx, uint8_t* base, bool dry, std::vector<Store>* stores, uint32_t (&ends)[3])
    {
        const uint32_t r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32, r8 = ctx.r8.u32;
        const uint32_t posAt = r6, wordsAt = R32(base, r4 + 8), countAt = R32(base, r4 + 12);
        uint32_t pos = (R32(base, posAt) + 1) & ~1u;
        uint32_t count = ((R32(base, countAt) + 3) & ~3u) + 4;
        uint32_t words = (R32(base, wordsAt) + 3) & ~3u;
        if (!dry) { W32(base, posAt, pos); W32(base, countAt, count); W32(base, wordsAt, words); }
        const __m128 scale = Vec(base, 0x82028C50u), bias = Vec(base, 0x82028C60u);
        uint32_t list = R32(base, r4 + 4);
        for (int32_t i = 0; i < int32_t(R32(base, list + 40)); i++)
        {
            const uint32_t to = R32(base, r8 + ((R32(base, list + 36) + uint32_t(i)) << 2)) + r5;
            count++;
            words += 4;
            uint32_t w[4];
            for (int k = 0; k < 4; k++) w[k] = 0x40400000u + uint32_t(int32_t(int16_t(R16(base, pos + 2 * k))));
            pos += 8;
            ctx.fpscr.enableFlushMode();
            const __m128 v = _mm_castsi128_ps(_mm_loadu_si128(reinterpret_cast<const __m128i*>(w)));
            Store s;
            s.at = to & ~15u;
            _mm_storeu_si128(reinterpret_cast<__m128i*>(s.w), _mm_castps_si128(_mm_add_ps(_mm_mul_ps(v, scale), bias)));
            if (dry) stores->push_back(s);
            else for (int k = 0; k < 4; k++) W32(base, s.at + 4 * k, s.w[k]);
            list = R32(base, r4 + 4);
        }
        if (!dry) { W32(base, posAt, pos); W32(base, wordsAt, words); W32(base, countAt, count); }
        ends[0] = pos; ends[1] = words; ends[2] = count;
        ctx.r3.u64 = count;
    }
}

extern "C" PPC_FUNC(__imp__sub_82181520);
PPC_FUNC(sub_82181520)
{
    namespace U = NativeUnpack;
    if (!U::On()) { __imp__sub_82181520(ctx, base); return; }
    uint32_t ends[3];
    if (U::g_checked.load(std::memory_order_relaxed) < 3000)
    {
        static thread_local std::vector<U::Store> stores;
        stores.clear();
        const uint32_t posAt = ctx.r6.u32, wordsAt = U::R32(base, ctx.r4.u32 + 8), countAt = U::R32(base, ctx.r4.u32 + 12);
        static thread_local PPCContext entry;
        entry = ctx;
        U::Unpack(entry, base, true, &stores, ends);
        const uint32_t mineR3 = entry.r3.u32;
        ctx.fpscr.setcsr(ctx.fpscr.csr);   // the float modes as ctx knows them
        __imp__sub_82181520(ctx, base);
        bool same = ctx.r3.u32 == mineR3 && U::R32(base, posAt) == ends[0] && U::R32(base, wordsAt) == ends[1] && U::R32(base, countAt) == ends[2];
        // the last store to each address is what memory holds now
        for (size_t i = 0; same && i < stores.size(); i++)
        {
            bool later = false;
            for (size_t j = i + 1; j < stores.size() && !later; j++) later = stores[j].at == stores[i].at;
            if (later) continue;
            for (int k = 0; k < 4; k++) same = same && U::R32(base, stores[i].at + 4 * k) == stores[i].w[k];
        }
        ctx.fpscr.setcsr(ctx.fpscr.csr);
        const uint32_t n = U::g_checked.fetch_add(1) + 1;
        if (!same)
        {
            U::g_failed.store(true);
            printf("unpack native: differs from the game's at call %u (%zu stores, r3 %u / %u): the game's own from now on\n", n, stores.size(), mineR3, ctx.r3.u32);
            fflush(stdout);
        }
        else if (n == 3000) { printf("unpack native: matched the game's on 3000 of 3000 calls (%zu stores in the last)\n", stores.size()); fflush(stdout); }
        return;
    }
    U::Unpack(ctx, base, false, nullptr, ends);
}

// 0.5.423: a search of a table by a 32-byte key (sub_820B64D0), native:
// 1.1% of the main thread on Marseille at 32 players (0.5.422's run; called
// from sub_8253C130, sub_8253B130, sub_82542DB8 and sub_8253CC70). The
// table at *(r3+116) holds *(r3+112) entries of 48 bytes; the first whose
// first eight words equal the key at r5 is the answer, written at r4 as
// the pair (r3, its index); none, (0, the word that was in the stack slot
// the original builds the pair in). Equal words are equal bytes, so each
// entry is two 16-byte compares instead of eight loads byte-swapped one by
// one. The first 3000 calls of a match also run the game's own and compare
// the pair ("table search native:" in the log); one difference and the
// game's own from then on. COD3_NATIVE_TABLESEARCH=0 turns it off.
namespace NativeTableSearch
{
    using NativeTrace::R32;
    using NativeTrace::W32;
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_TABLESEARCH"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }

    void Find(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t self = ctx.r3.u32, out = ctx.r4.u32, key = ctx.r5.u32, sp = ctx.r1.u32;
        const uint64_t r31 = _byteswap_uint64(ctx.r31.u64);
        memcpy(base + sp - 8, &r31, 8);   // the original keeps r31 there
        W32(base, sp - 16, 0);
        memcpy(base + out, base + sp - 16, 8);
        const int32_t count = int32_t(R32(base, self + 112));
        if (count <= 0) return;
        const uint32_t table = R32(base, self + 116);
        const __m128i k0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + key));
        const __m128i k1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + key + 16));
        for (int32_t i = 0; i < count; i++)
        {
            const uint8_t* e = base + uint32_t(table + 48u * uint32_t(i));
            const __m128i a = _mm_cmpeq_epi8(k0, _mm_loadu_si128(reinterpret_cast<const __m128i*>(e)));
            const __m128i b = _mm_cmpeq_epi8(k1, _mm_loadu_si128(reinterpret_cast<const __m128i*>(e + 16)));
            if (_mm_movemask_epi8(_mm_and_si128(a, b)) != 0xFFFF) continue;
            W32(base, sp - 16, self);
            W32(base, sp - 12, uint32_t(i));
            memcpy(base + out, base + sp - 16, 8);
            return;
        }
    }
}

extern "C" PPC_FUNC(__imp__sub_820B64D0);
PPC_FUNC(sub_820B64D0)
{
    namespace T = NativeTableSearch;
    if (!T::On()) { __imp__sub_820B64D0(ctx, base); return; }
    if (T::g_checked.load(std::memory_order_relaxed) < 3000)
    {
        const uint32_t out = ctx.r4.u32;
        uint8_t mine[8], game[8];
        T::Find(ctx, base);
        memcpy(mine, base + out, 8);
        __imp__sub_820B64D0(ctx, base);
        memcpy(game, base + out, 8);
        const uint32_t n = T::g_checked.fetch_add(1) + 1;
        if (memcmp(mine, game, 8) != 0)
        {
            T::g_failed.store(true);
            printf("table search native: differs from the game's at call %u (%08X %08X / %08X %08X): the game's own from now on\n",
                n, T::R32(mine, 0), T::R32(mine, 4), T::R32(game, 0), T::R32(game, 4));
            fflush(stdout);
        }
        else if (n == 3000) { printf("table search native: matched the game's on 3000 of 3000 calls\n"); fflush(stdout); }
        return;
    }
    T::Find(ctx, base);
}

// 0.5.424: a rotate of 8-byte elements (sub_82147008, std::rotate's
// gcd-cycles form: r3 first, r4 middle, r5 last), native: 0.75% of the main
// thread on Marseille at 32 players (0.5.423's run; called from
// sub_82147498, a merge). The elements are moved whole (two words each),
// never looked at, so the result is the same bytes however they are moved:
// the first k put aside, the rest moved down, the k after them. The
// original's gcd takes a division per step. Only 0 < k < n (the cases the
// original moves anything in, with sane sizes); anything else is the
// game's own. The first 3000 calls of a match run the game's own first;
// the range is put back, rotated natively and compared byte for byte
// ("rotate native:" in the log); one difference and the game's own from
// then on. COD3_NATIVE_ROTATE=0 turns it off.
namespace NativeRotate
{
    std::atomic<uint32_t> g_checked{ 0 };
    std::atomic<bool> g_failed{ false };
    bool On() { static const bool on = []() { const char* t = getenv("COD3_NATIVE_ROTATE"); return !(t && t[0] == '0'); }(); return on && !g_failed.load(std::memory_order_relaxed); }

    void Rotate(uint8_t* base, uint32_t first, uint32_t k, uint32_t n)
    {
        static thread_local std::vector<uint8_t> aside;
        aside.resize(size_t(k) * 8);
        memcpy(aside.data(), base + first, size_t(k) * 8);
        memmove(base + first, base + first + size_t(k) * 8, size_t(n - k) * 8);
        memcpy(base + first + size_t(n - k) * 8, aside.data(), size_t(k) * 8);
    }
}

extern "C" PPC_FUNC(__imp__sub_82147008);
PPC_FUNC(sub_82147008)
{
    namespace X = NativeRotate;
    const uint32_t first = ctx.r3.u32, middle = ctx.r4.u32, last = ctx.r5.u32;
    const int32_t k = int32_t(middle - first) >> 3, n = int32_t(last - first) >> 3;
    if (!X::On() || k <= 0 || k >= n || n > (1 << 20) || ((middle - first) & 7) || ((last - first) & 7)) { __imp__sub_82147008(ctx, base); return; }
    if (X::g_checked.load(std::memory_order_relaxed) < 3000)
    {
        static thread_local std::vector<uint8_t> before, game;
        const size_t bytes = size_t(n) * 8;
        before.assign(base + first, base + first + bytes);
        __imp__sub_82147008(ctx, base);
        game.assign(base + first, base + first + bytes);
        memcpy(base + first, before.data(), bytes);
        X::Rotate(base, first, uint32_t(k), uint32_t(n));
        const uint32_t c = X::g_checked.fetch_add(1) + 1;
        if (memcmp(base + first, game.data(), bytes) != 0)
        {
            memcpy(base + first, game.data(), bytes);
            X::g_failed.store(true);
            printf("rotate native: differs from the game's at call %u (%d of %d): the game's own from now on\n", c, k, n);
            fflush(stdout);
        }
        else if (c == 3000) { printf("rotate native: matched the game's on 3000 of 3000 calls\n"); fflush(stdout); }
        return;
    }
    X::Rotate(base, first, uint32_t(k), uint32_t(n));
}

// 0.5.376: the animation's quaternion blend (sub_820B5CD8), native. 3.7%
// of the main thread on Marseille at 32 players (every bone of every
// soldier blended each frame). out = q0 * a + q1 * b: a and b straight
// from (1 - t, t) when the two are close, else from polynomial sines of
// the angle between them (a polynomial arc cosine for the angle), with
// q1 turned round when the two point apart. Same steps, same rounding as
// the recompiled code; the first 5000 blends also run the game's own and
// compare the results bit for bit (logged); one difference and the game's
// own is used from then on. COD3_NATIVE_SLERP=0 turns it off.
namespace NativeSlerp
{
    inline float F(uint8_t* base, uint32_t at) { const uint32_t v = Guest::Read32(base, at); float f; memcpy(&f, &v, 4); return f; }
    inline void W(uint8_t* base, uint32_t at, float f) { uint32_t v; memcpy(&v, &f, 4); Guest::Write32(base, at, v); }
    inline float Mul(float a, float b) { return float(double(a) * double(b)); }
    std::atomic<uint32_t> g_checked{ 0 }, g_same{ 0 };
    std::atomic<bool> g_failed{ false };

    void Blend(uint8_t* base, uint32_t q0At, uint32_t q1At, double t, float out[4])
    {
        float q0[4], q1[4];
        for (int i = 0; i < 4; i++) { q0[i] = F(base, (q0At & ~15u) + 4 * i); q1[i] = F(base, (q1At & ~15u) + 4 * i); }
        // vmsum4fp: the four products, summed in pairs, as the SSE dot does.
        const float dot = (q0[0] * q1[0] + q0[1] * q1[1]) + (q0[2] * q1[2] + q0[3] * q1[3]);
        const float zero = F(base, 0x820641A0u), one = F(base, 0x8206178Cu);
        float x = dot;
        float w[4] = { float(double(one) - t), float(t), one, zero };
        if (dot < zero) { w[1] = float(-t); x = -dot; }
        if (x < F(base, 0x82085688u))
        {
            const float c1 = F(base, 0x82062C94u);
            float angle;
            if (x < c1)
            {
                const float x2 = Mul(x, x), x3 = Mul(x2, x), x5 = Mul(x3, x2), x7 = Mul(x5, x2);
                const float p5 = Mul(x5, F(base, 0x82085680u));
                float s = float(double(x7) * double(F(base, 0x82085684u)) - double(p5));
                s = float(-(double(x3) * double(F(base, 0x8208567Cu)) - double(s)));
                s = float(double(s) - double(x));
                angle = float(double(s) + double(F(base, 0x82085678u)));
            }
            else
            {
                float r = float(double(one) - double(x));
                r = Mul(r, c1);
                r = float(std::sqrt(double(r)));
                const float r2 = Mul(r, r), r3 = Mul(r2, r), r5 = Mul(r3, r2), r7 = Mul(r5, r2);
                const float p5 = Mul(r5, F(base, 0x82681E60u));
                float s = float(double(r7) * double(F(base, 0x82085674u)) + double(p5));
                s = float(double(r3) * double(F(base, 0x82085670u)) + double(s));
                angle = float(double(r) * double(F(base, 0x8268362Cu)) + double(s));
            }
            // The coefficients' vector: guest lanes 0, 1 and 2 for x^3, x^5, x^7.
            const float k3 = F(base, 0x82063F50u), k5 = F(base, 0x82063F54u), k7 = F(base, 0x82063F58u);
            float v[4];
            for (int i = 0; i < 4; i++)
            {
                const float a = w[i] * angle;
                const float a2 = a * a, a3 = a2 * a, a5 = a2 * a3, a7 = a2 * a5;
                float s = a7 * k7 + a;
                s = a5 * k5 + s;
                s = a3 * k3 + s;
                v[i] = s;
            }
            const float inv = float(double(one) / double(v[2]));
            for (int i = 0; i < 4; i++) w[i] = v[i] * inv;
        }
        for (int i = 0; i < 4; i++) out[i] = q1[i] * w[1] + q0[i] * w[0];
    }

    bool On()
    {
        static const bool on = []() { const char* t = getenv("COD3_NATIVE_SLERP"); return !(t && t[0] == '0'); }();
        return on && !g_failed.load(std::memory_order_relaxed);
    }
}

extern "C" PPC_FUNC(__imp__sub_820B5CD8);
PPC_FUNC(sub_820B5CD8)
{
    using namespace NativeSlerp;
    if (!On()) { __imp__sub_820B5CD8(ctx, base); return; }
    const uint32_t outAt = ctx.r3.u32 & ~15u;
    float out[4];
    Blend(base, ctx.r4.u32, ctx.r5.u32, ctx.f1.f64, out);
    if (g_checked.load(std::memory_order_relaxed) < 5000)
    {
        __imp__sub_820B5CD8(ctx, base);
        bool same = true;
        for (int i = 0; i < 4; i++) { const float g = F(base, outAt + 4 * i); if (memcmp(&g, &out[i], 4) != 0) same = false; }
        const uint32_t n = g_checked.fetch_add(1) + 1;
        if (same) g_same.fetch_add(1);
        else
        {
            g_failed.store(true);
            printf("slerp: native differs from the game's at blend %u (t %g): game %g %g %g %g, native %g %g %g %g; the game's own from now on\n", n, ctx.f1.f64,
                F(base, outAt), F(base, outAt + 4), F(base, outAt + 8), F(base, outAt + 12), out[0], out[1], out[2], out[3]);
            fflush(stdout);
        }
        if (n == 5000 || (n % 1000) == 0) { printf("slerp: native matched the game's on %u of %u blends\n", g_same.load(), n); fflush(stdout); }
        return;   // the game's own result stands while checking
    }
    for (int i = 0; i < 4; i++) W(base, outAt + 4 * i, out[i]);
}


void RosterTrapReport() { RosterTrap::Report(); }
#endif
