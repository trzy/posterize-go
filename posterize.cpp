/*
 * posterize.cpp
 * Bart Trzynadlowski, 5/15/2024
 * 
 * Image posterization: converts RGB images to 4-bit palettized images for display on Frame.
 */

#include <cstdint>
#include <cstdlib>
#include <memory>
#include <random>

struct PaletteValue
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    // Perceived luminance according to ITU BT.601.
    float luminance() const
    {
        // See: http://www.itu.int/rec/R-REC-BT.601 and https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
        return 0.299f * (float(r) / 255.0f) + 0.587f * (float(g) / 255.0f) + 0.114f * (float(b) / 255.0f);
    }
};

static void setDarkestColorToBlackAndIndex0(PaletteValue palette[], uint8_t *image4bit, size_t numBytes)
{
    // Find darkest color
    float darkestLuma = 1.0f;
    size_t darkestColor = 0;
    for (size_t i = 0; i < 16; i++)
    {
        float luma = palette[i].luminance();
        if (luma < darkestLuma)
        {
            darkestLuma = luma;
            darkestColor = i;
        }
    }

    // Make darkest color fully black
    palette[darkestColor].r = 0;
    palette[darkestColor].g = 0;
    palette[darkestColor].b = 0;

    // Swap with color 0 so that color 0 is black
    if (0 == darkestColor)
    {
        return;
    }
    PaletteValue tmp = palette[0];
    palette[0] = palette[darkestColor];
    palette[darkestColor] = tmp;

    // Construct a LUT that swaps occurrences of 0 <-> darkestColor for each 4-bit pixel
    uint8_t lut[256];
    for (size_t i = 0; i < 256; i++)
    {
        lut[i] = i;
    }
    lut[0x00 | 0x00] = (darkestColor << 4) | darkestColor;  // (0, 0) -> (darkestColor, darkestColor)
    lut[0x00 | darkestColor] = (darkestColor << 4) | 0x00;  // (0, darkestColor) -> (darkestColor, 0)
    lut[(darkestColor << 4) | 0x00] = 0x00 | darkestColor;  // (darkestColor, 0) -> (0, darkestColor)
    lut[(darkestColor << 4) | darkestColor] = 0x00;         // (darkestColor, darkestColor) -> (0, 0)

    // Remap pixels using the LUT
    for (size_t i = 0; i < numBytes; i++)
    {
        image4bit[i] = lut[image4bit[i]];
    }
}

