#pragma once

// The picture's look (F11, "Look"): auto exposure, a shadow lift, clarity,
// bloom, sharpening, a colour grade, a vignette and film grain, on the
// presented picture as the console would send it out (after its colour
// table), before the overlay. And the films (F11, "Smooth films"): the
// frame between two of a film's 30 a second made from the motion between
// them, so they play at 60, and sharpened. See render_post.cpp.

#include "settings.h"

#include <cstdint>

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

namespace RenderPost
{
    typedef HRESULT (WINAPI* CompileFunction)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
        ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

    // Whether the settings ask for any of it, with a film playing or not.
    bool Active(const Settings::Values& settings, bool film);

    // The target the present pass draws the picture into instead of the
    // window, of the picture's size; null when it is all off or the
    // target could not be made (then the present goes to the window as
    // before).
    ID3D11RenderTargetView* Target(ID3D11Device* device, CompileFunction compile, uint32_t width, uint32_t height);

    // At each of the title's swaps, with the new picture in the target:
    // the film's frames kept, and, with a film playing at about 30 a
    // second and a frame before this one, the frame halfway between the
    // two made in the target in the new one's place. True when it was:
    // then the target is shown (Apply), FilmFrameInterval() / 2 is waited,
    // and FilmShowNew() puts the new frame back for its own showing.
    // `phase` is where the frame made lies between them: 0.5 halfway; for
    // a screen of more than 60 frames a second, 1/n, then FilmBetween()
    // makes the frames at 2/n .. (n-1)/n, each shown in turn.
    bool FilmFrame(ID3D11DeviceContext* context, const Settings::Values& settings, bool playing, float phase = 0.5f);
    void FilmBetween(ID3D11DeviceContext* context, const Settings::Values& settings, float phase);
    double FilmFrameInterval();   // seconds between the film's frames, as measured
    void FilmShowNew(ID3D11DeviceContext* context);

    // A film's frame in the target, as the present pass drew it, cleaned
    // of the compression's banding and blocks (F11, "Film clean-up"),
    // before anything reads it.
    void FilmClean(ID3D11DeviceContext* context, const Settings::Values& settings, bool playing);

    // The ambient occlusion (F11, "Ambient occlusion"), drawn over the
    // scene's colour target as the title resolves the scene's depth:
    // after the solid world, before the smoke, the effects and the HUD.
    // The rectangle is the resolve's, in the target's pixels.
    void Occlusion(ID3D11DeviceContext* context, const Settings::Values& settings, ID3D11RenderTargetView* color,
                   ID3D11ShaderResourceView* depth, uint32_t samples, float x0, float y0, uint32_t width, uint32_t height, bool reversed);

    // The target's picture, for reading (the remastered films' thumbnails).
    ID3D11ShaderResourceView* Picture();

    // The picture in the target, with the look, onto the window's view
    // in the viewport. Leaves nothing bound.
    void Apply(ID3D11DeviceContext* context, const Settings::Values& settings, bool film,
               ID3D11RenderTargetView* window, const D3D11_VIEWPORT& viewport);

    // The surface light's level for this frame's world draws (0 .. 100):
    // the slider, by as much as the scene was metered as night.
    uint32_t SurfaceLight(const Settings::Values& settings);

    // A look's values (1 Remastered, 2 Clean, 3 Cinematic, 4 War film,
    // 5 Vivid) into the settings' sliders; 0 and 6 (custom) leave them.
    void ApplyPreset(Settings::Values& settings, int look);
}
