// The window the frames go to.
//
// The renderer presents from the command thread, at the title's swap; this
// thread makes the window, pumps its messages, tells the renderer the
// window's size, and asks for the last frame again when the title has gone
// quiet, so a resize or the settings menu never shows a stale picture.
// The window is either a plain one at 720p or a borderless one over the
// whole screen, from the settings or Alt+Enter.

#include "version.h"
#include "kernel.h"
#include "window.h"
#include "log.h"
#include "overlay.h"
#include "settings.h"
#include "render.h"
#include "input.h"
#include "button_prompts.h"
#include "window_geometry.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#include <Windows.h>

namespace
{
    // The window opens at 720p, which is what the console put on screen,
    // and the picture fills it at 16 by 9 whatever size it is made.
    constexpr int Width = 1280;
    constexpr int Height = 720;

    std::atomic<bool> g_running{ false };
    std::atomic<HWND> g_window{ nullptr };
    std::thread g_thread;

    std::atomic<uint32_t> g_frontBuffer{ 0 };
    std::atomic<bool> g_cursorHidden{ false };
    std::atomic<int> g_wheel{ 0 };   // wheel notches since the last take

    // The mouse's own movement, added up as the device reports it and taken
    // by whoever looks. Long is enough: a fast hand is a few thousand counts
    // a second and a look happens every few milliseconds.
    std::atomic<long> g_rawX{ 0 }, g_rawY{ 0 };
    std::atomic<bool> g_rawRegistered{ false };

    // Frames the guest finished, counted where the front buffer changes.
    std::atomic<uint64_t> g_guestFrames{ 0 };

    bool g_borderless = false;
    WINDOWPLACEMENT g_placement{};
    std::atomic<bool> g_toggleWanted{ false };   // Alt+Enter

    void ReportSize(HWND window)
    {
        RECT client{};
        GetClientRect(window, &client);
        Render::SetWindow(window, client.right - client.left, client.bottom - client.top);
    }

