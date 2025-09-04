#include "Input.h"
#include <SDL.h>
Action mapKey(SDL_Keycode key){
    switch(key){
        case SDLK_t: return Action::ToggleTexture;
        case SDLK_l: return Action::ReloadTexture;
        case SDLK_m: return Action::LoadMesh;
        case SDLK_h: return Action::ToggleHelp;
        default: return Action::None;
    }
}
