#ifndef IQ_PLOT_H
#define IQ_PLOT_H

#include "shared.h"

void draw_iq_plot(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type
);

#endif // IQ_PLOT_H