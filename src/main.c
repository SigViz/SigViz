// src/main.c (Corrected and Final Version)

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "text_renderer.h"
#ifndef __EMSCRIPTEN__
#include "tinyfiledialogs.h"
#endif

// --- Defines and Enums ---
#define INPUT_BUFFER_SIZE 256
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum { MODE_TYPING, MODE_COMMAND } AppMode;
typedef enum { MOD_ASK, MOD_FSK, MOD_PSK } ModulationType;
typedef enum { VIEW_TIME_DOMAIN, VIEW_IQ_PLOT } ViewMode;

// --- Global Variables ---
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TextObject status_line1, status_line2, mode_indicator_text, input_text_display, help_prompt_text;
TTF_Font* font_size_20 = NULL;
TTF_Font* font_size_18 = NULL;

// Screen and Signal Parameters
int SCREEN_WIDTH = 1240;
int SCREEN_HEIGHT = 720;
double amplitude = 100.0;
double frequency = 300.0;
int pixelsPerBit = 50;
double snr_db = 100.0; // High SNR = low noise. Set to 100 for "off"
double rolloff_factor = 0.35;
int bitsPerSymbol = 1; // 1=BPSK, 2=QPSK, 3=8PSK, etc.
double time_offset = 0.0; // Time in seconds to shift the signal
double sampling_rate = 4000.0;
double pixels_per_second = 500.0;

// App State
AppMode current_mode = MODE_TYPING;
ModulationType current_mod_type = MOD_ASK;
ViewMode current_view = VIEW_TIME_DOMAIN;
bool needsTextUpdate = true;
bool needsAngleUpdate = true;
bool showHelpScreen = false;
bool quit = false;

// Text Buffers
char inputText[INPUT_BUFFER_SIZE] = {0};
int inputTextLength = 0;
char activeMessage[INPUT_BUFFER_SIZE] = {0};
int activeMessageLength = 0;

#ifdef __EMSCRIPTEN__
extern void downloadFile(const void* data, int dataSize, const char* filename);
#endif

// --- Helper Functions ---

// Sinc function: sin(pi*x) / (pi*x)
double sinc(double x) {
    if (fabs(x) < 1e-9) {
        return 1.0; // Limit of sinc(x) as x -> 0 is 1.0
    }
    return sin(M_PI * x) / (M_PI * x);
}

// Raised Cosine pulse shaping function
double raised_cosine(double t, double T_s, double beta) {
    if (fabs(t) > 2 * T_s) return 0; // Optimization

    if (fabs(t) < 1e-9) {
        return 1.0;
    }

    if (beta > 1e-9 && fabs(fabs(2.0 * beta * t / T_s) - 1.0) < 1e-9) {
        return M_PI / 4.0 * sinc(1.0 / (2.0 * beta));
    }

    double term1 = sin(M_PI * t / T_s) / (M_PI * t / T_s);
    double term2 = cos(M_PI * beta * t / T_s) / (1.0 - pow(2.0 * beta * t / T_s, 2.0));
    return term1 * term2;
}

// Fetches the integer value of a symbol from the message buffer
int get_symbol_at_index(int symbol_index, const char* message, int message_len, int bits_per_sym) {
    int start_bit_index = symbol_index * bits_per_sym;
    if ((start_bit_index / 8) >= message_len) return 0;

    int symbol_value = 0;
    for (int i = 0; i < bits_per_sym; ++i) {
        int current_bit_index = start_bit_index + i;
        int char_index = current_bit_index / 8;
        if (char_index >= message_len) continue;

        int bit_in_char = current_bit_index % 8;
        int bit = (message[char_index] >> (7 - bit_in_char)) & 1;
        symbol_value = (symbol_value << 1) | bit;
    }
    return symbol_value;
}

