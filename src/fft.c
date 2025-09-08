#include "fft.h"
#include "shared.h"
#include <math.h>
#include <stdlib.h>

typedef struct {
    double real;
    double imag;
} Complex;

static void swap_complex(Complex* a, Complex* b) {
    Complex temp = *a;
    *a = *b;
    *b = temp;
}

static void fft(Complex* x, int N) {
    if (N <= 1) return;

    int j = 0;
    for (int i = 1; i < N; i++) {
        int bit = N >> 1;
        while (j >= bit) {
            j -= bit;
            bit >>= 1;
        }
        j += bit;
        if (i < j) {
            swap_complex(&x[i], &x[j]);
        }
    }

    for (int len = 2; len <= N; len <<= 1) {
        double angle = -2.0 * M_PI / len;
        Complex wlen = {cos(angle), sin(angle)};
        for (int i = 0; i < N; i += len) {
            Complex w = {1.0, 0.0};
            for (j = 0; j < len / 2; j++) {
                Complex u = x[i + j];
                Complex v = {x[i + j + len / 2].real * w.real - x[i + j + len / 2].imag * w.imag,
                             x[i + j + len / 2].real * w.imag + x[i + j + len / 2].imag * w.real};
                x[i + j].real = u.real + v.real;
                x[i + j].imag = u.imag + v.imag;
                x[i + j + len / 2].real = u.real - v.real;
                x[i + j + len / 2].imag = u.imag - v.imag;
                double w_real_temp = w.real * wlen.real - w.imag * wlen.imag;
                w.imag = w.real * wlen.imag + w.imag * wlen.real;
                w.real = w_real_temp;
            }
        }
    }
}

void calculate_and_draw_spectrum(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type)
{
    const int FFT_SIZE = 2048;
    Complex fft_buffer[FFT_SIZE];
    double psd[FFT_SIZE / 2];

    if (activeMessageLength > 0) {
        // Generate the signal data to be transformed
        double phase = 0.0; // Private phase accumulator for FSK in this scope
        double symbol_period_seconds = (double)pixelsPerBit / sampling_rate;
        int total_symbols = (activeMessageLength * 8) / bitsPerSymbol;

        for (int i = 0; i < FFT_SIZE; ++i) {
            // We use a fixed chunk of the signal starting from time_offset
            double current_time = time_offset + (double)i / sampling_rate;
            double y = 0.0;

            // This switch block is a direct copy of the one in time_domain.c
            switch (current_mod_type) {
                case MOD_ASK: {
                    double shaped_envelope = 0.0;
                    int M = 1 << bitsPerSymbol;
                    int current_symbol_index = (int)(current_time / symbol_period_seconds);
                    for (int j = -4; j <= 4; ++j) {
                        int symbol_index = current_symbol_index + j;
                        if (symbol_index < 0) continue;
                        int symbol_value = get_symbol_at_index(symbol_index % total_symbols, activeMessage, activeMessageLength, bitsPerSymbol);
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
                    int symbol_index = (int)(current_time / symbol_period_seconds);
                    int symbol_value = get_symbol_at_index(symbol_index % total_symbols, activeMessage, activeMessageLength, bitsPerSymbol);
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
                    for (int j = -4; j <= 4; ++j) {
                        int symbol_index = current_symbol_index + j;
                        if (symbol_index < 0) continue;
                        int symbol_value = get_symbol_at_index(symbol_index % total_symbols, activeMessage, activeMessageLength, bitsPerSymbol);
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
            
            // Populate the FFT buffer with the generated sample
            fft_buffer[i].real = y;
            fft_buffer[i].imag = 0.0;

            // Apply a Hann window to the sample
            double hann_window = 0.5 * (1 - cos(2 * M_PI * i / (FFT_SIZE - 1)));
            fft_buffer[i].real *= hann_window;
        }
    }
    
    // For now, let's just transform a simple sine wave for demonstration
    for(int i = 0; i < FFT_SIZE; ++i) {
        double current_time = (double)i / sampling_rate;
        fft_buffer[i].real = amplitude * sin(2.0 * M_PI * frequency * current_time);
        fft_buffer[i].imag = 0.0;
        double hann_window = 0.5 * (1 - cos(2 * M_PI * i / (FFT_SIZE - 1)));
        fft_buffer[i].real *= hann_window;
    }

    fft(fft_buffer, FFT_SIZE);

    for (int i = 0; i < FFT_SIZE / 2; ++i) {
        double power = fft_buffer[i].real * fft_buffer[i].real + fft_buffer[i].imag * fft_buffer[i].imag;
        psd[i] = 10.0 * log10(power / FFT_SIZE + 1e-12);
    }

    double max_db = -100.0;
    for (int i = 0; i < FFT_SIZE / 2; ++i) {
        if (psd[i] > max_db) max_db = psd[i];
    }

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, 50, SCREEN_HEIGHT - 50, SCREEN_WIDTH - 50, SCREEN_HEIGHT - 50);

    SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255);
    for (int i = 0; i < FFT_SIZE / 2; ++i) {
        int x_pos = 50 + (int)((double)i / (FFT_SIZE / 2) * (SCREEN_WIDTH - 100));
        int y_height = (int)((psd[i] - (max_db - 60)) / 60.0 * (SCREEN_HEIGHT - 100));
        if (y_height < 0) y_height = 0;
        int y_pos = SCREEN_HEIGHT - 50 - y_height;
        SDL_RenderDrawLine(renderer, x_pos, SCREEN_HEIGHT - 50, x_pos, y_pos);
    }
}