#include "comment.hpp"
#include "util.hpp"


SDL_Texture *create_text_texture(SDL_Renderer *renderer, const char *text, TTF_Font *font, SDL_Color color, SDL_Rect *rect)
{
    SDL_Surface *surface;
    SDL_Texture *texture;

    surface = TTF_RenderUTF8_Solid(font, text, color);
    texture = SDL_CreateTextureFromSurface(renderer, surface);

    rect->w = surface->w;
    rect->h = surface->h;
    SDL_FreeSurface(surface);

    return texture;
}

Comment::Comment(const std::string &text, SDL_Renderer *renderer, TTF_Font *font) {
    int num_rows = (SCREEN_H - FONT_SIZE) / FONT_SIZE;
    this->rect = { SCREEN_W, rand_range(0, num_rows) * FONT_SIZE, 0, 0 };
    this->color = { 255, 255, 255, 0 };
    this->texture = create_text_texture(renderer, text.c_str(), font, this->color, &this->rect);
}

Comment::~Comment() {
    SDL_DestroyTexture(this->texture);
}

void Comment::render(SDL_Renderer *renderer) {
    if (this->texture) {
        SDL_RenderCopy(renderer, this->texture, NULL, &this->rect);
    }
}

void Comment::update(const int fps) {
    int velocity = -(SCREEN_W + this->rect.w) / (5 * fps);
    this->rect.x += velocity;
}

bool Comment::is_finished() {
    return (this->rect.x + this->rect.w) < 0;
}
