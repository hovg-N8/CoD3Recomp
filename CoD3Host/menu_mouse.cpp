// Pointing at the title's own menus with the mouse (a PC addition, the
// campaign). 0.5.595: ported from FrankHUN88453's CoD3Recomp (ac3b2a52,
// menu_mouse.cpp), without his VIDEO page, his Quit entry and his test
// scripts; the picture's place from this tree's own fill-the-screen.
//
// The menus are the title's C++ front end, built for a pad: a menu system
// (FEMenuSystem: the front end's and the in-game one, their pointers at
// 0x82A8DF2C +28 and +32) keeps its menus (+4, +24 of them) and which is up
// (+12, -1 for none); a menu (FEMenu) keeps its entries (+4, +38 of them),
// the highlighted one (+34) and where it lays them out: from x, y (+12,
// +16) one row (+20) apart, less a centring offset (+24), from the first
// shown (+32). Its SetSelection (vtable +164, sub_824CE558) moves the
// highlight as the cross does. An entry (FEMenuEntry) holds the widget it
// shows (+16) and whether it is hidden (+22).
//
// Each frame the menu system updates (sub_824EE2E0, on the title's thread,
// for both systems), the entry under the pointer is found and highlighted
// through SetSelection, with the sound the cross makes; the left button is
// then A over an entry, the cross's left or right over an arrow, an option's
// value or a slider (input.cpp). COD3_MENULOG=1 prints the menu and the
// pointer every second; COD3_NOMENUMOUSE=1 turns it off.
//
// 0.5.602: the multiplayer program too. Its front end is the same classes,
// built again with other virtual slots, so no function of it is the
// campaign's twin and they were found from the images (COD3_IMAGEDUMP) by
// their vtables: the menus (60 classes) keep the move sound at +28 and
// their SetSelection at +192 (sub_824E9170, 0.90 of the campaign's code;
// its cross handler sub_824DDFA8 calls +28 then +192 as the campaign's
// calls +28 then +164); every field of a menu, an entry, an FEImage
// (0x820705B4, its draw an exact twin) and an FEText (0x820702DC) is the
// campaign's. A system keeps its menus at +4 and which is up at +28; five
// system classes (0x82070880, 0x820713E0, 0x82071478 the dialogs,
// 0x82071620, 0x82082B68) put their update at +52, and each of the four
// that has its own calls the base one, sub_824D5FC0, hooked here. A quad is
// drawn by sub_824D7E60 (the campaign's exact twin).

#include "menu_mouse.h"
#include "input.h"
#include "kernel.h"
#include "window.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#ifdef COD3_MP
extern "C" PPC_FUNC(__imp__sub_824D5FC0);   // the menu systems' update
extern "C" PPC_FUNC(__imp__sub_824D7E60);   // a front end quad drawn
#else
extern "C" PPC_FUNC(__imp__sub_824EE2E0);
extern "C" PPC_FUNC(__imp__sub_824BFDD8);
#endif

namespace
{
    // A system's fields.
#ifdef COD3_MP
    constexpr uint32_t SystemMenus = 4, SystemActive = 28, SystemCount = 0;   // no count found: bounded below
#else
    constexpr uint32_t SystemMenus = 4, SystemActive = 12, SystemCount = 24;
#endif
    // A menu's.
    constexpr uint32_t MenuEntries = 4, MenuRow = 20;
    constexpr uint32_t MenuFirstShown = 32, MenuSelected = 34, MenuCount = 38, MenuShown = 40, MenuLocked = 48;
#ifdef COD3_MP
    constexpr uint32_t MenuSetSelection = 192;
#else
    constexpr uint32_t MenuSetSelection = 164;
#endif
    // The cross's up and down (vtable +204, sub_824C5E50, and +208) play the
    // move sound (vtable +28, sub_824E7810: UI_Select) when the highlight
    // moves, if the menu has one (+49), then call SetSelection.
    constexpr uint32_t MenuHasMoveSound = 49, MenuPlayMoveSound = 28;
    // An entry's.
    constexpr uint32_t EntryWidget = 16, EntryHidden = 22;
    // Classes, by their vtables.
#ifdef COD3_MP
    constexpr uint32_t FEImageVtable = 0x820705B4, FETextVtable = 0x820702DC;
    constexpr uint32_t TitleMenuVtable = 0, DialogSystemVtable = 0x82071478;
    constexpr uint32_t VtablesFrom = 0x82070000, VtablesTo = 0x820A8000;
#else
    constexpr uint32_t FEImageVtable = 0x82070E04, FETextVtable = 0x82070B54;
    constexpr uint32_t TitleMenuVtable = 0x82071724, DialogSystemVtable = 0x820719D4;
    constexpr uint32_t VtablesFrom = 0x82070000, VtablesTo = 0x820A0000;
#endif

