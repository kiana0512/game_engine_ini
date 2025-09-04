#pragma once
#include <SDL.h>          // 必须：SDL_Keycode 在这里声明
enum class Action { ToggleTexture, ReloadTexture, LoadMesh, ToggleHelp, None };
Action mapKey(SDL_Keycode key);
