// Xbox 360 kernel imports: the filesystem.
//
// The title asks for paths like game:\sp\global.cod or
// \Device\Harddisk0\Partition1\... and expects the disc it shipped on. Those
// resolve under the installed game folder. Writes go to a saves folder next to
// the executable instead, so nothing ever modifies the installed copy.

#include "kernel.h"
#include "film_hd.h"
#include "write_watch.h"
#include <vector>
#include "scheduler.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <map>
#include <atomic>

namespace { constexpr uint32_t X_STATUS_PENDING = 0x00000103; }
#include <mutex>
#include <set>
#include <string>

#include <Windows.h>

namespace fs = std::filesystem;

namespace
{
    constexpr uint32_t X_STATUS_SUCCESS              = 0x00000000;
    constexpr uint32_t X_STATUS_END_OF_FILE          = 0xC0000011;
    constexpr uint32_t X_STATUS_INVALID_HANDLE       = 0xC0000008;
    constexpr uint32_t X_STATUS_NO_MORE_FILES        = 0x80000006;
    constexpr uint32_t X_STATUS_BUFFER_TOO_SMALL     = 0xC0000023;
    constexpr uint32_t X_STATUS_INVALID_PARAMETER    = 0xC000000D;
    constexpr uint32_t X_STATUS_NO_SUCH_FILE         = 0xC000000F;
    constexpr uint32_t X_STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034;
    constexpr uint32_t X_STATUS_ACCESS_DENIED        = 0xC0000022;

    struct GuestFile
    {
        // Opened on a directory rather than a file. Reads from one are
        // refused the way the console refuses them, rather than treating its
        // zero size as the end of a file.
        bool isDirectory = false;

        // Where the listing has got to. Built on the first query and walked one
        // entry at a time after that, which is the shape the caller expects:
        // ask repeatedly until told there are no more.
        std::vector<std::pair<std::string, uint64_t>> entries;
        size_t entriesTaken = 0;
        bool entriesBuilt = false;
        fs::path hostPath;

        HANDLE handle = INVALID_HANDLE_VALUE;
        uint64_t position = 0;
        uint64_t size = 0;
        std::string guestPath;
        bool writable = false;

        // Opened for synchronous I/O (FILE_SYNCHRONOUS_IO_ALERT or NONALERT
        // among the create options). Reads and writes on any other handle
        // are asynchronous: the console's driver takes the request, answers
        // STATUS_PENDING, and finishes it later through the status block,
        // the event and the completion routine. The title's own file layer
        // is built on exactly that: it issues overlapped reads and treats a
        // ReadFile that returns TRUE, rather than FALSE with ERROR_IO_PENDING,
        // as a failure. That is what stopped every film after its first
        // block. Here the work is done at once and then reported the way
        // the console reports it: pending, with everything already filled
        // in for whoever comes to collect it.
        bool synchronous = true;
    };

    // The create options NtOpenFile passes in a register, for NtCreateFile,
    // which takes them from the caller's stack.
    thread_local bool t_optionsOverridden = false;
    thread_local uint32_t t_optionsOverride = 0;

    std::mutex g_filesMutex;

    std::map<uint32_t, GuestFile>& Files()
    {
        static std::map<uint32_t, GuestFile> files;
        return files;
    }

    // Kept well clear of the object handles in kernel_objects.cpp so a handle
    // used against the wrong layer is rejected rather than silently accepted.
    uint32_t g_nextFileHandle = 0x00010000;

    // Early file activity says more about where a boot is stuck than anything
    // else, so the first requests are reported whether they succeed or not.
    std::atomic<int> g_logged{ 0 };
    // COD3_FILELOG=N raises the limit, for a run where the opens after the
    // first four hundred matter.
    const int LogLimit = []() { const char* t = getenv("COD3_FILELOG"); return t ? int(strtol(t, nullptr, 10)) : 400; }();

    void LogOpen(const std::string& guestPath, const char* result)
    {
        if (g_logged.fetch_add(1, std::memory_order_relaxed) >= LogLimit) return;
        printf("file: %-52s %s\n", guestPath.c_str(), result);
        fflush(stdout);
    }

    fs::path g_savesRoot;

    // Content packages the title has mounted, by root name, lowered.
    std::mutex g_mountMutex;
    std::map<std::string, fs::path> g_mounts;

    std::string ReadGuestString(const uint8_t* base, uint32_t address, uint32_t length)
    {
        std::string out;
        out.reserve(length);
        for (uint32_t i = 0; i < length; i++)
            out += static_cast<char>(*(base + address + i));
        return out;
    }

    // OBJECT_ATTRIBUTES { uint32 rootDirectory; uint32 namePtr; uint32 attributes; }
    // ANSI_STRING       { uint16 length; uint16 maximumLength; uint32 buffer; }
    std::string ObjectName(const uint8_t* base, uint32_t objectAttributes)
    {
        if (objectAttributes == 0) return {};
        const uint32_t namePtr = Guest::Read32(base, objectAttributes + 0x04);
        if (namePtr == 0) return {};

        const uint16_t length = Guest::Read16(base, namePtr + 0x00);
        const uint32_t buffer = Guest::Read32(base, namePtr + 0x04);
        if (buffer == 0 || length == 0) return {};
        return ReadGuestString(base, buffer, length);
    }

