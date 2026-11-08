// Xbox 360 kernel imports: the Xam layer, which on the console is the
// dashboard. Profiles, controllers, notifications, storage devices, and the
// system dialogs all come through here.
//
// None of it is backed by anything real. A single local player is reported so
// the title has a profile to work with, no controller is connected, no
// notification ever arrives, and every system dialog answers as if the user
// cancelled it immediately. Those answers are chosen to be the ones a title
// handles as a normal case rather than an error.

#include "debug_probe.h"
#include "kernel.h"
#include "log.h"
#include "sampler.h"
#include <atomic>
#include <chrono>
#include "input.h"
#include "settings.h"
#include "profile_store.h"
#include <fstream>

#include <cstdio>
#include <mutex>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cctype>

#include <Windows.h>

PPC_FUNC(__imp__NtSetEvent);

namespace
{
    constexpr uint32_t X_ERROR_SUCCESS            = 0x00000000;
    constexpr uint32_t X_ERROR_NO_SUCH_USER       = 0x00000525;
    constexpr uint32_t X_ERROR_FUNCTION_FAILED    = 0x0000045B;
    constexpr uint32_t X_ERROR_DEVICE_NOT_CONNECTED = 0x0000048F;
    constexpr uint32_t X_ERROR_NOT_FOUND          = 0x00000490;
    constexpr uint32_t X_ERROR_IO_PENDING         = 0x000003E5;

    constexpr uint32_t SignedInUser = 0;
    // The bot experiment (debug.txt bot_pads): players 2 to 4 as controllers
    // this runtime drives itself, signed in locally as "Bot 1" to "Bot 3".
    bool IsBot(uint32_t userIndex)
    {
        return userIndex >= 1 && userIndex <= 3 && int(userIndex) <= DebugProbe::BotPads();
    }
    // Who the player is. On the console a profile; here this machine's
    // Windows user. The XUID has to differ from one machine to the next:
    // two players with one XUID are one player to the title, and the second
    // to join a System Link game would be refused as already in it. An
    // offline XUID (0xE000...) made from the computer's and the user's
    // names stays the same on this machine from one run to the next.
    uint64_t PlayerXuid()
    {
        static const uint64_t xuid = [] {
            char computer[MAX_COMPUTERNAME_LENGTH + 1] = {};
            DWORD computerLength = sizeof(computer);
            GetComputerNameA(computer, &computerLength);
            char user[257] = {};
            DWORD userLength = sizeof(user);
            GetUserNameA(user, &userLength);
            uint64_t hash = 0xCBF29CE484222325ull;   // FNV-1a
            for (const char* text : { static_cast<const char*>(computer), "/", static_cast<const char*>(user) })
                for (const char* c = text; *c != 0; c++) { hash ^= uint8_t(*c); hash *= 0x100000001B3ull; }
            return 0xE000000000000000ull | (hash & 0x0000FFFFFFFFFFFFull);
        }();
        return xuid;
    }

    // The gamertag: the settings menu's, or the Windows user name, in the
    // characters a gamertag can have and no longer than one (15).
    std::string PlayerName()
    {
        std::string name = Settings::Get().playerName;
        if (name.empty())
        {
            char user[257] = {};
            DWORD length = sizeof(user);
            if (GetUserNameA(user, &length)) name = user;
        }
        std::string clean;
        for (char c : name)
            if (c >= 0x20 && c < 0x7F && clean.size() < 15) clean.push_back(c);
        return clean.empty() ? std::string("Player") : clean;
    }
}

// --- The signed in user ----------------------------------------------------

// ULONG XamUserGetSigninState(ULONG userIndex)
PPC_FUNC(__imp__XamUserGetSigninState)
{
    Kernel::CountImport("XamUserGetSigninState");
    // 0 not signed in, 1 signed in locally, 2 signed in to Live.
    ctx.r3.u32 = (ctx.r3.u32 == SignedInUser || IsBot(ctx.r3.u32)) ? 1 : 0;
}

// X_RESULT XamUserGetName(ULONG userIndex, char* buffer, ULONG length)
PPC_FUNC(__imp__XamUserGetName)
{
    Kernel::CountImport("XamUserGetName");
    const uint32_t userIndex = ctx.r3.u32;
    const uint32_t buffer = ctx.r4.u32;
    const uint32_t length = ctx.r5.u32;

    if ((userIndex != SignedInUser && !IsBot(userIndex)) || buffer == 0 || length == 0)
    {
        ctx.r3.u32 = X_ERROR_NO_SUCH_USER;
        return;
    }

    const std::string name = IsBot(userIndex) ? "Bot " + std::to_string(userIndex) : PlayerName();
    const uint32_t copy = std::min<uint32_t>(length - 1, uint32_t(name.size()));
    memcpy(Guest::Ptr(buffer), name.data(), copy);
    *(Guest::Ptr(buffer) + copy) = '\0';
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XamUserGetXUID(ULONG userIndex, ULONG type, XUID* out)
PPC_FUNC(__imp__XamUserGetXUID)
{
    Kernel::CountImport("XamUserGetXUID");
    static std::atomic<int> announced{ 0 };
    if ((announced.load(std::memory_order_relaxed) < 20 && announced.fetch_add(1) < 20))
    {
        printf("xam: XamUserGetXUID(user %u, type 0x%X) from 0x%08X\n",
            ctx.r3.u32, ctx.r4.u32, uint32_t(ctx.lr));
        fflush(stdout);
    }
    if ((ctx.r3.u32 != SignedInUser && !IsBot(ctx.r3.u32)) || ctx.r5.u32 == 0)
    {
        ctx.r3.u32 = X_ERROR_NO_SUCH_USER;
        return;
    }
    Guest::Write64(base, ctx.r5.u32, PlayerXuid() + (IsBot(ctx.r3.u32) ? ctx.r3.u32 * 0x1000 : 0));
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

namespace
{
    uint32_t CompleteOverlapped(PPCContext&, uint8_t*, uint32_t, uint32_t, uint32_t length = 0);
    std::mutex g_profileMutex;
    std::filesystem::path ProfileFile(uint32_t user)
    { return Kernel::SavesRoot() / "profiles" / ("user-" + std::to_string(user) + ".bin"); }
    ProfileStore::Values ReadProfile(uint32_t user)
    {
        ProfileStore::Values values;
        if (user > 3) return values;
        std::ifstream file(ProfileFile(user), std::ios::binary | std::ios::ate);
        if (!file) return values;
        const auto size = file.tellg();
        if (size < 12 || size > 9 * 1024 * 1024) return values;
        std::vector<uint8_t> data(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char*>(data.data()), data.size()) || !ProfileStore::Decode(data, values))
            printf("profile: ignored invalid saved profile for user %u\n", user);
        return values;
    }
    bool WriteProfile(uint32_t user, const ProfileStore::Values& values)
    {
        const auto data = ProfileStore::Encode(values);
        if (data.empty()) return false;
        const auto path = ProfileFile(user);
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        const auto temp = std::filesystem::path(path.wstring() + L".tmp");
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            if (!file.write(reinterpret_cast<const char*>(data.data()), data.size())) return false;
            file.flush();
            if (!file) return false;
        }
        return MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    }
    bool ProfileRange(uint32_t pointer, uint32_t size)
    { return pointer != 0 && uint64_t(pointer) + size <= Guest::TotalSize; }
}

