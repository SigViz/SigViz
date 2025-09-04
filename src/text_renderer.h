// src/text_renderer.h

#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// A structure to hold everything needed for one piece of text
typedef struct {
    SDL_Texture* texture;
    SDL_Rect     rect;
    TTF_Font* font;
    SDL_Color    color;
    SDL_Renderer* renderer;
} TextObject;

// Function declarations
TextObject create_text_object(SDL_Renderer* renderer, const char* font_path, int font_size, SDL_Color color);
void update_text_object(TextObject* text_obj, const char* new_text);
void draw_text_object(TextObject* text_obj, int x, int y);
void destroy_text_object(TextObject* text_obj);

#endif // TEXT_RENDERER_H