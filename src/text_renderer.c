// src/text_renderer.c

#include "text_renderer.h"
#include <stdio.h>

TextObject create_text_object(SDL_Renderer* renderer, TTF_Font* font, SDL_Color color) {
    TextObject obj;
    obj.renderer = renderer;
    obj.font = font; // Store the provided font pointer
    obj.color = color;
    obj.texture = NULL;
    obj.rect = (SDL_Rect){0, 0, 0, 0};
    return obj;
}

void update_text_object(TextObject* text_obj, const char* new_text) {
    if (text_obj->texture) {
        SDL_DestroyTexture(text_obj->texture);
    }
    const char* text_to_render = (new_text && new_text[0] != '\0') ? new_text : " ";
    
    SDL_Surface* surface = TTF_RenderText_Solid(text_obj->font, text_to_render, text_obj->color);
    if (surface) {
        text_obj->texture = SDL_CreateTextureFromSurface(text_obj->renderer, surface);
        text_obj->rect.w = surface->w;
        text_obj->rect.h = surface->h;
        SDL_FreeSurface(surface);
    }
}

void draw_text_object(TextObject* text_obj, int x, int y) {
    if (text_obj->texture) {
        text_obj->rect.x = x;
        text_obj->rect.y = y;
        SDL_RenderCopy(text_obj->renderer, text_obj->texture, NULL, &text_obj->rect);
    }
}

void destroy_text_object(TextObject* text_obj) {
    if (text_obj->texture) {
        SDL_DestroyTexture(text_obj->texture);
    }
    // We no longer close the font here, main.c is responsible for that.
}