    bool IsWriteAccess(uint32_t desiredAccess, uint32_t createDisposition)
    {
        constexpr uint32_t AccessGenericWrite = 0x40000000;
        constexpr uint32_t AccessWriteData = 0x0002;
        constexpr uint32_t AccessAppendData = 0x0004;
        if (desiredAccess & (AccessGenericWrite | AccessWriteData | AccessAppendData))
            return true;
        // FILE_CREATE, FILE_OVERWRITE, FILE_OVERWRITE_IF, FILE_SUPERSEDE
        return createDisposition == 0 || createDisposition == 2 ||
               createDisposition == 4 || createDisposition == 5;
    }

    // Turns a guest path into a host path. Returns an empty path when the
    // device is not one this runtime serves.
    // A film or its sound track, by extension.
    bool LooksLikeFilm(const std::string& guestPath)
    {
        static const char* const extensions[] = { ".wmv", ".wma" };
        for (const char* extension : extensions)
        {
            const size_t length = strlen(extension);
            if (guestPath.size() < length) continue;
            if (_stricmp(guestPath.c_str() + guestPath.size() - length, extension) == 0)
                return true;
        }
        return false;
    }

    fs::path Resolve(std::string guestPath, bool forWriting)
    {
        std::replace(guestPath.begin(), guestPath.end(), '\\', '/');

        // Strip a device prefix. Both the drive letter form the title uses and
        // the full device path the kernel uses point at the same disc.
        //
        // Written without the trailing separator so the device on its own
        // resolves to the disc root. The title opens it that way, and a match
        // that insisted on the separator turned that into no device at all.
        static const char* const discPrefixes[] = {
            "game:", "d:", "\\device/harddisk0/partition1",
            "/device/harddisk0/partition1", "/device/cdrom0", "cdrom0:",
        };

        std::string lowered = guestPath;
        std::transform(lowered.begin(), lowered.end(), lowered.begin(),
            [](unsigned char c) { return static_cast<char>(tolower(c)); });

        // A mounted content package: "save:" and what follows it.
        {
            std::lock_guard<std::mutex> lock(g_mountMutex);
            for (const auto& mount : g_mounts)
            {
                const std::string prefix = mount.first + ":";
                if (lowered.compare(0, prefix.size(), prefix) != 0) continue;
                if (lowered.size() > prefix.size() && lowered[prefix.size()] != '/') continue;
                std::string rest = guestPath.substr(prefix.size());
                while (!rest.empty() && rest.front() == '/') rest.erase(0, 1);
                if (rest.find("..") != std::string::npos) return {};
                return rest.empty() ? mount.second : mount.second / fs::path(rest);
            }
        }

        // A map pack's own drive, "dlc1:" and on: the title opens a pack's
        // maps there (its number from dlmaps.txt), having read the list from
        // the package it mounted under another name. Each is a package of
        // downloaded content (type 2) in the saves' content folder: the
        // number-th by name, or the first while there is only one.
        if (lowered.compare(0, 3, "dlc") == 0 && lowered.size() > 4 && isdigit((unsigned char)lowered[3]))
        {
            size_t at = 3;
            int number = 0;
            while (at < lowered.size() && isdigit((unsigned char)lowered[at])) number = number * 10 + (lowered[at++] - '0');
            if (at < lowered.size() && lowered[at] == ':')
            {
                std::vector<fs::path> packs;
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(g_savesRoot / "content" / "00000002", ec))
                    if (entry.is_directory()) packs.push_back(entry.path());
                std::sort(packs.begin(), packs.end());
                // The pack whose dlmaps.txt names this number (its first
                // column), else the number-th, else the first.
                auto packNumber = [](const fs::path& pack) {
                    FILE* list = _wfopen((pack / L"mp" / L"dlmaps.txt").c_str(), L"rb");
                    if (!list) return -1;
                    char line[512];
                    int found = -1;
                    while (found < 0 && fgets(line, sizeof(line), list))
                    {
                        const char* at = line;
                        while (*at == ' ' || *at == '\t') at++;
                        if (isdigit((unsigned char)*at)) found = atoi(at);
                    }
                    fclose(list);
                    return found;
                };
                if (!packs.empty())
                {
                    const fs::path* chosen = nullptr;
                    for (const fs::path& candidate : packs)
                        if (packNumber(candidate) == number) { chosen = &candidate; break; }
                    if (!chosen) chosen = (number >= 1 && size_t(number) <= packs.size()) ? &packs[number - 1] : &packs.front();
                    const fs::path& pack = *chosen;
                    std::string rest = guestPath.substr(at + 1);
                    while (!rest.empty() && rest.front() == '/') rest.erase(0, 1);
                    if (rest.find("..") != std::string::npos) return {};
                    return rest.empty() ? pack : pack / fs::path(rest);
                }
            }
        }

        std::string relative;
        bool matched = false;
        for (const char* prefix : discPrefixes)
        {
            const size_t length = strlen(prefix);
            if (lowered.compare(0, length, prefix) != 0) continue;

            // Either the whole path is the device, or what follows it starts a
            // path of its own.
            if (lowered.size() > length && lowered[length] != '/') continue;

            relative = guestPath.substr(length);
            while (!relative.empty() && relative.front() == '/') relative.erase(0, 1);
            matched = true;
            break;
        }