    void KeepVisible(HWND window)
    {
        if (g_borderless || IsIconic(window) || IsZoomed(window)) return;
        MONITORINFO monitor{sizeof(monitor)}; RECT frame{};
        if (!GetWindowRect(window,&frame) ||
            !GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor)) return;
        const auto fit=WindowGeometry::Fit({int(frame.left),int(frame.top),int(frame.right),int(frame.bottom)},
            {int(monitor.rcWork.left),int(monitor.rcWork.top),int(monitor.rcWork.right),int(monitor.rcWork.bottom)});
        if (fit.left==frame.left && fit.top==frame.top && fit.right==frame.right && fit.bottom==frame.bottom) return;
        SetWindowPos(window,nullptr,fit.left,fit.top,fit.right-fit.left,fit.bottom-fit.top,
            SWP_NOZORDER|SWP_NOOWNERZORDER|SWP_NOACTIVATE);
        printf("window: kept window visible at %d,%d %dx%d\n",fit.left,fit.top,fit.right-fit.left,fit.bottom-fit.top);
        fflush(stdout);
    }

    // The window over the whole of its monitor with no frame, or back to
    // the plain one where it was.
    void SetBorderless(HWND window, bool borderless)
    {
        if (borderless == g_borderless) return;
        Input::SuspendMouseCapture(true);
        g_borderless = borderless;
        if (borderless)
        {
            g_placement.length = sizeof(g_placement);
            GetWindowPlacement(window, &g_placement);
            MONITORINFO monitor{ sizeof(monitor) };
            GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
            SetWindowLongPtrW(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowPos(window, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                monitor.rcMonitor.right - monitor.rcMonitor.left, monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
        }
        else
        {
            SetWindowLongPtrW(window, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetWindowPlacement(window, &g_placement);
            SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
            KeepVisible(window);
        }
        ReportSize(window);
        Input::SuspendMouseCapture(false);
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
    {
        // Recentring a captured pointer while Windows drags a border/title bar
        // pulls the window toward the old centre. Let Windows own that input.
        if (message==WM_ENTERSIZEMOVE) Input::SuspendMouseCapture(true);
        if (message==WM_EXITSIZEMOVE)
        {
            KeepVisible(window);
            Input::SuspendMouseCapture(false);
        }
        if (message==WM_NCLBUTTONDOWN || message==WM_NCLBUTTONDBLCLK || message==WM_NCRBUTTONDOWN)
        {
            Input::SuspendMouseCapture(true);
            const LRESULT result=DefWindowProcW(window,message,w,l);
            KeepVisible(window);
            Input::SuspendMouseCapture(false);
            return result;
        }
        // Alt+Enter: the other window mode, through the settings so it sticks.
        if (message == WM_SYSKEYDOWN && w == VK_RETURN && (l & (1 << 29)) != 0)
        {
            g_toggleWanted.store(true);
            return 0;
        }
        // The settings menu sees every message first: F11 and F12, and all
        // of the keys and the mouse while it is open.
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_LBUTTONDOWN ||
            message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN || message == WM_MOUSEWHEEL)
            ButtonPrompts::KeyboardUsed();
        if (Overlay::HandleMessage(window, message, uint64_t(w), int64_t(l))) return 0;
        // Windows asks the window what the pointer should look like whenever it
        // moves over it, and answering with nothing is how a pointer is hidden
        // over one window without touching it anywhere else.
        if (message == WM_SETCURSOR && LOWORD(l) == HTCLIENT && g_cursorHidden.load())
        {
            SetCursor(nullptr);
            return TRUE;
        }
        if (message == WM_SIZE)
        {
            if (w != SIZE_MINIMIZED) ReportSize(window);
            return 0;
        }
        // 0.5.105: the guest's main/audio threads do not stop when only
        // the window message loop ends. Finish the log and exit as the
        // campaign/multiplayer handoff does, without Kernel::Exit's prompt.
        if (message == WM_CLOSE)
        {
            printf("window: close requested; ending the game process\n");
            Log::Finish();
            ExitProcess(0);
            return 0;
        }
        if (message == WM_DESTROY)
        {
            g_running.store(false);
            PostQuitMessage(0);
            return 0;
        }
        if (message == WM_MOUSEWHEEL)
        {
            g_wheel.fetch_add(GET_WHEEL_DELTA_WPARAM(w) / WHEEL_DELTA);
            return 0;
        }
        // The mouse as its device reports it. Windows still moves the pointer
        // as it always did; this is the movement beside it, before the
        // pointer speed and the acceleration have had their say.
        if (message == WM_INPUT)
        {
            alignas(8) uint8_t buffer[sizeof(RAWINPUT) + 16];
            UINT size = sizeof(buffer);
            const UINT got = GetRawInputData(reinterpret_cast<HRAWINPUT>(l), RID_INPUT, buffer, &size, sizeof(RAWINPUTHEADER));
            if (got != UINT(-1) && got >= sizeof(RAWINPUTHEADER))
            {
                const RAWINPUT* raw = reinterpret_cast<const RAWINPUT*>(buffer);
                if (GetForegroundWindow() == window && raw->header.dwType == RIM_TYPEMOUSE &&
                    (raw->data.mouse.lLastX != 0 || raw->data.mouse.lLastY != 0 || raw->data.mouse.usButtonFlags != 0))
                    ButtonPrompts::KeyboardUsed();
                if (raw->header.dwType == RIM_TYPEMOUSE)
                {
                    if ((raw->data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0)
                    {
                        // A tablet, a touch screen or a remote desktop, which
                        // report where the pointer is rather than how far it
                        // went: the step is the difference from the last one.
                        static LONG lastX = 0, lastY = 0;
                        static bool have = false;
                        if (have)
                        {
                            g_rawX.fetch_add(raw->data.mouse.lLastX - lastX, std::memory_order_relaxed);
                            g_rawY.fetch_add(raw->data.mouse.lLastY - lastY, std::memory_order_relaxed);
                        }
                        lastX = raw->data.mouse.lLastX;
                        lastY = raw->data.mouse.lLastY;
                        have = true;
                    }
                    else
                    {
                        g_rawX.fetch_add(raw->data.mouse.lLastX, std::memory_order_relaxed);
                        g_rawY.fetch_add(raw->data.mouse.lLastY, std::memory_order_relaxed);
                    }
                }
            }
            // Windows asks that this one always go on for its own cleanup.
            return DefWindowProcW(window, message, w, l);
        }
        // No beep for Alt combinations the menu does not have.
        if (message == WM_SYSCHAR) return 0;
        return DefWindowProcW(window, message, w, l);
    }

    void WindowThread()
    {
        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);
        windowClass.lpfnWndProc = WindowProc;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));

        // Resource 1 is the icon compiled into the executable, so the title bar
        // and the taskbar show the same thing Explorer does.
        windowClass.hIcon = LoadIconW(windowClass.hInstance, MAKEINTRESOURCEW(1));
        windowClass.hIconSm = windowClass.hIcon;
        windowClass.lpszClassName = L"CoD3Recomp";
        RegisterClassExW(&windowClass);

        RECT desired{ 0, 0, Width, Height };
        AdjustWindowRect(&desired, WS_OVERLAPPEDWINDOW, FALSE);

        HWND window = CreateWindowExW(0, L"CoD3Recomp",
#ifdef COD3_MP
            L"Call of Duty 3 Multiplayer - recompiled v" COD3_VERSION_W, WS_OVERLAPPEDWINDOW,
#else
            L"Call of Duty 3 - recompiled v" COD3_VERSION_W, WS_OVERLAPPEDWINDOW,
#endif
            CW_USEDEFAULT, CW_USEDEFAULT,
            desired.right - desired.left, desired.bottom - desired.top,
            nullptr, nullptr, windowClass.hInstance, nullptr);

        if (window == nullptr)
        {
            fprintf(stderr, "window: could not be created, error %lu\n", GetLastError());
            g_running.store(false);
            return;
        }

        g_window.store(window);

        // The mouse's own movement, for the look. The window is the target
        // and no flag is set, so the reports come only while it is in front:
        // moving the mouse over another window is not steering.
        RAWINPUTDEVICE mouse{};
        mouse.usUsagePage = 0x01;   // generic desktop
        mouse.usUsage = 0x02;       // mouse
        mouse.dwFlags = 0;
        mouse.hwndTarget = window;
        if (RegisterRawInputDevices(&mouse, 1, sizeof(mouse)) != FALSE) g_rawRegistered.store(true);
        else printf("window: raw mouse input could not be asked for, error %lu; the pointer is followed instead\n", GetLastError());

        ShowWindow(window, SW_SHOW);
        ReportSize(window);
        // In front and taking the keys from the start, rather than behind
        // the console it was started from. COD3_BACKGROUND=1 leaves it
        // behind, and the keyboard and mouse alone: a run driven by a
        // script while someone is using the machine.
        static const bool background = []() {
            const char* text = getenv("COD3_BACKGROUND");
            return text != nullptr && text[0] != 0 && text[0] != '0';
        }();
        if (!background)
        {
            SetForegroundWindow(window);
            SetFocus(window);
        }
        // COD3_FULLSCREEN=1 over the settings, for a run.
        static const int fullscreenOverride = []() { const char* t = getenv("COD3_FULLSCREEN"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
        printf("window: open at %dx%d\n", Width, Height);
        fflush(stdout);

        MSG message{};
        while (g_running.load(std::memory_order_acquire))
        {
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT) { g_running.store(false); break; }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!g_running.load()) break;

            // The window mode the settings ask for, or Alt+Enter did.
            Settings::Values settings = Settings::Get();
            if (g_toggleWanted.exchange(false))
            {
                settings.windowMode = settings.windowMode == 0 ? 1 : 0;
                Settings::Set(settings);
            }
            const bool borderless = fullscreenOverride >= 0 ? fullscreenOverride != 0 : settings.windowMode == 1;
            SetBorderless(window, borderless);
            // The window the size of the chosen resolution, when that
            // changes, and no larger than the screen it is on.
            {
                static int appliedWidth = 0, appliedHeight = 0;
                int width, height;
                Settings::Resolution(settings, width, height);
                if (!borderless && (width != appliedWidth || height != appliedHeight))
                {
                    appliedWidth = width;
                    appliedHeight = height;
                    MONITORINFO monitor{ sizeof(monitor) };
                    GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor);
                    RECT frame{ 0, 0, width, height };
                    AdjustWindowRect(&frame, WS_OVERLAPPEDWINDOW, FALSE);
                    int frameWidth = frame.right - frame.left, frameHeight = frame.bottom - frame.top;
                    const int workWidth = monitor.rcWork.right - monitor.rcWork.left, workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
                    if (frameWidth > workWidth) frameWidth = workWidth;
                    if (frameHeight > workHeight) frameHeight = workHeight;
                    const int x = monitor.rcWork.left + (workWidth - frameWidth) / 2, y = monitor.rcWork.top + (workHeight - frameHeight) / 2;
                    Input::SuspendMouseCapture(true);
                    SetWindowPos(window, nullptr, x, y, frameWidth, frameHeight, SWP_NOZORDER | SWP_NOOWNERZORDER);
                    ReportSize(window);
                    Input::SuspendMouseCapture(false);
                }
            }

            Render::PresentIdle();
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        g_window.store(nullptr);
        DestroyWindow(window);
    }
}

