
#ifndef MEDIA_H
#define MEDIA_H

#include <string>
#include <vector>

#include "Audio.h"
#include "Video.h"
#include "ffmpegtools.hpp"
#include <SDL2/SDL_image.h>

extern "C" {
#include "libavformat/avformat.h"
}

typedef int (*CB_FUNC)(void *data);
using btn_func = int(*)(void*);

struct SDL_Button{
    const char* btn_file;
    SDL_Texture *btn_texture;
    SDL_Rect* btn_rect;
    //int (*btn_cb_func)(void *data);
    CB_FUNC btn_cb_func;
    //btn_func btn_cb_f;
    
    int w,h;
    
    SDL_Button(){
        btn_texture = nullptr;
        w = 64;
        h = 64;
        btn_rect = nullptr;
        btn_cb_func = nullptr;
    }
    ~SDL_Button(){
        if(btn_texture){
            SDL_DestroyTexture(btn_texture);
            btn_texture = nullptr;
        }
    }
};

class MediaState
{
private:
    
    const char* filename;
    const char* filter_desc;
    const char* audio_background_filename;
  
    
    int audio_display_h;
    
    bool check_buttons(Uint32 x,Uint32 y);
    bool check_start_button(Uint32 x,Uint32 y);
    void init_control_buttons(int w,int h);
    bool mouse_left_down;
    bool open_input();
    void init();
    SDL_Button* start_play_btn;
    
public:
	AudioState *audio;
	VideoState *video;
    SwsContext *convert_ctx;
    AVFormatContext *pFormatCtx;
    bool quit;
    bool paused;
    bool started;

public:
	MediaState(const char *filename);
	~MediaState();
    void start();
    void stop();
    void play();
    void set_filter_desc(const char *_filter_desc){ filter_desc = _filter_desc; }
    void set_audio_background_filename(const char *_audio_bg_filename) {audio_background_filename = _audio_bg_filename;}
    
public:
    int sdl_display_frame(AVFrame* pFrame,AVFrame* displayFrame);
    int sdl_display_first_frame(AVFrame* pFrame);
    int sdl_display_image();
    bool sdl_loop_event();
    bool sdl_init();
    SDL_Thread *thread;
    
private:
    int sdl_display_audio_wave();
    int sdl_display_buttons();
    
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *video_texture;
    SDL_Rect rect;
    std::vector<SDL_Button*> *control_btns;
    
    FFmpegTools *ffmpeg;

private:

    pthread_t decode_thread_id;
    
};


#endif