// X_RESULT XamUserReadProfileSettings(titleId, userIndex, unk, unk,
//                                     settingCount, settingIds*, sizePtr*,
//                                     buffer*, overlapped)
PPC_FUNC(__imp__XamUserReadProfileSettings)
{
    Kernel::CountImport("XamUserReadProfileSettings");
    const uint32_t settingCount = ctx.r7.u32;
    const uint32_t sizePtr = ctx.r9.u32;
    const uint32_t buffer = ctx.r10.u32;
    if (ctx.r4.u32 > 3 || settingCount > ProfileStore::MaxEntries || !ProfileRange(sizePtr, 4) ||
        (settingCount && !ProfileRange(ctx.r8.u32, settingCount * 4)))
    { ctx.r3.u32 = 87; return; }
    static std::atomic<int> announced{ 0 };
    if ((announced.load(std::memory_order_relaxed) < 12 && announced.fetch_add(1) < 12))
    {
        printf("xam: XamUserReadProfileSettings(title 0x%08X, user %u, xuids %u/0x%08X, %u settings:",
            ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, settingCount);
        for (uint32_t i = 0; i < settingCount && i < 16 && ctx.r8.u32 != 0; i++)
            printf(" %08X", Guest::Read32(base, ctx.r8.u32 + i * 4));
        printf(", size %u, buffer 0x%08X, overlapped 0x%08X) from 0x%08X\n",
            sizePtr != 0 ? Guest::Read32(base, sizePtr) : 0, buffer,
            Guest::Read32(base, ctx.r1.u32 + 0x54), uint32_t(ctx.lr));
        fflush(stdout);
    }

    // Verified native consumer 82414788: source+0, user+8, id+16,
    // XUSER_DATA type+24 and union+32. Forty bytes per entry, not24.
    constexpr uint32_t HeaderSize = 8;
    constexpr uint32_t EntrySize = 40;
    const uint32_t settingIds = ctx.r8.u32, user = ctx.r4.u32;
    if (user > 3 || settingCount > ProfileStore::MaxEntries || !ProfileRange(sizePtr, 4) ||
        (settingCount && !ProfileRange(settingIds, settingCount * 4)))
    { ctx.r3.u32 = 87; return; }
    std::unique_lock<std::mutex> lock(g_profileMutex);
    const auto values = ReadProfile(user);
    lock.unlock(); // Completion callbacks may issue another profile request.
    uint32_t required = HeaderSize + settingCount * EntrySize;
    for (uint32_t i=0;i<settingCount;++i)
    {
        const auto found = values.find(Guest::Read32(base, settingIds + i*4));
        if (found != values.end() && (found->second.type == 4 || found->second.type == 6))
            required += (uint32_t(found->second.bytes.size()) + 7) & ~7u;
    }

    if (buffer == 0)
    {
        // The title is asking how much room it needs.
        if (sizePtr != 0)
            Guest::Write32(base, sizePtr, required);
        ctx.r3.u32 = 0x0000007A;   // ERROR_INSUFFICIENT_BUFFER
        return;
    }

    if (sizePtr != 0 && Guest::Read32(base, sizePtr) < required)
    {
        Guest::Write32(base, sizePtr, required);
        ctx.r3.u32 = 0x0000007A;
        return;
    }
    if (!ProfileRange(buffer, required)) { ctx.r3.u32 = 87; return; }
    memset(Guest::Ptr(buffer), 0, required);
    Guest::Write32(base, buffer + 0, settingCount);
    Guest::Write32(base, buffer + 4, buffer + HeaderSize);

    uint32_t payload = buffer + HeaderSize + settingCount * EntrySize;
    for (uint32_t i = 0; i < settingCount; i++)
    {
        const uint32_t entry = buffer + HeaderSize + i * EntrySize;
        const uint32_t id = Guest::Read32(base, settingIds + i*4);
        Guest::Write32(base, entry + 8, user);
        Guest::Write32(base, entry + 16, id);
        const auto found = values.find(id);
        if (found == values.end()) continue; // source0 means unset/default
        const auto& value = found->second;
        Guest::Write32(base, entry, 2);
        Guest::Write8(base, entry + 24, uint8_t(value.type));
        if (value.type == 4 || value.type == 6)
        {
            Guest::Write32(base, entry + 32, uint32_t(value.bytes.size()));
            Guest::Write32(base, entry + 36, payload);
            if (!value.bytes.empty()) memcpy(base + payload, value.bytes.data(), value.bytes.size());
            payload += (uint32_t(value.bytes.size()) + 7) & ~7u;
        }
        else memcpy(base + entry + 32, value.bytes.data(), value.bytes.size());
    }
    Guest::Write32(base, sizePtr, required);
    printf("profile: read %u settings for user %u, %zu saved values\n", settingCount, user, values.size());
    fflush(stdout);
    ctx.r3.u32 = CompleteOverlapped(ctx, base, Guest::Read32(base, ctx.r1.u32 + 0x54), X_ERROR_SUCCESS);
}

// X_RESULT XamUserWriteProfileSettings(...)
PPC_FUNC(__imp__XamUserWriteProfileSettings)
{
    Kernel::CountImport("XamUserWriteProfileSettings");
    // Native writer824135D8 and wrapper8222C0C8 verify these arguments.
    const uint32_t user=ctx.r4.u32, count=ctx.r5.u32, records=ctx.r6.u32, overlap=ctx.r7.u32;
    if (user > 3 || count > ProfileStore::MaxEntries || (count && !ProfileRange(records, count*40)))
    { ctx.r3.u32 = 87; return; }
    uint32_t result = X_ERROR_SUCCESS;
    {
        std::lock_guard<std::mutex> lock(g_profileMutex);
        auto values=ReadProfile(user);
        for(uint32_t i=0;i<count;++i)
        {
            const uint32_t entry=records+i*40, id=Guest::Read32(base,entry+16);
            const uint32_t type=Guest::Read8(base,entry+24);
            if(!type) { values.erase(id); continue; }
            uint32_t size=(type==2 || type==3)?8:4, data=entry+32;
            if(type==4 || type==6) { size=Guest::Read32(base,entry+32); data=Guest::Read32(base,entry+36); }
            if(size>ProfileStore::MaxPayload || (size && !ProfileRange(data,size))) { result=87; break; }
            ProfileStore::Value value{type, {}};
            if(size) value.bytes.assign(base+data,base+data+size);
            if(!ProfileStore::Valid(value)) { result=87; break; }
            values[id]=std::move(value);
        }
        if(!result && !WriteProfile(user,values)) result=5;
    }
    printf("profile: save user %u, %u settings, result %u\n",user,count,result); fflush(stdout);
    ctx.r3.u32=CompleteOverlapped(ctx,base,overlap,result);
}

