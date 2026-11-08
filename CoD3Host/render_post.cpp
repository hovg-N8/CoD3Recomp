// The picture's look (F11, "Look"). The present pass draws the frame,
// through the console's colour table and the FXAA when chosen, into a
// target of the frame's size instead of the window; its mips, made by the
// device, are the picture blurred wider and wider. From there up to three
// more passes put it on the window:
//
//   adapt    the scene's brightness, metered from a small mip (the middle
//            weighed more), eased toward over a second or so: the auto
//            exposure's eye. One texel, kept from frame to frame.
//   bright   the picture's highlights at a quarter of its size, in a
//            squared (nearly linear) space, past a soft threshold: the
//            bloom's source. Its own mips are the wider blurs of it.
//   finish   the picture, sharpened by its contrast with its four
//            neighbours (as AMD's CAS does); brought up by the exposure
//            when the scene is dark (only ever brightened, its highlights
//            rolled off so nothing clips that did not); the shade opened
//            up where the picture around is dark (a gamma lift weighed by
//            a wide blur, so black stays black and the sky is left be);
//            clarity, the detail against a narrow blur made stronger; the
//            bloom added in the squared space; then warmth, a contrast
//            curve, saturation, a vignette, film grain and a dither of a
//            step of the window's 8 bits, against banding.
//
// The films (the intros, the briefings) are 30 frames a second, and the
// title swaps once for each. With "Smooth films" on, at each such swap
// the motion between the film's last frame and the new one is found
// (coarse: a search of 13 by 13 steps of 16 pixels on the 16th size mips;
// refine: the best of the coarse vectors around, then a step of 4 pixels
// either way, on the quarter size mips), each vector the motion from the
// frame halfway between them to either one; the frame halfway is then
// each half fetched along its vector and averaged ("between"), a plain
// average where the two halves do not agree (a cut, smoke, something
// coming into view). It is shown at once and the new frame half a film
// frame later (render_d3d11.cpp): 60 a second, a sixtieth late. A film
// is sharpened too (CAS at its strongest and a fine unsharp mask), and
// left at its own exposure, so its fades stay fades.
//
// The maps are dark and hazy, made for a television in a living room: the
// exposure, the shadow lift and the clarity are what bring them up on a
// PC monitor. It works on the picture as the console sends it, the HUD
// included, as a ReShade preset would. Off (the default) it costs nothing:
// the present goes straight to the window as before.

#include "render_post.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>