    float ReadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t v = Guest::Read32(base, address);
        float f;
        memcpy(&f, &v, 4);
        return f;
    }
    int16_t Read16s(uint8_t* base, uint32_t address) { return int16_t(Guest::Read16(base, address)); }
    bool Valid(uint32_t p) { return p >= 0x40000000u && p < 0xA0000000u; }

    // The pointer in the menus' coordinates (640 by 480 over the 2D): false
    // when it is not over them or the window is not in front. The 2D is the
    // largest 16:9 rectangle in the middle of the window: the picture with
    // its bars, or with the screen filled (F11) the HUD and the menus kept
    // 16:9 in the middle (render_d3d11.cpp, RenderCommands::SetFill).
    bool PointerInMenus(float& x, float& y)
    {
        // 0.5.618: COD3_MENUCURSOR="22:320,141 26:320,181" (a test nobody plays):
        // the pointer at that place of the menus (640 by 480) from that many
        // seconds after the start, wobbling by a hair as a hand on the mouse does
        static const char* const forced = getenv("COD3_MENUCURSOR");
        if (forced != nullptr)
        {
            static const uint64_t start = GetTickCount64();
            static unsigned calls = 0;
            const double now = double(GetTickCount64() - start) / 1000.0;
            bool found = false;
            for (const char* at = forced; *at != 0;)
            {
                float t = 0.0f, px = 0.0f, py = 0.0f;
                if (sscanf(at, "%f:%f,%f", &t, &px, &py) == 3 && t <= now) { x = px; y = py; found = true; }
                while (*at != 0 && *at != ' ') at++;
                while (*at == ' ') at++;
            }
            if (found) x += (calls++ & 1) ? 0.5f : 0.0f;
            return found;
        }
        if (!Window::HasFocus()) return false;
        const HWND hwnd = static_cast<HWND>(Window::Handle());
        if (hwnd == nullptr) return false;
        POINT p{};
        RECT client{};
        if (!GetCursorPos(&p) || !ScreenToClient(hwnd, &p) || !GetClientRect(hwnd, &client)) return false;
        const float cw = float(client.right), ch = float(client.bottom);
        if (cw < 2.0f || ch < 2.0f) return false;
        const float aspect = 16.0f / 9.0f;
        float w = cw, h = cw / aspect;
        if (h > ch) { h = ch; w = h * aspect; }
        const float x0 = std::floor((cw - w) * 0.5f), y0 = std::floor((ch - h) * 0.5f);
        const float u = (float(p.x) + 0.5f - x0) / std::floor(w), v = (float(p.y) + 0.5f - y0) / std::floor(h);
        if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) return false;
        x = u * 640.0f;
        y = v * 480.0f;
        return true;
    }

    std::atomic<uint64_t> g_menuUpAt{ 0 };       // when a menu system last updated with a menu up (GetTickCount64)
    std::atomic<uint32_t> g_menuVtable{ 0 };     // the menu pointed at: its class, by its vtable
    std::atomic<uint32_t> g_currentMenu{ 0 };    // 0.5.637: and its address
    std::atomic<bool> g_switchTeamSelected{ false };   // 0.5.643: its highlighted entry reads SWITCH TEAM
    std::atomic<bool> g_teamFromPause{ false };        // 0.5.644: SWITCH SIDE taken: the team choice in the pause menu's place
    uint64_t g_teamAskedAt = 0;                        // when it was put up (to tell whether it was seen)
    bool g_teamFromPauseOpen = false;                  // 0.5.645: it is up, the pause flag kept set for it
    std::atomic<bool> g_pauseAsked{ false };           // 0.5.646: Start in the seat: the pause menu to put up

    struct Box
    {
        float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool Contains(float x, float y, float margin = 0.0f) const { return x >= x0 - margin && x < x1 + margin && y >= y0 - margin && y < y1 + margin; }
        float Width() const { return x1 - x0; }
        float Height() const { return y1 - y0; }
        float CentreX() const { return (x0 + x1) * 0.5f; }
        float CentreY() const { return (y0 + y1) * 0.5f; }
    };

    // The quads the front end drew lately (sub_824BFDD8 draws one), by
    // address: a menu keeps pictures of its other pages too, shown by their
    // flags and never drawn. A table of 16384 by a hash of the address, a
    // few places tried from there; written and read on the title's thread.
    struct DrawnQuad { uint32_t address = 0, at = 0; };
    DrawnQuad g_drawn[16384];
    constexpr uint32_t DrawnFor = 250, Probes = 8;

    uint32_t DrawnSlot(uint32_t quad) { return (quad * 2654435761u) >> 18; }

    bool DrawnLately(uint32_t quad)
    {
        const uint32_t now = uint32_t(GetTickCount64());
        for (uint32_t i = 0, at = DrawnSlot(quad); i < Probes; i++, at = (at + 1) & 16383)
            if (g_drawn[at].address == quad) return now - g_drawn[at].at < DrawnFor;
        return false;
    }

    void NoteDrawn(uint32_t quad)
    {
        const uint32_t now = uint32_t(GetTickCount64());
        uint32_t free = ~0u;
        for (uint32_t i = 0, at = DrawnSlot(quad); i < Probes; i++, at = (at + 1) & 16383)
        {
            if (g_drawn[at].address == quad) { g_drawn[at].at = now; return; }
            if (free == ~0u && (g_drawn[at].address == 0 || now - g_drawn[at].at >= DrawnFor)) free = at;
        }
        g_drawn[free != ~0u ? free : DrawnSlot(quad)] = { quad, now };
    }

    // The pictures a menu keeps (an FEImage among its words) where its first
    // quad is drawn, drawn ones only. A quad's corners are 20 bytes apart
    // from +16, x and y first, its colour's top byte the alpha.
    std::vector<Box> Pictures(uint8_t* base, uint32_t menu)
    {
        std::vector<Box> pictures;
        for (uint32_t o = 4; o < 512; o += 4)
        {
            const uint32_t p = Guest::Read32(base, menu + o);
            if (!Valid(p) || Guest::Read32(base, p) != FEImageVtable || Guest::Read8(base, p + 4) == 0) continue;
            const uint32_t quads = Guest::Read32(base, p + 32);
            const uint32_t quad = quads != 0 && Guest::Read32(base, p + 40) != 0 ? Guest::Read32(base, quads) : 0;
            if (quad == 0 || Guest::Read32(base, quad + 32) >> 24 == 0 || !DrawnLately(quad)) continue;
            Box box{ ReadFloat(base, quad + 16), ReadFloat(base, quad + 20), ReadFloat(base, quad + 76), ReadFloat(base, quad + 80) };
            if (box.x0 > box.x1) std::swap(box.x0, box.x1);
            if (box.y0 > box.y1) std::swap(box.y0, box.y1);
            // A slider's fill is drawn from its bar's left edge as far as
            // the value, while the picture stands at the bar's middle
            // (+20): the bar is that much either side.
            if (const float middle = ReadFloat(base, p + 20); middle > box.CentreX() + 1.0f && middle < box.x0 + 200.0f) box.x1 = 2.0f * middle - box.x0;
            if (box.Width() >= 2.0f && box.Height() >= 2.0f && box.x1 > 0.0f && box.x0 < 640.0f && box.y1 > 0.0f && box.y0 < 480.0f) pictures.push_back(box);
        }
        return pictures;
    }

    // A row with an arrow at each end: an option's (its value between, the
    // cross's left and right change it) or the chapter select's strip of
    // chapters (the one between in the middle is the one chosen). The
    // arrows are small pictures level with each other, the outermost two
    // of a row.
    struct Strip
    {
        Box left, right;
        int entry = -1;                 // the menu's entry on the row, -1 for none
        std::vector<Box> cells;         // the larger pictures between, left to right
    };
    std::vector<Strip> Strips(const std::vector<Box>& pictures)
    {
        const auto arrow = [](const Box& b) { return b.Width() >= 4.0f && b.Width() <= 28.0f && b.Height() >= 4.0f && b.Height() <= 34.0f; };
        std::vector<Strip> strips;
        std::vector<bool> used(pictures.size(), false);
        for (size_t i = 0; i < pictures.size(); i++)
        {
            if (used[i] || !arrow(pictures[i])) continue;
            Strip strip;
            strip.left = strip.right = pictures[i];
            int members = 0;
            for (size_t j = i; j < pictures.size(); j++)
                if (!used[j] && arrow(pictures[j]) && std::fabs(pictures[j].CentreY() - pictures[i].CentreY()) <= 3.0f)
                {
                    used[j] = true;
                    members++;
                    if (pictures[j].x0 < strip.left.x0) strip.left = pictures[j];
                    if (pictures[j].x0 > strip.right.x0) strip.right = pictures[j];
                }
            if (members < 2 || strip.right.x0 - strip.left.x1 < 30.0f) continue;
            for (const Box& b : pictures)
                if (b.Width() >= 30.0f && b.Height() >= 20.0f && b.x0 >= strip.left.x1 && b.x1 <= strip.right.x0 &&
                    std::fabs(b.CentreY() - strip.left.CentreY()) <= 6.0f)
                    strip.cells.push_back(b);
            std::sort(strip.cells.begin(), strip.cells.end(), [](const Box& a, const Box& b) { return a.x0 < b.x0; });
            strips.push_back(std::move(strip));
        }
        return strips;
    }

    // A text's string (an FEText's +28 points at a string object: the
    // characters' address, then the capacity and the length as halfwords).
    std::string TextOf(uint8_t* base, uint32_t widget)
    {
        const uint32_t text = Guest::Read32(base, widget + 28);
        if (text < 0x40000000u || text >= 0xC0000000u) return {};
        const uint32_t chars = Guest::Read32(base, text);
        const uint32_t length = std::min<uint32_t>(Guest::Read16(base, text + 6), 128);
        if (chars < 0x40000000u || chars >= 0xC0000000u) return {};
        std::string s(length, '\0');
        for (uint32_t i = 0; i < length; i++) s[i] = char(Guest::Read8(base, chars + i));
        return s;
    }

    // Where each of a menu's entries is on the screen, empty for one that
    // is not shown. An entry's widget is placed at its text's anchor (+44,
    // +48) with its box's width (+8) and alignment (+108 bits 4 and 5: 0
    // centred, 1 left, 2 right); its row is as tall as the step to its
    // neighbours, from a little above the anchor (the text's top) down. A
    // row with arrows reaches to them.
    //
    // A dialog (DialogMenu, in the DialogMenuSystem) lays its entries out
    // where they are not drawn: what it shows are the four button texts of
    // its panel (the system's +52; +24 to +36), filled from its entries, so
    // an entry is where the text that reads the same as it is.
    std::vector<Box> EntryBoxes(uint8_t* base, uint32_t system, uint32_t menu, bool dialog, const std::vector<Strip>& strips)
    {
        const uint32_t entries = Guest::Read32(base, menu + MenuEntries);
        const int count = Read16s(base, menu + MenuCount);
        if (entries == 0 || count <= 0 || count > 64) return {};
        std::vector<Box> boxes(static_cast<size_t>(count));
        if (dialog)
        {
            // 0.5.602: the panel's texts found, not read at a place (the
            // campaign's panel is the system's +52, its buttons +24 to +36;
            // the multiplayer's system is laid out otherwise): every FEText
            // the system points at, or one of the objects it points at does.
            std::vector<uint32_t> labels;
            // 0.5.635: and the menu's own (the pause menu's quit question,
            // 82071504: its YES and NO drawn from an object at its +8, that
            // object's +148 and +152)
            // 0.5.638: the menu's own first, in the order found (636 sorted
            // them by address: the question's entries went to another YES and
            // NO, at 118,137 and 118,157, and the clicks over the ones drawn at
            // 320,267 and 320,287 found nothing)
            for (const uint32_t holder : { menu, system })
            for (uint32_t o = 4; o < 256; o += 4)
            {
                const uint32_t p = Guest::Read32(base, holder + o);
                if (!Valid(p)) continue;
                const uint32_t pv = Guest::Read32(base, p);
                if (pv == FETextVtable) { labels.push_back(p); continue; }
                if (pv < VtablesFrom || pv >= VtablesTo) continue;
                for (uint32_t q = 4; q < 256; q += 4)   // 0.5.636: was 128; the quit question's YES and NO are at +148 and +152
                {
                    const uint32_t c = Guest::Read32(base, p + q);
                    if (Valid(c) && Guest::Read32(base, c) == FETextVtable) labels.push_back(c);
                }
            }
            {
                std::vector<uint32_t> once;
                for (const uint32_t l : labels) if (std::find(once.begin(), once.end(), l) == once.end()) once.push_back(l);
                labels.swap(once);
            }
            for (int i = 0; i < count; i++)
            {
                const uint32_t entry = Guest::Read32(base, entries + i * 4);
                if (entry == 0 || Guest::Read8(base, entry + EntryHidden) != 0) continue;
                const uint32_t widget = Guest::Read32(base, entry + EntryWidget);
                if (widget == 0) continue;
                const std::string text = TextOf(base, widget);
                if (text.empty()) continue;
                for (const uint32_t label : labels)
                {
                    if (label == widget || TextOf(base, label) != text || Guest::Read8(base, label + 4) == 0) continue;
                    const float lx = ReadFloat(base, label + 44), ly = ReadFloat(base, label + 48);
                    boxes[size_t(i)] = { lx - 90.0f, ly - 6.0f, lx + 90.0f, ly + 14.0f };
                    break;
                }
            }
            // 0.5.635: none read the same: the shown entries in order onto the
            // visible texts top to bottom, when there are as many of each
            bool found = false;
            for (const Box& b : boxes) found = found || b.Height() > 0.0f;
            if (!found)
            {
                std::vector<int> shown;
                for (int i = 0; i < count; i++)
                {
                    const uint32_t entry = Guest::Read32(base, entries + i * 4);
                    if (entry != 0 && Guest::Read8(base, entry + EntryHidden) == 0 && Guest::Read32(base, entry + EntryWidget) != 0) shown.push_back(i);
                }
                std::vector<std::pair<float, float>> places;
                for (const uint32_t label : labels)
                {
                    if (Guest::Read8(base, label + 4) == 0 || TextOf(base, label).empty()) continue;
                    const float lx = ReadFloat(base, label + 44), ly = ReadFloat(base, label + 48);
                    if (lx > 0.0f && lx < 640.0f && ly > 0.0f && ly < 480.0f) places.push_back({ ly, lx });
                }
                std::sort(places.begin(), places.end());
                if (!shown.empty() && places.size() == shown.size())
                    for (size_t k = 0; k < shown.size(); k++)
                        boxes[size_t(shown[k])] = { places[k].second - 90.0f, places[k].first - 6.0f, places[k].second + 90.0f, places[k].first + 14.0f };
            }
            return boxes;
        }
        float rowHeight = float(int32_t(Guest::Read32(base, menu + MenuRow)));
        {
            // The rows' step as laid out, which a menu may make other than its
            // row field: the smallest gap between two shown entries.
            float last = 0.0f, smallest = 0.0f;
            bool have = false;
            for (int i = 0; i < count; i++)
            {
                const uint32_t entry = Guest::Read32(base, entries + i * 4);
                if (entry == 0 || Guest::Read8(base, entry + EntryHidden) != 0) continue;
                const uint32_t widget = Guest::Read32(base, entry + EntryWidget);
                if (widget == 0) continue;
                const float wy = ReadFloat(base, widget + 48);
                if (have && std::fabs(wy - last) > 1.0f && (smallest == 0.0f || std::fabs(wy - last) < smallest)) smallest = std::fabs(wy - last);
                last = wy;
                have = true;
            }
            if (smallest > 0.0f) rowHeight = smallest;
        }
        if (!(rowHeight > 4.0f && rowHeight < 120.0f)) rowHeight = 20.0f;
        // A list longer than it shows: only the stretch on the screen.
        int first = 0, last = count;
        if (const int shown = Read16s(base, menu + MenuShown); shown > 0 && shown < count)
        {
            first = std::clamp(int(Read16s(base, menu + MenuFirstShown)), 0, count - 1);
            last = std::min(count, first + shown);
        }
        for (int i = first; i < last; i++)
        {
            const uint32_t entry = Guest::Read32(base, entries + i * 4);
            if (entry == 0 || Guest::Read8(base, entry + EntryHidden) != 0) continue;
            const uint32_t widget = Guest::Read32(base, entry + EntryWidget);
            if (widget == 0) continue;
            const float wx = ReadFloat(base, widget + 44), wy = ReadFloat(base, widget + 48);
            float width = ReadFloat(base, widget + 8);
            if (!(width > 8.0f && width < 640.0f)) width = 240.0f;
            const uint32_t align = (Guest::Read16(base, widget + 108) >> 4) & 3;
            Box box;
            box.x0 = align == 0 ? wx - width * 0.5f : align == 2 ? wx - width : wx;
            box.x1 = box.x0 + width;
            box.y0 = wy - rowHeight * 0.3f;
            box.y1 = box.y0 + rowHeight;
            for (const Strip& strip : strips)
                if (strip.entry == i)
                {
                    box.x0 = std::min(box.x0, strip.left.x0);
                    box.x1 = std::max(box.x1, strip.right.x1);
                }
            boxes[size_t(i)] = box;
        }
        // 0.5.640: a box as wide as its words at least (the team choice's
        // entries are 18 wide by their field, 118..136: "SPECTATOR" reaches far
        // past that, and the pointer on most of a word found nothing), about 9
        // a letter; never over the next box on its row
        for (int i = first; i < last; i++)
        {
            Box& box = boxes[size_t(i)];
            if (box.Height() <= 0.0f) continue;
            const uint32_t entry = Guest::Read32(base, entries + i * 4);
            const uint32_t widget = entry ? Guest::Read32(base, entry + EntryWidget) : 0;
            if (!widget) continue;
            const float wanted = float(TextOf(base, widget).size()) * 9.0f + 10.0f;
            if (box.Width() >= wanted) continue;
            float right = box.x0 + wanted;
            for (int j = first; j < last; j++)
            {
                const Box& other = boxes[size_t(j)];
                if (j == i || other.Height() <= 0.0f || other.x0 <= box.x0) continue;
                if (other.y0 < box.y1 && other.y1 > box.y0) right = std::min(right, other.x0);
            }
            box.x1 = std::max(box.x1, std::min(right, 640.0f));
        }
        return boxes;
    }

    // A call through the menu's vtable, as the title makes it.
    void CallVirtual(PPCContext& ctx, uint8_t* base, uint32_t function)
    {
        if (PPCFunc* f = Guest::Lookup(function)) f(ctx, base);
    }

    // The highlight moved to an entry as the cross moves it: the menu's move
    // sound, then SetSelection.
    void Highlight(PPCContext& ctx, uint8_t* base, uint32_t menu, int entry)
    {
        const uint32_t vtable = Guest::Read32(base, menu);
        if (Guest::Read8(base, menu + MenuHasMoveSound) != 0)
        {
            PPCContext call = ctx;
            call.r3.u64 = menu;
            CallVirtual(call, base, Guest::Read32(base, vtable + MenuPlayMoveSound));
        }
        PPCContext call = ctx;
        call.r3.u64 = menu;
        call.r4.u64 = uint64_t(entry);
        call.r5.u64 = 0;
        CallVirtual(call, base, Guest::Read32(base, vtable + MenuSetSelection));
    }

    // What the left button does where the pointer is (input.cpp asks).
    std::atomic<int> g_clickDirection{ 0 };     // the cross's left (-1) or right (1) it presses, 0 for none
    std::atomic<int> g_clickSteps{ 0 };         // how many times
    std::atomic<bool> g_clickTakes{ false };    // A: an entry highlighted under it, or the chapter in the middle
    // 0.5.608: the spectator's seat's box under the team choice (menu coordinates)
    std::mutex g_seatMutex;
    Box g_seat;
    std::atomic<bool> g_onSeat{ false };
    std::atomic<bool> g_pointable{ true };   // 0.5.614: the menu up has entries or arrows the pointer finds
    // 0.5.618: the lobby's lists: their class and where each keeps its highlighted row
    struct ListMenu { uint32_t vtable; uint32_t selected; };