        if (!matched)
        {
            // A path with no device is relative to the disc root.
            if (guestPath.find(':') != std::string::npos)
                return {};
            relative = guestPath;
            while (!relative.empty() && relative.front() == '/') relative.erase(0, 1);
        }

        // The device on its own is the root of the disc, which the title opens
        // to ask about the disc rather than about a file on it.
        if (relative.empty())
            return forWriting ? g_savesRoot : Guest::GameRoot;

        // Refuse to escape the root. A guest path is untrusted input here just
        // like any other file name coming from data.
        if (relative.find("..") != std::string::npos) return {};

        return (forWriting ? g_savesRoot : Guest::GameRoot) / fs::path(relative);
    }

    fs::path FindExisting(const fs::path& wanted);
}

std::filesystem::path Kernel::ResolveGuestPath(const std::string& guestPath)
{
    const fs::path wanted = Resolve(guestPath, false);
    if (wanted.empty()) return {};
    return FindExisting(wanted);
}

namespace
{
    // The disc is case preserving but the title is not consistent about case,
    // and NTFS lookups are case insensitive, so a direct probe is usually
    // enough. This falls back to a directory scan when it is not.
    fs::path FindExisting(const fs::path& wanted)
    {
        std::error_code ec;
        if (fs::exists(wanted, ec)) return wanted;

        const fs::path parent = wanted.parent_path();
        if (!fs::is_directory(parent, ec)) return {};

        const std::string name = wanted.filename().string();
        for (const auto& entry : fs::directory_iterator(parent, ec))
        {
            if (ec) break;
            if (_stricmp(entry.path().filename().string().c_str(), name.c_str()) == 0)
                return entry.path();
        }
        return {};
    }
}

std::filesystem::path Kernel::SavesRoot() { return g_savesRoot; }

void Kernel::MountContent(const std::string& rootName, const fs::path& folder)
{
    std::string lowered = rootName;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    std::lock_guard<std::mutex> lock(g_mountMutex);
    g_mounts[lowered] = folder;
}

void Kernel::UnmountContent(const std::string& rootName)
{
    std::string lowered = rootName;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
        [](unsigned char c) { return static_cast<char>(tolower(c)); });
    std::lock_guard<std::mutex> lock(g_mountMutex);
    g_mounts.erase(lowered);
}

void Kernel::InitializeFileSystem(const fs::path& exeDirectory)
{
    g_savesRoot = exeDirectory / "saves";
    // COD3_SAVES=path: the saves somewhere else, so a scripted test run
    // beside the installed game does not write over the player's saves.
    if (const char* saves = getenv("COD3_SAVES"); saves != nullptr && saves[0] != 0)
        g_savesRoot = fs::path(saves);
    std::error_code ec;
    fs::create_directories(g_savesRoot, ec);
}

