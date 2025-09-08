#ifndef TIME_DOMAIN_H
#define TIME_DOMAIN_H

#include "shared.h"

void draw_time_domain_view(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type
);

#endif // TIME_DOMAIN_H