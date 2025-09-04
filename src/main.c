// src/main.c

#define _GNU_SOURCE
#include <stdio.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "text_renderer.h"

#define SCREEN_WIDTH 1240
#define SCREEN_HEIGHT 720
#define INPUT_BUFFER_SIZE 256

typedef enum {
    MODE_TYPING,
    MODE_COMMAND
} AppMode;

int main(void) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window = SDL_CreateWindow("ASK Modulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create Text objects for the status display
    TextObject status_line1 = create_text_object(renderer, "/usr/share/fonts/JetBrainsMono/JetBrainsMonoNerdFont-Regular.ttf", 20, (SDL_Color){255, 255, 255, 255});
    TextObject status_line2 = create_text_object(renderer, "/usr/share/fonts/JetBrainsMono/JetBrainsMonoNerdFont-Regular.ttf", 20, (SDL_Color){255, 255, 255, 255});
    TextObject mode_indicator_text = create_text_object(renderer, "/usr/share/fonts/JetBrainsMono/JetBrainsMonoNerdFont-Regular.ttf", 18, (SDL_Color){150, 255, 150, 255}); // Green
    TextObject input_text_display = create_text_object(renderer, "/usr/share/fonts/JetBrainsMono/JetBrainsMonoNerdFont-Regular.ttf", 20, (SDL_Color){200, 200, 20, 255});

    // Control variables
    double amplitude = 100.0;
    double frequency = 3.0;
    int pixelsPerBit = 50;
    double noise_level = 0.0;
    int message_offset = 0;
    bool needsTextUpdate = true;
    bool needsAngleUpdate = true;

    AppMode current_mode = MODE_TYPING;
    
    // Input and Active Message variables
    char inputText[INPUT_BUFFER_SIZE] = {0};
    int inputTextLength = 0;
    char activeMessage[INPUT_BUFFER_SIZE] = {0};
    int activeMessageLength = 0;
    
    SDL_StartTextInput();

    SDL_Event e;
    int quit = 0;
    while (!quit) {
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) { quit = 1; } 

            // Handle text input only when in Typing Mode
            if (e.type == SDL_TEXTINPUT && current_mode == MODE_TYPING) {
                if (inputTextLength < INPUT_BUFFER_SIZE - 1) { strcat(inputText, e.text.text); inputTextLength++; needsTextUpdate = true; }
            } 
            
            if (e.type == SDL_KEYDOWN) {
                // The ESCAPE key toggles between modes
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (current_mode == MODE_TYPING) {
                        current_mode = MODE_COMMAND;
                        SDL_StopTextInput(); // Disable text input events
                    } else {
                        current_mode = MODE_TYPING;
                        SDL_StartTextInput(); // Re-enable text input events
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
                            message_offset = 0;
                            break;
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

        if (needsTextUpdate) {
            char buffer_l1[128], buffer_l2[128], buffer_mode[128];
            snprintf(buffer_l1, sizeof(buffer_l1), "y = %.1f * sin(%.1f * x)", amplitude, frequency);
            snprintf(buffer_l2, sizeof(buffer_l2), "Baud (px/bit): %d | Noise: %.2f", pixelsPerBit, noise_level);
            
            // Update mode indicator text based on the current mode
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

        int prev_y; // Variable to store the previous y-coordinate

        for (int x = 0; x < SCREEN_WIDTH; x++) {
            double low_amplitude = amplitude * 0.1; 
            double current_amplitude = low_amplitude; 

            if (activeMessageLength > 0) {
                int message_length_in_pixels = activeMessageLength * 8 * pixelsPerBit;
                int lookup_x = (x + message_offset) % message_length_in_pixels;
                int bit_index = lookup_x / pixelsPerBit; 
                int char_index = bit_index / 8;
                if (char_index < activeMessageLength) {
                    int bit_in_char = bit_index % 8;
                    int bit_value = (activeMessage[char_index] >> (7 - bit_in_char)) & 1;
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
            
            // Add noise to the clean 'y' value
            if (current_amplitude > 0 && noise_level > 0) { 
                y += (rand() % 21 - 10) * noise_level; 
            }
            
            // Calculate the final screen coordinate
            int current_y = (SCREEN_HEIGHT / 2) - (int)y;

            // On the first pixel (x=0), we can't draw a line yet, we just store the point.
            // For all other pixels, we draw a line from the previous point to the current one.
            if (x > 0) {
                SDL_RenderDrawLine(renderer, x - 1, prev_y, x, current_y);
            }
            
            // Update prev_y for the next iteration of the loop
            prev_y = current_y;
        }
        
        // Draw three status lines
        draw_text_object(&status_line1, 10, 10);
        draw_text_object(&status_line2, 10, 10 + status_line1.rect.h);
        draw_text_object(&mode_indicator_text, 10, 10 + status_line1.rect.h + status_line2.rect.h);
        draw_text_object(&input_text_display, 10, SCREEN_HEIGHT - input_text_display.rect.h - 10);

        SDL_RenderPresent(renderer); 

        if (needsAngleUpdate) {
            message_offset++; 
        }        
        
        SDL_Delay(10);
    }

    // --- CLEANUP ---
    SDL_StopTextInput(); 
    destroy_text_object(&status_line1);
    destroy_text_object(&status_line2);
    destroy_text_object(&mode_indicator_text);
    destroy_text_object(&input_text_display);
    SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit(); return 0;
}