// --- Waveform Export Function ---
void export_waveform() {
    if (activeMessageLength == 0) {
        printf("No active message to export.\n");
        return;
    }

    int total_samples = activeMessageLength * 8 * pixelsPerBit;
    float* waveform_data = (float*)malloc(total_samples * sizeof(float));
    if (waveform_data == NULL) {
        printf("Failed to allocate memory for waveform data.\n");
        return;
    }

    double phase = 0.0; // For FSK

    for (int x = 0; x < total_samples; x++) {
        double time = (double)x * 0.01;
        double y = 0.0;

        // NOTE: This logic is a mirror of the main drawing loop for consistency
        switch (current_mod_type) {
            case MOD_ASK: {
                double base_amplitude = 0.0;
                int current_symbol_index = (x / pixelsPerBit);
                for (int i = -2; i <= 2; ++i) {
                    int symbol_index = current_symbol_index + i;
                    if (symbol_index < 0) continue;
                    int bit_value = get_symbol_at_index(symbol_index, activeMessage, activeMessageLength, 1);
                    if (bit_value == 1) {
                        double time_in_sample = x % pixelsPerBit;
                        double symbol_center_offset = i * pixelsPerBit;
                        double time_from_center = time_in_sample - symbol_center_offset;
                        base_amplitude += raised_cosine(time_from_center, pixelsPerBit, rolloff_factor);
                    }
                }
                double current_amplitude = amplitude * (0.1 + 0.9 * base_amplitude);
                y = current_amplitude * sin(2.0 * M_PI * frequency * time);
                break;
            }
            case MOD_FSK: {
                int bit_index = x / pixelsPerBit;
                int bit_value = get_symbol_at_index(bit_index, activeMessage, activeMessageLength, 1);
                double freq_mark = frequency;
                double freq_space = frequency * 2.0;
                double current_freq = (bit_value == 1) ? freq_mark : freq_space;
                double phase_increment = 2.0 * M_PI * current_freq * 0.01;
                phase += phase_increment;
                y = amplitude * sin(phase);
                break;
            }
            case MOD_PSK: {
                double shaped_I = 0.0, shaped_Q = 0.0;
                int M = 1 << bitsPerSymbol;
                int current_symbol_index = (x / pixelsPerBit);
                for (int i = -2; i <= 2; ++i) {
                    int symbol_index = current_symbol_index + i;
                    if (symbol_index < 0) continue;
                    int symbol_value = get_symbol_at_index(symbol_index, activeMessage, activeMessageLength, bitsPerSymbol);
                    double angle = (2.0 * M_PI * symbol_value) / M;
                    if (M == 4) angle += M_PI / 4.0;
                    double current_I = cos(angle);
                    double current_Q = sin(angle);
                    double time_in_sample = x % pixelsPerBit;
                    double symbol_center_offset = i * pixelsPerBit;
                    double time_from_center = time_in_sample - symbol_center_offset;
                    double pulse_shape_value = raised_cosine(time_from_center, pixelsPerBit, rolloff_factor);
                    shaped_I += current_I * pulse_shape_value;
                    shaped_Q += current_Q * pulse_shape_value;
                }
                double carrier_phase = 2.0 * M_PI * frequency * time;
                y = amplitude * (shaped_I * cos(carrier_phase) - shaped_Q * sin(carrier_phase));
                break;
            }
        }

        if (amplitude > 0 && snr_db < 100) {
            double signal_power = (amplitude * amplitude) / 2.0;
            double snr_linear = pow(10.0, snr_db / 10.0);
            double noise_power = signal_power / snr_linear;
            double noise_amplitude_scaler = sqrt(3.0 * noise_power);
            double noise_sample = (2.0 * (rand() / (double)RAND_MAX) - 1.0) * noise_amplitude_scaler;
            y += noise_sample;
        }
        waveform_data[x] = (float)y;
    }

#ifdef __EMSCRIPTEN__
    downloadFile(waveform_data, total_samples * sizeof(float), "waveform.32fl");
    printf("Triggering full waveform download...\n");
#else
    char const * filterPatterns[1] = { "*.32fl" };
    char const * saveFileName = tinyfd_saveFileDialog("Save Waveform", "waveform.32fl", 1, filterPatterns, "32-bit Float Waveform");
    if (saveFileName) {
        FILE* outFile = fopen(saveFileName, "wb");
        if (outFile) {
            fwrite(waveform_data, sizeof(float), total_samples, outFile);
            fclose(outFile);
            printf("Full waveform exported to %s\n", saveFileName);
        } else {
            perror("Error opening file for writing");
        }
    } else {
        printf("Save file dialog cancelled.\n");
    }
#endif
    free(waveform_data);
}

