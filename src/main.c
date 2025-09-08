#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "shared.h"
#include "text_renderer.h"
#include "time_domain.h"
#include "iq_plot.h"
#include "fft.h"

#ifndef __EMSCRIPTEN__
#include "tinyfiledialogs.h"
#endif

#define INPUT_BUFFER_SIZE 256
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Global Variable Definitions ---
// The actual variables are defined here, once. The 'extern' keyword in
// shared.h makes them accessible to other files.
int SCREEN_WIDTH = 1240;
int SCREEN_HEIGHT = 720;
double amplitude = 100.0;
double frequency = 300.0;
int pixelsPerBit = 50;
double snr_db = 100.0;
double rolloff_factor = 0.35;
int bitsPerSymbol = 1;
double time_offset = 0.0;
double sampling_rate = 4000.0;
double pixels_per_second = 500.0;

AppMode current_mode = MODE_TYPING;
ModulationType current_mod_type = MOD_ASK;
ViewMode current_view = VIEW_TIME_DOMAIN;
bool needsTextUpdate = true;
bool needsAngleUpdate = true;
bool showHelpScreen = false;
bool quit = false;

char inputText[INPUT_BUFFER_SIZE] = {0};
int inputTextLength = 0;
char activeMessage[INPUT_BUFFER_SIZE] = {0};
int activeMessageLength = 0;

SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TextObject status_line1, status_line2, mode_indicator_text, input_text_display, help_prompt_text;
TTF_Font* font_size_20 = NULL;
TTF_Font* font_size_18 = NULL;

#ifdef __EMSCRIPTEN__
extern void downloadFile(const void* data, int dataSize, const char* filename);
#endif

