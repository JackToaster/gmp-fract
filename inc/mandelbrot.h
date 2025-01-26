#include <stdint.h>
#include "gmp.h"

typedef struct MandelbrotCFG {
    uint32_t iterations;
    double cx, cy; // center x/y
    double zoom;
} MandelbrotCFG;

uint32_t mandelbrot(double x, double y, MandelbrotCFG *cfg) {
    uint32_t iter = 0;

    double re_c = x / cfg->zoom + cfg->cx;
    double im_c = y / cfg->zoom + cfg->cy;
    // start at zero
    double re = 0.0;
    double im = 0.0;
    double re2 = 0.0;
    double im2 = 0.0;

    /*
    "Optimized escape time algorithm" from wikipedia
    x:= 0
    y:= 0
    x2:= 0
    y2:= 0

    while (x2 + y2 ≤ 4 and iteration < max_iteration) do
        y:= 2 * x * y + y0
        x:= x2 - y2 + x0
        x2:= x * x
        y2:= y * y
        iteration:= iteration + 1
    */
    while(iter < cfg->iterations) {
        im = 2 * re * im + im_c;
        re = re2 - im2 + re_c;
        re2 = re * re;
        im2 = im * im;

        if(re2 + im2 > 4.0) {
            return iter;
        }
        ++iter;
    }
    return INT32_MAX;
}

typedef struct ArbPrecFrame {
    mpf_t c_re, c_im; // center x/y
    mpf_t zoom;
} ArbPrecFrame;

typedef struct ArbPrecMandelbrotCFG {
    uint32_t iterations;
    mpf_t c_re, c_im; // center x/y
    mpf_t zoom;
    mp_bitcnt_t precision_bits;
} ArbPrecMandelbrotCFG;

uint32_t arb_prec_mandelbrot(double x, double y, ArbPrecMandelbrotCFG *cfg) {
    uint32_t iter = 0;

    // C value
    mpf_t re_c;
    mpf_t im_c;

    mpf_set_default_prec(cfg->precision_bits);

    // init with x/y
    mpf_init_set_d(re_c, x);
    mpf_init_set_d(im_c, y);

    // scale (not sure if modify in-place works?)
    mpf_div(re_c, re_c, cfg->zoom);
    mpf_div(im_c, im_c, cfg->zoom);

    // offset
    mpf_add(re_c, re_c, cfg->c_re);
    mpf_add(im_c, im_c, cfg->c_im);

    
    // start at zero
    mpf_t re, im, re2, im2;
    mpf_init(re);
    mpf_init(im);
    mpf_init(re2);
    mpf_init(im2);

    /*
    "Optimized escape time algorithm" from wikipedia
    x:= 0
    y:= 0
    x2:= 0
    y2:= 0

    while (x2 + y2 ≤ 4 and iteration < max_iteration) do
        y:= 2 * x * y + y0
        x:= x2 - y2 + x0
        x2:= x * x
        y2:= y * y
        iteration:= iteration + 1
    */
    while(iter < cfg->iterations) {
        // im = 2 * re * im + im_c;
        mpf_mul(im, re, im);
        mpf_mul_2exp(im, im, 1);
        mpf_add(im, im, im_c);

        // re = re2 - im2 + re_c;
        mpf_sub(re, re2, im2);
        mpf_add(re, re, re_c);

        // re2 = re * re;
        mpf_mul(re2, re, re);

        // im2 = im * im;
        mpf_mul(im2, im, im);

        if(mpf_get_d(re2) + mpf_get_d(im2) > 4.0) {
            return iter;
        }
        ++iter;
    }
    return UINT32_MAX;
}

// for perturbation theory version
typedef struct RefIter {
    uint32_t iterations;
    double *re;
    double *im;
} RefIter;

