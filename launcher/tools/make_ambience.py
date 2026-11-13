"""Create original, low-level WWII battle ambience; standard-library only."""
from array import array
from pathlib import Path
import math
import random
import wave

RATE = 22050
SECONDS = 24
COUNT = RATE * SECONDS
rng = random.Random(1944)
left = array('f', [0]) * COUNT
right = array('f', [0]) * COUNT
low = slower = 0.0
for i in range(COUNT):
    low = low * .985 + rng.uniform(-1, 1) * .015
    slower = slower * .997 + low * .003
    air = .022 * low + .035 * slower
    left[i] = air
    right[i] = air * .86 + .003 * rng.uniform(-1, 1)

def add_event(start, duration, pan, kind):
    begin = int(start * RATE)
    length = int(duration * RATE)
    filtered = 0.0
    gain_l = math.sqrt((1 - pan) / 2)
    gain_r = math.sqrt((1 + pan) / 2)
    for j in range(length):
        idx = begin + j
        if idx >= COUNT:
            break
        t = j / RATE
        filtered = filtered * .91 + rng.uniform(-1, 1) * .09
        if kind == 'artillery':
            envelope = (1 - math.exp(-t * 42)) * math.exp(-t * 1.9)
            value = envelope * (.07 * math.sin(2 * math.pi * (42 * t + 9 * (1 - math.exp(-t * 3)))) + filtered * .19)
        else:
            envelope = (1 - math.exp(-t * 1500)) * math.exp(-t * 48)
            value = envelope * (.07 * filtered + .025 * rng.uniform(-1, 1))
        left[idx] += value * gain_l
        right[idx] += value * gain_r

for time, pan in [(1.3,-.7),(6.1,.6),(11.7,-.1),(17.6,.85),(21.4,-.5)]:
    add_event(time, 2.5, pan, 'artillery')
for time in [3.2,4.7,8.2,10.0,14.3,16.2,19.4,22.5]:
    pan = rng.uniform(-.9,.9)
    for shot in range(rng.randrange(3,7)):
        add_event(time + shot * rng.uniform(.085,.12), .23, pan, 'rifle')

peak = max(max(abs(v) for v in left), max(abs(v) for v in right))
gain = min(1.0, .17 / max(peak, .001))
pcm = array('h')
for i in range(COUNT):
    fade = min(1.0, i / (RATE * .35), (COUNT - 1 - i) / (RATE * .35))
    pcm.append(round(left[i] * gain * fade * 32767))
    pcm.append(round(right[i] * gain * fade * 32767))
output = Path(__file__).resolve().parents[1] / 'assets' / 'battle-ambience.wav'
output.parent.mkdir(parents=True, exist_ok=True)
with wave.open(str(output), 'wb') as stream:
    stream.setnchannels(2)
    stream.setsampwidth(2)
    stream.setframerate(RATE)
    stream.writeframes(pcm.tobytes())
print(f'Original ambience: {SECONDS}s, stereo {RATE} Hz, peak {peak*gain:.3f}, {output.stat().st_size:,} bytes')