#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace
{
    const char* const PostSource =
        "Texture2D picture : register(t0); Texture2D bloom : register(t1); Texture2D<float> exposure : register(t2);\n"
        "SamplerState linearClamp : register(s0);\n"
        // pictureTexel: 1/width, 1/height, width/height, the frame (for the grain);
        // bloomTexel: 1/width, 1/height of the bloom's first mip, the threshold, its knee;
        // amounts: sharpening 0..1, bloom 0..1.5, saturation 0..2, contrast 0..0.75;
        // amounts2: vignette 0..1, grain 0..1, warmth -0.5..0.5;
        // amounts3: the exposure's most gain 1..3, shadow lift 0..1, clarity 0..1, the mip the exposure meters;
        // amounts4: the exposure's step toward the metered this frame, the shadow lift's mip, the clarity's mip, a film's fine detail 0..1.
        // The film's frame before and the new one, with their mips, the motion field and a second one.
        "Texture2D frameA : register(t4); Texture2D frameB : register(t5); Texture2D field : register(t6); Texture2D fieldB : register(t3);\n"
        "Texture2D judged : register(t7);\n"
        // amounts5: a film's clean-up 0..1, the phase of the frame between (0.5 halfway), the ambient occlusion 0..1, 1 for a film.
        // The occlusion's: amounts6: the rectangle's corner in the target (x, y), the tangent of half the
        // vertical field of view, 1 for a depth that is reversed (1 near); amounts7: the depth's samples, the radius.
        "cbuffer Post : register(b0) { float4 pictureTexel; float4 bloomTexel; float4 amounts; float4 amounts2; float4 amounts3; float4 amounts4; float4 amounts5;"
        " float4 amounts6; float4 amounts7; };\n"
        "Texture2D<float> sceneDepth : register(t8); Texture2DMS<float> sceneDepthMS : register(t9); Texture2D<float> occlusionMap : register(t10);\n"
        "struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
        "V vsmain(uint id : SV_VertexID) { V v; float2 t = float2((id << 1) & 2, id & 2);"
        " v.position = float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); v.uv = t; return v; }\n"
        "float luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
        "float hash(float2 p) { float3 p3 = frac(p.xyx * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return frac((p3.x + p3.y) * p3.z); }\n"
        // A mip of the picture through four taps half its texel apart: a
        // tent, so the device's box halvings do not show as blocks.
        "float3 blurred(float2 uv, float mip) { float2 o = pictureTexel.xy * exp2(mip) * 0.5;"
        " return 0.25 * (picture.SampleLevel(linearClamp, uv + float2(-o.x, -o.y), mip).rgb + picture.SampleLevel(linearClamp, uv + float2(o.x, -o.y), mip).rgb"
        " + picture.SampleLevel(linearClamp, uv + float2(-o.x, o.y), mip).rgb + picture.SampleLevel(linearClamp, uv + float2(o.x, o.y), mip).rgb); }\n"
        "float4 adapt(V v) : SV_Target {\n"
        "    float sum = 0, weights = 0;\n"
        "    [unroll] for (int y = 0; y < 5; y++) [unroll] for (int x = 0; x < 5; x++) {\n"
        "        float2 p = (float2(x, y) + 0.5) / 5.0; float l = luma(picture.SampleLevel(linearClamp, p, amounts3.w).rgb);\n"
        "        float w = 1.0 - 0.8 * length(p - 0.5); sum += l * l * w; weights += w; }\n"
        "    float now = sum / weights, before = exposure.Load(int3(0, 0, 0));\n"
        "    return before > 0.0 ? lerp(before, now, amounts4.x) : now; }\n"
        // Four bilinear taps cover the 4 by 4 texels a quarter size texel
        // stands for; each weighed down by its brightness so one bright
        // pixel does not flicker the bloom as it moves (Karis' average).
        "float4 bright(V v) : SV_Target {\n"
        "    float3 sum = 0; float weights = 0;\n"
        "    [unroll] for (int i = 0; i < 4; i++) {\n"
        "        float2 o = float2((i & 1) ? 1.0 : -1.0, (i & 2) ? 1.0 : -1.0) * pictureTexel.xy;\n"
        "        float3 c = picture.SampleLevel(linearClamp, v.uv + o, 0).rgb; c *= c;\n"
        "        float w = 1.0 / (1.0 + luma(c)); sum += c * w; weights += w; }\n"
        "    float3 c = sum / weights;\n"
        "    float b = max(c.r, max(c.g, c.b)), t = bloomTexel.z, k = bloomTexel.w;\n"
        "    float soft = clamp(b - t + k, 0.0, 2.0 * k); soft = soft * soft / (4.0 * k + 1e-5);\n"
        "    return float4(c * max(soft, b - t) / max(b, 1e-5), 1.0); }\n"
        "static const float bloomWeights[6] = { 0.6, 0.8, 1.0, 1.0, 0.9, 0.8 };\n"
        "float3 bloomAt(float2 uv) {\n"
        "    float3 sum = 0; float total = 0;\n"
        "    [unroll] for (int m = 0; m < 6; m++) {\n"
        "        float2 px = bloomTexel.xy * exp2(float(m));\n"
        "        float3 s = bloom.SampleLevel(linearClamp, uv + float2(-px.x, -px.y), m).rgb + bloom.SampleLevel(linearClamp, uv + float2(px.x, -px.y), m).rgb\n"
        "                 + bloom.SampleLevel(linearClamp, uv + float2(-px.x, px.y), m).rgb + bloom.SampleLevel(linearClamp, uv + float2(px.x, px.y), m).rgb;\n"
        "        sum += s * 0.25 * bloomWeights[m]; total += bloomWeights[m]; }\n"
        "    return sum / total; }\n"
        "float4 finish(V v) : SV_Target {\n"
        "    float2 uv = v.uv, px = pictureTexel.xy;\n"
        "    float3 c = picture.SampleLevel(linearClamp, uv, 0).rgb; float original = luma(c);\n"
        // A film's darkest parts are where its compression hides (blocks,
        // smears): the sharpening, the clarity and the fine detail ease off
        // there, so lifting them shows the shade, not the video's blotches.
        "    float detailKeep = amounts5.w > 0.5 ? smoothstep(0.02, 0.18, original) : 1.0;\n"
        "    if (amounts.x > 0.0) {\n"
        "        float3 n = picture.SampleLevel(linearClamp, uv + float2(0, -px.y), 0).rgb, s = picture.SampleLevel(linearClamp, uv + float2(0, px.y), 0).rgb;\n"
        "        float3 w = picture.SampleLevel(linearClamp, uv + float2(-px.x, 0), 0).rgb, e = picture.SampleLevel(linearClamp, uv + float2(px.x, 0), 0).rgb;\n"
        "        float3 mn = min(c, min(min(n, s), min(w, e))), mx = max(c, max(max(n, s), max(w, e)));\n"
        "        float3 amp = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-4)));\n"
        "        float3 k = amp * detailKeep * (-1.0 / lerp(8.0, 5.0, amounts.x));\n"
        "        c = saturate((c + (n + s + w + e) * k) / (1.0 + 4.0 * k)); }\n"
        // The exposure: the brightness times the gain in the squared space,
        // then Reinhard's curve with its white at the gain, so what was
        // white stays white and nothing new clips; with no gain, nothing.
        // Night: how far the metered scene is below a dim day's (amounts2.w:
        // 1 metered in a level, 2 in a film, 0 not metered). In a level the
        // exposure may then go two and a half times as far; a film, held at
        // its own exposure by day (its fades stay fades), gets the gain by
        // as much as it is night. And the shadow lift goes three times as
        // far: the night maps and their films stayed dark with the day's
        // limits, which a day scene needs to not wash out. A fade still
        // ends on black: the gain only scales what is there.
        "    float night = amounts2.w > 0.0 ? smoothstep(0.03, 0.003, exposure.Load(int3(0, 0, 0))) : 0.0;\n"
        "    float most = amounts2.w > 1.5 ? 1.0 + (amounts3.x - 1.0) * 1.5 * night : amounts3.x * (1.0 + 1.5 * night);\n"
        "    if (most > 1.0) { float gain = clamp(0.14 / max(exposure.Load(int3(0, 0, 0)), 1e-4), 1.0, most);\n"
        "        float l = luma(c), x = l * l * gain, y = x * (1.0 + x / (gain * gain)) / (1.0 + x); c *= sqrt(y) / max(l, 1e-4); }\n"
        // The shadow lift: the brightness raised to a power below one, the
        // more the darker the picture is around (a wide blur's brightness);
        // eased in over the first few steps above black, where a power's
        // steep start made the 8 bit steps of the game's dark fog blocks.
        "    if (amounts3.y > 0.0) { float around = luma(blurred(uv, amounts4.y)); float w = 1.0 - smoothstep(0.04, 0.45, around);\n"
        "        float l = luma(c), lifted = pow(max(l, 1e-5), lerp(1.0, 1.0 / (1.0 + 0.8 * amounts3.y * (1.0 + 2.0 * night)), w));\n"
        "        c *= lerp(l, lifted, smoothstep(0.0, 0.08, l)) / max(l, 1e-5); }\n"
        // Clarity: the picture's own detail, against a narrow blur, added
        // again to the brightness; held in so the edges of the sky do not halo.
        // A film's shade lifted by its own surroundings (amounts5.z, the
        // films' shadows): the films mix a bright sky with faces and ground
        // near black, which no lift of the whole picture can reach without
        // blowing the sky out. Each pixel's brightness is raised by how dark
        // the picture is round it (two blurs, an eighth and a thirty-second
        // of it, the darker taken, so a face against the sky is lifted too),
        // through a curve that keeps white white; the first steps above
        // black eased in, so the letterbox and true black stay black.
        "    if (amounts5.z > 0.0 && amounts5.w > 0.5) {\n"
        "        float around = min(luma(blurred(uv, amounts4.y - 2.0)), luma(blurred(uv, amounts4.y)));\n"
        "        float gain = clamp(0.14 / max(around, 0.025), 1.0, 1.0 + 2.0 * amounts5.z);\n"
        "        float l = luma(c), lit = l * gain / (1.0 + l * (gain - 1.0));\n"
        "        c *= lerp(1.0, lit / max(l, 1e-4), smoothstep(0.004, 0.03, l)); }\n"
        "    if (amounts3.z > 0.0) { float d = clamp(original - luma(blurred(uv, amounts4.z)), -0.1, 0.1) * detailKeep;\n"
        "        float l = luma(c); c *= max(l + d * amounts3.z * 1.5 * saturate(0.3 + 2.8 * l * (1.0 - l)), 0.0) / max(l, 1e-5); }\n"
        // A film's fine detail: an unsharp mask a pixel or two wide, on the
        // brightness, over the CAS: the films are soft, scaled up video.
        "    if (amounts4.w > 0.0) { float d = clamp(original - luma(blurred(uv, 0.5)), -0.12, 0.12) * detailKeep;\n"
        "        float l = luma(c); c *= max(l + d * amounts4.w * 0.8, 0.0) / max(l, 1e-5); }\n"
        "    if (amounts.y > 0.0) c = sqrt(c * c + bloomAt(uv) * amounts.y);\n"
        "    if (amounts2.z != 0.0) { float l = luma(c); float3 t = c * float3(1.0 + 0.25 * amounts2.z, 1.0 + 0.05 * amounts2.z, 1.0 - 0.25 * amounts2.z);"
        " c = t * (l / max(luma(t), 1e-4)); }\n"
        "    c = saturate(c);\n"
        // The S curve on the brightness alone, so the hues stay, and
        // eased off in the shadows: the maps are dark, and a plain curve
        // crushed what lies in shade to black.
        "    if (amounts.w > 0.0) { float l = luma(c); float s = lerp(l, l * l * (3.0 - 2.0 * l), amounts.w * smoothstep(0.0, 0.35, l));"
        " c = saturate(c * (s / max(l, 1e-4))); }\n"
        // The darkest tones alone brought up (with the shadow detail): a
        // bump that is most a third of the way to a fifth of white and gone
        // above it, so black stays black and the mid tones are left as they
        // are, and the deepest shade is not a hard, inky patch.
        "    if (amounts3.y > 0.0) { float l = luma(c), d = saturate(1.0 - l / 0.22);"
        " c = saturate(c * (1.0 + amounts3.y * 2.5 * d * d)); }\n"
        "    { float l = luma(c); float sat = max(c.r, max(c.g, c.b)) - min(c.r, min(c.g, c.b)); float amount = amounts.z - 1.0;"
        " if (amount > 0.0) amount *= 1.0 - 0.6 * sat; c = saturate(lerp(l.xxx, c, 1.0 + amount)); }\n"
        "    if (amounts2.x > 0.0) { float2 d = (uv - 0.5) * float2(pictureTexel.z, 1.0); c *= 1.0 - amounts2.x * 0.55 * smoothstep(0.2, 1.1, dot(d, d)); }\n"
        "    if (amounts2.y > 0.0) { float n = hash(v.position.xy + pictureTexel.w * 17.0) + hash(v.position.xy * 1.7 + pictureTexel.w * 31.0) - 1.0;"
        " float l = luma(c); c += n * amounts2.y * 0.06 * (0.35 + 4.0 * l * (1.0 - l)); }\n"
        "    c += (hash(v.position.xy + pictureTexel.w) - 0.5) / 255.0;\n"
        "    return float4(saturate(c), 1.0); }\n"
        // The films' motion. A vector o (in the picture's uv) puts a point of
        // the frame halfway at uv - o/2 in the one before and uv + o/2 in the
        // new one; its cost is how unlike the two 3 by 3 patches there are,
        // in colour (a plane's white stripes and the pale sky are much the
        // same brightness), plus a little for its length, so a flat sky
        // stays still.
        "float patchCost(float2 uv, float2 o, float mip, float2 step) { float sum = 0;\n"
        "    [unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) { float2 p = uv + float2(x, y) * step;\n"
        "        sum += dot(abs(frameA.SampleLevel(linearClamp, p - o * 0.5, mip).rgb - frameB.SampleLevel(linearClamp, p + o * 0.5, mip).rgb), 1.0 / 3.0); }\n"
        "    return sum / 9.0 + 0.0001 * length(o / pictureTexel.xy); }\n"
        "float2 fieldAt(Texture2D f, int2 at) { uint w, h; f.GetDimensions(w, h); return f.Load(int3(clamp(at, int2(0, 0), int2(w, h) - 1), 0)).xy; }\n"
        "int2 cellOf(Texture2D f, float2 uv) { uint w, h; f.GetDimensions(w, h); return int2(uv * float2(w, h)); }\n"
        // Coarse: every step of 16 pixels within 96 either way, on the
        // 16th size mips.
        "float4 coarse(V v) : SV_Target {\n"
        "    float2 step = pictureTexel.xy * 16.0; float best = 1e9; float2 bestO = 0;\n"
        "    [loop] for (int y = -6; y <= 6; y++) [loop] for (int x = -6; x <= 6; x++) {\n"
        "        float2 o = float2(x, y) * step; float cost = patchCost(v.uv, o, 4.0, step);\n"
        "        if (cost < best) { best = cost; bestO = o; } }\n"
        "    return float4(bestO, best, 1.0); }\n"
        // Refine, on the quarter size mips: the best of the coarse vectors
        // of the 5 by 5 blocks around and of the last pair's motion here
        // (fieldB: a plane keeps its motion from frame to frame), then a
        // step of 4 pixels from it either way.
        "float4 refine(V v) : SV_Target {\n"
        "    int2 at = cellOf(field, v.uv); float2 step = pictureTexel.xy * 4.0;\n"
        "    float2 bestO = fieldB.SampleLevel(linearClamp, v.uv, 0).xy; float best = patchCost(v.uv, bestO, 2.0, step);\n"
        "    [loop] for (int y = -2; y <= 2; y++) [unroll] for (int x = -2; x <= 2; x++) {\n"
        "        float2 o = fieldAt(field, at + int2(x, y)); float cost = patchCost(v.uv, o, 2.0, step);\n"
        "        if (cost < best) { best = cost; bestO = o; } }\n"
        "    float2 centre = bestO;\n"
        "    [unroll] for (int j = -1; j <= 1; j++) [unroll] for (int i = -1; i <= 1; i++) {\n"
        "        float2 o = centre + float2(i, j) * step; float cost = patchCost(v.uv, o, 2.0, step);\n"
        "        if (cost < best) { best = cost; bestO = o; } }\n"
        "    return float4(bestO, best, 1.0); }\n"
        // Detail, on the half size mips: the best of the refined vectors
        // of the 3 by 3 blocks around, then a step of 2 pixels either way.
        "float4 detail(V v) : SV_Target {\n"
        "    int2 at = cellOf(field, v.uv); float2 step = pictureTexel.xy * 2.0; float best = 1e9; float2 bestO = 0;\n"
        "    [unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) {\n"
        "        float2 o = fieldAt(field, at + int2(x, y)); float cost = patchCost(v.uv, o, 1.0, step);\n"
        "        if (cost < best) { best = cost; bestO = o; } }\n"
        "    float2 centre = bestO;\n"
        "    [unroll] for (int j = -1; j <= 1; j++) [unroll] for (int i = -1; i <= 1; i++) {\n"
        "        float2 o = centre + float2(i, j) * step; float cost = patchCost(v.uv, o, 1.0, step);\n"
        "        if (cost < best) { best = cost; bestO = o; } }\n"
        "    return float4(bestO, best, 1.0); }\n"
        // Each pixel of the frame between takes, of the detail vectors of
        // its own block and the eight around and the refined ones of the
        // same (fieldB), the one its own pixels agree on best (a cross of
        // five at full size and five at half), a little in favour of its
        // own block's so neighbours do not break up: blended vectors tore
        // a plane's edges, its pixels fetched half with its own motion and
        // half with the sky's.
        "float pixelCost(float2 uv, float2 o) { float sum = 0; float2 px = pictureTexel.xy;\n"
        "    const float2 cross5[5] = { float2(0, 0), float2(1, 0), float2(-1, 0), float2(0, 1), float2(0, -1) };\n"
        "    [unroll] for (int k = 0; k < 5; k++) {\n"
        "        float2 p = uv + cross5[k] * px, q = uv + cross5[k] * px * 2.0;\n"
        "        sum += dot(abs(frameA.SampleLevel(linearClamp, p - o * 0.5, 0).rgb - frameB.SampleLevel(linearClamp, p + o * 0.5, 0).rgb), 1.0 / 3.0);\n"
        "        sum += dot(abs(frameA.SampleLevel(linearClamp, q - o * 0.5, 1).rgb - frameB.SampleLevel(linearClamp, q + o * 0.5, 1).rgb), 1.0 / 3.0); }\n"
        "    return sum / 10.0 + 0.00005 * length(o / px); }\n"
        // A vector is only believed where the motion found at one of its
        // ends is the same. A long one could jump over something small and
        // fast (a wing, a piece of the Activision logo), both its ends on
        // empty sky or black and agreeing perfectly, and leave the thing
        // out of the frame between: a hole in the plane, the logo's pieces
        // flashing on and off. The background at such ends moves with the
        // background, not with the vector; the pixel then falls to the
        // plain average below instead of vanishing.
        "float endsDisagree(float2 uv, float2 o) { float2 px = pictureTexel.xy;\n"
        "    float2 a = field.SampleLevel(linearClamp, uv - o * 0.5, 0).xy, b = field.SampleLevel(linearClamp, uv + o * 0.5, 0).xy;\n"
        "    return min(0.15, 0.003 * min(length((a - o) / px), length((b - o) / px))); }\n"
        // How much of the detail field found no good match, over a grid of
        // 48 by 27 of it: a frame too hard to make (a plane turning close
        // by, an explosion) is not made, the one before is shown again, as
        // the film itself would.
        "float4 judge(V v) : SV_Target {\n"
        "    uint w, h; field.GetDimensions(w, h); float4 bad = 0;\n"
        "    [loop] for (int y = 0; y < 27; y++) [loop] for (int x = 0; x < 48; x++) {\n"
        "        float cost = field.Load(int3(int2((float2(x, y) + 0.5) / float2(48.0, 27.0) * float2(w, h)), 0)).z;\n"
        "        bad += float4(cost > 0.05, cost > 0.03, cost > 0.02, cost); }\n"
        "    return bad / (48.0 * 27.0); }\n"
        // The frame between at its phase (amounts5.y: 0 the one before, 1
        // the new one; 0.5 halfway, the one frame between at 60 a second,
        // more of them for a screen that shows more frames than that). The
        // fields hold the motion where a point is halfway; at another phase
        // a point moving by c is at the halfway point uv + c (0.5 - phase),
        // so each vector is judged there, and the field looked up there
        // again for one more: judged at the pixel itself, a moving thing
        // near either end took the background's motion and came out torn.
        "float4 between(V v) : SV_Target {\n"
        "    float p = amounts5.y, shift = 0.5 - p;\n"
        // Too hard to make (the share of poor matches, or of middling ones,
        // too high: an explosion, a logo's pieces flying apart): the nearer
        // of the film's own two frames instead, as the film itself shows.
        "    float4 hard = judged.Load(int3(0, 0, 0));\n"
        "    if (hard.x > 0.02 || hard.y > 0.012) { if (p < 0.5) return float4(frameA.SampleLevel(linearClamp, v.uv, 0).rgb, 1.0); return float4(frameB.SampleLevel(linearClamp, v.uv, 0).rgb, 1.0); }\n"
        "    int2 at = cellOf(field, v.uv), atB = cellOf(fieldB, v.uv);\n"
        "    float2 o = fieldAt(field, at); float best = 1e9;\n"
        "    [unroll] for (int y = -1; y <= 1; y++) [unroll] for (int x = -1; x <= 1; x++) {\n"
        "        float2 c = fieldAt(field, at + int2(x, y)), m = v.uv + c * shift;\n"
        "        float cost = pixelCost(m, c) + endsDisagree(m, c) + ((x != 0 || y != 0) ? 0.004 : 0.0);\n"
        "        if (cost < best) { best = cost; o = c; }\n"
        "        if (shift != 0.0) { c = fieldAt(field, cellOf(field, m)); m = v.uv + c * shift;\n"
        "            cost = pixelCost(m, c) + endsDisagree(m, c) + 0.004; if (cost < best) { best = cost; o = c; } }\n"
        "        c = fieldAt(fieldB, atB + int2(x, y)); m = v.uv + c * shift; cost = pixelCost(m, c) + endsDisagree(m, c) + 0.004;\n"
        "        if (cost < best) { best = cost; o = c; } }\n"
        "    float3 a = frameA.SampleLevel(linearClamp, v.uv - o * p, 0).rgb, b = frameB.SampleLevel(linearClamp, v.uv + o * (1.0 - p), 0).rgb;\n"
        "    float3 hereA = frameA.SampleLevel(linearClamp, v.uv, 0).rgb, hereB = frameB.SampleLevel(linearClamp, v.uv, 0).rgb;\n"
        "    float3 plain = lerp(hereA, hereB, p);\n"
        // Where even the best does not agree (what comes into view, smoke,
        // a cut), the two frames simply blended. And where a spot is lit in
        // both frames it is at least as lit as the dimmer between them: a
        // vector that matched dark background to dark background (a logo's
        // pieces flying across black) otherwise left holes in what stayed.
        "    float3 made = lerp(lerp(a, b, p), plain, saturate((best - 0.025) * 15.0));\n"
        "    return float4(max(made, min(hereA, hereB)), 1.0); }\n"
        // A film's clean-up, on its frame before anything else reads it:
        // the films are 720p video at 4 Mbit, and their flat and dark
        // areas show the compression's steps (banding, faint 8 pixel
        // blocks, ringing by the edges), which the night lift and the
        // sharpening then make plainer. Each pixel is averaged with points
        // round it on two rings (rotated pixel by pixel, so no pattern),
        // each weighed by how close in colour it is: only differences
        // smaller than a few steps of 8 bits are smoothed, more in the
        // dark, so edges and real detail are left as they are.
        "float4 clean(V v) : SV_Target {\n"
        "    float2 px = pictureTexel.xy; float s = max(1.0, 1.0 / (px.x * 1280.0));\n"
        "    float3 c = picture.SampleLevel(linearClamp, v.uv, 0).rgb;\n"
        "    float limit = amounts5.x * (1.5 + 2.0 * (1.0 - smoothstep(0.02, 0.3, luma(c)))) / 255.0;\n"
        "    float turn = hash(v.position.xy) * 6.2832; float3 sum = c; float weights = 1.0;\n"
        "    [unroll] for (int r = 0; r < 3; r++) [unroll] for (int k = 0; k < 6; k++) {\n"
        "        float angle = turn + float(k) * 1.0472 + float(r) * 0.5236;\n"
        "        float2 o = float2(cos(angle), sin(angle)) * (float(r) * 3.0 + 2.0) * s * px;\n"
        "        float3 q = picture.SampleLevel(linearClamp, v.uv + o, 0).rgb; float3 d = abs(q - c);\n"
        "        float w = saturate(1.5 - max(d.r, max(d.g, d.b)) / limit); sum += q * w; weights += w; }\n"
        "    return float4(sum / weights, 1.0); }\n"
        // The ambient occlusion, from the scene's depth as the title
        // resolves it (after the solid world, before the smoke, the
        // effects and the HUD). The depth is a distance's reciprocal (1 - d,
        // or d reversed, is about near / distance), so positions come out
        // in units of the near plane, which is all the shape needs. Each
        // pixel's surface faces the way its nearer neighbours say; twelve
        // points within a radius round it (a spiral, turned pixel by pixel)
        // shade it as far as they stand above that surface and close by:
        // corners, the foot of a wall, under a ledge, where grass meets a
        // rock. Far points (the gun against the world) do not count.
        "float sceneZ(int2 p) {\n"
        "    float d = amounts7.x > 1.5 ? sceneDepthMS.Load(p, 0) : sceneDepth.Load(int3(p, 0));\n"
        "    return 1.0 / max(amounts6.w > 0.5 ? d : 1.0 - d, 1e-7); }\n"
        "float3 viewAt(float2 q, float z) { float2 ndc = q * pictureTexel.xy * 2.0 - 1.0;\n"
        "    return float3(ndc.x * pictureTexel.z * amounts6.z * z, -ndc.y * amounts6.z * z, z); }\n"
        "int2 inRect(float2 q) { return int2(clamp(q, 0.5, 1.0 / pictureTexel.xy - 0.5) + amounts6.xy); }\n"
        "float4 occlusion(V v) : SV_Target {\n"
        "    float2 q = v.position.xy; float z = sceneZ(inRect(q));\n"
        "    if (amounts7.z > 0.5) return saturate(log2(z) / 16.0);\n"
        "    if (z > 30000.0) return 1.0;\n"
        "    float3 p = viewAt(q, z);\n"
        "    float3 l = viewAt(q - float2(1, 0), sceneZ(inRect(q - float2(1, 0)))), r = viewAt(q + float2(1, 0), sceneZ(inRect(q + float2(1, 0))));\n"
        "    float3 u = viewAt(q - float2(0, 1), sceneZ(inRect(q - float2(0, 1)))), d = viewAt(q + float2(0, 1), sceneZ(inRect(q + float2(0, 1))));\n"
        "    float3 dx = abs(r.z - z) < abs(z - l.z) ? r - p : p - l, dy = abs(d.z - z) < abs(z - u.z) ? d - p : p - u;\n"
        "    float3 n = normalize(cross(dx, dy)); if (dot(n, p) > 0.0) n = -n;\n"
        "    float radius = amounts7.y, reach = min(radius / z * 0.5 / (pictureTexel.y * amounts6.z), 0.12 / pictureTexel.y);\n"
        "    if (reach < 1.5) return 1.0;\n"
        "    float turn = hash(q) * 6.2832, shade = 0.0;\n"
        "    [unroll] for (int k = 0; k < 12; k++) {\n"
        "        float angle = turn + float(k) * 2.39996; float2 s = q + float2(cos(angle), sin(angle)) * reach * sqrt((float(k) + 0.5) / 12.0);\n"
        "        float3 w = viewAt(s, sceneZ(inRect(s))) - p; float len = length(w);\n"
        "        float close = saturate(1.0 - len / radius);\n"
        "        shade += saturate(dot(w, n) / max(len, 1e-4) - 0.05) * close; }\n"
        "    return saturate(1.0 - shade / 12.0 * 4.0); }\n"
        // Onto the scene: the occlusion smoothed over 5 by 5 of its pixels
        // that lie at much the same depth (its noise gone, its edges kept),
        // by the amount chosen, as a multiply.
        "float4 occlusionApply(V v) : SV_Target {\n"
        "    float2 q = v.position.xy - amounts6.xy; float z = sceneZ(int2(v.position.xy)); float sum = 0.0, weights = 1e-4;\n"
        "    [unroll] for (int y = -2; y <= 2; y++) [unroll] for (int x = -2; x <= 2; x++) {\n"
        "        float2 s = q + float2(x, y) * 1.5; float zs = sceneZ(inRect(s));\n"
        "        float w = exp(-abs(zs - z) / (z * 0.03)); sum += occlusionMap.Load(int3(int2(clamp(s, 0.5, 1.0 / pictureTexel.xy - 0.5)), 0)) * w; weights += w; }\n"
        "    return float4(lerp(1.0, sum / weights, amounts5.z).xxx, 1.0); }\n";

    struct Post
    {
        ID3D11Device* device = nullptr;
        bool failed = false;
        ComPtr<ID3D11VertexShader> vertexShader;
        ComPtr<ID3D11PixelShader> adapt, bright, finish, coarse, refine, detail, judge, between, clean, occlusion, occlusionApply;
        ComPtr<ID3D11SamplerState> sampler;
        ComPtr<ID3D11BlendState> multiply;       // the occlusion onto the scene
        ComPtr<ID3D11RasterizerState> rasterizer;
        ComPtr<ID3D11Buffer> constants;
        ComPtr<ID3D11Texture2D> picture, bloom, exposure[2];
        ComPtr<ID3D11ShaderResourceView> pictureView, bloomView, exposureView[2];
        ComPtr<ID3D11RenderTargetView> pictureTarget, bloomTarget, exposureTarget[2];
        uint32_t width = 0, height = 0, bloomWidth = 0, bloomHeight = 0, pictureMips = 1;
        uint32_t frame = 0;
        int exposureNow = 0;                     // the exposure texel written last
        std::chrono::steady_clock::time_point last{};

        // The films: their last two frames (with mips, for the search), the
        // coarse, the refined (this pair's and the last's) and the detail
        // motion fields.
        ComPtr<ID3D11Texture2D> filmFrame[2], coarseField, refinedField[2], detailField;
        ComPtr<ID3D11ShaderResourceView> filmView[2], coarseView, refinedView[2], detailView;
        ComPtr<ID3D11RenderTargetView> coarseTarget, refinedTarget[2], detailTarget;
        int refinedNow = 0;
        ComPtr<ID3D11Texture2D> judgedTexel;      // the share of the frame the motion found no match for
        ComPtr<ID3D11ShaderResourceView> judgedView;
        ComPtr<ID3D11RenderTargetView> judgedTarget;
        uint32_t filmWidth = 0, filmHeight = 0;
        int filmNow = 0;                         // the film frame written last
        bool filmBefore = false;                 // a frame of this film before it
        double filmInterval = 0.0;               // seconds between the film's swaps, averaged
        std::chrono::steady_clock::time_point filmSwap{};
        ComPtr<ID3D11Texture2D> cleaned;         // a film's frame cleaned up, copied back into the picture
        ComPtr<ID3D11RenderTargetView> cleanedTarget;

        // The ambient occlusion, a value a pixel of the scene's rectangle.
        ComPtr<ID3D11Texture2D> occlusionMap;
        ComPtr<ID3D11ShaderResourceView> occlusionView;
        ComPtr<ID3D11RenderTargetView> occlusionTarget;
        uint32_t occlusionWidth = 0, occlusionHeight = 0;
    };
    Post g;

    constexpr UINT BloomMips = 6;

    ComPtr<ID3DBlob> Compile(RenderPost::CompileFunction compile, const char* entry, const char* profile)
    {
        ComPtr<ID3DBlob> code, errors;
        if (FAILED(compile(PostSource, strlen(PostSource), "look program", nullptr, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors)))
        {
            printf("render: the look program (%s) did not compile: %s\n", entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
            fflush(stdout);
            return nullptr;
        }
        return code;
    }

    // 0.5.599: the look's programs compiled on a thread of their own. Compiled
    // here, on the drawing path, the first film of the campaign (the legal
    // screen, look on for films by default) stopped the drawing for seconds,
    // the title's own GPU timeout fired and it froze ("The GPU is hung", 598).
    // Until they are ready the picture goes without the look.
    struct Blobs
    {
        ComPtr<ID3DBlob> vs, adapt, bright, finish, coarse, refine, detail, judge, between, clean, occlusion, occlusionApply;
    };
    Blobs g_blobs;
    std::atomic<int> g_blobState{ 0 };   // 0 not begun, 1 compiling, 2 ready, 3 failed

    bool Programs(ID3D11Device* device, RenderPost::CompileFunction compile)
    {
        if (g.device != device) { g = Post(); g.device = device; }
        if (g.finish) return true;
        if (g.failed || compile == nullptr) return false;
        int state = g_blobState.load(std::memory_order_acquire);
        if (state == 0)
        {
            g_blobState.store(1, std::memory_order_release);
            std::thread([compile]() {
                Blobs b;
                b.vs = Compile(compile, "vsmain", "vs_5_0"); b.adapt = Compile(compile, "adapt", "ps_5_0");
                b.bright = Compile(compile, "bright", "ps_5_0"); b.finish = Compile(compile, "finish", "ps_5_0");
                b.coarse = Compile(compile, "coarse", "ps_5_0"); b.refine = Compile(compile, "refine", "ps_5_0");
                b.detail = Compile(compile, "detail", "ps_5_0"); b.judge = Compile(compile, "judge", "ps_5_0"); b.between = Compile(compile, "between", "ps_5_0");
                b.clean = Compile(compile, "clean", "ps_5_0"); b.occlusion = Compile(compile, "occlusion", "ps_5_0");
                b.occlusionApply = Compile(compile, "occlusionApply", "ps_5_0");
                const bool ok = b.vs && b.adapt && b.bright && b.finish && b.coarse && b.refine && b.detail && b.judge && b.between && b.clean && b.occlusion && b.occlusionApply;
                g_blobs = std::move(b);
                g_blobState.store(ok ? 2 : 3, std::memory_order_release);
            }).detach();
            return false;
        }
        if (state == 1) return false;
        if (state == 3) { g.failed = true; return false; }
        g.failed = true;   // until everything below is made
        const Blobs& b = g_blobs;
        ID3DBlob *vs = b.vs.Get(), *adapt = b.adapt.Get(), *bright = b.bright.Get(), *finish = b.finish.Get(), *coarse = b.coarse.Get(), *refine = b.refine.Get(),
                 *detail = b.detail.Get(), *judge = b.judge.Get(), *between = b.between.Get(), *clean = b.clean.Get(), *occlusion = b.occlusion.Get(),
                 *occlusionApply = b.occlusionApply.Get();
        if (FAILED(device->CreatePixelShader(clean->GetBufferPointer(), clean->GetBufferSize(), nullptr, &g.clean)) ||
            FAILED(device->CreatePixelShader(occlusion->GetBufferPointer(), occlusion->GetBufferSize(), nullptr, &g.occlusion)) ||
            FAILED(device->CreatePixelShader(occlusionApply->GetBufferPointer(), occlusionApply->GetBufferSize(), nullptr, &g.occlusionApply))) return false;
        D3D11_BLEND_DESC multiply{};
        multiply.RenderTarget[0].BlendEnable = TRUE;
        multiply.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
        multiply.RenderTarget[0].DestBlend = D3D11_BLEND_SRC_COLOR;
        multiply.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        multiply.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        multiply.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        multiply.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        multiply.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
        if (FAILED(device->CreateBlendState(&multiply, &g.multiply))) return false;
        if (FAILED(device->CreatePixelShader(detail->GetBufferPointer(), detail->GetBufferSize(), nullptr, &g.detail)) ||
            FAILED(device->CreatePixelShader(judge->GetBufferPointer(), judge->GetBufferSize(), nullptr, &g.judge))) return false;
        if (FAILED(device->CreatePixelShader(coarse->GetBufferPointer(), coarse->GetBufferSize(), nullptr, &g.coarse)) ||
            FAILED(device->CreatePixelShader(refine->GetBufferPointer(), refine->GetBufferSize(), nullptr, &g.refine)) ||
            FAILED(device->CreatePixelShader(between->GetBufferPointer(), between->GetBufferSize(), nullptr, &g.between))) return false;
        if (FAILED(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g.vertexShader)) ||
            FAILED(device->CreatePixelShader(adapt->GetBufferPointer(), adapt->GetBufferSize(), nullptr, &g.adapt)) ||
            FAILED(device->CreatePixelShader(bright->GetBufferPointer(), bright->GetBufferSize(), nullptr, &g.bright)) ||
            FAILED(device->CreatePixelShader(finish->GetBufferPointer(), finish->GetBufferSize(), nullptr, &g.finish))) return false;
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        D3D11_BUFFER_DESC buffer{};
        buffer.ByteWidth = 144;
        buffer.Usage = D3D11_USAGE_DEFAULT;
        buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        if (FAILED(device->CreateSamplerState(&sampler, &g.sampler)) || FAILED(device->CreateRasterizerState(&rasterizer, &g.rasterizer)) ||
            FAILED(device->CreateBuffer(&buffer, nullptr, &g.constants))) { g.finish.Reset(); return false; }
        // The exposure's two texels, read one and write the other each frame.
        D3D11_TEXTURE2D_DESC one{};
        one.Width = one.Height = 1;
        one.MipLevels = one.ArraySize = 1;
        one.Format = DXGI_FORMAT_R32_FLOAT;
        one.SampleDesc.Count = 1;
        one.Usage = D3D11_USAGE_DEFAULT;
        one.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        for (int i = 0; i < 2; i++)
        {
            const float none = 0.0f;
            const D3D11_SUBRESOURCE_DATA initial{ &none, sizeof(float), 0 };
            if (FAILED(device->CreateTexture2D(&one, &initial, &g.exposure[i])) ||
                FAILED(device->CreateShaderResourceView(g.exposure[i].Get(), nullptr, &g.exposureView[i])) ||
                FAILED(device->CreateRenderTargetView(g.exposure[i].Get(), nullptr, &g.exposureTarget[i]))) { g.finish.Reset(); return false; }
        }
        g.failed = false;
        printf("render: the look programs are ready\n");
        fflush(stdout);
        return true;
    }

    bool Targets(ID3D11Device* device, uint32_t width, uint32_t height)
    {
        if (g.pictureTarget && g.width == width && g.height == height) return true;
        g.picture.Reset(); g.pictureView.Reset(); g.pictureTarget.Reset();
        g.bloom.Reset(); g.bloomView.Reset(); g.bloomTarget.Reset();
        g.cleaned.Reset(); g.cleanedTarget.Reset();
        g.width = g.height = 0;
        // The picture with all its mips: the blurs the exposure, the shadow
        // lift and the clarity read.
        g.pictureMips = 1;
        while ((std::max(width, height) >> g.pictureMips) > 0) g.pictureMips++;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = g.pictureMips;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;   // a little finer than the window's 8 bits, for the grade
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.picture))) return false;
        device->CreateShaderResourceView(g.picture.Get(), nullptr, &g.pictureView);
        D3D11_RENDER_TARGET_VIEW_DESC view{};
        view.Format = desc.Format;
        view.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(g.picture.Get(), &view, &g.pictureTarget);
        // The bloom at a quarter size, its mips the device's own halvings.
        g.bloomWidth = std::max(1u, width / 4);
        g.bloomHeight = std::max(1u, height / 4);
        desc.Width = g.bloomWidth;
        desc.Height = g.bloomHeight;
        desc.MipLevels = BloomMips;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.bloom))) { g.pictureTarget.Reset(); return false; }
        device->CreateShaderResourceView(g.bloom.Get(), nullptr, &g.bloomView);
        view.Format = desc.Format;
        device->CreateRenderTargetView(g.bloom.Get(), &view, &g.bloomTarget);
        if (!g.pictureView || !g.pictureTarget || !g.bloomView || !g.bloomTarget) { g.pictureTarget.Reset(); return false; }
        g.width = width;
        g.height = height;
        printf("render: the look's targets made, %ux%u, %u mips (bloom %ux%u)\n", width, height, g.pictureMips, g.bloomWidth, g.bloomHeight);
        fflush(stdout);
        return true;
    }

    // A motion field of the given size: a vector and its cost a texel.
    bool MotionField(ID3D11Device* device, uint32_t width, uint32_t height, ComPtr<ID3D11Texture2D>& texture,
                     ComPtr<ID3D11ShaderResourceView>& view, ComPtr<ID3D11RenderTargetView>& target)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = std::max(1u, width);
        desc.Height = std::max(1u, height);
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &texture)) &&
               SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, &view)) &&
               SUCCEEDED(device->CreateRenderTargetView(texture.Get(), nullptr, &target));
    }

    // The film's frames and the motion fields, of the picture's size.
    bool FilmTargets(ID3D11Device* device)
    {
        if (g.detailTarget && g.filmWidth == g.width && g.filmHeight == g.height) return true;
        for (int i = 0; i < 2; i++) { g.filmFrame[i].Reset(); g.filmView[i].Reset(); }
        g.coarseField.Reset(); g.coarseView.Reset(); g.coarseTarget.Reset();
        for (int i = 0; i < 2; i++) { g.refinedField[i].Reset(); g.refinedView[i].Reset(); g.refinedTarget[i].Reset(); }
        g.detailField.Reset(); g.detailView.Reset(); g.detailTarget.Reset();
        g.judgedTexel.Reset(); g.judgedView.Reset(); g.judgedTarget.Reset();
        g.filmWidth = g.filmHeight = 0;
        g.filmBefore = false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = g.width;
        desc.Height = g.height;
        desc.MipLevels = std::min(g.pictureMips, 5u);   // down to the 16th size the coarse search reads
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        for (int i = 0; i < 2; i++)
            if (FAILED(device->CreateTexture2D(&desc, nullptr, &g.filmFrame[i])) ||
                FAILED(device->CreateShaderResourceView(g.filmFrame[i].Get(), nullptr, &g.filmView[i]))) return false;
        if (!MotionField(device, g.width / 16, g.height / 16, g.coarseField, g.coarseView, g.coarseTarget)) return false;
        for (int i = 0; i < 2; i++)
            if (!MotionField(device, g.width / 4, g.height / 4, g.refinedField[i], g.refinedView[i], g.refinedTarget[i])) return false;
        if (!MotionField(device, 1, 1, g.judgedTexel, g.judgedView, g.judgedTarget) ||
            !MotionField(device, g.width / 2, g.height / 2, g.detailField, g.detailView, g.detailTarget)) { g.detailTarget.Reset(); return false; }
        g.filmWidth = g.width;
        g.filmHeight = g.height;
        printf("render: the films' targets made, %ux%u (motion %ux%u, %ux%u and %ux%u)\n", g.width, g.height, g.width / 16, g.height / 16,
               g.width / 4, g.height / 4, g.width / 2, g.height / 2);
        fflush(stdout);
        return true;
    }

    // Draws through `program` into the target, the sources at t0 (the
    // picture), t1 (the bloom), t2 (the exposure), t4 and t5 (the film's
    // frames), t3 and t6 (motion fields), t7 (the judge's share), t8 or t9
    // (the scene's depth, of one sample or several) and t10 (the
    // occlusion) as given.
    constexpr UINT SourceSlots = 11;
    struct Sources
    {
        ID3D11ShaderResourceView* at[SourceSlots] = {};
    };

    void Draw(ID3D11DeviceContext* context, ID3D11RenderTargetView* target, const D3D11_VIEWPORT& viewport, ID3D11PixelShader* program,
              const Sources& sources)
    {
        ID3D11ShaderResourceView* none[SourceSlots] = {};
        context->PSSetShaderResources(0, SourceSlots, none);
        context->OMSetRenderTargets(1, &target, nullptr);
        context->RSSetViewports(1, &viewport);
        context->PSSetShader(program, nullptr, 0);
        context->PSSetShaderResources(0, SourceSlots, sources.at);
        context->Draw(3, 0);
        context->PSSetShaderResources(0, SourceSlots, none);
        ID3D11RenderTargetView* noTarget = nullptr;
        context->OMSetRenderTargets(1, &noTarget, nullptr);
    }

    // How much it is night, for the surface light, which is set in the
    // world's own draws long before this frame is metered: the metered
    // texel copied each frame into one of four staging texels and read
    // from the oldest when the GPU is done with it, without waiting (a
    // few frames late, which a change of scene over a second allows).
    std::atomic<float> g_night{ 0.0f };
    void ReadNight(ID3D11DeviceContext* context)
    {
        static ComPtr<ID3D11Texture2D> staging[4];
        static int next = 0, filled = 0;
        if (!staging[0])
        {
            D3D11_TEXTURE2D_DESC desc{};
            g.exposure[0]->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Device* device = nullptr;
            context->GetDevice(&device);
            if (device == nullptr) return;
            for (auto& s : staging) device->CreateTexture2D(&desc, nullptr, &s);
            device->Release();
            if (!staging[3]) return;
        }
        context->CopyResource(staging[next].Get(), g.exposure[g.exposureNow].Get());
        next = (next + 1) & 3;
        if (filled < 4) { filled++; return; }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(context->Map(staging[next].Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
        {
            const float metered = *static_cast<const float*>(mapped.pData);
            context->Unmap(staging[next].Get(), 0);
            // The same night as the look's shader: smoothstep(0.03, 0.003).
            const float t = std::clamp((0.03f - metered) / (0.03f - 0.003f), 0.0f, 1.0f);
            g_night.store(t * t * (3.0f - 2.0f * t), std::memory_order_relaxed);
        }
    }

    // Whether the scene's brightness is metered (the adapt pass): for the
    // exposure, and for the night lift of the shadows.
    bool Metering(const Settings::Values& settings)
    {
        return settings.postLook != 0 && (settings.postExposure > 0 || settings.postShadows > 0);
    }

    // The state every pass here shares, and the constants. A film is
    // left at its own exposure (its fades stay fades) and gets the
    // films' sharpening over the look's; with the look off, only that.
    // The look at night (F11, "At night"): the sliders moved from the
    // day's look toward a night one by as much as the scene is metered as
    // night (g_night), so a night map gets its own look and a day map its
    // own, with no switch to see. 0 the day's look throughout; 1 moonlit
    // (the shade and the surfaces brought up further, softer contrast, a
    // touch cooler); 2 as dark as the game (the look eased off).
    Settings::Values AtNight(const Settings::Values& day)
    {
        //  sharpen bloom saturation contrast vignette grain warmth exposure shadows clarity surface occlusion
        static const int nights[2][12] = {
            { 55, 15, 105, 18,  8,  0, -3, 45, 50, 45, 70, 40 },   // moonlit
            {  0,  0, 100,  0,  0,  0,  0,  0,  0,  0,  0,  0 },   // as dark as the game
        };
        if (day.postLook == 0 || day.postLookNight < 1 || day.postLookNight > 2) return day;
        const float t = g_night.load(std::memory_order_relaxed);
        if (t <= 0.0f) return day;
        const int* n = nights[day.postLookNight - 1];
        Settings::Values v = day;
        int* fields[12] = { &v.postSharpen, &v.postBloom, &v.postSaturation, &v.postContrast, &v.postVignette, &v.postGrain,
                            &v.postWarmth, &v.postExposure, &v.postShadows, &v.postClarity, &v.postSurface, &v.postOcclusion };
        for (int i = 0; i < 12; i++) *fields[i] = int(std::lround(*fields[i] + (n[i] - *fields[i]) * t));
        return v;
    }

    void State(ID3D11DeviceContext* context);
    void Begin(ID3D11DeviceContext* context, const Settings::Values& given, bool film, float phase = 0.5f)
    {
        Settings::Values settings = AtNight(given);
        // A film is video graded already: the look's own sharpening, clarity,
        // contrast, colour, bloom and grain stacked on its compression made
        // it harsh ("deep fried"); it keeps only the exposure, the shade
        // lifts and its own light sharpening.
        if (film)
        {
            settings.postSharpen = 0; settings.postClarity = 0; settings.postContrast = 0; settings.postSaturation = 100;
            settings.postBloom = 0; settings.postGrain = 0; settings.postWarmth = 0;
            // And brightened half as far as a level: the films' own dark look kept
            // halfway, which suits them better than lifting them all the way.
            settings.postExposure /= 2; settings.postShadows /= 2;
        }
        const bool look = settings.postLook != 0;
        const float filmSharpen = film ? std::clamp(settings.filmSharpen, 0, 100) / 100.0f : 0.0f;
        // The exposure follows the scene over about a second, whatever the
        // frame rate; a long gap (a load) and it starts from the scene again.
        // A film follows within a fifth of a second: it cuts from a dark
        // shot to a bright one at once, and a second's lag blew the bright
        // one out. The time is since the last metering (Apply's adapt pass):
        // this runs several times a frame (the clean-up, the frames
        // between), and timing each made the step next to nothing.
        const float seconds = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() - g.last).count(), 0.0f, 1.0f);
        const float step = 1.0f - std::exp(-seconds * (film ? 12.0f : 2.5f));
        const float levels = std::log2(float(std::max(g.width, g.height)));
        auto amount = [look](int value, int low, int high, int neutral) { return float(look ? std::clamp(value, low, high) : neutral); };
        // CAS stays stable up to 1.25 (its weight above -1/4).
        const float sharpen = std::max(amount(settings.postSharpen, 0, 100, 0) / 100.0f, filmSharpen * 0.6f);
        const float constants[36] = {
            1.0f / g.width, 1.0f / g.height, float(g.width) / float(g.height), float(g.frame++ & 1023),
            1.0f / g.bloomWidth, 1.0f / g.bloomHeight, 0.55f, 0.25f,
            sharpen, amount(settings.postBloom, 0, 100, 0) / 100.0f * 1.5f, amount(settings.postSaturation, 0, 200, 100) / 100.0f, amount(settings.postContrast, 0, 100, 0) / 100.0f * 0.75f,
            amount(settings.postVignette, 0, 100, 0) / 100.0f, amount(settings.postGrain, 0, 100, 0) / 100.0f, amount(settings.postWarmth, -50, 50, 0) / 100.0f,
            Metering(settings) ? (film ? 2.0f : 1.0f) : 0.0f,
            // The exposure meters at about 16 texels across; the shadow lift
            // looks at about a thirtieth of the picture around, the clarity a
            // few pixels.
            1.0f + amount(settings.postExposure, 0, 100, 0) / 100.0f * 1.5f, amount(settings.postShadows, 0, 100, 0) / 100.0f,
            amount(settings.postClarity, 0, 100, 0) / 100.0f, std::max(0.0f, levels - 4.0f),
            step, std::max(0.0f, levels - 5.5f), 2.0f, filmSharpen,
            film ? std::clamp(settings.filmClean, 0, 100) / 100.0f : 0.0f, phase,
            film ? std::clamp(settings.filmShadows, 0, 100) / 100.0f : 0.0f, film ? 1.0f : 0.0f };
        context->UpdateSubresource(g.constants.Get(), 0, nullptr, constants, 0, 0);
        State(context);
    }

    void State(ID3D11DeviceContext* context)
    {
        ID3D11ShaderResourceView* none[32] = {};
        context->PSSetShaderResources(0, 32, none);
        context->VSSetShaderResources(0, 32, none);
        context->RSSetState(g.rasterizer.Get());
        context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(nullptr, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->IASetInputLayout(nullptr);
        context->VSSetShader(g.vertexShader.Get(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        ID3D11Buffer* buffers[1] = { g.constants.Get() };
        context->VSSetConstantBuffers(0, 1, buffers);
        context->PSSetConstantBuffers(0, 1, buffers);
        ID3D11SamplerState* samplers[1] = { g.sampler.Get() };
        context->PSSetSamplers(0, 1, samplers);
    }

    D3D11_VIEWPORT Whole(uint32_t width, uint32_t height)
    {
        return D3D11_VIEWPORT{ 0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f };
    }
}

bool RenderPost::Active(const Settings::Values& settings, bool film)
{
    return settings.postLook != 0 || (film && (settings.filmSmooth || settings.filmSharpen > 0));
}

ID3D11RenderTargetView* RenderPost::Target(ID3D11Device* device, CompileFunction compile, uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0 || !Programs(device, compile)) return nullptr;
    return Targets(device, width, height) ? g.pictureTarget.Get() : nullptr;
}

void RenderPost::FilmClean(ID3D11DeviceContext* context, const Settings::Values& settings, bool playing)
{
    if (!playing || settings.filmClean <= 0 || !g.clean || !g.pictureView) return;
    if (!g.cleanedTarget)
    {
        D3D11_TEXTURE2D_DESC desc{};
        g.picture->GetDesc(&desc);
        desc.MipLevels = 1;
        desc.MiscFlags = 0;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ID3D11Device* device = nullptr;
        context->GetDevice(&device);
        if (device == nullptr) return;
        const bool made = SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &g.cleaned)) &&
                          SUCCEEDED(device->CreateRenderTargetView(g.cleaned.Get(), nullptr, &g.cleanedTarget));
        device->Release();
        if (!made) { g.cleaned.Reset(); g.cleanedTarget.Reset(); return; }
    }
    Begin(context, settings, true);
    Sources sources;
    sources.at[0] = g.pictureView.Get();
    Draw(context, g.cleanedTarget.Get(), Whole(g.width, g.height), g.clean.Get(), sources);
    context->CopySubresourceRegion(g.picture.Get(), 0, 0, 0, 0, g.cleaned.Get(), 0, nullptr);
}