// --- Waveform Export Function ---
void export_waveform() {
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
            if (e.key.keysym.mod & KMOD_CTRL) {
                switch (e.key.keysym.sym) {
                    case SDLK_1: current_view = VIEW_TIME_DOMAIN; needsTextUpdate = true; break;
                    case SDLK_2: current_view = VIEW_IQ_PLOT; needsTextUpdate = true; break;
                    case SDLK_3: current_view = VIEW_POWER_SPECTRUM; needsTextUpdate = true; break;
                }
            } else if (current_mode == MODE_COMMAND) {
                switch (e.key.keysym.sym) {
                    case SDLK_h: showHelpScreen = !showHelpScreen; needsTextUpdate = true; break;
                    case SDLK_1: current_mod_type = MOD_ASK; needsTextUpdate = true; break;
                    case SDLK_2: current_mod_type = MOD_FSK; needsTextUpdate = true; break;
                    case SDLK_3: current_mod_type = MOD_PSK; needsTextUpdate = true; break;
                    case SDLK_b:
                        if (e.key.keysym.mod & KMOD_SHIFT) { rolloff_factor += 0.05; } else { rolloff_factor -= 0.05; }
                        if (rolloff_factor > 1.0) rolloff_factor = 1.0;
                        if (rolloff_factor < 0.0) rolloff_factor = 0.0;
                        needsTextUpdate = true; break;
                    case SDLK_n:
                        if (e.key.keysym.mod & KMOD_SHIFT) { snr_db += 1.0; } else { snr_db -= 1.0; }
                        needsTextUpdate = true; break;
                    case SDLK_m:
                        if (e.key.keysym.mod & KMOD_SHIFT) { bitsPerSymbol++; } else { bitsPerSymbol--; }
                        if (bitsPerSymbol < 1) bitsPerSymbol = 1;
                        needsTextUpdate = true; break;
                    case SDLK_p:
                        if (e.key.keysym.mod & KMOD_SHIFT) { pixelsPerBit += 2; } else { pixelsPerBit -= 2; }
                        if (pixelsPerBit < 4) pixelsPerBit = 4;
                        needsTextUpdate = true; break;
                    case SDLK_SPACE: needsAngleUpdate = !needsAngleUpdate; break;
                    case SDLK_j: time_offset -= 0.1; if (time_offset < 0) time_offset = 0; break;
                    case SDLK_l: time_offset += 0.1; break;
                    case SDLK_0:
                        frequency = 300.0; amplitude = 100.0; snr_db = 100.0; pixelsPerBit = 50;
                        rolloff_factor = 0.35; bitsPerSymbol = 1; time_offset = 0.0;
                        needsTextUpdate = true; break;
                    case SDLK_r: time_offset = 0; break;
                    case SDLK_s: export_waveform(); break;
                }
            }
            if (!showHelpScreen) {
                switch (e.key.keysym.sym) {
                    case SDLK_UP: amplitude += 5.0; needsTextUpdate = true; break;
                    case SDLK_DOWN: amplitude -= 5.0; if (amplitude < 0) amplitude = 0; needsTextUpdate = true; break;
                    case SDLK_RIGHT: frequency += 1.0; needsTextUpdate = true; break;
                    case SDLK_LEFT: frequency -= 1.0; if (frequency < 1.0) frequency = 1.0; needsTextUpdate = true; break;
                    case SDLK_RETURN:
                        strcpy(activeMessage, inputText); activeMessageLength = inputTextLength;
                        inputText[0] = '\0'; inputTextLength = 0;
                        time_offset = 0; needsTextUpdate = true; break;
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
        int mod_ord = 1 << bitsPerSymbol;
        char mod_full_str[32];
        if(mod_ord == 2 && current_mod_type == MOD_PSK) sprintf(mod_full_str, "BPSK");
        else if(mod_ord == 4 && current_mod_type == MOD_PSK) sprintf(mod_full_str, "QPSK");
        else sprintf(mod_full_str, "%d-%s", mod_ord, mod_str);
        
        snprintf(buffer_l1, sizeof(buffer_l1), "A:%.0f F:%.0f %s", amplitude, frequency, mod_full_str);
        snprintf(buffer_l2, sizeof(buffer_l2), "px/bit:%d SNR:%.0fdB Roll-off:%.2f, Fs:%.f Hz", pixelsPerBit, snr_db, rolloff_factor, sampling_rate);        
        snprintf(buffer_mode, sizeof(buffer_mode), "Mode: %s", current_mode == MODE_TYPING ? "Typing" : "Command");

        update_text_object(&status_line1, buffer_l1);
        update_text_object(&status_line2, buffer_l2);
        update_text_object(&mode_indicator_text, buffer_mode);
        update_text_object(&input_text_display, inputText);
        update_text_object(&help_prompt_text, "H for help");
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
            "CTRL + 1,2,3     - Switch View (Time Domain, IQ Plot, Power Spectrum)",
            "Arrows    - Adjust Amplitude & Frequency",
            "M/Shift+M - Decrease/Increase PSK Order (BPSK, QPSK...)",
            "N/Shift+N - Decrease/Increase SNR",
            "B/Shift+B - Decrease/Increase Roll-off Factor",
            "P/Shift+P - Decrease/Increase Pixels per Bit",
            "F/Shift+F - Decrease/Increase Sampling Rate (CANNOT CHANGE FOR THE TIME BEING)",
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
        switch (current_view) {
            case VIEW_TIME_DOMAIN:
                draw_time_domain_view(renderer, activeMessage, activeMessageLength, current_mod_type);
                break;
            case VIEW_IQ_PLOT:
                draw_iq_plot(renderer, activeMessage, activeMessageLength, current_mod_type);
                break;
            case VIEW_POWER_SPECTRUM:
                calculate_and_draw_spectrum(renderer, activeMessage, activeMessageLength, current_mod_type);
                break;
        }
        draw_text_object(&status_line1, 10, 10);
        draw_text_object(&status_line2, 10, 10 + status_line1.rect.h);
        draw_text_object(&mode_indicator_text, 10, 10 + status_line1.rect.h + status_line2.rect.h);
        draw_text_object(&input_text_display, 10, SCREEN_HEIGHT - input_text_display.rect.h - 10);
        draw_text_object(&help_prompt_text, SCREEN_WIDTH - help_prompt_text.rect.w - 10, 10);
    }

    SDL_RenderPresent(renderer);

    if (needsAngleUpdate && !showHelpScreen) {
        time_offset += 1.0 / 60.0;
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
    (void)argc; (void)argv;
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("SigViz", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    const char* font_path = "assets/JetBrainsMonoNLNerdFont-Regular.ttf";
    font_size_20 = TTF_OpenFont(font_path, 20);
    font_size_18 = TTF_OpenFont(font_path, 18);
    if (!font_size_20 || !font_size_18) {
        printf("CRITICAL ERROR: Failed to load font: %s\n", TTF_GetError());
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