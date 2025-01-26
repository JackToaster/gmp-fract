#include <stdint.h>
#include "raylib.h"

#define MAX_ITER 100

typedef uint32_t (*Fractal)(double x, double y, void* cfg);

Color colorMap(uint32_t iter) {
    return ColorFromHSV((float) ((iter * 36) % 360), 1., 1.);
}

void DrawFractal(Image *image, Fractal fractal, void* cfg) {
    int32_t width = image->width;
    int32_t height = image->height;

    for(int32_t x = 0; x < width; ++x) {
        for(int32_t y = 0; y < height; ++y){
            // re/im range -2 to 2
            double re = (float)(x - width / 2) * 4. / (float)width;
            double im = (float)(y - height / 2) * 4. / (float)height;
            int iter = fractal(re, im, cfg);
            if(iter != UINT32_MAX) {
                ImageDrawPixel(image, x, y, colorMap(iter));
                // a
            }
        }
    }
}