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

typedef enum {
    MODE_TYPING,
    MODE_COMMAND
} AppMode;

// --- Global Variables ---
// All variables needed by the main loop are moved here to be accessible.
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TextObject status_line1, status_line2, mode_indicator_text, input_text_display;
TTF_Font* font_size_20 = NULL;
TTF_Font* font_size_18 = NULL;

double amplitude = 100.0;
double frequency = 30.0;
int pixelsPerBit = 50;
double noise_level = 0.0;
int message_offset = 0;
bool needsTextUpdate = true;
bool needsAngleUpdate = true;

AppMode current_mode = MODE_TYPING;
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
        if (e.type == SDL_QUIT) { quit = true; } 

        // Handle text input only when in Typing Mode
        if (e.type == SDL_TEXTINPUT && current_mode == MODE_TYPING) {
            if (inputTextLength < INPUT_BUFFER_SIZE - 1) { strcat(inputText, e.text.text); inputTextLength++; needsTextUpdate = true; }
        } 
        
        if (e.type == SDL_KEYDOWN) {
            // The ESCAPE key toggles between modes
            if (e.key.keysym.sym == SDLK_ESCAPE) {
                if (current_mode == MODE_TYPING) {
                    current_mode = MODE_COMMAND;
                    SDL_StopTextInput();
                } else {
                    current_mode = MODE_TYPING;
                    SDL_StartTextInput();
                }
                needsTextUpdate = true;
            }

            // Handle commands only when in Command Mode
            if (current_mode == MODE_COMMAND) {
                switch (e.key.keysym.sym) {
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
                }
            }

            // These keys work in either mode
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

    // --- UPDATE TEXT ---
    if (needsTextUpdate) {
        char buffer_l1[128], buffer_l2[128], buffer_mode[128];
        snprintf(buffer_l1, sizeof(buffer_l1), "y = %.f * sin(%.1f * x)", amplitude, frequency);
        snprintf(buffer_l2, sizeof(buffer_l2), "Baud (px/bit): %d | Noise: %.2f", pixelsPerBit, noise_level);
        
        if (current_mode == MODE_TYPING) {
            snprintf(buffer_mode, sizeof(buffer_mode), "Mode: Typing (Press ESC for commands)");
        } else {
            snprintf(buffer_mode, sizeof(buffer_mode), "Mode: Command (Press ESC to type)");
        }
        
        update_text_object(&status_line1, buffer_l1);
        update_text_object(&status_line2, buffer_l2);
        update_text_object(&mode_indicator_text, buffer_mode);
        update_text_object(&input_text_display, inputText);
        needsTextUpdate = false;
    }

    // --- DRAWING ---
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); SDL_RenderDrawLine(renderer, 0, SCREEN_HEIGHT / 2, SCREEN_WIDTH, SCREEN_HEIGHT / 2);

    int prev_y;
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        double low_amplitude = amplitude * 0.1; double current_amplitude = low_amplitude; 
        if (activeMessageLength > 0) {
            int message_length_in_pixels = activeMessageLength * 8 * pixelsPerBit;
            int lookup_x = (x + message_offset) % message_length_in_pixels;
            int bit_index = lookup_x / pixelsPerBit; int char_index = bit_index / 8;
            if (char_index < activeMessageLength) {
                int bit_in_char = bit_index % 8; int bit_value = (activeMessage[char_index] >> (7 - bit_in_char)) & 1;
                if (bit_value == 1) {
                    int x_in_bit = lookup_x % pixelsPerBit;
                    double phase_in_bit = ((double)x_in_bit / (double)pixelsPerBit) * M_PI;
                    double pulse_shape_factor = sin(phase_in_bit);
                    current_amplitude = low_amplitude + (amplitude - low_amplitude) * pulse_shape_factor;
                }
            }
        }
        double sin_arg = (((double)x * frequency) * M_PI / 180.0);
        double y = current_amplitude * sin(sin_arg);
        if (current_amplitude > 0 && noise_level > 0) { y += (rand() % 21 - 10) * noise_level; }
        int current_y = (SCREEN_HEIGHT / 2) - (int)y;
        if (x > 0) { SDL_RenderDrawLine(renderer, x - 1, prev_y, x, current_y); }
        prev_y = current_y;
    }
    
    draw_text_object(&status_line1, 10, 10);
    draw_text_object(&status_line2, 10, 10 + status_line1.rect.h);
    draw_text_object(&mode_indicator_text, 10, 10 + status_line1.rect.h + status_line2.rect.h);
    draw_text_object(&input_text_display, 10, SCREEN_HEIGHT - input_text_display.rect.h - 10);

    SDL_RenderPresent(renderer); 
    if (needsAngleUpdate) { message_offset++; }
    
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
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}