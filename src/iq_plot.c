#include "iq_plot.h"
#include "shared.h"
#include <math.h>
#include <stdlib.h>

void draw_iq_plot(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type)
{
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2);
    SDL_RenderDrawLine(renderer, SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT);

    if (activeMessageLength <= 0) return;

    int M = 1 << bitsPerSymbol;
    int total_symbols = (activeMessageLength * 8) / bitsPerSymbol;

    int prev_x_pos = SCREEN_WIDTH / 2;
    int prev_y_pos = SCREEN_HEIGHT / 2;

    for (int i = 0; i < total_symbols; ++i) {
        int symbol_value = get_symbol_at_index(i, activeMessage, activeMessageLength, bitsPerSymbol);
        double ideal_I = 0.0, ideal_Q = 0.0;

        switch (current_mod_type) {
            case MOD_ASK:
                ideal_I = (M == 1) ? symbol_value : (double)symbol_value / (M - 1);
                ideal_Q = 0.0;
                break;
            case MOD_FSK:
            case MOD_PSK:
                double angle = (2.0 * M_PI * symbol_value) / M;
                if (current_mod_type == MOD_PSK && M == 4) angle += M_PI / 4.0;
                ideal_I = cos(angle);
                ideal_Q = sin(angle);
                break;
        }

        double noise_I = 0.0, noise_Q = 0.0;
        if (snr_db < 100) {
            double snr_linear = pow(10.0, snr_db / 10.0);
            double noise_power_per_channel = 0.5 / snr_linear;
            double noise_std_dev = sqrt(noise_power_per_channel);
            
            double u1 = (rand() + 1.0) / (RAND_MAX + 1.0);
            double u2 = (rand() + 1.0) / (RAND_MAX + 1.0);
            noise_I = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2) * noise_std_dev;
            noise_Q = sqrt(-2.0 * log(u1)) * sin(2.0 * M_PI * u2) * noise_std_dev;
        }

        float plot_scale = SCREEN_HEIGHT / 3.0;
        int x_pos = SCREEN_WIDTH / 2 + (int)((ideal_I + noise_I) * plot_scale);
        int y_pos = SCREEN_HEIGHT / 2 - (int)((ideal_Q + noise_Q) * plot_scale);

        if (i > 0) {
            SDL_SetRenderDrawColor(renderer, 0, 150, 255, 100);
            SDL_RenderDrawLine(renderer, prev_x_pos, prev_y_pos, x_pos, y_pos);
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
        SDL_Rect point_rect = {x_pos - 2, y_pos - 2, 5, 5};
        SDL_RenderFillRect(renderer, &point_rect);

        prev_x_pos = x_pos;
        prev_y_pos = y_pos;
    }
}