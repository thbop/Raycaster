#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
// Normally SDL2 will redefine the main entry point of the program for Windows applications
// this doesn't seem to play nice with TCC, so we just undefine the redefinition
#ifdef __TINYC__
    #undef main
#endif

#define CHECK_ERROR(test, message) \
    do { \
        if((test)) { \
            fprintf(stderr, "%s\n", (message)); \
            exit(1); \
        } \
    } while(0)

#define SCREEN_WIDTH 384
#define SCREEN_HEIGHT 216
#define SCREEN_HALF_WIDTH 192
#define SCREEN_HALF_HEIGHT 108

#define VIEWPLANE_DISTANCE 64

typedef unsigned int u32;
typedef unsigned char u8;

typedef struct vec2 { float x, y; } vec2;

float cross2( vec2 p, vec2 q ) {
    return p.x * q.y - p.y * q.x;
}

struct {
    SDL_Window* window;
    SDL_Texture* texture;
    SDL_Renderer *renderer;
    u32 pixels[SCREEN_WIDTH * SCREEN_HEIGHT];
    bool running;
} state;

struct {
    vec2 pos;
    float rot;
} player;

static float Q_rsqrt( float number ) {
    // Don't ask, watch this: https://youtu.be/p8u_k2LIZyo?si=n8mRBL3u3PjkUHsR
    long i;
    float x2, y;
    
    x2 = number * 0.5f;
    y = number;
    i = *( long* ) &y;
    i = 0x5F3759DF - ( i >> 1 );
    y = *(float*) &i;
    y *= ( 1.5f - ( x2 * y*y ) );

    return y;
}


static float cast_ray( vec2 L0, vec2 L1, int sx ) { // Create some kind of line stepper to go through all the line points
    // Generates a direction from camera origin to view plane
    vec2 dir = { -(SCREEN_HALF_WIDTH >> 4) + (sx >> 4), VIEWPLANE_DISTANCE };
    float rdirlen = Q_rsqrt( dir.x*dir.x + dir.y*dir.y );
    dir.x *= rdirlen; dir.y *= rdirlen; // Divide by length

    // Line intersection tests
    vec2 s  = { L0.x - L1.x, L0.y - L1.y };
    vec2 qmp = { L0.x - player.pos.x, L0.y - player.pos.y }; // q - s
    float rxs = cross2( dir, s ); // r x s

    if ( !rxs ) return -1.0f;

    float t = cross2( qmp, (vec2){ s.x / rxs, s.y / rxs } );
    float u = cross2( qmp, (vec2){ dir.x / rxs, dir.y / rxs } ); // u will be helpful for uvs
    // printf( "%f %f\n", t, u );
    if ( t <= 0.0f || u < 0.0f || u > 1.0f ) return -1.0f;


    return t;
}

static void vline( int x, int halfheight, u32 color ) {
    for ( int j = 0; j < halfheight; j++ ) {
        state.pixels[ ((SCREEN_HALF_HEIGHT+j) * SCREEN_WIDTH) + x ] = color;
        state.pixels[ ((SCREEN_HALF_HEIGHT-j) * SCREEN_WIDTH) + x ] = color;
    }
}


int main(int argc, char** argv) {
    // Initialize SDL
    CHECK_ERROR(SDL_Init(SDL_INIT_VIDEO) != 0, SDL_GetError());

    // Create an SDL window
    state.window = SDL_CreateWindow(
        "Raycaster",
        SDL_WINDOWPOS_CENTERED_DISPLAY(0),
        SDL_WINDOWPOS_CENTERED_DISPLAY(0),
        1280, 720, SDL_WINDOW_ALLOW_HIGHDPI
    );
    CHECK_ERROR(state.window == NULL, SDL_GetError());

    state.renderer = SDL_CreateRenderer(state.window, -1, SDL_RENDERER_PRESENTVSYNC);    
    CHECK_ERROR(state.renderer == NULL, SDL_GetError());

    // Create a render texture
    state.texture = SDL_CreateTexture(
        state.renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_WIDTH, SCREEN_HEIGHT
    );
    CHECK_ERROR(state.texture == NULL, SDL_GetError());

    SDL_Event event;
    state.running = true;

    player.pos = (vec2){ 0.0f, 0.0f };

    vec2 world[] = {
        { 0.0f, 128.0f },
        { 0.0f, 64.0f },
        { -64.0f, 32.0f },
    };
    int worldSize = 3;

    long colors[] = {
        0xFF0000FF,
        0x00FF00FF
    };

    while (state.running) {
        // Process events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    state.running = false;
                    break;
            }
        }

        const u8* keystate = SDL_GetKeyboardState(NULL);
        if ( keystate[SDL_SCANCODE_W] )
            player.pos.y += 1;
        else if ( keystate[SDL_SCANCODE_S] )
            player.pos.y -= 1;
        
        if ( keystate[SDL_SCANCODE_D] )
            player.pos.x += 1;
        else if ( keystate[SDL_SCANCODE_A] )
            player.pos.x -= 1;

        memset(state.pixels, 0, sizeof(state.pixels)); // Clear screen

        for ( int i = 0; i < SCREEN_WIDTH; i++ ) {
            for ( int l = 0; l < worldSize; l++ ) {
                float dis = -1.0f;
                if ( l+1 <= worldSize ) {
                    dis = cast_ray( world[l], world[l+1], i );
                }
                if ( dis != -1.0f ) {
                    vline( i, SCREEN_HALF_HEIGHT - dis, colors[l] );
                }
            }
        }

        SDL_UpdateTexture(state.texture, NULL, state.pixels, SCREEN_WIDTH * 4); // 4 = sizeof(u32)
        SDL_RenderCopyEx(
            state.renderer, state.texture,
            NULL, NULL, 0.0, NULL, 0
        );
        SDL_RenderPresent(state.renderer);
    }

    // Release resources
    SDL_DestroyTexture(state.texture);
    SDL_DestroyRenderer(state.renderer);
    SDL_DestroyWindow(state.window);
    SDL_Quit();

    return 0;
}