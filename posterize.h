/*
 * posterize.h
 * Bart Trzynadlowski, 5/15/2024
 * 
 * Header file for image posterization.
 */

#ifndef POSTERIZE_H
#define POSTERIZE_H

#include <stdint.h>
#include <stdlib.h>

/*
 * Posterizes an image: reduces the color palette to 16 colors, with color 0 forced to black, and 
 * produces a 4-bit linear palettized image.
 * 
 * Parameters
 * ----------
 * image4bit:
 *      Output buffer to which the 4-bit image will be written. Must be of size numPixels / 2.
 * palette24bit:
 *      Output buffer to which the final palette will be written. Each color is a triplet of R, G,
 *      and B, where each element is an 8-bit unsigned integer.
 * rgbaIn:
 *      Input RGBA buffer stored as a linear sequence of pixels: R, G, B, and A. Each component is
 *      8 bits and the alpha channel is completely ignored.
 * numPixels:
 *      The total number of pixels (i.e., height * width).
 */
extern void posterize(uint8_t *image4bit, uint8_t *palette24bit, const uint8_t *rgbaIn, size_t numPixels);

/*
 * Given a 4-bit linear palettized image and the corresponding palette, produces an RGBA image. This
 * is intended for debugging the posterization algorithm.
 *
 * Parameters
 * ----------
 * rgba:
 *      Output buffer to which the RBGA image is written. Must be sized to hold numPixels * 4 bytes.
 * image4bit:
 *      The 4-bit image.
 * palette24bit:
 *      The palette.
 * numPixels:
 *      Number of pixels in the image.
 */
extern void applyColorsToPixelBuffer(uint8_t *rgba, const uint8_t *image4bit, const uint8_t *palette24bit, size_t numPixels);

#endif // POSTERIZE_H