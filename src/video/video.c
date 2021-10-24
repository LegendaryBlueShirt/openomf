#include <SDL.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "video/video.h"
#include "video/image.h"
#include "video/tcache.h"
#include "utils/allocator.h"
#include "utils/log.h"
#include "utils/list.h"
#include "resources/palette.h"
#include "video/video_state.h"
#include "video/video_hw.h"
#include "video/video_soft.h"
#include "plugins/plugins.h"

static video_state state;


void clear_render_target() {
    // Update target with black pixels
    int size = NATIVE_W * state.scale_factor * NATIVE_H * state.scale_factor * 4;
    char *pixels = omf_calloc(1, size);
    SDL_UpdateTexture(state.target, NULL, pixels, NATIVE_W * state.scale_factor * 4);
    omf_free(pixels);
}

void reset_targets() {
    if(state.target != NULL) {
        SDL_DestroyTexture(state.target);
    }
    state.target = SDL_CreateTexture(state.renderer,
                                     SDL_PIXELFORMAT_ABGR8888,
                                     SDL_TEXTUREACCESS_TARGET,
                                     NATIVE_W * state.scale_factor,
                                     NATIVE_H * state.scale_factor);
    clear_render_target();
}

int video_load_scaler(const char* name, int scale_factor) {
    scaler_init(&state.scaler);
    if(scale_factor <= 1) {
        return 0;
    }
    if(plugins_get_scaler(&state.scaler, name)) {
        return 1;
    }
    if(!scaler_is_factor_available(&state.scaler, scale_factor)) {
        return 1;
    }
    return 0;
}

int video_init(int window_w,
               int window_h,
               int fullscreen,
               int vsync,
               const char* scaler_name,
               int scale_factor) {
    state.w = window_w;
    state.h = window_h;
    state.fs = fullscreen;
    state.vsync = vsync;
    state.fade = 1.0f;
    state.target = NULL;
    state.target_move_x = 0;
    state.target_move_y = 0;

    // Load scaler (if any)
    memset(state.scaler_name, 0, sizeof(state.scaler_name));
    strncpy(state.scaler_name, scaler_name, sizeof(state.scaler_name)-1);
    if(video_load_scaler(scaler_name, scale_factor)) {
        DEBUG("Scaler \"%s\" plugin not found; using Nearest neighbour scaling.", scaler_name);
        state.scale_factor = 1;
    } else {
        DEBUG("Scaler \"%s\" loaded w/ factor %d", scaler_name, scale_factor);
        state.scale_factor = scale_factor;
    }

    // Clear palettes
    state.cur_palette = omf_calloc(1, sizeof(screen_palette));
    state.base_palette = omf_calloc(1, sizeof(palette));
    state.cur_palette->version = 1;

    // Form title string
    char title[32];
    snprintf(title, 32, "OpenOMF v%d.%d.%d", V_MAJOR, V_MINOR, V_PATCH);

    // Open window
    state.window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        window_w,
        window_h,
        SDL_WINDOW_SHOWN);
    if(state.window == NULL) {
        PERROR("Could not create window: %s", SDL_GetError());
        return 1;
    }

    // Set fullscreen if needed
    if(state.fs) {
        if(SDL_SetWindowFullscreen(state.window, SDL_WINDOW_FULLSCREEN) != 0) {
            PERROR("Could not set fullscreen mode!");
        } else {
            DEBUG("Fullscreen enabled!");
        }
    } else {
        SDL_SetWindowFullscreen(state.window, 0);
    }

    // Form flags
    int renderer_flags = SDL_RENDERER_ACCELERATED;
    if(state.vsync) {
        renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }

    // Create renderer
    state.renderer = SDL_CreateRenderer(
        state.window,
        -1,
        renderer_flags);
    if(state.renderer == NULL) {
        PERROR("Could not create renderer: %s", SDL_GetError());
        return 1;
    }

    // Default resolution for renderer. This will them get scaled up to screen size.
    SDL_RenderSetLogicalSize(state.renderer,
                             NATIVE_W * state.scale_factor,
                             NATIVE_H * state.scale_factor);

    // Disable screensaver :/
    SDL_DisableScreenSaver();

    // Set rendertargets
    reset_targets();

    // Init texture cache
    tcache_init(state.renderer, state.scale_factor, &state.scaler);

    // Init hardware renderer
    state.cur_renderer = VIDEO_RENDERER_HW;
    video_hw_init(&state);

    // Get renderer data
    SDL_RendererInfo rinfo;
    SDL_GetRendererInfo(state.renderer, &rinfo);

    // Show some info
    INFO("Video Init OK");
    INFO(" * Driver: %s", SDL_GetCurrentVideoDriver());
    INFO(" * Renderer: %s", rinfo.name);
    INFO(" * Accelerated: %s", (rinfo.flags & SDL_RENDERER_ACCELERATED) ? "Yes" : "No");
    INFO(" * VSync support: %s", (rinfo.flags & SDL_RENDERER_PRESENTVSYNC) ? "Yes" : "No");
    INFO(" * Target support: %s", (rinfo.flags & SDL_RENDERER_TARGETTEXTURE) ? "Yes" : "No");
    return 0;
}