// --- Controllers -----------------------------------------------------------

// The controller.
//
// Reporting no pad attached is not a neutral answer. The title polls for one
// about five hundred thousand times a second while it waits, which is what made
// it sit on its loading screen and never move: it was not loading, it was
// waiting for a controller that this runtime kept saying was not there. A pad
// on the first port with nothing pressed is the honest stand in until real
// input is wired up.

// X_RESULT XamInputGetState(ULONG userIndex, ULONG flags, XINPUT_STATE* state)
PPC_FUNC(__imp__XamInputGetState)
{
    Kernel::CountImport("XamInputGetState");
    {
        // One look at the thread pointer. Guest code reads thread state
        // through r13, and if this runtime never sets it up, everything it
        // reads through it is whatever happens to be at address zero.
        static std::atomic<int> looked{ 0 };
        if ((looked.load(std::memory_order_relaxed) < 3 && looked.fetch_add(1) < 3))
        {
            const uint32_t pcr = ctx.r13.u32;
            printf("thread pointer r13 = 0x%08X", pcr);
            if (pcr != 0)
            {
                const uint32_t thread = Guest::Read32(base, pcr + 0x100);
                printf(", *(r13+0x100) = 0x%08X", thread);
                if (thread != 0)
                    printf(", id = %u, +0x58 = %u",
                        Guest::Read32(base, thread + 0x14C),
                        Guest::Read32(base, thread + 0x58));
            }
            printf("\n");
            fflush(stdout);
        }
    }

    if (IsBot(ctx.r3.u32))
    {
        if (ctx.r5.u32 != 0)
        {
            static uint32_t packet = 0;
            int16_t lx, ly, rx, ry;
            DebugProbe::BotSticks(lx, ly, rx, ry);
            memset(Guest::Ptr(ctx.r5.u32), 0, 16);
            Guest::Write32(base, ctx.r5.u32 + 0, ++packet);
            Guest::Write16(base, ctx.r5.u32 + 4, DebugProbe::BotButtons());
            Guest::Write16(base, ctx.r5.u32 + 8, uint16_t(lx));
            Guest::Write16(base, ctx.r5.u32 + 10, uint16_t(ly));
            Guest::Write16(base, ctx.r5.u32 + 12, uint16_t(rx));
            Guest::Write16(base, ctx.r5.u32 + 14, uint16_t(ry));
        }
        ctx.r3.u32 = X_ERROR_SUCCESS;
        return;
    }
    if (ctx.r3.u32 != SignedInUser)
    {
        if (ctx.r5.u32 != 0) memset(Guest::Ptr(ctx.r5.u32), 0, 16);
        ctx.r3.u32 = X_ERROR_DEVICE_NOT_CONNECTED;
        return;
    }

    // COD3_PADRATE=1: how often the title reads the player's pad, once a
    // second, and how many of the reads come within two milliseconds of the
    // one before (a second read in the same frame).
    {
        static const bool rateLog = getenv("COD3_PADRATE") != nullptr;
        if (rateLog)
        {
            static uint64_t reads = 0, close = 0, second = 0;
            static auto last = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            reads++;
            if (now - last < std::chrono::milliseconds(2)) close++;
            // The gaps between reads, in milliseconds, as a histogram.
            static uint64_t gaps[8] = {};
            const long long us = std::chrono::duration_cast<std::chrono::microseconds>(now - last).count();
            gaps[us < 500 ? 0 : us < 1000 ? 1 : us < 2000 ? 2 : us < 4000 ? 3 : us < 6000 ? 4 : us < 9000 ? 5 : us < 14000 ? 6 : 7]++;
            last = now;
            const uint64_t tick = GetTickCount64() / 1000;
            if (tick != second)
            {
                second = tick;
                printf("xam: the pad read %llu times in a second, %llu of them within 2 ms of the one before; gaps <0.5 <1 <2 <4 <6 <9 <14 more: %llu %llu %llu %llu %llu %llu %llu %llu\n", (unsigned long long)reads, (unsigned long long)close,
                    (unsigned long long)gaps[0], (unsigned long long)gaps[1], (unsigned long long)gaps[2], (unsigned long long)gaps[3],
                    (unsigned long long)gaps[4], (unsigned long long)gaps[5], (unsigned long long)gaps[6], (unsigned long long)gaps[7]);
                fflush(stdout);
                memset(gaps, 0, sizeof(gaps));
                reads = 0; close = 0;
            }
        }
    }

    if (ctx.r5.u32 != 0)
    {
        // A packet number, then the pad itself: buttons, two triggers and four
        // thumb stick axes, in the order the console lays them out. The sticks
        // and triggers used to be written as zero whatever the player did,
        // which left the keyboard able to press buttons and nothing able to
        // move or aim.
        const Input::Pad pad = Input::State();

        memset(Guest::Ptr(ctx.r5.u32), 0, 16);
        Guest::Write32(base, ctx.r5.u32 + 0, Input::PacketNumber());
#ifdef COD3_MP
        // 128: raw Y edges are consumed once per server frame. Suppress the
        // unreliable parallel native binding only while that bridge is active;
        // pause/menu reports keep their native Y semantics.
        Guest::Write16(base, ctx.r5.u32 + 4, Input::NativeButtons(pad.buttons,Guest::Read8(base,0x82C73024)!=0));
#else
        Guest::Write16(base, ctx.r5.u32 + 4, pad.buttons);
#endif
        Guest::Write8(base, ctx.r5.u32 + 6, pad.leftTrigger);
        Guest::Write8(base, ctx.r5.u32 + 7, pad.rightTrigger);
        Guest::Write16(base, ctx.r5.u32 + 8, uint16_t(pad.thumbLX));
        Guest::Write16(base, ctx.r5.u32 + 10, uint16_t(pad.thumbLY));
        Guest::Write16(base, ctx.r5.u32 + 12, uint16_t(pad.thumbRX));
        Guest::Write16(base, ctx.r5.u32 + 14, uint16_t(pad.thumbRY));
    }
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XamInputGetCapabilities(userIndex, flags, XINPUT_CAPABILITIES*)
PPC_FUNC(__imp__XamInputGetCapabilities)
{
    Kernel::CountImport("XamInputGetCapabilities");

    if (ctx.r3.u32 != SignedInUser && !IsBot(ctx.r3.u32))
    {
        if (ctx.r5.u32 != 0) memset(Guest::Ptr(ctx.r5.u32), 0, 20);
        ctx.r3.u32 = X_ERROR_DEVICE_NOT_CONNECTED;
        return;
    }

    if (ctx.r5.u32 != 0)
    {
        memset(Guest::Ptr(ctx.r5.u32), 0, 20);
        // Type and subtype both one: an ordinary gamepad. The masks that
        // follow say which controls exist, and a gamepad has all of them.
        Guest::Write32(base, ctx.r5.u32, 0x01010000);
        Guest::Write16(base, ctx.r5.u32 + 4, 0xFFFF);
        Guest::Write16(base, ctx.r5.u32 + 6, 0xFFFF);
        Guest::Write32(base, ctx.r5.u32 + 8, 0xFFFFFFFF);
        Guest::Write32(base, ctx.r5.u32 + 12, 0xFFFFFFFF);
    }
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XamInputSetState(userIndex, flags, XINPUT_VIBRATION*)
PPC_FUNC(__imp__XamInputSetState)
{
    Kernel::CountImport("XamInputSetState");
    if (ctx.r3.u32 != SignedInUser)
    {
        ctx.r3.u32 = X_ERROR_DEVICE_NOT_CONNECTED;
        return;
    }
    // XINPUT_VIBRATION: the left (low frequency) motor's speed, then the
    // right's, big endian. Passed on to the real pad.
    if (ctx.r5.u32 != 0)
        Input::SetRumble(Guest::Read16(base, ctx.r5.u32 + 0), Guest::Read16(base, ctx.r5.u32 + 2));
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// --- Notifications ---------------------------------------------------------

// The notifications the dashboard sends a title, queued for it to collect.
//
// A title learns who is signed in from a notification, not by asking: the
// dashboard sends XN_SYS_SIGNINCHANGED when a profile signs in, and the
// title's handler then reads the sign-in state and marks the user present.
// With no notification ever arriving this title never asked, so the player
// was never signed in as far as it knew: the profile was never read, and
// starting a game stopped at "you are not signed in". The listener is
// handed that notification, for the one user this runtime signs in, as
// soon as it is created.
namespace
{
    constexpr uint32_t XN_SYS_SIGNINCHANGED = 0x0000000A;

    struct Notification { uint32_t id, parameter; };
    std::mutex g_notifyMutex;
    std::vector<Notification> g_notifications;
}

// HANDLE XamNotifyCreateListener(ULONGLONG areas)
PPC_FUNC(__imp__XamNotifyCreateListener)
{
    Kernel::CountImport("XamNotifyCreateListener");
    {
        std::lock_guard<std::mutex> lock(g_notifyMutex);
        g_notifications.push_back({ XN_SYS_SIGNINCHANGED, 1u << SignedInUser });
    }
    ctx.r3.u32 = 0x000F0001;
}

// BOOL XNotifyGetNext(HANDLE listener, ULONG id, ULONG* outId, ULONG* outParam)
PPC_FUNC(__imp__XNotifyGetNext)
{
    Kernel::CountImport("XNotifyGetNext");
    // The bot experiment: players signed in or out tell the title so.
    {
        static int bots = 0;
        const int now = DebugProbe::BotPads();
        if (now != bots)
        {
            bots = now;
            uint32_t mask = 1u << SignedInUser;
            for (int i = 1; i <= now; i++) mask |= 1u << i;
            std::lock_guard<std::mutex> lock(g_notifyMutex);
            g_notifications.push_back({ XN_SYS_SIGNINCHANGED, mask });
        }
    }
    const uint32_t wanted = ctx.r4.u32;
    Notification next{ 0, 0 };
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(g_notifyMutex);
        for (auto it = g_notifications.begin(); it != g_notifications.end(); ++it)
        {
            if (wanted != 0 && it->id != wanted) continue;
            next = *it;
            g_notifications.erase(it);
            found = true;
            break;
        }
    }
    if (ctx.r5.u32 != 0) Guest::Write32(base, ctx.r5.u32, next.id);
    if (ctx.r6.u32 != 0) Guest::Write32(base, ctx.r6.u32, next.parameter);
    if (found)
    {
        printf("xam: notification 0x%08X (0x%08X) collected\n", next.id, next.parameter);
        fflush(stdout);
    }
    ctx.r3.u32 = found ? 1 : 0;
}

// --- Memory the dashboard lends the title ----------------------------------

// void* XamAlloc(ULONG flags, ULONG size, void** out)
PPC_FUNC(__imp__XamAlloc)
{
    Kernel::CountImport("XamAlloc");
    // Routed through the guest allocator so these blocks live in guest memory
    // like every other pointer the title holds.
    const uint32_t size = ctx.r4.u32;
    const uint32_t out = ctx.r5.u32;

    ctx.r3.u32 = 0;                 // base address in, zero means anywhere
    ctx.r4.u32 = 0;
    // Reuse NtAllocateVirtualMemory by building its argument block on the
    // guest stack would be circular here, so allocate directly instead.
    extern void __imp__MmAllocatePhysicalMemoryEx(PPCContext&, uint8_t*);
    ctx.r4.u32 = size;
    ctx.r8.u32 = 0;
    __imp__MmAllocatePhysicalMemoryEx(ctx, base);

    const uint32_t address = ctx.r3.u32;
    if (out != 0)
        Guest::Write32(base, out, address);
    ctx.r3.u32 = address != 0 ? X_ERROR_SUCCESS : X_ERROR_FUNCTION_FAILED;
}

// void XamFree(void* pointer)
PPC_FUNC(__imp__XamFree)
{
    Kernel::CountImport("XamFree");
    ctx.r4.u32 = ctx.r3.u32;
    extern void __imp__MmFreePhysicalMemory(PPCContext&, uint8_t*);
    __imp__MmFreePhysicalMemory(ctx, base);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// --- Content and storage ---------------------------------------------------
//
// The console keeps saved games in content packages on a storage device the
// player picks in a dialog. Here there is one device, the saves folder
// beside the executable, and every package is a folder in saves/content
// named by the file name the title gives it, with the display name kept in
// a small file beside the title's own. Mounting a package makes its root
// name ("save:") a path prefix the file layer resolves into the folder.

namespace
{
    constexpr uint32_t X_ERROR_FILE_NOT_FOUND    = 0x00000002;
    constexpr uint32_t X_ERROR_ACCESS_DENIED     = 0x00000005;
    constexpr uint32_t X_ERROR_NO_MORE_FILES     = 0x00000012;
    constexpr uint32_t X_ERROR_ALREADY_EXISTS    = 0x000000B7;
    constexpr uint32_t DummyDeviceId = 0xF00D0000;

    // XCONTENT_DATA: device id, content type, a display name of 128 wide
    // characters, a file name of 42 bytes.
    constexpr uint32_t ContentDataSize = 308;

    struct ContentData
    {
        uint32_t deviceId = 0, contentType = 0;
        std::wstring displayName;
        std::string fileName;
    };

    ContentData ReadContentData(const uint8_t* base, uint32_t at)
    {
        ContentData data;
        if (at == 0) return data;
        data.deviceId = Guest::Read32(base, at);
        data.contentType = Guest::Read32(base, at + 4);
        for (uint32_t i = 0; i < 128; i++)
        {
            const uint16_t c = Guest::Read16(base, at + 8 + i * 2);
            if (c == 0) break;
            data.displayName += wchar_t(c);
        }
        for (uint32_t i = 0; i < 42; i++)
        {
            const char c = char(*(base + at + 8 + 256 + i));
            if (c == 0) break;
            data.fileName += c;
        }
        return data;
    }

    void WriteContentData(uint8_t* base, uint32_t at, const ContentData& data)
    {
        memset(base + at, 0, ContentDataSize);
        Guest::Write32(base, at, data.deviceId);
        Guest::Write32(base, at + 4, data.contentType);
        for (size_t i = 0; i < data.displayName.size() && i < 127; i++)
            Guest::Write16(base, uint32_t(at + 8 + i * 2), uint16_t(data.displayName[i]));
        for (size_t i = 0; i < data.fileName.size() && i < 41; i++)
            *(base + at + 8 + 256 + i) = uint8_t(data.fileName[i]);
    }

    // A file name the title gives is used as a folder name; only the plain
    // characters of it are kept, so it cannot leave the saves folder.
    std::string SafeName(const std::string& name)
    {
        std::string out;
        for (char c : name)
            if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == '.' || c == ' ') out += c;
        while (!out.empty() && (out.back() == '.' || out.back() == ' ')) out.pop_back();
        return out.empty() ? "unnamed" : out;
    }

    std::filesystem::path ContentFolder(const ContentData& data)
    {
        char type[16];
        snprintf(type, sizeof(type), "%08X", data.contentType);
        return Kernel::SavesRoot() / "content" / type / SafeName(data.fileName);
    }

    // XOVERLAPPED: result, length, context, event, completion routine, its
    // context, extended error. A call given one answers through it: the
    // result goes in, the event is set, and the call itself says pending.
    uint32_t CompleteOverlapped(PPCContext& ctx, uint8_t* base, uint32_t overlapped, uint32_t result, uint32_t length)
    {
        if (overlapped == 0) return result;
        Guest::Write32(base, overlapped + 0x00, result);
        Guest::Write32(base, overlapped + 0x04, length);
        Guest::Write32(base, overlapped + 0x18, result);
        const uint32_t event = Guest::Read32(base, overlapped + 0x0C);
        if (event != 0)
        {
            const uint64_t r3 = ctx.r3.u64, r4 = ctx.r4.u64;
            ctx.r3.u32 = event;
            ctx.r4.u32 = 0;
            __imp__NtSetEvent(ctx, base);
            ctx.r3.u64 = r3;
            ctx.r4.u64 = r4;
        }
        const uint32_t routine = Guest::Read32(base, overlapped + 0x10);
        if (routine != 0)
        {
            // Run here, on the calling thread, with the error, the length
            // and the title's context: what the console's own would get.
            if (PPCFunc* callback = Guest::Lookup(routine))
            {
                const uint64_t r3 = ctx.r3.u64, r4 = ctx.r4.u64, r5 = ctx.r5.u64;
                ctx.r3.u32 = result;
                ctx.r4.u32 = length;
                ctx.r5.u32 = Guest::Read32(base, overlapped + 0x14);
                callback(ctx, base);
                ctx.r3.u64 = r3; ctx.r4.u64 = r4; ctx.r5.u64 = r5;
            }
        }
        return X_ERROR_IO_PENDING;
    }

    // The enumerators the title has open: each a list of packages found.
    std::mutex g_enumeratorMutex;
    std::map<uint32_t, std::vector<ContentData>> g_enumerators;
    uint32_t g_nextEnumerator = 0x7E000001;

    std::vector<ContentData> FindContent(uint32_t contentType)
    {
        std::vector<ContentData> found;
        char type[16];
        snprintf(type, sizeof(type), "%08X", contentType);
        std::error_code ec;
        const std::filesystem::path folder = Kernel::SavesRoot() / "content" / type;
        if (!std::filesystem::is_directory(folder, ec)) return found;
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec))
        {
            if (ec) break;
            if (!entry.is_directory()) continue;
            ContentData data;
            data.deviceId = DummyDeviceId;
            data.contentType = contentType;
            data.fileName = entry.path().filename().string();
            data.displayName = std::wstring(data.fileName.begin(), data.fileName.end());
            if (FILE* names = _wfopen((entry.path() / L"displayname.txt").c_str(), L"rb"))
            {
                wchar_t text[130] = {};
                const size_t n = fread(text, sizeof(wchar_t), 128, names);
                fclose(names);
                if (n > 0) data.displayName.assign(text, n);
            }
            found.push_back(data);
        }
        return found;
    }
}

