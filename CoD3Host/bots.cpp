// Bots: players on the host with no one behind them (a PC addition,
// multiplayer only). The first step: a bot that joins and stands.
//
// How a player is added (from the title's own join, sub_824046D8, called by
// the session layer's HandlePlayerSetup for a player who has joined through
// the network): a free slot of the server's 24 clients (4992 bytes each, the
// array's address at +8 of svs, 0x82B3FDE0; the slot's state at +0, zero is
// free) is taken and set up (sub_82532540: its player entity), the player is
// connected into the world (sub_82593608, with where to stand and which way
// to face), the state made 1, the player's game record cleared in the places
// the title clears it, and the player begun (sub_8257A6C8). A player who
// joined over the network is then sent the game state (sub_82530720); a bot
// has no one to send it to, and nothing is ever sent to its slot
// (SV_SendClientSnapshot, sub_825320D0, is skipped for it).
//
// How a player is moved: every frame the game runs each player
// (ClientThink, sub_82593518) on the latest command in that player's ring
// of 64 (48 bytes each, at +1688 of the player's 6352 bytes at 0x82C2ECF0,
// the newest's number at +4760), which SV_GetUsercmd (sub_82530270) reads.
// A bot's commands will be written there by its brain; for now it stands.
//
// For trying: debug.txt "bot_add <n>" adds bots on the host until there are
// n. See docs/bots-notes.md.

#ifdef COD3_MP

#include <algorithm>
#include "player_capacity.h"
#include <atomic>
#include <cstdio>
// The console's "bot_add <n>" (kernel_video.cpp): bots added until there are
// n, as debug.txt's bot_add does (the larger of the two counts).
static std::atomic<int> g_consoleBots{0};
void BotsAsk(int count) { g_consoleBots.store(std::clamp(count, 0, int(PlayerCapacity::MaxBotsNow()))); }   // 0.5.283: MaxBotsNow is 31 at 32 (all slots but the host's)
// 0.5.92: the console's bot_setup N, for finding what zooms the host's view
// when bots are added: bit 1 leaves out the kit menu's callback, bit 2 the
// side switch. 1 (the kit left out) to begin with.
static std::atomic<int> g_botSetup{1};
static std::atomic<bool> g_botCamera{true};
static std::atomic<bool> g_botTactics{true};
// 0.5.130: each bot given a session player once added (bot_session 0/1).
static std::atomic<bool> g_botAutoSession{true};
// 0.5.135: a bot's session player with its own machine object (bot_remote 0/1).
static std::atomic<bool> g_botRemote{true};   // 0.5.145: on again (any number of bots), drawn by the hooks at the end
// 0.5.137: a bot takes a free local place of the session (bot_local 0/1).
static std::atomic<bool> g_botLocal{false};   // 0.5.145: off (local places are only 3)
// 0.5.163: the bot's own machine object marked "not on this machine" (+0x38 = 0)
// as 0.5.135-0.5.162 did; off: marked local as the host's.
static std::atomic<bool> g_botRemoteFlag{false};
void BotsLocal(int enabled) { g_botLocal.store(enabled != 0); printf("bots: local places for new bots %s\n", enabled ? "on" : "off"); fflush(stdout); }   // 0.5.136: off (Justin: a remote bot is invisible again)
void BotsRemote(int enabled) { g_botRemote.store(enabled != 0); printf("bots: own machine object for new bots %s\n", enabled ? "on" : "off"); fflush(stdout); }
void BotsAutoSession(int enabled) { g_botAutoSession.store(enabled != 0); printf("bots: automatic session players %s\n", enabled ? "on" : "off"); fflush(stdout); }
void BotsTactics(int enabled) { g_botTactics.store(enabled != 0); printf("bots: cover/head aim/collision routing %s\n", enabled ? "on" : "off"); fflush(stdout); }
void BotsCamera(int enabled) { g_botCamera.store(enabled != 0); printf("bots: server-only setup and isolated context %s\n", enabled ? "on" : "off"); fflush(stdout); }
static uint32_t g_viewWatchFrom = 0;   // when the host's view is watched from (0.5.91)
void BotsSetup(int bits) { g_botSetup.store(bits); printf("bots: setup %d (kit %s, side %s)\n", bits, (bits & 1) ? "left out" : "given", (bits & 2) ? "left out" : "switched"); fflush(stdout); }

#include "debug_probe.h"
#include "dom.h"
#include "settings.h"
#include "kernel.h"

uint32_t MpSessionPlayerAddress(uint8_t* base, uint32_t source, uint32_t offset);
#include "bot_navigation.h"
#include "bot_obstacle_navigation.h"
#include "bot_list_clear.h"
#include "bot_aim.h"
#include "bot_death_drop.h"
#include "bot_network_clock.h"
#include "bot_loadouts.h"
#include "bot_weapon_selection.h"
#include "bot_team_assignment.h"
#include "bot_failed_routes.h"
#include "bot_unstick.h"
#include "bot_map_knowledge.h"
#include "bot_kits.h"
#include "overlay.h"
#include "render.h"   // 0.5.228: LogNextFrame at a death
#include "render_commands.h"   // 0.5.234: the death view's blur while down
#include "input.h"    // 0.5.231: the photo tour presses A
#include "sampler.h"  // 0.5.345: the killcam study names functions
#include "spectator.h"   // 0.5.490: the spectator's calls in the killcam's hooks
#include "imgui.h"       // 0.5.641: the pre-match count
#include "flamethrower.h"   // 0.5.703: the view for flames drawn in the world
#include "flare.h"          // 0.5.739: and for the signal flare's glow
#include <cfloat>

#include <windows.h>
#include <dbghelp.h>   // 0.5.493: FlagTrap names the recompiled functions
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <cctype>
#include <optional>
#include <array>
#include <thread>
#include <map>
#include <vector>
#include <mutex>
#include "env_cache.h"   // 0.5.370: getenv read once, log flushed at most every 100 ms

extern "C" PPC_FUNC(__imp__sub_82535810);   // SV_Frame
void LobbyRulesTick(PPCContext& ctx, uint8_t* base);   // lobby_rules.cpp
void MpFixesFrame(uint8_t* base);                      // mp_fixes.cpp
void MpFixesAbilities(PPCContext& ctx, uint8_t* base);  // mp_fixes.cpp
void MpFixesLauncher(PPCContext& ctx, uint8_t* base);   // mp_fixes.cpp
void MpFixesKillcamFx(PPCContext& ctx, uint8_t* base, const float* at, const float* angles, const char* weapon, uint32_t event);   // mp_fixes.cpp (0.5.634)
extern "C" PPC_FUNC(__imp__sub_825320D0);   // SV_SendClientSnapshot(client)
extern "C" PPC_FUNC(__imp__sub_824D0AF0);
extern "C" PPC_FUNC(__imp__sub_82487718);
extern "C" PPC_FUNC(__imp__sub_82483190);
extern "C" PPC_FUNC(__imp__sub_824B4270);   // Pmove(pm)
extern "C" PPC_FUNC(__imp__sub_8256B8E0);   // a player's muzzle point (entity, out)   // is an entity where it may move   // the move's scale (a command)   // spawn a player (handle, origin, angles, side)

int MpFixesOwnSide();   // 0.5.540: (mp_fixes.cpp) the host's side from his kit, 0 unknown
namespace
{
    constexpr uint32_t Svs = 0x82B3FDE0;
    constexpr uint32_t ClientSize = PlayerCapacity::ServerClientBytes, MaxClients = PlayerCapacity::StorageClients;
    // 0.5.240: arrays are sized for every stored slot (32); loops go over the
    // slots in play, 24 or 32 (COD3_PLAYERS=32, mp_fixes.cpp MpCapClients).
    inline uint32_t ActiveSlots() { return PlayerCapacity::Active(); }
    inline uint32_t FirstBotSlot() { return PlayerCapacity::FirstBotSlot(); }   // 0.5.283: 1 (bots also in spare local slots) or 4
    constexpr uint32_t EntityList = 0x82BCC7FC;   // a pointer to the players' entities, by client number + 1

    constexpr uint32_t SetUpSlot = 0x82532540;
    constexpr uint32_t ConnectIntoWorld = 0x82593608;
    constexpr uint32_t BeginPlayer = 0x8257A6C8;

    bool g_bot[MaxClients] = {};
    uint32_t g_botSide[MaxClients] = {};
    // 0.5.207: each bot's lives (counted at each spawn) and how his last
    // death went, for the other machines (SendBotStates / ApplyMirrorStates).
    uint8_t g_botLife[MaxClients] = {};
    struct BotDeathInfo { uint32_t attacker = 0, inflictor = 0; int32_t damage = 0, means = 0, weapon = 0, hitLoc = 0, timeOffset = 0; float dir[3] = {}; bool haveDir = false; };
    BotDeathInfo g_botDeath[MaxClients];
    BotDeathDrop::State g_deathDrop[MaxClients];
    thread_local uint32_t g_deathDropEntity=0,g_deathDropCreated=0;
    // 0.5.139: each bot's local place (its byte's address) and his session
    // player's number, kept so his spawn can be done without it.
    uint32_t g_placeAt[MaxClients] = {};
    uint8_t g_placeNumber[MaxClients] = {};
    // 0.5.142: each bot's own local index (1 to 3), kept from his creation,
    // 0 for none. The game's current client (0x82BCBE9C) is a LOCAL client
    // index: its arrays have 4 entries. The bot's slot (4, 5, ...) written
    // there went past them and wrecked other data (the zoom, the crashes).
    uint32_t g_placeIndex[MaxClients] = {};
    bool g_inSpawn = false;
    uint32_t g_thinkSlot = MaxClients;   // 0.5.143: the bot whose think runs
    // 0.5.151: War's flag in play (WatchWar), the bots' objective there.
    struct WarGoal { bool valid = false; float at[3] = {}; uint32_t index = 0, when = 0; };
    WarGoal g_war;
    // 0.5.463: the other objective modes' objectives (WatchObjectives): every
    // entity whose class name says flag, ctf, hq or radio, where it is now
    // 0.5.476: out/live: a flag off its base (its prop 100 or more from where
    // it stood: carried or dropped) and where it is now
    struct ObjectiveGoals { bool valid = false; uint32_t when = 0; int count = 0; float at[8][3] = {}; char name[8][32] = {}; std::string mode; bool out[8] = {}; float live[8][3] = {}; };
    ObjectiveGoals g_obj;   // Respawn running: places left as it sets them
    // 0.5.475: CTF, each side's own flag (an index into g_obj; -1 not known),
    // set once a map by CtfSpawnSides: the flag a side's game spawns are by is
    // the enemy's (Justin: "the enemies spawn on our flag and we spawn on the
    // enemies flag"), its own the other
    int g_ctfOwn[3] = { -1, -1, -1 };
    // 0.5.482: each CTF flag's real state, from FlagEntityWatch: carried (its
    // model hidden under the map, z -9970; the carrier the nearest player when
    // it went), dropped (its model shown away from its base) or home
    int g_flagCarrier[2] = { -1, -1 };
    bool g_flagDropped[2] = {};
    float g_flagDropAt[2][3] = {};
    // 0.5.582: ways out of each flag's base as players walked them (a spot a
    // half second, from within 200 of the flag till 650 out; the last 16 a
    // flag), for attackers to walk in by backwards (MapKnowledgeTick records,
    // the route takes one within 700 of the enemy flag)
    std::vector<BotNavigation::Point> g_flagTrails[2][16];
    int g_flagTrailNext[2] = {};
    // 0.5.585: kept for the next games on the map (583-584: side 2's base had
    // 0-3 by the time its flag was taken); read in as g_trailsLoaded, put to
    // their flag (the one within 350 of their start) once the flags are known
    std::vector<std::vector<BotNavigation::Point>> g_trailsLoaded;
    bool g_trailsChanged = false;
    std::vector<BotNavigation::Point> g_trail[MaxClients];   // 0.5.522 (BotTrail; moved up 0.5.589)
    // 0.5.483: the point Think steers a bot to (after the route), for a
    // flag carrier's legs (CarrierHome)
    float g_steer[MaxClients][3] = {};
    uint8_t g_flagUseMode[MaxClients] = {};   // 0.5.484: 1 X alone, 2 X with activate (BotFlagUse)
    uint32_t g_portMovedAt[MaxClients] = {};  // 0.5.484: when CtfSpawnSides last teleported him
    // 0.5.487: a take's proof for ctf-flags.txt (FlagEntityWatch sets, CtfSpawnSides saves)
    uint32_t g_ctfProvenSide = 0; int g_ctfProvenFlag = -1; bool g_ctfSweepDue = false;
    bool g_ctfPadAssist = false;
    int g_captureTry = -1;   // 0.5.523: which capture request a bot made last (BotFlagClaim)
    bool g_ctfAttacker[MaxClients] = {};
    bool g_ctfOwnLegs[MaxClients] = {};   // 0.5.541: Think's CTF squad, escort or cover hold steers him (AttackerRoute stays out)
    bool g_ctfHold[MaxClients] = {};      // 0.5.541: a defender at his place, watching (Think)
    float g_ctfWatchYaw[MaxClients] = {}; // 0.5.541: the way he watches (degrees)
    const char* g_why[MaxClients] = {};       // 0.5.546: which of Think's ways out he took (the ctftrace line)
    const char* g_ctfDoing[MaxClients] = {};  // 0.5.546: his CTF role's errand this frame (ctftrace)
    char g_pickInfo[MaxClients][96] = {};     // 0.5.551: the learnt route's spot he steers to (ctftrace)
    const char* g_routeStart[MaxClients] = {}; // 0.5.553: how his route's start was chosen (ctftrace)
    const char* g_routeRedo[MaxClients] = {};  // 0.5.562: why his route was last worked out (ctftrace)
    int g_ctfLane[MaxClients] = {};            // 0.5.564: a CTF attacker's lane: -1 left, 0 middle, +1 right of the way between the flags
    uint32_t g_revivedAt[MaxClients] = {};   // 0.5.544: when a medic last stood him up (CtfSpawnSides leaves him there)
    bool g_dropPending[MaxClients] = {};   // 0.5.538: a dead carrier's flag to drop (FlagEntityWatch -> CarrierUnstick)   // 0.5.536: his CTF role is to take the enemy flag (Think)
    int g_ctfTrialBot = -1; uint32_t g_ctfTrialUntil = 0;   // 0.5.497: CtfTakeProbe's bot trial (FeedBots freezes him)
    std::vector<uint32_t> g_flagModels;
    std::vector<uint32_t> g_flagTriggers;   // 0.5.512: the flags' trigger_multiples (FlagEntityWatch), for FlagTrap   // 0.5.493: the flags' script_models (FlagEntityWatch), for FlagTrap   // 0.5.492: the unattended test tapping the local pad's X for a bot
    bool g_steerValid[MaxClients] = {};
    uint32_t ReservePlace(uint32_t slot)
    {
        if (g_placeIndex[slot] != 0) return g_placeIndex[slot];
        // 0.5.145: bots are remote players now, with no local place: all
        // of them share local index 3 (unused: one host, no split screen)
        // as their current client, so the game's 4-entry lists stay in
        // bounds however many bots there are.
        if (!g_botLocal.load())
        {
            g_placeIndex[slot] = 3;
            printf("bots: slot %u's local index 3 (shared scratch, remote bot)\n", slot);
            fflush(stdout);
            return 3;
        }
        for (uint32_t k = 1; k < 4; k++)
        {
            bool taken = false;
            for (uint32_t o = 0; o < ActiveSlots(); o++) if (o != slot && g_bot[o] && g_placeIndex[o] == k) taken = true;
            if (!taken) { g_placeIndex[slot] = k; break; }
        }
        printf("bots: slot %u's local index %u%s\n", slot, g_placeIndex[slot], g_placeIndex[slot] ? "" : " (none free: his slot is used, as before)");
        fflush(stdout);
        return g_placeIndex[slot];
    }
    bool g_firing[MaxClients] = {};   // the bot wants his weapon to fire this frame
    float g_wantYaw[MaxClients] = {}, g_wantPitch[MaxClients] = {};   // where his brain wants to look
    uint32_t g_engagedAt[MaxClients] = {};   // 0.5.456: when he first saw the one he shoots at (his aim settles from then)
    bool g_adsNow[MaxClients] = {};          // 0.5.456: his sights are up
    uint32_t g_reviveTarget[MaxClients] = {};
    float g_deadAt[MaxClients][3] = {};   // 0.5.224: where each player fell (BotRevive)   // 0.5.216: a medic's body to revive (slot + 1; 0 none)
    // 0.5.216: a medic bot on his way to this slot's body (dead players wait for him).
    bool MedicComing(uint32_t slot) { for (uint32_t s = 0; s < ActiveSlots(); s++) if (g_reviveTarget[s] == slot + 1) return true; return false; }
    int g_age[MaxClients] = {};   // frames since the bot was added
    float g_yaw[MaxClients] = {};
    uint32_t g_lookVehicleUntil[MaxClients] = {};   // 0.5.843: LookAtVehicle has his facing until then
    float g_vehicleGoal[MaxClients][3] = {};        // 0.5.849: the tank he walks to (its rear corner),
    uint32_t g_vehicleGoalAt[MaxClients] = {};      // set each frame by the vehicle code (real-time ms)
    int g_bots = 0;
    // 0.5.168: Justin wants a mix of bots. Each bot is given a role when
    // added: Objective (plays War's / Domination's flag), Rusher (runs at
    // the enemy, sprinting), Flanker (roams the map's outskirts), Camper
    // (finds a spot and holds it, crouched, moving on now and then); and a
    // class of his own (0-6: the game's seven), so their weapons differ.
    enum BotRole : uint8_t { RoleObjective, RoleRusher, RoleFlanker, RoleCamper };
    uint8_t g_role[MaxClients] = {};
    uint8_t g_botLoadout[MaxClients]={};
    // 0.5.198: each bot's kit, copied from bot_kits.txt when he is added.
    BotKits::Kit g_botKit[MaxClients]={};
    std::vector<BotKits::Kit> g_kits;
    // bot_kits.txt beside the executable: written with the defaults when
    // missing, read again whenever it has been saved since.
    const std::vector<BotKits::Kit>& LoadBotKits()
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        const std::filesystem::path file = std::filesystem::path(path).parent_path() / "bot_kits.txt";
        static std::filesystem::file_time_type seen{};
        std::error_code ec;
        if (!std::filesystem::exists(file, ec))
        {
            if (FILE* f = fopen(file.string().c_str(), "wb")) { fputs(BotKits::DefaultText(), f); fclose(f); }
            printf("bots: kits: wrote the default bot_kits.txt (edit it to change the bots' loadouts)\n"); fflush(stdout);
        }
        const auto when = std::filesystem::last_write_time(file, ec);
        if (!ec && when == seen && !g_kits.empty()) return g_kits;
        seen = when;
        std::string text;
        if (FILE* f = fopen(file.string().c_str(), "rb")) { char b[4096]; size_t n; while ((n = fread(b, 1, sizeof(b), f)) > 0) text.append(b, n); fclose(f); }
        std::string problems;
        g_kits = BotKits::Parse(text, &problems);
        if (g_kits.empty()) g_kits = BotKits::Defaults();
        printf("bots: kits: %zu from bot_kits.txt%s%s\n", g_kits.size(), problems.empty() ? "" : " (", problems.empty() ? "" : (problems + ")").c_str());
        fflush(stdout);
        return g_kits;
    }
    bool g_botLoadoutAssigned[MaxClients]={};
    int g_botNumber[MaxClients]={};   // 0.5.326: "Bot N" (the killcam's name)
    const char* RoleName(uint8_t r) { return r == RoleObjective ? "objective" : r == RoleRusher ? "rusher" : r == RoleFlanker ? "flanker" : "camper"; }

    // The players' command rings: 6352 bytes a player at 0x82C2ECF0, 64
    // commands of 48 bytes at +1688, the newest's number at +4760. A command:
    // +0 the server time, +4 the buttons (a bit each), +8 the weapon, +12/16/20
    // the view angles as shorts (360 degrees in 65536), +24/25/26 forward,
    // right and up (-127 .. 127), five floats from +28 (CL_FinishMove,
    // sub_82522B10; the moves from sub_82525338).
    constexpr uint32_t Players = 0x82C2ECF0, PlayerSize = 6352;
    constexpr uint32_t Ring = 1688, Newest = 4760, CommandSize = 48;

    // 0.5.159: THE ROOT CAUSE of the host's gun, controls and crashes with
    // bots. This array holds only this machine's FOUR local players (entry 4
    // at 0x82C34E30 is already the client's per-local-player input state at
    // 0x82C35030, 0x804 each; entry 5 reaches the gun view pointers at
    // 0x82C37070). The bots' commands were written into entries 4, 5...:
    // a bot in slot 4 wrote over the host's input state (prone standing up,
    // "someone else has control", gun glitches), a bot in slot 5 over his gun
    // view pointers (0.5.158's log: changed in FeedBots, every frame). Each
    // bot's commands now go in a block of his own, laid out the same, and the
    // server's SV_GetUsercmd (sub_82530270) takes a bot's from there.
    uint32_t g_commandBlock[MaxClients] = {};
    uint32_t CommandPlayer(uint8_t* base, uint32_t slot)
    {
        if (slot >= MaxClients || (slot < 4 && !g_bot[slot])) return Players + slot * PlayerSize;   // 0.5.283: a bot in a spare local slot has his own ring too
        if (g_commandBlock[slot] == 0)
        {
            g_commandBlock[slot] = Guest::AllocatePhysical(PlayerSize);
            if (g_commandBlock[slot]) memset(base + g_commandBlock[slot], 0, PlayerSize);
            printf("bots: slot %u's own command ring at %08X (not the local players' array)\n", slot, g_commandBlock[slot]);
            fflush(stdout);
        }
        return g_commandBlock[slot] ? g_commandBlock[slot] : Players + slot * PlayerSize;
    }
    uint32_t NewestCommand(uint8_t* base, uint32_t slot)
    {
        const uint32_t player = CommandPlayer(base, slot);
        return player + Ring + (Guest::Read32(base, player + Newest) & 63) * CommandSize;
    }

    uint32_t Clients(uint8_t* base) { return Guest::Read32(base, Svs + 8); }

    uint32_t Call(PPCContext& ctx, uint8_t* base, uint32_t function,
                  uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0, uint32_t r6 = 0, uint32_t r7 = 0)
    {
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x3000) & ~0xFu;
        call.r3.u32 = r3; call.r4.u32 = r4; call.r5.u32 = r5; call.r6.u32 = r6; call.r7.u32 = r7;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
        return call.r3.u32;
    }

    float ReadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t bits = Guest::Read32(base, address);
        float f; memcpy(&f, &bits, 4); return f;
    }

    // 0.5.144: the base field of view the game's view (sub_82453810) starts
    // from, a float at 0x82BA2BC0, clamped to at least 1: the host's view
    // came out at 1 (zoomed all the way in) once a second bot was added.
    // Kept while it is sane; if a bot's step wrecks it, put back, and the
    // step named in the log.
    constexpr uint32_t BaseFov = 0x82BA2BC0;
    float g_goodFov = 0.0f;
    void CheckFov(uint8_t* base, const char* where, uint32_t slot)
    {
        const float v = ReadFloat(base, BaseFov);
        if (v >= 10.0f && v <= 170.0f) { g_goodFov = v; return; }
        if (g_goodFov <= 0.0f) return;
        static int told = 0;
        if ((told < 40 && told++ < 40))
        {
            printf("bots: the base field of view went %.2f (bits %08X) at %s, slot %u: put back to %.2f\n",
                   v, Guest::Read32(base, BaseFov), where, slot, g_goodFov);
            fflush(stdout);
        }
        uint32_t bits; memcpy(&bits, &g_goodFov, 4);
        Guest::Write32(base, BaseFov, bits);
    }
    // 0.5.146: found by 0.5.145's log: during a bot's think, his weapon's
    // view code (sub_82519178, from sub_8251A950 / ClientThink) puts back a
    // field of view it saved in his own weapon state (+0x74) into the global
    // one; a bot's is 0, so the host's view went to 0 -> zoomed in. That is
    // a local player's thing; for any bot the global value is kept exactly
    // as it was across his think and his set-up steps.
    // 0.5.151: the host's first-person weapon view object (0x82C37070[0])
    // is 0: its fields live at guest addresses 0x0 and up, and the bots'
    // runs of that same code (their objects are 0 too) wrote over them: the
    // host's gun vanished, vehicles stuck. The first 0x800 bytes are put back
    // as they were after every bot's think and set-up step.
    constexpr uint32_t NullObjectBytes = 0x800;
    struct KeepFov
    {
        uint8_t* base; uint32_t bits;
        uint8_t low[NullObjectBytes];
        // 0.5.174: CL's stance latch is separate from kbuttons and HostInput.
        // 82525338 turns it into command bits; 82571930 clears all four
        // latches even when invoked by bot setup. Preserve only host slot0.
        uint32_t hostStance;
        uint8_t stanceHold[12]; // CL slot0 +1608: hold flag, initial stance, time
        // 0.5.156: 0.5.155's log: the gun view pointers are real (static
        // objects 0x400 apart: 829D8810 the host's, then 1-3), never
        // emptied; the host's OBJECT itself gets wrecked (its +4's table
        // called through garbage). It is kept as it was too.
        uint32_t hostView = 0;
        uint8_t view[0x400];
        explicit KeepFov(uint8_t* b) : base(b), bits(Guest::Read32(b, BaseFov))
        {
            hostStance = Guest::Read32(base, 0x82B57804);
            memcpy(stanceHold, base + Players + 1608, sizeof(stanceHold));
            memcpy(low, base, NullObjectBytes);
            hostView = Guest::Read32(base, 0x82C37070);
            if (hostView < 0x40000000u || hostView >= 0xA0000000u) hostView = 0;
            if (hostView) memcpy(view, base + hostView, sizeof(view));
        }
        ~KeepFov()
        {
            const uint32_t afterStance = Guest::Read32(base, 0x82B57804);
            if (afterStance != hostStance || memcmp(stanceHold, base + Players + 1608, sizeof(stanceHold)))
            {
                static int toldStance = 0;
                if ((toldStance < 40 && toldStance++ < 40)) { printf("bots: isolated step changed host stance latch %u -> %u or hold state: restored\n", hostStance, afterStance); fflush(stdout); }
                Guest::Write32(base, 0x82B57804, hostStance);
                memcpy(base + Players + 1608, stanceHold, sizeof(stanceHold));
            }
            if (hostView && Guest::Read32(base, 0x82C37070) == hostView && memcmp(view, base + hostView, sizeof(view)) != 0)
            {
                static int toldView = 0;
                if ((toldView < 20 && toldView++ < 20))
                {
                    printf("bots: a bot's step wrote the host's gun view object %08X, put back:", hostView);
                    int shown = 0;
                    for (uint32_t w = 0; w < sizeof(view) && shown < 12; w += 4)
                    {
                        const uint32_t was = (uint32_t(view[w]) << 24) | (uint32_t(view[w + 1]) << 16) | (uint32_t(view[w + 2]) << 8) | view[w + 3];
                        const uint32_t now = Guest::Read32(base, hostView + w);
                        if (was == now) continue;
                        printf(" +%03X %08X->%08X", w, was, now);
                        shown++;
                    }
                    printf("\n");
                    fflush(stdout);
                }
                memcpy(base + hostView, view, sizeof(view));
            }
            if (Guest::Read32(base, BaseFov) != bits) Guest::Write32(base, BaseFov, bits);
            if (memcmp(low, base, NullObjectBytes) != 0)
            {
                static int told = 0;
                if ((told < 20 && told++ < 20))
                {
                    // 0.5.154: every changed word (up to 12), old and new.
                    printf("bots: a bot's step wrote the host's weapon view, put back:");
                    int shown = 0;
                    for (uint32_t w = 0; w < NullObjectBytes && shown < 12; w += 4)
                    {
                        const uint32_t was = (uint32_t(low[w]) << 24) | (uint32_t(low[w + 1]) << 16) | (uint32_t(low[w + 2]) << 8) | low[w + 3];
                        const uint32_t now = Guest::Read32(base, w);
                        if (was == now) continue;
                        printf(" +%03X %08X->%08X", w, was, now);
                        shown++;
                    }
                    printf("\n");
                    fflush(stdout);
                }
                memcpy(base, low, NullObjectBytes);
            }
        }
    };

    // 0.5.109: setup must run with the same current-client context as
    // ClientThink. A bot has no local camera; never borrow host client 0.
    constexpr uint32_t CurrentClient = 0x82BCBE9C;
    uint32_t BotSlot(uint8_t* base, uint32_t entity)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (uint32_t slot = FirstBotSlot(); entities && slot < ActiveSlots(); ++slot)
            if (g_bot[slot] && Guest::Read32(base, entities + 4 * (slot + 1)) == entity) return slot;
        return MaxClients;
    }
    // 0.5.153: each bot's own first-person gun view memory. The game finds
    // a local player's gun view at 0x82C37070[current client]; the host's
    // entry is 0 (his lives at guest 0x0 on), and the bots' (index 3) was 0
    // too, so their runs of that code wrote into the host's. While any step
    // of a bot runs (ours, or the game's own spawn and think of his player),
    // entry 3 points at a block of his own, and the current client is 3.
    constexpr uint32_t GunViews = 0x82C37070, BotGunView = 3, GunViewBytes = 0x2000;
    uint32_t g_gunView[MaxClients] = {};
    uint32_t BotGunViewBlock(uint8_t* base, uint32_t slot)
    {
        if (slot >= MaxClients) return 0;
        if (g_gunView[slot] == 0)
        {
            g_gunView[slot] = Guest::AllocatePhysical(GunViewBytes);
            if (g_gunView[slot]) memset(base + g_gunView[slot], 0, GunViewBytes);
            printf("bots: slot %u's own gun view memory at %08X\n", slot, g_gunView[slot]);
            fflush(stdout);
        }
        return g_gunView[slot];
    }
    struct BotGunViewScope
    {
        uint8_t* base; uint32_t savedEntry, savedCurrent; bool on;
        BotGunViewScope(uint8_t* b, uint32_t slot, bool setCurrent)
            : base(b), savedEntry(Guest::Read32(b, GunViews + 4 * BotGunView)), savedCurrent(Guest::Read32(b, CurrentClient)), on(false)
        {
            const uint32_t block = BotGunViewBlock(b, slot);
            if (!block) return;
            on = true;
            Guest::Write32(base, GunViews + 4 * BotGunView, block);
            if (setCurrent) Guest::Write32(base, CurrentClient, BotGunView);
        }
        ~BotGunViewScope()
        {
            if (!on) return;
            Guest::Write32(base, GunViews + 4 * BotGunView, savedEntry);
            Guest::Write32(base, CurrentClient, savedCurrent);
        }
    };

    // 0.5.158: where the gun view pointers (0x82C37070, 4 words) change.
    // 0.5.157's hardware watch on them never fired, though they were wiped
    // when bot 2 came in: so they are compared at each step instead, and
    // the first step after which they differ is named.
    uint32_t g_gunPointers[4] = {};
    bool g_gunPointersKnown = false;
    void GunPointersCheck(uint8_t* base, const char* where, uint32_t slot = 0)
    {
        uint32_t now[4];
        for (int k = 0; k < 4; k++) now[k] = Guest::Read32(base, 0x82C37070 + 4 * k);
        if (g_gunPointersKnown && memcmp(now, g_gunPointers, sizeof(now)) != 0)
        {
            static int told = 0;
            if ((told < 60 && told++ < 60))
            {
                printf("bots: GUN VIEW POINTERS CHANGED at %s (slot %u): %08X %08X %08X %08X -> %08X %08X %08X %08X\n", where, slot,
                       g_gunPointers[0], g_gunPointers[1], g_gunPointers[2], g_gunPointers[3], now[0], now[1], now[2], now[3]);
                fflush(stdout);
            }
        }
        memcpy(g_gunPointers, now, sizeof(now));
        g_gunPointersKnown = true;
    }

    // 0.5.147: this machine's 4 local clients (0x82C7A270, 0xc88 each);
    // +0xb8 is 2 for one in play. The server frame runs a local client's
    // view and weapon code (sub_82534DB0 -> sub_82593D50) for every one at
    // 2: with two bots, local client 3 (the bots' shared current client)
    // came up at 2, and that code then ran for an empty local player: the
    // host's view went to 0, his gun vanished and he froze.
    constexpr uint32_t LocalClients = 0x82C7A270, LocalClientSize = 0xc88;
    uint32_t LocalState(uint32_t k) { return LocalClients + 0xb8 + k * LocalClientSize; }
    uint32_t g_localStateBefore[4] = {};
    bool g_localStateKnown = false;

    class BotClientScope
    {
        uint8_t* base;
        uint32_t saved, slot, hostRecord = 0, sights = 0, weapon = 0, fovBits = 0, index = 0, state = 0, gunEntry = 0;
        bool gunOn = false;
        KeepFov keep;   // 0.5.151: declared first: restores last
        const char* stage;
        bool active;
    public:
        BotClientScope(uint8_t* memory, uint32_t botSlot, const char* name, bool sharedRemoteIndex = false)
            : base(memory), saved(Guest::Read32(memory, CurrentClient)), slot(botSlot), keep(memory), stage(name),
              active(g_botCamera.load() && botSlot < MaxClients && (sharedRemoteIndex || botSlot >= 4))
        {
            if (!active) return;
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
            hostRecord = host ? Guest::Read32(base, host + 512) : 0;
            if (hostRecord) { sights = Guest::Read32(base, hostRecord + 172); weapon = Guest::Read32(base, hostRecord + 164); }
            CheckFov(base, "before", slot);
            fovBits = Guest::Read32(base, BaseFov);   // 0.5.146
            index = sharedRemoteIndex ? BotGunView : (g_placeIndex[slot] ? g_placeIndex[slot] : slot);
            if (index < 4) state = Guest::Read32(base, LocalState(index));   // 0.5.147
            Guest::Write32(base, CurrentClient, index);
            // 0.5.153: his own gun view memory while the step runs.
            if (index == BotGunView && !g_botLocal.load())
            {
                const uint32_t block = BotGunViewBlock(base, slot);
                if (block) { gunEntry = Guest::Read32(base, GunViews + 4 * BotGunView); Guest::Write32(base, GunViews + 4 * BotGunView, block); gunOn = true; }
            }
        }
        ~BotClientScope()
        {
            if (!active) return;
            if (gunOn) Guest::Write32(base, GunViews + 4 * BotGunView, gunEntry);   // 0.5.153
            GunPointersCheck(base, stage, slot);   // 0.5.158
            Guest::Write32(base, CurrentClient, saved);
            if (index >= 1 && index < 4 && !g_botLocal.load() && Guest::Read32(base, LocalState(index)) != state)
            {
                printf("bots: %s of slot %u set local client %u's state %u -> %u: put back\n", stage, slot, index,
                       state, Guest::Read32(base, LocalState(index)));
                Guest::Write32(base, LocalState(index), state);
            }
            if (fovBits && Guest::Read32(base, BaseFov) != fovBits) Guest::Write32(base, BaseFov, fovBits);   // 0.5.146
            CheckFov(base, stage, slot);
            printf("bots: setup context %s slot %u restored %u; host ADS %08X -> %08X, weapon %u -> %u\n",
                   stage, slot, saved, sights, hostRecord ? Guest::Read32(base, hostRecord + 172) : 0,
                   weapon, hostRecord ? Guest::Read32(base, hostRecord + 164) : 0);
            fflush(stdout);
        }
        BotClientScope(const BotClientScope&) = delete;
        BotClientScope& operator=(const BotClientScope&) = delete;
    };

    // A player's side: +4 of the team record his entity's +520 points to
    // (1 the allies, 2 the axis, as the side menus set it).
    uint32_t Side(uint8_t* base, uint32_t entity)
    {
        const uint32_t team = entity ? Guest::Read32(base, entity + 520) : 0;
        const uint32_t read = team ? Guest::Read32(base, team + 4) : 0;
        // 0.5.542: the host's side is his entity's team once it has held 2 s
        // (539: it read 2 at his CTF spawns while he was on 1). 540's kit
        // (American = 1) was wrong on Rouen (541: team 1, kit 2: his own bots
        // shot him, he was put by the enemy flag)
        const uint32_t list = entity ? Guest::Read32(base, EntityList) : 0;
        // 0.5.544: a bot's side is the one he was given: his team read is the
        // other one at the moment he spawns (542/543: 46 of 71 spawns), and the
        // UAV check had it equal to the given one the rest of the time
        for (uint32_t o = 1; list && o < MaxClients; o++)
            if (g_bot[o] && (g_botSide[o] == 1 || g_botSide[o] == 2) && Guest::Read32(base, list + 4 * (o + 1)) == entity) return g_botSide[o];
        if (list && entity == Guest::Read32(base, list + 4) && (read == 1 || read == 2))
        {
            static uint32_t stable = 0, pending = 0, pendingSince = 0;
            const uint32_t now = GetTickCount();
            if (read == stable) pending = 0;
            else if (read != pending) { pending = read; pendingSince = now; }
            else if (now - pendingSince >= 2000)
            {
                printf("bots: the host's side: %u (was %u; held 2 s)\n", read, stable); fflush(stdout);
                stable = read; pending = 0;
            }
            return stable ? stable : read;
        }
        return read;
    }

    // Each bot owns a stable side. New bots join the smaller team,
    // including connected humans; respawns never derive it from the host.
    uint32_t BotSide(uint8_t* base, uint32_t slot)
    {
        if (slot >= MaxClients) return 0;
        if (g_botSide[slot] == 1 || g_botSide[slot] == 2) return g_botSide[slot];
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t clients = Clients(base);
        uint32_t allies = 0, axis = 0;
        for (uint32_t other = 0; entities && clients && other < ActiveSlots(); ++other)
        {
            if (other == slot || Guest::Read32(base, clients + other * ClientSize) == 0) continue;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (other + 1));
            const uint32_t side = g_bot[other] ? g_botSide[other] : Side(base, entity);
            if (side == 1) ++allies;
            if (side == 2) ++axis;
        }
        const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t tieSide = Side(base, host) == 2 ? 1u : 2u;
        g_botSide[slot] = BotTeamAssignment::Choose(allies, axis, tieSide);
        printf("bots: slot %u assigned side %u (allies %u, axis %u before join)\n",
               slot, g_botSide[slot], allies, axis); fflush(stdout);
        return g_botSide[slot];
    }

    // A player's whereabouts as the log shows them: the first words of his
    // record (entity+512 points to it) and of the entity itself from +336,
    // for working out which of them is his origin.
    void LogWhere(uint8_t* base, const char* what, uint32_t slot, uint32_t entity)
    {
        const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
        printf("bots: %s %u record", what, slot);
        for (int i = 0; ps && i < 8; i++) printf(" %.1f", ReadFloat(base, ps + 4 * i));
        printf(" | entity");
        for (int i = 0; entity && i < 4; i++) printf(" %.1f", ReadFloat(base, entity + 336 + 4 * i));
        // ClientThink's tests (sub_82592CD0): +1932 must be 2; +1924 the
        // player's state (1 watching, 2 the end, 4 and 5 dead); his command's
        // time (+1936) against his last (+32) and the game's (0x82BB81D0+156).
        if (ps)
            printf(" | move type %d, health %d, +2040 %d, +2044 %d, speed %d",
                   int32_t(Guest::Read32(base, ps + 36)), int32_t(Guest::Read32(base, ps + 304)),
                   int32_t(Guest::Read32(base, ps + 2040)), int32_t(Guest::Read32(base, ps + 2044)),
                   int32_t(Guest::Read32(base, ps + 80)));
        if (ps)
            printf(" | connected %d, state %d, command %d, last %d, game %d",
                   int32_t(Guest::Read32(base, ps + 1932)), int32_t(Guest::Read32(base, ps + 1924)),
                   int32_t(Guest::Read32(base, ps + 1936)), int32_t(Guest::Read32(base, ps + 32)),
                   int32_t(Guest::Read32(base, 0x82BB81D0 + 156)));
        printf("\n"); fflush(stdout);
    }

    // The session's players (from +48832 of the session, Session below:
    // 24 of 784 bytes; sub_823E5F30 finds an entity's). Each: +0 a byte, his
    // number in the session's messages (24 and up: none), +4 not zero while
    // he is in, +8 his client slot. The spawn, and what each machine's game
    // draws of a player, go by these; a bot has none of his own.
    constexpr uint32_t SessionPlayers = 48832, SessionPlayerSize = 784;
    // 0.5.240: session numbers in play (24, or 32 with COD3_PLAYERS=32); one at
    // or past it means nobody.
    inline uint32_t SessionMaxNow() { return PlayerCapacity::Active(); }

    uint32_t SessionPlayerAt(uint8_t* base, uint32_t mode, uint32_t index)
    {
        if (mode == 0 || index >= SessionMaxNow()) return 0;
        const uint32_t object = mode + 0xAEB0; // mode + 48832 == object + 0x1010
        return MpSessionPlayerAddress(base, object, 0x1010 + index * SessionPlayerSize);
    }

    // The session itself: the word the pointer at 0x82BCCAC0 points to (zero
    // with none, as sub_82406BB8 tests).
    uint32_t Session(uint8_t* base)
    {
        const uint32_t pointer = Guest::Read32(base, 0x82BCCAC0);
        return pointer ? Guest::Read32(base, pointer) : 0;
    }

    uint32_t SessionPlayer(uint8_t* base, uint32_t slot)
    {
        const uint32_t mode = Session(base);
        if (mode == 0) return 0;
        for (uint32_t i = 0; i < SessionMaxNow(); i++)
        {
            const uint32_t player = SessionPlayerAt(base, mode, i);
            if (Guest::Read8(base, player) < SessionMaxNow() && Guest::Read32(base, player + 4) != 0 &&
                Guest::Read32(base, player + 8) == slot)
                return player;
        }
        return 0;
    }

    void LogSession(uint8_t* base)
    {
        const uint32_t mode = Session(base);
        printf("bots: session %08X\n", mode);
        for (uint32_t i = 0; mode && i < SessionMaxNow(); i++)
        {
            const uint32_t player = SessionPlayerAt(base, mode, i);
            const uint8_t number = Guest::Read8(base, player);
            const uint32_t in = Guest::Read32(base, player + 4);
            if (number >= SessionMaxNow() && in == 0) continue;
            printf("bots: session player %u at %08X: number %u, in %08X, slot %d\n", i, player, number, in,
                   int32_t(Guest::Read32(base, player + 8)));
            for (uint32_t row = 0; row < SessionPlayerSize; row += 32)
            {
                printf("bots:   +%03X", row);
                for (uint32_t w = 0; w < 32 && row + w < SessionPlayerSize; w += 4)
                    printf(" %08X", Guest::Read32(base, player + row + w));
                printf("\n");
            }
        }
        fflush(stdout);
    }

    // A session player for a bot: the host's own copied into a free place,
    // with the place's number and the bot's slot.
    bool GiveSessionPlayer(uint8_t* base, uint32_t slot)
    {
        const uint32_t mode = Session(base);
        const uint32_t host = SessionPlayer(base, 0);
        if (mode == 0 || host == 0 || SessionPlayer(base, slot) != 0) return false;
        for (uint32_t i = 0; i < SessionMaxNow(); i++)
        {
            const uint32_t player = SessionPlayerAt(base, mode, i);
            if (Guest::Read8(base, player) < SessionMaxNow() && Guest::Read32(base, player + 4) != 0) continue;
            memcpy(base + player, base + host, SessionPlayerSize);
            // 0.5.130: the copied +4 is a counted object the host's entry
            // holds (sub_824046D8 adds one to its count, +4, when it keeps
            // it): counted again for the copy, so the bot's entry going away
            // doesn't free the host's.
            const uint32_t held = Guest::Read32(base, player + 4);
            // 0.5.135: his own identity, not the host's. +4 is a counted
            // holder (+4 its count) of the machine's player object (+8):
            // its +0x38 byte says "on this machine" (sub_821AE440) and +0x34
            // tells machines apart (sub_821AE448). Shared with the host, the
            // bot counted as a second local player of the host's machine:
            // Justin, on 0.5.134: his own guns went full auto with endless
            // rounds and his hits stopped counting for a while, and the bot's
            // death never reached the kill feed. Now the bot gets a copy of
            // both (0x100 bytes each), "not on this machine", with his own id.
            const uint32_t object = held ? Guest::Read32(base, held + 8) : 0;
            if (g_botRemote.load() && held >= 0x40000000u && held < 0xA0000000u && object >= 0x40000000u && object < 0xA0000000u)
            {
                const uint32_t mine = Guest::AllocatePhysical(0x100), myObject = Guest::AllocatePhysical(0x100);
                memcpy(base + mine, base + held, 0x100);
                memcpy(base + myObject, base + object, 0x100);
                Guest::Write32(base, mine + 4, 0x10000);   // never counted down to freeing (it is ours, not the game's heap's)
                Guest::Write32(base, mine + 8, myObject);
                // 0.5.163: left "on this machine" (+0x38 as the host's, 1).
                // Justin, on 0.5.162 (20 bots seen, amazing): still no death
                // animation, they freeze and vanish. That worked only while
                // the bot shared the host's machine object (0.5.137-0.5.144);
                // several checks read +0x38 directly (sub_821AE440, from
                // sub_821B2BF8, sub_821B35E0, sub_823E4D80, sub_823FB298,
                // sub_82405158, sub_82405A88...), past the sub_823D9B50 hook.
                // The bot keeps his own object and id (+0x34); only "local"
                // is now true, as it was then (g_botRemoteFlag true = the old way).
                Guest::Write8(base, myObject + 0x38, g_botRemoteFlag.load() ? 0 : 1);
                // 0.5.164: the host's machine id kept (+0x34, compared by
                // sub_821AE448). Justin, on 0.5.163: still no death animation
                // or kill feed. Both worked in 0.5.140-0.5.144, when the bot's
                // machine was the host's own, id and all; the session's
                // death and kill messages carry the machine they are about,
                // and one about a machine id nobody has goes nowhere.
                if (g_botRemoteFlag.load())
                    Guest::Write32(base, myObject + 0x34, Guest::Read32(base, object + 0x34) ^ (0x5B000000u + slot));
                Guest::Write32(base, player + 4, mine);
                printf("bots: slot %u has his own machine object %08X (holder %08X), not the host's %08X\n", slot, myObject, mine, object);
            }
            else if (held)
                Guest::Write32(base, held + 4, Guest::Read32(base, held + 4) + 1);
            Guest::Write8(base, player, uint8_t(i));
            Guest::Write32(base, player + 8, slot);
            // 0.5.137: a local place of his own. The session keeps which of
            // its players are this machine's four local ones: four bytes at
            // +0x5991 of its object (the players' list is at +0x1010 of it,
            // so the object is the list's address less 0x1010), each a
            // session player's number or 24 and up for none (sub_824046D8,
            // sub_823D8FB0 and sub_823D8FF0 read them). A bot was in none,
            // so code that looks up "which local player is this" found no
            // place and fell back to the host's (the current client, 0):
            // the host's view zoomed, his guns went full auto, and two bots
            // crashed the animation code. Now a bot takes a free local place
            // (1 to 3) when there is one. Console bot_local 0 leaves it out.
            if (g_botLocal.load())
            {
                const uint32_t object = mode + SessionPlayers - 0x1010;
                const uint32_t places = object + 0x5991;
                const uint8_t hostNumber = Guest::Read8(base, host);
                if (Guest::Read8(base, places) == hostNumber)
                {
                    // 0.5.142: his own index (ReservePlace), not merely the
                    // first free byte: a dead bot's place is emptied
                    // (0.5.141), and a second bot took it.
                    int place = -1;
                    const uint32_t own = slot < MaxClients ? ReservePlace(slot) : 0;
                    if (own != 0 && Guest::Read8(base, places + own) >= SessionMaxNow()) place = int(own);
                    if (place > 0)
                    {
                        Guest::Write8(base, places + place, uint8_t(i));
                        g_placeAt[slot] = places + uint32_t(place);
                        g_placeNumber[slot] = uint8_t(i);
                        printf("bots: slot %u takes local place %d (session player %u)\n", slot, place, i);
                    }
                    else printf("bots: slot %u: no free local place (places %02X %02X %02X %02X)\n", slot,
                                Guest::Read8(base, places), Guest::Read8(base, places + 1), Guest::Read8(base, places + 2), Guest::Read8(base, places + 3));
                }
                else printf("bots: slot %u: local places not where expected (first %02X, host %02X)\n", slot, Guest::Read8(base, places), hostNumber);
            }
            // 0.5.131: his own name (Justin: the copy showed the host's).
            // The session player's name is the text at +104 (the host's
            // "HOVGtv" there in 0.5.130's log), up to 32 bytes: "Bot N",
            // N picked at random from 1 to 24, none two bots share.
            {
                static int number[MaxClients] = {};
                bool used[PlayerCapacity::StorageClients + 1] = {};
                const int top = int(ActiveSlots());   // 0.5.240: Bot 1 to Bot 24, or to 32
                for (uint32_t k = 0; k < ActiveSlots(); k++) if (k != slot && g_bot[k] && number[k] > 0) used[number[k]] = true;
                int free = 0;
                for (int n = 1; n <= top; n++) if (!used[n]) free++;
                int pick = free ? 1 + int((uint32_t(GetTickCount()) ^ (slot * 2654435761u)) % uint32_t(free)) : 1;
                int n = 1;
                for (; n <= top; n++) if (!used[n] && --pick == 0) break;
                if (n > top) n = 1;
                number[slot] = n;
                g_botNumber[slot] = n;   // 0.5.326: for the killcam's "killed by"
                char name[32] = {};
                snprintf(name, sizeof(name), "Bot %d", n);
                memset(base + player + 104, 0, 32);
                memcpy(base + player + 104, name, strlen(name));
                printf("bots: slot %u is named %s\n", slot, name);
            }
            printf("bots: slot %u given session player %u (a copy of %08X)\n", slot, i, host); fflush(stdout);
            return true;
        }
        return false;
    }

    bool AddBot(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t clients = Clients(base);
        if (clients == 0) return false;
        // From slot 4: the first four are the local players' (the server
        // sends to those by loopback, sub_825334E8, as their local player
        // entries say), and a remote player is given the first free slot.
        uint32_t slot = MaxClients;
        for (uint32_t i = 4; i < ActiveSlots(); i++)
            if (Guest::Read32(base, clients + i * ClientSize) == 0) { slot = i; break; }
        // 0.5.283: full 32 (Justin: not 28 or 30): with 4..31 full, the local
        // players' spare slots 3, 2 and 1 (no split screen here: only slot 0
        // is a person on this machine; a joiner takes the first free one, so
        // these go last). COD3_BOTS_ALL_SLOTS=0: from 4 only, as before.
        if (slot == MaxClients && FirstBotSlot() < 4)
            for (uint32_t i = 3; i >= FirstBotSlot() && i < 4; i--)
                if (Guest::Read32(base, clients + i * ClientSize) == 0) { slot = i; printf("bots: slots 4 and up full: a local player's spare slot %u for this bot\n", i); fflush(stdout); break; }
        if (slot == MaxClients) { printf("bots: no free slot\n"); fflush(stdout); return false; }
        const uint32_t client = clients + slot * ClientSize;
        printf("bots: adding a bot in slot %u (client %08X); the host's gun view pointer 0x82C37070 holds %08X\n", slot, client,
               Guest::Read32(base, 0x82C37070));
        // 0.5.155: 0.5.154's watch on guest 0x4 never fired: the host's gun
        // view is not wrecked in place, its pointer (0x82C37070[0]) is
        // emptied (the game then falls back on guest 0x0, all zero: field
        // of view 0, gun gone). The processor now names whoever writes it.
        {
            static bool watching = false;
            // 0.5.156: on the host's object's +4 (the one whose table the
            // crash calls), the pointer itself never changed.
            const uint32_t hostView = Guest::Read32(base, 0x82C37070);
            // 0.5.157: back on the pointers (156's log: they went 0, then
            // floats, right when bot 2 came in; 155's run had not hit it).
            (void)hostView;
            // 0.5.177: watch the error-queue POINTER, which176 saw become0.
            // ArmWatchpoints skips its calling thread. AddBot runs on main,
            // so using it here synchronously left main unwatched! A helper
            // arms main along with the other existing game threads.
            // 0.5.362: only with COD3_WATCH_ERRQUEUE=1 now: it held the one
            // hardware watch from the first bot on (the vehicle probe needs it)
            static const bool errQueueWatch = []() { const char* t = getenv("COD3_WATCH_ERRQUEUE"); return t && t[0] == '1'; }();
            if (!watching && errQueueWatch)
            {
                watching = true;
                Kernel::WatchWrite(0x82A87860);
                const HANDLE helper = CreateThread(nullptr, 0, [](void*) -> DWORD {
                    Kernel::ArmWatchpoints();
                    printf("bots: error-queue writer watch armed from helper (main included)\n"); fflush(stdout);
                    return 0;
                }, nullptr, 0, nullptr);
                if (helper) CloseHandle(helper);
                else { printf("bots: failed to start writer-watch helper: %lu\n", GetLastError()); fflush(stdout); }
            }
        }
        fflush(stdout);
        g_botSide[slot] = 0; // fresh slot/map: discard the previous occupant's team
        g_botLoadoutAssigned[slot]=false;
        g_bot[slot] = true;
        g_placeIndex[slot] = 0;
        ReservePlace(slot);
        BotClientScope clientScope(base, slot, "create");

        GunPointersCheck(base, "before the bot's slot is set up", slot);
        Call(ctx, base, SetUpSlot, client);
        GunPointersCheck(base, "SetUpSlot", slot);
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
        if (entity == 0) { printf("bots: slot %u has no entity\n", slot); fflush(stdout); g_bot[slot] = false; return false; }
        g_deathDrop[slot].NewLife(entity);

        // Where to stand and which way to face: nothing, as the title's join
        // passes for a new player (the spawn picks the place).
        const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
        memset(base + scratch, 0, 32);
        Call(ctx, base, ConnectIntoWorld, entity, scratch, scratch + 16, 0, 0);
        Guest::Write32(base, client + 44, 0xFFFFFFFFu);
        Guest::Write32(base, client + 0, 1);

        // The player's game record, cleared where the title's join clears it.
        const uint32_t record = Guest::Read32(base, entity + 512);
        if (record != 0)
        {
            Guest::Write32(base, record + 1924, 0);
            Guest::Write16(base, record + 1928, 0);
            Guest::Write16(base, record + 1340, 0);
            // 0.5.115: bots skip local kit menus, so choose a valid default.
            // Native getters 820CE3D8/820CE538 feed these halfwords to
            // 8220D1C8 -> 82206028. The join sentinel FFFF became class -1,
            // leaving the living bot without a weapon or character model.
            // 0.5.168: a class of his own, at random (the game's seven,
            // 0 to 6), and a role.
            // 0.5.198: from bot_kits.txt (BotKits): the kit his side is
            // shortest of, by weight.
            const std::vector<BotKits::Kit>& kits=LoadBotKits();
            std::vector<unsigned> teamCounts(kits.size(),0),totalCounts(kits.size(),0);
            const uint32_t side=BotSide(base,slot);
            for(uint32_t other=FirstBotSlot();other<ActiveSlots();++other) {
                if(other==slot || !g_bot[other] || !g_botLoadoutAssigned[other])continue;
                const unsigned choice=g_botLoadout[other];
                if(choice>=kits.size())continue;
                ++totalCounts[choice];
                if(BotSide(base,other)==side)++teamCounts[choice];
            }
            const unsigned choice=BotKits::Select(kits,teamCounts,totalCounts);
            g_botLoadout[slot]=uint8_t(choice);g_botLoadoutAssigned[slot]=true;
            g_botKit[slot]=kits[choice];
            const uint16_t kit=g_botKit[slot].base;
            printf("bots: loadout slot %u side %u kit %u: %s, %s, %d frags, %d smokes%s%s%s\n",slot,side,choice,
                g_botKit[slot].label[0]?g_botKit[slot].label:BotKits::ClassNames()[kit%7],
                g_botKit[slot].primary[0]?g_botKit[slot].primary:"the class's own gun",g_botKit[slot].frags,g_botKit[slot].smokes,
                g_botKit[slot].secondary[0]?", second ":"",g_botKit[slot].secondary,g_botKit[slot].sticky?", sticky bombs":"");fflush(stdout);
            Guest::Write16(base, record + 1920, kit);
            Guest::Write16(base, record + 1922, kit);
            {
                // 0.5.207: the mix kept per side (objective 25%, rusher 25%,
                // flanker 28%, camper 22%): each new bot takes the role his
                // side is furthest short of. The old roll, from the clock,
                // came out lopsided for bots added together (0.5.206 on Fuel
                // Plant: 7 campers, 8 flankers, 4 objective, 1 rusher of 20).
                static const float want[4] = { 0.38f, 0.25f, 0.25f, 0.12f };   // 0.5.277: more on the objective, fewer campers (Justin: bots not playing it)
                int have[4] = {}, total = 0;
                for (uint32_t other = FirstBotSlot(); other < ActiveSlots(); other++)
                    if (other != slot && g_bot[other] && BotSide(base, other) == side) { have[g_role[other] & 3]++; total++; }
                int best = 0; float bestShort = -1e9f;
                for (int k = 0; k < 4; k++)
                {
                    const float shortBy = want[k] * float(total + 1) - float(have[k]);
                    if (shortBy > bestShort + 1e-4f) { bestShort = shortBy; best = k; }
                }
                g_role[slot] = uint8_t(best);
                if (strcmp(g_botKit[slot].label, "demolitionist") == 0) g_role[slot] = RoleRusher;   // 0.5.326: Justin: more aggressive
                if (strcmp(g_botKit[slot].label, "rusher2") == 0) g_role[slot] = RoleFlanker;          // 0.5.329: Rusher 2 flanks
            }
            printf("bots: slot %u is a %s, class %u\n", slot, RoleName(g_role[slot]), kit);
            memset(base + record + 1504, 0, 406);
            Guest::Write32(base, record + 1504 + 408, 0);
        }
        const uint32_t team = Guest::Read32(base, entity + 520);
        // 0.5.168: his side from the start (was 0 until his side was picked a
        // moment later: Justin saw new bots as friendly green arrows on the
        // compass as they came in).
        if (team != 0) Guest::Write32(base, team + 4, BotSide(base, slot));
        GunPointersCheck(base, "before BeginPlayer", slot);
        Call(ctx, base, BeginPlayer, entity, 0);
        GunPointersCheck(base, "BeginPlayer", slot);
        printf("bots: bot in slot %u begun (entity %08X, record %08X)\n", slot, entity, record);
        fflush(stdout);
        g_bots++;
        // 0.5.154: with a second bot the host's gun view (guest 0x0 on: its
        // +4 is the object whose table the crashing calls read) is wrecked;
        // a hardware watch on guest 0x4 names every writer from now on.
        // (0.5.155: the watch moved to the host's gun view pointer, below.)
        g_age[slot] = 0;
        return true;
    }
}

// A bot's side and kit, picked the way the host's own menus pick them for
// a local player (InGameSwitchSides' apply, sub_8239FB10, and the kit
// menu's, sub_823B01C0): the game's callbacks at +3320 (side) and +3328
// (kit) of the object 0x82BCBF64 points to, given the player entity's handle
// (+484 of the entity), or, in the other mode the title can be in (the word
// the pointer at 0x82BCCAC0 points to not zero), sub_823EA4E0.
namespace
{
    constexpr uint32_t Callbacks = 0x82BCBF64, ModeObject = 0x82BCCAC0;
    constexpr uint32_t PickSideOther = 0x823EA4E0;

    void PickSide(PPCContext& ctx, uint8_t* base, uint32_t entity, uint32_t side)
    {
        BotClientScope clientScope(base, BotSlot(base, entity), "side");
        if (g_botCamera.load())
        {
            // Native 823EA4E0 at 823EA614 sets these two team fields.
            // Bots use server respawn separately; they have no local menus.
            const uint32_t team=Guest::Read32(base,entity+520);
            const uint32_t sessionPlayer=SessionPlayer(base,BotSlot(base,entity));
            if (team) Guest::Write32(base,team+4,side);
            if (sessionPlayer) Guest::Write16(base,sessionPlayer+604,uint16_t(side));
            // 0.5.506: COD3_BOT_SIDE_CALLBACK=1: the game's side callback too
            // (Callbacks +3320, as the menu path below), so the SCRIPT knows his
            // team (its join-team logic). Skipped, a bot has no script team, and
            // CTF's script refuses him the flag (488-505: bots touch the right
            // flag, its trigger records them, never given it; the host is).
            static const bool scriptSide = [] { const char* e = getenv("COD3_BOT_SIDE_CALLBACK"); return e && e[0] == '1'; }();
            if (scriptSide)
            {
                const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
                Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
                const uint32_t object = Guest::Read32(base, Callbacks);
                const uint32_t callback = object ? Guest::Read32(base, object + 3320) : 0;
                printf("bots: server-only side %u, entity %08X, session %08X; the script's side callback %08X called (COD3_BOT_SIDE_CALLBACK)\n", side, entity, sessionPlayer, callback);
                fflush(stdout);
                // 0.5.508: (handle, 1, 1) as the game's own menu (sub_8239FB10)
                // calls it; 507's (.., 1, 0) took the callback's other branch
                // and changed nothing either
                if (callback != 0) Call(ctx, base, callback, scratch, 1, 1);
                return;
            }
            printf("bots: server-only side %u, entity %08X, session %08X; menu callback skipped\n",side,entity,sessionPlayer);
            fflush(stdout);
            return;
        }
        const uint32_t mode = Guest::Read32(base, ModeObject);
        const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
        Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
        if (mode != 0 && Guest::Read32(base, mode) != 0)
        {
            printf("bots: side %u the other way\n", side); fflush(stdout);
            Call(ctx, base, PickSideOther, mode, entity, side, 0, 0);
            // That way goes through the session, which sets his team; setting
            // the team record under it froze him, half switched.
            return;
        }
        const uint32_t team = Guest::Read32(base, entity + 520);
        if (team != 0) Guest::Write32(base, team + 4, side);
        const uint32_t object = Guest::Read32(base, Callbacks);
        const uint32_t callback = object ? Guest::Read32(base, object + 3320) : 0;
        printf("bots: side %u (callback %08X)\n", side, callback); fflush(stdout);
        if (callback != 0) Call(ctx, base, callback, scratch, 1, 1);
    }

    // 0.5.217: a dead bot calls for a medic, as a player does from his death
    // screen (MPGAME_SELECT_KIT_CALL_MEDIC). The host handles a player's call
    // in sub_823E3860 (session message handler, vtable 0x820A0DB0): the
    // script callback at Callbacks+3352 (reenable_medic_call, sub_821F0DA0:
    // the "Medic!" shout, at most once in 5 s), given the caller's entity
    // number (+484). Called the same way for the bot.
    void CallMedic(PPCContext& ctx, uint8_t* base, uint32_t entity)
    {
        BotClientScope clientScope(base, BotSlot(base, entity), "medic call");
        const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
        Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
        const uint32_t object = Guest::Read32(base, Callbacks);
        const uint32_t callback = object ? Guest::Read32(base, object + 3352) : 0;
        static int told = 0;
        if ((told < 40 && told++ < 40)) { printf("bots: slot %u calls for a medic (callback %08X)\n", BotSlot(base, entity), callback); fflush(stdout); }
        if (callback != 0) Call(ctx, base, callback, scratch);
    }

    void PickKit(PPCContext& ctx, uint8_t* base, uint32_t entity, uint32_t kit)
    {
        BotClientScope clientScope(base, BotSlot(base, entity), "kit");
        if (g_botCamera.load())
        {
            printf("bots: keeping spawn kit for %08X; local kit menu skipped\n",entity);
            fflush(stdout);
            return;
        }
        const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
        Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
        const uint32_t object = Guest::Read32(base, Callbacks);
        const uint32_t callback = object ? Guest::Read32(base, object + 3328) : 0;
        printf("bots: kit %u (callback %08X)\n", kit, callback); fflush(stdout);
        if (callback != 0) Call(ctx, base, callback, scratch, kit);
    }

    // A bot put at one of his side's spawn points: the game's own
    // RespawnPlayer (sub_8220AF60), given the entity's handle and whether he
    // is on the allies, queues sub_822029F0, which asks the game type for a
    // spawn point of the side named "allies" or "axis" and spawns the player
    // there (the engine's spawn, +748 of the table at 0x82AA8030, given the
    // point's origin and angles).
    constexpr uint32_t RespawnPlayer = 0x8220AF60, SpawnAtPoint = 0x822029F0;
    constexpr uint32_t MakeString = 0x821E0E48;
    constexpr uint32_t AlliesName = 0x8205FCC8, AxisName = 0x8205FCD0;

    // RespawnPlayer only queues the spawn, and the queue it uses never ran it
    // for a bot; so sub_822029F0 is called here as the queued task would call
    // it: the entity's handle, and the side's name as one of the game's
    // strings (sub_821E0E48 makes one; sub_822029F0 lets go of it).
    void Respawn(PPCContext& ctx, uint8_t* base, uint32_t entity, bool allies)
    {
        // 0.5.139: Justin, on 0.5.138: frozen in place each time the bot
        // dies (his commands arrive, nothing moves), even with the current
        // client put back. The bot is spawned again while he holds a local
        // place, so the spawn is done as a local player's: the machine's
        // own player set-up runs for him and takes the host's controls. Out
        // of his local place for the spawn (as before 0.5.137, when spawning
        // never froze the host), and back in after.
        const uint32_t botSlot = BotSlot(base, entity);
        // 0.5.141: the place may already be empty (left at his death).
        const bool placed = botSlot < MaxClients && g_placeAt[botSlot] != 0 &&
                            (Guest::Read8(base, g_placeAt[botSlot]) == g_placeNumber[botSlot] ||
                             Guest::Read8(base, g_placeAt[botSlot]) == 0xFF);
        if (placed) Guest::Write8(base, g_placeAt[botSlot], 0xFF);
        struct PutBack
        {
            uint8_t* base; uint32_t at; uint8_t number; bool on;
            ~PutBack() { if (on) Guest::Write8(base, at, number); }
        } putBack{ base, placed ? g_placeAt[botSlot] : 0, placed ? g_placeNumber[botSlot] : uint8_t(0), placed };
        if (placed) { printf("bots: slot %u spawned out of his local place\n", botSlot); fflush(stdout); }
        struct SpawnFlag { SpawnFlag() { g_inSpawn = true; } ~SpawnFlag() { g_inSpawn = false; } } spawnFlag;
        BotClientScope clientScope(base, botSlot, "spawn");
        const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
        Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
        Guest::Write32(base, scratch + 8, 0);
        Call(ctx, base, MakeString, scratch + 8, allies ? AlliesName : AxisName);
        const uint32_t table = 0x82A98030, game = Guest::Read32(base, 0x82A99494);
        printf("bots: spawning as %s (string %08X, spawn %08X, point %08X, origin %08X, game %08X)\n",
               allies ? "allies" : "axis", Guest::Read32(base, scratch + 8),
               Guest::Read32(base, table + 748), Guest::Read32(base, table + 4084),
               Guest::Read32(base, table + 3948), game); fflush(stdout);
        LogWhere(base, "before", 0, entity);
        Call(ctx, base, SpawnAtPoint, scratch, scratch + 8);
        LogWhere(base, "after", 0, entity);
        // 0.5.464: not into the enemy's sights (Justin: "the enemy bots all kept
        // seeming to spawn in one area ... they just beamed me everytime";
        // 0.5.463's CTF on Rouen had most bots of both sides spawn in the map's
        // middle, around 0..750, the CTF bases far from it): with a living enemy
        // within 1200 of where the game put him, he is moved (the game's own
        // teleport, sub_8255E118, as the return point's) to his side's spawn
        // point (a spawn entity named for allies or axis; any spawn when the map
        // has none) farthest from every living enemy.
        if (const uint32_t record = Guest::Read32(base, entity + 512))
        {
            auto writeF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t mySide = Side(base, entity);
            std::vector<std::array<float, 3>> enemies;
            for (uint32_t o = 0; entities && o < ActiveSlots(); o++)
            {
                const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
                const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
                if (!r || e == entity || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
                const uint32_t s = Side(base, e);
                if (s == mySide && s != 0) continue;
                enemies.push_back({ ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8) });
            }
            auto nearestEnemy = [&](float x, float y) {
                float best = 1e30f;
                for (const auto& p : enemies) best = std::min(best, hypotf(p[0] - x, p[1] - y));
                return best;
            };
            const float x = ReadFloat(base, record), y = ReadFloat(base, record + 4);
            // 0.5.558: not in CTF, where CtfSpawnSides puts every spawn by his
            // own flag. 557's self-test: every bot's first life (24 of 24) ended
            // at the ENEMY flag's end, later lives 55 of 55 at his own: at the
            // start each side stands at the other's end (the port spawns bots by
            // the wrong one, 0.5.545), so every bot was "near an enemy" and this
            // moved him to "his side's spawn" by the allies/axis names, the wrong
            // end again, undoing the CTF move (user: "they are all grouped on
            // our flag").
            const bool ctfNow = g_obj.valid && GetTickCount() - g_obj.when < 10000 && g_obj.mode.find("ctf") != std::string::npos && g_ctfOwn[1] >= 0;
            if (!ctfNow && !enemies.empty() && std::isfinite(x) && nearestEnemy(x, y) < 1200.0f)
            {
                const char* const sideWord = allies ? "allies" : "axis";
                float best[3] = {}, bestD = -1.0f, anyBest[3] = {}, anyD = -1.0f;
                for (uint32_t i = 0; i < 4096; i++)
                {
                    const uint32_t e = Guest::Read32(base, 0x82BD1FC0 + 8 * i);
                    if (e < 0x40000000u || e >= 0xA0000000u) continue;
                    const uint32_t cls = Guest::Read32(base, e + 548);
                    if (cls < 0x40000000u || cls >= 0xA0000000u) continue;
                    char name[40] = {};
                    for (int c = 0; c < 39; c++) { const char ch = char(Guest::Read8(base, cls + 12 + c)); if (!ch) break; name[c] = char(tolower(static_cast<unsigned char>(ch))); }
                    if (!strstr(name, "spawn") || strstr(name, "intermission")) continue;
                    const float p[3] = { ReadFloat(base, e + 336), ReadFloat(base, e + 340), ReadFloat(base, e + 344) };
                    if (!std::isfinite(p[0]) || fabsf(p[0]) > 60000.0f) continue;
                    const float d = nearestEnemy(p[0], p[1]);
                    if (d > anyD) { anyD = d; memcpy(anyBest, p, sizeof(p)); }
                    if (strstr(name, sideWord) && d > bestD) { bestD = d; memcpy(best, p, sizeof(p)); }
                }
                const float* to = bestD >= 0.0f ? best : anyD >= 0.0f ? anyBest : nullptr;
                const float toD = bestD >= 0.0f ? bestD : anyD;
                if (to && toD > nearestEnemy(x, y) + 300.0f)
                {
                    static uint32_t position = 0;
                    if (!position) position = Guest::AllocatePhysical(0x20);
                    writeF(position, to[0]); writeF(position + 4, to[1]); writeF(position + 8, to[2] + 1.0f);
                    PPCContext place = ctx;
                    place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
                    place.r3.u64 = entity; place.r4.u64 = position; place.lr = 0;
                    Guest::Lookup(0x8255E118)(place, base);
                    for (int k = 0; k < 3; k++) writeF(record + 16 + 4 * k, 0.0f);
                    static int told = 0;
                    if (told++ < 60) { printf("bots: slot %u spawned %.0f from an enemy: moved to his side's spawn at %.0f %.0f (%.0f from one)\n", botSlot, nearestEnemy(x, y), to[0], to[1], toD); fflush(stdout); }
                }
            }
        }
        if (botSlot < MaxClients) g_botLife[botSlot]++;   // 0.5.207
        // 0.5.168: back in play (the 4 he was given dead is not undone by
        // the spawn itself, in case).
        if (const uint32_t record = Guest::Read32(base, entity + 512))
            if (int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 4)
                Guest::Write32(base, record + 1924, 3);
        // 0.5.166: Justin: right after a bot respawns he shows for a moment
        // as a friendly green arrow on the compass. The game's spawn puts
        // the host's side back on him (his session player is a copy of the
        // host's) and Think put it right only on his next frame. Now at once.
        {
            const uint32_t want = allies ? 1u : 2u;
            const uint32_t team = Guest::Read32(base, entity + 520);
            if (team != 0) Guest::Write32(base, team + 4, want);
            if (botSlot < MaxClients)
                if (const uint32_t player = SessionPlayer(base, botSlot)) Guest::Write16(base, player + 604, uint16_t(want));
            const uint32_t record=Guest::Read32(base,entity+512);
            if(botSlot<MaxClients && record && int32_t(Guest::Read32(base,record+304))>0)
                g_deathDrop[botSlot].NewLife(entity);
        }
    }

    void DropDeadBotWeapon(PPCContext& ctx,uint8_t* base,uint32_t slot,uint32_t entity)
    {
        uint32_t weapon=0;
        if(!g_deathDrop[slot].Begin(entity,weapon))return;
        const uint32_t ps=Guest::Read32(base,entity+512);
        if(!ps || weapon>=128)return;
        const uint8_t first=Guest::Read8(base,ps+1065),second=Guest::Read8(base,ps+1066);
        const bool owned=(Guest::Read32(base,ps+1056+4*(weapon>>5))&(1u<<(weapon&31)))!=0;
        if(!owned || (weapon!=first && weapon!=second)) {
            printf("bots: death drop slot %u weapon %u no longer owned/in kit; skipped\n",slot,weapon);fflush(stdout);return;
        }
        if(weapon==second && first!=second) {
            Guest::Write8(base,ps+1065,second);Guest::Write8(base,ps+1066,first);
        }
        BotClientScope context(base,slot,"death weapon drop");
        const uint32_t previousEntity=g_deathDropEntity,previousItem=g_deathDropCreated;
        g_deathDropEntity=entity;g_deathDropCreated=0;
        Call(ctx,base,0x82580E38,entity);
        const uint32_t item=g_deathDropCreated;
        g_deathDropEntity=previousEntity;g_deathDropCreated=previousItem;
        if(!item) { // unsuccessful native drop: undo only our temporary slot swap
            Guest::Write8(base,ps+1065,first);Guest::Write8(base,ps+1066,second);
        }
        printf("bots: death drop slot %u weapon %u native pickup %08X\n",slot,weapon,item);fflush(stdout);
        if(item>=0x40000000u && item<0xA0000000u) {
            printf("bots: drop snapshot slot %u item %08X handle %08X position %.1f %.1f %.1f model %08X rig %08X expires %u\n",
                slot,item,Guest::Read32(base,item+484),ReadFloat(base,item+336),ReadFloat(base,item+340),ReadFloat(base,item+344),
                Guest::Read32(base,item+540),Guest::Read32(base,item+492),Guest::Read32(base,item+760));fflush(stdout);
        }
    }

    void SetUpBots(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (entities == 0) return;
        for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
        {
            if (!g_bot[slot]) continue;
            const int age = ++g_age[slot];
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            if (entity == 0) continue;
            if (age == 15) g_viewWatchFrom = GetTickCount();   // the host's view watched from here (0.5.92)
            if (age == 20 && !(g_botSetup.load() & 2)) PickSide(ctx, base, entity, BotSide(base, slot));
            if (age == 40 && !(g_botSetup.load() & 1)) PickKit(ctx, base, entity, 0);
            if (age == 60) Respawn(ctx, base, entity, BotSide(base, slot) == 1);
        }
    }
}

// A bot's move, made by the game the way it makes a remote player's when
// his commands arrive (a remote player is run from his packets, not from
// the game's frame): ClientThink (sub_82593518) on the bot's entity handle,
// with the game's current client number (0x82BCBE9C, which SV_GetUsercmd
// reads) the bot's while it runs.
static uint32_t OwnedFlagWeapon(uint8_t* base, uint32_t record);   // 0.5.530: (below) for the bots' firing
namespace
{
    constexpr uint32_t ClientThink = 0x82593518;

    constexpr uint32_t ClientsMoveThemselves = 0x82B7C17C, MoveLocked = 0x82C73024;
    constexpr uint32_t FireWeapon = 0x82590C30;   // the server's firing of a player's weapon (his entity)

    void ThinkBots(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (entities == 0) return;
        const uint32_t saved = Guest::Read32(base, 0x82BCBE9Cu);
        for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
        {
            if (!g_bot[slot] || g_age[slot] < 45) continue;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            if (entity == 0) continue;
            KeepFov keepFov(base);   // 0.5.146
            BotGunViewScope gunView(base, slot, false);   // 0.5.153: his own gun view memory
            // 0.5.143: the current client is a local index (0-3); the bot's
            // own one (1-3) is used, not his slot (past the 4-entry lists:
            // the host's view zoomed with a second bot). ClientThink reads
            // it as the server client for SV_GetUsercmd, so that one call
            // is given the slot back (sub_82530270, below).
            Guest::Write32(base, CurrentClient, g_placeIndex[slot] ? g_placeIndex[slot] : slot);
            g_thinkSlot = slot;
            // His client's +1880 kept 2, as the host's own is: the server
            // fires a player's weapon (sub_82590C30) and applies damage to him
            // (sub_825862F8, by sub_8247E2F0) itself only for such a player;
            // others' it leaves to their own machines, which a bot has not.
            {
                const uint32_t clients = Clients(base);
                if (clients) Guest::Write32(base, clients + slot * ClientSize + 1880, 2);
            }
            // The weapon others see in his hands: the entity's byte +3 (the
            // host's holds his weapon there; a bot's was left 0, so his gun
            // was not drawn). Kept to the weapon in hand (record +164).
            {
                const uint32_t record = Guest::Read32(base, entity + 512);
                const uint32_t weapon = record ? Guest::Read32(base, record + 164) : 0;
                if (weapon < 256) Guest::Write8(base, entity + 3, uint8_t(weapon));
            }
            // Dead (health 0 or less) four seconds: spawned again, as the
            // game's own respawn would through his machine.
            {
                static uint32_t deadSince[MaxClients] = {};
                const uint32_t record = Guest::Read32(base, entity + 512);
                const int32_t health = record ? int32_t(Guest::Read32(base, record + 304)) : 1;
                const uint32_t nowMs = GetTickCount();
                if (health > 0) {
                    deadSince[slot] = 0;
                    g_deathDrop[slot].ObserveAlive(entity);
                }
                else if (deadSince[slot] == 0) deadSince[slot] = nowMs;
                // 0.5.168: dead is state 4. 0.5.165's dumps: the host's
                // record goes from 3 (in play) to 4 a moment after he dies; a
                // bot's stayed 3, so everything that shows a player's death
                // (sub_82400810 tests +1924 == 3: still alive) kept drawing
                // him standing: frozen, then gone at his respawn. Now a dead
                // bot is put in state 4 too, as the host is.
                // 0.5.169: this is its own test now. 0.5.168 put it in the
                // middle of the if/else chain above, so the respawn below ran
                // for every LIVE bot every frame (Justin: all 24 flashing,
                // falling, unkillable).
                if (health <= 0 && record && Guest::Read32(base, record + 1924) == 3)
                {
                    Guest::Write32(base, record + 1924, 4);
                    static int told = 0;
                    if ((told < 10 && told++ < 10)) { printf("bots: slot %u dead: state 3 -> 4, as the host's goes\n", slot); fflush(stdout); }
                }
                if (health<=0 && record) DropDeadBotWeapon(ctx,base,slot,entity);
                // 0.5.217: 1.5 s after he falls, he shouts for a medic (once a death).
                {
                    static uint32_t calledFor[MaxClients] = {};
                    if (health <= 0 && record && deadSince[slot] && nowMs - deadSince[slot] > 1500 && calledFor[slot] != deadSince[slot] &&
                        Guest::Read32(base, record + 1924) == 4 && getenv("COD3_NO_MEDIC_CALL") == nullptr)
                    {
                        calledFor[slot] = deadSince[slot];
                        CallMedic(ctx, base, entity);
                    }
                }
                // 0.5.216: a medic bot on his way: wait for him, up to 20 s.
                const bool waitForMedic = MedicComing(slot) && nowMs - deadSince[slot] < 25000;
                if (health <= 0 && waitForMedic)
                {
                    static uint32_t toldWait[MaxClients] = {};
                    if (toldWait[slot] != deadSince[slot]) { toldWait[slot] = deadSince[slot]; printf("bots: slot %u waits for a medic (one is coming; respawn held)\n", slot); fflush(stdout); }
                }
                if (health <= 0 && deadSince[slot] != 0 && nowMs - deadSince[slot] > 4000 && !waitForMedic && SessionPlayer(base, slot) != 0)
                {
                    deadSince[slot] = 0;
                    printf("bots: slot %u respawns\n", slot); fflush(stdout);
                    Respawn(ctx, base, entity, BotSide(base, slot) == 1);
                }
            }
            {
                int pokeOffset = 0, pokeValue = 0;
                DebugProbe::BotPoke(pokeOffset, pokeValue);
                const uint32_t record = Guest::Read32(base, entity + 512);
                if (record && pokeOffset > 0 && pokeOffset < 2400) Guest::Write8(base, record + pokeOffset, uint8_t(pokeValue));
            }
            // ClientThink (sub_82592CD0) clears a command's forward, side and
            // up moves while the word at 0x82B7C17C is set, as it is in a
            // networked game: there each player's own machine moves him. A
            // bot's is the host, so for his think it is cleared.
            // PmoveSingle (sub_824B39B8) also clears the moves of a player
            // whose byte at 0x82C73024 + his slot is set. That is a table of
            // the four local players' (sub_8247E218 sets all four), so for a
            // bot's slot, 4 and on, it reads past it into something else.
            // That byte is zero only for the bot's think, then put back.
            const uint32_t lockByte = MoveLocked + slot;
            const uint8_t locked = Guest::Read8(base, lockByte);
            Guest::Write8(base, lockByte, 0);
            // 0.5.143: and his local index's byte (the current client now).
            const uint32_t lockLocal = g_placeIndex[slot] ? MoveLocked + g_placeIndex[slot] : 0;
            const uint8_t lockedLocal = lockLocal ? Guest::Read8(base, lockLocal) : 0;
            if (lockLocal) Guest::Write8(base, lockLocal, 0);
            const uint32_t clientMoves = Guest::Read32(base, ClientsMoveThemselves);
            Guest::Write32(base, ClientsMoveThemselves, 0);
            Call(ctx, base, ClientThink, Guest::Read32(base, entity + 484));
            Guest::Write32(base, ClientsMoveThemselves, clientMoves);
            Guest::Write8(base, lockByte, locked);
            if (lockLocal) Guest::Write8(base, lockLocal, lockedLocal);
            CheckFov(base, "think", slot);
            // His shot. His weapon's own firing (in his move) never fires for
            // him: the server fires a player's weapon itself only through
            // sub_82590C30, gated on his client's +1880 being 2 (the host's
            // is; a bot's is 0). So when he wants to fire, at most every
            // 125 ms, that is called for him with the gate open just for it.
            static uint32_t lastShot[MaxClients] = {};
            const uint32_t nowMs = GetTickCount();
            // 0.5.530: never with a flag (firing it DROPS it: 529's carriers by
            // their own flag dropped it from sub_82590C30, the server's fire)
            const uint32_t fireRecord = Guest::Read32(base, entity + 512);
            const bool holdsFlag = fireRecord && OwnedFlagWeapon(base, fireRecord) != 0;
            if (g_firing[slot] && !holdsFlag && nowMs - lastShot[slot] >= 125 && !(DebugProbe::BotButtons() & 0x0100))
            {
                lastShot[slot] = nowMs;
                const uint32_t clients = Clients(base);
                const uint32_t gate = clients ? clients + slot * ClientSize + 1880 : 0;
                const uint32_t was = gate ? Guest::Read32(base, gate) : 0;
                if (gate) Guest::Write32(base, gate, 2);
                // The weapon fired is the entity's byte +3 (the host's holds
                // his weapon there; a bot's was left 0): his in hand.
                const uint32_t record = Guest::Read32(base, entity + 512);
                const uint32_t weapon = record ? Guest::Read32(base, record + 164) : 0;
                if (weapon != 0 && weapon < 256) Guest::Write8(base, entity + 3, uint8_t(weapon));
                // The shot goes along his view (+204 pitch, +208 yaw): set to
                // where his brain wants to look, give or take a little (so he
                // is a soldier, not an aimbot), for the shot only.
                float saved[2] = {};
                if (record)
                {
                    saved[0] = ReadFloat(base, record + 204); saved[1] = ReadFloat(base, record + 208);
                    // 0.5.456: loose at first, settling over 1.2 s (6 to 2 degrees),
                    // 1.5 more from the hip (Justin: "they seem to just lock on
                    // and beam me without me standing a chance"; was 2.5 always)
                    const uint32_t since = GetTickCount() - g_engagedAt[slot];
                    const float settle = since >= 1200u ? 0.0f : 1.0f - float(since) / 1200.0f;
                    const float spread = 2.0f + 4.0f * settle + (g_adsNow[slot] ? 0.0f : 1.5f);
                    const float pitch = g_wantPitch[slot] + (float(rand() % 2001) / 1000.0f - 1.0f) * spread;
                    const float yaw = g_wantYaw[slot] + (float(rand() % 2001) / 1000.0f - 1.0f) * spread;
                    uint32_t bits;
                    memcpy(&bits, &pitch, 4); Guest::Write32(base, record + 204, bits);
                    Guest::Write32(base, record + 2096, bits);
                    memcpy(&bits, &yaw, 4); Guest::Write32(base, record + 208, bits);
                    Guest::Write32(base, record + 2100, bits);
                }
                Call(ctx, base, FireWeapon, entity);
                if (record)
                {
                    uint32_t bits;
                    memcpy(&bits, &saved[0], 4); Guest::Write32(base, record + 204, bits);
                    memcpy(&bits, &saved[1], 4); Guest::Write32(base, record + 208, bits);
                }
                if (gate) Guest::Write32(base, gate, was);
                static uint32_t shots = 0;
                if (shots++ % 20 == 0) { printf("bots: slot %u fires (%u shots)\n", slot, shots); fflush(stdout); }
            }
            static uint8_t said[MaxClients] = {};
            if (said[slot] != locked + 1) { said[slot] = uint8_t(locked + 1); printf("bots: slot %u's lock byte %u\n", slot, locked); fflush(stdout); }
        }
        g_thinkSlot = MaxClients;
        Guest::Write32(base, CurrentClient, saved);
    }
}

// The bot's brain, run once a frame for each bot before his command is
// written. What he can know is what the server knows: every player's
// entity (its origin, +336), his record (+512: origin +0, view pitch +120
// and yaw +124, health +304) and his side.
//
// This first one is simple and campaign-like: the nearest living player of
// the other side is his target; he turns toward the target's head (a few
// degrees a frame, so it reads as aiming, not snapping), runs at him until
// he is close, then strafes from side to side, and fires once his aim is
// near. The command's angles are what his view becomes less what the game
// added when he spawned, so they are steered by the difference between the
// view he has and the one he wants.
struct BotOrders { int8_t forward, right; uint32_t buttons; bool wantsFire; int8_t up = 0; };
// 0.5.170: how far the enemy he can see is (flat), for the melee; set by
// Think each frame, a long way when he sees no one.
static float g_seeDist[MaxClients] = {};
// 0.5.197: how far his view is off what he wants to look at (yaw, degrees):
// on a mounted gun whose arc cannot reach, it stays large.
static float g_aimOff[MaxClients] = {};
// 0.5.198: an enemy he cannot reach (a floor between), left alone until then.
static uint32_t g_ignoreSlot[MaxClients] = {}, g_ignoreUntil[MaxClients] = {};
// 0.5.200: whom each bot is going after (0xFFFFFFFF: no one), so a crowd of
// bots spreads over several enemies instead of all chasing the nearest.
static uint32_t g_targetOf[MaxClients] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                                           0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu,
                                           0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
// 0.5.197: form, from his recent kills (+1) and deaths (-1), -5..5: a bot on
// a streak plays bolder, one dying again and again more carefully.
static int g_form[MaxClients] = {};
// 0.5.195: the fight's state BotExtras reads: a melee charge, and when (by
// GetTickCount) he wants his smoke thrown for cover.
static bool g_charging[MaxClients] = {};
static uint32_t g_wantSmoke[MaxClients] = {};
// 0.5.195: whom he has a clear line to (kept 1.5 s, so he fights who he can
// see rather than the nearest one behind a wall), and where (height too)
// his enemy was last seen, for tracking him when he ducks out of sight.
static uint32_t g_preferSlot[MaxClients], g_preferUntil[MaxClients] = {}, g_preferChecked[MaxClients] = {};
static uint32_t g_lastSeenSlot[MaxClients], g_lastSeenTime[MaxClients] = {};
static float g_lastSeenAt[MaxClients][3] = {};
static float g_pitch[MaxClients] = {};

// 0.5.89: crouching and grenades. The buttons, read off the host's own
// commands while each key was pressed (0.5.88's log): the stance key 0x2000
// (a tap crouches or stands up again, held it goes prone), the frag grenade
// 0x80 (held to cook, let go to throw), the smoke grenade 0x2. 0.5.88's
// guesses (0x200, 0x4000) were wrong: 0x4000 is set in every command.
static uint32_t g_nadeUntil[MaxClients] = {}, g_nextNade[MaxClients] = {};
static bool g_crouched[MaxClients] = {};
static uint32_t g_stanceTaps[MaxClients] = {};   // frames left of a stance tap
constexpr uint32_t StanceButton = 0x2000, GrenadeButton = 0x80, SprintButton = 0x4;
// 0.5.215: Justin never saw a bot's grenade or smoke land: 0.5.213's throw
// traces show the 0x80 and 0x2 presses changed nothing (no ammo spent), and
// the host's own presses in that log were 0x4000, 0x8000 and 0x200, never
// 0x80. So the button is now found by trying: each throw presses the next
// candidate, and 1.5 s later his grenade ammo (every weapon in his kit named
// like a grenade; smoke counted apart) is read back. The first press that
// spends one is kept for every bot ("bots: frag button found"). A bot with
// none left does not try.
// 0.5.416: what kind of grenade each weapon is, remembered. BotNadeAmmo and
// BotNadeWeapon looked at all 127 weapons each time a bot's grenades were
// asked about, each name lower-cased and searched up to nine times (BotNadeAmmo
// and strstr together 1.5% of the main thread on Marseille at 32
// players). The names do not change in a match: a weapon's kind is worked
// out again only when its name's bytes are not the ones it was worked out
// from (a new map's weapons can sit at the same addresses).
// COD3_NADEKINDCACHE=0: worked out every time, as before.
struct NadeKind { uint8_t name[40] = {}; bool known = false, nade = false, smoke = false, rg = false; };
static const NadeKind& NadeKindOf(uint8_t* base, uint32_t w, uint32_t text)
{
    static const bool cache = []() { const char* t = getenv("COD3_NADEKINDCACHE"); return !(t && t[0] == '0'); }();
    static thread_local NadeKind kinds[128];
    NadeKind& kind = kinds[w & 127];
    uint8_t raw[40] = {};
    for (int c = 0; c < 39; c++) { raw[c] = Guest::Read8(base, text + c); if (!raw[c]) break; }
    if (cache && kind.known && memcmp(kind.name, raw, sizeof(raw)) == 0) return kind;
    memcpy(kind.name, raw, sizeof(raw));
    char name[40] = {};
    for (int c = 0; c < 39 && raw[c]; c++) name[c] = char(tolower(raw[c]));
    // 0.5.739: World at War's signal flare is thrown as the smoke is (its copy, in its place)
    kind.smoke = strstr(name, "smoke") != nullptr || strstr(name, "flare") != nullptr;
    kind.nade = kind.smoke || strstr(name, "grenade") || strstr(name, "nade") || strstr(name, "mills") || strstr(name, "frag") ||
        strstr(name, "stiel") || strstr(name, "sticky") || strstr(name, "mk2") || strstr(name, "m24");
    kind.rg = strstr(name, "_rg") != nullptr;
    kind.known = true;
    return kind;
}
static uint32_t BotNadeAmmo(uint8_t* base, uint32_t record, bool smoke)
{
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    if (!list || !record) return 0;
    uint32_t total = 0;
    for (uint32_t w = 1; w < 128; w++)
    {
        const uint32_t def = Guest::Read32(base, list + 4 * w);
        if (def < 0x40000000u || def >= 0xA0000000u) continue;
        const uint32_t text = Guest::Read32(base, def + 8);
        if (text < 0x40000000u || text >= 0xC0000000u) continue;   // (0.5.757: copies' names lie at 0xAA..)
        const NadeKind& kind = NadeKindOf(base, w, text);
        if (!kind.nade || kind.smoke != smoke) continue;
        const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
        if (clip > 128 || spare > 128) continue;
        total += Guest::Read32(base, record + 4 * (172 + clip));
        if (spare != clip) total += Guest::Read32(base, record + 4 * (80 + spare));
    }
    return total;
}
struct NadeButtonSearch
{
    const char* what;
    uint32_t candidates[4];
    int tried = 0;           // index of the candidate now on trial
    int misses = 0;          // 0.5.629: its misses in a row
    uint32_t found = 0;      // the bit that spent one (0: still looking)
    uint32_t checkAt[32] = {}, before[32] = {}, bit[32] = {};
    uint32_t Next(uint32_t slot, uint32_t now, uint32_t ammo, uint32_t avoid)
    {
        uint32_t b = found;
        if (!b) { for (int n = 0; n < 4 && ((candidates[tried % 4] & avoid) || !candidates[tried % 4]); n++) tried++; b = candidates[tried % 4]; }
        if (slot < 32) { checkAt[slot] = now + 3000; before[slot] = ammo; bit[slot] = b; }
        return b;
    }
    void Check(uint32_t slot, uint32_t now, uint32_t ammo)
    {
        if (slot >= 32 || !checkAt[slot] || now < checkAt[slot]) return;
        const bool late = now > checkAt[slot] + 1500;   // he was dead or away in between: not a fair trial
        checkAt[slot] = 0;
        if (late || ammo > before[slot]) return;
        static int told = 0;
        if (ammo < before[slot])
        {
            misses = 0;
            if (!found) { found = bit[slot]; printf("bots: %s button found: %08X (slot %u, ammo %u -> %u)\n", what, found, slot, before[slot], ammo); fflush(stdout); }
        }
        else if (!found && bit[slot] == candidates[tried % 4])
        {
            // 0.5.629 (the user: bots "throwing more then one grenade at a time,
            // including smokes"). 628's play: slot 5's sticky (0x2) went
            // off 1.3 s after its press (the game's throw events), after the
            // 1.5 s check: "spent nothing", and every bot's frags went to 0x80;
            // slot 8's smoke the same. A press then threw the other kind or
            // one more. The check is at 3 s now, and a key is given up only
            // after two misses in a row (any spend between: kept).
            if (++misses < 2) { if ((told < 40 && told++ < 40)) { printf("bots: %s button %08X spent nothing (slot %u, ammo %u); once more before the next\n", what, bit[slot], slot, ammo); fflush(stdout); } return; }
            misses = 0;
            if ((told < 40 && told++ < 40)) { printf("bots: %s button %08X spent nothing twice (slot %u, ammo %u); trying the next\n", what, bit[slot], slot, ammo); fflush(stdout); }
            tried++;
        }
        else if (found && told++ < 40) { printf("bots: %s with %08X spent nothing (slot %u, ammo %u)\n", what, bit[slot], slot, ammo); fflush(stdout); }
    }
};
// 0.5.218's match: the host's frags AND smokes were both 0x2 ("host spent a
// frag ... keys 00000002"), and his throw's FIRST command carries the
// grenade's weapon number in +8 (5), then his gun again (0x18); the bots
// sent 0x2 with their gun's number (and sprint) and spent nothing. So 0x2
// comes first for both, and the first two commands of a bot's throw carry
// the grenade's number (g_throwWeapon, set from his kit).
// 0.5.454: only the two grenade keys (0x2, 0x80) are tried: 0.5.453's test had
// the smoke search gone on to 0x8000 after a sticky (not counted as a smoke)
// went with 0x2, and every bot then pressed that.
// 0.5.631: every log kept (play and tests, 10-23 to 10-30): the frag key found was
// 0x2 nearly always, the smoke key 0x2 too (and the host's own smoke); the few
// 0x80 / 0x4000 / 0x8000 "found" came while the search wandered (spends of other
// throws, "4 -> 0"), and a wandering key threw a second grenade (the user: "more
// then one grenade at a time, including smokes"). Both are 0x2 only; the weapon
// named in the throw's first command picks frag or smoke.
static NadeButtonSearch g_fragSearch{ "frag", { 0x2u, 0u, 0u, 0u } };
static NadeButtonSearch g_smokeSearch{ "smoke", { 0x2u, 0u, 0u, 0u } };
// 0.5.454: a bot's last throw of any grenade (Justin: "when bots throw smokes
// or sticky grenades its like they throw multiple at a time"): none again
// for 2.5 s, and each press is 200 ms (was 350)
static uint32_t g_lastThrowAt[32] = {};
constexpr uint32_t ThrowGapMs = 2500, ThrowPressMs = 200;
// 0.5.608: and one throw a side every 3 s within 900 of its last (the user:
// "when bots throw smokes or grenades like stickys they throw multiple";
// 0.5.454's gap was each bot's own: 607's 8-bot test had a side's whole wave,
// slots 4, 6, 8 and 10, throw smoke in the same instant as it went in)
constexpr uint32_t SideThrowGapMs = 3000;
static uint32_t g_sideThrowAt[3] = {};
static float g_sideThrowPos[3][2] = {};
static bool SideThrowFree(uint32_t side, float x, float y, uint32_t now)
{
    if (side < 1 || side > 2 || g_sideThrowAt[side] == 0) return true;
    return now - g_sideThrowAt[side] >= SideThrowGapMs || hypotf(x - g_sideThrowPos[side][0], y - g_sideThrowPos[side][1]) > 900.0f;
}
static void SideThrew(uint32_t side, float x, float y, uint32_t now)
{
    if (side < 1 || side > 2) return;
    g_sideThrowAt[side] = now; g_sideThrowPos[side][0] = x; g_sideThrowPos[side][1] = y;
}
// 0.5.220: 0.5.219's traces: the bot's throw started (record +168 went to 2,
// as the host's does) and was cancelled two commands later: the bot pressed
// the stance key (0x2000) with it and sprint (0x4) right after, where the
// host presses nothing else; the host's +164 also changes to the grenade
// and stays there until the throw is out (about 16 commands). So for 60
// commands after a throw starts the command carries no stance, sprint or
// fire (g_throwQuiet), and only the first names the grenade, as the host's.
static uint32_t g_throwWeapon[MaxClients] = {}, g_throwFrames[MaxClients] = {}, g_throwQuiet[MaxClients] = {};
// The weapon number of his frag (or smoke) with rounds left: the one the
// throw's first command names.
static uint32_t BotNadeWeapon(uint8_t* base, uint32_t record, bool smoke)
{
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    if (!list || !record) return 0;
    uint32_t best = 0, bestAmmo = 0;
    for (uint32_t w = 1; w < 128; w++)
    {
        const uint32_t def = Guest::Read32(base, list + 4 * w);
        if (def < 0x40000000u || def >= 0xA0000000u) continue;
        const uint32_t text = Guest::Read32(base, def + 8);
        if (text < 0x40000000u || text >= 0xC0000000u) continue;   // (0.5.757: copies' names lie at 0xAA..)
        const NadeKind& kind = NadeKindOf(base, w, text);
        if (!kind.nade || kind.smoke != smoke || kind.rg) continue;
        // 0.5.740: only one he owns (his 64-bit weapons mask, +1056): the signal
        // flare shares the smoke's rounds, and 0.5.739's bots named the smoke
        // they no longer had (the lower number) and threw smoke
        if (w < 64 && !((Guest::Read32(base, record + 1056 + 4 * (w >> 5)) >> (w & 31)) & 1)) continue;
        const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
        if (clip > 128 || spare > 128) continue;
        uint32_t ammo = Guest::Read32(base, record + 4 * (172 + clip));
        if (spare != clip) ammo += Guest::Read32(base, record + 4 * (80 + spare));
        // 0.5.756: the signal flare first when he has it (its rounds are the
        // smoke's pool: 755's flare-bots named another smoke-kind of the same
        // pool, and nothing went)
        if (smoke && ammo > 0 && Guest::Read8(base, text) == 's' && Guest::Read8(base, text + 1) == 'i') return w;
        if (ammo > bestAmmo) { bestAmmo = ammo; best = w; }
    }
    return best;
}
static uint32_t g_nadeBit[MaxClients] = {}, g_throwUntil[MaxClients] = {};   // the last also for the throw trace
static uint32_t BotGrenadeButton(uint32_t slot) { return g_nadeBit[slot] ? g_nadeBit[slot] : GrenadeButton; }
// The stance key tapped (one frame) whenever he wants to go down or come up,
// as a player taps it; held, it would put him prone.
// 0.5.195: Justin wants the bots to go prone more. The stance is now one
// he asks for (stand, crouch or prone; the last asked in a frame wins) and
// BotStanceControl, once per command, presses the stance key until his body
// is there: a tap for stand/crouch, held for prone (the game's own rule),
// read back from his eyes' height (+220: about 60 standing, 40 crouched,
// 10 prone). Prone only while he is not moving or jumping.
enum BotStance : uint8_t { StanceStand, StanceCrouch, StanceProne };
static uint8_t g_wantStance[MaxClients] = {};
static uint32_t g_stanceHoldUntil[MaxClients] = {}, g_stanceNextTry[MaxClients] = {};
static uint32_t BotCrouch(uint32_t slot, bool want)
{
    g_crouched[slot] = want;
    g_wantStance[slot] = want ? StanceCrouch : StanceStand;
    return 0;
}
static uint32_t BotProne(uint32_t slot)
{
    g_crouched[slot] = true;
    g_wantStance[slot] = StanceProne;
    return 0;
}
static uint8_t StanceFromEye(float eye)
{
    if (!std::isfinite(eye) || eye <= 0.0f) return StanceStand;
    if (eye < 24.0f) return StanceProne;
    if (eye < 50.0f) return StanceCrouch;
    return StanceStand;
}
static const char* StanceName(uint8_t s) { return s == StanceProne ? "prone" : s == StanceCrouch ? "crouched" : "standing"; }
static void ResetBotStance(uint32_t slot)
{
    g_wantStance[slot] = StanceStand;
    g_stanceHoldUntil[slot] = g_stanceNextTry[slot] = 0;
    g_crouched[slot] = false;
    g_stanceTaps[slot] = 0;
}
// The stance key for this command (GetTickCount timing): held while he
// goes prone (up to 0.9 s, again after 1.6 s if he is not down), tapped once
// (then 0.7 s to settle) to crouch or stand. Logged as it changes.
static uint32_t BotStanceControl(uint8_t* base, uint32_t slot, const BotOrders& orders)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return 0;
    const uint32_t now = GetTickCount();
    uint8_t want = g_wantStance[slot];
    if (want == StanceProne && (orders.forward || orders.right)) want = StanceCrouch;
    if (orders.up) want = StanceStand;
    const float eye = ReadFloat(base, record + 220);
    const uint8_t have = StanceFromEye(eye);
    static uint32_t told[MaxClients] = {};
    if (now < g_stanceHoldUntil[slot])
    {
        if (have == StanceProne || want != StanceProne) { g_stanceHoldUntil[slot] = 0; g_stanceNextTry[slot] = now + 500; return 0; }
        return StanceButton;
    }
    if (have == want || now < g_stanceNextTry[slot]) return 0;
    if (now - told[slot] > 4000)
    {
        told[slot] = now;
        printf("bots: slot %u %s -> %s (eye %.0f)\n", slot, StanceName(have), StanceName(want), eye); fflush(stdout);
    }
    if (want == StanceProne)
    {
        g_stanceHoldUntil[slot] = now + 900;
        g_stanceNextTry[slot] = now + 1600;
        return StanceButton;
    }
    g_stanceNextTry[slot] = now + 700;
    return StanceButton;
}

// Whether the command's pitch moves his view the same way (1) or the other
// way (-1). Set from debug.txt's bot_buttons 0x8000 bit for trying.
static float PitchSign() { return (DebugProbe::BotButtons() & 0x8000u) ? -1.0f : 1.0f; }

static float AngleDifference(float to, float from)
{
    float d = fmodf(to - from, 360.0f);
    if (d > 180.0f) d -= 360.0f;
    if (d < -180.0f) d += 360.0f;
    return d;
}

// 0.5.110: bounded native collision queries, using the corrected trace
// layout (position +0, plane normal +16). Each thread owns its scratch.
// 0.5.225: where the game thread's time goes (Justin: Marseille/ordnance
// dips; 0.5.224's profile: the game's main thread 97-100% busy, the
// graphics side 77%). Bot traces counted and timed; FeedBots and the
// server frame timed; logged every 5 s ("perf: ...").
static uint64_t g_traceNs = 0, g_traces = 0;
static BotNavigation::Hit BotTraceTimed(PPCContext& ctx, uint8_t* base, uint32_t ignored,
                                  uint32_t bodyEntity, BotNavigation::Point from,
                                  BotNavigation::Point to, bool body);
static BotNavigation::Hit BotTrace(PPCContext& ctx, uint8_t* base, uint32_t ignored,
                                  uint32_t bodyEntity, BotNavigation::Point from,
                                  BotNavigation::Point to, bool body)
{
    const auto t0 = std::chrono::steady_clock::now();
    BotNavigation::Hit hit = BotTraceTimed(ctx, base, ignored, bodyEntity, from, to, body);
    g_traceNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - t0).count());
    g_traces++;
    return hit;
}
static BotNavigation::Hit BotTraceTimed(PPCContext& ctx, uint8_t* base, uint32_t ignored,
                                  uint32_t bodyEntity, BotNavigation::Point from,
                                  BotNavigation::Point to, bool body)
{
    using namespace BotNavigation;
    Hit hit;
    if (!Finite(from) || !Finite(to)) return hit;
    static thread_local uint32_t memory = 0;
    if (!memory) memory = Guest::AllocatePhysical(0x21000);
    if (!memory) return hit;
    const uint32_t stack=memory+0x1FF00, scratch=memory+0x20000;
    const uint32_t start=scratch+96,end=scratch+112,mins=scratch+128,maxs=scratch+144,filter=scratch+160;
    memset(base+scratch,0,256);
    auto put=[&](uint32_t at,float f) { uint32_t word;memcpy(&word,&f,4);Guest::Write32(base,at,word); };
    put(start,from.x);put(start+4,from.y);put(start+8,from.z);
    put(end,to.x);put(end+4,to.y);put(end+8,to.z);
    if (body) {
        for (int k=0;k<3;k++) {
            float lo=ReadFloat(base,bodyEntity+240+4*k),hi=ReadFloat(base,bodyEntity+256+4*k);
            if (!std::isfinite(lo)||!std::isfinite(hi)||lo>hi||std::fabs(lo)>128||std::fabs(hi)>128) return hit;
            put(mins+4*k,lo);put(maxs+4*k,hi);
        }
        if (ReadFloat(base,maxs+8)-ReadFloat(base,mins+8)<8) return hit;
    }
    Guest::Write32(base,filter,0x82065DCC);
    Guest::Write32(base,filter+4,Guest::Read32(base,ignored+484));
    Guest::Write32(base,filter+20,0x00210011);
    Guest::Write32(base,stack,0);Guest::Write32(base,stack+84,0);Guest::Write32(base,stack+92,0);
    PPCContext call=ctx;call.r1.u64=stack;call.lr=0;
    call.r3.u64=scratch;call.r4.u64=start;call.r5.u64=mins;call.r6.u64=maxs;
    // Native82571EA0 uses r9=1 for the rounded capsule;82534200
    // forwards this to CM_BoxTrace824A0468. A box catches floor corners
    // and narrow geometry that the native player capsule can pass.
    call.r7.u64=end;call.r8.u64=filter;call.r9.u64=body?1:0;call.r10.u64=0;call.f1.f64=0;
    sub_82534200(call,base);
    hit.fraction=ReadFloat(base,scratch+32);
    hit.end={ReadFloat(base,scratch),ReadFloat(base,scratch+4),ReadFloat(base,scratch+8)};
    hit.normal={ReadFloat(base,scratch+16),ReadFloat(base,scratch+20),ReadFloat(base,scratch+24)};
    hit.handle=Guest::Read32(base,scratch+48);
    hit.surfaceFlags=Guest::Read32(base,scratch+36);
    hit.solid=base[scratch+60]||base[scratch+61];
    hit.valid=std::isfinite(hit.fraction)&&hit.fraction>=0&&hit.fraction<=1&&Finite(hit.end);
    if(body && (!hit.valid || hit.solid || hit.fraction<0.999f)) {
        const uint32_t index=Guest::Read32(base,ignored+484)&0x3FFFu;
        const uint32_t slot=index?index-1:MaxClients;
        static uint32_t lastTrace[MaxClients]={};
        if(slot<MaxClients && GetTickCount()-lastTrace[slot]>=3000) {
            lastTrace[slot]=GetTickCount();
            printf("bots: completed capsule trace slot %u from %.1f %.1f %.1f to %.1f %.1f %.1f fraction %.4f flags %u %u normal %.3f %.3f %.3f hit %08X\n",
                slot,from.x,from.y,from.z,to.x,to.y,to.z,hit.fraction,
                unsigned(base[scratch+60]),unsigned(base[scratch+61]),hit.normal.x,hit.normal.y,hit.normal.z,hit.handle);fflush(stdout);
        }
    }
    return hit;
}
static bool BotClipEmpty(uint8_t* base, uint32_t record)
{
    const uint32_t weapon = Guest::Read32(base, record + 164);
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t def = (weapon > 0 && weapon < 128 && list) ? Guest::Read32(base, list + 4 * weapon) : 0;
    if (def < 0x40000000u || def >= 0xA0000000u) return false;
    const uint32_t clip = Guest::Read32(base, def + 1468);
    return clip <= 128 && Guest::Read32(base, record + 4 * (172 + clip)) == 0;
}
struct BotTacticalState {
    uint32_t entity=0,checked=0,planned=0,sampled=0,coverUntil=0,coverSearch=0,escapeUntil=0,sightSince=0,targetSlot=0xFFFFFFFFu,logged=0;
    int health=100,hand=1;
    bool visible=false,haveStep=false;
    BotNavigation::Point step{},cover{},last{},ladderNormal{},ladderApproach{},jumpStart{};
    std::array<BotNavigation::Visit,8> visits{};
    unsigned visitNext=0,stalledSamples=0;
    BotNavigation::NetProgress progress;   // 0.5.196: stuck = no net progress, jitter or not
    BotNavigation::Point wanderTo{};
    uint32_t wanderUntil=0,ladderScanAt=0;
    uint32_t navChecked=0,nextJump=0,jumpUntil=0,jumpPulse=0,ladderUntil=0,ladderProgress=0,ladderCooldown=0;
    float ladderHeight=0;
    bool climbing=false,jumpPending=false;
    const char* stepWhy="none";   // 0.5.548: how the step was chosen (ctftrace)
    char blockWhy[120]="";        // 0.5.549: why the direct step was not reachable, the last time (ctftrace)
};
static BotTacticalState g_tactical[MaxClients];
// 0.5.195: where each bot failed to get past something, kept across his
// deaths (g_tactical is cleared when he dies), cleared for a new body or map.
static BotNavigation::FailedRoutes g_failedRoutes[MaxClients];
// 0.5.196: the height of the last goal he moved toward (NaN: none), so on a
// ladder he climbs down when what he wants is below him.
static float g_goalZ[MaxClients] = { NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN,
                                     NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN, NAN };
static uint32_t g_failedEntity[MaxClients] = {};

// 0.5.197: the map, learnt from where everyone walks (bot_map_knowledge.h),
// kept in bots\maps\<map>.txt beside the executable.
static BotNavigation::MapKnowledge g_mapKnow;
static std::string g_mapKnowName;
// Where each bot likes to be when no one is close: the outskirts, high
// ground (vantage points), low ground (trenches, gullies, cellars), or with
// the fight (hunter). By slot, so every team gets a mix; campers take the
// high ground. Three deaths on the way to or at his kind of spot and he tries
// the next kind (adapting).
// 0.5.329 (Justin): Rusher 2, a kit of his own (bot_kits.txt "rusher2"):
// the Sten, the Bren as his second gun, Mills bombs, built on the scout (his
// artillery, called with the binoculars). Very aggressive (a flanker who
// presses every fight); his errands go round: a push up to the enemy's
// spawn, clearing low ground (inside, room to room) toward them, back to the
// objective to hold it a while with the Bren, a flank round the outskirts,
// and again. BotRusher2 (below) picks his gun for it and calls his artillery.
static bool BotIsRusher2(uint32_t slot) { return slot < MaxClients && g_botLoadoutAssigned[slot] && strcmp(g_botKit[slot].label, "rusher2") == 0; }
struct BotExplore {
    uint32_t entity = 0;
    uint8_t r2Phase = 0;   // 0.5.329: Rusher 2's round of errands
    BotNavigation::MapKnowledge::Style style = BotNavigation::MapKnowledge::Style::Hunter;
    int32_t dest = -1;
    std::vector<BotNavigation::Point> path;
    size_t at = 0;
    uint32_t holdUntil = 0, giveUpAt = 0, nextPick = 0;
    unsigned strikesAtStart = 0;
    bool holdHigh = false;
    uint32_t activeTick = 0;   // 0.5.199: GetTickCount() when last on an errand (holdUntil is the server's clock)
    // 0.5.202: what this errand is for (Normal: his style's spot; Exit: out of
    // his own spawn; Push: up to the enemy's spawn; Flag: a look at the War
    // flag; Overwatch: a sniper's long watch from high ground).
    enum Purpose : uint8_t { Normal, Exit, Push, Flag, Overwatch } purpose = Normal;
    std::vector<BotNavigation::Point> watch;
    bool flagNext = false;
    // 0.5.282: a look out over a learnt lookout (a window or ledge where
    // long kills were made): its direction, until when, and the last one.
    float scanYaw = 0.0f;
    uint32_t scanUntil = 0, lookoutAt = 0;
    int32_t lookout = -1, lastLookout = -1;
};
static BotExplore g_explore[MaxClients];
static uint8_t g_styleShift[MaxClients] = {}, g_styleDeaths[MaxClients] = {};
// 0.5.202: where each side spawns, learnt from where its players appear
// (the last 32 spawns a side), and when each player last spawned (PC clock).
struct SideSpawns { BotNavigation::Point at[32]; int count = 0, next = 0; };
static SideSpawns g_spawns[3];
static uint32_t g_spawnedTick[MaxClients] = {};
static bool SpawnCentre(uint32_t side, BotNavigation::Point& out)
{
    if (side < 1 || side > 2 || g_spawns[side].count < 3) return false;
    double x = 0, y = 0, z = 0; const int n = g_spawns[side].count;
    for (int i = 0; i < n; i++) { x += g_spawns[side].at[i].x; y += g_spawns[side].at[i].y; z += g_spawns[side].at[i].z; }
    out = { float(x / n), float(y / n), float(z / n) };
    return true;
}
// 0.5.501: who really carries a flag: the game gives the carrier a flag
// weapon (mp_flag, mp_flag_axis, mp_flag_allies, by name in the weapon list);
// the owner's bit in +1056 says who. 498's "a bot took it" was credited by
// distance (the nearest), with the idle host 39 away taking flags himself.
// 0.5.526: the flag weapon (mp_flag, mp_flag_axis, mp_flag_allies) a player
// owns, 0 for none
static uint32_t OwnedFlagWeapon(uint8_t* base, uint32_t record)
{
    static int ids[3] = { -1, -1, -1 };
    if (ids[0] < 0)
    {
        const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
        const uint32_t wlist = Guest::Read32(base, 0x82BCBF9C);
        int n = 0;
        for (int32_t k = 1; wlist && k <= count && k < 128 && n < 3; k++)
        {
            const uint32_t def = Guest::Read32(base, wlist + 4 * uint32_t(k));
            if (!def) continue;
            const uint32_t text = Guest::Read32(base, def + 8);
            char name[24] = {};
            for (int c = 0; c < 23 && text; c++) { name[c] = char(Guest::Read8(base, text + c)); if (!name[c]) break; }
            if (strncmp(name, "mp_flag", 7) == 0) ids[n++] = k;
        }
        if (n == 0) return 0;
    }
    for (int i = 0; i < 3; i++)
        if (ids[i] > 0 && (Guest::Read32(base, record + 1056 + 4 * (uint32_t(ids[i]) >> 5)) & (1u << (ids[i] & 31)))) return uint32_t(ids[i]);
    return 0;
}
// 0.5.520: the holder of ONE flag's weapon (by name): 519 took the first
// holder of any flag weapon, so one flag's take went to the other's carrier,
// and the "sides corrected by a take" then swapped the sides. Taking side
// 2's flag gives mp_flag_allies (34; 516-519), so side 1's is mp_flag_axis.
static int FlagWeaponHolder(uint8_t* base, const char* want, uint32_t* weaponOut = nullptr)
{
    const int32_t count = int32_t(Guest::Read32(base, 0x82BCBF94));
    const uint32_t wlist = Guest::Read32(base, 0x82BCBF9C);
    int id = -1;
    for (int32_t k = 1; wlist && k <= count && k < 128 && id < 0; k++)
    {
        const uint32_t def = Guest::Read32(base, wlist + 4 * uint32_t(k));
        if (!def) continue;
        const uint32_t text = Guest::Read32(base, def + 8);
        char name[24] = {};
        for (int c = 0; c < 23 && text; c++) { name[c] = char(Guest::Read8(base, text + c)); if (!name[c]) break; }
        if (strcmp(name, want) == 0) id = k;
    }
    if (id < 0) return -1;
    const uint32_t list = Guest::Read32(base, EntityList);
    for (uint32_t o = 0; list && o < ActiveSlots() && o < MaxClients; o++)
    {
        const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        if (r && int32_t(Guest::Read32(base, r + 304)) > 0 && (Guest::Read32(base, r + 1056 + 4 * (uint32_t(id) >> 5)) & (1u << (id & 31))))
        { if (weaponOut) *weaponOut = uint32_t(id); return int(o); }
    }
    return -1;
}
// 0.5.482: where flag k is when it is not home (its carrier, or where it lies)
static bool FlagAway(uint8_t* base, int k, float out[3])
{
    if (k < 0 || k > 1) return false;
    if (g_flagDropped[k]) { memcpy(out, g_flagDropAt[k], 12); return true; }
    if (g_flagCarrier[k] < 0) return false;
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t e = list ? Guest::Read32(base, list + 4 * (uint32_t(g_flagCarrier[k]) + 1)) : 0;
    const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
    if (!r) return false;
    out[0] = ReadFloat(base, r); out[1] = ReadFloat(base, r + 4); out[2] = ReadFloat(base, r + 8);
    return true;
}
bool BotsScopedWeapon(uint8_t* base, uint32_t def);
static uint32_t HeldDef(uint8_t* base, uint32_t record)
{
    const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
    return (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
}
static const char* StyleName(BotNavigation::MapKnowledge::Style s)
{
    using S = BotNavigation::MapKnowledge::Style;
    return s == S::Outskirts ? "outskirts" : s == S::High ? "high ground" : s == S::Low ? "low ground" : "hunter";
}
static BotNavigation::MapKnowledge::Style BotStyle(uint32_t slot)
{
    using S = BotNavigation::MapKnowledge::Style;
    static const S bySlot[10] = { S::Outskirts, S::High, S::Low, S::Hunter, S::Outskirts, S::Low, S::High, S::Hunter, S::Outskirts, S::Hunter };
    S style = (g_role[slot] == RoleCamper) ? S::High : bySlot[slot % 10];
    for (uint8_t i = 0; i < g_styleShift[slot] % 4; i++)
        style = style == S::Outskirts ? S::High : style == S::High ? S::Low : style == S::Low ? S::Hunter : S::Outskirts;
    return style;
}

// 0.5.282: lookouts. Justin: bots should keep learning and get sharper game
// after game, and scouts and snipers should look out of windows. A kill made
// from 1100 or more away (anyone's: a person's or a bot's) marks where the
// shooter stood and the way he looked: a window, a ledge, a roof's edge that
// covers ground. The same spot again (within 160, facing within 35 degrees)
// scores it up. Kept per map beside the map's places (bots\maps\<map>
// .lookouts.txt); each new game starts from the old scores times 0.85, so
// what worked lately counts most (and the places' deaths and kills too).
// Snipers' watches go to the best lookouts; a scout or sniper passing within
// 200 of one stops 4-6 s and scans out of it, then goes on.
struct BotLookout { BotNavigation::Point at{}; float yaw = 0.0f, score = 0.0f; uint32_t usedAt = 0; };
static std::vector<BotLookout> g_lookouts;
static bool g_lookoutsChanged = false;
// 0.5.322 KILLCAM, stage 1 + 2 (Version A, docs/KILLCAM-PLAN.md).
// Recorder: every client frame (the camera hook, at most 30 a second) each
// player's origin (record +0..+8), view (+204 pitch, +208 yaw) and eye
// height are kept for 8 s. When this machine's player (client 0) is killed
// by another player (BotsLearnKill, from the G_Damage hook), the killer and
// the moment are kept; once the game's death view is up (view mode 19,
// 0.5.321's probe: 0 alive, 19 dead until the respawn), the camera
// (0x82C77E30: origin +48..+56, pitch +64, yaw +68, forward +80..+88;
// 0.5.321's probe) is put on the killer's recorded eyes from 4 s before the
// kill to 0.6 s after, at real speed, then the death view goes on as before.
// The world is the live one (Version A). The host's own player only (the
// joining PC has no server-side players to record). COD3_KILLCAM=0 off.
bool KillcamModelOk(uint8_t* base, uint32_t model);   // 0.5.365, defined by the vehicle probe
namespace Killcam
{
    // 0.5.628 (user: "the killcams ... still are a little bouncy", "solid camera
    // pisitionings instead of the jumping back and forth"): the killcam's clock
    // was GetTickCount64, which steps every 15.6 ms: samples (a server frame,
    // 15 ms, apart) shared a time or skipped one, and the replay's moment held
    // for frames then jumped. Its own clock now: the performance counter, in ms.
    uint64_t Ms()
    {
        static const double perMs = []() { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return double(f.QuadPart) / 1000.0; }();
        static const LONGLONG origin = []() { LARGE_INTEGER c; QueryPerformanceCounter(&c); return c.QuadPart; }();
        LARGE_INTEGER c; QueryPerformanceCounter(&c);
        return uint64_t(double(c.QuadPart - origin) / perMs) + 100000u;
    }
    struct Eye { float x, y, z, pitch, yaw; bool on; };
    struct Sample { uint64_t t; Eye eye[32]; };
    constexpr int Samples = 400;          // 0.5.346: 4.8 s at a sample a server frame (15 ms); 256 was 3.8 s, short of the 4 s played; 0.5.360: 400 (6 s): 4 s before + 1.5 s after the death
    // 0.5.360 (Justin, 359: "I can't tell when he even kills me ... when I
    // die it freezes slightly"). 22.17.22's frames: the killcam started on
    // the frame of the death and ended ON the kill, holding its last frame
    // still for 0.4 s (23 identical frames at 21.82-22.20). As CoD 4 does
    // it: your death plays first (DelayMs, the game's own death view, the
    // recording still running), then the killcam from PreMs before the
    // kill to PostMs after it: the shot, you falling, a beat, then back.
    // COD3_KILLCAM_DELAY / _PRE / _POST (ms) to tune.
    int EnvMs(const char* name, int dflt, int lo, int hi) { const char* t = getenv(name); int v = t ? atoi(t) : dflt; return v < lo ? lo : v > hi ? hi : v; }
    int DelayMs() { static const int v = EnvMs("COD3_KILLCAM_DELAY", 1500, 0, 1800); return v; }
    int PreMs() { static const int v = EnvMs("COD3_KILLCAM_PRE", 4000, 1000, 4000); return v; }
    int PostMs() { static const int v = EnvMs("COD3_KILLCAM_POST", 1200, 0, DelayMs()); return v; }
    Sample g_ring[Samples];
    int g_head = 0, g_count = 0;
    // 0.5.327: THE REAL REPLAY. Each sample also keeps every player's whole
    // playerstate (the first 1500 bytes of his record, the game's own ps
    // copy 820B1630 copies that much). On this machine the client's snapshot
    // takes its ps straight from the server's record (CL_ParseSnapshot
    // 82528258), and cgame reads its snapshots through 82451ED0 (into cg+48
    // / cg+1584, ps at +16). While the killcam plays, every snapshot cgame
    // reads gets the killer's recorded ps instead, flagged as followed
    // (ps+44 bit 0x00100000: cgame then interpolates it, no prediction,
    // 82465000), so the game itself draws his first-person view: his gun,
    // his aim, his shots (his ps events), as a spectator following him would
    // see. COD3_KILLCAM_REPLAY=0: the 0.5.323 camera-only killcam.
    constexpr uint32_t PsSize = 1500;
    // 0.5.332: the whole record kept (the ps and the rest of the player's
    // client state cgame reads: +1300, +1304, +2040, +2784..+2800)
    constexpr uint32_t RecSize = 2808;
    static uint8_t g_ps[Samples][32][RecSize];
    static bool g_psOk[Samples][32];
    // 0.5.329: and each player's whole entity (1072 bytes: the player
    // entities sit 0x430 apart), for everyone where they were
    constexpr uint32_t EntSize = 1072;
    static uint8_t g_ent[Samples][32][EntSize];
    static uint32_t g_entAt[Samples][32];
    // 0.5.341: POSES (Justin, 340: "it looked like i was laying down still").
    // A body's pose is not in its entity: it is the bone matrices in its
    // rig's pose buffer (entity +492 = the rig; rig +112 = the buffer, 48
    // bytes then 64 a bone; rig +171 the bones, +168 the model; read by
    // sub_8256B700, filled in place by sub_8251ED08 once a server frame:
    // for bots by UpdateBotBodies before the server frame). The killcam
    // replayed entities but every body kept its pose of NOW (yours: dead,
    // lying). Each sample now keeps every player's pose buffer too, and
    // during the replay each body's buffer holds its recorded pose (blended
    // between the two samples) for cgame's frame and the scene's, put back
    // after (the match's hit tests never see it). This machine's own body
    // is never evaluated by the game (first person), so the recorder
    // evaluates it (the same call as the bots' bodies).
    constexpr uint32_t PoseMax = 48 + 64 * 160;
    static uint8_t g_pose[Samples][32][PoseMax];
    static uint32_t g_poseSize[Samples][32];     // 0: none kept
    static uint16_t g_poseModel[Samples][32];
    // 0.5.358: the rig's model list of then (its gun). 358's dumps (the rig
    // is CoD's DObj; its builder is 82447898): +0x60.. each model's parent
    // bone, +0x68.. each model's first bone, +0x7C a mask, +0x80.. the models
    // (body, then the gun: kar98 4B144900), +0xAA how many models, +0xAB the
    // bones (92 body + 4 gun = 96). Dead, +0xAA goes back to 1 (the gun's
    // slot is left filled): so the replayed you had 96 bones but 1 model, no
    // gun. (353's "fallen flags" +97/+104 were the gun's parent bone and
    // first bone: zeroing them hid you.)
    constexpr uint32_t RigListBytes = 16 + 36 + 1;
    static uint8_t g_rigList[Samples][32][RigListBytes];
    static bool g_rigListOk[Samples][32];
    static uint32_t g_gunsPut = 0, g_rigsRebuilt = 0;   // 0.5.366: rebuilt
    void ReadRigList(uint8_t* base, uint32_t rig, uint8_t* out)
    {
        memcpy(out, base + rig + 0x60, 16); memcpy(out + 16, base + rig + 0x7C, 36); out[52] = base[rig + 0xAA];
    }
    void WriteRigList(uint8_t* base, uint32_t rig, const uint8_t* in)
    {
        memcpy(base + rig + 0x60, in, 16); memcpy(base + rig + 0x7C, in + 16, 36); base[rig + 0xAA] = in[52];
    }
    // 0.5.341: SHOTS (Justin, 340: "i didnt see the gun shooting"). cgame
    // plays an entity's events (fire, flash, sounds: CG_CheckEvents
    // sub_8246C208 from SetNextSnap sub_8246C2E0) when its sequence (+184)
    // is past the one it has played (+1000, in the entity). The replay put
    // back the RECORDED +1000 every frame, so in the past nothing was ever
    // new. The replay keeps its own "played" per player now.
    static uint32_t g_evDone[32];
    static bool g_evInit[32];
    // 0.5.346: THE TRUE RECORDING, part 1: every other object. 345's study
    // (CoD3MP.log "killcam study"): cgame draws from the server's entity
    // list (0x82BD9EC4, count 0x82BE9FC4: 245-250 listed: types 0 x151,
    // 1 players, 7 x25) and reads those entities' own bytes (other entities
    // sit 0x400 apart: their whole size); what it writes into them is its own
    // (+980, +996..+1000: events played). The replay showed the players of
    // then among the objects of NOW (no grenades, rockets, smoke of then).
    // Each sample now keeps the list and every listed non-player entity's
    // 0x400 bytes; the replay puts back the list and those bytes, exactly
    // the sample's (no blending), for cgame's scopes, and the live ones after.
    constexpr int ListMax = 1024, ObjMax = 320;
    constexpr uint32_t ObjSize = 0x400;
    constexpr uint32_t ListAt = 0x82BD9EC4u, ListCountAt = 0x82BE9FC4u;
    static uint32_t g_list[Samples][ListMax];
    static uint32_t g_listN[Samples];
    static uint32_t g_objAt[Samples][ObjMax];
    static uint8_t g_obj[Samples][ObjMax][ObjSize];
    static uint32_t g_objN[Samples];
    bool ObjOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_OBJECTS"); return !(t && t[0] == '0'); }(); return on; }
    bool BlendOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_BLEND"); return t && t[0] == '1'; }(); return on; }
    void RecordObjects(uint8_t* base, int at)
    {
        uint32_t n = Guest::Read32(base, ListCountAt);
        if (n > uint32_t(ListMax)) n = ListMax;
        g_listN[at] = n;
        g_objN[at] = 0;
        for (uint32_t i = 0; i < n; i++)
        {
            const uint32_t e = Guest::Read32(base, ListAt + 4 * i);
            g_list[at][i] = e;
            if (e < 0x40000000u || e >= 0xA0000000u || Guest::Read8(base, e) == 1) continue;   // players: their own path
            if (g_objN[at] >= uint32_t(ObjMax)) continue;
            g_objAt[at][g_objN[at]] = e;
            memcpy(g_obj[at][g_objN[at]], base + e, ObjSize);
            g_objN[at]++;
        }
    }
    static uint32_t g_evPlayed = 0, g_evFire = 0, g_posesPut = 0, g_posesMissed = 0;
    float GetBE(const uint8_t* buf, uint32_t at) { const uint32_t w = (uint32_t(buf[at]) << 24) | (uint32_t(buf[at + 1]) << 16) | (uint32_t(buf[at + 2]) << 8) | buf[at + 3]; float f; memcpy(&f, &w, 4); return f; }
    bool ReplayOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_REPLAY"); return !(t && t[0] == '0'); }(); return on; }
    void PutBE(uint8_t* buf, uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); buf[at] = uint8_t(w >> 24); buf[at + 1] = uint8_t(w >> 16); buf[at + 2] = uint8_t(w >> 8); buf[at + 3] = uint8_t(w); }
    uint64_t g_lastRecord = 0;
    int g_killer = -1;
    uint64_t g_deathAt = 0, g_playStart = 0;
    bool g_armed = false, g_playing = false;
    bool On() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM"); return !(t && t[0] == '0'); }(); return on; }
    int ClientOfEntity(uint8_t* base, uint32_t entity)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!entities || !entity) return -1;
        for (int c = 0; c < 32; c++) if (Guest::Read32(base, entities + 4 * uint32_t(c + 1)) == entity) return c;
        return -1;
    }
    // 0.5.343: a rig's first bone (16 floats after the 48-byte head), for the log
    void LogBone(uint8_t* base, const char* what, uint32_t rig)
    {
        const uint32_t buf = rig ? Guest::Read32(base, rig + 112) : 0;
        if (buf < 0x40000000u || buf >= 0xA0000000u) { printf("killcam: %s: no pose buffer\n", what); fflush(stdout); return; }
        printf("killcam: %s: rig %08X tree %08X bones %u, bone 0:", what, rig, Guest::Read32(base, rig + 128), Guest::Read8(base, rig + 171));
        for (int i = 0; i < 16; i++) printf(" %.2f", ReadFloat(base, buf + 48 + 4 * i));
        printf("\n"); fflush(stdout);
    }
    // 0.5.341: this machine's own body, evaluated (bots: UpdateBotBodies)
    void EvaluateOwn(PPCContext& ctx, uint8_t* base, uint32_t ent, uint32_t rec, uint32_t rig)
    {
        if (Guest::Read32(base, rec + 1932) != 2 || !Guest::Read32(base, rig + 128) || !Guest::Read32(base, rig + 32)) return;
        // 0.5.347: Justin, 346: "my body is stuck standing up while dead until
        // I respawn": this evaluation (the game never poses your own body in
        // first person) overwrote the game's own death pose. Alive only.
        if (int32_t(Guest::Read32(base, ent + 776)) <= 0) return;
        static int told = 0;
        if (!Guest::Read32(base, rig + 112))
        {
            Call(ctx, base, 0x82566430u, ent);   // the game's own: buffer made and filled
            if ((told < 3 && told++ < 3)) { printf("killcam: your body had no pose buffer: made (now %08X, %u bones)\n", Guest::Read32(base, rig + 112), Guest::Read8(base, rig + 171)); fflush(stdout); }
            return;
        }
        Call(ctx, base, 0x8251ED08u, rig, 0xFFFFFFFFu);
    }
    void Record(PPCContext& ctx, uint8_t* base, uint64_t now)
    {
        if (now - g_lastRecord < 15) return;   // 0.5.329: every server frame (was 30 a second from the drawing)
        g_lastRecord = now;
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!entities) return;
        Sample& s = g_ring[g_head];
        s.t = now;
        for (int c = 0; c < 32; c++)
        {
            Eye& e = s.eye[c];
            e.on = false;
            g_psOk[g_head][c] = false;
            const uint32_t ent = Guest::Read32(base, entities + 4 * uint32_t(c + 1));
            const uint32_t rec = ent ? Guest::Read32(base, ent + 512) : 0;
            if (rec < 0x40000000u || rec >= 0xA0000000u) continue;
            // 0.5.333: the dead are kept too (the moment you are shot and
            // fall is in the replay); only the camera's eye wants the living
            const bool alive = int32_t(Guest::Read32(base, ent + 776)) > 0;
            // 0.5.324: the entity's own origin (the server's; 323's bot killer
            // read z 0.0 and pitch 0.0 from his record), the record's view
            // angles when it has them, else the entity's angles
            e.x = ReadFloat(base, ent + 336); e.y = ReadFloat(base, ent + 340); e.z = ReadFloat(base, ent + 344);
            e.pitch = ReadFloat(base, rec + 204); e.yaw = ReadFloat(base, rec + 208);
            if (e.pitch == 0.0f && e.yaw == 0.0f) { e.pitch = ReadFloat(base, ent + 352); e.yaw = ReadFloat(base, ent + 356); }
            // 0.5.356: for a bot, the server's fire shoots along record +2096 /
            // +2100 (the view his machine would send; written by BotCommand and
            // the bot fire), not +204/+208 (355's killcam: the killer's eye
            // stared at a wall while he shot you). His eye in the replay is
            // that aim.
            if (g_bot[c])
            {
                const float ap = ReadFloat(base, rec + 2096), ay = ReadFloat(base, rec + 2100);
                if (std::isfinite(ap) && std::isfinite(ay) && fabsf(ap) <= 90.0f && fabsf(ay) <= 720.0f && (ap != 0.0f || ay != 0.0f)) { e.pitch = ap; e.yaw = ay; }
            }
            {
                static uint32_t toldAt[32] = {};
                const uint32_t ms = GetTickCount();
                if (c > 0 && ms - toldAt[c] > 20000) { toldAt[c] = ms; printf("killcam: client %d record %.1f %.1f %.1f view %.1f %.1f | entity %.1f %.1f %.1f angles %.1f %.1f\n", c, ReadFloat(base, rec), ReadFloat(base, rec + 4), ReadFloat(base, rec + 8), ReadFloat(base, rec + 204), ReadFloat(base, rec + 208), e.x, e.y, e.z, ReadFloat(base, ent + 352), ReadFloat(base, ent + 356)); fflush(stdout); }
            }
            // eye height: the record's view height (+220) when it reads as one, else standing
            float vh = ReadFloat(base, rec + 220);
            static int told = 0;
            if (told < 12 && c < 4) { told++; printf("killcam: client %d record +216 %d +220 %.2f (eye height check)\n", c, int32_t(Guest::Read32(base, rec + 216)), vh); fflush(stdout); }
            if (!(vh >= 8.0f && vh <= 80.0f)) vh = 60.0f;
            // 0.5.327: his whole ps, with the entity's place and the view
            // angles used above (a bot's ps origin z and pitch can read 0)
            if (ReplayOn() && std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z) && std::isfinite(e.pitch) && std::isfinite(e.yaw))
            {
                uint8_t* ps = g_ps[g_head][c];
                memcpy(ps, base + rec, RecSize);
                PutBE(ps, 0, e.x); PutBE(ps, 4, e.y); PutBE(ps, 8, e.z);
                PutBE(ps, 204, e.pitch); PutBE(ps, 208, e.yaw);
                g_psOk[g_head][c] = true;
                memcpy(g_ent[g_head][c], base + ent, EntSize);
                g_entAt[g_head][c] = ent;
                // 0.5.341: and his pose
                g_poseSize[g_head][c] = 0;
                g_rigListOk[g_head][c] = false;   // 0.5.358
                const uint32_t rig = Guest::Read32(base, ent + 492);
                if (rig >= 0x40000000u && rig < 0xA0000000u && Guest::Read32(base, rig + 172) == ent)
                {
                    if (c == 0) EvaluateOwn(ctx, base, ent, rec, rig);
                    // 0.5.358: what changes in a body's rig when its gun comes and goes.
                    // 357's video (22.17.22): the killer's gun in view is right the
                    // whole killcam; Justin: "still don't see my weapon" (on your
                    // body). Your rig read 92 bones even alive (352's dump), 96 at
                    // other times: so the gun is put on and taken off the rig. Each
                    // change of its +170 (models?) / +171 (bones) is dumped, for you
                    // and for the bots, with what +0 points at, so the next build
                    // can put your recorded gun on your replayed body.
                    {
                        static uint16_t last[32] = {}; static bool seen[32] = {}; static int dumps = 0;
                        const uint16_t mb = Guest::Read16(base, rig + 170);
                        if (c < 32 && (!seen[c] || last[c] != mb) && dumps < 30 && (c == 0 || dumps < 20))
                        {
                            const bool first = !seen[c];
                            seen[c] = true; last[c] = mb;
                            if (!first || c == 0)
                            {
                                dumps++;
                                printf("killcam: rig of client %d (%08X, %s, weapon %u) now +170 %u +171 %u:", c, rig, int32_t(Guest::Read32(base, ent + 776)) > 0 ? "alive" : "dead", Guest::Read32(base, rec + 164), mb >> 8, mb & 0xFF);
                                for (uint32_t o = 0; o < 0xC0; o += 4) printf("%s%08X", (o % 32) ? " " : "\n  ", Guest::Read32(base, rig + o));
                                for (const uint32_t o : { 0u, 32u, 64u, 96u })
                                {
                                    const uint32_t at = Guest::Read32(base, rig + o);
                                    if (at < 0x40000000u || at >= 0xC0000000u) continue;
                                    printf("\n  [+%u] %08X:", o, at);
                                    for (uint32_t k = 0; k < 48; k += 4) printf(" %08X", Guest::Read32(base, at + k));
                                }
                                printf("\n"); fflush(stdout);
                            }
                        }
                    }
                    // 0.5.352: your rig's first 0x100 bytes, once alive and once dead
                    // (Justin, 351: "I don't see the weapon I'm holding" in the
                    // killcam: the living body has 96 bones, the dead 92; what
                    // else of the rig changes when the weapon is dropped)
                    if (c == 0)
                    {
                        static bool aliveDone = false, deadDone = false;
                        const bool isAlive = int32_t(Guest::Read32(base, ent + 776)) > 0;
                        bool& done = isAlive ? aliveDone : deadDone;
                        if (!done && (isAlive ? g_count > 200 : true))
                        {
                            done = true;
                            printf("killcam: your rig %s (%08X):", isAlive ? "ALIVE" : "DEAD", rig);
                            for (uint32_t k = 0; k < 0x100; k += 4) printf("%s%08X", k % 32 ? " " : "\n  ", Guest::Read32(base, rig + k));
                            printf("\n"); fflush(stdout);
                        }
                    }
                    // 0.5.343: your body's pose while you play, every 10 s (is it standing?)
                    if (c == 0)
                    {
                        static uint32_t toldAt = 0; static int toldN = 0;
                        const uint32_t ms = GetTickCount();
                        if (toldN < 30 && ms - toldAt > 10000) { toldAt = ms; toldN++; char what[96]; snprintf(what, sizeof what, "your body while you play (%s, +1932 %u)", int32_t(Guest::Read32(base, ent + 776)) > 0 ? "alive" : "dead", Guest::Read32(base, rec + 1932)); LogBone(base, what, rig); }
                    }
                    const uint32_t buf = Guest::Read32(base, rig + 112), bones = Guest::Read8(base, rig + 171);
                    const uint32_t size = 48 + 64 * bones;
                    if (buf >= 0x40000000u && buf < 0xA0000000u && bones && size <= PoseMax)
                    {
                        memcpy(g_pose[g_head][c], base + buf, size);
                        g_poseSize[g_head][c] = size;
                        g_poseModel[g_head][c] = Guest::Read16(base, rig + 168);
                    }
                    ReadRigList(base, rig, g_rigList[g_head][c]);   // 0.5.358
                    g_rigListOk[g_head][c] = true;
                    static int told = 0;
                    if ((told < 6 && told++ < 6)) { printf("killcam: client %d's pose: rig %08X buffer %08X, %u bones, model %u%s\n", c, rig, buf, bones, Guest::Read16(base, rig + 168), size > PoseMax ? " (too many bones: not kept)" : ""); fflush(stdout); }
                }
            }
            e.z += vh;
            e.on = alive && std::isfinite(e.x) && std::isfinite(e.y) && std::isfinite(e.z) && std::isfinite(e.pitch) && std::isfinite(e.yaw);
        }
        if (ObjOn()) RecordObjects(base, g_head);   // 0.5.346
        g_head = (g_head + 1) % Samples;
        if (g_count < Samples) g_count++;
    }
    // the killer's eye at time t, between the two samples around it
    bool EyeAt(int who, uint64_t t, Eye& out)
    {
        const Sample* before = nullptr; const Sample* after = nullptr;
        for (int k = 0; k < g_count; k++)
        {
            const Sample& s = g_ring[(g_head - 1 - k + Samples) % Samples];
            if (!s.eye[who].on) continue;
            if (s.t <= t) { before = &s; break; }
            after = &s;
        }
        if (!before && !after) return false;
        if (!before) { out = after->eye[who]; return true; }
        if (!after || after->t == before->t) { out = before->eye[who]; return true; }
        const float f = float(double(t - before->t) / double(after->t - before->t));
        const Eye& a = before->eye[who]; const Eye& b = after->eye[who];
        auto lerpAngle = [&](float x, float y) { float d = fmodf(y - x + 540.0f, 360.0f) - 180.0f; return x + d * f; };
        out = { a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f, lerpAngle(a.pitch, b.pitch), lerpAngle(a.yaw, b.yaw), true };
        return true;
    }
    void NoteKill(uint8_t* base, uint32_t attacker, uint32_t victim)
    {
        if (!On()) return;
        const int v = ClientOfEntity(base, victim), a = ClientOfEntity(base, attacker);
        if (v != 0 || a <= 0) return;   // only this machine's player, killed by another player
        g_killer = a;
        g_deathAt = Ms();
        g_armed = true;
        g_playing = false;
        printf("killcam: this player killed by client %d: the killcam is ready\n", a);
        fflush(stdout);
    }
    // 0.5.327: the killer's recorded ps for time t (the newest sample not
    // after t, else the oldest kept)
    const uint8_t* PsAt(int who, uint64_t t)
    {
        const uint8_t* oldest = nullptr;
        for (int k = 0; k < g_count; k++)
        {
            const int i = (g_head - 1 - k + Samples) % Samples;
            if (!g_psOk[i][who]) continue;
            if (g_ring[i].t <= t) return g_ps[i][who];
            oldest = g_ps[i][who];
        }
        return oldest;
    }
    bool Replaying() { return On() && ReplayOn() && g_playing && g_killer > 0 && g_killer < 32; }
    bool ListSwapOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_AS_KILLER"); return !(t && t[0] == '0'); }(); return on; }
    constexpr uint32_t Cg = 0x82C7D490u, CgSize = 6368u, LocalClient = 0x82BCBE9Cu;
    bool EyeBlendOn();   // 0.5.628 (below)
    bool BlendPs(int who, uint64_t t, uint8_t* out, bool eye);
    // a snapshot cgame has just read (82451ED0's result): the killer's ps in it
    void PatchSnapshot(uint8_t* base, uint32_t snap)
    {
        if (!Replaying() || snap < 0x40000000u || snap >= 0xA0000000u) return;
        const uint64_t into = Ms() - g_playStart;
        const uint8_t* ps = PsAt(g_killer, g_deathAt - PreMs() + into);
        static uint8_t blended[RecSize];
        if (EyeBlendOn() && BlendPs(g_killer, g_deathAt - PreMs() + into, blended, true)) ps = blended;   // 0.5.628
        if (!ps) return;
        memcpy(base + snap + 16, ps, PsSize);
        const uint32_t flags = snap + 16 + 44;
        Guest::Write32(base, flags, Guest::Read32(base, flags) | 0x00100000u);   // followed: interpolated, not predicted
        // 0.5.339 (338: everyone drawn at last, but still not you): with the
        // follow bit on, CG_Player (82469968, loc_82469E68) hides the body of
        // the client the snapshot names at +1360 (its ps +1344) (the followed one), as
        // entities[that + 1]. The killer's ps names himself there, and his
        // place in the entity list holds YOU during the killcam: so you were
        // hidden. Named 0 instead: place 1, which holds the killer.
        {
            static int told = 0;
            if ((told < 4 && told++ < 4)) { printf("killcam: snapshot ps +1360 (the followed client) was %d: set to 0\n", int32_t(Guest::Read32(base, snap + 1360))); fflush(stdout); }
            Guest::Write32(base, snap + 1360, 0);   // snap+1360 (= ps+1344): CG_Player reads [cg+32]+1360
        }
        static int told = 0;
        if ((told < 6 && told++ < 6))
        {
            printf("killcam: snapshot %08X (time %d) given client %d's ps: origin %.1f %.1f %.1f view %.1f %.1f, pm_type %d, flags %08X, weapon %u, health %d\n",
                   snap, int32_t(Guest::Read32(base, snap + 4)), g_killer, ReadFloat(base, snap + 16), ReadFloat(base, snap + 20), ReadFloat(base, snap + 24),
                   ReadFloat(base, snap + 16 + 204), ReadFloat(base, snap + 16 + 208), int32_t(Guest::Read32(base, snap + 16 + 36)),
                   Guest::Read32(base, flags), Guest::Read32(base, snap + 16 + 164), int32_t(Guest::Read32(base, snap + 16 + 304)));
            fflush(stdout);
        }
    }
    // both snapshots cgame holds now (cg+32 the current, cg+36 the next), at the start
    void PatchHeld(uint8_t* base)
    {
        const uint32_t cg = Cg + CgSize * Guest::Read32(base, LocalClient);
        const uint32_t a = Guest::Read32(base, cg + 32), b = Guest::Read32(base, cg + 36);
        PatchSnapshot(base, a);
        if (b != a) PatchSnapshot(base, b);
    }
    // 0.5.628: each frame of the replay, the eye of now (blended) in both
    // snapshots cgame holds: it interpolates the followed ps between them,
    // which were given the killer's state at the moments they were read (a
    // server frame apart, not the frame's moment): the view went past and
    // back. The same eye in both is exactly the eye of now. Only the place,
    // speed and view: his events stay where they were put.
    void HoldEye(uint8_t* base)
    {
        if (!Replaying() || !EyeBlendOn()) return;
        static uint8_t eye[RecSize];
        if (!BlendPs(g_killer, g_deathAt - PreMs() + (Ms() - g_playStart), eye, true)) return;
        const uint32_t cg = Cg + CgSize * Guest::Read32(base, LocalClient);
        for (const uint32_t held : { Guest::Read32(base, cg + 32), Guest::Read32(base, cg + 36) })
        {
            if (held < 0x40000000u || held >= 0xA0000000u) continue;
            for (uint32_t k : { 0u, 4u, 8u, 16u, 20u, 24u, 204u, 208u, 212u }) memcpy(base + held + 16 + k, eye + k, 4);
        }
    }
    // after cgame's player state is made (82464D38): the view weapon it took
    // from this machine's own player on the server (cg+4732) from the killer's
    void PatchWeapon(uint8_t* base)
    {
        if (!Replaying()) return;
        const uint32_t cg = Cg + CgSize * Guest::Read32(base, LocalClient);
        const uint32_t weapon = Guest::Read32(base, cg + 3136 + 164), list = Guest::Read32(base, 0x82BCBF9C);
        if (weapon && weapon < 128 && list) Guest::Write32(base, cg + 4732, Guest::Read32(base, list + 4 * weapon));
    }
    // 0.5.328: 327's log: the snapshots got the killer's ps and the view mode
    // went to 0 (alive), but the view stayed at our own body: on the host,
    // cgame's first-person view (824674E0 mode 0, loc_82467840) reads this
    // player's SERVER record (entities[1]+512: +0 origin, +204 angles)
    // directly, and so do the view mode (82462C68) and the view weapon. So for
    // the length of cgame's frame (CG_DrawActiveFrame 8246C748) this player's
    // record holds the killer's recorded ps; the real one goes back right
    // after, before the server runs again. Nothing of the match changes.
    // 0.5.329 (Justin, 328: "seen it for sure", but laggy, and the others
    // not where they were): the two samples around the replay's moment and
    // how far between them, so places and aims are blended smoothly (a
    // sample a server frame) instead of jumping from one to the next.
    bool Around(int who, uint64_t t, int& a, int& b, float& f, bool force = false)
    {
        a = b = -1; f = 0.0f;
        for (int k = 0; k < g_count; k++)
        {
            const int i = (g_head - 1 - k + Samples) % Samples;
            if (!g_psOk[i][who]) continue;
            if (g_ring[i].t <= t) { a = i; break; }
            b = i;
        }
        if (a < 0 && b < 0) return false;
        if (a < 0) { a = b; return true; }
        if (!BlendOn() && !force) { b = a; f = 0.0f; return true; }   // 0.5.346: exactly the sample (no blending; COD3_KILLCAM_BLEND=1: 329's)
        if (b < 0 || g_ring[b].t == g_ring[a].t) { b = a; return true; }
        f = std::clamp(float(double(t - g_ring[a].t) / double(g_ring[b].t - g_ring[a].t)), 0.0f, 1.0f);
        return true;
    }
    float LerpAngle(float x, float y, float f) { const float d = fmodf(y - x + 540.0f, 360.0f) - 180.0f; return x + d * f; }
    // the entity's places that hold its origin (found once, from moving
    // players: the offsets whose three floats are its +336 origin)
    std::vector<uint32_t> g_originAt;
    bool g_originFound = false;
    void FindOrigins()
    {
        if (g_originFound || g_count < 20) return;
        int votes[EntSize / 4] = {};
        int players = 0;
        const int i = (g_head - 1 + Samples) % Samples;
        for (int c = 0; c < 32; c++)
        {
            if (!g_psOk[i][c]) continue;
            const uint8_t* e = g_ent[i][c];
            const float x = GetBE(e, 336), y = GetBE(e, 340), z = GetBE(e, 344);
            if (!std::isfinite(x) || (fabsf(x) < 1.0f && fabsf(y) < 1.0f)) continue;
            players++;
            for (uint32_t k = 0; k + 12 <= EntSize; k += 4)
                if (fabsf(GetBE(e, k) - x) < 0.5f && fabsf(GetBE(e, k + 4) - y) < 0.5f && fabsf(GetBE(e, k + 8) - z) < 0.5f) votes[k / 4]++;
        }
        if (players < 3) return;
        g_originFound = true;
        std::string list;
        for (uint32_t k = 0; k < EntSize / 4; k++)
            if (votes[k] * 2 >= players) { g_originAt.push_back(k * 4); list += " +" + std::to_string(k * 4); }
        printf("killcam: an entity's origin is kept at%s (of %d players)\n", list.c_str(), players);
        fflush(stdout);
    }
    // the blended ps / entity of client c at the replay's moment, into out
    // 0.5.628: eye: the killer's eye (his place, speed and view) blended
    // between the two samples around t whatever COD3_KILLCAM_BLEND says, the
    // rest of his state exactly the earlier sample's (his shots on their
    // frame); not across a jump (a respawn). COD3_KILLCAM_EYEBLEND=0: as before.
    bool EyeBlendOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_EYEBLEND"); return !(t && t[0] == '0'); }(); return on; }
    bool BlendPs(int who, uint64_t t, uint8_t* out, bool eye = false)
    {
        int a, b; float f;
        if (!Around(who, t, a, b, f, eye)) return false;
        const uint8_t* A = g_ps[a][who]; const uint8_t* B = g_ps[b][who];
        if (eye)
        {
            const float dx = GetBE(B, 0) - GetBE(A, 0), dy = GetBE(B, 4) - GetBE(A, 4), dz = GetBE(B, 8) - GetBE(A, 8);
            if (dx * dx + dy * dy + dz * dz > 128.0f * 128.0f) { B = A; f = 0.0f; }
        }
        memcpy(out, eye ? A : (f < 0.5f ? A : B), RecSize);   // whole state: the nearer sample (the eye's: the earlier)
        for (uint32_t k : { 0u, 4u, 8u, 16u, 20u, 24u }) PutBE(out, k, GetBE(A, k) + (GetBE(B, k) - GetBE(A, k)) * f);
        for (uint32_t k : { 204u, 208u, 212u }) PutBE(out, k, LerpAngle(GetBE(A, k), GetBE(B, k), f));
        return true;
    }
    bool BlendEnt(int who, uint64_t t, uint8_t* out, uint32_t& at)
    {
        int a, b; float f;
        if (!Around(who, t, a, b, f)) return false;
        if (g_entAt[a][who] != g_entAt[b][who]) b = a;
        at = g_entAt[a][who];
        const uint8_t* A = g_ent[a][who]; const uint8_t* B = g_ent[b][who];
        memcpy(out, (f < 0.5f ? A : B), EntSize);
        for (uint32_t k : g_originAt)
            for (uint32_t j = 0; j < 12; j += 4) PutBE(out, k + j, GetBE(A, k + j) + (GetBE(B, k + j) - GetBE(A, k + j)) * f);
        for (uint32_t j = 352; j < 364; j += 4) PutBE(out, j, LerpAngle(GetBE(A, j), GetBE(B, j), f));
        return true;
    }
    // 0.5.341: client c's recorded pose at the replay's moment (the 48-byte
    // head from the nearer sample, the bone matrices blended), for a rig of
    // this model and size only
    bool BlendPose(int who, uint64_t t, uint16_t model, uint32_t size, uint8_t* out)
    {
        int a, b; float f;
        if (!Around(who, t, a, b, f)) return false;
        const bool okA = g_poseSize[a][who] == size && g_poseModel[a][who] == model;
        const bool okB = g_poseSize[b][who] == size && g_poseModel[b][who] == model;
        if (!okA && !okB) return false;
        if (!okA || !okB || a == b) { memcpy(out, okA ? g_pose[a][who] : g_pose[b][who], size); return true; }
        const uint8_t* A = g_pose[a][who]; const uint8_t* B = g_pose[b][who];
        memcpy(out, f < 0.5f ? A : B, 48);
        for (uint32_t k = 48; k + 4 <= size; k += 4)
        {
            const float x = GetBE(A, k), y = GetBE(B, k);
            PutBE(out, k, (std::isfinite(x) && std::isfinite(y)) ? x + (y - x) * f : (f < 0.5f ? x : y));
        }
        return true;
    }
    // 0.5.328: 327's log: the snapshots got the killer's ps and the view mode
    // went to 0 (alive), but the view stayed at our own body: on the host,
    // cgame's first-person view (824674E0 mode 0, loc_82467840) reads this
    // player's SERVER record (entities[1]+512: +0 origin, +204 angles)
    // directly, and so do the view mode (82462C68) and the view weapon. So for
    // the length of cgame's frame (CG_DrawActiveFrame 8246C748) this player's
    // record holds the killer's recorded ps; the real one goes back right
    // after, before the server runs again. Nothing of the match changes.
    // 0.5.329: every player too: his entity and his record as they were at
    // that moment (blended), so the others (and you) are where they were.
    // The killer's own body is moved out of sight (his view is the camera).
    // The entity's record pointer (+512) is never touched.
    struct Saved { uint32_t ent = 0, rec = 0, visAt = 0, vis = 0, poseAt = 0, poseSize = 0, rigAt = 0, rigBuf = 0; uint16_t rigModel = 0; uint8_t rigBones = 0; bool rigDead = false; uint8_t rig97 = 0; uint16_t rig104 = 0; uint32_t rig192 = 0; bool listPut = false; uint8_t list[RigListBytes]; uint8_t listWritten[RigListBytes]; uint32_t bufWritten = 0; uint16_t modelWritten = 0; uint8_t bonesWritten = 0; uint8_t e[EntSize]; uint8_t ps[RecSize]; uint8_t pose[PoseMax]; };
    static uint32_t g_replayBuf[32];   // 0.5.347: a guest pose buffer per player for the replay
    static uint32_t g_poseNoRecord = 0, g_rigsRevived = 0;
    // 0.5.333: cgame draws only the entities in this machine's last snapshot
    // (CG_AddPacketEntities 8246BED8: entity+532 -> +256 equals the counter
    // at 0x82C28274). Your own player is never in your own snapshot, so 332
    // never drew you (Justin: "not seeing me or the moment they shoot me").
    // During the replay every recorded player is marked in it (and so drawn
    // wherever he was, not only where you can see from your body now).
    constexpr uint32_t SnapshotCounter = 0x82C28274u;   // 0.5.338: 0x82C30000 - 32304 + 164 (333-337 used 0x82C281D4: a sum done wrong; every replayed player then failed the test and none was drawn)
    static Saved g_saved[32];
    static bool g_swapped = false;
    static uint32_t g_listAt = 0, g_ownEnt = 0, g_killerEnt = 0;   // 0.5.332: the entity list's two places swapped
    bool OthersOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_OTHERS"); return !(t && t[0] == '0'); }(); return on; }
    bool PoseOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_POSES"); return !(t && t[0] == '0'); }(); return on; }   // 0.5.341
    // 0.5.342: Justin, 341: "I was laying on the floor" in the killcam. This
    // machine's own body is drawn in first person, so the game never moves
    // its animation tree on: its pose stays the last one it was given (the
    // fall of the last death). 341 recorded that pose every sample and put
    // it on the replayed you: lying while you ran. Your recorded pose is no
    // longer put; during the replay you are drawn as any other player, so
    // cgame animates you from your recorded movement. COD3_KILLCAM_OWN_POSE=1
    // puts it again (341's way).
    bool OwnPoseOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_OWN_POSE"); return t && t[0] == '1'; }(); return on; }
    static uint32_t g_ownPoseSkipped = 0;
    // 0.5.332 (Justin, 331: "like watching a ghost move around": no gun, no
    // muzzle flash, he could not be seen being killed). FOUND (static, the
    // generated code): on the host cgame draws straight from the server's
    // entities and records:
    //  - the gun in view (CG_AddViewWeapon 8246A600, from CG_DrawActiveFrame
    //    8246C748) is drawn from entities[local+1]: its record (+512) and
    //    that ENTITY's model and animations (8248FA50(entity) -> 82468D58).
    //    331 put the killer's ps into OUR record but our entity stayed ours
    //    (dead): no gun, no flash.
    //  - every player body (CG_Player 82469968, its entity in r3) is left out
    //    when it is entities[local+1], cgame is in first person and the
    //    view's ps is alive: so our own past self was never drawn.
    // So for the length of cgame's frame the entity list's two places are
    // swapped: place 1 (this machine's player) holds the KILLER's entity and
    // the killer's place holds ours. cgame then is the killer: his view, his
    // gun with its animations and flashes, his body left out, and ours drawn
    // as any other player, where we stood. Everyone's entity and record are
    // their recorded ones (blended) as in 329. Put back right after.
    // 0.5.346: the list and the other objects of the sample at t
    static uint32_t g_liveList[ListMax], g_liveListN = 0;
    static bool g_objSwapped = false;
    static uint8_t g_objLive[ObjMax][ObjSize];
    static uint32_t g_objLiveAt[ObjMax], g_objLiveN = 0, g_objMarkAt[ObjMax], g_objMark[ObjMax];
    // object -> events played (cgame's +1000) in the replay. 0.5.628 (user: "i
    // dont see explosions in the bots killcams"): cgame's 8246C208 plays an
    // event object (type, +0, past 18: an explosion, an impact) once, when its
    // +1000 is 0, then sets it 1; others by their sequence (+184). The replay
    // kept one +1000 an object's place for the whole replay: event objects come
    // and go in the same places, so after the first in a place every later one
    // was "played" already. An object's place now starts again (0 for an event
    // object, its sequence for others) when it was not there the frame before,
    // its type changed, or (an event object) it is somewhere else.
    // 0.5.629: 628's play (2 killcams): 0 event objects in either: CoD3's
    // explosions are not event objects of their own; a grenade that goes off
    // keeps its place, may change its type and adds the explosion to its own
    // sequence. 628 started such an object again at its sequence, the
    // explosion counted played. Now: an object there the frame before keeps
    // what it has played, whatever its type (an event object new or moved:
    // 0); one that comes into the list after the first frame starts one
    // short of its sequence (its newest event plays: a new grenade's throw,
    // an explosion of its own); on the first frame, at its sequence (nothing
    // from before the replay again). Logged at the end: the objects' events
    // the replay had and cgame played, and their types.
    // 0.5.634: 629-631's killcams: still no explosion in any object, as event
    // objects or in objects' own sequences. A live one is the server's call of
    // its weapon's event 56 (sub_82556180 from 825836A4, the projectile's
    // explosion), played once. Each such call is kept (time, place, angles,
    // weapon) and played again in the replay (PlayBlasts, MpFixesKillcamFx).
    // COD3_KILLCAM_BLASTS=0 off.
    struct Blast { uint64_t t; float at[3], angles[3]; char weapon[40]; uint32_t event; };
    constexpr int Blasts = 64;
    static Blast g_blast[Blasts];
    static int g_blastHead = 0, g_blastCount = 0;
    static uint64_t g_blastDone = 0;   // the replay's moment played up to
    static uint32_t g_blastsPlayed = 0;
    bool BlastOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_BLASTS"); return !(t && t[0] == '0'); }(); return on; }
    struct ObjMemo { uint32_t done = 0, seen = 0, sig = 0; uint8_t type = 0; };
    static uint32_t g_objSeqEvents = 0, g_objSeqPlayed = 0;
    static uint32_t g_objTypes[64];
    static std::map<uint32_t, ObjMemo> g_objDone;
    static uint32_t g_objEvents = 0, g_objEventsShown = 0;
    static uint32_t g_objPut = 0, g_objFrames = 0, g_objRigChanged = 0;
    int SampleAt(uint64_t t)
    {
        for (int k = 0; k < g_count; k++)
        {
            const int i = (g_head - 1 - k + Samples) % Samples;
            if (g_ring[i].t <= t) return i;
        }
        return g_count ? (g_head - g_count + Samples) % Samples : -1;
    }
    void SwapInObjects(uint8_t* base, uint64_t t)
    {
        g_objSwapped = false;
        if (!ObjOn()) return;
        const int k = SampleAt(t);
        if (k < 0 || !g_listN[k]) return;
        g_liveListN = Guest::Read32(base, ListCountAt);
        if (g_liveListN > uint32_t(ListMax)) g_liveListN = ListMax;
        for (uint32_t i = 0; i < g_liveListN; i++) g_liveList[i] = Guest::Read32(base, ListAt + 4 * i);
        for (uint32_t i = 0; i < g_listN[k]; i++) Guest::Write32(base, ListAt + 4 * i, g_list[k][i]);
        Guest::Write32(base, ListCountAt, g_listN[k]);
        const uint32_t counter = Guest::Read32(base, SnapshotCounter);
        g_objLiveN = 0;
        for (uint32_t j = 0; j < g_objN[k]; j++)
        {
            const uint32_t e = g_objAt[k][j];
            const uint8_t* rec = g_obj[k][j];
            memcpy(g_objLive[g_objLiveN], base + e, ObjSize);
            g_objLiveAt[g_objLiveN] = e;
            // cgame's own fields stay cgame's: +980..+1003 (written by it, 345).
            // 0.5.348: and the object's links stay the live ones: +484..+547
            // (its model instance +492, cgame entity +532 and the rest of the
            // pointers 345 saw read there). 347 froze 2.5 s into a killcam in
            // the scene's model-instance list walk (8243E2B8 <- 82446170 <-
            // 824483C0 <- 82448A18): an object of then carried a model
            // instance that had been freed and linked again since, and cgame
            // put it in its list a second time: a list that never ends.
            // 0.5.351: only the entity's STATE, +0..+483 (type, flags, its
            // movement and place, angles, events: what a CoD snapshot carries
            // and a CoD4 killcam replays). The rest (+484..) is the server's
            // own bookkeeping: links, handles, timers. 350 crashed after a
            // killcam in a worker thread (sub_824472C0 -> sub_820FC1F0, read
            // at 0x28538ADE): an effects job that runs beside cgame reads
            // those fields; values of then there point at nothing now.
            memcpy(base + e, rec, 484);
            // counted: an object whose model instance is another now (freed, reused)
            {
                const uint32_t thenRig = (uint32_t(rec[492]) << 24) | (uint32_t(rec[493]) << 16) | (uint32_t(rec[494]) << 8) | rec[495];
                if (thenRig != Guest::Read32(base, e + 492)) g_objRigChanged++;
            }
            // events: what the replay has played (from the first frame's sequence)
            {
                const uint8_t type = rec[0];
                const bool event = type > 18;
                uint32_t sig = type;
                if (event && g_originFound && !g_originAt.empty() && g_originAt.front() + 12 <= ObjSize)
                    for (uint32_t j = 0; j < 12; j++) sig = (sig * 16777619u) ^ rec[g_originAt.front() + j];
                auto it = g_objDone.find(e);
                const bool there = it != g_objDone.end() && it->second.seen == g_objFrames;
                const uint32_t seq = Guest::Read32(base, e + 184);
                if (it == g_objDone.end()) it = g_objDone.emplace(e, ObjMemo{}).first;
                ObjMemo& m = it->second;
                if (event && (!there || m.sig != sig || m.type != type)) { m.done = 0; g_objEvents++; }
                else if (!there) m.done = g_objFrames == 0 ? seq : (seq ? seq - 1 : 0);
                if (!event && m.done > seq) m.done = seq;   // the place's object is another: none of the past
                if (!there) g_objTypes[type & 63]++;
                m.seen = g_objFrames + 1; m.sig = sig; m.type = type;
                Guest::Write32(base, e + 1000, m.done);
            }
            // in this frame's snapshot (its cgame entity +532 -> +256)
            const uint32_t sv = Guest::Read32(base, e + 532);
            g_objMarkAt[g_objLiveN] = 0;
            if (sv >= 0x40000000u && sv < 0xA0000000u)
            {
                g_objMarkAt[g_objLiveN] = sv + 256; g_objMark[g_objLiveN] = Guest::Read32(base, sv + 256);
                Guest::Write32(base, sv + 256, counter);
            }
            g_objLiveN++;
        }
        g_objPut += g_objLiveN; g_objFrames++;
        g_objSwapped = true;
        static int told = 0;
        if (told < 3) { told++; printf("killcam: objects of then: sample %d ms before now, %u listed (live %u), %u objects put\n", int(Ms() - g_ring[k].t), g_listN[k], g_liveListN, g_objLiveN); fflush(stdout); }
    }
    void SwapOutObjects(uint8_t* base)
    {
        if (!g_objSwapped) return;
        g_objSwapped = false;
        for (int j = int(g_objLiveN) - 1; j >= 0; j--)
        {
            const uint32_t e = g_objLiveAt[j];
            auto it = g_objDone.find(e);
            if (it != g_objDone.end())
            {
                const uint32_t played = Guest::Read32(base, e + 1000);   // what cgame played
                const uint32_t seq = Guest::Read32(base, e + 184);
                if (Guest::Read8(base, e) > 18) { if (it->second.done == 0 && played != 0) g_objEventsShown++; }
                else
                {
                    if (seq > it->second.done) g_objSeqEvents += seq - it->second.done;
                    if (played > it->second.done && played <= seq) g_objSeqPlayed += played - it->second.done;
                }
                it->second.done = played;
            }
            if (g_objMarkAt[j]) Guest::Write32(base, g_objMarkAt[j], g_objMark[j]);
            memcpy(base + e, g_objLive[j], ObjSize);
        }
        for (uint32_t i = 0; i < g_liveListN; i++) Guest::Write32(base, ListAt + 4 * i, g_liveList[i]);
        Guest::Write32(base, ListCountAt, g_liveListN);
    }
    // 0.5.357: the killer's gun in view. 356's video (21.58.19): the
    // killer's hands and gun show only in the last half second (his melee);
    // 356's log has his ps right (weapon 42, pm_type 0, +240 0x18), so the
    // ps gates of CG_AddViewWeapon (8246A600) pass. Two gates left that are
    // not his ps but this machine's cgame (local client lc): cg+0xC3C
    // (0x82C7D490 + 0x18E0*lc, nonzero: no gun, 8246A660) and cg+0x1794
    // (nonzero: no gun, 82468DA8), and the gun's model at 0x82C7A270 +
    // 0xC88*lc +0xC8 (0: no gun, 82468E10). This machine's player is dead,
    // so its cgame state says "no gun"; the killer was alive. For the frame
    // the two cg gates are 0 (put back after); all of them are logged.
    // COD3_KILLCAM_GUNGATE=0: left alone (only logged).
    bool GunGateOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_GUNGATE"); return !(t && t[0] == '0'); }(); return on; }
    static uint32_t g_gateAt[2] = {}, g_gateWas[2] = {};
    static int g_gateN = 0;
    static uint32_t g_gunGateCleared = 0;
    bool GunOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_GUNS"); return !(t && t[0] == '0'); }(); return on; }   // 0.5.358
    void GunGates(uint8_t* base, uint32_t entities)
    {
        g_gateN = 0;
        const uint32_t lc = Guest::Read32(base, 0x82BCBE9Cu);
        if (lc > 3) return;
        const uint32_t cg = 0x82C7D490u + 0x18E0u * lc, wv = 0x82C7A270u + 0xC88u * lc;
        const uint32_t ent = Guest::Read32(base, entities + 4 * (lc + 1));
        const uint32_t ps = ent ? Guest::Read32(base, ent + 512) : 0;
        const uint32_t gv = Guest::Read32(base, 0x82C37070u + 4 * lc);
        const uint32_t glob = Guest::Read32(base, 0x82BCBECCu);
        static uint64_t toldAt = 0; static int toldN = 0;
        const uint64_t ms = Ms();
        if (toldN < 40 && ms - toldAt >= 250)
        {
            toldAt = ms; toldN++;
            printf("killcam: gun gates (client %u): ps %08X +2C %08X pm_type %d +F0 %08X weapon %u | cg+C3C %08X cg+1794 %08X | gun model +C8 %08X +D0 %08X | gun view %08X +4 %08X | %08X+14 %08X\n",
                   lc, ps, ps ? Guest::Read32(base, ps + 0x2C) : 0, ps ? int32_t(Guest::Read32(base, ps + 0x24)) : 0, ps ? Guest::Read32(base, ps + 0xF0) : 0, ps ? Guest::Read32(base, ps + 0xA4) : 0,
                   Guest::Read32(base, cg + 0xC3C), Guest::Read32(base, cg + 0x1794), Guest::Read32(base, wv + 0xC8), Guest::Read32(base, wv + 0xD0),
                   gv, Guest::Read32(base, gv + 4), glob, glob ? Guest::Read32(base, glob + 0x14) : 0);
            fflush(stdout);
        }
        if (!GunGateOn()) return;
        for (const uint32_t at : { cg + 0xC3Cu, cg + 0x1794u })
        {
            const uint32_t v = Guest::Read32(base, at);
            if (!v) continue;
            g_gateAt[g_gateN] = at; g_gateWas[g_gateN] = v; g_gateN++;
            Guest::Write32(base, at, 0);
            g_gunGateCleared++;
        }
    }
    void SwapIn(uint8_t* base)
    {
        g_swapped = false; g_listAt = 0;
        if (!Replaying()) return;
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!entities) return;
        const uint64_t t = g_deathAt - PreMs() + (Ms() - g_playStart);
        static uint8_t ent[EntSize], theirs[RecSize];
        const bool others = OthersOn() && g_originFound;
        bool killerDone = false;
        for (int c = 0; c < 32; c++)
        {
            Saved& sv = g_saved[c];
            sv.ent = sv.rec = 0; sv.visAt = 0;
            const uint32_t e = Guest::Read32(base, entities + 4 * uint32_t(c + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            if (r < 0x40000000u || r >= 0xA0000000u) continue;
            if (!others && c != g_killer) continue;
            uint32_t at = 0;
            if (!BlendPs(c, t, theirs, c == g_killer && EyeBlendOn())) continue;
            const bool entOk = g_originFound && BlendEnt(c, t, ent, at) && at == e;
            memcpy(sv.ps, base + r, RecSize); sv.rec = r;
            memcpy(base + r, theirs, RecSize);
            sv.poseAt = 0;
            if (entOk)
            {
                memcpy(sv.e, base + e, EntSize); sv.ent = e;
                // 0.5.353: the entity's state of then, but its links (+484..+547:
                // model instance +492, record +512, team +520, cgame entity +532,
                // ...) stay the live ones. 352's guard: the looping model
                // instance (8299B810) belonged to no current player: a bot who
                // respawned since has a new model instance, and putting back his
                // old one (now another body's) drew that body twice.
                memcpy(base + e, ent, 492);
                memcpy(base + e + 496, ent + 496, 512 - 496);
                memcpy(base + e + 516, ent + 516, EntSize - 516);
                // 0.5.354: 353 kept all of +484..+547 live and you vanished
                // from the replay (your dead entity's fields there); now only
                // the model instance +492 and the record +512 stay live.
                // 0.5.341: the events played so far in the replay (not the recorded +1000)
                if (!g_evInit[c]) { g_evInit[c] = true; g_evDone[c] = Guest::Read32(base, e + 184); }
                Guest::Write32(base, e + 1000, g_evDone[c]);
                // 0.5.341: his recorded pose in his (live) rig's buffer
                const uint32_t rig = (uint32_t(sv.e[492]) << 24) | (uint32_t(sv.e[493]) << 16) | (uint32_t(sv.e[494]) << 8) | sv.e[495];
                const uint32_t buf = (rig >= 0x40000000u && rig < 0xA0000000u) ? Guest::Read32(base, rig + 112) : 0;
                const uint32_t size = buf ? 48 + 64 * uint32_t(Guest::Read8(base, rig + 171)) : 0;
                (void)buf; (void)size;
                // 0.5.347: 346's log: "poses put 3376, bodies without one 9706":
                // a body's pose was put only when its rig had the same model and
                // bone count as then (dead bodies have 92 bones, living 96; a bot
                // who respawned has a new rig state). Now the rig is pointed, for
                // cgame's scopes, at a buffer holding the recorded pose, with the
                // recorded model and bone count (rig +112, +168, +171: what 345
                // measured cgame and the drawing read), put back after.
                int pa = -1, pb = -1; float pf = 0.0f;
                if (PoseOn() && rig >= 0x40000000u && rig < 0xA0000000u && Around(c, t, pa, pb, pf) && g_poseSize[pa][c] > 48)
                {
                    uint32_t& rb = g_replayBuf[c];
                    if (!rb) rb = Guest::AllocatePhysical(PoseMax);
                    if (rb)
                    {
                        memcpy(base + rb, g_pose[pa][c], g_poseSize[pa][c]);
                        sv.rigAt = rig; sv.rigBuf = Guest::Read32(base, rig + 112);
                        sv.rigModel = Guest::Read16(base, rig + 168); sv.rigBones = Guest::Read8(base, rig + 171);
                        Guest::Write32(base, rig + 112, rb);
                        Guest::Write16(base, rig + 168, g_poseModel[pa][c]);
                        Guest::Write8(base, rig + 171, uint8_t((g_poseSize[pa][c] - 48) / 64));
                        sv.bufWritten = rb; sv.modelWritten = g_poseModel[pa][c]; sv.bonesWritten = uint8_t((g_poseSize[pa][c] - 48) / 64);   // 0.5.366
                        // 0.5.358: and its model list of then (its gun in hand), when
                        // that list's bones add up to the recorded pose's
                        sv.listPut = false;
                        if (GunOn() && g_rigListOk[pa][c])
                        {
                            const uint8_t* L = g_rigList[pa][c];
                            const uint32_t n = L[52], bones = (g_poseSize[pa][c] - 48) / 64;
                            bool ok = n >= 1 && n <= 8;
                            for (uint32_t i = 0; ok && i < n; i++)
                            {
                                const uint32_t m = (uint32_t(L[16 + 4 + 4 * i]) << 24) | (uint32_t(L[16 + 5 + 4 * i]) << 16) | (uint32_t(L[16 + 6 + 4 * i]) << 8) | L[16 + 7 + 4 * i];
                                if (!KillcamModelOk(base, m)) ok = false;   // 0.5.365: as the tag lookup reads it
                                if (L[8 + i] >= bones) ok = false;   // each model's first bone inside the pose
                                // 0.5.365: the body (model 0) must be the one the rig has now
                                if (i == 0 && m != Guest::Read32(base, rig + 0x80)) ok = false;
                            }
                            if (ok)
                            {
                                ReadRigList(base, rig, sv.list);
                                WriteRigList(base, rig, L);
                                memcpy(sv.listWritten, L, RigListBytes);   // 0.5.366
                                sv.listPut = true;
                                if (n > sv.list[52]) g_gunsPut++;
                            }
                        }
                        // 0.5.353: a body that is dead NOW has its model instance in
                        // its fallen mode (352's dump of yours, alive vs dead: +97
                        // 0 -> 1, +104 0 -> 0x005C, +192 0 -> 0x0F, +132 a pointer);
                        // in the replay it was alive: those three back to their
                        // living values for the frame (Justin: "I don't see my
                        // weapon"; a fallen body is drawn without one).
                        // 0.5.355: OFF unless COD3_KILLCAM_REVIVE=1: since this came in
                        // (353) you were drawn by cgame every frame (log) yet not seen
                        // in the killcam (Justin, 353 and 354).
                        static const bool reviveOn = []() { const char* v = getenv("COD3_KILLCAM_REVIVE"); return v && v[0] == '1'; }();
                        sv.rigDead = reviveOn && Guest::Read8(base, rig + 97) != 0;
                        if (sv.rigDead)
                        {
                            sv.rig97 = Guest::Read8(base, rig + 97); sv.rig104 = Guest::Read16(base, rig + 104); sv.rig192 = Guest::Read32(base, rig + 192);
                            Guest::Write8(base, rig + 97, 0); Guest::Write16(base, rig + 104, 0); Guest::Write32(base, rig + 192, 0);
                            g_rigsRevived++;
                        }
                        g_posesPut++;
                    }
                    else g_posesMissed++;
                }
                else { g_posesMissed++; g_poseNoRecord++; }
            }
            if (c == g_killer) killerDone = true;
            // 0.5.333: in this frame's snapshot, so he is drawn
            const uint32_t sv2 = Guest::Read32(base, e + 532);
            if (entOk && sv2 >= 0x40000000u && sv2 < 0xA0000000u)
            {
                sv.visAt = sv2 + 256; sv.vis = Guest::Read32(base, sv.visAt);
                Guest::Write32(base, sv.visAt, Guest::Read32(base, SnapshotCounter));
            }
        }
        SwapInObjects(base, t);   // 0.5.346
        g_swapped = true;
        // 0.5.334: 333 still did not show you (videos 00.14.31 and 00.15.09):
        // CG_Player (82469968) leaves out an entity whose +16 has bit 0x80;
        // your own entity may carry it as this machine's player. Cleared on
        // the replayed you while you were alive. And the log says where you
        // were from the killer's eyes (distance, angle off his aim).
        {
            const uint32_t me = Guest::Read32(base, entities + 4);
            if (me && g_saved[0].ent == me)
            {
                const uint32_t flags = Guest::Read32(base, me + 16);
                const bool alive = int32_t(Guest::Read32(base, me + 776)) > 0;
                if ((flags & 0x80u) && alive) Guest::Write32(base, me + 16, flags & ~0x80u);
                const uint32_t kr = Guest::Read32(base, Guest::Read32(base, entities + 4 * uint32_t(g_killer + 1)) + 512);
                static uint64_t toldAt = 0;
                const uint64_t ms = Ms();
                if (kr && ms - toldAt >= 500)
                {
                    toldAt = ms;
                    const float dx = ReadFloat(base, me + 336) - ReadFloat(base, kr), dy = ReadFloat(base, me + 340) - ReadFloat(base, kr + 4);
                    const float bearing = atan2f(dy, dx) * 57.29578f, yaw = ReadFloat(base, kr + 208);
                    printf("killcam: %llu ms before your death: you at %.0f %.0f %.0f (type %u, flags %08X%s, health %d, +224 %u +228 %08X), %.0f from him, %.0f deg off his aim (yaw %.0f pitch %.0f)\n",
                           (unsigned long long)(g_deathAt > t ? g_deathAt - t : 0), ReadFloat(base, me + 336), ReadFloat(base, me + 340), ReadFloat(base, me + 344),
                           Guest::Read8(base, me), flags, (flags & 0x80u) && alive ? ", 0x80 cleared" : "", int32_t(Guest::Read32(base, me + 776)),
                           Guest::Read32(base, me + 224), Guest::Read32(base, me + 228), sqrtf(dx * dx + dy * dy), fmodf(bearing - yaw + 540.0f, 360.0f) - 180.0f, yaw, ReadFloat(base, kr + 204));
                    fflush(stdout);
                }
            }
        }
        // the two places in the entity list
        const uint32_t own = Guest::Read32(base, entities + 4), killer = Guest::Read32(base, entities + 4 * uint32_t(g_killer + 1));
        if (killerDone && own && killer && ListSwapOn())
        {
            g_listAt = entities; g_ownEnt = own; g_killerEnt = killer;
            Guest::Write32(base, entities + 4, killer);
            Guest::Write32(base, entities + 4 * uint32_t(g_killer + 1), own);
        }
        GunGates(base, entities);   // 0.5.357
        static int told = 0;
        if ((told < 3 && told++ < 3))
        {
            const uint32_t r = Guest::Read32(base, killer + 512);
            printf("killcam: frame drawn as client %d (origin %.1f %.1f %.1f, weapon %u, +240 %08X, pm_type %d), list %s, others %s\n", g_killer, ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8),
                   Guest::Read32(base, r + 164), Guest::Read32(base, r + 240), int32_t(Guest::Read32(base, r + 36)), g_listAt ? "swapped" : "not swapped", others ? "where they were" : "as now");
            fflush(stdout);
        }
    }
    // 0.5.344: your recorded pose, put right before cgame draws you (CG_Player
    // hook). 343's log: while you play, the recorder's own evaluation gives a
    // real, moving pose each sample (bone 0 changes; a standing hip 34 up).
    // During the replay something after SwapIn writes your dead body's pose
    // into the buffer every frame (bone 0 "before": no translation, the
    // fallen body's layout): that is what overwrote 341's put. 343's own
    // evaluation there stood you up but froze (the tree does not move on).
    // So the recorded, moving pose goes in at the last moment instead.
    static uint32_t g_ownPut = 0, g_ownNoRecord = 0;
    bool PutOwnPose(uint8_t* base, uint32_t ent)
    {
        if (!Replaying() || !PoseOn()) return false;
        const uint32_t rig = Guest::Read32(base, ent + 492);
        if (rig < 0x40000000u || rig >= 0xA0000000u || Guest::Read32(base, rig + 172) != ent) return false;
        const uint32_t buf = Guest::Read32(base, rig + 112);
        const uint32_t size = 48 + 64 * uint32_t(Guest::Read8(base, rig + 171));
        // 0.5.349: the replay's own buffers (0.5.347) are physical memory,
        // above 0xA0000000: 348's log "your recorded pose put 0 times": this
        // test turned them away and the evaluation below froze you instead.
        if (buf < 0x40000000u || size <= 48 || size > PoseMax) return false;
        const uint64_t t = g_deathAt - PreMs() + (Ms() - g_playStart);
        static uint8_t pose[PoseMax];
        uint32_t n = size;
        if (!BlendPose(0, t, Guest::Read16(base, rig + 168), size, pose))
        {
            // 344: your body has fewer bones once dead (343's log: 96 alive,
            // 92 in the replay): the nearest recorded pose, its first bones
            int a, b; float f;
            if (!Around(0, t, a, b, f)) { g_ownNoRecord++; return false; }
            const int k = f < 0.5f ? a : b;
            const uint32_t have = g_poseSize[k][0] ? g_poseSize[k][0] : g_poseSize[k == a ? b : a][0];
            const int use = g_poseSize[k][0] ? k : (k == a ? b : a);
            if (!have) { g_ownNoRecord++; return false; }
            n = std::min(size, have);
            memcpy(pose, g_pose[use][0], n);
            static int told = 0;
            if (told < 3) { told++; printf("killcam: your recorded pose has %u bones, your body now %u: the first %u put\n", (have - 48) / 64, (size - 48) / 64, (n - 48) / 64); fflush(stdout); }
        }
        Saved& sv = g_saved[0];
        if (!sv.poseAt) { memcpy(sv.pose, base + buf, size); sv.poseAt = buf; sv.poseSize = size; }
        memcpy(base + buf, pose, n);
        g_ownPut++;
        return true;
    }
    void SwapOut(uint8_t* base)
    {
        for (int i = g_gateN - 1; i >= 0; i--) Guest::Write32(base, g_gateAt[i], g_gateWas[i]);   // 0.5.357
        g_gateN = 0;
        if (g_listAt)
        {
            Guest::Write32(base, g_listAt + 4, g_ownEnt);
            Guest::Write32(base, g_listAt + 4 * uint32_t(g_killer + 1), g_killerEnt);
            g_listAt = 0;
        }
        if (!g_swapped) return;
        SwapOutObjects(base);   // 0.5.346 (objects were put last)
        for (int c = 31; c >= 0; c--)
        {
            Saved& sv = g_saved[c];
            if (sv.visAt) { Guest::Write32(base, sv.visAt, sv.vis); sv.visAt = 0; }
            if (sv.poseAt) { memcpy(base + sv.poseAt, sv.pose, sv.poseSize); sv.poseAt = 0; }   // 0.5.341
            if (sv.rigAt)   // 0.5.347: the rig's own buffer, model and bones back
            {
                // 0.5.366 (Justin 00:36, 0.5.365's clip 00.33.12: a soldier in the
                // T pose (arms out, no gun) next to your body right after the
                // killcam, and "sometimes bots in the T position" while
                // playing). If the game rebuilt a body's rig during the
                // replayed frame (a replayed weapon change makes it lay the
                // body's models out again: sub_82447898 writes the model list,
                // counts and a new pose buffer), putting back what was there
                // before the frame left that body with a model list, bone count
                // and pose buffer that don't belong together: the T pose (the
                // pose never worked out). Now each part is put back only if it
                // still holds what the replay wrote; what the game wrote stays.
                bool rebuilt = false;
                if (sv.listPut)
                {
                    uint8_t now[RigListBytes]; ReadRigList(base, sv.rigAt, now);
                    if (memcmp(now, sv.listWritten, RigListBytes) == 0) WriteRigList(base, sv.rigAt, sv.list); else rebuilt = true;
                    sv.listPut = false;
                }
                if (sv.rigDead) { Guest::Write8(base, sv.rigAt + 97, sv.rig97); Guest::Write16(base, sv.rigAt + 104, sv.rig104); Guest::Write32(base, sv.rigAt + 192, sv.rig192); sv.rigDead = false; }   // 0.5.353
                if (Guest::Read32(base, sv.rigAt + 112) == sv.bufWritten) Guest::Write32(base, sv.rigAt + 112, sv.rigBuf); else rebuilt = true;
                if (Guest::Read16(base, sv.rigAt + 168) == sv.modelWritten) Guest::Write16(base, sv.rigAt + 168, sv.rigModel); else rebuilt = true;
                if (Guest::Read8(base, sv.rigAt + 171) == sv.bonesWritten) Guest::Write8(base, sv.rigAt + 171, sv.rigBones); else rebuilt = true;
                if (rebuilt)
                {
                    g_rigsRebuilt++;
                    static int told = 0;
                    if (told < 10) { told++; printf("killcam: client %d's rig %08X was rebuilt by the game during the replayed frame: its new layout kept (buffer %08X, %u models, %u bones)\n", c, sv.rigAt,
                        Guest::Read32(base, sv.rigAt + 112), Guest::Read8(base, sv.rigAt + 0xAA), Guest::Read8(base, sv.rigAt + 171)); fflush(stdout); }
                }
                sv.rigAt = 0;
            }
            if (sv.ent) g_evDone[c] = Guest::Read32(base, sv.ent + 1000);                      // 0.5.341: what cgame played
            if (sv.ent) memcpy(base + sv.ent, sv.e, EntSize);
            if (sv.rec) memcpy(base + sv.rec, sv.ps, RecSize);
            sv.ent = sv.rec = 0;
        }
        g_swapped = false;
    }
    // 0.5.329: the recorder, after each server frame (sub_82535810)
    // 0.5.367 (Justin 00:40: "occasionally some soldiers are in the T
    // position or lying on the ground dead-looking, but their bodies are
    // obviously still alive"). A watch on every living player's body, every
    // 2 s outside the killcam: logged once a minute per player when
    //  - his body is in its dead mode (rig +0xC0 nonzero; 0x0F once dead),
    //  - his pose buffer (+112) is one of the killcam's own buffers,
    //  - bones (+0xAB) don't match his models (92 + 4 with a gun),
    //  - his pose hasn't changed in 6 s while he moved (not worked out: the
    //    T pose).
    // With what the killcam did last (time since it ended) and the rig.
    static uint64_t g_lastReplayEnd = 0;
    // 0.5.368: when cgame last drew each player's body (CG_Player), so the
    // frozen-pose test only looks at bodies on screen (a body nobody draws
    // isn't posed: 367's log flagged those)
    static std::atomic<uint64_t> g_drawnAt[32];
    void NoteDrawn(uint8_t* base, uint32_t entity)
    {
        if (entity < 0x40000000u || entity >= 0xC0000000u) return;
        const uint32_t rec = Guest::Read32(base, entity + 512);
        if (rec < 0x40000000u || rec >= 0xC0000000u) return;
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (int c = 0; entities && c < 32; c++)
            if (Guest::Read32(base, entities + 4 * uint32_t(c + 1)) == entity) { g_drawnAt[c].store(Ms()); return; }
    }
    void BodyWatch(uint8_t* base)
    {
        static uint64_t lastAt = 0; static uint64_t toldAt[32] = {}; static uint32_t sum[32] = {}; static float at[32][3] = {}; static uint64_t sameSince[32] = {};
        const uint64_t ms = Ms();
        if (ms - lastAt < 2000) return;
        lastAt = ms;
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!entities) return;
        for (int c = 0; c < 32; c++)
        {
            const uint32_t e = Guest::Read32(base, entities + 4 * uint32_t(c + 1));
            if (e < 0x40000000u || e >= 0xC0000000u || Guest::Read8(base, e) != 1) continue;
            const int32_t health = int32_t(Guest::Read32(base, e + 776));
            const uint32_t rig = Guest::Read32(base, e + 492);
            if (health <= 0 || rig < 0x40000000u || rig >= 0xC0000000u || Guest::Read32(base, rig + 172) != e) { sameSince[c] = 0; continue; }
            const uint32_t buf = Guest::Read32(base, rig + 112), dead = 0;   // 0.5.368: +0xC0 is 0x0F on every living body (367's log): not a dead mode
            const uint32_t models = Guest::Read8(base, rig + 0xAA), bones = Guest::Read8(base, rig + 0xAB);
            bool ours = false; for (int k = 0; k < 32; k++) if (g_replayBuf[k] && buf == g_replayBuf[k]) ours = true;
            const bool bonesOff = models == 1 ? bones != 92 : (models == 0 || bones < 92 + 1);   // 0.5.368: guns differ (93, 94, 96, 102 seen in 367); only impossible counts
            uint32_t h = 0;
            if (buf >= 0x40000000u && buf < 0xC0000000u && bones && bones <= 160) for (uint32_t o = 48; o < 48 + 64u * bones; o += 4) h = h * 31 + Guest::Read32(base, buf + o);
            const float x = ReadFloat(base, e + 336), y = ReadFloat(base, e + 340), z = ReadFloat(base, e + 344);
            const float moved = fabsf(x - at[c][0]) + fabsf(y - at[c][1]) + fabsf(z - at[c][2]);
            at[c][0] = x; at[c][1] = y; at[c][2] = z;
            if (h != sum[c] || moved < 4.0f) { sum[c] = h; sameSince[c] = 0; }
            else if (!sameSince[c]) sameSince[c] = ms;
            const bool drawn = ms - g_drawnAt[c].load() < 1000, riding = Guest::Read32(base, e + 900) != 0;   // 0.5.368
            if (!drawn || riding) sameSince[c] = 0;
            const bool frozen = sameSince[c] && ms - sameSince[c] >= 6000;
            if (!(dead || ours || bonesOff || frozen) || ms - toldAt[c] < 60000) continue;
            toldAt[c] = ms;
            printf("bodies: client %d alive (health %d) but %s%s%s%s; rig %08X buffer %08X, %u models, %u bones, +C0 %08X; killcam ended %llu s ago\n", c, health,
                   dead ? "in its dead mode " : "", ours ? "on a killcam pose buffer " : "", bonesOff ? "bones don't match its models " : "", frozen ? "pose frozen while moving (T pose?) " : "",
                   rig, buf, models, bones, dead, (unsigned long long)(g_lastReplayEnd ? (ms - g_lastReplayEnd) / 1000 : 0));
            printf("bodies:   rig:"); for (uint32_t o = 0; o < 0xC8; o += 4) printf("%s%08X", (o % 32) ? " " : "\n    ", Guest::Read32(base, rig + o)); printf("\n");
            fflush(stdout);
        }
    }
    // 0.5.634: the explosions of then (MpFixesKillcamFx), each at its moment
    // of the replay, oldest first
    void PlayBlasts(PPCContext& ctx, uint8_t* base)
    {
        if (!BlastOn() || !ReplayOn()) return;
        const uint64_t t = g_deathAt - PreMs() + (Ms() - g_playStart);
        if (t <= g_blastDone) return;
        for (int k = g_blastCount; k > 0; k--)
        {
            const Blast& b = g_blast[(g_blastHead - k + Blasts) % Blasts];
            if (b.t <= g_blastDone || b.t > t) continue;
            MpFixesKillcamFx(ctx, base, b.at, b.angles, b.weapon, b.event);
            g_blastsPlayed++;
            static int told = 0;
            if (told < 30) { told++; printf("killcam: explosion of then played: %s event %u at %.0f %.0f %.0f, %lld ms from the death\n", b.weapon, b.event, b.at[0], b.at[1], b.at[2], (long long)b.t - (long long)g_deathAt); fflush(stdout); }
        }
        g_blastDone = t;
    }
    void ServerFrame(PPCContext& ctx, uint8_t* base)
    {
        if (On() && !g_playing) BodyWatch(base);   // 0.5.367
        if (On() && g_playing) PlayBlasts(ctx, base);   // 0.5.634
        if (!On() || g_playing) return;
        Record(ctx, base, Ms());
        FindOrigins();
    }
    // the camera hook, after the game's own view: the recorder
    float g_putX = 0, g_putY = 0, g_putZ = 0, g_putYaw = 0;
    void Frame(uint8_t* base, uint32_t camera, uint32_t mode)
    {
        (void)mode;
        if (!On()) return;
        if (g_playing)
        {
            static uint64_t toldAt = 0;
            const uint64_t ms = Ms();
            if (ms - toldAt >= 500)
            {
                toldAt = ms;
                const uint32_t view = 0x82C7D490u + 6368u * Guest::Read32(base, camera + 480);
                printf("killcam: after the view code: view %.1f %.1f %.1f yaw %.1f (put %.1f %.1f %.1f yaw %.1f) camera %.1f %.1f %.1f\n",
                       ReadFloat(base, view + 4960), ReadFloat(base, view + 4964), ReadFloat(base, view + 4968), ReadFloat(base, view + 5028),
                       g_putX, g_putY, g_putZ, g_putYaw, ReadFloat(base, camera + 48), ReadFloat(base, camera + 52), ReadFloat(base, camera + 56));
                fflush(stdout);
            }
        }
        // 0.5.329: recorded from the server frame now (ServerFrame below)
    }
    // 0.5.323: 322 played (its log) but nothing showed: the camera object
    // (+48..) is only the input. sub_824674E0's view modes write the view
    // itself into the local view block (0x82C7D490 + 6368 * client: origin
    // +4960..+4968, angles +5024..+5032), and sub_82463150, at its end,
    // turns those angles into the view's axes (+4976..). So the killer's eye
    // goes into the view block just before sub_82463150 runs.
    void Apply(uint8_t* base, uint32_t camera, uint32_t mode)
    {
        if (!On()) return;
        const uint64_t now = Ms();
        if (g_armed && !g_playing && mode == 19)
        {
            if (now - g_deathAt < uint64_t(DelayMs())) return;   // 0.5.360: your death first
            if (now - g_deathAt > uint64_t(DelayMs()) + 4000) { g_armed = false; return; }   // the death view came too late
            g_playing = true;
            g_playStart = now;
            for (bool& b : g_evInit) b = false;   // 0.5.341
            g_objDone.clear(); g_objPut = g_objFrames = g_objRigChanged = 0; g_objEvents = g_objEventsShown = 0; g_objSeqEvents = g_objSeqPlayed = 0; memset(g_objTypes, 0, sizeof g_objTypes);   // 0.5.346
            g_blastDone = g_deathAt - PreMs(); g_blastsPlayed = 0;   // 0.5.634
            g_evPlayed = g_evFire = g_posesPut = g_posesMissed = 0; g_ownPoseSkipped = 0; g_ownPut = g_ownNoRecord = 0; g_poseNoRecord = 0; g_rigsRevived = 0; g_gunGateCleared = 0; g_gunsPut = 0; g_rigsRebuilt = 0;
            // 0.5.326: the killer's name; the HUD off while it plays (the
            // "critically wounded" text and the rest), on again after
            {
                char title[64];
                const int n = (g_killer >= 0 && g_killer < int(MaxClients)) ? g_botNumber[g_killer] : 0;
                if (n > 0) snprintf(title, sizeof title, "KILLCAM  -  Bot %d", n); else snprintf(title, sizeof title, "KILLCAM  -  Player");
                Overlay::Notice(title, PreMs() + PostMs() + 200);
            }
            // 0.5.628 (user: "the person who killed you's true vision, hud,
            // explosions"): the HUD stays on (cgame draws it from the killer's
            // ps: his ammo, his compass); COD3_KILLCAM_HUD=0: off as before
            static const bool hudOff = []() { const char* t = getenv("COD3_KILLCAM_HUD"); return t && t[0] == '0'; }();
            if (hudOff) Kernel::QueueConsoleCommand("cg_draw2D 0");
            if (ReplayOn()) { printf("killcam: replay through client %d's own playerstate\n", g_killer); fflush(stdout); PatchHeld(base); }
            printf("killcam: playing client %d's last 4 s\n", g_killer);
            fflush(stdout);
        }
        if (!g_playing) return;
        HoldEye(base);   // 0.5.628
        const uint64_t into = now - g_playStart;
        // 0.5.327: in the replay the view mode follows the killer's ps (alive),
        // so it ends on time, or when this player is alive again
        bool respawned = false;
        if (ReplayOn())
        {
            const uint32_t entities = Guest::Read32(base, EntityList);
            // 0.5.332: inside cgame's frame place 1 may hold the killer: ours is kept aside
            const uint32_t me = g_listAt ? g_ownEnt : entities ? Guest::Read32(base, entities + 4) : 0;
            // 0.5.330: 329 ended every replay after half a second ("respawned"):
            // this runs inside cgame's frame, while our entity holds its
            // recorded (alive) copy: the real health is in the saved copy then
            int32_t health = me ? int32_t(Guest::Read32(base, me + 776)) : 0;
            if (g_swapped && g_saved[0].ent == me && me)
            {
                const uint8_t* h = g_saved[0].e + 776;
                health = int32_t((uint32_t(h[0]) << 24) | (uint32_t(h[1]) << 16) | (uint32_t(h[2]) << 8) | h[3]);
            }
            respawned = into > 1000 && me && health > 0;
            static uint32_t lastMode = 0xFFFFFFFFu;
            if (mode != lastMode) { lastMode = mode; printf("killcam: replay %llu ms in: view mode %u\n", (unsigned long long)into, mode); fflush(stdout); }
        }
        if ((ReplayOn() ? respawned : mode != 19) || into > uint64_t(PreMs() + PostMs()))
        {
            g_playing = false; g_armed = false;
            Kernel::QueueConsoleCommand("cg_draw2D 1");   // 0.5.326
            g_lastReplayEnd = now;   // 0.5.367
            printf("killcam: over (%s): %u events played (%u of them shots), poses put %u times, %u bodies without one, your own pose left to cgame %u times; your recorded pose put before drawing %u times (%u with no recorded pose); objects of then put %u times over %u frames (%zu objects, model instance changed since %u times); bodies with no recorded pose %u; fallen bodies stood up %u times; gun gates opened %u times; guns put back in hands %u times; rigs rebuilt by the game mid-frame %u\n", into <= uint64_t(PreMs() + PostMs()) ? "respawned" : "played out",
                   g_evPlayed, g_evFire, g_posesPut, g_posesMissed, g_ownPoseSkipped, g_ownPut, g_ownNoRecord, g_objPut, g_objFrames, g_objDone.size(), g_objRigChanged, g_poseNoRecord, g_rigsRevived, g_gunGateCleared, g_gunsPut, g_rigsRebuilt);
            printf("killcam: event objects (explosions, impacts): %u in the replay, %u of them played by cgame (0.5.628); objects' own events: %u new, %u played (0.5.629); types:",
                   g_objEvents, g_objEventsShown, g_objSeqEvents, g_objSeqPlayed);
            for (int ty = 0; ty < 64; ty++) if (g_objTypes[ty]) printf(" %d x%u", ty, g_objTypes[ty]);
            printf("\n");
            {
                uint32_t then = 0;   // 0.5.634: explosions kept from the replay's 4 s and after
                for (int k = 0; k < g_blastCount; k++)
                {
                    const Blast& b = g_blast[(g_blastHead - 1 - k + Blasts) % Blasts];
                    if (b.t > g_deathAt - PreMs() && b.t <= g_deathAt + PostMs()) then++;
                }
                printf("killcam: explosions of then (0.5.634): %u kept in the replay's time, %u played again\n", then, g_blastsPlayed);
            }
            fflush(stdout);
            return;
        }
        if (ReplayOn()) return;   // 0.5.327: the game draws it from the killer's ps
        Eye e;
        if (!EyeAt(g_killer, g_deathAt - PreMs() + into, e)) return;
        const float p = e.pitch * 0.0174533f, y = e.yaw * 0.0174533f;
        auto put = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, camera + at, w); };
        put(48, e.x); put(52, e.y); put(56, e.z);
        put(64, e.pitch); put(68, e.yaw);
        put(80, cosf(p) * cosf(y)); put(84, cosf(p) * sinf(y)); put(88, -sinf(p));
        const uint32_t view = 0x82C7D490u + 6368u * Guest::Read32(base, camera + 480);
        auto putv = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, view + at, w); };
        putv(4960, e.x); putv(4964, e.y); putv(4968, e.z);
        putv(5024, e.pitch); putv(5028, e.yaw); putv(5032, 0.0f);
        g_putX = e.x; g_putY = e.y; g_putZ = e.z; g_putYaw = e.yaw;
        static int told = 0;
        if ((told < 3 && told++ < 3)) { printf("killcam: view put at %.1f %.1f %.1f, pitch %.1f yaw %.1f\n", e.x, e.y, e.z, e.pitch, e.yaw); fflush(stdout); }
    }
}

// 0.5.634: a weapon's explosion (event 56) as the server plays it: kept for
// the killcam (from mp_fixes.cpp's sub_82556180 wrapper)
void BotsKillcamNoteEvent(uint8_t* base, uint32_t entity, uint32_t weapon, uint32_t event)
{
    using namespace Killcam;
    if (!On() || !BlastOn()) return;
    if (entity < 0x40000000u || entity >= 0xA0000000u || weapon < 0x40000000u || weapon >= 0xA0000000u) return;
    Blast& b = g_blast[g_blastHead];
    b.t = Ms(); b.event = event;
    for (int i = 0; i < 3; i++) { b.at[i] = ReadFloat(base, entity + 336 + 4 * i); b.angles[i] = ReadFloat(base, entity + 352 + 4 * i); }
    size_t n = 0;
    for (; n + 1 < sizeof b.weapon; n++) { const char c = char(Guest::Read8(base, weapon + uint32_t(n))); if (!c) break; b.weapon[n] = c; }
    b.weapon[n] = 0;
    if (!n) return;
    g_blastHead = (g_blastHead + 1) % Blasts;
    if (g_blastCount < Blasts) g_blastCount++;
    static int told = 0;
    if (told < 40) { told++; printf("killcam: explosion kept: %s event %u at %.0f %.0f %.0f angles %.0f %.0f %.0f\n", b.weapon, event, b.at[0], b.at[1], b.at[2], b.angles[0], b.angles[1], b.angles[2]); fflush(stdout); }
}

void BotsLearnKill(uint8_t* base, uint32_t attacker, uint32_t victim)
{
    using namespace BotNavigation;
    Killcam::NoteKill(base, attacker, victim);   // 0.5.322
    if (!attacker || !victim || attacker == victim) return;
    const uint32_t a = Guest::Read32(base, attacker + 512), v = Guest::Read32(base, victim + 512);
    if (a < 0x40000000u || a >= 0xA0000000u || v < 0x40000000u || v >= 0xA0000000u) return;
    const Point from{ ReadFloat(base, a), ReadFloat(base, a + 4), ReadFloat(base, a + 8) };
    const Point to{ ReadFloat(base, v), ReadFloat(base, v + 4), ReadFloat(base, v + 8) };
    if (!Finite(from) || !Finite(to)) return;
    const float flat = std::hypot(to.x - from.x, to.y - from.y);
    if (flat < 1100.0f) return;
    const float yaw = atan2f(to.y - from.y, to.x - from.x);
    for (BotLookout& l : g_lookouts)
    {
        float d = yaw - l.yaw; while (d > 3.14159f) d -= 6.28318f; while (d < -3.14159f) d += 6.28318f;
        if (std::hypot(l.at.x - from.x, l.at.y - from.y) < 160.0f && std::fabs(l.at.z - from.z) < 80.0f && std::fabs(d) < 0.61f)
        {
            l.yaw += d * 0.3f; l.score += 1.0f; g_lookoutsChanged = true;
            static int told = 0;
            if ((told < 30 && told++ < 30)) { printf("bots: lookout at %.0f %.0f %.0f scored up to %.1f (a kill %.0f away)\n", l.at.x, l.at.y, l.at.z, l.score, flat); fflush(stdout); }
            return;
        }
    }
    if (g_lookouts.size() >= 300)
    {
        auto worst = std::min_element(g_lookouts.begin(), g_lookouts.end(), [](const BotLookout& x, const BotLookout& y) { return x.score < y.score; });
        *worst = {};
        worst->at = from; worst->yaw = yaw; worst->score = 1.0f;
    }
    else g_lookouts.push_back({ from, yaw, 1.0f, 0 });
    g_lookoutsChanged = true;
    static int told = 0;
    if ((told < 30 && told++ < 30)) { printf("bots: lookout learnt at %.0f %.0f %.0f (a kill %.0f away)\n", from.x, from.y, from.z, flat); fflush(stdout); }
}
static std::filesystem::path LookoutFile(const std::string& map);
static void SaveLookouts(const std::string& map)
{
    if (map.empty() || !g_lookoutsChanged) return;
    std::error_code ec;
    const std::filesystem::path file = LookoutFile(map);
    std::filesystem::create_directories(file.parent_path(), ec);
    if (FILE* f = fopen(file.string().c_str(), "wb"))
    {
        fprintf(f, "CoD3 bot lookouts 1 %zu\n", g_lookouts.size());
        for (const BotLookout& l : g_lookouts) fprintf(f, "%.0f %.0f %.0f %.3f %.2f\n", l.at.x, l.at.y, l.at.z, l.yaw, l.score);
        fclose(f);
        g_lookoutsChanged = false;
        printf("bots: %zu lookouts for %s saved\n", g_lookouts.size(), map.c_str()); fflush(stdout);
    }
}
static void LoadLookouts(const std::string& map)
{
    g_lookouts.clear(); g_lookoutsChanged = false;
    if (map.empty()) return;
    if (FILE* f = fopen(LookoutFile(map).string().c_str(), "rb"))
    {
        char line[160]; size_t count = 0;
        if (fgets(line, sizeof(line), f) && sscanf(line, "CoD3 bot lookouts 1 %zu", &count) == 1)
            while (g_lookouts.size() < 300 && fgets(line, sizeof(line), f))
            {
                BotLookout l;
                if (sscanf(line, "%f %f %f %f %f", &l.at.x, &l.at.y, &l.at.z, &l.yaw, &l.score) != 5) continue;
                l.score *= 0.85f;   // the last games count most
                if (l.score >= 0.5f) g_lookouts.push_back(l);
            }
        fclose(f);
        g_lookoutsChanged = true;
    }
    printf("bots: %zu lookouts for %s\n", g_lookouts.size(), map.c_str()); fflush(stdout);
}
// The best lookout for a watch near `around` (within `range`), not one another
// bot is on, not used in the last minute; -1 none.
static int32_t BestLookout(BotNavigation::Point around, float range, uint32_t slot, uint32_t tick)
{
    int32_t best = -1; float bestScore = 1.4f;
    for (size_t i = 0; i < g_lookouts.size(); i++)
    {
        const BotLookout& l = g_lookouts[i];
        if (tick - l.usedAt < 60000 && l.usedAt) continue;
        bool taken = false;
        for (uint32_t o = 0; o < ActiveSlots(); o++) if (o != slot && g_explore[o].lookout == int32_t(i)) taken = true;
        if (taken) continue;
        const float d = std::hypot(l.at.x - around.x, l.at.y - around.y);
        if (d > range) continue;
        const float score = l.score - d / 2000.0f;
        if (score > bestScore) { bestScore = score; best = int32_t(i); }
    }
    return best;
}

// One step of a bot's own errand when no enemy is near or in sight: a route
// over the learnt map to a spot of his kind, then a while there (high and low
// ground: 15-35 s on watch, low; the outskirts: a short look round and on).
// False: no errand (the map not learnt yet, a hunter, or an enemy close).
static bool ExploreStep(uint8_t* base, uint32_t self, uint32_t slot, uint32_t now, const float me[3], float myEye, bool found, bool visible,
                        float enemyDistance, const float enemyAt[3], float goalOut[3], float lookOut[3], bool& holding)
{
    using namespace BotNavigation;
    using S = MapKnowledge::Style;
    (void)base;
    BotExplore& ex = g_explore[slot];
    if (ex.entity != self) { ex = {}; ex.entity = self; ex.style = BotStyle(slot); }
    if (!g_mapKnow.Ready()) return false;
    // 0.5.202: Justin: bots struggled to leave their spawn, the enemy never
    // pushed toward his, no one went back to check the flag, and snipers did
    // not hold and watch. Each errand now has a purpose, chosen when it is
    // picked: out of his own spawn first (his first 15 s, within 900 of it),
    // a sniper's overwatch, a push to the enemy's spawn (half the rushers),
    // a look at the War flag (every other errand for a quarter of the bots),
    // else his style's spot, defenders leaning to their own half and
    // attackers to the enemy's.
    const uint32_t record = Guest::Read32(base, self + 512);
    const uint32_t mySide = BotSide(base, slot), enemySide = mySide == 1 ? 2 : 1;
    Point own{}, enemy{};
    const bool haveOwn = SpawnCentre(mySide, own), haveEnemy = SpawnCentre(enemySide, enemy);
    const bool sniper = record && BotsScopedWeapon(base, HeldDef(base, record));
    const uint32_t tick = GetTickCount();
    const Point here{ me[0], me[1], me[2] - myEye };
    const bool justSpawned = haveOwn && tick - g_spawnedTick[slot] < 15000 && Distance(here, own) < 900.0f;
    const bool pusher = g_role[slot] == RoleRusher && slot % 2 == 0 && haveEnemy;
    const bool rusher2 = BotIsRusher2(slot) && haveEnemy;   // 0.5.329
    const bool special = justSpawned || sniper || pusher || rusher2 || ex.purpose != BotExplore::Normal;
    const S style = (ex.style == S::Hunter && !found) ? S::Outskirts : ex.style;
    if (style == S::Hunter && !special) return false;
    const float range = style == S::Outskirts ? 900.0f : style == S::High ? 1700.0f : 1100.0f;
    // 0.5.200: once on an errand he keeps to it unless an enemy is in sight or
    // within 550 (0.5.199 gave it up for any enemy within 900-1700: with 20
    // bots that was nearly always, so only 37 errands ran on Rouen).
    // 0.5.202: leaving spawn and pushing keep on unless one is within 400.
    const bool onErrand = ex.dest >= 0 || now < ex.holdUntil;
    const bool determined = ex.purpose == BotExplore::Exit || ex.purpose == BotExplore::Push;
    if (found && (visible || enemyDistance < (onErrand ? (determined ? 400.0f : 550.0f) : (special ? 700.0f : range)))) { ex.holdUntil = 0; return false; }
    const Point feet{ me[0], me[1], me[2] - myEye };
    holding = false;
    ex.activeTick = GetTickCount();
    if (ex.dest >= int32_t(g_mapKnow.nodes.size())) ex.dest = -1;
    if (now < ex.holdUntil)
    {
        holding = true;
        memcpy(goalOut, me, 12); goalOut[2] = feet.z;
        // On watch: toward where the enemy was last known, else sweeping
        // the map's middle side to side.
        const MapKnowledge::Shape& shape = g_mapKnow.Measure();
        float yaw = found ? atan2f(enemyAt[1] - me[1], enemyAt[0] - me[0]) : atan2f(shape.centre.y - me[1], shape.centre.x - me[0]);
        if (!found) yaw += sinf(float(now % 9000) / 9000.0f * 6.2831853f) * 1.05f;
        // 0.5.282: out of a learnt lookout: its way, swept 35 degrees each side.
        if (!found && now < ex.scanUntil) yaw = ex.scanYaw + sinf(float(now % 5000) / 5000.0f * 6.2831853f) * 0.6f;
        // 0.5.202: a sniper watches the busiest ways in turn, 4 s each.
        if (ex.purpose == BotExplore::Overwatch && !ex.watch.empty())
        {
            const Point w = ex.watch[(now / 4000) % ex.watch.size()];
            yaw = atan2f(w.y - me[1], w.x - me[0]) + sinf(float(now % 4000) / 4000.0f * 6.2831853f) * 0.12f;
        }
        lookOut[0] = me[0] + cosf(yaw) * 600.0f; lookOut[1] = me[1] + sinf(yaw) * 600.0f; lookOut[2] = me[2];
        return true;
    }
    if (ex.dest >= 0 && (now >= ex.giveUpAt || g_tactical[slot].progress.strikes >= ex.strikesAtStart + 3))
    {
        g_mapKnow.Failed(ex.dest);
        printf("bots: slot %u gives up on his %s spot at %.0f %.0f %.0f (%s)\n", slot, StyleName(style), g_mapKnow.nodes[size_t(ex.dest)].at.x,
               g_mapKnow.nodes[size_t(ex.dest)].at.y, g_mapKnow.nodes[size_t(ex.dest)].at.z, now >= ex.giveUpAt ? "took too long" : "stuck");
        fflush(stdout);
        ex.dest = -1; ex.nextPick = now + 1500;
    }
    if (ex.dest < 0)
    {
        if (now < ex.nextPick) return false;
        std::vector<Point> claimed;
        for (uint32_t other = 0; other < ActiveSlots(); other++)
            if (other != slot && g_bot[other] && g_explore[other].dest >= 0 && size_t(g_explore[other].dest) < g_mapKnow.nodes.size())
                claimed.push_back(g_mapKnow.nodes[size_t(g_explore[other].dest)].at);
        // 0.5.198: only spots a learnt route reaches; else one no farther
        // than 1200 (walked straight).
        const std::vector<uint8_t> reach = g_mapKnow.Reach(feet);
        // 0.5.202: this errand's purpose and what it asks of the pick.
        S pickStyle = style == S::Hunter ? S::Outskirts : style;
        Point towards{ NAN, NAN, NAN }; float lean = 0.0f, nearRadius = 900.0f;
        ex.purpose = BotExplore::Normal;
        ex.flagNext = !ex.flagNext;
        if (justSpawned) { ex.purpose = BotExplore::Exit; pickStyle = S::Away; towards = own; if (haveEnemy) { lean = 0.0f; } }
        else if (rusher2)
        {
            // 0.5.329: push, clear, the objective, flank, round again
            const uint8_t phase = ex.r2Phase++ % 4;
            const bool flag = g_war.valid && GetTickCount() - g_war.when < 10000;
            if (phase == 0 || (phase == 2 && !flag)) { ex.purpose = BotExplore::Push; pickStyle = S::Near; towards = enemy; nearRadius = 900.0f; }
            else if (phase == 1) { pickStyle = S::Low; towards = enemy; lean = 1.0f; }
            else if (phase == 2) { ex.purpose = BotExplore::Flag; pickStyle = S::Near; towards = { g_war.at[0], g_war.at[1], g_war.at[2] }; nearRadius = 450.0f; }
            else { pickStyle = S::Outskirts; towards = enemy; lean = 1.5f; }
            static int told = 0;
            if ((told < 60 && told++ < 60)) { printf("bots: slot %u (rusher 2) errand %u: %s\n", slot, phase, phase == 0 || (phase == 2 && !flag) ? "push to the enemy's spawn" : phase == 1 ? "clear low ground toward them" : phase == 2 ? "back to the objective" : "flank round the outskirts"); fflush(stdout); }
        }
        else if (sniper)
        {
            ex.purpose = BotExplore::Overwatch; pickStyle = S::High; if (haveEnemy) { towards = enemy; lean = 1.5f; }
            // 0.5.282: a learnt lookout first, when there is a good one in reach.
            const int32_t l = BestLookout(feet, 4500.0f, slot, GetTickCount());
            const int32_t node = l >= 0 ? g_mapKnow.Nearest(g_lookouts[size_t(l)].at, 200.0f) : -1;
            if (node >= 0)
            {
                bool claimedNode = false;
                for (uint32_t other = 0; other < ActiveSlots(); other++) if (other != slot && g_bot[other] && g_explore[other].dest == node) claimedNode = true;
                if (!claimedNode)
                {
                    ex.lookout = l;
                    g_lookouts[size_t(l)].usedAt = GetTickCount();
                    printf("bots: slot %u (sniper) takes the lookout at %.0f %.0f %.0f (score %.1f)\n", slot, g_lookouts[size_t(l)].at.x, g_lookouts[size_t(l)].at.y,
                           g_lookouts[size_t(l)].at.z, g_lookouts[size_t(l)].score);
                    fflush(stdout);
                }
            }
        }
        else if (pusher) { ex.purpose = BotExplore::Push; pickStyle = S::Near; towards = enemy; nearRadius = 700.0f; }
        else if (slot % 4 == 1 && ex.flagNext && g_war.valid && GetTickCount() - g_war.when < 10000)
        { ex.purpose = BotExplore::Flag; pickStyle = S::Near; towards = { g_war.at[0], g_war.at[1], g_war.at[2] }; nearRadius = 350.0f; }
        else if (haveOwn && haveEnemy)
        {
            const bool defensive = g_role[slot] == RoleCamper || g_form[slot] <= -2;
            const bool aggressive = g_role[slot] == RoleRusher || g_form[slot] >= 2;
            if (defensive) { towards = own; lean = 1.5f; }
            else if (aggressive) { towards = enemy; lean = 1.5f; }
        }
        const unsigned seed = slot * 131u + now / 1000u;
        ex.dest = -1;
        if (ex.purpose == BotExplore::Overwatch && ex.lookout >= 0 && size_t(ex.lookout) < g_lookouts.size())
            ex.dest = g_mapKnow.Nearest(g_lookouts[size_t(ex.lookout)].at, 200.0f);   // 0.5.282
        if (ex.dest < 0) { ex.lookout = -1; ex.dest = g_mapKnow.Pick(pickStyle, feet, seed, claimed, &reach, 1e30f, towards, lean, nearRadius); }
        if (ex.dest < 0) ex.dest = g_mapKnow.Pick(pickStyle, feet, seed, claimed, nullptr, 1200.0f, towards, lean, nearRadius);
        if (ex.dest < 0 && ex.purpose != BotExplore::Normal)
        {   // nothing of that kind: his style's spot after all
            ex.purpose = BotExplore::Normal; pickStyle = style == S::Hunter ? S::Outskirts : style;
            ex.dest = g_mapKnow.Pick(pickStyle, feet, seed, claimed, &reach);
        }
        if (ex.dest < 0) { ex.nextPick = now + 5000; return false; }
        const Point dest = g_mapKnow.nodes[size_t(ex.dest)].at;
        ex.path = g_mapKnow.Path(feet, ex.dest);
        const bool routed = !ex.path.empty();
        if (!routed) ex.path.push_back(dest);
        ex.at = 0;
        ex.giveUpAt = now + 20000 + uint32_t(Distance(feet, dest) * 15.0f);
        ex.strikesAtStart = g_tactical[slot].progress.strikes;
        static const char* const purposes[] = { "", " to leave his spawn", " to push toward the enemy's spawn", " to check the flag", " for a sniper's watch" };
        printf("bots: slot %u heads for %s%s at %.0f %.0f %.0f (%.0f away, %s, %zu steps)\n", slot, StyleName(pickStyle), purposes[ex.purpose], dest.x, dest.y, dest.z,
               Distance(feet, dest), routed ? "a learnt route" : "straight", ex.path.size());
        fflush(stdout);
    }
    const Point dest = g_mapKnow.nodes[size_t(ex.dest)].at;
    if (Distance(feet, dest) < 90.0f && std::fabs(feet.z - dest.z) < 80.0f)
    {
        g_mapKnow.Reached(ex.dest);
        const uint32_t jitter = (slot * 977u + now) % 20000u;
        ex.holdUntil = BotIsRusher2(slot) ? now + 2500 + jitter % 2500 : now + (style == S::Outskirts ? 2000 + jitter / 5 : 15000 + jitter);   // 0.5.329: Rusher 2 keeps moving
        // 0.5.469: on, after 5 to 10 s, all but the campers (Justin: "clear houses,
        // and rooms and keep moving on unless needing to stay to defend or attack
        // objectives"; was 15 to 35 s at each spot reached)
        if (!BotIsRusher2(slot) && style != S::Outskirts && g_role[slot] != RoleCamper) ex.holdUntil = now + 5000 + jitter / 4;
        // 0.5.202: by purpose: out of spawn, straight on; a push or the flag,
        // a short wait in ambush; a sniper, 35-60 s watching the busiest ways.
        if (ex.purpose == BotExplore::Exit) ex.holdUntil = now + 500;
        else if (ex.purpose == BotExplore::Push) ex.holdUntil = now + (BotIsRusher2(slot) ? 3000 + jitter % 3000 : 6000 + jitter % 6000);
        else if (ex.purpose == BotExplore::Flag) ex.holdUntil = now + (BotIsRusher2(slot) ? 12000 + jitter % 6000 : 6000 + jitter % 4000);   // 0.5.329: Rusher 2 holds it with the Bren
        else if (ex.purpose == BotExplore::Overwatch)
        {
            ex.holdUntil = now + 35000 + jitter % 25000; ex.watch = g_mapKnow.Watch(dest, 3);
            // 0.5.282: on a lookout: its own way first among the ways watched.
            if (ex.lookout >= 0 && size_t(ex.lookout) < g_lookouts.size())
            {
                const BotLookout& l = g_lookouts[size_t(ex.lookout)];
                ex.watch.insert(ex.watch.begin(), { dest.x + cosf(l.yaw) * 1500.0f, dest.y + sinf(l.yaw) * 1500.0f, dest.z });
                ex.watch.insert(ex.watch.begin(), { dest.x + cosf(l.yaw) * 1500.0f, dest.y + sinf(l.yaw) * 1500.0f, dest.z });
            }
            ex.lookout = -1;
        }
        ex.holdHigh = style == S::High || ex.purpose == BotExplore::Overwatch;
        printf("bots: slot %u reached his %s spot; on watch %u s\n", slot, StyleName(style), (ex.holdUntil - now) / 1000);
        fflush(stdout);
        ex.dest = -1; ex.nextPick = ex.holdUntil;
        holding = true;
        memcpy(goalOut, me, 12); goalOut[2] = feet.z;
        memcpy(lookOut, me, 12);
        return true;
    }
    // 0.5.282: a scout (or a sniper) on his way past a learnt lookout stops
    // there 4-6 s and scans out of it, then goes on (Justin: scouts should
    // look out of the windows they come across, not stay in them).
    if ((g_botKit[slot].base == 6 || sniper) && !g_lookouts.empty())
    {
        for (size_t i = 0; i < g_lookouts.size(); i++)
        {
            BotLookout& l = g_lookouts[i];
            if (int32_t(i) == ex.lastLookout && tick - ex.lookoutAt < 90000) continue;
            if (l.score < 1.0f || std::hypot(l.at.x - feet.x, l.at.y - feet.y) > 200.0f || std::fabs(l.at.z - feet.z) > 80.0f) continue;
            const uint32_t pause = 4000 + (slot * 389u + tick) % 2000u;
            ex.lastLookout = int32_t(i); ex.lookoutAt = tick;
            ex.scanYaw = l.yaw; ex.scanUntil = now + pause; ex.holdUntil = now + pause;
            ex.giveUpAt += pause;
            l.usedAt = tick;
            static int told = 0;
            if ((told < 60 && told++ < 60)) { printf("bots: slot %u looks out of the lookout at %.0f %.0f %.0f for %u s\n", slot, l.at.x, l.at.y, l.at.z, pause / 1000); fflush(stdout); }
            holding = true;
            memcpy(goalOut, me, 12); goalOut[2] = feet.z;
            lookOut[0] = me[0] + cosf(l.yaw) * 600.0f; lookOut[1] = me[1] + sinf(l.yaw) * 600.0f; lookOut[2] = me[2];
            return true;
        }
    }
    // Along the route: the next step once within reach, and on to a later
    // one that is just as near and level (no zigzag over every cell).
    while (ex.at + 1 < ex.path.size() && Distance(feet, ex.path[ex.at]) < 80.0f && std::fabs(feet.z - ex.path[ex.at].z) < 90.0f) ex.at++;
    for (size_t k = std::min(ex.path.size() - 1, ex.at + 3); k > ex.at; k--)
        if (Distance(feet, ex.path[k]) < 170.0f && std::fabs(feet.z - ex.path[k].z) < 40.0f) { ex.at = k; break; }
    const Point step = ex.path[std::min(ex.at, ex.path.size() - 1)];
    goalOut[0] = step.x; goalOut[1] = step.y; goalOut[2] = step.z;
    lookOut[0] = step.x; lookOut[1] = step.y; lookOut[2] = me[2];
    return true;
}

static void TacticalMove(PPCContext& ctx,uint8_t* base,uint32_t self,uint32_t threat,
                         uint32_t slot,uint32_t now,int health,float viewYaw,
                         const float target[3],bool atFlag,BotOrders& orders,
                         const float* moveGoal=nullptr)
{
    if (!g_botTactics.load()) return;
    using namespace BotNavigation;
    auto& state=g_tactical[slot];
    const uint32_t record=Guest::Read32(base,self+512);
    const Point feet{ReadFloat(base,record),ReadFloat(base,record+4),ReadFloat(base,record+8)};
    auto& failed=g_failedRoutes[slot];
    if(g_failedEntity[slot]!=self){failed.Clear();g_failedEntity[slot]=self;}
    failed.Tick(now);
    auto query=[&](Point a,Point b,bool body) {return BotTrace(ctx,base,self,self,a,b,body);};
    state.climbing=false;
    const bool attached=(Guest::Read32(base,record+44)&0x10u)!=0;
    if ((attached && now>=state.ladderCooldown) || now<state.ladderUntil) {
        Point normal=attached?Point{ReadFloat(base,record+100),ReadFloat(base,record+104),ReadFloat(base,record+108)}:state.ladderNormal;
        if (Finite(normal)&&std::hypot(normal.x,normal.y)>0.7f) {
            if (!state.ladderProgress || std::fabs(feet.z-state.ladderHeight)>12) {
                state.ladderProgress=now;state.ladderHeight=feet.z;
            }
            if (now-state.ladderProgress<3500) {
                if(attached)state.ladderUntil=now+1000;
                const float face=std::atan2(-normal.y,-normal.x)*57.29578f;
                g_yaw[slot]+=std::clamp(AngleDifference(face,viewYaw),-8.0f,8.0f);
                // 0.5.196: down when his goal is well below him (back on the stick).
                const bool downward=std::isfinite(g_goalZ[slot]) && g_goalZ[slot]<feet.z-90;
                orders.forward=downward?-127:127;orders.right=orders.up=0;
                if(!attached && Distance(feet,state.ladderApproach)>18) {
                    const float toward=std::atan2(state.ladderApproach.y-feet.y,state.ladderApproach.x-feet.x)-viewYaw*0.01745329252f;
                    orders.forward=int8_t(std::cos(toward)*110);orders.right=int8_t(-std::sin(toward)*110);
                }
                orders.buttons=BotCrouch(slot,false);orders.wantsFire=false;
                state.climbing=true;state.haveStep=false;
                return; // native PM_LadderMove makes forward input climb
            }
            state.ladderCooldown=now+8000;state.ladderUntil=0;
            state.escapeUntil=now+3000;state.hand=-state.hand;
        } else state.ladderUntil=0;
    }
    if(atFlag)return;
    // 0.5.196: the goal's real height (the planner flattens it to his feet).
    const float goalZ=target[2];
    g_goalZ[slot]=goalZ;
    const bool goalAbove=std::isfinite(goalZ)&&goalZ>feet.z+140, goalBelow=std::isfinite(goalZ)&&goalZ<feet.z-90;
    // A ladder near him, found by the native ladder surface flag (never by
    // assuming a wall is climbable): 8 directions, out to 160 units, at chest
    // and head height (0.5.110 looked 80 units in 5 directions only).
    auto scanLadder=[&](float yaw)->bool {
        if(now<state.ladderCooldown || now-state.ladderScanAt<700) return false;
        state.ladderScanAt=now;
        for(float height:{32.0f,72.0f})
        for(float degrees:{0.0f,45.0f,-45.0f,90.0f,-90.0f,135.0f,-135.0f,180.0f}) {
            const float angle=yaw+degrees*0.01745329252f;
            const Point eye{feet.x,feet.y,feet.z+height};
            auto hit=query(eye,{feet.x+std::cos(angle)*160,feet.y+std::sin(angle)*160,feet.z+height},false);
            if(!LadderSurface(hit))continue;
            const Point approach{hit.end.x+hit.normal.x*24,hit.end.y+hit.normal.y*24,feet.z};
            Point supported;
            if(!Reachable(feet,approach,query,supported))continue;
            state.ladderApproach=supported;
            state.ladderNormal=hit.normal;state.ladderUntil=now+6000;
            state.ladderProgress=now;state.ladderHeight=feet.z;
            const float face=std::atan2(-hit.normal.y,-hit.normal.x)*57.29578f;
            g_yaw[slot]+=std::clamp(AngleDifference(face,viewYaw),-8.0f,8.0f);
            orders.forward=orders.right=orders.up=0;orders.buttons=BotCrouch(slot,false);orders.wantsFire=false;
            state.climbing=true;state.haveStep=false;
            printf("bots: navigation slot %u approaches native ladder %.0f away (goal %s)\n",slot,Distance(feet,hit.end),
                goalAbove?"above":goalBelow?"below":"level");fflush(stdout);
            return true;
        }
        return false;
    };
    // 0.5.166: also when his gun is empty (reloading) with someone about, and
    // sooner when hurt (60, was 40).
    bool reloading=false;
    {
        const uint32_t weapon=Guest::Read32(base,record+164);
        const uint32_t list=Guest::Read32(base,0x82BCBF9C);
        const uint32_t def=(weapon>0&&weapon<128&&list)?Guest::Read32(base,list+4*weapon):0;
        if (def>=0x40000000u&&def<0xA0000000u) {
            const uint32_t clip=Guest::Read32(base,def+1468),spare=Guest::Read32(base,def+1460);
            if (clip<=128&&spare<=128) reloading=Guest::Read32(base,record+4*(172+clip))==0&&Guest::Read32(base,record+4*(80+spare))>0;
        }
    }
    // 0.5.195: also, now and then, in a long gunfight (seen 3 s), unless he
    // is a rusher: real players fight from cover, not the open.
    const bool longFight=state.visible && now-state.sightSince>3000 && g_role[slot]!=RoleRusher && ((now/2500+slot)%3)==0;
    if (threat && now-state.coverSearch>2500 && (health<=60 || health<state.health-14 || reloading || longFight)) {
        state.coverSearch=now;
        Point enemy{target[0],target[1],target[2]};
        auto sight=[&](Point a,Point b,bool body){return BotTrace(ctx,base,threat,self,a,b,body);};
        if (Cover(feet,enemy,query,sight,state.cover)) {
            state.coverUntil=now+(health<=35?6500:4000);state.planned=0;
            if(health<=50)g_wantSmoke[slot]=GetTickCount()+400;   // 0.5.195: smoke to cover the run
            printf("bots: tactics slot %u seeks cover at %.0f %.0f\n",slot,state.cover.x,state.cover.y);fflush(stdout);
        }
    }
    state.health=health;
    const bool shelter=now<state.coverUntil;
    if (shelter && Distance(feet,state.cover)<35) {
        orders.forward=orders.right=0;
        orders.buttons&=~SprintButton;
        // 0.5.195: in cover: badly hurt, he lies flat; otherwise he peeks,
        // up to shoot then down again, every 1.5 s.
        if(health<=35)BotProne(slot);
        else BotCrouch(slot,((now/1500)+slot)%2==0);
        return;
    }
    if (!shelter && orders.forward==0 && orders.right==0) return;
    Point goal=shelter?state.cover:(moveGoal?Point{moveGoal[0],moveGoal[1],feet.z}:Point{target[0],target[1],feet.z});
    // 0.5.196: breaking free: a wander point overrides the goal for a while.
    const bool wandering=now<state.wanderUntil;
    if(wandering) {
        if(Distance(feet,state.wanderTo)<48) state.wanderUntil=0;
        else if(orders.forward<0) orders.forward=127;
    }
    if (!shelter && orders.forward<0) {
        float yaw=std::atan2(feet.y-goal.y,feet.x-goal.x);
        goal={feet.x+std::cos(yaw)*128,feet.y+std::sin(yaw)*128,feet.z};
    }
    if (now-state.sampled>=900) {
        const float moved=Distance(feet,state.last);
        if(state.sampled && moved<12) {
            ++state.stalledSamples;
            if(now>=state.escapeUntil) {state.hand=-state.hand;state.escapeUntil=now+3000;}
            // 0.5.195: Justin, on 0.5.191: some bots fail a jump and then
            // retry the same obstacle instead of going round it. A straight
            // step he has stalled on for ~3 s is remembered as failed, so
            // the planner stops choosing it.
            if(state.haveStep && !state.jumpPending && state.stalledSamples==3) {
                const unsigned strikes=failed.Remember(feet,state.step,now);
                printf("bots: navigation slot %u stuck going to %.0f %.0f from %.0f %.0f (strike %u): rerouting\n",
                    slot,state.step.x,state.step.y,feet.x,feet.y,strikes);fflush(stdout);
            }
            state.haveStep=false;
            if(state.jumpPending && now>=state.jumpUntil) {
                state.nextJump=now+8000;state.jumpPending=false;
                state.visits[state.visitNext++%state.visits.size()]={state.step,true};
                // 0.5.195: the failed jump is remembered (start and landing):
                // not tried again from there, and the route goes round.
                const unsigned strikes=failed.Remember(state.jumpStart,state.step,now);
                state.hand=-state.hand;state.escapeUntil=now+3000;
                printf("bots: navigation slot %u jump at %.0f %.0f failed (strike %u, %u places remembered): going round\n",
                    slot,state.jumpStart.x,state.jumpStart.y,strikes,failed.Active(now));fflush(stdout);
            }
        } else {state.stalledSamples=0; if(moved>24)state.jumpPending=false;}
        state.visits[state.visitNext++%state.visits.size()]={feet,true};
        state.sampled=now;state.last=feet;
    }
    if(now<state.wanderUntil) goal=state.wanderTo;
    // 0.5.196: stuck = no net progress for 3.5 s while he wants to move
    // (jitter included). Each repeat tries harder: 1 another way round (the
    // spot remembered as failed), 2 a jump or a ladder or a drop, 3+ a wander
    // 300-500 units off in a new direction for 5 s.
    {
        const bool holding=shelter && Distance(feet,state.cover)<35;
        if(state.progress.Track(feet,now,!holding && (orders.forward!=0 || orders.right!=0 || state.haveStep),!shelter && Distance(feet,goal)>300.0f)) {
            {   // 0.5.597: how many were the back-and-forth test's, once in 25
                static unsigned toldRegion=0; unsigned all=0;
                for (const auto& t : g_tactical) all+=t.progress.regionStrikes;
                if (all/25!=toldRegion) {toldRegion=all/25;printf("bots: navigation: %u strikes so far for going back and forth (8 s within 200)\n",all);fflush(stdout);}
            }
            const unsigned strikes=state.progress.strikes;
            const Point aim=state.haveStep?state.step:goal;
            failed.Remember(feet,aim,now);
            state.haveStep=false;state.hand=-state.hand;state.escapeUntil=now+3000;state.planned=0;
            const char* action="another way";
            Point landing;
            if(strikes>=2 && JumpOver(feet,goal,query,landing) && !failed.Blocks(feet,landing,now)) {
                state.step=landing;state.haveStep=true;state.jumpStart=feet;
                state.jumpPulse=now+100;state.jumpUntil=now+700;state.nextJump=now+2500;state.jumpPending=true;
                action="a jump";state.stepWhy="stuck: jump";
            } else if((strikes>=2 || goalBelow) && DropDown(feet,goal,query,landing) && !failed.Blocks(feet,landing,now)) {
                state.step=landing;state.haveStep=true;action="a drop down";state.stepWhy="stuck: drop";
            } else if(strikes>=2 && scanLadder(std::atan2(goal.y-feet.y,goal.x-feet.x))) {
                printf("bots: navigation slot %u stuck %u times: tries the ladder\n",slot,strikes);fflush(stdout);
                return;
            } else if(strikes==2) {
                // 0.5.485: a hop toward the goal when no planned jump is found
                // (483: 1200 stuck lines, 332 wanders, not one jump: JumpOver
                // never trusted a landing; user: "they need to learn to jump")
                const float hx=goal.x-feet.x,hy=goal.y-feet.y,hl=std::max(1.0f,std::hypot(hx,hy));
                state.step=Point{feet.x+hx/hl*96.0f,feet.y+hy/hl*96.0f,feet.z};state.haveStep=true;state.jumpStart=feet;
                state.jumpPulse=now+100;state.jumpUntil=now+700;state.nextJump=now+2500;state.jumpPending=true;
                action="a hop toward the goal";state.stepWhy="stuck: hop";
            } else if(strikes>=3) {
                auto avoid=[&](Point p){ return failed.NearFailedTarget(p,now)?400.0f:0.0f; };
                Point out;
                if(Wander(feet,goal,slot+strikes,query,avoid,out)) {
                    state.wanderTo=out;state.wanderUntil=now+5000;state.step=out;state.haveStep=false;action="a wander";state.stepWhy="wander";
                }
            }
            printf("bots: navigation slot %u stuck (no progress in 3.5 s, strike %u) at %.0f %.0f %.0f: tries %s\n",
                slot,strikes,feet.x,feet.y,feet.z,action);fflush(stdout);
            // 0.5.466: stuck six times running (some 20 s; 0.5.465's War on Rouen
            // had bots at strike 9), with no enemy in sight within 1500: put
            // three standing spots along his learnt route to the goal (the
            // game's own teleport, sub_8255E118), or by his side's spawns when
            // no route is known
            // 0.5.518: the flag carrier whatever is in sight (517's carrier hit the
            // same wall 11 times mid-map and never got home)
            const bool carrierStuck=g_flagCarrier[0]==int(slot)||g_flagCarrier[1]==int(slot);
            if(strikes>=4 && (g_seeDist[slot]>1500.0f || carrierStuck)) {   // 0.5.485: 4 (was 6, some 20 s)
                Point to{};bool have=false;
                const int32_t cell=g_mapKnow.Nearest(goal,400.0f);
                if(cell>=0){const std::vector<Point> way=g_mapKnow.Path(feet,cell);if(!way.empty()){to=way[std::min<size_t>(3,way.size()-1)];have=true;}}
                if(!have){Point spawn;if(SpawnCentre(BotSide(base,slot),spawn)){to=spawn;have=true;}}
                const uint32_t record=Guest::Read32(base,self+512);
                if(have && record && Distance(to,feet)>64) {
                    static uint32_t position=0;
                    if(!position)position=Guest::AllocatePhysical(0x20);
                    auto put=[&](uint32_t at,float f){uint32_t w;memcpy(&w,&f,4);Guest::Write32(base,at,w);};
                    put(position,to.x);put(position+4,to.y);put(position+8,to.z+1.0f);
                    PPCContext place=ctx;place.r1.u32=(ctx.r1.u32-0x1000)&~0xFu;place.r3.u64=self;place.r4.u64=position;place.lr=0;
                    Guest::Lookup(0x8255E118)(place,base);
                    for(int k=0;k<3;k++)put(record+16+4*k,0.0f);
                    state.progress.Reset();state.haveStep=false;state.wanderUntil=0;
                    printf("bots: navigation slot %u unstuck: put at %.0f %.0f %.0f (%s)\n",slot,to.x,to.y,to.z,cell>=0?"along his route":"by his spawns");fflush(stdout);
                }
            }
        }
    }
    // Hold a chosen waypoint; only invalidate it if collision changed.
    if(state.haveStep && now>=state.jumpUntil && now-state.navChecked>=250) {
        state.navChecked=now;Point landing;
        if(!Reachable(feet,state.step,query,landing))state.haveStep=false;
    }
    const bool reached=state.haveStep&&Distance(feet,state.step)<28;
    if(now>=state.jumpUntil && (!state.planned || now-state.planned>=600 || (reached&&now-state.planned>=150))) {
        state.planned=now;
        if(!state.haveStep || reached || now<state.escapeUntil) {
            const float yaw=std::atan2(goal.y-feet.y,goal.x-feet.x);
            const float length=std::fmin(Distance(feet,goal),96.0f);
            Point direct{feet.x+std::cos(yaw)*length,feet.y+std::sin(yaw)*length,feet.z},landing;
            // 0.5.561: a goal within 192 (Climb's four pieces of 48) is tested
            // the whole way, as the route picker tests it, so a spot it found
            // walkable is walked: Climb lets a piece rise by its length, so a
            // slope or steps that pass as one 170 walk can fail as one 96 step
            // (560's self-test: side 2's attackers at 850 -930 refused the step
            // to a walkable spot 140-220 north, the hull meeting raised ground,
            // normal up 0.99, and turned off on a detour).
            bool clear=false;
            bool ledge=false;   // 0.5.608: what stops the step is a raised edge (its face up), not a wall
            if(Distance(feet,goal)<=192.0f && Reachable(feet,goal,query,landing)) clear=true;   // the step: the goal's own ground
            else clear=Reachable(feet,direct,query,landing);
            // 0.5.549: why not, Flat's own tests again (548's trace: 285 of the
            // 355 samples with the legs over 90 off the goal had a "blocked"
            // direct step turned into a detour 90-180 off, though the goal had
            // a clear line at knee and chest): what each test hit (its handle)
            if(!clear) {
                Point s{feet.x,feet.y,feet.z+1},e{direct.x,direct.y,feet.z+1};
                const Hit low=query(s,e,true);
                const Point r{feet.x,feet.y,feet.z+18};
                const Hit up=query(s,r,true);
                const Hit high=query(r,{direct.x,direct.y,feet.z+18},true);
                const bool lowOk=low.valid&&!low.solid&&low.fraction>=0.999f, highOk=up.valid&&!up.solid&&up.fraction>=0.999f&&high.valid&&!high.solid&&high.fraction>=0.999f;
                ledge=!lowOk && low.valid && !low.solid && low.normal.z>0.7f;
                if(!lowOk && !highOk)
                    snprintf(state.blockWhy,sizeof(state.blockWhy),"hull: feet %s at %.2f (handle %X, normal up %.2f), stair %s at %.2f (handle %X)",
                             !low.valid?"no trace":low.solid?"starts solid":"hits",low.fraction,low.handle,low.normal.z,
                             !up.valid||up.solid||up.fraction<0.999f?"no headroom":!high.valid?"no trace":high.solid?"starts solid":"hits",high.fraction,high.handle);
                else {
                    const Point end=lowOk?e:Point{direct.x,direct.y,feet.z+18};
                    const Hit floor=query(end,{end.x,end.y,feet.z-32},true);
                    snprintf(state.blockWhy,sizeof(state.blockWhy),"floor: %s (fraction %.2f, %.0f under his feet, normal up %.2f; hull clear %s)",
                             !floor.valid?"no trace":floor.solid?"starts solid":floor.fraction>=1?"none within 32 down":floor.normal.z<0.65f?"too steep":floor.end.z<feet.z-30?"too far down":floor.end.z>feet.z+18?"too high":"fine (Climb failed)",
                             floor.fraction,feet.z-floor.end.z,floor.normal.z,lowOk?"at the feet":"a stair up");
                }
            } else state.blockWhy[0]=0;
            if(clear && now>=state.escapeUntil && !failed.Blocks(feet,landing,now)) {state.step=landing;state.haveStep=true;state.stepWhy="direct";}
            else {
                // One supported low jump, then a long cooldown after failure.
                if(goalBelow && DropDown(feet,goal,query,landing) && !failed.Blocks(feet,landing,now)) {
                    state.step=landing;state.haveStep=true;state.stepWhy="drop";
                    static uint32_t toldDrop[MaxClients]={};
                    if(now-toldDrop[slot]>4000){toldDrop[slot]=now;printf("bots: navigation slot %u drops down toward a lower goal (%.0f below)\n",slot,feet.z-landing.z);fflush(stdout);}
                } else
                // 0.5.608: also at once over a raised edge (a curb, a step of
                // rubble higher than a stair): 607's carrier slot 4 at 900 700 had
                // his route's next spot 68 off, the step "blocked" by ground 0.98
                // up at his feet, and detoured 160 off and back for 10 s, never
                // stalled long enough to try the jump
                if((state.stalledSamples || ledge) && now>=state.nextJump && JumpOver(feet,goal,query,landing) &&
                   !failed.Blocks(feet,landing,now)) {
                    state.step=landing;state.haveStep=true;state.jumpStart=feet;
                    state.jumpPulse=now+100;state.jumpUntil=now+700;
                    state.nextJump=now+2500;state.jumpPending=true;state.stepWhy="low jump";
                    printf("bots: navigation slot %u tries a cleared low jump\n",slot);fflush(stdout);
                } else {
                    auto route=Detour(feet,goal,state.hand,now<state.escapeUntil,state.visits,query,&failed,now);
                    state.haveStep=route.valid;if(route.valid){state.step=route.next;state.stepWhy=now<state.escapeUntil?"detour (escaping)":clear?"detour (direct failed-before)":"detour (blocked)";}
                    else if(ShortEscape(feet,goal,state.hand,query,landing)) {
                        state.step=landing;state.haveStep=true;state.stepWhy="short escape";
                    }
                }
                // Nearby ladders are identified by the native surface flag,
                // never by assuming every wall is climbable.
                if((!state.haveStep || state.stalledSamples || state.progress.strikes || goalAbove || goalBelow) && scanLadder(yaw)) return;
            }
        }
    }
    if (!state.haveStep) {orders.forward=orders.right=0;orders.buttons&=~SprintButton;return;}
    if(now<state.jumpUntil) {
        orders.up=now<state.jumpPulse?127:0;
        orders.buttons&=~SprintButton;
        orders.buttons|=BotCrouch(slot,false);
    }
    const float yaw=std::atan2(state.step.y-feet.y,state.step.x-feet.x);
    const float relative=yaw-viewYaw*0.01745329252f;
    orders.forward=int8_t(std::clamp(std::cos(relative)*127.0f,-127.0f,127.0f));
    // The stick's positive right is clockwise from the view's forward.
    orders.right=int8_t(std::clamp(-std::sin(relative)*127.0f,-127.0f,127.0f));
    if (shelter) {orders.buttons&=~SprintButton;orders.buttons|=BotCrouch(slot,false);}
    // 0.5.196: teammates closer than ~72 units push him aside, so a crowd in
    // front of an obstacle spreads out instead of jostling in one spot.
    {
        const uint32_t entities=Guest::Read32(base,EntityList);
        const uint32_t mySide=Side(base,self);
        Point mates[MaxClients];int count=0;
        for(uint32_t other=0;entities && other<ActiveSlots();other++) {
            const uint32_t entity=Guest::Read32(base,entities+4*(other+1));
            if(!entity || entity==self || Side(base,entity)!=mySide) continue;
            const uint32_t theirs=Guest::Read32(base,entity+512);
            if(!theirs || int32_t(Guest::Read32(base,theirs+304))<=0) continue;
            mates[count++]={ReadFloat(base,theirs),ReadFloat(base,theirs+4),ReadFloat(base,theirs+8)};
        }
        const Push push=Separation(feet,mates,count,state.visible?90.0f:220.0f);   // 0.5.200: 220 out of a fight (was 72)
        const float strength=std::hypot(push.x,push.y);
        if(strength>0.05f) {
            const float rel=std::atan2(push.y,push.x)-viewYaw*0.01745329252f;
            const float m=std::fmin(strength,1.5f)*70.0f;
            orders.forward=int8_t(std::clamp(orders.forward+std::cos(rel)*m,-127.0f,127.0f));
            orders.right=int8_t(std::clamp(orders.right-std::sin(rel)*m,-127.0f,127.0f));
        }
    }
    if (now-state.logged>3000) {
        state.logged=now;
        printf("bots: tactics slot %u route %.0f %.0f, visibility %d, cover %d\n",slot,state.step.x,state.step.y,state.visible,shelter);fflush(stdout);
    }
}

static uint32_t phase0(uint32_t slot) { return slot * 7919u; }
// 0.5.201: Justin: a bot's bazooka shot looked like several rockets, and bots
// killed many teammates. Bots fire by pressing the trigger every other frame
// (right for rifles: a semi-automatic wants each shot its own press); a
// launcher now gets one press, then nothing for 2.5 s, and never at someone
// closer than 300 (its own blast) or with a teammate within 300 of the target.
// Any gun: no shot while a teammate stands in the line of fire (within 30
// units of it, plus 3 per 100 of distance for the spread, between him and the
// target); he steps aside instead.
static bool BotHoldsLauncher(uint8_t* base, uint32_t record)
{
    const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
    if (def < 0x40000000u || def >= 0xA0000000u) return false;
    const uint32_t text = Guest::Read32(base, def + 8);
    if (text < 0x40000000u || text >= 0xA0000000u) return false;
    char name[32] = {};
    for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
    return strstr(name, "bazooka") || strstr(name, "panzerschreck") || strstr(name, "panzerfaust") || strstr(name, "_rg");
}
// 0.5.224: a shotgun in hand (Justin: shotgun kits should play up close and
// aggressive).
static bool BotHoldsShotgun(uint8_t* base, uint32_t record)
{
    const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
    if (def < 0x40000000u || def >= 0xA0000000u) return false;
    const uint32_t text = Guest::Read32(base, def + 8);
    if (text < 0x40000000u || text >= 0xA0000000u) return false;
    char name[32] = {};
    for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
    return strstr(name, "shotgun") || strstr(name, "trench");
}
// A living teammate's chest, for each one but him (players and bots alike).
template<class F> static void ForEachMate(uint8_t* base, uint32_t self, uint32_t mySide, F&& f)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t o = 0; entities && o < ActiveSlots(); o++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
        if (!e || e == self || Side(base, e) != mySide || mySide == 0) continue;
        const uint32_t r = Guest::Read32(base, e + 512);
        if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0 || Guest::Read32(base, r + 1924) != 3) continue;
        float eye = ReadFloat(base, r + 220);
        if (!std::isfinite(eye) || eye < 4 || eye > 80) eye = 58;
        f(o, ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8) + eye * 0.7f);
    }
}
static bool MateInLine(uint8_t* base, uint32_t self, uint32_t mySide, const float me[3], const float target[3], uint32_t& who)
{
    const float sx = target[0] - me[0], sy = target[1] - me[1], sz = target[2] - me[2];
    const float len2 = sx * sx + sy * sy + sz * sz;
    if (len2 < 1.0f) return false;
    bool blocked = false;
    ForEachMate(base, self, mySide, [&](uint32_t o, float x, float y, float z) {
        if (blocked) return;
        const float t = ((x - me[0]) * sx + (y - me[1]) * sy + (z - me[2]) * sz) / len2;
        if (t <= 0.02f || t >= 1.02f) return;
        const float px = me[0] + sx * t - x, py = me[1] + sy * t - y, pz = me[2] + sz * t - z;
        const float along = sqrtf(len2) * t;
        if (px * px + py * py + pz * pz * 0.5f < powf(30.0f + along * 0.03f, 2.0f)) { blocked = true; who = o; }
    });
    return blocked;
}
static bool MateNear(uint8_t* base, uint32_t self, uint32_t mySide, const float at[3], float radius)
{
    bool near_ = false;
    ForEachMate(base, self, mySide, [&](uint32_t, float x, float y, float z) {
        if (hypotf(x - at[0], y - at[1]) < radius && fabsf(z - at[2]) < 160.0f) near_ = true;
    });
    return near_;
}
static uint32_t g_nextRocket[MaxClients] = {}, g_sidestepUntil[MaxClients] = {};
static int8_t g_sidestepDir[MaxClients] = {};
// 0.5.326 (Justin: bots should use their equipment, rifle grenades too):
// a bot whose kit owns a rifle grenade (m1garand_rg / k98_rg: the
// rifleman's and the Demolitionist's special, a weapon of its own) switches
// to it for a shot at someone 400 to 2200 units off he can see, not onto
// teammates, then back to his gun (the command's weapon word, as the host's
// switch). The launcher rules (aimed, one press, no team kills) fire it; the
// aim is lofted with the distance. Every 10 to 16 s, the Demolitionist every
// 6 to 9 s. Logged "bots: slot N fires a rifle grenade".
static uint32_t g_rgUntil[MaxClients] = {}, g_nextRg[MaxClients] = {}, g_rgWeapon[MaxClients] = {};
// 0.5.330: the gun a bot's own logic wants up (Rusher 2's Sten or Bren; 0:
// none), asked for in his commands; and whether he holds a special (rifle
// grenade, mine, pack, binoculars): then his commands ask for what he holds.
// 329's log: Rusher 2 asked for the Sten every 4 s and kept the Bren, and the
// rifle grenades were taken out and put away at once: the commands always
// asked for the first gun of his two slots (ArmedPrimary/PreferPrimary).
static uint32_t g_wantGun[MaxClients] = {};
// 0.5.332: a weapon his logic is switching him to (the script's switch by
// name takes a moment): his commands ask for that one meanwhile. 331's log:
// Rusher 2's binoculars and the rifle grenades were switched to, but the
// commands kept asking for the gun in his hands, so the switch never
// finished (no binocular fire event, no barrage, the rounds unspent).
static uint32_t g_switchTo[MaxClients] = {}, g_switchUntil[MaxClients] = {};
static bool BotHoldsSpecial(uint8_t* base, uint32_t record)
{
    const uint32_t def = HeldDef(base, record);
    const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
    if (text < 0x40000000u || text >= 0xA0000000u) return false;
    char name[32] = {};
    for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
    return strstr(name, "_rg") || strstr(name, "mp_mine") || strstr(name, "mp_ammo") || strstr(name, "mp_revive") || strstr(name, "binoculars");
}
static bool BotIsDemolitionist(uint32_t slot) { return slot < MaxClients && g_botLoadoutAssigned[slot] && strcmp(g_botKit[slot].label, "demolitionist") == 0; }
static int BotWeaponRounds(uint8_t* base, uint32_t record, uint32_t n)
{
    if (n == 0 || n >= 128 || !record) return -1;
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t def = list ? Guest::Read32(base, list + 4 * n) : 0;
    if (def < 0x40000000u || def >= 0xA0000000u) return -1;
    if (!(Guest::Read32(base, record + 1056 + 4 * (n >> 5)) & (1u << (n & 31)))) return -1;
    const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
    if (clip > 128 || spare > 128) return -1;
    return int(Guest::Read32(base, record + 4 * (172 + clip))) + int(Guest::Read32(base, record + 4 * (80 + spare)));
}
// his owned rifle grenade with rounds left (0: none)
static uint32_t BotRifleGrenade(uint8_t* base, uint32_t record)
{
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    for (int32_t n = 1; list && record && n < 128; n++)
    {
        const uint32_t def = Guest::Read32(base, list + 4 * uint32_t(n));
        const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
        if (text < 0x40000000u || text >= 0xC0000000u) continue;   // (0.5.757: copies' names lie at 0xAA..)
        char name[32] = {};
        for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
        if (!strstr(name, "_rg")) continue;
        if (BotWeaponRounds(base, record, uint32_t(n)) > 0) return uint32_t(n);
    }
    return 0;
}
static bool BotHoldsRifleGrenade(uint8_t* base, uint32_t record)
{
    const uint32_t def = HeldDef(base, record);
    const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
    if (text < 0x40000000u || text >= 0xA0000000u) return false;
    char name[32] = {};
    for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
    return strstr(name, "_rg") != nullptr;
}

namespace { uint32_t WeaponNumber(uint8_t* base, const char* name); }   // (below)
static bool BotSwitch(PPCContext& ctx, uint8_t* base, uint32_t self, const char* weapon);   // (below)
bool BotsGroundZ(PPCContext& ctx, uint8_t* base, uint32_t ignore, float x, float y, float top, float& z);   // (below)
bool BotsLineClear(PPCContext& ctx, uint8_t* base, uint32_t ignore, float ax, float ay, float az, float bx, float by, float bz);   // (below)
// 0.5.457: a weapon he owns with rounds left (clip or spare): its number, else 0
static uint32_t BotOwnsLoaded(uint8_t* base, uint32_t record, const char* name)
{
    const uint32_t w = WeaponNumber(base, name);
    if (!w || w >= 128 || !((Guest::Read32(base, record + 1056 + 4 * (w >> 5)) >> (w & 31)) & 1u)) return 0;
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    const uint32_t def = list ? Guest::Read32(base, list + 4 * w) : 0;
    if (def < 0x40000000u || def >= 0xA0000000u) return 0;
    const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
    const int32_t inClip = clip < 128 ? int32_t(Guest::Read32(base, record + 4 * (172 + clip))) : 0;
    const int32_t inSpare = spare < 128 ? int32_t(Guest::Read32(base, record + 4 * (80 + spare))) : 0;
    return inClip > 0 || inSpare > 0 ? w : 0;
}
// 0.5.541: cover round a side's own flag for its defenders (user: "defender
// bots to take up cover in buildings or behind walls waiting and scanning the
// horizon for enemies who may come near the flag"). Ground spots 220 to 700
// from the flag, 16 a ring; a spot is cover with walls within 96 in 2 to 6 of
// 8 directions (not open, not shut in), better seeing the flag and the way
// the enemy comes (toward their flag), at least 200 apart. Once a map.
static const std::vector<BotNavigation::Point>& CtfCoverSpots(PPCContext& ctx, uint8_t* base, uint32_t self, uint32_t side,
                                                              const float* flag, const float* enemyFlag)
{
    static std::vector<BotNavigation::Point> spots[3];
    static float madeFor[3][2] = {};
    static bool made[3] = {};
    if (side > 2) side = 0;
    if (made[side] && hypotf(madeFor[side][0] - flag[0], madeFor[side][1] - flag[1]) < 1.0f) return spots[side];
    made[side] = true; spots[side].clear(); madeFor[side][0] = flag[0]; madeFor[side][1] = flag[1];
    struct Candidate { BotNavigation::Point p; int score; };
    std::vector<Candidate> found;
    const float wx = enemyFlag[0] - flag[0], wy = enemyFlag[1] - flag[1], wl = fmaxf(1.0f, hypotf(wx, wy));
    const float ax = flag[0] + wx / wl * 600.0f, ay = flag[1] + wy / wl * 600.0f;   // the way they come
    for (float radius : { 220.0f, 340.0f, 460.0f, 580.0f, 700.0f })
        for (int k = 0; k < 16; k++)
        {
            const float a = float(k) * 0.3926991f + radius * 0.01f;
            const float x = flag[0] + cosf(a) * radius, y = flag[1] + sinf(a) * radius;
            float z;
            if (!BotsGroundZ(ctx, base, self, x, y, flag[2] + 160.0f, z) || fabsf(z - flag[2]) > 160.0f) continue;
            int walls = 0;
            for (int d = 0; d < 8; d++)
            {
                const float da = float(d) * 0.7853982f;
                if (!BotsLineClear(ctx, base, self, x, y, z + 40.0f, x + cosf(da) * 96.0f, y + sinf(da) * 96.0f, z + 40.0f)) walls++;
            }
            if (walls < 2 || walls > 6) continue;
            const bool seesFlag = BotsLineClear(ctx, base, self, x, y, z + 60.0f, flag[0], flag[1], flag[2] + 40.0f);
            const bool seesWay = BotsLineClear(ctx, base, self, x, y, z + 60.0f, ax, ay, flag[2] + 60.0f);
            if (!seesFlag && !seesWay) continue;
            found.push_back({ { x, y, z }, walls + (seesFlag ? 2 : 0) + (seesWay ? 3 : 0) + (g_mapKnow.Nearest({ x, y, z }, 200.0f) >= 0 ? 2 : 0) });
        }
    std::stable_sort(found.begin(), found.end(), [](const Candidate& a, const Candidate& b) { return a.score > b.score; });
    for (const Candidate& c : found)
    {
        bool apart = true;
        for (const auto& s : spots[side]) if (hypotf(s.x - c.p.x, s.y - c.p.y) < 200.0f) apart = false;
        if (apart) spots[side].push_back(c.p);
        if (spots[side].size() >= 12) break;
    }
    printf("bots: side %u's flag (%.0f %.0f): %zu cover spots for its defenders (of %zu candidates)\n", side, flag[0], flag[1], spots[side].size(), found.size());
    fflush(stdout);
    return spots[side];
}

static BotOrders Think(PPCContext& ctx, uint8_t* base, uint32_t slot, uint32_t now)
{
    BotOrders orders = {};
    g_seeDist[slot] = 1.0e9f;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    if (!record) return orders;
    const int32_t health = int32_t(Guest::Read32(base, record + 304));
    if (health <= 0)
    {
        // Dead: tap the use button now and then, which asks to respawn.
        orders.buttons = (now / 500) % 2 ? 0x20 : 0;
        g_crouched[slot] = false;   // he comes back standing
        g_stanceTaps[slot] = 0;
        ResetBotStance(slot);
        g_charging[slot] = false;
        g_tactical[slot] = {};
        return orders;
    }
    if (g_tactical[slot].entity != self) {
        g_tactical[slot] = {};g_tactical[slot].entity=self;g_tactical[slot].hand=slot%2?1:-1;
    }
    // 0.5.166: from his own eyes (+220, lower when he crouches or lies),
    // not always 60 above his feet.
    const float myEye = BotAim::EyeHeight(ReadFloat(base, record + 220));
    const float me[3] = { ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) + myEye };
    // His side, kept in both places the session's switch sets (the team
    // record's +4 and his session player's +604): the game's spawn puts the
    // host's side back on him (his session player is a copy of the host's).
    if (const uint32_t player = SessionPlayer(base, slot))
    {
        const uint32_t want = BotSide(base, slot);
        const uint32_t team = Guest::Read32(base, self + 520);
        if (team != 0 && (Guest::Read32(base, team + 4) != want || Guest::Read16(base, player + 604) != want))
        {
            static uint32_t told[MaxClients] = {};
            if (told[slot]++ < 5)
            {
                printf("bots: slot %u back to side %u (team %u, session %u)\n", slot, want, Guest::Read32(base, team + 4),
                       Guest::Read16(base, player + 604));
                fflush(stdout);
            }
            Guest::Write32(base, team + 4, want);
            Guest::Write16(base, player + 604, uint16_t(want));
        }
    }
    {
        int pokeOffset = 0, pokeValue = 0;
        DebugProbe::BotPoke(pokeOffset, pokeValue);
        if (pokeOffset > 0 && pokeOffset < 2400) Guest::Write8(base, record + pokeOffset, uint8_t(pokeValue));
    }
    const uint32_t mySide = Side(base, self);

    float best = 1e30f, bestRank = 1e30f, target[3] = {};
    bool found = false;
    uint32_t targetSlot = 0xFFFFFFFFu;
    uint32_t nearSlot[3] = { 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu };
    float nearDist[3] = { 1e30f, 1e30f, 1e30f }, nearAt[3][3] = {};
    for (uint32_t other = 0; other < ActiveSlots(); other++)
    {
        if (other == slot) continue;
        if (other == g_ignoreSlot[slot] && now < g_ignoreUntil[slot]) continue;   // 0.5.198: out of reach through a floor
        const uint32_t entity = Guest::Read32(base, entities + 4 * (other + 1));
        const uint32_t theirs = entity ? Guest::Read32(base, entity + 512) : 0;
        if (!theirs) continue;
        if (int32_t(Guest::Read32(base, theirs + 304)) <= 0) continue;
        if (Guest::Read32(base, theirs + 1924) != 3) continue;   // in the game, not watching or dead
        const uint32_t side = Side(base, entity);
        if (side == mySide && side != 0) continue;
        // 0.5.166: Justin: the bots should shoot him when he lies down.
        // A prone or crouched player's eyes (+220) are low (about 10 and
        // 40); anything under 20 was taken as a mistake and replaced by a
        // standing player's 58, and the aim never went under 24: they shot
        // over a prone player. Now his real eye height, a little under it
        // (the head and shoulders), down to 6.
        float eye=ReadFloat(base,theirs+220);
        if (!std::isfinite(eye)||eye<4||eye>80) eye=58;
        // 0.5.456: the body, waist to upper chest (Justin: "aim more from the
        // waist to the upper chest area with less being killed immediately by
        // headshots"; was eye height less 2: the head): 45% to 75% of his
        // eye height, drifting every 0.7 s, each bot on his own rhythm
        const float sway=0.45f+0.30f*(0.5f+0.5f*sinf(float(now)*0.009f+float(slot)*1.7f+float(other)*0.9f));
        const float height=std::clamp(eye*sway,4.0f,60.0f);
        const float at[3] = { ReadFloat(base, theirs), ReadFloat(base, theirs + 4), ReadFloat(base, theirs + 8) + height };
        const float dx = at[0] - me[0], dy = at[1] - me[1], dz = at[2] - me[2];
        const float distance = dx * dx + dy * dy + dz * dz;
        // 0.5.195: one he has a clear line to counts as much nearer.
        float rank = (other == g_preferSlot[slot] && now < g_preferUntil[slot]) ? distance * 0.25f : distance;
        // 0.5.200: already chased by two or more teammates: he counts as
        // farther for each one past the first (the others pick someone else).
        {
            int chasers = 0;
            for (uint32_t b = FirstBotSlot(); b < ActiveSlots(); b++) if (b != slot && g_bot[b] && g_targetOf[b] == other && BotSide(base, b) == BotSide(base, slot)) chasers++;
            if (chasers > 1 && !(other == g_preferSlot[slot] && now < g_preferUntil[slot])) rank *= 1.0f + 0.8f * float(chasers - 1);
        }
        if (rank < bestRank) { bestRank = rank; best = distance; memcpy(target, at, sizeof(target)); found = true; targetSlot = other; }
        for (int k = 0; k < 3; k++)
            if (distance < nearDist[k])
            {
                for (int m = 2; m > k; m--) { nearDist[m] = nearDist[m - 1]; nearSlot[m] = nearSlot[m - 1]; memcpy(nearAt[m], nearAt[m - 1], 12); }
                nearDist[k] = distance; nearSlot[k] = other; memcpy(nearAt[k], at, 12);
                break;
            }
    }

    g_targetOf[slot] = found ? targetSlot : 0xFFFFFFFFu;   // 0.5.200
    uint32_t threat=found?Guest::Read32(base,entities+4*(targetSlot+1)):0;
    auto& tactics=g_tactical[slot];
    bool visible=found;
    if (g_botTactics.load() && found) {
        if (now-tactics.checked>=200 || tactics.targetSlot!=targetSlot) {
            bool before=tactics.visible;
            auto hit=BotTrace(ctx,base,self,self,{me[0],me[1],me[2]}, {target[0],target[1],target[2]},false);
            tactics.visible=hit.valid&&!hit.solid&&(hit.fraction>=0.97f || hit.handle==Guest::Read32(base,threat+484));
            if (tactics.visible && (!before || tactics.targetSlot!=targetSlot)) tactics.sightSince=now;
            tactics.targetSlot=targetSlot;tactics.checked=now;
        }
        visible=tactics.visible;
    }
    // 0.5.195: the one he chose is behind a wall: is one of the next nearest
    // in sight? Then he fights that one (and keeps to him a while).
    if (g_botTactics.load() && found && !visible && now - g_preferChecked[slot] >= 300)
    {
        g_preferChecked[slot] = now;
        for (int k = 0; k < 3; k++)
        {
            if (nearSlot[k] >= MaxClients || nearSlot[k] == targetSlot) continue;
            const uint32_t other = Guest::Read32(base, entities + 4 * (nearSlot[k] + 1));
            if (!other) continue;
            auto hit = BotTrace(ctx, base, self, self, {me[0], me[1], me[2]}, {nearAt[k][0], nearAt[k][1], nearAt[k][2]}, false);
            if (!hit.valid || hit.solid || !(hit.fraction >= 0.97f || hit.handle == Guest::Read32(base, other + 484))) continue;
            targetSlot = nearSlot[k]; best = nearDist[k]; memcpy(target, nearAt[k], 12);
            threat = other; visible = true;
            tactics.visible = true; tactics.targetSlot = targetSlot; tactics.sightSince = now; tactics.checked = now;
            g_preferSlot[slot] = targetSlot; g_preferUntil[slot] = now + 1500;
            break;
        }
    }
    if (found && visible) { g_preferSlot[slot] = targetSlot; g_preferUntil[slot] = now + 1500; }
    // 0.5.198: Justin's 0.5.197 log: slot 6 stood five minutes on a floor
    // with his enemy 30 to 140 units away below it, looking straight down,
    // never in sight. An enemy close across but a floor away (90+ up or
    // down) and unseen for 6 s is left alone for 20 s.
    {
        static uint32_t floorSince[MaxClients] = {};
        const float ax = target[0] - me[0], ay = target[1] - me[1];
        if (found && !visible && hypotf(ax, ay) < 300.0f && fabsf(target[2] - me[2]) > 90.0f)
        {
            if (!floorSince[slot]) floorSince[slot] = now;
            if (now - floorSince[slot] > 6000)
            {
                g_ignoreSlot[slot] = targetSlot; g_ignoreUntil[slot] = now + 20000; floorSince[slot] = 0;
                printf("bots: slot %u leaves slot %u alone: %.0f %s him through a floor\n", slot, targetSlot, fabsf(target[2] - me[2]),
                       target[2] > me[2] ? "below" : "above");
                fflush(stdout);
            }
        }
        else floorSince[slot] = 0;
    }
    // 0.5.195: tracking: where (and how high) he was last seen. Out of sight
    // for under 3 s, he is looked for there, up or down, not through the wall.
    if (found && visible) { memcpy(g_lastSeenAt[slot], target, 12); g_lastSeenSlot[slot] = targetSlot; g_lastSeenTime[slot] = now; }
    else if (found && g_lastSeenSlot[slot] == targetSlot && now - g_lastSeenTime[slot] < 3000) memcpy(target, g_lastSeenAt[slot], 12);
    float enemyAt[3] = {};
    memcpy(enemyAt, target, 12);
    // 0.5.170: for the melee, smoke (BotExtras).
    if (found && visible) { const float ex = target[0] - me[0], ey = target[1] - me[1]; g_seeDist[slot] = sqrtf(ex * ex + ey * ey); }

    // His view: +204 pitch, +208 yaw (what the server's fire shoots along,
    // sub_8256E978).
    const float viewPitch = BotAim::SignedPitch(ReadFloat(base, record + 204)), viewYaw = ReadFloat(base, record + 208);
    // Once approaching/attached, face the ladder before combat aiming can
    // turn the bot away from it. Native forward input controls climbing.
    if(g_botTactics.load() && (((Guest::Read32(base,record+44)&0x10u)!=0 && now>=tactics.ladderCooldown) || now<tactics.ladderUntil)) {
        const float ladderGoal[3]={me[0],me[1],me[2]+128};
        orders.forward=127;
        TacticalMove(ctx,base,self,0,slot,now,health,viewYaw,ladderGoal,false,orders);
        if(tactics.climbing){g_why[slot] = "ladder"; return orders;}
    }


    // 0.5.88: the objective. In a Domination the flag his side should take
    // or hold (dom.cpp); he goes for it unless an enemy is near (900 units).
    float goal[3] = {};
    bool atFlag = false;
    bool hasGoal = Domination::BotGoal(mySide, slot, me, goal, atFlag);
    // 0.5.151: in War, the flag in play (both sides go for it: taking it
    // pushes the line on, holding it keeps it).
    // 0.5.165: Justin, on 0.5.164 with 20 bots: most stood still. They all
    // went to War's one flag and held it crouched. Now about a third of them
    // (every third slot) hunt instead, the rest play the flag.
    // 0.5.168: by role: only the objective players go for it.
    // 0.5.205: Justin, on 0.5.204: "improve the logic to play more
    // objectively if in an objective type game mode". 0.5.200 sent only the
    // first four objective players of a side to War's flag, and to a ring
    // 120-360 out: nobody stood on it, so it was seldom taken. Now a share of
    // each side (40%, 3 to 7 bots: objective players first, then rushers,
    // then flankers; campers keep their posts) plays the flag in play, in
    // two jobs: takers (every other one) onto the flag itself, within 90
    // units, where the capture counts; screens 200-450 out on the side the
    // enemy comes from (towards his spawns), each at his own angle, to catch
    // them before they reach it. Takers keep going under fire from afar.
    bool warTaker = false;
    if (!hasGoal && g_war.valid && GetTickCount() - g_war.when < 10000 && g_role[slot] != RoleCamper)
    {
        const uint32_t side = BotSide(base, slot);
        int sideBots = 0, rank = -1, order = 0;
        for (uint32_t b = FirstBotSlot(); b < ActiveSlots(); b++) if (g_bot[b] && BotSide(base, b) == side) sideBots++;
        for (uint8_t role : { uint8_t(RoleObjective), uint8_t(RoleRusher), uint8_t(RoleFlanker) })
            for (uint32_t b = FirstBotSlot(); b < ActiveSlots(); b++)
                if (g_bot[b] && g_role[b] == role && BotSide(base, b) == side) { if (b == slot) rank = order; order++; }
        int quota = (sideBots * 14 + 10) / 20;   // 0.5.471: 70% of the side (0.5.277: 55%; was 40%, at most 7)
        if (quota < 3) quota = 3;
        if (quota > 14) quota = 14;   // 0.5.471: 14 (was 11)
        if (rank >= 0 && rank < quota)
        {
            memcpy(goal, g_war.at, sizeof(goal));
            warTaker = rank % 3 != 2;   // 0.5.471: two in three onto the flag (was one in two)
            BotNavigation::Point enemySpawn;
            const bool knowEnemy = SpawnCentre(side == 1 ? 2 : 1, enemySpawn);
            const float facing = knowEnemy ? atan2f(enemySpawn.y - goal[1], enemySpawn.x - goal[0]) : float(slot) * 2.39996323f;
            if (warTaker)
            {
                const float angle = float(slot) * 2.39996323f, radius = 20.0f + float((slot * 13u) % 70u);
                goal[0] += cosf(angle) * radius; goal[1] += sinf(angle) * radius;
            }
            else
            {
                const float spread = (float((rank / 2) % 4) - 1.5f) * 0.55f;   // about -47 to +47 degrees
                const float radius = 200.0f + float((slot * 37u) % 250u);
                goal[0] += cosf(facing + spread) * radius; goal[1] += sinf(facing + spread) * radius;
            }
            const float gx = goal[0] - me[0], gy = goal[1] - me[1];
            atFlag = gx * gx + gy * gy < 70.0f * 70.0f && fabsf(goal[2] - me[2]) < 120.0f;
            hasGoal = true;
            static uint32_t told[MaxClients] = {};
            if (GetTickCount() - told[slot] > 20000)
            {
                told[slot] = GetTickCount();
                printf("bots: slot %u plays War's flag %u as a %s (%d of %d on side %u's flag team), going to %.0f %.0f\n", slot,
                       g_war.index, warTaker ? "taker" : "screen", rank + 1, quota, side, goal[0], goal[1]);
                fflush(stdout);
            }
        }
    }
    // 0.5.463: the other objective modes (WatchObjectives). With two flags or
    // more (Capture the Flag): his side's is the one nearest his spawn, the
    // enemy's the one nearest theirs; objective players and rushers go for the
    // enemy's, and with it on them (it is within 90: carried) back to their
    // own to bring it in; one flanker in three stays by his own flag (300 out,
    // his own angle). With one (Single Capture the Flag, Headquarters' radio):
    // every one but the campers goes to it, each to his own place by it.
    g_ctfOwnLegs[slot] = false; g_ctfHold[slot] = false; g_ctfAttacker[slot] = false;   // 0.5.541: set again below while in CTF
    g_ctfDoing[slot] = nullptr;
    g_ctfLane[slot] = 0;
    if (!hasGoal && g_obj.valid && GetTickCount() - g_obj.when < 10000 && (g_role[slot] != RoleCamper || g_obj.mode.find("ctf") != std::string::npos))
    {
        const uint32_t side = BotSide(base, slot);
        BotNavigation::Point mine, theirs;
        const bool spawns = SpawnCentre(side, mine) && SpawnCentre(side == 1 ? 2 : 1, theirs);
        auto nearest = [&](const BotNavigation::Point& to) {
            int best = 0; float bestD = 1e30f;
            for (int k = 0; k < g_obj.count; k++) { const float d = hypotf(g_obj.at[k][0] - to.x, g_obj.at[k][1] - to.y); if (d < bestD) { bestD = d; best = k; } }
            return best;
        };
        // 0.5.480: in CTF, only once the sides are known from real spawns
        // (CtfSpawnSides); g_spawns' centres had the flags swapped
        const bool ctfKnown = side >= 1 && side <= 2 && g_ctfOwn[side] >= 0 && g_ctfOwn[side] < g_obj.count && g_ctfOwn[3 - side] >= 0 && g_ctfOwn[3 - side] < g_obj.count;
        const bool ctfWait = g_obj.mode.find("ctf") != std::string::npos && g_obj.count == 2 && !ctfKnown;
        if (ctfWait) {}
        else if (g_obj.count >= 2 && (spawns || ctfKnown))
        {
            int own = spawns ? nearest(mine) : 0, enemy = spawns ? nearest(theirs) : 1;
            if (side <= 2 && g_ctfOwn[side] >= 0 && g_ctfOwn[side] < g_obj.count && g_ctfOwn[3 - side] >= 0 && g_ctfOwn[3 - side] < g_obj.count)
            { own = g_ctfOwn[side]; enemy = g_ctfOwn[3 - side]; }   // 0.5.475
            if (own != enemy)
            {
                // 0.5.473: carrying kept for the life once he has been on the
                // enemy flag (within 60; the flag is taken by touch), till he
                // is home (within 90 of his own) or dies: 463's test (within
                // 90 of the enemy base) turned a carrier round as he left it
                // 0.5.482: carrying from the game itself (FlagEntityWatch: the
                // enemy flag hidden with him as its carrier); was guessed from
                // standing near it
                const bool carrying = own <= 1 && enemy <= 1 && g_flagCarrier[enemy] == int(slot);
                float ownAway[3], enemyAway[3];
                const bool ownOut = own <= 1 && FlagAway(base, own, ownAway);
                const bool enemyOut = enemy <= 1 && FlagAway(base, enemy, enemyAway) && !carrying;
                // 0.5.484: CTF roles by order within his team, whatever his
                // kit or class (user: "half bots pushing forward half bots
                // defending ... half stay back and push to the middle / stay
                // near spawn defending the flag and others ... push the flag
                // and focus on getting it home; others who notice our flag
                // has been picked up need to escort the flag carrier home"):
                // even ranks attack; odd ranks defend, every other one in the
                // middle, the rest by his own flag
                uint32_t rank = 0;
                for (uint32_t o = 0; o < slot && o < MaxClients; o++) if (g_bot[o] && BotSide(base, o) == side) rank++;
                const bool attacker = rank % 2 == 0, middle = !attacker && rank % 4 == 1;
                g_ctfAttacker[slot] = attacker;   // 0.5.536: for AttackerRoute
                const bool defender = !attacker;
                const float spread = float(slot) * 2.39996323f;   // his own angle, so they do not bunch
                memcpy(goal, (carrying || defender) ? g_obj.at[own] : g_obj.at[enemy], sizeof(goal));
                const char* doing = carrying ? "brings the flag home" : attacker ? "goes for the enemy flag" : middle ? "holds the middle" : "guards his flag";
                const int mate = enemy <= 1 ? g_flagCarrier[enemy] : -1;
                // 0.5.541: a team mate's spot if he is alive and playing
                auto aliveAt = [&](uint32_t o, float* at) -> bool {
                    const uint32_t e = (entities && o < MaxClients) ? Guest::Read32(base, entities + 4 * (o + 1)) : 0;
                    const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
                    if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0 || Guest::Read32(base, r + 1924) != 3) return false;
                    at[0] = ReadFloat(base, r); at[1] = ReadFloat(base, r + 4); at[2] = ReadFloat(base, r + 8);
                    return true;
                };
                const float axX = g_obj.at[enemy][0] - g_obj.at[own][0], axY = g_obj.at[enemy][1] - g_obj.at[own][1];
                const float axL = fmaxf(1.0f, hypotf(axX, axY)), ux = axX / axL, uy = axY / axL;
                // 0.5.541: (user: "friendly bots protecting the flag carrier on
                // his journey home") every attacker, and a middle defender within
                // 1500 of him, takes a place in a formation round the carrier
                // (the host too), facing home: a point man 260 ahead, flanks 190
                // out to either side, two behind. Not the long way round.
                float carrierAt[3];
                const bool mateAlive = mate >= 0 && mate != int(slot) && aliveAt(uint32_t(mate), carrierAt);
                const bool escort = mateAlive && (attacker || (middle && hypotf(carrierAt[0] - me[0], carrierAt[1] - me[1]) < 1500.0f));
                if (carrying) {}
                // 0.5.485: to a DROPPED flag only some go (484: 98 + 101 runs
                // to one lying flag, the user: "alot of bots still crowding"):
                // the middle defenders to their own, half the attackers to the
                // enemy's; a carried flag still has every defender after it
                else if (defender && ownOut && (middle || !g_flagDropped[own]))
                {
                    // our flag taken or lying out: every defender after it
                    memcpy(goal, ownAway, sizeof(goal));
                    doing = g_flagDropped[own] ? "returns his dropped flag" : "hunts the enemy carrying his flag";
                }
                // 0.5.598: a standoff (our carrier home, within 600 of our flag,
                // but our flag out): two attackers stay by him, the rest hunt the
                // enemy carrying our flag (a capture needs it back). 597's
                // self-test: both flags carried from 2 minutes in to the end,
                // side 1's carrier slot 6 sat 7 minutes at his flag with every
                // attacker of his round him and only the defenders after theirs.
                else if (escort && attacker && ownOut && (rank / 2) % 6 >= 2 &&
                         hypotf(carrierAt[0] - g_obj.at[own][0], carrierAt[1] - g_obj.at[own][1]) < 600.0f)
                {
                    memcpy(goal, ownAway, sizeof(goal));
                    doing = g_flagDropped[own] ? "returns his dropped flag" : "hunts the enemy carrying his flag (standoff)";
                }
                else if (escort)
                {
                    float hx = g_obj.at[own][0] - carrierAt[0], hy = g_obj.at[own][1] - carrierAt[1];
                    const float hl = fmaxf(1.0f, hypotf(hx, hy)); hx /= hl; hy /= hl;
                    static const float turn[6] = { 0.0f, 1.25f, -1.25f, 2.5f, -2.5f, 3.14159f };
                    static const float out[6] = { 260.0f, 190.0f, 190.0f, 210.0f, 210.0f, 230.0f };
                    const uint32_t place = (attacker ? rank / 2 : rank / 4 + 3) % 6;
                    const float a = atan2f(hy, hx) + turn[place];
                    goal[0] = carrierAt[0] + cosf(a) * out[place]; goal[1] = carrierAt[1] + sinf(a) * out[place]; goal[2] = carrierAt[2];
                    doing = place == 0 ? "escorts the flag carrier (point)" : place < 3 ? "escorts the flag carrier (flank)" : "escorts the flag carrier (rear)";
                    g_ctfOwnLegs[slot] = true;
                }
                else if (attacker && enemyOut && rank % 4 == 0) { memcpy(goal, enemyAway, sizeof(goal)); doing = "goes for the dropped enemy flag"; }
                else if (attacker)
                {
                    // 0.5.541: (user: "make the aggressive bots really push for
                    // the flag as a unit") a loose line across the way (140
                    // apart) until 700 from their flag; one more than 450 ahead
                    // of his squad's average (along the way between the flags)
                    // waits for them, at most 6 s in every 20.
                    const float myProgress = (me[0] - g_obj.at[own][0]) * ux + (me[1] - g_obj.at[own][1]) * uy;
                    const float toEnemy = hypotf(g_obj.at[enemy][0] - me[0], g_obj.at[enemy][1] - me[1]);
                    const float lateral = (float((rank / 2) % 3) - 1.0f) * 140.0f;
                    g_ctfLane[slot] = int((rank / 2) % 3) - 1;   // 0.5.564: his lane, for the route
                    if (toEnemy > 700.0f) { goal[0] += -uy * lateral; goal[1] += ux * lateral; }
                    float sum = 0.0f; int squad = 0;
                    for (uint32_t o = 0; o < MaxClients; o++)
                    {
                        float at[3];
                        if (o == slot || !g_bot[o] || !g_ctfAttacker[o] || BotSide(base, o) != side || !aliveAt(o, at)) continue;
                        // 0.5.561: his squad is the attackers within 1200 of him,
                        // not those still in base or just spawned there (560's
                        // self-test: 82 of 554 mid-map samples were a leader
                        // waiting, held back by team mates at home)
                        if (hypotf(at[0] - me[0], at[1] - me[1]) > 1200.0f) continue;
                        sum += (at[0] - g_obj.at[own][0]) * ux + (at[1] - g_obj.at[own][1]) * uy; squad++;
                    }
                    static uint32_t waitFrom[MaxClients] = {};
                    if (squad >= 2 && toEnemy > 700.0f && myProgress > sum / float(squad) + 450.0f)
                    {
                        if (!waitFrom[slot] || now - waitFrom[slot] > 20000) waitFrom[slot] = now;
                        if (now - waitFrom[slot] < 6000)
                        {
                            // 0.5.562: he holds where he is, crouched, watching
                            // the way ahead (was: a goal 200 ahead of the squad's
                            // average, behind him). 561's self-test: the moved goal
                            // made his route be worked out again backwards, and
                            // again forwards (often another way) when the wait
                            // ended: slot 10 turned back from -603 -1409 to
                            // -1232 -487 so. His goal and route stay as they are.
                            atFlag = true;
                            g_ctfHold[slot] = true;
                            g_ctfWatchYaw[slot] = atan2f(g_obj.at[enemy][1] - me[1], g_obj.at[enemy][0] - me[0]) * 57.29578f;
                            doing = "waits for his squad";
                            g_ctfOwnLegs[slot] = true;
                        }
                    }
                    // 0.5.567: the regroup before the assault. 565 and 566's
                    // 10-minute self-tests: no attacker in 35-36 lives came within
                    // 700 of the enemy flag; they reached it one or two at a time
                    // and were killed 1000-1500 out by the many there (attackers
                    // leaving base, middle holders), running on or not. Coming into
                    // 1700-2300 of the enemy flag he holds (crouched, watching)
                    // until three of his side's attackers are in the wave (1500-2600
                    // out, wherever their lane), or 15 s have gone; then they go in.
                    {
                        static uint32_t stageFrom[MaxClients] = {};
                        static bool pushing[MaxClients] = {};
                        if (toEnemy > 2600.0f) { pushing[slot] = false; stageFrom[slot] = 0; }
                        if (!pushing[slot] && toEnemy > 1700.0f && toEnemy < 2300.0f && !g_ctfHold[slot])
                        {
                            int wave = 1;
                            for (uint32_t o = 0; o < MaxClients; o++)
                            {
                                float at[3];
                                if (o == slot || !g_bot[o] || !g_ctfAttacker[o] || BotSide(base, o) != side || !aliveAt(o, at)) continue;
                                const float d = hypotf(g_obj.at[enemy][0] - at[0], g_obj.at[enemy][1] - at[1]);
                                if (d > 1500.0f && d < 2600.0f) wave++;
                            }
                            if (!stageFrom[slot]) stageFrom[slot] = now;
                            if (wave >= 3 || now - stageFrom[slot] >= 15000)
                            {
                                pushing[slot] = true;
                                static int told = 0;
                                if (told++ < 80) { printf("bots: ctf: slot %u (side %u) goes in: %d of his side's attackers in the wave, after %.1f s\n", slot, side, wave, (now - stageFrom[slot]) / 1000.0f); fflush(stdout); }
                            }
                            else
                            {
                                atFlag = true;
                                g_ctfHold[slot] = true;
                                g_ctfWatchYaw[slot] = atan2f(g_obj.at[enemy][1] - me[1], g_obj.at[enemy][0] - me[0]) * 57.29578f;
                                doing = "regroups for the assault";
                                g_ctfOwnLegs[slot] = true;
                            }
                        }
                    }
                }
                else
                {
                    // 0.5.541: defenders hold a place and watch: the middle ones
                    // between the flags (as before), the others a cover spot by
                    // their flag (CtfCoverSpots); there (within 70) each crouches
                    // and sweeps his view over the way the enemy comes
                    float hold[3] = { goal[0], goal[1], goal[2] };
                    if (middle)
                    {
                        const float mx = (g_obj.at[own][0] + g_obj.at[enemy][0]) * 0.5f, my = (g_obj.at[own][1] + g_obj.at[enemy][1]) * 0.5f;
                        const float across = (float((rank / 4) % 5) - 2.0f) * 300.0f;
                        hold[0] = mx - uy * across - ux * 250.0f; hold[1] = my + ux * across - uy * 250.0f;
                    }
                    else
                    {
                        const std::vector<BotNavigation::Point>& cover = CtfCoverSpots(ctx, base, self, side, g_obj.at[own], g_obj.at[enemy]);
                        if (!cover.empty())
                        {
                            const BotNavigation::Point& c = cover[(rank / 4) % cover.size()];
                            hold[0] = c.x; hold[1] = c.y; hold[2] = c.z;
                            doing = "holds cover by his flag";
                        }
                        else
                        {
                            const float r = 150.0f + float(rank % 4) * 80.0f;
                            hold[0] += cosf(spread) * r; hold[1] += sinf(spread) * r;
                        }
                    }
                    memcpy(goal, hold, sizeof(goal));
                    if (hypotf(hold[0] - me[0], hold[1] - me[1]) < 70.0f && fabsf(hold[2] - me[2]) < 100.0f)
                    {
                        atFlag = true;
                        g_ctfHold[slot] = true;
                        const float wx = g_obj.at[own][0] + ux * 900.0f - me[0], wy = g_obj.at[own][1] + uy * 900.0f - me[1];
                        g_ctfWatchYaw[slot] = atan2f(wy, wx) * 57.29578f;
                    }
                }
                hasGoal = true;
                g_ctfDoing[slot] = doing;   // 0.5.546
                static uint32_t told[MaxClients] = {};
                if (GetTickCount() - told[slot] > 20000)
                {
                    told[slot] = GetTickCount();
                    printf("bots: slot %u plays %s: %s (%s)\n", slot, g_obj.mode.c_str(), doing, g_obj.name[enemy]);
                    fflush(stdout);
                }
            }
        }
        else if (g_obj.count >= 1)
        {
            const int k = g_obj.count == 1 ? 0 : nearest({ me[0], me[1], me[2] });
            memcpy(goal, g_obj.at[k], sizeof(goal));
            hasGoal = true;
            static uint32_t told[MaxClients] = {};
            if (GetTickCount() - told[slot] > 20000)
            {
                told[slot] = GetTickCount();
                printf("bots: slot %u plays %s: goes to the %s\n", slot, g_obj.mode.c_str(), g_obj.name[k]);
                fflush(stdout);
            }
        }
    }
    // 0.5.849: a bot on his way to a vehicle far off: that is his goal, walked by the learnt map's
    // route below like an objective (botveh5: walked straight at it, the one going for a Sherman by the
    // river got stuck round 3800 1250); no ring round it, no long way
    const bool toVehicle = GetTickCount() - g_vehicleGoalAt[slot] < 600u;
    if (toVehicle) { memcpy(goal, g_vehicleGoal[slot], sizeof(goal)); hasGoal = true; atFlag = false; }
    // 0.5.196: each objective player his own place around the flag (40-100
    // units out), so they do not all pile onto one point (Domination).
    if (hasGoal && !atFlag && !toVehicle && !g_war.valid && !(g_obj.valid && GetTickCount() - g_obj.when < 10000))   // 0.5.463: onto a flag itself (it is picked up by touching)
    {
        // 0.5.200: a ring 120 to 360 out (was 40 to 100), each his own angle.
        const float angle = float(slot) * 2.39996323f, radius = 120.0f + float(slot % 4) * 80.0f;
        goal[0] += cosf(angle) * radius; goal[1] += sinf(angle) * radius;
    }
    // 0.5.457: the long way round (Justin: "still some grouping up instead of
    // splitting up even if that means taking the long way to the
    // objectives"): to an objective 1400 or more away, two bots in three go
    // first to a point of their own beside the straight way (half way along,
    // 600 to 1200 off it, left or right by turns, on ground the trace finds
    // near his height), then on to it; done there, or 20 s on.
    const bool carrierNow = g_flagCarrier[0] == int(slot) || g_flagCarrier[1] == int(slot);   // 0.5.483: straight home, no long way
    if (hasGoal && !atFlag && !toVehicle && slot % 3 != 0 && !carrierNow && !g_ctfOwnLegs[slot] && !g_ctfAttacker[slot])   // 0.5.541: a CTF squad or escort keeps together
    {
        static float via[MaxClients][3] = {};
        static uint8_t viaLife[MaxClients] = {};
        static uint32_t viaUntil[MaxClients] = {};
        static bool viaOn[MaxClients] = {};
        const float gx = goal[0] - me[0], gy = goal[1] - me[1];
        const float length = sqrtf(gx * gx + gy * gy);
        if (viaLife[slot] != uint8_t(g_botLife[slot] + 1))
        {
            viaLife[slot] = uint8_t(g_botLife[slot] + 1);
            viaOn[slot] = false;
            if (length > 1400.0f)
            {
                const float side = (slot / 3) % 2 ? 1.0f : -1.0f, off = 600.0f + float((slot * 97u) % 600u);
                const float vx = me[0] + gx * 0.5f - gy / length * off * side, vy = me[1] + gy * 0.5f + gx / length * off * side;
                float gz;
                if (BotsGroundZ(ctx, base, self, vx, vy, me[2] + 1200.0f, gz) && fabsf(gz - me[2]) < 400.0f)
                {
                    via[slot][0] = vx; via[slot][1] = vy; via[slot][2] = gz;
                    viaOn[slot] = true; viaUntil[slot] = now + 20000;
                    static int told = 0;
                    if (told++ < 60) { printf("bots: slot %u takes the long way: by %.0f %.0f first (%.0f off the straight way)\n", slot, vx, vy, off * side); fflush(stdout); }
                }
            }
        }
        if (viaOn[slot] && (now >= viaUntil[slot] || hypotf(via[slot][0] - me[0], via[slot][1] - me[1]) < 250.0f)) viaOn[slot] = false;
        if (viaOn[slot]) memcpy(goal, via[slot], sizeof(goal));
    }
    // 0.5.466: to an objective along the ways players have walked (Justin, War on
    // Rouen: "alot of the bots seem to get stuck walking around ... have them
    // actively find a way to the flag/objectives"; its log: 193 stuck, 62 of
    // them round -800 0, strikes up to 9): a route over the learnt map's links
    // (MapKnowledge::Path, A*, deaths dearer), worked out again every 3 s or
    // when the goal moves 200, followed a standing spot at a time (the next
    // one 140 or more away); straight at it when no route is known.
    if (hasGoal && !atFlag && record)
    {
        static std::vector<BotNavigation::Point> route[MaxClients];
        static uint32_t routeAt[MaxClients] = {};
        static float routeFor[MaxClients][2] = {};
        const BotNavigation::Point feet{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
        // 0.5.559: a route he is getting along is kept: worked out again only
        // when the goal moves 200, the route runs out, or no spot of it has
        // been reached for 5 s (was every 3 s whatever). 558's self-test: 11 of
        // 26 attacker lives never left side 1's base (round -1800 0): the route
        // made again every 3 s from a slightly different start, with the links
        // found bad meanwhile, went out by the north side of the flag's building
        // one time and the south the next (goals -1841 264 / -1572 -159 by
        // turns), so he turned back and forth and took neither.
        static uint32_t reachedAt[MaxClients] = {};
        static size_t lastLeft[MaxClients] = {};
        if (route[slot].size() < lastLeft[slot]) reachedAt[slot] = now;
        lastLeft[slot] = route[slot].size();
        const bool routeStale = route[slot].empty() || now - reachedAt[slot] >= 5000;
        const bool goalMoved = hypotf(goal[0] - routeFor[slot][0], goal[1] - routeFor[slot][1]) > 200.0f;
        if ((routeStale && now - routeAt[slot] >= (route[slot].empty() ? 3000u : 1000u)) || goalMoved)
        {
            // 0.5.562: why it is worked out again, counted for the log once a minute
            {
                static uint32_t counts[4] = {}, toldAt = 0;
                const int why = goalMoved ? 0 : route[slot].empty() ? 1 : reachedAt[slot] == 0 ? 2 : 3;
                counts[why]++;
                g_routeRedo[slot] = why == 0 ? "goal moved" : why == 1 ? "route ran out" : why == 2 ? "lost (none walkable)" : "no spot reached in 5 s";
                if (GetTickCount() - toldAt >= 60000)
                {
                    toldAt = GetTickCount();
                    printf("bots: routes worked out again (all bots so far): goal moved %u, ran out %u, lost %u, no spot reached in 5 s %u\n", counts[0], counts[1], counts[2], counts[3]);
                    fflush(stdout);
                }
            }
            routeAt[slot] = now; routeFor[slot][0] = goal[0]; routeFor[slot][1] = goal[1];
            reachedAt[slot] = now; lastLeft[slot] = 0;
            const int32_t cell = g_mapKnow.Nearest({ goal[0], goal[1], goal[2] }, 400.0f);
            // 0.5.553: the route starts at the nearest learnt spot he can walk
            // to straight (Reachable, as his step), its own spot first. Path
            // started at the nearest by distance and left it out: 552's
            // self-test had the first spot 80-400 off and past a wall 739
            // times of 891 (the step hit the wall 583 times).
            int32_t start = -1;
            auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
            // 0.5.607: of the eight nearest he can walk to, the one with the least
            // way round toward the goal (to it plus from it to the goal), not the
            // nearest: 606's carrier slot 7, at -1160 44 going east, had his route
            // start at -1205 68 behind him, back toward the base he had just left
            {
                float bestWay = 1e30f;
                for (const int32_t around : g_mapKnow.NearestList(feet, 400.0f, 8))
                {
                    const BotNavigation::Point at = g_mapKnow.At(around);
                    const float way = hypotf(at.x - feet.x, at.y - feet.y) + hypotf(goal[0] - at.x, goal[1] - at.y);
                    if (way >= bestWay) continue;
                    BotNavigation::Point landing;
                    if (BotNavigation::Reachable(feet, at, query, landing)) { start = around; bestWay = way; }
                }
            }
            const bool walkableStart = start >= 0;
            if (start < 0) start = g_mapKnow.Nearest(feet, 300.0f);
            // 0.5.554: the route's first six links walked by the test his step
            // uses, spot to spot; one that fails is marked (dearer for every
            // bot from now on) and the route worked out again, three times at
            // most. 553's self-test: from a start he could walk to, the next
            // spots were still out of reach 555 times of 822 (slot 22 went
            // round -650 -700 for half a minute): the learnt links are any
            // player's moves between two cells, jumps and climbs too, and two
            // cells' spots can have a corner between them.
            std::vector<int32_t> cells;
            std::vector<uint8_t> via;
            int marked = 0, crossed = 0;
            for (int round = 0; round < 3 && cell >= 0 && start >= 0; round++)
            {
                // 0.5.564: a CTF attacker's route keeps to his lane (left,
                // middle or right of the way between the flags) where the
                // streets allow: 563's self-test had 12 of 24 attacker deaths
                // by the other side's attackers, both sides' running the same
                // best way through the same streets head on.
                BotNavigation::MapKnowledge::Lane lane;
                const uint32_t laneSide = BotSide(base, slot);
                // 0.5.565: only out in the middle (over 900 from both flags),
                // softer (0.25 a unit): 564's lanes from the spawn pushed routes
                // out of side 1's cramped base into its walls (4 of 9 attacker
                // lives left it; 561-563: 85-87%), reach fell (3 lives within
                // 1500 of the enemy flag, 563: 12)
                if (g_ctfLane[slot] != 0 && (laneSide == 1 || laneSide == 2) && g_ctfOwn[laneSide] >= 0 && g_ctfOwn[3 - laneSide] >= 0 &&
                    hypotf(feet.x - g_obj.at[g_ctfOwn[laneSide]][0], feet.y - g_obj.at[g_ctfOwn[laneSide]][1]) > 900.0f &&
                    hypotf(feet.x - g_obj.at[g_ctfOwn[3 - laneSide]][0], feet.y - g_obj.at[g_ctfOwn[3 - laneSide]][1]) > 900.0f)
                {
                    lane.a = { g_obj.at[g_ctfOwn[laneSide]][0], g_obj.at[g_ctfOwn[laneSide]][1], 0.0f };
                    lane.b = { g_obj.at[g_ctfOwn[3 - laneSide]][0], g_obj.at[g_ctfOwn[3 - laneSide]][1], 0.0f };
                    lane.side = g_ctfLane[slot]; lane.perUnit = 0.25f;
                }
                cells = g_mapKnow.PathCells(start, cell, 8000, true, lane.side != 0 ? &lane : nullptr);
                via.assign(cells.size(), 0);
                bool redo = false;
                for (size_t k = 0; k + 1 < cells.size(); k++)
                {
                    if (g_mapKnow.IsBadLink(cells[k], cells[k + 1])) continue;   // known: kept only for want of another way
                    BotNavigation::Point landing;
                    const BotNavigation::Point a = g_mapKnow.At(cells[k]);
                    // 0.5.580: the first six, and every link within 700 of a flag
                    // (579's self-test: side 2's routes into the building at -1792
                    // -91 went -1795 -304 -> -1675 -148 through its wall corner,
                    // past the six checked, and the bots went round and round it)
                    bool byFlag = false;
                    if (k >= 6)
                    {
                        const BotNavigation::Point b = g_mapKnow.At(cells[k + 1]);
                        for (int f = 0; f < g_obj.count && f < 2 && !byFlag; f++)
                            byFlag = hypotf(a.x - g_obj.at[f][0], a.y - g_obj.at[f][1]) < 700.0f || hypotf(b.x - g_obj.at[f][0], b.y - g_obj.at[f][1]) < 700.0f;
                        if (!byFlag) continue;
                    }
                    if (BotNavigation::Reachable(a, g_mapKnow.At(cells[k + 1]), query, landing)) continue;
                    // 0.5.580: not straight: the way a player walked it this
                    // session (a doorway), if he can get to its start and from its
                    // end; the route then goes through those two spots
                    BotNavigation::MapKnowledge::Crossing cross;
                    if (g_mapKnow.CrossingOf(cells[k], cells[k + 1], cross) &&
                        BotNavigation::Reachable(a, cross.from, query, landing) && BotNavigation::Reachable(cross.to, g_mapKnow.At(cells[k + 1]), query, landing))
                    { via[k] = 1; crossed++; continue; }
                    {
                        g_mapKnow.BadLink(cells[k], cells[k + 1]);
                        // 0.5.579: which, by a flag (the ways in and out of the
                        // flag buildings), the first 80
                        {
                            const BotNavigation::Point b = g_mapKnow.At(cells[k + 1]);
                            for (int f = 0; f < g_obj.count && f < 2; f++)
                                if (hypotf(a.x - g_obj.at[f][0], a.y - g_obj.at[f][1]) < 600.0f || hypotf(b.x - g_obj.at[f][0], b.y - g_obj.at[f][1]) < 600.0f)
                                {
                                    static int toldBad[2] = {};
                                    if (toldBad[f]++ < 50) { printf("bots: learnt map: link by flag %d not walkable: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", f, a.x, a.y, a.z, b.x, b.y, b.z); fflush(stdout); }
                                    break;
                                }
                        }
                        marked++; redo = true;
                    }
                }
                if (!redo) break;
            }
            route[slot].clear();
            for (size_t k = 0; k < cells.size(); k++)
            {
                route[slot].push_back(g_mapKnow.At(cells[k]));
                BotNavigation::MapKnowledge::Crossing cross;
                if (k < via.size() && via[k] && k + 1 < cells.size() && g_mapKnow.CrossingOf(cells[k], cells[k + 1], cross))
                { route[slot].push_back(cross.from); route[slot].push_back(cross.to); }
            }
            static uint32_t crossTotal = 0, crossTold = 0;
            crossTotal += uint32_t(crossed);
            if (crossed && crossTotal / 25 != crossTold) { crossTold = crossTotal / 25; printf("bots: learnt map: %u links not straight walked the way a player went (%zu ways known)\n", crossTotal, g_mapKnow.crossings.size()); fflush(stdout); }
            static uint32_t badTotal = 0, badTold = 0;
            badTotal += uint32_t(marked);
            if (marked && badTotal / 25 != badTold) { badTold = badTotal / 25; printf("bots: learnt map: %u links found not walkable by a bot so far (%zu marked)\n", badTotal, g_mapKnow.badLinks.size()); fflush(stdout); }
            g_routeStart[slot] = walkableStart ? "a spot he can walk to" : "the nearest (none walkable)";
            static int told = 0;
            if (told++ < 40 && !route[slot].empty()) { printf("bots: slot %u routes to his objective: %zu steps over the learnt map\n", slot, route[slot].size()); fflush(stdout); }
        }
        // 0.5.582: a CTF attacker within 700 of the enemy flag (at home) goes in
        // by a way a player walked out of its base, backwards: from the spot of
        // one he can walk to straight that is the fewest spots from the flag
        // (once a second till he has one; a route made again drops it)
        {
            static bool onTrail[MaxClients] = {};
            static uint32_t trailCheck[MaxClients] = {};
            if (routeAt[slot] == now) onTrail[slot] = false;
            const uint32_t ts = BotSide(base, slot);
            const int enemyFlag = (ts == 1 || ts == 2) ? g_ctfOwn[3 - ts] : -1;
            // 0.5.592: a carrier within 700 of his own flag (at home) goes in to it
            // the same way, by one of its own base's ways out backwards, once a
            // life (over his own trail's route): 591's capture, slot 21, was put on
            // his flag by the stuck help from 207 out, outside its building
            const int hf = (ts == 1 || ts == 2) ? g_ctfOwn[ts] : -1;
            static uint8_t homeInLife[MaxClients] = {};
            const bool carrierIn = carrierNow && hf >= 0 && hf <= 1 && homeInLife[slot] != uint8_t(g_botLife[slot] + 1);
            const int ef = carrierIn ? hf : (g_ctfAttacker[slot] && !carrierNow) ? enemyFlag : -1;   // the flag he goes in to
            if (ef >= 0 && ef <= 1 && (carrierIn || !onTrail[slot]) && g_flagCarrier[ef] < 0 && !g_flagDropped[ef] &&
                now - trailCheck[slot] >= 1000 && hypotf(g_obj.at[ef][0] - feet.x, g_obj.at[ef][1] - feet.y) < 700.0f &&
                hypotf(g_obj.at[ef][0] - feet.x, g_obj.at[ef][1] - feet.y) > 120.0f)
            {
                trailCheck[slot] = now;
                struct Pick { float score; int t; size_t i; };
                std::vector<Pick> picks;
                for (int t = 0; t < 16; t++)
                    for (size_t i = 0; i < g_flagTrails[ef][t].size(); i++)
                    {
                        const BotNavigation::Point& p = g_flagTrails[ef][t][i];
                        const float d = hypotf(p.x - feet.x, p.y - feet.y);
                        if (d < 350.0f && fabsf(p.z - feet.z) < 100.0f) picks.push_back({ float(i) * 80.0f + d, t, i });
                    }
                std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.score < b.score; });
                auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
                for (size_t k = 0; k < picks.size() && k < 4; k++)
                {
                    const auto& way = g_flagTrails[ef][picks[k].t];
                    BotNavigation::Point landing;
                    if (!BotNavigation::Reachable(feet, way[picks[k].i], query, landing)) continue;
                    route[slot].clear();
                    for (size_t i = picks[k].i + 1; i-- > 0;) route[slot].push_back(way[i]);
                    route[slot].push_back({ g_obj.at[ef][0], g_obj.at[ef][1], way[0].z });
                    onTrail[slot] = true; reachedAt[slot] = now; lastLeft[slot] = route[slot].size();
                    if (carrierIn) homeInLife[slot] = uint8_t(g_botLife[slot] + 1);
                    static int told = 0;
                    if (told++ < 60) { printf("bots: slot %u (side %u%s) goes in by a way out of the flag's base: %zu spots from %.0f %.0f %.0f\n", slot, ts, carrierIn ? ", carrier, home" : "", route[slot].size(), way[picks[k].i].x, way[picks[k].i].y, way[picks[k].i].z); fflush(stdout); }
                    break;
                }
            }
            // 0.5.589: a carrier goes home the way he came: his own trail this life
            // (a spot every 120 walked from his spawn, BotTrail), backwards, from
            // the spot of it nearest home he can walk to straight; taken at the
            // take, and again a second after any route made over it. 588's
            // self-test: carrier slot 6's learnt-map routes home were "lost" and
            // made again from wherever he stood, each another way (697 1272, then
            // 1113 1244 3 s on, 400 back), and the stuck help put him forward 4 times.
            static std::vector<BotNavigation::Point> homeWay[MaxClients];
            static uint8_t homeWayLife[MaxClients] = {};
            if (carrierNow && homeWayLife[slot] != uint8_t(g_botLife[slot] + 1))
            {
                homeWayLife[slot] = uint8_t(g_botLife[slot] + 1);
                homeWay[slot] = g_trail[slot];
                const int own = (ts == 1 || ts == 2) ? g_ctfOwn[ts] : -1;
                // 0.5.590: a trail that began at least 500 nearer his flag than the
                // flag he took (589 asked for one begun within 1500 of his flag:
                // 3 carriers of 3 had none); its end on, his route from there
                const size_t had = homeWay[slot].size();
                const float frontHome = had && own >= 0 && own <= 1 ? hypotf(homeWay[slot].front().x - g_obj.at[own][0], homeWay[slot].front().y - g_obj.at[own][1]) : -1.0f;
                const int took = g_flagCarrier[0] == int(slot) ? 0 : 1;
                const float flagHome = own >= 0 && own <= 1 ? hypotf(g_obj.at[took][0] - g_obj.at[own][0], g_obj.at[took][1] - g_obj.at[own][1]) : 0.0f;
                if (own < 0 || own > 1 || had < 4 || frontHome > flagHome - 500.0f)
                    homeWay[slot].clear();
                printf("bots: carrier slot %u: his trail this life %zu spots, begun %.0f from his flag (the flag he took %.0f)\n", slot, had, frontHome, flagHome); fflush(stdout);
                static int told = 0;
                if (told++ < 40) { printf("bots: carrier slot %u: his own way home: %zu spots%s\n", slot, homeWay[slot].size(), homeWay[slot].empty() ? " (none fit)" : ""); fflush(stdout); }
            }
            if (carrierNow && !homeWay[slot].empty() && !onTrail[slot] && now - trailCheck[slot] >= 1000)
            {
                trailCheck[slot] = now;
                const auto& way = homeWay[slot];
                auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
                struct Pick { float score; size_t i; };
                std::vector<Pick> picks;
                for (size_t i = 0; i < way.size(); i++)
                {
                    const float d = hypotf(way[i].x - feet.x, way[i].y - feet.y);
                    if (d < 400.0f && fabsf(way[i].z - feet.z) < 120.0f) picks.push_back({ float(i) * 120.0f + d, i });
                }
                std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.score < b.score; });
                for (size_t k = 0; k < picks.size() && k < 5; k++)
                {
                    BotNavigation::Point landing;
                    if (!BotNavigation::Reachable(feet, way[picks[k].i], query, landing)) continue;
                    route[slot].clear();
                    for (size_t i = picks[k].i + 1; i-- > 0;) route[slot].push_back(way[i]);
                    const int own = g_ctfOwn[ts];
                    route[slot].push_back({ g_obj.at[own][0], g_obj.at[own][1], way[0].z });
                    onTrail[slot] = true; reachedAt[slot] = now; lastLeft[slot] = route[slot].size();
                    static int told = 0;
                    if (told++ < 80) { printf("bots: carrier slot %u goes home the way he came: %zu spots from %.0f %.0f\n", slot, route[slot].size(), way[picks[k].i].x, way[picks[k].i].y); fflush(stdout); }
                    break;
                }
            }
            // 0.5.584: a carrier within 600 of the flag he took (its base) goes
            // out by one of its ways out, forwards: from a spot of one he can walk
            // to straight, the way whose end is nearest his own flag, to its end
            // (his route home from there). 583's self-test: slot 4 took the flag at
            // 2112 462, his route out ran into a wall (1856 344 -> 1726 356), the
            // stuck help put him back 10 s on, and he was shot in its base.
            const int cf = g_flagCarrier[0] == int(slot) ? 0 : g_flagCarrier[1] == int(slot) ? 1 : -1;
            // 0.5.607: each base's way out once a life (exitLife; both picks below):
            // 606's 8-bot test, carrier slot 7 picked one six times in the base
            // he took the flag from, each from another spot (often behind him:
            // his velocity turned round every pick), and outside it the learnt
            // map route home started back toward it, so the way out was taken
            // again: round and round, the stuck help put him on 4 times
            static uint8_t exitLife[MaxClients] = {};
            const bool exitFree = exitLife[slot] != uint8_t(g_botLife[slot] + 1);
            if (carrierNow && exitFree && cf >= 0 && hf >= 0 && hf <= 1 && hf != cf && !onTrail[slot] && now - trailCheck[slot] >= 1000 &&
                hypotf(g_obj.at[cf][0] - feet.x, g_obj.at[cf][1] - feet.y) < 600.0f)
            {
                trailCheck[slot] = now;
                struct Pick { float score; int t; size_t i; };
                std::vector<Pick> picks;
                for (int t = 0; t < 16; t++)
                {
                    const auto& way = g_flagTrails[cf][t];
                    if (way.size() < 3) continue;
                    const float endHome = hypotf(way.back().x - g_obj.at[hf][0], way.back().y - g_obj.at[hf][1]);
                    for (size_t i = 0; i + 2 < way.size(); i++)
                    {
                        const float d = hypotf(way[i].x - feet.x, way[i].y - feet.y);
                        if (d < 300.0f && fabsf(way[i].z - feet.z) < 100.0f) picks.push_back({ d + endHome * 0.5f, t, i });
                    }
                }
                std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.score < b.score; });
                auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
                for (size_t k = 0; k < picks.size() && k < 4; k++)
                {
                    const auto& way = g_flagTrails[cf][picks[k].t];
                    BotNavigation::Point landing;
                    if (!BotNavigation::Reachable(feet, way[picks[k].i], query, landing)) continue;
                    route[slot].assign(way.begin() + ptrdiff_t(picks[k].i), way.end());
                    onTrail[slot] = true; reachedAt[slot] = now; lastLeft[slot] = route[slot].size();
                    exitLife[slot] = uint8_t(g_botLife[slot] + 1);
                    static int told = 0;
                    if (told++ < 60) { printf("bots: carrier slot %u (side %u) goes out by a way out of the flag's base: %zu spots from %.0f %.0f %.0f to %.0f %.0f %.0f\n", slot, ts, route[slot].size(), way[picks[k].i].x, way[picks[k].i].y, way[picks[k].i].z, way.back().x, way.back().y, way.back().z); fflush(stdout); }
                    break;
                }
                // 0.5.585: none (584's carriers never took one): why, once a life
                static uint8_t toldNone[MaxClients] = {};
                if (!onTrail[slot] && toldNone[slot] != uint8_t(g_botLife[slot] + 1))
                {
                    toldNone[slot] = uint8_t(g_botLife[slot] + 1);
                    int ways = 0; for (const auto& way : g_flagTrails[cf]) ways += way.size() >= 3;
                    printf("bots: carrier slot %u at %.0f %.0f %.0f: no way out of the base taken (%d ways known, %zu spots near, none walkable straight)\n", slot, feet.x, feet.y, feet.z, ways, picks.size()); fflush(stdout);
                }
            }
            // 0.5.596: any bot within 350 of his own flag (at home) going over 700
            // from it leaves by one of its base's ways out, forwards, the one
            // ending nearest where he goes. 594's self-test: 75 of 302 stalls were
            // side 1's bots in and by their flag's building (-1850 -220), its spots
            // past its walls ("first (none walkable)", no step at all).
            if (!carrierNow && exitFree && hf >= 0 && hf <= 1 && !onTrail[slot] && g_flagCarrier[hf] < 0 && !g_flagDropped[hf] && now - trailCheck[slot] >= 1000 &&
                hypotf(g_obj.at[hf][0] - feet.x, g_obj.at[hf][1] - feet.y) < 350.0f && hypotf(g_obj.at[hf][0] - goal[0], g_obj.at[hf][1] - goal[1]) > 700.0f)
            {
                trailCheck[slot] = now;
                struct Pick { float score; int t; size_t i; };
                std::vector<Pick> picks;
                for (int t = 0; t < 16; t++)
                {
                    const auto& way = g_flagTrails[hf][t];
                    if (way.size() < 3) continue;
                    const float endGoal = hypotf(way.back().x - goal[0], way.back().y - goal[1]);
                    for (size_t i = 0; i + 2 < way.size(); i++)
                    {
                        const float d = hypotf(way[i].x - feet.x, way[i].y - feet.y);
                        if (d < 300.0f && fabsf(way[i].z - feet.z) < 100.0f) picks.push_back({ d + endGoal * 0.5f, t, i });
                    }
                }
                std::sort(picks.begin(), picks.end(), [](const Pick& a, const Pick& b) { return a.score < b.score; });
                auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
                for (size_t k = 0; k < picks.size() && k < 4; k++)
                {
                    const auto& way = g_flagTrails[hf][picks[k].t];
                    BotNavigation::Point landing;
                    if (!BotNavigation::Reachable(feet, way[picks[k].i], query, landing)) continue;
                    route[slot].assign(way.begin() + ptrdiff_t(picks[k].i), way.end());
                    onTrail[slot] = true; reachedAt[slot] = now; lastLeft[slot] = route[slot].size();
                    exitLife[slot] = uint8_t(g_botLife[slot] + 1);
                    static int told = 0;
                    if (told++ < 60) { printf("bots: slot %u (side %u) leaves his base by a way out: %zu spots from %.0f %.0f to %.0f %.0f\n", slot, ts, route[slot].size(), way[picks[k].i].x, way[picks[k].i].y, way.back().x, way.back().y); fflush(stdout); }
                    break;
                }
            }
        }
        // 0.5.606: a carrier, every half second: where he is, his route's next
        // spot and how many are left, and his velocity (605's 8-bot test: slot
        // 4 ran at 200 for 12 s at a "walkable" spot 330 off and stayed round
        // -1700 350; the 3 s ctftrace cannot show why)
        if (carrierNow)
        {
            static uint32_t carryTold[MaxClients] = {};
            if (now - carryTold[slot] >= 500)
            {
                carryTold[slot] = now;
                const float vx = ReadFloat(base, record + 16), vy = ReadFloat(base, record + 20);
                const bool any = !route[slot].empty();
                printf("carrytrace: slot %u at %.0f %.0f %.0f vel %.0f %.0f; next %.0f %.0f (%.0f off), %zu left; routed %u ms ago\n", slot, feet.x, feet.y, feet.z, vx, vy,
                       any ? route[slot].front().x : 0.0f, any ? route[slot].front().y : 0.0f, any ? hypotf(route[slot].front().x - feet.x, route[slot].front().y - feet.y) : 0.0f,
                       route[slot].size(), unsigned(now - routeAt[slot]));
                fflush(stdout);
            }
        }
        // 0.5.552: a spot is dropped once he is within 40 of it (was 140).
        // 551's self-test: in 736 of 797 attacker samples none of the next
        // spots could be walked to straight, nor the first: dropping every
        // spot within 140 dropped the ones that lead round the corner beside
        // him, so all that was left lay past it, straight at it was the wall
        // (544 refused steps), and he circled.
        // 0.5.573: the last spot of the route he reached (he walked from it, so
        // he can walk back to it, and the next spot is linked to it)
        static BotNavigation::Point lastReached[MaxClients] = {};
        static bool haveLast[MaxClients] = {};
        while (!route[slot].empty() && hypotf(route[slot].front().x - feet.x, route[slot].front().y - feet.y) < 40.0f)
        { lastReached[slot] = route[slot].front(); haveLast[slot] = true; route[slot].erase(route[slot].begin()); }
        // 0.5.559: and reaching any of the next six drops those before it (the
        // picker steers to the farthest walkable one, so the first is often
        // never come within 40 of), so a shorter route is real progress
        for (size_t k = std::min<size_t>(6, route[slot].size()); k-- > 1;)
            if (hypotf(route[slot][k].x - feet.x, route[slot][k].y - feet.y) < 40.0f)
            { lastReached[slot] = route[slot][k]; haveLast[slot] = true; route[slot].erase(route[slot].begin(), route[slot].begin() + ptrdiff_t(k) + 1); break; }
        if (routeAt[slot] == now) haveLast[slot] = false;   // a new route: the old one's spot is not on it
        if (route[slot].size() < 2) snprintf(g_pickInfo[slot], sizeof(g_pickInfo[slot]), "no route spot (%zu left): straight at the goal", route[slot].size());   // 0.5.551
        // 0.5.579: an attacker within 400 of the enemy flag, once a life: his
        // route's last spots (how it goes into the flag's building)
        if (g_ctfAttacker[slot])
        {
            static uint8_t toldLife[MaxClients] = {};
            const uint32_t ts = BotSide(base, slot);
            if ((ts == 1 || ts == 2) && g_ctfOwn[3 - ts] >= 0 && g_ctfOwn[3 - ts] <= 1 && toldLife[slot] != uint8_t(g_botLife[slot] + 1) &&
                hypotf(g_obj.at[g_ctfOwn[3 - ts]][0] - feet.x, g_obj.at[g_ctfOwn[3 - ts]][1] - feet.y) < 400.0f)
            {
                toldLife[slot] = uint8_t(g_botLife[slot] + 1);
                std::string tail;
                for (size_t k = route[slot].size() > 4 ? route[slot].size() - 4 : 0; k < route[slot].size(); k++)
                { char t[48]; snprintf(t, sizeof(t), " %.0f %.0f %.0f;", route[slot][k].x, route[slot][k].y, route[slot][k].z); tail += t; }
                printf("bots: slot %u (side %u) %.0f from the enemy flag at %.0f %.0f %.0f: his route's last spots (%zu left):%s\n", slot, ts,
                       hypotf(g_obj.at[g_ctfOwn[3 - ts]][0] - feet.x, g_obj.at[g_ctfOwn[3 - ts]][1] - feet.y), feet.x, feet.y, feet.z, route[slot].size(), tail.c_str());
                fflush(stdout);
            }
        }
        if (route[slot].size() >= 2)
        {
            // 0.5.467: the farthest of the next four spots he has a clear line
            // to (knee and chest high), else the next one: 466's bots walked
            // straight at the next spot (128 to 256 off) and cut the corners
            // into the walls (Justin: "still noticing alot of bots just walking
            // into walls"; its log: 357 walls met)
            // 0.5.468: looked again every 250 ms (not every frame), the spot kept between
            static uint32_t lookedAt[MaxClients] = {};
            static BotNavigation::Point picked[MaxClients] = {};
            if (now - lookedAt[slot] >= 250 || hypotf(picked[slot].x - feet.x, picked[slot].y - feet.y) < 40.0f)
            {
                lookedAt[slot] = now;
                size_t pick = 0;
                // 0.5.552: the farthest of the first six spots he can walk to
                // straight by the very test his step uses (TacticalMove's
                // Reachable: his body's box, a stair, a slope in pieces of 48,
                // walkable ground at the end), so the spot and the step agree;
                // none: the first (his step goes round)
                const size_t look = std::min<size_t>(5, route[slot].size() - 1);
                auto query = [&](BotNavigation::Point a, BotNavigation::Point b, bool body) { return BotTrace(ctx, base, self, self, a, b, body); };
                bool passed = false;
                for (size_t k = look + 1; k-- > 0;)
                {
                    BotNavigation::Point landing;
                    if (BotNavigation::Reachable(feet, route[slot][k], query, landing)) { pick = k; passed = true; break; }
                }
                // 0.5.560: none of the next six walkable: the farthest walkable
                // spot further along the SAME route (up to 24, once a second) is
                // taken and those before it dropped, so he keeps his way out;
                // none on the route: worked out again at once from a walkable
                // start. 559's self-test: side 1's trapped attackers ran at full
                // speed inside the flag's building with no route spot walkable
                // (the step hit a wall at x -1600), and each 5 s a new route
                // aimed at another wall.
                const char* how = passed ? "walkable" : "first (none walkable)";
                // 0.5.563: none walkable in a straight line, but a spot of the
                // route within 200: he is on his way, not lost; steered to the
                // nearest one (those before it dropped), his step finding its
                // way round what is between. 562's self-test: routes were made
                // again 1700 times for "lost" (goal moved 38, ran out 102, no
                // spot reached 137): Reachable's single sweep over more than
                // 192 fails on any rise over 18, so on Rouen's uneven streets
                // far spots are seldom "walkable", and each new route could
                // point another way.
                if (!passed)
                {
                    // 0.5.573: first, back to the last spot of the route he reached
                    // when he can walk there straight (572's carrier test: a carrier
                    // at 650 450 steered to a spot within 200 he could not walk to
                    // straight, and the step's small Detour search sent him 100-150
                    // off his way, 21 s for half the way home)
                    if (haveLast[slot] && hypotf(lastReached[slot].x - feet.x, lastReached[slot].y - feet.y) > 40.0f)
                    {
                        BotNavigation::Point landing;
                        if (BotNavigation::Reachable(feet, lastReached[slot], query, landing))
                        {
                            route[slot].insert(route[slot].begin(), lastReached[slot]);
                            pick = 0; passed = true; how = "back to the last spot reached";
                        }
                    }
                }
                if (!passed)
                {
                    // the farthest along within 200, not the nearest: one he has
                    // passed would turn him back
                    size_t along = route[slot].size();
                    for (size_t k = std::min<size_t>(24, route[slot].size()); k-- > 0;)
                        if (hypotf(route[slot][k].x - feet.x, route[slot][k].y - feet.y) < 200.0f) { along = k; break; }
                    if (along < route[slot].size())
                    {
                        route[slot].erase(route[slot].begin(), route[slot].begin() + ptrdiff_t(along));
                        pick = 0; passed = true; how = "on the route within 200 (not straight)";
                    }
                }
                if (!passed)
                {
                    static uint32_t widerAt[MaxClients] = {};
                    if (now - widerAt[slot] >= 1000)
                    {
                        widerAt[slot] = now;
                        const size_t reach = std::min<size_t>(24, route[slot].size());   // (not "far": a Windows macro)
                        for (size_t k = reach; k-- > look + 1;)
                        {
                            BotNavigation::Point landing;
                            if (BotNavigation::Reachable(feet, route[slot][k], query, landing))
                            {
                                route[slot].erase(route[slot].begin(), route[slot].begin() + ptrdiff_t(k));
                                pick = 0; passed = true; how = "walkable further along (re-anchored)";
                                break;
                            }
                        }
                        if (!passed) { reachedAt[slot] = 0; how = "none on the route walkable (worked out again)"; }
                    }
                }
                snprintf(g_pickInfo[slot], sizeof(g_pickInfo[slot]), "spot %zu of %zu, %s, %.0f away; made (%s)", pick, route[slot].size(),
                         how, hypotf(route[slot][pick].x - feet.x, route[slot][pick].y - feet.y),
                         g_routeRedo[slot] ? g_routeRedo[slot] : "?");
                picked[slot] = route[slot][pick];
            }
            goal[0] = picked[slot].x; goal[1] = picked[slot].y; goal[2] = picked[slot].z;
        }
    }
    memcpy(g_steer[slot], goal, sizeof(g_steer[slot])); g_steerValid[slot] = hasGoal;   // 0.5.483
    const float enemyDistance = found ? sqrtf(best) : 1e30f;
    // 0.5.471: a taker keeps on to the flag unless an enemy is within 250 (was
    // 600): 0.5.470's War on Rouen had the middle flag in play from 91 s to
    // the end (386 s), never taken, both sides' takers stopping to trade shots
    // round it (Justin: "the enemy bots are not catching and hoding the
    // objective and pushing to the next"); on it he fights, crouched
    // 0.5.566: a CTF attacker within 1600 of the enemy flag runs on for it
    // as War's taker does (an enemy within 250 stops him, not 900). 565's
    // 10-minute self-test: no attacker in 36 lives came within 700 of the
    // enemy flag; both sides' died about 1250 out from it (side 1 at x 850 to
    // 1160, side 2 at x -500), stopping to fight the stream of the enemy's
    // respawns leaving their base, one or two against many
    bool ctfRunner = false, clearFirst = false, ctfDash = false, ctfCommitted = false;
    if (g_ctfAttacker[slot] && !carrierNow)
    {
        const uint32_t rs = BotSide(base, slot);
        if ((rs == 1 || rs == 2) && g_ctfOwn[3 - rs] >= 0 && g_ctfOwn[3 - rs] <= 1)
        {
            const float toEnemyFlag = hypotf(g_obj.at[g_ctfOwn[3 - rs]][0] - me[0], g_obj.at[g_ctfOwn[3 - rs]][1] - me[1]);
            ctfRunner = toEnemyFlag < 1600.0f;
            // 0.5.576: within 300 he dashes for it whatever is about (the grab
            // is a touch at 60): 575's 10-minute match had slot 4 stand 147 from
            // the enemy flag for half a minute in Think's fight (an enemy within
            // the runner's 250 there always), till shot
            ctfDash = toEnemyFlag < 300.0f;
            // 0.5.581: within 450 he is committed: no falling back hurt (580's
            // self-test: slot 4, on the walkable way into the building at -1792
            // -91, 273 from the flag, fell back 300 behind smoke at low health
            // and was killed out there anyway)
            ctfCommitted = toEnemyFlag < 450.0f;
            // 0.5.574: within 700 of it, an enemy in sight within 600 is
            // fought before the grab (the wave clears the flag's defenders):
            // 571-573's carrier tests, a taker put on the flag, died 0.3-6 s
            // after the take, shot by the cover defenders there
            // 0.5.575: OFF unless COD3_CTF_CLEARFIRST=1: 574's assault test
            // (waves put 500-800 out) had them stall 350-400 from the flag
            // trading shots with defenders in cover, no take in 10 assaults;
            // kept for comparing
            static const bool clearFirstOn = []() { const char* t = getenv("COD3_CTF_CLEARFIRST"); return t && t[0] == '1'; }();
            clearFirst = clearFirstOn && toEnemyFlag < 700.0f && visible && enemyDistance < 600.0f;
        }
    }
    // 0.5.568: and a flag carrier always runs (he cannot fire; Think's fight
    // branch stood him still)
    const bool toFlag = hasGoal && !atFlag && (carrierNow || ctfDash || (!clearFirst && (!visible || enemyDistance > ((warTaker || ctfRunner) ? 250.0f : 900.0f))));
    // 0.5.583: running for the flag with an enemy in sight within 900 he
    // shoots on the move: looks and fires at him, legs to the flag (582's
    // self-test: side 2's three attacker deaths nearest the flag at -1792 -91,
    // 211-418 from it, were all running "to goal", looking at the flag, shot by
    // the defenders in cover 130-300 off without a shot back). Not a carrier.
    const bool runGun = toFlag && !carrierNow && found && visible && enemyDistance < 900.0f;
    if (toFlag && !runGun)
    {
        memcpy(target, goal, sizeof(target));
        target[2] = me[2];   // look ahead, level, while running there
    }
    // 0.5.197: his own errand on the learnt map (outskirts, high or low
    // ground) while no enemy is near or in sight (ExploreStep).
    bool exploring = false, exploreHold = false;
    float exploreGoal[3] = {}, exploreLook[3] = {};
    if (!toFlag && !atFlag && g_botTactics.load())
        exploring = ExploreStep(base, self, slot, now, me, myEye, found, visible, enemyDistance, enemyAt, exploreGoal, exploreLook, exploreHold);
    if (exploring) memcpy(target, exploreLook, sizeof(target));
    if (!found && !hasGoal && !exploring)
    {
        auto routeRoam=[&]() {
            const float angle=g_yaw[slot]*0.01745329252f;
            const float roam[3]={me[0]+std::cos(angle)*384,me[1]+std::sin(angle)*384,me[2]-myEye};
            TacticalMove(ctx,base,self,0,slot,now,health,viewYaw,roam,false,orders);
        };
        static uint32_t lastLonely[MaxClients] = {};
        if (now - lastLonely[slot] > 3000)
        {
            lastLonely[slot] = now;
            const uint32_t host = Guest::Read32(base, entities + 4);
            const uint32_t hostRecord = host ? Guest::Read32(base, host + 512) : 0;
            printf("bots: slot %u (side %u) finds no one; the host: side %u, health %d, state %d\n", slot, mySide,
                   Side(base, host), hostRecord ? int32_t(Guest::Read32(base, hostRecord + 304)) : -1,
                   hostRecord ? int32_t(Guest::Read32(base, hostRecord + 1924)) : -1);
            fflush(stdout);
        }
        // No one to fight. 0.5.168, by role:
        static uint32_t nextTurn[MaxClients] = {}, campUntil[MaxClients] = {}, roamUntil[MaxClients] = {};
        static float heading[MaxClients] = {};
        const uint32_t jitter = (slot * 7919u + now / 1000u) % 5000u;
        if (g_role[slot] == RoleCamper)
        {
            // Patrol before taking a short lookout pause; no40-60 second
            // camping hold on spawn when there is nobody visible to fight.
            if (campUntil[slot] == 0) { roamUntil[slot]=now+8000;campUntil[slot]=now+11000; }
            if (now >= campUntil[slot]) { roamUntil[slot]=now+8000;campUntil[slot]=now+11000; }
            if (now < roamUntil[slot]) { g_yaw[slot] += 0.4f; orders.forward = 110; routeRoam(); g_why[slot] = "roam (no goal)"; return orders; }
            g_yaw[slot] += (now / 2500) % 2 ? 0.7f : -0.7f;
            orders.buttons = BotCrouch(slot, true);
            routeRoam(); g_why[slot] = "roam (no goal)"; return orders;
        }
        if (g_role[slot] == RoleFlanker || g_role[slot] == RoleRusher)
        {
            // Long straight runs in a heading changed every 6-11 s (the
            // outskirts: away from the middle of where everyone is); the
            // rusher sprints them.
            if (now >= nextTurn[slot])
            {
                nextTurn[slot] = now + 6000 + jitter;
                heading[slot] = g_yaw[slot] + 70.0f + float((slot * 37u + now / 977u) % 220u);
            }
            const float step = AngleDifference(heading[slot], g_yaw[slot]);
            g_yaw[slot] += fmaxf(-4.0f, fminf(4.0f, step));
            orders.forward = 127;
            if (g_role[slot] == RoleRusher || fabsf(step) < 10.0f) orders.buttons |= SprintButton;
            routeRoam(); g_why[slot] = "roam (no goal)"; return orders;
        }
        // The objective player with nothing to do: wander, turning slowly.
        g_yaw[slot] += 0.6f;
        orders.forward = 90;
        routeRoam(); g_why[slot] = "roam (no goal)"; return orders;
    }
    // 0.5.195: how he plays this fight, from his role, his health, his gun
    // and his enemy's health: aggressive (closes in), defensive (keeps his
    // distance, low, from cover), a melee charge, a retreat behind smoke, or
    // a flank (round the side instead of straight at him).
    int32_t targetHealth = 100;
    if (found && threat) { const uint32_t theirs = Guest::Read32(base, threat + 512); if (theirs) targetHealth = int32_t(Guest::Read32(base, theirs + 304)); }
    enum { FightNormal, FightAggressive, FightDefensive } fight = FightNormal;
    if (health >= 70 && targetHealth <= 40) fight = FightAggressive;
    // 0.5.197: his form: on a streak (two or more kills ahead) he presses
    // on; dying again and again (three or more behind) he plays it safe.
    if (g_form[slot] >= 2 && health >= 60) fight = FightAggressive;
    if (health <= 45 || g_role[slot] == RoleCamper || (g_form[slot] <= -3 && g_role[slot] != RoleRusher)) fight = FightDefensive;
    if (g_role[slot] == RoleRusher && health > 30) fight = FightAggressive;
    // 0.5.224: a shotgun closes in whatever his role, unless nearly dead.
    const bool shotgun = BotHoldsShotgun(base, record);
    if (shotgun && health > 25) fight = FightAggressive;
    if (BotIsDemolitionist(slot) && health > 20) fight = FightAggressive;   // 0.5.326: Justin: the Demolitionist presses on
    if (BotIsRusher2(slot) && health > 35) fight = FightAggressive;         // 0.5.329: Rusher 2 too
    const bool emptyGun = BotClipEmpty(base, record);
    const bool charge = found && visible && !atFlag && enemyDistance < 230.0f &&
                        (emptyGun || (!shotgun && (fight == FightAggressive || health > targetHealth + 30)));   // 0.5.224: a loaded shotgun shoots point blank instead
    if (charge && !g_charging[slot]) { printf("bots: slot %u charges in to melee (%.0f away, health %d vs %d%s)\n", slot, enemyDistance, health, targetHealth, emptyGun ? ", gun empty" : ""); fflush(stdout); }
    g_charging[slot] = charge;
    static uint32_t retreatUntil[MaxClients] = {}, retreatStart[MaxClients] = {}, retreatCooldown[MaxClients] = {};
    if (found && visible && !atFlag && !charge && health <= 35 && enemyDistance < 750.0f &&
        now >= retreatCooldown[slot] && now >= tactics.coverUntil && !(g_role[slot] == RoleRusher && health > 20) && !ctfCommitted)
    {
        retreatStart[slot] = now; retreatUntil[slot] = now + 3000; retreatCooldown[slot] = now + 9000;
        g_wantSmoke[slot] = GetTickCount() + 500;
        printf("bots: slot %u falls back behind smoke (health %d, enemy %.0f away)\n", slot, health, enemyDistance); fflush(stdout);
    }
    const bool retreating = found && now < retreatUntil[slot];
    const bool smokeFirst = retreating && now - retreatStart[slot] < 450;   // facing him: the smoke lands between
    float escape[3] = {};
    if (retreating)
    {
        const float away = atan2f(me[1] - enemyAt[1], me[0] - enemyAt[0]) + (slot % 2 ? 0.35f : -0.35f);
        escape[0] = me[0] + cosf(away) * 420.0f; escape[1] = me[1] + sinf(away) * 420.0f; escape[2] = me[2];
        if (!smokeFirst) memcpy(target, escape, 12);
    }
    static uint32_t flankUntil[MaxClients] = {}, flankNext[MaxClients] = {};
    static float flankAt[MaxClients][3] = {};
    const bool flankRole = g_role[slot] == RoleFlanker || (g_role[slot] == RoleObjective && slot % 3 == 0 && !hasGoal);
    if (found && !exploring && !retreating && !charge && !atFlag && !toFlag && flankRole && enemyDistance > 500.0f && enemyDistance < 2200.0f &&
        now >= flankNext[slot] && fight != FightDefensive)
    {
        const float side = ((now / 7000) + slot) % 2 ? 1.3f : -1.3f;   // about 75 degrees round
        const float angle = atan2f(me[1] - enemyAt[1], me[0] - enemyAt[0]) + side;
        const float radius = fminf(enemyDistance * 0.6f, 700.0f);
        flankAt[slot][0] = enemyAt[0] + cosf(angle) * radius; flankAt[slot][1] = enemyAt[1] + sinf(angle) * radius; flankAt[slot][2] = me[2];
        flankUntil[slot] = now + 6000; flankNext[slot] = now + 14000 + phase0(slot) % 6000;
        printf("bots: slot %u flanks round to %.0f %.0f (enemy %.0f away)\n", slot, flankAt[slot][0], flankAt[slot][1], enemyDistance); fflush(stdout);
    }
    bool flanking = found && now < flankUntil[slot] && !retreating && !charge;
    if (flanking && (hypotf(flankAt[slot][0] - me[0], flankAt[slot][1] - me[1]) < 90.0f || (visible && enemyDistance < 400.0f)))
    { flankUntil[slot] = 0; flanking = false; }
    // 0.5.541: a CTF defender at his place with no one about: crouched most
    // of the time, his view sweeping 55 either side of the way the enemy
    // comes, slowly (his own phase); with an enemy he fights from there (atFlag)
    if (!found && g_ctfHold[slot])
    {
        const float want = g_ctfWatchYaw[slot] + sinf(float((now + slot * 977u) % 7854u) * 0.0008f) * 55.0f;
        g_yaw[slot] += fmaxf(-2.0f, fminf(2.0f, AngleDifference(want, g_yaw[slot])));
        orders.buttons = BotCrouch(slot, (now / 1000u + slot * 3u) % 10u < 7u);
        g_why[slot] = "cover hold"; return orders;
    }
    if (!found && hasGoal && atFlag)
    {
        // At his flag with no one about: hold it, looking around.
        // 0.5.219: Justin: they crouch too much on the flag. Crouched a
        // third of the time now (4 s of every 12, staggered by slot),
        // standing the rest with a slow step about the flag.
        g_yaw[slot] += 0.8f;
        const uint32_t cycle = (now / 1000u + slot * 5u) % 12u;
        orders.buttons = BotCrouch(slot, cycle >= 8);
        if (cycle < 8) orders.right = ((now / 1500u + slot) % 2) ? 40 : -40;
        g_why[slot] = "flag hold"; return orders;
    }
    const float dx = target[0] - me[0], dy = target[1] - me[1], dz = target[2] - me[2];
    const float flat = sqrtf(dx * dx + dy * dy);
    const float wantYaw = atan2f(dy, dx) * 57.29578f;
    // 0.5.186: native AngleVectors8254E120 uses Z=-sin(pitch).
    // Feed the same signed native pitch to command, view and actual shot.
    float wantPitch = BotAim::Pitch(dx,dy,dz);
    const bool throwing = now < g_nadeUntil[slot];
    if (throwing) wantPitch -= 14.0f; // upward grenade loft is negative
    const bool riflenade = record && BotHoldsRifleGrenade(base, record);   // 0.5.326
    if (riflenade) wantPitch -= std::clamp(flat / 110.0f, 4.0f, 24.0f);   // its arc: more loft further off
    g_wantYaw[slot] = wantYaw; g_wantPitch[slot] = wantPitch;
    const float yawError = AngleDifference(wantYaw, viewYaw);
    g_aimOff[slot] = yawError;   // 0.5.197
    const float pitchError = AngleDifference(wantPitch, viewPitch);
    // Turn toward the target. His view is what is sent plus an offset the
    // game set at his spawn, shown several frames late; steering by the
    // error over-corrected and swung. So the offset is measured while what
    // is sent has held still for a while (the view has caught up), and what
    // is sent is the wanted yaw less the offset, eased (at most 8 degrees a
    // frame) so he turns like a player.
    {
        static float offset[MaxClients] = {};
        static bool known[MaxClients] = {};
        static float lastSent[MaxClients] = {};
        static uint32_t stillSince[MaxClients] = {};
        if (fabsf(AngleDifference(g_yaw[slot], lastSent[slot])) > 0.5f) { stillSince[slot] = now; lastSent[slot] = g_yaw[slot]; }
        if (now - stillSince[slot] > 300)
        {
            const float measured = AngleDifference(viewYaw, g_yaw[slot]);
            offset[slot] = known[slot] ? offset[slot] + AngleDifference(measured, offset[slot]) * 0.5f : measured;
            known[slot] = true;
        }
        // 0.5.843: not while he lines up on a vehicle (LookAtVehicle turns him; this
        // pulled him back 8 degrees a frame: sherman6, his view 50-160 off the tank)
        if (known[slot] && int32_t(GetTickCount() - g_lookVehicleUntil[slot]) >= 0)   // (real time: Think's now is the server's)
        {
            const float step = AngleDifference(wantYaw - offset[slot], g_yaw[slot]);
            g_yaw[slot] += fmaxf(-8.0f, fminf(8.0f, step));
        }
    }
    // Pitch the same way as yaw: his view's pitch (+204) is what is sent
    // plus an offset (about 10 degrees seen), learnt while the sent pitch
    // holds still.
    {
        static float pOffset[MaxClients] = {};
        static float lastSentP[MaxClients] = {};
        static uint32_t stillSinceP[MaxClients] = {};
        if (fabsf(g_pitch[slot] - lastSentP[slot]) > 0.5f) { stillSinceP[slot] = now; lastSentP[slot] = g_pitch[slot]; }
        if (now - stillSinceP[slot] > 300)
            pOffset[slot] += ((viewPitch - g_pitch[slot] * PitchSign()) - pOffset[slot]) * 0.5f;
        const float target = (wantPitch - pOffset[slot]) * PitchSign();
        g_pitch[slot] += (target - g_pitch[slot]) * 0.3f;
    }
    g_pitch[slot] = fmaxf(-70.0f, fminf(70.0f, g_pitch[slot]));

    // Each bot a little different: his slot shifts his timings.
    const uint32_t phase = slot * 7919u;
    // 0.5.90: a reaction time. When he first sees someone (a new nearest
    // enemy) he takes 350 to 650 ms before he fires, as a player would.
    static uint32_t seenSlot[MaxClients], seenAt[MaxClients];
    if (found && targetSlot != seenSlot[slot]) { seenSlot[slot] = targetSlot; seenAt[slot] = now; }
    // 0.5.456: 450 to 850 ms (was 350 to 650)
    const bool reacted = found && visible && now - seenAt[slot] >= 450 + (phase % 400) &&
                         (!g_botTactics.load() || now-tactics.sightSince>=450+(phase%400));
    g_engagedAt[slot] = seenAt[slot];
    if (retreating)
    {
        orders.wantsFire = false; orders.buttons = 0;
        if (smokeFirst) { g_why[slot] = "retreat (smoke)"; orders.forward = 0; orders.right = 0; return orders; }
        orders.forward = 127; orders.right = 0; orders.buttons |= SprintButton;
        BotCrouch(slot, false);
        TacticalMove(ctx,base,self,0,slot,now,health,viewYaw,escape,false,orders);
        g_why[slot] = "retreat"; return orders;
    }
    if (exploring)
    {
        // 0.5.197: on watch at his spot (high ground prone or crouched, low
        // ground crouched), or on his way there along the learnt route.
        if (exploreHold)
        {
            orders.forward = 0; orders.right = 0;
            using St = BotNavigation::MapKnowledge::Style;
            const St st = g_explore[slot].style;
            if (g_explore[slot].purpose == BotExplore::Overwatch)
            {   // 0.5.202: lying down, scoped in, watching
                BotProne(slot);
                if (BotsScopedWeapon(base, HeldDef(base, record))) orders.buttons |= 0x8u;
                g_why[slot] = "explore (overwatch)"; return orders;
            }
            if (st == St::High && slot % 2) BotProne(slot);
            else BotCrouch(slot, st == St::High || st == St::Low);
            g_why[slot] = "explore (hold)"; return orders;
        }
        orders.forward = 127; orders.right = 0;
        BotCrouch(slot, false);
        const float fx = exploreGoal[0] - me[0], fy = exploreGoal[1] - me[1];
        if (g_explore[slot].style == BotNavigation::MapKnowledge::Style::Outskirts || fx * fx + fy * fy > 500.0f * 500.0f) orders.buttons |= SprintButton;
        TacticalMove(ctx,base,self,0,slot,now,health,viewYaw,exploreGoal,false,orders);
        g_why[slot] = "explore (errand)"; return orders;
    }
    // 0.5.198: crossing to the flag, or charging in from range, with an enemy
    // near enough to see him: a smoke thrown ahead first, now and then.
    {
        static uint32_t nextCover[MaxClients] = {};
        if ((toFlag || (charge && flat > 400.0f)) && found && enemyDistance < 1800.0f && now >= nextCover[slot])
        {
            nextCover[slot] = now + 25000 + phase % 15000;
            g_wantSmoke[slot] = GetTickCount() + 400;
            printf("bots: slot %u smokes his way %s\n", slot, toFlag ? "to the flag" : "in"); fflush(stdout);
        }
    }
    if (toFlag && !runGun)
    {
        // Running to the flag, weaving a little, sprinting (0x4, the button
        // the host's commands carry while he runs flat out).
        orders.forward = 127;
        orders.right = ((now + phase) / 1400) % 2 ? 30 : -30;
        orders.buttons |= SprintButton;
        TacticalMove(ctx,base,self,0,slot,now,health,viewYaw,goal,false,orders);
        g_why[slot] = "to goal"; return orders;
    }

    bool crouch = false, prone = false;
    if (atFlag)
    {
        // Holding a flag with an enemy near: stays on it, fighting.
        // 0.5.219: crouched half the time (was always), strafing when up.
        orders.forward = 0;
        orders.right = 0;
        crouch = ((now + phase) / 2000) % 2 == 0;
        if (!crouch) orders.right = ((now + phase) / 900) % 2 ? 60 : -60;
    }
    else if (runGun)
    {
        orders.forward = 127;
        orders.right = ((now + phase) / 1400) % 2 ? 30 : -30;
    }
    else if (charge)
    {
        // 0.5.195: straight at him, sprinting until arm's reach (BotExtras melees).
        orders.forward = 127; orders.right = 0;
        if (flat > 110.0f) orders.buttons |= SprintButton;
    }
    else
    {
        // 0.5.195: the range he keeps depends on how he plays this fight.
        const bool aggressive = fight == FightAggressive, defensive = fight == FightDefensive;
        const float closeIn = shotgun && aggressive ? 110.0f : aggressive ? 250.0f : defensive ? 900.0f : 500.0f;
        const float backOff = shotgun && aggressive ? 0.0f : aggressive ? 80.0f : defensive ? 450.0f : 150.0f;
        if (flat > closeIn) orders.forward = (defensive && visible) ? 0 : 127;
        else if (flat < backOff) orders.forward = -60;
        const int8_t strafe = aggressive ? 90 : defensive ? 40 : 70;
        orders.right = ((now + phase) / 900) % 2 ? strafe : -strafe;
        // 0.5.224: a shotgun sprints in from beyond its range (450), weaving.
        if (shotgun && aggressive && flat > 450.0f) { orders.forward = 127; orders.right = ((now + phase) / 600) % 2 ? 45 : -45; orders.buttons |= SprintButton; }
        // At range, he stops to shoot: crouched, or dropped prone when hurt,
        // defensive, or far off (0.5.88 crouched a third of the time).
        // A shotgun never stops at range: it closes in.
        // 0.5.457: past 450 (defensive 300; was 700 and 400), two times in three
        // (was one in three); prone when hurt (60 or less), defensive or past
        // 900 (was 50 and 1100). Justin: "not alot of bots take cover or go prone"
        if (!shotgun && visible && fabsf(yawError) < 15.0f && flat > (defensive ? 300.0f : 450.0f) &&
            (defensive || ((now + phase) / 2200) % 3 != 2))
        {
            orders.forward = 0; orders.right = 0;
            if ((health <= 60 || defensive || flat > 900.0f) && ((now + phase) / 4000) % 2 == 0) prone = true;
            else crouch = true;
        }
        if (flanking)
        {
            orders.forward = 127; orders.right = 0; crouch = prone = false;
            if (!visible) orders.buttons |= SprintButton;
        }
    }
    // Fire when the aim is close, in bursts (pressed every other frame, so a
    // rifle that wants each shot its own press fires too).
    // 0.5.201: no team kills, one rocket at a time (BotHoldsLauncher above).
    const bool launcher = BotHoldsLauncher(base, record);
    // 0.5.457: a launcher bot's pistol up close (it never fires within 300, so
    // he stood there), his launcher again past 700 while it has rockets
    {
        static uint32_t nextSwap[MaxClients] = {};
        static const char* const pistols[] = { "colt", "p38", "webley", "tt33" };
        static const char* const launchers[] = { "bazooka", "panzerschreck" };
        if (record && found && now >= nextSwap[slot] && !throwing)
        {
            const bool riflenadeHeld = riflenade;
            if (launcher && !riflenadeHeld && visible && flat < 350.0f)
            {
                for (const char* name : pistols)
                    if (BotOwnsLoaded(base, record, name) && BotSwitch(ctx, base, self, name))
                    {
                        nextSwap[slot] = now + 3000;
                        static int told = 0;
                        if (told++ < 40) { printf("bots: slot %u draws his %s (launcher, enemy %.0f away)\n", slot, name, flat); fflush(stdout); }
                        break;
                    }
            }
            else if (!launcher && flat > 700.0f)
            {
                for (const char* name : launchers)
                    if (BotOwnsLoaded(base, record, name) && BotSwitch(ctx, base, self, name))
                    {
                        nextSwap[slot] = now + 3000;
                        static int told = 0;
                        if (told++ < 40) { printf("bots: slot %u takes his %s again (enemy %.0f away)\n", slot, name, flat); fflush(stdout); }
                        break;
                    }
            }
        }
    }
    uint32_t blocker = 0xFFFFFFFFu;
    const bool aimed = !throwing && reacted && fabsf(yawError) < 6.0f && fabsf(pitchError) < 6.0f && (flat < 1600.0f || (riflenade && flat < 2300.0f));
    // 0.5.202: aim down the sights (button 0x8, the game's +speed: what the
    // left trigger sends; mp_fixes reads it as the sights). 0.5.201 fired no
    // rockets at all: a launcher fires only aimed, and bots never aimed.
    // Launchers and scoped rifles always aim; other guns past 700 units unless
    // charging or pressing hard. The shot waits for the sights to come up.
    constexpr uint32_t AdsButton = 0x8;
    const bool scopedHeld = BotsScopedWeapon(base, HeldDef(base, record));
    static uint32_t adsSince[MaxClients] = {};
    const bool wantAds = visible && reacted && !throwing && fabsf(yawError) < 20.0f &&
                         (BotHoldsLauncher(base, record) || scopedHeld || (flat > 250.0f && !charge));   // 0.5.456: sights up past 250 (was 700, and not when aggressive): Justin, "make sure the bots do ads when shooting and adjusting"
    if (wantAds) { if (!adsSince[slot]) adsSince[slot] = now; } else adsSince[slot] = 0;
    const bool adsReady = !wantAds || now - adsSince[slot] >= (BotHoldsLauncher(base, record) ? 700u : 350u);
    g_adsNow[slot] = wantAds && adsSince[slot] && now - adsSince[slot] >= 250u;
    bool mateBlocks = false;
    if (aimed)
    {
        mateBlocks = MateInLine(base, self, mySide, me, target, blocker) || (launcher && MateNear(base, self, mySide, target, 300.0f));
        if (mateBlocks)
        {
            if (now >= g_sidestepUntil[slot] + 1500) { g_sidestepUntil[slot] = now + 600; g_sidestepDir[slot] = ((now / 600) + slot) % 2 ? 1 : -1; }
            static int told = 0;
            if ((told < 120 && told++ < 120)) { printf("bots: slot %u holds fire: teammate %d in the way%s\n", slot, blocker < MaxClients ? int(blocker) : -1, launcher ? " (launcher)" : ""); fflush(stdout); }
        }
    }
    if (aimed && adsReady && !mateBlocks && (!launcher || (flat > 300.0f && now >= g_nextRocket[slot])))
    {
        static uint32_t frame = 0;
        const uint32_t fire = (DebugProbe::BotButtons() & 0x00FF) ? (DebugProbe::BotButtons() & 0x00FF) : 0x1;
        if (launcher)
        {
            orders.buttons = (orders.buttons & AdsButton) | fire;   // one press; the next allowed in 2.5 s
            g_nextRocket[slot] = now + 2500;
            if (riflenade && g_rgUntil[slot] > now + 1200) g_rgUntil[slot] = now + 1200;   // 0.5.326: fired: his gun again
            static int told = 0;
            if ((told < 60 && told++ < 60)) { printf("bots: slot %u fires his launcher at %.0f (one shot)\n", slot, flat); fflush(stdout); }
        }
        else orders.buttons = (orders.buttons & AdsButton) | ((++frame & 1) ? fire : 0);
        orders.wantsFire = true;
    }
    if (wantAds) orders.buttons |= AdsButton;
    // 0.5.88: a grenade, at someone 450 to 1400 units off he's facing,
    // every 18 to 25 seconds: the grenade button held a moment (aimed a
    // little up), then let go to throw.
    if (g_nextNade[slot] == 0) g_nextNade[slot] = now + 5000 + (phase % 5000);   // none in his first seconds
    // 0.5.166: also at someone he saw in the last 4 s who has gone behind
    // cover, 300 to 1100 units off: the grenade's job.
    static uint32_t lastSeen[MaxClients] = {};
    if (visible) lastSeen[slot] = now;
    // 0.5.198: Justin: more grenades. Every 9 to 14 s (was 18 to 25), from
    // 350 to 1500 units at someone in sight (a little nearer with enemies
    // bunched: two or more within 300 of him), or 250 to 1200 at someone who
    // went behind cover in the last 6 s.
    const bool hidingNear = found && !visible && now - lastSeen[slot] < 6000 && flat > 250.0f && flat < 1200.0f;
    int bunched = 0;
    for (int k = 0; k < 3; k++)
        if (nearSlot[k] < MaxClients && nearSlot[k] != targetSlot && hypotf(nearAt[k][0] - target[0], nearAt[k][1] - target[1]) < 300.0f) bunched++;
    if (!throwing && now - g_lastThrowAt[slot % 32] >= ThrowGapMs && SideThrowFree(mySide, me[0], me[1], now) && (visible || hidingNear) && now >= g_nextNade[slot] && flat > (visible ? (bunched ? 300.0f : 350.0f) : 250.0f) && flat < 1500.0f && fabsf(yawError) < 12.0f &&
        !MateNear(base, self, mySide, target, 320.0f))   // 0.5.201: not onto teammates
    {
        const uint32_t ammo = BotNadeAmmo(base, Guest::Read32(base, self + 512), false);
        g_nextNade[slot] = now + (BotIsDemolitionist(slot) ? 5000 + (phase % 3000) : 9000 + (phase % 5000));   // 0.5.326: the Demolitionist's sticky bombs more often
        if (ammo)
        {
            g_nadeBit[slot] = g_fragSearch.Next(slot, now, ammo, 0);
            g_nadeUntil[slot] = g_throwUntil[slot] = now + ThrowPressMs;
            g_lastThrowAt[slot % 32] = now;
            SideThrew(mySide, me[0], me[1], now);   // 0.5.608
            g_throwWeapon[slot] = BotNadeWeapon(base, Guest::Read32(base, self + 512), false); g_throwFrames[slot] = 1; g_throwQuiet[slot] = 60;   // the first command names the grenade; 60 commands with no stance, sprint or fire
            printf("bots: slot %u throws a grenade at %.0f%s (button %08X, weapon %u, %u left)\n", slot, flat, bunched ? " (enemies bunched)" : hidingNear ? " (behind cover)" : "", g_nadeBit[slot], g_throwWeapon[slot], ammo); fflush(stdout);
        }
        else { static int toldNone = 0; if ((toldNone < 30 && toldNone++ < 30)) { printf("bots: slot %u has no grenades left to throw\n", slot); fflush(stdout); } }
    }
    g_fragSearch.Check(slot, now, BotNadeAmmo(base, Guest::Read32(base, self + 512), false));
    // 0.5.326: a rifle grenade (above): someone in sight 400..2200 off, none of his side near him
    if (g_nextRg[slot] == 0) g_nextRg[slot] = now + 4000 + (phase % 4000);
    {
        // 0.5.328: 327 fired none: what his rifle grenades read as (once a slot's life, riflemen and Demolitionists)
        static uint32_t toldLife[MaxClients] = {};
        const uint16_t kit = g_botLoadoutAssigned[slot] ? g_botKit[slot].base : 0xFFFF;
        if (record && (kit % 7) == 2 && toldLife[slot] != g_botLife[slot] + 1u)
        {
            toldLife[slot] = g_botLife[slot] + 1u;
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            for (uint32_t n = 1; list && n < 128; n++)
            {
                const uint32_t def = Guest::Read32(base, list + 4 * n);
                const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
                if (text < 0x40000000u || text >= 0xC0000000u) continue;   // (0.5.757: copies' names lie at 0xAA..)
                char name[32] = {};
                for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
                if (!strstr(name, "_rg")) continue;
                const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
                printf("bots: slot %u (%s) rifle grenade %u %s: owned %u, clip #%u %d, spare #%u %d, held %u, slots %u %u %u %u\n", slot, g_botKit[slot].label[0] ? g_botKit[slot].label : "rifleman", n, name,
                       (Guest::Read32(base, record + 1056 + 4 * (n >> 5)) >> (n & 31)) & 1u, clip, clip < 128 ? int32_t(Guest::Read32(base, record + 4 * (172 + clip))) : -1,
                       spare, spare < 128 ? int32_t(Guest::Read32(base, record + 4 * (80 + spare))) : -1, Guest::Read32(base, record + 164),
                       Guest::Read8(base, record + 1064), Guest::Read8(base, record + 1065), Guest::Read8(base, record + 1066), Guest::Read8(base, record + 1067));
            }
            fflush(stdout);
        }
    }
    if (!throwing && visible && reacted && now >= g_nextRg[slot] && now >= g_rgUntil[slot] && flat > 400.0f && flat < 2200.0f && fabsf(yawError) < 25.0f &&
        !MateNear(base, self, mySide, target, 350.0f) && !BotHoldsLauncher(base, record))
    {
        const uint32_t rg = BotRifleGrenade(base, record);
        g_nextRg[slot] = now + (BotIsDemolitionist(slot) ? 5000 + (phase % 2000) : 7000 + (phase % 4000));   // 0.5.456: more often (was 6-9 s / 10-16 s)
        if (rg)
        {
            g_rgWeapon[slot] = rg; g_rgUntil[slot] = now + 5000;
            static int told = 0;
            if ((told < 200 && told++ < 200)) { printf("bots: slot %u fires a rifle grenade (weapon %u) at %.0f%s\n", slot, rg, flat, BotIsDemolitionist(slot) ? " (demolitionist)" : ""); fflush(stdout); }
        }
    }
    if (now < g_nadeUntil[slot]) orders.buttons = BotGrenadeButton(slot);
    // 0.5.90: far off (over 1200 units) and not shooting, he sprints in.
    if (!crouch && !prone && !orders.wantsFire && !throwing && orders.forward == 127 && flat > (g_role[slot] == RoleRusher ? 300.0f : 1200.0f)) orders.buttons |= SprintButton;
    if (prone) BotProne(slot); else BotCrouch(slot, crouch);
    // Finder for his real view angles: each second, every float in his
    // record that moved as much as the sent yaw (or pitch) did, modulo 360.
    {
        static float before[MaxClients][600] = {};
        static float sentBefore[MaxClients][2] = {};
        static uint32_t lastLook[MaxClients] = {};
        if (now - lastLook[slot] > 1000)
        {
            lastLook[slot] = now;
            const float dYaw = g_yaw[slot] - sentBefore[slot][0], dPitch = g_pitch[slot] - sentBefore[slot][1];
            if (fabsf(AngleDifference(dYaw, 0.0f)) > 8.0f || fabsf(dPitch) > 4.0f)
            {
                printf("bots: view finder (yaw moved %.1f, pitch %.1f):", AngleDifference(dYaw, 0.0f), dPitch);
                for (uint32_t i = 0; i < 600; i++)
                {
                    const float v = ReadFloat(base, record + 4 * i);
                    if (!(v > -400.0f && v < 400.0f)) continue;
                    const float d = AngleDifference(v - before[slot][i], 0.0f);
                    if (fabsf(AngleDifference(dYaw, 0.0f)) > 8.0f && fabsf(d - AngleDifference(dYaw, 0.0f)) < 1.0f) printf(" yaw@+%u(%.1f)", i * 4, v);
                    if (fabsf(dPitch) > 4.0f && fabsf(d - dPitch) < 1.0f) printf(" pitch@+%u(%.1f)", i * 4, v);
                    if (fabsf(dPitch) > 4.0f && fabsf(d + dPitch) < 1.0f) printf(" -pitch@+%u(%.1f)", i * 4, v);
                }
                printf("\n"); fflush(stdout);
            }
            for (uint32_t i = 0; i < 600; i++) before[slot][i] = ReadFloat(base, record + 4 * i);
            sentBefore[slot][0] = g_yaw[slot]; sentBefore[slot][1] = g_pitch[slot];
        }
    }
    static uint32_t lastSaid[MaxClients] = {};
    if (now - lastSaid[slot] > 2000)
    {
        lastSaid[slot] = now;
        printf("bots: slot %u side %u targets %.0f away, aim off %.1f %.1f (view %.1f %.1f, want %.1f %.1f, sent %.1f %.1f), fire %X\n",
               slot, mySide, flat, yawError, pitchError, viewYaw, viewPitch, wantYaw, wantPitch, g_yaw[slot], g_pitch[slot],
               orders.buttons);
        printf("bots:   record +100..+136:");
        for (uint32_t o = 100; o <= 136; o += 4) printf(" %.1f", ReadFloat(base, record + o));
        printf("\n");
        fflush(stdout);
    }
    // 0.5.200: closing on an enemy he cannot see from far off: each bot his
    // own lane (up to 520 units to either side of the straight line), so a
    // group arrives from several directions, not in single file.
    float laneAt[3] = {};
    const float* moveTo = runGun ? goal : flanking ? flankAt[slot] : nullptr;
    if (!flanking && found && !visible && !atFlag && enemyDistance > 600.0f)
    {
        const float lx = target[0] - me[0], ly = target[1] - me[1], ll = hypotf(lx, ly);
        if (ll > 1.0f)
        {
            const float lane = float(int((slot * 7u) % 5u) - 2) * 260.0f * fminf(1.0f, (enemyDistance - 600.0f) / 600.0f);
            laneAt[0] = target[0] - ly / ll * lane; laneAt[1] = target[1] + lx / ll * lane; laneAt[2] = me[2] - myEye;
            if (lane != 0.0f) moveTo = laneAt;
        }
    }
    TacticalMove(ctx,base,self,threat,slot,now,health,viewYaw,target,atFlag,orders,moveTo);
    // 0.5.201: a teammate in his line of fire: a step aside to clear it.
    if (now < g_sidestepUntil[slot] && !runGun) { orders.right = int8_t(g_sidestepDir[slot] * 110); }
    g_why[slot] = runGun ? "to goal, shooting" : "fight"; return orders;
}

// 0.5.170: Justin: the bots should melee when close, use their smoke
// grenades, reload, and jump. The buttons, from the game's own table of
// them (CL_FinishMove, sub_82522B10: bit n is the key at 0x82B53E20 +
// 0x138 + 0x18 n; each key's +command found by its handler): 0x1 attack,
// 0x2 smoke grenade, 0x4 sprint, 0x10 melee, 0x20 use, 0x40 use+reload,
// 0x80 the class's grenade, 0x400 reload, 0x800/0x1000 lean, 0x2000 stance.
// A jump is the command's up move (+26) at 127, as +moveup sends it.
constexpr uint32_t MeleeButton = 0x10, SmokeButton = 0x2, ReloadButton = 0x400;
static void BotExtras(uint8_t* base, uint32_t slot, BotOrders& orders)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return;
    const int32_t health = int32_t(Guest::Read32(base, record + 304));
    const uint32_t now = GetTickCount();
    const uint32_t phase = slot * 7919u;
    static uint32_t nextMelee[MaxClients], nextSmoke[MaxClients], smokeUntil[MaxClients], smokeBit[MaxClients] = {};
    static uint32_t nextReload[MaxClients], reloadUntil[MaxClients];
    if (g_tactical[slot].climbing) return;
    static int told = 0;
    // Melee: someone in sight within arm's reach (the game's own melee
    // reaches about 64; its assist pulls him the rest).
    if (g_charging[slot] && g_seeDist[slot] < 110.0f) orders.buttons &= ~SprintButton;
    if (g_seeDist[slot] < (g_charging[slot] ? 96.0f : 72.0f) && now >= nextMelee[slot])
    {
        orders.buttons = (orders.buttons & ~1u) | MeleeButton;
        nextMelee[slot] = now + 700 + phase % 300;
        if ((told < 60 && told++ < 60)) { printf("bots: slot %u melees (%.0f away)\n", slot, g_seeDist[slot]); fflush(stdout); }
    }
    // Smoke: hurt (half health or less) with someone in sight, once in a
    // while: thrown at his feet's way to cover himself, as a player would.
    if (nextSmoke[slot] == 0) nextSmoke[slot] = now + 10500 + phase % 8400;   // 0.5.333: 5% later
    // 0.5.195: also when he falls back or runs for cover (g_wantSmoke), if
    // his last smoke was 15 s or more ago.
    const bool evasive = now < g_wantSmoke[slot] && now >= nextSmoke[slot];   // 0.5.277: his full wait (Justin, 32 test: far too much smoke; 198 cut it to 6 s)
    // 0.5.224: Justin saw little smoke: also, pushing forward (objective
    // players and rushers) with an enemy in sight 700 to 2500 off, a smoke
    // to cross under (at most every 16-26 s, as the others).
    const bool screen = g_role[slot] == RoleRusher && slot % 3 == 0 && orders.forward > 60 &&   // 0.5.277: a third of the rushers only
                        g_seeDist[slot] > 700.0f && g_seeDist[slot] < 2500.0f && now >= nextSmoke[slot];
    const uint32_t throwSide = BotSide(base, slot);   // 0.5.608: one throw a side at a time (SideThrowFree)
    const float throwX = ReadFloat(base, record), throwY = ReadFloat(base, record + 4);
    // 0.5.752: COD3_BOT_SMOKE_EVERY=ms (tests): a smoke thrown that often, whatever else
    static const uint32_t smokeEvery = getenv("COD3_BOT_SMOKE_EVERY") ? uint32_t(atoi(getenv("COD3_BOT_SMOKE_EVERY"))) : 0;
    const bool forced = smokeEvery > 0 && now >= nextSmoke[slot];
    if (((health <= 60 && g_seeDist[slot] < 1500.0f && now >= nextSmoke[slot]) || evasive || screen || forced) && now - g_lastThrowAt[slot % 32] >= ThrowGapMs &&
        SideThrowFree(throwSide, throwX, throwY, now))
    {
        const uint32_t ammo = BotNadeAmmo(base, record, true);
        nextSmoke[slot] = now + (smokeEvery ? smokeEvery : 47250 + phase % 21000);   // 0.5.333: 5% fewer (Justin): 47 to 68 s (277: 45 to 65 s; 198: 16 to 26 s)
        static int toldSmoke = 0;   // 0.5.199: its own count (shared with the melees it ran out)
        if (ammo)
        {
            smokeBit[slot] = g_smokeSearch.Next(slot, now, ammo, 0);
            smokeUntil[slot] = g_throwUntil[slot] = now + ThrowPressMs;
            g_lastThrowAt[slot % 32] = now;
            SideThrew(throwSide, throwX, throwY, now);   // 0.5.608
            // 0.5.754: 30 commands name the smoke (or the flare), as the host's throw
            // does (its 159 commands named 0x30, the flare, 740's trace): 753's trace
            // had the bots name it in the first only, their gun from the second, and
            // the game took the gun back (+168 3, then 1; +164 the gun): no smoke
            // or flare thrown, nothing spent (their frags go with one)
            g_throwWeapon[slot] = BotNadeWeapon(base, record, true); g_throwFrames[slot] = 30; g_throwQuiet[slot] = 60;   // 60 commands with no stance, sprint or fire
            if ((toldSmoke < 200 && toldSmoke++ < 200)) { printf("bots: slot %u throws smoke (health %d%s, button %08X, weapon %u, %u left)\n", slot, health, evasive ? ", for cover" : "", smokeBit[slot], g_throwWeapon[slot], ammo); fflush(stdout); }
        }
        else
        {
            if ((toldSmoke < 200 && toldSmoke++ < 200)) { printf("bots: slot %u has no smoke left (health %d)\n", slot, health); fflush(stdout); }
            // 0.5.243: every bot "has no smoke left" though his kit gave him
            // smokes: what the counter sees, the first three times.
            static int dumped = 0;
            if ((dumped < 3 && dumped++ < 3))
            {
                const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                printf("bots: smoke count look, slot %u:", slot);
                for (uint32_t w = 1; list && w < 128; w++)
                {
                    const uint32_t def = Guest::Read32(base, list + 4 * w);
                    if (def < 0x40000000u || def >= 0xA0000000u) continue;
                    const uint32_t text = Guest::Read32(base, def + 8);
                    if (text < 0x40000000u || text >= 0xC0000000u) continue;   // (0.5.757: copies' names lie at 0xAA..)
                    char name[40] = {};
                    for (int c = 0; c < 39; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; name[c] = ch; }
                    std::string lower(name); for (char& ch : lower) ch = char(tolower(static_cast<unsigned char>(ch)));
                    if (lower.find("smoke") == std::string::npos && lower.find("grenade") == std::string::npos && lower.find("stiel") == std::string::npos) continue;
                    const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
                    const bool owned = (Guest::Read32(base, record + 1056 + 4 * (w >> 5)) >> (w & 31)) & 1;
                    printf(" [%u %s owned %d clip#%u=%d spare#%u=%d]", w, name, owned ? 1 : 0, clip,
                           clip <= 128 ? int32_t(Guest::Read32(base, record + 4 * (172 + clip))) : -1, spare,
                           spare <= 128 ? int32_t(Guest::Read32(base, record + 4 * (80 + spare))) : -1);
                }
                printf(" | slots");
                for (int i = 0; i < 10; i++) printf(" %u", Guest::Read8(base, record + 1064 + i));
                printf("\n"); fflush(stdout);
            }
        }
    }
    g_smokeSearch.Check(slot, now, BotNadeAmmo(base, record, true));
    if (now < smokeUntil[slot]) { orders.buttons = smokeBit[slot] ? smokeBit[slot] : SmokeButton; orders.wantsFire = false; }   // 0.5.219: alone (no sprint)
    // Reload: the gun in hand's clip empty with rounds to spare (Justin never
    // saw a bot reload).
    {
        const uint32_t weapon = Guest::Read32(base, record + 164);
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t def = (weapon && weapon < 128 && list) ? Guest::Read32(base, list + 4 * weapon) : 0;
        if (def >= 0x40000000u && def < 0xA0000000u)
        {
            const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
            if (clip <= 128 && spare <= 128 && Guest::Read32(base, record + 4 * (172 + clip)) == 0 &&
                Guest::Read32(base, record + 4 * (80 + spare)) > 0 && now >= nextReload[slot])
            {
                reloadUntil[slot] = now + 150;
                nextReload[slot] = now + 2500;
                if ((told < 60 && told++ < 60)) { printf("bots: slot %u reloads weapon %u\n", slot, weapon); fflush(stdout); }
            }
        }
        if (now < reloadUntil[slot]) orders.buttons = (orders.buttons & ~1u) | ReloadButton;
    }

}

// 0.5.197: mounted guns and vehicles.
//
// Justin's 0.5.196 log: slot 9 stood 3.5 minutes in one spot, his view stuck
// at -177 while his aim asked for +80, his moves doing nothing. He had
// reloaded beside a mounted gun: reloading is also the use button, and the
// gun took him. The player state says so (+240 bit 0x100000: on a mounted
// gun, as the Kangaroo exits read it; entity +900: in a vehicle). Now a bot
// on a mounted gun fights from it while an enemy is in its reach, and gets
// off (the use button) when no one is, or when the one he wants is out of
// its arc. If getting off fails five times he is sent back to spawn. The
// spot is remembered: bots no longer reload there by accident, and campers
// and high-ground bots with no one about go to it on purpose.
//
// Vehicles (Justin: some bots should drive, ride, or man the gun while a bot
// drives): up to two vehicles a side get a bot crew, a driver and a gunner,
// from the bots nearest a free vehicle with no fight on. They walk to it and
// press use beside it; once in (entity +900), the seat is +1300 of the
// player state. The driver heads for the learnt map's outskirts or the War
// flag, steering with the stick and the view, backing up when he stops
// moving; the gunner and riders fight from their seats. A bot also hops in
// beside a human driver near him. After two minutes, or with the vehicle
// stuck, they get out. All of this is logged ("bots: vehicle ...") as the
// controls are not seen working yet.
static BotNavigation::Point g_mgSpots[16];
static int g_mgSpotCount = 0;
static bool NearMgSpot(BotNavigation::Point p, float radius)
{
    for (int i = 0; i < g_mgSpotCount; i++)
        if (BotNavigation::Distance(p, g_mgSpots[i]) < radius && std::fabs(p.z - g_mgSpots[i].z) < 60.0f) return true;
    return false;
}
static void RememberMgSpot(BotNavigation::Point p)
{
    if (NearMgSpot(p, 60.0f)) return;
    g_mgSpots[g_mgSpotCount % 16] = p;
    if (g_mgSpotCount < 16) g_mgSpotCount++;
    printf("bots: a mounted gun learnt at %.0f %.0f %.0f (%d known)\n", p.x, p.y, p.z, g_mgSpotCount); fflush(stdout);
}
// 0.5.199: activate 0x20, activatereload 0x40 (the console's X: what a
// player presses to get in) and reload 0x400. 0.5.198's bots pressed 0x20 and
// 0x400 beside 40 motorcycles and never got on.
constexpr uint32_t UseButtons = 0x20 | 0x40 | 0x400;
constexpr uint32_t FlagUseButton = 0x40;   // 0.5.483: CTF flags (BotFlagUse)

struct BotVehicleInfo { uint32_t entity, record; BotNavigation::Point at; float yaw; char kind[24]; uint32_t driver, riders; };
static BotVehicleInfo g_vehicles[24];
static int g_vehicleCount = 0;
static uint32_t g_vehiclesAt = 0;
// The vehicle a player's entity is in (0: none) and his seat.
static uint32_t InVehicle(uint8_t* base, uint32_t entity, int32_t* seat = nullptr)
{
    const uint32_t link = entity ? Guest::Read32(base, entity + 900) : 0;
    const uint32_t vehicle = link ? Guest::Read32(base, link) : 0;
    if (seat) { const uint32_t client = Guest::Read32(base, entity + 512); *seat = client ? int32_t(Guest::Read32(base, client + 1300)) : -1; }
    return vehicle;
}
// The map's vehicles (the pool sub_82564520 walks: 0x82BCBDB4, records of
// 1872 bytes, their count at 0x82C281D0 + 3088; a record's handle at +368
// gives the entity through 0x82BD1FC0; its kind at +376).
static void ListVehicles(uint8_t* base, uint32_t now)
{
    if (now - g_vehiclesAt < 1000) return;
    g_vehiclesAt = now;
    g_vehicleCount = 0;
    const uint32_t pool = Guest::Read32(base, 0x82BCBDB4);
    const int records = int(Guest::Read16(base, 0x82C281D0 + 3088));
    for (int i = 0; pool && i < records && i < 64 && g_vehicleCount < 24; i++)
    {
        const uint32_t record = pool + 1872 * uint32_t(i);
        const uint32_t handle = Guest::Read32(base, record + 368), index = handle & 0x3FFF;
        if (index >= 4096) continue;
        const uint32_t entry = 0x82BD1FC0 + 8 * index;
        if ((handle >> 14) != Guest::Read32(base, entry + 4)) continue;
        const uint32_t entity = Guest::Read32(base, entry);
        if (!entity) continue;
        BotVehicleInfo& v = g_vehicles[g_vehicleCount];
        v.entity = entity; v.record = record;
        v.at = { ReadFloat(base, entity + 336), ReadFloat(base, entity + 340), ReadFloat(base, entity + 344) };
        v.yaw = ReadFloat(base, entity + 356);
        if (!BotNavigation::Finite(v.at)) continue;
        memset(v.kind, 0, sizeof(v.kind));
        const int kind = int(int16_t(Guest::Read16(base, record + 376)));
        const uint32_t def = kind >= 0 && kind < 32 ? Guest::Read32(base, 0x82B2DC90 + 4 * uint32_t(kind)) : 0;
        for (int c = 0; def && c < 23; c++) { const char ch = char(Guest::Read8(base, def + c)); if (!ch) break; v.kind[c] = ch; }
        v.driver = 0; v.riders = 0;
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (uint32_t other = 0; entities && other < ActiveSlots(); other++)
        {
            const uint32_t them = Guest::Read32(base, entities + 4 * (other + 1));
            int32_t seat = -1;
            if (!them || InVehicle(base, them, &seat) != entity) continue;
            v.riders++;
            if (seat == 0) v.driver = them;
        }
        g_vehicleCount++;
    }
}

// 0.5.199: his view on the vehicle's middle, a little down (the use check
// goes by where he looks).
static void LookAtVehicle(uint8_t* base, uint32_t slot, uint32_t record, uint32_t vehicle)
{
    const float eye = BotAim::EyeHeight(ReadFloat(base, record + 220));
    const float dx = ReadFloat(base, vehicle + 336) - ReadFloat(base, record), dy = ReadFloat(base, vehicle + 340) - ReadFloat(base, record + 4);
    const float dz = ReadFloat(base, vehicle + 344) + 30.0f - (ReadFloat(base, record + 8) + eye);
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz)) return;
    const float want = BotAim::Pitch(dx, dy, dz) * PitchSign();
    g_pitch[slot] += std::clamp(want - g_pitch[slot], -5.0f, 5.0f);
    // 0.5.841: and his facing, every frame (a third of what is left, at most 8 degrees: the view
    // lags the orders a frame or two). Before, tank_fire turned him 30 degrees once a try, so he
    // pressed use looking past the tank (sherman1-4: no Sherman in five, either side, while the
    // user got into one by hand: the game never even checked him against it).
    const float bearing = atan2f(dy, dx) * 57.29578f;
    g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)) * 0.33f, -8.0f, 8.0f);
    g_lookVehicleUntil[slot] = GetTickCount() + 250;
}

struct BotSeat {
    uint32_t entity = 0;
    // mounted guns
    uint32_t mountedSince = 0, lastSawFromGun = 0, offTries = 0, nextOff = 0, useUntil = 0, mgCooldown = 0;
    // vehicles
    uint32_t vehicle = 0;          // the one he is crew for (0: none)
    bool driver = false, inside = false;
    uint32_t claimedAt = 0, enterTries = 0, nextEnter = 0, boardedAt = 0, cooldown = 0;
    uint32_t moveCheck = 0, reverseUntil = 0, leaveTries = 0, nextLeave = 0;
    BotNavigation::Point lastVehicleAt{}, driveTo{};
    bool haveDriveTo = false;
};
static BotSeat g_seat[MaxClients];

// 0.5.796: "tank_fire [seconds]" (console or COD3_CMD), for tests: tests
// cannot fire a tank themselves, and bots get into one only by luck (they go
// for one, then give up in a fight). The first living bot is put (the game's
// teleport, sub_8255E118) beside the free tank nearest the host, presses use
// until he is in, then fires the tank's own gun (the fire button, held 0.3 s
// every 3 s) at the ground ~700 ahead of it, for the seconds asked (60 by
// default). Nothing is made by hand: the game's own shot, shell and events.
// Logged "bots: tank_fire ...".
// 0.5.799: a second bot rides as the gunner (the tank's machine gun, seat 1)
// once the first is in: "tank_fire [seconds] [crew 1|2]" (2 by default).
static std::atomic<int> g_tankFireAsk{0}, g_tankFireCrew{2};
struct TankFireState { uint32_t slot = ~0u, entity = 0, vehicle = 0, until = 0, nextUse = 0, useUntil = 0, nextShot = 0, shotUntil = 0, tries = 0, shots = 0; bool inside = false; };
static TankFireState g_tankFire, g_tankGunner;
static std::vector<uint32_t> g_tankFireFailed;   // 0.5.800: tanks he could not get into
void BotsTankFire(int seconds, int crew)
{
    g_tankFireCrew.store(crew == 1 ? 1 : 2);
    g_tankFireAsk.store(seconds > 0 ? seconds : 60);
    printf("bots: tank_fire %d s asked, crew %d\n", seconds > 0 ? seconds : 60, crew == 1 ? 1 : 2);
    fflush(stdout);
}
static bool IsTankKind(const char* kind)
{
    std::string k = kind;
    for (char& c : k) c = char(tolower(static_cast<unsigned char>(c)));
    return k.find("panzer") != std::string::npos || k.find("sherman") != std::string::npos || k.find("tiger") != std::string::npos;
}
// 0.5.798: a vehicle's own update (sub_82582280, the vehicle r3, its time
// r4; called from sub_82591B40) fires its gun (events 25, 26, the shell)
// only for a rider whose move-lock byte (0x82C73024 + his client's +2804) is
// clear and whose buttons (his client's +1940) have fire. That byte is set
// for every bot (a networked game's "not this machine's player"; ThinkBots
// clears it only for his own think), so a bot in a tank never fired its
// cannon (796's tankfire2: in a Panzer, fire held 41 times, no tank event;
// tankfire1's turret events were the HOST's, "host buttons 00000001"). While a
// vehicle is updated the bots' bytes are clear, as for their thinks.
extern "C" PPC_FUNC(__imp__sub_82582280);
PPC_FUNC(sub_82582280)
{
    uint32_t bytes[MaxClients * 2]; uint8_t was[MaxClients * 2]; int n = 0;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); entities && slot < ActiveSlots() && slot < MaxClients; slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t e = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        uint32_t index[2] = { slot, r ? Guest::Read32(base, r + 2804) : 0xFFFFFFFFu };
        for (uint32_t i : index)
        {
            if (i >= 64) continue;
            bool dup = false; for (int k = 0; k < n; k++) if (bytes[k] == MoveLocked + i) dup = true;
            if (dup || n >= int(MaxClients * 2)) continue;
            bytes[n] = MoveLocked + i; was[n] = Guest::Read8(base, bytes[n]); Guest::Write8(base, bytes[n], 0); n++;
        }
    }
    __imp__sub_82582280(ctx, base);
    for (int k = n - 1; k >= 0; k--) Guest::Write8(base, bytes[k], was[k]);
}

// true: this bot is the tank_fire bot this frame (his orders are set here)
static bool BotTankFire(PPCContext& ctx, uint8_t* base, uint32_t slot, uint32_t self, uint32_t record, BotOrders& orders, uint32_t inVehicle, int32_t seatNo)
{
    using namespace BotNavigation;
    TankFireState& t = g_tankFire;
    const uint32_t now = GetTickCount();
    const int asked = g_tankFireAsk.exchange(0);
    if (asked > 0)
    {
        t = {}; g_tankGunner = {}; g_tankFireFailed.clear();
        t.until = now + uint32_t(asked) * 1000u;
    }
    if (!t.until) return false;
    if (now >= t.until)
    {
        printf("bots: tank_fire over: %u cannon shots, %u machine gun bursts asked\n", t.shots, g_tankGunner.shots); fflush(stdout);
        t = {}; g_tankGunner = {};
        return false;
    }
    if (t.slot == ~0u) { t.slot = slot; t.entity = self; printf("bots: tank_fire: slot %u is the driver (the cannon)\n", slot); fflush(stdout); }
    if (t.slot != slot)
    {
        // 0.5.799: the gunner: once the driver is in, into the same tank (seat 1)
        TankFireState& g = g_tankGunner;
        if (g_tankFireCrew.load() < 2 || !t.inside || !t.vehicle) return false;
        // 0.5.806: a bot of the driver's side (seelowcheck1: the gunner could not
        // get in, 16 tries; in tankmortar2 one of his side did)
        if (g.slot == ~0u && BotSide(base, slot) != BotSide(base, t.slot)) return false;
        if (g.slot == ~0u) { g.slot = slot; g.entity = self; printf("bots: tank_fire: slot %u is the gunner (the machine gun)\n", slot); fflush(stdout); }
        if (g.slot != slot) return false;
        if (g.entity != self) { g.entity = self; g.inside = false; g.tries = 0; }
        ListVehicles(base, now);
        const BotVehicleInfo* tank = nullptr;
        for (int i = 0; i < g_vehicleCount; i++) if (g_vehicles[i].entity == t.vehicle) tank = &g_vehicles[i];
        orders.forward = orders.right = 0;
        orders.buttons &= ~(1u | UseButtons | SprintButton | StanceButton);
        orders.wantsFire = false;
        if (inVehicle)
        {
            if (!g.inside) { g.inside = true; g.nextShot = now + 1000; printf("bots: tank_fire: slot %u is in %s, seat %d: fires the machine gun\n", slot, tank ? tank->kind : "?", seatNo); fflush(stdout); }
            if (tank)
            {
                // the same ground ~700 ahead of the tank
                const float yaw = tank->yaw * 0.017453293f;
                const float gx = tank->at.x + 700.0f * cosf(yaw), gy = tank->at.y + 700.0f * sinf(yaw);
                const float eye = BotAim::EyeHeight(ReadFloat(base, record + 220));
                const float dx = gx - ReadFloat(base, record), dy = gy - ReadFloat(base, record + 4), dz = tank->at.z - (ReadFloat(base, record + 8) + eye);
                if (std::isfinite(dx) && std::isfinite(dy) && std::isfinite(dz))
                {
                    const float bearing = atan2f(dy, dx) * 57.29578f;
                    g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)), -3.0f, 3.0f);
                    const float want = BotAim::Pitch(dx, dy, dz) * PitchSign();
                    g_pitch[slot] += std::clamp(want - g_pitch[slot], -3.0f, 3.0f);
                    g_wantYaw[slot] = bearing; g_wantPitch[slot] = want;
                }
            }
            // bursts: 1.2 s of fire every 4 s
            if (now >= g.nextShot) { g.shotUntil = now + 1200; g.nextShot = now + 4000; g.shots++; if (g.shots <= 40) { printf("bots: tank_fire: machine gun burst %u\n", g.shots); fflush(stdout); } }
            if (now < g.shotUntil) { orders.buttons |= 1u; orders.wantsFire = true; }
            return true;
        }
        if (g.inside) { g.inside = false; g.tries = 0; printf("bots: tank_fire: slot %u (gunner) is out: in again\n", slot); fflush(stdout); }
        if (!tank) return true;
        if (now < g.useUntil) { orders.buttons |= UseButtons; LookAtVehicle(base, slot, record, tank->entity); return true; }
        if (now < g.nextUse) { LookAtVehicle(base, slot, record, tank->entity); return true; }
        if (g.tries >= 32) { if (g.tries++ == 32) { printf("bots: tank_fire: slot %u could not get in as the gunner\n", slot); fflush(stdout); } return true; }
        static uint32_t gunnerPlace = 0;
        if (!gunnerPlace) gunnerPlace = Guest::AllocatePhysical(0x20);
        // 0.5.845: eight places round it at 95, then 130, as the driver's (0.5.844)
        const float side = float((g.tries / 2) % 8) * 0.78539816f;
        const float reach = (g.tries / 16) % 2 ? 130.0f : 95.0f;
        const float yaw = tank->yaw * 0.017453293f + side;
        const float at[3] = { tank->at.x + reach * cosf(yaw), tank->at.y + reach * sinf(yaw), tank->at.z + 8.0f };
        if (g.tries % 2 == 0)
        {
            auto putF = [&](uint32_t a, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, a, w); };
            putF(gunnerPlace, at[0]); putF(gunnerPlace + 4, at[1]); putF(gunnerPlace + 8, at[2]);
            PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
            place.r3.u64 = self; place.r4.u64 = gunnerPlace; place.lr = 0;
            Guest::Lookup(0x8255E118)(place, base);
        }
        const float bearing = atan2f(tank->at.y - ReadFloat(base, record + 4), tank->at.x - ReadFloat(base, record)) * 57.29578f;
        g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)), -30.0f, 30.0f);
        LookAtVehicle(base, slot, record, tank->entity);
        g.tries++;
        g.useUntil = now + 400; g.nextUse = g.useUntil + 1200;
        printf("bots: tank_fire: slot %u (gunner) presses use at %s (try %u)\n", slot, tank->kind, g.tries); fflush(stdout);
        return true;
    }
    if (t.entity != self) { t.entity = self; t.vehicle = 0; t.inside = false; t.tries = 0; printf("bots: tank_fire: slot %u has a new body: again\n", slot); fflush(stdout); }
    ListVehicles(base, now);
    const BotVehicleInfo* tank = nullptr;
    for (int i = 0; i < g_vehicleCount; i++) if (g_vehicles[i].entity == (inVehicle ? inVehicle : t.vehicle)) tank = &g_vehicles[i];
    // pick: the free tank nearest the host (so the host sees it)
    if (!t.vehicle || !tank)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
        const Point hostAt = host ? Point{ ReadFloat(base, host + 336), ReadFloat(base, host + 340), ReadFloat(base, host + 344) } : Point{};
        float best = 1e9f;
        tank = nullptr;
        // 0.5.802: his own side's first (a Sherman for the Allies, a Panzer IV
        // for the Axis): mervtank1's Axis bot pressed use at a Sherman 12 times
        const bool allies = BotSide(base, slot) == 1;
        for (int i = 0; i < g_vehicleCount; i++)
        {
            const BotVehicleInfo& v = g_vehicles[i];
            if (!IsTankKind(v.kind) || v.riders) continue;
            if (std::find(g_tankFireFailed.begin(), g_tankFireFailed.end(), v.entity) != g_tankFireFailed.end()) continue;   // 0.5.800
            std::string kind = v.kind;
            for (char& c : kind) c = char(tolower(static_cast<unsigned char>(c)));
            // 0.5.811: the Panzer IV first whatever his side: on Seelow no bot has got
            // into a Sherman (tankboom1, seelowcheck1-3: twelve tries each), while
            // the Allied and Axis bots alike get into a Panzer IV
            (void)allies;
            // 0.5.839: COD3_TANK_FIRE_KIND=sherman (or panzer): that kind first (the Shermans' test)
            static const char* const wantKind = getenv("COD3_TANK_FIRE_KIND");
            const bool own = wantKind ? kind.find(wantKind) != std::string::npos : kind.find("sherman") == std::string::npos;
            const float d = Distance(hostAt, v.at) + (own ? 0.0f : 100000.0f);
            if (d < best) { best = d; tank = &v; }
        }
        if (best >= 100000.0f) best -= 100000.0f;
        static uint32_t toldNone = 0;
        if (!tank) { if (now - toldNone > 5000) { toldNone = now; printf("bots: tank_fire: no free tank (%d vehicles)\n", g_vehicleCount); fflush(stdout); } return false; }
        t.vehicle = tank->entity; t.tries = 0; t.nextUse = 0;
        printf("bots: tank_fire: slot %u (side %d) takes %s at %.0f %.0f %.0f (%.0f from the host)\n", slot, BotSide(base, slot), tank->kind, tank->at.x, tank->at.y, tank->at.z, best); fflush(stdout);
    }
    orders.forward = orders.right = 0;
    orders.buttons &= ~(1u | UseButtons | SprintButton | StanceButton);
    if (inVehicle)
    {
        if (!t.inside)
        {
            t.inside = true; t.nextShot = now + 1500;
            printf("bots: tank_fire: slot %u is in %s, seat %d: fires\n", slot, tank ? tank->kind : "?", seatNo); fflush(stdout);
        }
        // aim at the ground ~700 ahead of the tank
        if (tank)
        {
            const float yaw = tank->yaw * 0.017453293f;
            const float gx = tank->at.x + 700.0f * cosf(yaw), gy = tank->at.y + 700.0f * sinf(yaw);
            const float eye = BotAim::EyeHeight(ReadFloat(base, record + 220));
            const float dx = gx - ReadFloat(base, record), dy = gy - ReadFloat(base, record + 4), dz = tank->at.z - (ReadFloat(base, record + 8) + eye);
            if (std::isfinite(dx) && std::isfinite(dy) && std::isfinite(dz))
            {
                const float bearing = atan2f(dy, dx) * 57.29578f;
                g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)), -3.0f, 3.0f);
                const float want = BotAim::Pitch(dx, dy, dz) * PitchSign();
                g_pitch[slot] += std::clamp(want - g_pitch[slot], -3.0f, 3.0f);
            }
        }
        if (now >= t.nextShot)
        {
            t.shotUntil = now + 300; t.nextShot = now + 3000; t.shots++;
            printf("bots: tank_fire: shot %u (fire held 0.3 s): view pitch %.1f yaw %.1f, wanted pitch %.1f; tank at %.0f %.0f %.0f yaw %.0f\n", t.shots,
                   ReadFloat(base, record + 204), ReadFloat(base, record + 208), g_pitch[slot], tank ? tank->at.x : 0.0f, tank ? tank->at.y : 0.0f, tank ? tank->at.z : 0.0f, tank ? tank->yaw : 0.0f);
            fflush(stdout);
        }
        if (now < t.shotUntil) orders.buttons |= 1u;
        return true;
    }
    if (t.inside) { t.inside = false; t.tries = 0; printf("bots: tank_fire: slot %u is out of the tank: in again\n", slot); fflush(stdout); }
    if (!tank) return true;
    if (now < t.useUntil)
    {
        orders.buttons |= UseButtons; LookAtVehicle(base, slot, record, tank->entity);
        // 0.5.842: where he really is and looks while pressing (sherman5: still never in)
        static uint32_t toldTry = ~0u;
        if (toldTry != t.tries && t.useUntil - now < 200)
        {
            toldTry = t.tries;
            const float bx = ReadFloat(base, record), by = ReadFloat(base, record + 4), bz = ReadFloat(base, record + 8);
            const float bearing = atan2f(tank->at.y - by, tank->at.x - bx) * 57.29578f;
            printf("bots: tank_fire: slot %u pressing use: at %.0f %.0f %.0f (%.0f from the tank's middle, %.0f below/above it), view yaw %.0f (the tank at %.0f), pitch %.0f, buttons %08X\n",
                   slot, bx, by, bz, sqrtf((tank->at.x - bx) * (tank->at.x - bx) + (tank->at.y - by) * (tank->at.y - by)), bz - tank->at.z,
                   ReadFloat(base, record + 208), bearing, ReadFloat(base, record + 204), orders.buttons);
            fflush(stdout);
        }
        return true;
    }
    if (now < t.nextUse) { LookAtVehicle(base, slot, record, tank->entity); return true; }
    // beside it (the side the try says), then use
    if (t.tries >= 32)   // 0.5.844: 16 places, two presses each (was 12)
    {
        // 0.5.800: not that one again (tankmortar1: the same Sherman ten times over)
        g_tankFireFailed.push_back(tank->entity);
        if (g_tankFireFailed.size() >= 6) g_tankFireFailed.erase(g_tankFireFailed.begin());
        printf("bots: tank_fire: slot %u could not get into %s: another\n", slot, tank->kind); fflush(stdout); t.vehicle = 0; return true;
    }
    static uint32_t placeAt = 0;
    if (!placeAt) placeAt = Guest::AllocatePhysical(0x20);
    // 0.5.844: eight places round it (sides and corners) at 95 and then 130 (was the four sides at 110:
    // sherman7 faced the tank at each and was never let in; the user got into one from its rear corner)
    const float side = float((t.tries / 2) % 8) * 0.78539816f;
    const float reach = (t.tries / 16) % 2 ? 130.0f : 95.0f;
    const float yaw = tank->yaw * 0.017453293f + side;
    const float at[3] = { tank->at.x + reach * cosf(yaw), tank->at.y + reach * sinf(yaw), tank->at.z + 8.0f };
    if (t.tries % 2 == 0)
    {
        auto putF = [&](uint32_t a, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, a, w); };
        putF(placeAt, at[0]); putF(placeAt + 4, at[1]); putF(placeAt + 8, at[2]);
        PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        place.r3.u64 = self; place.r4.u64 = placeAt; place.lr = 0;
        Guest::Lookup(0x8255E118)(place, base);
    }
    const float bearing = atan2f(tank->at.y - ReadFloat(base, record + 4), tank->at.x - ReadFloat(base, record)) * 57.29578f;
    g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)), -30.0f, 30.0f);
    LookAtVehicle(base, slot, record, tank->entity);
    t.tries++;
    t.useUntil = now + 400; t.nextUse = t.useUntil + 1200;
    printf("bots: tank_fire: slot %u presses use at %s (try %u, at %.0f %.0f %.0f)\n", slot, tank->kind, t.tries, at[0], at[1], at[2]); fflush(stdout);
    return true;
}

static int BotsCrewing(uint8_t* base, uint32_t side)
{
    int count = 0; uint32_t seen[MaxClients]; int n = 0;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t s = FirstBotSlot(); s < ActiveSlots(); s++)
    {
        if (!g_bot[s] || !g_seat[s].vehicle || !entities) continue;
        const uint32_t e = Guest::Read32(base, entities + 4 * (s + 1));
        if (!e || Side(base, e) != side) continue;
        bool dup = false; for (int i = 0; i < n; i++) if (seen[i] == g_seat[s].vehicle) dup = true;
        if (!dup && n < int(MaxClients)) { seen[n++] = g_seat[s].vehicle; count++; }
    }
    return count;
}

// After Think and BotExtras: what a mounted gun or a vehicle changes in his
// orders this frame.
// 0.5.199: serverTime is the clock Think and TacticalMove keep (the server's,
// from the commands). 0.5.198 handed TacticalMove the PC's clock here: the
// net-progress check saw a bot "stuck" twice a frame while he walked to a
// vehicle (30,847 log lines on mp_guns) and his real stuck count kept
// resetting, so the escalation never ran.
static void BotSeats(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders, uint32_t serverTime)
{
    using namespace BotNavigation;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    BotSeat& seat = g_seat[slot];
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) { if (seat.entity != self) seat = {}; seat.entity = self; seat.vehicle = 0; seat.inside = false; return; }
    if (seat.entity != self) { seat = {}; seat.entity = self; }
    const uint32_t now = GetTickCount();
    const Point feet{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    const bool sees = g_tactical[slot].visible && g_seeDist[slot] < 5000.0f;
    // 0.5.326 (Justin: the Demolitionist drives about the map): he takes a
    // vehicle in a fight too, unless someone is within 500 of him; drives it
    // to a far part of the map (another style of place each time, over 1500
    // off), gets out there to fight and later takes one again.
    const bool demo = BotIsDemolitionist(slot);
    const bool closeFight = g_tactical[slot].visible && g_seeDist[slot] < 500.0f;
    const bool busy = demo ? closeFight : sees;
    int32_t seatNo = -1;
    const uint32_t inVehicle = InVehicle(base, self, &seatNo);
    const bool mounted = (Guest::Read32(base, record + 240) & 0x100000u) != 0 && !inVehicle;
    if (BotTankFire(ctx, base, slot, self, record, orders, inVehicle, seatNo)) return;   // 0.5.796: the test's tank gunner
    if (now < seat.useUntil)
    {
        orders.buttons = (orders.buttons & ~1u) | UseButtons; orders.forward = orders.right = 0;
        if (seat.vehicle && !seat.inside) LookAtVehicle(base, slot, record, seat.vehicle);
        return;
    }

    // --- a mounted gun ---
    if (mounted)
    {
        if (!seat.mountedSince) { seat.mountedSince = now; seat.lastSawFromGun = now; seat.offTries = 0; RememberMgSpot(feet);
            printf("bots: slot %u is on a mounted gun at %.0f %.0f %.0f\n", slot, feet.x, feet.y, feet.z); fflush(stdout); }
        const bool canReach = sees && std::fabs(g_aimOff[slot]) < 30.0f;
        if (canReach) seat.lastSawFromGun = now;
        orders.forward = orders.right = 0;
        const bool leave = now - seat.lastSawFromGun > (sees ? 3000u : 6000u) || now - seat.mountedSince > 60000;
        if (leave && now >= seat.nextOff)
        {
            if (++seat.offTries > 5)
            {
                printf("bots: slot %u could not get off the mounted gun: back to spawn\n", slot); fflush(stdout);
                Respawn(ctx, base, self, BotSide(base, slot) == 1);
                seat = {}; seat.entity = self;
                return;
            }
            seat.useUntil = now + 150 + 250 * seat.offTries;
            seat.nextOff = seat.useUntil + 1500;
            seat.mgCooldown = now + 60000;
            printf("bots: slot %u gets off the mounted gun (%s, try %u)\n", slot, sees ? "his target is out of its reach" : "no one to shoot", seat.offTries);
            fflush(stdout);
        }
        return;
    }
    // 0.5.198: twenty stuck checks in a row (70 s without getting anywhere
    // while he wants to move) and nothing has freed him: back to spawn.
    if (g_tactical[slot].progress.strikes >= 20 && !inVehicle)
    {
        printf("bots: slot %u has been stuck %u times at %.0f %.0f %.0f: back to spawn\n", slot, g_tactical[slot].progress.strikes, feet.x, feet.y, feet.z);
        fflush(stdout);
        g_tactical[slot].progress.Reset();
        Respawn(ctx, base, self, BotSide(base, slot) == 1);
        return;
    }
    if (seat.mountedSince) { printf("bots: slot %u is off the mounted gun after %u s\n", slot, (now - seat.mountedSince) / 1000); fflush(stdout); seat.mountedSince = 0; }
    // Near a known mounted gun: no reload there by accident; a camper or a
    // high-ground bot with no one about takes it on purpose.
    if (NearMgSpot(feet, 90.0f))
    {
        orders.buttons &= ~0x400u;
        const bool keen = g_role[slot] == RoleCamper || g_explore[slot].style == MapKnowledge::Style::High;
        if (keen && !sees && !inVehicle && now >= seat.mgCooldown && NearMgSpot(feet, 50.0f))
        {
            seat.useUntil = now + 300; seat.mgCooldown = now + 45000;
            printf("bots: slot %u takes the mounted gun here\n", slot); fflush(stdout);
            return;
        }
    }

    // --- vehicles ---
    if (!Settings::Get().botVehicles) return;
    ListVehicles(base, now);
    const BotVehicleInfo* mine = nullptr;
    for (int i = 0; i < g_vehicleCount; i++) if (g_vehicles[i].entity == (inVehicle ? inVehicle : seat.vehicle)) mine = &g_vehicles[i];
    if (inVehicle)
    {
        if (!seat.inside)
        {
            seat.inside = true; seat.boardedAt = now; seat.vehicle = inVehicle; seat.moveCheck = now; seat.leaveTries = 0;
            seat.driver = seatNo == 0;
            seat.lastVehicleAt = mine ? mine->at : feet; seat.haveDriveTo = false;
            printf("bots: vehicle: slot %u is in %s, seat %d (%s)\n", slot, mine ? mine->kind : "?", seatNo, seat.driver ? "driving" : "riding"); fflush(stdout);
        }
        bool leave = now - seat.boardedAt > 120000;
        if (seat.driver && mine)
        {
            // Where to: the War flag, else a spot on the outskirts.
            if (!seat.haveDriveTo || Distance(mine->at, seat.driveTo) < 300.0f)
            {
                const bool arrived = seat.haveDriveTo;
                seat.haveDriveTo = false;
                if (demo && arrived && now - seat.boardedAt > 25000)
                { leave = true; printf("bots: vehicle: slot %u (demolitionist) is there: gets out to fight\n", slot); fflush(stdout); }
                if (demo && g_mapKnow.Ready())
                {
                    std::vector<Point> none;
                    static const MapKnowledge::Style styles[4] = { MapKnowledge::Style::Away, MapKnowledge::Style::High, MapKnowledge::Style::Outskirts, MapKnowledge::Style::Low };
                    static uint32_t turn[MaxClients] = {};
                    for (int tries = 0; tries < 8 && !seat.haveDriveTo; tries++)
                    {
                        const MapKnowledge::Style st = styles[(turn[slot]++) & 3];
                        const int32_t n = g_mapKnow.Pick(st, mine->at, slot * 977u + now / 1000u + uint32_t(tries) * 131u, none);
                        if (n >= 0 && Distance(mine->at, g_mapKnow.nodes[size_t(n)].at) > 1500.0f) { seat.driveTo = g_mapKnow.nodes[size_t(n)].at; seat.haveDriveTo = true; }
                    }
                }
                if (seat.haveDriveTo) {}
                else if (g_war.valid && now - g_war.when < 10000 && Distance(mine->at, { g_war.at[0], g_war.at[1], g_war.at[2] }) > 600.0f)
                { seat.driveTo = { g_war.at[0], g_war.at[1], g_war.at[2] }; seat.haveDriveTo = true; }
                else if (g_mapKnow.Ready())
                {
                    std::vector<Point> none;
                    const int32_t n = g_mapKnow.Pick(MapKnowledge::Style::Outskirts, mine->at, slot * 977u + now / 1000u, none);
                    if (n >= 0) { seat.driveTo = g_mapKnow.nodes[size_t(n)].at; seat.haveDriveTo = true; }
                }
                if (seat.haveDriveTo) { printf("bots: vehicle: slot %u drives to %.0f %.0f\n", slot, seat.driveTo.x, seat.driveTo.y); fflush(stdout); }
            }
            orders.buttons &= ~(1u | UseButtons | SprintButton);
            if (seat.haveDriveTo)
            {
                const float bearing = atan2f(seat.driveTo.y - mine->at.y, seat.driveTo.x - mine->at.x) * 57.29578f;
                const float turn = AngleDifference(bearing, mine->yaw);
                const bool reversing = now < seat.reverseUntil;
                orders.forward = reversing ? -127 : (std::fabs(turn) > 100.0f ? 70 : 127);
                const float steer = std::clamp(-turn * 2.5f, -127.0f, 127.0f);
                orders.right = int8_t(reversing ? -steer : steer);
                g_yaw[slot] += std::clamp(AngleDifference(bearing, g_yaw[slot]), -3.0f, 3.0f);
                // Stopped (under 40 units in 2.5 s): back up for 1.5 s, the wheel the other way.
                if (now - seat.moveCheck >= 2500)
                {
                    const float moved = Distance(mine->at, seat.lastVehicleAt);
                    if (moved < 40.0f && !reversing)
                    {
                        seat.reverseUntil = now + 1500;
                        static uint32_t told[MaxClients] = {};
                        if (told[slot]++ < 20) { printf("bots: vehicle: slot %u is stuck: backs up\n", slot); fflush(stdout); }
                        if (++seat.leaveTries >= 6) leave = true;
                    }
                    else if (moved >= 40.0f) seat.leaveTries = 0;
                    seat.moveCheck = now; seat.lastVehicleAt = mine->at;
                }
            }
            else { orders.forward = orders.right = 0; }
        }
        else
        {
            // Riding or on the vehicle's gun: Think's aim and fire stay, no moves.
            orders.forward = orders.right = 0;
            orders.buttons &= ~(SprintButton | StanceButton | UseButtons);
            if (!sees && mine && !mine->driver && now - seat.boardedAt > 20000) leave = true;   // no driver and nothing to shoot
        }
        if (leave && now >= seat.nextLeave)
        {
            seat.useUntil = now + 300; seat.nextLeave = now + 2500;
            printf("bots: vehicle: slot %u gets out\n", slot); fflush(stdout);
        }
        return;
    }
    if (seat.inside)
    {
        printf("bots: vehicle: slot %u is out after %u s\n", slot, (now - seat.boardedAt) / 1000); fflush(stdout);
        seat.inside = false; seat.vehicle = 0; seat.cooldown = now + (demo ? 20000 : 60000);
    }
    const uint32_t side = Side(base, self);
    // Not crew yet: a free vehicle near, nothing to fight, a couple of crews a side.
    if (!seat.vehicle && !busy && now >= seat.cooldown && g_vehicleCount > 0 && (demo || (slot % 3) != 2))
    {
        const BotVehicleInfo* best = nullptr; float bestDistance = demo ? 2600.0f : 1800.0f; bool asDriver = false;
        for (int i = 0; i < g_vehicleCount; i++)
        {
            const BotVehicleInfo& v = g_vehicles[i];
            int crew = 0; bool driverClaimed = v.driver != 0;
            for (uint32_t o = FirstBotSlot(); o < ActiveSlots(); o++) if (o != slot && g_bot[o] && g_seat[o].vehicle == v.entity) { crew++; if (g_seat[o].driver) driverClaimed = true; }
            if (crew + int(v.riders) >= 2) continue;
            // A human driving: hop in beside him (within 600).
            const bool humanDriver = v.driver != 0;
            const float d = Distance(feet, v.at);
            if (humanDriver ? d > 600.0f : d > bestDistance) continue;
            if (!humanDriver && !driverClaimed && !demo && BotsCrewing(base, side) >= 2) continue;
            if (demo && driverClaimed && !humanDriver) continue;   // he drives
            best = &v; bestDistance = d; asDriver = !driverClaimed;
        }
        if (best)
        {
            seat.vehicle = best->entity; seat.driver = asDriver; seat.claimedAt = now; seat.enterTries = 0; seat.nextEnter = 0;
            mine = best;   // 0.5.198: 0.5.197 looked for it before the claim and gave up at once ("gone") every time
            printf("bots: vehicle: slot %u goes for %s %.0f away as %s\n", slot, best->kind, bestDistance, asDriver ? "driver" : "gunner"); fflush(stdout);
        }
    }
    if (!seat.vehicle) return;
    if (!mine || busy || now - seat.claimedAt > (demo ? 60000u : 45000u))
    {
        printf("bots: vehicle: slot %u gives up on his vehicle (%s)\n", slot, !mine ? "gone" : busy ? "a fight" : "took too long"); fflush(stdout);
        seat.vehicle = 0; seat.cooldown = now + (busy ? (demo ? 8000 : 15000) : (demo ? 30000 : 90000));
        return;
    }
    // Walk to it; beside it, face it and press use.
    // 0.5.846: to its nearer rear corner (100 from its middle, 135 degrees round from its front), not
    // its middle: the hull stopped him short of 75 from the middle, and a tank lets you in from its
    // corners (the user's Sherman, sherman8/9's: the sides at 110 never did)
    const float d = Distance(feet, mine->at);
    const float bearing = atan2f(mine->at.y - feet.y, mine->at.x - feet.x) * 57.29578f;
    float corner[2];
    {
        const float a1 = (mine->yaw + 135.0f) * 0.017453293f, a2 = (mine->yaw - 135.0f) * 0.017453293f;
        const float c1[2] = { mine->at.x + 100.0f * cosf(a1), mine->at.y + 100.0f * sinf(a1) };
        const float c2[2] = { mine->at.x + 100.0f * cosf(a2), mine->at.y + 100.0f * sinf(a2) };
        const float d1 = hypotf(c1[0] - feet.x, c1[1] - feet.y), d2 = hypotf(c2[0] - feet.x, c2[1] - feet.y);
        corner[0] = d1 <= d2 ? c1[0] : c2[0]; corner[1] = d1 <= d2 ? c1[1] : c2[1];
    }
    float toCorner = hypotf(corner[0] - feet.x, corner[1] - feet.y);
    // 0.5.850: near it, the corner he heads for changes every 8 s he has not got in, nearest first, all
    // four (botveh6: 7 of 8 gave up 130-340 from the corner they had: it was walled or blocked)
    if (toCorner <= 300.0f)
    {
        static uint32_t nearSince[MaxClients] = {}, nearFor[MaxClients] = {};
        if (nearFor[slot] != seat.claimedAt) { nearFor[slot] = seat.claimedAt; nearSince[slot] = now; }
        const uint32_t shift = ((now - nearSince[slot]) / 8000u) % 4u;
        if (shift)
        {
            float c[4][2], dist[4]; int order[4] = { 0, 1, 2, 3 };
            const float turns[4] = { 135.0f, -135.0f, 45.0f, -45.0f };
            for (int k = 0; k < 4; k++)
            {
                const float a = (mine->yaw + turns[k]) * 0.017453293f;
                c[k][0] = mine->at.x + 100.0f * cosf(a); c[k][1] = mine->at.y + 100.0f * sinf(a);
                dist[k] = hypotf(c[k][0] - feet.x, c[k][1] - feet.y);
            }
            std::sort(order, order + 4, [&](int x, int y) { return dist[x] < dist[y]; });
            static uint32_t lastShift[MaxClients] = {};
            const int pick = order[shift];
            corner[0] = c[pick][0]; corner[1] = c[pick][1];
            toCorner = dist[pick];
            if (lastShift[slot] != shift) { lastShift[slot] = shift; printf("bots: vehicle: slot %u tries another corner of %s (%.0f %.0f, %.0f away)\n", slot, mine->kind, corner[0], corner[1], toCorner); fflush(stdout); }
        }
    }
    // 0.5.849: far off, Think walks him there by its route (his goal the corner); his orders are its
    if (toCorner > 300.0f)
    {
        g_vehicleGoal[slot][0] = corner[0]; g_vehicleGoal[slot][1] = corner[1]; g_vehicleGoal[slot][2] = mine->at.z;
        g_vehicleGoalAt[slot] = now;
        static uint32_t toldAt[MaxClients] = {};
        if (now - toldAt[slot] > 4000) { toldAt[slot] = now; printf("bots: vehicle: slot %u routes to %s: %.0f from its corner (%.0f %.0f), at %.0f %.0f %.0f\n", slot, mine->kind, toCorner, corner[0], corner[1], feet.x, feet.y, feet.z); fflush(stdout); }
        return;
    }
    if (toCorner > 35.0f && d > 75.0f)
    {
        // 0.5.851: round the tank, not through it: when the corner is more than 50 degrees round from
        // him (as seen from its middle), the next 45 degrees of a ring 170 out first (botveh7: bots
        // hovered 170-375 off, walking straight at a corner on its far side into the hull)
        float aim[2] = { corner[0], corner[1] };
        {
            const float mine0 = atan2f(feet.y - mine->at.y, feet.x - mine->at.x) * 57.29578f;
            const float cornerAt = atan2f(corner[1] - mine->at.y, corner[0] - mine->at.x) * 57.29578f;
            const float aroundDeg = AngleDifference(cornerAt, mine0);
            if (fabsf(aroundDeg) > 50.0f)
            {
                const float stepAt = (mine0 + (aroundDeg > 0 ? 45.0f : -45.0f)) * 0.017453293f;
                aim[0] = mine->at.x + 170.0f * cosf(stepAt); aim[1] = mine->at.y + 170.0f * sinf(stepAt);
            }
        }
        const float cornerBearing = atan2f(aim[1] - feet.y, aim[0] - feet.x) * 57.29578f;
        {
            static uint32_t toldAt[MaxClients] = {};
            if (now - toldAt[slot] > 4000) { toldAt[slot] = now; printf("bots: vehicle: slot %u on his way to %s: %.0f from its middle, %.0f from its corner (%.0f %.0f), at %.0f %.0f %.0f\n", slot, mine->kind, d, toCorner, corner[0], corner[1], feet.x, feet.y, feet.z); fflush(stdout); }
        }
        const float goal[3] = { aim[0], aim[1], feet.z };
        const float viewYaw = ReadFloat(base, record + 208);
        orders.forward = 127; orders.right = 0;
        if (d > 500.0f) orders.buttons |= SprintButton;
        g_yaw[slot] += std::clamp(AngleDifference(cornerBearing, viewYaw), -6.0f, 6.0f);
        // 0.5.848: Think's own steering left out meanwhile (botveh4: it turned him back to its own goal
        // as fast as this turned him to the tank: four bots walked from ~1000 to ~2000 away in 40 s)
        g_lookVehicleUntil[slot] = GetTickCount() + 250;
        TacticalMove(ctx, base, self, 0, slot, serverTime, int32_t(Guest::Read32(base, record + 304)), viewYaw, goal, false, orders);
        return;
    }
    orders.forward = orders.right = 0;
    g_yaw[slot] += std::clamp(AngleDifference(bearing, ReadFloat(base, record + 208)), -8.0f, 8.0f);
    LookAtVehicle(base, slot, record, mine->entity);
    if (now >= seat.nextEnter)
    {
        if (++seat.enterTries > 6)
        {
            printf("bots: vehicle: slot %u could not get into %s\n", slot, mine->kind); fflush(stdout);
            seat.vehicle = 0; seat.cooldown = now + 120000;
            return;
        }
        seat.useUntil = now + 200 + 200 * seat.enterTries;
        seat.nextEnter = seat.useUntil + 1500;
        printf("bots: vehicle: slot %u presses use at %s (%.0f away, try %u, view off %.0f, link %08X)\n", slot, mine->kind, d, seat.enterTries,
               AngleDifference(bearing, ReadFloat(base, record + 208)), Guest::Read32(base, self + 900)); fflush(stdout);
    }
}

// 0.5.199: the Support's ammo packs and the Medic's health packs. They are
// the class's "special" (_mp_loadout.bro: sub_821F3EC0 gives "mp_ammo" or
// "mp_ammo_axis", sub_821F3DF0 "mp_revive", both run HealthAmmoDispenser).
// A support or medic bot with two or more teammates within 450, no enemy in
// sight, takes the special out (the script's switch, mp_fixes.cpp), uses it
// (fire held 0.4 s), and goes back to his gun; once a minute at most (the
// ability's own timer decides whether it works). First try: the log says
// each step ("bots: pack ...").
bool MpFixesSwitchWeaponFor(PPCContext& ctx, uint8_t* base, uint32_t entity, const char* weapon);
// 0.5.277: mines (Justin: bots should lay more). A bot whose kit has the
// special mp_mine (owned: the weapon's bit at +1056 of his record) puts one
// down, the way BotDispense uses a pack: no enemy in sight, not in a
// vehicle, near the War flag (within 900) or a camper where he holds,
// every 35 to 55 s, three a life at most. It is taken out, fire is held
// 0.4 s (the mine's drop), and he goes back to his gun. Logged "bots: mine".
namespace { uint32_t WeaponNumber(uint8_t* base, const char* name); }   // (below)
static bool BotSwitch(PPCContext& ctx, uint8_t* base, uint32_t self, const char* weapon)
{
    if (!MpFixesSwitchWeaponFor(ctx, base, self, weapon)) return false;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = 0; entities && slot < MaxClients; slot++)
        if (Guest::Read32(base, entities + 4 * (slot + 1)) == self)
        {
            const uint32_t n = WeaponNumber(base, weapon);
            g_switchTo[slot] = (n && n < 128) ? n : 0;
            g_switchUntil[slot] = GetTickCount() + 2500;
            break;
        }
    return true;
}
void MpFixesGiveMines(PPCContext& ctx, uint8_t* base, uint32_t entity);   // mp_fixes.cpp (0.5.278)
static void BotLayMine(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders)
{
    struct Lay { uint32_t entity = 0, stage = 0, at = 0, next = 0, laid = 0, since = 0; bool given = false; char back[32] = {}; };   // reset each death
    static Lay lay[MaxClients];
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    Lay& m = lay[slot];
    if (m.entity != self || !record || int32_t(Guest::Read32(base, record + 304)) <= 0) { const uint32_t e = self; m = {}; m.entity = e; return; }
    if (!g_botLoadoutAssigned[slot]) return;
    static uint32_t mineId = 0;
    if (!mineId) mineId = WeaponNumber(base, "mp_mine");
    if (!mineId || mineId >= 128) return;
    bool owns = (Guest::Read32(base, record + 1056 + 4 * (mineId >> 5)) & (1u << (mineId & 31))) != 0;
    const uint32_t now = GetTickCount();
    // 0.5.278: 277's bots never owned one (no class's special is a mine):
    // anti-armor bots, and campers whose special is not a pack (not support
    // or medic), are given mines once a life, 3 s into it.
    {
        if (!m.since) m.since = now;
        const uint16_t kit = g_botKit[slot].base;
        const bool wants = kit == 5 || (g_role[slot] == RoleCamper && kit != 0 && kit != 3);
        if (!owns && wants && !m.given && now - m.since > 3000)
        {
            m.given = true;
            MpFixesGiveMines(ctx, base, self);
            owns = (Guest::Read32(base, record + 1056 + 4 * (mineId >> 5)) & (1u << (mineId & 31))) != 0;
            static int told = 0;
            if ((told < 40 && told++ < 40)) { printf("bots: mine: slot %u (%s) given mines: %s\n", slot, BotKits::ClassNames()[kit % 7], owns ? "owns mp_mine now" : "still none"); fflush(stdout); }
        }
    }
    const bool sees = g_tactical[slot].visible && g_seeDist[slot] < 4000.0f;
    auto backToGun = [&](const char* why) {
        if (m.back[0]) BotSwitch(ctx, base, self, m.back);
        static int told = 0;
        if ((told < 60 && told++ < 60)) { printf("bots: mine: slot %u back to %s (%s)\n", slot, m.back[0] ? m.back : "?", why); fflush(stdout); }
        m.stage = 0; m.next = now + 35000 + (slot * 2903u) % 20000u;
    };
    if (m.stage == 0)
    {
        if (!owns || m.laid >= 3 || sees || g_seat[slot].inside || g_seat[slot].mountedSince) return;
        if (!m.next) { m.next = now + 8000 + (slot * 1931u) % 15000u; return; }
        if (now < m.next) return;
        const BotNavigation::Point me{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
        const bool nearFlag = g_war.valid && GetTickCount() - g_war.when < 10000 &&
                              BotNavigation::Distance(me, { g_war.at[0], g_war.at[1], g_war.at[2] }) < 900.0f;
        if (!nearFlag && g_role[slot] != RoleCamper) { m.next = now + 4000; return; }
        const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
        const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
        memset(m.back, 0, sizeof(m.back));
        for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; m.back[c] = ch; }
        if (!BotSwitch(ctx, base, self, "mp_mine")) { m.next = now + 60000; return; }
        static int told = 0;
        if ((told < 60 && told++ < 60)) { printf("bots: mine: slot %u takes out mp_mine (%s, back to %s after)\n", slot, nearFlag ? "near the flag" : "where he holds", m.back); fflush(stdout); }
        m.stage = 1; m.at = now;
        return;
    }
    if (sees) { backToGun("an enemy in sight"); return; }
    orders.forward = orders.right = 0; orders.buttons &= ~SprintButton;
    if (m.stage == 1 && now - m.at >= 900) { m.stage = 2; m.at = now; }
    if (m.stage == 2)
    {
        orders.buttons = (orders.buttons & ~0x80u) | 1u;   // fire: put it down
        orders.wantsFire = true;
        if (now - m.at >= 400) { m.stage = 3; m.at = now; m.laid++; }
        return;
    }
    if (m.stage == 3 && now - m.at >= 1200) backToGun("laid");
}
// 0.5.329: rifle grenades. 328's log: no rifleman or Demolitionist bot
// owned one (owned 0, no rounds): the class special (sub_821EE7C8, which
// gives m1garand_rg / k98_rg) is not given to bots. Now given once a life,
// 3 s in, as the mines are (MpFixesGiveRifleGrenades). Think asks for one
// (g_rgUntil, in server time); here he takes it out by name, as the mines
// and packs are (MpFixesSwitchWeaponFor), Think's launcher rules fire it,
// and he goes back to his gun when the window ends.
void MpFixesGiveRifleGrenades(PPCContext& ctx, uint8_t* base, uint32_t entity);   // mp_fixes.cpp (0.5.329)
static void BotRifleGrenades(PPCContext& ctx, uint8_t* base, uint32_t slot, uint32_t serverTime)
{
    struct Rg { uint32_t entity = 0, since = 0; bool given = false, out = false; char back[32] = {}; };
    static Rg rg[MaxClients];
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    Rg& r = rg[slot];
    if (r.entity != self || !record || int32_t(Guest::Read32(base, record + 304)) <= 0) { const uint32_t e = self; r = {}; r.entity = e; return; }
    if (!g_botLoadoutAssigned[slot] || (g_botKit[slot].base % 7) != 2) return;   // riflemen (and the Demolitionist, built on one)
    const uint32_t now = GetTickCount();
    if (!r.since) r.since = now;
    if (!r.given && now - r.since > 3000)
    {
        r.given = true;
        MpFixesGiveRifleGrenades(ctx, base, self);
        const uint32_t n = BotRifleGrenade(base, record);
        static int told = 0;
        if ((told < 60 && told++ < 60)) { printf("bots: slot %u (%s) given rifle grenades: %s (weapon %u, %d rounds)\n", slot, g_botKit[slot].label[0] ? g_botKit[slot].label : "rifleman", n ? "has them" : "still none", n, n ? BotWeaponRounds(base, record, n) : 0); fflush(stdout); }
    }
    const bool want = serverTime < g_rgUntil[slot] && g_rgWeapon[slot] && !g_seat[slot].inside && !g_seat[slot].mountedSince;
    if (want && !r.out)
    {
        if (BotWeaponRounds(base, record, g_rgWeapon[slot]) <= 0) { g_rgUntil[slot] = 0; return; }
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        auto nameOf = [&](uint32_t n, char* out) {
            memset(out, 0, 32);
            const uint32_t def = (n && n < 128 && list) ? Guest::Read32(base, list + 4 * n) : 0;
            const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
            for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; out[c] = ch; }
        };
        char rgName[32];
        nameOf(g_rgWeapon[slot], rgName);
        nameOf(Guest::Read32(base, record + 164), r.back);
        if (strstr(r.back, "_rg")) r.back[0] = 0;
        if (!rgName[0] || !BotSwitch(ctx, base, self, rgName)) { g_rgUntil[slot] = 0; return; }
        r.out = true;
        static int told = 0;
        if ((told < 80 && told++ < 80)) { printf("bots: slot %u takes out %s (back to %s after)\n", slot, rgName, r.back[0] ? r.back : "?"); fflush(stdout); }
    }
    else if (!want && r.out)
    {
        r.out = false;
        if (r.back[0]) BotSwitch(ctx, base, self, r.back);
    }
}
// 0.5.329: Rusher 2's guns and artillery. His gun: the Bren while he holds
// the objective (an errand there, on watch), the Sten otherwise. His
// artillery: the scout's own, called with his binoculars (mp_binoculars:
// they hold a round while it is ready). Someone in sight 900 to 4500 off and
// no one within 600: binoculars out, up to his eyes (aim), fire to mark the
// spot, back to his gun. Logged "bots: rusher 2".
int MpFixesRusherArtillery(PPCContext& ctx, uint8_t* base, uint32_t entity);   // mp_fixes.cpp (0.5.330)
static void BotRusher2(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders, uint32_t serverTime)
{
    struct R2 { uint32_t entity = 0, since = 0, stage = 0, at = 0, nextCall = 0, nextSwitch = 0, nextReady = 0; int ready = -1; bool told = false; char back[32] = {}; };
    static R2 r2[MaxClients];
    if (!BotIsRusher2(slot)) return;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    R2& r = r2[slot];
    if (r.entity != self || !record || int32_t(Guest::Read32(base, record + 304)) <= 0) { const uint32_t e = self; r = {}; r.entity = e; g_wantGun[slot] = 0; return; }
    const uint32_t now = GetTickCount();
    if (!r.since) { r.since = now; r.nextCall = now + 8000; }
    // 0.5.330: his class's artillery: ready or not (every 3 s; the binoculars given when it is)
    if (now >= r.nextReady && now - r.since > 3000 && r.stage == 0)
    {
        r.nextReady = now + 3000;
        const int was = r.ready;
        r.ready = MpFixesRusherArtillery(ctx, base, self);
        if (r.ready != was) { printf("bots: rusher 2: slot %u artillery %s\n", slot, r.ready > 0 ? "ready" : r.ready == 0 ? "recharging" : "not available"); fflush(stdout); }
    }
    if (g_seat[slot].inside || g_seat[slot].mountedSince) return;
    static uint32_t bino = 0, sten = 0, bren = 0;
    if (!bino) bino = WeaponNumber(base, "mp_binoculars");
    if (!sten) sten = WeaponNumber(base, "sten");
    if (!bren) bren = WeaponNumber(base, "bren");
    auto owns = [&](uint32_t n) { return n && n < 128 && (Guest::Read32(base, record + 1056 + 4 * (n >> 5)) & (1u << (n & 31))) != 0; };
    const uint32_t held = Guest::Read32(base, record + 164);
    if (!r.told && now - r.since > 2000)
    {
        r.told = true;
        printf("bots: rusher 2: slot %u: sten %u %s, bren %u %s, binoculars %u %s (%d rounds), holds %u\n", slot, sten, owns(sten) ? "owned" : "not owned", bren, owns(bren) ? "owned" : "not owned",
               bino, owns(bino) ? "owned" : "not owned", owns(bino) ? BotWeaponRounds(base, record, bino) : 0, held);
        fflush(stdout);
    }
    const bool sees = g_tactical[slot].visible;
    const float dist = g_seeDist[slot];
    const BotExplore& ex = g_explore[slot];
    // the artillery
    if (r.stage == 0 && r.ready > 0 && sees && dist > 900.0f && dist < 4500.0f && now >= r.nextCall && owns(bino) && BotWeaponRounds(base, record, bino) > 0)
    {
        memset(r.back, 0, sizeof(r.back));
        const char* back = owns(sten) ? "sten" : owns(bren) ? "bren" : "";
        strncpy(r.back, back, sizeof(r.back) - 1);
        r.nextCall = now + 30000;
        if (BotSwitch(ctx, base, self, "mp_binoculars"))
        {
            r.stage = 1; r.at = now;
            printf("bots: rusher 2: slot %u calls artillery on someone %.0f away (binoculars out)\n", slot, dist); fflush(stdout);
        }
    }
    if (r.stage > 0)
    {
        auto backToGun = [&](const char* why) {
            if (r.back[0]) BotSwitch(ctx, base, self, r.back);
            printf("bots: rusher 2: slot %u back to %s (%s, binoculars %d rounds)\n", slot, r.back[0] ? r.back : "?", why, BotWeaponRounds(base, record, bino)); fflush(stdout);
            r.stage = 0; r.nextSwitch = now + 3000;
        };
        if (sees && dist < 600.0f) { backToGun("someone close"); return; }
        orders.forward = orders.right = 0;
        orders.buttons &= ~(SprintButton | 0x1u | GrenadeButton);
        orders.wantsFire = false;
        if (r.stage == 1 && now - r.at >= 1300) { r.stage = 2; r.at = now; }          // up
        if (r.stage >= 2) orders.buttons |= 0x8u;                                       // at his eyes
        if (r.stage == 2 && now - r.at >= 900) { r.stage = 3; r.at = now; }
        if (r.stage == 3) { orders.buttons |= 0x1u; orders.wantsFire = true; if (now - r.at >= 300) { r.stage = 4; r.at = now; } }   // mark it
        if (r.stage == 4 && now - r.at >= 900) { r.ready = -1; r.nextReady = now + 2000; backToGun("marked"); }
        return;
    }
    // his gun: the Bren holding the objective, the Sten moving
    (void)serverTime;
    const bool holdingFlag = ex.purpose == BotExplore::Flag && serverTime < ex.holdUntil;
    const uint32_t want = holdingFlag && owns(bren) ? bren : owns(sten) ? sten : 0;
    // 0.5.330: asked for in his commands (329 switched by name every 4 s and
    // the commands put the Bren back each time); not changed mid-fight up close
    if (want != g_wantGun[slot] && now >= r.nextSwitch && !(sees && dist < 350.0f))
    {
        g_wantGun[slot] = want;
        r.nextSwitch = now + 3000;
        static int told = 0;
        if ((told < 60 && told++ < 60)) { printf("bots: rusher 2: slot %u wants the %s (%s)\n", slot, want == bren ? "Bren" : "Sten", holdingFlag ? "holding the objective" : "on the move"); fflush(stdout); }
    }
}
static void BotDispense(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders)
{
    struct Pack { uint32_t entity = 0, stage = 0, at = 0, next = 0; char back[32] = {}; };
    static Pack pack[MaxClients];
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    Pack& p = pack[slot];
    if (p.entity != self) { p = {}; p.entity = self; }
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0 || !g_botLoadoutAssigned[slot]) { p.stage = 0; return; }
    const uint16_t kit = g_botKit[slot].base;
    if (kit != 0 && kit != 3) return;   // support, medic
    const uint32_t now = GetTickCount();
    const bool american = BotSide(base, slot) == 1;
    const char* special = kit == 3 ? "mp_revive" : american ? "mp_ammo" : "mp_ammo_axis";
    // 0.5.456: only an enemy within 1500 stops it (was 5000: on a busy map one
    // was always in sight; 0.5.453's whole test had one ammo and one health
    // pack). Justin: "make sure they use the ... medics, and ammo packs as intended"
    const bool sees = g_tactical[slot].visible && g_seeDist[slot] < 1500.0f;
    if (!p.next) p.next = now + 12000 + slot * 1777u % 15000u;
    auto backToGun = [&](const char* why) {
        if (p.back[0]) BotSwitch(ctx, base, self, p.back);
        printf("bots: pack: slot %u back to %s (%s)\n", slot, p.back[0] ? p.back : "?", why); fflush(stdout);
        p.stage = 0; p.next = now + 30000;   // 0.5.456: 30 s (was 60)
    };
    if (p.stage == 0)
    {
        if (now < p.next || sees || g_seat[slot].inside || g_seat[slot].mountedSince) return;
        const BotNavigation::Point me{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
        int nearMates = 0;
        const uint32_t side = Side(base, self);
        for (uint32_t o = 0; o < ActiveSlots(); o++)
        {
            if (o == slot) continue;
            const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            if (!r || Side(base, e) != side || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
            if (BotNavigation::Distance(me, { ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8) }) < 700.0f) nearMates++;   // 0.5.456: 700 (was 450)
        }
        // 0.5.456: one teammate near is enough (was two), or none when he needs
        // it himself: a medic hurt (70 or less), a support short of rounds
        bool needsIt = false;
        if (kit == 3) needsIt = int32_t(Guest::Read32(base, record + 304)) <= 70;
        else
        {
            const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
            const uint32_t spare = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 1460) : 0;
            needsIt = spare > 0 && spare <= 128 && int32_t(Guest::Read32(base, record + 4 * (80 + spare))) <= 30;
        }
        if (nearMates < 1 && !needsIt) { p.next = now + 3000; return; }
        // The gun to go back to: the one in hand now.
        const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
        const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
        memset(p.back, 0, sizeof(p.back));
        for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; p.back[c] = ch; }
        if (!BotSwitch(ctx, base, self, special)) { p.next = now + 60000; return; }
        printf("bots: pack: slot %u (%s) takes out %s for %d teammates nearMates (back to %s after)\n", slot, kit == 3 ? "medic" : "support", special, nearMates, p.back);
        fflush(stdout);
        p.stage = 1; p.at = now;
        return;
    }
    if (sees) { backToGun("an enemy in sight"); return; }
    orders.forward = orders.right = 0; orders.buttons &= ~SprintButton;
    if (p.stage == 1 && now - p.at >= 900) { p.stage = 2; p.at = now; }
    if (p.stage == 2)
    {
        orders.buttons = (orders.buttons & ~0x80u) | 1u;   // fire: put it down / throw it
        orders.wantsFire = true;
        if (now - p.at >= 400) { p.stage = 3; p.at = now; }
        return;
    }
    if (p.stage == 3 && now - p.at >= 800) backToGun("used");
}

// 0.5.216: medics revive. Justin: medic bots never revive anyone. The
// game's own revive (found in the retail code): the medic's special
// "mp_revive" is weapon class 11 ("revive", def+176). Firing it runs
// sub_82463A48 -> sub_82575910: a 100-unit trace from his eye along his view
// that must hit a teammate's player entity (same team, +520 -> +4) whose
// record is dead (state +1924 == 4); then sub_82579408 sends the revive
// (session message 93, sub_823E86A8) and the script's Plyr_Revive
// (_mp_common.bro, sub_82201D40) stands him up. Missing it sets
// record+2948 (the "nobody to revive" hint).
// So a medic bot with no enemy close looks for the nearest dead teammate
// (human or bot) within 1600 that no other medic has taken, walks to the
// body (the centre of its bounds, entity +272..+288), takes mp_revive out,
// looks down at it, fires, and checks that he stood up (three tries).
// Dead bots wait for a medic who is coming (up to 20 s) instead of
// respawning after 4 s. Log: "bots: revive ...".
static bool BotRevive(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders, uint32_t now)
{
    struct Job { uint32_t entity = 0, target = 0, stage = 0, at = 0, next = 0, claimedAt = 0, tries = 0; bool sent = false; char back[32] = {}; };
    static Job job[MaxClients];
    static uint32_t deadSince[MaxClients] = {};
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return false;
    const uint32_t self = Guest::Read32(base, entities + 4 * (slot + 1));
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    Job& j = job[slot];
    if (j.entity != self) { j = {}; j.entity = self; g_reviveTarget[slot] = 0; }
    // Every player's time of death, as this medic sees it.
    for (uint32_t o = 0; o < ActiveSlots(); o++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        if (!r || int32_t(Guest::Read32(base, r + 304)) > 0) deadSince[o] = 0;
        else if (!deadSince[o])
        {
            deadSince[o] = now;
            // 0.5.224: where he fell, kept: a dead player's entity can move off
            // with his spectating view (0.5.223: the medic reached Justin's
            // body twice and lost it each time).
            g_deadAt[o][0] = ReadFloat(base, r); g_deadAt[o][1] = ReadFloat(base, r + 4); g_deadAt[o][2] = ReadFloat(base, r + 8);
        }
    }
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0 || !g_botLoadoutAssigned[slot] || g_botKit[slot].base != 3)
    { j.stage = 0; j.target = 0; g_reviveTarget[slot] = 0; return false; }
    if (g_seat[slot].inside || g_seat[slot].mountedSince) return false;
    // 0.5.219: 0.5.217's match: 8 of 11 revives given up for "an enemy in sight" (900);
    // now only an enemy within 500, and never once he is at the body.
    // 0.5.225: 0.5.224: 6 of 7 trips still given up for an enemy in sight
    // (deaths happen in fights): now only one within 300.
    const bool sees = g_tactical[slot].visible && g_seeDist[slot] < 300.0f;
    const BotNavigation::Point feet{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    auto finish = [&](const char* why, uint32_t cooldown) {
        if (j.stage >= 2 && j.back[0]) BotSwitch(ctx, base, self, j.back);
        printf("bots: revive: slot %u stops (%s)%s%s\n", slot, why, j.stage >= 2 ? ", back to " : "", j.stage >= 2 ? j.back : ""); fflush(stdout);
        j.stage = 0; j.target = 0; j.tries = 0; g_reviveTarget[slot] = 0; j.next = now + cooldown;
    };
    auto bodyAt = [&](uint32_t e, BotNavigation::Point& at) {
        const uint32_t who = BotSlot(base, e);
        if (who < MaxClients && deadSince[who] && std::isfinite(g_deadAt[who][0])) { at = { g_deadAt[who][0], g_deadAt[who][1], g_deadAt[who][2] + 10.0f }; return true; }
        const float x = (ReadFloat(base, e + 272) + ReadFloat(base, e + 288)) * 0.5f;
        const float y = (ReadFloat(base, e + 276) + ReadFloat(base, e + 292)) * 0.5f;
        const float z = (ReadFloat(base, e + 280) + ReadFloat(base, e + 296)) * 0.5f;
        const uint32_t r = Guest::Read32(base, e + 512);
        const BotNavigation::Point origin{ ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8) };
        if (std::isfinite(x) && std::isfinite(y) && std::isfinite(z) && BotNavigation::Distance(origin, { x, y, z }) < 120.0f) at = { x, y, z };
        else at = { origin.x, origin.y, origin.z + 10.0f };
        return std::isfinite(at.x) && std::isfinite(at.y) && std::isfinite(at.z);
    };
    auto deadMate = [&](uint32_t o, uint32_t side) {
        const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        return r && Side(base, e) == side && int32_t(Guest::Read32(base, r + 304)) <= 0 && Guest::Read32(base, r + 1924) == 4 ? e : 0u;
    };
    const uint32_t side = Side(base, self);
    // 0.5.226: Justin: no medic has ever come to him. Once each time he is
    // down, every living medic on his side says what he is doing about it.
    {
        static uint32_t told[MaxClients] = {};
        static int lines = 0;
        const uint32_t he = Guest::Read32(base, entities + 4);
        const uint32_t hisRecord = he ? Guest::Read32(base, he + 512) : 0;
        if (slot != 0 && deadSince[0] && told[slot] != deadSince[0] && he && Side(base, he) == side && lines < 80 && !deadMate(0, side))
        {
            told[slot] = deadSince[0]; lines++;
            printf("bots: revive: Justin (slot 0) is down but not revivable to medic slot %u: health %d, state %u (4 is revivable)\n", slot,
                   hisRecord ? int32_t(Guest::Read32(base, hisRecord + 304)) : 0, hisRecord ? Guest::Read32(base, hisRecord + 1924) : 0); fflush(stdout);
        }
        if (slot != 0 && deadSince[0] && told[slot] != deadSince[0] && deadMate(0, side) && lines < 80)
        {
            told[slot] = deadSince[0]; lines++;
            BotNavigation::Point at{};
            const bool where = he && bodyAt(he, at);
            bool taken = false;
            for (uint32_t s = 0; s < ActiveSlots(); s++) if (s != slot && g_reviveTarget[s] == 1) taken = true;
            printf("bots: revive: Justin (slot 0) is down; medic slot %u: %.0f away%s, job stage %u on slot %d, enemy within 500 %d, next try in %d ms%s\n",
                   slot, where ? BotNavigation::Distance(feet, at) : -1.0f, where ? "" : " (no body position)", j.stage, int(j.target) - 1, sees ? 1 : 0,
                   int(j.next) - int(now), taken ? ", another medic has him" : ""); fflush(stdout);
        }
    }
    if (j.stage == 0)
    {
        if (now < j.next || sees) return false;
        uint32_t best = 0; float bestDistance = 1000.0f;   // 0.5.224: 1000 (was 1600: most far trips ended "he is up again")
        for (uint32_t o = 0; o < ActiveSlots(); o++)
        {
            if (o == slot || !deadSince[o] || now - deadSince[o] > 25000) continue;
            const uint32_t e = deadMate(o, side);
            if (!e) continue;
            bool taken = false;
            for (uint32_t s = 0; s < ActiveSlots(); s++) if (s != slot && g_reviveTarget[s] == o + 1) taken = true;
            if (taken) continue;
            BotNavigation::Point at;
            if (!bodyAt(e, at)) continue;
            const float d = BotNavigation::Distance(feet, at);
            if (d < bestDistance) { best = o + 1; bestDistance = d; }
        }
        if (!best) { j.next = now + 1000; return false; }
        j.target = best; j.stage = 1; j.claimedAt = now; j.tries = 0; g_reviveTarget[slot] = best;
        printf("bots: revive: slot %u (medic) goes to revive slot %u, %.0f away\n", slot, best - 1, bestDistance); fflush(stdout);
    }
    const uint32_t o = j.target - 1;
    const uint32_t te = Guest::Read32(base, entities + 4 * (o + 1));
    const uint32_t tr = te ? Guest::Read32(base, te + 512) : 0;
    if (!tr) { finish("his body is gone", 2000); return false; }
    if (int32_t(Guest::Read32(base, tr + 304)) > 0)
    {
        static int toldUp = 0;
        if (j.stage >= 3 && toldUp++ < 100) { printf("bots: revive: slot %u REVIVED slot %u (health %d)\n", slot, o, int32_t(Guest::Read32(base, tr + 304))); fflush(stdout); }
        finish(j.stage >= 3 ? "revived" : "he is up again", 4000);
        return false;
    }
    if (!deadMate(o, side)) { finish("not a revivable teammate any more", 2000); return false; }
    if (sees && j.stage < 2) { finish("an enemy in sight", 4000); return false; }
    if (now - j.claimedAt > 22000) { finish("took too long", 15000); return false; }
    BotNavigation::Point body;
    if (!bodyAt(te, body)) { finish("no body position", 5000); return false; }
    const float d = hypotf(body.x - feet.x, body.y - feet.y);
    const float viewYaw = ReadFloat(base, record + 208);
    const float bearing = atan2f(body.y - feet.y, body.x - feet.x) * 57.29578f;
    if (j.stage == 1)
    {
        if (d > 38.0f)
        {
            const float goal[3] = { body.x, body.y, feet.z };
            orders.forward = 127; orders.right = 0;
            orders.buttons &= ~1u; orders.wantsFire = false;
            if (d > 400.0f) orders.buttons |= SprintButton; else orders.buttons &= ~SprintButton;
            g_yaw[slot] += std::clamp(AngleDifference(bearing, viewYaw), -6.0f, 6.0f);
            TacticalMove(ctx, base, self, 0, slot, now, int32_t(Guest::Read32(base, record + 304)), viewYaw, goal, false, orders);
            return true;
        }
        const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
        const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
        memset(j.back, 0, sizeof(j.back));
        for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; j.back[c] = ch; }
        if (_stricmp(j.back, "mp_revive") == 0) j.back[0] = 0;
        if (!BotSwitch(ctx, base, self, "mp_revive")) { finish("could not take out mp_revive", 60000); return false; }
        printf("bots: revive: slot %u at slot %u's body (%.0f away), takes out mp_revive (back to %s after)\n", slot, o, d, j.back[0] ? j.back : "?"); fflush(stdout);
        j.stage = 2; j.at = now;
    }
    // At the body: still, looking down at it.
    orders.forward = orders.right = 0; orders.buttons = 0; orders.wantsFire = false;
    {
        const float eye = BotAim::EyeHeight(ReadFloat(base, record + 220));
        const float dx = body.x - feet.x, dy = body.y - feet.y, dz = body.z - (feet.z + eye);
        const float want = BotAim::Pitch(dx, dy, dz);
        g_wantYaw[slot] = bearing; g_wantPitch[slot] = want;
        g_yaw[slot] += std::clamp(AngleDifference(bearing, viewYaw), -8.0f, 8.0f);
        g_pitch[slot] += std::clamp(want * PitchSign() - g_pitch[slot], -6.0f, 6.0f);
        g_pitch[slot] = fmaxf(-70.0f, fminf(70.0f, g_pitch[slot]));
    }
    if (d > 120.0f) { j.stage = 1; return true; }   // pushed off: walk back (keeps mp_revive out)
    // 0.5.220: 0.5.219's one medic at a body never fired (his view never came
    // within 12 degrees: the view lags what is sent). Fires after 2 s anyway.
    if (j.stage == 2 && now - j.at >= 900 && (fabsf(AngleDifference(bearing, viewYaw)) < 20.0f || now - j.at >= 2000)) { j.stage = 3; j.at = now; }
    if (j.stage == 3 && !j.sent)
    {
        j.sent = true;
        // 0.5.225: no medic bot's revive has ever reached the game's own check
        // (its trace from his view). What the trace leads to is called here
        // directly as he uses the kit: sub_823E86A8 (the session's revive
        // message: session = *(*(0x82BCCAC0)), the dead player's entity, the
        // medic's), as sub_82579408 does after a hit. COD3_REVIVE_DIRECT=0
        // leaves it to the kit alone.
        // 0.5.226: 0.5.225's six messages never stood anyone up (the host
        // sends message 93 to the host: it never comes back). What its
        // handler runs is the script's Plyr_Revive (_mp_common.bro,
        // sub_82201D40: r3 the address of the dead player's script handle,
        // r4 the medic's): full health, state 3, his kit again, the medic's
        // revive counted. So that is called here, on the host, as the
        // script would. COD3_REVIVE_DIRECT=message: 0.5.225's message;
        // =0: the kit's own trace alone.
        static const int direct = [] { const char* t = getenv("COD3_REVIVE_DIRECT"); return t && t[0] == '0' ? 0 : (t && t[0] == 'm') ? 2 : 1; }();
        auto handleOf = [&](uint32_t s, uint32_t e) -> uint32_t {
            const uint32_t index = s + 1, entry = 0x82BD1FC0 + index * 8;
            return Guest::Read32(base, entry) == e ? ((Guest::Read32(base, entry + 4) << 14) | index) : 0u;
        };
        const uint32_t deadHandle = te ? handleOf(o, te) : 0, medicHandle = handleOf(slot, self);
        if (direct == 1 && deadHandle && medicHandle)
        {
            static uint32_t scratch = 0;
            if (!scratch) scratch = Guest::AllocatePhysical(0x10);
            Guest::Write32(base, scratch, deadHandle);
            Guest::Write32(base, scratch + 4, medicHandle);
            {
                BotClientScope clientScope(base, o, "revive");
                Call(ctx, base, 0x82201D40, scratch, scratch + 4);
            }
            static int told = 0;
            if ((told < 60 && told++ < 60)) { printf("bots: revive: slot %u stands slot %u up (Plyr_Revive, handles %08X %08X): health now %d, state %u\n", slot, o, deadHandle, medicHandle,
                                      int32_t(Guest::Read32(base, tr + 304)), Guest::Read32(base, tr + 1924)); fflush(stdout); }
            // 0.5.544: Plyr_Revive gives health and state 3 but leaves a bot's
            // body in its death pose (user: "a bot in the dead animation running
            // around"; 543: slot 13 revived at 29249). A player's client stands
            // him up; a bot has none. So he is spawned again the game's way (a
            // fresh body) and put back where he lay, with the revive's health.
            const int32_t revivedHealth = int32_t(Guest::Read32(base, tr + 304));
            if (revivedHealth > 0 && Guest::Read32(base, tr + 1924) == 3 && o < MaxClients)
            {
                const float lay[3] = { ReadFloat(base, tr), ReadFloat(base, tr + 4), ReadFloat(base, tr + 8) };
                g_revivedAt[o] = GetTickCount();
                Respawn(ctx, base, te, BotSide(base, o) == 1);
                const uint32_t nr = Guest::Read32(base, te + 512);
                if (nr && int32_t(Guest::Read32(base, nr + 304)) > 0)
                {
                    static uint32_t layAt = 0;
                    if (!layAt) layAt = Guest::AllocatePhysical(0x20);
                    auto putF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
                    putF(layAt, lay[0]); putF(layAt + 4, lay[1]); putF(layAt + 8, lay[2] + 1.0f);
                    PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
                    place.r3.u64 = te; place.r4.u64 = layAt; place.lr = 0;
                    Guest::Lookup(0x8255E118)(place, base);
                    for (int k = 0; k < 3; k++) putF(nr + 16 + 4 * k, 0.0f);
                    Guest::Write32(base, nr + 304, uint32_t(revivedHealth));
                    printf("bots: revive: slot %u respawned in place (a fresh body, not the death pose) at %.0f %.0f, health %d\n", o, lay[0], lay[1], revivedHealth);
                    fflush(stdout);
                }
            }
        }
        else if (direct == 2 && te)
        {
            const uint32_t holder = Guest::Read32(base, ModeObject);
            const uint32_t session = holder ? Guest::Read32(base, holder) : 0;
            if (session) Call(ctx, base, 0x823E86A8, session, te, self);
            static int told = 0;
            if ((told < 60 && told++ < 60)) { printf("bots: revive: slot %u sends the revive for slot %u as a message (session %08X)\n", slot, o, session); fflush(stdout); }
        }
        else if (direct == 1)
        {
            static int told = 0;
            if ((told < 30 && told++ < 30)) { printf("bots: revive: slot %u has no script handle for slot %u (%08X) or himself (%08X)\n", slot, o, deadHandle, medicHandle); fflush(stdout); }
        }
    }
    if (j.stage == 3)
    {
        // 0.5.222: 0.5.221's three tries never reached the game's revive check
        // (record+2948, which a miss sets, stayed -1): what he holds and the
        // weapon state while he fires, for finding out why.
        {
            static int told = 0;
            const uint32_t held = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t def = (held && held < 128 && list) ? Guest::Read32(base, list + 4 * held) : 0;
            const uint32_t text = (def >= 0x40000000u && def < 0xA0000000u) ? Guest::Read32(base, def + 8) : 0;
            char name[32] = {};
            for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; name[c] = ch; }
            if ((told < 120 && told++ < 120)) { printf("bots: revive trace slot %u: holds %u %s (type %u class %u) state %u, hint %u, aim pitch %.1f yaw %.1f\n", slot, held, name,
                def ? Guest::Read32(base, def + 172) : 0, def ? Guest::Read32(base, def + 176) : 0, Guest::Read32(base, record + 168),
                Guest::Read32(base, record + 2948), g_wantPitch[slot], g_wantYaw[slot]); fflush(stdout); }
        }
        orders.buttons = 1u; orders.wantsFire = true;   // fire: the revive
        if (now - j.at >= 450) { j.stage = 4; j.at = now; }
    }
    if (j.stage == 4 && now - j.at >= 2000)
    {
        static int toldMiss = 0;
        if ((toldMiss < 60 && toldMiss++ < 60)) { printf("bots: revive: slot %u's revive on slot %u did not take (try %u, %.0f away, view pitch %.1f, nobody-hint %u)\n", slot, o, j.tries + 1, d,
                                      ReadFloat(base, record + 204), Guest::Read32(base, record + 2948)); fflush(stdout); }
        if (++j.tries >= 3) { finish("three tries without a revive", 20000); return false; }
        j.stage = 2; j.at = now - 600; j.sent = false;
    }
    return true;
}

// 0.5.219: feelers. Justin: bots still walk into walls; they should scan
// as they walk. Every 150 ms, whatever moves him this frame (the planner,
// a roam, a fight's strafe, a teammate's push), a body-sized sweep 44 units
// ahead along the way he is actually going, at step height (18 up, so
// stairs and slopes pass). A wall there (hit, surface steeper than 53
// degrees): the move is turned along the wall (its push into the wall
// taken out), toward whichever side a second and third feeler (40 degrees
// left and right) find more open; head-on with both sides shut, he backs
// off a moment and turns. Learning: three bumps within 80 units in 12 s
// and that spot is remembered as a failed way (g_failedRoutes, as a stuck
// step is), so the planner goes round it from then on. Log "bots: feelers".
static bool PoseHolds(uint32_t slot);
static void BotFeelers(PPCContext& ctx, uint8_t* base, uint32_t slot, BotOrders& orders, uint32_t now)
{
    using namespace BotNavigation;
    struct Feel { uint32_t at = 0, until = 0, bumps = 0, firstBump = 0, backUntil = 0, scanNext = 0, escapeUntil = 0; float slideYaw = 0; bool sliding = false; Point bumpAt{}; };
    static Feel feel[MaxClients];
    static uint32_t total = 0, learnt = 0, lastTold = 0;
    if (orders.forward == 0 && orders.right == 0) return;
    if (orders.up || g_tactical[slot].climbing || g_seat[slot].inside || g_seat[slot].mountedSince || PoseHolds(slot)) return;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return;
    Feel& f = feel[slot];
    const float viewYaw = ReadFloat(base, record + 208) * 0.01745329252f;
    const float moveYaw = viewYaw + std::atan2(-float(orders.right), float(orders.forward));
    const float magnitude = std::fmin(127.0f, std::hypot(float(orders.forward), float(orders.right)));
    auto steer = [&](float yaw, float scale) {
        const float rel = yaw - viewYaw;
        orders.forward = int8_t(std::clamp(std::cos(rel) * magnitude * scale, -127.0f, 127.0f));
        orders.right = int8_t(std::clamp(-std::sin(rel) * magnitude * scale, -127.0f, 127.0f));
    };
    if (now < f.backUntil) { steer(f.slideYaw, 1.0f); orders.buttons &= ~SprintButton; return; }
    if (now < f.escapeUntil) { steer(f.slideYaw, 1.0f); return; }   // 0.5.467: the way the look round chose, held
    if (now - f.at < 150) { if (f.sliding && now < f.until) steer(f.slideYaw, 1.0f); return; }
    f.at = now; f.sliding = false;
    const Point feet{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    if (!Finite(feet)) return;
    auto probe = [&](float yaw, float length) {
        const Point from{ feet.x, feet.y, feet.z + 18.0f };
        const Point to{ feet.x + std::cos(yaw) * length, feet.y + std::sin(yaw) * length, feet.z + 18.0f };
        return BotTrace(ctx, base, self, self, from, to, true);
    };
    const Hit ahead = probe(moveYaw, 44.0f);
    if (!ahead.valid || ahead.fraction >= 0.95f || ahead.normal.z > 0.6f) return;   // open, or only a slope/step
    // 0.5.469: stairs (Justin: bots should "be able to walk upstairs"): a stair's
    // risers pass the 18-high feeler for a step or two, then meet it, and the
    // stairs were taken for a wall and slid along. Clear at 44 high and a floor
    // ahead 2 to 40 above his feet, flat enough to stand on: a step, walked up.
    {
        const Point fromHigh{ feet.x, feet.y, feet.z + 44.0f };
        const Point toHigh{ feet.x + std::cos(moveYaw) * 44.0f, feet.y + std::sin(moveYaw) * 44.0f, feet.z + 44.0f };
        const Hit high = BotTrace(ctx, base, self, self, fromHigh, toHigh, true);
        if (!high.valid || high.fraction >= 0.95f)
        {
            const Hit floor = BotTrace(ctx, base, self, self, toHigh, { toHigh.x, toHigh.y, feet.z - 8.0f }, true);
            const float rise = floor.end.z - feet.z;
            if (floor.valid && floor.fraction < 1.0f && floor.normal.z > 0.7f && rise > 2.0f && rise < 40.0f)
            {
                static int told = 0;
                if (told++ < 40) { printf("bots: feelers: slot %u takes the stairs at %.0f %.0f (a step %.0f up)\n", slot, feet.x, feet.y, rise); fflush(stdout); }
                return;
            }
        }
    }
    total++;
    // 0.5.467: a look round (Justin: "they need to learn to turn and look around
    // and walk through doors and into other rooms and be able to walk upstairs
    // and keep actively searching for new routes instead of soley walking into
    // a wall over and over"). Meeting a wall again where he met one (80, 12 s),
    // he sweeps 24 ways (every 15 degrees) 240 out at knee and chest height:
    // a doorway or a stair's opening is clear at both. The clearest way (70 at
    // least), the nearer to where he was going the better, is held 1.5 s.
    if (now >= f.scanNext)   // 0.5.469: at the first wall (was the second at one spot)
    {
        auto probeAt = [&](float yaw, float height) {
            const Point from{ feet.x, feet.y, feet.z + height };
            const Point to{ feet.x + std::cos(yaw) * 240.0f, feet.y + std::sin(yaw) * 240.0f, feet.z + height };
            return BotTrace(ctx, base, self, self, from, to, true);
        };
        float bestYaw = 0.0f, bestScore = -1e9f, bestClear = 0.0f;
        for (int k = 0; k < 24; k++)
        {
            const float yaw = moveYaw + float(k) * 0.2617994f;
            const Hit knee = probeAt(yaw, 18.0f), chest = probeAt(yaw, 48.0f);
            const float clear = std::fmin(knee.valid ? knee.fraction : 1.0f, chest.valid ? chest.fraction : 1.0f) * 240.0f;
            if (clear < 55.0f) continue;   // 0.5.469: 55 (was 70): narrower doorways
            const float score = clear + 120.0f * std::cos(float(k) * 0.2617994f);
            if (score > bestScore) { bestScore = score; bestYaw = yaw; bestClear = clear; }
        }
        f.scanNext = now + 2500;
        if (bestScore > -1e8f)
        {
            f.slideYaw = bestYaw; f.sliding = true; f.until = now + 1500; f.escapeUntil = now + 1500;
            steer(f.slideYaw, 1.0f);
            static int told = 0;
            if (told++ < 80) { printf("bots: feelers: slot %u looks round at %.0f %.0f: takes the way %.0f degrees off (%.0f clear)\n", slot, feet.x, feet.y, AngleDifference(bestYaw * 57.29578f, moveYaw * 57.29578f), bestClear); fflush(stdout); }
            if (f.bumps) f.bumps++;
            return;
        }
    }
    // Along the wall: the move with its part into the wall taken out.
    const float nx = ahead.normal.x, ny = ahead.normal.y, nl = std::hypot(nx, ny);
    float mx = std::cos(moveYaw), my = std::sin(moveYaw);
    if (nl > 0.1f) { const float d = (mx * nx + my * ny) / (nl * nl); mx -= d * nx; my -= d * ny; }
    const Hit left = probe(moveYaw + 0.7f, 40.0f), right = probe(moveYaw - 0.7f, 40.0f);
    const float openLeft = left.valid ? left.fraction : 0.0f, openRight = right.valid ? right.fraction : 0.0f;
    if (std::hypot(mx, my) < 0.35f)   // head-on
    {
        if (openLeft < 0.5f && openRight < 0.5f) { f.slideYaw = moveYaw + 3.14159265f; f.backUntil = now + 400; }   // boxed in: back off
        else f.slideYaw = moveYaw + (openLeft >= openRight ? 1.2f : -1.2f);
    }
    else
    {
        f.slideYaw = std::atan2(my, mx);
        // Of the two ways along the wall, the more open one.
        const float alongLeft = AngleDifference(f.slideYaw * 57.29578f, moveYaw * 57.29578f) > 0 ? openLeft : openRight;
        const float alongOther = alongLeft == openLeft ? openRight : openLeft;
        if (alongOther > alongLeft + 0.3f) f.slideYaw += 3.14159265f;
    }
    f.sliding = true; f.until = now + 450;
    steer(f.slideYaw, 1.0f);
    // Learn the spot.
    if (f.bumps && Distance(feet, f.bumpAt) < 80.0f && now - f.firstBump < 12000) f.bumps++;
    else { f.bumps = 1; f.firstBump = now; f.bumpAt = feet; }
    if (f.bumps == 3)
    {
        const Point blocked{ feet.x + std::cos(moveYaw) * 64.0f, feet.y + std::sin(moveYaw) * 64.0f, feet.z };
        const unsigned strikes = g_failedRoutes[slot].Remember(feet, blocked, now);
        g_tactical[slot].haveStep = false; g_tactical[slot].planned = 0;
        learnt++;
        printf("bots: feelers: slot %u keeps meeting a wall at %.0f %.0f %.0f: remembered (strike %u), rerouting\n", slot, feet.x, feet.y, feet.z, strikes); fflush(stdout);
    }
    if (now - lastTold > 30000) { lastTold = now; printf("bots: feelers: %u walls felt and slid along, %u spots learnt so far\n", total, learnt); fflush(stdout); }
}

// 0.5.197: each bot's kit once a life (the weapons he has rounds for), to
// find what the Support's ammo packs and the Medic's health packs are and
// how they are used (the next step: bots that hand them out).
static void BotKitProbe(uint8_t* base, uint32_t slot)
{
    static uint32_t told[MaxClients] = {}, lastEntity[MaxClients] = {};
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t self = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
    const uint32_t record = self ? Guest::Read32(base, self + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) { lastEntity[slot] = 0; return; }
    if (lastEntity[slot] == self || told[slot] >= 6) return;
    lastEntity[slot] = self; told[slot]++;
    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
    if (!list) return;
    printf("bots: kit slot %u (class %u/%u, in hand %u):", slot, Guest::Read16(base, record + 1920), Guest::Read16(base, record + 1922), Guest::Read32(base, record + 164));
    for (uint32_t w = 1; w < 128; w++)
    {
        const uint32_t def = Guest::Read32(base, list + 4 * w);
        if (def < 0x40000000u || def >= 0xA0000000u) continue;
        const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
        if (clip > 128 || spare > 128) continue;
        const uint32_t inClip = Guest::Read32(base, record + 4 * (172 + clip)), inSpare = Guest::Read32(base, record + 4 * (80 + spare));
        if (!inClip && !inSpare) continue;
        char name[32] = {};
        const uint32_t text = Guest::Read32(base, def + 8);
        for (int c = 0; text && c < 31; c++) { const char ch = char(Guest::Read8(base, text + c)); if (!ch) break; name[c] = ch; }
        printf(" %u:%s(%u/%u)", w, name, inClip, inSpare);
    }
    printf("\n"); fflush(stdout);
}

// Each bot's command for this frame, into its ring. For trying, from
// debug.txt: bot_sticks' left stick walks (lx right, ly forward), its right
// stick's x turns, bot_buttons are the buttons.
// 0.5.206: the pose test, for seeing how a gun sits in a bot's hands
// (third-person models; mp_fixes.cpp's world fit). "bot_pose <guns>
// [distance] [facing]": the bots are each given a gun from the list (in
// turn, comma separated) and spawned again with it, then kept still in a
// row in front of the host, looking toward him turned by facing degrees
// (90: his right side, the gun's side, toward the host), without firing.
// "bot_pose off" lets them go. "bot_pose_test [distance]": two bots, one
// with each campaign gun and one beside him with the gun it copies, from
// the right side, the front and the left, a screenshot of each (beside the
// executable, screenshots\), every step logged ("pose test:"); "quit" at
// the end with COD3_POSE_QUIT=1 (that quit froze the game in testing:
// port/tests/RUN-GUN-POSE-TEST.ps1 closes it itself once "pose test: done"
// is in the log instead).
int32_t MpFixesCampaignWeaponNumber(const char* name);   // mp_fixes.cpp
namespace
{
    std::mutex g_poseLock;
    std::atomic<bool> g_poseOn{ false };
    std::vector<std::string> g_poseGuns;
    float g_poseDistance = 100.0f, g_poseFacing = 90.0f;
    std::atomic<int> g_poseAsked{ 0 };   // a new set of guns: spawn the bots again with them
    int g_poseDone = 0;
    struct PoseStep { std::string guns; float facing; bool shot; };
    std::vector<PoseStep> g_poseTest;
    size_t g_poseStep = 0;
    uint32_t g_poseStepAt = 0;
    bool g_poseTesting = false;
    // the weapon each posed bot is made to hold (his kit's main weapon, by
    // name in the weapon list): owned, its clip and spare filled, and asked
    // for in his commands. Without it, after a spawn the bots' weapon choice
    // took the pistol for the campaign guns (0.5.206's first pose test:
    // "slots 39 and 11", holds 11).
    uint32_t g_poseWeapon[MaxClients] = {};
    uint32_t WeaponNumber(uint8_t* base, const char* name)
    {
        if (const int32_t campaign = MpFixesCampaignWeaponNumber(name); campaign > 0) return uint32_t(campaign);
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        for (uint32_t n = 1; list && n < 128; n++)
        {
            const uint32_t def = Guest::Read32(base, list + 4 * n);
            if (def < 0x40000000u || def >= 0xA0000000u) continue;
            const uint32_t text = Guest::Read32(base, def + 8);
            if (text >= 0x40000000u && text < 0xA0000000u && _stricmp(reinterpret_cast<const char*>(base + text), name) == 0) return n;
        }
        return 0;
    }
    std::vector<std::string> SplitGuns(const std::string& text)
    {
        std::vector<std::string> guns;
        std::string one;
        for (char c : text + ",")
        {
            if (c == ',' || c == ' ') { if (!one.empty()) guns.push_back(one); one.clear(); }
            else one += char(tolower(static_cast<unsigned char>(c)));
        }
        return guns;
    }
}
void BotsPose(const std::string& args)
{
    std::lock_guard<std::mutex> guard(g_poseLock);
    char guns[256] = {};
    float distance = 100.0f, facing = 90.0f;
    if (sscanf(args.c_str(), " %255s %f %f", guns, &distance, &facing) < 1 || _stricmp(guns, "off") == 0)
    {
        g_poseOn.store(false); g_poseTesting = false;
        memset(g_poseWeapon, 0, sizeof(g_poseWeapon));
        printf("bots: pose off\n"); fflush(stdout);
        return;
    }
    g_poseGuns = SplitGuns(guns);
    for (const std::string& g : g_poseGuns)
        if (!BotKits::KnownPrimary(g)) { printf("bots: pose: \"%s\" is not a main weapon a kit can name\n", g.c_str()); fflush(stdout); }
    g_poseDistance = std::clamp(distance, 40.0f, 600.0f);
    g_poseFacing = facing;
    g_poseOn.store(true);
    g_poseAsked++;
    printf("bots: pose %s at %.0f, facing %.0f\n", guns, g_poseDistance, g_poseFacing); fflush(stdout);
}
void BotsPoseTest(const std::string& args)
{
    float distance = 100.0f;
    sscanf(args.c_str(), " %f", &distance);
    std::lock_guard<std::mutex> guard(g_poseLock);
    // each campaign gun beside the gun it copies (mp_fixes.cpp's Guns[])
    static const char* const pairs[][2] = {
        { "mg42", "mg34" }, { "bren", "bar" }, { "sten", "thompson" }, { "fg42", "bar" }, { "fg42_scoped", "bar" },
        { "gewehr43", "m1garand" }, { "lee_enfield", "kar98" }, { "lee_enfield_sniper", "kar98_sniper" } };
    g_poseTest.clear();
    for (const auto& pair : pairs)
    {
        const std::string guns = std::string(pair[0]) + "," + pair[1];
        for (float facing : { 90.0f, 0.0f, -90.0f }) g_poseTest.push_back({ guns, facing, true });
    }
    g_poseDistance = std::clamp(distance, 40.0f, 600.0f);
    g_poseStep = 0; g_poseStepAt = 0; g_poseTesting = true;
    BotsAsk(2);
    printf("pose test: %zu steps (8 guns beside their copies, right side, front, left side), at %.0f\n", g_poseTest.size(), g_poseDistance);
    fflush(stdout);
}
// His kit given again now (the class kit, sub_82206028, with his kit's main
// weapon: BotsPrimaryName), as ReKitBots does.
static void PoseKit(PPCContext& ctx, uint8_t* base, uint32_t slot, uint32_t entity)
{
    static uint32_t scratch = 0;
    const uint32_t index = slot + 1, entry = 0x82BD1FC0 + index * 8;
    if (Guest::Read32(base, entry) != entity) return;
    const uint32_t handle = (Guest::Read32(base, entry + 4) << 14) | index;
    if (!scratch) scratch = Guest::AllocatePhysical(0x10);
    Guest::Write32(base, scratch, handle);
    BotClientScope clientScope(base, slot, "pose kit");
    Call(ctx, base, 0x82206028, scratch, 0);
}
// Once a frame before the commands: the test's steps, new guns, and each
// posed bot kept in his place. index: his place in the row.
static void PoseFrame(PPCContext& ctx, uint8_t* base)
{
    if (!g_poseOn.load() && !g_poseTesting) return;
    std::lock_guard<std::mutex> guard(g_poseLock);
    const uint32_t now = GetTickCount();
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t hostRecord = host ? Guest::Read32(base, host + 512) : 0;
    if (!hostRecord) return;
    std::vector<uint32_t> slots;
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        if (entity && Guest::Read32(base, entity + 512)) slots.push_back(slot);
    }
    // the test: each step its guns (spawned again when they change), 3 s to
    // settle, a screenshot, 1 s more
    if (g_poseTesting)
    {
        if (slots.size() < 2) { static uint32_t told = 0; if (now - told > 3000) { told = now; printf("pose test: waiting for two bots (%zu)\n", slots.size()); fflush(stdout); } return; }
        if (g_poseStep >= g_poseTest.size())
        {
            g_poseTesting = false;
            printf("pose test: done\n"); fflush(stdout);
            if (getenv("COD3_POSE_QUIT")) Kernel::QueueConsoleCommand("quit");
            return;
        }
        PoseStep& step = g_poseTest[g_poseStep];
        if (g_poseStepAt == 0)
        {
            const bool newGuns = !g_poseOn.load() || SplitGuns(step.guns) != g_poseGuns;
            g_poseGuns = SplitGuns(step.guns);
            g_poseFacing = step.facing;
            g_poseOn.store(true);
            if (newGuns) g_poseAsked++;
            g_poseStepAt = now + (newGuns ? 4000 : 2000);
            printf("pose test: step %zu of %zu: %s, facing %.0f\n", g_poseStep + 1, g_poseTest.size(), step.guns.c_str(), step.facing); fflush(stdout);
        }
        else if (step.shot && int32_t(now - g_poseStepAt) >= 0)
        {
            step.shot = false;
            Overlay::RequestScreenshot();
            std::string held;
            for (uint32_t slot : slots)
            {
                const uint32_t record = Guest::Read32(base, Guest::Read32(base, entities + 4 * (slot + 1)) + 512);
                const uint32_t weapon = Guest::Read32(base, record + 164), list = Guest::Read32(base, 0x82BCBF9C);
                const uint32_t def = list && weapon < 128 ? Guest::Read32(base, list + 4 * weapon) : 0;
                const uint32_t name = def ? Guest::Read32(base, def + 8) : 0;
                char one[96];
                snprintf(one, sizeof(one), " bot %u holds %u %s (kit %s) at %.0f %.0f %.0f;", slot, weapon,
                         name > 0x40000000u && name < 0xA0000000u ? reinterpret_cast<const char*>(base + name) : "?", g_botKit[slot].primary,
                         ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8));
                held += one;
            }
            printf("pose test: screenshot of step %zu (%s, facing %.0f):%s\n", g_poseStep + 1, step.guns.c_str(), step.facing, held.c_str()); fflush(stdout);
            g_poseStepAt = now + 1000;
        }
        else if (!step.shot && int32_t(now - g_poseStepAt) >= 0)
        {
            g_poseStep++; g_poseStepAt = 0;
        }
    }
    if (!g_poseOn.load() || g_poseGuns.empty()) return;
    // new guns: each bot's kit's main weapon, then spawned again with it
    if (g_poseDone != g_poseAsked.load())
    {
        g_poseDone = g_poseAsked.load();
        for (size_t k = 0; k < slots.size(); k++)
        {
            const uint32_t slot = slots[k];
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            memset(g_botKit[slot].primary, 0, sizeof(g_botKit[slot].primary));
            strncpy(g_botKit[slot].primary, g_poseGuns[k % g_poseGuns.size()].c_str(), sizeof(g_botKit[slot].primary) - 1);
            g_botLoadoutAssigned[slot] = true;
            printf("bots: pose: slot %u given %s, spawned again\n", slot, g_botKit[slot].primary); fflush(stdout);
            Respawn(ctx, base, entity, BotSide(base, slot) == 1);
            PoseKit(ctx, base, slot, entity);
            g_poseWeapon[slot] = WeaponNumber(base, g_botKit[slot].primary);
            printf("bots: pose: slot %u to hold weapon %u (%s)\n", slot, g_poseWeapon[slot], g_botKit[slot].primary); fflush(stdout);
        }
    }
    // each in his place: in front of the host (along his view), in a row
    // across it, 44 apart, on the host's height
    const float hostYaw = ReadFloat(base, hostRecord + 208) * 0.01745329252f;
    const float fx = cosf(hostYaw), fy = sinf(hostYaw), rx = sinf(hostYaw), ry = -cosf(hostYaw);
    const float h[3] = { ReadFloat(base, hostRecord), ReadFloat(base, hostRecord + 4), ReadFloat(base, hostRecord + 8) };
    for (size_t k = 0; k < slots.size(); k++)
    {
        const uint32_t slot = slots[k];
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = Guest::Read32(base, entity + 512);
        if (int32_t(Guest::Read32(base, record + 304)) <= 0) continue;
        // his gun owned, with rounds (clip +4*(172 + def+1468), spare +4*(80 + def+1460))
        if (const uint32_t n = g_poseWeapon[slot]; n > 0 && n < 128)
        {
            const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
            const uint32_t def = list ? Guest::Read32(base, list + 4 * n) : 0;
            const uint32_t bit = record + 1056 + 4 * (n >> 5);
            Guest::Write32(base, bit, Guest::Read32(base, bit) | (1u << (n & 31)));
            if (def >= 0x40000000u && def < 0xA0000000u)
            {
                const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
                if (clip < 128 && Guest::Read32(base, record + 4 * (172 + clip)) == 0) Guest::Write32(base, record + 4 * (172 + clip), 20);
                if (spare < 128 && Guest::Read32(base, record + 4 * (80 + spare)) == 0) Guest::Write32(base, record + 4 * (80 + spare), 60);
            }
        }
        const float across = (float(k) - float(slots.size() - 1) / 2.0f) * 44.0f;
        const float want[3] = { h[0] + fx * g_poseDistance + rx * across, h[1] + fy * g_poseDistance + ry * across, h[2] };
        const float at[3] = { ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
        if (fabsf(at[0] - want[0]) + fabsf(at[1] - want[1]) + fabsf(at[2] - want[2]) > 4.0f)
            for (int i = 0; i < 3; i++)
            {
                uint32_t bits; memcpy(&bits, &want[i], 4);
                Guest::Write32(base, record + 4 * i, bits);
                Guest::Write32(base, entity + 336 + 4 * i, bits);
                Guest::Write32(base, record + 16 + 4 * i, 0);
            }
        // his view: toward the host, turned by facing; level
        const float face = atan2f(h[1] - want[1], h[0] - want[0]) * 57.29578f + g_poseFacing;
        const float viewYaw = ReadFloat(base, record + 208);
        g_yaw[slot] += std::clamp(AngleDifference(face, viewYaw), -8.0f, 8.0f);
        g_pitch[slot] *= 0.7f;
        g_wantYaw[slot] = face; g_wantPitch[slot] = 0.0f;
    }
}
// 0.5.474: CTF's flag is taken with the use button, not by touch (the game's
// hint_flag_pickup): 473's bots stood on the enemy flag five times and never
// had it (Justin: "still they havent picked up a flag"). On the enemy flag
// (within 80), he stands and presses use in pulses (400 ms on, 200 off) for
// up to 3 s, then moves on.
// 0.5.483: a bot with the enemy flag goes home whatever happens (user: "once
// a bot grabs the enemy flag his mission is to get back to his flag to score
// no matter what"; a carrier cannot shoot, "his team mates defend him"): his
// legs go along his route home (g_steer), sprinting, never stopping,
// crouching or backing off for a fight; no seats, revives, packs or mines
// on the way (FeedBots). Attackers go with him (the CTF goal: escort).
static bool BotCarries(uint32_t slot) { return g_flagCarrier[0] == int(slot) || g_flagCarrier[1] == int(slot); }
// 0.5.522: each bot's own trail this life (a point every 120 walked, up to
// 300), recorded while he does not carry; carrying, he walks it back: a way
// he has just walked, from his side to the enemy flag (520's carriers, on the
// learnt map's routes, wandered 2 minutes and the watchdog's points led away).
// (g_trail itself is declared with the CTF state at the top: 0.5.589's routes use it)
static uint32_t g_trailLife[MaxClients] = {};
// 0.5.536: CTF routes (user: record a run and the bots learn the flag route):
// every time a player, bot or the host, reaches the enemy flag (within 120)
// with a trail that began by his own flag (within 1500), the trail is kept
// as a route for his side on that map, in ctf-routes.txt ("<map> <side>
// x,y,z ..."), up to 16 a side. Attackers walk one (AttackerRoute), carriers
// one reversed when their own trail runs out.
static std::map<std::string, std::vector<std::vector<BotNavigation::Point>>> g_ctfRoutes;   // key: map + side
static std::string g_routesLoadedFor;
static void LoadCtfRoutes(const std::string& map)
{
    if (g_routesLoadedFor == map) return;
    g_routesLoadedFor = map;
    g_ctfRoutes.clear();
    FILE* f = fopen("ctf-routes.txt", "r");
    if (!f) return;
    char line[16384];
    int n = 0;
    while (fgets(line, sizeof(line), f))
    {
        char name[64]; int side = 0, used = 0;
        if (sscanf(line, "%63s %d%n", name, &side, &used) != 2 || map != name || side < 1 || side > 2) continue;
        std::vector<BotNavigation::Point> route;
        const char* p = line + used;
        float x, y, z; int got = 0;
        while (sscanf(p, " %f,%f,%f%n", &x, &y, &z, &got) == 3) { route.push_back({ x, y, z }); p += got; }
        if (route.size() >= 3) { g_ctfRoutes[map + std::to_string(side)].push_back(route); n++; }
    }
    fclose(f);
    printf("bots: ctf routes for %s: %d loaded from ctf-routes.txt\n", map.c_str(), n); fflush(stdout);
}
static void SaveCtfRoute(const std::string& map, uint32_t side, const std::vector<BotNavigation::Point>& route, uint32_t slot)
{
    std::vector<std::vector<BotNavigation::Point>>& list = g_ctfRoutes[map + std::to_string(side)];
    if (list.size() >= 16) return;
    list.push_back(route);
    if (FILE* f = fopen("ctf-routes.txt", "a"))
    {
        fprintf(f, "%s %u", map.c_str(), side);
        for (const BotNavigation::Point& q : route) fprintf(f, " %.0f,%.0f,%.0f", q.x, q.y, q.z);
        fprintf(f, "\n");
        fclose(f);
    }
    printf("bots: ctf route saved for side %u on %s (%zu points, walked by %s slot %u; %zu routes now)\n", side, map.c_str(), route.size(), g_bot[slot] ? "bot" : "player", slot, list.size()); fflush(stdout);
}
static void BotTrail(uint8_t* base, uint32_t slot)
{
    if (slot >= MaxClients) return;
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    // 0.5.536: a new life by health (the host has no g_spawnedTick)
    static bool dead[MaxClients] = {};
    static bool routeSaved[MaxClients] = {};
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) { dead[slot] = true; return; }
    if (dead[slot] || g_trailLife[slot] == 0) { dead[slot] = false; g_trailLife[slot] = 1; g_trail[slot].clear(); routeSaved[slot] = false; }
    if (BotCarries(slot)) return;
    const BotNavigation::Point here{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    std::vector<BotNavigation::Point>& t = g_trail[slot];
    // 0.5.537: a jump (over 400 since the last point: a teleport, a spawn move,
    // an unstick) starts the trail over: 536 saved the test host's teleports
    // as a "route" across the map
    if (!t.empty() && hypotf(t.back().x - here.x, t.back().y - here.y) > 400.0f) { t.clear(); routeSaved[slot] = false; }
    if (t.empty() || hypotf(t.back().x - here.x, t.back().y - here.y) >= 120.0f)
    {
        if (t.size() >= 300) t.erase(t.begin());
        t.push_back(here);
    }
    // a route: at the enemy flag, his trail begun by his own
    const uint32_t side = Side(base, entity);
    if (!routeSaved[slot] && side >= 1 && side <= 2 && g_ctfOwn[side] >= 0 && g_ctfOwn[side] <= 1 && g_ctfOwn[3 - side] >= 0 && g_ctfOwn[3 - side] <= 1 && t.size() >= 5)
    {
        const float* enemy = g_obj.at[g_ctfOwn[3 - side]];
        const float* own = g_obj.at[g_ctfOwn[side]];
        if (hypotf(enemy[0] - here.x, enemy[1] - here.y) < 120.0f && hypotf(own[0] - t.front().x, own[1] - t.front().y) < 1500.0f)
        {
            routeSaved[slot] = true;
            const std::string map = Kernel::CurrentMap();
            LoadCtfRoutes(map);
            SaveCtfRoute(map, side, t, slot);
        }
    }
}
// 0.5.536: an attacker (his CTF role), the enemy flag at its base, not in a
// fight, walks a saved route of his side's toward it (one by his slot, from
// the point nearest him on), his legs after the feelers' as a carrier's
static void AttackerRoute(uint8_t* base, uint32_t slot, BotOrders& orders)
{
    // 0.5.547: off unless COD3_CTF_SAVED_ROUTES=1. 546's self-test trace: with
    // the saved route on his legs an attacker made a median 3 units toward the
    // enemy flag in 3 s (52% of samples stalled) against 13 on his own way
    // (39%); near the flag it pulled against his own path (slot 7 held at 276
    // from it, slot 15 at 1282). The routes are still learnt (BotTrail).
    static const bool on = getenv("COD3_CTF_SAVED_ROUTES") != nullptr && getenv("COD3_CTF_SAVED_ROUTES")[0] == '1';
    if (!on) return;
    if (slot >= MaxClients || !g_bot[slot] || !g_ctfAttacker[slot] || g_ctfOwnLegs[slot] || BotCarries(slot) || orders.wantsFire) return;
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return;
    const uint32_t side = Side(base, entity);
    if (side < 1 || side > 2 || g_ctfOwn[3 - side] < 0 || g_ctfOwn[3 - side] > 1) return;
    const int enemy = g_ctfOwn[3 - side];
    if (g_flagCarrier[enemy] >= 0 || g_flagDropped[enemy]) return;
    const std::string map = Kernel::CurrentMap();
    LoadCtfRoutes(map);
    const auto it = g_ctfRoutes.find(map + std::to_string(side));
    if (it == g_ctfRoutes.end() || it->second.empty()) return;
    // 0.5.542: a route he gets nowhere on is dropped for him for 60 s, the
    // next one tried, and with none left the navigation's own way (541: side
    // 1's routes go over raised places, z 55-80, a bot cannot climb; its
    // attackers walked into the wall below at -1103 132 the whole game)
    static uint32_t bannedUntil[MaxClients][8] = {};
    static uint32_t progressAt[MaxClients] = {}; static size_t bestNext[MaxClients] = {}; static size_t onRoute[MaxClients] = {};
    const uint32_t now = GetTickCount();
    const size_t count = it->second.size();
    size_t pick = count;
    for (size_t k = 0; k < count && pick == count; k++) { const size_t r = (slot + k) % count; if (r >= 8 || now >= bannedUntil[slot][r]) pick = r; }
    if (pick == count) return;
    if (onRoute[slot] != pick) { onRoute[slot] = pick; bestNext[slot] = 0; progressAt[slot] = now; }
    const std::vector<BotNavigation::Point>& route = it->second[pick];
    const float mx = ReadFloat(base, record), my = ReadFloat(base, record + 4);
    if (hypotf(g_obj.at[enemy][0] - mx, g_obj.at[enemy][1] - my) < 250.0f) return;   // the last steps his own
    size_t nearest = 0; float bestD = 1e30f;
    for (size_t i = 0; i < route.size(); i++) { const float d = hypotf(route[i].x - mx, route[i].y - my); if (d < bestD) { bestD = d; nearest = i; } }
    if (bestD > 600.0f) { progressAt[slot] = now; return; }   // far off it: his own way back to it
    size_t next = nearest;
    while (next + 1 < route.size() && hypotf(route[next].x - mx, route[next].y - my) < 80.0f) next++;
    if (next > bestNext[slot] || now - progressAt[slot] > 30000) { bestNext[slot] = next; progressAt[slot] = now; }
    else if (now - progressAt[slot] > 4000)
    {
        if (pick < 8) bannedUntil[slot][pick] = now + 60000;
        printf("bots: slot %u (side %u) gets nowhere on saved route %zu (stuck before point %zu at %.0f %.0f): dropped for 60 s\n", slot, side, pick, next, mx, my);
        fflush(stdout);
        onRoute[slot] = count;
        return;
    }
    const float dx = route[next].x - mx, dy = route[next].y - my;
    const float rel = atan2f(dy, dx) - ReadFloat(base, record + 208) * 0.01745329252f;
    orders.forward = int8_t(std::clamp(cosf(rel) * 127.0f, -127.0f, 127.0f));
    orders.right = int8_t(std::clamp(-sinf(rel) * 127.0f, -127.0f, 127.0f));
    static uint32_t told = 0;
    if (told++ < 20) { printf("bots: slot %u (side %u) takes a saved route to the enemy flag (%zu points, at point %zu)\n", slot, side, route.size(), next); fflush(stdout); }
}
static void CarrierHome(uint8_t* base, uint32_t slot, BotOrders& orders)
{
    static bool told[MaxClients] = {};
    if (!BotCarries(slot)) { told[slot] = false; return; }
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return;
    const float mx = ReadFloat(base, record), my = ReadFloat(base, record + 4);
    // 0.5.522: his own trail back (the last points first), then his flag
    float target[2] = { g_steer[slot][0], g_steer[slot][1] };
    bool have = g_steerValid[slot];
    std::vector<BotNavigation::Point>& t = g_trail[slot];
    // 0.5.535: from the trail point nearest him (the later ones dropped: he
    // may stand past a corner from the last one he laid), then on toward home
    if (!t.empty())
    {
        size_t nearest = t.size() - 1; float bestD = 1e30f;
        for (size_t i = 0; i < t.size(); i++) { const float d = hypotf(t[i].x - mx, t[i].y - my); if (d < bestD) { bestD = d; nearest = i; } }
        t.resize(nearest + 1);
    }
    while (!t.empty() && hypotf(t.back().x - mx, t.back().y - my) < 80.0f) t.pop_back();
    {   // 0.5.523: within 300 of his flag, straight to it (the trail's points lead back out)
        const uint32_t s = BotSide(base, slot);
        if (s >= 1 && s <= 2 && g_ctfOwn[s] >= 0 && g_ctfOwn[s] <= 1 && hypotf(g_obj.at[g_ctfOwn[s]][0] - mx, g_obj.at[g_ctfOwn[s]][1] - my) < 300.0f) t.clear();
    }
    if (!t.empty()) { target[0] = t.back().x; target[1] = t.back().y; have = true; }
    else
    {
        const uint32_t side = BotSide(base, slot);
        if (side >= 1 && side <= 2 && g_ctfOwn[side] >= 0 && g_ctfOwn[side] <= 1) { target[0] = g_obj.at[g_ctfOwn[side]][0]; target[1] = g_obj.at[g_ctfOwn[side]][1]; have = true; }
    }
    if (!have) return;
    const float dx = target[0] - mx, dy = target[1] - my;
    if (dx * dx + dy * dy < 20.0f * 20.0f) return;
    // 0.5.568: his legs are Think's (his learnt route home, round the walls)
    // unless Think gave him none or he is within 300 of his flag (then straight
    // onto it, to touch it). 567's 10-minute self-test, the first bot takes since
    // the spawns were put right (3): carrier slot 11 stood 9 s at 1823 215, Think's
    // legs along his route (111 60) and these, straight back along his trail,
    // the other way (-108 -66), into a wall, till he was shot.
    bool straight = orders.forward == 0 && orders.right == 0;
    {
        const uint32_t s = BotSide(base, slot);
        if (s >= 1 && s <= 2 && g_ctfOwn[s] >= 0 && g_ctfOwn[s] <= 1 && hypotf(g_obj.at[g_ctfOwn[s]][0] - mx, g_obj.at[g_ctfOwn[s]][1] - my) < 300.0f) straight = true;
    }
    if (straight)
    {
        const float rel = atan2f(dy, dx) - ReadFloat(base, record + 208) * 0.01745329252f;
        orders.forward = int8_t(std::clamp(cosf(rel) * 127.0f, -127.0f, 127.0f));
        orders.right = int8_t(std::clamp(-sinf(rel) * 127.0f, -127.0f, 127.0f));
    }
    // user: "the flag carrier cant shoot so just make his mission home while
    // his team mates defend him": no firing, sprint all the way
    orders.buttons &= ~(UseButtons | StanceButton | 0x8u | 1u);   // 0x8: aim down the sights
    orders.wantsFire = false;
    orders.buttons |= SprintButton;
    if (!told[slot]) { told[slot] = true; printf("bots: slot %u has the flag: home whatever happens\n", slot); fflush(stdout); }
}
// 0.5.517: THE CTF PICKUP IS ASKED FOR BY THE PLAYER'S CLIENT. 516's wraps:
// the flag handler (sub_82204A08) and its caller (sub_82205788) ran only for
// the host's takes, never a bot's. Their caller, sub_823EB8F0, handles a
// message from a player's client (read from its bit stream, sub_820D9D08)
// and calls the server's callback at Callbacks (0x82BCBF64) +3420 with the
// message's value, a pointer to the player's handle and a flag: the client,
// seeing its player in the flag's pick-up area, asks; the server gives. A
// bot has no client, so nothing ever asked (488-516 all agree). Now, while a
// bot stands on the enemy flag at its base (BotFlagUse pressing), every 500
// ms that callback is called for him as the host's request was: (2, his
// handle, 0) (516's log: sub_82205788(2, &0x4001, 0, ..) for each take).
// 0.5.519: a flag carrier not 150 nearer home in 10 s is put 3 spots further
// along his learnt route home (the game's teleport, as the navigation's
// unstuck does). 518's carrier circled one spot (960 270) for 100 s, the
// navigation's own stuck test never firing (the jitter counted as progress).
static void CarrierUnstick(PPCContext& ctx, uint8_t* base, uint32_t slot)
{
    static float best[MaxClients] = {};
    static uint32_t since[MaxClients] = {};
    // 0.5.538: a bot carrier killed drops the flag, as the game does for a
    // player (user: "when an enemy drops a flag it just goes invisible ... but
    // it shows on the radar"; the game never dropped it for a bot): the drop
    // callback (Callbacks +3424, 523: it drops a carrier's flag where he is)
    if (slot < MaxClients && g_bot[slot] && (BotCarries(slot) || g_dropPending[slot]))
    {
        const uint32_t dlist = Guest::Read32(base, EntityList);
        const uint32_t dEntity = dlist ? Guest::Read32(base, dlist + 4 * (slot + 1)) : 0;
        const uint32_t dRecord = dEntity ? Guest::Read32(base, dEntity + 512) : 0;
        static bool dropped[MaxClients] = {};
        const bool deadNow = !dRecord || int32_t(Guest::Read32(base, dRecord + 304)) <= 0;
        if (!deadNow) dropped[slot] = false;
        else if (!dropped[slot])
        {
            dropped[slot] = true; g_dropPending[slot] = false;
            uint32_t handle = 0;
            for (uint32_t i = 0; i < 4096 && !handle && dEntity; i++) if (Guest::Read32(base, 0x82BD1FC0 + 8 * i) == dEntity) handle = 0x4000u | i;
            const uint32_t object = Guest::Read32(base, 0x82BCBF64);
            const uint32_t drop = object ? Guest::Read32(base, object + 3424) : 0;
            if (handle && drop)
            {
                static uint32_t cell = 0;
                if (!cell) cell = Guest::AllocatePhysical(0x10);
                Guest::Write32(base, cell, handle);
                printf("bots: carrier slot %u killed: his flag dropped (the drop callback %08X)\n", slot, drop); fflush(stdout);
                BotClientScope clientScope(base, slot, "flag drop");
                Call(ctx, base, drop, cell, 0, 0);
            }
            return;
        }
    }
    if (slot >= MaxClients || !BotCarries(slot)) { if (slot < MaxClients) since[slot] = 0; return; }
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) return;
    const uint32_t side = BotSide(base, slot);
    if (side < 1 || side > 2 || g_ctfOwn[side] < 0 || g_ctfOwn[side] > 1) return;
    const float* home = g_obj.at[g_ctfOwn[side]];
    const BotNavigation::Point feet{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    const float dist = hypotf(home[0] - feet.x, home[1] - feet.y);
    const uint32_t now = GetTickCount();
    if (!since[slot] || dist < best[slot] - 150.0f) { best[slot] = dist; since[slot] = now; return; }
    if (now - since[slot] < 10000) return;
    since[slot] = now; best[slot] = dist;
    // 0.5.523: within 400 of home, onto his flag (522 put a carrier 108 from
    // home back along his trail, away)
    if (dist < 400.0f)
    {
        static uint32_t nearPos = 0;
        if (!nearPos) nearPos = Guest::AllocatePhysical(0x20);
        auto putN = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
        putN(nearPos, home[0] + 20.0f); putN(nearPos + 4, home[1]); putN(nearPos + 8, home[2] + 1.0f);
        PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        place.r3.u64 = entity; place.r4.u64 = nearPos; place.lr = 0;
        Guest::Lookup(0x8255E118)(place, base);
        printf("bots: carrier slot %u stuck %.0f from home: put by his flag\n", slot, dist); fflush(stdout);
        return;
    }
    // 0.5.522: along his own trail first: the point two back
    // 0.5.586: only one within 300 (a step back out of a dead end); farther,
    // forwards along his route instead: 585's self-test put slot 6, stuck at
    // 1021 1320, back at 1998 1211, by the base he took the flag from
    if (g_trail[slot].size() >= 2 &&
        hypotf(g_trail[slot][g_trail[slot].size() - 2].x - feet.x, g_trail[slot][g_trail[slot].size() - 2].y - feet.y) < 300.0f)
    {
        g_trail[slot].pop_back();
        const BotNavigation::Point to = g_trail[slot].back();
        g_trail[slot].pop_back();
        static uint32_t trailPos = 0;
        if (!trailPos) trailPos = Guest::AllocatePhysical(0x20);
        auto putT = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
        putT(trailPos, to.x); putT(trailPos + 4, to.y); putT(trailPos + 8, to.z + 1.0f);
        PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        place.r3.u64 = entity; place.r4.u64 = trailPos; place.lr = 0;
        Guest::Lookup(0x8255E118)(place, base);
        for (int k = 0; k < 3; k++) putT(record + 16 + 4 * k, 0.0f);
        printf("bots: carrier slot %u stuck %.0f from home for 10 s: put back along his own trail at %.0f %.0f (%zu points left)\n", slot, dist, to.x, to.y, g_trail[slot].size()); fflush(stdout);
        return;
    }
    const int32_t cell = g_mapKnow.Nearest({ home[0], home[1], home[2] }, 600.0f);
    const std::vector<BotNavigation::Point> way = cell >= 0 ? g_mapKnow.Path(feet, cell) : std::vector<BotNavigation::Point>{};
    if (way.empty()) { printf("bots: carrier slot %u stuck %.0f from home, no learnt route\n", slot, dist); fflush(stdout); return; }
    const BotNavigation::Point to = way[std::min<size_t>(3, way.size() - 1)];
    static uint32_t position = 0;
    if (!position) position = Guest::AllocatePhysical(0x20);
    auto put = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
    put(position, to.x); put(position + 4, to.y); put(position + 8, to.z + 1.0f);
    PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
    place.r3.u64 = entity; place.r4.u64 = position; place.lr = 0;
    Guest::Lookup(0x8255E118)(place, base);
    for (int k = 0; k < 3; k++) put(record + 16 + 4 * k, 0.0f);
    printf("bots: carrier slot %u stuck %.0f from home for 10 s: put at %.0f %.0f along his route (%zu spots)\n", slot, dist, to.x, to.y, way.size()); fflush(stdout);
}
static void BotFlagClaim(PPCContext& ctx, uint8_t* base, uint32_t slot)
{
    static uint32_t lastAt[MaxClients] = {};
    if (slot >= MaxClients || !g_bot[slot]) return;
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0 || Guest::Read32(base, record + 1924) != 3) return;
    const uint32_t side = BotSide(base, slot);
    if (side < 1 || side > 2 || g_ctfOwn[side] < 0 || g_ctfOwn[3 - side] < 0) return;
    const int enemy = g_ctfOwn[3 - side], own = g_ctfOwn[side];
    if (enemy > 1 || own > 1) return;
    const float mx = ReadFloat(base, record), my = ReadFloat(base, record + 4);
    auto near60 = [&](const float* p) { return hypotf(p[0] - mx, p[1] - my) < 60.0f; };
    // 0.5.518: the four requests a client makes (the first word the side
    // whose flag it is): the enemy's at its base or lying out (take); his own
    // at its base while he carries the enemy's (capture); his own lying out
    // (return)
    uint32_t ask = 0; const char* why = "";
    // 0.5.533: in the unattended test only side 1 takes (so side 1's flag stays
    // home and its carrier can capture: 532's took both at once)
    static const bool testMode = [] { const char* m = getenv("COD3_CTF_PROBE"); return m && m[0]; }();   // 0.5.538: probe only
    if (testMode && side == 2 && !BotCarries(slot)) {}
    else if (!BotCarries(slot) && g_flagCarrier[enemy] < 0 && !g_flagDropped[enemy] && near60(g_obj.at[enemy])) { ask = 3 - side; why = "take"; }
    // 0.5.524: not the enemy's flag lying by his OWN flag (523: a carrier's
    // drop at his base (+3424, "C") was taken straight back 5 times: if
    // dropping it there is the capture, leave it to the game)
    else if (!BotCarries(slot) && g_flagDropped[enemy] && near60(g_flagDropAt[enemy]) &&
             hypotf(g_flagDropAt[enemy][0] - g_obj.at[own][0], g_flagDropAt[enemy][1] - g_obj.at[own][1]) > 300.0f) { ask = 3 - side; why = "take (lying out)"; }
    // 0.5.531: no capture requests: a capture is the game's own, walking onto
    // the home flag (user; 526's host); A (his side) made the game announce
    // "flag returned" twice a second (user: "it says enemy flag returned over
    // and over"), B did nothing, C dropped the flag
    else if (g_flagDropped[own] && near60(g_flagDropAt[own])) { ask = side; why = "return"; }
    // 0.5.532: the capture, asked as a client does (sub_823E3298 -> Callbacks
    // +3432 -> sub_82203E00): carrying, on his own flag at its base, every 2 s:
    // (his slot, his side - 1); sub_82203E00's wrapper shows a real one's
    if (!ask && BotCarries(slot) && g_flagCarrier[own] < 0 && !g_flagDropped[own] &&
        hypotf(g_obj.at[own][0] - mx, g_obj.at[own][1] - my) < 40.0f)
    {
        static uint32_t capAt[MaxClients] = {};
        const uint32_t nowCap = GetTickCount();
        if (nowCap - capAt[slot] < 2000) return;
        capAt[slot] = nowCap;
        const uint32_t object = Guest::Read32(base, 0x82BCBF64);
        const uint32_t capture = object ? Guest::Read32(base, object + 3432) : 0;
        // 0.5.533: (his client number, 1 or 0 by turns): sub_823E3298 passes the
        // message's first value as a client index (a per-client array) and a
        // 0/1 value second; 532 never asked (its own flag was away: no
        // capture then, as in the game)
        static uint32_t capTurn[MaxClients] = {};
        const uint32_t second = (capTurn[slot]++ % 2) ? 0u : 1u;
        g_captureTry = int(second);
        static int toldCap = 0;
        if (toldCap++ < 40) { printf("bots: slot %u (side %u) on his own flag carrying: the capture callback %08X asked (%u, %u), as a client does\n", slot, side, capture, slot, second); fflush(stdout); }
        if (!capture) return;
        BotClientScope clientScope(base, slot, "flag capture");
        Call(ctx, base, capture, slot, second);
        return;
    }
    if (!ask) return;
    const uint32_t now = GetTickCount();
    if (now - lastAt[slot] < 500) return;
    lastAt[slot] = now;
    uint32_t handle = 0;
    for (uint32_t i = 0; i < 4096 && !handle; i++) if (Guest::Read32(base, 0x82BD1FC0 + 8 * i) == entity) handle = 0x4000u | i;
    const uint32_t object = Guest::Read32(base, 0x82BCBF64);
    const uint32_t callback = object ? Guest::Read32(base, object + 3420) : 0;
    if (!handle || !callback) return;
    static uint32_t cell = 0;
    if (!cell) cell = Guest::AllocatePhysical(0x10);
    Guest::Write32(base, cell, handle);
    static int told = 0;
    if (told++ < 60) { printf("bots: slot %u (side %u) asks the server's flag callback %08X to %s: (%u, handle %08X, 0), as a client does\n", slot, side, callback, why, ask, handle); fflush(stdout); }
    BotClientScope clientScope(base, slot, "flag claim");
    // 0.5.518: the first word is the side whose flag it is (517: side 1 bots
    // asking with 2 took side 2's flag; a side 2 bot asking with 2 at side
    // 1's flag was refused 14 times)
    if (ask == 0xC0DE)
    {
        const uint32_t other = object ? Guest::Read32(base, object + 3424) : 0;   // 0.5.523: try C
        if (other) Call(ctx, base, other, cell, 0, 0);
        return;
    }
    Call(ctx, base, callback, ask, cell, 0);
}
static void BotFlagUse(uint8_t* base, uint32_t slot, BotOrders& orders)
{
    static uint32_t arrived[MaxClients] = {};
    if (!g_obj.valid || g_obj.count < 2 || GetTickCount() - g_obj.when > 10000 || g_obj.mode.find("ctf") == std::string::npos) { arrived[slot] = 0; return; }
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!record || int32_t(Guest::Read32(base, record + 304)) <= 0) { arrived[slot] = 0; return; }
    const float me[3] = { ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    const uint32_t side = BotSide(base, slot);
    // 0.5.480: the enemy flag only from the sides known from real spawns
    if (side < 1 || side > 2 || g_ctfOwn[3 - side] < 0 || g_ctfOwn[3 - side] >= g_obj.count) { arrived[slot] = 0; return; }
    const int enemy = g_ctfOwn[3 - side];
    // and on his own (within 80) too, in case bringing it in wants use as well
    auto near80 = [&](int k) { return hypotf(g_obj.at[k][0] - me[0], g_obj.at[k][1] - me[1]) < 80.0f && fabsf(g_obj.at[k][2] - me[2]) < 120.0f; };
    // 0.5.482: at base only while it is there (not carried or lying out)
    int at = (enemy <= 1 && g_flagCarrier[enemy] < 0 && !g_flagDropped[enemy] && near80(enemy)) ? enemy : -1;
    // 0.5.476: and on a flag lying out (dropped; 0.5.482: the real state)
    for (int k = 0; k < 2 && k < g_obj.count && at < 0; k++)
        if (g_flagDropped[k] && hypotf(g_flagDropAt[k][0] - me[0], g_flagDropAt[k][1] - me[1]) < 80.0f && fabsf(g_flagDropAt[k][2] - me[2]) < 120.0f) at = k;
    const uint32_t now = GetTickCount();
    if (at < 0) { arrived[slot] = 0; return; }
    if (!arrived[slot]) { arrived[slot] = now; printf("bots: slot %u presses use on the %s flag\n", slot, at == enemy ? "enemy" : "his own"); fflush(stdout); }
    const uint32_t held = now - arrived[slot];
    // 0.5.479: on to the flag's very place while pressing, for 5 s (was:
    // stood still within 80, 3 s): 478's bots pressed use on the enemy flag
    // 23 times and never had it (its props never moved), standing short of
    // its small touch area; Justin took it walking onto it
    // 0.5.484: 8 s (was 5)
    if (held > 8000) { g_flagUseMode[slot] = 0; return; }
    const float* spot = (g_flagDropped[at] ? g_flagDropAt[at] : g_obj.at[at]);
    const float dx = spot[0] - me[0], dy = spot[1] - me[1];
    orders.right = 0;
    // 0.5.484: facing it and looking down at it, stopped 30 out (was: onto
    // its very place, eyes level): the user took it from 40 away, looking at
    // it; X counts as use only with the pickup prompt up, which needs the
    // flag in view. 483's bots held X 116 times with no touch logged at all.
    const float want = atan2f(dy, dx) * 57.2957795f;
    g_yaw[slot] += fmaxf(-8.0f, fminf(8.0f, AngleDifference(want, g_yaw[slot])));
    const float flat = sqrtf(dx * dx + dy * dy);
    g_pitch[slot] = atan2f((me[2] + 60.0f) - (spot[2] + 20.0f), fmaxf(flat, 1.0f)) * 57.2957795f;   // down is positive
    orders.forward = flat > 12.0f ? 70 : 0;   // 0.5.497: 12 (was 30; the host took it from 18, still)
    orders.buttons &= ~(StanceButton | SprintButton);   // 0.5.497: upright, not sprinting
    // 0.5.480: held (user: took it holding use), after a 200 ms release so
    // the press is new; again from fresh each 2.5 s
    // 0.5.483: 0x40 alone (the console's X): the user took it holding 0x40
    // (482's log: host buttons 00000044, 00000040, then TAKEN); the bots held
    // 0x20|0x40|0x400 (UseButtons, for vehicles) 1092 times and never had it,
    // the reload (0x400) most likely in the way
    // 0.5.484: in turns of 2.4 s: X alone 1 s, then X with activate (0x20,
    // no reload) 1 s, each after a 200 ms release; the mode in use goes into
    // the TAKEN line (g_flagUseMode), to learn which the game wants
    orders.buttons &= ~UseButtons;
    // 0.5.497: X alone held steadily after a 200 ms release, as the host's
    // command-only take in 496 (no turns with activate)
    if (held >= 200) { orders.buttons = (orders.buttons & ~1u) | FlagUseButton; orders.wantsFire = false; g_flagUseMode[slot] = 1; }
}
// 0.5.475: CTF's sides spawn by their own flag. 474's CTF on Rouen (Justin:
// "the enemies spawn on our flag and we spawn on the enemies flag ... Teams
// should spawn on there respected flag side"): the game put the allies about
// 1300 1600 (by the flag at 2112 462) and the axis about -1650 -1100 (by the
// one at -1792 -91). Once a map, each side's own flag is set as the one
// farther from where its game spawns are (g_ctfOwn); then everyone (the
// players too) who spawns nearer the enemy's flag than his own is moved (the
// game's teleport, sub_8255E118) to a spawn point within 1400 of his own
// flag, none within 80 of a living player, away from living enemies where
// there is one (at random among those, so they do not stack).
// 0.5.481: what the game changes in a flag when it is taken. A taken flag's
// model never moved (478-480: no "prop moved" for the flags, though the user
// took one in 478), so the port could not tell a flag taken. Every 250 ms the
// script_models and trigger_multiples within 100 of a flag are compared word
// by word (their first 1 KB) with the last look; a changed word is logged with
// who is within 200 of that flag (slot, side, bot or player, distance). A word
// that has changed 12 times is muted (clocks, animation).
static void FlagEntityWatch(uint8_t* base, const std::string& key)
{
    struct Watched { uint32_t e, index; char cls[20]; int flag; uint32_t words[256]; uint8_t changes[256]; };
    static std::vector<Watched> list;
    static std::string listFor;
    static uint32_t last = 0, told = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    if (listFor != key)
    {
        listFor = key; list.clear(); told = 0; g_flagModels.clear(); g_flagTriggers.clear(); g_flagCarrier[0] = g_flagCarrier[1] = -1; g_flagDropped[0] = g_flagDropped[1] = false;
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = Guest::Read32(base, 0x82BD1FC0 + 8 * i);
            if (e < 0x40000000u || e >= 0xA0000000u) continue;
            const uint32_t cls = Guest::Read32(base, e + 548);
            if (cls < 0x40000000u || cls >= 0xA0000000u) continue;
            char name[20] = {};
            for (int c = 0; c < 19; c++) { const char ch = char(Guest::Read8(base, cls + 12 + c)); if (!ch) break; name[c] = char(tolower(static_cast<unsigned char>(ch))); }
            if (strcmp(name, "script_model") != 0 && strcmp(name, "trigger_multiple") != 0) continue;
            const float p[2] = { ReadFloat(base, e + 336), ReadFloat(base, e + 340) };
            for (int k = 0; k < 2; k++)
                if (hypotf(p[0] - g_obj.at[k][0], p[1] - g_obj.at[k][1]) < 100.0f)
                {
                    Watched w = {}; w.e = e; w.index = i; w.flag = k; memcpy(w.cls, name, sizeof(w.cls));
                    for (int j = 0; j < 256; j++) w.words[j] = Guest::Read32(base, e + 4 * j);
                    list.push_back(w);
                    if (strcmp(name, "script_model") == 0) g_flagModels.push_back(e);   // 0.5.493
                    else g_flagTriggers.push_back(e);                                 // 0.5.512
                    printf("flagwatch: watching %s %u (entity %08X) at flag %d (%.0f %.0f)\n", name, i, e, k, g_obj.at[k][0], g_obj.at[k][1]); fflush(stdout);
                    break;
                }
        }
    }
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (Watched& w : list)
    {
        std::string changed;
        for (int j = 0; j < 256; j++)
        {
            const uint32_t v = Guest::Read32(base, w.e + 4 * j);
            if (v == w.words[j]) continue;
            if (w.changes[j] < 255) w.changes[j]++;
            if (w.changes[j] <= 12 && changed.size() < 600)
            {
                char t[48]; snprintf(t, sizeof(t), " +%d %08X->%08X", 4 * j, w.words[j], v); changed += t;
            }
            w.words[j] = v;
        }
        if (changed.empty() || told >= 300) continue;
        told++;
        std::string around;
        for (uint32_t o = 0; entities && o < ActiveSlots() && o < MaxClients; o++)
        {
            const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
            const float d = hypotf(ReadFloat(base, r) - g_obj.at[w.flag][0], ReadFloat(base, r + 4) - g_obj.at[w.flag][1]);
            if (d > 200.0f) continue;
            char t[48]; snprintf(t, sizeof(t), " slot %u side %u %s %.0f;", o, Side(base, e), g_bot[o] ? "bot" : "PLAYER", d); around += t;
        }
        printf("flagwatch: %s %u at flag %d changed:%s | near:%s\n", w.cls, w.index, w.flag, changed.c_str(), around.empty() ? " nobody" : around.c_str());
        fflush(stdout);
    }
    // 0.5.482: each flag's state from its script_models (481's capture: taken,
    // the model went to z -9970 with the taker 24 from it; dropped, shown at
    // 204 -531; captured, back at its base)
    for (int k = 0; k < 2; k++)
    {
        // 0.5.525: a carrier who has died carries nothing (524's stayed a
        // "carrier" after death and respawn, wandering for his "home")
        if (g_flagCarrier[k] >= 0 && entities)
        {
            const uint32_t ce = Guest::Read32(base, entities + 4 * (uint32_t(g_flagCarrier[k]) + 1));
            const uint32_t cr = ce ? Guest::Read32(base, ce + 512) : 0;
            if (!cr || int32_t(Guest::Read32(base, cr + 304)) <= 0)
            {
                printf("flagwatch: flag %d's carrier slot %d died\n", k, g_flagCarrier[k]); fflush(stdout);
                if (g_flagCarrier[k] > 0 && g_flagCarrier[k] < int(MaxClients) && g_bot[g_flagCarrier[k]]) g_dropPending[g_flagCarrier[k]] = true;   // 0.5.538
                g_flagCarrier[k] = -1;
            }
        }
        bool hidden = false, away = false; float at[3] = {};
        for (const Watched& w : list)
        {
            if (w.flag != k || strcmp(w.cls, "script_model") != 0) continue;
            const float p[3] = { ReadFloat(base, w.e + 336), ReadFloat(base, w.e + 340), ReadFloat(base, w.e + 344) };
            if (!std::isfinite(p[2])) continue;
            if (p[2] < -5000.0f) hidden = true;
            else if (hypotf(p[0] - g_obj.at[k][0], p[1] - g_obj.at[k][1]) > 50.0f) { away = true; memcpy(at, p, sizeof(at)); }
        }
        const float* last = g_flagDropped[k] ? g_flagDropAt[k] : g_obj.at[k];
        if (hidden && g_flagCarrier[k] < 0)
        {
            // 0.5.520: this flag's own weapon: side 2's flag gives mp_flag_allies
            const int ownerSide = g_ctfOwn[1] == k ? 1 : g_ctfOwn[2] == k ? 2 : 0;
            // taken: by the nearest living player to where it was
            // 0.5.588: of the other side only (a side's own flag is never carried
            // by it): 586's carrier test had flag 1 "taken" 13 times by side 2's
            // own defenders standing by it (the real taker a side 1 attacker
            // further off), and the stuck-carrier help put them on it every 10 s
            int best = -1; float bestD = 250.0f; uint32_t bestSide = 0;
            for (uint32_t o = 0; entities && o < ActiveSlots() && o < MaxClients; o++)
            {
                const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
                const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
                if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
                const uint32_t oSide = g_bot[o] ? BotSide(base, o) : Side(base, e);
                if (ownerSide && oSide == uint32_t(ownerSide)) continue;
                const float d = hypotf(ReadFloat(base, r) - last[0], ReadFloat(base, r + 4) - last[1]);
                if (d < bestD) { bestD = d; best = int(o); bestSide = oSide; }
            }
            // 0.5.501: the flag weapon's owner when there is one (the real carrier)
            uint32_t flagWeapon = 0;
            const int holder = ownerSide ? FlagWeaponHolder(base, ownerSide == 2 ? "mp_flag_allies" : "mp_flag_axis", &flagWeapon) : -1;
            if (holder >= 0 && holder != best)
            {
                printf("flagwatch: the nearest was slot %d, but slot %d owns the flag weapon %u: him\n", best, holder, flagWeapon); fflush(stdout);
                best = holder;
                const uint32_t e = Guest::Read32(base, entities + 4 * (uint32_t(holder) + 1));
                bestSide = e ? Side(base, e) : 0;
            }
            // 0.5.526: nobody found (a flag hidden with no holder: 525's after
            // its carrier died): no take, and no line every frame
            if (best < 0)
            {
                static uint32_t toldAt[2] = {};
                if (GetTickCount() - toldAt[k] > 10000) { toldAt[k] = GetTickCount(); printf("flagwatch: flag %d hidden with nobody holding it\n", k); fflush(stdout); }
                continue;
            }
            g_flagCarrier[k] = best; g_flagDropped[k] = false;
            const uint8_t mode = best >= 0 ? g_flagUseMode[best] : 0;   // 0.5.484
            printf("flagwatch: flag %d (%.0f %.0f) TAKEN by slot %d (side %u, %s, %.0f away%s)\n", k, g_obj.at[k][0], g_obj.at[k][1], best, bestSide, best >= 0 && g_bot[best] ? "bot" : "player", bestD,
                   best >= 0 && g_bot[best] ? (mode == 1 ? ", holding X alone" : mode == 2 ? ", holding X with activate" : ", not pressing") : ""); fflush(stdout);
            if (best > 0 && g_bot[best]) { printf("flagwatch: a BOT took it (slot %d, side %u)\n", best, bestSide); fflush(stdout); }   // 0.5.511
            // 0.5.520: a take no longer proves or puts right the sides (519: a take
            // credited to the other flag's carrier swapped them mid-match); the
            // sides come from ctf-flags.txt and the start; only an owner-checked
            // take (the flag's own weapon) by a player still proves them
            const bool checked = holder >= 0 && holder == best;
            if (checked && !g_bot[best] && bestSide >= 1 && bestSide <= 2) { g_ctfProvenSide = bestSide; g_ctfProvenFlag = k; }   // 0.5.487: saved by CtfSpawnSides
            if (false && best >= 0 && bestSide >= 1 && bestSide <= 2 && !g_flagDropped[k] && g_ctfOwn[bestSide] == k)
            {
                g_ctfOwn[bestSide] = 1 - k; g_ctfOwn[3 - bestSide] = k;
                printf("bots: ctf sides corrected by a take: side %u owns the flag at %.0f %.0f\n", bestSide, g_obj.at[1 - k][0], g_obj.at[1 - k][1]); fflush(stdout);
            }
        }
        else if (!hidden && away && (!g_flagDropped[k] || hypotf(at[0] - g_flagDropAt[k][0], at[1] - g_flagDropAt[k][1]) > 1.0f))
        {
            g_flagCarrier[k] = -1; g_flagDropped[k] = true; memcpy(g_flagDropAt[k], at, sizeof(at));
            printf("flagwatch: flag %d dropped at %.0f %.0f %.0f\n", k, at[0], at[1], at[2]); fflush(stdout);
        }
        else if (!hidden && !away && (g_flagCarrier[k] >= 0 || g_flagDropped[k]))
        {
            // 0.5.584: home straight from carried (no drop between) is a capture
            if (g_flagCarrier[k] >= 0 && !g_flagDropped[k]) { printf("flagwatch: flag %d CAPTURED by slot %d\n", k, g_flagCarrier[k]); fflush(stdout); }
            g_flagCarrier[k] = -1; g_flagDropped[k] = false;
            printf("flagwatch: flag %d home (the last bot capture request's second value: %d)\n", k, g_captureTry); fflush(stdout);
        }
    }
}
// 0.5.485: what the game's scripts know about a player that they do not know
// about a bot. 484: bots could neither take the enemy flag nor return their
// own dropped one (98 and 101 tries; it lay out 9 minutes) while the user
// took it at once: the CTF script most likely compares the player's own
// script-side team with the flag's, and the port puts bots on a side through
// the engine. The script variables of the host (slot 0) and of the first bot
// of each side (sub_821DE680 on the entity handle, +484: +0 the count, +8
// pairs of name hash and value) are logged side by side, with what differs:
// once 20 s into a CTF match and at each take.
static void ScriptVarsCompare(PPCContext& ctx, uint8_t* base, const char* why)
{
    const uint32_t list = Guest::Read32(base, EntityList);
    if (!list) return;
    auto vars = [&](uint32_t slot, std::map<uint32_t, uint32_t>& out) {
        const uint32_t e = Guest::Read32(base, list + 4 * (slot + 1));
        const uint32_t handle = e ? Guest::Read32(base, e + 484) : 0;
        if (!handle) return false;
        const uint32_t table = Call(ctx, base, 0x821DE680, handle);
        if (table < 0x40000000u || table >= 0xA0000000u) return false;
        const uint32_t count = std::min<uint32_t>(Guest::Read32(base, table), 256);
        for (uint32_t i = 0; i < count; i++) out[Guest::Read32(base, table + 8 + 8 * i)] = Guest::Read32(base, table + 12 + 8 * i);
        return true;
    };
    std::map<uint32_t, uint32_t> host;
    if (!vars(0, host)) return;
    uint32_t bots[3] = { 0, MaxClients, MaxClients };
    for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
    {
        if (!g_bot[o]) continue;
        const uint32_t s = BotSide(base, o);
        if (s >= 1 && s <= 2 && bots[s] == MaxClients) bots[s] = o;
    }
    const uint32_t hostSide = BotSide(base, 0);
    std::string line;
    for (const auto& kv : host) { char t[24]; snprintf(t, sizeof(t), " %08X=%08X", kv.first, kv.second); line += t; }
    printf("scriptvars (%s): host slot 0 side %u, %zu vars:%s\n", why, hostSide, host.size(), line.c_str());
    for (int s = 1; s <= 2; s++)
    {
        if (bots[s] == MaxClients) continue;
        std::map<uint32_t, uint32_t> bot;
        if (!vars(bots[s], bot)) continue;
        std::string missing, extra, differ;
        for (const auto& kv : host)
        {
            const auto it = bot.find(kv.first);
            char t[40];
            if (it == bot.end()) { snprintf(t, sizeof(t), " %08X", kv.first); missing += t; }
            else if (it->second != kv.second) { snprintf(t, sizeof(t), " %08X(%08X/%08X)", kv.first, kv.second, it->second); differ += t; }
        }
        for (const auto& kv : bot) if (!host.count(kv.first)) { char t[24]; snprintf(t, sizeof(t), " %08X=%08X", kv.first, kv.second); extra += t; }
        printf("scriptvars (%s): bot slot %u side %u, %zu vars; MISSING (host has):%s | ONLY THE BOT:%s | DIFFER (host/bot):%s\n",
               why, bots[s], s, bot.size(), missing.empty() ? " none" : missing.c_str(), extra.empty() ? " none" : extra.c_str(), differ.empty() ? " none" : differ.c_str());
    }
    fflush(stdout);
}
// 0.5.493: who moves a flag's model (the game's CTF take hides it 10000 down).
// 488-492 could not find what the take checks by probing fields; so the pages
// holding the flags' script_models are made read-only (unattended test only)
// and a write to a model's origin (+336..+347) stops the CPU: the native
// stack (the recompiled sub_XXXXXXXX functions, by the PDB) is logged
// ("flagtrap:"), then that one write is let through (single step) and the
// page locked again. The host's take at 200 s then names the take's code.
// 0.5.513: the trigger-touch queue's consumer (sub_8256FFC8, each server
// frame): a touch is queued by sub_82567328 as (trigger handle, toucher
// handle, the trigger's +204, the toucher's +204) at 0x82C281D0 + 3196, 16
// bytes each, the count at +7292; here each one's handles are resolved in
// the entity table (0x82BD1FC0: index = handle & 0x3FFF, the generation
// handle >> 14 at +4 of the entry) and both +204s compared with the queued
// ones; only then the script is told (sub_820B0898: "trigger"). 488-512: a
// bot's touch is recorded on the trigger but the script never acts. So each
// queued touch BY A BOT is checked the same way first and its outcome logged.
extern "C" PPC_FUNC(__imp__sub_8256FFC8);
PPC_FUNC(sub_8256FFC8)
{
    constexpr uint32_t Queue = 0x82C281D0u, Table = 0x82BD1FC0u;
    const int32_t count = int32_t(Guest::Read32(base, Queue + 7292));
    const uint32_t list = Guest::Read32(base, EntityList);
    static int told = 0;
    static uint32_t passed = 0, failed[4] = {};
    for (int32_t i = 0; list && i < count && i < 256; i++)
    {
        const uint32_t at = Queue + 3196 + 16 * uint32_t(i);
        const uint32_t trigH = Guest::Read32(base, at), actH = Guest::Read32(base, at + 4);
        const uint32_t trig204 = Guest::Read32(base, at + 8), act204 = Guest::Read32(base, at + 12);
        auto resolve = [&](uint32_t h) -> uint32_t {
            const uint32_t idx = h & 0x3FFFu;
            if (idx >= 4096) return 0;
            return Guest::Read32(base, Table + 8 * idx + 4) == ((h >> 14) & 0x3FFFFu) ? Guest::Read32(base, Table + 8 * idx) : 0;
        };
        // is the toucher a bot? (by his handle's entity, else his entity's +484)
        const uint32_t actRaw = Guest::Read32(base, Table + 8 * (actH & 0x3FFFu));
        int slot = -1;
        for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients && slot < 0; o++)
        {
            if (!g_bot[o]) continue;
            const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
            if (e && (e == actRaw || Guest::Read32(base, e + 484) == actH)) slot = int(o);
        }
        if (slot < 0) continue;
        // 0.5.514: the flags' triggers only (513's 60 lines were all other
        // triggers, spawn and zone volumes): the trigger's entity among
        // FlagEntityWatch's flag triggers
        {
            const uint32_t trigRaw = Guest::Read32(base, Table + 8 * (trigH & 0x3FFFu));
            bool flagTrigger = false;
            for (uint32_t f : g_flagTriggers) if (f == trigRaw) flagTrigger = true;
            if (!flagTrigger) continue;
        }
        const uint32_t trig = resolve(trigH), act = resolve(actH);
        int why = -1;
        if (!trig) why = 0;
        else if (Guest::Read32(base, trig + 204) != trig204) why = 1;
        else if (!act) why = 2;
        else if (Guest::Read32(base, act + 204) != act204) why = 3;
        static const char* const reasons[4] = { "the trigger's handle does not resolve", "the trigger's +204 changed", "the BOT's handle does not resolve", "the BOT's +204 changed" };
        if (why < 0) passed++; else failed[why]++;
        if (told < 60)
        {
            told++;
            const uint32_t e = Guest::Read32(base, list + 4 * (uint32_t(slot) + 1));
            printf("touchqueue: bot slot %d touched FLAG trigger %08X: %s (his handle %08X: table entity %08X gen %u, his entity %08X +484 %08X; +204 queued %08X now %08X)\n",
                   slot, trigH, why < 0 ? "PASSES (the script is told)" : reasons[why], actH, actRaw, Guest::Read32(base, Table + 8 * (actH & 0x3FFFu) + 4),
                   e, e ? Guest::Read32(base, e + 484) : 0, act204, act ? Guest::Read32(base, act + 204) : 0);
            fflush(stdout);
        }
    }
    static uint32_t lastSum = 0;
    if (const uint32_t sum = passed + failed[0] + failed[1] + failed[2] + failed[3]; sum && sum / 50 != lastSum / 50)
    {
        lastSum = sum;
        printf("touchqueue: bots' FLAG touches so far: %u passed, %u trigger unresolved, %u trigger changed, %u bot unresolved, %u bot +204 changed\n", passed, failed[0], failed[1], failed[2], failed[3]);
        fflush(stdout);
    }
    __imp__sub_8256FFC8(ctx, base);
}
// 0.5.516: the game's scripts are compiled native code: sub_82204A08 is the
// CTF flag handler itself (its end gives the carrier the flag weapon, gi
// +2664/+2872/+2692, then hides the base flag, sub_821E5718); its caller
// sub_82205788. Both wrapped: each call logged with its arguments and the
// player (r4 points at his handle in sub_82204A08), so a bot's touch shows
// where it stops: never reaching the caller, the caller not calling the
// handler, or the handler returning early.
namespace
{
    int SlotOfHandle(uint8_t* base, uint32_t handle)
    {
        const uint32_t list = Guest::Read32(base, EntityList);
        const uint32_t e = (handle & 0x3FFFu) < 4096 ? Guest::Read32(base, 0x82BD1FC0u + 8 * (handle & 0x3FFFu)) : 0;
        for (uint32_t o = 0; list && e && o < ActiveSlots() && o < MaxClients; o++) if (Guest::Read32(base, list + 4 * (o + 1)) == e) return int(o);
        return -1;
    }
    std::atomic<int> g_flagCallsTold{ 0 };
}
extern "C" PPC_FUNC(__imp__sub_82204A08);
PPC_FUNC(sub_82204A08)
{
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32;
    const uint32_t handle = (r4 >= 0x40000000u && r4 < 0xA0000000u) ? Guest::Read32(base, r4) : 0;
    const int slot = handle ? SlotOfHandle(base, handle) : -1;
    const bool bot = slot > 0 && g_bot[slot];
    const bool tell = g_flagCallsTold.fetch_add(1) < 3000;   // 0.5.522: 3000 (the bots' capture asks used up 200)
    if (tell) { printf("flagcode: sub_82204A08 (the flag handler) called: r3 %08X, player handle %08X (slot %d, %s), r5 %08X, r6 %08X\n", r3, handle, slot, bot ? "BOT" : slot == 0 ? "host" : "?", r5, r6); fflush(stdout); }
    __imp__sub_82204A08(ctx, base);
    if (tell) { printf("flagcode: sub_82204A08 returned %08X for slot %d\n", ctx.r3.u32, slot); fflush(stdout); }
}
// 0.5.525: the other two message callbacks too (+3424 sub_822049D8: 523
// showed it DROPS the carried flag; +3428 sub_821F33A0), to see what a real
// client's capture sends (the test puts the host, carrying, by his flag)
extern "C" PPC_FUNC(__imp__sub_822049D8);
PPC_FUNC(sub_822049D8)
{
    const uint32_t h = (ctx.r3.u32 >= 0x40000000u && ctx.r3.u32 < 0xA0000000u) ? Guest::Read32(base, ctx.r3.u32) : 0;
    if (g_flagCallsTold.fetch_add(1) < 3000) { printf("flagcode: +3424 sub_822049D8 (drop) called: r3 %08X (-> %08X slot %d), r4 %08X, r5 %08X\n", ctx.r3.u32, h, h ? SlotOfHandle(base, h) : -1, ctx.r4.u32, ctx.r5.u32); fflush(stdout); }
    // 0.5.529: who drops it (528: a bot carrier by his own flag was dropped by
    // the game, where the host's carry was captured): the native callers
    {
        void* frames[24] = {};
        const USHORT n = RtlCaptureStackBackTrace(0, 24, frames, nullptr);
        char buffer[sizeof(SYMBOL_INFO) + 128] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        std::string chain;
        for (USHORT i = 0; i < n && i < 14; i++)
        {
            memset(buffer, 0, sizeof(buffer)); symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 127;
            DWORD64 d = 0; char t[160];
            if (SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(frames[i]), &d, symbol)) snprintf(t, sizeof(t), " %s+0x%llx", symbol->Name, (unsigned long long)d);
            else snprintf(t, sizeof(t), " %p", frames[i]);
            chain += t;
        }
        printf("flagcode: the drop's callers:%s\n", chain.c_str()); fflush(stdout);
    }
    __imp__sub_822049D8(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_821F33A0);
PPC_FUNC(sub_821F33A0)
{
    const uint32_t a = (ctx.r3.u32 >= 0x40000000u && ctx.r3.u32 < 0xA0000000u) ? Guest::Read32(base, ctx.r3.u32) : 0;
    const uint32_t b = (ctx.r4.u32 >= 0x40000000u && ctx.r4.u32 < 0xA0000000u) ? Guest::Read32(base, ctx.r4.u32) : 0;
    if (g_flagCallsTold.fetch_add(1) < 3000) { printf("flagcode: +3428 sub_821F33A0 called: r3 %08X (-> %08X), r4 %08X (-> %08X slot %d), r5 %08X, r6 %08X\n", ctx.r3.u32, a, ctx.r4.u32, b, b ? SlotOfHandle(base, b) : -1, ctx.r5.u32, ctx.r6.u32); fflush(stdout); }
    __imp__sub_821F33A0(ctx, base);
}
// 0.5.532: THE CAPTURE IS ASKED FOR TOO. 531's trap: the host's capture moved
// the flag home from sub_82203E00, called (through Callbacks +3432) by
// sub_823E3298, a client message handler like the take's. Wrapped to log the
// real arguments of a capture (r3, r4: its first branch is on r4).
extern "C" PPC_FUNC(__imp__sub_82203E00);
PPC_FUNC(sub_82203E00)
{
    if (g_flagCallsTold.fetch_add(1) < 3000) { printf("flagcode: sub_82203E00 (the CAPTURE) called: r3 %08X, r4 %08X, r5 %08X\n", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32); fflush(stdout); }
    __imp__sub_82203E00(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82205788);
PPC_FUNC(sub_82205788)
{
    const uint32_t r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32, r6 = ctx.r6.u32;
    std::string args;
    for (uint32_t v : { r3, r4, r5, r6 })
    {
        char t[64];
        const uint32_t pointed = (v >= 0x40000000u && v < 0xA0000000u) ? Guest::Read32(base, v) : 0;
        const int slot = (pointed & 0xFFFFC000u) == 0x4000u ? SlotOfHandle(base, pointed) : ((v & 0xFFFFC000u) == 0x4000u ? SlotOfHandle(base, v) : -1);
        snprintf(t, sizeof(t), " %08X(->%08X%s%d)", v, pointed, slot >= 0 ? " slot " : "", slot);
        args += t;
    }
    const bool tell = g_flagCallsTold.fetch_add(1) < 3000;   // 0.5.522: 3000 (the bots' capture asks used up 200)
    if (tell) { printf("flagcode: sub_82205788 (its caller) called:%s\n", args.c_str()); fflush(stdout); }
    __imp__sub_82205788(ctx, base);
}
namespace FlagTrap
{
    struct Range { uintptr_t from, to; };
    std::vector<Range> g_origins;      // host addresses of the models' origins
    std::vector<Range> g_touches;      // 0.5.512: the flag triggers' +760..+808
    std::vector<Range> g_touch496;     // 0.5.531: and their +496
    std::vector<uintptr_t> g_pages;    // the pages locked
    std::atomic<int> g_told{ 0 };
    thread_local uintptr_t t_relock = 0;
    thread_local DWORD t_relockProt = PAGE_READONLY;
    bool g_symbols = false;
    // 0.5.494: reads of chosen words (the pad state found by PadScan), while on
    std::vector<uintptr_t> g_readWords, g_readPages;
    std::atomic<bool> g_readOn{ false };
    // 0.5.496: named ranges of one player's memory, and the interpreter's code
    struct WatchRange { uintptr_t from, to; const char* who; const char* region; };
    std::vector<WatchRange> g_ranges;
    uintptr_t g_vmFrom = 0, g_vmTo = 0;
    std::atomic<int> g_readTold{ 0 };
    std::string StackText(CONTEXT* context);
    void PrintStack(CONTEXT* context)
    {
        printf("flagtrap: a flag model's origin written by:%s\n", StackText(context).c_str()); fflush(stdout);
    }
    std::string StackText(CONTEXT* context)
    {
        void* frames[40] = {};
        const USHORT n = RtlCaptureStackBackTrace(0, 40, frames, nullptr);
        const HANDLE process = GetCurrentProcess();
        char buffer[sizeof(SYMBOL_INFO) + 256] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        std::string out;
        // the faulting instruction first, then the callers (after the dispatcher)
        auto name = [&](DWORD64 pc) {
            memset(buffer, 0, sizeof(buffer));
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 255;
            DWORD64 displacement = 0;
            char t[300];
            if (SymFromAddr(process, pc, &displacement, symbol)) snprintf(t, sizeof(t), " %s+0x%llx", symbol->Name, (unsigned long long)displacement);
            else snprintf(t, sizeof(t), " %llx", (unsigned long long)pc);
            out += t;
        };
        name(context->Rip);
        bool past = false;
        for (USHORT i = 0; i < n; i++)
        {
            const DWORD64 pc = reinterpret_cast<DWORD64>(frames[i]);
            if (!past) { memset(buffer, 0, sizeof(buffer)); symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 255; DWORD64 d = 0;
                         if (SymFromAddr(process, pc, &d, symbol) && strstr(symbol->Name, "KiUserExceptionDispatch")) past = true; continue; }
            name(pc);
        }
        return out;
    }
    LONG CALLBACK Handler(EXCEPTION_POINTERS* info)
    {
        const DWORD code = info->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_SINGLE_STEP && t_relock)
        {
            DWORD old; VirtualProtect(reinterpret_cast<void*>(t_relock), 4096, (g_readOn || t_relockProt == PAGE_READONLY) ? t_relockProt : PAGE_READWRITE, &old);
            t_relock = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (code != EXCEPTION_ACCESS_VIOLATION || info->ExceptionRecord->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
        const uintptr_t at = info->ExceptionRecord->ExceptionInformation[1];
        const uintptr_t page = at & ~uintptr_t(4095);
        // 0.5.494: a read-watched page: the reads of the watched words from
        // the script interpreter's calls are logged
        for (uintptr_t p : g_readPages)
        {
            if (p != page) continue;
            DWORD old;  /* 0.5.496: unlocked first, so the value can be read */ VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &old);
            // 0.5.496: reads of one player's memory (entity, record, session)
            // made while the script interpreter (sub_82204A08) is on the stack:
            // the player fields the CTF script looks at, for a bot pressing at
            // the enemy flag and for the host taking it. One line per (who,
            // region, offset, the function that read it).
            if (g_readOn && g_vmFrom)
                for (const WatchRange& r : g_ranges)
                {
                    if (at < r.from || at >= r.to) continue;
                    void* frames[48] = {};
                    const USHORT n = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
                    bool vm = false;
                    for (USHORT i = 0; i < n && !vm; i++) { const uintptr_t pc = reinterpret_cast<uintptr_t>(frames[i]); vm = pc >= g_vmFrom && pc < g_vmTo; }
                    if (!vm) break;
                    const uint32_t offset = uint32_t(at - r.from) & ~3u;
                    char reader[160] = "?";
                    {
                        char buffer[sizeof(SYMBOL_INFO) + 128] = {};
                        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
                        symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 127;
                        DWORD64 d = 0;
                        if (SymFromAddr(GetCurrentProcess(), info->ContextRecord->Rip, &d, symbol)) snprintf(reader, sizeof(reader), "%s", symbol->Name);
                    }
                    char key[256]; snprintf(key, sizeof(key), "%s %s +%u %s", r.who, r.region, offset, reader);
                    static std::vector<std::string> seen;
                    bool fresh = true; for (const std::string& s : seen) if (s == key) fresh = false;
                    if (fresh && seen.size() < 400)
                    {
                        seen.push_back(key);
                        uint32_t value; memcpy(&value, reinterpret_cast<void*>(at & ~uintptr_t(3)), 4);
                        printf("vmread: %s %s +%u = %08X by %s\n", r.who, r.region, offset, _byteswap_ulong(value), reader); fflush(stdout);
                    }
                    break;
                }
            t_relock = page; t_relockProt = PAGE_NOACCESS;
            info->ContextRecord->EFlags |= 0x100;
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        if (info->ExceptionRecord->ExceptionInformation[0] != 1) return EXCEPTION_CONTINUE_SEARCH;
        bool ours = false;
        for (uintptr_t p : g_pages) if (p == page) ours = true;
        if (!ours) return EXCEPTION_CONTINUE_SEARCH;
        // 0.5.530: each distinct chain once (a take, a drop, a return, a
        // capture), not the first 60 writes
        for (const Range& r : g_origins)
            if (at >= r.from && at < r.to)
            {
                const std::string stack = StackText(info->ContextRecord);
                static std::vector<std::string> seenOrigin;
                bool fresh = true; for (const std::string& s : seenOrigin) if (s == stack) fresh = false;
                if (fresh && seenOrigin.size() < 40) { seenOrigin.push_back(stack); printf("flagtrap: a flag model's origin written by:%s\n", stack.c_str()); fflush(stdout); }
                break;
            }
        // 0.5.512: a flag trigger's touch fields (+760 time, +764, +804 the
        // toucher): who writes them, each distinct chain once (the host's
        // touch and a bot's compared: does a bot's go on into the script?)
        for (const Range& r : g_touches)
            if (at >= r.from && at < r.to)
            {
                const std::string stack = StackText(info->ContextRecord);
                static std::vector<std::string> seenTouch;
                bool fresh = true; for (const std::string& s : seenTouch) if (s == stack) fresh = false;
                if (fresh && seenTouch.size() < 40)
                {
                    seenTouch.push_back(stack);
                    printf("flagtrap: a flag trigger's touch field +%u written by:%s\n", unsigned(at - (r.from - 760)), stack.c_str()); fflush(stdout);
                }
                break;
            }
        for (const Range& r : g_touch496)
            if (at >= r.from && at < r.to)
            {
                const std::string stack = StackText(info->ContextRecord);
                static std::vector<std::string> seen496;
                bool fresh = true; for (const std::string& s : seen496) if (s == stack) fresh = false;
                if (fresh && seen496.size() < 40)
                {
                    seen496.push_back(stack);
                    printf("flagtrap: a flag trigger's +496 written by:%s\n", stack.c_str()); fflush(stdout);
                }
                break;
            }
        DWORD old; VirtualProtect(reinterpret_cast<void*>(page), 4096, PAGE_READWRITE, &old);
        t_relock = page; t_relockProt = PAGE_READONLY;
        info->ContextRecord->EFlags |= 0x100;   // one instruction, then EXCEPTION_SINGLE_STEP locks it again
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    void Arm(uint8_t* base, const std::vector<uint32_t>& models, const std::vector<uint32_t>& triggers = {})
    {
        if (!g_pages.empty() || models.empty()) return;
        for (uint32_t e : triggers)
        {
            const uintptr_t from = reinterpret_cast<uintptr_t>(base + e + 760);
            g_touches.push_back({ from, from + 48 });
            const uintptr_t page = from & ~uintptr_t(4095);
            bool have = false; for (uintptr_t p : g_pages) if (p == page) have = true;
            if (!have) g_pages.push_back(page);
            // 0.5.531: +496 too (set at the host's capture touch, never at a
            // bot carrier's)
            const uintptr_t f496 = reinterpret_cast<uintptr_t>(base + e + 496);
            g_touch496.push_back({ f496, f496 + 4 });
            const uintptr_t page496 = f496 & ~uintptr_t(4095);
            have = false; for (uintptr_t p : g_pages) if (p == page496) have = true;
            if (!have) g_pages.push_back(page496);
        }
        const HANDLE process = GetCurrentProcess();
        SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
        g_symbols = SymInitialize(process, nullptr, TRUE) != FALSE || GetLastError() == ERROR_INVALID_PARAMETER;
        { char b[sizeof(SYMBOL_INFO) + 64] = {}; auto* s = reinterpret_cast<SYMBOL_INFO*>(b); s->SizeOfStruct = sizeof(SYMBOL_INFO); s->MaxNameLen = 63; DWORD64 d;
          SymFromAddr(process, reinterpret_cast<DWORD64>(&Arm), &d, s); }   // the PDB loaded now, not inside the first trap
        AddVectoredExceptionHandler(1, Handler);
        for (uint32_t e : models)
        {
            const uintptr_t from = reinterpret_cast<uintptr_t>(base + e + 336);
            g_origins.push_back({ from, from + 12 });
            const uintptr_t page = from & ~uintptr_t(4095);
            bool have = false; for (uintptr_t p : g_pages) if (p == page) have = true;
            if (!have) g_pages.push_back(page);
        }
        for (uintptr_t p : g_pages) { DWORD old; VirtualProtect(reinterpret_cast<void*>(p), 4096, PAGE_READONLY, &old); }
        printf("flagtrap: armed on %zu flag models (%zu pages)\n", models.size(), g_pages.size()); fflush(stdout);
    }
}
// 0.5.488: the unattended CTF test's probe (COD3_BIG_MODE set only), for what
// the game checks on a flag take that the bots fail: 487's bots pressed use
// at the real enemy flag 812 times and fired its trigger 78 times, never
// given it; the user takes it at once. 25 s after the sides are known: every
// word where ALL living bots (5 or more) agree and the host differs, in the
// entity (1 KB), the player record (+512, 8 KB) and the session player (1
// KB). 30 s: the host (god) put 30 from the enemy flag and X tapped every
// 300 ms for 10 s, as the control: does a player take it in this test?
static void CtfTakeProbe(PPCContext& ctx, uint8_t* base)
{
    // 0.5.538: only with COD3_CTF_PROBE=1 (the user plays in the unattended
    // test's matches: its teleports of the host and pad taps broke his game)
    const char* mode = getenv("COD3_CTF_PROBE");
    if (!mode || !mode[0] || g_ctfOwn[1] < 0 || g_obj.count != 2) return;
    static uint32_t knownAt = 0, stage = 0, lastTap = 0;
    const uint32_t now = GetTickCount();
    if (!knownAt) knownAt = now;
    const uint32_t list = Guest::Read32(base, EntityList);
    if (!list) return;
    const uint32_t hostE = Guest::Read32(base, list + 4);
    const uint32_t hostR = hostE ? Guest::Read32(base, hostE + 512) : 0;
    if (!hostR) return;
    if (stage == 0 && now - knownAt > 25000)
    {
        stage = 1;
        // 0.5.491's search for script method names found none (the scripts are
        // compiled in): removed
        FlagTrap::Arm(base, g_flagModels, g_flagTriggers);   // 0.5.493; 0.5.512 the triggers' touch fields too
        // 0.5.515: 514: bot 22 (side 2) at the enemy flag touched trigger 111
        // 23 times and its twin 112 (same place) never; the host's take had
        // both of its flag's triggers record him. The engine's touch loop
        // (sub_8256F6B0) matches the toucher's +324 (contents) against a mask:
        // +324 of the host, of each bot by side, and of the four flag
        // triggers (with +320 and +328 beside it)
        {
            std::string players;
            for (uint32_t o = 0; o < ActiveSlots() && o < MaxClients; o++)
            {
                const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
                if (!e || (o != 0 && !g_bot[o])) continue;
                char t[64]; snprintf(t, sizeof(t), " %s%u(side %u):%08X", o == 0 ? "HOST" : "", o, Side(base, e), Guest::Read32(base, e + 324)); players += t;
            }
            std::string trigs;
            for (uint32_t f : g_flagTriggers)
            {
                char t[96]; snprintf(t, sizeof(t), " %08X: +320 %08X +324 %08X +328 %08X @%.0f %.0f", f, Guest::Read32(base, f + 320), Guest::Read32(base, f + 324), Guest::Read32(base, f + 328), ReadFloat(base, f + 336), ReadFloat(base, f + 340)); trigs += t;
            }
            printf("ctfprobe: contents +324:%s\nctfprobe: flag triggers:%s\n", players.c_str(), trigs.c_str()); fflush(stdout);
        }
        // 0.5.508: every bot's side again, the way the game's own menu does it
        // in a session game (sub_8239FB10's other branch): the session's side
        // change sub_823EA4E0(mode object, entity, side 1/2, 0, 0), now that
        // the bots have session players (at join they had none: "session
        // 00000000"). In the local mode, the callback (handle, 1, 1) instead.
        {
            const uint32_t modeObject = Guest::Read32(base, 0x82BCCAC0);
            const bool session = modeObject && Guest::Read32(base, modeObject) != 0;
            int done = 0;
            for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
            {
                if (!g_bot[o]) continue;
                const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
                const uint32_t s = e ? Side(base, e) : 0;
                if (s < 1 || s > 2) continue;
                if (session) Call(ctx, base, 0x823EA4E0, modeObject, e, s, 0, 0);
                else
                {
                    const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
                    Guest::Write32(base, scratch, Guest::Read32(base, e + 484));
                    const uint32_t object = Guest::Read32(base, 0x82BCBF64);
                    const uint32_t callback = object ? Guest::Read32(base, object + 3320) : 0;
                    if (callback) Call(ctx, base, callback, scratch, 1, 1);
                }
                done++;
            }
            printf("ctfprobe: %d bots' sides set again through the %s (mode object %08X, session player of bot 1: %08X)\n", done,
                   session ? "session's side change (sub_823EA4E0)" : "script's side callback (local mode)", modeObject, SessionPlayer(base, 1)); fflush(stdout);
        }
        // 0.5.504: 503's read watch saw the script read almost nothing of a
        // player's own memory: its fields (team, pers, ...) live in the script's
        // variables. Those of the host and of a bot of each side, by name: a
        // player's script handle is his index in the entity table (0x82BD1FC0,
        // 8 bytes each) with 0x4000 (the kit hook's handles: 0x4005 ...); his
        // variables sub_821DE680(handle), a field sub_821DE958(vars, hash), the
        // hash of a name sub_820E10E0 (as weapons are found by name).
        {
            auto call = [&](uint32_t fn, uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0) {
                PPCContext c = ctx;
                c.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
                c.r3.u64 = r3; c.r4.u64 = r4; c.r5.u64 = r5; c.lr = 0;
                Guest::Lookup(fn)(c, base);
                return c.r3.u32;
            };
            static uint32_t text = 0;
            if (!text) text = Guest::AllocatePhysical(0x40);
            auto hashOf = [&](const char* name) { memset(base + text, 0, 0x40); memcpy(base + text, name, strlen(name) + 1); return call(0x820E10E0, text); };
            auto handleOf = [&](uint32_t slot) -> uint32_t {
                const uint32_t e = Guest::Read32(base, list + 4 * (slot + 1));
                for (uint32_t i = 0; e && i < 4096; i++) if (Guest::Read32(base, 0x82BD1FC0 + 8 * i) == e) return 0x4000u | i;
                return 0;
            };
            uint32_t who[3] = { 0, 0, 0 };
            for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
            {
                if (!g_bot[o]) continue;
                const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
                const uint32_t s = e ? Side(base, e) : 0;
                if (s >= 1 && s <= 2 && !who[s]) who[s] = o;
            }
            const char* names[] = { "team", "pers", "sessionteam", "sessionstate", "pteam", "class", "curclass", "isflagcarrier", "carryflag", "carryobject",
                                    "hasflag", "flag", "carrying", "canpickup", "usingobject", "spawned", "spawntime", "lastspawntime", "health", "name",
                                    "statusicon", "isbot", "bot", "ingame", "connected", "alive", "isalive", "touchtriggers", "pickupflag", "isplayer" };
            const uint32_t slots[3] = { 0, who[1], who[2] };
            uint32_t vars[3] = {};
            for (int p = 0; p < 3; p++) { const uint32_t h = (p == 0 || slots[p]) ? handleOf(slots[p]) : 0; vars[p] = h ? call(0x821DE680, h) : 0;
                printf("scriptfields: %s slot %u: handle %08X vars %08X%s\n", p == 0 ? "host" : "bot", slots[p], h, vars[p], p == 0 ? "" : ""); }
            auto field = [&](int p, uint32_t hash) -> std::string {
                if (!vars[p]) return "-";
                const uint32_t at = call(0x821DE958, vars[p], hash);
                if (!at) return "missing";
                char t[40]; snprintf(t, sizeof(t), "%08X %08X", Guest::Read32(base, at), Guest::Read32(base, at + 4)); return t;
            };
            {
                char line[256]; snprintf(line, sizeof(line), "scriptfields: known 14D6461A (ability): host %s | bot1 %s | bot2 %s", field(0, 0x14D6461A).c_str(), field(1, 0x14D6461A).c_str(), field(2, 0x14D6461A).c_str());
                printf("%s\n", line);
            }
            for (const char* n : names)
            {
                const uint32_t h = hashOf(n);
                const std::string a = field(0, h), b = field(1, h), c = field(2, h);
                if (a == "missing" && b == "missing" && c == "missing") continue;
                printf("scriptfields: %-14s %08X: host %s | bot side1 %s | bot side2 %s\n", n, h, a.c_str(), b.c_str(), c.c_str());
            }
            // 0.5.505: 504 found "spawntime" for all three (the hash matches the
            // field names); the other 29 names are not CoD3's. Every field then,
            // without names: the variables' words dumped (+0..+0x200) for the
            // layout, and every (hash, value) pair a lookup finds: a hash is
            // taken as real when sub_821DE958 finds it in that player's table.
            for (int p = 0; p < 3; p++)
            {
                if (!vars[p]) continue;
                std::string raw;
                for (uint32_t w = 0; w < 0x200; w += 4) { char t[12]; snprintf(t, sizeof(t), " %08X", Guest::Read32(base, vars[p] + w)); raw += t; }
                printf("scriptfields: %s raw%s\n", p == 0 ? "host" : p == 1 ? "bot1" : "bot2", raw.c_str());
            }
            // 0.5.509: the table's layout, from the lookup itself (sub_821DE958):
            // +0 the count, +8 a POINTER to the entries, 8 bytes each (hash,
            // value); it returns the entry + 4. (505 looked for the pairs at
            // +8 itself and found none.)
            std::map<uint32_t, uint32_t> pairs[3];
            for (int p = 0; p < 3; p++)
            {
                if (!vars[p]) continue;
                const uint32_t count = std::min<uint32_t>(Guest::Read32(base, vars[p]), 400);
                const uint32_t entries = Guest::Read32(base, vars[p] + 8);
                if (entries < 0x40000000u || entries >= 0xA0000000u) continue;
                for (uint32_t i = 0; i < count; i++) pairs[p][Guest::Read32(base, entries + 8 * i)] = Guest::Read32(base, entries + 8 * i + 4);
            }
            for (int p = 0; p < 2; p++)
            {
                std::string all;
                for (const auto& kv : pairs[p]) { char t[24]; snprintf(t, sizeof(t), " %08X=%08X", kv.first, kv.second); all += t; }
                printf("scriptfields: %s all %zu:%s\n", p == 0 ? "host" : "bot1", pairs[p].size(), all.c_str());
            }
            std::string onlyHost, differ, onlyBots;
            for (const auto& kv : pairs[0])
            {
                const bool in1 = pairs[1].count(kv.first) != 0, in2 = pairs[2].count(kv.first) != 0;
                char t[64];
                if (!in1 && !in2) { snprintf(t, sizeof(t), " %08X=%08X", kv.first, kv.second); onlyHost += t; }
                else if ((in1 && pairs[1][kv.first] != kv.second) && (in2 && pairs[2][kv.first] != kv.second))
                { snprintf(t, sizeof(t), " %08X(%08X/%08X/%08X)", kv.first, kv.second, pairs[1][kv.first], pairs[2][kv.first]); differ += t; }
            }
            for (const auto& kv : pairs[1]) if (!pairs[0].count(kv.first)) { char t[32]; snprintf(t, sizeof(t), " %08X=%08X", kv.first, kv.second); onlyBots += t; }
            printf("scriptfields: %zu/%zu/%zu fields found; ONLY THE HOST:%s\nscriptfields: DIFFER (host/bot1/bot2):%s\nscriptfields: ONLY BOT1:%s\n",
                   pairs[0].size(), pairs[1].size(), pairs[2].size(), onlyHost.empty() ? " none" : onlyHost.c_str(), differ.empty() ? " none" : differ.c_str(), onlyBots.empty() ? " none" : onlyBots.c_str());
            fflush(stdout);
        }
        struct Area { const char* name; uint32_t size; };
        const Area areas[3] = { { "entity", 1024 }, { "record", 8192 }, { "session", 1024 } };
        std::vector<uint32_t> bots;
        for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
        {
            if (!g_bot[o]) continue;
            const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            if (r && int32_t(Guest::Read32(base, r + 304)) > 0) bots.push_back(o);
        }
        if (bots.size() < 5) { stage = 0; return; }
        for (int a = 0; a < 3; a++)
        {
            auto at = [&](uint32_t slot) -> uint32_t {
                const uint32_t e = Guest::Read32(base, list + 4 * (slot + 1));
                return a == 0 ? e : a == 1 ? (e ? Guest::Read32(base, e + 512) : 0) : SessionPlayer(base, slot);
            };
            const uint32_t h = at(0);
            if (!h) continue;
            std::string out; int found = 0;
            for (uint32_t off = 0; off < areas[a].size && found < 160; off += 4)
            {
                uint32_t common = 0; bool agree = true;
                for (size_t i = 0; i < bots.size() && agree; i++)
                {
                    const uint32_t p = at(bots[i]);
                    if (!p) { agree = false; break; }
                    const uint32_t v = Guest::Read32(base, p + off);
                    if (i == 0) common = v; else if (v != common) agree = false;
                }
                const uint32_t hv = Guest::Read32(base, h + off);
                if (!agree || hv == common) continue;
                char t[40]; snprintf(t, sizeof(t), " +%u %08X/%08X", off, hv, common); out += t; found++;
            }
            printf("ctfprobe: %s, host/all %zu bots:%s\n", areas[a].name, bots.size(), out.empty() ? " none" : out.c_str()); fflush(stdout);
        }
    }
    // 0.5.489: 488's diff (host vs all 22 bots) left four candidates for what
    // the take checks (the host, god, took the flag at once tapping X from 30
    // out): session +624 (host 1, bots 0), session +636 (host -1, bots 0),
    // record +776 (host 1, bots 0), record +1488 (host 1.0f, bots 0). Each
    // second the bots are given ONE of them by slot % 5 (group 0 nothing, the
    // control); the TAKEN line's slot tells the group (logged as "group").
    // 0.5.492: that experiment took nothing; 491 found no script method names
    // (CoD3's scripts are compiled in). The test of the theory instead (the
    // take asks the LOCAL pad): from 60 s to 190 s, while any bot is pressing
    // use at the enemy flag, the local pad's X is tapped too (the host far
    // off in his spawn); a bot's take then says the check reads the pad.
    if (stage >= 1 && now - knownAt > 60000 && now - knownAt < 190000)
    {
        bool pressing = false;
        for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients && !pressing; o++) if (g_bot[o] && g_flagUseMode[o]) pressing = true;
        g_ctfPadAssist = pressing;
        if (pressing && now - lastTap > 300) { lastTap = now; Input::PressButton("x"); }
    }
    else g_ctfPadAssist = false;
    // 0.5.510: 509's script tables: the host has 15 fields, a bot 8, and the
    // bots' 1C4A56B2 is FEFEFF00 (never set; the host's is 1, his side) and
    // their 14D6461A FEFEFEFE (the host 0): the script's player set-up never
    // ran fully for them. Each second, every bot's 1C4A56B2 set (side 1: 1;
    // side 2: 2 for even slots, 1 for odd, to learn whether it is the team
    // or a yes) and 14D6461A to 0. A bot's take then logs his slot.
    // 0.5.522: 521's bot capture asks (his side, 1) ran the flag handler,
    // which did not score. The host's own capture, watched: carrying a flag
    // 5 s, he is put 20 from his own flag; the flagcode wrappers log what his
    // client's capture calls; Callbacks +3428 (the message handler's other
    // callback) printed too
    {
        static uint32_t hostSince = 0; static bool hostMoved = false, said3428 = false;
        if (!said3428) { said3428 = true; const uint32_t object = Guest::Read32(base, 0x82BCBF64); printf("ctfprobe: Callbacks +3420 %08X, +3424 %08X, +3428 %08X\n", object ? Guest::Read32(base, object + 3420) : 0, object ? Guest::Read32(base, object + 3424) : 0, object ? Guest::Read32(base, object + 3428) : 0); fflush(stdout); }
        const bool hostCarries = g_flagCarrier[0] == 0 || g_flagCarrier[1] == 0;
        if (!hostCarries) { hostSince = 0; hostMoved = false; }
        else
        {
            if (!hostSince) hostSince = now;
            const uint32_t s = Side(base, hostE);
            if (!hostMoved && now - hostSince > 5000 && s >= 1 && s <= 2 && g_ctfOwn[s] >= 0 && g_ctfOwn[s] <= 1)   // 0.5.530: on again
            {
                hostMoved = true;
                const float* home = g_obj.at[g_ctfOwn[s]];
                static uint32_t position = 0;
                if (!position) position = Guest::AllocatePhysical(0x20);
                auto writeF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
                writeF(position, home[0] + 20.0f); writeF(position + 4, home[1]); writeF(position + 8, home[2] + 1.0f);
                PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
                place.r3.u64 = hostE; place.r4.u64 = position; place.lr = 0;
                Guest::Lookup(0x8255E118)(place, base);
                printf("ctfprobe: HOST CAPTURE TEST: the host (side %u, carrying) put 20 from his own flag at %.0f %.0f\n", s, home[0], home[1]); fflush(stdout);
            }
        }
    }
    // 0.5.521: the capture tried apart from the pathing: a bot carrying a flag
    // 20 s is put 20 from his own flag (unattended test only), where
    // BotFlagClaim asks to CAPTURE. 520's carriers took flags but wandered.
    {
        static uint32_t carryingSince[MaxClients] = {};
        static bool moved[MaxClients] = {};
        for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
        {
            if (!g_bot[o] || !BotCarries(o)) { carryingSince[o] = 0; moved[o] = false; continue; }
            if (!carryingSince[o]) carryingSince[o] = now;
            if (true || moved[o] || now - carryingSince[o] < 20000) continue;   // 0.5.534: off: 533's bots captured; now they must walk home
            const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
            const uint32_t s = e ? Side(base, e) : 0;
            if (s < 1 || s > 2 || g_ctfOwn[s] < 0 || g_ctfOwn[s] > 1) continue;
            const float* home = g_obj.at[g_ctfOwn[s]];
            static uint32_t position = 0;
            if (!position) position = Guest::AllocatePhysical(0x20);
            auto writeF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
            writeF(position, home[0] + 20.0f); writeF(position + 4, home[1]); writeF(position + 8, home[2] + 1.0f);
            PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
            place.r3.u64 = e; place.r4.u64 = position; place.lr = 0;
            Guest::Lookup(0x8255E118)(place, base);
            moved[o] = true;
            printf("ctfprobe: CAPTURE TEST: carrier slot %u (side %u) put 20 from his own flag at %.0f %.0f\n", o, s, home[0], home[1]); fflush(stdout);
            // 0.5.528: is a script thread waiting on his home triggers (+496,
            // set while one waits: the host's capture had it, 526's bot not)?
            for (uint32_t f : g_flagTriggers)
                if (hypotf(ReadFloat(base, f + 336) - home[0], ReadFloat(base, f + 340) - home[1]) < 100.0f)
                { printf("ctfprobe: CAPTURE TEST: his home trigger %08X: +496 %08X (0: no script thread waiting)\n", f, Guest::Read32(base, f + 496)); fflush(stdout); }
        }
    }
    if (stage >= 1)
    {
        static uint32_t fixAt = 0;
        if (now - fixAt >= 1000)
        {
            fixAt = now;
            int fixed = 0;
            for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
            {
                if (!g_bot[o]) continue;
                const uint32_t e = Guest::Read32(base, list + 4 * (o + 1));
                const uint32_t s = e ? Side(base, e) : 0;
                if (s < 1 || s > 2) continue;
                uint32_t h = 0;
                for (uint32_t i = 0; i < 4096 && !h; i++) if (Guest::Read32(base, 0x82BD1FC0 + 8 * i) == e) h = 0x4000u | i;
                const uint32_t table = h ? Call(ctx, base, 0x821DE680, h) : 0;
                if (!table) continue;
                const uint32_t want = s;   // 0.5.511: his side (510's split taught nothing: no take either way)
                // 0.5.511: the 7 fields only the host has, added: a new entries
                // array (guest memory of the port's) with the bot's own entries
                // and the host's missing ones (the host's values; 7654A134, a
                // time, the bot's spawn time), the table pointed at it, count up
                static uint32_t grown[MaxClients] = {};
                const uint32_t hostTable = Call(ctx, base, 0x821DE680, 0x4001u);
                if (grown[o] != table && hostTable)
                {
                    const uint32_t count = std::min<uint32_t>(Guest::Read32(base, table), 64), entries = Guest::Read32(base, table + 8);
                    const uint32_t hCount = std::min<uint32_t>(Guest::Read32(base, hostTable), 64), hEntries = Guest::Read32(base, hostTable + 8);
                    if (entries >= 0x40000000u && entries < 0xA0000000u && hEntries >= 0x40000000u && hEntries < 0xA0000000u)
                    {
                        const uint32_t fresh = Guest::AllocatePhysical(8 * 96);
                        uint32_t n = 0;
                        for (uint32_t i = 0; i < count; i++) { Guest::Write32(base, fresh + 8 * n, Guest::Read32(base, entries + 8 * i)); Guest::Write32(base, fresh + 8 * n + 4, Guest::Read32(base, entries + 8 * i + 4)); n++; }
                        uint32_t spawn = 0;
                        if (const uint32_t at = Call(ctx, base, 0x821DE958, table, 0xF922BE58u)) spawn = Guest::Read32(base, at);
                        for (uint32_t j = 0; j < hCount; j++)
                        {
                            const uint32_t hash = Guest::Read32(base, hEntries + 8 * j);
                            bool have = false;
                            for (uint32_t i = 0; i < n; i++) if (Guest::Read32(base, fresh + 8 * i) == hash) have = true;
                            if (have) continue;
                            Guest::Write32(base, fresh + 8 * n, hash);
                            Guest::Write32(base, fresh + 8 * n + 4, hash == 0x7654A134u ? spawn : Guest::Read32(base, hEntries + 8 * j + 4));
                            n++;
                        }
                        Guest::Write32(base, table + 8, fresh);
                        Guest::Write32(base, table, n);
                        grown[o] = table;
                        static int toldGrow = 0;
                        if (toldGrow++ < 4) { printf("ctfprobe: bot slot %u's script table grown %u -> %u fields (the host's missing ones added)\n", o, count, n); fflush(stdout); }
                    }
                }
                if (const uint32_t at = Call(ctx, base, 0x821DE958, table, 0x1C4A56B2u)) { Guest::Write32(base, at, want); fixed++; }
                if (const uint32_t at = Call(ctx, base, 0x821DE958, table, 0x14D6461Au)) Guest::Write32(base, at, 0);
            }
            static bool told = false;
            if (!told && fixed) { told = true; printf("ctfprobe: %d bots' script field 1C4A56B2 set (side 1: 1; side 2: 2 even slots, 1 odd) and 14D6461A to 0, every second\n", fixed); fflush(stdout); }
        }
    }
    // 0.5.496: (494/495's pad scans caught mostly the reload X starts.) Now
    // the player's own memory is read-watched while the script interpreter
    // runs: up to three bots, 3 s each, while pressing use at the enemy flag
    // (60-190 s), then the host during his take (205.5-212.5 s). "vmread:"
    // lines: who, region, offset, value, the reading function.
    {
        auto unwatch = [&]() {
            FlagTrap::g_readOn = false;
            for (uintptr_t q : FlagTrap::g_readPages) { DWORD old; VirtualProtect(reinterpret_cast<void*>(q), 4096, PAGE_READWRITE, &old); }
            FlagTrap::g_readPages.clear(); FlagTrap::g_ranges.clear();
        };
        auto watch = [&](uint32_t slot, const char* who) {
            unwatch();
            if (!FlagTrap::g_vmFrom)
            {
                char buffer[sizeof(SYMBOL_INFO) + 128] = {};
                auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
                symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 127;
                // 0.5.502: the interpreter's native code from the game's function
                // table (Guest::Lookup); 128 KB from there (its offsets seen go
                // to +0x1d34). 496's lookup by name found nothing.
                (void)symbol;
                if (const uintptr_t fn = reinterpret_cast<uintptr_t>(Guest::Lookup(0x82204A08))) { FlagTrap::g_vmFrom = fn; FlagTrap::g_vmTo = fn + 0x20000; }
                for (const char* name : { "sub_82204A08" })
                    if (!FlagTrap::g_vmFrom && SymFromName(GetCurrentProcess(), name, symbol) && symbol->Size)
                    { FlagTrap::g_vmFrom = uintptr_t(symbol->Address); FlagTrap::g_vmTo = uintptr_t(symbol->Address + symbol->Size); break; }
                printf("ctfprobe: interpreter code %llx-%llx\n", (unsigned long long)FlagTrap::g_vmFrom, (unsigned long long)FlagTrap::g_vmTo); fflush(stdout);
            }
            const uint32_t e = Guest::Read32(base, list + 4 * (slot + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            const uint32_t s = SessionPlayer(base, slot);
            auto add = [&](uint32_t guest, uint32_t size, const char* region) {
                if (!guest) return;
                const uintptr_t from = reinterpret_cast<uintptr_t>(base + guest);
                FlagTrap::g_ranges.push_back({ from, from + size, who, region });
                for (uintptr_t q = from & ~uintptr_t(4095); q < from + size; q += 4096)
                {
                    bool have = false; for (uintptr_t x : FlagTrap::g_readPages) if (x == q) have = true;
                    bool flagPage = false; for (uintptr_t x : FlagTrap::g_pages) if (x == q) flagPage = true;
                    if (!have && !flagPage) FlagTrap::g_readPages.push_back(q);
                }
            };
            add(e, 1024, "entity"); add(r, 8192, "record"); add(s, 1024, "session");
            FlagTrap::g_readOn = true;
            for (uintptr_t q : FlagTrap::g_readPages) { DWORD old; VirtualProtect(reinterpret_cast<void*>(q), 4096, PAGE_NOACCESS, &old); }
            printf("ctfprobe: watching %s (slot %u) script reads: %zu pages\n", who, slot, FlagTrap::g_readPages.size()); fflush(stdout);
        };
        // 0.5.502: the windows on again (the interpreter's range now from the
        // function table): up to three bots, 3 s each, while pressing at the
        // enemy flag (60-190 s); then the host put 18 from the enemy flag and
        // watched 7 s (200-207 s), his take the comparison
        static uint32_t watchedAt = 0, botsWatched = 0;
        if (watchedAt && now - watchedAt > 3000 && FlagTrap::g_readOn && stage < 2) { unwatch(); watchedAt = 0; printf("ctfprobe: bot watch off\n"); fflush(stdout); }
        if (!watchedAt && stage >= 1 && botsWatched < 3 && now - knownAt > 60000 && now - knownAt < 190000)
            for (uint32_t o = 1; o < ActiveSlots() && o < MaxClients; o++)
                if (g_bot[o] && g_flagUseMode[o]) { watch(o, "bot"); watchedAt = now; botsWatched++; break; }
        auto putNear = [&](uint32_t entity, int flag) {
            const uint32_t r = Guest::Read32(base, entity + 512);
            const float px = ReadFloat(base, r), py = ReadFloat(base, r + 4);
            const float dx = g_obj.at[flag][0] - px, dy = g_obj.at[flag][1] - py, dl = fmaxf(1.0f, hypotf(dx, dy));
            static uint32_t position = 0;
            if (!position) position = Guest::AllocatePhysical(0x20);
            auto writeF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
            writeF(position, g_obj.at[flag][0] - dx / dl * 18.0f); writeF(position + 4, g_obj.at[flag][1] - dy / dl * 18.0f); writeF(position + 8, g_obj.at[flag][2] + 1.0f);
            PPCContext place = ctx;
            place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
            place.r3.u64 = entity; place.r4.u64 = position; place.lr = 0;
            Guest::Lookup(0x8255E118)(place, base);
        };
        // 0.5.525: the host on the enemy flag early (35 s), no read watch; the
        // host capture test above then puts him, carrying, by his own flag
        // 0.5.528: the host parked 150 from his OWN flag at 5 s (in his spawn
        // by the enemy flag he took it by standing there, muddling the bots')
        if (false && stage >= 1 && stage < 5 && now - knownAt > 5000)   // 0.5.530: off again: the host captures for the trap
        {
            stage = 5;
            const uint32_t side = Side(base, hostE);
            if (side >= 1 && side <= 2 && g_ctfOwn[side] >= 0 && g_ctfOwn[side] <= 1)
            {
                const float* home = g_obj.at[g_ctfOwn[side]];
                static uint32_t parkPos = 0;
                if (!parkPos) parkPos = Guest::AllocatePhysical(0x20);
                auto writeP = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
                writeP(parkPos, home[0] + 150.0f); writeP(parkPos + 4, home[1]); writeP(parkPos + 8, home[2] + 1.0f);
                PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
                place.r3.u64 = hostE; place.r4.u64 = parkPos; place.lr = 0;
                Guest::Lookup(0x8255E118)(place, base);
                printf("ctfprobe: the host parked by his own flag (out of the bots' way)\n"); fflush(stdout);
            }
        }
        if (stage == 1 && now - knownAt > 35000)   // 0.5.530: on again: the host's capture traced
        {
            stage = 2;
            const uint32_t side = Side(base, hostE);
            if (side >= 1 && side <= 2)
            {
                if (watchedAt) { unwatch(); watchedAt = 0; }
                putNear(hostE, g_ctfOwn[3 - side]);
                printf("ctfprobe: the host put 18 from the enemy flag (to take it, then capture)\n"); fflush(stdout);
            }
        }
        if (stage == 2 && now - knownAt > 42000) { stage = 4; printf("ctfprobe: the host %s\n", (g_flagCarrier[0] == 0 || g_flagCarrier[1] == 0) ? "HAS a flag" : "has no flag"); fflush(stdout); }
    }
}
// 0.5.641: THE PRE-MATCH COUNTDOWN (user: "if we added a pre match timer would
// be perfect ... make the auto balance teams re arrange anybody so teams stay
// balanced in case any players spawn out of place or on the wrong side it can
// correct it before the game starts"). On the machine that runs the server:
// from the level's first server frame (0.5.647; 0.5.641 to 646: from the first
// moment this machine's player was alive) in a level (the
// level's time, 0x82C2826C, gone back = a new level), COD3_PREMATCH seconds
// (10; 0 off, at most 30) in which every player's moves and buttons are
// cleared before Pmove (the look stays), the bots' brains rest as a posed
// bot's do (still, not firing: no stuck tests), and the player's own unstick
// waits. One second in, the sides are counted (connected players, the bots by
// their given side, the rest by their team): while one has two or more over
// the other, a bot of the larger is given the smaller and respawned at its
// side's spawn (players are not moved). A big count in the middle of the
// screen, then GO!. Not on a joining machine yet (no count shown there).
namespace Prematch
{
    std::atomic<bool> g_frozen{ false };
    std::atomic<int> g_msLeft{ -1 };
    std::atomic<uint64_t> g_goUntil{ 0 };
    uint32_t g_lastLevel = 0, g_startLevel = 0;
    bool g_started = false, g_balanced = false, g_done = false;
    int Seconds()
    {
        static const int v = [] {
            const char* t = getenv("COD3_PREMATCH");
            return t && *t ? std::clamp(atoi(t), 0, 30) : 10;
        }();
        return v;
    }
    bool Frozen() { return g_frozen.load(std::memory_order_relaxed); }
    void Balance(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t clients = Clients(base);
        if (!entities || !clients) return;
        auto sideOf = [&](uint32_t slot) -> uint32_t {
            if (Guest::Read32(base, clients + slot * ClientSize) == 0) return 0;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            if (!entity) return 0;
            return g_bot[slot] ? g_botSide[slot] : Side(base, entity);
        };
        uint32_t count[3] = {};
        for (uint32_t slot = 0; slot < ActiveSlots(); slot++) { const uint32_t s = sideOf(slot); if (s == 1 || s == 2) count[s]++; }
        printf("prematch: sides before the start: allies %u, axis %u\n", count[1], count[2]); fflush(stdout);
        for (int moves = 0; moves < 16; moves++)
        {
            const uint32_t big = count[1] >= count[2] ? 1u : 2u, small = 3u - big;
            if (count[big] < count[small] + 2) break;
            int pick = -1;
            for (int slot = int(ActiveSlots()) - 1; slot >= int(FirstBotSlot()); slot--)
                if (g_bot[slot] && sideOf(uint32_t(slot)) == big) { pick = slot; break; }
            if (pick < 0) { printf("prematch: sides %u to %u and no bot on the larger to move (players are not moved)\n", count[1], count[2]); fflush(stdout); break; }
            const uint32_t entity = Guest::Read32(base, entities + 4 * (uint32_t(pick) + 1));
            g_botSide[pick] = small;
            PickSide(ctx, base, entity, small);
            Respawn(ctx, base, entity, small == 1);
            count[big]--; count[small]++;
            printf("prematch: bot slot %d moved from side %u to side %u (now allies %u, axis %u)\n", pick, big, small, count[1], count[2]); fflush(stdout);
        }
    }
    // each server frame (SV_Frame's hook, so only where the server runs)
    void ServerFrame(PPCContext& ctx, uint8_t* base)
    {
        if (Seconds() == 0) return;
        const uint32_t level = Guest::Read32(base, 0x82C2826C);
        if (level < g_lastLevel)
        {
            g_started = g_balanced = g_done = false;
            g_frozen.store(false); g_msLeft.store(-1);
        }
        g_lastLevel = level;
        if (g_done) return;
        // 0.5.647 (user: "the prematch timer needs to start the moment the game
        // timer starts ... even if im still in the menus and havent picked my
        // weapon yet"): from the level's first server frame, when its clock
        // starts (644's log: it waited for your spawn, 15 s in)
        if (!g_started)
        {
            if (level == 0) return;
            g_started = true; g_startLevel = level;
            printf("prematch: the countdown starts with the match (%d s, level time %u)\n", Seconds(), level); fflush(stdout);
        }
        const int into = int(level - g_startLevel), left = Seconds() * 1000 - into;
        if (left > 0)
        {
            g_frozen.store(true); g_msLeft.store(left);
            // the sides balanced 2 s before GO, once the players and bots have
            // joined (at the start of a level they still come in)
            if (!g_balanced && into >= std::max(1000, Seconds() * 1000 - 2000)) { g_balanced = true; Balance(ctx, base); }
            return;
        }
        g_frozen.store(false); g_msLeft.store(-1); g_done = true;
        g_goUntil.store(GetTickCount64() + 1200);
        for (uint32_t slot = 0; slot < MaxClients; slot++) g_tactical[slot].progress.Reset();   // standing still was not being stuck
        printf("prematch: GO (level time %u)\n", level); fflush(stdout);
    }
    bool HudActive() { return g_msLeft.load() > 0 || GetTickCount64() < g_goUntil.load(); }
    void HudDraw(int width, int height)
    {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float scale = height / 1080.0f;
        const int left = g_msLeft.load();
        const ImU32 gold = IM_COL32(240, 200, 90, 255), white = IM_COL32(255, 255, 255, 245), shadow = IM_COL32(0, 0, 0, 190);
        auto centred = [&](const char* text, float size, float y, ImU32 colour) {
            const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            const ImVec2 at(width * 0.5f - extent.x * 0.5f, y);
            draw->AddText(font, size, ImVec2(at.x + 3.0f * scale, at.y + 3.0f * scale), shadow, text);
            draw->AddText(font, size, at, colour, text);
        };
        if (left > 0)
        {
            char number[8];
            snprintf(number, sizeof number, "%d", (left + 999) / 1000);
            centred("MATCH BEGINS IN", 44.0f * scale, height * 0.30f, white);
            centred(number, 150.0f * scale, height * 0.30f + 52.0f * scale, gold);
        }
        else centred("GO!", 150.0f * scale, height * 0.30f + 20.0f * scale, gold);
    }
}
bool BotsPrematchFrozen() { return Prematch::Frozen(); }   // 0.5.641

// 0.5.541: the player caught in the scenery (user: a knife lunge sometimes
// lands him "in an area where im stuck"). Every quarter second his spot is
// sampled; spots where he walked normally (moved, not falling, not on a
// ladder) are kept. Move held 2 s with under 4 units of movement: he is put
// at the newest kept spot at least 48 away and 3 s old. Not on a ladder (0x10
// of the movement flags +44), prone (0x1), or with flag bits never seen while
// he walked (a mounted gun). Settings: unstick. The knife is not touched.
static void PlayerUnstick(PPCContext& ctx, uint8_t* base)
{
    struct Spot { uint32_t at; float x, y, z; };
    static std::vector<Spot> good;
    static uint32_t sampleAt = 0, heldSince = 0, cooldown = 0, walkFlags = 0, lastEntity = 0;
    static float anchor[3] = {}, last[3] = {};
    const uint32_t now = GetTickCount();
    if (now - sampleAt < 250) return;
    sampleAt = now;
    const uint32_t list = Guest::Read32(base, EntityList);
    const uint32_t entity = list ? Guest::Read32(base, list + 4) : 0;
    const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
    if (!Settings::Get().unstick || !record || g_bot[0] || int32_t(Guest::Read32(base, record + 304)) <= 0 ||
        Guest::Read32(base, record + 1924) != 3 || entity != lastEntity || Prematch::Frozen())   // 0.5.641: held still by the countdown
    {
        good.clear(); heldSince = 0; lastEntity = entity;
        return;
    }
    const float p[3] = { ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) };
    const float step = hypotf(p[0] - last[0], p[1] - last[1]), dz = p[2] - last[2];
    if (step > 400.0f || fabsf(dz) > 400.0f) good.clear();   // a spawn or a teleport
    memcpy(last, p, sizeof last);
    const uint32_t flags = Guest::Read32(base, record + 44);
    const uint32_t command = NewestCommand(base, 0);
    const int f = int8_t(Guest::Read8(base, command + 24)), r = int8_t(Guest::Read8(base, command + 25));
    const bool held = abs(f) > 20 || abs(r) > 20;
    const bool odd = (flags & 0x11u) != 0;
    if (!odd && step > 20.0f && step < 150.0f && dz > -30.0f && dz < 30.0f)
    {
        walkFlags |= flags & ~0x100u;
        good.push_back({ now, p[0], p[1], p[2] });
        if (good.size() > 60) good.erase(good.begin());
    }
    if (!held || odd) { heldSince = 0; return; }
    if (!heldSince || fabsf(p[0] - anchor[0]) + fabsf(p[1] - anchor[1]) + fabsf(p[2] - anchor[2]) > 4.0f)
    {
        heldSince = now; memcpy(anchor, p, sizeof anchor);
        return;
    }
    if (now - heldSince < 2000 || now < cooldown) return;
    heldSince = 0;
    const uint32_t unusual = flags & ~(walkFlags | 0x100u | 0x2u);
    if (unusual)
    {
        printf("unstick: held still with movement flags %08X (bits %08X never seen walking): left alone (a mounted gun?)\n", flags, unusual); fflush(stdout);
        cooldown = now + 5000;
        return;
    }
    const Spot* to = nullptr;
    for (auto it = good.rbegin(); it != good.rend() && !to; ++it)
        if (now - it->at >= 3000 && hypotf(it->x - p[0], it->y - p[1]) >= 48.0f) to = &*it;
    if (!to)
    {
        printf("unstick: stuck at %.0f %.0f %.0f (flags %08X) but no earlier good spot\n", p[0], p[1], p[2], flags); fflush(stdout);
        cooldown = now + 5000;
        return;
    }
    static uint32_t position = 0;
    if (!position) position = Guest::AllocatePhysical(0x20);
    auto put = [&](uint32_t at, float v) { uint32_t w; memcpy(&w, &v, 4); Guest::Write32(base, at, w); };
    put(position, to->x); put(position + 4, to->y); put(position + 8, to->z + 1.0f);
    PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
    place.r3.u64 = entity; place.r4.u64 = position; place.lr = 0;
    Guest::Lookup(0x8255E118)(place, base);
    for (int k = 0; k < 3; k++) put(record + 16 + 4 * k, 0.0f);
    printf("unstick: player stuck at %.0f %.0f %.0f (flags %08X, move %d %d) for 2 s: put back at %.0f %.0f %.0f (%u ms ago)\n",
           p[0], p[1], p[2], flags, f, r, to->x, to->y, to->z, now - to->at); fflush(stdout);
    good.clear();
    cooldown = now + 5000;
}

static void CtfSpawnSides(PPCContext& ctx, uint8_t* base)
{
    static std::string ownFor;
    static bool alive[MaxClients] = {};
    const std::string key = g_obj.mode + Kernel::CurrentMap();
    const bool ctf = g_obj.valid && g_obj.count == 2 && GetTickCount() - g_obj.when < 10000 && g_obj.mode.find("ctf") != std::string::npos;
    if (!ctf) return;
    // 0.5.480: each side's own flag from where the game really spawns it (its
    // last 16 spawns, before any move), not g_spawns: 475-479 had side 1
    // "spawning by" 2112 462 from g_spawns while the game spawned it about
    // -500 -800, so the flags were swapped, players moved to the ENEMY flag
    // (user: "spawns at the enemy flag") and bots pressed use on their own.
    // Re-decided on every spawn, so a side switch at half time follows.
    // 0.5.545: votes[side][flag]: evidence that the side owns that flag
    // (CtfSpawnSides' block below says what counts)
    static uint32_t votes[3][2] = {};
    if (ownFor != key) { ownFor = key; g_ctfOwn[1] = g_ctfOwn[2] = -1; memset(votes, 0, sizeof votes); }
    FlagEntityWatch(base, key);   // 0.5.481
    BotTrail(base, 0);            // 0.5.536: the host's trail too: his runs to the enemy flag become routes
    CtfTakeProbe(ctx, base);      // 0.5.488: unattended tests only
    // 0.5.485's ScriptVarsCompare read text, not a variable table (entity +484
    // is not a script handle for players): not called.
    (void)&ScriptVarsCompare;
    // 0.5.487: which flag each side owns, PROVEN by takes, kept in
    // ctf-flags.txt (the game folder) as "<map> <x> <y>": side 1's own flag.
    // The match-start guess was wrong in 486's unattended test (side 1 given
    // 2112 462; every user game proved side 1 owns -1792 -91: the user took
    // 2112 462 as side 1 in 481, -1792 -91 as side 2 in 482), so bots pressed
    // use on their own flag. Rouen seeded with that proof.
    // 0.5.543: OFF. Takes were credited to the host's side as read, which is
    // flipped (541: read 1, his bots' take proved 2; 542: read 2, proved 1),
    // so the file flipped too and teams were put by the enemy flag. The game
    // spawns each side by its OWN flag (541 and 542: every bot take was of the
    // flag far from his side's spawns): the sides come from the bots' spawns.
    // 0.5.545: (the file stays off; the host's own takes vote below)
    if (false)
    {
        static std::string provenFor; static bool loaded = false;
        static std::map<std::string, std::array<float, 2>> proven;
        if (!loaded)
        {
            loaded = true;
            proven["rouen"] = { -1792.0f, -91.0f };
            if (FILE* f = fopen("ctf-flags.txt", "r"))
            {
                char name[64]; float x, y;
                while (fscanf(f, "%63s %f %f", name, &x, &y) == 3) proven[name] = { x, y };
                fclose(f);
            }
        }
        const std::string map = Kernel::CurrentMap();
        // a take by a side proves the other flag is that side's own
        if (g_ctfProvenSide >= 1 && g_ctfProvenSide <= 2 && g_ctfProvenFlag >= 0 && g_ctfProvenFlag <= 1)
        {
            const int side1Own = g_ctfProvenSide == 1 ? 1 - g_ctfProvenFlag : g_ctfProvenFlag;
            const std::array<float, 2> at = { g_obj.at[side1Own][0], g_obj.at[side1Own][1] };
            const auto old = proven.find(map);
            if (old == proven.end() || hypotf(old->second[0] - at[0], old->second[1] - at[1]) > 50.0f)
            {
                proven[map] = at;
                if (FILE* f = fopen("ctf-flags.txt", "w")) { for (const auto& kv : proven) fprintf(f, "%s %.0f %.0f\n", kv.first.c_str(), kv.second[0], kv.second[1]); fclose(f); }
                printf("bots: ctf: proven by a take on %s: side 1 owns the flag at %.0f %.0f (saved)\n", map.c_str(), at[0], at[1]); fflush(stdout);
            }
            g_ctfProvenSide = 0; g_ctfProvenFlag = -1;
        }
        const auto it = proven.find(map);
        if (it != proven.end() && g_obj.count == 2)
        {
            const int own1 = hypotf(g_obj.at[0][0] - it->second[0], g_obj.at[0][1] - it->second[1]) <= hypotf(g_obj.at[1][0] - it->second[0], g_obj.at[1][1] - it->second[1]) ? 0 : 1;
            if (g_ctfOwn[1] != own1)
            {
                const bool first = g_ctfOwn[1] < 0;
                g_ctfOwn[1] = own1; g_ctfOwn[2] = 1 - own1;
                printf("bots: ctf sides (proven for %s): side 1 owns the flag at %.0f %.0f; side 2 the one at %.0f %.0f%s\n", map.c_str(),
                       g_obj.at[own1][0], g_obj.at[own1][1], g_obj.at[1 - own1][0], g_obj.at[1 - own1][1], first ? "" : " (the guess was wrong: put right)"); fflush(stdout);
                if (provenFor != key) { provenFor = key; g_ctfSweepDue = true; }
            }
        }
    }
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return;
    // 0.5.545: which flag each side owns, from what the GAME does. Every match
    // of 541-544 agrees (the user: "the bots who were supposed to be my
    // teammates are still killing me"; 544 had put him on side 2, his team
    // read 1, and side 1 bots 4 and 11, his team, killed him):
    //   the host's team read is right (541 1, 542 2, 543 2 then 1, 544 1);
    //   the game spawns the host by his OWN flag (read 1 by -1792 -91 in 541,
    //   543 and 544; read 2 by 2112 462 in 542 and 543) and he takes the far
    //   one (541: read 1 took 2112 462; 542: read 2 took -1792 -91);
    //   the port's own bot spawns (Respawn) put every bot by the ENEMY flag
    //   (side 1 by 2112 462 35 of 35, side 2 by -1792 -91 36 of 36 in
    //   542-543); bot takes prove nothing (the port makes them, by g_ctfOwn).
    // So: a bot's spawn votes for the OTHER flag (1 each); the host's spawn for
    // the one he is by (3); a take of his for the other one (8, consumed
    // below). Sides: a bot's is the one he was given; the host's his read.
    auto sideOf = [&](uint32_t o, uint32_t e) -> uint32_t {
        if (o < MaxClients && g_bot[o]) return BotSide(base, o);
        return Side(base, e);   // the host or a joiner: his own team read
    };
    auto decide = [&]() {
        int own[3] = { -1, -1, -1 };
        for (int s = 1; s <= 2; s++)
            if (votes[s][0] + votes[s][1] >= 2 && votes[s][0] != votes[s][1]) own[s] = votes[s][0] > votes[s][1] ? 0 : 1;
        if (own[1] < 0 && own[2] >= 0) own[1] = 1 - own[2];
        if (own[2] < 0 && own[1] >= 0) own[2] = 1 - own[1];
        if (own[1] < 0 || own[1] == own[2] || own[1] == g_ctfOwn[1]) return false;
        g_ctfOwn[1] = own[1]; g_ctfOwn[2] = own[2];
        printf("bots: ctf sides (votes for flag 0 / flag 1: side 1 %u / %u, side 2 %u / %u): side 1 owns the flag at %.0f %.0f; side 2 the one at %.0f %.0f\n",
               votes[1][0], votes[1][1], votes[2][0], votes[2][1], g_obj.at[own[1]][0], g_obj.at[own[1]][1], g_obj.at[own[2]][0], g_obj.at[own[2]][1]);
        fflush(stdout);
        return true;
    };
    auto trim = [&](uint32_t s) { if (votes[s][0] + votes[s][1] > 24) { votes[s][0] /= 2; votes[s][1] /= 2; } };   // the newest count most
    // the host's take (FlagEntityWatch: a player's, the flag's own weapon on him)
    if (g_ctfProvenSide >= 1 && g_ctfProvenSide <= 2 && g_ctfProvenFlag >= 0 && g_ctfProvenFlag <= 1)
    {
        votes[g_ctfProvenSide][1 - g_ctfProvenFlag] += 8; trim(g_ctfProvenSide);
        printf("bots: ctf: a player of side %u took flag %d: side %u owns the other\n", g_ctfProvenSide, g_ctfProvenFlag, g_ctfProvenSide); fflush(stdout);
        decide();
    }
    g_ctfProvenSide = 0; g_ctfProvenFlag = -1;
    struct Body { float p[3]; uint32_t side; bool bot; };
    std::vector<Body> living;
    for (uint32_t o = 0; o < ActiveSlots() && o < MaxClients; o++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
        living.push_back({ { ReadFloat(base, r), ReadFloat(base, r + 4), ReadFloat(base, r + 8) }, sideOf(o, e), g_bot[o] });
    }
    // the start: the living bots where they were spawned vote at once, and
    // once the sides are known everyone not by his flag is moved (sweep)
    static bool sweep = false;
    if (g_ctfOwn[1] < 0)
    {
        for (uint32_t o = 0; o < ActiveSlots() && o < MaxClients; o++)
        {
            if (!g_bot[o]) continue;
            const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
            const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
            if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0 || (g_portMovedAt[o] && GetTickCount() - g_portMovedAt[o] < 600000)) continue;
            const float bx = ReadFloat(base, r), by = ReadFloat(base, r + 4);
            const float d0 = hypotf(g_obj.at[0][0] - bx, g_obj.at[0][1] - by), d1 = hypotf(g_obj.at[1][0] - bx, g_obj.at[1][1] - by);
            const uint32_t s = BotSide(base, o);
            if ((s == 1 || s == 2) && fminf(d0, d1) < 0.7f * fmaxf(d0, d1)) votes[s][d0 < d1 ? 1 : 0]++;   // the OTHER flag
        }
        if (decide()) sweep = true;
        else { votes[1][0] = votes[1][1] = votes[2][0] = votes[2][1] = 0; }   // from the spawns as they come
    }
    // 0.5.569: COD3_CTF_CARRIERTEST=1 (BENCH-CTF-CARRY.bat only, never in a
    // played game): every 60 s with both flags at home, one living attacker of
    // each side by turns is put 40 from the enemy flag, so his own pickup takes
    // it and the carry home is what the test measures (567: 3 takes in 10 min,
    // too few to tell whether a carrier gets home)
    {
        static const bool carrierTest = []() { const char* t = getenv("COD3_CTF_CARRIERTEST"); return t && t[0] == '1'; }();
        static uint32_t nextAt = 0; static int turn = 0;
        const uint32_t tnow = GetTickCount();
        if (carrierTest && g_ctfOwn[1] >= 0 && g_ctfOwn[2] >= 0)
        {
            if (!nextAt) nextAt = tnow + 30000;
            if (tnow >= nextAt && g_flagCarrier[0] < 0 && g_flagCarrier[1] < 0 && !g_flagDropped[0] && !g_flagDropped[1])
            {
                nextAt = tnow + 60000;
                const uint32_t s = 1u + uint32_t(turn++ % 2);
                // 0.5.571: three, as an assault comes (the regroup's wave): the
                // taker 40 from the flag, two more 150 out either side of the way
                // home. 570's lone carriers took it 10 of 10 times and died 0.3 to
                // 1.4 s later, in the enemy's base, alone.
                int placed = 0;
                const float* ef = g_obj.at[g_ctfOwn[3 - s]];
                const float* of = g_obj.at[g_ctfOwn[s]];
                const float hx = of[0] - ef[0], hy = of[1] - ef[1], hl = std::max(1.0f, hypotf(hx, hy));
                // 0.5.572: the escorts' places are learnt-map spots (where players
                // have stood) 100-400 from the flag, at its height, on the home
                // side, 80 apart: 571's places worked out by offsets (150 home,
                // 120 aside) never had ground at the flag's height (no escort was
                // put, 7 tests of 7)
                std::vector<BotNavigation::Point> escortAt;
                // 0.5.574: all three 500-800 out (the wave comes in and fights
                // for the flag; the taker is not put on it)
                for (const int32_t c : g_mapKnow.NearestList({ ef[0], ef[1], ef[2] }, 800.0f, 400))
                {
                    const BotNavigation::Point p = g_mapKnow.At(c);
                    const float d = hypotf(p.x - ef[0], p.y - ef[1]);
                    if (d < 500.0f || d > 800.0f || fabsf(p.z - ef[2]) > 120.0f || ((p.x - ef[0]) * hx + (p.y - ef[1]) * hy) < 0.0f) continue;
                    bool apart = true;
                    for (const auto& q : escortAt) if (hypotf(q.x - p.x, q.y - p.y) < 80.0f) apart = false;
                    if (apart) escortAt.push_back(p);
                    if (escortAt.size() >= 3) break;
                }
                for (uint32_t o = 0; o < ActiveSlots() && o < MaxClients && placed < 3; o++)
                {
                    if (!g_bot[o] || !g_ctfAttacker[o] || BotSide(base, o) != s) continue;
                    const uint32_t e = Guest::Read32(base, entities + 4 * (o + 1));
                    const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
                    if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0 || Guest::Read32(base, r + 1924) != 3) continue;
                    if (size_t(placed) >= escortAt.size()) break;   // no more places
                    const float px = escortAt[size_t(placed)].x, py = escortAt[size_t(placed)].y, pz = escortAt[size_t(placed)].z + 8.0f;
                    static uint32_t at = 0;
                    if (!at) at = Guest::AllocatePhysical(0x20);
                    auto put = [&](uint32_t a, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, a, w); };
                    put(at, px); put(at + 4, py); put(at + 8, pz);
                    PPCContext place = ctx; place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
                    place.r3.u64 = e; place.r4.u64 = at; place.lr = 0;
                    Guest::Lookup(0x8255E118)(place, base);
                    for (int k = 0; k < 3; k++) put(r + 16 + 4 * k, 0.0f);
                    g_portMovedAt[o] = tnow;
                    printf("carriertest: slot %u (side %u) put for the assault at %.0f %.0f, %.0f from the enemy flag\n", o, s, px, py, hypotf(px - ef[0], py - ef[1])); fflush(stdout);
                    placed++;
                }
                if (placed < 3) { printf("carriertest: only %d put (%zu escort places found by the flag)\n", placed, escortAt.size()); fflush(stdout); }
            }
        }
    }
    const bool sweepNow = sweep;
    sweep = false;
    for (uint32_t slot = 0; slot < ActiveSlots() && slot < MaxClients; slot++)
    {
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        const bool now = record && int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
        // 0.5.558: or put somewhere else by the game while alive: 557's
        // self-test had each bot spawned "the game's way" a second time at the
        // start AFTER the move here (slot 4: moved at line 10954, spawned again
        // at 11894), by the enemy's end. A living bot (not carrying a flag)
        // that jumps over 1000 in a frame and lands nearer the enemy's flag
        // than his own counts as a spawn. The port's own moves never do that
        // (a carrier's and a stuck bot's go toward home or along his way).
        static float lastAt[MaxClients][2] = {};
        static bool lastKnown[MaxClients] = {};
        bool jumped = false;
        if (record)
        {
            const float jx = ReadFloat(base, record), jy = ReadFloat(base, record + 4);
            // (0.5.570: not when this port moved him itself in the last 2 s:
            // 569's carrier test had every bot it put by the enemy flag taken
            // for a game re-spawn and sent home)
            const bool portMoved = slot < MaxClients && g_portMovedAt[slot] && GetTickCount() - g_portMovedAt[slot] < 2000;
            if (now && alive[slot] && lastKnown[slot] && g_bot[slot] && !BotCarries(slot) && !portMoved && std::isfinite(jx) && hypotf(jx - lastAt[slot][0], jy - lastAt[slot][1]) > 1000.0f)
            {
                const uint32_t js = BotSide(base, slot);
                if ((js == 1 || js == 2) && g_ctfOwn[js] >= 0 && g_ctfOwn[3 - js] >= 0)
                {
                    const float* jo = g_obj.at[g_ctfOwn[js]]; const float* je = g_obj.at[g_ctfOwn[3 - js]];
                    if (hypotf(je[0] - jx, je[1] - jy) < hypotf(jo[0] - jx, jo[1] - jy))
                    {
                        jumped = true;
                        printf("bots: ctf: slot %u put at %.0f %.0f by the game while alive (from %.0f %.0f), by the enemy's end: taken as a spawn\n", slot, jx, jy, lastAt[slot][0], lastAt[slot][1]);
                        fflush(stdout);
                    }
                }
            }
            lastAt[slot][0] = jx; lastAt[slot][1] = jy; lastKnown[slot] = now;
        }
        const bool spawned = now && (!alive[slot] || sweepNow || jumped);   // 0.5.483: or the start sweep
        alive[slot] = now;
        if (!spawned) continue;
        // 0.5.544: stood up by a medic, not spawned: left where he lay (543's
        // slot 13 was revived and then moved across the map as if spawned)
        if (g_bot[slot] && g_revivedAt[slot] && GetTickCount() - g_revivedAt[slot] < 3000) continue;
        const float x = ReadFloat(base, record), y = ReadFloat(base, record + 4);
        const float d0 = hypotf(g_obj.at[0][0] - x, g_obj.at[0][1] - y), d1 = hypotf(g_obj.at[1][0] - x, g_obj.at[1][1] - y);
        const int nearFlag = d0 < 0.7f * d1 ? 0 : d1 < 0.7f * d0 ? 1 : -1;   // the flag he was spawned by
        const uint32_t side = sideOf(slot, entity);
        if (side < 1 || side > 2) continue;
        // his spawn votes (not the sweep: he may stand anywhere then)
        if (!sweepNow && nearFlag >= 0)
        {
            if (g_bot[slot]) votes[side][1 - nearFlag] += 1;   // the port spawns bots by the enemy flag
            else if (slot == 0)
            {
                votes[side][nearFlag] += 3;                       // the game spawns the host by his own
                printf("bots: ctf: the host (side %u, his team read) spawned at %.0f %.0f, by the flag at %.0f %.0f\n", side, x, y, g_obj.at[nearFlag][0], g_obj.at[nearFlag][1]);
                fflush(stdout);
            }
            trim(side);
            decide();
        }
        if (g_ctfOwn[side] < 0 || g_ctfOwn[3 - side] < 0) continue;   // 0.5.480: not known yet: leave him where the game put him
        const float* own = g_obj.at[g_ctfOwn[side]];
        if (slot == 0 && !g_bot[0] && nearFlag >= 0 && nearFlag != g_ctfOwn[side] && !sweepNow)
        {
            // the game spawned the host by the other flag than the votes say
            // is his: never seen in 541-544; the game is left to it
            printf("bots: ctf: the host (side %u) spawned by the flag at %.0f %.0f, not his side's: left where the game put him\n", side, g_obj.at[nearFlag][0], g_obj.at[nearFlag][1]);
            fflush(stdout);
            continue;
        }
        if (hypotf(own[0] - x, own[1] - y) <= 500.0f) continue;   // 0.5.539: 500 (was 900): every spawn by his flag
        // 0.5.480: the map's spawn points only (g_spawns' learnt places
        // dropped: user, 479: spawns "stacked/stuck")
        std::vector<std::array<float, 3>> spots;
        // 0.5.539: AT his flag (user: "teams are supposed to spawn on their own
        // flag or near"; 538 put them 1000-1400 out, Rouen's spawn points
        // there, a median 1250 from the flag all match): a ring 160-360 round
        // it, each spot's ground found by a trace (within 120 of the flag's
        // height), worked out once a map a flag; the map's spawn points within
        // 700 too; the old 1400 ones only when there is none nearer
        {
            static std::string ringFor[2];
            static std::vector<std::array<float, 3>> ring[2];
            const int k = g_ctfOwn[side];
            if (k >= 0 && k <= 1 && ringFor[k] != key)
            {
                ringFor[k] = key; ring[k].clear();
                for (int r = 0; r < 3; r++)
                    for (int a = 0; a < 12; a++)
                    {
                        const float ang = float(a) * 0.5235988f + float(r) * 0.26f, rad = 160.0f + 100.0f * float(r);
                        const float x = own[0] + cosf(ang) * rad, y = own[1] + sinf(ang) * rad;
                        float z = 0;
                        // 0.5.540: and in sight of the flag at chest height, ground
                        // to ground (user fell outside the map: a ring spot past
                        // the map's edge had ground under it)
                        if (BotsGroundZ(ctx, base, entity, x, y, own[2] + 150.0f, z) && fabsf(z - own[2]) < 120.0f &&
                            BotsLineClear(ctx, base, entity, own[0], own[1], own[2] + 48.0f, x, y, z + 48.0f) &&
                            BotsLineClear(ctx, base, entity, own[0], own[1], own[2] + 20.0f, x, y, z + 20.0f)) ring[k].push_back({ x, y, z });
                    }
                // 0.5.542: a cramped flag (541: side 1's at -1792 -91 had 1
                // spot, every one of his side put on it) gets the places players
                // have run (ctf-routes.txt, either side's) 150 to 650 from it,
                // within 60 of its height, 90 apart, up to 10 more
                if (ring[k].size() < 6)
                {
                    const std::string map = Kernel::CurrentMap();
                    LoadCtfRoutes(map);
                    for (const char* s : { "1", "2" })
                    {
                        const auto it = g_ctfRoutes.find(map + s);
                        if (it == g_ctfRoutes.end()) continue;
                        for (const auto& route : it->second)
                            for (const auto& p : route)
                            {
                                const float d = hypotf(p.x - own[0], p.y - own[1]);
                                if (d < 150.0f || d > 650.0f || fabsf(p.z - own[2]) > 60.0f || ring[k].size() >= 16) continue;
                                float gz = 0;   // standing there, not mid-jump
                                if (!BotsGroundZ(ctx, base, entity, p.x, p.y, p.z + 40.0f, gz) || fabsf(gz - p.z) > 20.0f) continue;
                                bool apart = true;
                                for (const auto& q : ring[k]) if (hypotf(q[0] - p.x, q[1] - p.y) < 90.0f) apart = false;
                                if (apart) ring[k].push_back({ p.x, p.y, gz });
                            }
                    }
                }
                printf("bots: ctf: %zu spawn spots round the flag at %.0f %.0f\n", ring[k].size(), own[0], own[1]); fflush(stdout);
            }
            if (k >= 0 && k <= 1) spots = ring[k];
        }
        for (int pass = 0; pass < 2 && (pass == 0 || spots.empty()); pass++)
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = Guest::Read32(base, 0x82BD1FC0 + 8 * i);
            if (e < 0x40000000u || e >= 0xA0000000u) continue;
            const uint32_t cls = Guest::Read32(base, e + 548);
            if (cls < 0x40000000u || cls >= 0xA0000000u) continue;
            char name[40] = {};
            for (int c = 0; c < 39; c++) { const char ch = char(Guest::Read8(base, cls + 12 + c)); if (!ch) break; name[c] = char(tolower(static_cast<unsigned char>(ch))); }
            if (!strstr(name, "spawn") || strstr(name, "intermission")) continue;
            const std::array<float, 3> p = { ReadFloat(base, e + 336), ReadFloat(base, e + 340), ReadFloat(base, e + 344) };
            if (!std::isfinite(p[0]) || fabsf(p[0]) > 60000.0f || hypotf(p[0] - own[0], p[1] - own[1]) > (pass == 0 ? 700.0f : 1400.0f)) continue;
            spots.push_back(p);
        }
        float bestScore = -1e30f; std::array<float, 3> to = {};
        uint32_t jitter = GetTickCount() * 2654435761u + slot * 40503u;
        for (const auto& p : spots)
        {
            float room = 300.0f; bool threat = false;
            for (const Body& b : living)
            {
                const float d = hypotf(b.p[0] - p[0], b.p[1] - p[1]);
                if (fabsf(b.p[2] - p[2]) < 100.0f) room = std::min(room, d);
                if (b.side != side && d < 900.0f) threat = true;
            }
            jitter = jitter * 1103515245u + 12345u;
            // 0.5.483: near his flag first (482 moved the user from 1627 1418
            // to 914 423, farther from it: room counted, nearness not)
            const float toFlag = hypotf(p[0] - own[0], p[1] - own[1]);
            const float score = room - (threat ? 1000.0f : 0.0f) - (room < 64.0f ? 2000.0f : 0.0f) - 0.25f * toFlag - (toFlag > 900.0f ? 400.0f : 0.0f) + float((jitter >> 16) % 60);   // 0.5.480: never on top of someone if there is a free one
            if (score > bestScore) { bestScore = score; to = p; }
        }
        if (spots.empty()) continue;
        const size_t pickSize = spots.size();
        static uint32_t position = 0;
        if (!position) position = Guest::AllocatePhysical(0x20);
        auto writeF = [&](uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); };
        writeF(position, to[0]); writeF(position + 4, to[1]); writeF(position + 8, to[2] + 1.0f);
        PPCContext place = ctx;
        place.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu;
        place.r3.u64 = entity; place.r4.u64 = position; place.lr = 0;
        Guest::Lookup(0x8255E118)(place, base);
        for (int k = 0; k < 3; k++) writeF(record + 16 + 4 * k, 0.0f);
        if (slot < MaxClients) g_portMovedAt[slot] = GetTickCount();   // 0.5.484
        living.push_back({ { to[0], to[1], to[2] }, side });
        static int told = 0;
        if (told++ < 80) { printf("bots: ctf: slot %u (side %u%s) spawned at %.0f %.0f away from his flag: moved to %.0f %.0f by it (%zu spots)\n", slot, side, slot < MaxClients && g_bot[slot] ? "" : ", a player", x, y, to[0], to[1], pickSize); fflush(stdout); }
        static uint32_t moves = 0, movesTold = 0;
        if (++moves % 25 == 0 && moves != movesTold) { movesTold = moves; printf("bots: ctf: %u spawns moved to their own flag so far\n", moves); fflush(stdout); }
    }
}
static bool PoseHolds(uint32_t slot) { return g_poseOn.load() && g_bot[slot]; }

static void FeedBots(PPCContext& ctx, uint8_t* base)
{
    PoseFrame(ctx, base);   // 0.5.206
    // The time the host's own player's newest command carries: the server's
    // clock as the commands count it.
    const uint32_t serverTime = Guest::Read32(base, NewestCommand(base, 0));
    int16_t lx, ly, rx, ry;
    DebugProbe::BotSticks(lx, ly, rx, ry);
    const uint32_t buttons = DebugProbe::BotButtons();
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot]) continue;
        // With bot_sticks all zero, the bot's own brain drives him (Think
        // below); otherwise the sticks do, for trying things by hand.
        const bool brain = lx == 0 && ly == 0 && rx == 0 && ry == 0;
        BotOrders orders = {};
        int8_t legsThink[2] = {}, legsFeel[2] = {};   // 0.5.546: his legs after Think and after the feelers (ctftrace)
        if (brain && (PoseHolds(slot) || Prematch::Frozen())) orders = {};   // 0.5.206: posed: still, not firing (PoseFrame steers his view); 0.5.641: the countdown too
        else if (brain) { g_why[slot] = "early"; orders = Think(ctx, base, slot, serverTime); BotExtras(base, slot, orders);
                     legsThink[0] = orders.forward; legsThink[1] = orders.right;
                     orders.buttons = (orders.buttons & ~StanceButton) | BotStanceControl(base, slot, orders);
                     if (!BotCarries(slot)) {                           // 0.5.483: a flag carrier does none of these
                     BotSeats(ctx, base, slot, orders, serverTime);   // 0.5.197: mounted guns, vehicles
                     if (!BotRevive(ctx, base, slot, orders, serverTime))   // 0.5.216: medics revive
                         BotDispense(ctx, base, slot, orders);           // 0.5.199: ammo and health packs
                     BotLayMine(ctx, base, slot, orders); }              // 0.5.277: mines near the flag
                     BotRifleGrenades(ctx, base, slot, serverTime);      // 0.5.329: rifle grenades given and taken out
                     BotRusher2(ctx, base, slot, orders, serverTime);    // 0.5.329: Rusher 2's guns and artillery
                     BotTrail(base, slot);                               // 0.5.522: his own way, to walk back carrying
                     BotFeelers(ctx, base, slot, orders, serverTime);    // 0.5.219: feel ahead for walls
                     legsFeel[0] = orders.forward; legsFeel[1] = orders.right;
                     CarrierHome(base, slot, orders);                    // 0.5.483: a flag carrier's legs go home (0.5.535: after the feelers: his trail is walkable; they turned him about)
                     AttackerRoute(base, slot, orders);                  // 0.5.536: attackers along a saved route
                     BotFlagUse(base, slot, orders);                     // 0.5.474: CTF's flag taken with use
                     BotFlagClaim(ctx, base, slot);                      // 0.5.517: and asked for, as a client does
                     CarrierUnstick(ctx, base, slot);                    // 0.5.519: a carrier always gets home
                     BotKitProbe(base, slot);
                     // 0.5.546: CTF trace, every 3 s a bot with a CTF errand:
                     // where he is, how far the enemy flag is, his errand, which
                     // way out of Think he took, his goal, and his legs after
                     // Think, after the feelers and in the end (the saved route
                     // takes them after the feelers), so a slow attacker's cause
                     // shows (0.5.545: attackers crawled 1000 in 2.5 min)
                     if (g_ctfDoing[slot])
                     {
                         static uint32_t tracedAt[MaxClients] = {};
                         const uint32_t tnow = GetTickCount();
                         const uint32_t tl = Guest::Read32(base, EntityList);
                         const uint32_t te = tl ? Guest::Read32(base, tl + 4 * (slot + 1)) : 0;
                         const uint32_t tr = te ? Guest::Read32(base, te + 512) : 0;
                         const uint32_t ts = BotSide(base, slot);
                         if (tr && int32_t(Guest::Read32(base, tr + 304)) > 0 && tnow - tracedAt[slot] >= 3000 && (ts == 1 || ts == 2) && g_ctfOwn[3 - ts] >= 0 && g_ctfOwn[3 - ts] <= 1)
                         {
                             tracedAt[slot] = tnow;
                             const float tx = ReadFloat(base, tr), ty = ReadFloat(base, tr + 4);
                             const float* ef = g_obj.at[g_ctfOwn[3 - ts]];
                             // 0.5.547: his view, his real speed, and how far off his
                             // goal the final legs push him (forward along the view,
                             // right to its right: the world way is the view less the
                             // legs' own angle) and off where he really moves
                             const float viewDeg = ReadFloat(base, tr + 208);
                             const float vx = ReadFloat(base, tr + 16), vy = ReadFloat(base, tr + 20);
                             const float speed = hypotf(vx, vy);
                             const float goalDeg = atan2f(g_steer[slot][1] - ty, g_steer[slot][0] - tx) * 57.29578f;
                             const bool legsOn = orders.forward != 0 || orders.right != 0;
                             const float legsDeg = viewDeg + atan2f(-float(orders.right), float(orders.forward)) * 57.29578f;
                             const float legsOff = legsOn ? fabsf(AngleDifference(legsDeg, goalDeg)) : -1.0f;
                             const float moveOff = speed > 20.0f ? fabsf(AngleDifference(atan2f(vy, vx) * 57.29578f, goalDeg)) : -1.0f;
                             // 0.5.548: TacticalMove's step: where, how far off the
                             // goal's way, how it was chosen, an escape or wander on
                             const BotTacticalState& tst = g_tactical[slot];
                             const float stepOff = tst.haveStep ? fabsf(AngleDifference(atan2f(tst.step.y - ty, tst.step.x - tx) * 57.29578f, goalDeg)) : -1.0f;
                             printf("ctftrace: slot %u side %u at %.0f %.0f, enemy flag %.0f away: %s; think %s; goal %.0f %.0f (%.0f away)%s; legs think %d %d, feelers %d %d, final %d %d%s%s; view %.0f, speed %.0f, legs %.0f off the goal, moving %.0f off it; step %s %.0f off the goal (%.0f away, %s)%s%s, stalled %u, strikes %u%s%s; route: %s\n",
                                    slot, ts, tx, ty, hypotf(ef[0] - tx, ef[1] - ty), g_ctfDoing[slot], g_why[slot] ? g_why[slot] : "?",
                                    g_steer[slot][0], g_steer[slot][1], hypotf(g_steer[slot][0] - tx, g_steer[slot][1] - ty), g_steerValid[slot] ? "" : " (no goal)",
                                    legsThink[0], legsThink[1], legsFeel[0], legsFeel[1], orders.forward, orders.right,
                                    orders.wantsFire ? ", firing" : "", (orders.buttons & SprintButton) ? ", sprinting" : "",
                                    viewDeg, speed, legsOff, moveOff,
                                    tst.haveStep ? "on" : "none", stepOff, tst.haveStep ? hypotf(tst.step.x - tx, tst.step.y - ty) : 0.0f, tst.stepWhy,
                                    serverTime < tst.escapeUntil ? ", escaping" : "", serverTime < tst.wanderUntil ? ", wandering" : "", tst.stalledSamples, tst.progress.strikes,
                                    tst.blockWhy[0] ? "; direct blocked: " : "", tst.blockWhy, g_pickInfo[slot]);
                             fflush(stdout);
                         }
                     } }
        else g_yaw[slot] += float(rx) / 32768.0f * 3.0f;
        // 0.5.484: no moving unless alive and playing (+1924 == 3): user, 483:
        // bots "stuck in a tpose or a down animation ... sliding around the
        // map"; a dead or downed body driven by orders would slide. Each 1 s a
        // bot whose place changes while not playing, or that slides 150+ a
        // second with no legs ordered, is logged, with when the port last
        // moved him (CTF spawns, g_portMovedAt)
        {
            const uint32_t list = Guest::Read32(base, EntityList);
            const uint32_t self = list ? Guest::Read32(base, list + 4 * (slot + 1)) : 0;
            const uint32_t rec = self ? Guest::Read32(base, self + 512) : 0;
            const int32_t hp = rec ? int32_t(Guest::Read32(base, rec + 304)) : 0;
            const uint32_t state = rec ? Guest::Read32(base, rec + 1924) : 0;
            const bool playing = rec && hp > 0 && state == 3;
            if (!playing) { orders.forward = orders.right = 0; orders.up = 0; orders.buttons &= ~(SprintButton | UseButtons); orders.wantsFire = false; }
            // 0.5.485: and his velocity held at 0 while dead or down: 484's
            // "SLIDES" were 140 of 150 dead or downed bots (health <= 0, state
            // 4 or 5) gliding 50 to 300 a second with no orders, the port's
            // teleports long past (user: bodies "sliding around the map", T-poses)
            if (rec && !playing && hp <= 0)
                for (int k = 0; k < 3; k++) Guest::Write32(base, rec + 16 + 4 * k, 0);
            static float lastAt[MaxClients][2] = {};
            static uint32_t lastLook[MaxClients] = {}, slideTold = 0;
            static bool legs[MaxClients] = {};
            legs[slot] = legs[slot] || orders.forward != 0 || orders.right != 0;
            const uint32_t tick = GetTickCount();
            if (rec && tick - lastLook[slot] >= 1000)
            {
                const float x = ReadFloat(base, rec), y = ReadFloat(base, rec + 4);
                const float moved = hypotf(x - lastAt[slot][0], y - lastAt[slot][1]);
                if (lastLook[slot] && moved > 40.0f && moved < 3000.0f && (!playing || (!legs[slot] && moved > 150.0f)) && slideTold < 150)
                {
                    slideTold++;
                    printf("bots: slot %u SLIDES %.0f in 1 s (%s; health %d, state %u; legs %s; the port last moved him %d ms ago)\n", slot, moved,
                           playing ? "playing" : "not playing", hp, state, legs[slot] ? "on" : "off", g_portMovedAt[slot] ? int(tick - g_portMovedAt[slot]) : -1);
                    fflush(stdout);
                }
                lastAt[slot][0] = x; lastAt[slot][1] = y; lastLook[slot] = tick; legs[slot] = false;
            }
        }
        // 0.5.497: the unattended test's bot trial: frozen, X (0x40) alone
        if (int(slot) == g_ctfTrialBot && GetTickCount() < g_ctfTrialUntil) { orders = {}; orders.buttons = 0x40u; }
        if(brain) {
            static uint32_t mobilityAt[MaxClients]={};
            if(serverTime-mobilityAt[slot]>=5000) {
                mobilityAt[slot]=serverTime;
                const uint32_t list=Guest::Read32(base,EntityList);
                const uint32_t entity=list?Guest::Read32(base,list+4*(slot+1)):0;
                const uint32_t record=entity?Guest::Read32(base,entity+512):0;
                if(record) {
                    printf("bots: mobility slot %u role %s health %d position %.0f %.0f %.0f move %d %d route %d stalled %u visible %d\n",
                        slot,RoleName(g_role[slot]),int32_t(Guest::Read32(base,record+304)),
                        ReadFloat(base,record),ReadFloat(base,record+4),ReadFloat(base,record+8),
                        int(orders.forward),int(orders.right),g_tactical[slot].haveStep,
                        g_tactical[slot].stalledSamples,g_tactical[slot].visible);fflush(stdout);
                }
            }
        }
        const uint32_t player = CommandPlayer(base, slot);   // 0.5.159
        const uint32_t next = (Guest::Read32(base, player + Newest) + 1);
        const uint32_t command = player + Ring + (next & 63) * CommandSize;
        memset(base + command, 0, CommandSize);
        Guest::Write32(base, command + 0, serverTime);
        if (brain && g_throwQuiet[slot]) { orders.buttons &= ~(StanceButton | SprintButton | 1u); orders.wantsFire = false; g_throwQuiet[slot]--; }   // 0.5.220
        // 0.5.531: holding a flag, no fire, melee, grenade, smoke, aim, reload
        // or use in his command, whatever wanted them (530: a carrier's melee
        // charge fired the flag through his own command and dropped it)
        if (brain)
            if (const uint32_t carryEntity = Guest::Read32(base, Guest::Read32(base, EntityList) + 4 * (slot + 1)), carryRecord = carryEntity ? Guest::Read32(base, carryEntity + 512) : 0;
                carryRecord && OwnedFlagWeapon(base, carryRecord))
            {
                orders.buttons &= ~(1u | MeleeButton | GrenadeButton | SmokeButton | 0x8u | ReloadButton | UseButtons);
                orders.wantsFire = false;
            }
        Guest::Write32(base, command + 4, brain ? orders.buttons : (buttons & 0x00FF));
        g_firing[slot] = ((brain ? orders.buttons : buttons) & 1) != 0 || (brain && orders.wantsFire);
        // 0.5.207: Justin: "it may say the bot is throwing grenades and
        // smokes but I don't think I've ever seen any". The logs agree: 0.5.206
        // had no grenade or smoke weapon event at all, 0.5.204 about 6 throws
        // for 71 "throws a grenade". For the first 12 throws (frag 0x80 or
        // smoke 0x2 pressed), every frame for 1.2 s: what the command carries
        // (+4 buttons, +8 weapon word) and his record's weapon words
        // (+160..+215) and weapon slots (+1056..+1071), to compare with the
        // host's own throws (logged the same way below).
        const uint32_t traceList = Guest::Read32(base, EntityList);
        const uint32_t traceEntity = traceList ? Guest::Read32(base, traceList + 4 * (slot + 1)) : 0;
        const uint32_t record = traceEntity ? Guest::Read32(base, traceEntity + 512) : 0;
        // The weapon he holds (+8 of a command, as the host's own carry it):
        // the one his record has in hand (+164), or the one waiting (+196).
        // With none his move speed is nothing; he could turn but not walk.
        {
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
            const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
            uint32_t weapon = record ? Guest::Read32(base, record + 164) : 0;
            if (weapon == 0 && record) weapon = Guest::Read32(base, record + 196);
            if (weapon > 255) weapon = 0;
            // 0.5.133: the session respawn leaves his hands empty though he
            // still owns his kit (0.5.132's kit again changed nothing: "in
            // hand 0" after it). A command's weapon is how a player asks for
            // the weapon he wants up, so with nothing in hand he asks for
            // the last one he held (his first kit's, 19 in the logs).
            static uint32_t lastHeld[MaxClients] = {};
            if (weapon != 0) lastHeld[slot] = weapon;
            else if (record)
            {
                const uint32_t remembered = lastHeld[slot];
                weapon = BotWeaponSelection::Recover(remembered,
                    Guest::Read8(base, record + 1066), Guest::Read8(base, record + 1065),
                    [&](uint32_t n) { return (Guest::Read32(base, record + 1056 + 4 * (n >> 5)) & (1u << (n & 31))) != 0; });
                lastHeld[slot] = weapon;
                if (weapon != remembered)
                {
                    static int recovered = 0;
                    if ((recovered < 60 && recovered++ < 60)) { printf("bots: slot %u empty hands: stale weapon %u replaced by owned kit weapon %u\n", slot, remembered, weapon); fflush(stdout); }
                }
            }
            // 0.5.134: out of ammunition, he asks for his other gun. A
            // weapon's rounds (as work/read_weapon_cycle.py reads them): its
            // def from the list at 0x82BCBF9C, its clip at the record's
            // +4*(172 + def+1468) and its spare at +4*(80 + def+1460); owned
            // when bit (n & 31) of +1056 + 4*(n >> 5) is set; the kit's guns
            // in the slot bytes +1065 (the pistol) and +1066 (the main one).
            if (record && weapon != 0)
            {
                auto rounds = [&](uint32_t n) -> int {
                    if (n == 0 || n >= 128) return -1;
                    const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
                    const uint32_t def = list ? Guest::Read32(base, list + 4 * n) : 0;
                    if (def < 0x40000000u || def >= 0xA0000000u) return -1;
                    if (!(Guest::Read32(base, record + 1056 + 4 * (n >> 5)) & (1u << (n & 31)))) return -1;
                    const uint32_t clip = Guest::Read32(base, def + 1468), spare = Guest::Read32(base, def + 1460);
                    if (clip > 128 || spare > 128) return -1;
                    return int(Guest::Read32(base, record + 4 * (172 + clip))) + int(Guest::Read32(base, record + 4 * (80 + spare)));
                };
                const uint32_t preferred=BotLoadouts::ArmedPrimary(Guest::Read8(base,record+1065),Guest::Read8(base,record+1066),
                    [&](uint32_t n) {
                        const uint32_t list=Guest::Read32(base,0x82BCBF9C);
                        const uint32_t def=list?Guest::Read32(base,list+4*n):0;
                        if(def<0x40000000u || def>=0xA0000000u)return true;
                        const uint32_t text=Guest::Read32(base,def+8);
                        if(text<0x40000000u || text>=0xA0000000u)return true;
                        return strcmp(reinterpret_cast<const char*>(base+text),"colt")==0 ||
                               strcmp(reinterpret_cast<const char*>(base+text),"p38")==0;
                    },rounds);
                // 0.5.330: a special in hand stays; a gun his logic wants comes up
                if (brain && GetTickCount() < g_switchUntil[slot] && g_switchTo[slot] && (Guest::Read32(base, record + 1056 + 4 * (g_switchTo[slot] >> 5)) & (1u << (g_switchTo[slot] & 31))))
                {
                    weapon = g_switchTo[slot];   // 0.5.332: the switch under way
                    if (Guest::Read32(base, record + 164) == weapon) g_switchUntil[slot] = 0;
                }
                else if (brain && BotHoldsSpecial(base, record)) {}
                else if (brain && g_wantGun[slot] && rounds(g_wantGun[slot]) > 0) weapon = g_wantGun[slot];
                else weapon=BotLoadouts::PreferPrimary(weapon,preferred,rounds);
                lastHeld[slot]=weapon;
                if (rounds(weapon) == 0)
                {
                    const uint32_t main = Guest::Read8(base, record + 1066), pistol = Guest::Read8(base, record + 1065);
                    for (const uint32_t other : { main, pistol })
                        if (other != weapon && rounds(other) > 0)
                        {
                            static int told = 0;
                            if ((told < 40 && told++ < 40)) printf("bots: slot %u is out of rounds for %u: asks for %u (%d rounds)\n", slot, weapon, other, rounds(other));
                            weapon = other;
                            lastHeld[slot] = other;
                            break;
                        }
                }
            }
            if (PoseHolds(slot) && g_poseWeapon[slot]) weapon = g_poseWeapon[slot];   // 0.5.206: posed: his gun
            // 0.5.219: a throw's first two commands name the grenade, as the host's do.
            // 0.5.222: 0.5.221's traces: with the grenade named in the command the
            // throw started (+168 = 2 for ~20 commands, as the host's) but his
            // weapon in hand (+164) stayed his gun, where the host's is the
            // grenade for the whole throw; the server takes a bot's command
            // weapon only for a normal switch. So the weapon in hand is set to
            // the grenade as the throw starts, as the host's shows.
            // (0.5.752-757: a smoke-kind throw's 40 commands were traced here, to find
            // why bots' smokes and flares did not go: see BotNadeWeapon and the smoke throw)
            if (brain && g_throwFrames[slot] && g_throwWeapon[slot])
            {
                weapon = g_throwWeapon[slot]; g_throwFrames[slot]--;
                if (record) Guest::Write32(base, record + 164, g_throwWeapon[slot]);
            }
            // 0.5.526: a flag carrier asks for the flag (the game's take gives
            // him mp_flag_allies/_axis and switches to it; his command still
            // named his gun, so he switched back: no flag in his hands to see,
            // and dying he dropped his gun, the flag left hidden for ever, the
            // user: "it doesnt actually show their model holding a flag ...
            // when they die ... its either invisible")
            if (record)
                if (const uint32_t flagWeapon = OwnedFlagWeapon(base, record)) weapon = flagWeapon;
            Guest::Write32(base, command + 8, weapon);
            // Trace after command +8 has been populated. Before 0.5.209 this
            // sampled the cleared command word, hiding the selected weapon.
            if (brain && record)
            {
                static uint32_t traceUntil[MaxClients] = {};
                static int traces = 0;
                const uint32_t sent = orders.buttons;
                const uint32_t tick = GetTickCount();
                if ((g_throwFrames[slot] || (sent & 0x2u)) && tick >= traceUntil[slot] + 3000 && traces < 24) { traceUntil[slot] = tick + 1200; traces++; }
                if (tick < traceUntil[slot])
                {
                    printf("bots: throw trace slot %u: buttons %08X weapon word %08X | +160:", slot, sent, Guest::Read32(base, command + 8));
                    for (uint32_t w = 160; w < 216; w += 4) printf(" %08X", Guest::Read32(base, record + w));
                    printf(" | +1056:");
                    for (uint32_t w = 1056; w < 1072; w += 4) printf(" %08X", Guest::Read32(base, record + w));
                    printf("\n");
                    fflush(stdout);
                }
            }
            static uint32_t said[MaxClients] = {};
            if (said[slot] != weapon + 1)
            {
                said[slot] = weapon + 1;
                printf("bots: slot %u holds weapon %u\n", slot, weapon); fflush(stdout);
            }
        }
        if (brain)
        {
            if (!(DebugProbe::BotButtons() & 0x2000))
                Guest::Write32(base, command + 12, uint32_t(int32_t(g_pitch[slot] * 65536.0f / 360.0f)) & 0xFFFF);
            Guest::Write32(base, command + 16, uint32_t(int32_t(g_yaw[slot] * 65536.0f / 360.0f)) & 0xFFFF);
            Guest::Write8(base, command + 24, uint8_t(orders.forward));
            Guest::Write8(base, command + 25, uint8_t(orders.right));
            Guest::Write8(base, command + 26, uint8_t(orders.up));   // 0.5.170: a jump
            // The view his machine would send with each command (+28 pitch,
            // +32 yaw, as the host's carry it): for a host-side player the
            // server's fire shoots along it (record +2096/+2100), not +204/+208.
            {
                uint32_t bits;
                memcpy(&bits, &g_wantPitch[slot], 4); Guest::Write32(base, command + 28, bits);
                memcpy(&bits, &g_wantYaw[slot], 4); Guest::Write32(base, command + 32, bits);
            }
        }
        else
        {
            Guest::Write32(base, command + 16, uint32_t(int32_t(g_yaw[slot] * 65536.0f / 360.0f)) & 0xFFFF);
            Guest::Write8(base, command + 24, uint8_t(int8_t(ly / 258)));
            Guest::Write8(base, command + 25, uint8_t(int8_t(lx / 258)));
        }
        Guest::Write32(base, player + Newest, next);
    }
    // The host's buttons whenever they change, for learning which is which.
    {
        static uint32_t lastButtons = 0;
        const uint32_t hostButtons = Guest::Read32(base, NewestCommand(base, 0) + 4);
        if (hostButtons != lastButtons)
        {
            lastButtons = hostButtons;
            printf("bots: host buttons %08X\n", hostButtons); fflush(stdout);
        }
        // 0.5.207: the host's own frag or smoke, the same way as the bots'
        // (see "throw trace"): every frame it is held and 1 s after.
        static uint32_t hostTraceUntil = 0;
        static int hostTraces = 0;
        const uint32_t tick = GetTickCount();
        if ((hostButtons & 0x82u) && hostTraces < 400) hostTraceUntil = tick + 1000;
        if (tick < hostTraceUntil && hostTraces < 400)
        {
            hostTraces++;
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
            const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
            const uint32_t cmd = NewestCommand(base, 0);
            printf("bots: throw trace host: buttons %08X weapon word %08X (command words %08X %08X %08X)", hostButtons, Guest::Read32(base, cmd + 8),
                   Guest::Read32(base, cmd + 12), Guest::Read32(base, cmd + 16), Guest::Read32(base, cmd + 20));
            if (record)
            {
                printf(" | +160:");
                for (uint32_t w = 160; w < 216; w += 4) printf(" %08X", Guest::Read32(base, record + w));
                printf(" | +1056:");
                for (uint32_t w = 1056; w < 1072; w += 4) printf(" %08X", Guest::Read32(base, record + w));
            }
            printf("\n");
            fflush(stdout);
        }
        // 0.5.215: the surest way to the grenade buttons: when the host's own
        // frag or smoke count drops, the keys he pressed in the 1.5 s before
        // (fire, sprint, aim and stance left out) are the throw. Logged as
        // "bots: host spent a frag" and given to the bots' search.
        {
            static uint32_t recent[90] = {}, recentAt = 0, lastFrag = 0, lastSmoke = 0, hostEntity = 0;
            recent[recentAt++ % 90] = hostButtons;
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
            const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
            const bool alive = record && int32_t(Guest::Read32(base, record + 304)) > 0;
            const uint32_t frag = alive ? BotNadeAmmo(base, record, false) : 0, smoke = alive ? BotNadeAmmo(base, record, true) : 0;
            if (host != hostEntity || !alive) { hostEntity = host; lastFrag = frag; lastSmoke = smoke; }
            else
            {
                uint32_t pressed = 0;
                for (uint32_t b : recent) pressed |= b;
                pressed &= ~(0x1u | 0x4u | 0x8u | StanceButton);
                static int told = 0;
                if (frag < lastFrag && told++ < 40)
                {
                    printf("bots: host spent a frag (%u -> %u); keys pressed before it %08X\n", lastFrag, frag, pressed); fflush(stdout);
                    if (!g_fragSearch.found)
                        for (uint32_t c : g_fragSearch.candidates)
                            if (c && (pressed & c)) { g_fragSearch.found = c; printf("bots: frag button found: %08X (from the host)\n", c); fflush(stdout); break; }
                }
                if (smoke < lastSmoke && told++ < 40)
                {
                    printf("bots: host spent a smoke (%u -> %u); keys pressed before it %08X\n", lastSmoke, smoke, pressed); fflush(stdout);
                    if (!g_smokeSearch.found)
                        for (uint32_t c : g_smokeSearch.candidates)
                            if (c && c != g_fragSearch.found && (pressed & c)) { g_smokeSearch.found = c; printf("bots: smoke button found: %08X (from the host)\n", c); fflush(stdout); break; }
                }
                lastFrag = frag; lastSmoke = smoke;
            }
        }
    }
    // Deaths, for learning how a death is shown: for any player (host or
    // bot) whose health falls to 0 or less, his entity's first 256 bytes and
    // record's +36..+60, at the death and 300 ms after.
    {
        static int32_t lastHealth[MaxClients] = {};
        static uint32_t diedAt[MaxClients] = {};
        static bool second[MaxClients] = {};
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (uint32_t slot = 0; slot < ActiveSlots() && entities; slot++)
        {
            if (slot != 0 && !g_bot[slot]) continue;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            const uint32_t r = entity ? Guest::Read32(base, entity + 512) : 0;
            if (!r) continue;
            const int32_t health = int32_t(Guest::Read32(base, r + 304));
            bool dump = false;
            if (lastHealth[slot] > 0 && health <= 0) { diedAt[slot] = serverTime; second[slot] = true; dump = true; }
            else if (second[slot] && serverTime - diedAt[slot] > 300) { second[slot] = false; dump = true; }
            lastHealth[slot] = health;
            if (!dump) continue;
            printf("bots: death of %s %u (%s): entity", slot == 0 ? "host" : "bot", slot, second[slot] ? "at once" : "300 ms on");
            for (uint32_t o = 0; o < 256; o += 4) printf(" %08X", Guest::Read32(base, entity + o));
            printf(" | record +36..+60");
            for (uint32_t o = 36; o <= 60; o += 4) printf(" %08X", Guest::Read32(base, r + o));
            printf(" | state %d\n", int32_t(Guest::Read32(base, r + 1924)));
            // 0.5.165: his session player too (784 bytes), to compare a bot's
            // death with the host's: the host's death plays (others see it
            // on their consoles) and reaches the kill feed; a bot's does
            // neither. Also 1 s on (third dump).
            if (const uint32_t sp = SessionPlayer(base, slot))
            {
                printf("bots: session player of %s %u (%s) at %08X:", slot == 0 ? "host" : "bot", slot, second[slot] ? "at once" : "300 ms on", sp);
                for (uint32_t o = 0; o < SessionPlayerSize; o += 4) printf(" %08X", Guest::Read32(base, sp + o));
                printf("\n");
            }
            fflush(stdout);
        }
    }
    // bot_bring: each new number puts the bots in a row beside the host's
    // player (his record's origin, and the entity's), for finding them.
    static int brought = 0;
    const int bring = DebugProbe::BotBring();
    if (bring != brought)
    {
        brought = bring;
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t hostRecord = host ? Guest::Read32(base, host + 512) : 0;
        int n = 0;
        for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && hostRecord; slot++)
        {
            if (!g_bot[slot]) continue;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
            if (!record) continue;
            n++;
            for (int i = 0; i < 3; i++)
            {
                float v = ReadFloat(base, hostRecord + 4 * i);
                if (i == 0) v += 80.0f * n;
                if (i == 2) v += 8.0f;
                uint32_t bits; memcpy(&bits, &v, 4);
                Guest::Write32(base, record + 4 * i, bits);
                Guest::Write32(base, entity + 336 + 4 * i, bits);
                Guest::Write32(base, record + 16 + 4 * i, 0);
            }
            LogWhere(base, "brought", slot, entity);
        }
    }
    // Where each bot is, every two seconds, for watching it.
    static uint32_t last = 0;
    if (serverTime - last > 1000)
    {
        last = serverTime;
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (uint32_t slot = 0; slot < ActiveSlots() && entities; slot++)
        {
            if (slot != 0 && !g_bot[slot]) continue;
            const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
            LogWhere(base, slot == 0 ? "host" : "bot", slot, entity);
            // The fire gate (sub_82590C30): his record's +2804 names a client,
            // whose +1880 must be 2 for his weapon to fire; and +240's flags.
            if (const uint32_t r = entity ? Guest::Read32(base, entity + 512) : 0)
            {
                const int32_t index = int32_t(Guest::Read32(base, r + 2804));
                const uint32_t clients = Clients(base);
                printf("bots: %s %u fire gate: client %d, its +1880 %d, +240 %08X, entity +603 %u\n",
                       slot == 0 ? "host" : "bot", slot, index,
                       (index >= 0 && clients) ? int32_t(Guest::Read32(base, clients + index * ClientSize + 1880)) : -1,
                       Guest::Read32(base, r + 240), Guest::Read8(base, entity + 603));
            }
            // His weapon and ammunition, as words: +160..+200 (the weapon in
            // hand and the one waiting) and +1040..+1100 (the ammunition).
            if (const uint32_t r = entity ? Guest::Read32(base, entity + 512) : 0)
            {
                printf("bots: %s %u weapon", slot == 0 ? "host" : "bot", slot);
                for (uint32_t o = 160; o <= 200; o += 4) printf(" %X", Guest::Read32(base, r + o));
                printf(" | ammo");
                for (uint32_t o = 1040; o <= 1100; o += 4) printf(" %08X", Guest::Read32(base, r + o));
                printf("\n");
            }
            // The newest command, to see where the game keeps what.
            const uint32_t command = NewestCommand(base, slot);
            printf("bots: %s %u command", slot == 0 ? "host" : "bot", slot);
            for (uint32_t w = 0; w < CommandSize; w += 4) printf(" %08X", Guest::Read32(base, command + w));
            printf("\n"); fflush(stdout);
        }
    }
}

// Each bot given a session player (a copy of the host's), his side set and
// spawned again the game's own way: debug.txt's bot_session 2, and done by
// itself once the console's bot_add has added its bots.
static std::string BotsCvar(PPCContext& ctx, uint8_t* base, const char* name)
{
    static uint32_t names = 0, used = 0;
    if (names == 0) names = Guest::AllocatePhysical(0x1000);
    const uint32_t size = uint32_t(strlen(name)) + 1;
    if (used + size > 0x1000) used = 0;
    const uint32_t at = names + used;
    memcpy(base + at, name, size);
    used += (size + 3) & ~3u;
    const uint32_t cvar = Call(ctx, base, 0x8254A468, at);
    const uint32_t text = cvar ? Guest::Read32(base, cvar + 4) : 0;
    return text ? std::string(reinterpret_cast<const char*>(base + text)) : std::string("?");
}
// 0.5.149: War, learnt for the bots (no behaviour yet). The War script
// (_mp_war, compiled into the title) gathers the map's "war_flag" entities
// into the level object's (0x82A99494) array at +0x138 (count +0x140,
// element i through sub_8221DB70); each flag's script variables are a table
// (sub_821DE680 on the entity's handle: +0 the count, +8 pairs of hash and
// value). Each 2 s in a War match, every flag's table is logged when it
// changed, so a capture shows which variable is the flag's owner.
// 0.5.463: Capture the Flag, Single Capture the Flag and Headquarters, for the
// bots (Justin: "make sure they can play and learn other games modes"). Every
// 2 s in any mode but War, Battle and Team Battle, the entity table (0x82BD1FC0)
// is read for entities whose class name (+548, its text at +12) has "flag",
// "ctf", "hq" or "radio" in it, and where they are (+336). The first time in a
// map every class name seen is logged, and each objective found.
static void WatchObjectives(PPCContext& ctx, uint8_t* base)
{
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 2000) return;
    last = now;
    const std::string gametype = BotsCvar(ctx, base, "mp_gametype");
    if (gametype.empty() || gametype == "war" || gametype == "dm" || gametype == "tdm") { g_obj.valid = false; g_obj.mode = gametype; return; }
    static std::string loggedFor;
    const std::string map = Kernel::CurrentMap();
    const bool tell = loggedFor != gametype + map;
    if (tell) loggedFor = gametype + map;
    // 0.5.483: which CTF settings exist (user: a dropped flag home after 6 s;
    // 482's lay 13 minutes): the time limit shows the probe works
    if (tell && gametype.find("ctf") != std::string::npos)
    {
        std::string have;
        for (const char* name : { "scr_ctf_timelimit", "scr_ctf_scorelimit", "scr_ctf_returntime", "scr_ctf_flagreturntime", "scr_ctf_returndelay",
                                  "ctf_returntime", "ctf_flagreturntime", "scr_flagreturntime", "scr_ctf_idleflagreturntime", "scr_ctf_flagautoreturn" })
        {
            const std::string v = BotsCvar(ctx, base, name);
            have += std::string(" ") + name + "=" + v;
        }
        printf("objectives: ctf settings:%s\n", have.c_str()); fflush(stdout);
    }
    ObjectiveGoals found;
    found.mode = gametype;
    std::string classes;
    static uint32_t propIdx[24]; static float propAt[24][3], propNow[24][3]; static uint32_t propTold[24]; static int propCount = 0;   // 0.5.474/476
    static std::string propsFor;
    for (uint32_t i = 0; i < 4096; i++)
    {
        const uint32_t e = Guest::Read32(base, 0x82BD1FC0 + 8 * i);
        if (e < 0x40000000u || e >= 0xA0000000u) continue;
        const uint32_t cls = Guest::Read32(base, e + 548);
        if (cls < 0x40000000u || cls >= 0xA0000000u) continue;
        char name[32] = {};
        for (int c = 0; c < 31; c++) { const char ch = char(Guest::Read8(base, cls + 12 + c)); if (!ch) break; name[c] = char(tolower(static_cast<unsigned char>(ch))); }
        if (tell && classes.find(std::string(" ") + name + " ") == std::string::npos && classes.size() < 1500) classes += std::string(" ") + name + " ";
        // 0.5.464: 463's CTF on Rouen took the spawn points (spawn_ctf_allies_primary
        // ...): the only other names there were script_model and
        // trigger_multiple, the flags' touch areas: those, in the flag modes
        const bool flagMode = gametype.find("ctf") != std::string::npos;
        // 0.5.472: a script_model's model by name (+540 points at the model; its
        // name the text there, or at the pointer it holds), to tell the flags:
        // 0.5.471's CTF took the trigger_multiples, whose origin is 0 0 0 (a
        // brush trigger's place is its bounds), and sent the bots to the map's
        // middle (Justin: "on ctf they didnt ever pick up a flag"); its listing
        // was used up on the map before
        char modelName[48] = {};
        if (strcmp(name, "script_model") == 0)
        {
            const uint32_t model = Guest::Read32(base, e + 540);
            for (uint32_t at : { model, model >= 0x40000000u && model < 0xA0000000u ? Guest::Read32(base, model) : 0u })
            {
                if (at < 0x40000000u || at >= 0xA0000000u || modelName[0]) continue;
                char text[48] = {};
                bool printable = true;
                for (int c = 0; c < 47; c++) { const char ch = char(Guest::Read8(base, at + c)); if (!ch) break; if (ch < 32 || ch > 126) { printable = false; break; } text[c] = char(tolower(static_cast<unsigned char>(ch))); }
                if (printable && strlen(text) >= 3) memcpy(modelName, text, sizeof(text));
            }
        }
        // 0.5.474: the props (the flags among them) followed: one that leaves
        // where it was first seen (by 100 or more) is logged, so a log shows a
        // flag taken (it goes with its carrier) or brought back
        if (flagMode && strcmp(name, "script_model") == 0)
        {
            if (propsFor != gametype + map) { propsFor = gametype + map; propCount = 0; }
            const float p[3] = { ReadFloat(base, e + 336), ReadFloat(base, e + 340), ReadFloat(base, e + 344) };
            int k = 0;
            while (k < propCount && propIdx[k] != i) k++;
            if (k < propCount) memcpy(propNow[k], p, sizeof(p));
            if (k == propCount && propCount < 24) { propIdx[k] = i; memcpy(propAt[k], p, sizeof(p)); memcpy(propNow[k], p, sizeof(p)); propTold[k] = 0; propCount++; }
            else if (k < propCount && hypotf(p[0] - propAt[k][0], p[1] - propAt[k][1]) >= 100.0f && now - propTold[k] > 5000)
            {
                propTold[k] = now;
                printf("objectives: prop %u moved from %.0f %.0f to %.0f %.0f %.0f\n", i, propAt[k][0], propAt[k][1], p[0], p[1], p[2]); fflush(stdout);
            }
        }
        if (tell && (strcmp(name, "script_model") == 0 || strcmp(name, "trigger_multiple") == 0))
        {
            static int listed = 0;
            static std::string listedFor;
            if (listedFor != loggedFor) { listedFor = loggedFor; listed = 0; }   // 0.5.472: a list per map and mode
            if (listed++ < 80) { printf("objectives: %s %u at %.0f %.0f %.0f, model \"%s\"\n", name, i, ReadFloat(base, e + 336), ReadFloat(base, e + 340), ReadFloat(base, e + 344), modelName); fflush(stdout); }
        }
        if (strncmp(name, "spawn", 5) == 0) continue;
        // 0.5.473: 472's model names came back empty on Rouen, so nothing was
        // taken (no goal: the bots wandered). The flags there are the placed
        // trigger_multiples (two at each base: -1792 -91 and 2112 462; the
        // rest sit at 0 0 0), so a trigger_multiple off 0 0 0 is an objective,
        // one per place (those within 150 are the same)
        const bool objective = strstr(name, "flag") || strstr(name, "hq") || strstr(name, "radio") ||
                               strcmp(name, "trigger_multiple") == 0 ||
                               (flagMode && strstr(modelName, "flag")) || (!flagMode && (strstr(modelName, "radio") || strstr(modelName, "hq")));
        if (!objective) continue;
        if (ReadFloat(base, e + 336) == 0.0f && ReadFloat(base, e + 340) == 0.0f) continue;   // 0.5.472: never the map's 0 0 0 (a brush trigger)
        if (found.count >= 8) continue;
        for (int k = 0; k < 3; k++) found.at[found.count][k] = ReadFloat(base, e + 336 + 4 * k);
        if (!std::isfinite(found.at[found.count][0]) || fabsf(found.at[found.count][0]) > 60000.0f) continue;
        bool same = false;
        for (int k = 0; k < found.count && !same; k++) same = hypotf(found.at[k][0] - found.at[found.count][0], found.at[k][1] - found.at[found.count][1]) < 150.0f;
        if (same) continue;
        memcpy(found.name[found.count], flagMode ? "flag" : name, flagMode ? 5 : sizeof(name));
        found.count++;
    }
    // 0.5.476: a flag off its base. 475's flag (prop 49) lay at -71 284 after
    // its carrier fell, and no one went for it: now a prop that stood within
    // 150 of an objective and is 100 or more from there marks that flag out,
    // at the prop's place
    for (int k = 0; k < found.count && gametype.find("ctf") != std::string::npos; k++)
        for (int j = 0; j < propCount; j++)
            if (hypotf(propAt[j][0] - found.at[k][0], propAt[j][1] - found.at[k][1]) < 150.0f &&
                hypotf(propNow[j][0] - propAt[j][0], propNow[j][1] - propAt[j][1]) >= 100.0f)
            { found.out[k] = true; memcpy(found.live[k], propNow[j], sizeof(found.live[k])); }
    // 0.5.503: in CTF the two bases are kept once found (per map): a taken
    // flag's triggers move off and a dropped one adds its own, so the count
    // left 2 and every CTF part (sides, spawn moves, the bots' flag goals,
    // FlagEntityWatch) stopped till both were home (502: the host's dropped
    // flag lay out 130 s, no one went for it). The flags' state comes from
    // FlagEntityWatch (the models).
    if (gametype.find("ctf") != std::string::npos)
    {
        static std::string basesFor;
        static float bases[2][3];
        static bool have = false;
        if (basesFor != gametype + map) { basesFor = gametype + map; have = false; }
        if (!have && found.count == 2) { have = true; memcpy(bases, found.at, sizeof(bases)); printf("objectives: ctf bases kept: %.0f %.0f and %.0f %.0f\n", bases[0][0], bases[0][1], bases[1][0], bases[1][1]); fflush(stdout); }
        if (have && found.count != 2)
        {
            static int told = 0;
            if (told++ < 20) { printf("objectives: ctf: %d flag places seen (a flag out): the two bases kept\n", found.count); fflush(stdout); }
            found.count = 2;
            for (int k = 0; k < 2; k++) { memcpy(found.at[k], bases[k], sizeof(bases[k])); memcpy(found.name[k], "flag", 5); found.out[k] = false; }
        }
        else if (have) memcpy(found.at, bases, sizeof(bases));   // the same order every time
    }
    found.valid = found.count > 0;
    found.when = now;
    if (tell)
    {
        printf("objectives: mode \"%s\" on %s, class names:%s\n", gametype.c_str(), map.c_str(), classes.c_str());
        for (int k = 0; k < found.count; k++) printf("objectives: %s at %.0f %.0f %.0f\n", found.name[k], found.at[k][0], found.at[k][1], found.at[k][2]);
        fflush(stdout);
    }
    g_obj = found;
}
// 0.5.471: War at a glance, every 10 s: the flag in play and how many of each
// side are on it (within 150) and near it (within 450), so a log shows
// whether the bots hold it, contest it or never reach it.
static void WarReadout(uint8_t* base)
{
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 10000 || !g_war.valid || now - g_war.when > 10000) return;
    last = now;
    const uint32_t entities = Guest::Read32(base, EntityList);
    int on[3] = {}, nearBy[3] = {};
    for (uint32_t i = 0; entities && i < ActiveSlots(); i++)
    {
        const uint32_t e = Guest::Read32(base, entities + 4 * (i + 1));
        const uint32_t r = e ? Guest::Read32(base, e + 512) : 0;
        if (!r || int32_t(Guest::Read32(base, r + 304)) <= 0) continue;
        const uint32_t s = Side(base, e);
        if (s < 1 || s > 2) continue;
        const float d = hypotf(ReadFloat(base, r) - g_war.at[0], ReadFloat(base, r + 4) - g_war.at[1]);
        if (d < 150.0f) on[s]++;
        if (d < 450.0f) nearBy[s]++;
    }
    printf("war: flag %u in play at %.0f %.0f: side 1 %d on it, %d near; side 2 %d on it, %d near%s\n", g_war.index, g_war.at[0], g_war.at[1],
           on[1], nearBy[1], on[2], nearBy[2], on[1] && on[2] ? " (contested)" : on[1] || on[2] ? " (one side on it)" : "");
    fflush(stdout);
}
static void WatchWar(PPCContext& ctx, uint8_t* base)
{
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 2000) return;
    last = now;
    static std::string mode;
    const std::string gametype = BotsCvar(ctx, base, "mp_gametype");
    if (gametype != mode) { mode = gametype; printf("war: game type now \"%s\"\n", gametype.c_str()); fflush(stdout); }
    if (gametype != "war") { g_war.valid = false; return; }
    const uint32_t level = Guest::Read32(base, 0x82A99494);
    if (level < 0x40000000u || level >= 0xA0000000u) return;
    const uint32_t count = Guest::Read32(base, level + 0x140);
    if (count == 0 || count > 16) return;
    static std::string seen[16];
    static int dumps = 0;
    for (uint32_t i = 0; i < count && dumps < 400; i++)
    {
        const uint32_t at = Call(ctx, base, 0x8221DB70, level + 0x138, i);
        const uint32_t handle = (at >= 0x40000000u && at < 0xA0000000u) ? Guest::Read32(base, at) : 0;
        if (handle == 0) continue;
        const uint32_t table = Call(ctx, base, 0x821DE680, handle);
        if (table < 0x40000000u || table >= 0xA0000000u) continue;
        const uint32_t n = Guest::Read32(base, table), pairs = Guest::Read32(base, table + 8);
        if (n > 64 || pairs < 0x40000000u || pairs >= 0xA0000000u) continue;
        std::string line;
        char text[96];
        for (uint32_t k = 0; k < n; k++)
        {
            const uint32_t hash = Guest::Read32(base, pairs + 8 * k), value = Guest::Read32(base, pairs + 8 * k + 4);
            snprintf(text, sizeof(text), " %08X=%08X", hash, value);
            line += text;
            if (value >= 0x40000000u && value < 0xA0000000u)
            {
                snprintf(text, sizeof(text), "[%08X %08X %08X %08X]", Guest::Read32(base, value), Guest::Read32(base, value + 4),
                         Guest::Read32(base, value + 8), Guest::Read32(base, value + 12));
                line += text;
            }
        }
        // 0.5.151: from 0.5.150's War: F430CD43 is 0 for the one flag in play
        // (the middle at the start, then the next one along after each
        // capture) and -999 for the rest; 53377998 is the holder (-1.0 or
        // +1.0 by side, 0 none). The flag in play is where both sides fight:
        // the bots go there. Its place: its entity's origin (+336, the
        // entity from the handle's entry at 0x82BD1FC0), else its F2F5EAB4
        // piece's.
        {
            int32_t inPlay = -1; uint32_t piece = 0;
            for (uint32_t k = 0; k < n; k++)
            {
                const uint32_t hash = Guest::Read32(base, pairs + 8 * k), value = Guest::Read32(base, pairs + 8 * k + 4);
                if (hash == 0xF430CD43u) inPlay = int32_t(value);
                if (hash == 0xF2F5EAB4u) piece = value;
            }
            if (inPlay == 0)
            {
                float at[3] = {};
                const uint32_t handles[2] = { handle, piece };
                for (uint32_t h = 0; h < 2; h++)
                {
                    const uint32_t entity = handles[h] ? Guest::Read32(base, 0x82BD1FC0 + 8 * (handles[h] & 0x3FFF)) : 0;
                    if (entity < 0x40000000u || entity >= 0xA0000000u) continue;
                    for (int c = 0; c < 3; c++) at[c] = ReadFloat(base, entity + 336 + 4 * c);
                    if (fabsf(at[0]) + fabsf(at[1]) > 1.0f && fabsf(at[0]) < 100000.0f && fabsf(at[1]) < 100000.0f) break;
                    at[0] = at[1] = at[2] = 0.0f;
                }
                if (fabsf(at[0]) + fabsf(at[1]) > 1.0f)
                {
                    if (!g_war.valid || g_war.index != i)
                    {
                        printf("war: the flag in play is %u, at %.1f %.1f %.1f: the bots go there\n", i, at[0], at[1], at[2]);
                        fflush(stdout);
                    }
                    g_war.valid = true; g_war.index = i; g_war.when = now;
                    memcpy(g_war.at, at, sizeof(at));
                }
            }
        }
        if (line == seen[i]) continue;
        seen[i] = line;
        dumps++;
        printf("war: flag %u of %u (handle %08X, level +0xb0 %d):%s\n", i, count, handle,
               int32_t(Guest::Read32(base, level + 0xb0)), line.c_str());
        fflush(stdout);
    }
}

// 0.5.226: Justin: as he dies he sees "a glimpse of me, like split
// screen", very sudden, then it focuses in while he waits to respawn. Not
// known yet whether it is the game's own death camera or ours. His first
// three deaths a match: screenshots 2, 6, 12, 20 and 35 frames after he
// goes down (the screenshots folder), and for 60 frames which local client
// the game is on, his state and the gun view pointers. Log "death view:".
static void HostDeathCapture(uint8_t* base)
{
    static uint32_t frame = 0, deaths = 0, lastRecord = 0;
    static bool down = false;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
    if (!record) { down = false; RenderCommands::SetPlayerDown(false); return; }
    if (record != lastRecord) { lastRecord = record; down = false; }
    // 0.5.228: only a real death (state 3 or 4: in play), not the wait to
    // spawn (state 0: 0.5.226's first two "deaths" were the spawn screen).
    const uint32_t state = Guest::Read32(base, record + 1924);
    const bool dead = int32_t(Guest::Read32(base, record + 304)) <= 0 && (state == 3 || state == 4);
    // 0.5.234: the depth of field pass drawn while he is down (the death
    // view's double vision is laid over its blur; render_commands.cpp).
    RenderCommands::SetPlayerDown(dead);
    if (!dead) { if (state != 0) down = false; return; }
    if (!down) { down = true; frame = 0; deaths++; }
    // 0.5.252: the screenshots and the full-frame draw log below froze the
    // game for a couple of seconds on each of the first deaths (Justin:
    // "when I die it freezes"). The death view is fixed (0.5.236), so they
    // run only with COD3_DEATH_CAPTURE=1.
    static const bool capture = getenv("COD3_DEATH_CAPTURE") != nullptr;
    if (!capture || deaths > 3 || frame > 60) return;
    if (frame == 2 || frame == 6 || frame == 12 || frame == 20 || frame == 35) Overlay::RequestScreenshot();
    // 0.5.228: 0.5.226's screenshots of a death by a bot: frame 2 all
    // blurred, frame 6 a sharp standing view (the moment of the hit, frozen),
    // 12 and 20 that frozen picture doubled over the death camera on the
    // ground: the "split screen" glimpse. The first death's frame 7 has its
    // draws logged (as F12 does) to find the pass that lays the frozen
    // picture over the view.
    if (frame == 7 && deaths == 1) Render::LogNextFrame();
    printf("death view: death %u frame %u: client %u, state %u, +1932 %u, gun view %08X %08X %08X %08X%s\n", deaths, frame,
           Guest::Read32(base, CurrentClient), Guest::Read32(base, record + 1924), Guest::Read32(base, record + 1932),
           Guest::Read32(base, 0x82C37070), Guest::Read32(base, 0x82C37074), Guest::Read32(base, 0x82C37078), Guest::Read32(base, 0x82C3707C),
           (frame == 2 || frame == 6 || frame == 12 || frame == 20 || frame == 35) ? " (screenshot)" : "");
    if (frame == 60) fflush(stdout);
    frame++;
}

// The host's view after bots are given out: his field of view set again
// (1 s and 3 s on), and the view's variables logged each second for 20 s.
static void WatchHostView(PPCContext& ctx, uint8_t* base)
{
    if (g_viewWatchFrom == 0) return;
    const uint32_t since = GetTickCount() - g_viewWatchFrom;
    static uint32_t lastSecond = 0xFFFFFFFFu;
    const uint32_t second = since / 1000;
    if (second == lastSecond) return;
    lastSecond = second;
    if (second == 1 || second == 3)
        Kernel::QueueConsoleCommand("set cg_fov " + std::to_string(Settings::Get().fov));
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
    printf("bots: (gun view pointers %08X %08X %08X %08X) host view %us after the bots: cg_fov %s, cg_fovscale %s, cg_drawgun %s, in hand %u, +172 %08X, +220 %.1f\n",
           Guest::Read32(base, 0x82C37070), Guest::Read32(base, 0x82C37074), Guest::Read32(base, 0x82C37078), Guest::Read32(base, 0x82C3707C),
           second, BotsCvar(ctx, base, "cg_fov").c_str(), BotsCvar(ctx, base, "cg_fovscale").c_str(),
           BotsCvar(ctx, base, "cg_drawgun").c_str(), record ? Guest::Read32(base, record + 164) : 0,
           record ? Guest::Read32(base, record + 172) : 0, record ? ReadFloat(base, record + 220) : 0.0f);
    fflush(stdout);
    if (second >= 20) { g_viewWatchFrom = 0; lastSecond = 0xFFFFFFFFu; }
}

static void SessionBots(PPCContext& ctx, uint8_t* base)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && entities; slot++)
    {
        if (!g_bot[slot] || !GiveSessionPlayer(base, slot)) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        if (entity == 0) continue;
        // The session player is a copy of the host's, his side with
        // it. A side is two things the session's switch
        // (sub_823EA4E0) sets together: the team record's +4 and the
        // session player's +604. Setting only the first froze him,
        // half switched; the switch itself refuses him (the side
        // balance test, +3324 of the callbacks). So both, here.
        const uint32_t side = BotSide(base, slot);
        const uint32_t team = Guest::Read32(base, entity + 520);
        if (team != 0) Guest::Write32(base, team + 4, side);
        if (const uint32_t player = SessionPlayer(base, slot)) Guest::Write16(base, player + 604, uint16_t(side));
        printf("bots: slot %u on side %u\n", slot, side); fflush(stdout);
        Respawn(ctx, base, entity, side == 1);
    }
    LogSession(base);
    // 0.5.90: Justin: adding bots leaves his own view zoomed in, and aiming
    // zooms it out while held: his field of view became narrower than his
    // sights'. Each bot's session player is a copy of his, and giving them
    // out looks to reset his view settings. So his own field of view is set
    // again a second and three seconds on, and his view's state logged for
    // twenty seconds.
    g_viewWatchFrom = GetTickCount();
}

// 0.5.127: host-only bots have no network-player visual update. 126 proved
// their sole rig update ran during composition, before component creation.
// Evaluate their native animated body after the server frame, using each
// bot's own rig/pose. Native hidden-state/LOD handling remains in8251ED08.
static void UpdateBotBodies(PPCContext& ctx, uint8_t* base)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if (!entities) return;
    for (uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
    {
        if (!g_bot[slot]) continue;
        const uint32_t entity=Guest::Read32(base,entities+4*(slot+1));
        const uint32_t ps=entity?Guest::Read32(base,entity+512):0;
        const uint32_t rig=entity?Guest::Read32(base,entity+492):0;
        if (!ps || Guest::Read32(base,ps+1932)!=2 || !rig ||
            Guest::Read32(base,rig+172)!=entity || !Guest::Read32(base,rig+128) ||
            !Guest::Read32(base,rig+112) || !Guest::Read32(base,rig+32)) continue;
        Call(ctx,base,0x8251ED08,rig,0xFFFFFFFFu);
    }
}

// 0.5.132: a bot's gun. The class kit (sub_82206028: r3 the address of the
// player's script handle, r4 the class) is given to a bot when he first
// spawns (log: "class: kit for handle 00004005 ... class 0", then "holds
// weapon 19"), but his session respawn (bot_session/130) spawns him again
// after it, empty-handed: "holds weapon 0" from then on, so no gun is drawn
// in his hands and he never has ammunition to run out of. A living bot (state
// 3, health above 0) with nothing in hand and nothing waiting (+164, +196) is
// given his kit again, at most once a second and five times a life.
static void ReKitBots(PPCContext& ctx, uint8_t* base)
{
    static uint32_t lastTry[MaxClients] = {};
    static int tries[MaxClients] = {};
    static uint32_t scratch = 0;
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return;
    const uint32_t now = GetTickCount();
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot]) { tries[slot] = 0; continue; }
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t ps = entity ? Guest::Read32(base, entity + 512) : 0;
        if (!ps) continue;
        const bool alive = Guest::Read32(base, ps + 1924) == 3 && int32_t(Guest::Read32(base, ps + 304)) > 0;
        const bool empty = Guest::Read32(base, ps + 164) == 0 && Guest::Read32(base, ps + 196) == 0;
        if (!alive) { tries[slot] = 0; continue; }
        if (!empty) continue;
        if (tries[slot] >= 5 || now - lastTry[slot] < 1000) continue;
        // His script handle: his entity's entry (index slot + 1, as the
        // kits' log shows 4005 for slot 4) and its generation.
        const uint32_t index = slot + 1;
        const uint32_t entry = 0x82BD1FC0 + index * 8;
        if (Guest::Read32(base, entry) != entity) continue;
        const uint32_t handle = (Guest::Read32(base, entry + 4) << 14) | index;
        if (!scratch) scratch = Guest::AllocatePhysical(0x10);
        Guest::Write32(base, scratch, handle);
        lastTry[slot] = now;
        tries[slot]++;
        printf("bots: slot %u alive with no weapon: kit given again (handle %08X, try %d)\n", slot, handle, tries[slot]);
        fflush(stdout);
        BotClientScope clientScope(base, slot, "re-kit");
        Call(ctx, base, 0x82206028, scratch, 0);
        printf("bots: slot %u after the kit: in hand %u, waiting %u\n", slot, Guest::Read32(base, ps + 164), Guest::Read32(base, ps + 196));
        fflush(stdout);
    }
}

// 0.5.138: the host's own client kept current. Justin, on 0.5.137: after he
// killed the bot (his local place 1) he was frozen in place, his commands
// still coming in but moving nothing. The current client (0x82BCBE9C, also
// the local player the game is working for) is left on the bot's after his
// death, so the host's controller drove nobody. Between server frames, with
// only the host playing on this machine, it is put back to 0.
static void KeepHostCurrent(uint8_t* base, const char* when)
{
    if (g_bots <= 0)
    {
        for (uint32_t k = 0; k < 4; k++) g_localStateBefore[k] = Guest::Read32(base, LocalState(k));
        g_localStateKnown = true;
        return;   // 0.5.145: also for remote bots
    }
    CheckFov(base, when, 0);
    // 0.5.147: remote bots are no local players: local clients 1-3 kept as
    // they were before any bot (none of them in play).
    if (!g_botLocal.load() && g_localStateKnown)
        for (uint32_t k = 1; k < 4; k++)
        {
            const uint32_t now = Guest::Read32(base, LocalState(k));
            if (now == g_localStateBefore[k]) continue;
            static int toldState = 0;
            if ((toldState < 30 && toldState++ < 30)) { printf("bots: local client %u's state %u -> %u %s: put back\n", k, g_localStateBefore[k], now, when); fflush(stdout); }
            Guest::Write32(base, LocalState(k), g_localStateBefore[k]);
        }
    const uint32_t current = Guest::Read32(base, 0x82BCBE9Cu);
    if (current == 0) return;
    static int told = 0;
    if ((told < 30 && told++ < 30)) { printf("bots: current client was %u %s: put back to the host's 0\n", current, when); fflush(stdout); }
    Guest::Write32(base, CurrentClient, 0);
}

// 0.5.194: the host's bots on every machine, with no command on the others.
// Justin and his friend, on 0.5.193 (friend hosting, Justin joined): the
// host's bot_add showed bots only to the host. A joiner's bot_add made bots
// that only that joiner's game knew (its scoreboard, nobody else's).
// Every machine in a System Link game runs its own server; a remote player
// is in a machine's game only through a session player of that machine
// (24 entries of 784 bytes, indexed by the session's number for him), joined
// into the world by the title's join, sub_824046D8(session object, player,
// origin, angles), as HandlePlayerSetup (sub_82405158) does for one who joins
// mid-match and sub_82407538 does for all at the match's start. The host's
// bots had session players only on the host, so the other machines had
// nowhere to put what the host sends about them.
// Now: the host sends a roster ("C3BR") over the side channel every second:
// the host's own session number, then each bot's session number, slot, side,
// kit and name. A joiner gives each a session player of its own: the host's
// entry on that machine copied (his machine, so the host's packets about the
// bot are the host machine's), with the bot's number, name and side, and
// joins it the title's way. No brain, no commands and no server-side bot on
// the joiner: the bots run on the host only. bot_add works only on the host.
#include "net.h"
#include <mutex>
namespace BotRoster
{
    constexpr uint32_t ObjectFromSession = 0xAEB0;   // the session's object from its word (sub_82404580's +0xAEB0)
    constexpr uint32_t PlayersInObject = 0x1010;     // the 24 session players (sub_82407538: object +4112)
    constexpr uint32_t LocalPlaces = 0x5991;         // this machine's four local session numbers (22929)
    constexpr uint32_t Started = 0x5995;             // the match has begun (22933, set by sub_82407538)
    constexpr uint32_t Join = 0x824046D8;            // the title's join of a session player into the world
    constexpr uint8_t Version = 1;
    constexpr int EntryBytes = 20, HeaderBytes = 8, MaxEntries = 30;   // 0.5.275: 30 (32 players: 2 people + 30 bots), arrays by session number 32

    struct Entry { uint8_t number = 255, slot = 0, side = 0; int8_t kit = -1; char name[16] = {}; };
    struct Roster
    {
        bool valid = false;
        uint8_t hostNumber = 255, count = 0;
        Entry bots[MaxEntries];
        uint32_t from = 0, at = 0;
    };
    std::mutex g_lock;
    Roster g_latest;

    // This machine's mirrors of the host's bots, by session number.
    bool g_mirror[32] = {};
    uint32_t g_joinTriedAt[32] = {};
    bool g_kitTold[32] = {};
    // 0.5.207: each host bot's life as the host has it ("C3BS", 4 a second):
    // alive or not, his life count, and how he last died (the killer's
    // session number, means, weapon, hit location, damage, direction).
    constexpr int StateBytes = 18, StateHeader = 8;
    struct State { uint8_t number = 255, alive = 0, life = 0, killer = 255, means = 0, weapon = 0, hitLoc = 0, side = 0; int16_t damage = 0, timeOffset = 0; int16_t dir[3] = {}; };
    struct States { bool valid = false; uint8_t count = 0; State bots[MaxEntries]; uint32_t at = 0; };
    States g_states;
    // This machine's view of each mirror's life (joiner side).
    uint8_t g_mirrorLife[32] = {}, g_mirrorKilledLife[32] = {};
    bool g_mirrorLifeKnown[32] = {};
    uint32_t g_mirrorSpawnTried[32] = {};

    uint32_t Object(uint8_t* base) { const uint32_t mode = Session(base); return mode ? mode + ObjectFromSession : 0; }
    uint8_t LocalNumber(uint8_t* base)
    {
        const uint32_t object = Object(base);
        return object ? Guest::Read8(base, object + LocalPlaces) : 255;
    }
    float F(uint8_t* base, uint32_t a) { const uint32_t v = Guest::Read32(base, a); float f; memcpy(&f, &v, 4); return f; }
    void WriteF(uint8_t* base, uint32_t a, float f) { uint32_t v; memcpy(&v, &f, 4); Guest::Write32(base, a, v); }
}

// The side channel's thread (lobby_rules.cpp) hands every "C3BR" here.
void BotsOnRoster(const uint8_t* packet, int size, unsigned int from)
{
    using namespace BotRoster;
    if (size < HeaderBytes || packet[4] != Version) return;
    const int count = packet[6];
    if (count > MaxEntries || size < HeaderBytes + count * EntryBytes) return;
    Roster r;
    r.valid = true;
    r.hostNumber = packet[5];
    r.count = uint8_t(count);
    r.from = from;
    r.at = GetTickCount();
    for (int i = 0; i < count; i++)
    {
        const uint8_t* e = packet + HeaderBytes + i * EntryBytes;
        r.bots[i].number = e[0];
        r.bots[i].slot = e[1];
        r.bots[i].side = e[2];
        r.bots[i].kit = int8_t(e[3]);
        memcpy(r.bots[i].name, e + 4, 15);
        r.bots[i].name[15] = 0;
    }
    std::lock_guard<std::mutex> hold(g_lock);
    const bool changed = !g_latest.valid || g_latest.count != r.count || g_latest.hostNumber != r.hostNumber;
    g_latest = r;
    if (changed)
    {
        printf("bots: roster from %u.%u.%u.%u: host number %u, %d bots\n", from & 255, (from >> 8) & 255,
               (from >> 16) & 255, from >> 24, r.hostNumber, count);
        fflush(stdout);
    }
}

// 0.5.207: the side channel's thread hands every "C3BS" here.
void BotsOnStates(const uint8_t* packet, int size, unsigned int)
{
    using namespace BotRoster;
    if (size < StateHeader || packet[4] != Version) return;
    const int count = packet[6];
    if (count > MaxEntries || size < StateHeader + count * StateBytes) return;
    States st;
    st.valid = true; st.count = uint8_t(count); st.at = GetTickCount();
    for (int i = 0; i < count; i++)
    {
        const uint8_t* e = packet + StateHeader + i * StateBytes;
        State& b = st.bots[i];
        b.number = e[0]; b.alive = e[1]; b.life = e[2]; b.killer = e[3]; b.means = e[4]; b.weapon = e[5]; b.hitLoc = e[6]; b.side = e[7];
        b.damage = int16_t(e[8] << 8 | e[9]); b.timeOffset = int16_t(e[10] << 8 | e[11]);
        for (int k = 0; k < 3; k++) b.dir[k] = int16_t(e[12 + 2 * k] << 8 | e[13 + 2 * k]);
    }
    std::lock_guard<std::mutex> hold(g_lock);
    g_states = st;
}

// Whether this machine is the one whose bots everyone sees: a devmap or
// offline game (no session), or the session's own host (his player is the
// session's number 0: the host's log 0 local, the joiner's 1). A roster
// that came from another machine in the last 10 s settles it: that one hosts.
static bool ThisMachineHosts(uint8_t* base)
{
    using namespace BotRoster;
    if (Session(base) == 0) return true;
    const uint8_t mine = LocalNumber(base);
    {
        std::lock_guard<std::mutex> hold(g_lock);
        if (g_latest.valid && GetTickCount() - g_latest.at < 10000 && g_latest.hostNumber != mine) return false;
    }
    return mine == 0;
}

// 0.5.275: for the UAV plane on every machine (mp_fixes.cpp): whether an
// entity is one of this machine's bots, and whether this machine runs them.
bool BotsIsBotEntity(uint8_t* base, uint32_t entity)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = 0; entity && entities && slot < ActiveSlots() && slot < MaxClients; slot++)
        if (Guest::Read32(base, entities + 4 * (slot + 1)) == entity) return g_bot[slot];
    return false;
}
bool BotsThisMachineHostsBots(uint8_t* base) { return ThisMachineHosts(base); }
// 0.5.738: whether a movement record (an entity's +512) is one of this
// machine's bots' (mp_fixes.cpp: the mantle assist, kept for bots only).
bool BotsIsBotRecord(uint8_t* base, uint32_t record)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = 0; record && entities && slot < ActiveSlots() && slot < MaxClients; slot++)
    {
        const uint32_t ent = Guest::Read32(base, entities + 4 * (slot + 1));
        if (ent && Guest::Read32(base, ent + 512) == record) return g_bot[slot];
    }
    return false;
}

// The host: the roster, every second, while it has bots (and once more
// with none after the last goes, so the others know).
static void SendBotRoster(uint8_t* base)
{
    using namespace BotRoster;
    static uint32_t last = 0;
    static int lastCount = -1;
    const uint32_t now = GetTickCount();
    if (now - last < 1000) return;
    last = now;
    if (Session(base) == 0 || !ThisMachineHosts(base)) return;
    if (g_bots <= 0 && lastCount <= 0) return;
    uint8_t packet[HeaderBytes + MaxEntries * EntryBytes] = { 'C', '3', 'B', 'R', Version, LocalNumber(base), 0, 0 };
    int count = 0;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && count < MaxEntries; slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t player = SessionPlayer(base, slot);
        if (player == 0) continue;
        const uint8_t number = Guest::Read8(base, player);
        if (number >= SessionMaxNow()) continue;
        const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        const int16_t kit = record ? int16_t(Guest::Read16(base, record + 1920)) : -1;
        uint8_t* e = packet + HeaderBytes + count * EntryBytes;
        e[0] = number;
        e[1] = uint8_t(slot);
        e[2] = uint8_t(Guest::Read16(base, player + 604));
        e[3] = uint8_t(int8_t(kit >= 0 && kit < 127 ? kit : -1));
        for (int c = 0; c < 15; c++) e[4 + c] = Guest::Read8(base, player + 104 + c);
        e[19] = 0;
        count++;
    }
    packet[6] = uint8_t(count);
    Net::SideSend(packet, HeaderBytes + count * EntryBytes);
    if (count != lastCount)
    {
        printf("bots: roster sent: host number %u, %d bots\n", packet[5], count);
        fflush(stdout);
    }
    lastCount = count;
}

// 0.5.207: the host: each bot's life, 4 times a second (see BotsOnStates).
// The session number of whoever is in a slot (any player, the host's own
// too), for naming a killer the other machines know.
static uint8_t SessionNumberOfSlot(uint8_t* base, uint32_t slot)
{
    const uint32_t player = SessionPlayer(base, slot);
    return player ? Guest::Read8(base, player) : 255;
}
static void SendBotStates(uint8_t* base)
{
    using namespace BotRoster;
    static uint32_t last = 0;
    const uint32_t now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    if (Session(base) == 0 || !ThisMachineHosts(base) || g_bots <= 0) return;
    uint8_t packet[StateHeader + MaxEntries * StateBytes] = { 'C', '3', 'B', 'S', Version, LocalNumber(base), 0, 0 };
    const uint32_t entities = Guest::Read32(base, EntityList);
    int count = 0;
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && count < MaxEntries && entities; slot++)
    {
        if (!g_bot[slot]) continue;
        const uint8_t number = SessionNumberOfSlot(base, slot);
        if (number >= SessionMaxNow()) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        const bool alive = record && int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
        const BotDeathInfo& d = g_botDeath[slot];
        uint8_t killer = 255;
        for (uint32_t k = 0; k < ActiveSlots() && d.attacker; k++)
            if (Guest::Read32(base, entities + 4 * (k + 1)) == d.attacker) { killer = SessionNumberOfSlot(base, k); break; }
        uint8_t* e = packet + StateHeader + count * StateBytes;
        e[0] = number; e[1] = alive ? 1 : 0; e[2] = g_botLife[slot]; e[3] = killer;
        e[4] = uint8_t(d.means); e[5] = uint8_t(d.weapon); e[6] = uint8_t(d.hitLoc);
        e[7] = uint8_t(Guest::Read16(base, SessionPlayer(base, slot) + 604));
        const int16_t damage = int16_t(std::max(-32000, std::min(32000, d.damage))), offset = int16_t(std::max(-32000, std::min(32000, d.timeOffset)));
        e[8] = uint8_t(uint16_t(damage) >> 8); e[9] = uint8_t(damage); e[10] = uint8_t(uint16_t(offset) >> 8); e[11] = uint8_t(offset);
        for (int k = 0; k < 3; k++)
        {
            const int16_t v = d.haveDir && std::isfinite(d.dir[k]) ? int16_t(std::max(-1.0f, std::min(1.0f, d.dir[k])) * 30000.0f) : 0;
            e[12 + 2 * k] = uint8_t(uint16_t(v) >> 8); e[13 + 2 * k] = uint8_t(v);
        }
        count++;
    }
    packet[6] = uint8_t(count);
    Net::SideSend(packet, StateHeader + count * StateBytes);
}

// A joiner: a session player for each of the host's bots, joined the
// title's way, and his kit (a remote player's starts at -1, set by his own
// machine's class choice, which a bot never makes).
static void MirrorHostBots(PPCContext& ctx, uint8_t* base)
{
    using namespace BotRoster;
    Roster r;
    {
        std::lock_guard<std::mutex> hold(g_lock);
        r = g_latest;
    }
    const uint32_t now = GetTickCount();
    if (!r.valid || now - r.at > 5000) return;
    const uint32_t object = Object(base);
    if (object == 0) return;
    const uint8_t mine = LocalNumber(base);
    if (r.hostNumber >= SessionMaxNow() || r.hostNumber == mine) return;   // our own roster, come back
    if (g_bots > 0) return;   // this machine runs bots of its own (it should not now)
    if (Guest::Read8(base, object + Started) == 0) return;
    const uint32_t players = MpSessionPlayerAddress(base, object, PlayersInObject);
    const uint32_t hostEntry = players + r.hostNumber * SessionPlayerSize;
    if (Guest::Read8(base, hostEntry) != r.hostNumber || Guest::Read32(base, hostEntry + 4) == 0)
    {
        static uint32_t told = 0;
        if (now - told > 5000)
        {
            told = now;
            printf("bots: roster's host number %u has no session player here (number %u, holder %08X): waiting\n",
                   r.hostNumber, Guest::Read8(base, hostEntry), Guest::Read32(base, hostEntry + 4));
            fflush(stdout);
        }
        return;
    }
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (int i = 0; i < r.count; i++)
    {
        const Entry& b = r.bots[i];
        if (b.number >= SessionMaxNow() || b.number == r.hostNumber || b.number == mine) continue;
        const uint32_t entry = players + b.number * SessionPlayerSize;
        const bool present = Guest::Read8(base, entry) == b.number && Guest::Read32(base, entry + 4) != 0;
        if (present && !g_mirror[b.number])
        {
            static uint32_t conflictTold[32] = {};
            if (now - conflictTold[b.number] > 10000)
            {
                conflictTold[b.number] = now;
                printf("bots: host bot number %u (%s) is a player of this machine's session already (slot %d): left alone\n",
                       b.number, b.name, int32_t(Guest::Read32(base, entry + 8)));
                fflush(stdout);
            }
            continue;
        }
        if (!present)
        {
            // The host's entry here, copied: his machine (the +4 holder,
            // counted once more for the copy), then the bot's number, no
            // slot yet (-1), his name and side.
            memcpy(base + entry, base + hostEntry, SessionPlayerSize);
            const uint32_t held = Guest::Read32(base, entry + 4);
            if (held) Guest::Write32(base, held + 4, Guest::Read32(base, held + 4) + 1);
            Guest::Write8(base, entry, b.number);
            Guest::Write32(base, entry + 8, 0xFFFFFFFFu);
            g_mirror[b.number] = true;
            g_joinTriedAt[b.number] = 0;
            g_kitTold[b.number] = false;
            printf("bots: host bot %s (number %u, host slot %u, side %u, kit %d) given a session player here (holder %08X)\n",
                   b.name, b.number, b.slot, b.side, b.kit, held);
            fflush(stdout);
        }
        // Name and side kept as the host has them.
        if (memcmp(base + entry + 104, b.name, strlen(b.name) + 1) != 0)
        {
            memset(base + entry + 104, 0, 32);
            memcpy(base + entry + 104, b.name, strlen(b.name));
        }
        if (b.side == 1 || b.side == 2) Guest::Write16(base, entry + 604, b.side);
        const int32_t slot = int32_t(Guest::Read32(base, entry + 8));
        if (slot < 0)
        {
            if (g_joinTriedAt[b.number] != 0 && now - g_joinTriedAt[b.number] < 2000) continue;
            g_joinTriedAt[b.number] = now;
            // As sub_82407538: origin from the entry's +496, angles (0, +536, 0).
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x3000) & ~0xFu;
            const uint32_t vectors = call.r1.u32 + 0x200;   // between the join's frames and ours
            WriteF(base, vectors + 0, F(base, entry + 496));
            WriteF(base, vectors + 4, F(base, entry + 500));
            WriteF(base, vectors + 8, F(base, entry + 504));
            WriteF(base, vectors + 16, 0.0f);
            WriteF(base, vectors + 20, F(base, entry + 536));
            WriteF(base, vectors + 24, 0.0f);
            call.r3.u32 = object;
            call.r4.u32 = entry;
            call.r5.u32 = vectors;
            call.r6.u32 = vectors + 16;
            call.lr = 0;
            Guest::Lookup(Join)(call, base);
            const int32_t joined = int32_t(Guest::Read32(base, entry + 8));
            const uint32_t entity = (entities && joined >= 0 && joined < int32_t(MaxClients)) ? Guest::Read32(base, entities + 4 * (joined + 1)) : 0;
            printf("bots: host bot %s (number %u) joined here in slot %d, entity %08X\n", b.name, b.number, joined, entity);
            fflush(stdout);
            continue;
        }
        if (slot >= int32_t(MaxClients)) continue;
        const uint32_t entity = entities ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        if (record && b.kit >= 0 && Guest::Read16(base, record + 1920) == 0xFFFF)
        {
            Guest::Write16(base, record + 1920, uint16_t(b.kit));
            Guest::Write16(base, record + 1922, uint16_t(b.kit));
            if (!g_kitTold[b.number])
            {
                g_kitTold[b.number] = true;
                printf("bots: host bot %s (slot %d here) given the host's kit %d\n", b.name, slot, b.kit);
                fflush(stdout);
            }
        }
    }
    // 0.5.207: the friend, on 0.5.206: the bots "just freeze then
    // disappear" when they die. 0.5.206's death watch on his machine: his
    // copy of the host's own player is alive there (health 100, state 3) and
    // his game plays that player's deaths itself ("player dies", the flying
    // death, DeathState), but every bot's copy sat at health 0, state 0: never
    // spawned on his side, so there was nothing to kill or animate (the host
    // plays a bot's death its own way since 0.5.167, so no session message
    // goes out for it either). Now, from the host's "C3BS": a copy whose bot
    // has a new life on the host, and is not alive here, is spawned here the
    // game's way (sub_822029F0, as the host spawns bots); and one the host
    // says has died, still alive here, dies here through the game's own
    // player-dies routine (sub_82587268) with the host's killer, means,
    // weapon, direction and hit location, so the death animation plays.
    // COD3_NO_MIRROR_LIFE=1 turns it off.
    {
        static const bool off = getenv("COD3_NO_MIRROR_LIFE") != nullptr;
        States st;
        {
            std::lock_guard<std::mutex> hold(g_lock);
            st = g_states;
        }
        static uint32_t dirMemory = 0;
        if (!dirMemory) { dirMemory = Guest::AllocatePhysical(64); if (dirMemory) memset(base + dirMemory, 0, 64); }
        static int told = 0;
        if (!off && st.valid && now - st.at < 3000)
            for (int i = 0; i < st.count; i++)
            {
                const State& b = st.bots[i];
                if (b.number >= SessionMaxNow() || !g_mirror[b.number]) continue;
                const uint32_t entry = players + b.number * SessionPlayerSize;
                const int32_t slot = int32_t(Guest::Read32(base, entry + 8));
                if (slot < 0 || slot >= int32_t(MaxClients) || !entities) continue;
                const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
                const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
                if (!record) continue;
                const bool aliveHere = int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
                if (!g_mirrorLifeKnown[b.number]) { g_mirrorLifeKnown[b.number] = true; g_mirrorLife[b.number] = uint8_t(b.life - 1); g_mirrorKilledLife[b.number] = uint8_t(b.life - 1); }
                if (b.alive && !aliveHere && (g_mirrorLife[b.number] != b.life || g_mirrorSpawnTried[b.number] == 0) &&
                    now - g_mirrorSpawnTried[b.number] > 1500)
                {
                    g_mirrorSpawnTried[b.number] = now;
                    const bool allies = b.side == 1;
                    const uint32_t scratch = (ctx.r1.u32 - 0x400) & ~0xFu;
                    Guest::Write32(base, scratch, Guest::Read32(base, entity + 484));
                    Guest::Write32(base, scratch + 8, 0);
                    Call(ctx, base, MakeString, scratch + 8, allies ? AlliesName : AxisName);
                    Call(ctx, base, SpawnAtPoint, scratch, scratch + 8);
                    if (int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 4) Guest::Write32(base, record + 1924, 3);
                    const bool nowAlive = int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
                    if (nowAlive) g_mirrorLife[b.number] = b.life;
                    if ((told < 300 && told++ < 300))
                    {
                        printf("bots: mirror number %u (slot %d) spawned here for its life %u on the host: health %d, state %d%s\n", b.number, slot, b.life,
                               int32_t(Guest::Read32(base, record + 304)), int32_t(Guest::Read32(base, record + 1924)), nowAlive ? "" : " (NOT alive after the spawn)");
                        fflush(stdout);
                    }
                }
                else if (!b.alive && aliveHere && g_mirrorKilledLife[b.number] != b.life)
                {
                    g_mirrorKilledLife[b.number] = b.life;
                    uint32_t killer = 0;
                    if (b.killer < SessionMaxNow())
                    {
                        const int32_t ks = int32_t(Guest::Read32(base, players + b.killer * SessionPlayerSize + 8));
                        if (ks >= 0 && ks < int32_t(MaxClients)) killer = Guest::Read32(base, entities + 4 * (ks + 1));
                    }
                    if (!killer) killer = entity;   // no known killer: his own
                    const bool dir = b.dir[0] || b.dir[1] || b.dir[2];
                    for (int k = 0; k < 3 && dirMemory; k++) { const float f = b.dir[k] / 30000.0f; uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, dirMemory + 4 * k, w); }
                    PPCContext call = ctx;
                    call.r1.u32 = (ctx.r1.u32 - 0x3000) & ~0xFu;
                    call.r3.u32 = entity; call.r4.u32 = killer; call.r5.u32 = killer;
                    call.r6.u32 = uint32_t(int32_t(b.damage)); call.r7.u32 = b.means; call.r8.u32 = b.weapon;
                    call.r9.u32 = dir && dirMemory ? dirMemory : 0; call.r10.u32 = b.hitLoc;
                    Guest::Write32(base, call.r1.u32 + 0x54, uint32_t(int32_t(b.timeOffset)));
                    call.lr = 0;
                    Guest::Lookup(0x82587268)(call, base);
                    if ((told < 300 && told++ < 300))
                    {
                        printf("bots: mirror number %u (slot %d) dies here as on the host (life %u, killer number %u, means %u, weapon %u): health %d, state %d\n",
                               b.number, slot, b.life, b.killer, b.means, b.weapon, int32_t(Guest::Read32(base, record + 304)), int32_t(Guest::Read32(base, record + 1924)));
                        fflush(stdout);
                    }
                }
            }
        // The death watch (0.5.205), now only when a copy's health or state
        // changes here (the block changes all the time: it is his movement).
        static int32_t lastHealth[32], lastState[32];
        static bool primed[32] = {};
        static int watched = 0;
        for (uint32_t n = 0; n < SessionMaxNow() && watched < 600; n++)
        {
            const bool host = n == r.hostNumber;
            if (!g_mirror[n] && !host) continue;
            const uint32_t entry = players + n * SessionPlayerSize;
            const int32_t slot = int32_t(Guest::Read32(base, entry + 8));
            const uint32_t entity = (entities && slot >= 0 && slot < int32_t(MaxClients)) ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
            const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
            const int32_t health = record ? int32_t(Guest::Read32(base, record + 304)) : -999, state = record ? int32_t(Guest::Read32(base, record + 1924)) : -999;
            if (primed[n] && (health != lastHealth[n] || state != lastState[n]))
            {
                watched++;
                printf("bots: death watch here: %s number %u (slot %d): health %d -> %d, state %d -> %d\n", host ? "the host's player" : "mirror", n, slot,
                       lastHealth[n], health, lastState[n], state);
                fflush(stdout);
            }
            lastHealth[n] = health; lastState[n] = state; primed[n] = true;
        }
    }
    // What the mirrors are doing, every 5 s: if the host's packets reach
    // them, their positions and health change here.
    static uint32_t lastReport = 0;
    if (now - lastReport < 5000) return;
    lastReport = now;
    int shown = 0;
    for (uint32_t n = 0; n < SessionMaxNow(); n++)
    {
        if (!g_mirror[n]) continue;
        const uint32_t entry = players + n * SessionPlayerSize;
        const int32_t slot = int32_t(Guest::Read32(base, entry + 8));
        const uint32_t entity = (entities && slot >= 0 && slot < int32_t(MaxClients)) ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        if (record)
            printf("bots: mirror number %u slot %d: health %d, at %.0f %.0f %.0f, kit %d, drawable %08X, side %u\n", n, slot,
                   int32_t(Guest::Read32(base, record + 304)), F(base, record), F(base, record + 4), F(base, record + 8),
                   int16_t(Guest::Read16(base, record + 1920)), Guest::Read32(base, entity + 536), Guest::Read16(base, entry + 604));
        else
            printf("bots: mirror number %u slot %d: no entity (number byte %u, holder %08X)\n", n, slot,
                   Guest::Read8(base, entry), Guest::Read32(base, entry + 4));
        if (++shown >= 24) break;
    }
    fflush(stdout);
}

// 0.5.197: the map learnt as it is played (bot_map_knowledge.h): every
// player's feet every half second, deaths where they fell, and a kill for the
// bot who had the dead one in sight. Loaded when a map starts and saved every
// minute and when the map changes, in bots\maps\<map>.txt beside the
// executable. Also each bot's form (kills and deaths) and his switch to
// another kind of spot after three deaths with his own kind.
static std::filesystem::path MapKnowledgeFile(const std::string& map)
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::filesystem::path folder = std::filesystem::path(path).parent_path() / "bots" / "maps";
    std::string safe = map;
    for (char& c : safe) if (!isalnum((unsigned char)c) && c != '_' && c != '-') c = '_';
    return folder / (safe + ".txt");
}
static std::filesystem::path LookoutFile(const std::string& map)
{
    std::filesystem::path file = MapKnowledgeFile(map);
    file.replace_extension(".lookouts.txt");
    return file;
}
// 0.5.585: each flag's ways out (g_flagTrails), a line each: its spots' x y z
static std::filesystem::path FlagTrailsFile(const std::string& map)
{
    std::filesystem::path file = MapKnowledgeFile(map);
    return file.replace_filename(file.stem().string() + "-ctf-ways.txt");
}
static void SaveFlagTrails()
{
    if (g_mapKnowName.empty() || !g_trailsChanged) return;
    if (FILE* f = fopen(FlagTrailsFile(g_mapKnowName).string().c_str(), "wb"))
    {
        int count = 0;
        for (int k = 0; k < 2; k++)
            for (const auto& way : g_flagTrails[k])
            {
                if (way.size() < 3) continue;
                for (const auto& p : way) fprintf(f, "%.0f %.0f %.0f ", p.x, p.y, p.z);
                fprintf(f, "\n"); count++;
            }
        fclose(f);
        g_trailsChanged = false;
        printf("bots: ctf ways out for %s saved (%d)\n", g_mapKnowName.c_str(), count); fflush(stdout);
    }
}
static void LoadFlagTrails(const std::string& map)
{
    for (auto& flag : g_flagTrails) for (auto& way : flag) way.clear();
    g_flagTrailNext[0] = g_flagTrailNext[1] = 0;
    g_trailsLoaded.clear(); g_trailsChanged = false;
    if (map.empty()) return;
    if (FILE* f = fopen(FlagTrailsFile(map).string().c_str(), "rb"))
    {
        char line[8192];
        while (fgets(line, sizeof(line), f))
        {
            std::vector<BotNavigation::Point> way;
            const char* at = line; float x, y, z; int used;
            while (sscanf(at, "%f %f %f %n", &x, &y, &z, &used) == 3) { way.push_back({ x, y, z }); at += used; }
            if (way.size() >= 3) g_trailsLoaded.push_back(std::move(way));
        }
        fclose(f);
        printf("bots: ctf ways out for %s read (%zu)\n", map.c_str(), g_trailsLoaded.size()); fflush(stdout);
    }
}
static void SaveMapKnowledge(const char* why)
{
    SaveLookouts(g_mapKnowName);   // 0.5.282
    SaveFlagTrails();               // 0.5.585
    if (g_mapKnowName.empty() || g_mapKnow.changes == 0) return;
    std::error_code ec;
    const std::filesystem::path file = MapKnowledgeFile(g_mapKnowName);
    std::filesystem::create_directories(file.parent_path(), ec);
    std::filesystem::path temporary = file; temporary += ".part";
    if (FILE* f = fopen(temporary.string().c_str(), "wb"))
    {
        const std::string text = g_mapKnow.Save();
        const bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
        fclose(f);
        if (ok) { std::filesystem::rename(temporary, file, ec); if (!ec) g_mapKnow.changes = 0; }
        printf("bots: map knowledge for %s saved (%zu places, %s)\n", g_mapKnowName.c_str(), g_mapKnow.nodes.size(), why); fflush(stdout);
    }
}
static void MapKnowledgeTick(uint8_t* base)
{
    const uint32_t now = GetTickCount();
    static uint32_t lastSample = 0, lastSave = 0, lastReport = 0, lastDecay = 0;
    static bool alive[MaxClients] = {};
    static BotNavigation::Point lastFeet[MaxClients] = {};
    const std::string map = Kernel::CurrentMap();
    if (map != g_mapKnowName)
    {
        SaveMapKnowledge("map change");
        g_mapKnow.Clear();
        g_mapKnowName = map;
        LoadLookouts(map);   // 0.5.282
        LoadFlagTrails(map);   // 0.5.585
        for (auto& e : g_explore) e = {};
        g_mgSpotCount = 0;
        for (auto& sp : g_spawns) sp = {};   // 0.5.202
        if (!map.empty())
        {
            if (FILE* f = fopen(MapKnowledgeFile(map).string().c_str(), "rb"))
            {
                std::string text; char buffer[65536]; size_t got;
                while ((got = fread(buffer, 1, sizeof(buffer), f)) > 0) text.append(buffer, got);
                fclose(f);
                const bool ok = g_mapKnow.Load(text);
                // 0.5.282: each game starts from the last ones' deaths and
                // kills times 0.85: what happens now outweighs old games, so
                // the bots keep adjusting (their routes avoid deaths, their
                // spots favour kills).
                if (ok) for (auto& n : g_mapKnow.nodes) { n.deaths = uint16_t(n.deaths * 85u / 100u); n.kills = uint16_t(n.kills * 85u / 100u); n.failed = uint16_t(n.failed * 85u / 100u); }
                printf("bots: map knowledge for %s: %s (%zu places)\n", map.c_str(), ok ? "loaded" : "unreadable, starting over", g_mapKnow.nodes.size());
                if (!ok) g_mapKnow.Clear();
            }
            else printf("bots: map knowledge for %s: none yet, learning it now\n", map.c_str());
            fflush(stdout);
        }
        lastSave = now;
    }
    if (map.empty() || now - lastSample < 500) return;
    lastSample = now;
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return;
    for (uint32_t slot = 0; slot < ActiveSlots(); slot++)
    {
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        const bool living = record && int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
        const BotNavigation::Point feet = record ? BotNavigation::Point{ ReadFloat(base, record), ReadFloat(base, record + 4), ReadFloat(base, record + 8) }
                                                 : BotNavigation::Point{ NAN, NAN, NAN };
        // Riding in a vehicle or on a mounted gun is not walking.
        // 0.5.577: nor falling fast (through the world: 575's map had spots at
        // z -29388..-13524 under the flag at -1792 -91), nor just put somewhere
        // by this port (its teleports are not ways a player walked)
        const bool falling = record && fabsf(ReadFloat(base, record + 24)) > 300.0f;
        const bool portMoved = slot < MaxClients && g_portMovedAt[slot] && GetTickCount() - g_portMovedAt[slot] < 1500;
        const bool walking = living && !falling && !portMoved && !InVehicle(base, entity) && !(Guest::Read32(base, record + 240) & 0x100000u);
        g_mapKnow.Observe(int(slot), feet, now, walking);
        // 0.5.582: a player walking out from a flag at home (its base): his
        // trail kept once he is 650 out. 581's self-test: side 2's attackers
        // within 300 of the flag at -1792 -91 had their route run down to the
        // flag itself outside the building's east wall (x -1540) and went
        // straight at it into the wall; side 1's bots walk out of it each life.
        if (slot == 0 && g_obj.count >= 2 && !g_trailsLoaded.empty())
        {
            int put = 0;
            for (auto& way : g_trailsLoaded)
                for (int f = 0; f < 2; f++)
                    if (hypotf(way.front().x - g_obj.at[f][0], way.front().y - g_obj.at[f][1]) < 350.0f)
                    { g_flagTrails[f][g_flagTrailNext[f]++ % 16] = std::move(way); put++; break; }
            printf("bots: ctf ways out read in: %d put to their flags (%d and %d)\n", put, std::min(g_flagTrailNext[0], 16), std::min(g_flagTrailNext[1], 16)); fflush(stdout);
            g_trailsLoaded.clear();
        }
        if (slot < MaxClients && g_obj.count >= 2)
        {
            static std::vector<BotNavigation::Point> trail[MaxClients];
            static uint8_t trailFlag[MaxClients] = {};   // flag + 1, 0 none
            if (!walking) { trail[slot].clear(); trailFlag[slot] = 0; }
            else if (trailFlag[slot] == 0)
            {
                for (int f = 0; f < 2; f++)
                    if (g_flagCarrier[f] < 0 && !g_flagDropped[f] && hypotf(feet.x - g_obj.at[f][0], feet.y - g_obj.at[f][1]) < 300.0f)
                    { trailFlag[slot] = uint8_t(f + 1); trail[slot].assign(1, feet); break; }
            }
            else
            {
                const int f = trailFlag[slot] - 1;
                const float d = hypotf(feet.x - g_obj.at[f][0], feet.y - g_obj.at[f][1]);
                if (g_flagCarrier[f] >= 0 || g_flagDropped[f] || trail[slot].size() > 40) { trail[slot].clear(); trailFlag[slot] = 0; }
                else if (d < 300.0f) trail[slot].assign(1, feet);   // still by it: the trail starts from here (0.5.585: 300, was 200)
                else
                {
                    const BotNavigation::Point& last = trail[slot].back();
                    if (hypotf(feet.x - last.x, feet.y - last.y) > 30.0f) trail[slot].push_back(feet);
                    if (d > 650.0f)
                    {
                        if (trail[slot].size() >= 3)
                        {
                            g_flagTrails[f][g_flagTrailNext[f]++ % 16] = trail[slot];
                            g_trailsChanged = true;
                            static int told = 0;
                            if (told++ < 30) { printf("bots: a way out of flag %d's base kept: %zu spots, from %.0f %.0f %.0f to %.0f %.0f %.0f\n", f, trail[slot].size(), trail[slot].front().x, trail[slot].front().y, trail[slot].front().z, feet.x, feet.y, feet.z); fflush(stdout); }
                        }
                        trail[slot].clear(); trailFlag[slot] = 0;
                    }
                }
            }
        }
        if (alive[slot] && !living && record)
        {
            g_mapKnow.Death(lastFeet[slot]);
            if (slot < MaxClients && g_bot[slot])
            {
                g_form[slot] = std::max(-5, g_form[slot] - 1);
                const bool onErrand = g_explore[slot].dest >= 0 || now - g_explore[slot].activeTick < 3000;   // 0.5.199: one clock
                if (onErrand && ++g_styleDeaths[slot] >= 3)
                {
                    g_styleDeaths[slot] = 0; g_styleShift[slot]++;
                    const auto before = g_explore[slot].style;
                    g_explore[slot].style = BotStyle(slot);
                    printf("bots: slot %u keeps dying on his %s errands: tries %s now\n", slot, StyleName(before), StyleName(g_explore[slot].style)); fflush(stdout);
                }
            }
            for (uint32_t bot = FirstBotSlot(); bot < ActiveSlots(); bot++)
                if (g_bot[bot] && bot != slot && g_lastSeenSlot[bot] == slot && now - g_lastSeenTime[bot] < 1500)
                {
                    const uint32_t botEntity = Guest::Read32(base, entities + 4 * (bot + 1));
                    const uint32_t botRecord = botEntity ? Guest::Read32(base, botEntity + 512) : 0;
                    if (!botRecord) continue;
                    g_mapKnow.Kill({ ReadFloat(base, botRecord), ReadFloat(base, botRecord + 4), ReadFloat(base, botRecord + 8) });
                    g_form[bot] = std::min(5, g_form[bot] + 1);
                    break;
                }
        }
        // 0.5.202: a spawn: where this side appears.
        if (!alive[slot] && living && entity)
        {
            const uint32_t side = Side(base, entity);
            if (side >= 1 && side <= 2 && BotNavigation::Finite(feet))
            {
                SideSpawns& sp = g_spawns[side];
                sp.at[sp.next] = feet; sp.next = (sp.next + 1) % 32; if (sp.count < 32) sp.count++;
            }
            g_spawnedTick[slot] = now;
        }
        alive[slot] = living;
        if (living) lastFeet[slot] = feet;
    }
    if (now - lastDecay >= 60000) { lastDecay = now; for (int& f : g_form) f += f > 0 ? -1 : f < 0 ? 1 : 0; }
    // 0.5.200: how spread out the bots are, every 30 s (the median distance to
    // a bot's nearest teammate, and the share within 300 of one), and how
    // many are on an errand now, to measure the bunching Justin sees.
    {
        static uint32_t lastSpread = 0;
        if (now - lastSpread >= 30000)
        {
            lastSpread = now;
            std::vector<float> nearest; int close = 0, errands = 0, bots = 0;
            for (uint32_t a = FirstBotSlot(); a < ActiveSlots(); a++)
            {
                if (!g_bot[a] || !alive[a]) continue;
                bots++;
                if (g_explore[a].dest >= 0 || now - g_explore[a].activeTick < 1000) errands++;
                float bestD = 1e9f;
                for (uint32_t b = 0; b < ActiveSlots(); b++)
                {
                    if (b == a || !alive[b]) continue;
                    const uint32_t ea = Guest::Read32(base, entities + 4 * (a + 1)), eb = Guest::Read32(base, entities + 4 * (b + 1));
                    if (!ea || !eb || Side(base, ea) != Side(base, eb)) continue;
                    bestD = std::min(bestD, BotNavigation::Distance(lastFeet[a], lastFeet[b]));
                }
                if (bestD < 1e8f) { nearest.push_back(bestD); if (bestD < 300.0f) close++; }
            }
            if (!nearest.empty())
            {
                std::sort(nearest.begin(), nearest.end());
                printf("bots: spread: %d bots, nearest teammate median %.0f, %d of %zu within 300, %d on errands\n", bots, nearest[nearest.size() / 2], close,
                       nearest.size(), errands);
                fflush(stdout);
            }
        }
    }
    if (now - lastSave >= 60000) { lastSave = now; SaveMapKnowledge("every minute"); }
    if (now - lastReport >= 120000)
    {
        lastReport = now;
        size_t links = 0; for (const auto& n : g_mapKnow.nodes) links += n.links.size();
        const auto& shape = g_mapKnow.Measure();
        printf("bots: map knowledge for %s: %zu places, %zu ways between them; middle %.0f %.0f, outskirts from %.0f out, ground low %.0f / high %.0f%s\n",
               map.c_str(), g_mapKnow.nodes.size(), links, shape.centre.x, shape.centre.y, shape.outer, shape.zLow, shape.zHigh,
               g_mapKnow.Ready() ? "" : " (still learning: bots roam as before)");
        fflush(stdout);
    }
}

// 0.5.197: the player-limit study's first probe (docs/PLAYER-LIMIT-STUDY.md,
// Phase 0). Nothing changes in play. The 80,972 bytes after the 24 player
// states (0x82C54070..0x82C67CBC, room for 12 more of their 6352-byte
// records) are zero in the executable and no code names them. If they stay
// zero through whole matches, 16v16 (and 18v18) can keep the array where it
// is and grow into them. Checked every server frame; the first byte found
// set is logged with where it is, and a line every 2 minutes says whether
// the space is still clean. Also logged when it changes: where the session
// object lives (the executable's data or the heap), for moving its player
// list later.
static void PlayerLimitProbe(uint8_t* base)
{
    // 0.5.218: EntityList's level slot moved from +100 to +132 (136-byte
    // object, scripts/patch_recomp.py patch_entity_list_level); both are
    // written while the cap is 24. Checked every 5 s: they must match, and
    // slots 25..32 (+100..+128 less the level copy) stay empty.
    {
        static uint32_t at = 0, lastLevel = 0; static int told = 0;
        const uint32_t now = GetTickCount();
        if (now - at >= 5000)
        {
            at = now;
            const uint32_t list = Guest::Read32(base, EntityList);
            if (list)
            {
                const uint32_t oldSlot = Guest::Read32(base, list + 100), newSlot = Guest::Read32(base, list + 132);
                uint32_t spare = 0;
                for (uint32_t o = 104; o < 132; o += 4) spare |= Guest::Read32(base, list + o);
                if ((oldSlot != newSlot || spare || newSlot != lastLevel) && told < 40)
                {
                    told++; lastLevel = newSlot;
                    printf("player limit: entity list %08X level +100 %08X +132 %08X spare slots %s%s\n", list, oldSlot, newSlot, spare ? "IN USE" : "clean",
                           oldSlot == newSlot ? "" : " (MISMATCH: an old build, or a store that was missed)");
                    fflush(stdout);
                }
            }
        }
    }
    constexpr uint32_t States = 0x82C2ECF0, StateSize = 6352;
    constexpr uint32_t GapStart = 0x82C54070, GapEnd = 0x82C67CBC;
    static bool dirty = false;
    static uint32_t lastReport = 0, frames = 0, lastSession = 0;
    const uint32_t now = GetTickCount();
    ++frames;
    if (!lastReport) lastReport = now;
    if (!dirty)
    {
        uint32_t found = 0;
        const uint64_t* words = reinterpret_cast<const uint64_t*>(base + GapStart);
        const size_t count = (GapEnd - GapStart) / 8;
        for (size_t i = 0; i < count && !found; i++)
            if (words[i])
                for (uint32_t b = 0; b < 8; b++)
                    if (base[GapStart + i * 8 + b]) { found = GapStart + uint32_t(i) * 8 + b; break; }
        if (!found && Guest::Read32(base, GapEnd - 4))
            for (uint32_t a = GapEnd - 4; a < GapEnd; a++) if (base[a]) { found = a; break; }
        if (found)
        {
            dirty = true;
            const uint32_t record = (found - States) / StateSize, within = (found - States) % StateSize;
            printf("player-limit probe: the space after the 24 player states is USED: byte 0x%08X (would be record %u, +%u) is 0x%02X, after %u server frames. 0x%08X: %08X %08X %08X %08X\n",
                found, record, within, base[found], frames, found & ~15u, Guest::Read32(base, found & ~15u), Guest::Read32(base, (found & ~15u) + 4),
                Guest::Read32(base, (found & ~15u) + 8), Guest::Read32(base, (found & ~15u) + 12));
            fflush(stdout);
        }
    }
    const uint32_t session = Session(base);
    if (session != lastSession)
    {
        lastSession = session;
        if (session)
        {
            const uint32_t object = session + 0xAEB0;
            printf("player-limit probe: session at 0x%08X, its object (players at +0x1010, places at +0x5991) at 0x%08X: %s\n",
                session, object, (object >= 0x82000000u && object < 0x83000000u) ? "in the executable's data" : "on the heap");
            fflush(stdout);
        }
    }
    if (now - lastReport >= 120000)
    {
        lastReport = now;
        if (!dirty) printf("player-limit probe: the space after the 24 player states is still all zero (%u server frames)\n", frames);
        else
        {
            uint32_t set = 0;
            for (uint32_t a = GapStart; a < GapEnd; a++) set += base[a] != 0;
            printf("player-limit probe: the space after the 24 player states has %u bytes set now\n", set);
        }
        fflush(stdout);
    }
    // 0.5.205: where the 24 players' records really are. 0x82C2ECF0 + 4 x
    // 6352 is exactly the host's input state (0x82C35030) and the gun view
    // pointers (0x82C37070) are inside "record 5": that array is the FOUR
    // local players' (6352 bytes each), not the 24 server players' (the
    // 0.5.197 study's guess, and why the space after it is used). Once a
    // match has bots: each slot's entity (the list at 0x82BCC7FC) and its
    // record (entity +512), and the steps between them.
    static bool mapped = false;
    if (!mapped && g_bots >= 4)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        uint32_t entity[MaxClients] = {}, record[MaxClients] = {};
        int have = 0;
        for (uint32_t slot = 0; slot < ActiveSlots() && entities; slot++)
        {
            entity[slot] = Guest::Read32(base, entities + 4 * (slot + 1));
            record[slot] = entity[slot] ? Guest::Read32(base, entity[slot] + 512) : 0;
            if (record[slot]) have++;
        }
        if (have >= 4)
        {
            mapped = true;
            printf("player-limit probe: entity list %08X; per slot entity / record (step from the slot before):", entities);
            for (uint32_t slot = 0; slot < ActiveSlots(); slot++)
            {
                if (!entity[slot]) continue;
                const int32_t de = slot && entity[slot - 1] ? int32_t(entity[slot] - entity[slot - 1]) : 0;
                const int32_t dr = slot && record[slot - 1] && record[slot] ? int32_t(record[slot] - record[slot - 1]) : 0;
                printf(" %u:%08X(%d)/%08X(%d)", slot, entity[slot], de, record[slot], dr);
            }
            printf("\n");
            fflush(stdout);
        }
    }
}

static void UavShowsBots(uint8_t* base);   // 0.5.167
// 0.5.228: the photo tour (PHOTO-TOUR.bat, COD3_PHOTO_TOUR=1 or a list of
// maps). Justin wants loading pictures for the DLC maps, which have none of
// their own, without the gun and the HUD in them. The tour loads each map
// in turn (devmap), turns off the HUD and the gun (cg_draw2D 0, cg_drawGun
// 0), god and noclip on, and takes four pictures a map: from the map's
// learnt places (bots\maps\<map>.txt), the four farthest out from its
// middle in each quarter, 140 units above the ground, looking at the middle
// a little down; a map not learnt yet is shot from his spawn in four
// directions. The camera is placed by writing his origin (record +0..8,
// velocity +16..24 cleared) after each server frame, and turned through the
// local client's view angles (mouse_look's +1668 pitch, +1672 yaw) by the
// difference to record +204/+208. Log "photo tour:", screenshots folder.
namespace PhotoTour
{
    struct View { float x, y, z, yaw, pitch; };
    std::vector<std::string> g_maps;
    size_t g_map = 0;
    int g_stage = -1;   // -1 not started; 0 load asked; 1 settling; 2 shooting; 3 done
    uint32_t g_at = 0, g_started = 0;
    std::vector<View> g_views;
    size_t g_view = 0;
    bool g_shot = false;
    float g_spawn[4] = {};

    bool Wanted()
    {
        static const bool on = [] { const char* t = getenv("COD3_PHOTO_TOUR"); const char* f = getenv("COD3_PHOTO_FREE"); const char* l = getenv("COD3_PHOTO_LEARN");
                                    return (t && t[0] && t[0] != '0') || (f && f[0]) || (l && l[0]); }();
        return on;
    }
    // 0.5.236: the learning run (PHOTO-TOUR.bat, before the pictures of a
    // map the bots have not learnt yet, COD3_PHOTO_LEARN=<map>): the map
    // loaded as in the tour, 16 bots added, four minutes of them running
    // about (the map knowledge saved each minute and at the end), then the
    // game ends. The pictures are taken in a fresh run with no bots in them.
    bool Learn()
    {
        static const bool on = [] { const char* l = getenv("COD3_PHOTO_LEARN"); return l && l[0] && !getenv("COD3_PHOTO_TOUR"); }();
        return on;
    }
    // 0.5.233: free photo mode (PHOTO-FREE.bat <map>): the map loaded and
    // him spawned as in the tour, then god and noclip and nothing more: he
    // flies (noclip: move keys, jump up, crouch down), F10 hides the HUD and
    // the gun, F12 takes the picture.
    bool Free()
    {
        static const bool on = [] { const char* f = getenv("COD3_PHOTO_FREE"); return f && f[0]; }();
        return on;
    }
    void LoadList()
    {
        const char* t = Free() ? getenv("COD3_PHOTO_FREE") : Learn() ? getenv("COD3_PHOTO_LEARN") : getenv("COD3_PHOTO_TOUR");
        std::string list = (t && strcmp(t, "1") != 0) ? t : "rimling,ordnance,mpdepot2,longyard,mpmonte,mpstalag,mp_hilltops,mp_isle,mp_guns,shipyard,mp_farm";
        std::string cur;
        for (char c : list + ",") { if (c == ',' || c == ' ') { if (!cur.empty()) g_maps.push_back(cur); cur.clear(); } else cur += c; }
    }
    void PlanViews(const std::string& map)
    {
        g_views.clear(); g_view = 0;
        std::vector<std::array<float, 3>> places;
        if (FILE* f = fopen(MapKnowledgeFile(map).string().c_str(), "rb"))
        {
            char line[512];
            bool first = true;
            while (fgets(line, sizeof(line), f))
            {
                if (first) { first = false; continue; }
                int cx, cy, cz; float x, y, z;
                if (sscanf(line, "%d %d %d %f %f %f", &cx, &cy, &cz, &x, &y, &z) == 6) places.push_back({ x, y, z });
            }
            fclose(f);
        }
        if (places.size() >= 20)
        {
            // 0.5.236: Justin: "take some from the views we have been but
            // also some at a more head level looking at buildings and down
            // streets, like a true photographer". Twelve views a map now:
            // four high over the map looking into its middle (as 0.5.232),
            // two at roof height (200 over a place halfway out), and up to
            // six at eye height (64 over a learnt place) looking down the
            // longest open line the bots walk: for a place and one of 16
            // directions, the learnt places ahead within 110 units of the
            // line (and 220 in height) are its "street", followed while no
            // gap is longer than 350; the deepest streets win, no two
            // cameras closer than 1400 units, nor two near ones looking the
            // same way. Tilted a little up from the street's far end, so
            // the buildings fill the frame and not the cobbles.
            double mx = 0, my = 0, mz = 0;
            for (auto& p : places) { mx += p[0]; my += p[1]; mz += p[2]; }
            mx /= places.size(); my /= places.size(); mz /= places.size();
            float farthest = 0;
            for (auto& p : places) farthest = std::max(farthest, float(hypot(p[0] - mx, p[1] - my)));
            auto nearest = [&](double wx, double wy) {
                const std::array<float, 3>* best = nullptr; double bestD = 1e30;
                for (auto& p : places) { const double d = hypot(p[0] - wx, p[1] - wy); if (d < bestD) { bestD = d; best = &p; } }
                return best;
            };
            auto lookAtMiddle = [&](const std::array<float, 3>& at, float z, float maxPitch) {
                const double dist = std::max(200.0, hypot(mx - at[0], my - at[1]));
                const float yaw = float(atan2(my - at[1], mx - at[0]) * 57.29578);
                const float pitch = float(atan2(z - mz, dist) * 57.29578);
                g_views.push_back({ at[0], at[1], z, yaw, std::min(pitch, maxPitch) });
            };
            for (int k = 0; k < 4; k++)
            {
                const double a = k * 1.5707963267948966 + 0.7853981633974483;
                if (const auto* best = nearest(mx + cos(a) * farthest * 0.75, my + sin(a) * farthest * 0.75))
                    lookAtMiddle(*best, std::max(float(mz) + 450.0f, (*best)[2] + 300.0f), 35.0f);
            }
            for (int k = 0; k < 2; k++)
            {
                const double a = k * 3.141592653589793 + 0.3;
                if (const auto* best = nearest(mx + cos(a) * farthest * 0.45, my + sin(a) * farthest * 0.45))
                    lookAtMiddle(*best, (*best)[2] + 200.0f, 25.0f);
            }
            struct Street { double depth; size_t at; double angle; float farZ; };
            std::vector<Street> streets;
            const size_t step = std::max<size_t>(1, places.size() / 400);
            std::vector<std::pair<double, float>> ahead;
            for (size_t i = 0; i < places.size(); i += step)
            {
                const auto& c = places[i];
                for (int k = 0; k < 16; k++)
                {
                    const double a = k * 0.39269908169872414, ux = cos(a), uy = sin(a);
                    ahead.clear();
                    for (auto& p : places)
                    {
                        const double dx = p[0] - c[0], dy = p[1] - c[1];
                        const double t = dx * ux + dy * uy;
                        if (t <= 0 || t > 4000) continue;
                        if (fabs(dy * ux - dx * uy) > 110 || fabs(p[2] - c[2]) > 220) continue;
                        ahead.push_back({ t, p[2] });
                    }
                    std::sort(ahead.begin(), ahead.end());
                    double depth = 0, last = 0; float farZ = c[2];
                    for (auto& [t, z] : ahead) { if (t - last > 350) break; depth = t; last = t; farZ = z; }
                    if (depth >= 900) streets.push_back({ depth, i, a, farZ });
                }
            }
            std::sort(streets.begin(), streets.end(), [](const Street& a, const Street& b) { return a.depth > b.depth; });
            const size_t before = g_views.size();
            for (const Street& s : streets)
            {
                if (g_views.size() - before >= 6) break;
                const auto& c = places[s.at];
                const float yaw = float(s.angle * 57.29578);
                bool tooClose = false;
                for (size_t v = before; v < g_views.size() && !tooClose; v++)
                {
                    const double d = hypot(c[0] - g_views[v].x, c[1] - g_views[v].y);
                    if (d < 1400 || (d < 2500 && fabs(AngleDifference(yaw, g_views[v].yaw)) < 20)) tooClose = true;
                }
                if (tooClose) continue;
                const float z = c[2] + 64.0f;
                const float pitch = std::clamp(float(atan2(z - (s.farZ + 40.0f), s.depth) * 57.29578) - 3.0f, -4.0f, 6.0f);
                g_views.push_back({ c[0], c[1], z, yaw, pitch });
                printf("photo tour: %s: eye height view %zu down a street %.0f deep, at %.0f %.0f, yaw %.0f, pitch %.1f\n",
                       map.c_str(), g_views.size(), s.depth, c[0], c[1], yaw, pitch);
            }
            printf("photo tour: %s: %zu learnt places, middle %.0f %.0f %.0f, %zu views (4 high, 2 roof height, %zu eye height)\n",
                   map.c_str(), places.size(), mx, my, mz, g_views.size(), g_views.size() - before);
        }
        else if (false)
        {
            // 0.5.232: 0.5.231's rimling shots stood at head height against
            // walls and trees, one inside a house. Now high above the map
            // (450 over its middle's ground, 300 over the place below at
            // least), eight around it: four at three quarters of the way out,
            // four between them at half, each over the learnt place nearest
            // that point, looking at the middle, tilted down to it.
            double mx = 0, my = 0, mz = 0;
            for (auto& p : places) { mx += p[0]; my += p[1]; mz += p[2]; }
            mx /= places.size(); my /= places.size(); mz /= places.size();
            float farthest = 0;
            for (auto& p : places) farthest = std::max(farthest, float(hypot(p[0] - mx, p[1] - my)));
            for (int k = 0; k < 8; k++)
            {
                const double a = k * 0.7853981633974483;
                const double r = farthest * ((k & 1) ? 0.45 : 0.75);
                const double wx = mx + cos(a) * r, wy = my + sin(a) * r;
                const std::array<float, 3>* best = nullptr; double bestD = 1e30;
                for (auto& p : places) { const double d = hypot(p[0] - wx, p[1] - wy); if (d < bestD) { bestD = d; best = &p; } }
                if (!best) continue;
                const float z = std::max(float(mz) + 450.0f, (*best)[2] + 300.0f);
                const double dist = std::max(200.0, hypot(mx - (*best)[0], my - (*best)[1]));
                const float yaw = float(atan2(my - (*best)[1], mx - (*best)[0]) * 57.29578);
                const float pitch = float(atan2(z - mz, dist) * 57.29578);
                g_views.push_back({ (*best)[0], (*best)[1], z, yaw, std::min(pitch, 35.0f) });
            }
            printf("photo tour: %s: %zu learnt places, middle %.0f %.0f %.0f, %zu views from above\n", map.c_str(), places.size(), mx, my, mz, g_views.size());
        }
        else
        {
            for (int k = 0; k < 8; k++) g_views.push_back({ g_spawn[0], g_spawn[1], g_spawn[2] + ((k & 1) ? 60.0f : 350.0f), g_spawn[3] + 90.0f * (k / 2), (k & 1) ? 5.0f : 20.0f });
            printf("photo tour: %s: not learnt yet (%zu places): four views from the spawn at %.0f %.0f %.0f\n", map.c_str(), places.size(), g_spawn[0], g_spawn[1], g_spawn[2]);
        }
        fflush(stdout);
    }
    void Hold(uint8_t* base, uint32_t record, const View& v)
    {
        uint32_t word;
        auto put = [&](uint32_t at, float f) { memcpy(&word, &f, 4); Guest::Write32(base, at, word); };
        put(record, v.x); put(record + 4, v.y); put(record + 8, v.z);
        put(record + 16, 0.0f); put(record + 20, 0.0f); put(record + 24, 0.0f);
        const uint32_t index = Guest::Read32(base, 0x82A2A1C8);
        if (index > 3) return;
        const uint32_t client = 0x82A57900 + index * 6352;
        const float pitchNow = ReadFloat(base, record + 204), yawNow = ReadFloat(base, record + 208);
        put(client + 1672, ReadFloat(base, client + 1672) + AngleDifference(v.yaw, yawNow));
        put(client + 1668, ReadFloat(base, client + 1668) + AngleDifference(v.pitch, pitchNow));
    }
    void Frame()
    {
        if (!Wanted()) return;
        if (g_stage == 3)
        {
            // Free mode keeps the split screen out while he flies.
            if (Free())
                for (uint32_t k = 1; k < 4; k++)
                    if (Guest::Read32(Guest::Base, LocalState(k)) != 0) Guest::Write32(Guest::Base, LocalState(k), 0);
            return;
        }
        uint8_t* base = Guest::Base;
        const uint32_t now = GetTickCount();
        if (g_stage == -1)
        {
            if (!g_started) { g_started = now; LoadList(); printf("photo tour: %zu maps\n", g_maps.size()); fflush(stdout); }
            if (now - g_started < 20000) return;   // the menus first
            g_stage = 0; g_at = 0;
        }
        if (g_map >= g_maps.size())
        {
            Kernel::QueueConsoleCommand("cg_draw2D 1\ncg_drawGun 1");
            printf("photo tour: done (%zu maps): the pictures are in the screenshots folder\n", g_maps.size()); fflush(stdout);
            g_stage = 3;
            // 0.5.232: the console's quit froze the game in the tour (rimling:
            // done, then fps 0 and the window stayed, so PHOTO-TOUR.bat never
            // went on). The process is ended outright 3 s on instead (the
            // last screenshot written by then), from its own thread.
            if (getenv("COD3_PHOTO_QUIT"))
                std::thread([] { Sleep(3000); printf("photo tour: ending the game\n"); fflush(stdout); TerminateProcess(GetCurrentProcess(), 0); }).detach();
            return;
        }
        const std::string& map = g_maps[g_map];
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
        // 0.5.232: Justin: a devmap from the menu opens as four-player split
        // screen (four team menus). Local clients 1-3 (+0xb8 of each, 2 in
        // play) are kept out for the whole tour: one view, his.
        if (g_mapKnowName == map)
            for (uint32_t k = 1; k < 4; k++)
            {
                const uint32_t state = Guest::Read32(base, LocalState(k));
                if (state != 0)
                {
                    static int told = 0;
                    if ((told < 12 && told++ < 12)) { printf("photo tour: local client %u was %u (split screen): taken out\n", k, state); fflush(stdout); }
                    Guest::Write32(base, LocalState(k), 0);
                }
            }
        if (g_stage == 4)
        {
            // 0.5.236: the learning run: the bots running about the map.
            static uint32_t toldAt = 0;
            if (now - toldAt >= 60000) { toldAt = now; printf("photo tour: %s: bots learning the map, %u s of 240\n", map.c_str(), (now - g_at) / 1000); fflush(stdout); }
            if (now - g_at < 240000) return;
            SaveMapKnowledge("photo tour learning run");
            g_map++; g_stage = 0; g_at = 0;
            return;
        }
        if (g_stage == 0)
        {
            if (!g_at) { Kernel::QueueConsoleCommand("devmap " + map); g_at = now; printf("photo tour: loading %s\n", map.c_str()); fflush(stdout); return; }
            if (now - g_at > 120000) { printf("photo tour: %s did not load in 2 minutes: skipped\n", map.c_str()); fflush(stdout); g_map++; g_at = 0; return; }
            // 0.5.231: Justin: the tour stopped at the team menu (a devmap
            // opens it like any game). Once the map is in, A is pressed
            // every 1.5 s (auto-assign, then the class, then its confirm)
            // until he is alive.
            static uint32_t pressedAt = 0;
            static int presses = 0;
            const bool alive = record && int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
            if (g_mapKnowName == map && !alive && now - g_at > 10000 && now - pressedAt > 1500)
            {
                pressedAt = now;
                Input::PressButton("a");
                if ((presses < 60 && presses++ < 60)) { printf("photo tour: %s: pressing A for the team and class menus (state %u)\n", map.c_str(), record ? Guest::Read32(base, record + 1924) : 0); fflush(stdout); }
            }
            if (g_mapKnowName == map && alive && now - g_at > 8000)
            {
                g_spawn[0] = ReadFloat(base, record); g_spawn[1] = ReadFloat(base, record + 4); g_spawn[2] = ReadFloat(base, record + 8); g_spawn[3] = ReadFloat(base, record + 208);
                if (Learn())
                {
                    Kernel::QueueConsoleCommand("god");
                    BotsAsk(16);
                    printf("photo tour: %s: learning run: 16 bots for four minutes\n", map.c_str()); fflush(stdout);
                    g_stage = 4; g_at = now;
                    return;
                }
                if (Free())
                {
                    Kernel::QueueConsoleCommand("god\nnoclip");
                    Overlay::Notice("Photo mode: fly with the move keys (jump up, crouch down). F10 hides the HUD and gun, F12 takes a picture.", 10000);
                    printf("photo tour: %s: free photo mode (god, noclip): fly, F10, F12\n", map.c_str()); fflush(stdout);
                    g_stage = 3;
                    return;
                }
                Kernel::QueueConsoleCommand("god\nnoclip\ncg_draw2D 0\ncg_drawGun 0");
                g_stage = 1; g_at = now;
            }
            return;
        }
        if (!record) return;
        if (g_stage == 1)
        {
            if (now - g_at < 4000) return;   // textures stream in
            PlanViews(map);
            // 0.5.734: COD3_PHOTO_SKY="pitch yaw" turns every view that way
            // (the sky's sun sprite at the worldspawn sundirection: Seelow
            // Night's orange dot at -55 340).
            if (const char* sky = getenv("COD3_PHOTO_SKY"))
            {
                float pitch = 0, yaw = 0;
                if (sscanf(sky, "%f %f", &pitch, &yaw) == 2)
                    for (View& v : g_views) { v.pitch = pitch; v.yaw = yaw; }
            }
            g_stage = 2; g_at = now; g_shot = false;
            if (g_views.empty()) { g_map++; g_stage = 0; g_at = 0; }
            return;
        }
        if (g_stage == 2)
        {
            if (g_view >= g_views.size()) { g_map++; g_stage = 0; g_at = 0; return; }
            const View& v = g_views[g_view];
            Hold(base, record, v);
            // 3.5 s at each view: the camera there, the streaming caught up.
            if (!g_shot && now - g_at >= 3500)
            {
                Overlay::RequestScreenshot(); g_shot = true;
                if (getenv("COD3_SHOTS_DRAWS")) Render::LogNextFrame();   // 0.5.734: its draws pictured too
                const time_t clock = time(nullptr); struct tm local{}; localtime_s(&local, &clock);
                printf("photo tour: %s view %zu at %.0f %.0f %.0f yaw %.0f: screenshot %02d%02d%02d\n", map.c_str(), g_view + 1, v.x, v.y, v.z, v.yaw, local.tm_hour, local.tm_min, local.tm_sec);
                fflush(stdout);
            }
            if (now - g_at >= 4200) { g_view++; g_at = now; g_shot = false; }
        }
    }
}

// 0.5.239: the big-game test (BIG-TEST.bat, COD3_BIG_TEST=<maps>): for the
// player-cap work. Each map is loaded (devmap), he joins (A pressed as in
// the photo tour), then god and COD3_BIG_BOTS bots (20 by default, as many
// as the cap allows) for COD3_BIG_SECONDS (150); the scoreboard is shown now
// and then. After the last map the cap probe (cap-probe.csv) is written and
// the game ends itself. Log lines "big test:".
void MpCapProbeWrite(const char* why);
void MpCapProbeTick();
namespace BigTest
{
    std::vector<std::string> g_maps;
    size_t g_map = 0;
    int g_stage = -1;   // -1 menus; 0 loading; 1 bots playing; 2 done
    uint32_t g_at = 0, g_started = 0;
    const char* Maps() { static const char* t = getenv("COD3_BIG_TEST"); return t && t[0] && t[0] != '0' ? t : nullptr; }
    int Number(const char* name, int fallback) { const char* t = getenv(name); return t && t[0] ? atoi(t) : fallback; }
    void Frame()
    {
        if (!Maps() || g_stage == 2) return;
        uint8_t* base = Guest::Base;
        const uint32_t now = GetTickCount();
        if (g_stage == -1)
        {
            if (!g_started)
            {
                g_started = now;
                std::string cur;
                for (const char* c = Maps(); ; c++)
                {
                    if (*c == ',' || *c == ' ' || *c == 0) { if (!cur.empty()) g_maps.push_back(cur); cur.clear(); if (!*c) break; }
                    else cur += *c;
                }
                printf("big test: %zu maps, %d bots, %d s a map\n", g_maps.size(), Number("COD3_BIG_BOTS", 20), Number("COD3_BIG_SECONDS", 150)); fflush(stdout);
            }
            if (now - g_started < 20000) return;
            g_stage = 0; g_at = 0;
        }
        if (g_map >= g_maps.size())
        {
            g_stage = 2;
            MpCapProbeWrite("big test done");
            printf("big test: done (%zu maps); cap-probe.csv written; ending the game\n", g_maps.size()); fflush(stdout);
            std::thread([] { Net::PublicShutdown(); Sleep(3000); TerminateProcess(GetCurrentProcess(), 0); }).detach();   // (0.5.428: off the public list first)
            return;
        }
        const std::string& map = g_maps[g_map];
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t record = host ? Guest::Read32(base, host + 512) : 0;
        if (g_mapKnowName == map)
            for (uint32_t k = 1; k < 4; k++)
                if (Guest::Read32(base, LocalState(k)) != 0) Guest::Write32(base, LocalState(k), 0);
        if (g_stage == 0)
        {
            if (!g_at)
            {
                // 0.5.486: COD3_BIG_MODE=ctf (or any mode) set before the map
                // loads, for unattended CTF tests (CTF-TEST.bat)
                if (const char* mode = getenv("COD3_BIG_MODE"); mode && mode[0])
                {
                    Kernel::QueueConsoleCommand(std::string("set mp_gametype ") + mode);
                    Kernel::QueueConsoleCommand(std::string("set g_gametype ") + mode);
                    printf("big test: game mode %s\n", mode); fflush(stdout);
                }
                Kernel::QueueConsoleCommand("devmap " + map); g_at = now; printf("big test: loading %s\n", map.c_str()); fflush(stdout); return;
            }
            if (now - g_at > 120000) { printf("big test: %s did not load in 2 minutes: skipped\n", map.c_str()); fflush(stdout); g_map++; g_at = 0; return; }
            // 0.5.729: a devmap can be lost (put on the title's command buffer
            // while the title runs it: seelow14bots ran both sets, not the
            // devmap, and opened no file): no map archive opened in 15 s ->
            // devmap again (every 15 s, the 2-minute limit still counting).
            static uint32_t resentAt = 0;
            if (Kernel::CurrentMap() != map && now - g_at > 15000 && now - std::max(g_at, resentAt) > 15000)
            {
                resentAt = now;
                Kernel::QueueConsoleCommand("devmap " + map);
                printf("big test: %s: no map archive opened yet: devmap sent again\n", map.c_str()); fflush(stdout);
            }
            static uint32_t pressedAt = 0;
            const bool alive = record && int32_t(Guest::Read32(base, record + 304)) > 0 && Guest::Read32(base, record + 1924) == 3;
            if (g_mapKnowName == map && !alive && now - g_at > 10000 && now - pressedAt > 1500) { pressedAt = now; Input::PressButton("a"); }
            if (g_mapKnowName == map && alive && now - g_at > 8000)
            {
                Kernel::QueueConsoleCommand("god");
                BotsAsk(Number("COD3_BIG_BOTS", 20));
                printf("big test: %s: in; asking for %d bots (session %08X)\n", map.c_str(), Number("COD3_BIG_BOTS", 20), Session(base)); fflush(stdout);
                g_stage = 1; g_at = now;
            }
            return;
        }
        // g_stage 1: the bots play.
        // 0.5.386: the benchmark (BENCH-MARSEILLE.bat, COD3_BENCH_VIEWS=1 or
        // "x,y,z,yaw,pitch;..."): the camera held at fixed views in turn,
        // COD3_BENCH_VIEW_SECONDS (35) each, flying (noclip) and god, so
        // every run measures the same scenes while the bots play. The view
        // a second of the log belongs to is the last "bench: view" line
        // before it. The default views are Marseille's: two from high over
        // the city (6000 to 7000 draws), two down long streets at eye height.
        {
            static std::vector<PhotoTour::View> views;
            static bool parsed = false, flying = false;
            static int shown = -1;
            if (!parsed)
            {
                parsed = true;
                if (const char* t = getenv("COD3_BENCH_VIEWS"); t && t[0] && t[0] != '0')
                {
                    if (strcmp(t, "1") == 0)
                        views = { { 465.0f, -5617.0f, 373.0f, 80.0f, 5.7f }, { 2011.0f, 3327.0f, 373.0f, -100.0f, 5.6f },
                                  { 446.0f, -5448.0f, 50.0f, 90.0f, -1.2f }, { 1882.0f, 2562.0f, 13.0f, 248.0f, -1.6f } };
                    else
                    {
                        std::string list = t;
                        size_t at = 0;
                        while (at < list.size())
                        {
                            const size_t end = std::min(list.find(';', at), list.size());
                            PhotoTour::View v{};
                            if (sscanf(list.substr(at, end - at).c_str(), "%f,%f,%f,%f,%f", &v.x, &v.y, &v.z, &v.yaw, &v.pitch) == 5) views.push_back(v);
                            at = end + 1;
                        }
                    }
                }
            }
            if (!views.empty() && record)
            {
                if (!flying) { flying = true; Kernel::QueueConsoleCommand("noclip"); }
                const uint32_t each = uint32_t(std::max(5, Number("COD3_BENCH_VIEW_SECONDS", 35))) * 1000u;
                const int index = int(((now - g_at) / each) % views.size());
                if (index != shown)
                {
                    shown = index;
                    const PhotoTour::View& v = views[size_t(index)];
                    printf("bench: view %d at %.0f %.0f %.0f, yaw %.0f, pitch %.1f\n", index + 1, v.x, v.y, v.z, v.yaw, v.pitch);
                    // 0.5.389: the game's own setviewpos (devmap), which
                    // turns the view on the server side as a spawn does.
                    char command[128];
                    snprintf(command, sizeof(command), "setviewpos %.0f %.0f %.0f %.1f %.1f", v.x, v.y, v.z, v.yaw, v.pitch);
                    Kernel::QueueConsoleCommand(command);
                    fflush(stdout);
                }
                // 0.5.388: the place every frame; the turn every 300 ms only
                // (0.5.387's pictures: the place right, the view never
                // turned, the correction added again each frame before the
                // last had reached the server).
                {
                    const PhotoTour::View& v = views[size_t(index)];
                    uint32_t word;
                    auto put = [&](uint32_t at, float f) { memcpy(&word, &f, 4); Guest::Write32(base, at, word); };
                    put(record, v.x); put(record + 4, v.y); put(record + 8, v.z);
                    // 0.5.702: COD3_BENCH_LOOKAT=x,y,z,distance: the view does not hold
                    // the yaw it is given, so the place follows the view instead: every
                    // frame that distance back from the point along the way he faces
                    // (the eye 60 above the origin), the point in the middle of the picture
                    static const std::vector<float> lookAt = [] {
                        std::vector<float> out;
                        if (const char* t = getenv("COD3_BENCH_LOOKAT"))
                            for (const char* p = t; *p;) { char* e = nullptr; out.push_back(strtof(p, &e)); if (e == p) break; p = (*e == ',') ? e + 1 : e; }
                        return out;
                    }();
                    if (lookAt.size() >= 4)
                    {
                        const float pitch = ReadFloat(base, record + 204) * 0.01745329252f, yaw = ReadFloat(base, record + 208) * 0.01745329252f;
                        const float fx = std::cos(pitch) * std::cos(yaw), fy = std::cos(pitch) * std::sin(yaw), fz = -std::sin(pitch);
                        put(record, lookAt[0] - fx * lookAt[3]); put(record + 4, lookAt[1] - fy * lookAt[3]); put(record + 8, lookAt[2] - fz * lookAt[3] - 60.0f);
                        static uint32_t toldLook = 0;
                        if (now - toldLook >= 3000) { toldLook = now; printf("bench: looking at %.0f %.0f %.0f from %.0f %.0f %.0f (pitch %.1f yaw %.1f)\n", lookAt[0], lookAt[1], lookAt[2], lookAt[0] - fx * lookAt[3], lookAt[1] - fy * lookAt[3], lookAt[2] - fz * lookAt[3] - 60.0f, pitch * 57.29578f, yaw * 57.29578f); fflush(stdout); }
                    }
                    put(record + 16, 0.0f); put(record + 20, 0.0f); put(record + 24, 0.0f);
                    static uint32_t turnedAt = 0;
                    const uint32_t local = Guest::Read32(base, 0x82A2A1C8);
                    static const bool turnByClient = getenv("COD3_BENCH_CLIENTTURN") != nullptr;
                    if (turnByClient && local <= 3 && now - turnedAt >= 300)
                    {
                        turnedAt = now;
                        const uint32_t client = 0x82A57900 + local * 6352;
                        const float pitchNow = ReadFloat(base, record + 204), yawNow = ReadFloat(base, record + 208);
                        put(client + 1672, ReadFloat(base, client + 1672) + AngleDifference(v.yaw, yawNow));
                        put(client + 1668, ReadFloat(base, client + 1668) + AngleDifference(v.pitch, pitchNow));
                    }
                }
                // 0.5.410: the view held. setviewpos turns him once, then
                // something on the server's side turns him again (0.5.408's
                // city run: every view commanded pitch 0 yaw 0; the pictures
                // 0 0, then 20.7 156.6 for views 2-4; 0.5.407's -180 and
                // -114.3), so per-view numbers were not the same scenes. The
                // facing is checked twice a second and setviewpos sent again
                // when it is more than 1.5 degrees off (at most once a
                // second); logged "bench: turned back". COD3_BENCH_HOLDTURN=0:
                // once a view only, as before.
                // 0.5.411: off unless COD3_BENCH_HOLDTURN=1. 0.5.410's run
                // faced yaw 90 all along and setviewpos sent again every
                // second never moved it: here setviewpos places him but
                // does not turn him. The facing stays one way for a whole
                // run, so runs are compared at the same draw counts, and
                // "bench: hold check" says which way each view faced.
                {
                    static const bool holdTurn = [] { const char* t = getenv("COD3_BENCH_HOLDTURN"); return t != nullptr && t[0] == '1'; }();
                    static uint32_t turnCheckedAt = 0, turnResentAt = 0;
                    static int turnResends = 0, turnResendsLogged = 0;
                    if (holdTurn && now - turnCheckedAt >= 500)
                    {
                        turnCheckedAt = now;
                        const PhotoTour::View& v = views[size_t(index)];
                        const float pitchOff = AngleDifference(v.pitch, ReadFloat(base, record + 204));
                        const float yawOff = AngleDifference(v.yaw, ReadFloat(base, record + 208));
                        if ((fabsf(pitchOff) > 1.5f || fabsf(yawOff) > 1.5f) && now - turnResentAt >= 1000)
                        {
                            turnResentAt = now;
                            turnResends++;
                            char command[128];
                            snprintf(command, sizeof(command), "setviewpos %.0f %.0f %.0f %.1f %.1f", v.x, v.y, v.z, v.yaw, v.pitch);
                            Kernel::QueueConsoleCommand(command);
                            if (turnResendsLogged < 60)
                            {
                                turnResendsLogged++;
                                printf("bench: turned back (view %d, %d so far): pitch was %.1f off, yaw %.1f off\n", index + 1, turnResends, pitchOff, yawOff);
                                fflush(stdout);
                            }
                        }
                    }
                    // Where he faces 30 s into each view (3 s before the end of a shorter one):
                    // whether the hold worked, for the whole view.
                    static int heldOf = -1, heldWatch = -1;
                    static uint32_t heldSince = 0;
                    if (index != heldWatch) { heldWatch = index; heldSince = now; }
                    if (index != heldOf && now - heldSince >= std::min<uint32_t>(30000u, each > 3000u ? each - 3000u : each))
                    {
                        heldOf = index;
                        printf("bench: hold check, view %d: pitch %.1f yaw %.1f (wanted %.1f %.1f), %d turned back so far\n", index + 1,
                               ReadFloat(base, record + 204), ReadFloat(base, record + 208), views[size_t(index)].pitch, views[size_t(index)].yaw, turnResends);
                        fflush(stdout);
                    }
                }
                // 0.5.390: an A/B switch within each view (COD3_BENCH_TOGGLE=
                // "command A|command B"): the two commands in turn every
                // COD3_BENCH_TOGGLE_SECONDS (6), logged "bench: setting A/B",
                // so a setting is compared at the very same place and view.
                {
                    static std::vector<std::string> toggle;
                    static bool toggleParsed = false;
                    static int toggleShown = -1;
                    if (!toggleParsed)
                    {
                        toggleParsed = true;
                        if (const char* t = getenv("COD3_BENCH_TOGGLE"); t && t[0])
                        {
                            std::string all = t;
                            const size_t bar = all.find('|');
                            if (bar != std::string::npos) toggle = { all.substr(0, bar), all.substr(bar + 1) };
                        }
                    }
                    if (toggle.size() == 2)
                    {
                        const uint32_t period = uint32_t(std::max(2, Number("COD3_BENCH_TOGGLE_SECONDS", 6))) * 1000u;
                        const int which = int(((now - g_at) / period) & 1);
                        if (which != toggleShown)
                        {
                            toggleShown = which;
                            Kernel::QueueConsoleCommand(toggle[size_t(which)]);
                            printf("bench: setting %c (%s)\n", which ? 'B' : 'A', toggle[size_t(which)].c_str());
                            fflush(stdout);
                        }
                    }
                }
                // 0.5.387: a picture of each view, 10 s in, the first time
                // round (screenshots folder), so where the camera is can be
                // checked; and where the player record says he is.
                static int shotOf = -1;
                static uint32_t viewSince = 0;
                if (index != shotOf && viewSince == 0) viewSince = now;
                if (index != shotOf && now - viewSince >= 10000)
                {
                    shotOf = index; viewSince = 0;
                    if (index < 4 && (now - g_at) < each * uint32_t(views.size())) Overlay::RequestScreenshot();
                    const uint32_t local = Guest::Read32(base, 0x82A2A1C8);
                    const uint32_t client = 0x82A57900 + (local <= 3 ? local : 0) * 6352;
                    printf("bench: view %d picture; he is at %.0f %.0f %.0f, pitch %.1f yaw %.1f; local client %u, its angles %.1f %.1f\n", index + 1, ReadFloat(base, record), ReadFloat(base, record + 4),
                           ReadFloat(base, record + 8), ReadFloat(base, record + 204), ReadFloat(base, record + 208), local, ReadFloat(base, client + 1668), ReadFloat(base, client + 1672));
                    fflush(stdout);
                }
            }
        }
        static uint32_t toldAt = 0, scoreAt = 0;
        if (now - toldAt >= 15000)
        {
            toldAt = now;
            int sessionPlayers = 0;
            if (const uint32_t mode = Session(base))
                for (uint32_t i = 0; i < SessionMaxNow(); i++)
                {
                    const uint32_t p = SessionPlayerAt(base, mode, i);
                    if (Guest::Read8(base, p) < SessionMaxNow() && Guest::Read32(base, p + 4) != 0) sessionPlayers++;
                }
            printf("big test: %s: %u s, %d bots in, %d session players, host health %d\n", map.c_str(), (now - g_at) / 1000, g_bots, sessionPlayers,
                   record ? int32_t(Guest::Read32(base, record + 304)) : -1);
            fflush(stdout);
            MpCapProbeWrite("big test, during a map");
        }
        // 0.5.248: Justin watches the test and the scoreboard opening and
        // closing on its own every 25 s was in his way: only with
        // COD3_BIG_SCOREBOARD=1 now.
        if (Number("COD3_BIG_SCOREBOARD", 0) != 0 && now - scoreAt >= 25000) { scoreAt = now; Input::PressButton("back"); }
        if (now - g_at < uint32_t(Number("COD3_BIG_SECONDS", 150)) * 1000u) return;
        printf("big test: %s finished\n", map.c_str()); fflush(stdout);
        g_map++; g_stage = 0; g_at = 0;
    }
}

// 0.5.361: vehicles can't be entered with 32 players on (Justin 23:03:
// PLAY-24-PLAYERS.bat: he gets in; with max_players 32 the prompt shows but
// nothing happens, bots the same: "presses use ... link 00000000"). Static
// reading found no 24-limit in the entry path, so this measures it: while
// you stand within 160 of a vehicle, once a second (at most 24 times a map),
// your entity, your record's first 1500 bytes, the vehicle's entity and
// vehicle record, and EntityList's 34 slots go to the log. Run once at 24
// and once at 32: the words that differ are what blocks the way in.
// COD3_VEHICLE_PROBE=0: off.
// 0.5.365: a model as the tag lookup (sub_82434E60 -> sub_820D2498) reads it:
// the first non-empty of its eight part pointers (+0x24..), that part's
// table (+8), the table's count (+0x10) and names (+0x14). Justin, 0.5.364
// (00:03): the game crashed in the killcam after a vehicle death, reading
// 0x96170000 there (no memory: the hole between the heap and the image).
static bool GuestPtrOk(uint32_t a) { return (a >= 0x40000000u && a < 0x80000000u) || (a >= 0x82000000u && a < 0x84000000u) || (a >= 0xA0000000u && a < 0xC0000000u); }
bool KillcamModelOk(uint8_t* base, uint32_t model)
{
    if (!GuestPtrOk(model)) return false;
    uint32_t part = 0;
    for (uint32_t k = 0; k < 8 && !part; k++) part = Guest::Read32(base, model + 0x24 + 4 * k);
    if (!GuestPtrOk(part)) return false;
    const uint32_t table = Guest::Read32(base, part + 8);
    if (!table) return true;   // no tags: the lookup stops there
    if (!GuestPtrOk(table)) return false;
    const uint32_t count = Guest::Read32(base, table + 0x10);
    if (count == 0) return true;
    return count <= 4096 && GuestPtrOk(Guest::Read32(base, table + 0x14));
}
// how many of the rig's models (from the first) can be looked through safely
uint32_t KillcamRigModelsOk(uint8_t* base, uint32_t rig)
{
    const uint32_t n = Guest::Read8(base, rig + 0xAA);
    for (uint32_t i = 0; i < n && i < 8; i++)
        if (!KillcamModelOk(base, Guest::Read32(base, rig + 0x80 + 4 * i))) return i;
    return n;
}
bool KillcamReplayingNow() { return Killcam::Replaying(); }
int BotsNumberOfClient(int client) { return (client >= 0 && client < int(MaxClients)) ? g_botNumber[client] : 0; }   // 0.5.490: spectator.cpp's names

namespace VehicleProbe
{
    void Dump(uint8_t* base, const char* what, uint32_t at, uint32_t bytes)
    {
        printf("vehprobe: %s %08X:", what, at);
        for (uint32_t o = 0; o < bytes; o += 4) { if (o % 64 == 0) printf("\n  +%04X", o); printf(" %08X", Guest::Read32(base, at + o)); }
        printf("\n");
    }
    void Frame(uint8_t* base)
    {
        static const bool on = []() { const char* t = getenv("COD3_VEHICLE_PROBE"); return t && t[0] == '1'; }();   /* 0.5.365: off unless =1 (vehicles fixed in 0.5.364) */
        if (!on) return;
        static uint64_t lastAt = 0; static int dumps = 0; static std::string map;
        const std::string now = Kernel::CurrentMap();
        if (now != map) { map = now; dumps = 0; }
        const uint64_t ms = GetTickCount64();
        if (dumps >= 24 || ms - lastAt < 1000) return;
        const uint32_t list = Guest::Read32(base, 0x82BCC7FCu);
        const uint32_t me = list ? Guest::Read32(base, list + 4) : 0;
        if (me < 0x40000000u || me >= 0xA0000000u) return;
        const float mx = ReadFloat(base, me + 336), my = ReadFloat(base, me + 340), mz = ReadFloat(base, me + 344);
        const uint32_t pool = Guest::Read32(base, 0x82BCBDB4u);
        const int records = int(Guest::Read16(base, 0x82C281D0u + 3088));
        for (int i = 0; pool && i < records && i < 64; i++)
        {
            const uint32_t record = pool + 1872u * uint32_t(i);
            const uint32_t handle = Guest::Read32(base, record + 368), index = handle & 0x3FFF;
            if (index >= 4096) continue;
            const uint32_t entry = 0x82BD1FC0u + 8 * index;
            if ((handle >> 14) != Guest::Read32(base, entry + 4)) continue;
            const uint32_t v = Guest::Read32(base, entry);
            if (v < 0x40000000u || v >= 0xA0000000u) continue;
            const float dx = ReadFloat(base, v + 336) - mx, dy = ReadFloat(base, v + 340) - my, dz = ReadFloat(base, v + 344) - mz;
            const float d = sqrtf(dx * dx + dy * dy + dz * dz);
            if (!(d < 160.0f)) continue;
            lastAt = ms; dumps++;
            // 0.5.362: 361's dumps (24 vs 32, Justin 23:17): at 24 your +900 (the
            // vehicle link) is set when you get in; at 32 it never is. Who
            // writes it (the way in) is caught here: a write watch on it, the
            // writer's guest call chain goes to the log ("watchpoint:").
            {
                static bool watched = false;
                if (!watched) { watched = true; Kernel::WatchWrite(me + 900); }
                // armed from a helper: ArmWatchpoints skips its own thread, and
                // this one (the server's) may be the writer
                const HANDLE helper = CreateThread(nullptr, 0, [](void*) -> DWORD { Kernel::ArmWatchpoints(); return 0; }, nullptr, 0, nullptr);
                if (helper) CloseHandle(helper);
            }
            const int kind = int(int16_t(Guest::Read16(base, record + 376)));
            const uint32_t def = kind >= 0 && kind < 32 ? Guest::Read32(base, 0x82B2DC90u + 4 * uint32_t(kind)) : 0;
            char name[40] = {}; for (int k = 0; def && k < 39; k++) { name[k] = char(Guest::Read8(base, def + k)); if (!name[k]) break; }
            const uint32_t rec = Guest::Read32(base, me + 512);
            printf("vehprobe: %d players (cap %u), you %.0f from %s (record %d), your +900 link %08X, health %d\n", int(Guest::Read32(base, 0x82C281D0u + 148)),
                   PlayerCapacity::Active(), d, name, i, Guest::Read32(base, me + 900), int32_t(Guest::Read32(base, me + 776)));
            printf("vehprobe: EntityList:"); for (uint32_t k = 0; k < 34; k++) printf(" %08X", Guest::Read32(base, list + 4 * k)); printf("\n");
            Dump(base, "your entity", me, 1024);
            if (rec >= 0x40000000u && rec < 0xA0000000u) Dump(base, "your record", rec, 1500);
            Dump(base, "vehicle entity", v, 1024);
            Dump(base, "vehicle record", record, 1872);
            fflush(stdout);
            return;
        }
    }
}

// 0.5.363: 362's watch (24 players, Justin 23:29): your vehicle link is
// written by sub_82571CE0 <- 825812D8 <- 825816E0 <- 82585540 <- 823FE1C0,
// and 823FE1C0 is run by the session's message pump (821BD8C0 -> 821BF1B0
// -> 821C3510 -> 823D8C08, the handler by message type): getting in is a
// session message ("player n wants seat s of vehicle v"). 823FE1C0 checks:
// the sender (823DCF70), the vehicle entry (session +0x59B0 + v*0x210,
// 823DD390), the player by number (823DD068: the roster entry, its client
// +8), the vehicle against the player (8255EFE0), the seat (823D9338), then
// 82585540 seats him. Each step's answer is logged here for the run at 32,
// to see which one says no. COD3_VEHICLE_PROBE=0: quiet.
namespace VehicleUse
{
    thread_local int g_in = 0;
    bool On() { static const bool on = []() { const char* t = getenv("COD3_VEHICLE_PROBE"); return t && t[0] == '1'; }();   /* 0.5.365: off unless =1 (vehicles fixed in 0.5.364) */ return on; }
    std::atomic<int> g_lines{0};
    bool Tell() { return On() && g_lines.fetch_add(1) < 400; }
}
extern "C" PPC_FUNC(__imp__sub_823FE1C0);
PPC_FUNC(sub_823FE1C0)
{
    const uint32_t session = ctx.r3.u32, msg = ctx.r4.u32;
    if (VehicleUse::Tell()) { printf("vehuse: seat request (823FE1C0) session %08X message %08X, players in play %u\n", session, msg, PlayerCapacity::Active()); fflush(stdout); }
    VehicleUse::g_in++;
    __imp__sub_823FE1C0(ctx, base);
    VehicleUse::g_in--;
    if (VehicleUse::Tell()) { printf("vehuse: seat request done\n"); fflush(stdout); }
}
#define VEHUSE_STEP(addr, label, args) \
extern "C" PPC_FUNC(__imp__sub_##addr); \
PPC_FUNC(sub_##addr) \
{ \
    if (!VehicleUse::g_in) { __imp__sub_##addr(ctx, base); return; } \
    const uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32, a5 = ctx.r5.u32; (void)a5; \
    __imp__sub_##addr(ctx, base); \
    if (VehicleUse::Tell()) { printf("vehuse:   " label " (" #addr ") r3 %08X r4 %08X r5 %08X -> %08X\n", a3, a4, a5, ctx.r3.u32); fflush(stdout); } \
}
VEHUSE_STEP(823DCF70, "sender in the roster", 2)
VEHUSE_STEP(823DD068, "player by number", 2)
VEHUSE_STEP(823DD390, "vehicle entry usable", 1)
// 0.5.840: logged outside the seat request too (no request was ever sent for a Sherman on Seelow:
// sherman3), with the vehicle's kind
extern "C" PPC_FUNC(__imp__sub_8255EFE0);
PPC_FUNC(sub_8255EFE0)
{
    const uint32_t a3 = ctx.r3.u32, a4 = ctx.r4.u32, a5 = ctx.r5.u32;
    __imp__sub_8255EFE0(ctx, base);
    if (VehicleUse::On())
    {
        static int told = 0;
        if (told++ < 300) { printf("vehuse:   vehicle vs player (8255EFE0)%s r3 %08X r4 %08X r5 %08X -> %08X\n", VehicleUse::g_in ? " [in request]" : "", a3, a4, a5, ctx.r3.u32); fflush(stdout); }
    }
}
VEHUSE_STEP(823D9338, "seat check", 3)
VEHUSE_STEP(82585540, "SEAT HIM", 3)
#undef VEHUSE_STEP

// 0.5.735: the host's own player, once a second of real time, in every game:
// "where: x y z | velocity x y z". FeedBots' "bots: host 0
// record" line follows the commands' clock and stopped after the first ~80 s of
// the user's plays, so their falls under Seelow and stuck places could not be
// found from the log (tools: route the "where:" lines against the map's ground).
static void LogHostWhere(uint8_t* base)
{
    static auto next = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    if (now < next) return;
    next = now + std::chrono::seconds(1);
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t ps = host ? Guest::Read32(base, host + 512) : 0;
    if (!ps) return;
    printf("where: %.1f %.1f %.1f | velocity %.1f %.1f %.1f\n",
           ReadFloat(base, ps), ReadFloat(base, ps + 4), ReadFloat(base, ps + 8),
           ReadFloat(base, ps + 16), ReadFloat(base, ps + 20), ReadFloat(base, ps + 24));
    fflush(stdout);
}

PPC_FUNC(sub_82535810)
{
    LogHostWhere(base);
    GunPointersCheck(base, "between server frames (the client's frame, menus, drawing)");
    KeepHostCurrent(base, "before the server frame");
    static uint64_t botsNs = 0, serverNs = 0, frames = 0, otherNs = 0;
    static auto lastEnd = std::chrono::steady_clock::now();
    static auto reportAt = std::chrono::steady_clock::now();
    const auto t0 = std::chrono::steady_clock::now();
    otherNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t0 - lastEnd).count());
    FeedBots(ctx, base);
    const auto t1 = std::chrono::steady_clock::now();
    GunPointersCheck(base, "FeedBots");
    WatchHostView(ctx, base);
    HostDeathCapture(base);   // 0.5.226
    WatchWar(ctx, base);   // 0.5.149
    WatchObjectives(ctx, base);   // 0.5.463
    WarReadout(base);   // 0.5.471
    GunPointersCheck(base, "WatchWar");
    {
        static bool registered = false;   // 0.5.641
        if (!registered) { registered = true; Overlay::SetMatchHud(Prematch::HudActive, Prematch::HudDraw); }
        Prematch::ServerFrame(ctx, base);
    }
    __imp__sub_82535810(ctx, base);
    {
        // 0.5.369: what the killcam's recorder costs each server frame
        static double recMs = 0; static uint32_t recN = 0; static uint64_t recTold = 0;
        const auto r0 = std::chrono::steady_clock::now();
        Killcam::ServerFrame(ctx, base);   // 0.5.329: the killcam's recorder
        recMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - r0).count(); recN++;
        const uint64_t nowMs = GetTickCount64();
        if (nowMs - recTold >= 10000 && recN) { printf("perf: killcam recorder %.2f ms a server frame (%u frames)\n", recMs / recN, recN); fflush(stdout); recTold = nowMs; recMs = 0; recN = 0; }
    }
    VehicleProbe::Frame(base);   // 0.5.361
    {
        const auto t2 = std::chrono::steady_clock::now();
        botsNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        serverNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count());
        frames++;
        if (t2 - reportAt >= std::chrono::seconds(5) && frames)
        {
            // 0.5.398: and how much of that the thread was really running
            // (its CPU time a frame): the rest it waited (for the command
            // thread's ring, the job workers, the frame pacing).
            static uint64_t lastCpu = 0;
            FILETIME created{}, exited{}, kernel{}, user{};
            double cpuMs = 0;
            if (GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user))
            {
                const uint64_t cpu = (uint64_t(kernel.dwHighDateTime) << 32 | kernel.dwLowDateTime) + (uint64_t(user.dwHighDateTime) << 32 | user.dwLowDateTime);
                if (lastCpu != 0 && cpu >= lastCpu) cpuMs = double(cpu - lastCpu) / 10000.0 / double(frames);
                lastCpu = cpu;
            }
            printf("perf: a game frame: bots %.2f ms (%llu traces a frame, %.2f ms of them), server frame %.2f ms, the rest (client, drawing) %.2f ms; %llu frames, %d bots; main thread CPU %.2f ms a frame\n",
                   botsNs / 1e6 / frames, (unsigned long long)(g_traces / frames), g_traceNs / 1e6 / frames, serverNs / 1e6 / frames, otherNs / 1e6 / frames,
                   (unsigned long long)frames, g_bots, cpuMs);
            fflush(stdout);
            botsNs = serverNs = otherNs = frames = 0; g_traces = g_traceNs = 0; reportAt = t2;
        }
        lastEnd = std::chrono::steady_clock::now();
    }
    GunPointersCheck(base, "the game's server frame (SV_Frame)");
    PhotoTour::Frame();   // 0.5.228: after the server frame (the camera's place held)
    BigTest::Frame();   // 0.5.239
    MpCapProbeTick();   // 0.5.239
    LobbyRulesTick(ctx, base);
    Domination::Frame(ctx, base);
    MpFixesFrame(base);
    MpFixesAbilities(ctx, base);
    MpFixesLauncher(ctx, base);
    GunPointersCheck(base, "lobby rules / mp fixes");
    SetUpBots(ctx, base);
    GunPointersCheck(base, "SetUpBots");
    {
        // The console's bot_add: once its bots are in, their sessions.
        static int seen = 0, wait = 0;
        const int asked = g_consoleBots.load();
        if (asked != seen && asked > 0 && g_bots >= asked)
        {
            // (0.5.60 gave them their sessions here, as debug.txt's
            // bot_session 2 does: in a devmap game that crashed the game a
            // little after. Left to debug.txt until the bots get their turn.)
            // 0.5.130: Justin, on 0.5.129 with debug.txt's bot_session 2 in
            // a System Link match: the bot is SEEN for the first time (he
            // fought, took damage, died and came back). The game draws and
            // updates a player (sub_82400810, from sub_82403840's walk over
            // the session's 24 players) only through his session player.
            // So each bot is now given one here, half a second after the
            // last is added, once the session exists (a devmap has none).
            if (++wait > 30)
            {
                seen = asked; wait = 0;
                if (Session(base) != 0 && g_botAutoSession.load()) SessionBots(ctx, base);
            }
        }
        else if (asked == 0) seen = 0;
    }
    GunPointersCheck(base, "session players");
    // 0.5.162: Justin, on 0.5.161: bot_add 20 said 20, but he saw 3. The
    // log: 14 were added (8 tries a request), and only the first 6 got a
    // session player, the thing that makes them drawn, because those are
    // given out only once ALL the bots asked for are in. Now any bot
    // without one is given one once he is half a second old.
    {
        static int every = 0;
        if (++every >= 30 && Session(base) != 0 && g_botAutoSession.load())
        {
            every = 0;
            bool missing = false;
            for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
                if (g_bot[slot] && g_age[slot] > 30 && SessionPlayer(base, slot) == 0) missing = true;
            if (missing) SessionBots(ctx, base);
        }
    }
    ThinkBots(ctx, base);
    GunPointersCheck(base, "ThinkBots");
    ReKitBots(ctx, base);
    GunPointersCheck(base, "ReKitBots");
    UpdateBotBodies(ctx, base);
    GunPointersCheck(base, "UpdateBotBodies");
    UavShowsBots(base);   // 0.5.167 (defined at the end of this file)
    SendBotRoster(base);        // 0.5.194: the host tells the others its bots
    SendBotStates(base);        // 0.5.207: and how each one's life goes
    MirrorHostBots(ctx, base);  // 0.5.194: a joiner puts the host's bots in its game
    PlayerLimitProbe(base);     // 0.5.197: is the space after the player states free?
    MapKnowledgeTick(base);     // 0.5.197: the bots learn the map
    CtfSpawnSides(ctx, base);   // 0.5.475: CTF's sides spawn by their own flag
    PlayerUnstick(ctx, base);   // 0.5.541: the player caught in the scenery is put back
    KeepHostCurrent(base, "after the server frame");
    // bot_session: log the session's players; 2 also gives each bot one and
    // spawns him again, this time the game's own way.
    static int sessionAsked = 0;
    const int session = DebugProbe::BotSession();
    if (session != sessionAsked)
    {
        sessionAsked = session;
        LogSession(base);
        // The host's player record and each bot's, word by word where they
        // differ, for finding what a bot is missing.
        {
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
            const uint32_t hostRecord = host ? Guest::Read32(base, host + 512) : 0;
            for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && hostRecord; slot++)
            {
                if (!g_bot[slot]) continue;
                const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
                const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
                if (!record) continue;
                printf("bots: record differences, host %08X, bot %u %08X\n", hostRecord, slot, record);
                for (uint32_t w = 0; w < 2400; w += 4)
                {
                    const uint32_t a = Guest::Read32(base, hostRecord + w), b = Guest::Read32(base, record + w);
                    if (a != b) printf("bots:   +%04u host %08X bot %08X\n", w, a, b);
                }
                printf("bots: entity differences, host %08X, bot %08X\n", host, entity);
                for (uint32_t w = 0; w < 900; w += 4)
                {
                    const uint32_t a = Guest::Read32(base, host + w), b = Guest::Read32(base, entity + w);
                    if (a != b) printf("bots:   +%04u host %08X bot %08X\n", w, a, b);
                }
                fflush(stdout);
            }
        }
        if (session == 2) SessionBots(ctx, base);
    }
    // A slot given back (the bot left with a map change or a restart).
    const uint32_t clients = Clients(base);
    if (clients == 0) return;
    for (uint32_t i = 0; i < ActiveSlots(); i++)
        if (g_bot[i] && Guest::Read32(base, clients + i * ClientSize) == 0) { g_bot[i] = false; g_bots--; }
    static int tries = 0;
    static int consoleAsked = -1;
    const int asked = g_consoleBots.load();
    if (asked != consoleAsked) { consoleAsked = asked; tries = 0; }
    const int wanted = std::max(DebugProbe::BotAdd(), asked);
    // 0.5.194: bots are the host's. A joiner's bot_add made bots only its
    // own game knew; now it is refused, and the host's bots come to it.
    if (wanted > g_bots && !ThisMachineHosts(base))
    {
        static uint32_t told = 0;
        if (GetTickCount() - told > 10000)
        {
            told = GetTickCount();
            printf("bots: only the host can add bots (this machine is session number %u): bot_add %d ignored\n",
                   BotRoster::LocalNumber(base), wanted);
            fflush(stdout);
        }
        return;
    }
    if (g_bots < wanted && tries < 64)   // 0.5.162: was 8 (bot_add 20 stopped at 14)
    {
        tries++;
        const bool added = AddBot(ctx, base);
        GunPointersCheck(base, "AddBot");
        if (!added) return;
    }
    if (wanted == 0) tries = 0;
}

PPC_FUNC(sub_825320D0)
{
    const uint32_t clients = Clients(base);
    if (clients != 0 && ctx.r3.u32 >= clients)
    {
        const uint32_t slot = (ctx.r3.u32 - clients) / ClientSize;
        if (slot < MaxClients && g_bot[slot]) return;   // no one to send to
    }
    __imp__sub_825320D0(ctx, base);
}


// The game's spawn of a player (sub_824D0AF0: the entity's handle, the spawn
// point's origin and angles, the side's name). Network-owned players normally
// route placement through the session to their owning machine. Host bots have
// no remote owner, so they use the offline placement path; joiner mirrors also
// use that local path after their roster entry identifies them as a host-owned
// bot, rather than sending a spawn back to the host.
PPC_FUNC(sub_824D0AF0)
{
    const uint32_t handle = ctx.r3.u32;
    const uint32_t index = handle & 0x3FFF;
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (index < 4096 && entities != 0)
    {
        const uint32_t record = 0x82BD1DC0 + 512 + index * 8;
        const uint32_t entity = Guest::Read32(base, record);
        if (entity != 0 && Guest::Read32(base, record + 4) == (handle >> 14))
        {
            for (uint32_t slot = 1; slot < ActiveSlots(); slot++)
            {
                if (Guest::Read32(base, entities + 4 * (slot + 1)) != entity) continue;
                const bool localBot = slot >= FirstBotSlot() && g_bot[slot];
                const uint32_t sessionPlayer = SessionPlayer(base, slot);
                const uint8_t sessionNumber = sessionPlayer ? Guest::Read8(base, sessionPlayer) : uint8_t(SessionMaxNow());
                const bool mirrorBot = sessionNumber < SessionMaxNow() && BotRoster::g_mirror[sessionNumber];
                if (!localBot && !mirrorBot) continue;
                if (localBot && sessionPlayer != 0)
                {
                    // With a session player of his own: the game's own way.
                    printf("bots: slot %u spawns the game's way\n", slot); fflush(stdout);
                    __imp__sub_824D0AF0(ctx, base);
                    LogWhere(base, "spawned", slot, entity);
                    return;
                }
                const uint32_t origin = ctx.r4.u32, angles = ctx.r5.u32;
                if (mirrorBot)
                {
                    printf("bots: joiner mirror slot %u uses local spawn path despite its remote session owner\n", slot);
                    fflush(stdout);
                }
                // The current-client field indexes four local clients, not
                // the server slot. Mirrors have no local place, so share the
                // known spare index 3 while placement/finalization runs.
                std::optional<BotClientScope> mirrorScope;
                if (mirrorBot) mirrorScope.emplace(base, slot, "mirror spawn", true);
                printf("bots: slot %u spawns at %.0f %.0f %.0f\n", slot, ReadFloat(base, origin),
                       ReadFloat(base, origin + 4), ReadFloat(base, origin + 8)); fflush(stdout);
                Call(ctx, base, ConnectIntoWorld, entity, origin, angles, 1, 0);
                // 0.5.117: network player setup (823F818C / 82400D40)
                // creates generic_human through the native model factory.
                // Script kit/model-definition setup alone does not create
                // this per-entity drawable object (entity+536).
                if (!Guest::Read32(base, entity + 536))
                {
                    const uint32_t human = Call(ctx, base, 0x824B5688, 0x820653E4);
                    if (human && Guest::Read32(base, human + 4))
                        Call(ctx, base, 0x8257C090, entity, human);
                    printf("bots: slot %u native generic_human %08X, drawable %08X\n",
                           slot, human, Guest::Read32(base, entity + 536));
                    fflush(stdout);
                }
                // 0.5.114: native offline spawn (82406BB8) follows placement
                // with callbacks+3312(handle*, 0). Placement alone clears
                // health and leaves the player inactive. Let the game's
                // spawn finalizer set state, health and loadout for this bot.
                // Respawn's BotClientScope keeps the current client isolated.
                const uint32_t callbacks = Guest::Read32(base, Callbacks);
                const uint32_t finalize = callbacks ? Guest::Read32(base, callbacks + 3312) : 0;
                const uint32_t scratch = (ctx.r1.u32 - 0x200) & ~0xFu;
                Guest::Write32(base, scratch, handle);
                printf("bots: slot %u spawn finalizer %08X\n", slot, finalize);
                fflush(stdout);
                if (finalize) Call(ctx, base, finalize, scratch, 0);
                // 0.5.125: the normal network-player setup also constructs
                // the animation component for the composed body (82400D40).
                // A model/rig alone leaves no component at rig+32. Use the
                // native constructor, which validates the model resource,
                // allocates private per-entity state and schedules its task.
                // bot_setup bit4 skips this step for an old/new comparison.
                const uint32_t rig=Guest::Read32(base,entity+492);
                if (!(g_botSetup.load() & 4) && rig && !Guest::Read32(base,rig+32))
                {
                    Call(ctx,base,0x8243F138,rig,0);
                    printf("bots: slot %u native animation rig %08X component %08X flags %08X\n",
                           slot,rig,Guest::Read32(base,rig+32),Guest::Read32(base,entity+624));
                    fflush(stdout);
                }
                LogWhere(base, "spawned", slot, entity);
                return;
            }
        }
    }
    __imp__sub_824D0AF0(ctx, base);
}

// ClientThink (sub_82592CD0) moves a player (Pmove, sub_824B4270) only when
// sub_82483190 says yes for his entity (its place, +876, in a part of the
// level marked; a player's machine has it so), or when his move type is 1 or
// 2. A bot's never was, so he could turn but never walk: for a bot, yes.
PPC_FUNC(sub_82483190)
{
    const uint32_t entity = ctx.r3.u32;
    __imp__sub_82483190(ctx, base);
    if (ctx.r3.u32 != 0) return;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && entities; slot++)
        if (g_bot[slot] && Guest::Read32(base, entities + 4 * (slot + 1)) == entity) { ctx.r3.u64 = 1; return; }
}

// Pmove (sub_824B4270; pm+0 the player's record, pm+4 his command, so its
// forward move at pm+28). Watched for a bot's record: whether it runs, his
// flags (+44; 0x4000 freezes him: his moves are cleared), +2048 (which sets
// that flag), and his forward move.
PPC_FUNC(sub_824B4270)
{
    const uint32_t pm = ctx.r3.u32;
    const uint32_t record = Guest::Read32(base, pm);
    bool bot = false;
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && entities; slot++)
    {
        const uint32_t entity = g_bot[slot] ? Guest::Read32(base, entities + 4 * (slot + 1)) : 0;
        if (entity && Guest::Read32(base, entity + 512) == record) bot = true;
    }
    static uint32_t count = 0;
    const bool say = bot && count++ % 60 == 0;
    if (say)
    {
        printf("bots: Pmove for a bot (%u): flags %08X, type %d, +2048 %d, forward %d right %d, buttons %X, time %d, last %d, ground %d, +36..+60:",
               count, Guest::Read32(base, record + 44), int32_t(Guest::Read32(base, record + 36)),
               int32_t(Guest::Read32(base, record + 2048)), int8_t(Guest::Read8(base, pm + 28)),
               int8_t(Guest::Read8(base, pm + 29)), Guest::Read32(base, pm + 8), int32_t(Guest::Read32(base, pm + 4)),
               int32_t(Guest::Read32(base, record + 32)), int32_t(Guest::Read32(base, record + 136)));
        for (uint32_t o = 36; o <= 60; o += 4) printf(" %08X", Guest::Read32(base, record + o));
        printf("\n");
        fflush(stdout);
    }
    // 0.5.641: the pre-match countdown: no moving, jumping, firing or throwing
    // for anyone (the command's buttons +8, its forward, right and up +28..+30)
    if (Prematch::Frozen())
    {
        Guest::Write32(base, pm + 8, 0);
        Guest::Write8(base, pm + 28, 0); Guest::Write8(base, pm + 29, 0); Guest::Write8(base, pm + 30, 0);
    }
    __imp__sub_824B4270(ctx, base);
    if (say)
    {
        printf("bots: after Pmove at %.1f %.1f, velocity %.1f %.1f\n", ReadFloat(base, record),
               ReadFloat(base, record + 4), ReadFloat(base, record + 16), ReadFloat(base, record + 20));
        fflush(stdout);
    }
}

// A player's muzzle point (sub_8256B8E0: his entity, the point out): the
// entity's origin raised by his record's eye height (+220). For a bot the
// eye height may never have been set, which put his shots at his feet; so
// invalid/unset eye height gets a fallback. Valid crouch/prone heights
// are preserved so the shot origin agrees with the brain's aiming origin.
PPC_FUNC(sub_8256B8E0)
{
    const uint32_t entity = ctx.r3.u32, out = ctx.r4.u32;
    __imp__sub_8256B8E0(ctx, base);
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && entities; slot++)
    {
        if (!g_bot[slot] || Guest::Read32(base, entities + 4 * (slot + 1)) != entity) continue;
        const float originZ = ReadFloat(base, entity + 344), muzzleZ = ReadFloat(base, out + 8);
        const uint32_t record = Guest::Read32(base, entity + 512);
        static uint32_t said = 0;
        if (said++ % 40 == 0)
        {
            printf("bots: slot %u muzzle %.1f above his origin (eye height %.1f)\n", slot, muzzleZ - originZ,
                   record ? ReadFloat(base, record + 220) : -1.0f);
            fflush(stdout);
        }
        const float eye = record ? ReadFloat(base,record+220) : 0.0f;
        if (std::isfinite(originZ) && (!std::isfinite(eye) || eye<4.0f || eye>80.0f))
        {
            const float raised = originZ + BotAim::EyeHeight(eye);
            uint32_t bits; memcpy(&bits, &raised, 4);
            Guest::Write32(base, out + 8, bits);
        }
        break;
    }
}

// 0.5.198: a scoped rifle, by its name (def +8): the snipers' and the
// scoped FG42. 0.5.197 went by the scope overlay field (+0x650), but every
// weapon has one (most point at an empty name), so every gun walked slow.
bool BotsScopedWeapon(uint8_t* base, uint32_t def)
{
    if (def < 0x40000000u || def >= 0xA0000000u) return false;
    const uint32_t text = Guest::Read32(base, def + 8);
    if (text < 0x40000000u || text >= 0xA0000000u) return false;
    char name[32] = {};
    for (int c = 0; c < 31; c++) { const char ch = char(tolower(Guest::Read8(base, text + c))); if (!ch) break; name[c] = ch; }
    return strstr(name, "sniper") || strstr(name, "scoped") || strcmp(name, "springfield") == 0;
}

// The move's scale for a command (sub_82487718: the larger of its forward,
// side and up moves, against the player's speed). Only the bot's commands
// have a forward of exactly 124 (bot_sticks' 32000), so those are counted,
// for telling whether his walking move is reached at all.
// Aiming down the sights (the record's +172, 0 to 1): with Faster aiming
// walk a move while the sights are up goes at the Aiming walk speed setting
// (per cent of the hip walk; the game's own is 40).
PPC_FUNC(sub_82487718)
{
    const bool bots = Guest::Read8(base, ctx.r3.u32 + 24) == 124;
    __imp__sub_82487718(ctx, base);
    // 0.5.197: a scoped rifle (its weapon has a scope overlay, +0x650) walks
    // at the Scoped walk speed (30% of the hip walk by default, slower than
    // the game's 40%), whatever Faster aiming walk says.
    bool scoped = false;
    {
        const uint32_t move = Guest::Read32(base, 0x82B922C8);
        const uint32_t record = move ? Guest::Read32(base, move) : 0;
        const uint32_t weapon = record ? Guest::Read32(base, record + 164) : 0;
        const uint32_t list = Guest::Read32(base, 0x82BCBF9C);
        const uint32_t def = (weapon && weapon < 128 && list) ? Guest::Read32(base, list + 4 * weapon) : 0;
        scoped = BotsScopedWeapon(base, def);   // 0.5.198: by name (0.5.197's overlay test matched every gun)
    }
    if (Settings::Get().fasterAds || scoped)
    {
        const uint32_t move = Guest::Read32(base, 0x82B922C8);
        const uint32_t record = move ? Guest::Read32(base, move) : 0;
        if (record)
        {
            auto fl = [&](uint32_t at) { const uint32_t b = Guest::Read32(base, record + at); float f; memcpy(&f, &b, 4); return f; };
            // The sights' fraction is +172 (0 down, 1 fully up: seen with the
            // aiming probe, mp_fixes.cpp).
            const float sights = fl(172);
            if (sights > 0.01f && sights <= 1.0f)
            {
                const double before = ctx.f1.f64;
                // +1124 is the game's own aimed walk (0.4 of the hip walk);
                // the setting is the share wanted instead.
                float game = fl(1124);
                if (!(game > 0.05f && game < 1.0f)) game = 0.4f;
                const double wanted = (scoped ? Settings::Get().sniperWalk : Settings::Get().aimWalk) / 100.0;
                const double factor = scoped ? wanted / game : (wanted > game ? wanted / game : 1.0);
                const double quicker = before * (1.0 + (factor - 1.0) * sights);
                ctx.f1.f64 = quicker;
                static int said = 0;
                if ((said < 6 && said++ < 6)) { printf("moves: aiming %.2f, scale %.3f -> %.3f (+1124 %.3f, +1128 %.3f)\n", sights, before, quicker, fl(1124), fl(1128)); fflush(stdout); }
            }
        }
    }
    if (!bots) return;
    static uint32_t count = 0;
    if (count++ % 120 == 0)
    {
        printf("bots: a bot's move scale %.3f (%u times)\n", ctx.f1.f64, count); fflush(stdout);
    }
}

#endif

#ifdef COD3_MP
// 0.5.113 camera boundary diagnostics: observe native results, never force
// the host FOV or ADS. Generated 82453810/824674E0 verify these exact fields.
extern "C" PPC_FUNC(__imp__sub_82453810);
PPC_FUNC(sub_82453810)
{
    const uint32_t client=Guest::Read32(base,0x82BCBE9C);
    __imp__sub_82453810(ctx,base);
    static uint32_t logged[MaxClients]={};
    const uint32_t now=GetTickCount();
    if (client>=MaxClients || now-logged[client]<2000) return;
    logged[client]=now;
    const uint32_t entities=Guest::Read32(base,EntityList);
    const uint32_t entity=entities?Guest::Read32(base,entities+4*(client+1)):0;
    const uint32_t record=entity?Guest::Read32(base,entity+512):0;
    printf("bots: native FOV client %u => %.2f (base %.2f), ADS %.3f, flags %08X, bots %d\n",client,ctx.f1.f64,ReadFloat(base,BaseFov),
           record?ReadFloat(base,record+172):-1.0f,record?Guest::Read32(base,record+240):0,g_bots);
    fflush(stdout);
}
// 0.5.321 KILLCAM PROBE (diagnostic, read-only; COD3_KILLCAM_PROBE=0 off):
// what the view code is made of, before the killcam is built on it.
// - the view mode sub_82462C68 returns (0..21, the jump table in
//   sub_824674E0) each time it changes, with the local player's health;
// - the camera object (r3, +480 its client) as floats +0..+604, with the
//   player's origin (record +0) and view angles (+204/+208), every 3 s and
//   at each mode change, to find the camera's origin and angles;
// - every 5 s, for each other player, where his origin appears in the
//   client's globals (0x82C00000..0x82D00000): the client-side entities.
namespace
{
    bool KillcamProbeOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_PROBE"); return t && t[0] == '1'; }(); return on; }   // 0.5.322: off unless =1
    bool KillcamEntityProbeOn() { static const bool on = []() { const char* t = getenv("COD3_KILLCAM_ENTITY_PROBE"); return !(t && t[0] == '0'); }(); return on; }   // 0.5.325: on (find the client's entities)
    uint32_t g_kcMode = 0xFFFFFFFFu, g_kcModeAt = 0;
    void KillcamProbeCamera(uint8_t* base, uint32_t camera, bool force)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t me = entities ? Guest::Read32(base, entities + 4) : 0;
        const uint32_t rec = me ? Guest::Read32(base, me + 512) : 0;
        std::string line;
        char t[40];
        for (uint32_t at = 0; at <= 604; at += 4) { snprintf(t, sizeof t, " %u=%.2f", at, ReadFloat(base, camera + at)); line += t; }
        printf("killcam probe: camera %08X mode %u | player origin %.2f %.2f %.2f view %.2f %.2f health %d | entity origin %.2f %.2f %.2f angles %.2f %.2f |%s\n",
               camera, g_kcMode, rec ? ReadFloat(base, rec) : 0.0f, rec ? ReadFloat(base, rec + 4) : 0.0f, rec ? ReadFloat(base, rec + 8) : 0.0f,
               rec ? ReadFloat(base, rec + 204) : 0.0f, rec ? ReadFloat(base, rec + 208) : 0.0f, me ? int32_t(Guest::Read32(base, me + 776)) : -1,
               me ? ReadFloat(base, me + 336) : 0.0f, me ? ReadFloat(base, me + 340) : 0.0f, me ? ReadFloat(base, me + 344) : 0.0f,
               me ? ReadFloat(base, me + 352) : 0.0f, me ? ReadFloat(base, me + 356) : 0.0f, line.c_str());
        (void)force;
        fflush(stdout);
    }
    void KillcamProbeEntities(uint8_t* base)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!entities) return;
        int scanned = 0;   // 0.5.325: three a time (a 6 MB scan each)
        for (uint32_t c = 0; c < 32 && scanned < 3; c++)
        {
            const uint32_t e = Guest::Read32(base, entities + 4 * (c + 1));
            if (!e || int32_t(Guest::Read32(base, e + 776)) <= 0) continue;
            scanned++;
            const float x = ReadFloat(base, e + 336), y = ReadFloat(base, e + 340), z = ReadFloat(base, e + 344);
            if (fabsf(x) < 1.0f && fabsf(y) < 1.0f) continue;
            std::string hits;
            int n = 0;
            // 0.5.325: finite values only (322's scan matched NaN everywhere), the
            // whole static area and the heap's first part
            for (uint32_t a = 0x82A00000u; a < 0x83000000u - 12 && n < 16; a += 4)
            {
                const float fx = ReadFloat(base, a);
                if (!std::isfinite(fx) || !(fabsf(fx - x) <= 3.0f)) continue;
                const float fy = ReadFloat(base, a + 4), fz = ReadFloat(base, a + 8);
                if (!std::isfinite(fy) || !std::isfinite(fz) || !(fabsf(fy - y) <= 3.0f) || !(fabsf(fz - z) <= 12.0f)) continue;
                char t[16]; snprintf(t, sizeof t, " %08X", a); hits += t; n++;
            }
            printf("killcam probe: client %u entity %08X at %.1f %.1f %.1f found in the client's globals at%s\n", c, e, x, y, z, hits.empty() ? " (nowhere)" : hits.c_str());
        }
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82462C68);
PPC_FUNC(sub_82462C68)
{
    const uint32_t camera = ctx.r3.u32;
    __imp__sub_82462C68(ctx, base);
    if (!camera || Guest::Read32(base, camera + 480) != 0) return;
    const uint32_t mode = ctx.r3.u32;
    if (mode != g_kcMode && !KillcamProbeOn()) { g_kcMode = mode; g_kcModeAt = GetTickCount(); return; }   // 0.5.322: kept for the killcam
    if (mode != g_kcMode)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t me = entities ? Guest::Read32(base, entities + 4) : 0;
        printf("killcam probe: view mode %u -> %u (health %d, at %u ms)\n", g_kcMode, mode, me ? int32_t(Guest::Read32(base, me + 776)) : -1, GetTickCount());
        fflush(stdout);
        g_kcMode = mode;
        g_kcModeAt = GetTickCount();
    }
}
// 0.5.703: a line through the world for the flamethrower (mp_fixes.cpp): how
// far along from -> to it runs before something solid (1: all the way, or
// no answer), the ignored entity's own body left out
float BotsFlameTrace(PPCContext& ctx, uint8_t* base, uint32_t ignored, const float from[3], const float to[3])
{
    using namespace BotNavigation;
    const Hit h = BotTrace(ctx, base, ignored, ignored, { from[0], from[1], from[2] }, { to[0], to[1], to[2] }, false);
    if (!h.valid || !std::isfinite(h.fraction)) return 1.0f;
    return std::clamp(h.fraction, 0.0f, 1.0f);
}
extern "C" PPC_FUNC(__imp__sub_824674E0);
PPC_FUNC(sub_824674E0)
{
    const uint32_t camera=ctx.r3.u32;
    __imp__sub_824674E0(ctx,base);
    if (KillcamProbeOn() && camera && Guest::Read32(base,camera+480)==0)
    {
        static uint32_t lastDump=0, lastScan=0, dumpsAfterChange=0, changeSeen=0;
        const uint32_t now=GetTickCount();
        if (changeSeen!=g_kcModeAt) { changeSeen=g_kcModeAt; dumpsAfterChange=3; }
        if (now-lastDump>=3000 || (dumpsAfterChange && now-lastDump>=400)) { lastDump=now; if (dumpsAfterChange) dumpsAfterChange--; KillcamProbeCamera(base,camera,false); }
        if (now-lastScan>=5000) { lastScan=now; KillcamProbeEntities(base); }
    }
    else if (KillcamEntityProbeOn() && camera && Guest::Read32(base,camera+480)==0)
    {
        static uint32_t lastScan2=0;
        const uint32_t now=GetTickCount();
        if (now-lastScan2>=10000) { lastScan2=now; KillcamProbeEntities(base); }
    }
    if (camera && Guest::Read32(base,camera+480)==0) Killcam::Frame(base,camera,g_kcMode);   // 0.5.322
}
// 0.5.323: the view's axes from its angles, at the end of sub_824674E0 (not
// in view modes 16 and 18): the killcam's view goes in just before.
extern "C" PPC_FUNC(__imp__sub_82463150);
// 0.5.327: the killcam replay's two hooks (Killcam above): cgame's
// read-the-next-snapshot (CG_ReadNextSnapshot: returns the snapshot it
// filled, cg+48 or cg+1584) and its player-state interpolation (the
// no-prediction path a followed player takes).
// 0.5.334: whether cgame's player drawing is asked to draw the replayed you
// 0.5.337: counts per killcam frame: entities through the type switch
// (8246B5B8), players drawn (CG_Player 82469968), and whether you were
// among either. 336's log: you are in the packet list and pass its test,
// yet the player drawing never had you.
// 0.5.345: KILLCAM STUDY (no play change; COD3_KILLCAM_STUDY=0 off). Justin
// asked for a true recording, as Call of Duty 4's killcam: what the killer's
// machine saw, replayed by the game's own drawing code. On this machine
// cgame reads the server's memory directly (no network snapshot to save:
// CL_ParseSnapshot 82528258 copies the ps from the server record;
// CG_AddPacketEntities 8246BED8 walks the server's entity list at
// 0x82BD9EC4, count at 0x82BE9FC4). So the recording must hold exactly what
// cgame reads there, and nothing is to be guessed: this build measures it.
// For a few cgame frames (CG_DrawActiveFrame 8246C748 and the scene entry
// 82448A18) the pages of every listed entity, every client record, rig and
// svEntity are closed; each access faults, is counted as (kind, offset,
// read/write, guest function), the page is opened for that one instruction
// and closed again. Twice: once in normal play (alive, 30 s in, bots in),
// once during a killcam. Then the log lists, per kind, the byte ranges
// cgame read and wrote, the functions that did, and the entities' spacing.
namespace KillcamStudy
{
    struct Region { uint32_t start, end; int kind; };   // guest [start, end)
    enum Kind { KEntPlayer, KEntOther, KRecord, KRig, KSv, KCount };
    const char* const KindNames[KCount] = { "player entity", "other entity", "client record", "rig", "svEntity" };
    std::vector<Region> g_regions;                      // sorted by start
    std::vector<uint32_t> g_pages;                      // closed guest pages
    std::map<uint64_t, uint32_t> g_hits;                // kind<<40 | offset<<8 | write -> count
    std::map<uint64_t, uint32_t> g_funcs;               // kind<<40 | function<<1 | write -> count
    std::map<uint32_t, uint32_t> g_types;               // entity type -> how many listed
    thread_local uintptr_t t_rearm = 0;
    DWORD g_thread = 0;
    bool g_armed = false;
    bool (*g_prevHook)(void*) = nullptr;
    uint32_t g_faults = 0, g_otherThread = 0, g_frames = 0;
    std::mutex g_lock;
    bool On() { static const bool on = [] { const char* t = getenv("COD3_KILLCAM_STUDY"); return t && t[0] == '1'; }(); return on; }   // 0.5.346: measured in 345; now only with COD3_KILLCAM_STUDY=1

    bool Hook(void* pointers)
    {
        auto* info = static_cast<EXCEPTION_POINTERS*>(pointers);
        const auto* record = info->ExceptionRecord;
        if (record->ExceptionCode == EXCEPTION_SINGLE_STEP && t_rearm)
        {
            DWORD was = 0;
            if (g_armed) VirtualProtect(reinterpret_cast<void*>(t_rearm), 4096, PAGE_NOACCESS, &was);
            t_rearm = 0;
            return true;
        }
        if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2)
            return g_prevHook ? g_prevHook(pointers) : false;
        const uintptr_t at = uintptr_t(record->ExceptionInformation[1]);
        if (at < uintptr_t(Guest::Base) || at >= uintptr_t(Guest::Base) + 0x100000000ull) return g_prevHook ? g_prevHook(pointers) : false;
        const uint32_t guest = uint32_t(at - uintptr_t(Guest::Base));
        const uint32_t page = guest & ~4095u;
        if (!std::binary_search(g_pages.begin(), g_pages.end(), page)) return g_prevHook ? g_prevHook(pointers) : false;
        const bool write = record->ExceptionInformation[0] == 1;
        if (GetCurrentThreadId() == g_thread)
        {
            // the region whose start is the nearest at or before the address
            auto it = std::upper_bound(g_regions.begin(), g_regions.end(), guest, [](uint32_t a, const Region& r) { return a < r.start; });
            if (it != g_regions.begin())
            {
                --it;
                if (guest < it->end)
                {
                    const uint32_t function = Sampler::FunctionAt(info->ContextRecord->Rip);
                    std::lock_guard<std::mutex> hold(g_lock);
                    g_faults++;
                    g_hits[(uint64_t(it->kind) << 40) | (uint64_t(guest - it->start) << 8) | (write ? 1 : 0)]++;
                    g_funcs[(uint64_t(it->kind) << 40) | (uint64_t(function) << 1) | (write ? 1 : 0)]++;
                }
            }
        }
        else g_otherThread++;
        DWORD was = 0;
        VirtualProtect(Guest::Base + page, 4096, PAGE_READWRITE, &was);
        t_rearm = uintptr_t(Guest::Base + page);
        info->ContextRecord->EFlags |= 0x100;
        return true;
    }

    void AddRegion(uint32_t start, uint32_t size, int kind)
    {
        if (start < 0x40000000u || start >= 0xA0000000u) return;
        g_regions.push_back({ start, start + size, kind });
        for (uint32_t p = start & ~4095u; p < start + size; p += 4096) g_pages.push_back(p);
    }

    void Arm(uint8_t* base)
    {
        g_regions.clear(); g_pages.clear();
        const uint32_t count = Guest::Read32(base, 0x82BE9FC4u), list = 0x82BD9EC4u;
        const uint32_t entities = Guest::Read32(base, EntityList);
        std::vector<uint32_t> ptrs;
        for (uint32_t i = 0; i < count && i < 2048; i++)
        {
            const uint32_t e = Guest::Read32(base, list + 4 * i);
            if (e < 0x40000000u || e >= 0xA0000000u) continue;
            ptrs.push_back(e);
            const uint8_t type = Guest::Read8(base, e);
            if (g_frames == 0) g_types[type]++;
            AddRegion(e, 0x430, type == 1 ? KEntPlayer : KEntOther);
        }
        for (uint32_t c = 0; entities && c < 32; c++)
        {
            const uint32_t e = Guest::Read32(base, entities + 4 * (c + 1));
            if (!e) continue;
            AddRegion(Guest::Read32(base, e + 512), 2992, KRecord);
            const uint32_t rig = Guest::Read32(base, e + 492);
            AddRegion(rig, 0x100, KRig);
            AddRegion(Guest::Read32(base, e + 532), 0x110, KSv);
        }
        std::sort(g_regions.begin(), g_regions.end(), [](const Region& a, const Region& b) { return a.start < b.start; });
        std::sort(g_pages.begin(), g_pages.end());
        g_pages.erase(std::unique(g_pages.begin(), g_pages.end()), g_pages.end());
        if (g_frames == 0)
        {
            std::sort(ptrs.begin(), ptrs.end());
            std::map<uint32_t, uint32_t> gaps;
            for (size_t i = 1; i < ptrs.size(); i++) gaps[ptrs[i] - ptrs[i - 1]]++;
            printf("killcam study: %u listed entities, %zu pages closed; entity types:", count, g_pages.size());
            for (const auto& [t, n] : g_types) printf(" %u x%u", t, n);
            printf("\nkillcam study: entity spacing (gap x count):");
            int shown = 0;
            for (const auto& [g, n] : gaps) if (shown++ < 12) printf(" %X x%u", g, n);
            printf(" | first %08X last %08X\n", ptrs.empty() ? 0 : ptrs.front(), ptrs.empty() ? 0 : ptrs.back());
            fflush(stdout);
        }
        g_thread = GetCurrentThreadId();
        if (Kernel::FaultHook != Hook) { g_prevHook = Kernel::FaultHook; Kernel::FaultHook = Hook; }
        g_armed = true;
        DWORD was = 0;
        for (uint32_t p : g_pages) VirtualProtect(Guest::Base + p, 4096, PAGE_NOACCESS, &was);
    }

    void Disarm()
    {
        if (!g_armed) return;
        g_armed = false;
        DWORD was = 0;
        for (uint32_t p : g_pages) VirtualProtect(Guest::Base + p, 4096, PAGE_READWRITE, &was);
    }

    void Report(const char* label)
    {
        std::lock_guard<std::mutex> hold(g_lock);
        printf("killcam study (%s): %u frames, %u accesses counted, %u by other threads (not counted)\n", label, g_frames, g_faults, g_otherThread);
        for (int k = 0; k < KCount; k++)
            for (int w = 0; w < 2; w++)
            {
                // ranges of offsets, coalesced (a gap under 4 bytes joins them)
                std::vector<std::pair<uint32_t, uint32_t>> ranges;
                for (const auto& [key, n] : g_hits)
                {
                    if (int(key >> 40) != k || int(key & 1) != w) continue;
                    const uint32_t off = uint32_t((key >> 8) & 0xFFFFFFFFull);
                    if (!ranges.empty() && off <= ranges.back().second + 4) ranges.back().second = std::max(ranges.back().second, off);
                    else ranges.push_back({ off, off });
                }
                if (ranges.empty()) continue;
                printf("killcam study (%s): %s %s:", label, KindNames[k], w ? "WRITTEN" : "read");
                for (const auto& [a, b] : ranges) { if (a == b) printf(" +%u", a); else printf(" +%u..%u", a, b); }
                printf("\n");
                printf("killcam study (%s): %s %s by:", label, KindNames[k], w ? "written" : "read");
                for (const auto& [key, n] : g_funcs)
                    if (int(key >> 40) == k && int(key & 1) == w) printf(" sub_%08X x%u", uint32_t((key >> 1) & 0xFFFFFFFFull), n);
                printf("\n");
            }
        fflush(stdout);
        g_hits.clear(); g_funcs.clear(); g_types.clear(); g_faults = g_otherThread = 0; g_frames = 0;
    }

    // which study runs now: 0 none, 1 normal play, 2 killcam
    int g_which = 0, g_left = 0;
    bool g_doneLive = false, g_doneKillcam = false;
    uint32_t g_aliveSince = 0;
    // called at each scope (CG_DrawActiveFrame / scene); true = armed for it
    bool Begin(uint8_t* base)
    {
        if (!On() || g_armed) return false;
        if (g_which == 0)
        {
            const uint32_t entities = Guest::Read32(base, EntityList);
            const uint32_t own = entities ? Guest::Read32(base, entities + 4) : 0;
            const bool alive = own && int32_t(Guest::Read32(base, own + 776)) > 0;
            const uint32_t now = GetTickCount();
            if (!alive) g_aliveSince = 0; else if (!g_aliveSince) g_aliveSince = now;
            if (!g_doneKillcam && Killcam::Replaying() && Killcam::Ms() - Killcam::g_playStart > 1000) { g_which = 2; g_left = 6; printf("killcam study: measuring during a killcam (6 scopes)\n"); fflush(stdout); }
            else if (!g_doneLive && alive && g_bots >= 4 && now - g_aliveSince > 30000) { g_which = 1; g_left = 6; printf("killcam study: measuring normal play (6 scopes)\n"); fflush(stdout); }
            else return false;
        }
        Arm(base);
        return true;
    }
    void End()
    {
        Disarm();
        g_frames++;
        if (--g_left > 0) return;
        if (g_which == 1) { g_doneLive = true; Report("normal play"); }
        else { g_doneKillcam = true; Report("killcam"); }
        g_which = 0;
    }
}

namespace KillcamCount { uint32_t sw = 0, swYou = 0, players = 0, playersYou = 0, frames = 0; uint64_t toldFor = 0; uint8_t youType = 0; }
extern "C" PPC_FUNC(__imp__sub_8246B5B8);
PPC_FUNC(sub_8246B5B8)
{
    if (Killcam::g_listAt)
    {
        KillcamCount::sw++;
        if (ctx.r3.u32 == Killcam::g_ownEnt) { KillcamCount::swYou++; KillcamCount::youType = Guest::Read8(base, ctx.r3.u32); }
    }
    __imp__sub_8246B5B8(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_82469968);
PPC_FUNC(sub_82469968)
{
    Killcam::NoteDrawn(base, ctx.r3.u32);   // 0.5.368: the body watch only judges bodies cgame draws
    // 0.5.352: 351's guard caught the freeze's cause: the scene's model list
    // looped on one node pointing at itself, holding a player's model
    // instance (node +0x5C = 8299DA10, a player rig): that body was added to
    // the scene twice in one frame. In the replay the killer's entity sits
    // in this machine's place and is drawn as the view (his gun and arms,
    // from his own model instance); his body must not be drawn as well (a
    // spectator following him never sees his body). So CG_Player is not
    // run for him while the killcam plays.
    // 0.5.356: OFF: 355's video showed no gun in the killer's view since 352 (CG_Player
    // readies his entity for the view weapon too); the double add is refused at the
    // list now (SceneAddGuard). COD3_KILLCAM_SKIP_KILLER=1 brings it back.
    static const bool skipKiller = []() { const char* v = getenv("COD3_KILLCAM_SKIP_KILLER"); return v && v[0] == '1'; }();
    if (skipKiller && Killcam::g_listAt && Killcam::Replaying() && ctx.r3.u32 == Killcam::g_killerEnt)
    {
        static uint32_t skipped = 0;
        if (skipped++ < 3) { printf("killcam: the killer's own body not drawn (it is the view)\n"); fflush(stdout); }
        return;
    }
    if (Killcam::g_listAt)
    {
        KillcamCount::players++;
        if (ctx.r3.u32 == Killcam::g_ownEnt)
        {
            KillcamCount::playersYou++;
            // 0.5.343: Justin, 342: still lying in the killcam. Your body's
            // pose is evaluated here from the replayed you (your recorded
            // entity: alive, running), as the bots' bodies are each frame
            // (8251ED08), before cgame draws it. Logged before/after.
            const uint32_t rig = Guest::Read32(base, ctx.r3.u32 + 492);
            if (!Killcam::OwnPoseOn() && rig >= 0x40000000u && rig < 0xA0000000u && Guest::Read32(base, rig + 172) == ctx.r3.u32 &&
                Guest::Read32(base, rig + 112) && Guest::Read32(base, rig + 128) && Guest::Read32(base, rig + 32))
            {
                static int toldEv = 0;
                if (toldEv < 4) Killcam::LogBone(base, "you in the replay, before", rig);
                // 0.5.344: the recorded (moving) pose; 343's evaluation only when none
                const bool put = Killcam::PutOwnPose(base, ctx.r3.u32);
                // 0.5.349: never evaluated while the rig holds the replay's buffer
                if (!put && !Killcam::g_saved[0].rigAt) Call(ctx, base, 0x8251ED08u, rig, 0xFFFFFFFFu);
                if (toldEv < 4) { toldEv++; Killcam::LogBone(base, put ? "you in the replay, recorded pose put" : "you in the replay, evaluated (no recorded pose)", rig); }
            }
            static int told = 0;
            if ((told < 6 && told++ < 6)) { printf("killcam: cgame draws the replayed you (entity %08X, +16 %08X)\n", ctx.r3.u32, Guest::Read32(base, ctx.r3.u32 + 16)); fflush(stdout); }
        }
    }
    __imp__sub_82469968(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_8246C748);
PPC_FUNC(sub_8246C748)   // 0.5.328: CG_DrawActiveFrame: the killcam's swap
{
    const bool mine = !Killcam::g_swapped && !Killcam::g_listAt;   // 0.5.335: not inside the other swap
    if (mine) Killcam::SwapIn(base);
    const bool spec = mine && !Killcam::g_swapped && Spectator::SwapIn(base);   // 0.5.490: following from his eyes
    const bool study = KillcamStudy::Begin(base);   // 0.5.345
    __imp__sub_8246C748(ctx,base);
    if (study) KillcamStudy::End();
    if (spec) Spectator::SwapOut(base);
    if (mine) Killcam::SwapOut(base);
}
// 0.5.335: 334's log: the killer turned his aim onto you (3 deg off, 360
// away) and you were never drawn ("cgame draws the replayed you" never
// came). FOUND: the players' bodies are added in another of cgame's entry
// points, its scene (cgame export table 0x82DD54B8 +60 = 82448A18 ->
// 824483C0 -> CG_AddPacketEntities 8246BED8), outside CG_DrawActiveFrame,
// where 332-334 swapped. There everyone was where he is now and you were
// still this machine's (dead) player, left out. The same swap now covers it.
// 0.5.350: GUARD + EVIDENCE for the killcam freeze. 347 and 349 froze in a
// killcam with cgame's main thread walking, forever, the scene's list of model
// instances (8243E2B8 <- 82446170: head word 0x825FFB74, next pointer at a
// node's +0, null-terminated): a node had been linked into it twice, making a
// loop. Before each walk the list is checked (up to 20000 nodes); a loop is
// cut where it closes (its last node's next set to 0) and logged with the
// node, what it holds (+0x5C, +0x60) and which entity (if any) owns it as its
// model instance (+492), so the cause can be found and fixed for real.
// 0.5.354: THE LOOP'S ROOT, at the door. The scene's model lists (0x825FFB70:
// list A count +0, ends at +8/+0xC; list B count +0x10, ends at +0x18/+0x1C)
// take a model instance at their tail: node (r3 - 8) next = the list's end,
// node prev = the tail, tail next = node, tail = node. Taking the node that
// is ALREADY the tail makes node next = node: 352/353's guard saw exactly
// that (a node following itself, +0x60 = its entity, e.g. 82A4C410). So the
// add is refused when the node is already the tail (the same model added
// twice in a row in the replay), logged with its entity and the caller.
static void SceneAddGuard(uint8_t* base, uint32_t node, uint32_t tailAt, const char* list, uint32_t lr, bool& skip)
{
    skip = false;
    if (node < 0x40000000u) return;
    // 0.5.418: the tail check always, killcam or not. 416's two-player test
    // froze on the frame the killcam ended ("killcam: over", then the main
    // thread in 8243E198 for ever with model instance 5F586190, the one the
    // guard had refused a dozen times in the replay): the same double add
    // came once more with Replaying() already false, unguarded, and the node
    // followed itself. A double add at the tail is what makes the loop, and
    // checking for it is one read. COD3_SCENETAILGUARD=0: replay only, as before.
    static const bool tailAlways = []() { const char* t = getenv("COD3_SCENETAILGUARD"); return !(t && t[0] == '0'); }();
    bool present = Guest::Read32(base, tailAt) == node;
    if (!present || !tailAlways)
    {
        // 0.5.369: only while the killcam plays (every double add it ever caught
        // was in the replay: 367's log, 12 of 12). Walking the whole list on
        // every add, every frame, costs time with 32 players' worth of models.
        if (!Killcam::Replaying()) return;
        // 0.5.356: refused when the node is ANYWHERE in the list already (at the
        // tail it makes a loop; elsewhere it cuts off everything after it)
        const uint32_t end = tailAt - 4;   // the list's end node (its next word); tail = end + 4
        for (uint32_t n = Guest::Read32(base, end), k = 0; !present && n && n != end && k < 4096; n = Guest::Read32(base, n), k++)
            if (n == node) present = true;
        if (!present) return;
    }
    skip = true;
    // 0.5.418: those outside the replay counted apart, so the replay's dozen
    // never hide the one that used to freeze the game
    static int told = 0, toldOutside = 0;
    int& count = Killcam::Replaying() ? told : toldOutside;
    if (count < 12)
    {
        count++;
        const uint32_t e = Guest::Read32(base, node + 0x60);
        printf("killcam guard: model instance %08X added to scene list %s twice in a row (caller %08X; entity %08X type %u +484 %08X; killcam %s): second add skipped\n",
               node, list, lr, e, (e >= 0x40000000u && e < 0xA0000000u) ? Guest::Read8(base, e) : 0u, (e >= 0x40000000u && e < 0xA0000000u) ? Guest::Read32(base, e + 484) : 0u,
               Killcam::Replaying() ? "playing" : "not playing");
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82433498);
PPC_FUNC(sub_82433498)
{
    bool skip; SceneAddGuard(base, ctx.r3.u32 - 8, 0x825FFB7Cu, "A", uint32_t(ctx.lr), skip);
    if (skip) return;
    __imp__sub_82433498(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82433440);
PPC_FUNC(sub_82433440)
{
    bool skip; SceneAddGuard(base, ctx.r3.u32 - 8, 0x825FFB8Cu, "B", uint32_t(ctx.lr), skip);
    if (skip) return;
    __imp__sub_82433440(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8243E2B8);
PPC_FUNC(sub_8243E2B8)
{
    // 0.5.369: the loop check only while the killcam plays (loops only ever
    // came from the replay's double adds, now refused at the add)
    if (!Killcam::Replaying()) { __imp__sub_8243E2B8(ctx, base); return; }
    static std::vector<uint32_t> seen;
    seen.clear();
    uint32_t prev = 0, node = Guest::Read32(base, 0x825FFB74u);
    int steps = 0;
    while (node && node != 0x825FFB78u && steps < 20000)
    {
        if (node < 0x40000000u || node >= 0xC0000000u) break;
        if (std::find(seen.begin(), seen.end(), node) != seen.end())
        {
            static int told = 0;
            if (told < 20)
            {
                told++;
                uint32_t owner = 0; int ownerIndex = -1; uint8_t ownerType = 0;
                const uint32_t count = Guest::Read32(base, 0x82BE9FC4u);
                for (uint32_t i = 0; i < count && i < 2048; i++)
                {
                    const uint32_t e = Guest::Read32(base, 0x82BD9EC4u + 4 * i);
                    if (e >= 0x40000000u && e < 0xA0000000u && Guest::Read32(base, e + 492) == Guest::Read32(base, node + 0x5C)) { owner = e; ownerIndex = int(i); ownerType = Guest::Read8(base, e); break; }
                }
                int rigOf = -1;
                {
                    const uint32_t ents = Guest::Read32(base, EntityList), rg = Guest::Read32(base, node + 0x5C);
                    for (int c = 0; ents && c < 32; c++) { const uint32_t e = Guest::Read32(base, ents + 4 * uint32_t(c + 1)); if (e && Guest::Read32(base, e + 492) == rg) { rigOf = c; break; } }
                }
                printf("killcam guard: model instance +5C is the rig of client %d (killer %d)\n", rigOf, Killcam::g_killer);
                printf("killcam guard: the scene's model list loops: node %08X (seen at step %zu, now step %d, after %08X), +5C %08X +60 %08X; owner entity %08X (list %d, type %u); killcam %s; cut\n",
                       node, size_t(std::find(seen.begin(), seen.end(), node) - seen.begin()), steps, prev,
                       Guest::Read32(base, node + 0x5C), Guest::Read32(base, node + 0x60), owner, ownerIndex, ownerType, Killcam::Replaying() ? "playing" : "not playing");
                fflush(stdout);
            }
            if (prev) Guest::Write32(base, prev, 0);
            break;
        }
        seen.push_back(node);
        prev = node;
        node = Guest::Read32(base, node);
        steps++;
    }
    __imp__sub_8243E2B8(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82448A18);
PPC_FUNC(sub_82448A18)
{
    const bool mine = !Killcam::g_swapped && !Killcam::g_listAt;
    if (mine) Killcam::SwapIn(base);
    const bool spec = mine && !Killcam::g_swapped && Spectator::SwapIn(base);   // 0.5.490
    if (Killcam::Replaying()) { static int told = 0; if ((told < 3 && told++ < 3)) { printf("killcam: scene entry 82448A18 (%s)\n", mine ? "swapped here" : "already swapped"); fflush(stdout); } }
    const bool study = KillcamStudy::Begin(base);   // 0.5.345
    __imp__sub_82448A18(ctx,base);
    if (study) KillcamStudy::End();
    if (spec) Spectator::SwapOut(base);
    if (mine) Killcam::SwapOut(base);
}
// 0.5.336: 335 still did not draw you, and nothing says whether the scene
// step ran inside the swap. CG_AddPacketEntities itself (8246BED8) now does
// the swap if it is not on yet, and says, once a killcam, whether you are in
// its list and pass its test (your +532 -> +256 against the counter).
extern "C" PPC_FUNC(__imp__sub_8246BED8);
PPC_FUNC(sub_8246BED8)
{
    const bool mine = !Killcam::g_swapped && !Killcam::g_listAt;
    if (mine) Killcam::SwapIn(base);
    const bool spec = mine && !Killcam::g_swapped && Spectator::SwapIn(base);   // 0.5.490
    if (Killcam::Replaying())
    {
        static uint64_t toldFor = 0;
        if (toldFor != Killcam::g_deathAt)
        {
            toldFor = Killcam::g_deathAt;
            const uint32_t own = Killcam::g_listAt ? Killcam::g_ownEnt : 0;
            const uint32_t at = 0x82BD1DC0u, count = Guest::Read32(base, at + 0x18204u), list = at + 0x10000u - 32252u;
            int index = -1;
            for (uint32_t i = 0; own && i < count && i < 4096; i++) if (Guest::Read32(base, list + 4 * i) == own) { index = int(i); break; }
            const uint32_t sv = own ? Guest::Read32(base, own + 532) : 0;
            printf("killcam: packet entities (%s): you %08X %s (index %d of %u), type %u, +532 %08X -> +256 %08X, counter %08X, +16 %08X\n",
                   mine ? "swapped here" : Killcam::g_listAt ? "already swapped" : "NOT swapped", own, index >= 0 ? "in the list" : "NOT in the list", index, count,
                   own ? Guest::Read8(base, own) : 0, sv, sv ? Guest::Read32(base, sv + 256) : 0, Guest::Read32(base, Killcam::SnapshotCounter), own ? Guest::Read32(base, own + 16) : 0);
            fflush(stdout);
        }
    }
    const uint32_t sw0 = KillcamCount::sw, swYou0 = KillcamCount::swYou, pl0 = KillcamCount::players, plYou0 = KillcamCount::playersYou;
    __imp__sub_8246BED8(ctx,base);
    if (Killcam::Replaying() && Killcam::g_listAt)
    {
        KillcamCount::frames++;
        if (KillcamCount::frames % 30 == 1)
        {
            printf("killcam: frame %u: %u entities to the type switch (you %u, type %u), %u players drawn (you %u)\n", KillcamCount::frames,
                   KillcamCount::sw - sw0, KillcamCount::swYou - swYou0, KillcamCount::youType, KillcamCount::players - pl0, KillcamCount::playersYou - plYou0);
            fflush(stdout);
        }
    }
    if (spec) Spectator::SwapOut(base);
    if (mine) Killcam::SwapOut(base);
}
extern "C" PPC_FUNC(__imp__sub_82451ED0);
PPC_FUNC(sub_82451ED0)
{
    __imp__sub_82451ED0(ctx,base);
    if (ctx.r3.u32) { Killcam::PatchSnapshot(base,ctx.r3.u32); Spectator::PatchSnapshot(base,ctx.r3.u32); }   // 0.5.490
}
extern "C" PPC_FUNC(__imp__sub_82464D38);
PPC_FUNC(sub_82464D38)
{
    __imp__sub_82464D38(ctx,base);
    Killcam::PatchWeapon(base);
    Spectator::PatchWeapon(base);   // 0.5.490
}
// 0.5.341: the events cgame plays during the replay (CG_EntityEvent
// sub_8246BC18, r3 the entity, r4 the event), counted, the first named
extern "C" PPC_FUNC(__imp__sub_8246BC18);
PPC_FUNC(sub_8246BC18)
{
    if (Killcam::Replaying() && Killcam::g_swapped)
    {
        Killcam::g_evPlayed++;
        const uint32_t ev = ctx.r4.u32;
        if (ev >= 186 && ev <= 192) Killcam::g_evFire++;
        static uint64_t toldFor = 0; static int told = 0;
        if (toldFor != Killcam::g_deathAt) { toldFor = Killcam::g_deathAt; told = 0; }
        if ((told < 12 && told++ < 12))
        {
            const int who = Killcam::ClientOfEntity(base, ctx.r3.u32);
            printf("killcam: replayed event %u for entity %08X (client %d%s)\n", ev, ctx.r3.u32, who, ctx.r3.u32 == Killcam::g_killerEnt ? ", the killer" : ctx.r3.u32 == Killcam::g_ownEnt ? ", you" : "");
            fflush(stdout);
        }
    }
    __imp__sub_8246BC18(ctx,base);
}
// 0.5.341 (Justin, 340: "You are critically wounded" over the killcam): the
// death panel's update (sub_823A3690, r3 the panel, +76 its state: 0 and 1
// show none of its four lines, 2..5 the wounded / dead / respawn texts)
// runs with state 0 while the killcam plays; its state is kept.
extern "C" PPC_FUNC(__imp__sub_823A3690);
PPC_FUNC(sub_823A3690)
{
    const uint32_t panel = ctx.r3.u32;
    if (Killcam::g_playing && panel)
    {
        const uint32_t state = Guest::Read32(base, panel + 76);
        static uint64_t toldFor = 0; static uint32_t calls = 0;
        calls++;
        if (toldFor != Killcam::g_deathAt) { toldFor = Killcam::g_deathAt; printf("killcam: death text hidden (panel %08X, state %u)\n", panel, state); fflush(stdout); }
        Guest::Write32(base, panel + 76, 0);
        __imp__sub_823A3690(ctx, base);
        Guest::Write32(base, panel + 76, state);
        return;
    }
    __imp__sub_823A3690(ctx, base);
}
PPC_FUNC(sub_82463150)
{
    const uint32_t camera=ctx.r3.u32;
    if (camera && Guest::Read32(base,camera+480)==0) { Killcam::Apply(base,camera,g_kcMode); Spectator::ApplyCamera(base,camera); }   // 0.5.490: the free and behind cameras
    __imp__sub_82463150(ctx,base);
    // 0.5.703: the view as drawn, for the flames drawn in the world (flamethrower.cpp):
    // the local view block's origin (+4960) and axes (+4976: forward, left, up),
    // just made from its angles above, and the camera's fov
    if (camera && Guest::Read32(base,camera+480)==0)
    {
        const uint32_t view=0x82C7D490u;
        float eye[3],axes[9];
        for (int k=0;k<3;k++) eye[k]=ReadFloat(base,view+4960+4*k);
        for (int k=0;k<9;k++) axes[k]=ReadFloat(base,view+4976+4*k);
        Flame::Camera(eye,axes,ReadFloat(base,camera+96));
        Flare::Camera(eye,axes,ReadFloat(base,camera+96));   // 0.5.739
        static int told=0;
        if (told<3) { told++; printf("flamethrower: view at %.0f %.0f %.0f, forward %.2f %.2f %.2f, left %.2f %.2f %.2f, angles %.1f %.1f, fov %.1f\n",eye[0],eye[1],eye[2],axes[0],axes[1],axes[2],axes[3],axes[4],axes[5],ReadFloat(base,view+5024),ReadFloat(base,view+5028),ReadFloat(base,camera+96)); fflush(stdout); }
    }
    static uint32_t logged=0;
    const uint32_t now=GetTickCount();
    if (!camera || Guest::Read32(base,camera+480)!=0 || now-logged<2000) return;
    logged=now;
    printf("bots: local camera %08X auto %.3f, FOV %.2f, tween %.2f %.2f %.2f, bots %d\n",
           camera,ReadFloat(base,camera+48),ReadFloat(base,camera+96),ReadFloat(base,camera+272),
           ReadFloat(base,camera+276),ReadFloat(base,camera+280),g_bots);
    fflush(stdout);
}
#endif

#ifdef COD3_MP
// 0.5.116: model selection asks whether the player has a local session
// entry. Host bots intentionally have none. Admit only its exact call site;
// other ownership checks (local input, views and networking) stay native.
extern "C" PPC_FUNC(__imp__sub_824C3490);
PPC_FUNC(sub_824C3490)
{
    if (ctx.lr == 0x821FB408)
    {
        const uint32_t handle = ctx.r3.u32;
        const uint32_t index = handle & 0x3FFF;
        if (index < 4096)
        {
            const uint32_t entry = 0x82BD1FC0 + index * 8;
            const uint32_t entity = Guest::Read32(base, entry);
            if (entity && Guest::Read32(base, entry + 4) == (handle >> 14))
            {
                const uint32_t slot = BotSlot(base, entity);
                if (slot < MaxClients)
                {
                    printf("bots: slot %u admitted to native model selection (no local session)\n", slot);
                    fflush(stdout);
                    ctx.r3.u64 = 1;
                    return;
                }
            }
        }
    }
    __imp__sub_824C3490(ctx, base);
}
#endif
#ifdef COD3_MP
// 0.5.126: native-path diagnostics for two user-reproduced failures. These
// wrappers observe only; each delegates exactly once with the original context.
extern "C" PPC_FUNC(__imp__sub_8251ED08);
PPC_FUNC(sub_8251ED08)
{
    const uint32_t rig=ctx.r3.u32;
    const uint32_t entity=rig?Guest::Read32(base,rig+172):0;
    const uint32_t slot=entity?BotSlot(base,entity):MaxClients;
    static uint32_t calls[MaxClients]={}, logged[MaxClients]={};
    if (slot<MaxClients)
    {
        ++calls[slot];
        const uint32_t now=GetTickCount();
        if (calls[slot]==1 || now-logged[slot]>=2000)
        {
            logged[slot]=now;
            const uint32_t extra=Guest::Read32(base,entity+524);
            printf("bots: model update slot %u calls %u caller %08X mode %d rig %08X pose %08X component %08X entity620 %08X entity864 %08X extra1304 %08X\n",
                   slot,calls[slot],uint32_t(ctx.lr),int32_t(ctx.r4.u32),rig,
                   Guest::Read32(base,rig+112),Guest::Read32(base,rig+32),
                   Guest::Read32(base,entity+620),Guest::Read32(base,entity+864),
                   extra?Guest::Read32(base,extra+1304):0);
            fflush(stdout);
        }
    }
    __imp__sub_8251ED08(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_8251E5F0);
PPC_FUNC(sub_8251E5F0)
{
    const uint32_t rig=ctx.r3.u32;
    const uint32_t entity=rig?Guest::Read32(base,rig+172):0;
    const uint32_t slot=entity?BotSlot(base,entity):MaxClients;
    static uint32_t calls[MaxClients]={}, logged[MaxClients]={};
    if (slot<MaxClients)
    {
        ++calls[slot];
        const uint32_t now=GetTickCount();
        if (calls[slot]==1 || now-logged[slot]>=2000)
        {
            logged[slot]=now;
            printf("bots: animated model update slot %u calls %u model %u mode %d pose %08X\n",
                   slot,calls[slot],ctx.r4.u32,int32_t(ctx.r5.u32),Guest::Read32(base,rig+112));
            fflush(stdout);
        }
    }
    __imp__sub_8251E5F0(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_82457A60);
PPC_FUNC(sub_82457A60)
{
    const uint32_t client=Guest::Read32(base,CurrentClient);
    const uint32_t entities=Guest::Read32(base,EntityList);
    const uint32_t entity=(entities && client<4)?Guest::Read32(base,entities+4*(client+1)):0;
    const uint32_t ps=entity?Guest::Read32(base,entity+512):0;
    static uint32_t count=0;
    const bool say=ps && count++<128;
    if (say)
    {
        const uint32_t movement=0x82C80000u-11120u+client*6368u;
        printf("weapons: native cycle client %u caller %08X forward %u requireAmmo %u selected %u held %u last %u slots %u %u flags44 %08X flags240 %08X movement32 %08X\n",
               client,uint32_t(ctx.lr),ctx.r3.u32,ctx.r4.u32,
               Guest::Read32(base,0x82B9F310u+client*4),Guest::Read32(base,ps+164),Guest::Read32(base,ps+196),
               Guest::Read8(base,ps+1065),Guest::Read8(base,ps+1066),Guest::Read32(base,ps+44),
               Guest::Read32(base,ps+240),Guest::Read32(base,movement+32));
        fflush(stdout);
    }
    __imp__sub_82457A60(ctx,base);
    if (say)
    {
        printf("weapons: native cycle result client %u selected %u held %u\n",client,
               Guest::Read32(base,0x82B9F310u+client*4),Guest::Read32(base,ps+164));
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82485D28);
PPC_FUNC(sub_82485D28)
{
    const uint32_t target=ctx.r3.u32, client=ctx.r4.u32, caller=uint32_t(ctx.lr);
    static uint32_t count=0;
    const bool say=client<4 && count++<128;
    const uint32_t before=say?Guest::Read32(base,0x82B9F310u+client*4):0;
    __imp__sub_82485D28(ctx,base);
    if (say)
    {
        printf("weapons: native select client %u caller %08X target %u selected %u -> %u result %u\n",
               client,caller,target,before,Guest::Read32(base,0x82B9F310u+client*4),ctx.r3.u32);
        fflush(stdout);
    }
}

// A session player's death (sub_82404DD0, the session object in r3). At its
// end, for a player of this machine (sub_823D9B50: his machine object is
// local), it clears the byte at [[0x82BCCAC0]]+8. The bot shares the host's
// machine object, so his death cleared it too, and the host froze, unable even
// to pause, until he himself died (0.5.137-0.5.139). While the host is alive,
// a change of that byte by a bot's death is undone (0.5.140).
extern "C" PPC_FUNC(__imp__sub_82404DD0);
PPC_FUNC(sub_82404DD0)
{
    auto flagAt = [&]() -> uint32_t {
        const uint32_t session = Guest::Read32(base, 0x82BCCAC0u);
        if (session < 0x40000000u || session >= 0x90000000u) return 0;
        const uint32_t first = Guest::Read32(base, session);
        if (first < 0x40000000u || first >= 0x90000000u) return 0;
        return first + 8;
    };
    const uint32_t at = flagAt();
    const uint8_t before = at ? Guest::Read8(base, at) : 0;
    __imp__sub_82404DD0(ctx, base);
    if (!at || g_bots <= 0) return;
    const uint8_t after = Guest::Read8(base, at);
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t hostRecord = host ? Guest::Read32(base, host + 512) : 0;
    const int32_t health = hostRecord ? int32_t(Guest::Read32(base, hostRecord + 304)) : -1;
    const int32_t state = hostRecord ? int32_t(Guest::Read32(base, hostRecord + 1924)) : -1;
    static int told = 0;
    if ((told < 40 && told++ < 40))
    {
        printf("bots: a session death: local flag %u -> %u, the host's health %d, state %d\n", before, after, health, state);
        fflush(stdout);
    }
    if (after != before && health > 0 && state == 3)
    {
        Guest::Write8(base, at, before);
        static int fixed = 0;
        if ((fixed < 40 && fixed++ < 40)) { printf("bots: a bot's death cleared the host's local flag: put back to %u\n", before); fflush(stdout); }
    }
}


// 0.5.141: Justin, on 0.5.140: still frozen when the bot dies. The log shows
// that in the frame of the bot's death the session's frame (sub_82404580,
// from sub_82406C70) calls through a broken object (its table read 0x4035),
// then other threads do the same: something of this machine's local players
// is wrecked by the death of a bot holding a local place. So a bot who has
// been alive leaves his local place as soon as he is dead, before the session
// layer's update (sub_82406C70) sees him; his respawn (Respawn) gives it back.
static void ReleaseDeadPlaces(uint8_t* base)
{
    static bool wasAlive[MaxClients] = {};
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return;
    for (uint32_t slot = 1; slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot] || g_placeAt[slot] == 0) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t r = entity ? Guest::Read32(base, entity + 512) : 0;
        if (!r) continue;
        const bool alive = int32_t(Guest::Read32(base, r + 304)) > 0 && Guest::Read32(base, r + 1924) == 3;
        if (alive)
        {
            wasAlive[slot] = true;
            // 0.5.142: alive again with his place still empty (it was
            // emptied once more right after a respawn): given back.
            if (Guest::Read8(base, g_placeAt[slot]) == 0xFF)
            {
                Guest::Write8(base, g_placeAt[slot], g_placeNumber[slot]);
                static int back = 0;
                if ((back < 40 && back++ < 40)) { printf("bots: slot %u is alive: back in his local place\n", slot); fflush(stdout); }
            }
            continue;
        }
        if (!wasAlive[slot]) continue;   // not yet spawned once: keep it (the zoom, 0.5.137)
        if (Guest::Read8(base, g_placeAt[slot]) != g_placeNumber[slot]) continue;
        Guest::Write8(base, g_placeAt[slot], 0xFF);
        static int told = 0;
        if ((told < 40 && told++ < 40)) { printf("bots: slot %u is dead: leaves his local place\n", slot); fflush(stdout); }
    }
}

extern "C" PPC_FUNC(__imp__sub_82406C70);
PPC_FUNC(sub_82406C70)
{
    if (g_bots > 0 && !g_inSpawn) ReleaseDeadPlaces(base);
    __imp__sub_82406C70(ctx, base);
}


// 0.5.143: SV_GetUsercmd(client, out). ClientThink passes the current client
// as the server client; for a bot's think that is now his local index, so
// his slot is given instead (his commands are there).
extern "C" PPC_FUNC(__imp__sub_82530270);
PPC_FUNC(sub_82530270)
{
    if (g_thinkSlot < MaxClients && g_placeIndex[g_thinkSlot] != 0 && ctx.r3.u32 == g_placeIndex[g_thinkSlot])
        ctx.r3.u32 = g_thinkSlot;
    // 0.5.159: a bot's command from his own ring (see CommandPlayer).
    const uint32_t client = ctx.r3.u32;
    if (client >= FirstBotSlot() && client < MaxClients && g_bot[client] && g_commandBlock[client] != 0)   // 0.5.283: from slot 1
    {
        const uint32_t command = NewestCommand(base, client);
        const uint32_t out = ctx.r4.u32;
        if (out) memcpy(base + out, base + command, CommandSize);
        return;
    }
    __imp__sub_82530270(ctx, base);
}


// 0.5.145: bots as remote players (each his own machine object, not on this
// machine), so there is no limit of 3 and none of this machine's local-player
// code takes them. A remote player is drawn from what his own machine sends
// (sub_82400810: for a player not on this machine, sub_823D9B50 false, it
// reads the network's copy, sub_823EC930); a bot's machine is the host and
// sends nothing, so he was invisible (0.5.135). While sub_82400810 updates a
// bot's session player, "is he on this machine" answers yes for him alone,
// so his body is worked out from his entity, as the host's own is.
namespace {
    uint32_t g_drawingBot = 0;
    int g_botsLocal = 0;
    // Nonzero only inside the supplemental session tick. Its native tail
    // walks all 24 players; only this bot may run under its private gun view.
    uint32_t g_supplementalBotPlayer = 0;
}
static bool IsBotSessionPlayer(uint8_t* base, uint32_t player)
{
    if (player < 0x40000000u || player >= 0xA0000000u) return false;
    const int32_t slot = int32_t(Guest::Read32(base, player + 8));
    return slot >= int32_t(FirstBotSlot()) && slot < int32_t(MaxClients) && g_bot[slot];
}
extern "C" PPC_FUNC(__imp__sub_82400810);
PPC_FUNC(sub_82400810)
{
    const uint32_t player = ctx.r3.u32;
    if (g_supplementalBotPlayer && player != g_supplementalBotPlayer)
    {
        static int skipped = 0;
        if ((skipped < 5 && skipped++ < 5)) { printf("bots: isolated session tick skips player %08X (active bot %08X)\n", player, g_supplementalBotPlayer); fflush(stdout); }
        return; // native caller ignores the return value
    }
    const bool bot = g_botRemote.load() && g_bots > 0 && IsBotSessionPlayer(base, player);
    const uint32_t was = g_drawingBot;
    if (bot)
    {
        // The normal session tail also reaches bots while CurrentClient=0.
        // Give this bot the same private context as his supplemental tick.
        const uint32_t slot = Guest::Read32(base, player + 8);
        if (!BotGunViewBlock(base, slot)) return;
        KeepFov keep(base);
        BotGunViewScope gunView(base, slot, !g_botLocal.load());
        const uint32_t thinking = g_thinkSlot;
        g_thinkSlot = slot;
        g_drawingBot = player;
        __imp__sub_82400810(ctx, base);
        g_thinkSlot = thinking;
    }
    else __imp__sub_82400810(ctx, base);
    g_drawingBot = was;
    static int told = 0;
    if (bot && told++ < 10) { printf("bots: session player %08X (slot %d) drawn as this machine's\n", player, int32_t(Guest::Read32(base, player + 8))); fflush(stdout); }
}
extern "C" PPC_FUNC(__imp__sub_823D9B50);
PPC_FUNC(sub_823D9B50)
{
    if (g_drawingBot != 0 && ctx.r3.u32 == g_drawingBot) { ctx.r3.u64 = 1; return; }
    // 0.5.160: inside a call that plays a player's death (sub_823E9998),
    // every bot counts as this machine's.
    if (g_botsLocal > 0 && IsBotSessionPlayer(base, ctx.r3.u32)) { ctx.r3.u64 = 1; return; }
    // 0.5.161: everywhere. Justin, on 0.5.160 (3 bots, all stable): no
    // death animation, no kill feed for their deaths, no gun in their hands.
    // All three are what the game takes from a remote player's own console
    // (sub_823D9B50 false: "his machine sends it"); a bot's never does. The
    // trouble that made the bots remote (0.5.135-0.5.144: zoom, the host's
    // guns going full auto, freezes) came from their commands being written
    // over the host's input and gun view (fixed in 0.5.159), so the bots now
    // count as this machine's players in every one of these checks, while
    // keeping their own machine object, no local place and their own
    // command rings and gun view memory.
    if (g_botRemote.load() && g_bots > 0 && IsBotSessionPlayer(base, ctx.r3.u32))
    {
        static int told = 0;
        if ((told < 5 && told++ < 5)) { printf("bots: session player %08X (a bot) counted as this machine's\n", ctx.r3.u32); fflush(stdout); }
        ctx.r3.u64 = 1;
        return;
    }
    __imp__sub_823D9B50(ctx, base);
}


// 0.5.147: a local client's frame (sub_82593D50: his view and weapon object,
// 0x82C37070[current client]) run by the server frame for a local client
// other than the host's while the bots are remote players: there is no such
// player, only the bots' shared index (3), and running it wrecked the host's
// view (field of view 0), gun and controls. Skipped.
extern "C" PPC_FUNC(__imp__sub_82593D50);
PPC_FUNC(sub_82593D50)
{
    const uint32_t current = Guest::Read32(base, 0x82BCBE9Cu);
    if (current != 0 && current < 4 && g_bots > 0 && !g_botLocal.load() && g_thinkSlot >= MaxClients)
    {
        static int told = 0;
        if ((told < 20 && told++ < 20)) { printf("bots: local client %u's frame skipped (no such player; bots are remote)\n", current); fflush(stdout); }
        return;
    }
    __imp__sub_82593D50(ctx, base);
}


// 0.5.148: 0.5.147's log: the field of view still went to 0 every frame,
// now from the host's own local-client frame (no phantom one ran). The
// weapon-view object (sub_82519178's r3, 0x82C37070[current client]) keeps a
// saved field of view at +0x74 that it puts back into the global one; once a
// bot's code had left the global at 0 while it was saved, the saved value
// itself is 0 and comes back every frame. Healed: if this call leaves the
// global below 1 while it was sane before, both the global and the
// object's +0x74 get the sane value back.
extern "C" PPC_FUNC(__imp__sub_82519178);
PPC_FUNC(sub_82519178)
{
    const uint32_t object = ctx.r3.u32;
    const float before = ReadFloat(base, BaseFov);
    __imp__sub_82519178(ctx, base);
    if (g_bots <= 0) return;
    const float after = ReadFloat(base, BaseFov);
    const float good = (before >= 10.0f && before <= 170.0f) ? before : g_goodFov;
    if (after >= 1.0f || good < 10.0f) return;
    uint32_t bits; memcpy(&bits, &good, 4);
    Guest::Write32(base, BaseFov, bits);
    // 0.5.150: the object was 0 in 0.5.148's log: its +0x74 is guest
    // address 0x74, which the bots' own runs of this code share.
    const bool objectOk = object == 0 || (object >= 0x40000000u && object < 0xA0000000u);
    const float saved = objectOk ? ReadFloat(base, object + 0x74) : -1.0f;
    if (objectOk && !(saved >= 10.0f && saved <= 170.0f)) Guest::Write32(base, object + 0x74, bits);
    static int told = 0;
    if ((told < 20 && told++ < 20))
    {
        printf("bots: weapon view %08X (current client %u) put the field of view to %.2f (its saved %.2f): healed to %.2f\n",
               object, Guest::Read32(base, CurrentClient), after, saved, good);
        fflush(stdout);
    }
}


// 0.5.150: the first-person weapon view of a local client (sub_8251A950, its
// object 0x82C37070[current client]) run while the current client is a
// bot's (index 3 or his slot): the bots have no first-person view, and the
// object for them is 0, the same as the host's at times, so their runs wrote
// over the host's saved field of view (guest 0x74): his gun flickered in and
// out. Skipped for bots.
extern "C" PPC_FUNC(__imp__sub_8251A950);
PPC_FUNC(sub_8251A950)
{
    const uint32_t current = Guest::Read32(base, 0x82BCBE9Cu);
    if (current != 0 && g_bots > 0 && !g_botLocal.load())
    {
        static int told = 0;
        if ((told < 10 && told++ < 10)) { printf("bots: first-person weapon view for current client %u skipped (a bot's)\n", current); fflush(stdout); }
        return;
    }
    __imp__sub_8251A950(ctx, base);
}


// 0.5.152: 0.5.151's log: with two bots the host's gun went (in hand 0) and
// the game then crashed in the gun view (sub_8251A848) under a player's
// spawn/think done by the game itself (sub_82593608, the entity in r3), not
// by our steps. The host's gun view lives at guest 0x0 (its object is 0),
// and so does a bot's. So whatever the game itself runs for a bot's entity
// (his spawn sub_82593608, his think sub_82593518 by handle) keeps guest
// 0x0-0x7FF and the field of view as they were (KeepFov).
static bool IsBotEntity(uint8_t* base, uint32_t entity)
{
    return g_bots > 0 && entity >= 0x40000000u && entity < 0xA0000000u && BotSlot(base, entity) < MaxClients;
}
// 0.5.175: 8246A7D8 receives entity events after the session/think scopes.
// Events165/166/167 ONLY set the current local client's input stance latch
// to stand/crouch/prone (native cases8246AEB4/AEF4/AF34). Bots have no local
// controller; their body stance comes from their own player-state movement.
// Applying their echoed stance events to CurrentClient0 stands the host up.
extern "C" PPC_FUNC(__imp__sub_8246A7D8);
PPC_FUNC(sub_8246A7D8)
{
    const uint32_t event = ctx.r4.u32;
    if (event >= 165 && event <= 167 && IsBotEntity(base, ctx.r3.u32))
    {
        static int told = 0;
        if ((told < 60 && told++ < 60)) { printf("bots: stance event %u from slot %u excluded from local input (client %u, caller %08X)\n", event, BotSlot(base, ctx.r3.u32), Guest::Read32(base, CurrentClient), uint32_t(ctx.lr)); fflush(stdout); }
        return;
    }
    __imp__sub_8246A7D8(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_82593608);
PPC_FUNC(sub_82593608)
{
    if (!IsBotEntity(base, ctx.r3.u32)) { __imp__sub_82593608(ctx, base); return; }
    const uint32_t slot = BotSlot(base, ctx.r3.u32);
    KeepFov keep(base);
    BotGunViewScope gunView(base, slot, !g_botLocal.load());   // 0.5.153
    const uint32_t thinking = g_thinkSlot;
    if (thinking >= MaxClients) g_thinkSlot = slot;
    __imp__sub_82593608(ctx, base);
    g_thinkSlot = thinking;
}
bool MpFixesFlameOverheated(uint8_t* base, uint32_t entity);   // mp_fixes.cpp (0.5.720)
extern "C" PPC_FUNC(__imp__sub_82593518);
PPC_FUNC(sub_82593518)
{
    const uint32_t handle = ctx.r3.u32;
    const uint32_t entity = handle ? Guest::Read32(base, 0x82BD1FC0 + 8 * (handle & 0x3FFF)) : 0;
    // 0.5.720: an overheated M2 (World at War's overheating) fires no more: the
    // fire bit (1) off in the player's newest command, the one this think runs on
    if (entity && MpFixesFlameOverheated(base, entity))
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        for (uint32_t c = 0; entities && c < MaxClients; c++)
            if (Guest::Read32(base, entities + 4 * (c + 1)) == entity)
            {
                const uint32_t player = Players + PlayerSize * c;
                const uint32_t command = player + Ring + CommandSize * (Guest::Read32(base, player + Newest) & 63u);
                Guest::Write32(base, command + 4, Guest::Read32(base, command + 4) & ~1u);
                break;
            }
    }
    if (!IsBotEntity(base, entity)) { __imp__sub_82593518(ctx, base); return; }
    const uint32_t slot = BotSlot(base, entity);
    KeepFov keep(base);
    BotGunViewScope gunView(base, slot, !g_botLocal.load());   // 0.5.153
    const uint32_t thinking = g_thinkSlot;
    if (thinking >= MaxClients) g_thinkSlot = slot;
    __imp__sub_82593518(ctx, base);
    g_thinkSlot = thinking;
}


// 0.5.157: THE CAUSE of the host's broken gun, freeze and crashes with two
// bots in War. War's script (_mp_war, e.g. sub_82214EB8, sub_82212190) calls
// two of the engine's callbacks for every player, by his client number:
// table 0x82BCBF64 +0x400 (sub_824C9810) and +0x404 (sub_824B52A0). Both end
// in sub_8244A1D0, which SETS the current client (0x82BCBE9C) to that number
// and writes that client's view kick into the per-local-client view state
// (0x82C7D490 + n * 0x18E0, 4 entries). Those are for this machine's local
// players (0-3); for a bot (client 4, 5...) the current client was left at
// his number, and the client side then indexed its 4-entry lists past their
// end: the per-client array at 0x82C35030 (0x804 each) runs straight into the
// gun view pointers at 0x82C37070 (seen zeroed then full of floats in
// 0.5.156's log), the host's weapon was reselected to 0, and the gun view
// code crashed. A non-local client's view kick is now skipped, and the
// current client left alone.
static bool NotLocalClient(uint32_t client) { return client != 0xFFFFFFFFu && client > 3; }
static void TellSkippedKick(const char* where, uint32_t client)
{
    static int told = 0;
    if ((told < 20 && told++ < 20)) { printf("bots: %s for client %u (not a local player) skipped: the current client stays %u\n", where, client, 0u); fflush(stdout); }
}
extern "C" PPC_FUNC(__imp__sub_8244A1D0);
PPC_FUNC(sub_8244A1D0)
{
    if (NotLocalClient(ctx.r9.u32)) { TellSkippedKick("a view kick (sub_8244A1D0)", ctx.r9.u32); return; }
    __imp__sub_8244A1D0(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_824C9810);
PPC_FUNC(sub_824C9810)
{
    if (NotLocalClient(ctx.r4.u32)) { TellSkippedKick("the script's callback +0x400 (sub_824C9810)", ctx.r4.u32); return; }
    __imp__sub_824C9810(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_824B52A0);
PPC_FUNC(sub_824B52A0)
{
    if (NotLocalClient(ctx.r4.u32)) { TellSkippedKick("the script's callback +0x404 (sub_824B52A0)", ctx.r4.u32); return; }
    __imp__sub_824B52A0(ctx, base);
}


// 0.5.160: the bots' death animation, gone since they became remote players
// (0.5.145). A player's death from the server's side (sub_8258C778) goes to
// the session layer through sub_823E9998 with the players' handles: for a
// player of this machine it plays it here (0x823E9C68 on), for a remote one
// it makes a message for his machine to play it, and a bot's machine is the
// host, which never receives it. During that call the bots count as this
// machine's, so their deaths are played here as they were before 0.5.145.
extern "C" PPC_FUNC(__imp__sub_823E9998);
PPC_FUNC(sub_823E9998)
{
    if (g_bots <= 0 || !g_botRemote.load()) { __imp__sub_823E9998(ctx, base); return; }
    g_botsLocal++;
    __imp__sub_823E9998(ctx, base);
    g_botsLocal--;
    static int told = 0;
    if ((told < 20 && told++ < 20)) { printf("bots: a death (sub_823E9998) played with the bots as this machine's\n"); fflush(stdout); }
}


// 0.5.162: Justin, on 0.5.161: the bots' guns show again, but still no death
// animation and nothing in the kill feed. Both worked in 0.5.137-0.5.144,
// when a bot held one of the session's local places (session +0x5991, the
// numbers of this machine's 4 players). The game asks those through
// sub_823D8FF0 (is this number one of ours) and sub_823D8FB0 (which local
// index is this entry). A bot's number now answers "ours", index 3 (the
// bots' shared local index, 0.5.145), without holding a place.
static bool IsBotSessionNumber(uint8_t* base, uint32_t object, uint32_t number)
{
    if (number >= SessionMaxNow() || object < 0x40000000u || object >= 0xA0000000u) return false;
    return IsBotSessionPlayer(base, MpSessionPlayerAddress(base, object, 0x1010 + number * 0x310));
}
extern "C" PPC_FUNC(__imp__sub_823D8FF0);
PPC_FUNC(sub_823D8FF0)
{
    if (g_botRemote.load() && g_bots > 0 && IsBotSessionNumber(base, ctx.r3.u32, ctx.r4.u32 & 0xFF))
    {
        static int told = 0;
        if ((told < 5 && told++ < 5)) { printf("bots: session number %u (a bot) answered as one of this machine's places\n", ctx.r4.u32 & 0xFF); fflush(stdout); }
        ctx.r3.u64 = 1;
        return;
    }
    __imp__sub_823D8FF0(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_823D8FB0);
PPC_FUNC(sub_823D8FB0)
{
    const uint32_t object = ctx.r3.u32, entry = ctx.r4.u32;
    __imp__sub_823D8FB0(ctx, base);
    if (int32_t(ctx.r3.u32) >= 0 || entry == 0 || !g_botRemote.load() || g_bots <= 0) return;
    if (IsBotSessionNumber(base, object, Guest::Read8(base, entry)))
    {
        static int told = 0;
        if ((told < 5 && told++ < 5)) { printf("bots: session entry %08X (a bot) given local index 3\n", entry); fflush(stdout); }
        ctx.r3.u64 = 3;
    }
}


// 0.5.167: the bots' death animation and their deaths in the kill feed.
// 0.5.165's dumps: at a death the host's session player has a block at
// +0x100..+0x138 filled (where and how he died); a bot's stays empty. The
// session fills it, and sends it to every console, only in its update of
// this machine's local players (sub_82403840: the current client's local
// place, session +0x5991, names the session player; sub_823F2E20 builds the
// messages), run by the session's frame (sub_82404580) for each local
// player in play. The bots hold no place, so it never ran for them. Now,
// after the session's frame, it is run once for each bot: local place 3
// (the bots' scratch index) given his number for that call, the current
// client 3, his own gun view memory, and the session timer words that call
// advances (+0x85B0, +0x861C of the session object) kept as they were. Its
// 0.5.173 explicitly locks physical input for this scratch local client.
// The player-update wrapper also excludes every other player during this call.
namespace {
    bool g_inBotUpdate = false;
    BotNetworkClock::State g_botNetworkClock[MaxClients];
    uint32_t g_botPacketsBuilt[MaxClients] = {}, g_botPacketsSent[MaxClients] = {};
}
extern "C" PPC_FUNC(__imp__sub_82404580);
PPC_FUNC(sub_82404580)
{
    const double dt = ctx.f1.f64;
    const uint32_t mode = ctx.r3.u32;
    PPCContext saved = ctx;
    __imp__sub_82404580(ctx, base);
    if (g_bots <= 0 || g_inBotUpdate || !g_botRemote.load() || mode < 0x40000000u || mode >= 0xA0000000u) return;
    const uint32_t object = mode + 0xAEB0;
    const uint32_t places = object + 0x5991;
    const uint32_t timerA = object + 0x85B0, timerB = object + 0x861C;
    g_inBotUpdate = true;
    // 0.5.171: Justin, on 0.5.170: lying prone he now and then stood up "like
    // someone was controlling me". The keys' state (0x82B53E20, read and
    // cleared by the command builder) and the host's input state (0x82C35030)
    // are kept as they were across the bots' updates, and the log says if a
    // bot's update changed them.
    constexpr uint32_t Keys = 0x82B53E20, KeysSize = 0x300, HostInput = 0x82C35030, HostInputSize = 0x804;
    static uint8_t keysBefore[KeysSize], inputBefore[HostInputSize];
    memcpy(keysBefore, base + Keys, KeysSize);
    memcpy(inputBefore, base + HostInput, HostInputSize);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t player = SessionPlayer(base, slot);
        if (player == 0) continue;
        const uint8_t number = Guest::Read8(base, player);
        if (number >= SessionMaxNow()) continue;
        const uint8_t place3 = Guest::Read8(base, places + 3);
        if (place3 < SessionMaxNow() && place3 != number) continue;   // a real third local player (split screen): leave it
        const uint32_t a = Guest::Read32(base, timerA), b = Guest::Read32(base, timerB);
        if (!BotGunViewBlock(base, slot)) continue; // never borrow the host gun view
        KeepFov keep(base);
        BotGunViewScope gunView(base, slot, true);   // current client 3, his own gun view
        Guest::Write8(base, places + 3, number);
        PPCContext call = saved;
        call.r1.u32 = (saved.r1.u32 - 0x3000) & ~0xFu;
        call.r3.u32 = object;
        call.f1.f64 = dt;
        call.lr = 0;
        const uint8_t locked = Guest::Read8(base, MoveLocked + BotGunView);
        Guest::Write8(base, MoveLocked + BotGunView, 1);
        const uint32_t previousPlayer = g_supplementalBotPlayer;
        g_supplementalBotPlayer = player;
        // Native82403840 throttles full/near/far sends with three clocks
        // indexed by CurrentClient. Every bot borrows3, so each needs its
        // own persistent clocks; restore the real scratch client's words.
        auto& networkClock=g_botNetworkClock[slot];
        networkClock.Bind(mode,player);
        uint32_t clocksBefore[3];
        for(unsigned i=0;i<3;++i) {
            const uint32_t at=object+BotNetworkClock::Offset(i,BotGunView);
            clocksBefore[i]=Guest::Read32(base,at);
            Guest::Write32(base,at,networkClock.sent[i]);
        }
        Guest::Lookup(0x82403840)(call, base);
        for(unsigned i=0;i<3;++i) {
            const uint32_t at=object+BotNetworkClock::Offset(i,BotGunView);
            networkClock.sent[i]=Guest::Read32(base,at);
            Guest::Write32(base,at,clocksBefore[i]);
        }
        static uint32_t lastNetworkReport[MaxClients]={};
        if(GetTickCount()-lastNetworkReport[slot]>=5000) {
            lastNetworkReport[slot]=GetTickCount();
            printf("bots: replication slot %u built %u sends %u clocks %u %u %u\n",slot,
                g_botPacketsBuilt[slot],g_botPacketsSent[slot],networkClock.sent[0],networkClock.sent[1],networkClock.sent[2]);fflush(stdout);
        }
        g_supplementalBotPlayer = previousPlayer;
        Guest::Write8(base, MoveLocked + BotGunView, locked);
        Guest::Write8(base, places + 3, place3);
        Guest::Write32(base, timerA, a);
        Guest::Write32(base, timerB, b);
    }
    {
        const bool keys = memcmp(keysBefore, base + Keys, KeysSize) != 0;
        const bool input = memcmp(inputBefore, base + HostInput, HostInputSize) != 0;
        if (keys || input)
        {
            static int changed = 0;
            if ((changed < 30 && changed++ < 30)) { printf("bots: a bot's session update changed the host's %s%s%s: put back\n", keys ? "keys" : "", keys && input ? " and " : "", input ? "input" : ""); fflush(stdout); }
            memcpy(base + Keys, keysBefore, KeysSize);
            memcpy(base + HostInput, inputBefore, HostInputSize);
        }
    }
    g_inBotUpdate = false;
    static int told = 0;
    if ((told < 3 && told++ < 3)) { printf("bots: isolated session update for each bot (weapon/session state; physical input locked)\n"); fflush(stdout); }
}

// Count native serialization and transport calls during the isolated bot
// update. A send call does not prove that a peer received/rendered the bot.
extern "C" PPC_FUNC(__imp__sub_823F2E20);
PPC_FUNC(sub_823F2E20)
{
    const uint32_t player=ctx.r3.u32;
    const bool bot=g_supplementalBotPlayer==player && IsBotSessionPlayer(base,player);
    const uint32_t slot=bot?Guest::Read32(base,player+8):0;
    __imp__sub_823F2E20(ctx,base);
    if(bot)++g_botPacketsBuilt[slot];
}
extern "C" PPC_FUNC(__imp__sub_823DCCA8);
PPC_FUNC(sub_823DCCA8)
{
    if(g_supplementalBotPlayer && IsBotSessionPlayer(base,g_supplementalBotPlayer))
        ++g_botPacketsSent[Guest::Read32(base,g_supplementalBotPlayer+8)];
    __imp__sub_823DCCA8(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_823DCF10);
PPC_FUNC(sub_823DCF10)
{
    if(g_supplementalBotPlayer && IsBotSessionPlayer(base,g_supplementalBotPlayer))
        ++g_botPacketsSent[Guest::Read32(base,g_supplementalBotPlayer+8)];
    __imp__sub_823DCF10(ctx,base);
}

// 0.5.167: the UAV shows the bots. The compass's enemy markers
// (sub_825049C8, on while the UAV sets enemy_radar, 0x82B2CF90 +12) are for
// players whose +0x84 (the time they last fired) is within 15 s of the
// level's time (0x82C2826C): a bot's firing (sub_82590C30, called by us)
// never sets it, so they never showed. While the UAV is on, every bot's is
// kept at the level's time: all of them on the compass, all the time.
static void UavShowsBots(uint8_t* base)
{
    if (Guest::Read32(base, 0x82B2CF90 + 12) != 1) return;
    const uint32_t entities = Guest::Read32(base, EntityList);
    if (!entities) return;
    const uint32_t levelTime = Guest::Read32(base, 0x82C2826C);
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots(); slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        if (record) Guest::Write32(base, record + 0x84, levelTime);
    }
    // 0.5.169: what the compass tests (sub_825049C8), every 2 s while the
    // UAV is on: a bot shows if his side (entity +520 -> +4) is the host's
    // enemy and his +0x84 is within 15 s of the level's time.
    static uint32_t lastTold = 0;
    const uint32_t now = GetTickCount();
    if (now - lastTold < 2000) return;
    lastTold = now;
    const uint32_t host = Guest::Read32(base, entities + 4);
    const uint32_t hostTeam = host ? Guest::Read32(base, host + 520) : 0;
    printf("bots: UAV check: level time %u, host side %u:", levelTime, hostTeam ? Guest::Read32(base, hostTeam + 4) : 99);
    int shown = 0;
    for (uint32_t slot = FirstBotSlot(); slot < ActiveSlots() && shown < 6; slot++)
    {
        if (!g_bot[slot]) continue;
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        const uint32_t record = entity ? Guest::Read32(base, entity + 512) : 0;
        const uint32_t team = entity ? Guest::Read32(base, entity + 520) : 0;
        printf(" [%u side %u fired %u]", slot, team ? Guest::Read32(base, team + 4) : 99, record ? Guest::Read32(base, record + 0x84) : 0);
        shown++;
    }
    printf("\n"); fflush(stdout);
}


// 0.5.170: which of the game's death steps run for a bot's death, against
// the host's own (Justin: still no death animation for any bot). Each of
// these, the first 40 calls, with what its arguments are: a player's
// entity, record or script handle (index slot + 1) names him.
static const char* WhoIs(uint8_t* base, uint32_t v, char* out)
{
    const uint32_t entities = Guest::Read32(base, EntityList);
    for (uint32_t slot = 0; entities && slot < ActiveSlots(); slot++)
    {
        const uint32_t entity = Guest::Read32(base, entities + 4 * (slot + 1));
        if (!entity) continue;
        const char* who = (slot < MaxClients && g_bot[slot]) ? "bot" : "host";
        if (v == entity) { sprintf(out, "%s %u's entity", who, slot); return out; }
        if (v == Guest::Read32(base, entity + 512)) { sprintf(out, "%s %u's record", who, slot); return out; }
        if (v == Guest::Read32(base, entity + 520)) { sprintf(out, "%s %u's team", who, slot); return out; }
        if (v > 0x3FFF && v < 0x01000000u && (v & 0x3FFF) == slot + 1) { sprintf(out, "%s %u's handle?", who, slot); return out; }
    }
    sprintf(out, "%08X", v);
    return out;
}
#define DEATH_TRACE(addr, name) \
    extern "C" PPC_FUNC(__imp__sub_##addr); \
    PPC_FUNC(sub_##addr) \
    { \
        static int calls = 0; \
        if ((calls < 40 && calls++ < 40)) \
        { \
            char a[48], b[48], c[48]; \
            printf("bots: death step %s (%s) at %u: r3 %s, r4 %s, r5 %s\n", #addr, name, GetTickCount(), \
                   WhoIs(base, ctx.r3.u32, a), WhoIs(base, ctx.r4.u32, b), WhoIs(base, ctx.r5.u32, c)); \
            fflush(stdout); \
        } \
        __imp__sub_##addr(ctx, base); \
    }
DEATH_TRACE(8258C778, "server: a player killed")
DEATH_TRACE(8220B008, "script: DeathState")
DEATH_TRACE(821E5028, "script: Player_Dying notify")
DEATH_TRACE(821E4660, "script: PLAYER_DYING notify")
DEATH_TRACE(82463B78, "playerDeathHitGround")
DEATH_TRACE(82584188, "PLAYER_DEATH_FLYING")
#undef DEATH_TRACE


// 0.5.171: THE BOTS' DEATHS. 0.5.170's trace: at the host's death the game
// ran its player-dies routine (sub_82587268: drops the gun, the death
// animation through sub_82584188, the scripts' DeathState and Player_Dying,
// the obituary); at a bot's death none of it ran. A lethal hit reaches
// sub_823FD118, which with no network session (*r3 == 0) calls the
// player-dies routine itself and then the obituary callback; with a session
// it hands the death to the session layer (sub_823ED658), whose message
// comes back to the victim's own machine: a bot has none, so his death was
// never played. A bot's death now takes the no-session way: r3 points at a
// zero word for that call, so the routine runs here, on the host.
extern "C" PPC_FUNC(__imp__sub_823FD118);
PPC_FUNC(sub_823FD118)
{
    static uint32_t zero = 0;
    char who[48];
    const char* victim = WhoIs(base, ctx.r4.u32, who);
    const bool bot = strncmp(victim, "bot ", 4) == 0 && strstr(victim, "entity") != nullptr;
    static int told = 0;
    if ((told < 60 && told++ < 60)) { printf("bots: lethal hit (sub_823FD118) on %s, session %08X%s\n", victim, Guest::Read32(base, ctx.r3.u32), bot ? ": played here (no-session way)" : ""); fflush(stdout); }
    if (bot && g_botRemote.load())
    {
        if (!zero) { zero = Guest::AllocatePhysical(16); if (zero) memset(base + zero, 0, 16); }
        if (zero) ctx.r3.u32 = zero;
    }
    __imp__sub_823FD118(ctx, base);
}
// 0.5.220: Justin: when bots kill each other the killfeed shows both names
// green. The killfeed line (sub_82456920: victim, attacker, means) colours
// each name in team games by sub_8244DB20: that team against the LOCAL
// player's, read through CurrentClient (0x82BCBE9C) + 1 in the entity list.
// A bot's kill happens inside his own step, where BotClientScope has set
// CurrentClient to the bot's local place: the comparison was against an
// empty or wrong player, and everything came out "same team". The host's
// own (0) is put back for the line, then the bot's again.
// 0.5.246: the killfeed line's colours, set where the line is handed to
// the HUD (sub_82529D78, called only by sub_82456920 at 0x82456CC4: r5 the
// attacker's colour, r7 the victim's, 4 floats each). 0.5.245 changed only
// the team-game colour (sub_8244DB20), but Justin still saw "green killed
// green": the line's other path (no team game flag at 0x82B913F8) colours a
// name green when sub_8247E2F0 says it is a player of this machine, and the
// bots count as this machine's (0.5.160/161). Justin's rule: the one who
// dies red, the killer green if on the host's side, red if not.
static struct { bool active; uint32_t victim, attacker, hostSide; } g_feedLine = {};
extern "C" PPC_FUNC(__imp__sub_82529D78);
PPC_FUNC(sub_82529D78)
{
    if (g_feedLine.active)
    {
        auto paint = [&](uint32_t at, bool red) {
            if (at < 0x40000000u || at >= 0xA0000000u) return;
            const float k = 0.25f;   // the game's own (0x820631A4)
            const float rgb[3] = { red ? 1.0f : k, red ? k : 1.0f, k };
            for (int i = 0; i < 3; i++) { uint32_t v; memcpy(&v, &rgb[i], 4); Guest::Write32(base, at + 4 * i, v); }
        };
        // 0.5.281: Justin's rule now: each name by its side as this machine's
        // player sees it, the victim too: green his own team, red the
        // enemy's ("red killed green", never "red killed red" for a kill
        // between the two teams). A victim with no side stays red.
        {
            const uint32_t v = g_feedLine.victim;
            const uint32_t team = (v >= 0x40000000u && v < 0xA0000000u) ? Guest::Read32(base, v + 520) : 0;
            const uint32_t side = team ? Guest::Read32(base, team + 4) : 0;
            const bool known = (side == 1 || side == 2) && (g_feedLine.hostSide == 1 || g_feedLine.hostSide == 2);
            paint(ctx.r7.u32, known ? side != g_feedLine.hostSide : true);
        }
        const uint32_t a = g_feedLine.attacker;
        if (a >= 0x40000000u && a < 0xA0000000u && a != g_feedLine.victim && (g_feedLine.hostSide == 1 || g_feedLine.hostSide == 2))
        {
            const uint32_t team = Guest::Read32(base, a + 520);
            const uint32_t side = team ? Guest::Read32(base, team + 4) : 0;
            if (side == 1 || side == 2) paint(ctx.r5.u32, side != g_feedLine.hostSide);
        }
    }
    __imp__sub_82529D78(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82456920);
PPC_FUNC(sub_82456920)
{
    const uint32_t saved = Guest::Read32(base, 0x82BCBE9Cu);
    const bool swap = g_bots > 0 && saved != 0;
    if (swap) Guest::Write32(base, CurrentClient, 0);
    // 0.5.243: Justin: the bots he kills are no longer red in the killfeed.
    // Each name's colour is its entity's team (+520 -> +4, 1 or 2) against
    // the host's; anything else draws white. A bot's team word, read here,
    // is put to his side (his session player's +604) for the line if it is
    // not 1 or 2, and the values are logged for the first 40 lines.
    const uint32_t victim = ctx.r3.u32, attacker = ctx.r4.u32;
    auto teamOf = [&](uint32_t e) -> uint32_t { return (e >= 0x40000000u && e < 0xA0000000u) ? Guest::Read32(base, e + 520) : 0; };
    const uint32_t victimTeam = teamOf(victim), attackerTeam = teamOf(attacker);
    const uint32_t victimSide = victimTeam ? Guest::Read32(base, victimTeam + 4) : 99, attackerSide = attackerTeam ? Guest::Read32(base, attackerTeam + 4) : 99;
    uint32_t fixedTeam = 0, fixedWas = 0;
    const uint32_t entities = Guest::Read32(base, EntityList);
    const uint32_t host = entities ? Guest::Read32(base, entities + 4) : 0;
    const uint32_t hostTeam = teamOf(host);
    const uint32_t hostSide = hostTeam ? Guest::Read32(base, hostTeam + 4) : 0;
    // 0.5.245: Justin's rule: whoever dies is red, the killer keeps his
    // team's colour. sub_8244DB20 (team r3, colour out r4) draws a name red
    // when its team (1/2) is not the local player's; it is called twice by
    // this line, the victim's first (0x82456B68, r25 = victim) and then the
    // attacker's (0x82456C3C). The victim's call is given the other side
    // from the host's (the host's own death included). Team games only (the
    // line skips colours otherwise).
    g_feedLine = { true, victim, attacker, hostSide };
    {
        static int toldTeams = 0;
        if ((toldTeams < 40 && toldTeams++ < 40))
        {
            char a[48], b[48];
            printf("bots: killfeed line: victim %s side %u, attacker %s side %u, host side %u%s\n", WhoIs(base, victim, a), victimSide,
                   WhoIs(base, attacker, b), attackerSide, hostTeam ? Guest::Read32(base, hostTeam + 4) : 99,
                   " (both by side: green ours, red theirs)");
            fflush(stdout);
        }
    }
    __imp__sub_82456920(ctx, base);
    g_feedLine.active = false;
    if (fixedTeam) Guest::Write32(base, fixedTeam + 4, fixedWas);
    if (swap) Guest::Write32(base, CurrentClient, saved);
    static int told = 0;
    if (swap && told++ < 10) { printf("bots: killfeed line drawn as the host sees it (current client %u -> 0 and back)\n", saved); fflush(stdout); }
}

extern "C" PPC_FUNC(__imp__sub_82587268);
PPC_FUNC(sub_82587268)
{
    const uint32_t victim=ctx.r3.u32,slot=BotSlot(base,victim);
    if(slot>=FirstBotSlot() && slot<MaxClients && g_bot[slot]) {
        // 0.5.207: the death as the routine was given it (self, inflictor,
        // attacker, damage, means, weapon, direction, hit location; the
        // time offset is its ninth, on the stack at +0x54), for the joiners.
        BotDeathInfo& d=g_botDeath[slot];
        d.inflictor=ctx.r4.u32;d.attacker=ctx.r5.u32;d.damage=int32_t(ctx.r6.u32);d.means=int32_t(ctx.r7.u32);
        d.weapon=int32_t(ctx.r8.u32);d.hitLoc=int32_t(ctx.r10.u32);d.timeOffset=int32_t(Guest::Read32(base,ctx.r1.u32+0x54));
        d.haveDir=ctx.r9.u32>=0x40000000u && ctx.r9.u32<0xA0000000u;
        for(int k=0;k<3;k++)d.dir[k]=d.haveDir?ReadFloat(base,ctx.r9.u32+4*k):0.0f;
        const uint32_t ps=Guest::Read32(base,victim+512);
        if(ps) {
            const uint32_t first=Guest::Read8(base,ps+1065),second=Guest::Read8(base,ps+1066);
            uint32_t held=Guest::Read32(base,ps+164);
            if(!held)held=Guest::Read8(base,victim+3);
            auto owned=[&](uint32_t id) {
                return (id==first || id==second) && (Guest::Read32(base,ps+1056+4*(id>>5))&(1u<<(id&31)))!=0;
            };
            g_deathDrop[slot].Capture(victim,held,second,first,owned);
        }
    }
    static int calls = 0;
    if ((calls < 40 && calls++ < 40))
    {
        char a[48], b[48];
        printf("bots: player dies (sub_82587268): %s, killed by %s, means %u\n", WhoIs(base, ctx.r3.u32, a), WhoIs(base, ctx.r5.u32, b), ctx.r7.u32);
        fflush(stdout);
    }
    const uint32_t queue = Guest::Read32(base, 0x82A87860);
    static int queueShown = 0;   // 0.5.436: the first 40 deaths only
    const bool showQueue = queueShown < 40 && queueShown++ < 40;
    if (showQueue) { printf("bots: death queue BEFORE: object %08X count %u entries %08X\n", queue, Guest::Read32(base, queue), Guest::Read32(base, queue + 4)); fflush(stdout); }
    __imp__sub_82587268(ctx, base);
    if (showQueue) { printf("bots: death queue AFTER: object %08X count %u entries %08X\n", Guest::Read32(base, 0x82A87860), Guest::Read32(base, queue), Guest::Read32(base, queue + 4)); fflush(stdout); }
}

// 0.5.176: kill-crash evidence. This is the eight-entry native error queue,
// not a draw list. Log its producer and death transition before renderer drain.
extern "C" PPC_FUNC(__imp__sub_8210DCF8);
PPC_FUNC(sub_8210DCF8)
{
    const uint32_t queue = ctx.r3.u32, format = ctx.r5.u32, caller = uint32_t(ctx.lr);
    const uint32_t count = Guest::Read32(base, queue), severity = ctx.r4.u32;
    // 0.5.436: the first 40, then a count every 10 s. A 0.5.435 internet game
    // with bots logged "apsMemory : pool is empty" 2700 times a second: 64 MB
    // of log in 80 s, and the start of the game (the join) was lost.
    static std::atomic<uint32_t> calls{ 0 }, unshown{ 0 };
    static std::atomic<uint64_t> shownAt{ 0 };
    const bool show = calls.fetch_add(1) < 40;
    if (show)
    {
        char message[193] = {};
        if (format >= 0x40000000u && format < 0xA0000000u)
            for (size_t i = 0; i < sizeof(message) - 1; ++i) { const uint8_t c = Guest::Read8(base, format + uint32_t(i)); if (!c) break; message[i] = (c >= 32 && c < 127) ? char(c) : ' '; }
        printf("bots: native error enqueue: object %08X count %u severity %u caller %08X format %08X [%s] args %08X %08X %08X\n", queue, count, severity, caller, format, message, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32); fflush(stdout);
    }
    else
    {
        unshown.fetch_add(1);
        const uint64_t now = GetTickCount64();
        uint64_t last = shownAt.load();
        if (now - last >= 10000 && shownAt.compare_exchange_strong(last, now))
        {
            printf("bots: native error enqueue: %u more not shown (the last: caller %08X format %08X args %08X %08X %08X)\n", unshown.exchange(0), caller, format, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32); fflush(stdout);
        }
    }
    __imp__sub_8210DCF8(ctx, base);
    if (show) { printf("bots: native error enqueued: count %u entries %08X\n", Guest::Read32(base, queue), Guest::Read32(base, queue + 4)); fflush(stdout); }
}
extern "C" PPC_FUNC(__imp__sub_824309F0);
PPC_FUNC(sub_824309F0)
{
    const uint32_t queue = Guest::Read32(base, 0x82A87860);
    if (!queue)
    {
        static int missing = 0;
        if ((missing < 5 && missing++ < 5)) { printf("bots: error-queue pointer cleared; drain skipped, guest0 untouched; cause unresolved\n"); fflush(stdout); }
        ctx.r3.u64 = 0;
        return;
    }
    const uint32_t count = Guest::Read32(base, queue);
    if (count != 0)
    {
        printf("bots: native error drain: object %08X count %u entries %08X caller %08X\n", queue, count, Guest::Read32(base, queue + 4), uint32_t(ctx.lr)); fflush(stdout);
    }
    // Native producer8210DCF8 admits at most8; never walk a corrupt count
    // until the 2GB demand-commit limit. Keep evidence and quarantine count.
    if (count > 8)
    {
        printf("bots: CORRUPT error queue quarantined; count %u exceeds native capacity8; cause unresolved\n", count); fflush(stdout);
        Guest::Write32(base, queue, 0);
        ctx.r3.u64 = count;
        return;
    }
    __imp__sub_824309F0(ctx, base);
}

// 0.5.185: both183/184 hardware watches caught820D3638 overwriting the
// error-queue pointer while clearing the renderer's temporary list. Its
// negative-capacity branch allocates0 bytes, copies old count, then clears
// count anyway. Empty it BEFORE that copy; retain native ownership cleanup.
extern "C" PPC_FUNC(__imp__sub_820D3638);
PPC_FUNC(sub_820D3638)
{
    const uint32_t list=ctx.r3.u32;
    auto state=BotListClear::Prepare(list,
        [&](uint32_t at){return Guest::Read32(base,at);},
        [&](uint32_t at,uint32_t value){Guest::Write32(base,at,value);});
    static unsigned samples=0;
    if(state.checked && (state.discarded || samples++<6)) {
        printf("bots: renderer list clear %08X buffer %08X capacity %08X count %u caller %08X%s\n",
            list,state.buffer,state.capacity,state.count,uint32_t(ctx.lr),
            state.discarded?"; discarded elements BEFORE zero-byte copy":"");fflush(stdout);
    }
    __imp__sub_820D3638(ctx,base);
    if(state.discarded) {
        printf("bots: renderer list clear completed: buffer %08X capacity %08X count %u; error queue %08X\n",
            Guest::Read32(base,list),Guest::Read32(base,list+4),Guest::Read32(base,list+8),Guest::Read32(base,0x82A87860));fflush(stdout);
    }
}

// Native DropWeapon82580E38 checks the local-place list directly, not the
// generic bot-local hook. Answer only during our explicit bot drop call.
extern "C" PPC_FUNC(__imp__sub_823E5CC0);
PPC_FUNC(sub_823E5CC0)
{
    if(g_deathDropEntity && ctx.r4.u32==g_deathDropEntity && uint32_t(ctx.lr)==0x82580E74u) {
        ctx.r3.u64=1;return;
    }
    __imp__sub_823E5CC0(ctx,base);
}
extern "C" PPC_FUNC(__imp__sub_82580080);
PPC_FUNC(sub_82580080)
{
    const uint32_t owner=ctx.r3.u32,weapon=ctx.r4.u32;
    __imp__sub_82580080(ctx,base);
    if(g_deathDropEntity && owner==g_deathDropEntity) {
        g_deathDropCreated=ctx.r3.u32;
        printf("bots: native dropped item weapon %u entity %08X\n",weapon,g_deathDropCreated);fflush(stdout);
    }
}

// Used only by the existing native kit-name override, during this bot's
// own kit call. Human/custom-class settings and peer players are unchanged.
const char* BotsPrimaryName(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return "";
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return g_botKit[slot].primary;
    return "";
}

// 0.5.284: his kit's second main weapon (for the pistol: "" none) and
// whether his grenades are sticky bombs (the Demolitionist's).
const char* BotsSecondaryName(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return "";
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return g_botKit[slot].secondary;
    return "";
}
bool BotsKitSticky(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return false;
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return g_botKit[slot].sticky;
    return false;
}

// 0.5.329: whether his grenades are Mills bombs (Rusher 2's).
bool BotsKitMills(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return false;
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return g_botKit[slot].mills;
    return false;
}

// 0.5.753: his kit's smokes are World at War's signal flares (bot_kits.txt "flare")
bool BotsKitFlare(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return false;
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return g_botKit[slot].flare;
    return false;
}

// 0.5.198: his kit's grenades (-1: the class's own count).
bool BotsKitGrenades(uint8_t* base,uint32_t entity,int& frags,int& smokes,bool& american)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return false;
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
        { frags=g_botKit[slot].frags; smokes=g_botKit[slot].smokes; american=BotSide(base,slot)==1; return true; }
    return false;
}

int BotsBaseClass(uint8_t* base,uint32_t entity)
{
    const uint32_t entities=Guest::Read32(base,EntityList);
    if(!entity || !entities)return -1;
    for(uint32_t slot=FirstBotSlot();slot<ActiveSlots();++slot)
        if(g_bot[slot] && g_botLoadoutAssigned[slot] && Guest::Read32(base,entities+4*(slot+1))==entity)
            return int(g_botKit[slot].base);
    return -1;
}

// 0.5.276: for the killstreak sync (mp_fixes.cpp): a slot's session number
// (255: none) and this machine's own (255: no session).
int BotsSessionNumberOfSlot(uint8_t* base, uint32_t slot) { return SessionNumberOfSlot(base, slot); }
int BotsLocalSessionNumber(uint8_t* base) { return Session(base) ? BotRoster::LocalNumber(base) : 255; }

// 0.5.285: for the mortar killstreak (mp_fixes.cpp): a player's side, the
// ground under a point (a line straight down from top) and whether a line
// is clear of the world (ignore: an entity the line passes through).
int BotsSideOf(uint8_t* base, uint32_t entity) { return int(Side(base, entity)); }
bool BotsGroundZ(PPCContext& ctx, uint8_t* base, uint32_t ignore, float x, float y, float top, float& z)
{
    if (!ignore) return false;
    const BotNavigation::Hit hit = BotTrace(ctx, base, ignore, ignore, {x, y, top}, {x, y, top - 12000.0f}, false);
    if (!hit.valid || hit.fraction >= 1.0f) return false;
    z = hit.end.z;
    return std::isfinite(z);
}
// 0.5.608: the map's sky over x, y: a line up from `from` to the first thing it
// meets (the sky's brush, or a roof if `from` is under one); false if it starts
// in something solid or meets nothing within `reach` (the UAV plane's ceiling)
bool BotsSkyZ(PPCContext& ctx, uint8_t* base, uint32_t ignore, float x, float y, float from, float reach, float& z)
{
    if (!ignore) return false;
    const BotNavigation::Hit hit = BotTrace(ctx, base, ignore, ignore, {x, y, from}, {x, y, from + reach}, false);
    if (!hit.valid || hit.solid || hit.fraction <= 0.0f || hit.fraction >= 1.0f) return false;
    z = hit.end.z;
    return std::isfinite(z);
}
bool BotsLineClear(PPCContext& ctx, uint8_t* base, uint32_t ignore, float ax, float ay, float az, float bx, float by, float bz)
{
    if (!ignore) return true;
    const BotNavigation::Hit hit = BotTrace(ctx, base, ignore, ignore, {ax, ay, az}, {bx, by, bz}, false);
    return !hit.valid || hit.fraction > 0.97f;
}

#endif
