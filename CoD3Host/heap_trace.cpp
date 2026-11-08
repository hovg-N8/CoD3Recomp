// A check on the title's own heap: every block malloc hands out is
// remembered, and a free of one it does not hold is reported with the
// caller's chain. A block freed twice puts itself on a bin's list twice,
// the list turns into a cycle, and the next malloc that walks it never
// returns: that was the second level's load, one run in two. This says
// who frees what twice. Behind COD3_HEAPCHECK=1, nothing otherwise.
//
// The title's heap is dlmalloc under a critical section: sub_820CC1D0 and
// its siblings allocate (sub_820CBC40 under the lock; two of them from a
// heap the caller names, the level's), sub_820CC468(block) and
// sub_820CC4C0(heap, block) free (sub_820CBF58 under the lock). The recompiled
// functions are reached through weak aliases, which these definitions
// replace.

#include "heap_trace.h"
#include "kernel.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <unordered_set>

#include <Windows.h>

namespace
{
    bool Wanted()
    {
        static const bool wanted = getenv("COD3_HEAPCHECK") != nullptr;
        return wanted;
    }

    std::mutex g_mutex;
    std::unordered_set<uint32_t> g_live;
    std::atomic<int> g_reported{ 0 };
    // (co-op v45k, merged in 0.5.671) the blocks followed always (in the campaign), so the
    // joining copy's guard can tell a double free; COD3_NOHEAPFOLLOW turns the following off
    std::atomic<bool> g_guard{ false };
    bool Following() { static const bool off = getenv("COD3_NOHEAPFOLLOW") != nullptr; return !off; }

    // The guest call chain from the context: the link register, then the
    // return addresses the prologues saved eight bytes below each back
    // chain pointer.
    void PrintChain(const PPCContext& ctx, uint8_t* base)
    {
        printf(" from %08X", uint32_t(ctx.lr));
        uint32_t frame = ctx.r1.u32;
        for (int depth = 0; depth < 12; depth++)
        {
            if (frame < 0x10000 || frame >= 0xC0000000u) break;
            const uint32_t caller = Guest::Read32(base, frame);
            if (caller <= frame || caller - frame > 0x100000 || (caller & 7) != 0 || caller >= 0xC0000000u) break;
            const uint32_t address = Guest::Read32(base, caller - 8);
            if (address < 0x82000000u || address >= 0x8A000000u) break;
            printf(" %08X", address);
            frame = caller;
        }
    }

    void Allocated(uint32_t pointer)
    {
        if (pointer == 0) return;
        std::lock_guard<std::mutex> lock(g_mutex);
        g_live.insert(pointer);
    }

    // false: not a block the heap holds (and, guarded, the free is not to be done)
    bool Freed(const PPCContext& ctx, uint8_t* base, uint32_t pointer, const char* what)
    {
        if (pointer == 0) return true;
        bool known;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            known = g_live.erase(pointer) != 0;
        }
        if (known) return true;
        if (g_guard.load(std::memory_order_relaxed))
        {
            static std::atomic<int> told{ 0 };
            if ((told.load(std::memory_order_relaxed) < 20 && told.fetch_add(1) < 20)) { printf("heap: co-op guard: a %s of 0x%08X, not a live block: not done", what, pointer); PrintChain(ctx, base); printf("\n"); fflush(stdout); }
            return false;
        }
        if (!Wanted() || (g_reported.load(std::memory_order_relaxed) >= 12 || g_reported.fetch_add(1) >= 12)) return true;
        const uint32_t header = Guest::Read32(base, pointer - 4);
        printf("heap: %s of 0x%08X, which is not a live block (chunk header %08X, size %u)", what, pointer, header, header & ~7u);
        PrintChain(ctx, base);
        printf("\n");
        fflush(stdout);
        return true;
    }
}
void HeapTrace::SetGuard(bool on) { if (g_guard.exchange(on) != on) { printf("heap: co-op guard %s\n", on ? "on" : "off"); fflush(stdout); } }

