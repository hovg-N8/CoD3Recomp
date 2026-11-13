// (0.5.987) For co-op probes: whether a range of the host's memory is committed (readable), so a scan of the
// guest's heap skips what was never handed out instead of faulting.
#include <windows.h>
#include <cstddef>

bool CoopCommitted(const void* p, size_t n)
{
    const char* at = static_cast<const char*>(p);
    const char* end = at + n;
    while (at < end)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(at, &info, sizeof info) == 0) return false;
        if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        at = static_cast<const char*>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}