// --- Main Loop Function ---
void main_loop() {
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);
            needsTextUpdate = true;
        }

        if (e.type == SDL_QUIT) { quit = true; }

        if (e.type == SDL_TEXTINPUT && current_mode == MODE_TYPING) {
            if (inputTextLength < INPUT_BUFFER_SIZE - 1) {
                strcat(inputText, e.text.text);
                inputTextLength++;
                needsTextUpdate = true;
            }
        }

        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_TAB && !showHelpScreen) {
                current_mode = (current_mode == MODE_TYPING) ? MODE_COMMAND : MODE_TYPING;
                if (current_mode == MODE_TYPING) SDL_StartTextInput(); else SDL_StopTextInput();
                needsTextUpdate = true;
            }

            if (current_mode == MODE_COMMAND && !(e.key.keysym.mod & KMOD_CTRL)) {
                switch (e.key.keysym.sym) {
                    case SDLK_h: showHelpScreen = !showHelpScreen; needsTextUpdate = true; break;
                    case SDLK_1: current_mod_type = MOD_ASK; needsTextUpdate = true; break;
                    case SDLK_2: current_mod_type = MOD_FSK; needsTextUpdate = true; break;
                    case SDLK_3: current_mod_type = MOD_PSK; needsTextUpdate = true; break;
                    case SDLK_b:
                        if (e.key.keysym.mod & KMOD_SHIFT) { rolloff_factor += 0.05; } else { rolloff_factor -= 0.05; }
                        if (rolloff_factor > 1.0) { rolloff_factor = 1.0; } // Use braces or separate lines
                        if (rolloff_factor < 0.0) { rolloff_factor = 0.0; }
                        needsTextUpdate = true; break;
                    case SDLK_n:
                        if (e.key.keysym.mod & KMOD_SHIFT) { snr_db += 1.0; } else { snr_db -= 1.0; }
                        needsTextUpdate = true; break;
                    case SDLK_m:
                        if (e.key.keysym.mod & KMOD_SHIFT) { bitsPerSymbol++; } else { bitsPerSymbol--; }
                        if (bitsPerSymbol > 4) { bitsPerSymbol = 4; } // Use braces or separate lines
                        if (bitsPerSymbol < 1) { bitsPerSymbol = 1; }
                        needsTextUpdate = true; break;
                    case SDLK_p:
                        if (e.key.keysym.mod & KMOD_SHIFT) { pixelsPerBit += 2; } else { pixelsPerBit -= 2; }
                        if (pixelsPerBit < 4) pixelsPerBit = 4;
                        needsTextUpdate = true; break;
                    case SDLK_SPACE: needsAngleUpdate = !needsAngleUpdate; break;
                    case SDLK_j: time_offset -= 0.1; if (time_offset < 0) { time_offset = 0; } break; // Use time_offset
                    case SDLK_l: time_offset += 0.1; break;   
                    case SDLK_0:
                        frequency = 30.0; amplitude = 100.0; snr_db = 100.0; pixelsPerBit = 50;
                        rolloff_factor = 0.35; bitsPerSymbol = 1; time_offset = 0.0; // Use time_offset
                        needsTextUpdate = true; break;
                    case SDLK_r: time_offset = 0; break; 
                    case SDLK_s: export_waveform(); break;
                }
            }

            if (!showHelpScreen) {
                if (e.key.keysym.mod & KMOD_CTRL) { // Check if the Control key is held down
                    switch (e.key.keysym.sym) {
                        case SDLK_1:
                            current_view = VIEW_TIME_DOMAIN;
                            printf("Switched to Time Domain view.\n");
                            break;
                        case SDLK_2:
                            // I/Q plot is only meaningful for PSK/QAM
                            if (current_mod_type == MOD_PSK) {
                                current_view = VIEW_IQ_PLOT;
                                printf("Switched to I/Q Plot view.\n");
                            } else {
                                printf("I/Q Plot is only available for PSK modulation.\n");
                            }
                            break;
                    }
                }

                switch (e.key.keysym.sym) {
                    case SDLK_UP: amplitude += 5.0; needsTextUpdate = true; break;
                    case SDLK_DOWN: amplitude -= 5.0; if (amplitude < 0) amplitude = 0; needsTextUpdate = true; break;
                    case SDLK_RIGHT: frequency += 0.1; needsTextUpdate = true; break;
                    case SDLK_LEFT: frequency -= 0.1; if (frequency < 0.1) frequency = 0.1; needsTextUpdate = true; break;
                    case SDLK_RETURN:
                        strcpy(activeMessage, inputText); activeMessageLength = inputTextLength;
                        inputText[0] = '\0'; inputTextLength = 0;
                        time_offset = 0; needsTextUpdate = true; break; // Use time_offset
                    case SDLK_BACKSPACE:
                        if (inputTextLength > 0) { inputText[--inputTextLength] = '\0'; }
                        needsTextUpdate = true; break;
                }
            }
        }
    }

    if (needsTextUpdate) {
        char buffer_l1[256], buffer_l2[256], buffer_mode[256];
        const char* mod_str = (current_mod_type == MOD_ASK) ? "ASK" : (current_mod_type == MOD_FSK) ? "FSK" : "PSK";
        
        char psk_order_str[16] = "";
        if (current_mod_type == MOD_PSK) {
            sprintf(psk_order_str, " (%d-PSK)", 1 << bitsPerSymbol);
        }

        snprintf(buffer_l1, sizeof(buffer_l1), "A:%.0f F:%.1f %s%s", amplitude, frequency, mod_str, psk_order_str);
        snprintf(buffer_l2, sizeof(buffer_l2), "px/bit:%d SNR:%.0fdB Roll-off:%.2f", pixelsPerBit, snr_db, rolloff_factor);
        snprintf(buffer_mode, sizeof(buffer_mode), "Mode: %s (TAB to switch)", current_mode == MODE_TYPING ? "Typing" : "Command");

        update_text_object(&status_line1, buffer_l1);
        update_text_object(&status_line2, buffer_l2);
        update_text_object(&mode_indicator_text, buffer_mode);
        update_text_object(&input_text_display, inputText);
        update_text_object(&help_prompt_text, "Press H for help");
        needsTextUpdate = false;
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (showHelpScreen) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_Rect overlayRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderFillRect(renderer, &overlayRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        const char* help_lines[] = {
            "--- CONTROLS (COMMAND MODE) ---",
            " ",
            "TAB       - Toggle Typing/Command Mode",
            "H         - Toggle this Help Screen",
            "1,2,3     - Switch Modulation (ASK, FSK, PSK)",
            "Arrows    - Adjust Amplitude & Frequency",
            "M/Shift+M - Decrease/Increase PSK Order (BPSK, QPSK...)",
            "N/Shift+N - Decrease/Increase SNR",
            "B/Shift+B - Decrease/Increase Roll-off Factor",
            "P/Shift+P - Decrease/Increase Pixels per Bit",
            "J/L       - Scroll Left/Right through Signal",
            "R         - Reset Scroll to Start",
            "Space     - Pause/Resume Scrolling",
            "0 (zero)  - Reset All Waveform Parameters",
            "S         - Save Waveform as .32fl file",
            "Enter     - Modulate Typed Message",
            NULL
        };
        int y_pos = 100;
        for (int i = 0; help_lines[i] != NULL; i++) {
            update_text_object(&status_line1, help_lines[i]);
            draw_text_object(&status_line1, (SCREEN_WIDTH - status_line1.rect.w) / 2, y_pos);
            y_pos += status_line1.rect.h + 5;
        }
    } else {
    // This block runs when the help screen is OFF
    if (current_view == VIEW_TIME_DOMAIN) {
        // --- TIME DOMAIN VIEW ---
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
                double noise_amplitude_scaler = sqrt(3.0 * noise_power);
                double noise_sample = (2.0 * (rand() / (double)RAND_MAX) - 1.0) * noise_amplitude_scaler;
                y += noise_sample;
            }

            int current_y = (SCREEN_HEIGHT / 2) - (int)y;
            if (x > 0) {
                SDL_RenderDrawLine(renderer, x - 1, prev_y, x, current_y);
            }
            prev_y = current_y;
        } // End for loop
    } else if (current_view == VIEW_IQ_PLOT) {
        // --- I/Q PLOT VIEW ---
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2); // I-axis
        SDL_RenderDrawLine(renderer, SCREEN_WIDTH / 2, 0, SCREEN_WIDTH / 2, SCREEN_HEIGHT); // Q-axis

        if (activeMessageLength > 0) {
            int M = 1 << bitsPerSymbol;
            int total_symbols = (activeMessageLength * 8) / bitsPerSymbol;

            for (int i = 0; i < total_symbols; ++i) {
                int symbol_value = get_symbol_at_index(i, activeMessage, activeMessageLength, bitsPerSymbol);
                double angle = (2.0 * M_PI * symbol_value) / M;
                if (M == 4) angle += M_PI / 4.0;
                double ideal_I = cos(angle);
                double ideal_Q = sin(angle);

                double noise_I = 0.0, noise_Q = 0.0;
                if (snr_db < 100) {
                    double snr_linear = pow(10.0, snr_db / 10.0);
                    double noise_power_per_channel = 0.5 / snr_linear;
                    double noise_std_dev = sqrt(noise_power_per_channel);
                    noise_I = ((rand() / (double)RAND_MAX) - 0.5 + (rand() / (double)RAND_MAX) - 0.5) * noise_std_dev * 4.0;
                    noise_Q = ((rand() / (double)RAND_MAX) - 0.5 + (rand() / (double)RAND_MAX) - 0.5) * noise_std_dev * 4.0;
                }

                float plot_scale = SCREEN_HEIGHT / 3.0;
                int x_pos = SCREEN_WIDTH / 2 + (int)((ideal_I + noise_I) * plot_scale);
                int y_pos = SCREEN_HEIGHT / 2 - (int)((ideal_Q + noise_Q) * plot_scale);

                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_Rect point_rect = {x_pos - 2, y_pos - 2, 5, 5};
                SDL_RenderFillRect(renderer, &point_rect);
            }
        }
    } // End view check

    // Draw text overlays
    draw_text_object(&status_line1, 10, 10);
    draw_text_object(&status_line2, 10, 10 + status_line1.rect.h);
    draw_text_object(&mode_indicator_text, 10, 10 + status_line1.rect.h + status_line2.rect.h);
    draw_text_object(&input_text_display, 10, SCREEN_HEIGHT - input_text_display.rect.h - 10);
    draw_text_object(&help_prompt_text, SCREEN_WIDTH - help_prompt_text.rect.w - 10, 10);
}

    SDL_RenderPresent(renderer);
    if (needsAngleUpdate && !showHelpScreen) {
        time_offset += 1.0 / 60.0; // Scroll at a constant time rate (e.g., for 60fps)
    }

    #ifndef __EMSCRIPTEN__
    SDL_Delay(10);
    #endif

    if (quit) {
        #ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
        #endif
    }
}

