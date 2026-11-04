#pragma once
#include <cstdint>
namespace MenuInput {
inline uint16_t Apply(uint16_t buttons,bool titleScreen,bool inLevel,bool space,bool enter) {
    if(inLevel) { if(enter) buttons|=0x0010;return buttons; }
    if(space || enter) {
        buttons &= uint16_t(~(0x1000|0x0010));
        buttons |= titleScreen ? 0x0010 : 0x1000;
    }
    return buttons;
}
}
