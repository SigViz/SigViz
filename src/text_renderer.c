// src/text_renderer.c

#include "text_renderer.h"
#include <stdio.h>

TextObject create_text_object(SDL_Renderer* renderer, const char* font_path, int font_size, SDL_Color color) {
    TextObject obj;
    obj.renderer = renderer;
    obj.font = TTF_OpenFont(font_path, font_size);
    if (!obj.font) {
        printf("Failed to load font: %s\n", TTF_GetError());
    }
    obj.color = color;
    obj.texture = NULL;
    obj.rect = (SDL_Rect){0, 0, 0, 0};
    return obj;
}

void update_text_object(TextObject* text_obj, const char* new_text) {
    // Destroy the old texture if it exists
    if (text_obj->texture) {
        SDL_DestroyTexture(text_obj->texture);
    }
    
    // TTF_RenderText_Solid returns NULL for empty strings, so render a space instead
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
    if (text_obj->font) {
        TTF_CloseFont(text_obj->font);
    }
}