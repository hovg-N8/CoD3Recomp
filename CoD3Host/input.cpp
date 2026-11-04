#include "input.h"
#include "menu_input.h"
#include "keyboard_actions.h"
#include "button_prompts.h"
#include "sprint_input.h"
#include "weapon_cycle_input.h"
#include "kernel.h"
#include "window.h"
#include "overlay.h"
#include "settings.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

#include <Windows.h>

namespace
{
    std::atomic<bool> g_scopedBreath{true};
    std::atomic<bool> g_tacticalPointActive{false}, g_tacticalPointCanReturn{false}, g_tacticalPointSaveHeld{false};
    std::atomic<bool> g_tacticalPointReturnHeld{false};
    std::atomic<bool> g_weaponCycleEnabled{false};
    WeaponCycleInput::Latch g_weaponCycle;
    // The Xbox button masks, which are also the XInput ones.
    constexpr uint16_t PadUp        = 0x0001;
    constexpr uint16_t PadDown      = 0x0002;
    constexpr uint16_t PadLeft      = 0x0004;
    constexpr uint16_t PadRight     = 0x0008;
    constexpr uint16_t PadStart     = 0x0010;
    constexpr uint16_t PadBack      = 0x0020;
    constexpr uint16_t PadLeftThumb  = 0x0040;
    constexpr uint16_t PadRightThumb = 0x0080;
    constexpr uint16_t PadLeftShoulder  = 0x0100;
    constexpr uint16_t PadRightShoulder = 0x0200;
    constexpr uint16_t PadA         = 0x1000;
    constexpr uint16_t PadB         = 0x2000;
    constexpr uint16_t PadX         = 0x4000;
    constexpr uint16_t PadY         = 0x8000;

    constexpr int16_t StickFull = 32767;

    struct XInputGamepad
    {
        uint16_t buttons;
        uint8_t leftTrigger;
        uint8_t rightTrigger;
        int16_t thumbLX, thumbLY, thumbRX, thumbRY;
    };
    struct XInputState
    {
        uint32_t packetNumber;
        XInputGamepad gamepad;
    };

    struct XInputVibration { uint16_t left, right; };

    using GetStateFunction = uint32_t (WINAPI*)(uint32_t, XInputState*);
    using SetStateFunction = uint32_t (WINAPI*)(uint32_t, XInputVibration*);
    GetStateFunction g_getState = nullptr;
    SetStateFunction g_setState = nullptr;
    bool g_looked = false;

    void FindXInput()
    {
        if (g_looked) return;
        g_looked = true;

        // Whichever version this machine has. Loading it by name keeps the
        // build from needing the library at all.
        static const wchar_t* const names[] = {
            L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll" };
        for (const wchar_t* name : names)
        {
            HMODULE library = LoadLibraryW(name);
            if (library == nullptr) continue;
            g_getState = reinterpret_cast<GetStateFunction>(
                reinterpret_cast<void*>(GetProcAddress(library, "XInputGetState")));
            g_setState = reinterpret_cast<SetStateFunction>(
                reinterpret_cast<void*>(GetProcAddress(library, "XInputSetState")));
            if (g_getState != nullptr)
            {
                printf("input: XInput from %ls\n", name);
                fflush(stdout);
                return;
            }
        }
        printf("input: no XInput library on this machine, so no controller can be read\n");
        fflush(stdout);
    }

    // The pads, on any of XInput's four slots.
    //
    // Only slot 0 used to be read. Windows gives a controller whichever slot
    // is free when it connects, and anything else that speaks XInput takes
    // one first: Steam Input's virtual pad, DS4Windows, a wheel, a second pad
    // that was switched off, or the same pad reconnected. A controller on
    // slot 1 lit up and was never read, so it moved nothing and jumped
    // nothing while the keyboard worked. Every connected slot is read now
    // and merged, the way the keyboard and the pad already are.
    //
    // Asking an empty slot is slow (a millisecond or more, every call), and
    // the title asks hundreds of times a second, so empty slots are looked at
    // again only once a second.
    constexpr int SlotCount = 4;
    bool g_slotConnected[SlotCount] = {};
    std::chrono::steady_clock::time_point g_lastScan{};
    XInputGamepad g_lastPadSeen{};

    // A stick inside its deadzone is a stick at rest. Outside it, the value
    // goes through untouched: the title has its own deadzone and curve for a
    // console pad, and stretching the travel here as well would stack two
    // deadzones and make the slowest walk unreachable. This one is only so a
    // worn stick that never quite centres does not take the keys' and the
    // mouse's place. Radial, so a diagonal is not cut short.
    void ApplyDeadzone(int16_t& x, int16_t& y, double deadzone)
    {
        if (std::hypot(double(x), double(y)) <= deadzone) { x = 0; y = 0; }
    }

    // Reads every connected pad into one. Buttons from any of them, each
    // trigger and stick from whichever is pushed furthest.
    bool ReadPads(XInputGamepad& out)
    {
        out = {};
        if (g_getState == nullptr) return false;

        const auto now = std::chrono::steady_clock::now();
        const bool scan = now - g_lastScan >= std::chrono::seconds(1);
        if (scan) g_lastScan = now;

        const double deadzone = 32767.0 * std::clamp(Settings::Get().stickDeadzone, 0, 50) / 100.0;
        bool any = false;
        double bestLeft = 0.0, bestRight = 0.0;
        for (int slot = 0; slot < SlotCount; slot++)
        {
            if (!g_slotConnected[slot] && !scan) continue;
            XInputState state{};
            const bool connected = g_getState(uint32_t(slot), &state) == ERROR_SUCCESS;
            if (connected != g_slotConnected[slot])
            {
                g_slotConnected[slot] = connected;
                printf("input: controller %s on XInput slot %d\n", connected ? "connected" : "disconnected", slot);
                fflush(stdout);
            }
            if (!connected) continue;
            any = true;

            XInputGamepad pad = state.gamepad;
            ApplyDeadzone(pad.thumbLX, pad.thumbLY, deadzone);
            ApplyDeadzone(pad.thumbRX, pad.thumbRY, deadzone);
            // The trigger's own threshold, as XInput defines it.
            if (pad.leftTrigger < 30) pad.leftTrigger = 0;
            if (pad.rightTrigger < 30) pad.rightTrigger = 0;

            out.buttons |= pad.buttons;
            out.leftTrigger = std::max(out.leftTrigger, pad.leftTrigger);
            out.rightTrigger = std::max(out.rightTrigger, pad.rightTrigger);
            const double left = std::hypot(double(pad.thumbLX), double(pad.thumbLY));
            const double right = std::hypot(double(pad.thumbRX), double(pad.thumbRY));
            if (left > bestLeft) { bestLeft = left; out.thumbLX = pad.thumbLX; out.thumbLY = pad.thumbLY; }
            if (right > bestRight) { bestRight = right; out.thumbRX = pad.thumbRX; out.thumbRY = pad.thumbRY; }
        }
        return any;
    }

    bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }

    // The mouse, held by the window.
    //
    // A first person game steers with the right stick, and a mouse is a
    // relative device, so the two only meet if the pointer is pinned: the
    // cursor goes back to the middle of the window after every read and the
    // distance it travelled becomes the stick's deflection. It is taken by
    // clicking in the window and given back with Escape, because a window that
    // swallows the pointer without asking cannot be left.
    std::atomic<bool> g_mouseHeld{ false };
    std::atomic<bool> g_captureSuspended{ false };
    std::atomic<bool> g_objectivesVisible{false};
    std::atomic<bool> g_mainMenuSeen{false};
    bool g_binocularKey = false;   // the campaign's Binoculars out key held (ReadKeyboard, under Refresh's lock)
    POINT g_centre{};

    void SetHold(bool hold)
    {
        if (hold && g_captureSuspended.load()) return;
        if (g_mouseHeld.load() == hold) return;

        const HWND window = static_cast<HWND>(Window::Handle());
        if (window == nullptr) return;

        if (hold)
        {
            RECT client{};
            GetClientRect(window, &client);
            POINT topLeft{ client.left, client.top };
            POINT bottomRight{ client.right, client.bottom };
            ClientToScreen(window, &topLeft);
            ClientToScreen(window, &bottomRight);

            g_centre.x = (topLeft.x + bottomRight.x) / 2;
            g_centre.y = (topLeft.y + bottomRight.y) / 2;

            RECT screen{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
            ClipCursor(&screen);
            SetCursorPos(g_centre.x, g_centre.y);
            printf("input: the mouse is held by the window and steers; "
                   "Escape gives it back\n");
        }
        else
        {
            ClipCursor(nullptr);
            printf("input: the mouse is free; click in the window to steer "
                   "with it\n");
        }
        fflush(stdout);

        Window::SetCursorHidden(hold);
        g_mouseHeld.store(hold);
    }

    // How far the pointer moved since the last look, in stick units. Full
    // deflection at about twenty five pixels in a sixtieth of a second, a
    // brisk turn rather than a twitch.
    // Direct aim (input.h, TakeLook): the mouse's counts go straight to the
    // title's view angles instead of through the right stick. They pile up
    // here between the title's frames; the look is direct while the title
    // has asked for them lately and nothing has asked for the stick back.
    std::mutex g_lookMutex;
    long g_lookX = 0, g_lookY = 0;
    std::atomic<int64_t> g_lookAskedMs{ -100000 };
    std::atomic<bool> g_lookStick{ false };

    int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    bool LookDirect()
    {
        return Settings::Get().directMouse && !g_lookStick.load(std::memory_order_relaxed) &&
               NowMs() - g_lookAskedMs.load(std::memory_order_relaxed) < 300;
    }

    // The mouse's movement since the last look, in its own counts (or the
    // pointer's pixels without raw input). False while it is not held.
    bool MouseDelta(long& dx, long& dy, bool& raw)
    {
        dx = 0;
        dy = 0;
        // The mouse's own movement is taken whether it is used or not, so
        // that what piled up while the menu was open is not a jump the
        // moment the window takes the mouse back.
        long rawX = 0, rawY = 0;
        raw = Window::TakeRawMouse(rawX, rawY) && Settings::Get().rawMouse;
        if (g_captureSuspended.load() || !g_mouseHeld.load()) return false;
        if (raw)
        {
            dx = rawX;
            dy = rawY;
            // The pointer is still put back in the middle: it is hidden and
            // nothing reads it, but a click has to land in the window and it
            // must never come to rest against an edge of the screen.
            POINT now{};
            if (GetCursorPos(&now) && (now.x != g_centre.x || now.y != g_centre.y))
                SetCursorPos(g_centre.x, g_centre.y);
        }
        else
        {
            POINT now{};
            if (!GetCursorPos(&now)) return true;
            dx = now.x - g_centre.x;
            dy = now.y - g_centre.y;
            if (dx != 0 || dy != 0) SetCursorPos(g_centre.x, g_centre.y);
        }
        return true;
    }

    void ReadMouse(int16_t& outX, int16_t& outY)
    {
        outX = 0;
        outY = 0;

        long dx = 0, dy = 0;
        bool raw = false;
        if (LookDirect())
        {
            std::lock_guard<std::mutex> lock(g_lookMutex);
            if (MouseDelta(dx, dy, raw)) { g_lookX += dx; g_lookY += dy; }
            return;
        }
        if (!MouseDelta(dx, dy, raw)) return;


        // COD3_MOUSELOG=1: what the look took, once a second, to see that
        // the mouse's own counts arrive and how many a turn is worth.
        static const bool log = getenv("COD3_MOUSELOG") != nullptr;
        if (log && (dx != 0 || dy != 0))
        {
            static uint64_t second = 0, counts = 0, reads = 0;
            counts += uint64_t(std::abs(dx) + std::abs(dy));
            reads++;
            const uint64_t now = GetTickCount64() / 1000;
            if (now != second)
            {
                second = now;
                printf("input: %s, %llu counts over %llu reads\n", raw ? "the mouse's own movement" : "the pointer's travel",
                    (unsigned long long)counts, (unsigned long long)reads);
                fflush(stdout);
                counts = 0; reads = 0;
            }
        }

        // A stick's deflection is a speed: the title turns by it times its
        // frame's time, and reads the pad once a frame (twice, the second
        // read a moment after the first). The distance the mouse went since
        // the read before is a speed only at one frame rate, so taken as it
        // is, the turn would slow down as the frame rate went up (at 100 a
        // second to 60 per cent of the turn at 60). It is scaled to the
        // distance over a sixtieth of a second, the rate the gain was set
        // at, so a sweep of the hand is the same turn at any frame rate.
        // The read that comes straight after another is left as it is: what
        // it carries is the crumb in between.
        double scale = 1.0;
        {
            static auto lastRead = std::chrono::steady_clock::now();
            const auto now = std::chrono::steady_clock::now();
            const double interval = std::chrono::duration<double>(now - lastRead).count();
            lastRead = now;
            if (interval >= 0.002) scale = (1.0 / 60.0) / std::min(interval, 0.05);
        }

        // The settings menu's sensitivity scales the gain, both axes alike.
        const double gain = 1300.0 * Settings::Get().mouseSensitivity * scale;
        const long x = long(std::clamp(double(dx) * gain, -32767.0, 32767.0));
        // Screen coordinates grow downwards and a stick grows upwards.
        const long y = long(std::clamp(-double(dy) * gain, -32767.0, 32767.0));
        outX = int16_t(x);
        outY = int16_t(y);
    }

    // One step of the menu highlight for each stretch of mouse movement.
    //
    // A menu reads the pad's cross as presses, not as a position, so the
    // movement is turned into presses: travel builds up, and each time enough
    // of it has gone by in one direction the step is reported once and the
    // total is cleared. The gap keeps a single flick from running down the
    // whole list, and the press is held for two reads so the title cannot miss
    // it between polls.
    uint16_t MenuStep()
    {
        static POINT last{};
        static bool have = false;
        static long travel = 0;
        static uint16_t holding = 0;
        static int holdReads = 0;

        POINT now{};
        if (!GetCursorPos(&now)) return 0;

        if (!have) { last = now; have = true; return 0; }
        const long dy = now.y - last.y;
        last = now;

        if (holdReads > 0)
        {
            holdReads--;
            return holding;
        }
        holding = 0;

        // Movement the other way cancels what had built up, so a wobble does
        // not eventually add up to a step.
        if ((dy > 0) != (travel > 0)) travel = 0;
        travel += dy;

        constexpr long Step = 28;   // pixels of travel to one press
        if (travel >= Step)  { travel = 0; holding = PadDown; holdReads = 2; }
        if (travel <= -Step) { travel = 0; holding = PadUp;   holdReads = 2; }
        return holding;
    }

    // Presses from a script, for driving the title without anyone at the
    // keyboard: COD3_PAD="5:start 8:a 9:down 10:a" presses each button at
    // that many seconds from the start. Names are start back a b x y up
    // down left right ls rs; a plus joins several ("10:down+a"), and the
    // seconds can have a decimal. "40:lt/6" and "40:rt/6" hold the left
    // or right trigger for that many seconds (aiming, firing), and
    // "40:ly+/5" or "40:rx-50/2" hold a stick (lx ly rx ry, + or -, an
    // optional per cent): walking and looking round without a hand on it.
    //
    // A press lasts two of the title's own reads of the pad and no longer.
    // It used to last a quarter of a second, and a menu that opened while
    // the button was still down took it as pressed again: one Start on the
    // title screen fell through the main menu, the single player menu and
    // a dialogue and started a game, or landed anywhere depending on how
    // fast the screens came.
    struct ScriptedPress { double at; uint16_t buttons; int readsLeft; bool begun; bool keyboardSpace=false; };
    std::vector<ScriptedPress> g_script;
    // A held trigger or stick: which (0 and 1 the left and right trigger,
    // 2 to 5 the left stick's x and y and the right stick's x and y) and how
    // far, as a fraction of full, negative for left and down.
    struct ScriptedHold { double at, until; int control; float amount; };
    std::vector<ScriptedHold> g_holds;
    bool g_scriptParsed = false;
    std::chrono::steady_clock::time_point g_scriptStart;

    uint16_t ButtonNamed(const std::string& name)
    {
        static const std::pair<const char*, uint16_t> names[] = {
            { "start", PadStart }, { "back", PadBack }, { "a", PadA }, { "b", PadB },
            { "x", PadX }, { "y", PadY }, { "up", PadUp }, { "down", PadDown },
            { "left", PadLeft }, { "right", PadRight },
            { "ls", PadLeftShoulder }, { "rs", PadRightShoulder }, { "l3", PadLeftThumb }, { "r3", PadRightThumb },
        };
        for (const auto& entry : names) if (name == entry.first) return entry.second;
        return 0;
    }

    // 0.5.231: Input::PressButton's press, from the game's thread.
    std::atomic<uint16_t> g_pressButtons{ 0 };
    std::atomic<int> g_pressReads{ 0 };

    uint16_t ScriptedButtons()
    {
        if (g_pressReads.load() > 0) { g_pressReads.fetch_sub(1); return g_pressButtons.load(); }
        if (!g_scriptParsed)
        {
            g_scriptParsed = true;
            g_scriptStart = std::chrono::steady_clock::now();
            if (const char* text = getenv("COD3_PAD"))
            {
                std::string all(text);
                size_t at = 0;
                while (at < all.size())
                {
                    const size_t end = all.find(' ', at);
                    const std::string item = all.substr(at, end == std::string::npos ? std::string::npos : end - at);
                    at = end == std::string::npos ? all.size() : end + 1;
                    const size_t colon = item.find(':');
                    if (colon == std::string::npos) continue;
                    ScriptedPress press{ atof(item.substr(0, colon).c_str()), 0, 2, false };
                    std::string rest = item.substr(colon + 1);
                    // Replay the same conversion as a physical Space press.
                    if(rest=="space") {press.keyboardSpace=true;g_script.push_back(press);continue;}
                    if (rest.compare(0,5,"back/")==0) {
                        g_holds.push_back({press.at,press.at+atof(rest.c_str()+5),6,1.0f});continue;
                    }
                    if (rest.compare(0, 3, "lt/") == 0 || rest.compare(0, 3, "rt/") == 0)
                    {
                        g_holds.push_back({ press.at, press.at + atof(rest.c_str() + 3), rest[0] == 'l' ? 0 : 1, 1.0f });
                        continue;
                    }
                    // A stick: "rx+/3" holds the right stick fully right for
                    // three seconds, "ly+60/2" the left stick six tenths up
                    // for two (walking forward). lx ly rx ry, + or -, an
                    // optional per cent, then the seconds.
                    if (rest.size() >= 4 && (rest[0] == 'l' || rest[0] == 'r') && (rest[1] == 'x' || rest[1] == 'y') &&
                        (rest[2] == '+' || rest[2] == '-'))
                    {
                        const size_t slash = rest.find('/');
                        if (slash == std::string::npos) continue;
                        const std::string percent = rest.substr(3, slash - 3);
                        float amount = percent.empty() ? 1.0f : float(atof(percent.c_str())) / 100.0f;
                        if (rest[2] == '-') amount = -amount;
                        const int control = 2 + (rest[0] == 'r' ? 2 : 0) + (rest[1] == 'y' ? 1 : 0);
                        g_holds.push_back({ press.at, press.at + atof(rest.c_str() + slash + 1), control, amount });
                        continue;
                    }
                    size_t from = 0;
                    while (from <= rest.size())
                    {
                        const size_t plus = rest.find('+', from);
                        press.buttons |= ButtonNamed(rest.substr(from, plus == std::string::npos ? std::string::npos : plus - from));
                        if (plus == std::string::npos) break;
                        from = plus + 1;
                    }
                    g_script.push_back(press);
                }
                printf("input: %zu scripted presses and %zu holds from COD3_PAD\n", g_script.size(), g_holds.size());
            }
        }
        if (g_script.empty()) return 0;
        const double seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - g_scriptStart).count();
        uint16_t buttons = 0;
        for (ScriptedPress& press : g_script)
        {
            if (seconds < press.at || press.readsLeft <= 0) continue;
            if (!press.begun)
            {
                press.begun = true;
#ifndef COD3_MP
                if(press.keyboardSpace) {
                    press.buttons=MenuInput::Apply(0,!g_mainMenuSeen.load(),Kernel::Stats().filesOpened.load()>=40,true,false);
                    printf("input: replayed Space -> 0x%04X (main menu seen %d)\n",press.buttons,int(g_mainMenuSeen.load()));fflush(stdout);
                }
#endif
                // A press is where something is about to happen, so the
                // kernel trace, if one was asked for, starts here.
                Kernel::StartKernelTrace();
            }
            buttons |= press.buttons;
        }
        return buttons;
    }