void video_reinit_renderer() {
    // Clear old texture cache entries
    tcache_clear();

    // Kill old renderer
    SDL_DestroyRenderer(state.renderer);

    // Create a new renderer
    int renderer_flags = SDL_RENDERER_ACCELERATED;
    if(state.vsync) {
         renderer_flags |= SDL_RENDERER_PRESENTVSYNC;
    }
    state.renderer = SDL_CreateRenderer(state.window, -1, renderer_flags);
    SDL_RenderSetLogicalSize(state.renderer,
                             NATIVE_W * state.scale_factor,
                             NATIVE_H * state.scale_factor);
    tcache_reinit(state.renderer, state.scale_factor, &state.scaler);

     // Reset rendertarget
    reset_targets();
}

int video_reinit(int window_w,
                 int window_h,
                 int fullscreen,
                 int vsync,
                 const char* scaler_name,
                 int scale_factor) {

    // Tells if something has changed in video settings
    int changed = 0;

    // Check scaler
    if(strcmp(scaler_name, state.scaler_name) != 0) {
        strncpy(state.scaler_name, scaler_name, sizeof(state.scaler_name)-1);
        changed = 1;
    }
    if(scale_factor != state.scale_factor) {
        changed = 1;
    }

    // Set window size if necessary
    if(window_w != state.w || window_h != state.h || fullscreen != state.fs) {
        SDL_SetWindowSize(state.window, window_w, window_h);
        DEBUG("Changing resolution to %dx%d", window_w, window_h);
        changed = 1;
    }

    // Set fullscreen if necessary
    if(fullscreen != state.fs) {
        if(SDL_SetWindowFullscreen(state.window, fullscreen ? SDL_WINDOW_FULLSCREEN : 0) != 0) {
            PERROR("Could not set fullscreen mode!");
        } else {
            DEBUG("Fullscreen changed!");
        }

        changed = 1;
    }

    // Center window if we are changing into or in window mode
    if(!fullscreen && changed) {
        SDL_SetWindowPosition(state.window,
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
    }

    // Check for vsync changes
    if(vsync != state.vsync) {
        changed = 1;
    }

    // Set video state
    state.vsync = vsync;
    state.fs = fullscreen;
    state.w = window_w;
    state.h = window_h;

    // Load scaler
    if(video_load_scaler(scaler_name, scale_factor)) {
        DEBUG("Scaler %s plugin not found; using Nearest neighbour scaling.");
        state.scale_factor = 1;
    } else {
        DEBUG("Scaler %s loaded w/ factor %d", scaler_name, scale_factor);
        state.scale_factor = scale_factor;
    }

    // If any settings changed, reinit the screen
    if(changed) {
        video_reinit_renderer();
    }

    state.cb.render_reinit(&state);
    return 0;
}

void video_move_target(int x, int y) {
    state.target_move_x = x * state.scale_factor;
    state.target_move_y = y * state.scale_factor;
}

void video_get_state(int *w, int *h, int *fs, int *vsync) {
    if(w != NULL) {
        *w = state.w;
    }
    if(h != NULL) {
        *h = state.h;
    }
    if(fs != NULL) {
        *fs = state.fs;
    }
    if(vsync != NULL) {
        *vsync = state.vsync;
    }
}

void video_select_renderer(int renderer) {
    if(renderer == state.cur_renderer) {
        return;
    }
    state.cb.render_close(&state);
    state.cur_renderer = renderer;
    switch(renderer) {
        case VIDEO_RENDERER_QUIRKS:
            video_soft_init(&state);
            break;
        case VIDEO_RENDERER_HW:
            video_hw_init(&state);
            break;
    }
}

void video_set_fade(float fade) {
    state.fade = fade;
}

int video_screenshot(image *img) {
    image_create(img, state.w, state.h);
    int ret = SDL_RenderReadPixels(state.renderer, NULL, SDL_PIXELFORMAT_ABGR8888, img->data, img->w * 4);
    if(ret != 0) {
        PERROR("Unable to read pixels from rendertarget: %s", SDL_GetError());
        return 1;
    }
    return 0;
}

int video_area_capture(surface *sur, int x, int y, int w, int h) {
    float scale_x = (float)state.w / NATIVE_W;
    float scale_y = (float)state.h / NATIVE_H;

    // Correct position (take scaling into account)
    SDL_Rect r;
    r.x = x * scale_x;
    r.y = y * scale_y;
    r.w = w * scale_x;
    r.h = h * scale_y;

    // Create a new surface
    surface_create(sur, SURFACE_TYPE_RGBA, r.w, r.h);

    // Read pixels
    int ret = SDL_RenderReadPixels(state.renderer, &r, SDL_PIXELFORMAT_ABGR8888, sur->data, sur->w * 4);
    if(ret != 0) {
        surface_free(sur);
        PERROR("Unable to read pixels from renderer: %s", SDL_GetError());
        return 1;
    }

    return 0;
}

void video_force_pal_refresh() {
    memcpy(state.cur_palette->data, state.base_palette->data, 768);
    state.cur_palette->version++;
}

void video_set_base_palette(const palette *src) {
    memcpy(state.base_palette, src, sizeof(palette));
    memcpy(state.cur_palette->data, state.base_palette->data, 768);
    state.cur_palette->version++;
}

palette *video_get_base_palette() {
    return state.base_palette;
}

void video_copy_pal_range(const palette *src, int src_start, int dst_start, int amount) {
    memcpy(state.cur_palette->data + dst_start * 3,
           src->data + src_start * 3,
           amount * 3);
    state.cur_palette->version++;
}

screen_palette* video_get_pal_ref() {
    return state.cur_palette;
}

void video_render_prepare() {
    // Reset palette
    memcpy(state.cur_palette->data, state.base_palette->data, 768);
    SDL_SetRenderTarget(state.renderer, state.target);
    state.cb.render_prepare(&state);
}

void video_render_background(surface *sur) {
    state.cb.render_background(&state, sur);
}

void video_render_sprite_tint(
        surface *sur,
        int sx,
        int sy,
        color c,
        int pal_offset) {

    // Position & correct height
    SDL_Rect dst;
    dst.w = sur->w;
    dst.h = sur->h;
    dst.x = sx;
    dst.y = sy;

    // Render
    state.cb.render_fsot(
        &state,
        sur,
        &dst,
        SDL_BLENDMODE_BLEND,
        pal_offset,
        0, 255, c); // pal_offset, opacity, tint
}

// Wrapper
void video_render_sprite(
        surface *sur,
        int sx,
        int sy,
        unsigned int rendering_mode,
        int pal_offset) {

    video_render_sprite_flip_scale_opacity(
        sur,
        sx, sy,
        rendering_mode,
        pal_offset,
        FLIP_NONE,
        1.0f,
        255);
}

// Wrapper
void video_render_sprite_flip_scale(
        surface *sur,
        int sx,
        int sy,
        unsigned int rendering_mode,
        int pal_offset,
        unsigned int flip_mode,
        float y_percent) {

    video_render_sprite_flip_scale_opacity(
        sur,
        sx, sy,
        rendering_mode,
        pal_offset,
        flip_mode,
        y_percent,
        255);
}

void video_render_sprite_flip_scale_opacity(
        surface *sur,
        int sx,
        int sy,
        unsigned int rendering_mode,
        int pal_offset,
        unsigned int flip_mode,
        float y_percent,
        uint8_t opacity) {

    video_render_sprite_flip_scale_opacity_tint(
        sur,
        sx, sy,
        rendering_mode,
        pal_offset,
        flip_mode,
        y_percent,
        opacity,
        color_create(0xFF, 0xFF, 0xFF, 0xFF));
}

void video_render_sprite_size(
        surface *sur,
        int sx, int sy,
        int sw, int sh) {

    // Position
    SDL_Rect dst;
    dst.w = sw;
    dst.h = sh;
    dst.x = sx;
    dst.y = sy;

    // Render
    state.cb.render_fsot(
        &state,
        sur,
        &dst,
        SDL_BLENDMODE_BLEND, // blendmode
        0, // Pal offset
        0, // flip
        0xFF, // opacity
        color_create(0xFF, 0xFF, 0xFF, 0xFF)); // tint
}

void video_render_sprite_flip_scale_opacity_tint(
        surface *sur,
        int sx,
        int sy,
        unsigned int rendering_mode,
        int pal_offset,
        unsigned int flip_mode,
        float y_percent,
        uint8_t opacity,
        color tint) {

    // Position
    SDL_Rect dst;
    dst.w = sur->w;
    dst.h = sur->h * y_percent;
    dst.x = sx;
    dst.y = sy + (sur->h - dst.h) / 2;

    // Flipping
    SDL_RendererFlip flip = 0;
    if(flip_mode & FLIP_HORIZONTAL) flip |= SDL_FLIP_HORIZONTAL;
    if(flip_mode & FLIP_VERTICAL) flip |= SDL_FLIP_VERTICAL;

    // Blend mode
    SDL_BlendMode blend_mode = SDL_BLENDMODE_BLEND;
    if(rendering_mode == BLEND_ADDITIVE)
        blend_mode = SDL_BLENDMODE_ADD;

    // Render
    state.cb.render_fsot(&state, sur, &dst, blend_mode, pal_offset, flip, opacity, tint);
}

// Called on every game tick
void video_tick() {
    tcache_tick();
}

// Called after frame has been rendered
void video_render_finish() {
    // Tell software/hardware renderer to finish up whatever it was doing
    state.cb.render_finish(&state);

    // Set our rendertarget to screen buffer.
    SDL_SetRenderTarget(state.renderer, NULL);

    // Clear screen (borders)
    SDL_SetRenderDrawColor(state.renderer, 0, 0, 0, 255);
    SDL_RenderClear(state.renderer);

    // Handle fading by color modulation
    uint8_t v = 255.0f * state.fade;
    SDL_SetTextureColorMod(state.target, v, v, v);

    // Set screen position. take into account scaling and target moves (screen shakes)
    SDL_Rect dst;
    dst.x = state.target_move_x * state.scale_factor;
    dst.y = state.target_move_y * state.scale_factor;
    dst.w = NATIVE_W * state.scale_factor;
    dst.h = NATIVE_H * state.scale_factor;
    SDL_RenderCopy(state.renderer, state.target, NULL, &dst);

    // Reset color modulation to normal
    SDL_SetTextureColorMod(state.target, 0xFF, 0xFF, 0xFF);

    // Flip buffers. If vsync is off, we should sleep here
    // so hat our main loop doesn't eat up all cpu :)
    SDL_RenderPresent(state.renderer);
    if(!state.vsync) {
        SDL_Delay(1);
    }
}

void video_close() {
    state.cb.render_close(&state);
    tcache_close();
    SDL_DestroyTexture(state.target);
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    omf_free(state.cur_palette);
    omf_free(state.base_palette);
    INFO("Video deinit.");
}
