#ifndef FFT_H
#define FFT_H

#include "shared.h"

void calculate_and_draw_spectrum(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type,
    WindowType current_window_type,
    ViewMode current_view,
    int mouse_x
);

#endif // FFT_H