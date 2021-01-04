#include <iostream>
#include <memory>
#include <queue>
#include <vector>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#include <SDL.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>
#include <switch.h>

#include "comment.hpp"
#include "common.hpp"
#include "server.hpp"
#include "util.hpp"

const std::string SEPARATOR = std::string("##SEP##");


int main(int argc, char **argv) {
    if (R_FAILED(appletSetAutoSleepDisabled(true))) {
        std::cerr << "appletSetAutoSleepDisabled(true): failed" << std::endl;
    }

    if (R_FAILED(appletSetMediaPlaybackState(true))) {
        std::cerr << "appletSetMediaPlaybackState(true): failed" << std::endl;
    }

    if (R_SUCCEEDED(socketInitializeDefault())) {
        if (nxlinkStdio() < 0) {
            std::cerr << "nxlinkStdio(): failed" << std::endl;
        }
    }

    srand(time(NULL));

    romfsInit();
    chdir("romfs:/");

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER);
    Mix_Init(MIX_INIT_OGG);
    TTF_Init();

    SDL_InitSubSystem(SDL_INIT_JOYSTICK);
    SDL_JoystickEventState(SDL_ENABLE);
    SDL_JoystickOpen(0);

    SDL_InitSubSystem(SDL_INIT_AUDIO);
    Mix_AllocateChannels(5);
    Mix_OpenAudio(48000, AUDIO_S16, 2, 4096);

    Mix_Chunk *const se_comment_arrived = Mix_LoadWAV("data/comment_arrived.wav");
    Mix_Chunk *const se_comment_empty = Mix_LoadWAV("data/comment_empty.wav");

    /*!
    * "Cica" is lisenced under the SIL Open Font License 1.1
    * by https://github.com/miiton/Cica
    */
    TTF_Font *const font = TTF_OpenFont("data/Cica-Regular.ttf", FONT_SIZE);

    SDL_Window *const window = SDL_CreateWindow("smileswitch", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer *const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    pthread_t thread;
    std::queue<std::string> comment_queue;
    bool should_exit = false;

    const int err = pthread_create(&thread, NULL, serve, &comment_queue);
    if (err == 0) {
        pthread_detach(thread);
    } else {
        errno = err;
        handle_error("pthread_create() faild");
        should_exit = true;
    }

    double fps = 60.0;
    SDL_Event event;
    std::vector<std::unique_ptr<Comment>> comments;
    while (!should_exit && appletMainLoop()) {
        const Uint64 start = SDL_GetPerformanceCounter();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                should_exit = true;

            if (event.type == SDL_JOYBUTTONDOWN) {
                if (event.jbutton.button == JOY_PLUS) {
                    std::cout << "JOY_PLUS" << std::endl;
                    should_exit = true;
                }
            }
        }

        if (!comment_queue.empty()) {
            std::string text = comment_queue.front();
            comment_queue.pop();
            std::cout << text << std::endl;

            const std::string::size_type pos = text.find(SEPARATOR);
            if (pos != std::string::npos) {
                text = text.substr(pos + SEPARATOR.length(), std::string::npos);
            }

            if (!text.empty()) {
                Comment *const comment = new Comment(text, renderer, font);
                comments.push_back(std::unique_ptr<Comment>(comment));
                // std::cout << "comments length: " << comments.size() << std::endl;
            }

            Mix_Chunk *const sound = text.empty() ? se_comment_empty : se_comment_arrived;
            if (sound) {
                Mix_PlayChannel(-1, sound, 0);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
        SDL_RenderClear(renderer);

        for (auto &&comment : comments) {
            comment->update((int)fps);
            comment->render(renderer);
        }
        std::erase_if(comments, [](auto &&c) { return c->is_finished(); });

        SDL_RenderPresent(renderer);

        const Uint64 end = SDL_GetPerformanceCounter();
        const double elapsed = (end - start) / (double)SDL_GetPerformanceFrequency();
        fps = 1.0 / elapsed;
    	// std::cout << "Current FPS: " << fps << std::endl;
    }

    TTF_CloseFont(font);

    Mix_HaltChannel(-1);
    if (se_comment_arrived) {
        Mix_FreeChunk(se_comment_arrived);
    }
    if (se_comment_empty) {
        Mix_FreeChunk(se_comment_empty);
    }
    Mix_CloseAudio();

    TTF_Quit();
    Mix_Quit();
    SDL_Quit();
    romfsExit();

    socketExit();
    return 0;
}
