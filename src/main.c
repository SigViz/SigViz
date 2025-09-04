// src/main.c

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "text_renderer.h"

int SCREEN_WIDTH = 1240;
int SCREEN_HEIGHT = 720;
#define INPUT_BUFFER_SIZE 256

// AppMode
typedef enum { MODE_TYPING, MODE_COMMAND } AppMode;

// Modulation Type
typedef enum { MOD_ASK, MOD_FSK, MOD_PSK } ModulationType;

// --- Global Variables ---
// All variables needed by the main loop are moved here to be accessible.
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TextObject status_line1, status_line2, mode_indicator_text, input_text_display, help_prompt_text;
TTF_Font* font_size_20 = NULL;
TTF_Font* font_size_18 = NULL;

double amplitude = 100.0;
double frequency = 30.0;
int pixelsPerBit = 50;
double noise_level = 0.0;
int message_offset = 0;
bool needsTextUpdate = true;
bool needsAngleUpdate = true;
bool showHelpScreen = false;

AppMode current_mode = MODE_TYPING;
ModulationType current_mod_type = MOD_ASK;

char inputText[INPUT_BUFFER_SIZE] = {0};
int inputTextLength = 0;
char activeMessage[INPUT_BUFFER_SIZE] = {0};
int activeMessageLength = 0;
bool quit = false;