    // The triggers and sticks a script holds now.
    void ScriptedHolds(Input::Pad& pad)
    {
        if (g_holds.empty()) return;
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - g_scriptStart).count();
        for (const ScriptedHold& hold : g_holds)
        {
            if (seconds < hold.at || seconds >= hold.until) continue;
            const int16_t stick = int16_t(std::clamp(hold.amount, -1.0f, 1.0f) * 32767.0f);
            switch (hold.control)
            {
            case 0: pad.leftTrigger = 255; break;
            case 1: pad.rightTrigger = 255; break;
            case 2: pad.thumbLX = stick; break;
            case 3: pad.thumbLY = stick; break;
            case 4: pad.thumbRX = stick; break;
            case 5: pad.thumbRY = stick; break;
            case 6: pad.buttons |= PadBack; break;
            }
        }
    }

    // The title has read the pad: the presses it saw count down.
    void ScriptedRead(uint16_t buttons)
    {
        for (ScriptedPress& press : g_script)
            if (press.begun && press.readsLeft > 0 && (buttons & press.buttons) == press.buttons)
                press.readsLeft--;
    }

    // The keyboard and mouse stand in for a pad. Only while the title's own
    // window is in front, so typing anywhere else is not taken as input.
    Input::Pad ReadKeyboard()
    {
        Input::Pad pad{};
        g_binocularKey = false;
        // COD3_NOKEYBOARD=1 ignores the keyboard and mouse entirely, for
        // runs driven by a script while someone is using the machine: the
        // key state is global, and typing anywhere walked the title's menus.
        static const bool ignored = []() {
            const char* text = getenv("COD3_NOKEYBOARD");
            if (text == nullptr) text = getenv("COD3_BACKGROUND");   // a run nobody is playing
            return text != nullptr && text[0] != 0 && text[0] != '0';
        }();
        if (ignored) return pad;
#ifndef COD3_MP
        // Dedicated keyboard commands keep Reload separate from Use and avoid
        // mapping objectives to an unbound direction on the console's D-pad.
        static KeyboardActions::State actions;
        const auto bindings=Settings::Get();
        const bool enabled=Window::HasFocus() && !Overlay::IsOpen() && !g_captureSuspended.load();
        const auto boundDown=[&](Settings::Action action) { return enabled && bindings.keys[action]!=0 && Down(bindings.keys[action]); };
        const std::string commands=KeyboardActions::Update(actions,boundDown(Settings::Reload),boundDown(Settings::Objectives),
            boundDown(Settings::BinocularsOut),boundDown(Settings::BinocularsAway),enabled);
        g_objectivesVisible.store(actions.objectives,std::memory_order_relaxed);
        if(!commands.empty()) Kernel::QueueConsoleCommand(commands);
#endif
        if (!Window::HasFocus() || Overlay::IsOpen() || g_captureSuspended.load())
        {
            // Out of focus, or the settings menu is up and the keys and the
            // mouse are its.
            if (g_mouseHeld.load()) SetHold(false);
            return pad;
        }

        // Taking and giving back the pointer.
        //
        // In a level the window takes the mouse by itself, the moment it is in
        // front, and a click in it takes the mouse back after Escape: looking
        // around is what a mouse is for in a shooter, and asking for a key
        // first was asking every player to read the console. In the menus the
        // left button used to take it, which made the mouse useless for the
        // one thing a menu wants it for: clicking; F1 and the middle button
        // take it there. Escape always gives it back, and in a level it is
        // also Start, so the pause menu opens with the pointer free to leave.
        const bool inLevel = Kernel::Stats().filesOpened.load(std::memory_order_relaxed) >= 40;
        const bool escapeDown = Down(VK_ESCAPE);
        // Start is held for a few reads after Escape frees the pointer, so
        // the title's own poll, which is slower than these reads, sees it.
        static int startReadsLeft = 0;
        if (g_mouseHeld.load() && escapeDown) { SetHold(false); startReadsLeft = 10; }
        else if (!g_mouseHeld.load())
        {
            static bool takenForLevel = false;
            if (!inLevel) takenForLevel = false;
            const bool firstTime = inLevel && !takenForLevel;
            POINT pointer{}; RECT client{};
            const HWND captureWindow=static_cast<HWND>(Window::Handle());
            const bool inClient=captureWindow && GetCursorPos(&pointer) &&
                ScreenToClient(captureWindow,&pointer) && GetClientRect(captureWindow,&client) && PtInRect(&client,pointer);
            if (Down(VK_F1) || (inClient && Down(VK_MBUTTON)) || firstTime || (inLevel && inClient && Down(VK_LBUTTON) && !escapeDown))
            {
                SetHold(true);
                if (inLevel) takenForLevel = true;
            }
        }

        const bool held = g_mouseHeld.load();
#ifndef COD3_MP
        // The game owns the held/keyup state of +scores. Emulate Back for the
        // entire toggle interval rather than injecting competing commands.
        if (g_objectivesVisible.load(std::memory_order_relaxed)) pad.buttons |= PadBack;
#endif

        // The keys, where the console's buttons are on a keyboard, from the
        // settings menu: jump is A, crouch B, use and reload X, weapon Y,
        // melee the right stick pressed, sprint the left, the grenade the
        // right bumper and the smoke the left, objectives left on the cross,
        // pause Start. The arrows and Enter always walk the menus.
        const Settings::Values keys = Settings::Get();
        struct Mapping { int key; uint16_t button; };
        const Mapping mappings[] = {
            #ifdef COD3_MP
            { VK_RETURN, PadStart },
#endif
            { VK_UP, PadUp }, { VK_DOWN, PadDown },
            { VK_LEFT, PadLeft }, { VK_RIGHT, PadRight },
            { keys.keys[Settings::Jump], PadA },
            { keys.keys[Settings::Crouch], PadB },
            { keys.keys[Settings::Use], PadX },
#ifdef COD3_MP
            { keys.keys[Settings::Reload], PadX },
#endif
            { keys.keys[Settings::Weapon], PadY },
            { keys.keys[Settings::Melee], PadRightThumb },
            { keys.keys[Settings::Sprint], PadLeftThumb },
            { keys.keys[Settings::Grenade], PadRightShoulder },
            { keys.keys[Settings::Smoke], PadLeftShoulder },
            #ifdef COD3_MP
            { keys.keys[Settings::Objectives], PadLeft },
#endif
#ifdef COD3_MP
            { keys.keys[Settings::BinocularsOut], PadUp },
#endif
            { keys.keys[Settings::BinocularsAway], PadDown },
        };
#ifndef COD3_MP
        // The campaign's binoculars are the left stick held in, standing
        // (Refresh, after the sprint): the Binoculars out key held is that.
        g_binocularKey = keys.keys[Settings::BinocularsOut] != 0 && Down(keys.keys[Settings::BinocularsOut]);
#endif

        for (const Mapping& mapping : mappings)
            if (mapping.key != 0 && Down(mapping.key)) pad.buttons |= mapping.button;
        // Pause, when it is not the Escape that frees the mouse (below).
        if (inLevel && keys.keys[Settings::Pause] != 0 && keys.keys[Settings::Pause] != VK_ESCAPE && Down(keys.keys[Settings::Pause]))
            pad.buttons |= PadStart;

        // The wheel changes weapon, either way: the console has one button
        // for it, which cycles, so up and down both press it, held for a
        // few reads so the title's own poll sees the press.
        {
            static int wheelReadsLeft = 0;
            const int wheel = Window::TakeWheel();
            if (wheel != 0) wheelReadsLeft = 8;
            if (wheelReadsLeft > 0) { pad.buttons |= PadY; wheelReadsLeft--; }
        }

        // The title's first screen waits for Start, and asking a player to know
        // that is asking them to guess. While the mouse is free, which is every
        // menu, Space is Start as well as A.
        // Not in the multiplayer program: its menus take a press of A and
        // Start together as neither, and it has no press start screen.
#ifndef COD3_MP
        pad.buttons=MenuInput::Apply(pad.buttons,!g_mainMenuSeen.load(),inLevel,Down(VK_SPACE),Down(VK_RETURN));
#endif
#ifdef COD3_MP
        if (!held && Down(VK_ESCAPE) && !inLevel) pad.buttons |= PadBack;
#else
        // Front-end cancel is controller B; Back is the objectives button.
        if (!held && Down(VK_ESCAPE) && !inLevel) pad.buttons |= PadB;
#endif
        if (inLevel && startReadsLeft > 0) { pad.buttons |= PadStart; startReadsLeft--; }

        // Driving a menu with the mouse.
        //
        // The menus are built for a pad: a highlight that moves up and down and
        // a button that takes it. There is no way to ask the title what is
        // under the pointer, so pointing at an entry cannot select it. What can
        // be done is to make the mouse do what the pad does. Moving it up or
        // down steps the highlight, one step per movement rather than one per
        // pixel, and the left button takes the entry. It is not pointing and
        // clicking, but it is a menu driven with the mouse.
        if (!held && !inLevel)
        {
            if (Down(VK_LBUTTON)) pad.buttons |= PadA;
            if (Down(VK_RBUTTON)) pad.buttons |= PadBack;
            // Page Up and Page Down are the bumpers, which turn a menu's
            // pages (the System Link custom rules, lobby_rules.cpp).
            if (Down(VK_PRIOR)) pad.buttons |= PadLeftShoulder;
            if (Down(VK_NEXT)) pad.buttons |= PadRightShoulder;
            pad.buttons |= MenuStep();
        }

        // Moving. WASD is the left stick so it is analogue, and the arrows stay
        // on the pad's cross so menus that only read that still work.
        int16_t x = 0, y = 0;
        if (keys.keys[Settings::Left] && Down(keys.keys[Settings::Left])) x -= StickFull;
        if (keys.keys[Settings::Right] && Down(keys.keys[Settings::Right])) x += StickFull;
        if (keys.keys[Settings::Backward] && Down(keys.keys[Settings::Backward])) y -= StickFull;
        if (keys.keys[Settings::Forward] && Down(keys.keys[Settings::Forward])) y += StickFull;
        pad.thumbLX = x;
        pad.thumbLY = y;

        ReadMouse(pad.thumbRX, pad.thumbRY);

        // Firing and aiming, where a shooter expects them: while the mouse
        // is held, or for keys that are not mouse buttons at any time.
        const int fire = keys.keys[Settings::Fire], aim = keys.keys[Settings::Aim];
        const auto mouseButton = [](int key) { return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON || key == VK_XBUTTON1 || key == VK_XBUTTON2; };
        if (fire != 0 && (held || !mouseButton(fire)) && Down(fire)) pad.rightTrigger = 255;
        if (aim != 0 && (held || !mouseButton(aim)) && Down(aim)) pad.leftTrigger = 255;

        return pad;
    }

    std::atomic<bool> g_binoculars{false}, g_binocularFire{false}, g_sprinting{false}, g_sprintHeld{false};
    std::atomic<int> g_aimPresses{0}, g_startPresses{0}, g_upPresses{0}, g_downPresses{0}, g_rightPresses{0};
    bool g_aimWasDown = false;
    std::atomic<bool> g_mortar{false};   // 0.5.285
    std::atomic<int> g_mortarFires{0};
    std::atomic<bool> g_mortarPassFire{false};   // 0.5.300
    bool g_mortarFireWas = false;
    std::atomic<int> g_crouchTap{0};   // 0.5.307: reads left of a crouch tap
    std::mutex g_mutex;
    Input::Pad g_pad{};
    uint32_t g_packet = 1;
    std::chrono::steady_clock::time_point g_lastRead;

    bool Same(const Input::Pad& a, const Input::Pad& b)
    {
        return a.buttons == b.buttons &&
               a.leftTrigger == b.leftTrigger && a.rightTrigger == b.rightTrigger &&
               a.thumbLX == b.thumbLX && a.thumbLY == b.thumbLY &&
               a.thumbRX == b.thumbRX && a.thumbRY == b.thumbRY;
    }

    // Asking Windows on every call would cost more than the title's own frame.
    // Once every four milliseconds is finer than any frame it draws.
    void Refresh()
    {
        const auto now = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> lock(g_mutex);
        if (now - g_lastRead < std::chrono::milliseconds(4) && g_script.empty() && g_holds.empty()) return;
        g_lastRead = now;

        FindXInput();

        // A pad and the keyboard together, not one or the other. A pad that is
        // plugged in and untouched used to silence the keyboard completely,
        // which looks exactly like input not working at all.
        Input::Pad pad = ReadKeyboard();
        const uint16_t fromKeyboard = pad.buttons;
        const uint16_t fromScript = ScriptedButtons();
        uint16_t fromPad = 0;
        pad.buttons |= fromScript;
        ScriptedHolds(pad);
        // COD3_NOPAD=1 ignores a physical pad the same way, for the same
        // reason: one lying on the desk with a button pressed walked the
        // title's menus in place of the script.
        static const bool padIgnored = []() {
            const char* text = getenv("COD3_NOPAD");
            return text != nullptr && text[0] != 0 && text[0] != '0';
        }();
        if (!padIgnored && Settings::Get().controller)
        {
            XInputGamepad state{};
            if (ReadPads(state))
            {
                if (state.buttons != g_lastPadSeen.buttons ||
                    (state.leftTrigger > 30 && std::abs(int(state.leftTrigger)-int(g_lastPadSeen.leftTrigger)) > 8) ||
                    (state.rightTrigger > 30 && std::abs(int(state.rightTrigger)-int(g_lastPadSeen.rightTrigger)) > 8) ||
                    (std::abs(int(state.thumbLX)) > 12000 && std::abs(int(state.thumbLX)-int(g_lastPadSeen.thumbLX)) > 2048) ||
                    (std::abs(int(state.thumbLY)) > 12000 && std::abs(int(state.thumbLY)-int(g_lastPadSeen.thumbLY)) > 2048) ||
                    (std::abs(int(state.thumbRX)) > 12000 && std::abs(int(state.thumbRX)-int(g_lastPadSeen.thumbRX)) > 2048) ||
                    (std::abs(int(state.thumbRY)) > 12000 && std::abs(int(state.thumbRY)-int(g_lastPadSeen.thumbRY)) > 2048))
                    ButtonPrompts::ControllerUsed();
                g_lastPadSeen = state;
                fromPad = state.buttons;
                pad.buttons |= state.buttons;
                pad.leftTrigger = std::max(pad.leftTrigger, state.leftTrigger);
                pad.rightTrigger = std::max(pad.rightTrigger, state.rightTrigger);
                // A stick pushed past its deadzone wins over the keys and
                // the mouse; one at rest leaves them theirs.
                if (state.thumbLX != 0 || state.thumbLY != 0)
                {
                    pad.thumbLX = state.thumbLX;
                    pad.thumbLY = state.thumbLY;
                }
                if (state.thumbRX != 0 || state.thumbRY != 0)
                {
                    pad.thumbRX = state.thumbRX;
                    pad.thumbRY = state.thumbRY;
                }
            }
            else g_lastPadSeen = {};
        }

#ifdef COD3_MP
        // Use real held buttons before SprintInput removes stationary L3.
        // Moving/aiming leaves both original controls available.
        {
            const bool stationary=std::abs(int(pad.thumbLX))<8000 && std::abs(int(pad.thumbLY))<8000;
            const bool ready=stationary && pad.leftTrigger<=30;
            const bool save=ready && (fromPad&PadLeftThumb)!=0;
            // 0.5.181: Justin: the return is held X on the controller now
            // (was R3), from anywhere, moving or not. X still reaches the
            // game (reload/use on the press); only a 2-second hold returns.
            const bool back=(fromPad&PadX)!=0;
            g_tacticalPointSaveHeld.store(save);
            g_tacticalPointReturnHeld.store(back);
            if(g_tacticalPointActive.load()) {
                if(save) pad.buttons&=uint16_t(~PadLeftThumb);
            }
        }
#endif

        // Tap to sprint (a PC addition, as later games of the series have it).
        // The title runs only while the left stick is held in, so a press
        // of sprint, from the pad or the keyboard, keeps it held for the
        // title until the run would end anyway: the stick or the keys no
        // longer forward, aiming down the sights or firing, or sprint
        // pressed again.
        static SprintInput::State sprint;
        {
#ifdef COD3_MP
            const bool scopedBreath=g_scopedBreath.load();
            const bool standing=false;
#else
            const bool scopedBreath=false; // campaign uses separate binocular/breath bindings
            const bool standing=true;      // L3 held standing: the campaign's binoculars
#endif
            pad.buttons=SprintInput::Filter(sprint,pad.buttons,pad.thumbLY,pad.leftTrigger,
                                            pad.rightTrigger,Settings::Get().toggleSprint,scopedBreath,standing);
        }
        g_sprintHeld.store((pad.buttons & PadLeftThumb)!=0 && pad.thumbLY>8000 && pad.leftTrigger<=30);
#ifndef COD3_MP
        // The Binoculars out key: the left stick held in, while not pushing
        // forward (forward, the title would take it for a run).
        // Keyboard binoculars use the latched command above; L3 remains the physical pad control.
#endif
        // Running, for the campaign's binoculars (movement_fixes.cpp): a run
        // latched by tap to sprint, the stick pushed well forward, or L3
        // held with the stick past half way forward (a run is only ever
        // forward). L3 held standing, walking slowly, strafing or backing
        // off is not one: that is the binoculars themselves. (A third of the
        // way, 8000 up, counted as a run, and the click itself can push the
        // stick that far: a player holding it in to raise them, standing,
        // was taken for a runner and they never came up.) Forward walking
        // while ADS remains walking, not a sprint report.
        g_sprinting.store(pad.leftTrigger<=30 && (sprint.running ||
                          ((pad.buttons & PadLeftThumb)!=0 && pad.thumbLY>=SprintInput::RunStart)));
        // Presses of Start and the d-pad's up and down, counted as they
        // happen here (every few milliseconds): the game's frame, where the
        // binoculars look (mp_fixes.cpp), comes only twenty times a second,
        // and Escape's Start lasts a few reads, so it saw neither for sure.
        {
            static uint16_t was = 0;
            const uint16_t now = pad.buttons;
            WeaponCycleInput::Observe(g_weaponCycle,now);
#ifdef COD3_MP
            // Observe the shared input for the reported Y / wheel failure.
            if ((now & PadY) && !(was & PadY))
            {
                printf("input: weapon-cycle Y press (%s)\n", (fromPad & PadY) ? "controller" : "keyboard/wheel");
                fflush(stdout);
            }
#endif
            if ((now & PadStart) && !(was & PadStart)) g_startPresses.fetch_add(1);
            if ((now & PadUp) && !(was & PadUp)) g_upPresses.fetch_add(1);
            if ((now & PadDown) && !(was & PadDown)) g_downPresses.fetch_add(1);
            if ((now & PadRight) && !(was & PadRight)) g_rightPresses.fetch_add(1);
            was = now;
        }
        // Binoculars out (mp_fixes.cpp): always at the eyes (aim held for
        // the title), the left trigger's presses kept for the zoom instead,
        // and the right trigger passed on only when it may fire.
        if (g_binoculars.load())
        {
            const bool aim = pad.leftTrigger > 30;
            if (aim && !g_aimWasDown) g_aimPresses.fetch_add(1);
            g_aimWasDown = aim;
            pad.leftTrigger = 255;
            if (!g_binocularFire.load()) pad.rightTrigger = 0;
        }
        else g_aimWasDown = false;
        // 0.5.285: at the mortar: held in place, the right trigger's presses
        // the mortar's (mp_fixes.cpp), the view still turned by the right
        // stick and the mouse.
        if (g_mortar.load())
        {
            const bool fire = pad.rightTrigger > 60;
            if (fire && !g_mortarFireWas) g_mortarFires.fetch_add(1);
            g_mortarFireWas = fire;
            if (!g_mortarPassFire.load()) pad.rightTrigger = 0;
            pad.leftTrigger = 0;
            pad.thumbLX = 0;
            pad.thumbLY = 0;
            pad.buttons &= uint16_t(~(PadA | PadB | PadX | PadY | PadLeftThumb | PadRightThumb | PadLeftShoulder | PadRightShoulder));
        }
        else g_mortarFireWas = false;
        // 0.5.307: a crouch tap asked for by the mortar (down at it, up after).
        if (g_crouchTap.load() > 0) { pad.buttons |= PadB; g_crouchTap.fetch_sub(1); }

        if (!Same(pad, g_pad))
        {
            // Where a button came from, the first few times it changes:
            // the title sometimes walks its own menus with no one pressing
            // anything, and this says whether anything was pressed at all.
            static int announced = 0;
            if (pad.buttons != g_pad.buttons && (announced < 40 && announced++ < 40))
            {
                printf("input: buttons 0x%04X (keyboard 0x%04X, script 0x%04X, pad 0x%04X, focus %d)\n",
                    pad.buttons, fromKeyboard, fromScript, fromPad, Window::HasFocus() ? 1 : 0);
                fflush(stdout);
            }
            // The pad's own changes, separately, so a run where the keyboard
            // used up the forty lines above still says whether the pad was
            // ever seen pressing anything.
            static uint16_t lastFromPad = 0;
            static int padAnnounced = 0;
            if (fromPad != lastFromPad && (padAnnounced < 40 && padAnnounced++ < 40))
            {
                printf("input: pad buttons 0x%04X, left stick %d,%d\n", fromPad, pad.thumbLX, pad.thumbLY);
                fflush(stdout);
            }
            lastFromPad = fromPad;
            g_pad = pad;
            g_packet++;
        }
    }
}