#ifdef COD3_MP
    constexpr ListMenu ListMenus[] = { { 0x8207E95C, 250 },     // the multiplayer main menu (617's walk)
                                       { 0x82083D3C, 246 } };   // System Link's Create / Find Match (learnt in the user's 620 play)
#else
    constexpr ListMenu ListMenus[] = { { 0, 0 } };
#endif
    // 0.5.620: and the ones learnt while playing (Learn, below): a list whose
    // highlight is seen to move gets its row's place found from it
    ListMenu g_learnt[16] = {};
    int g_learntN = 0;
    const ListMenu* ListOf(uint32_t vtable)
    {
        static const bool learnTest = getenv("COD3_MENULEARN_TEST") != nullptr;   // a test: the known ones left to be learnt
        if (!learnTest)
            for (const ListMenu& l : ListMenus) if (l.vtable != 0 && l.vtable == vtable) return &l;
        for (int i = 0; i < g_learntN; i++) if (g_learnt[i].vtable == vtable) return &g_learnt[i];
        return nullptr;
    }

    // 0.5.620: LEARNING A LIST'S ROW. On a menu of no entries of its own, not
    // known yet, with full width row bars: a halfword of its first 1 KB under
    // 32 that steps by one (or wraps round to or from 0) while the rest of
    // such small numbers stay is its highlighted row, as +250 is the main
    // menu's (617-619's walks: no other small number of it moved). Twice the
    // same place: learnt for that class of menu ("menu: learnt" in the log).
    void Learn(uint8_t* base, uint32_t menu)
    {
        static const bool off = [] { const char* t = getenv("COD3_MENULEARN"); return t && t[0] == '0'; }();
        if (off || g_learntN >= 16) return;
        static uint32_t watched = 0, candidate = 0;
        static int seen = 0;
        static uint8_t last[1024];
        if (menu != watched) { watched = menu; candidate = 0; seen = 0; memcpy(last, base + menu, sizeof last); return; }
        uint32_t stepped = 0xFFFFFFFFu;
        int steppedCount = 0, otherCount = 0;
        for (uint32_t o = 0; o < sizeof last; o += 2)
        {
            const uint16_t was = uint16_t((last[o] << 8) | last[o + 1]);
            const uint16_t now = Guest::Read16(base, menu + o);
            if (was == now || was >= 32 || now >= 32) continue;
            const bool step = was + 1 == now || now + 1 == was || (was == 0 && now > 0) || (now == 0 && was > 0);
            if (step) { if (stepped == 0xFFFFFFFFu) stepped = o; steppedCount++; }
            else otherCount++;
        }
        memcpy(last, base + menu, sizeof last);
        // the row and, at most, a mirror of it (the main menu keeps it twice)
        if (stepped == 0xFFFFFFFFu || steppedCount > 2 || otherCount > 0) return;
        if (stepped == candidate) seen++;
        else { candidate = stepped; seen = 1; }
        if (seen < 2) return;
        const uint32_t vtable = Guest::Read32(base, menu);
        if (ListOf(vtable)) return;
        g_learnt[g_learntN++] = { vtable, stepped };
        printf("menu: learnt: the list %08X (vtable %08X) keeps its highlighted row at +%u: the pointer steers it now\n", menu, vtable, stepped);
        fflush(stdout);
    }
    std::atomic<int> g_listStep{ 0 };   // the cross's up (-1) or down (1) toward the row under the pointer

    // The pointer's work on the menu up: the entry under it highlighted
    // when the pointer has moved or the left button is down (so the cross
    // and the keys still move the highlight while it rests), as the menu's
    // own SetSelection does it. Over an arrow, its row is highlighted and
    // the button is the cross's left or right; over an option's value too
    // (right, the next one); over a chapter of the strip, as many presses
    // as it is from the middle one. The button is A only over the entry
    // that is highlighted, so a click never takes another one.
    // 0.5.635: a dialog in a system of another class than the dialogs' (user,
    // in a match: the pause menu's quit question "just stays on no and i cant
    // use the mouse to ... click yes"). Played logs: its menu (82071504, 4
    // entries) lays YES and NO out at y -50..6, off the screen, and draws them
    // from texts it holds at 320,267 and 320,287; the click was A on the one
    // highlighted, NO. A menu none of whose shown entries is on the screen is
    // pointed at as a dialog: by the texts that read as its entries.
    bool EntriesOffScreen(uint8_t* base, uint32_t system, uint32_t menu)
    {
        bool any = false;
        for (const Box& b : EntryBoxes(base, system, menu, false, {}))
        {
            if (b.Height() <= 0.0f) continue;
            any = true;
            // another play had them at -111,-22..111,6 and -111,6..111,34: off by their x
            if (b.x0 >= 0.0f && b.y0 >= 0.0f && b.x1 <= 640.0f && b.y1 <= 480.0f) return false;
        }
        return any;
    }

    void Point(PPCContext& ctx, uint8_t* base, uint32_t system, uint32_t menu, bool dialog)
    {
        const bool offScreen = !dialog && EntriesOffScreen(base, system, menu);   // 0.5.635
        if (offScreen) dialog = true;
        static float lastX = -1.0f, lastY = -1.0f;
        float x, y;
        if (!PointerInMenus(x, y))
        {
            g_clickDirection.store(0);
            g_clickTakes.store(false);
            g_listStep.store(0);
            lastX = lastY = -1.0f;
            return;
        }
        // 0.5.618: THE LOBBY'S LISTS: menus of no entries of their own whose rows
        // are full width bars, 20 high (the multiplayer main menu, 8207E95C: 616's
        // shape, bars 111..211), keeping their highlighted row in a halfword (617's
        // walk: +250, one up a down press). The row under the pointer is reached by
        // presses of the cross (input.cpp, ListStep), so the pointer highlights as
        // a PC menu does, and the click is A on it.
        if (const ListMenu* list = ListOf(Guest::Read32(base, menu)))
        {
            std::vector<Box> rows;
            for (const Box& b : Pictures(base, menu))
                if (b.Width() >= 300.0f && b.Height() >= 16.0f && b.Height() <= 24.0f) rows.push_back(b);
            std::sort(rows.begin(), rows.end(), [](const Box& a, const Box& b) { return a.y0 < b.y0; });
            int hover = -1;
            for (size_t i = 0; i < rows.size(); i++) if (rows[i].Contains(x, y)) { hover = int(i); break; }
            const int selected = Read16s(base, menu + list->selected);
            const bool moved = std::fabs(x - lastX) > 0.3f || std::fabs(y - lastY) > 0.3f;
            lastX = x; lastY = y;
            static uint64_t movedAt = 0;
            if (moved) movedAt = GetTickCount64();
            // only while the pointer is being used (moved in the last 2 s), so the keys and the pad still move the highlight
            // 0.5.621: the button down steers too (620's play log: clicks on the learnt
            // System Link list did nothing while its highlight was still on its way)
            const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool steering = hover >= 0 && (GetTickCount64() - movedAt < 2000 || pressed);
            g_listStep.store(steering && selected != hover ? (hover > selected ? 1 : -1) : 0);
            g_pointable.store(!rows.empty());
            g_clickDirection.store(0);
            g_clickSteps.store(0);
            // over no row: A on the row highlighted, as elsewhere (0.5.600)
            g_clickTakes.store(hover < 0 || hover == selected);
            return;
        }
        g_listStep.store(0);
        const std::vector<Box> pictures = dialog ? std::vector<Box>{} : Pictures(base, menu);
        std::vector<Strip> strips = Strips(pictures);
        std::vector<Box> boxes = EntryBoxes(base, system, menu, dialog, strips);
        // Each strip's entry: the one whose row it is level with.
        for (Strip& strip : strips)
        {
            // 0.5.636: of the entries on its row, the one between its arrows
            // first (System Link's Create Match, 82082F0C: two entries a row,
            // the name's at 248..298 and the value's at 392..441 between the
            // arrows at 306 and 528; the arrows went to the name's, and the
            // cross's right only moved the highlight over to the value's)
            float best = 0.0f;
            for (size_t i = 0; i < boxes.size(); i++)
            {
                if (boxes[i].Height() <= 0.0f) continue;
                const float d = std::fabs(boxes[i].CentreY() - strip.left.CentreY());
                // 0.5.642: by where the box starts (640 widened the names' boxes:
                // MAX NUMBER OF PLAYERS reached 392, its centre between the arrows)
                const bool between = boxes[i].x0 >= strip.left.x1 - 4.0f && boxes[i].x0 < strip.right.x0;
                const float cost = d + (between ? 0.0f : 1000.0f);
                if (d < boxes[i].Height() * 0.6f && (strip.entry < 0 || cost < best)) { strip.entry = int(i); best = cost; }
            }
        }
        // 0.5.642 (user: "i cant click on the arrows in the max numbers of
        // players section and in the advance options ... i was able to click a
        // few but nothing else"). 641's log: System Link's Advanced Options
        // (8208310C) is 6 rows of arrows and 12 entries, a name and a value a
        // row, but some values' boxes are not found (no entry 5 or 7) and
        // "shown 8" leaves out the last rows' entries: those arrows had no
        // entry. On a menu of only arrow rows and two entries each (Create Match
        // 82082F0C too: 3 and 6), row k's arrows are entry 2k+1's, the value's.
        {
            const int count = Read16s(base, menu + MenuCount);
            bool arrowsOnly = !strips.empty();
            for (const Strip& s : strips) arrowsOnly = arrowsOnly && s.cells.empty();
            if (arrowsOnly && count == int(strips.size()) * 2)
            {
                std::vector<Strip*> rows;
                for (Strip& s : strips) rows.push_back(&s);
                std::sort(rows.begin(), rows.end(), [](const Strip* a, const Strip* b) { return a->left.CentreY() < b->left.CentreY(); });
                for (size_t k = 0; k < rows.size(); k++) rows[k]->entry = int(2 * k + 1);
                if (boxes.size() < size_t(count)) boxes.resize(size_t(count));
            }
        }
        // A menu of entries also keeps the arrows of its other pages, not
        // drawn: only the rows of its entries count there.
        if (!boxes.empty())
            strips.erase(std::remove_if(strips.begin(), strips.end(), [](const Strip& s) { return s.entry < 0; }), strips.end());
        boxes = EntryBoxes(base, system, menu, dialog, strips);

        // 0.5.636: the off-screen dialog (the quit question) steered as the
        // lobby's lists are: 635's play log had it found, yet every click was A
        // on NO (its highlight set through SetSelection does not take there).
        // The cross brings the highlight to the entry under the pointer, one
        // press at a time (input.cpp ListSteps), and the click is A once there.
        // 0.5.643: the team choice too (user, 642: before picking a team "it
        // still doesnt scroll or highlight the box its supposed be hovering
        // over"; SetSelection does not show there). Its SPECTATOR (entry 3) is
        // reached by the cross's down from AXIS (spectator.cpp) and its click
        // is the seat's (input.cpp, PointerOnSeat).
#ifdef COD3_MP
        const bool teamChoice = Guest::Read32(base, menu) == 0x82084854u;
#else
        const bool teamChoice = false;
#endif
        if (offScreen || teamChoice)
        {
            int hover = -1;
            bool any = false;
            for (size_t i = 0; i < boxes.size(); i++)
            {
                if (boxes[i].Height() <= 0.0f) continue;
                any = true;
                if (hover < 0 && boxes[i].Contains(x, y)) hover = int(i);
            }
            const int selected = Read16s(base, menu + MenuSelected);
            const bool moved = std::fabs(x - lastX) > 0.3f || std::fabs(y - lastY) > 0.3f;
            lastX = x; lastY = y;
            static uint64_t movedAt = 0;
            if (moved) movedAt = GetTickCount64();
            const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            const bool steering = hover >= 0 && (GetTickCount64() - movedAt < 2000 || pressed);
            g_listStep.store(steering && selected != hover ? (hover > selected ? 1 : -1) : 0);
            g_pointable.store(any);
            g_clickDirection.store(0);
            g_clickSteps.store(0);
            g_onSeat.store(teamChoice && hover == 3);
            g_clickTakes.store(hover < 0 || hover == selected);
            static uint32_t toldMenu = 0;
            static int told = 0;
            if (menu != toldMenu && told < 20)
            {
                toldMenu = menu; told++;
                printf("menu: %08X (vtable %08X) pointed at by its texts, steered by the cross:", menu, Guest::Read32(base, menu));
                for (size_t i = 0; i < boxes.size(); i++) if (boxes[i].Height() > 0.0f) printf(" entry %zu at %.0f,%.0f..%.0f,%.0f", i, boxes[i].x0, boxes[i].y0, boxes[i].x1, boxes[i].y1);
                printf("\n");
                fflush(stdout);
            }
            return;
        }

        int over = -1, direction = 0, steps = 0;
        bool takes = false;
        for (const Strip& strip : strips)
        {
            const float margin = 5.0f;
            if (strip.left.Contains(x, y, margin)) { direction = -1; steps = 1; over = strip.entry; break; }
            if (strip.right.Contains(x, y, margin)) { direction = 1; steps = 1; over = strip.entry; break; }
            if (strip.entry >= 0)
            {
                // An option's value, between its arrows.
                // 0.5.642: the row's height by its arrows (an entry's box may not be found)
                const Box value{ strip.left.x1 + margin, strip.left.y0 - 2.0f, strip.right.x0 - margin, strip.left.y1 + 2.0f };
                if (value.Contains(x, y)) { direction = 1; steps = 1; over = strip.entry; break; }
            }
            else if (!strip.cells.empty())
            {
                // The strip's chapters: the middle one is the one chosen.
                const float middle = (strip.left.x1 + strip.right.x0) * 0.5f;
                size_t centre = 0;
                for (size_t c = 1; c < strip.cells.size(); c++)
                    if (std::fabs(strip.cells[c].CentreX() - middle) < std::fabs(strip.cells[centre].CentreX() - middle)) centre = c;
                for (size_t c = 0; c < strip.cells.size(); c++)
                    if (strip.cells[c].Contains(x, y))
                    {
                        const int k = int(c) - int(centre);
                        if (k == 0) takes = true;
                        else { direction = k < 0 ? -1 : 1; steps = std::abs(k); }
                        break;
                    }
                if (takes || direction != 0) break;
            }
        }
        // A slider: a bar on an entry's row that has no arrows (the
        // sensitivities, the volume). Its left half is the cross's left, its
        // right half the right, again and again while the button is held.
        if (direction == 0 && !takes)
        {
            const Box* widest = nullptr;
            int row = -1;
            for (const Box& b : pictures)
            {
                if (b.Width() < 40.0f || b.Width() > 220.0f || b.Height() < 4.0f || b.Height() > 24.0f || !b.Contains(x, y, 4.0f)) continue;
                for (size_t i = 0; i < boxes.size(); i++)
                {
                    if (boxes[i].Height() <= 0.0f || std::fabs(b.CentreY() - boxes[i].CentreY()) >= boxes[i].Height() * 0.6f) continue;
                    bool arrows = false;
                    for (const Strip& strip : strips) arrows = arrows || strip.entry == int(i);
                    if (arrows) continue;
                    if (widest == nullptr || b.Width() > widest->Width()) { widest = &b; row = int(i); }
                }
            }
            if (widest != nullptr) { direction = x < widest->CentreX() ? -1 : 1; steps = 1; over = row; }
        }
        if (direction == 0 && !takes)
            for (size_t i = 0; i < boxes.size(); i++)
                if (boxes[i].Height() > 0.0f && boxes[i].Contains(x, y)) { over = int(i); break; }
        // A menu of no entries with a strip (the chapter select): its large
        // picture, the chosen chapter's, takes it too.
        if (direction == 0 && !takes && boxes.empty() && !strips.empty())
        {
            const Box* largest = nullptr;
            for (const Box& b : pictures)
                if (b.Width() >= 120.0f && b.Height() >= 80.0f && (largest == nullptr || b.Width() * b.Height() > largest->Width() * largest->Height())) largest = &b;
            if (largest != nullptr && largest->Contains(x, y)) takes = true;
        }

        // 0.5.614: whether this menu could be pointed at (entries found, or
        // arrows, or a dialog's buttons); not (user, 613: the lobby's screens
        // with no entries of their own: the mouse did nothing there), input.cpp
        // drives it as before 0.5.595: moving steps the highlight, a click is A
        {
            bool any = !strips.empty();
            for (const Box& b : boxes) any = any || b.Height() > 0.0f;
            g_pointable.store(any);
        }
        // 0.5.608: the spectator's seat, under the team choice's entries: over
        // it the click is the seat's (input.cpp), nothing of the menu's
        {
            bool onSeat = false;
            if (MenuMouse::TeamMenuUp())
            {
                std::lock_guard<std::mutex> lock(g_seatMutex);
                onSeat = g_seat.Height() > 0.0f && g_seat.Contains(x, y);
            }
            g_onSeat.store(onSeat);
            // 0.5.640: SPECTATOR highlighted under the pointer as the others
            // are (the click is still the seat's: input.cpp)
            if (onSeat) { over = 3; direction = 0; steps = 0; takes = false; }
        }
        const bool moved = std::fabs(x - lastX) > 0.3f || std::fabs(y - lastY) > 0.3f;
        const bool pressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        lastX = x;
        lastY = y;
        static const bool logging = getenv("COD3_MENULOG") != nullptr;
        if (over >= 0 && (moved || pressed) && Read16s(base, menu + MenuSelected) != over && Guest::Read8(base, menu + MenuLocked) == 0)
        {
            if (logging) printf("menu: the pointer (%.1f %.1f) highlights entry %d\n", x, y, over);
            Highlight(ctx, base, menu, over);
            if (g_onSeat.load() && Read16s(base, menu + MenuSelected) != 3) Guest::Write16(base, menu + MenuSelected, 3);   // 0.5.640: the game's own may refuse our fourth
        }
        const bool onEntry = over >= 0 && Read16s(base, menu + MenuSelected) == over;
        g_clickDirection.store(direction);
        g_clickSteps.store(steps);
        // 0.5.600 (user, 599: the left click "didnt register" in the pause
        // menu's save and quit): over no entry, arrow or slider the click is A
        // on the entry highlighted, as a click was before 0.5.595, so a box
        // found a little off the text never leaves the click doing nothing.
        g_clickTakes.store(takes || (onEntry && direction == 0) || (over < 0 && direction == 0 && !g_onSeat.load()));
        if (logging)
        {
            static uint64_t told = 0;
            if (GetTickCount64() - told >= 1000)
            {
                told = GetTickCount64();
                printf("menu: menu %08X (vtable %08X%s), pointer at %.1f,%.1f: %zu strips, %zu entries, over %d, direction %d x%d, takes %d\n", menu,
                    Guest::Read32(base, menu), dialog ? ", a dialog" : "", x, y, strips.size(), boxes.size(), over, direction, steps, g_clickTakes.load() ? 1 : 0);
                fflush(stdout);
            }
        }
    }
}