// NTSTATUS NtCreateFile(HANDLE* handle, ACCESS_MASK access,
//                       OBJECT_ATTRIBUTES* attributes, IO_STATUS_BLOCK* status,
//                       LARGE_INTEGER* allocationSize, ULONG fileAttributes,
//                       ULONG shareAccess, ULONG createDisposition,
//                       ULONG createOptions)
PPC_FUNC(__imp__NtCreateFile)
{
    Kernel::CountImport("NtCreateFile");
    Scheduler::Release();
    struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
    std::lock_guard<std::mutex> lock(g_filesMutex);

    const uint32_t handleOut = ctx.r3.u32;
    const uint32_t desiredAccess = ctx.r4.u32;
    const uint32_t objectAttributes = ctx.r5.u32;
    const uint32_t ioStatusBlock = ctx.r6.u32;
    const uint32_t createDisposition = ctx.r10.u32;

    // The ninth argument is the first one past the registers: it sits in
    // the caller's parameter save area, at 0x54 from its stack pointer.
    const uint32_t createOptions = t_optionsOverridden
        ? t_optionsOverride : Guest::Read32(base, ctx.r1.u32 + 0x54);
    t_optionsOverridden = false;
    constexpr uint32_t FileSynchronousIoAlert = 0x10;
    constexpr uint32_t FileSynchronousIoNonAlert = 0x20;

    const std::string guestPath = ObjectName(base, objectAttributes);
    if (guestPath.empty())
    {
        // An open whose name could not be read is still an open the title
        // wanted, and there are more of these than there are named ones.
        // Printing the structure it passed says which shape it used.
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 12 && announced.fetch_add(1) < 12) && objectAttributes != 0)
        {
            printf("file: open with an unreadable name, attributes at 0x%08X:",
                objectAttributes);
            for (int i = 0; i < 5; i++)
                printf(" %08X", Guest::Read32(base, objectAttributes + i * 4));
            printf("\n");
            fflush(stdout);
        }
        ctx.r3.u32 = X_STATUS_INVALID_PARAMETER;
        return;
    }

    const bool writing = IsWriteAccess(desiredAccess, createDisposition);
    fs::path hostPath = Resolve(guestPath, writing);
    if (hostPath.empty())
    {
        printf("file: no device for %s\n", guestPath.c_str());
        ctx.r3.u32 = X_STATUS_OBJECT_NAME_NOT_FOUND;
        return;
    }

    // The films, unless COD3_NOFILMS=1 asks for them to be left out: then
    // they are reported absent, which the title handles the way it handles
    // a missing film, and goes on. (They were left out by default while the
    // player could not run; it can now, with its own decoder, its sound
    // through the XMA contexts and a read that says how much it read.)
    static const bool noFilms = getenv("COD3_NOFILMS") != nullptr;
    if (!writing && noFilms && LooksLikeFilm(guestPath))
    {
        LogOpen(guestPath, "skipped");
        ctx.r3.u32 = X_STATUS_OBJECT_NAME_NOT_FOUND;
        return;
    }

    if (!writing)
    {
        const fs::path found = FindExisting(hostPath);
        if (found.empty())
        {
            LogOpen(guestPath, "NOT FOUND");
            ctx.r3.u32 = X_STATUS_OBJECT_NAME_NOT_FOUND;
            return;
        }
        hostPath = found;
        // A film: its remastered frames, if it has any (film_hd.cpp).
        if (LooksLikeFilm(guestPath)) FilmHd::Opened(hostPath);
    }
    else
    {
        std::error_code creating;
        fs::create_directories(hostPath.parent_path(), creating);
    }

    // FILE_OPEN is 1, FILE_OPEN_IF is 3; everything else creates or truncates.
    DWORD creation = OPEN_EXISTING;
    if (writing)
        creation = (createDisposition == 1) ? OPEN_EXISTING
                 : (createDisposition == 3) ? OPEN_ALWAYS
                 : CREATE_ALWAYS;

    // A directory is opened with a flag of its own.
    //
    // The title opens the disc root, D:\\, the way it opens anything else.
    // Without this that call fails, and the title answers a disc it cannot open
    // by throwing: XenonRecomp translates no exception handling at all, so the
    // throw runs off through a jump buffer into whatever happens to be there.
    // The first sign of it was an indirect call to 0x825C6F68, an address past
    // the end of the code, made with a structure full of stack pointers.
    std::error_code ec;
    const bool directory = fs::is_directory(hostPath, ec);

    HANDLE host = CreateFileW(hostPath.c_str(),
        writing ? (GENERIC_READ | GENERIC_WRITE) : GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, creation,
        directory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr);

    if (host == INVALID_HANDLE_VALUE)
    {
        LogOpen(guestPath, "cannot open");
        ctx.r3.u32 = (GetLastError() == ERROR_ACCESS_DENIED)
            ? X_STATUS_ACCESS_DENIED : X_STATUS_OBJECT_NAME_NOT_FOUND;
        return;
    }

    LARGE_INTEGER size{};
    GetFileSizeEx(host, &size);

    GuestFile file;
    file.isDirectory = directory;
    file.hostPath = hostPath;
    file.handle = host;
    file.size = static_cast<uint64_t>(size.QuadPart);
    file.guestPath = guestPath;
    file.writable = writing;
    file.synchronous = (createOptions & (FileSynchronousIoAlert | FileSynchronousIoNonAlert)) != 0;

    const uint32_t handle = g_nextFileHandle;
    g_nextFileHandle += 4;
    Files()[handle] = file;

    LogOpen(guestPath, "opened");
    Kernel::Stats().filesOpened.fetch_add(1, std::memory_order_relaxed);

    // A multiplayer map's own archive (D:\mp\<map>\<map>.cod): the map
    // being loaded, for what is kept off some maps (mp_fixes.cpp).
    {
        std::string lower = guestPath;
        for (char& c : lower) c = char(tolower(static_cast<unsigned char>(c)));
        const size_t at = lower.find("\\mp\\");
        if (at != std::string::npos)
        {
            const size_t start = at + 4, slash = lower.find('\\', start);
            if (slash != std::string::npos && lower.compare(slash + 1, std::string::npos, lower.substr(start, slash - start) + ".cod") == 0)
                Kernel::CurrentMap() = lower.substr(start, slash - start);
        }
    }

    // Who opens an archive: the guest's call chain, once for each .cod (for
    // finding how the title mounts its banks, to mount one of its own).
    {
        static std::set<std::string> traced;
        std::string lower = guestPath;
        for (char& c : lower) c = char(tolower(static_cast<unsigned char>(c)));
        if (lower.size() > 4 && (lower.compare(lower.size() - 4, 4, ".cod") == 0 || lower.compare(lower.size() - 4, 4, ".wbk") == 0) && traced.insert(lower).second && traced.size() < 64)
        {
            printf("file: %s opened from %08X", guestPath.c_str(), uint32_t(ctx.lr));
            uint32_t frame = ctx.r1.u32;
            for (int depth = 0; depth < 16; depth++)
            {
                const uint32_t caller = Guest::Read32(base, frame);
                if (caller <= frame || caller - frame > 0x100000 || (caller & 7) != 0 || caller >= 0xC0000000u) break;
                const uint32_t address = Guest::Read32(base, caller - 8);
                if (address < 0x82000000u || address >= 0x8A000000u) break;
                printf(" %08X", address);
                frame = caller;
            }
            printf("  (r3-r6 %08X %08X %08X %08X)\n", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
            fflush(stdout);
        }
    }

    if (handleOut != 0)
        Guest::Write32(base, handleOut, handle);
    if (ioStatusBlock != 0)
    {
        Guest::Write32(base, ioStatusBlock + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ioStatusBlock + 4, 1);   // FILE_OPENED
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtOpenFile(HANDLE* handle, ACCESS_MASK access,
//                     OBJECT_ATTRIBUTES* attributes, IO_STATUS_BLOCK* status,
//                     ULONG shareAccess, ULONG openOptions)
PPC_FUNC(__imp__NtOpenFile)
{
    Kernel::CountImport("NtOpenFile");
    // Same shape as NtCreateFile with FILE_OPEN, which is what the arguments
    // already mean here.
    const uint32_t shareAccess = ctx.r7.u32;
    (void)shareAccess;
    t_optionsOverridden = true;
    t_optionsOverride = ctx.r8.u32;   // openOptions
    ctx.r10.u32 = 1;   // FILE_OPEN
    __imp__NtCreateFile(ctx, base);
}

// NTSTATUS NtReadFile(HANDLE handle, HANDLE event, APC*, void*,
//                     IO_STATUS_BLOCK* status, void* buffer, ULONG length,
//                     LARGE_INTEGER* offset)
namespace
{
    // COD3_TRACEFILE names part of a guest path; every read of a file whose
    // path contains it is printed with its offset, length and outcome.
    bool TracedFile(const std::string& guestPath)
    {
        static const std::string needle = []() {
            const char* text = getenv("COD3_TRACEFILE");
            std::string value = text != nullptr ? text : "";
            for (char& c : value) c = char(tolower(uint8_t(c)));
            return value;
        }();
        if (needle.empty()) return false;
        std::string lower = guestPath;
        for (char& c : lower) c = char(tolower(uint8_t(c)));
        return lower.find(needle) != std::string::npos;
    }
}

PPC_FUNC(__imp__NtReadFile)
{
    Kernel::CountImportOn("NtReadFile", ctx.r3.u32);
    Scheduler::Release();
    struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
    std::unique_lock<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found == Files().end()) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    GuestFile& file = found->second;

    const uint32_t ioStatusBlock = ctx.r7.u32;
    const uint32_t buffer = ctx.r8.u32;
    const uint32_t length = ctx.r9.u32;
    const uint32_t offsetPtr = ctx.r10.u32;

    // An asynchronous read names an event to signal when it finishes, and the
    // title waits on that event rather than on the call returning. Reading the
    // bytes and not signalling leaves it waiting forever for a read that has
    // already happened.
    const uint32_t completionEvent = ctx.r4.u32;
    const uint32_t apcRoutine = ctx.r5.u32;
    const uint32_t apcContext = ctx.r6.u32;

    struct Complete
    {
        uint32_t event;
        ~Complete() { if (event != 0) Kernel::SignalHandle(event); }
    } complete{ completionEvent };

    if (apcRoutine != 0)
    {
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 3 && announced.fetch_add(1) < 3))
        {
            printf("file: reads are asynchronous, completing through 0x%08X\n",
                apcRoutine);
            fflush(stdout);
        }
    }

    if (buffer == 0) { ctx.r3.u32 = X_STATUS_INVALID_PARAMETER; return; }

    if (offsetPtr != 0)
        file.position = Guest::Read64(base, offsetPtr);

    if (file.position >= file.size)
    {
        if (ioStatusBlock != 0)
        {
            Guest::Write32(base, ioStatusBlock + 0, X_STATUS_END_OF_FILE);
            Guest::Write32(base, ioStatusBlock + 4, 0);
        }
        if (TracedFile(file.guestPath))
        {
            printf("trace: read %u at %llu of %s: past the end (size %llu)\n",
                length, (unsigned long long)file.position, file.guestPath.c_str(),
                (unsigned long long)file.size);
            fflush(stdout);
        }
        if (apcRoutine != 0)
            Kernel::QueueApc(apcRoutine, apcContext, ioStatusBlock, X_STATUS_END_OF_FILE, 0);
        ctx.r3.u32 = (apcRoutine != 0 || !file.synchronous) ? X_STATUS_PENDING : X_STATUS_END_OF_FILE;
        return;
    }

    DWORD read = 0;
    {
        LARGE_INTEGER position;
        position.QuadPart = static_cast<LONGLONG>(file.position);
        SetFilePointerEx(file.handle, position, nullptr, FILE_BEGIN);

        // Straight into guest memory: file data is bytes, so there is no
        // byte order to correct. The guest interprets the big endian
        // contents itself.
        // The system writes this, not the CPU: a watched page would fail it.
        WriteWatch::Forget(buffer, length);
        const BOOL ok = ReadFile(file.handle, Guest::Ptr(buffer), length, &read, nullptr);
        if (!ok)
        {
            ctx.r3.u32 = X_STATUS_END_OF_FILE;
            return;
        }
    }

    // A read that runs past the end of one of the game's archives is filled
    // with zeros to its full length and reported as all read, the way a
    // whole-sector read off the disc comes back. The island level asks for
    // 409600 bytes of a 196608 byte archive (its streamer reads in that
    // unit), and given the end of file it then met it reported a dirty
    // disc; given the zeros it plays. Only the archives (.cod, .wbk) and
    // the configs get this: the film player reads its .wma and .wmv by the
    // count that comes back, and told a 121647 byte sound track was 122880
    // it read on past the end and reported a dirty disc of its own. Saved
    // games keep the exact count too. COD3_NOPADREADS=1 turns it off.
    static const bool padReads = getenv("COD3_NOPADREADS") == nullptr;
    const bool archive = [&]() {
        const std::string& path = file.guestPath;
        const size_t dot = path.rfind('.');
        if (dot == std::string::npos) return false;
        std::string extension = path.substr(dot);
        for (char& c : extension) c = char(tolower(uint8_t(c)));
        return extension == ".cod" || extension == ".wbk" || extension == ".cfg";
    }();
    if (padReads && archive && read < length && file.guestPath.rfind("savedrive:", 0) != 0)
    {
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 20 && announced.fetch_add(1) < 20))
            printf("file: read of %u at %llu of %s (%llu bytes) padded with %u zeros\n", length, (unsigned long long)file.position,
                file.guestPath.c_str(), (unsigned long long)file.size, length - read);
        memset(Guest::Ptr(buffer) + read, 0, length - read);
        read = length;
    }

    file.position += read;
    Kernel::Stats().fileBytesRead.fetch_add(read, std::memory_order_relaxed);

    // How much came out of each file. The log below only ever printed the first
    // few reads of the whole run, which all came from the first archive, and
    // that made every later file look as though it was opened and never read.
    Kernel::CountFileRead(file.guestPath, read);

    {
        // Which file, not only how much: a load that stops is easier to place
        // when the last read names the archive it came from.
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 8 && announced.fetch_add(1) < 8))
        {
            printf("file: read %u of %u from %s at offset %llu%s\n",
                read, length, file.guestPath.c_str(),
                (unsigned long long)(file.position - read),
                apcRoutine != 0 ? ", asynchronous" : "");
            fflush(stdout);
        }
    }
    // One answer, used everywhere: the status block, the completion routine
    // and the return value all say the same thing. They used to disagree at
    // the end of a file, where the call returned end-of-file while the
    // routine was told success.
    const uint32_t finalStatus = (read == 0) ? X_STATUS_END_OF_FILE : X_STATUS_SUCCESS;
    if (ioStatusBlock != 0)
    {
        Guest::Write32(base, ioStatusBlock + 0, finalStatus);
        Guest::Write32(base, ioStatusBlock + 4, read);
    }
    ctx.r3.u32 = finalStatus;
    if (TracedFile(file.guestPath))
    {
        Kernel::StartKernelTrace();
        printf("trace: read %u of %u at %llu of %s into 0x%08X, status 0x%08X%s%s, from 0x%08X\n",
            read, length, (unsigned long long)(file.position - read), file.guestPath.c_str(),
            buffer, finalStatus, completionEvent != 0 ? ", event" : "",
            apcRoutine != 0 ? ", apc" : "", uint32_t(ctx.lr));
        fflush(stdout);
    }

    // A read that named a completion routine is asynchronous as far as the
    // title is concerned, however quickly the bytes actually arrived. It gets
    // the answer the console gives: the work is pending, and the routine will
    // run when the thread next waits.
    if (apcRoutine != 0)
    {
        Kernel::QueueApc(apcRoutine, apcContext, ioStatusBlock, finalStatus, read);
        ctx.r3.u32 = X_STATUS_PENDING;
    }
    else if (!file.synchronous)
    {
        // An asynchronous handle: the status block already holds the
        // outcome and the event, if any, is signalled on the way out, so
        // the title finds the read finished the moment it looks.
        ctx.r3.u32 = X_STATUS_PENDING;
    }
}

