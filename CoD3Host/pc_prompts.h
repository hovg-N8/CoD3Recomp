#pragma once

// The button prompts for the keyboard and mouse (pc_prompts.cpp).

#include <cstdint>

namespace PcPrompts
{
    // Once a frame (the swap): the key caps for what the player uses now.
    void Frame(uint8_t* base);
}