// X_RESULT XamContentCreateEnumerator(userIndex, deviceId, contentType,
//                                     contentFlags, itemsPerEnumerate,
//                                     bufferSize*, handle*)
PPC_FUNC(__imp__XamContentCreateEnumerator)
{
    Kernel::CountImport("XamContentCreateEnumerator");
    const uint32_t contentType = ctx.r5.u32, items = ctx.r7.u32;
    if (ctx.r8.u32 != 0) Guest::Write32(base, ctx.r8.u32, (items ? items : 1) * ContentDataSize);
    uint32_t handle;
    {
        std::lock_guard<std::mutex> lock(g_enumeratorMutex);
        handle = g_nextEnumerator++;
        g_enumerators[handle] = FindContent(contentType);
    }
    if (ctx.r9.u32 != 0) Guest::Write32(base, ctx.r9.u32, handle);
    printf("xam: content enumerator for type 0x%08X: %zu packages\n", contentType, g_enumerators[handle].size());
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XamEnumerate(HANDLE, flags, buffer, size, itemsReturned*, overlapped)
PPC_FUNC(__imp__XamEnumerate)
{
    Kernel::CountImport("XamEnumerate");
    const uint32_t handle = ctx.r3.u32, buffer = ctx.r5.u32, size = ctx.r6.u32;
    const uint32_t itemsOut = ctx.r7.u32, overlapped = ctx.r8.u32;
    std::vector<ContentData> batch;
    {
        std::lock_guard<std::mutex> lock(g_enumeratorMutex);
        auto found = g_enumerators.find(handle);
        if (found != g_enumerators.end())
        {
            const uint32_t room = buffer != 0 ? size / ContentDataSize : 0;
            while (!found->second.empty() && batch.size() < room)
            {
                batch.push_back(found->second.front());
                found->second.erase(found->second.begin());
            }
        }
    }
    for (size_t i = 0; i < batch.size(); i++)
        WriteContentData(base, uint32_t(buffer + i * ContentDataSize), batch[i]);
    if (itemsOut != 0) Guest::Write32(base, itemsOut, uint32_t(batch.size()));
    const uint32_t result = batch.empty() ? X_ERROR_NO_MORE_FILES : X_ERROR_SUCCESS;
    ctx.r3.u32 = CompleteOverlapped(ctx, base, overlapped, result, uint32_t(batch.size()));
}

// X_RESULT XamContentCreateEx(userIndex, rootName, contentData*, flags,
//                             disposition*, licenseMask*, cacheSize,
//                             contentSize (64), overlapped)
PPC_FUNC(__imp__XamContentCreateEx)
{
    Kernel::CountImport("XamContentCreateEx");
    std::string rootName;
    for (uint32_t i = 0; i < 32 && ctx.r4.u32 != 0; i++)
    {
        const char c = char(*(base + ctx.r4.u32 + i));
        if (c == 0) break;
        rootName += c;
    }
    const ContentData data = ReadContentData(base, ctx.r5.u32);
    const uint32_t flags = ctx.r6.u32 & 0xF;
    const uint32_t dispositionOut = ctx.r7.u32, licenseOut = ctx.r8.u32;
    const uint32_t overlapped = Guest::Read32(base, ctx.r1.u32 + 0x54);

    const std::filesystem::path folder = ContentFolder(data);
    std::error_code ec;
    const bool exists = std::filesystem::is_directory(folder, ec);
    // XCONTENTFLAG_CREATENEW 1, CREATEALWAYS 2, OPENEXISTING 3, OPENALWAYS 4,
    // TRUNCATEEXISTING 5.
    uint32_t result = X_ERROR_SUCCESS;
    uint32_t disposition = exists ? 2 : 1;   // XCONTENT_OPENED_EXISTING, XCONTENT_CREATED_NEW
    if (flags == 1 && exists) result = X_ERROR_ALREADY_EXISTS;
    else if ((flags == 3 || flags == 5) && !exists) result = X_ERROR_FILE_NOT_FOUND;
    else
    {
        if ((flags == 2 || flags == 5) && exists) std::filesystem::remove_all(folder, ec);
        std::filesystem::create_directories(folder, ec);
        if (ec) result = X_ERROR_ACCESS_DENIED;
        else
        {
            if (FILE* names = _wfopen((folder / L"displayname.txt").c_str(), L"wb"))
            {
                fwrite(data.displayName.c_str(), sizeof(wchar_t), data.displayName.size(), names);
                fclose(names);
            }
            Kernel::MountContent(rootName, folder);
        }
    }
    if (dispositionOut != 0) Guest::Write32(base, dispositionOut, disposition);
    if (licenseOut != 0) Guest::Write32(base, licenseOut, 0xFFFFFFFF);
    printf("xam: content \"%s\" (type 0x%08X, file %s, flags %u) as %s: %s, result 0x%X\n",
        std::string(data.displayName.begin(), data.displayName.end()).c_str(), data.contentType,
        data.fileName.c_str(), flags, rootName.c_str(), folder.string().c_str(), result);
    fflush(stdout);
    ctx.r3.u32 = CompleteOverlapped(ctx, base, overlapped, result);
}

// X_RESULT XamContentClose(rootName, overlapped)
PPC_FUNC(__imp__XamContentClose)
{
    Kernel::CountImport("XamContentClose");
    std::string rootName;
    for (uint32_t i = 0; i < 32 && ctx.r3.u32 != 0; i++)
    {
        const char c = char(*(base + ctx.r3.u32 + i));
        if (c == 0) break;
        rootName += c;
    }
    Kernel::UnmountContent(rootName);
    ctx.r3.u32 = CompleteOverlapped(ctx, base, ctx.r4.u32, X_ERROR_SUCCESS);
}

// X_RESULT XamContentDelete(userIndex, contentData*, overlapped)
PPC_FUNC(__imp__XamContentDelete)
{
    Kernel::CountImport("XamContentDelete");
    const ContentData data = ReadContentData(base, ctx.r4.u32);
    std::error_code ec;
    const std::filesystem::path folder = ContentFolder(data);
    const bool existed = std::filesystem::is_directory(folder, ec);
    if (existed) std::filesystem::remove_all(folder, ec);
    ctx.r3.u32 = CompleteOverlapped(ctx, base, ctx.r5.u32, existed ? X_ERROR_SUCCESS : X_ERROR_FILE_NOT_FOUND);
}

// X_RESULT XamContentGetCreator(userIndex, contentData*, isCreator*, xuid*, overlapped)
PPC_FUNC(__imp__XamContentGetCreator)
{
    Kernel::CountImport("XamContentGetCreator");
    if (ctx.r5.u32 != 0) Guest::Write32(base, ctx.r5.u32, 1);
    if (ctx.r6.u32 != 0) Guest::Write64(base, ctx.r6.u32, PlayerXuid());
    ctx.r3.u32 = CompleteOverlapped(ctx, base, ctx.r7.u32, X_ERROR_SUCCESS);
}

// --- System dialogs --------------------------------------------------------
// Every one answers as if the user dismissed it. A title normally treats that
// as a valid outcome and carries on.

PPC_FUNC(__imp__XamShowSigninUI) { Kernel::CountImport("XamShowSigninUI"); ctx.r3.u32 = X_ERROR_SUCCESS; }
// X_RESULT XamShowDeviceSelectorUI(userIndex, contentType, contentFlags,
//                                  totalRequested (64), deviceId*, overlapped)
// The one device, chosen without a dialog.
PPC_FUNC(__imp__XamShowDeviceSelectorUI)
{
    Kernel::CountImport("XamShowDeviceSelectorUI");
    if (ctx.r7.u32 != 0) Guest::Write32(base, ctx.r7.u32, DummyDeviceId);
    printf("xam: the storage device asked for (type 0x%08X, %llu bytes) is the saves folder\n",
        ctx.r4.u32, (unsigned long long)ctx.r6.u64);
    fflush(stdout);
    ctx.r3.u32 = CompleteOverlapped(ctx, base, ctx.r8.u32, X_ERROR_SUCCESS);
}
PPC_FUNC(__imp__XamShowMessageBoxUI) { Kernel::CountImport("XamShowMessageBoxUI"); ctx.r3.u32 = X_ERROR_SUCCESS; }
PPC_FUNC(__imp__XamShowMessageBoxUIEx) { Kernel::CountImport("XamShowMessageBoxUIEx"); ctx.r3.u32 = X_ERROR_SUCCESS; }
PPC_FUNC(__imp__XamShowMarketplaceUI) { Kernel::CountImport("XamShowMarketplaceUI"); ctx.r3.u32 = X_ERROR_SUCCESS; }

PPC_FUNC(__imp__XamShowDirtyDiscErrorUI)
{
    Kernel::CountImport("XamShowDirtyDiscErrorUI");
    printf("\nThe game reported a dirty or unreadable disc. The installed copy\n");
    printf("is missing something it expected to find.\n");
    // Who decided so: the title's own frames above this call, and what
    // this thread asked the kernel for just before.
    {
        CONTEXT host{};
        RtlCaptureContext(&host);
        uint32_t functions[12] = {};
        const int count = Sampler::WalkGuestStack(&host, functions, 12);
        printf("  called from 0x%08X;", uint32_t(ctx.lr));
        if (count > 0)
        {
            printf(" guest call chain, innermost first:");
            for (int i = 0; i < count; i++) printf("%ssub_%08X", i == 0 ? " " : " <- ", functions[i]);
        }
        printf("\n");
        Kernel::ReportRecentCalls(GetCurrentThreadId());
    }
    fflush(stdout);
    Guest::Shutdown();
    Kernel::Exit(1);
}

// --- Gamer tiles -----------------------------------------------------------

PPC_FUNC(__imp__XamParseGamerTileKey) { Kernel::CountImport("XamParseGamerTileKey"); ctx.r3.u32 = X_ERROR_FUNCTION_FAILED; }
PPC_FUNC(__imp__XamReadTileToTexture) { Kernel::CountImport("XamReadTileToTexture"); ctx.r3.u32 = X_ERROR_FUNCTION_FAILED; }
PPC_FUNC(__imp__XamWriteGamerTile) { Kernel::CountImport("XamWriteGamerTile"); ctx.r3.u32 = X_ERROR_FUNCTION_FAILED; }

// --- Title lifetime --------------------------------------------------------

PPC_FUNC(__imp__XamGetExecutionId)
{
    Kernel::CountImport("XamGetExecutionId");
    // The title's own execution id, which it uses to identify itself.
    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, 0);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// --- Launch data and relaunching the title ---------------------------------
//
// Between missions the title reboots itself, as console titles do to start
// with clean memory: its "reboot" command (sub_82514710) hands
// XamLoaderSetLaunchData a 664-byte block of its campaign state (0x82A8E610)
// and asks XamLoaderLaunchTitle for game:\default.xex; the new instance reads
// the block back at start (sub_823FCCA8, "retrieving stub data") and goes on
// to the next mission. Here the block is written to a file and this
// executable started again with COD3_LAUNCHDATA naming it; the new process
// hands it back. Launching anything else (the dashboard, the multiplayer
// executable) still ends the process.

namespace
{
    std::mutex g_launchMutex;
    std::vector<uint8_t> g_launchOut;   // what this process will hand on
    std::vector<uint8_t> g_launchIn;    // what this process was started with
    bool g_launchInRead = false;

    const std::vector<uint8_t>& LaunchDataIn()
    {
        std::lock_guard<std::mutex> lock(g_launchMutex);
        if (!g_launchInRead)
        {
            g_launchInRead = true;
            if (const char* path = getenv("COD3_LAUNCHDATA"))
            {
                if (FILE* in = fopen(path, "rb"))
                {
                    uint8_t buffer[4096];
                    size_t got;
                    while ((got = fread(buffer, 1, sizeof(buffer), in)) > 0)
                        g_launchIn.insert(g_launchIn.end(), buffer, buffer + got);
                    fclose(in);
                    // Read once: a later start from the menu has none.
                    std::error_code ignored;
                    std::filesystem::remove(path, ignored);
                }
                printf("xam: started with %zu bytes of launch data from %s\n", g_launchIn.size(), path);
            }
        }
        return g_launchIn;
    }

    // This executable again, with the launch data in a file and the scripted
    // run's knobs left out (the relaunched title goes where its data says).
    // Starts another program from this one's folder, with this process's
    // environment and none of its arguments.
    bool StartSibling(const wchar_t* name)
    {
        wchar_t executable[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        const std::filesystem::path other = std::filesystem::path(executable).parent_path() / name;
        std::wstring commandLine = L"\"" + other.wstring() + L"\"";
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        const std::wstring folder = other.parent_path().wstring();
        if (!CreateProcessW(other.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE,
                            nullptr, folder.c_str(), &startup, &process))
            return false;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }

    bool Relaunch()
    {
        std::filesystem::path file = Kernel::SavesRoot();
        if (file.empty()) file = std::filesystem::temp_directory_path();
        file /= "launchdata.bin";
        {
            std::lock_guard<std::mutex> lock(g_launchMutex);
            FILE* out = _wfopen(file.c_str(), L"wb");
            if (out == nullptr) return false;
            if (!g_launchOut.empty()) fwrite(g_launchOut.data(), 1, g_launchOut.size(), out);
            fclose(out);
        }

        std::wstring environment;
        if (wchar_t* block = GetEnvironmentStringsW())
        {
            for (const wchar_t* entry = block; *entry != 0; entry += wcslen(entry) + 1)
            {
                // A scripted run's level, commands and pad presses belong to
                // the first process, not to the one the title starts.
                if (_wcsnicmp(entry, L"COD3_MAP=", 9) == 0 || _wcsnicmp(entry, L"COD3_CMD=", 9) == 0 ||
                    _wcsnicmp(entry, L"COD3_PAD=", 9) == 0 || _wcsnicmp(entry, L"COD3_WIN=", 9) == 0 ||
                    _wcsnicmp(entry, L"COD3_LAUNCHDATA=", 16) == 0) continue;
                environment += entry;
                environment.push_back(0);
            }
            FreeEnvironmentStringsW(block);
        }
        environment += L"COD3_LAUNCHDATA=" + file.wstring();
        environment.push_back(0);
        environment.push_back(0);

        wchar_t executable[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        std::wstring commandLine = GetCommandLineW();
        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable, commandLine.data(), nullptr, nullptr, FALSE, CREATE_UNICODE_ENVIRONMENT,
                            environment.data(), nullptr, &startup, &process))
            return false;
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
        return true;
    }
}

PPC_FUNC(__imp__XamLoaderGetLaunchDataSize)
{
    Kernel::CountImport("XamLoaderGetLaunchDataSize");
    const std::vector<uint8_t>& data = LaunchDataIn();
    if (ctx.r3.u32 != 0) Guest::Write32(base, ctx.r3.u32, uint32_t(data.size()));
    ctx.r3.u32 = data.empty() ? X_ERROR_NOT_FOUND : X_ERROR_SUCCESS;
}

// DWORD XamLoaderGetLaunchData(void* buffer, DWORD size)
PPC_FUNC(__imp__XamLoaderGetLaunchData)
{
    Kernel::CountImport("XamLoaderGetLaunchData");
    const std::vector<uint8_t>& data = LaunchDataIn();
    if (data.empty() || ctx.r3.u32 == 0) { ctx.r3.u32 = X_ERROR_NOT_FOUND; return; }
    const size_t count = std::min<size_t>(ctx.r4.u32, data.size());
    memcpy(base + ctx.r3.u32, data.data(), count);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// DWORD XamLoaderSetLaunchData(const void* data, DWORD size)
PPC_FUNC(__imp__XamLoaderSetLaunchData)
{
    Kernel::CountImport("XamLoaderSetLaunchData");
    {
        std::lock_guard<std::mutex> lock(g_launchMutex);
        g_launchOut.clear();
        if (ctx.r3.u32 != 0 && ctx.r4.u32 != 0 && ctx.r4.u32 < 0x10000)
            g_launchOut.assign(base + ctx.r3.u32, base + ctx.r3.u32 + ctx.r4.u32);
    }
    printf("xam: launch data set, %u bytes\n", ctx.r4.u32);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

#ifndef COD3_MP
void CoopBeforeRelaunch();   // coop.cpp
// (co-op v45, merged in 0.5.671) the joining copy started again from nothing, this program once more
// with this process's environment (its co-op mode and the host's address in it): it joins the host and
// follows him into his mission as it first did. Used when the host goes back to a checkpoint
// (coop_lobby.cpp ReloadMission, coop_swap.cpp).
void CoopRelaunchFresh()
{
    if (StartSibling(L"CoD3.exe"))
    {
        printf("coop: started again from nothing; this process ends.\n");
        fflush(stdout);
        // (v45g) at once: ExitProcess from the game's thread waited for ever on the second time (the log's
        // own shutdown, with this process's output already sent to coop-join-reload.log)
        TerminateProcess(GetCurrentProcess(), 0);
    }
    printf("coop: could not start this program again (error %lu)\n", GetLastError()); fflush(stdout);
}
#endif
PPC_FUNC(__imp__XamLoaderLaunchTitle)
{
    Kernel::CountImport("XamLoaderLaunchTitle");

    // Which title, and from where. A path of nothing is the dashboard, and a
    // title that asks for the dashboard has usually just decided that
    // something is fatally wrong: the call chain says what.
    char path[256] = {};
    printf("\nThe game asked to launch another title");
    if (ctx.r3.u32 != 0)
    {
        for (size_t i = 0; i + 1 < sizeof(path); i++)
        {
            path[i] = char(Guest::Base[ctx.r3.u32 + i]);
            if (path[i] == 0) break;
        }
        printf(": \"%s\"", path);
    }
    else
    {
        printf(": the dashboard");
    }
    printf(", flags 0x%08X, from 0x%08X.\n", ctx.r4.u32, uint32_t(ctx.lr));

    std::string lower(path);
    for (char& c : lower) c = char(tolower(static_cast<unsigned char>(c)));
    const auto endsWith = [&](const char* tail) {
        const size_t n = strlen(tail);
        return lower.size() >= n && lower.compare(lower.size() - n, n, tail) == 0;
    };

    // The other executable: the single player menu's "Multiplayer" asks for
    // codmp_xenonf.xex, and multiplayer's way back asks for default.xex.
    // Each is a program of its own here, beside this one.
#ifdef COD3_MP
    const wchar_t* const sibling = endsWith("default.xex") ? L"CoD3.exe" : nullptr;
#else
    const wchar_t* const sibling = endsWith("codmp_xenonf.xex") ? L"CoD3MP.exe" : nullptr;
#endif
    if (sibling != nullptr)
    {
        if (StartSibling(sibling))
        {
            printf("Started %ls; this process ends.\n", sibling);
            fflush(stdout);
            Log::Finish();
            ExitProcess(0);
        }
        printf("Could not start %ls beside this program (error %lu).\n", sibling, GetLastError());
    }

#ifndef COD3_MP
    // Itself: the next mission. Started again, and this process ends.
    // (A co-op host tells the joining copy first: coop.cpp.)
    if (endsWith("default.xex"))
    {
        CoopBeforeRelaunch();
        if (Relaunch())
        {
            printf("Relaunched for the next mission; this process ends.\n");
            fflush(stdout);
            Log::Finish();
            ExitProcess(0);
        }
        printf("The relaunch failed (error %lu).\n", GetLastError());
    }
#endif
    {
        uint32_t functions[12] = {};
        const int count = Sampler::FunctionsOnStack(functions, 12);
        if (count > 0)
        {
            printf("  guest call chain, innermost first:");
            for (int i = 0; i < count; i++)
                printf("%ssub_%08X", i == 0 ? " " : " <- ", functions[i]);
            printf("\n");
        }
    }
    Kernel::ReportRecentCalls(GetCurrentThreadId());
    printf("Shutting down.\n");
    fflush(stdout);
    Guest::Shutdown();
    Kernel::Exit(0);
}

PPC_FUNC(__imp__XamLoaderTerminateTitle)
{
    Kernel::CountImport("XamLoaderTerminateTitle");
    printf("\nThe game asked to terminate. Shutting down.\n");
    fflush(stdout);
    Guest::Shutdown();
    Kernel::Exit(0);
}

// --- Cross process messages ------------------------------------------------

// X_RESULT XMsgInProcessCall(ULONG app, ULONG message, void* arg1, void* arg2)
PPC_FUNC(__imp__XMsgInProcessCall)
{
    Kernel::CountImport("XMsgInProcessCall");
    const uint32_t app = ctx.r3.u32, message = ctx.r4.u32, buffer = ctx.r5.u32;

    // The music player (app 0xFA). The multiplayer front end asks for its
    // status every frame, to know whether the player has music of their own
    // playing over the game's: XMPGetStatus, { client, DWORD* state }. It is
    // idle (0): there is no music player here.
    if (app == 0xFA && message == 0x00070009 && buffer != 0)
    {
        const uint32_t state = Guest::Read32(base, buffer + 4);
        if (state != 0) Guest::Write32(base, state, 0);
        ctx.r3.u32 = X_ERROR_SUCCESS;
        return;
    }

    static std::atomic<int> announced{ 0 };
    if ((announced.load(std::memory_order_relaxed) < 20 && announced.fetch_add(1) < 20))
    {
        printf("xam: XMsgInProcessCall(app 0x%X, message 0x%X, 0x%08X, 0x%08X) from 0x%08X", 
            ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, uint32_t(ctx.lr));
        if (ctx.r5.u32 != 0)
        {
            printf("  arg1:");
            for (int i = 0; i < 6; i++) printf(" %08X", Guest::Read32(base, ctx.r5.u32 + i * 4));
        }
        printf("\n");
        fflush(stdout);
    }
    ctx.r3.u32 = X_ERROR_FUNCTION_FAILED;
}
PPC_FUNC(__imp__XMsgStartIORequest) { Kernel::CountImport("XMsgStartIORequest"); ctx.r3.u32 = X_ERROR_FUNCTION_FAILED; }
PPC_FUNC(__imp__XMsgStartIORequestEx) { Kernel::CountImport("XMsgStartIORequestEx"); ctx.r3.u32 = X_ERROR_FUNCTION_FAILED; }
