#include "export_waveform.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern void downloadFile(const void* data, int dataSize, const char* filename);
#endif

#ifndef __EMSCRIPTEN__
#include "tinyfiledialogs.h"
#endif

void export_waveform(
    const char* activeMessage,
    int activeMessageLength,
    ModulationType current_mod_type)
{
    if (activeMessageLength == 0) {
        printf("No active message to export.\n");
        return;
    }

    double symbol_period_seconds = (double)pixelsPerBit / sampling_rate;
    int total_symbols = (activeMessageLength * 8) / bitsPerSymbol;
    if (total_symbols == 0 && activeMessageLength > 0) total_symbols = 1;
    int total_samples = (int)(total_symbols * symbol_period_seconds * sampling_rate);

    if (total_samples <= 0) {
        printf("No samples to export.\n");
        return;
    }

    float* waveform_data = (float*)malloc(total_samples * sizeof(float));
    if (waveform_data == NULL) {
        printf("Failed to allocate memory for waveform data.\n");
        return;
    }

    double phase = 0.0;

    for (int i = 0; i < total_samples; i++) {
        double current_time = (double)i / sampling_rate;
        double y = 0.0;

        switch (current_mod_type) {
            case MOD_ASK: {
                double shaped_envelope = 0.0;
                int M = 1 << bitsPerSymbol;
                int current_symbol_index = (int)(current_time / symbol_period_seconds);
                for (int j = -4; j <= 4; ++j) {
                    int symbol_index = current_symbol_index + j;
                    if (symbol_index < 0 || symbol_index >= total_symbols) continue;
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
                int symbol_index = (int)(current_time / symbol_period_seconds);
                if (symbol_index >= total_symbols) symbol_index = total_symbols - 1;
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
                for (int j = -4; j <= 4; ++j) {
                    int symbol_index = current_symbol_index + j;
                    if (symbol_index < 0 || symbol_index >= total_symbols) continue;
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
        waveform_data[i] = (float)y;
    }

#ifdef __EMSCRIPTEN__
    downloadFile(waveform_data, total_samples * sizeof(float), "waveform.32fl");
#else
    char const * filterPatterns[1] = { "*.32fl" };
    char const * saveFileName = tinyfd_saveFileDialog("Save Waveform", "waveform.32fl", 1, filterPatterns, "32-bit Float Waveform");
    if (saveFileName) {
        FILE* outFile = fopen(saveFileName, "wb");
        if (outFile) {
            fwrite(waveform_data, sizeof(float), total_samples, outFile);
            fclose(outFile);
        }
    }
#endif
    free(waveform_data);
}