// --- Main Entry Point ---
int main(int argc, char* argv[]) {
    (void)argc; (void)argv; // Suppress unused parameter warnings
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("SigViz", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    const char* font_path = "assets/JetBrainsMonoNLNerdFont-Regular.ttf";
    font_size_20 = TTF_OpenFont(font_path, 20);
    font_size_18 = TTF_OpenFont(font_path, 18);
    if (font_size_20 == NULL || font_size_18 == NULL) {
        printf("CRITICAL ERROR: Failed to load font at '%s'. TTF_Error: %s\n", font_path, TTF_GetError());
        return 1;
    }

    status_line1 = create_text_object(renderer, font_size_18, (SDL_Color){255, 255, 255, 255});
    status_line2 = create_text_object(renderer, font_size_18, (SDL_Color){255, 255, 255, 255});
    help_prompt_text = create_text_object(renderer, font_size_18, (SDL_Color){180, 180, 180, 255});
    mode_indicator_text = create_text_object(renderer, font_size_18, (SDL_Color){150, 255, 150, 255});
    input_text_display = create_text_object(renderer, font_size_20, (SDL_Color){200, 200, 20, 255});

    SDL_StartTextInput();

    #ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop, 0, 1);
    #else
    while (!quit) {
        main_loop();
    }
    #endif

    TTF_CloseFont(font_size_20);
    TTF_CloseFont(font_size_18);
    destroy_text_object(&status_line1);
    destroy_text_object(&status_line2);
    destroy_text_object(&mode_indicator_text);
    destroy_text_object(&input_text_display);
    destroy_text_object(&help_prompt_text);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}