MenuMouse::Click MenuMouse::ClickUnderPointer()
{
    Click click;
    if (!MenuUp()) return click;
    click.direction = g_clickDirection.load(std::memory_order_relaxed);
    click.steps = g_clickSteps.load(std::memory_order_relaxed);
    click.takes = g_clickTakes.load(std::memory_order_relaxed);
    return click;
}

bool MenuMouse::MenuUp() { return GetTickCount64() - g_menuUpAt.load(std::memory_order_relaxed) < 200; }
bool MenuMouse::SwitchTeamSelected() { return MenuUp() && g_switchTeamSelected.load(); }
bool MenuMouse::SlowSteps() { return TeamMenuUp(); }
void MenuMouse::AskPauseMenu() { g_pauseAsked.store(true); }
void MenuMouse::AskTeamChoiceFromPause() { g_teamFromPause.store(true); printf("menu: SWITCH SIDE taken: the team choice asked for\n"); fflush(stdout); }
uint32_t MenuMouse::CurrentMenu() { return MenuUp() ? g_currentMenu.load(std::memory_order_relaxed) : 0; }
bool MenuMouse::TitleScreen() { return MenuUp() && g_menuVtable.load(std::memory_order_relaxed) == TitleMenuVtable; }

namespace
{
    // The menu up in a system, 0 for none. The multiplayer's systems keep no
    // count found yet: there the menu must be one (its vtable a front end
    // class's with the move sound at +28, as all 60 of its menu classes).
    uint32_t ActiveMenu(uint8_t* base, uint32_t system)
    {
        const int32_t active = int32_t(Guest::Read32(base, system + SystemActive));
        const uint32_t menus = Guest::Read32(base, system + SystemMenus);
        if (active < 0 || !Valid(menus)) return 0;
#ifdef COD3_MP
        if (active >= 64) return 0;
        const uint32_t menu = Guest::Read32(base, menus + uint32_t(active) * 4);
        if (!Valid(menu)) return 0;
        const uint32_t vtable = Guest::Read32(base, menu);
        if (vtable < VtablesFrom || vtable >= VtablesTo || Guest::Read32(base, vtable + MenuPlayMoveSound) != 0x8250A3C0u) return 0;
        const int entries = Read16s(base, menu + MenuCount);
        return entries >= 0 && entries <= 64 ? menu : 0;
#else
        if (active >= int32_t(Guest::Read32(base, system + SystemCount))) return 0;
        return Guest::Read32(base, menus + uint32_t(active) * 4);
#endif
    }

    void SystemUpdate(PPCContext& ctx, uint8_t* base);

    // 0.5.608: the team choice's class (Allies / Axis / Auto-assign: 603's log,
    // "menu: up: ... (vtable 82084854), 3 entries" as the test joined)
#ifdef COD3_MP
    constexpr uint32_t TeamMenuVtable = 0x82084854;
#else
    constexpr uint32_t TeamMenuVtable = 0;
#endif
    std::atomic<bool> g_teamUp{ false };
    uint32_t g_teamSystem = 0;   // 0.5.610: the team choice's system and its place in it
    int32_t g_teamIndex = -1;
    std::atomic<uint64_t> g_teamUpAt{ 0 };
    std::atomic<bool> g_hidden{ false }, g_restore{ false };
    struct Taken { uint32_t system = 0; int32_t active = -1; };
    Taken g_taken[8];
    int g_takenN = 0;
    void (*g_frameHook)(PPCContext&, uint8_t*) = nullptr;
}

