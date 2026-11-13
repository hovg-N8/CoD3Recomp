"""Mix the original launcher score, recreated effects and voiced fiction."""
from array import array
from math import exp, pi, sin, tanh
from pathlib import Path
import random
import wave

assets = Path(__file__).resolve().parents[1] / "assets"
rate = 22050
duration = 64
count = rate * duration
rng = random.Random(1944)
mix = array("f", [0]) * (count * 2)


def pcm(path, channels):
    with wave.open(str(path), "rb") as wav:
        assert (wav.getframerate(), wav.getnchannels(), wav.getsampwidth()) == (rate, channels, 2)
        return array("h", wav.readframes(wav.getnframes()))


score = pcm(assets / "normandy-score.wav", 2)
ambience = pcm(assets / "battle-ambience.wav", 2)
for i in range(len(mix)):
    mix[i] = (2.4 * score[i % len(score)] + 1.65 * ambience[i % len(ambience)]) / 32768


def effect(at, length, pan, heavy):
    low = 0
    for i in range(int(length * rate)):
        t = i / rate
        noise = rng.uniform(-1, 1)
        low += (0.024 if heavy else 0.32) * (noise - low)
        if heavy:
            value = (0.22 * sin(2 * pi * (45 * t + 4 * (1 - exp(-t * 9)))) +
                     0.35 * low) * min(t * 95, 1) * exp(-t * 3.4)
        else:
            value = (0.36 * noise * exp(-t * 120) + 0.36 * low * exp(-t * 29))
        sample = (round(at * rate) + i) * 2
        mix[sample] += value * (1 - pan)
        mix[sample + 1] += value * pan


for start in (1.5, 8.1, 26.4, 33.3, 42.0, 59.1):
    pan = rng.uniform(0.18, 0.82)
    for shot in range(rng.randint(3, 6)):
        effect(start + shot * 0.105, 0.26, pan, False)
for start in (0.9, 9.2, 28, 36, 43.4, 61):
    effect(start, 2.1, rng.uniform(0.2, 0.8), True)

for index, at in enumerate((4.3, 10.2, 14.3, 38.4, 46.5)):
    clip = pcm(Path(__file__).parent / "radio-voice" / f"{index}.wav", 1)
    slow = smooth = 0
    for i, value in enumerate(clip):
        x = value / 32768
        slow += 0.075 * (x - slow)
        smooth += 0.38 * (x - slow - smooth)
        t = i / rate
        gain = min(t * 28, 1, (len(clip) - i) / (rate * 0.07))
        voice = (tanh(2.6 * smooth) * 0.19 + rng.uniform(-0.008, 0.008)) * gain
        sample = (round(at * rate) + i) * 2
        if sample + 1 >= len(mix):
            raise ValueError("Radio clip extends beyond the loop")
        for channel in range(2):
            mix[sample + channel] = 0.62 * mix[sample + channel] + voice

output = array("h")
for i, sample in enumerate(mix):
    frame = i // 2
    fade = min(1, frame / (rate * 0.28), (count - 1 - frame) / (rate * 0.28))
    output.append(round(tanh(sample * 1.32) * 27000 * fade))
with wave.open(str(assets / "field-radio.wav"), "wb") as wav:
    wav.setparams((2, 2, rate, 0, "NONE", "not compressed"))
    wav.writeframes(output.tobytes())
