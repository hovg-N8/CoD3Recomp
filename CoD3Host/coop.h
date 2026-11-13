// Co-op v40 (Team Friend, Claude session, 2026-10-29): the campaign played
// the World at War way. The design is docs/COOP-V40-DESIGN.md; this header
// is shared by coop.cpp (the network, the lobby, the hooks), coop_host.cpp
// (the host's second player and his snapshots), coop_join.cpp (the joining
// copy's frozen world and its puppets) and coop_laststand.cpp (last stand,
// revive, mission failed). Campaign program only.
#pragma once
#ifndef COD3_MP

#include "kernel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Coop
{
    enum class Mode { Off, Host, Join };
    Mode GetMode();
    inline bool IsHost() { return GetMode() == Mode::Host; }
    inline bool IsJoin() { return GetMode() == Mode::Join; }
    double Now();                  // seconds since this process started

    // ---- guest memory -------------------------------------------------
    constexpr uint32_t EntityTable = 0x82A2DCD0;   // 4096 x (entity, generation)
    constexpr uint32_t InfoPool = 0x82A59DB0;
    // (v44t) a set anim queues a pose job in a small array with no bound (sub_824F39A0: count at 0x82A2A228,
    // 820-byte entries from 0x829C91F8, emptied by sub_824F38E8 once a frame in G_RunFrame). The game makes
    // a few a frame; ours, a set anim for every animation of every puppet new at once (a checkpoint), ran
    // over it and wrote over what follows: the crash in sub_8216E680. Ours wait their turn now.
    inline bool AnimJobRoom(const uint8_t* base) { return Guest::Read32(base, 0x82A2A228) < 140; }   // (v46: the queue posed early when full (coop_join.cpp sub_824F39A0), so no longer kept to a few of ours a frame: with 6 (v45r), in a busy frame never room, and a body or a puppet waited in its bind pose)      // animation records, 52 bytes
    constexpr uint32_t Level = 0x82A4E790;
    constexpr uint32_t ScriptVm = 0x82A90128;
    constexpr uint32_t HudElems = 0x829B01B8;      // 16 x 124 bytes
    inline bool Valid(uint32_t a) { return a >= 0x40000000u && a < 0xC0000000u; }
    inline float F(const uint8_t* base, uint32_t a) { const uint32_t w = Guest::Read32(base, a); float f; memcpy(&f, &w, 4); return f; }
    inline void SetF(uint8_t* base, uint32_t a, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, a, w); }
    inline uint32_t EntityAt(const uint8_t* base, uint32_t i) { if (i >= 4096) return 0; const uint32_t e = Guest::Read32(base, EntityTable + i * 8); return Valid(e) ? e : 0; }
    inline uint32_t GenAt(const uint8_t* base, uint32_t i) { return Guest::Read32(base, EntityTable + i * 8 + 4); }
    inline uint32_t RefOf(const uint8_t* base, uint32_t i) { return i | (GenAt(base, i) << 14); }
    inline uint32_t FromRef(const uint8_t* base, uint32_t ref) { const uint32_t i = ref & 0x3FFF; return (i < 4096 && (ref >> 14) == GenAt(base, i)) ? EntityAt(base, i) : 0; }
    int32_t IndexOf(const uint8_t* base, uint32_t e);   // the entity's number (its +500 low bits, checked), -1 if none
    std::string Text(const uint8_t* base, uint32_t a, uint32_t max = 96);
    std::string BrocText(const uint8_t* base, uint32_t field);   // a Broc::string field
    uint32_t ModelSlot(const uint8_t* base, uint32_t e, int slot);   // the model resource in slot 0..3
    std::string ResName(const uint8_t* base, uint32_t res);           // a model resource's name (+72)
    uint32_t GuestAlloc(uint32_t size);                                // guest memory for small things (never freed)
    uint32_t GuestText(uint8_t* base, const std::string& text);        // a kept copy of a text in guest memory
    uint32_t GuestBroc(uint8_t* base, const std::string& text);        // a slot holding a kept Broc::string
    uint32_t Scratch(uint8_t* base);                                   // 256 bytes for call arguments
    PPCContext CallCtx(PPCContext& ctx);
    void CallGuest(PPCContext& ctx, uint8_t* base, uint32_t function);  // ctx's registers set by the caller
    // this copy's player (entity 1 in a mission; 0 in a restarted one, test 9)
    uint32_t Me(const uint8_t* base);
    inline uint32_t Record(const uint8_t* base, uint32_t e) { const uint32_t r = e ? Guest::Read32(base, e + 528) : 0; return Valid(r) || (r >= 0x82000000u && r < 0x83000000u) ? r : 0; }
    bool InMission(const uint8_t* base);
    void DeleteEntity(PPCContext& ctx, uint8_t* base, uint32_t index);   // the scripts' delete
    void LinkEntity(PPCContext& ctx, uint8_t* base, uint32_t e);
    void Notice(const std::string& text, int ms = 3000);

    // ---- the network ----------------------------------------------------
    // Message kinds. Unreliable ones go as they are; reliable ones ride in
    // REL packets until the other side acknowledges them, in order.
    enum Kind : uint8_t
    {
        K_HELLO = 1,      // joining -> host: hello, its version and name
        K_BEACON = 2,     // host -> joining: the mission and its count
        K_SNAP = 3,       // host -> joining: a piece of a snapshot
        K_PS = 4,         // host -> joining: the joining player's state there
        K_CMD = 5,        // joining -> host: his command, view and place
        K_REL = 6,        // reliable messages
        K_FILM = 7,       // the mission's film (ready / over)
        K_FX = 8,         // (0.5.872) host -> joining: an effect played there (coop_events.cpp)
    };
    enum RelKind : uint8_t
    {
        R_CONFIGSTRING = 1,   // u16 index, text
        R_SERVERCMD = 2,      // text (not a config string's "cs")
        R_NAME = 3,           // u16 id, u8 what (1 model, 2 anim list, 3 tag, 4 class), text
        R_LASTSTAND = 4,      // the last stand state of the two players
        R_MAPSTART = 5,       // u32 count, the mission's name
        R_EVENT = 6,          // u8 what, data: a sound, an effect, a message
        R_HUD = 7,            // the HUD elements (16 x 124 bytes, changed ones)
        R_MISSION = 8,        // u8 what (1 failed, 2 restarted, 3 ended), text
        R_HIT = 9,            // host -> joining: he was hit (damage, from where)
        R_DOWN = 10,          // joining -> host: a request (revive held, give up)
        R_INV = 12,           // (0.5.901) host -> joining: the weapons and rounds he has there (coop_inventory.cpp)
        R_INVMINE = 13,       // (0.5.902) joining -> host: what his own copy owns (a pickup there is his on the host too)
        R_SELFHIT = 11,       // joining -> host: his own game hurt him (a fall, a trigger): i32 damage, i32 means
    };
    constexpr uint32_t ProtocolVersion = 40007;
    bool NetOpen();
    int NetPort();               // the host's UDP port (28970; the joining copy's is one more, the lobby's search two more)
    void SetMode(Mode mode, const std::string& hostAddress = std::string());   // the lobby's choice (game thread, outside a mission)
    void NetForgetPeer();        // the other side said goodbye
    void NetPoll(PPCContext& ctx, uint8_t* base, bool full);    // reads everything waiting (full: in the frame)
    void SendUnreliable(Kind kind, const void* data, size_t size);
    void SendReliable(RelKind kind, const void* data, size_t size);
    bool PeerConnected();
    double PeerSeenAt();
    // the other side's clock: the sender's time to show now (the joining copy
    // shows the host's world this far in the past)
    double PeerTimeNow();
    double ShowTime();
    double PacketTime();         // the sending side's clock when the packet being read was sent
    std::string& MyMap();
    uint32_t MissionCount();     // the host's count of missions and restarts
    uint32_t& MissionCounter();
    void FollowMap(const std::string& name, uint32_t count);   // the joining copy: go where the host is
    void ReloadMission();
    bool SwapPending();          // (v46) this copy is a checkpoint reload's, still hidden behind the old one
    void SwapReveal(const char* why);
    void SwapFrame();
    void NetSilence();           // (v46) the old copy of a swap: its socket let go for good (the new one takes the port)
    bool Swapping();             // (v46) the old copy: waiting for its new one (the loading screen)        // (v45) the joining copy: load its mission again (the host went back to a checkpoint)
    std::string PeerName();      // the other player's name (from the lobby's messages)
    std::string MyName();        // this player's (the settings' Player name, else the Windows user's)

    // ---- the lobby (coop_lobby.cpp): World at War's co-op lobby -----------
    // F2: host a game or find one (on the network, or by address), both
    // players listed with their ready marks, the host's mission, difficulty
    // and scoring, a countdown, then both into the mission together.
    void LobbyFrame(PPCContext& ctx, uint8_t* base);   // each frame, co-op on or off
    void Lobby(PPCContext& ctx, uint8_t* base);        // co-op on: hello and beacon, following the host
    bool LobbyReceive(uint8_t* base, Kind kind, const uint8_t* data, size_t size);
    bool ScoringOn();            // the lobby's competitive scoring

    // ---- the host's side (coop_host.cpp) --------------------------------
    void HostFrame(PPCContext& ctx, uint8_t* base);
    void HostReceive(PPCContext& ctx, uint8_t* base, Kind kind, const uint8_t* data, size_t size);
    void HostReceiveReliable(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size);
    void HostMissionStart(const std::string& name);
    void HostNewPeer();   // (v45b) a new joining process: names, config strings and dvars again when it comes in
    void HostClientThink(PPCContext& ctx, uint8_t* base);       // after client 0's think
    void HostRespawn(PPCContext& ctx, uint8_t* base, int who);   // (0.5.869) a downed player (0 the host, 1 the joining player) back in beside the other
    uint32_t BodyMoveHash(int move);     // (0.5.882) the bodies' moves by name (coop_host.cpp)
    const char* BodyMoveText(int move);
    int BodyAct(int move, uint32_t act); // a way of moving (0..12) with what he does (1 firing, 2 throwing, 4 aiming)
    uint32_t HostActBits(const uint8_t* base, int who);
    // (0.5.891) how a body moves, steadied. Test t890a: the other player "flickers when moving": his way of
    // moving was chosen every frame from the step since the frame before, and his place comes in steps (a
    // snapshot, then frames with no step): forward, left, forward, left, ten times a second. The speed and the
    // direction are now taken over the last 0.3 s, the axis (forward/back or sideways) is kept unless the other
    // is clearly more, and a new way of moving must be wanted for 0.15 s before it is taken.
    struct BodyMotion
    {
        struct S { double t; float x, y; };
        S q[64] = {}; int n = 0;
        float vx = 0, vy = 0, speed = 0; bool forwardAxis = true;
        int base = -1, cand = -1; double candAt = 0;
        void Feed(double now, float x, float y)
        {
            if (n && (fabsf(x - q[n - 1].x) > 300.0f || fabsf(y - q[n - 1].y) > 300.0f)) n = 0;   // (put somewhere else: not a move)
            int drop = 0; while (drop < n - 1 && now - q[drop].t > 0.3) drop++;
            if (n == 64 && !drop) drop = 1;
            if (drop) { memmove(q, q + drop, sizeof(S) * size_t(n - drop)); n -= drop; }
            q[n++] = S{ now, x, y };
            const double dt = now - q[0].t;
            if (dt >= 0.1) { vx = float((x - q[0].x) / dt); vy = float((y - q[0].y) / dt); speed = std::min(sqrtf(vx * vx + vy * vy), 600.0f); }
            else if (n == 1) { vx = vy = speed = 0; }
        }
        bool ForwardAxis(float fwd, float side) { forwardAxis = forwardAxis ? fabsf(fwd) * 1.5f >= fabsf(side) : fabsf(fwd) > fabsf(side) * 1.5f; return forwardAxis; }
        // (0.5.892) one throw, swing or reload is one animation from its start to its end: it changed between
        // its standing and its crouched kind in the middle (t891a: "crouch_grenade_throw / stand_grenade_throw")
        int actMove = -1; uint32_t actWas = 0;
        int Act(uint32_t act, int moved) { if (act >= 3 && act == actWas && actMove >= 0) return actMove; actWas = act; actMove = act >= 3 ? moved : -1; return moved; }
        int Settle(double now, int want)
        {
            if (base < 0 || want == base) { base = want; cand = -1; return base; }
            if (want != cand) { cand = want; candAt = now; }
            if (now - candAt >= 0.15) { base = want; cand = -1; }
            return base;
        }
    };
    // (0.5.892) the rifle butt, from the weapon state (the record's +136): 10 is the swing, 11 what follows it,
    // but 11 follows a grenade's throw too (t891a: 3 -> 11 with weapon animation 2): every throw ended in a
    // swing of the rifle butt on the other player's screen. 11 counts only when it came from 10.
    inline bool MeleeState(uint32_t ws, uint32_t& before) { if (ws != 11) before = ws; return ws == 10 || (ws == 11 && before == 10); }
    // the button that throws (the record's +1352): 0x2. (0.5.902) 0x4000 is NOT a throw: it is held while crouched
    // (t901u: 0x4000 comes and goes with view height 40; 0x2000 with prone), so every crouch looked like a throw
    constexpr uint32_t ThrowButtons = 0x2u;
    inline bool OneShotMove(int move) { return move >= 26; }   // throws, reloads, swings: played once, held at their end
    int TidyBodyTree(uint8_t* base, uint32_t tree, int32_t anim, int32_t tiltAnim = -1, float tilt = 0.0f);
    // (0.5.895) the aim up and down: the aim-up or aim-down animation of a still, aiming or firing body, set in its
    // tree and kept going; returns its number (or -1) and how much of it shows (0..1, the pitch over 45 degrees)
    int BodyTilt(PPCContext& ctx, uint8_t* base, uint32_t tree, int move, float pitch, float& share);
    inline float Pitch180(float p) { while (p > 180.0f) p -= 360.0f; while (p < -180.0f) p += 360.0f; return p; }
    constexpr float AimPitchMin = 10.0f;   // looking this far up or down, a still body takes its aiming pose
    //   // (0.5.880) a body's tree: no parent node weighing with nothing under it
    bool NearHisBlast(const float p[3]);   // (0.5.916) the host: an effect here is the blast of the joining player's own missile
    bool JoinerUseHeld();        // (0.5.875) the host: the joining player holds Use (his pad's X)
    uint32_t SecondPlayer();     // the host: client 1's entity (0 when none)
    uint32_t SecondBody();       // the host: the entity drawn for the joining player
    uint32_t HostBody();         // the host: the entity drawn for the host (hidden here)
    bool IsPlayerEntity(const uint8_t* base, uint32_t e);   // the host's player or client 1
    void HostScriptDvar(const std::string& name, const std::string& value, uint32_t flags);   // a level script's setdvar
    enum EventKind : uint8_t { E_DVAR = 1, E_CALL = 2 };   // R_EVENT's first byte
    // coop_events.cpp: the host's sounds, speech, music and earthquakes, played on the joining copy too
    void EventsReceive(const uint8_t* data, size_t size);
    void FxForward(PPCContext& ctx, uint8_t* base);          // (0.5.872) the host: an effect being played (sub_82423F08)
    void FxQueueForward(PPCContext& ctx, uint8_t* base);    // (0.5.881) the host: the waiting effects about to be played (sub_820FBD00)
    void FxReceive(const uint8_t* data, size_t size);        // the joining copy: one of the host's
    void EventsFrame(PPCContext& ctx, uint8_t* base, bool frozen);

    // ---- the joining copy's side (coop_join.cpp) ------------------------
    void JoinFrame(PPCContext& ctx, uint8_t* base);
    void JoinReceive(PPCContext& ctx, uint8_t* base, Kind kind, const uint8_t* data, size_t size);
    void JoinReceiveReliable(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size);
    void JoinMissionStart(const std::string& name);
    void JoinMissionLoaded(uint8_t* base);   // (v46) after SV_SpawnServer: a reload in this process is done
    // (0.5.901) coop_inventory.cpp: the joining player's weapons and rounds, kept by the host
    void TestGo(PPCContext& ctx, uint8_t* base, uint32_t me, uint32_t rec);
    void TestLive(uint8_t* base, uint32_t me);
    void TestViewScan(uint8_t* base, uint32_t me);   // (0.5.983) COD3_COOP_VIEWSCAN: where the client game keeps its view
    void TestAimModel(PPCContext& ctx, uint8_t* base, uint32_t me);   // (0.5.977) COD3_COOP_TESTAIMMODEL: turned to things of a model   // (0.5.972) COD3_COOP_TESTLIVE: Use held by a grenade lying near this copy's player   // (0.5.907) COD3_COOP_TESTGO: a test puts this copy's player somewhere
    void InventoryHostFrame(uint8_t* base, uint32_t hostRec, uint32_t secondRec);
    void InventoryJoinReceive(const uint8_t* data, size_t size);
    void InventoryHostReceive(const uint8_t* data, size_t size);   // (0.5.902)
    void InventoryJoinFrame(uint8_t* base, uint32_t rec);
    bool JoinFrozen();           // the joining copy's world is the host's now
    bool JoinIsPuppet(const uint8_t* base, uint32_t e);
    void JoinOwnScriptDvar(const std::string& name, const std::string& value);   // our own scripts' setdvar (before the freeze)
    // the host's player as the joining copy draws him (his body's place, else the host's word) and his health
    bool JoinPartner(const uint8_t* base, float pos[3], int& health, int& maxHealth);
    // the joining copy's entity drawn for a host's entity number (0xFFFE: the host's player, 0xFFFD: mine), 0 if none
    uint32_t JoinLocalFor(const uint8_t* base, uint16_t hostNumber);

    // ---- the co-op HUD (coop_hud.cpp) -------------------------------------
    struct HudState
    {
        bool active = false, havePartner = false, fill = true, nearDowned = false, scoring = true;
        int me = 0;                       // 0: this copy is the host's, 1: the joining player's
        int fov = 65;
        char names[2][32] = {};
        bool down[2] = {};
        float bleed[2] = {}, revive[2] = {}, bleedTotal = 30.0f;
        int health[2] = {}, maxHealth[2] = {};
        float pos[2][3] = {};             // the players' feet
        float eye[3] = {}, view[3] = {};  // this copy's view
        int kills[2] = {}, revives[2] = {}, downs[2] = {}, score[2] = {}, points[2] = {};
        double pointsUntil = 0, scoresUntil = 0;
    };
    void HudUpdate(const HudState& s);   // the game thread, each frame
    void HudFrame(PPCContext& ctx, uint8_t* base);   // fills a HudState from both sides' state and hands it over

    // ---- WaW's rules (coop_laststand.cpp) --------------------------------
    void LastStandFrame(PPCContext& ctx, uint8_t* base);
    void LastStandReceive(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size);
    void LastStandMissionStart();
    // the host: a player brought to 0: true when last stand takes him instead of death
    bool LastStandTakes(PPCContext& ctx, uint8_t* base, uint32_t player, int32_t damage, int32_t means, uint32_t attacker);
    bool PlayerDown(int who);    // 0 the host, 1 the joining player (the joining copy: as the host last said)
    void DownStance(uint8_t* base, uint32_t player, bool down);   // a downed player lies down
    void LastStandHud(HudState& s);   // down, bleed-out, revive and the scores, for the HUD (either side)
    void InjectDamage(PPCContext& ctx, uint8_t* base, uint32_t target, int32_t damage, int32_t means);   // the host: a hit given on purpose
    double TestTime(const char* env);   // COD3_COOP_TEST*: seconds into the mission (or -1)
    double MissionTime();               // seconds since this copy's player came into the mission
}

#endif
