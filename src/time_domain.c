#include "time_domain.h"
#include "shared.h"
#include <math.h>
#include <stdlib.h>

void draw_time_domain_view(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2);

    int prev_y = SCREEN_HEIGHT / 2;
    double phase = 0.0; // For FSK phase accumulation

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double current_time = time_offset + ((double)x / pixels_per_second);
        int sample_index = (int)(current_time * sampling_rate);

        if (activeMessageLength > 0) {
            int total_message_samples = activeMessageLength * 8 * pixelsPerBit;
            if (total_message_samples > 0) {
                sample_index %= total_message_samples;
            }
        }

        double y = 0.0;
        double symbol_period_seconds = (double)pixelsPerBit / sampling_rate;
        
        switch (current_mod_type) {
            case MOD_ASK: {
                double shaped_envelope = 0.0;
                int M = 1 << bitsPerSymbol;
                int current_symbol_index = (int)(current_time / symbol_period_seconds);

                for (int i = -4; i <= 4; ++i) {
                    int symbol_index = current_symbol_index + i;
                    if (symbol_index < 0) continue;

                    int symbol_value = get_symbol_at_index(symbol_index, activeMessage, activeMessageLength, bitsPerSymbol);
                    double impulse_value = (M == 1) ? symbol_value : (double)symbol_value / (M - 1);
                    
                    double symbol_center_time = (symbol_index + 0.5) * symbol_period_seconds;
                    double time_from_center = current_time - symbol_center_time;
                    double filter_kernel_value = raised_cosine(time_from_center, symbol_period_seconds, rolloff_factor);
                    
                    shaped_envelope += impulse_value * filter_kernel_value;
                }
                double current_amplitude = amplitude * shaped_envelope;
                y = current_amplitude * sin(2.0 * M_PI * frequency * current_time);
                break;
            }
            case MOD_FSK: {
                int M = 1 << bitsPerSymbol;
                int symbol_index = (int)(current_time / symbol_period_seconds);
                int symbol_value = get_symbol_at_index(symbol_index, activeMessage, activeMessageLength, bitsPerSymbol);
                
                double frequency_separation = frequency / 2.0;
                double current_freq = frequency + (symbol_value * frequency_separation);

                double phase_increment = 2.0 * M_PI * current_freq / sampling_rate;
                phase += phase_increment;
                y = amplitude * sin(phase);
                break;
            }
            case MOD_PSK: {
                double shaped_I = 0.0, shaped_Q = 0.0;
                int M = 1 << bitsPerSymbol;
                int current_symbol_index = (int)(current_time / symbol_period_seconds);

                for (int i = -4; i <= 4; ++i) {
                    int symbol_index = current_symbol_index + i;
                    if (symbol_index < 0) continue;

                    int symbol_value = get_symbol_at_index(symbol_index, activeMessage, activeMessageLength, bitsPerSymbol);
                    double angle = (2.0 * M_PI * symbol_value) / M;
                    if (M == 4) angle += M_PI / 4.0;
                    double impulse_I = cos(angle);
                    double impulse_Q = sin(angle);

                    double symbol_center_time = (symbol_index + 0.5) * symbol_period_seconds;
                    double time_from_center = current_time - symbol_center_time;
                    double filter_kernel_value = raised_cosine(time_from_center, symbol_period_seconds, rolloff_factor);
                    
                    shaped_I += impulse_I * filter_kernel_value;
                    shaped_Q += impulse_Q * filter_kernel_value;
                }
                double carrier_phase = 2.0 * M_PI * frequency * current_time;
                y = amplitude * (shaped_I * cos(carrier_phase) - shaped_Q * sin(carrier_phase));
                break;
            }
        }

        if (amplitude > 0 && snr_db < 100) {
            double signal_power = (amplitude * amplitude) / 2.0;
            double snr_linear = pow(10.0, snr_db / 10.0);
            double noise_power = signal_power / snr_linear;
            double noise_std_dev = sqrt(noise_power);

            double u1 = (rand() + 1.0) / (RAND_MAX + 1.0);
            double u2 = (rand() + 1.0) / (RAND_MAX + 1.0);
            double gaussian_noise = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
            
            y += gaussian_noise * noise_std_dev;
        }

        int current_y = (SCREEN_HEIGHT / 2) - (int)y;
        if (x > 0) {
            SDL_RenderDrawLine(renderer, x - 1, prev_y, x, current_y);
        }
        prev_y = current_y;
    }
}