#include "pixels.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace windowpane
{
namespace
{
void ValidateSize(int width, int height)
{
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
    {
        throw std::invalid_argument("Image dimensions must be between 1 and 4096.");
    }
}

void ValidateSource(std::span<const Pixel> source, int width, int height)
{
    ValidateSize(width, height);
    if (source.size() != static_cast<std::size_t>(width) * height)
    {
        throw std::invalid_argument("Pixel buffer does not match image dimensions.");
    }
}

std::uint8_t Scale(std::uint8_t value, double factor)
{
    return static_cast<std::uint8_t>(std::lround(value * factor));
}
} // namespace

Rect SelectRect(int x1, int y1, int x2, int y2, Rect bounds)
{
    if (bounds.width <= 0 || bounds.height <= 0)
    {
        throw std::invalid_argument("Selection bounds are empty.");
    }
    const int left = std::clamp(std::min(x1, x2), bounds.x, bounds.x + bounds.width);
    const int top = std::clamp(std::min(y1, y2), bounds.y, bounds.y + bounds.height);
    const int right = std::clamp(std::max(x1, x2), bounds.x, bounds.x + bounds.width);
    const int bottom = std::clamp(std::max(y1, y2), bounds.y, bounds.y + bounds.height);
    return {left, top, right - left, bottom - top};
}

bool ValidCrop(Rect crop, int sourceWidth, int sourceHeight)
{
    return crop.x >= 0 && crop.y >= 0 && crop.width >= 8 && crop.height >= 8 && crop.width <= 4096 &&
           crop.height <= 4096 && crop.x <= sourceWidth && crop.y <= sourceHeight &&
           crop.width <= sourceWidth - crop.x && crop.height <= sourceHeight - crop.y;
}

Rect FitContent(int sourceWidth, int sourceHeight, int width, int height)
{
    ValidateSize(sourceWidth, sourceHeight);
    ValidateSize(width, height);
    const double scale =
        std::min(static_cast<double>(width) / sourceWidth, static_cast<double>(height) / sourceHeight);
    const int fittedWidth = std::max(1, static_cast<int>(std::floor(sourceWidth * scale)));
    const int fittedHeight = std::max(1, static_cast<int>(std::floor(sourceHeight * scale)));
    return {(width - fittedWidth) / 2, (height - fittedHeight) / 2, fittedWidth, fittedHeight};
}

Pixel StraightColor(Pixel pixel)
{
    if (!pixel.a)
    {
        return {};
    }
    const auto channel = [alpha = pixel.a](std::uint8_t value) {
        return static_cast<std::uint8_t>(std::min(255, (value * 255 + alpha / 2) / alpha));
    };
    return {channel(pixel.b), channel(pixel.g), channel(pixel.r), 255};
}

Pixel ApplyAppearance(Pixel pixel, const Appearance& appearance)
{
    double coverage = 1.0;
    if (appearance.keyEnabled)
    {
        const Pixel color = StraightColor(pixel);
        const double distance =
            std::max({std::abs(color.r - appearance.key.r), std::abs(color.g - appearance.key.g),
                      std::abs(color.b - appearance.key.b)}) *
            100.0 / 255.0;
        const double tolerance = std::clamp(appearance.tolerance, 0, 100);
        const double softness = std::clamp(appearance.softness, 0, 100);
        coverage = distance <= tolerance ? 0.0
                   : softness == 0       ? 1.0
                                         : std::min(1.0, (distance - tolerance) / softness);
    }
    const double factor = coverage * std::clamp(appearance.opacity, 0, 100) / 100.0;
    return {Scale(pixel.b, factor), Scale(pixel.g, factor), Scale(pixel.r, factor), Scale(pixel.a, factor)};
}

std::vector<Pixel> Compose(std::span<const Pixel> source, int sourceWidth, int sourceHeight, int width,
                           int height, const Appearance& appearance)
{
    ValidateSource(source, sourceWidth, sourceHeight);
    const Rect content = FitContent(sourceWidth, sourceHeight, width, height);
    std::vector<Pixel> output(static_cast<std::size_t>(width) * height);
    for (int y = 0; y < content.height; ++y)
    {
        const auto sy = static_cast<std::size_t>(y) * sourceHeight / content.height;
        for (int x = 0; x < content.width; ++x)
        {
            const auto sx = static_cast<std::size_t>(x) * sourceWidth / content.width;
            output[static_cast<std::size_t>(y + content.y) * width + x + content.x] =
                ApplyAppearance(source[sy * sourceWidth + sx], appearance);
        }
    }
    return output;
}

Pixel Sample(std::span<const Pixel> source, int sourceWidth, int sourceHeight, int width, int height, int x,
             int y)
{
    ValidateSource(source, sourceWidth, sourceHeight);
    const Rect content = FitContent(sourceWidth, sourceHeight, width, height);
    if (x < content.x || y < content.y || x >= content.x + content.width || y >= content.y + content.height)
    {
        throw std::invalid_argument("Select a pixel inside the mirrored content.");
    }
    const auto sx = static_cast<std::size_t>(x - content.x) * sourceWidth / content.width;
    const auto sy = static_cast<std::size_t>(y - content.y) * sourceHeight / content.height;
    return StraightColor(source[sy * sourceWidth + sx]);
}
} // namespace windowpane
