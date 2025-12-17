#define SDL_MAIN_HANDLED
#include "SDL.h"
#include "SDL_net.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    SDL_SetMainReady();
    if (SDL_Init(0) < 0) {
        fprintf(stderr, "could not initialize sdl2: %s\n", SDL_GetError());
        return 1;
    }
    if (SDLNet_Init() == -1) {
        fprintf(stderr, "SDLNet_Init: %s\n", SDLNet_GetError());
    }
    SDLNet_Quit();
    SDL_Quit();
    return 0;
}
