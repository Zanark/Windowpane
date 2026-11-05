#include "pixels.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace windowpane;

namespace
{
int checks{};
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
} // namespace

int main()
{
    try
    {
        const Rect selection = SelectRect(180, 90, -20, -40, {0, 0, 150, 100});
        Check(selection.x == 0 && selection.y == 0 && selection.width == 150 && selection.height == 90,
              "Reversed crop clips to source.");
        const Rect negative = SelectRect(-900, -100, -500, 60, {-800, -200, 500, 400});
        Check(negative.x == -800 && negative.width == 300, "Negative monitor coordinates work.");
        Check(!ValidCrop({0, 0, 7, 8}, 100, 100), "Tiny crops reject.");
        Check(!ValidCrop({95, 0, 8, 8}, 100, 100), "Out-of-source crop rejects.");
        Check(ValidCrop({92, 92, 8, 8}, 100, 100), "Exact source edge accepts.");

        Appearance appearance;
        appearance.opacity = 50;
        const Pixel white = ApplyAppearance({255, 255, 255, 255}, appearance);
        Check(white.a == 128 && white.r == 128, "Opacity uses premultiplied alpha.");
        const Pixel translucent = ApplyAppearance({64, 0, 128, 128}, appearance);
        Check(translucent.r == 64 && translucent.a == 64, "Existing alpha is not multiplied twice.");
        appearance.opacity = 100;
        appearance.keyEnabled = true;
        appearance.key = {200, 100, 20, 255};
        appearance.tolerance = 0;
        appearance.softness = 0;
        Check(ApplyAppearance(appearance.key, appearance).a == 0, "Exact sampled key disappears.");
        Check(ApplyAppearance({255, 255, 255, 255}, appearance).a == 255, "Non-key text stays opaque.");
        appearance.tolerance = 4;
        appearance.softness = 8;
        const Pixel feather = ApplyAppearance({220, 100, 20, 255}, appearance);
        Check(feather.a > 0 && feather.a < 255, "Soft edge has fractional alpha.");
        appearance.opacity = 0;
        Check(ApplyAppearance({255, 255, 255, 255}, appearance).a == 0, "Zero opacity hides all pixels.");
        Check(StraightColor({64, 0, 128, 128}).r == 255, "Color picker unpremultiplies source.");
        Check(StraightColor({0, 0, 0, 0}).a == 0, "Transparent sample does not divide by zero.");

        const std::vector<Pixel> source{{255, 0, 0, 255}, {0, 0, 255, 255}};
        appearance = {};
        auto output = Compose(source, 2, 1, 4, 4, appearance);
        Check(output[0].a == 0 && output[4].b == 255 && output[7].r == 255 && output[15].a == 0,
              "Scale preserves aspect with transparent margins.");
        Check(Sample(source, 2, 1, 4, 4, 3, 1).r == 255, "Picker maps scaled content to original source.");
        bool rejected = false;
        try
        {
            static_cast<void>(Sample(source, 2, 1, 4, 4, 0, 0));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        Check(rejected, "Picker rejects letterbox instead of sampling desktop.");
        rejected = false;
        try
        {
            static_cast<void>(Compose(source, 2, 2, 4, 4, appearance));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        Check(rejected, "Malformed frame buffer rejects explicitly.");
        rejected = false;
        try
        {
            static_cast<void>(Compose(source, 2, 1, 5000, 4, appearance));
        }
        catch (const std::invalid_argument&)
        {
            rejected = true;
        }
        Check(rejected, "Output dimensions have a memory bound.");
        std::cout << checks << " pixel/crop checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Pixel test failed: " << error.what() << '\n';
        return 1;
    }
}
