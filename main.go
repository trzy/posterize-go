package main

import (
	//#cgo CXXFLAGS: -std=c++17
	// #include "posterize.h"
	// #include <stdlib.h>
	"C"
	"fmt"
	"image/jpeg"
	"os"
)
import (
	"image"
	"image/color"
)

func decodeJPEGToLinearRGBA(filePath string) ([]uint8, int, int, error) {
	// Open the JPEG file
	file, err := os.Open(filePath)
	if err != nil {
		return nil, 0, 0, err
	}
	defer file.Close()

	// Decode the JPEG image
	img, err := jpeg.Decode(file)
	if err != nil {
		return nil, 0, 0, err
	}

	// Get the dimensions of the image
	bounds := img.Bounds()
	width, height := bounds.Dx(), bounds.Dy()

	// Flatten the RGBA values into a linear array
	rgbaLinear := make([]uint8, width*height*4)
	index := 0
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			r, g, b, a := img.At(x, y).RGBA()
			rgbaLinear[index] = uint8(r >> 8)
			rgbaLinear[index+1] = uint8(g >> 8)
			rgbaLinear[index+2] = uint8(b >> 8)
			rgbaLinear[index+3] = uint8(a >> 8)
			index += 4
		}
	}

	return rgbaLinear, width, height, nil
}

func linearRGBAToImage(rgbaLinear []uint8, width, height int) *image.RGBA {
	// Create a new RGBA image
	rgbaImage := image.NewRGBA(image.Rect(0, 0, width, height))

	// Copy RGBA values from linear slice to the image
	index := 0
	for y := 0; y < height; y++ {
		for x := 0; x < width; x++ {
			rgbaImage.Set(x, y, color.RGBA{
				R: rgbaLinear[index],
				G: rgbaLinear[index+1],
				B: rgbaLinear[index+2],
				A: rgbaLinear[index+3],
			})
			index += 4
		}
	}

	return rgbaImage
}

func saveImageToJPEG(image *image.RGBA, filePath string) error {
	// Create the output file
	outFile, err := os.Create(filePath)
	if err != nil {
		return err
	}
	defer outFile.Close()

	// Encode the image as JPEG and write to file
	err = jpeg.Encode(outFile, image, nil)
	if err != nil {
		return err
	}

	return nil
}

func main() {
	buffer := make([]byte, 1024)
	for i := 0; i < 1024; i++ {
		buffer[i] = 1
	}

	filePath := "bouquet.jpg"
	rgbaLinear, width, height, err := decodeJPEGToLinearRGBA(filePath)
	if err != nil {
		fmt.Println("Error:", err)
		return
	}

	// Posterize the image
	image4bit := make([]uint8, width*height/2)
	palette24bit := make([]uint8, 16*3)
	cImage4bit := (*C.uchar)(&image4bit[0])
	cPalette24bit := (*C.uchar)(&palette24bit[0])
	cRgbaIn := (*C.uchar)(&rgbaLinear[0])
	cNumPixels := C.size_t(width * height)
	C.posterize(cImage4bit, cPalette24bit, cRgbaIn, cNumPixels)

	// Convert back so we will be able to write it to a JPEG image
	C.applyColorsToPixelBuffer(cRgbaIn, cImage4bit, cPalette24bit, cNumPixels)

	// Convert linear RGBA to image
	rgbaImage := linearRGBAToImage(rgbaLinear, width, height)

	// Save image to JPEG file
	err = saveImageToJPEG(rgbaImage, "bouquet_4bit.jpg")
	if err != nil {
		fmt.Println("Error:", err)
		return
	}
	fmt.Println("Success")
}
