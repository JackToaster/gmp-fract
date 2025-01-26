#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_raylib.c"
#include "draw_fractal.h"
#include "mandelbrot.h"
#include "gmp.h"
#include "pthread.h"

#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }

#define N_THREADS 1

const uint32_t FONT_ID_BODY_24 = 0;
const uint32_t FONT_ID_BODY_16 = 1;
const Clay_Color COLOR_LIGHT = (Clay_Color) {224, 215, 210, 55};
const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};


Clay_Dimensions prev_screen_dims = {0.0, 0.0};

// todo settings/split this logic to another file
uint32_t decimation_level;
const uint32_t N_DECIMATIONS = 3;
const float DECIMATION_FAC = 3.0;
const float final_pixel_scale = 2.0;

Image *fractal_image[N_DECIMATIONS];
Texture2D fractal_tex[N_DECIMATIONS];



FractalRenderer_t renderer;
PerturbMandelbrotCFG fractal_config;
RefIter fractal_ref_iter;
ArbPrecFrame fractal_frame;

// set configurations and generate reference orbit
void configure_renderer(void) {
    uint32_t prec = 1024; // bits
    uint32_t ref_iterations = 20000;
    mpf_init_set_str(fractal_frame.c_re, "-1479946223325078880202580653442e-30", 10);
    mpf_init_set_str(fractal_frame.c_im,  "0000901397329020353980197791866e-30", 10);
    mpf_init_set_str(fractal_frame.zoom, "1e10", 10);

    printf("building reference iteration...\n");
    double currentTime = GetTime();

    // TODO For some reason ref always has iterations set to 0 :/
    fractal_ref_iter = build_ref_iter(&fractal_frame, prec, ref_iterations);

    printf("ref iter time: %f ms\n", (GetTime() - currentTime) * 1000);

    // set configuration
    fractal_config = (PerturbMandelbrotCFG){
        .iterations = 20000,
        .frame = &fractal_frame,
        .reference = &fractal_ref_iter
    };
}

void reset_decimation_level(void) {
    decimation_level = N_DECIMATIONS - 1;
    for(uint32_t i = 0; i < N_DECIMATIONS; ++i) {
        if(fractal_image[i] != NULL && IsImageValid(*fractal_image[i])){
            UnloadImage(*fractal_image[i]);
        }
        free(fractal_image[i]);
        fractal_image[i] = NULL;

        if(IsTextureValid(fractal_tex[i])) {
            UnloadTexture(fractal_tex[i]);
        }
    }
}

void update_texture_from_image(void) {
    if(IsTextureValid(fractal_tex[decimation_level])) { UnloadTexture(fractal_tex[decimation_level]); }
    fractal_tex[decimation_level] = LoadTextureFromImage(*fractal_image[decimation_level]);
}
void redraw_fractal_dec(uint32_t screen_width, uint32_t screen_height) {
    float pixel_scale = final_pixel_scale / pow(DECIMATION_FAC, decimation_level);
    renderer_startRender(&renderer, screen_width * pixel_scale, screen_height * pixel_scale);
    fractal_image[decimation_level] = renderer_getResultImage(&renderer);
}


Clay_LayoutConfig sidebarItemLayout = (Clay_LayoutConfig) {
    .sizing = { .width = CLAY_SIZING_GROW(0), .height = CLAY_SIZING_FIXED(50) },
};

// Re-useable components are just normal functions
void SidebarItemComponent() {
    CLAY(CLAY_LAYOUT(sidebarItemLayout), CLAY_RECTANGLE({ .color = COLOR_ORANGE })) {}
}

Clay_RenderCommandArray CreateLayout() {
    Clay_BeginLayout();
    CLAY(CLAY_ID("OuterContainer"), CLAY_LAYOUT({ .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .padding = CLAY_PADDING_ALL(16), .childGap = 16 })) {
        CLAY(CLAY_ID("SideBar"),
            CLAY_LAYOUT({ .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_FIXED(300), .height = CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(16), .childGap = 16 }),
            CLAY_RECTANGLE({ .color = COLOR_LIGHT, .cornerRadius = {5} })
        ) {

            // Standard C code like loops etc work inside components
            for (int i = 0; i < 5; i++) {
                SidebarItemComponent();
            }
        }
    }
    return Clay_EndLayout();
}

typedef struct
{
    Clay_Vector2 clickOrigin;
    Clay_Vector2 positionOrigin;
    bool mouseDown;
} ScrollbarData;

ScrollbarData scrollbarData = {};

bool debugEnabled = false;