Input::Pad Input::State()
{
    Refresh();
    std::lock_guard<std::mutex> lock(g_mutex);
    ScriptedRead(g_pad.buttons);
    return g_pad;
}

uint16_t Input::Buttons()
{
    return State().buttons;
}

void Input::SetBinoculars(bool out, bool mayFire) { g_binoculars.store(out); g_binocularFire.store(mayFire); }
int Input::TakeAimPresses() { Refresh(); return g_aimPresses.exchange(0); }
void Input::TakePresses(int& start, int& up, int& down)
{
    Refresh();
    start = g_startPresses.exchange(0);
    up = g_upPresses.exchange(0);
    down = g_downPresses.exchange(0);
}
int Input::TakeRightPresses() { Refresh(); return g_rightPresses.exchange(0); }
void Input::SetMortar(bool on) { g_mortar.store(on); if (!on) { g_mortarFires.store(0); g_mortarPassFire.store(false); } }
void Input::SetMortarPassFire(bool pass) { g_mortarPassFire.store(pass); }
void Input::PressCrouch() { g_crouchTap.store(3); }
int Input::TakeMortarFires() { Refresh(); return g_mortarFires.exchange(0); }
bool Input::Running() { Refresh(); return g_sprinting.load(); }
bool Input::SprintHeld() { Refresh(); return g_sprintHeld.load(); }

