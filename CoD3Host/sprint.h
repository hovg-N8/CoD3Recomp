// 0.5.601: the campaign's own sprint put back (sprint.cpp; ported from
// FrankHUN88453's CoD3Recomp, ac3b2a52). Campaign only.
#pragma once

namespace Sprint
{
    // Sprint held or not (input.cpp: L3 while moving forward, tap to sprint,
    // the sprint key), as often as the input is read: +sprint / -sprint on a
    // change.
    void Hold(bool held);
}
