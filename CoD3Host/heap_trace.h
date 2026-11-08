#pragma once

// The check on the title's own heap (heap_trace.cpp, COD3_HEAPCHECK=1|2).

#include "kernel.h"

namespace HeapTrace
{
    // sub_823F04B0 is about to destroy the object in r3: noted and, the
    // first twenty times, printed with the object's vtable and the caller's
    // chain. Nothing without COD3_HEAPCHECK. The override that calls this
    // is in title_fixes.cpp.
    void Destroying(const PPCContext& ctx, uint8_t* base);
    // (co-op v45k, merged in 0.5.671) the joining copy: a free of a block the heap does not hold (freed
    // already, or never its) is not done. Set by coop.cpp when co-op joins; every block is followed from the start for it.
    void SetGuard(bool on);
}