extern "C"
{
    void posterize(uint8_t *image4bit, uint8_t *palette24bit, const uint8_t *rgbaIn, size_t numPixels)
    {
        // Palette
        size_t numColors = 16;
        PaletteValue palette[numColors];

        // Make a local copy of RGBA buffer so we can safely clobber alpha channel
        size_t numBytes = numPixels * 4;
        std::unique_ptr<uint8_t[]> rgba = std::make_unique<uint8_t[]>(numBytes);
        memcpy(rgba.get(), rgbaIn, numBytes);

        // Randomize assignment of pixels to the k clusters, using alpha channel
        std::random_device dev;
        std::mt19937 rng(dev());
        std::uniform_int_distribution<std::mt19937::result_type> random(0, unsigned(numColors - 1));   // [0, numColors-1]
        for (size_t i = 0; i < numBytes; i += 4)
        {
            rgba[i + 3] = random(rng);
        }

        // Centroid for each color cluster (mean RGB value)
        struct Color
        {
            uint64_t r = 0;
            uint64_t g = 0;
            uint64_t b = 0;
        };
        Color centroids[numColors];
        int numPixelsInCluster[numColors];

        // Repeat k-means until complete
        size_t maxIterations = 24;
        size_t iterations = 0;
        bool didChange = false;
        do {
            didChange = false;

            // Compute average for each cluster
            for (size_t i = 0; i < numColors; i++)
            {
                centroids[i] = Color(); // zero out
                numPixelsInCluster[i] = 0;
            }
            for (size_t i = 0; i < numBytes; i += 4)
            {
                size_t k = rgba[i + 3];
                centroids[k].r += rgba[i + 0];
                centroids[k].g += rgba[i + 1];
                centroids[k].b += rgba[i + 2];
                numPixelsInCluster[k]++;
            }
            for (size_t i = 0; i < numColors; i++)
            {
                if (numPixelsInCluster[i] != 0)
                {
                    centroids[i].r /= numPixelsInCluster[i];
                    centroids[i].g /= numPixelsInCluster[i];
                    centroids[i].b /= numPixelsInCluster[i];
                }
            }

            // Assign each pixel to nearest cluster (cluster whose centroid is nearest)
            for (size_t i = 0; i < numBytes; i += 4)
            {
                // Compute distance^2 to each cluster centroid
                uint64_t distance[numColors];
                for (size_t j = 0; j < numColors; j++)
                {
                    uint64_t dr = centroids[j].r - rgba[i + 0];
                    uint64_t dg = centroids[j].g - rgba[i + 1];
                    uint64_t db = centroids[j].b - rgba[i + 2];
                    distance[j] = dr * dr + dg * dg + db * db;
                }

                // Which is nearest?
                size_t bestK = 0;
                size_t nearestDistance = distance[0];
                for (size_t j = 1; j < numColors; j++)
                {
                    if (distance[j] < nearestDistance)
                    {
                        nearestDistance = distance[j];
                        bestK = j;
                    }
                }

                // Did an assignment change?
                didChange |= rgba[i + 3] != bestK;

                // Assign
                rgba[i + 3] = bestK;
            }

            iterations++;
        } while (didChange && iterations < maxIterations);

        // Create palette
        for (size_t i = 0; i < numColors; i++)
        {
            palette[i] = { .r = uint8_t(centroids[i].r), .g = uint8_t(centroids[i].g), .b = uint8_t(centroids[i].b) };
        }

        // Assign colors to output pixels
        for (size_t i = 0; i < numPixels; i++)
        {
            uint8_t colorIdx = rgba[i * 4 + 3]; // color is just the cluster index, k
            size_t byteIdx = i / 2;
            size_t shiftAmount = (~i & 1) * 4;  // even pixel in high nibble, odd in low
            uint8_t mask = 0xf0 >> shiftAmount; // mask if reverse: mask off low nibble when writing high nibble, etc.
            image4bit[byteIdx] = (image4bit[byteIdx] & mask) | (colorIdx << shiftAmount);
        }

        // Force darkest color to black and make that color index 0. On Frame, color 0 is
        // transparent.
        setDarkestColorToBlackAndIndex0(palette, image4bit, numPixels / 2);

        // Copy out the palette
        for (size_t i = 0; i < numColors; i++)
        {
            palette24bit[i * 3 + 0] = palette[i].r;
            palette24bit[i * 3 + 1] = palette[i].g;
            palette24bit[i * 3 + 2] = palette[i].b;
        }
    }

    void applyColorsToPixelBuffer(uint8_t *rgba, const uint8_t *image4bit, const uint8_t *palette24bit, size_t numPixels)
    {
        for (size_t i = 0; i < numPixels; i++)
        {
            size_t byteIdx = i / 2;
            size_t shiftAmount = (~i & 1) * 4;  // even pixel in high nibble, odd in low
            uint8_t colorIdx = (image4bit[byteIdx] >> shiftAmount) & 0xf;  // extract color value in nibble
            rgba[i * 4 + 0] = palette24bit[colorIdx * 3 + 0];   // r
            rgba[i * 4 + 1] = palette24bit[colorIdx * 3 + 1];   // g
            rgba[i * 4 + 2] = palette24bit[colorIdx * 3 + 2];   // b
            rgba[i * 4 + 3] = 0xff;                             // a
        }
    }
}