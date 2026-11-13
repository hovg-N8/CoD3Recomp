"""Render the launcher's original four-bar score as a looping PCM WAV."""
from array import array
from pathlib import Path
from math import exp, pi, sin
import wave

RATE = 22050
BEAT = 0.75
BAR = BEAT * 4
DURATION = BAR * 8
COUNT = int(RATE * DURATION)
left = array("f", [0]) * COUNT
right = array("f", [0]) * COUNT


def note(midi, start, length, gain, pan, strings):
    frequency = 440 * 2 ** ((midi - 69) / 12)
    offset = round(start * RATE)
    for sample in range(round(length * RATE)):
        t = sample / RATE
        phase = 2 * pi * frequency * t
        if strings:
            envelope = min(t / 0.42, 1, (length - t) / 0.6)
            tone = (sin(phase) + 0.33 * sin(phase * 2.003)
                    + 0.17 * sin(phase * 3.009)) * (0.93 + 0.07 * sin(t * 19))
        else:
            envelope = min(t / 0.018, 1, (length - t) / 0.3) * exp(-2.3 * t)
            tone = sin(phase) + 0.27 * sin(phase * 2.006) + 0.11 * sin(phase * 3.015)
        value = gain * envelope * tone
        index = (offset + sample) % COUNT
        left[index] += value * (1 - pan)
        right[index] += value * pan


chords = [(38, 45, 50, 53), (34, 41, 50, 53), (41, 48, 53, 57), (36, 43, 50, 55)]
melody = [(62, 65, 69), (65, 62, 60), (57, 60, 65), (62, 60, 55),
          (62, 65, 69), (70, 69, 65), (67, 65, 60), (62, 57, 60)]
for bar, pitches in enumerate(melody):
    for index, pitch in enumerate(chords[bar % len(chords)]):
        note(pitch, bar * BAR - 0.22, BAR + 0.48, 0.018 if index == 0 else 0.009,
             0.3 + 0.13 * index, True)
    for pitch, beat in zip(pitches, (0, 1.5, 2.75)):
        note(pitch, bar * BAR + beat * BEAT, 1.8, 0.047, 0.43, False)
        note(pitch, bar * BAR + beat * BEAT + 0.28, 1.8, 0.011, 0.73, False)

samples = array("h")
for l, r in zip(left, right):
    samples.extend((round(l * 32767), round(r * 32767)))
output = Path(__file__).resolve().parents[1] / "assets" / "normandy-score.wav"
with wave.open(str(output), "wb") as wav:
    wav.setparams((2, 2, RATE, 0, "NONE", "not compressed"))
    wav.writeframes(samples.tobytes())
