#pragma once

#include <string>

#include <SDL.h>
#include <SDL_ttf.h>

#include "common.hpp"

const int FONT_SIZE = 72;


SDL_Texture *create_text_texture(SDL_Renderer *renderer, const char* text, TTF_Font *font, SDL_Color color, SDL_Rect *rect);

class Comment {
    SDL_Texture *texture;
    SDL_Rect rect;
    SDL_Color color;

public:
    Comment(const std::string &text, SDL_Renderer *renderer, TTF_Font *font);
    ~Comment();
    void render(SDL_Renderer *renderer);
    void update(const int fps);
    bool is_finished();
};
