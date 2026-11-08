#pragma once

// The window the finished frames go to: made and pumped on a thread of its
// own; the renderer presents into it from the command thread.

#include <cstdint>

namespace Window
{
    void Open();
    void Close();
    bool IsOpen();

    // Whether the title's own window is the one in front. Keyboard input is
    // only taken when it is, so typing elsewhere is not read as a button.
    bool HasFocus();

    // The window itself, for the few things that need to talk to Windows
    // about it: pinning the pointer inside it and putting it back in the
    // middle. Handed out as a void pointer so nothing else has to include
    // Windows.h to ask.
    void* Handle();
    // (co-op v46) shown and put in front: a copy that started hidden (COD3_COOP_HIDDEN, the checkpoint reload)
    void Reveal();

    // Whether to draw a pointer over the window. It is hidden while the mouse
    // is steering, because a cursor pinned to the middle of the screen is only
    // something to look at.
    void SetCursorHidden(bool hidden);

    // Where the guest resolved its last frame, for the count of frames.
    void SetFrontBuffer(uint32_t address, uint32_t width, uint32_t height);
    // How far the mouse itself moved since the last call, in the counts its
    // device reports. This is the movement of the mouse, not of the pointer:
    // the system's pointer speed and its "enhance pointer precision" never
    // touch it, so a turn in the game is the same turn on every machine.
    // False when the raw input could not be had, and the caller falls back
    // to following the pointer.
    bool TakeRawMouse(long& x, long& y);

    // Wheel notches turned since the last call, positive away from the user.
    int TakeWheel();
}