void UpdateDrawFrame(void)
{
    Vector2 mouseWheelDelta = GetMouseWheelMoveV();
    float mouseWheelX = mouseWheelDelta.x;
    float mouseWheelY = mouseWheelDelta.y;

    if (IsKeyPressed(KEY_D)) {
        debugEnabled = !debugEnabled;
        Clay_SetDebugModeEnabled(debugEnabled);
    }
    //----------------------------------------------------------------------------------
    // Handle scroll containers
    Clay_Vector2 mousePosition = RAYLIB_VECTOR2_TO_CLAY_VECTOR2(GetMousePosition());
    Clay_SetPointerState(mousePosition, IsMouseButtonDown(0) && !scrollbarData.mouseDown);
    Clay_Dimensions screen_dims = (Clay_Dimensions) { (float)GetScreenWidth(), (float)GetScreenHeight() };
    Clay_SetLayoutDimensions(screen_dims);
    if (!IsMouseButtonDown(0)) {
        scrollbarData.mouseDown = false;
    }

    if (IsMouseButtonDown(0) && !scrollbarData.mouseDown && Clay_PointerOver(Clay__HashString(CLAY_STRING("ScrollBar"), 0, 0))) {
        Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(Clay__HashString(CLAY_STRING("MainContent"), 0, 0));
        scrollbarData.clickOrigin = mousePosition;
        scrollbarData.positionOrigin = *scrollContainerData.scrollPosition;
        scrollbarData.mouseDown = true;
    } else if (scrollbarData.mouseDown) {
        Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(Clay__HashString(CLAY_STRING("MainContent"), 0, 0));
        if (scrollContainerData.contentDimensions.height > 0) {
            Clay_Vector2 ratio = (Clay_Vector2) {
                scrollContainerData.contentDimensions.width / scrollContainerData.scrollContainerDimensions.width,
                scrollContainerData.contentDimensions.height / scrollContainerData.scrollContainerDimensions.height,
            };
            if (scrollContainerData.config.vertical) {
                scrollContainerData.scrollPosition->y = scrollbarData.positionOrigin.y + (scrollbarData.clickOrigin.y - mousePosition.y) * ratio.y;
            }
            if (scrollContainerData.config.horizontal) {
                scrollContainerData.scrollPosition->x = scrollbarData.positionOrigin.x + (scrollbarData.clickOrigin.x - mousePosition.x) * ratio.x;
            }
        }
    }

    Clay_UpdateScrollContainers(true, (Clay_Vector2) {mouseWheelX, mouseWheelY}, GetFrameTime());
    // Generate the auto layout for rendering
    // double currentTime = GetTime();
    Clay_RenderCommandArray renderCommands = CreateLayout();
    // printf("layout time: %f microseconds\n", (GetTime() - currentTime) * 1000 * 1000);
    // RENDERING ---------------------------------
//    currentTime = GetTime();
    BeginDrawing();
    ClearBackground(BLACK);

    RendererState_t r_state = renderer_update(&renderer);
    if(memcmp(&screen_dims, &prev_screen_dims, sizeof(screen_dims))) {
        prev_screen_dims = screen_dims;
        if(r_state == RENDERING) {
            renderer_cancel(&renderer);
        }
        reset_decimation_level();
        redraw_fractal_dec(screen_dims.width, screen_dims.height);
    }

     r_state = renderer_update(&renderer);
    if(r_state == RENDERING) {
        // update texture from image
        update_texture_from_image();
    } else {
        // render finished, load texture one last time
        if(r_state == FINISHED) {
            update_texture_from_image();

            renderer.state = IDLE;

            // check if we still have to do the next decimation level
            if(decimation_level > 0) {
                decimation_level--;
                redraw_fractal_dec(screen_dims.width, screen_dims.height);
            }
        }
    }
    
    // draw image
    for(int32_t dec = N_DECIMATIONS - 1; dec >= 0; dec--) {
        if(IsTextureValid(fractal_tex[dec])) {
            DrawTexturePro(fractal_tex[dec], (Rectangle) { 0.0, 0.0, fractal_tex[dec].width, fractal_tex[dec].height}, (Rectangle) { 0.0, 0.0, screen_dims.width, screen_dims.height}, (Vector2) { 0.0, 0.0 }, 0.0, WHITE);
        }
    }
    // draw UI on top
    // Clay_Raylib_Render(renderCommands);
    EndDrawing();
//    printf("render time: %f ms\n", (GetTime() - currentTime) * 1000);

    //----------------------------------------------------------------------------------
}


void HandleClayErrors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);
    exit(1);
}



int main(void) {
    uint64_t totalMemorySize = Clay_MinMemorySize();
    Clay_Arena clayMemory = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, malloc(totalMemorySize));
    Clay_Initialize(clayMemory, (Clay_Dimensions) { (float)GetScreenWidth(), (float)GetScreenHeight() }, (Clay_ErrorHandler) { HandleClayErrors });
    Clay_SetMeasureTextFunction(Raylib_MeasureText, 0);
    Clay_Raylib_Initialize(1024, 768, "Clay - Raylib Renderer Example", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI | FLAG_MSAA_4X_HINT);
    
    configure_renderer();
    renderer_init(&renderer, (Fractal) &perturb_mandelbrot, (void*)&fractal_config, N_THREADS);
    reset_decimation_level();

    Raylib_fonts[FONT_ID_BODY_24] = (Raylib_Font) {
        .font = LoadFontEx("resources/Roboto-Regular.ttf", 48, 0, 400),
        .fontId = FONT_ID_BODY_24,
    };
	SetTextureFilter(Raylib_fonts[FONT_ID_BODY_24].font.texture, TEXTURE_FILTER_BILINEAR);

    Raylib_fonts[FONT_ID_BODY_16] = (Raylib_Font) {
        .font = LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400),
        .fontId = FONT_ID_BODY_16,
    };
    SetTextureFilter(Raylib_fonts[FONT_ID_BODY_16].font.texture, TEXTURE_FILTER_BILINEAR);

    //--------------------------------------------------------------------------------------

    // Main UI loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
    return 0;
}