#pragma once

// Sound leaving the machine.
//
// The title mixes its own audio and hands the finished frame to the console
// once every 5.33 milliseconds: 256 samples for each of six channels, as big
// endian floats, one channel after another rather than interleaved. All this
// does is take that frame and give it to Windows: as 5.1 when the speakers
// have six channels or more (or F11 asks for it), the six channels as they
// are, or folded into two for stereo.
//
// Nothing here decodes anything: the voices the title feeds through the
// console's XMA hardware are decoded by kernel_audio.cpp (FFmpeg) before the
// title mixes them.

#include <cstdint>
#include <string>

namespace Audio
{
    // Opened on the first frame, so a run that never makes a sound never takes
    // the sound device.
    bool Open();
    void Close();

    // One frame from the guest, at the guest address it was submitted at.
    // Returns false if it could not be played, which is not an error worth
    // stopping for: the title is told the frame was accepted either way, since
    // a title that waits for its own audio to drain stops dead.
    bool SubmitFrame(uint32_t guestAddress);

    // How many frames were played and how many had to be dropped because the
    // device had not finished with the ones before.
    void Report();

    // 0.5.653: a WAV file (16-bit PCM, any rate, mono or stereo) played once,
    // mixed into the title's own frames at this gain (CoD2's Webley's
    // gunshot, mp_fixes.cpp). Read once and kept.
    void PlayClip(const std::string& path, float gain);
}