// The heap's free lists, checked around a call that walks them. A bin's
// list is circular through fd; a chunk that appears twice makes a cycle,
// and the malloc that walks it never returns. This looks for a cycle in
// every bin, and says which chunk repeats and after how many steps.
namespace
{
    bool BinsAreSound(uint8_t* base, uint32_t mstate, uint32_t& bad, uint32_t& repeat)
    {
        auto word = [&](uint32_t a) { return Guest::Read32(base, a); };
        for (uint32_t bin = 1; bin < 128; bin++)
        {
            const uint32_t head = mstate + 44 + (bin << 3);
            uint32_t slow = word(head + 8), fast = slow;
            int steps = 0;
            while (fast != head && steps++ < 20000)
            {
                if (fast < 0x10000 || fast >= 0xC0000000u) break;
                fast = word(fast + 8);
                if (fast == head || fast < 0x10000 || fast >= 0xC0000000u) break;
                fast = word(fast + 8);
                slow = word(slow + 8);
                if (fast == slow) { bad = bin; repeat = fast; return false; }
            }
        }
        return true;
    }

    // The title's one malloc state, from the first call that names it.
    std::atomic<uint32_t> g_mstate{ 0 };
    std::atomic<int> g_binsReported{ 0 };

    // COD3_HEAPCHECK=2: the unsorted list (bin 1), where the malloc that
    // never returns spins, checked at the edges of every allocation and
    // every free. The first check that finds a cycle says whether the heap
    // was already broken when the title called in (something outside the
    // heap wrote into it) or broke inside the call (a double free).
    bool Thorough()
    {
        static const bool thorough = []() { const char* t = getenv("COD3_HEAPCHECK"); return t != nullptr && t[0] == '2'; }();
        return thorough;
    }

    // The malloc state: the word the heap's own sub_820CB990 reads, at
    // 0x82909940, which points at the state itself.
    uint32_t MallocState(uint8_t* base)
    {
        uint32_t mstate = g_mstate.load(std::memory_order_relaxed);
        if (mstate != 0) return mstate;
        const uint32_t at = Guest::Read32(base, 0x82909940u);
        if (at < 0x82000000u || at >= 0x8A000000u) return 0;
        g_mstate.store(at, std::memory_order_relaxed);
        printf("heap: the malloc state is at %08X\n", at);
        fflush(stdout);
        return at;
    }

    // The list is walked at two speeds; the two meeting at a chunk is a
    // cycle. A pointer that is not guest memory ends the walk without a
    // verdict: an empty or not yet built list holds such a value, and
    // saying "broken" of one of those would be saying it of every bin the
    // title has never used.
    bool UnsortedIsSound(uint8_t* base, uint32_t mstate, uint32_t& repeat, int& steps)
    {
        const uint32_t head = mstate + 52;
        auto chunk = [&](uint32_t at) { return at >= 0x10000 && at < 0xC0000000u && at != head; };
        auto next = [&](uint32_t at) { return Guest::Read32(base, at + 8); };
        uint32_t slow = next(head), fast = slow;
        steps = 0;
        while (chunk(fast) && steps++ < 40000)
        {
            fast = next(fast);
            if (!chunk(fast)) break;
            fast = next(fast);
            slow = next(slow);
            if (fast == slow && chunk(fast)) { repeat = fast; return false; }
        }
        if (steps >= 40000) { repeat = slow; return false; }
        return true;
    }

    std::atomic<int> g_cycleReported{ 0 };

    void CheckUnsorted(PPCContext& ctx, uint8_t* base, const char* when)
    {
        if (!Thorough() || g_cycleReported.load(std::memory_order_relaxed) >= 3) return;
        const uint32_t mstate = MallocState(base);
        if (mstate == 0) return;
        uint32_t repeat = 0;
        int steps = 0;
        if (UnsortedIsSound(base, mstate, repeat, steps)) return;
        if ((g_cycleReported.load(std::memory_order_relaxed) >= 3 || g_cycleReported.fetch_add(1) >= 3)) return;
        printf("heap: the unsorted list is a cycle through %08X after %d chunks, %s", repeat, steps, when);
        PrintChain(ctx, base);
        printf("\n");
        fflush(stdout);
    }

    void CheckBins(PPCContext& ctx, uint8_t* base, const char* when, uint32_t pointer)
    {
const uint32_t mstate = MallocState(base);
        if (mstate == 0) return;
        if (g_binsReported.load(std::memory_order_relaxed) >= 4) return;
        uint32_t bad = 0, repeat = 0;
        if (BinsAreSound(base, mstate, bad, repeat)) return;
        if ((g_binsReported.load(std::memory_order_relaxed) >= 4 || g_binsReported.fetch_add(1) >= 4)) return;
        printf("heap: bin %u holds a cycle through %08X %s 0x%08X", bad, repeat, when, pointer);
        PrintChain(ctx, base);
        printf("\n");
        fflush(stdout);
    }
}

