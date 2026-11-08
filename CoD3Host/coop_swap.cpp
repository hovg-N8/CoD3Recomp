// Co-op v46: the joining copy's checkpoint reload, seamless.
//
// When the host goes back to a checkpoint the joining copy has to load its mission again: its stopped
// world cannot follow the host's (World at War's clients reload with the server too). Loaded again in
// this process its stopped world's script threads and deleted things broke the new level (v45a, v46a/b:
// a spin, then a crash on Mayenne), so a new process loads it, as at the first join, but now behind this
// one: it starts hidden where this window is, while this one, silent on the network, shows the loading
// screen; when the new one is back in beside the host it shows its window and tells this one, which ends.
// No window closes in front of the player. If the new one dies, or never says, this one ends anyway
// after a while and the new one shows itself (or the old way, a plain restart, if it never started).
// (0.5.671 merge: campaign only, as the other co-op files; empty in multiplayer.)
#ifndef COD3_MP

#include "coop.h"
#include "window.h"

#include <Windows.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

void CoopRelaunchFresh();   // kernel_xam.cpp

namespace
{
    HANDLE g_swapEvent = nullptr;     // the new process: the old one's event, to say it is in
    bool g_revealed = false;
    double g_startedAt = -1;
}

// the old process: start the new one hidden behind this window; false if it could not be started
bool CoopSwapRelaunch()
{
    wchar_t executable[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    const std::filesystem::path exe(executable);
    char name[96]; snprintf(name, sizeof name, "Local\\CoD3CoopSwap_%lu_%lu", GetCurrentProcessId(), GetTickCount());
    HANDLE ready = CreateEventA(nullptr, TRUE, FALSE, name);
    if (!ready) return false;
    // where this window is, for the new one's
    if (HWND w = static_cast<HWND>(Window::Handle()))
    {
        RECT r{};
        if (GetWindowRect(w, &r) && !IsIconic(w))
        {
            char rect[96]; snprintf(rect, sizeof rect, "%ld,%ld,%ld,%ld", r.left, r.top, r.right - r.left, r.bottom - r.top);
            SetEnvironmentVariableA("COD3_WINDOW_RECT", rect);
        }
    }
    SetEnvironmentVariableA("COD3_COOP_RELOAD", "1"); _putenv_s("COD3_COOP_RELOAD", "1");
    SetEnvironmentVariableA("COD3_COOP_HIDDEN", "1");
    SetEnvironmentVariableA("COD3_COOP_SWAP", name);
    std::wstring commandLine = L"\"" + exe.wstring() + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    const std::wstring folder = exe.parent_path().wstring();
    const BOOL ok = CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, folder.c_str(), &startup, &process);
    SetEnvironmentVariableA("COD3_COOP_HIDDEN", nullptr);
    SetEnvironmentVariableA("COD3_COOP_SWAP", nullptr);
    if (!ok) { CloseHandle(ready); return false; }
    CloseHandle(process.hThread);
    printf("coop: swap: the new copy (process %lu) loads behind this one; this one waits for it\n", process.dwProcessId); fflush(stdout);
    std::thread([ready, child = process.hProcess] {
        HANDLE both[2] = { ready, child };
        const DWORD r = WaitForMultipleObjects(2, both, FALSE, 150000);
        if (r == WAIT_OBJECT_0) { printf("coop: swap: the new copy is in: this one ends\n"); fflush(stdout); Sleep(250); }
        else if (r == WAIT_OBJECT_0 + 1) { printf("coop: swap: the new copy ended before it was in: started again the old way\n"); fflush(stdout); CoopRelaunchFresh(); }
        else { printf("coop: swap: the new copy said nothing in 150 s: this one ends (it shows itself)\n"); fflush(stdout); }
        TerminateProcess(GetCurrentProcess(), 0);
    }).detach();
    return true;
}

namespace Coop
{
    bool SwapPending() { const char* s = getenv("COD3_COOP_SWAP"); return s && s[0] && !g_revealed; }
    // the new process: back in beside the host (or given up waiting): its window shown, the old one told
    void SwapReveal(const char* why)
    {
        if (g_revealed) return;
        const char* s = getenv("COD3_COOP_SWAP");
        if (!s || !s[0]) return;
        g_revealed = true;
        Window::Reveal();
        if (HANDLE e = OpenEventA(EVENT_MODIFY_STATE, FALSE, s)) { SetEvent(e); CloseHandle(e); }
        printf("coop: swap: this copy shown (%s); the old one told to end\n", why); fflush(stdout);
    }
    // each frame (the new process): shown at the latest 120 s after it started
    void SwapFrame()
    {
        if (!SwapPending()) return;
        if (g_startedAt < 0) g_startedAt = Now();
        if (Now() - g_startedAt > 120.0) SwapReveal("120 s and not back in: shown anyway");
    }
}
#endif