bool MenuMouse::TeamMenuUp() { return GetTickCount64() - g_teamUpAt.load(std::memory_order_relaxed) < 300; }
bool MenuMouse::PointerOnSeat() { return TeamMenuUp() && g_onSeat.load(); }
bool MenuMouse::Pointable() { return MenuUp() && g_pointable.load(); }
int MenuMouse::ListStep() { return MenuUp() ? g_listStep.load() : 0; }
bool MenuMouse::OpenTeamChoice(uint8_t* base)
{
    if (!Valid(g_teamSystem) || g_teamIndex < 0 || g_teamIndex >= 64) return false;
    const uint32_t menus = Guest::Read32(base, g_teamSystem + SystemMenus);
    const uint32_t menu = Valid(menus) ? Guest::Read32(base, menus + uint32_t(g_teamIndex) * 4) : 0;
    if (!Valid(menu) || Guest::Read32(base, menu) != TeamMenuVtable) { printf("menu: the team choice is not where it was (system %08X, %d)\n", g_teamSystem, g_teamIndex); fflush(stdout); return false; }
    Guest::Write32(base, g_teamSystem + SystemActive, uint32_t(g_teamIndex));
    printf("menu: the team choice put up again (system %08X, menu %d)\n", g_teamSystem, g_teamIndex); fflush(stdout);
    return true;
}
bool MenuMouse::SeatBox(float& x0, float& y0, float& x1, float& y1)
{
    if (!TeamMenuUp()) return false;
    std::lock_guard<std::mutex> lock(g_seatMutex);
    if (g_seat.Height() <= 0.0f) return false;
    x0 = g_seat.x0 / 640.0f; y0 = g_seat.y0 / 480.0f; x1 = g_seat.x1 / 640.0f; y1 = g_seat.y1 / 480.0f;
    return true;
}
bool MenuMouse::Hidden() { return g_hidden.load(); }
void MenuMouse::SetHidden(bool hidden)
{
    if (hidden == g_hidden.load()) return;
    g_hidden.store(hidden);
    if (!hidden) g_restore.store(true);
    printf("menu: the game's menus %s\n", hidden ? "taken down (spectating)" : "put back"); fflush(stdout);
}
void MenuMouse::SetFrameHook(void (*hook)(PPCContext&, uint8_t*)) { g_frameHook = hook; }

// 0.5.649: THE TEAM CHOICE'S GOLD FRAME (user: "the gold frame never moves to the
// selection i pick"; 646-648's screenshots: it frames ALLIES whatever is
// selected). 647/648's probes: over 12 row changes no word of the menu's
// entries, texts, panel or other objects changed but the row and the soldier
// shown, and without our SPECTATOR row (PLAY-NO-SPECTATOR-ROW.bat) it stays as
// well: nothing in this port moves it. So it is moved here: its lines are the
// thin, wide quads drawn across the rows (found among the quads drawn lately,
// FrameTrack, from the team choice's update), each drawn moved by the selected
// row's place less the framed row's, and put back as it was after.
namespace
{
    std::atomic<uint32_t> g_frameQuads[8] = {};
    std::atomic<float> g_frameDelta{ 0.0f };
    std::atomic<uint64_t> g_frameUntil{ 0 };
    float QuadY(uint8_t* base, uint32_t quad, int k) { return ReadFloat(base, quad + 20 + 20 * uint32_t(k)); }
    void FrameTrack(uint8_t* base, uint32_t menu)
    {
        static uint64_t lastAt = 0;
        const uint64_t now = GetTickCount64();
        g_frameUntil.store(now + 300);
        const std::vector<Box> boxes = EntryBoxes(base, 0, menu, false, {});
        float top = 1e9f, bottom = -1e9f;
        for (const Box& b : boxes) if (b.Height() > 0.0f) { top = std::min(top, b.y0); bottom = std::max(bottom, b.y1); }
        const int selected = Read16s(base, menu + MenuSelected);
        if (top > bottom || selected < 0 || size_t(selected) >= boxes.size() || boxes[size_t(selected)].Height() <= 0.0f) { g_frameDelta.store(0.0f); return; }
        static uint32_t found[8] = {};
        static int foundN = 0;
        static float frameMid = 0.0f;
        if (now - lastAt >= 250)
        {
            lastAt = now;
            foundN = 0;
            float sum = 0.0f;
            const uint32_t tick = uint32_t(now);
            for (uint32_t i = 0; i < 16384 && foundN < 8; i++)
            {
                const uint32_t quad = g_drawn[i].address;
                if (!Valid(quad) || tick - g_drawn[i].at >= 150) continue;
                const float x0 = ReadFloat(base, quad + 16), x1 = ReadFloat(base, quad + 76);
                float y0 = QuadY(base, quad, 0), y1 = y0;
                for (int k = 1; k < 4; k++) { y0 = std::min(y0, QuadY(base, quad, k)); y1 = std::max(y1, QuadY(base, quad, k)); }
                if (!std::isfinite(x0) || !std::isfinite(x1) || !std::isfinite(y0) || !std::isfinite(y1)) continue;
                if (std::fabs(x1 - x0) < 200.0f || y1 - y0 > 6.0f || y0 < top - 8.0f || y1 > bottom + 8.0f) continue;
                found[foundN++] = quad;
                sum += (y0 + y1) * 0.5f;
            }
            frameMid = foundN ? sum / float(foundN) : 0.0f;
            for (int k = 0; k < 8; k++) g_frameQuads[k].store(k < foundN ? found[k] : 0u);
            static int told = 0;
            if (told < 6)
            {
                told++;
                printf("menu: the team choice's gold frame: %d lines around y %.1f (rows %.0f..%.0f), row %d selected:", foundN, frameMid, top, bottom, selected);
                for (int k = 0; k < foundN; k++) printf(" %08X y %.1f..%.1f x %.0f..%.0f", found[k], QuadY(base, found[k], 0), QuadY(base, found[k], 3), ReadFloat(base, found[k] + 16), ReadFloat(base, found[k] + 76));
                printf("\n"); fflush(stdout);
            }
        }
        if (!foundN) { g_frameDelta.store(0.0f); return; }
        // the row the lines frame as the game left them: the one whose middle is nearest theirs
        int framed = -1; float best = 1e9f;
        for (size_t i = 0; i < boxes.size(); i++)
            if (boxes[i].Height() > 0.0f && std::fabs(boxes[i].CentreY() - frameMid) < best) { best = std::fabs(boxes[i].CentreY() - frameMid); framed = int(i); }
        g_frameDelta.store(framed < 0 ? 0.0f : boxes[size_t(selected)].CentreY() - boxes[size_t(framed)].CentreY());
    }
    // around a quad's draw: moved for the draw, put back after
    struct FrameShift
    {
        uint8_t* base = nullptr; uint32_t quad = 0; uint32_t saved[4] = {};
        FrameShift(uint8_t* b, uint32_t q)
        {
            if (!q || GetTickCount64() > g_frameUntil.load()) return;
            const float delta = g_frameDelta.load();
            if (delta == 0.0f) return;
            bool listed = false;
            for (const auto& f : g_frameQuads) listed = listed || f.load() == q;
            if (!listed) return;
            base = b; quad = q;
            for (int k = 0; k < 4; k++)
            {
                saved[k] = Guest::Read32(base, quad + 20 + 20 * uint32_t(k));
                const float y = QuadY(base, quad, k) + delta;
                uint32_t w; memcpy(&w, &y, 4);
                Guest::Write32(base, quad + 20 + 20 * uint32_t(k), w);
            }
        }
        ~FrameShift() { if (base) for (int k = 0; k < 4; k++) Guest::Write32(base, quad + 20 + 20 * uint32_t(k), saved[k]); }
    };
}

// A quad of the front end drawn (an FEImage's draw calls this for each of
// its quads): noted, for the pictures the pointer can find.
#ifdef COD3_MP
PPC_FUNC(sub_824D7E60)
{
    NoteDrawn(ctx.r3.u32);
    FrameShift shift(base, ctx.r3.u32);   // 0.5.649: the team choice's gold frame
    __imp__sub_824D7E60(ctx, base);
}

// A menu system's update: the base one, which every system's own calls.
PPC_FUNC(sub_824D5FC0)
{
    SystemUpdate(ctx, base);
    __imp__sub_824D5FC0(ctx, base);
}
#else
PPC_FUNC(sub_824BFDD8)
{
    NoteDrawn(ctx.r3.u32);
    __imp__sub_824BFDD8(ctx, base);
}

// A menu system's update (both systems come here each frame).
PPC_FUNC(sub_824EE2E0)
{
    SystemUpdate(ctx, base);
    __imp__sub_824EE2E0(ctx, base);
}
#endif

