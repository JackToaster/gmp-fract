#include <stdint.h>
#include "raylib.h"
#include "pthread.h"

#define MAX_ITER 100

typedef uint32_t (*Fractal)(double x, double y, void* cfg);

Color colorMap(uint32_t iter) {
    return ColorFromHSV((float) ((iter * 5) % 360), 1., 1.);
}

void DrawFractal(Image *image, Fractal fractal, void* cfg) {
    int32_t width = image->width;
    int32_t height = image->height;

    for(int32_t x = 0; x < width; ++x) {
        for(int32_t y = 0; y < height; ++y){
            // re/im range -2 to 2
            double re = ((float) x - (float)width / 2) * 2. / (float)width;
            double im = ((float) y - (float)height / 2) * 2. / (float)width;
            int iter = fractal(re, im, cfg);
            if(iter != UINT32_MAX) {
                ImageDrawPixel(image, x, y, colorMap(iter));
                // a
            }
        }
    }
}

typedef struct RenderThreadSync_t {
    uint32_t n_threads;
    pthread_t *tid; // thread IDs for render threads
    pthread_mutex_t mtx; // mutex for grabbing image rows without race conditions (and setting/reading cancel condition)
    Image *image; // image to write into
    uint32_t row_idx; // stores the next available row for a thread to grab

    Fractal fractal; // pointer to fractal function
    void* fractal_cfg; // configuration for fractal function (stores zoom, x/y center, and other params depending on fractal function)
    bool cancel; // set to true to cancel render
} RenderThreadSync_t;

typedef void* (*render_thread_t)(void* arg);

void* render_thread(RenderThreadSync_t *sync) {
    int width = sync->image->width;
    int height = sync->image->height;
    while(1) {
        // acquire row
        if(pthread_mutex_lock(&(sync->mtx))) { return NULL; }
        int32_t row_idx = sync->row_idx;
        ++(sync->row_idx);
        bool cancel = sync->cancel;
        pthread_mutex_unlock(&(sync->mtx));
        // exit thread early
        if(cancel) { return NULL; }

        // check if done
        if(row_idx >= height) { break; }
        else {
            // fractal drawing loop
            for(int32_t x = 0; x < width; ++x) {
                // re/im range -2 to 2
                double re = ((float)x + 0.5 - (float)width / 2) * 4. / (float)width; // +0.5 centers pixel on coordinate
                double im = ((float)row_idx + 0.5 - (float)height / 2) * 4. / (float)width; // to keep image from moving when resolution changes
                int iter = sync->fractal(re, im, sync->fractal_cfg);
                if(iter != UINT32_MAX) {
                    ImageDrawPixel(sync->image, x, row_idx, colorMap(iter));
                }
            } // end for
        } // end else

    } // end while(1)

    return NULL;
}

// start rendering asynchronously, return a RenderThreadSync_t to control/monitor the rendering.
RenderThreadSync_t* DrawFractal_threaded_start(Image *image, Fractal fractal, void* cfg, uint32_t threads) {
    pthread_t *tid = malloc(threads * sizeof(pthread_t));

    RenderThreadSync_t *sync = malloc(sizeof(RenderThreadSync_t));
    
    sync->fractal = fractal;
    sync->fractal_cfg = cfg,
    sync->image = image;
    sync->row_idx = 0;
    sync->tid = tid;
    sync->n_threads = threads;
    sync->cancel = false;

    // printf("w%i h%i\n", sync->image->width, sync->image->height);


    printf("Created mutex\n");
    if(pthread_mutex_init(&(sync->mtx), NULL)) {
        printf("failed to create mutex :(\n");
    };

    for(uint32_t i = 0; i < threads; ++i) {
        pthread_create(&(sync->tid[i]), NULL, (void* (*)(void*))&render_thread, (void*) sync);
    }
    return sync;
}

typedef struct RenderThreadStatus_t {
    double progress_pct;
    bool done;
} RenderThreadStatus_t;

// Returns the status (percent and bool for done) of the rendering
RenderThreadStatus_t check_threaded_render_status(RenderThreadSync_t *sync) {
    if(sync == NULL) {
        printf("Attempt to check status of null render thread object\n");
        return (RenderThreadStatus_t) {.done=false, .progress_pct = 0.0};
    }
    pthread_mutex_lock(&(sync->mtx));
    uint32_t next_row = sync->row_idx;
    pthread_mutex_unlock(&(sync->mtx));


    // printf("w%i h%i\n", sync->image->width, sync->image->height);

    // check if we're done
    if(next_row == sync->image->height + sync->n_threads) {
        return (RenderThreadStatus_t) {.done=true, .progress_pct = 1.0};
    } else {
        return (RenderThreadStatus_t) {.done=false, .progress_pct = (float) next_row / (float) sync->image->height};
    }
}

