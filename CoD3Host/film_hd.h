#pragma once

// Remastered films: a film's frames from a file beside it (forest.wmv's
// forest.c3film), made off line at a higher resolution and frame rate
// (tools\remaster-films.py), shown in place of the title's own while it
// plays them, kept in step with them by their look. The title still plays
// its film, its sound and its timing; only the picture is replaced. See
// film_hd.cpp.

#include "render_post.h"

#include <cstdint>
#include <filesystem>

namespace FilmHd
{
    // The title opened a file of a film (kernel_file.cpp): its .wmv's
    // remastered frames, if there is a file of them, are used while it plays.
    void Opened(const std::filesystem::path& hostPath);

    // At each of the title's swaps, with the frame it showed in `picture`
    // (the look's target, width by height): the remastered film kept in
    // step with it. True when this swap's remastered frames can be shown;
    // then Draw() makes them in place of the title's and the made frames
    // between. `playing` false ends it.
    bool Swap(ID3D11Device* device, ID3D11DeviceContext* context, RenderPost::CompileFunction compile,
              ID3D11ShaderResourceView* picture, uint32_t width, uint32_t height, bool playing);

    // Whether the last swap's frames are the remastered film's.
    bool Active();

    // The remastered frame `phase` of the way from the film's frame before
    // the last swap's to the last swap's own (1), into the target (width by
    // height), through the display's colour table as the title's picture
    // goes. False when it is not decoded yet: the target is left as it is.
    bool Draw(ID3D11DeviceContext* context, float phase, ID3D11RenderTargetView* target, uint32_t width, uint32_t height,
              ID3D11ShaderResourceView* gammaTable);
}