// NTSTATUS NtWriteFile(...) with the same argument shape as NtReadFile
PPC_FUNC(__imp__NtWriteFile)
{
    Kernel::CountImport("NtWriteFile");
    Scheduler::Release();
    struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
    std::lock_guard<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found == Files().end()) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    GuestFile& file = found->second;

    if (!file.writable) { ctx.r3.u32 = X_STATUS_ACCESS_DENIED; return; }

    const uint32_t ioStatusBlock = ctx.r7.u32;
    const uint32_t buffer = ctx.r8.u32;
    const uint32_t length = ctx.r9.u32;
    const uint32_t offsetPtr = ctx.r10.u32;

    if (offsetPtr != 0)
        file.position = Guest::Read64(base, offsetPtr);

    LARGE_INTEGER position;
    position.QuadPart = static_cast<LONGLONG>(file.position);
    SetFilePointerEx(file.handle, position, nullptr, FILE_BEGIN);

    DWORD written = 0;
    WriteFile(file.handle, Guest::Ptr(buffer), length, &written, nullptr);
    file.position += written;
    if (file.position > file.size) file.size = file.position;

    if (ioStatusBlock != 0)
    {
        Guest::Write32(base, ioStatusBlock + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ioStatusBlock + 4, written);
    }
    if (ctx.r4.u32 != 0) Kernel::SignalHandle(ctx.r4.u32);   // the completion event
    ctx.r3.u32 = file.synchronous ? X_STATUS_SUCCESS : X_STATUS_PENDING;
}