namespace
{
// 0.5.623: SPECTATOR AS THE TEAM CHOICE'S OWN ENTRY (user, on 0.5.615's row drawn
// over it: "it needs to look like an exact game setting"). The team choice
// (82084854) keeps AUTO SELECT, ALLIES and AXIS as menu entries: an entry (its
// +16 the FEText it shows) in the array at the menu's +4, 3 of them (+38). The
// entries and their texts are owned by the menu's lists at +88 and +96, which
// its teardown (vtable +4, sub_823ABDD8) deletes; the array is only cleared.
// So a fourth entry is made, never the game's to delete: a copy of AXIS's entry
// and of its text, the text's string made "SPECTATOR", in an array of our own
// with the three of the game's; the game lays it out, draws it in its own font,
// highlights it and moves the cross onto it as onto the others. Taking it is
// the spectator's seat (spectator.cpp's FilterPad: A on it never reaches the
// game). While the team choice is not up, the game's own array and count are
// put back. COD3_NOSEATENTRY=1: none.
uint32_t g_seatArray = 0, g_seatEntryAt = 0, g_seatTextAt = 0, g_seatString = 0, g_seatChars = 0;
uint32_t g_seatMenu = 0, g_seatOrigArray = 0;
int16_t g_seatOrigCount = 0;
std::atomic<bool> g_seatSelected{ false };
std::atomic<int> g_seatRowAsked{ -1 };   // 0.5.640: the row the cross asks for (spectator.cpp)
std::atomic<int> g_teamSelected{ -1 };
std::atomic<uint64_t> g_seatSeenAt{ 0 };
// 0.5.625: 624's probe: the menu's texts are drawn from its layer (the menu's
// +72, also the front end's manager's +372): its +12 the array of the widgets
// it draws, +16 their count and room (int16 each; AUTO SELECT ... AXIS's texts
// in it, AXIS's the sixth). So the copy of AXIS's text is put in that list too,
// in an array of our own (the game's widgets and ours, room for 64), the
// game's array and count put back when the team choice is not up. Where the
// copy is drawn: each of its numbers that differs between ALLIES's text and
// AXIS's (its place) is AXIS's and the step once more, while the game left it
// at AXIS's; the same for the entry (its box, which the pointer finds).
uint32_t g_seatLayer = 0, g_seatWidgets = 0, g_seatOrigWidgets = 0, g_seatOrigWidgetWord = 0;
void PlaceBelow(uint8_t* base, uint32_t copy, uint32_t above, uint32_t aboveThat, uint32_t words)
{
    for (uint32_t k = 0; k < words; k++)
    {
        const uint32_t a = Guest::Read32(base, above + 4 * k), b = Guest::Read32(base, aboveThat + 4 * k);
        if (a == b || Guest::Read32(base, copy + 4 * k) != a) continue;
        float fa, fb; memcpy(&fa, &a, 4); memcpy(&fb, &b, 4);
        if (!std::isfinite(fa) || !std::isfinite(fb) || std::fabs(fa) > 4096.0f || std::fabs(fb) > 4096.0f) continue;
        if (std::fabs(fa) <= 1.0f && std::fabs(fb) <= 1.0f) continue;   // colours, flags: not a place
        const float f = fa + (fa - fb);
        uint32_t w; memcpy(&w, &f, 4);
        Guest::Write32(base, copy + 4 * k, w);
    }
}
void SeatWidget(uint8_t* base, uint32_t menu)
{
    if (Guest::Read32(base, menu + MenuEntries) != g_seatArray) return;
    const uint32_t allies = Guest::Read32(base, g_seatArray + 4), axis = Guest::Read32(base, g_seatArray + 8);
    if (!Valid(allies) || !Valid(axis)) return;
    const uint32_t alliesText = Guest::Read32(base, allies + EntryWidget), axisText = Guest::Read32(base, axis + EntryWidget);
    if (!Valid(alliesText) || !Valid(axisText)) return;
    PlaceBelow(base, g_seatEntryAt, axis, allies, 0x100 / 4);
    PlaceBelow(base, g_seatTextAt, axisText, alliesText, 0x80 / 4);
    {
        // 0.5.627: a second after, once: is the copy's string still ours, and
        // what the text's +100 (a neighbour of its string) holds
        static uint64_t firstAt = 0; static bool told = false;
        if (!firstAt) firstAt = GetTickCount64();
        if (!told && GetTickCount64() - firstAt > 1000)
        {
            told = true;
            const uint32_t str = Guest::Read32(base, g_seatTextAt + 28), nb = Guest::Read32(base, g_seatTextAt + 100);
            printf("menu: SPECTATOR a second on: the text's +28 %08X (ours %08X): %08X %08X %08X %08X; its +100 %08X:", str, g_seatString,
                   Guest::Read32(base, str), Guest::Read32(base, str + 4), Guest::Read32(base, str + 8), Guest::Read32(base, str + 12), nb);
            if (Valid(nb)) for (uint32_t k = 0; k < 8; k++) printf(" %08X", Guest::Read32(base, nb + 4 * k));
            printf("\n"); fflush(stdout);
        }
    }
    const uint32_t layer = Guest::Read32(base, menu + 72);
    if (!Valid(layer)) return;
    const uint32_t widgets = Guest::Read32(base, layer + 12);
    if (widgets == g_seatWidgets && g_seatWidgets) return;   // ours in place
    const uint32_t word = Guest::Read32(base, layer + 16);
    const uint32_t n = word >> 16, room = word & 0xFFFF;
    if (!Valid(widgets) || n == 0 || n > 60 || room < n || room > 4096) return;
    // every one a widget (a vtable of the front end's), so n is their count
    bool found = false;
    for (uint32_t i = 0; i < n; i++)
    {
        const uint32_t w = Guest::Read32(base, widgets + 4 * i);
        const uint32_t vt = Valid(w) ? Guest::Read32(base, w) : 0;
        if (vt < 0x82000000u || vt - 0x82000000u >= (1u << 20)) return;   // a vtable of the front end's (the first MB)
        if (w == axisText) found = true;
    }
    static int told = 0;
    if (!found) { if (told < 3) { told++; printf("menu: the team choice's layer %08X holds no AXIS text (%u widgets): SPECTATOR not drawn\n", layer, n); fflush(stdout); } return; }
    if (!g_seatWidgets && !(g_seatWidgets = Guest::AllocatePhysical(0x100))) return;
    for (uint32_t i = 0; i < n; i++) Guest::Write32(base, g_seatWidgets + 4 * i, Guest::Read32(base, widgets + 4 * i));
    Guest::Write32(base, g_seatWidgets + 4 * n, g_seatTextAt);
    g_seatLayer = layer; g_seatOrigWidgets = widgets; g_seatOrigWidgetWord = word;
    Guest::Write32(base, layer + 12, g_seatWidgets);
    // count and room both n + 1 when they were equal; else only the first
    Guest::Write32(base, layer + 16, ((n + 1) << 16) | (room == n ? n + 1 : std::max(room, n + 1)));
    if (told < 6) { told++; printf("menu: SPECTATOR's text put in the team choice's layer %08X (%u widgets, room %u, now %u)\n", layer, n, room, n + 1); fflush(stdout); }
}
void SeatEntry(uint8_t* base, uint32_t menu)
{
    static const bool off = getenv("COD3_NOSEATENTRY") != nullptr;
    if (off) return;
    g_seatSeenAt.store(GetTickCount64());
    const uint32_t array = Guest::Read32(base, menu + MenuEntries);
    const int16_t count = Read16s(base, menu + MenuCount);
    if (array != g_seatArray)
    {
        // the game's own array (first time, or the menu made itself again)
        if (count != 3 || !Valid(array)) return;
        const uint32_t axis = Guest::Read32(base, array + 8);
        const uint32_t axisText = Valid(axis) ? Guest::Read32(base, axis + EntryWidget) : 0;
        if (!Valid(axisText) || Guest::Read32(base, axisText) != FETextVtable) return;
        if (!g_seatArray)
        {
            g_seatArray = Guest::AllocatePhysical(0x40);
            g_seatEntryAt = Guest::AllocatePhysical(0x100);
            g_seatTextAt = Guest::AllocatePhysical(0x100);
            g_seatString = Guest::AllocatePhysical(0x40);
            g_seatChars = Guest::AllocatePhysical(0x20);
            if (!g_seatArray || !g_seatEntryAt || !g_seatTextAt || !g_seatString || !g_seatChars) { g_seatArray = 0; return; }
        }
        // the copies, made again from AXIS each time the game's array is new
        memcpy(base + g_seatEntryAt, base + axis, 0x100);
        memcpy(base + g_seatTextAt, base + axisText, 0x80);
        Guest::Write32(base, g_seatEntryAt + EntryWidget, g_seatTextAt);
        const uint32_t axisString = Guest::Read32(base, axisText + 28);
        if (axisString >= 0x40000000u && axisString < 0xC0000000u) memcpy(base + g_seatString, base + axisString, 0x10);
        static const char label[] = "SPECTATOR";
        // 0.5.627: 626's log: the game's string holds its characters itself
        // (4AF96D7C 00140004 00010000 "AXIS": +0 its characters = its +12, +4
        // room 20 and length 4, +8 1, +12 the characters): ours the same
        // (0x20 bytes then: room 20)
        memset(base + g_seatString, 0, 0x20);
        Guest::Write32(base, g_seatString, g_seatString + 12);
        Guest::Write16(base, g_seatString + 4, 20);                           // room
        Guest::Write16(base, g_seatString + 6, uint16_t(sizeof label - 1));   // length
        Guest::Write32(base, g_seatString + 8, Guest::Read32(base, axisString + 8));
        memcpy(base + g_seatString + 12, label, sizeof label);
        Guest::Write32(base, g_seatTextAt + 28, g_seatString);
        // 0.5.626: 625's picture: the fourth row drawn in the game's place and
        // font, but reading AXIS: the words drawn are held elsewhere too. Every
        // word of the copy that points at "AXIS" (its characters, or a string
        // whose first word points at them; ASCII or UTF-16) points at SPECTATOR
        // in our own copy instead; what was found is told once
        {
            static uint32_t pool = 0, poolUsed = 0;
            if (!pool) pool = Guest::AllocatePhysical(0x400);
            auto isAxis = [&](uint32_t at) -> int {
                if (!Valid(at)) return 0;
                if (!memcmp(base + at, "AXIS", 5)) return 1;
                static const uint8_t wide[] = { 0, 'A', 0, 'X', 0, 'I', 0, 'S', 0, 0 };
                if (!memcmp(base + at, wide, sizeof wide)) return 2;
                return 0;
            };
            auto chars = [&](int kind) -> uint32_t {
                if (!pool || poolUsed + 0x40 > 0x400) return 0;
                const uint32_t at = pool + poolUsed; poolUsed += 0x40;
                memset(base + at, 0, 0x40);
                for (int i = 0; label[i]; i++) { if (kind == 1) base[at + i] = uint8_t(label[i]); else base[at + 2 * i + 1] = uint8_t(label[i]); }
                return at;
            };
            static int told = 0;
            for (uint32_t k = 0; k < 0x80 / 4; k++)
            {
                if (k == 7) continue;   // +28: made above
                const uint32_t v = Guest::Read32(base, g_seatTextAt + 4 * k);
                if (int kind = isAxis(v))
                {
                    const uint32_t c = chars(kind);
                    if (c) Guest::Write32(base, g_seatTextAt + 4 * k, c);
                    if (told < 8) { told++; printf("menu: SPECTATOR: the text's +%u held AXIS's characters (%s)\n", 4 * k, kind == 1 ? "ascii" : "utf-16"); fflush(stdout); }
                }
                else if (Valid(v) && (kind = isAxis(Guest::Read32(base, v))))
                {
                    const uint32_t c = chars(kind), obj = c ? chars(1) : 0;
                    if (!obj) continue;
                    memcpy(base + obj, base + v, 0x20);
                    Guest::Write32(base, obj, c);
                    for (uint32_t h = 4; h < 0x10; h += 2) if (Guest::Read16(base, obj + h) == 4) Guest::Write16(base, obj + h, uint16_t(sizeof label - 1));
                    Guest::Write32(base, g_seatTextAt + 4 * k, obj);
                    if (told < 8) { told++; printf("menu: SPECTATOR: the text's +%u held a string of AXIS's (%s): %08X %08X %08X %08X\n", 4 * k, kind == 1 ? "ascii" : "utf-16",
                        Guest::Read32(base, v), Guest::Read32(base, v + 4), Guest::Read32(base, v + 8), Guest::Read32(base, v + 12)); fflush(stdout); }
                }
            }
            if (poolUsed == 0 && told < 8) { told++; printf("menu: SPECTATOR: no other AXIS found in the text; its +28 string: %08X %08X %08X %08X\n",
                Guest::Read32(base, axisString), Guest::Read32(base, axisString + 4), Guest::Read32(base, axisString + 8), Guest::Read32(base, axisString + 12)); fflush(stdout); }
            poolUsed = 0;   // the next copy (made again) reuses the pool
        }
        g_seatMenu = menu; g_seatOrigArray = array; g_seatOrigCount = count;
        for (uint32_t i = 0; i < 3; i++) Guest::Write32(base, g_seatArray + 4 * i, Guest::Read32(base, array + 4 * i));
        Guest::Write32(base, g_seatArray + 12, g_seatEntryAt);
        Guest::Write32(base, menu + MenuEntries, g_seatArray);
        Guest::Write16(base, menu + MenuCount, 4);
        static int told = 0;
        if (told < 6) { told++; printf("menu: the team choice's fourth entry, SPECTATOR (entry %08X, text %08X: copies of AXIS's %08X / %08X)\n", g_seatEntryAt, g_seatTextAt, axis, axisText); fflush(stdout); }
    }
    else if (count == 3)
    {
        // the game counted its own three again (it made itself again in our array)
        Guest::Write32(base, g_seatArray + 12, g_seatEntryAt);
        Guest::Write16(base, menu + MenuCount, 4);
    }
    SeatWidget(base, menu);
    // 0.5.640 (user: "in the menus you cannot actually scroll down to spectator
    // and click it"): the seat's box is SPECTATOR's row now (since 0.5.623 it
    // was never set: the row drawn by us went), for the pointer and the click
    {
        const std::vector<Box> boxes = EntryBoxes(base, 0, menu, false, {});
        std::lock_guard<std::mutex> lock(g_seatMutex);
        g_seat = boxes.size() >= 4 && boxes[3].Height() > 0.0f ? boxes[3] : Box{};
        if (g_seat.Height() > 0.0f && g_seat.Width() < 120.0f) g_seat.x1 = g_seat.x0 + 120.0f;
    }
    const int16_t selected = Read16s(base, menu + MenuSelected);
    // the cross's down from AXIS and up from SPECTATOR (spectator.cpp asks):
    // the game's own walk knows only its three
    if (const int row = g_seatRowAsked.exchange(-1); row == 2 || row == 3)
    {
        Guest::Write16(base, menu + MenuSelected, uint16_t(row));
        static int told = 0;
        if (told < 10) { told++; printf("menu: the team choice's row %d by the cross (was %d)\n", row, selected); fflush(stdout); }
    }
    // 0.5.650 (user, 649: on SPECTATOR "it still shows the team flag for axis ...
    // it should really just be like how auto assign looks blank/darked out"):
    // 647/648's probes: the side's soldier and flag are shown by the menu's
    // +0xF0 / +0xF4 (1.0 / 0.1 on ALLIES and AXIS, 0 / 0 on AUTO SELECT); the
    // game sets them on its own rows only, so SPECTATOR (ours) kept AXIS's.
    // On SPECTATOR they are 0 as on AUTO SELECT; back up to AXIS, AXIS's again.
    {
        static int16_t was = -1;
        const int16_t now = Read16s(base, menu + MenuSelected);
        auto put = [&](float a, float b) {
            uint32_t w; memcpy(&w, &a, 4); Guest::Write32(base, menu + 0xF0, w);
            memcpy(&w, &b, 4); Guest::Write32(base, menu + 0xF4, w);
        };
        if (now == 3) put(0.0f, 0.0f);
        else if (was == 3 && now == 2) put(1.0f, 0.1f);
        was = now;
    }
    // 0.5.647: PROBE of the gold frame (user, 646: "the little yellow box around
    // allies never moves to axis, spectator or auto assign"; screenshots: the
    // frame stays on ALLIES while the soldier shown follows the selection; no
    // front end quad of it in the dumps). Each time the highlighted row
    // changes (12 times at most), every word that differs from the dump before
    // in the menu (+0..+0x200), its four entries (+0..+0x40) and their texts
    // (+0..+0xA0): what follows the selection, and what should.
    {
        static uint32_t seenMenu = 0;
        static int16_t lastSel = -2;
        static int dumps = 0;
        static uint8_t prevMenu[0x200], prevEntry[4][0x40], prevText[4][0xA0];
        const int16_t sel = Read16s(base, menu + MenuSelected);
        if (menu != seenMenu) { seenMenu = menu; lastSel = -2; }
        if (sel != lastSel && dumps < 12)
        {
            const bool first = lastSel == -2;
            printf("menu: frame probe: team choice row %d (was %d)%s\n", sel, lastSel, first ? ", first look: all words" : "");
            auto diff = [&](const char* what, uint32_t at, uint8_t* prev, uint32_t size) {
                if (!Valid(at)) return;
                int shown = 0;
                for (uint32_t o = 0; o < size; o += 4)
                {
                    uint32_t was; memcpy(&was, prev + o, 4);
                    const uint32_t now = Guest::Read32(base, at + o);
                    uint32_t nowBe = now;
                    if (!first && was == nowBe) continue;
                    if (shown % 8 == 0) printf("%smenu:   %s %08X:", shown ? "\n" : "", what, at);
                    printf(" +%X %08X", o, now);
                    shown++;
                    memcpy(prev + o, &nowBe, 4);
                }
                if (shown) printf("\n");
            };
            diff("menu", menu, prevMenu, sizeof prevMenu);
            // 0.5.648: 647's probe: no entry and no text changes with the row;
            // the menu's other objects (+0x3C, +0x54) and its layer's widgets
            // (+0x48: +12 their array, +16 count): each one's place and state
            {
                static uint8_t prevObj[2][0x100];
                const uint32_t objs[2] = { Guest::Read32(base, menu + 0x3C), Guest::Read32(base, menu + 0x54) };
                diff("object +3C", objs[0], prevObj[0], sizeof prevObj[0]);
                diff("object +54", objs[1], prevObj[1], sizeof prevObj[1]);
                static uint8_t prevWidget[40][0x70];
                const uint32_t layer = Guest::Read32(base, menu + 0x48);
                const uint32_t widgets = Valid(layer) ? Guest::Read32(base, layer + 12) : 0;
                const uint32_t n = Valid(layer) ? std::min<uint32_t>(Guest::Read32(base, layer + 16) >> 16, 40u) : 0;
                if (first) printf("menu:   layer %08X: %u widgets at %08X\n", layer, n, widgets);
                for (uint32_t i = 0; Valid(widgets) && i < n; i++)
                {
                    char what[24];
                    snprintf(what, sizeof what, "widget %u", i);
                    diff(what, Guest::Read32(base, widgets + 4 * i), prevWidget[i], sizeof prevWidget[i]);
                }
            }
            const uint32_t array = Guest::Read32(base, menu + MenuEntries);
            for (int i = 0; i < 4 && Valid(array); i++)
            {
                const uint32_t entry = Guest::Read32(base, array + 4 * uint32_t(i));
                char what[16];
                snprintf(what, sizeof what, "entry %d", i); diff(what, entry, prevEntry[i], sizeof prevEntry[i]);
                snprintf(what, sizeof what, "text %d", i); diff(what, Valid(entry) ? Guest::Read32(base, entry + EntryWidget) : 0, prevText[i], sizeof prevText[i]);
            }
            fflush(stdout);
            lastSel = sel;
            dumps++;
        }
    }
    g_teamSelected.store(Read16s(base, menu + MenuSelected));
    g_seatSelected.store(Read16s(base, menu + MenuSelected) == 3);
}
// While the team choice is not up: the game's own array and count back.
void SeatEntryAway(uint8_t* base)
{
    if (!g_seatMenu || GetTickCount64() - g_seatSeenAt.load() < 300) return;
    if (g_seatLayer && Guest::Read32(base, g_seatLayer + 12) == g_seatWidgets)
    {
        Guest::Write32(base, g_seatLayer + 12, g_seatOrigWidgets);
        Guest::Write32(base, g_seatLayer + 16, g_seatOrigWidgetWord);
    }
    g_seatLayer = 0;
    if (Guest::Read32(base, g_seatMenu + MenuEntries) == g_seatArray)
    {
        Guest::Write32(base, g_seatMenu + MenuEntries, g_seatOrigArray);
        Guest::Write16(base, g_seatMenu + MenuCount, uint16_t(g_seatOrigCount));
        if (Read16s(base, g_seatMenu + MenuSelected) >= g_seatOrigCount) Guest::Write16(base, g_seatMenu + MenuSelected, 0);
    }
    g_seatMenu = 0;
    g_seatSelected.store(false);
    g_teamSelected.store(-1);
    { std::lock_guard<std::mutex> lock(g_seatMutex); g_seat = Box{}; }   // 0.5.640
}

// 0.5.614: once a menu class (40 at most a run), what it shows (user, 613:
// the lobby's screens could not be pointed at: their items are not menu
// entries): its fields, its entries' boxes, each text it or an object of it
// holds (visible, where, what it reads) and each picture drawn, so they can
// be made pointable from a played log. "menu: shape" lines.
void DumpMenu(uint8_t* base, uint32_t system, uint32_t menu, bool dialog)
{
    static uint32_t seen[40] = {};
    static int seenN = 0;
    const uint32_t vtable = Guest::Read32(base, menu);
    for (int i = 0; i < seenN; i++) if (seen[i] == vtable) return;
    if (seenN >= 40) return;
    seen[seenN++] = vtable;
    printf("menu: shape of %08X (vtable %08X%s): first %d selected %d count %d shown %d flags %04X locked %u\n", menu, vtable, dialog ? ", a dialog" : "",
           Read16s(base, menu + MenuFirstShown), Read16s(base, menu + MenuSelected), Read16s(base, menu + MenuCount), Read16s(base, menu + MenuShown),
           Guest::Read16(base, menu + 42), Guest::Read8(base, menu + MenuLocked));
    const std::vector<Box> boxes = EntryBoxes(base, system, menu, dialog, {});
    for (size_t i = 0; i < boxes.size(); i++)
        if (boxes[i].Height() > 0.0f) printf("menu:   entry %zu at %.0f,%.0f..%.0f,%.0f\n", i, boxes[i].x0, boxes[i].y0, boxes[i].x1, boxes[i].y1);
    // 0.5.643: what each entry reads (the pause menu's were never in a log)
    {
        const uint32_t entries = Guest::Read32(base, menu + MenuEntries);
        const int count = Read16s(base, menu + MenuCount);
        for (int i = 0; Valid(entries) && i < count && i < 64; i++)
        {
            const uint32_t entry = Guest::Read32(base, entries + uint32_t(i) * 4);
            const uint32_t widget = Valid(entry) ? Guest::Read32(base, entry + EntryWidget) : 0;
            printf("menu:   entry %d reads \"%s\"%s\n", i, Valid(widget) ? TextOf(base, widget).c_str() : "", Valid(entry) && Guest::Read8(base, entry + EntryHidden) ? " (hidden)" : "");
        }
    }
    int texts = 0;
    auto text = [&](const char* where, uint32_t o, uint32_t w) {
        if (texts >= 40) return;
        texts++;
        printf("menu:   text %s+%u %08X vis %u at %.0f,%.0f width %.0f align %u: \"%s\"\n", where, o, w, Guest::Read8(base, w + 4), ReadFloat(base, w + 44), ReadFloat(base, w + 48),
               ReadFloat(base, w + 8), (Guest::Read16(base, w + 108) >> 4) & 3, TextOf(base, w).c_str());
    };
    for (uint32_t o = 4; o < 512; o += 4)
    {
        const uint32_t p = Guest::Read32(base, menu + o);
        if (!Valid(p)) continue;
        const uint32_t pv = Guest::Read32(base, p);
        if (pv == FETextVtable) { text("", o, p); continue; }
        if (pv < VtablesFrom || pv >= VtablesTo || pv == FEImageVtable) continue;
        for (uint32_t q = 4; q < 160; q += 4)
        {
            const uint32_t c = Guest::Read32(base, p + q);
            if (Valid(c) && Guest::Read32(base, c) == FETextVtable) { char where[24]; snprintf(where, sizeof where, "+%u>", o); text(where, q, c); }
        }
    }
    for (const Box& b : Pictures(base, menu)) printf("menu:   picture at %.0f,%.0f..%.0f,%.0f\n", b.x0, b.y0, b.x1, b.y1);
    fflush(stdout);
}

// 0.5.617: on a menu of no entries of its own (the lobby's lists: 616's play log), each
// small number of its first 1 KB that changes (as a halfword or a word, both values
// under 256), to find where such a list keeps its highlighted row. "menu: field"
// lines, at most 400 a run; COD3_MENUFIELDS=0 off.
void WatchFields(uint8_t* base, uint32_t menu)
{
    static const bool off = [] { const char* t = getenv("COD3_MENUFIELDS"); return t && t[0] == '0'; }();
    static uint32_t watched = 0;
    static uint8_t last[1024];
    static int told = 0;
    if (off || told >= 400) return;
    if (menu != watched) { watched = menu; memcpy(last, base + menu, sizeof last); return; }
    int lines = 0;
    for (uint32_t o = 0; o < sizeof last && lines < 8; o += 2)
    {
        const uint16_t was = uint16_t((last[o] << 8) | last[o + 1]);
        const uint16_t now = Guest::Read16(base, menu + o);
        if (was == now || was >= 256 || now >= 256) continue;
        // a word's low half is checked as the word too: skip its high half's echo
        printf("menu: field %08X (vtable %08X) +%u: %u -> %u\n", menu, Guest::Read32(base, menu), o, was, now);
        lines++; told++;
    }
    memcpy(last, base + menu, sizeof last);
    // 0.5.618: the objects it holds too (617: Xbox Live's option rows kept no
    // row number in the menu itself): their first 256 bytes, 24 of them at most
    static uint32_t child[24] = {};
    static uint8_t childLast[24][256];
    static uint32_t childOf = 0;
    if (childOf != menu)
    {
        childOf = menu;
        int n = 0;
        for (uint32_t o = 4; o < 1024 && n < 24; o += 4)
        {
            const uint32_t p = Guest::Read32(base, menu + o);
            if (!Valid(p)) continue;
            const uint32_t pv = Guest::Read32(base, p);
            if (pv < VtablesFrom || pv >= VtablesTo || pv == FETextVtable || pv == FEImageVtable) continue;
            child[n] = p; memcpy(childLast[n], base + p, 256); n++;
        }
        for (; n < 24; n++) child[n] = 0;
    }
    else
        for (int i = 0; i < 24 && child[i]; i++)
        {
            for (uint32_t o = 0; o < 256 && lines < 12; o += 2)
            {
                const uint16_t was = uint16_t((childLast[i][o] << 8) | childLast[i][o + 1]);
                const uint16_t now = Guest::Read16(base, child[i] + o);
                if (was == now || was >= 256 || now >= 256) continue;
                printf("menu: field %08X's object %08X (vtable %08X) +%u: %u -> %u\n", menu, child[i], Guest::Read32(base, child[i]), o, was, now);
                lines++; told++;
            }
            memcpy(childLast[i], base + child[i], 256);
        }
    if (lines) fflush(stdout);
}

}   // namespace