bool Input::MouseHeld()
{
    return g_mouseHeld.load();
}

void Input::SuspendMouseCapture(bool suspended)
{
    g_captureSuspended.store(suspended);
    if (suspended) SetHold(false);
}

uint32_t Input::PacketNumber()
{
    Refresh();
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_packet;
}

const char* Input::KeyboardHelp()
{
    return "Enter or Space start, mouse moves the menu and clicks to choose, "
           "F1 steers with it, Escape frees it";
}

std::string Input::ControllerStatus()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    FindXInput();
    if (g_getState == nullptr) return "no XInput on this machine";
    std::string slots;
    for (int slot = 0; slot < SlotCount; slot++)
        if (g_slotConnected[slot]) slots += (slots.empty() ? "" : ", ") + std::to_string(slot);
    if (slots.empty()) return "no controller found; plug one in or check it is in XInput mode";
    char text[160];
    snprintf(text, sizeof(text), "connected on slot %s; buttons 0x%04X, left stick %d,%d",
        slots.c_str(), g_lastPadSeen.buttons, g_lastPadSeen.thumbLX, g_lastPadSeen.thumbLY);
    return text;
}

void Input::SetRumble(uint16_t left, uint16_t right)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    FindXInput();
    if (g_setState == nullptr) return;
    const Settings::Values values = Settings::Get();
    if (!values.controller || !values.rumble) { left = 0; right = 0; }
    // The title sends the same strength over and over; only a change is
    // passed on.
    static uint16_t lastLeft = 0xFFFF, lastRight = 0xFFFF;
    if (left == lastLeft && right == lastRight) return;
    lastLeft = left; lastRight = right;
    XInputVibration vibration{ left, right };
    for (int slot = 0; slot < SlotCount; slot++)
        if (g_slotConnected[slot]) g_setState(uint32_t(slot), &vibration);
}

