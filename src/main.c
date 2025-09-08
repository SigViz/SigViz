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
#include "export_waveform.h"

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
double spectrum_center_freq = 1000.0; 
double spectrum_span = 1000.0;        

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
                    case SDLK_s: export_waveform(activeMessage, activeMessageLength, current_mod_type); break;
                }
            }
            if (!showHelpScreen) {
                if (current_view == VIEW_POWER_SPECTRUM) {
                    switch (e.key.keysym.sym) {
                        case SDLK_LEFT: // Pan left
                            spectrum_center_freq -= spectrum_span / 10.0;
                            if (spectrum_center_freq < 0) spectrum_center_freq = 0;
                            break;
                        case SDLK_RIGHT: // Pan right
                            spectrum_center_freq += spectrum_span / 10.0;
                            if (spectrum_center_freq > sampling_rate / 2.0) spectrum_center_freq = sampling_rate / 2.0;
                            break;
                        case SDLK_DOWN: // Zoom out
                            spectrum_span *= 1.5;
                            if (spectrum_span > sampling_rate / 2.0) spectrum_span = sampling_rate / 2.0;
                            break;
                        case SDLK_UP: // Zoom in
                            spectrum_span /= 1.5;
                            if (spectrum_span < 10.0) spectrum_span = 10.0;
                            break;
                    }
                }
                // This block is inside if (!showHelpScreen)
                if (current_view == VIEW_TIME_DOMAIN) {
                    switch (e.key.keysym.sym) {
                        case SDLK_UP: amplitude += 5.0; needsTextUpdate = true; break;
                        case SDLK_DOWN: amplitude -= 5.0; if (amplitude < 0) amplitude = 0; needsTextUpdate = true; break;
                        case SDLK_RIGHT: frequency += 1.0; needsTextUpdate = true; break;
                        case SDLK_LEFT: frequency -= 1.0; if (frequency < 1.0) frequency = 1.0; needsTextUpdate = true; break;
                    }
                }
                // This switch handles keys that work in any view
                switch (e.key.keysym.sym) {
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
        snprintf(buffer_mode, sizeof(buffer_mode), "Mode: %s (Press TAB to switch)", current_mode == MODE_TYPING ? "Typing" : "Command");

        update_text_object(&status_line1, buffer_l1);
        update_text_object(&status_line2, buffer_l2);
        update_text_object(&mode_indicator_text, buffer_mode);
        update_text_object(&input_text_display, inputText);
        if (current_mode == MODE_TYPING) {
            update_text_object(&help_prompt_text, "Press TAB then H for help");    
        } else {
            update_text_object(&help_prompt_text, "Press H for help");    
        }
        
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
            "--- CONTROLS (ANY MODE) ---",
            "Enter     - Modulate Typed Message",
            "Backspace - Erase letter",
            "TAB       - Toggle Typing/Command Mode",
            "Arrows    - Adjust Amplitude & Frequency (Time Domain) | Zoom & Move (Power Spectrum)",
            " ",
            "--- CONTROLS (COMMAND MODE) ---",
            " ",
            "H         - Toggle this Help Screen",
            "1,2,3     - Switch Modulation (ASK, FSK, PSK)",
            "CTRL + 1,2,3     - Switch View (Time Domain, IQ Plot, Power Spectrum)",
            "M/Shift+M - Decrease/Increase Modulation Order (BPSK, QPSK...)",
            "N/Shift+N - Decrease/Increase SNR",
            "B/Shift+B - Decrease/Increase Roll-off Factor",
            "P/Shift+P - Decrease/Increase Pixels per Bit",
            "F/Shift+F - Decrease/Increase Sampling Rate (CANNOT CHANGE FOR THE TIME BEING)",
            "J/L       - Scroll Left/Right through Signal",
            "R         - Reset Scroll to Start",
            "Space     - Pause/Resume Scrolling",
            "0 (zero)  - Reset All Waveform Parameters",
            "S         - Save Waveform as .32fl file",
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