bool MenuMouse::TeamSeatSelected() { return TeamMenuUp() && g_seatSelected.load(); }
int MenuMouse::TeamSelected() { return TeamMenuUp() ? g_teamSelected.load() : -1; }
void MenuMouse::AskTeamRow(int row) { g_seatRowAsked.store(row); }
void MenuMouse::Tick(uint8_t* base)
{
    SeatEntryAway(base);
#ifdef COD3_MP
    // 0.5.645: the team choice put up by SWITCH SIDE has gone (a side taken,
    // SPECTATOR, or back): the pause flag it kept set is cleared, so you move again
    if (g_teamFromPauseOpen && g_teamAskedAt == 0 && GetTickCount64() - g_teamUpAt.load() > 400)
    {
        g_teamFromPauseOpen = false;
        Guest::Write8(base, 0x82C73024u, 0);
        printf("menu: SWITCH SIDE's team choice gone: the pause flag cleared\n"); fflush(stdout);
    }
    // 0.5.646 (user, 645: "when in game and picking spectator it still doesnt let
    // you pause the game with the start button ... or the escape button"): 645's
    // log: Escape's Start reached the game in the seat, and the game puts up no
    // pause menu for a spectator (state 1). So it is put up here, as the game has
    // it: the pause menu (82083934) of the in-game system the team choice lives
    // in (645: system 8268A950, menu 0; found by its class), with the pause flag
    // set; its own B / RESUME then toggles the flag off and takes it down.
    if (g_pauseAsked.exchange(false))
    {
        int index = -1;
        const uint32_t menus = Valid(g_teamSystem) ? Guest::Read32(base, g_teamSystem + SystemMenus) : 0;
        for (int i = 0; Valid(menus) && i < 64 && index < 0; i++)
        {
            const uint32_t m = Guest::Read32(base, menus + uint32_t(i) * 4);
            if (Valid(m) && Guest::Read32(base, m) == 0x82083934u) index = i;
        }
        if (index >= 0 && int32_t(Guest::Read32(base, g_teamSystem + SystemActive)) < 0)
        {
            Guest::Write8(base, 0x82C73024u, 1);
            Guest::Write32(base, g_teamSystem + SystemActive, uint32_t(index));
            printf("menu: the pause menu put up in the spectator's seat (system %08X, menu %d)\n", g_teamSystem, index); fflush(stdout);
        }
        else { printf("menu: the pause menu asked for in the seat: %s\n", index < 0 ? "not found (no team choice seen yet)" : "a menu is up already"); fflush(stdout); }
    }
#endif
}

