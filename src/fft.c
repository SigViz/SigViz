#include "fft.h"
#include "shared.h"
#include <math.h>
#include <stdlib.h>

// A simple structure to hold a complex number
typedef struct {
    double real;
    double imag;
} Complex;

// Swaps two complex numbers (can be static as it's only used here)
static void swap_complex(Complex* a, Complex* b) {
    Complex temp = *a;
    *a = *b;
    *b = temp;
}

// The Radix-2 Cooley-Tukey FFT algorithm
static void fft(Complex* x, int N) {
    if (N <= 1) return;

    // Bit-Reversal Permutation
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

    // Cooley-Tukey Algorithm
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

// Main function to calculate and draw the power spectrum
void calculate_and_draw_spectrum(
    SDL_Renderer* renderer,
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type,
    // Add the parameters here as well
    WindowType current_window_type,
    ViewMode current_view,
    int mouse_x)
{
    // 1. DYNAMIC MEMORY ALLOCATION
    if ((fft_size & (fft_size - 1)) != 0 || fft_size == 0) {
        return; // FFT size must be a power of 2
    }
    Complex* fft_buffer = (Complex*)malloc(fft_size * sizeof(Complex));
    double* psd = (double*)malloc((fft_size / 2) * sizeof(double));

    if (fft_buffer == NULL || psd == NULL) {
        if (fft_buffer) free(fft_buffer);
        if (psd) free(psd);
        return;
    }

    // 2. Generate the signal data to be transformed
    if (activeMessageLength > 0) {
        double phase = 0.0;
        double symbol_period_seconds = (double)pixelsPerBit / sampling_rate;
        int total_symbols = (activeMessageLength * 8) / bitsPerSymbol;

        for (int i = 0; i < fft_size; ++i) {
            double current_time = time_offset + (double)i / sampling_rate;
            double y = 0.0;

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
            
            fft_buffer[i].real = y;
            fft_buffer[i].imag = 0.0;

            double window_value = 1.0;
            if (current_window_type == WINDOW_HANN) {
                window_value = 0.5 * (1 - cos(2 * M_PI * i / (fft_size - 1)));
            } else if (current_window_type == WINDOW_HAMMING) {
                window_value = 0.54 - 0.46 * cos(2 * M_PI * i / (fft_size - 1));
            }
            fft_buffer[i].real *= window_value;
        }
    } else {
        for(int i = 0; i < fft_size; ++i) {
            fft_buffer[i].real = 0.0;
            fft_buffer[i].imag = 0.0;
        }
    }

    // 3. Run the FFT
    fft(fft_buffer, fft_size);

    // 4. Calculate the Power Spectral Density (PSD) in dB
    for (int i = 0; i < fft_size / 2; ++i) {
        double power = fft_buffer[i].real * fft_buffer[i].real + fft_buffer[i].imag * fft_buffer[i].imag;
        if (spectrum_power > 1) {
            power = pow(power, spectrum_power);
        }
        psd[i] = 10.0 * log10(power / fft_size + 1e-12);
    }
    
    // 5. Draw the spectrum with hover detection
    double nyquist = sampling_rate / 2.0;
    double freq_per_bin = nyquist / (fft_size / 2.0);

    double start_freq = spectrum_center_freq - (spectrum_span / 2.0);
    double end_freq = spectrum_center_freq + (spectrum_span / 2.0);
    if (start_freq < 0) start_freq = 0;
    if (end_freq > nyquist) end_freq = nyquist;

    int start_bin = (int)(start_freq / freq_per_bin);
    int end_bin = (int)(end_freq / freq_per_bin);
    if (start_bin < 0) start_bin = 0;
    if (end_bin >= fft_size / 2) end_bin = fft_size / 2 - 1;
    if (end_bin <= start_bin) end_bin = start_bin + 1;

    double max_db = -150.0;
    for (int i = start_bin; i <= end_bin; ++i) {
        if (psd[i] > max_db) max_db = psd[i];
    }

    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderDrawLine(renderer, 50, SCREEN_HEIGHT - 50, SCREEN_WIDTH - 50, SCREEN_HEIGHT - 50);

    // Reset hover info at the start of the frame
    hovered_frequency = 0.0;
    hovered_power = -999.0;

    for (int i = start_bin; i <= end_bin; ++i) {
        int x_pos = 50 + (int)(((double)(i - start_bin) / (end_bin - start_bin)) * (SCREEN_WIDTH - 100));
        int y_height = (int)((psd[i] - (max_db - 90.0)) / 90.0 * (SCREEN_HEIGHT - 100));
        if (y_height < 0) y_height = 0;
        int y_pos = SCREEN_HEIGHT - 50 - y_height;

        // Check if the mouse is over the current bar
        if (abs(mouse_x - x_pos) <= 2 && current_view == VIEW_POWER_SPECTRUM) {
            hovered_frequency = i * freq_per_bin;
            hovered_power = psd[i];
            SDL_SetRenderDrawColor(renderer, 255, 100, 100, 255); // Highlight in red
        } else {
            SDL_SetRenderDrawColor(renderer, 100, 255, 100, 255); // Default green
        }
        SDL_RenderDrawLine(renderer, x_pos, SCREEN_HEIGHT - 50, x_pos, y_pos);
    }
    
    // 6. FREE DYNAMICALLY ALLOCATED MEMORY
    free(fft_buffer);
    free(psd);
}