bool Input::TakeLook(long& dx, long& dy)
{
    g_lookAskedMs.store(NowMs(), std::memory_order_relaxed);
    g_lookStick.store(false, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_lookMutex);
    if (!Settings::Get().directMouse) { g_lookX = g_lookY = 0; dx = dy = 0; return false; }
    // What moved since the title's last poll of the pad, so the turn is
    // as fresh as this frame.
    long moreX = 0, moreY = 0;
    bool raw = false;
    if (MouseDelta(moreX, moreY, raw)) { g_lookX += moreX; g_lookY += moreY; }
    dx = g_lookX;
    dy = g_lookY;
    g_lookX = g_lookY = 0;
    return true;
}

void Input::LookByStick()
{
    g_lookStick.store(true, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_lookMutex);
    g_lookX = g_lookY = 0;
}

void Input::SetScopedBreath(bool enabled)
{
    g_scopedBreath.store(enabled);
    printf("input: scoped L3 native hold-breath %s\n",enabled?"on":"off");
    fflush(stdout);
}

unsigned Input::TakeWeaponCyclePresses()
{
    Refresh();
    std::lock_guard<std::mutex> lock(g_mutex);
    return WeaponCycleInput::Drain(g_weaponCycle);
}
void Input::SetWeaponCycleEnabled(bool enabled) { g_weaponCycleEnabled.store(enabled); }
uint16_t Input::NativeButtons(uint16_t buttons,bool paused)
{
    return WeaponCycleInput::Report(buttons,g_weaponCycleEnabled.load(),paused);
}

void Input::SetTacticalPointInput(bool active,bool canReturn) {g_tacticalPointActive.store(active);g_tacticalPointCanReturn.store(canReturn);}
bool Input::TacticalPointSaveHeld() {Refresh();return g_tacticalPointSaveHeld.load();}
bool Input::TacticalPointReturnHeld() {Refresh();return g_tacticalPointReturnHeld.load();}

void Input::PressButton(const char* name)
{
    const uint16_t buttons = ButtonNamed(name);
    if (!buttons) return;
    g_pressButtons.store(buttons);
    g_pressReads.store(2);
}

#ifndef COD3_MP
bool Input::ObjectivesVisible() { return g_objectivesVisible.load(std::memory_order_relaxed); }
#endif

void Input::MainMenuSeen() { g_mainMenuSeen.store(true); }