RefIter build_ref_iter(ArbPrecFrame *frame, mp_bitcnt_t precision_bits, uint32_t iterations) {
    double *re_pts = (double*) malloc(iterations * sizeof(double));
    double *im_pts = (double*) malloc(iterations * sizeof(double));

    uint32_t iter = 0;

    // C value
    mpf_t re_c;
    mpf_t im_c;

    mpf_set_default_prec(precision_bits);

    // init c
    mpf_init_set(re_c, frame->c_re);
    mpf_init_set(im_c, frame->c_im);

    // start at zero
    mpf_t re, im, re2, im2;
    mpf_init(re);
    mpf_init(im);
    mpf_init(re2);
    mpf_init(im2);

    /*
    "Optimized escape time algorithm" from wikipedia
    x:= 0
    y:= 0
    x2:= 0
    y2:= 0

    while (x2 + y2 ≤ 4 and iteration < max_iteration) do
        y:= 2 * x * y + y0
        x:= x2 - y2 + x0
        x2:= x * x
        y2:= y * y
        iteration:= iteration + 1
    */
   // first point is 0
   re_pts[0] = 0.0;
   im_pts[0] = 0.0;

    int i = 1;
    while(i < iterations) {
        // im = 2 * re * im + im_c;
        mpf_mul(im, re, im);
        mpf_mul_2exp(im, im, 1);
        mpf_add(im, im, im_c);

        // re = re2 - im2 + re_c;
        mpf_sub(re, re2, im2);
        mpf_add(re, re, re_c);

        // re2 = re * re;
        mpf_mul(re2, re, re);

        // im2 = im * im;
        mpf_mul(im2, im, im);

        // write re/im to array
        re_pts[i] = mpf_get_d(re);
        im_pts[i] = mpf_get_d(im);

        ++i;

        if(mpf_get_d(re2) + mpf_get_d(im2) > 40.0) {
            return (RefIter) {iter, re_pts, im_pts};
        }
    }

    return (RefIter) {iterations, re_pts, im_pts};
}

void drop_ref_iter(RefIter *ref) {
    free(ref->im);
    free(ref->re);
}

typedef struct PerturbMandelbrotCFG {
    uint32_t iterations;
    RefIter *reference;
    ArbPrecFrame *frame;
} PerturbMandelbrotCFG;

uint32_t perturb_mandelbrot(double x, double y, PerturbMandelbrotCFG *cfg) {
    double reDz = 0.0;
    double imDz = 0.0;
    double scale = 1.0 / mpf_get_d(cfg->frame->zoom);
    double reDc = x * scale;
    double imDc = y * scale;

    uint32_t iteration = 0;
    uint32_t ref_iteration = 0;

    // TODO SIMDify
    while(iteration < cfg->iterations) {
        double reRef = cfg->reference->re[ref_iteration];
        double imRef = cfg->reference->im[ref_iteration];

        double temp_reDz = 2 * (reDz * reRef - imDz * imRef) + reDz * reDz - imDz * imDz + reDc;
        double temp_imDz = 2 * (reDz * imRef + imDz * reRef + reDz * imDz) + imDc;

        reDz = temp_reDz;
        imDz = temp_imDz;

        ref_iteration++;

        double re_z = reRef + reDz;
        double im_z = imRef + imDz;

        double abs_z2 = re_z * re_z + im_z * im_z;
        if(abs_z2 > 100.0) {
            return iteration;
        }

        double abs_dz2 = reDz * reDz + imDz * imDz;

        // apparently this is supposed to fix glitches, but it seems to just create them.
        // if(abs_z2 < abs_dz2 || ref_iteration >= cfg->reference->iterations) {
        //     // dz = z
        //     reDz = re_z; imDz = im_z;
        //     ref_iteration = 0;
        // }

        iteration++;
    }

    return UINT32_MAX;
}
/*
From FractalForums
complex Reference[]; // Reference orbit (MUST START WITH ZERO)

int MaxRefIteration; // The last valid iteration of the reference (the iteration just before it escapes)

complex dz = 0, dc; // Delta z and Delta c

int Iteration = 0, RefIteration = 0;

 

while (Iteration < MaxIteration) {

    dz = 2 * dz * Reference[RefIteration] + dz * dz + dc;

    ReDz = 2 * (ReDz * ReRef - ImDz * ImRef) + ReDz * ReDz - ImDz * ImDz + ReDc;
    ImDz = 2 * (ReDz * ImRef + ImDz * ReRef) + 2 * ReDz * ImDz + ImDc;

    x = 2*(x*x0-y*y0) + x2-y2 + dx0
    y = 2*(x*y0+y*x0) + 2*x*y + dy0
    RefIteration++;

    

    complex z = Reference[RefIteration] + dz;

    if (|z| > BailoutRadius) break;

    if (|z| < |dz| || RefIteration == MaxRefIteration) {

        dz = z;

        RefIteration = 0;

    }

    Iteration++;

}
*/