bool RenderPost::FilmFrame(ID3D11DeviceContext* context, const Settings::Values& settings, bool playing, float phase)
{
    if (!playing || !settings.filmSmooth || !g.between || !g.pictureView)
    {
        g.filmBefore = false;
        g.filmInterval = 0.0;
        return false;
    }
    ID3D11Device* device = nullptr;
    context->GetDevice(&device);
    const bool made = device != nullptr && FilmTargets(device);
    if (device != nullptr) device->Release();
    if (!made) return false;

    // The time since the film's last swap, and its average: about a 30th.
    const auto now = std::chrono::steady_clock::now();
    const double since = g.filmBefore ? std::chrono::duration<double>(now - g.filmSwap).count() : 0.0;
    g.filmSwap = now;
    // A long gap (a film started again, a stall) starts the average again.
    if (!g.filmBefore || since > 0.1) g.filmInterval = 0.0;
    else g.filmInterval = g.filmInterval > 0.0 ? g.filmInterval + (since - g.filmInterval) * 0.2 : since;

    // The new frame kept, with its mips.
    const int before = g.filmNow;
    g.filmNow ^= 1;
    context->CopySubresourceRegion(g.filmFrame[g.filmNow].Get(), 0, 0, 0, 0, g.picture.Get(), 0, nullptr);
    context->GenerateMips(g.filmView[g.filmNow].Get());
    const bool had = g.filmBefore;
    g.filmBefore = true;
    // Only between two frames a film's 30th apart: not across a stall,
    // and not for one already at 60.
    if (!had || g.filmInterval < 0.022 || g.filmInterval > 0.050 || since < 0.015 || since > 0.070) return false;

    Begin(context, settings, true, phase);
    Sources sources;
    sources.at[4] = g.filmView[before].Get();
    sources.at[5] = g.filmView[g.filmNow].Get();
    Draw(context, g.coarseTarget.Get(), Whole(std::max(1u, g.width / 16), std::max(1u, g.height / 16)), g.coarse.Get(), sources);
    // The refined field with the last pair's beside it, the detail field
    // from it, and the frame between from the detail and refined fields.
    const int refinedBefore = g.refinedNow;
    g.refinedNow ^= 1;
    sources.at[6] = g.coarseView.Get();
    sources.at[3] = g.refinedView[refinedBefore].Get();
    Draw(context, g.refinedTarget[g.refinedNow].Get(), Whole(std::max(1u, g.width / 4), std::max(1u, g.height / 4)), g.refine.Get(), sources);
    sources.at[6] = g.refinedView[g.refinedNow].Get();
    sources.at[3] = nullptr;
    Draw(context, g.detailTarget.Get(), Whole(std::max(1u, g.width / 2), std::max(1u, g.height / 2)), g.detail.Get(), sources);
    sources.at[6] = g.detailView.Get();
    Draw(context, g.judgedTarget.Get(), Whole(1, 1), g.judge.Get(), sources);
    sources.at[7] = g.judgedView.Get();
    // COD3_FILMDEBUG=1: the judge's figures for each frame between, read
    // back at once (it waits for the GPU: for finding the thresholds only).
    static const bool debug = getenv("COD3_FILMDEBUG") != nullptr;
    if (debug)
    {
        static ComPtr<ID3D11Texture2D> staging;
        if (!staging)
        {
            D3D11_TEXTURE2D_DESC desc{};
            g.judgedTexel->GetDesc(&desc);
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Device* device = nullptr;
            context->GetDevice(&device);
            if (device != nullptr) { device->CreateTexture2D(&desc, nullptr, &staging); device->Release(); }
        }
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (staging)
        {
            context->CopyResource(staging.Get(), g.judgedTexel.Get());
            if (SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            {
                const uint16_t* h = static_cast<const uint16_t*>(mapped.pData);
                auto half = [](uint16_t v) { const int e = (v >> 10) & 31, m = v & 1023; const float f = e == 0 ? m / 1024.0f * 6.1035e-5f : std::ldexp(1.0f + m / 1024.0f, e - 15); return (v & 0x8000) ? -f : f; };
                printf("film: judged %.4f %.4f %.4f mean cost %.4f\n", half(h[0]), half(h[1]), half(h[2]), half(h[3]));
                context->Unmap(staging.Get(), 0);
            }
        }
    }
    sources.at[3] = g.refinedView[g.refinedNow].Get();
    Draw(context, g.pictureTarget.Get(), Whole(g.width, g.height), g.between.Get(), sources);
    return true;
}

void RenderPost::FilmBetween(ID3D11DeviceContext* context, const Settings::Values& settings, float phase)
{
    if (!g.between || !g.filmView[0] || !g.filmView[1]) return;
    Begin(context, settings, true, phase);
    Sources sources;
    sources.at[4] = g.filmView[g.filmNow ^ 1].Get();
    sources.at[5] = g.filmView[g.filmNow].Get();
    sources.at[6] = g.detailView.Get();
    sources.at[7] = g.judgedView.Get();
    sources.at[3] = g.refinedView[g.refinedNow].Get();
    Draw(context, g.pictureTarget.Get(), Whole(g.width, g.height), g.between.Get(), sources);
}

ID3D11ShaderResourceView* RenderPost::Picture()
{
    return g.pictureView.Get();
}

double RenderPost::FilmFrameInterval()
{
    return g.filmInterval;
}

void RenderPost::FilmShowNew(ID3D11DeviceContext* context)
{
    if (g.filmFrame[g.filmNow]) context->CopySubresourceRegion(g.picture.Get(), 0, 0, 0, 0, g.filmFrame[g.filmNow].Get(), 0, nullptr);
}

void RenderPost::Apply(ID3D11DeviceContext* context, const Settings::Values& settings, bool film,
                       ID3D11RenderTargetView* window, const D3D11_VIEWPORT& viewport)
{
    if (!g.finish || !g.pictureView) return;
    Begin(context, settings, film);
    context->GenerateMips(g.pictureView.Get());
    const bool look = settings.postLook != 0;
    // The scene is metered in films too, for the night lift; only its
    // exposure gain is left out of them.
    const bool exposure = Metering(settings);
    const bool bloom = look && settings.postBloom > 0;
    if (exposure)
    {
        const int before = g.exposureNow;
        g.exposureNow ^= 1;
        Sources sources;
        sources.at[0] = g.pictureView.Get();
        sources.at[2] = g.exposureView[before].Get();
        Draw(context, g.exposureTarget[g.exposureNow].Get(), Whole(1, 1), g.adapt.Get(), sources);
        g.last = std::chrono::steady_clock::now();
        ReadNight(context);
    }
    if (bloom)
    {
        Sources sources;
        sources.at[0] = g.pictureView.Get();
        Draw(context, g.bloomTarget.Get(), Whole(g.bloomWidth, g.bloomHeight), g.bright.Get(), sources);
        context->GenerateMips(g.bloomView.Get());
    }
    Sources sources;
    sources.at[0] = g.pictureView.Get();
    sources.at[1] = g.bloomView.Get();
    sources.at[2] = g.exposureView[g.exposureNow].Get();
    Draw(context, window, viewport, g.finish.Get(), sources);
}

void RenderPost::ApplyPreset(Settings::Values& v, int look)
{
    //  sharpen bloom saturation contrast vignette grain warmth exposure shadows clarity surface occlusion
    static const int presets[6][12] = {
        {  0,  0, 100,  0,  0,  0,  0,  0,  0,  0,  0,  0 },   // (off)
        { 60, 15, 112, 20, 10,  0,  4, 45, 40, 50, 60, 50 },   // Remastered: the dark maps brought up, the shade opened, the detail drawn out
        { 55,  0, 110, 15,  0,  0,  0, 30, 30, 30, 35, 40 },   // Clean: crisper, a touch brighter, a touch more colour
        { 40, 35, 105, 30, 25,  0,  6, 25, 25, 25, 30, 60 },   // Cinematic: glow, deeper contrast, a soft vignette
        { 35, 25,  70, 35, 35, 25, 15, 25, 30, 30, 30, 55 },   // War film: muted and warm, with grain
        { 50, 20, 135, 20, 15,  0,  0, 35, 35, 35, 40, 45 },   // Vivid: strong colour
    };
    if (look < 1 || look > 5) return;
    const int* p = presets[look];
    v.postSharpen = p[0]; v.postBloom = p[1]; v.postSaturation = p[2]; v.postContrast = p[3];
    v.postVignette = p[4]; v.postGrain = p[5]; v.postWarmth = p[6];
    v.postExposure = p[7]; v.postShadows = p[8]; v.postClarity = p[9]; v.postSurface = p[10]; v.postOcclusion = p[11];
}

uint32_t RenderPost::SurfaceLight(const Settings::Values& settings)
{
    // At night only, by as much as it is night: a day map is bright enough
    // and its surfaces would wash out. 100 on the slider at full night is
    // a gain of 2 on the darkest surfaces' light (the level over 50).
    if (settings.postLook == 0 || !Metering(settings)) return 0;
    const Settings::Values night = AtNight(settings);
    return uint32_t(std::lround(std::clamp(night.postSurface, 0, 100) * g_night.load(std::memory_order_relaxed)));
}

void RenderPost::Occlusion(ID3D11DeviceContext* context, const Settings::Values& settings, ID3D11RenderTargetView* color,
                           ID3D11ShaderResourceView* depth, uint32_t samples, float x0, float y0, uint32_t width, uint32_t height, bool reversed)
{
    const int occlusionAmount = AtNight(settings).postOcclusion;
    if (settings.postLook == 0 || occlusionAmount <= 0 || !g.occlusion || color == nullptr || depth == nullptr || width < 16 || height < 16) return;
    if (!g.occlusionTarget || g.occlusionWidth != width || g.occlusionHeight != height)
    {
        g.occlusionMap.Reset(); g.occlusionView.Reset(); g.occlusionTarget.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ID3D11Device* device = nullptr;
        context->GetDevice(&device);
        if (device == nullptr) return;
        const bool made = SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &g.occlusionMap)) &&
                          SUCCEEDED(device->CreateShaderResourceView(g.occlusionMap.Get(), nullptr, &g.occlusionView)) &&
                          SUCCEEDED(device->CreateRenderTargetView(g.occlusionMap.Get(), nullptr, &g.occlusionTarget));
        device->Release();
        if (!made) { g.occlusionTarget.Reset(); return; }
        g.occlusionWidth = width;
        g.occlusionHeight = height;
    }
    // The title's cg_fov is the horizontal field of view of a 4:3 picture
    // (a wider one sees more at the sides): half its tangent, by 3/4, is
    // the vertical one's. The radius is in units of the near plane (about
    // four inches): some two and a half feet. COD3_AORADIUS for another.
    const float tanHalf = std::tan(std::clamp(settings.fov, 65, 100) * 0.5f * 3.14159265f / 180.0f) * 0.75f;
    static const float radius = getenv("COD3_AORADIUS") ? float(atof(getenv("COD3_AORADIUS"))) : 8.0f;
    // COD3_AOSHOW=2: the depth instead (its distance's log over 16, in units of the near plane), for checking it is read the right way round.
    static const bool depthShow = getenv("COD3_AOSHOW") != nullptr && getenv("COD3_AOSHOW")[0] == '2';
    static int told = 0;
    if (told++ == 0) { printf("render: ambient occlusion over %ux%u at %.0f,%.0f, %u samples, depth %s\n", width, height, x0, y0, samples, reversed ? "reversed" : "near 0"); fflush(stdout); }
    const float constants[36] = {
        1.0f / width, 1.0f / height, float(width) / float(height), 0.0f,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0.0f, 0.5f, getenv("COD3_AOSHOW") != nullptr ? 1.0f : std::clamp(occlusionAmount, 0, 100) / 100.0f, 0.0f,
        x0, y0, tanHalf, reversed ? 1.0f : 0.0f,
        float(samples), radius, depthShow ? 1.0f : 0.0f, 0.0f };
    context->UpdateSubresource(g.constants.Get(), 0, nullptr, constants, 0, 0);
    State(context);
    Sources sources;
    sources.at[samples > 1 ? 9 : 8] = depth;
    Draw(context, g.occlusionTarget.Get(), Whole(width, height), g.occlusion.Get(), sources);
    sources.at[10] = g.occlusionView.Get();
    // COD3_AOSHOW=1: the occlusion itself in the scene's place, for seeing it.
    static const bool show = getenv("COD3_AOSHOW") != nullptr;
    context->OMSetBlendState(show ? nullptr : g.multiply.Get(), nullptr, 0xFFFFFFFF);
    Draw(context, color, D3D11_VIEWPORT{ x0, y0, float(width), float(height), 0.0f, 1.0f }, g.occlusionApply.Get(), sources);
    context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
}
