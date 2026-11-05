#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace windowpane
{
struct Rect
{
    int x{};
    int y{};
    int width{};
    int height{};
};

struct Pixel
{
    std::uint8_t b{};
    std::uint8_t g{};
    std::uint8_t r{};
    std::uint8_t a{};
};
static_assert(sizeof(Pixel) == 4);

struct Appearance
{
    int opacity{100};
    bool keyEnabled{};
    Pixel key{255, 128, 0, 255};
    int tolerance{8};
    int softness{5};
};

Rect SelectRect(int x1, int y1, int x2, int y2, Rect bounds);
bool ValidCrop(Rect crop, int sourceWidth, int sourceHeight);
Rect FitContent(int sourceWidth, int sourceHeight, int width, int height);
Pixel StraightColor(Pixel pixel);
Pixel ApplyAppearance(Pixel premultiplied, const Appearance& appearance);
std::vector<Pixel> Compose(std::span<const Pixel> source, int sourceWidth, int sourceHeight, int width,
                           int height, const Appearance& appearance);
Pixel Sample(std::span<const Pixel> source, int sourceWidth, int sourceHeight, int width, int height, int x,
             int y);
} // namespace windowpane