// sub_823F04B0(object): the destructor that ran twice over the same
// objects at a level's teardown and freed their fields twice (the fix is
// in title_fixes.cpp). Every object it destroys is noted, so one destroyed
// twice still shows here if another path ever does the same.
void HeapTrace::Destroying(const PPCContext& ctx, uint8_t* base)
{
    if (!Wanted()) return;
    const uint32_t object = ctx.r3.u32;
    static std::mutex mutex;
    static std::unordered_set<uint32_t> destroyed;
    static int announced = 0;
    bool again = false;
    {
        std::lock_guard<std::mutex> lock(mutex);
        again = !destroyed.insert(object).second;
    }
    if (announced < 20)
    {
        announced++;
        // The object's vtable and its first slot: the deleting destructor
        // the caller reaches through it.
        const uint32_t vtable = (object >= 0x10000 && object < 0xC0000000u) ? Guest::Read32(base, object) : 0;
        const uint32_t slot0 = (vtable >= 0x10000 && vtable < 0xC0000000u) ? Guest::Read32(base, vtable) : 0;
        printf("heap: destroying 0x%08X%s, vtable %08X slot0 %08X", object, again ? " AGAIN" : "", vtable, slot0);
        PrintChain(ctx, base);
        printf("\n");
        fflush(stdout);
    }
}

extern "C" PPC_FUNC(__imp__sub_820CC1D0);
PPC_FUNC(sub_820CC1D0)
{
    CheckUnsorted(ctx, base, "entering an allocation");
    __imp__sub_820CC1D0(ctx, base);
    if (Wanted() || Following()) Allocated(ctx.r3.u32);
    CheckUnsorted(ctx, base, "leaving an allocation");
}

extern "C" PPC_FUNC(__imp__sub_820CC238);
PPC_FUNC(sub_820CC238)
{
    CheckUnsorted(ctx, base, "entering an allocation");
    __imp__sub_820CC238(ctx, base);
    if (Wanted() || Following()) Allocated(ctx.r3.u32);
    CheckUnsorted(ctx, base, "leaving an allocation");
}

extern "C" PPC_FUNC(__imp__sub_820CC2A0);
PPC_FUNC(sub_820CC2A0)
{
    // From a heap of the title's choosing (the level's), the same block shape.
    CheckUnsorted(ctx, base, "entering an allocation");
    __imp__sub_820CC2A0(ctx, base);
    if (Wanted() || Following()) Allocated(ctx.r3.u32);
    CheckUnsorted(ctx, base, "leaving an allocation");
}

extern "C" PPC_FUNC(__imp__sub_820CC330);
PPC_FUNC(sub_820CC330)
{
    CheckUnsorted(ctx, base, "entering an allocation");
    __imp__sub_820CC330(ctx, base);
    if (Wanted() || Following()) Allocated(ctx.r3.u32);
    CheckUnsorted(ctx, base, "leaving an allocation");
}

extern "C" PPC_FUNC(__imp__sub_820CC398);
PPC_FUNC(sub_820CC398)
{
    CheckUnsorted(ctx, base, "entering an allocation");
    __imp__sub_820CC398(ctx, base);
    if (Wanted() || Following()) Allocated(ctx.r3.u32);
    CheckUnsorted(ctx, base, "leaving an allocation");
}

extern "C" PPC_FUNC(__imp__sub_820CC468);
PPC_FUNC(sub_820CC468)
{
    const uint32_t pointer = ctx.r3.u32;
    CheckUnsorted(ctx, base, "entering a free");
    if ((Wanted() || Following()) && !Freed(ctx, base, pointer, "free")) return;
    __imp__sub_820CC468(ctx, base);
    // After the free: a list it has just joined may have closed on itself.
    if (Wanted()) CheckBins(ctx, base, "after freeing", pointer);
    CheckUnsorted(ctx, base, "leaving a free");
}

extern "C" PPC_FUNC(__imp__sub_820CC4C0);
PPC_FUNC(sub_820CC4C0)
{
    // free(heap, block): the block is the second argument.
    const uint32_t pointer = ctx.r4.u32;
    CheckUnsorted(ctx, base, "entering a free from a heap");
    if ((Wanted() || Following()) && !Freed(ctx, base, pointer, "free from a heap")) return;
    __imp__sub_820CC4C0(ctx, base);
    if (Wanted()) CheckBins(ctx, base, "after freeing", pointer);
    CheckUnsorted(ctx, base, "leaving a free from a heap");
}