// NTSTATUS NtQueryInformationFile(HANDLE, IO_STATUS_BLOCK*, void* info,
//                                 ULONG length, ULONG infoClass)
PPC_FUNC(__imp__NtQueryInformationFile)
{
    Kernel::CountImportOn("NtQueryInformationFile", (ctx.r3.u32 << 16) | (ctx.r7.u32 & 0xFFFF));
    std::lock_guard<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found == Files().end()) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    const GuestFile& file = found->second;

    const uint32_t info = ctx.r5.u32;
    const uint32_t infoClass = ctx.r7.u32;

    if (info == 0) { ctx.r3.u32 = X_STATUS_INVALID_PARAMETER; return; }

    switch (infoClass)
    {
    case 14:  // FilePositionInformation
        Guest::Write64(base, info, file.position);
        break;
    // A directory says so. Every one of these used to describe whatever was
    // opened as an ordinary file, and the title, having opened the disc root
    // and been told it was a file, threw IdvFileError and stopped.
    case 5:   // FileStandardInformation
        Guest::Write64(base, info + 0x00, file.size);   // allocation size
        Guest::Write64(base, info + 0x08, file.size);   // end of file
        Guest::Write32(base, info + 0x10, 1);           // number of links
        Guest::Write8(base, info + 0x14, 0);            // delete pending
        Guest::Write8(base, info + 0x15, file.isDirectory ? 1 : 0);
        break;
    case 34:  // FileNetworkOpenInformation
        // Four timestamps, then the allocation size and the end of file, then
        // the attributes. This is how the title asks how big an archive is, so
        // getting the class number wrong stops it reading the archive at all.
        memset(Guest::Ptr(info), 0, 0x38);
        Guest::Write64(base, info + 0x20, file.size);   // allocation size
        Guest::Write64(base, info + 0x28, file.size);   // end of file
        Guest::Write32(base, info + 0x30, file.isDirectory ? 0x10u : 0x80u);
        break;

    case 4:   // FileBasicInformation: timestamps and attributes
        memset(Guest::Ptr(info), 0, 0x28);
        Guest::Write32(base, info + 0x20, file.isDirectory ? 0x10u : 0x80u);
        break;
    default:
        printf("NtQueryInformationFile: class %u is not implemented\n", infoClass);
        ctx.r3.u32 = X_STATUS_INVALID_PARAMETER;
        return;
    }

    if (ctx.r4.u32 != 0)
    {
        Guest::Write32(base, ctx.r4.u32 + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ctx.r4.u32 + 4, ctx.r6.u32);
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtSetInformationFile(HANDLE, IO_STATUS_BLOCK*, void* info,
//                               ULONG length, ULONG infoClass)
PPC_FUNC(__imp__NtSetInformationFile)
{
    Kernel::CountImportOn("NtSetInformationFile", (ctx.r3.u32 << 16) | (ctx.r7.u32 & 0xFFFF));
    std::lock_guard<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found == Files().end()) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    GuestFile& file = found->second;

    const uint32_t info = ctx.r5.u32;
    const uint32_t infoClass = ctx.r7.u32;

    if (infoClass == 14 && info != 0)          // FilePositionInformation
    {
        file.position = Guest::Read64(base, info);
    }
    else if (infoClass == 20 && info != 0)     // FileEndOfFileInformation
    {
        file.size = Guest::Read64(base, info);
    }
    else
    {
        printf("NtSetInformationFile: class %u is not implemented\n", infoClass);
    }

    if (ctx.r4.u32 != 0)
    {
        Guest::Write32(base, ctx.r4.u32 + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ctx.r4.u32 + 4, 0);
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtQueryVolumeInformationFile(...)
PPC_FUNC(__imp__NtQueryVolumeInformationFile)
{
    Kernel::CountImportOn("NtQueryVolumeInformationFile", (ctx.r3.u32 << 16) | (ctx.r7.u32 & 0xFFFF));
    const uint32_t info = ctx.r5.u32;
    const uint32_t length = ctx.r6.u32;
    const uint32_t infoClass = ctx.r7.u32;

    {
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 6 && announced.fetch_add(1) < 6))
        {
            printf("file: volume information class %u asked for, %u bytes\n",
                infoClass, length);
            fflush(stdout);
        }
    }

    if (info != 0) memset(Guest::Ptr(info), 0, length > 64 ? 64 : length);

    // FileFsSizeInformation: total and free space, in allocation units. A
    // disc that reports no space at all is a disc the title cannot trust, so
    // it gets the shape of a real one: a great many 2 KB sectors, half free.
    if (infoClass == 3 && info != 0 && length >= 24)
    {
        Guest::Write64(base, info + 0,  0x00400000);   // total allocation units
        Guest::Write64(base, info + 8,  0x00200000);   // available
        Guest::Write32(base, info + 16, 1);            // sectors per unit
        Guest::Write32(base, info + 20, 2048);         // bytes per sector
    }
    // FileFsDeviceInformation: a CD-ROM, which is what D: is on the console.
    else if (infoClass == 4 && info != 0 && length >= 8)
    {
        Guest::Write32(base, info + 0, 2);             // FILE_DEVICE_CD_ROM
        Guest::Write32(base, info + 4, 0);
    }

    if (ctx.r4.u32 != 0)
    {
        Guest::Write32(base, ctx.r4.u32 + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ctx.r4.u32 + 4, length);
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtQueryDirectoryFile(HANDLE, HANDLE event, APC*, void* apcContext,
//                               IO_STATUS_BLOCK*, void* info, ULONG length,
//                               ANSI_STRING* name, BOOLEAN restartScan)
//
// Listing a directory.
//
// This used to answer that there were no more files, always. The title opens
// the disc root and asks what is on it, and an empty answer is not "no files":
// it is a disc with nothing readable on it, which is what the title then said,
// by throwing. Since XenonRecomp translates no exception handling, that throw
// ended the run with a message about a dirty disc and no way to see why.
//
// One entry per call, in the order the host gives them, until there are none
// left. The pattern in the name argument is ignored: this title asks for
// everything, and answering with more than was asked for is worse than
// answering with all of it.
PPC_FUNC(__imp__NtQueryDirectoryFile)
{
    Kernel::CountImportOn("NtQueryDirectoryFile", ctx.r3.u32);
    std::lock_guard<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found == Files().end()) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

    GuestFile& file = found->second;
    const uint32_t ioStatusBlock = ctx.r7.u32;
    const uint32_t info = ctx.r8.u32;
    const uint32_t length = ctx.r9.u32;

    if (!file.isDirectory) { ctx.r3.u32 = X_STATUS_INVALID_PARAMETER; return; }

    if (!file.entriesBuilt)
    {
        file.entriesBuilt = true;
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(file.hostPath, ec))
        {
            std::error_code sizeError;
            const uint64_t size = entry.is_directory(sizeError)
                ? 0 : uint64_t(entry.file_size(sizeError));
            file.entries.emplace_back(entry.path().filename().string(),
                entry.is_directory(sizeError) ? UINT64_MAX : size);
        }

        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 4 && announced.fetch_add(1) < 4))
        {
            printf("file: %s holds %zu entries\n",
                file.guestPath.c_str(), file.entries.size());
            fflush(stdout);
        }
    }

    if (file.entriesTaken >= file.entries.size())
    {
        ctx.r3.u32 = X_STATUS_NO_MORE_FILES;
        return;
    }

    const auto& entry = file.entries[file.entriesTaken];
    const std::string& name = entry.first;
    const bool isDirectory = entry.second == UINT64_MAX;
    const uint64_t size = isDirectory ? 0 : entry.second;

    // The console lays this out as a next-entry offset, an index, four times
    // sixty four bits of time, the size, the allocation size, the attributes,
    // the length of the name, and then the name itself in plain bytes.
    constexpr uint32_t HeaderBytes = 72;
    if (info == 0 || length < HeaderBytes + name.size())
    {
        ctx.r3.u32 = X_STATUS_BUFFER_TOO_SMALL;
        return;
    }

    memset(Guest::Ptr(info), 0, HeaderBytes);
    Guest::Write32(base, info + 0, 0);                       // no next entry
    Guest::Write32(base, info + 4, uint32_t(file.entriesTaken));
    Guest::Write64(base, info + 40, size);                   // end of file
    Guest::Write64(base, info + 48, size);                   // allocation size
    Guest::Write32(base, info + 56, isDirectory ? 0x10u : 0x80u);  // directory or normal
    Guest::Write32(base, info + 60, uint32_t(name.size()));
    for (size_t i = 0; i < name.size(); i++)
        Guest::Write8(base, info + 64 + uint32_t(i), uint8_t(name[i]));

    file.entriesTaken++;

    if (ioStatusBlock != 0)
    {
        Guest::Write32(base, ioStatusBlock + 0, X_STATUS_SUCCESS);
        Guest::Write32(base, ioStatusBlock + 4, HeaderBytes + uint32_t(name.size()));
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtFlushBuffersFile(HANDLE, IO_STATUS_BLOCK*)
PPC_FUNC(__imp__NtFlushBuffersFile)
{
    Kernel::CountImport("NtFlushBuffersFile");
    std::lock_guard<std::mutex> lock(g_filesMutex);

    auto found = Files().find(ctx.r3.u32);
    if (found != Files().end() && found->second.writable)
        FlushFileBuffers(found->second.handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

namespace
{
    std::mutex g_readMutex;
    std::map<std::string, uint64_t> g_bytesByFile;
}

void Kernel::CountFileRead(const std::string& guestPath, uint32_t bytes)
{
    if (bytes == 0) return;
    std::lock_guard<std::mutex> lock(g_readMutex);
    g_bytesByFile[guestPath] += bytes;
}

void Kernel::ReportFileReads()
{
    std::lock_guard<std::mutex> lock(g_readMutex);
    if (g_bytesByFile.empty()) return;

    // Biggest first, because the question is nearly always which file the title
    // is actually pulling on.
    std::vector<std::pair<std::string, uint64_t>> sorted(
        g_bytesByFile.begin(), g_bytesByFile.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    printf("file: read from");
    int shown = 0;
    for (const auto& entry : sorted)
    {
        if (shown++ >= 10) break;
        printf(" %s %.1f MB", entry.first.c_str(), entry.second / 1048576.0);
    }
    printf("\n");
    fflush(stdout);
}

bool Kernel::IsFileHandle(uint32_t handle)
{
    std::lock_guard<std::mutex> lock(g_filesMutex);
    return Files().find(handle) != Files().end();
}

bool Kernel::CloseFileHandle(uint32_t handle)
{
    std::lock_guard<std::mutex> lock(g_filesMutex);
    auto found = Files().find(handle);
    if (found == Files().end()) return false;

    if (found->second.handle != INVALID_HANDLE_VALUE)
        CloseHandle(found->second.handle);
    Files().erase(found);
    return true;
}