void Window::Open()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) return;
    g_thread = std::thread(WindowThread);
}

void Window::Close()
{
    if (g_running.exchange(false) && g_thread.joinable())
        g_thread.join();
}

bool Window::IsOpen() { return g_running.load(); }

void* Window::Handle() { return g_window.load(); }

void Window::SetCursorHidden(bool hidden)
{
    g_cursorHidden.store(hidden);

    // The pointer is already sitting over the window, so nothing would ask
    // again until it moved. Sending the question once makes the change show.
    const HWND window = g_window.load();
    if (window != nullptr)
        PostMessageW(window, WM_SETCURSOR, WPARAM(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
}

bool Window::HasFocus()
{
    const HWND window = g_window.load();
    return window != nullptr && GetForegroundWindow() == window;
}

bool Window::TakeRawMouse(long& x, long& y)
{
    if (!g_rawRegistered.load()) { x = 0; y = 0; return false; }
    x = g_rawX.exchange(0, std::memory_order_relaxed);
    y = g_rawY.exchange(0, std::memory_order_relaxed);
    return true;
}

int Window::TakeWheel() { return g_wheel.exchange(0); }

void Window::SetFrontBuffer(uint32_t address, uint32_t width, uint32_t height)
{
    const uint32_t previous = g_frontBuffer.exchange(address);
    if (previous != address)
        g_guestFrames.fetch_add(1, std::memory_order_relaxed);

    // The first few: the title flips between two buffers every frame, and
    // one line a frame is a log of nothing else.
    static std::atomic<int> announced{ 0 };
    if (previous != address && (announced.load(std::memory_order_relaxed) < 6 && announced.fetch_add(1) < 6))
    {
        printf("window: front buffer at 0x%08X, %ux%u\n", address, width, height);
        fflush(stdout);
    }
}
