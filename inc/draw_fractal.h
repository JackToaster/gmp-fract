#include <stdint.h>
#include "raylib.h"
#include "pthread.h"

#define MAX_ITER 100

typedef uint32_t (*Fractal)(double x, double y, void* cfg);

Color colorMap(uint32_t iter) {
    return ColorFromHSV((float) ((iter) % 360), 1., 1.);
}

void DrawFractal(Image *image, Fractal fractal, void* cfg) {
    int32_t width = image->width;
    int32_t height = image->height;

    for(int32_t x = 0; x < width; ++x) {
        for(int32_t y = 0; y < height; ++y){
            // re/im range -2 to 2
            double re = (float)(x - width / 2) * 4. / (float)width;
            double im = (float)(y - height / 2) * 4. / (float)width;
            int iter = fractal(re, im, cfg);
            if(iter != UINT32_MAX) {
                ImageDrawPixel(image, x, y, colorMap(iter));
                // a
            }
        }
    }
}

typedef struct RenderThreadSync {
    pthread_mutex_t *mtx;
    Image *image;
    Fractal fractal;
    uint32_t row_idx;
    void* fractal_cfg;
} RenderThreadSync;

typedef void* (*render_thread_t)(void* arg);

void* render_thread(RenderThreadSync *sync) {
    int width = sync->image->width;
    int height = sync->image->height;
    while(1) {
        // acquire row
        if(pthread_mutex_lock(sync->mtx)) { return NULL; }
        int32_t row_idx = sync->row_idx;
        ++(sync->row_idx);
        pthread_mutex_unlock(sync->mtx);

        // check if done
        if(row_idx >= height) { break; }
        else {
            // fractal drawing loop
            for(int32_t x = 0; x < width; ++x) {
                // re/im range -2 to 2
                double re = (float)(x - width / 2) * 4. / (float)width;
                double im = (float)(row_idx - height / 2) * 4. / (float)width;
                int iter = sync->fractal(re, im, sync->fractal_cfg);
                if(iter != UINT32_MAX) {
                    ImageDrawPixel(sync->image, x, row_idx, colorMap(iter));
                }
            } // end for
        } // end else

    } // end while(1)

    return NULL;
}

void DrawFractal_threaded(Image *image, Fractal fractal, void* cfg, uint32_t threads) {
    pthread_t *tid = malloc(threads * sizeof(pthread_t));

    pthread_mutex_t sync_mutex;
    RenderThreadSync sync = {
        .fractal = fractal,
        .fractal_cfg = cfg,
        .image = image,
        .row_idx = 0,
        .mtx = &sync_mutex
    };
    pthread_mutex_init(&sync_mutex, NULL);

    for(uint32_t i = 0; i < threads; ++i) {
        pthread_create(&(tid[i]), NULL, (void* (*)(void*))&render_thread, (void*) &sync);
    }
    for(uint32_t i = 0; i < threads; ++i) {
        pthread_join(tid[i], NULL);
    }
    pthread_mutex_destroy(&sync_mutex);


    free(tid);
}