// --- Main Loop Function ---
// All the repeating logic from your old while loop now lives here.
void main_loop() {
    // --- EVENT HANDLING ---
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_WINDOWEVENT) {
            // Check if the window was resized
            if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                // If so, update our screen dimension variables
                SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);
            }
        }

        if (e.type == SDL_QUIT) { quit = true; } 

        // Handle text input only when in Typing Mode
        if (e.type == SDL_TEXTINPUT && current_mode == MODE_TYPING) {
            if (inputTextLength < INPUT_BUFFER_SIZE - 1) { strcat(inputText, e.text.text); inputTextLength++; needsTextUpdate = true; }
        } 
        
        if (e.type == SDL_KEYDOWN) {
            // The ESCAPE key toggles between modes
            if (e.key.keysym.sym == SDLK_TAB) {
                    if (showHelpScreen == false) {
                        if (current_mode == MODE_TYPING) {
                        current_mode = MODE_COMMAND;
                        SDL_StopTextInput();
                    } else {
                        current_mode = MODE_TYPING;
                        SDL_StartTextInput();
                    }
                    needsTextUpdate = true;
                }
            }

            // Handle commands only when in Command Mode
            if (current_mode == MODE_COMMAND) {
                switch (e.key.keysym.sym) {
                    case SDLK_h:
                        showHelpScreen = !showHelpScreen;
                        needsTextUpdate = true;
                        break;
                    // Keybindings for changing modulation type
                    case SDLK_1: current_mod_type = MOD_ASK; needsTextUpdate = true; break;
                    case SDLK_2: current_mod_type = MOD_FSK; needsTextUpdate = true; break;
                    case SDLK_3: current_mod_type = MOD_PSK; needsTextUpdate = true; break;
                    case SDLK_b:
                        if (e.key.keysym.mod & KMOD_SHIFT) { pixelsPerBit += 2; }
                        else { pixelsPerBit -= 2; if (pixelsPerBit < 4) pixelsPerBit = 4; }
                        needsTextUpdate = true; break;
                    case SDLK_n:
                        if (e.key.keysym.mod & KMOD_SHIFT) { noise_level += 0.05; }
                        else { noise_level -= 0.05; if (noise_level < 0) noise_level = 0; }
                        needsTextUpdate = true; break;
                    case SDLK_SPACE:
                        needsAngleUpdate = !needsAngleUpdate;
                        break;
                    case SDLK_j:
                        if (message_offset < 10) { message_offset = 0; } 
                        else { message_offset -= 10; }
                        break;
                    case SDLK_l:
                        message_offset += 10;
                        break;
                    case SDLK_0:
                        frequency = 30.0; amplitude = 100.0; noise_level = 0.0; pixelsPerBit = 50;
                        needsTextUpdate = true; break;
                    case SDLK_r:
                        message_offset = 0;
                        break;
                }
            }

            // These keys work in either mode
            if (showHelpScreen == false) {
                switch (e.key.keysym.sym) {
                        case SDLK_UP:    amplitude += 5.0; needsTextUpdate = true; break;
                        case SDLK_DOWN:  amplitude -= 5.0; if (amplitude < 0) amplitude = 0; needsTextUpdate = true; break;
                        case SDLK_RIGHT: frequency += 0.1; needsTextUpdate = true; break;
                        case SDLK_LEFT:  frequency -= 0.1; if (frequency < 0.1) frequency = 0.1; needsTextUpdate = true; break;
                        case SDLK_RETURN:
                            strcpy(activeMessage, inputText); activeMessageLength = inputTextLength;
                            inputText[0] = '\0'; inputTextLength = 0;
                            message_offset = 0; needsTextUpdate = true; break;
                        case SDLK_BACKSPACE:
                            if (inputTextLength > 0) { inputText[inputTextLength - 1] = '\0'; inputTextLength--; }
                            needsTextUpdate = true; break;
                }
            }
        }
    }

    // --- UPDATE TEXT ---
    if (needsTextUpdate) {
        char buffer_l1[128], buffer_l2[128], buffer_mode[128];

        // Get a string for the current modulation type
        const char* mod_type_string;
        switch (current_mod_type) {
            case MOD_ASK: mod_type_string = "ASK"; break;
            case MOD_FSK: mod_type_string = "FSK"; break;
            case MOD_PSK: mod_type_string = "PSK"; break;
            default:      mod_type_string = "Unknown"; break;
        }
        snprintf(buffer_l1, sizeof(buffer_l1), " Carrier Signal: y = %.f * sin(%.1f * x), %s Modulation", amplitude, frequency, mod_type_string);
        snprintf(buffer_l2, sizeof(buffer_l2), "Baud (px/bit): %d | Noise: %.2f", pixelsPerBit, noise_level);
        
        if (current_mode == MODE_TYPING) {
            snprintf(buffer_mode, sizeof(buffer_mode), "Mode: Typing (Press TAB for commands)");
        } else {
            snprintf(buffer_mode, sizeof(buffer_mode), "Mode: Command (1:ASK 2:FSK 3:PSK) (Press TAB for typing)");
        }
        
        update_text_object(&status_line1, buffer_l1);
        update_text_object(&status_line2, buffer_l2);
        update_text_object(&mode_indicator_text, buffer_mode);
        update_text_object(&input_text_display, inputText);
        if (current_mode == MODE_COMMAND) {
            update_text_object(&help_prompt_text, "Press H for help");
        } else {
            update_text_object(&help_prompt_text, "Press TAB for commands, then H for help");
        }
        needsTextUpdate = false;
    }

    if (showHelpScreen) {
        // Next, draw a semi-transparent overlay
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200); // Dark, semi-transparent
        SDL_Rect overlayRect = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
        SDL_RenderFillRect(renderer, &overlayRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        // Finally, draw the help text lines
        const char* help_lines[] = {
            "--- CONTROLS (COMMAND MODE) ---",
            " ",
            "ESC       -  Toggle Typing/Command Mode",
            "H         -  Toggle this Help Screen",
            "1, 2, 3   -  Switch Modulation (ASK, FSK, PSK)",
            "Down / Up Arrows -  Decrease / Increase Amplitude",
            "Left / Right Arrows -  Decrease / Increase Frequency",
            "B / Shift+B -  Decrease / Increase Baud Rate",
            "N / Shift+N -  Decrease / Increase Noise",
            "Enter     -  Modulate Typed Message",
            "Space     -  Pause Message Offset",
            "J / K     -  Decrease / Increase Message Offset",
            NULL
        };

        int y_pos = 100; // Starting Y position for the text
        for (int i = 0; help_lines[i] != NULL; i++) {
            update_text_object(&status_line1, help_lines[i]); // Re-use an existing text object
            draw_text_object(&status_line1, (SCREEN_WIDTH - status_line1.rect.w) / 2, y_pos); // Center the text
            y_pos += status_line1.rect.h + 5; // Move down for the next line
        }
    } else {
        // --- DRAWING ---
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2);

        int prev_y;
        double y = 0.0;

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int bit_value = 0;
            if (activeMessageLength > 0) {
                int message_length_in_pixels = activeMessageLength * 8 * pixelsPerBit;
                int lookup_x = (x + message_offset) % message_length_in_pixels;
                int bit_index = lookup_x / pixelsPerBit; 
                int char_index = bit_index / 8;
                if (char_index < activeMessageLength) {
                    int bit_in_char = bit_index % 8;
                    bit_value = (activeMessage[char_index] >> (7 - bit_in_char)) & 1;
                }
            }

            // --- Main modulation logic switch ---
            switch (current_mod_type) {
                case MOD_ASK: {
                    double low_amplitude = amplitude * 0.1; 
                    double current_amplitude = low_amplitude; 
                    if (bit_value == 1) {
                        int x_in_bit = (x + message_offset) % pixelsPerBit;
                        double phase_in_bit = ((double)x_in_bit / (double)pixelsPerBit) * M_PI;
                        double pulse_shape_factor = sin(phase_in_bit);
                        current_amplitude = low_amplitude + (amplitude - low_amplitude) * pulse_shape_factor;
                    }
                    double sin_arg = (((double)x * frequency) * M_PI / 180.0);
                    y = current_amplitude * sin(sin_arg);
                    break;
                }

                case MOD_FSK: {
                    // For FSK, a '0' is a low frequency and a '1' is a high frequency.
                    double freq_mark = frequency; // Frequency for '1'
                    double freq_space = frequency * 2.0; // Frequency for '0'
                    double current_freq = (bit_value == 1) ? freq_mark : freq_space;
                    // Note: Simple FSK has phase discontinuities at bit changes.
                    double sin_arg = (((double)x * current_freq) * M_PI / 180.0);
                    y = amplitude * sin(sin_arg);
                    break;
                }

                case MOD_PSK: {
                    // For BPSK, a '0' is normal phase, a '1' is inverted phase.
                    double phase_shift = (bit_value == 1) ? M_PI : 0.0;
                    double sin_arg = (((double)x * frequency) * M_PI / 180.0) + phase_shift;
                    y = amplitude * sin(sin_arg);
                    break;
                }
            }

            if (amplitude > 0 && noise_level > 0) { y += (rand() % 21 - 10) * noise_level; }
            int current_y = (SCREEN_HEIGHT / 2) - (int)y;
            if (x > 0) { SDL_RenderDrawLine(renderer, x - 1, prev_y, x, current_y); }
            prev_y = current_y;
        }
    
        draw_text_object(&status_line1, 10, 10);
        draw_text_object(&status_line2, 10, 10 + status_line1.rect.h);
        draw_text_object(&mode_indicator_text, 10, 10 + status_line1.rect.h + status_line2.rect.h);
        draw_text_object(&input_text_display, 10, SCREEN_HEIGHT - input_text_display.rect.h - 10);
        draw_text_object(&help_prompt_text, SCREEN_WIDTH - help_prompt_text.rect.w - 10, 10);
    }

    SDL_RenderPresent(renderer); 
    if (needsAngleUpdate && !showHelpScreen) { message_offset++; }
    
    // For native builds, we add a small delay. For web, the browser controls the frame rate.
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
// main() is now only responsible for one-time setup.
int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("SigViz", 
                              SDL_WINDOWPOS_CENTERED, 
                              SDL_WINDOWPOS_CENTERED, 
                              0, 0, 
                              SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_FULLSCREEN_DESKTOP);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_GetRendererOutputSize(renderer, &SCREEN_WIDTH, &SCREEN_HEIGHT);

    // Load Fonts
    const char* font_path = "assets/JetBrainsMonoNLNerdFont-Regular.ttf";
    font_size_20 = TTF_OpenFont(font_path, 20);
    font_size_18 = TTF_OpenFont(font_path, 18);

    if (font_size_20 == NULL || font_size_18 == NULL) {
        printf("CRITICAL ERROR: Failed to load font at '%s'. TTF_Error: %s\n", font_path, TTF_GetError());
        return 1;
    }

    status_line1 = create_text_object(renderer, font_size_20, (SDL_Color){255, 255, 255, 255});
    status_line2 = create_text_object(renderer, font_size_20, (SDL_Color){255, 255, 255, 255});
    help_prompt_text = create_text_object(renderer, font_size_18, (SDL_Color){180, 180, 180, 255});
    mode_indicator_text = create_text_object(renderer, font_size_18, (SDL_Color){150, 255, 150, 255});
    input_text_display = create_text_object(renderer, font_size_20, (SDL_Color){200, 200, 20, 255});
    
    SDL_StartTextInput();

    // This block starts the main loop in the correct way for each platform.
    #ifdef __EMSCRIPTEN__
        emscripten_set_main_loop(main_loop, 0, 1);
    #else
        while (!quit) {
            main_loop();
        }
    #endif

    // --- CLEANUP ---
    // This code will run when the native app closes.
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