namespace
{
void SystemUpdate(PPCContext& ctx, uint8_t* base)
{
    static const bool off = getenv("COD3_NOMENUMOUSE") != nullptr;
    // A dialog (DialogMenuSystem, whose update comes here too) is in front
    // of the menu it was opened over, which goes on updating: while one is
    // up, only its buttons are pointed at.
    static uint64_t dialogUpAt = 0;
    const uint32_t system = ctx.r3.u32;
    if (g_frameHook) g_frameHook(ctx, base);
    SeatEntryAway(base);   // 0.5.623
    // 0.5.608: the menus taken down while spectating, and put back after
    if (Valid(system))
    {
        if (g_restore.exchange(false))
        {
            for (int i = 0; i < g_takenN; i++)
                if (Valid(g_taken[i].system) && int32_t(Guest::Read32(base, g_taken[i].system + SystemActive)) < 0)
                {
                    Guest::Write32(base, g_taken[i].system + SystemActive, uint32_t(g_taken[i].active));
                    printf("menu: system %08X's menu %d put back\n", g_taken[i].system, g_taken[i].active); fflush(stdout);
                }
            g_takenN = 0;
        }
        // 0.5.643 (user, 642: in the seat "both pausing on controller and hitting
        // escape never pulled up the pause menu"): only the menus the game puts
        // up for a spectator to join by (the team choice 82084854, the class
        // choice 82084644, the spawn screen 82085174) are taken down; the pause
        // menu and its questions are left up and pointed at as ever
        const uint32_t hiddenMenu = g_hidden.load() ? ActiveMenu(base, system) : 0;
        const uint32_t hiddenClass = Valid(hiddenMenu) ? Guest::Read32(base, hiddenMenu) : 0;
#ifdef COD3_MP
        const bool joinMenu = hiddenClass == 0x82084854u || hiddenClass == 0x82084644u || hiddenClass == 0x82085174u;
#else
        const bool joinMenu = true;
#endif
        if (g_hidden.load() && (!Valid(hiddenMenu) || joinMenu))
        {
            if (const uint32_t menu = ActiveMenu(base, system); Valid(menu))
            {
                const int32_t active = int32_t(Guest::Read32(base, system + SystemActive));
                bool known = false;
                for (int i = 0; i < g_takenN; i++) if (g_taken[i].system == system) { g_taken[i].active = active; known = true; }
                if (!known && g_takenN < 8) g_taken[g_takenN++] = { system, active };
                Guest::Write32(base, system + SystemActive, 0xFFFFFFFFu);
                static int told = 0;
                if (told < 60) { told++; printf("menu: system %08X's menu %d (vtable %08X) taken down while spectating\n", system, active, Guest::Read32(base, menu)); fflush(stdout); }
            }
            return;
        }
    }
    if (Valid(system) && !off)
    {
        {
            if (const uint32_t menu = ActiveMenu(base, system); Valid(menu))
            {
#ifdef COD3_MP
                // 0.5.644 (user, 643: "when you click to change teams mid games it
                // doesnt pull up that second menu ... the options to choose, the
                // opposite team, auto assign, allies, axis, and spectator"): the
                // pause menu's (82083934) SWITCH SIDE asks only YES / NO (643's log:
                // 82071504). Taken (spectator.cpp keeps its A from the game), the
                // pause menu goes (its system's menu -1 and the pause flag, the
                // byte togglepaused sets at 0x82C73024, cleared) and the team
                // choice of the match's start is put up in its place: AUTO SELECT,
                // ALLIES, AXIS and SPECTATOR, which the game applies as at the start
                if (Guest::Read32(base, menu) == 0x82083934u && g_teamFromPause.exchange(false))
                {
                    const int32_t pauseIndex = int32_t(Guest::Read32(base, system + SystemActive));
                    Guest::Write32(base, system + SystemActive, 0xFFFFFFFFu);
                    // 0.5.645: the pause flag stays set while the team choice is up
                    // (644's log: cleared, the in-game menus stopped updating and the
                    // window took the mouse back for aiming 1-2 s after: the highlight
                    // never seen moving, a click on SPECTATOR never the seat's, the
                    // game's own A on our fourth row leaving you a spectator on the
                    // floor). Cleared when the team choice goes (MenuMouse::Tick).
                    const bool up = MenuMouse::OpenTeamChoice(base);
                    if (up) g_teamFromPauseOpen = true;
                    if (!up) Guest::Write32(base, system + SystemActive, uint32_t(pauseIndex));   // no team choice known: the pause menu stays
                    else g_teamAskedAt = GetTickCount64();
                    printf("menu: SWITCH SIDE: the pause menu (system %08X, menu %d) for the team choice (system %08X, menu %d): %s\n", system, pauseIndex,
                           g_teamSystem, g_teamIndex, up ? "put up" : "not known, the pause menu kept");
                    fflush(stdout);
                    return;
                }
#endif
                const uint64_t now = GetTickCount64();
                const bool dialog = Guest::Read32(base, system) == DialogSystemVtable;
                if (dialog) dialogUpAt = now;
                g_menuUpAt.store(now, std::memory_order_relaxed);
                if (TeamMenuVtable != 0 && Guest::Read32(base, menu) == TeamMenuVtable)
                {
                    g_teamUpAt.store(now, std::memory_order_relaxed);
                    if (g_teamAskedAt) { printf("menu: SWITCH SIDE: the team choice is up and updated %llu ms after\n", (unsigned long long)(now - g_teamAskedAt)); fflush(stdout); g_teamAskedAt = 0; }   // 0.5.644
                    g_teamSystem = system;   // 0.5.610: where it lives, to put it up again
                    g_teamIndex = int32_t(Guest::Read32(base, system + SystemActive));
                    SeatEntry(base, menu);   // 0.5.623: SPECTATOR as the team choice's own fourth entry
                    FrameTrack(base, menu);   // 0.5.649: its gold frame on the row selected
                }
                if (dialog || now - dialogUpAt >= 200)
                {
                    // 0.5.600: each menu that comes up, once (at most 100 a run)
                    static uint32_t lastMenu = 0;
                    static int told = 0;
                    if (menu != lastMenu && told < 100)
                    {
                        lastMenu = menu;
                        told++;
                        printf("menu: up: %08X (vtable %08X%s), %d entries, entry %d highlighted, the mouse %s\n", menu, Guest::Read32(base, menu),
                            dialog ? ", a dialog" : "", Read16s(base, menu + MenuCount), Read16s(base, menu + MenuSelected), Input::MouseHeld() ? "held" : "free");
                        fflush(stdout);
                    }
                    g_menuVtable.store(Guest::Read32(base, menu), std::memory_order_relaxed);
                    g_currentMenu.store(menu, std::memory_order_relaxed);   // 0.5.637
                    {
                        // 0.5.643: the entry highlighted reads SWITCH TEAM (the pause menu's)
                        bool switchTeam = false;
                        const uint32_t entries = Guest::Read32(base, menu + MenuEntries);
                        const int selected = Read16s(base, menu + MenuSelected);
                        if (Valid(entries) && selected >= 0 && selected < Read16s(base, menu + MenuCount))
                        {
                            const uint32_t entry = Guest::Read32(base, entries + uint32_t(selected) * 4);
                            const uint32_t widget = Valid(entry) ? Guest::Read32(base, entry + EntryWidget) : 0;
                            // 0.5.644: 643's log: the pause menu reads SELECT KIT, SWITCH SIDE,
                            // RESPAWN, OPTIONS, EDIT GAME SETTINGS, QUIT
                            const std::string text = Valid(widget) ? TextOf(base, widget) : std::string();
                            switchTeam = text == "SWITCH SIDE" || text == "SWITCH TEAM";
                        }
                        g_switchTeamSelected.store(switchTeam);
                    }
                    if (Read16s(base, menu + MenuCount) == 0)
                    {
                        WatchFields(base, menu);   // 0.5.617
                        // 0.5.620: a list not known yet, with row bars: its row learnt
                        if (!ListOf(Guest::Read32(base, menu)))
                        {
                            bool bars = false;
                            for (const Box& b : Pictures(base, menu)) bars = bars || (b.Width() >= 300.0f && b.Height() >= 16.0f && b.Height() <= 24.0f);
                            if (bars) Learn(base, menu);
                        }
                    }
                    {
                        // the dump a little after it comes up, once its pictures are drawn
                        static uint32_t menuAt[8] = {};
                        static uint64_t firstSeen[8] = {};
                        int k = -1, oldest = 0;
                        for (int i = 0; i < 8; i++) { if (menuAt[i] == menu) k = i; if (firstSeen[i] < firstSeen[oldest]) oldest = i; }
                        if (k < 0) { k = oldest; menuAt[k] = menu; firstSeen[k] = now; }
                        else if (now - firstSeen[k] >= 500) DumpMenu(base, system, menu, dialog);
                    }
                    if (!Input::MouseHeld()) Point(ctx, base, system, menu, dialog);
                }
            }
        }
    }
}
}