// called once all threads are done rendering
void DrawFractal_threaded_end (RenderThreadSync_t *sync) {
    // join threads
    for(uint32_t i = 0; i < sync->n_threads; ++i) {
        pthread_join(sync->tid[i], NULL);
    }
    // clean up mutex
    pthread_mutex_destroy(&(sync->mtx));

    // free allocated memory
    free(sync->tid);
    free(sync);
    return;
}

void DrawFractal_threaded(Image *image, Fractal fractal, void* cfg, uint32_t threads) {
    pthread_t *tid = malloc(threads * sizeof(pthread_t));

    RenderThreadSync_t sync = {
        .fractal = fractal,
        .fractal_cfg = cfg,
        .image = image,
        .row_idx = 0,
        .tid = tid
    };
    pthread_mutex_init(&(sync.mtx), NULL);

    for(uint32_t i = 0; i < threads; ++i) {
        pthread_create(&(tid[i]), NULL, (void* (*)(void*))&render_thread, (void*) &sync);
    }
    for(uint32_t i = 0; i < threads; ++i) {
        pthread_join(tid[i], NULL);
    }
    pthread_mutex_destroy(&(sync.mtx));

    free(tid);
}

typedef enum {
    IDLE, // no data, no threads
    RENDERING, // threads running & filling in image
    FINISHED, // finished, image ready
} RendererState_t;


// Fractal renderer
// API:
// renderer_init(...) -> create and set up renderer
// renderer_startRender(...) -> create image and start render threads
// renderer_progress(...) -> return progress (bool done/progress 0-1)
// renderer_update(...) -> call repeatedly from UI thread to update status and see when the render is done.
//                         Once the render is finished this call will close out and clean up the render threads.'
// renderer_getResultImage(...) -> returns a pointer
typedef struct FractalRenderer_t {
    void* fractal_cfg;
    Fractal fractal_fn;

    RendererState_t state;

    RenderThreadSync_t *thread_sync; // only valid while in RENDERING state

    uint32_t n_threads;
} FractalRenderer_t;

void renderer_init(FractalRenderer_t *r, Fractal fractal, void* cfg, uint32_t threads) {
    r->fractal_fn = fractal;
    r->fractal_cfg = cfg;
    r->state = IDLE;
    r->n_threads = threads;
    r->thread_sync = NULL;
}

void renderer_startRender(FractalRenderer_t *r, uint32_t width, uint32_t height) {
    printf("Start rendering w=%i h=%i\n", width, height);
    if(r->state == RENDERING) {
        printf("Cannot start rendering - render already in progress\n");
        return;
    }
    Image* new_image = malloc(sizeof(Image));
    *new_image = GenImageColor(width, height, (Color) {0,0,0,0});

    // printf("w%i h%i\n", new_image->width, new_image->height);


    r->thread_sync = DrawFractal_threaded_start(new_image, r->fractal_fn, r->fractal_cfg, r->n_threads);
    r->state = RENDERING;
}

RenderThreadStatus_t renderer_progress(FractalRenderer_t *r) {
    if(r->state == IDLE) {
        return (RenderThreadStatus_t) {.done=false, .progress_pct = 0.0};
    } else {
        return check_threaded_render_status(r->thread_sync);
    }
}

// cancel the render where it is and clean up
void renderer_cancel(FractalRenderer_t *r) {
    printf("Cancel rendering\n");

    if(r->state != RENDERING) {return;}

    pthread_mutex_lock(&(r->thread_sync->mtx));
    r->thread_sync->cancel = true;
    pthread_mutex_unlock(&(r->thread_sync->mtx));

    DrawFractal_threaded_end(r->thread_sync);

    r->state = IDLE;
}

RendererState_t renderer_update(FractalRenderer_t *r) {
    if(r->state == IDLE) {
        return IDLE;
    } else {
        if(r->state == RENDERING) {
            // check status if rendering
            RenderThreadStatus_t stat = renderer_progress(r);
            if(stat.done) {
                printf("render finished, cleaning up\n");
                // close out threads & clean up
                DrawFractal_threaded_end(r->thread_sync);
                r->state = FINISHED;
            }
        }
        return r->state;
    }
}

Image* renderer_getResultImage(FractalRenderer_t *r) {

    printf("Read image\n");
    if(renderer_update(r) == IDLE) {
        printf("Attempt to read result image before rendering started\n");
        return NULL;
    }

    return r->thread_sync->image;
}