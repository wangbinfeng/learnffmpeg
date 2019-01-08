

#include <iostream>
#include <string>


#include "Media.h"

#include <setjmp.h>
extern "C"{
    #include <libavutil/time.h>
 #include "jpeglib.h"
}
#include "boost/algorithm/string.hpp"
#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

static const double SYNC_THRESHOLD = 0.01;
static const double NOSYNC_THRESHOLD = 10.0;
const int AUDIO_DISPLAY_H = 50;
const int BTN_DISPLAY_H = 100;

extern "C"{
    static void* decode_thread(void *data);
    //static int decode_thread(void *data);
    static void video_refresh_timer(void *userdata);
    static void schedule_refresh(MediaState *media, int delay);
    static uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque);
    static void video_refresh_timer(void *userdata);
    
    static int media_play(void* data);
    static int media_pause(void* data);
    static int media_stop(void* data);
    static int media_backward(void* data);
    static int media_forward(void* data);
    
};


MediaState::MediaState(const char* input_file)
	:filename(input_file)
{
    init();
}

MediaState::~MediaState()
{
    stop();
}

void MediaState::init(){
    
    pFormatCtx = nullptr;
    filter_desc = nullptr;
    
    audio = nullptr;
    video = nullptr;
    quit = false;
    
    
    window           = nullptr;
    video_texture    = nullptr;
    renderer         = nullptr;
    convert_ctx = nullptr;
    
    audio_display_h = AUDIO_DISPLAY_H;
    control_btns = nullptr;
    mouse_left_down = false;
    paused = false;
    ffmpeg = nullptr;
    
    started = false;
    start_play_btn = nullptr;
}

void MediaState::stop(){
    if(convert_ctx){
        sws_freeContext(convert_ctx);
        convert_ctx = nullptr;
    }
    
    if(control_btns){
        for(auto it=control_btns->cbegin(); it != control_btns->cend(); ++it){
            delete *it;
        }
        delete control_btns;
        control_btns = nullptr;
    }
    
    if(video_texture){
        SDL_DestroyTexture(video_texture);
        video_texture = nullptr;
    }
    
    if (window){
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    if(renderer){
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    
    IMG_Quit();
    
    if(audio){
        delete audio;
        audio = nullptr;
    }
    
    if (video){
        delete video;
        video = nullptr;
    }
    if(ffmpeg)
        delete ffmpeg;
    avformat_network_deinit();
    
}

void MediaState::start(){
    FFmpegTools *ff = new FFmpegTools;
    ff->open_input_file(filename);

    AVFrame *frame = av_frame_alloc();
    ff->get_first_video_frame(frame);
    delete ff;
    
    sdl_init();
    sdl_display_first_frame(frame);
    av_frame_free(&frame);
    sdl_loop_event();
}

void MediaState::play(){
    started = true;
    //sdl_init();
    if(!open_input())
        return;
    
    if(audio){
        printf("audio available ...\n");
        audio->start(); // create audio thread
    }
    if(video){
        printf("video available ...\n");
        video->start(); // create video thread
    }
    pthread_create(&decode_thread_id,NULL,decode_thread,this);
    //thread = SDL_CreateThread(decode_thread, "", this);
    
    schedule_refresh(this, 40);
    //sdl_loop_event();
}

void MediaState::init_control_buttons(int w,int h){
    if (IMG_Init(IMG_INIT_PNG)==-1 || IMG_Init(IMG_INIT_JPG) == -1)
        return;
    
    std::vector<const char*> buttons{
        "/Users/wangbinfeng/dev/opencvdemo/backward-button.png",
        "/Users/wangbinfeng/dev/opencvdemo/play-button.png",
        "/Users/wangbinfeng/dev/opencvdemo/pause-button.png",
        "/Users/wangbinfeng/dev/opencvdemo/stop-button.png",
        "/Users/wangbinfeng/dev/opencvdemo/forward-button.png"
    };
    
    typedef int (*btn_cb_func)(void *data) ;
    
    std::vector<btn_cb_func> btn_cbs{
        media_backward,
        media_play,
        media_pause,
        media_stop,
        media_forward
    };
    control_btns = new std::vector<SDL_Button*>(buttons.size());
    
    int btn_w = 64;
    int nums = static_cast<int>(buttons.size());
    
    int x= (w - nums * btn_w - 10)/2;
    if(x < 0){
        btn_w = 32;
        x = (w - nums * btn_w - 10)/2;
    }
    //int y= h + AUDIO_DISPLAY_H + (BTN_DISPLAY_H - btn_w - 2)/2 + 2;
    int y = h - btn_w -2;
    
    for(int i=0;i<buttons.size();i++){
        SDL_Button* btn = new SDL_Button;
        btn->btn_cb_func = btn_cbs[i];
        btn->btn_file = strdup(buttons[i]);
        
        btn->btn_rect = new SDL_Rect;
        btn->btn_rect->x = x;
        btn->btn_rect->y = y;
        btn->btn_rect->w = btn_w;
        btn->btn_rect->h = btn_w;
        
        x = x + btn_w + 10;
        
        control_btns->push_back(btn);
        
        SDL_Surface *pSurface = IMG_Load(buttons[i]);
        if(pSurface == nullptr){
            printf("cat't load surface %s \n",buttons[i]);
            continue;
        }else
            printf(" load surface %s ,w:%dx%d\n",buttons[i],pSurface->clip_rect.w,pSurface->clip_rect.h);
        
        Uint32 color_key=SDL_MapRGB(pSurface->format,255,255,255);
        SDL_SetColorKey(pSurface,SDL_TRUE,color_key);
       
        btn->btn_texture = SDL_CreateTextureFromSurface(renderer, pSurface);
        
        if (btn->btn_texture == NULL) {
            printf("Error image load: %s\n", SDL_GetError());
        }
        
        SDL_FreeSurface(pSurface);
        pSurface = NULL;
        
    }
    
}


int MediaState::sdl_display_buttons(){

    for(auto it=control_btns->cbegin(); it != control_btns->cend(); ++it){
    
        SDL_Button *btn = *it;
        if(btn == nullptr)
            continue;
        //printf("Rect(%d,%d,%d,%d)\n",x,y,btn_w,btn_w);
        SDL_RenderDrawRect(renderer, btn->btn_rect);
        //printf("check_buttons RECT(%d,%d) %s\n",btn->btn_rect->x,btn->btn_rect->y,btn->btn_file);
        SDL_Texture *btn_texture = btn->btn_texture;
        SDL_RenderCopy(renderer,btn_texture,NULL,btn->btn_rect);
        
    }
    
    return 0;
    
}

bool MediaState::check_start_button(Uint32 x, Uint32 y){
    printf("check start_button ...");
   if(!start_play_btn)
       return false;
    
   if (x >= start_play_btn->btn_rect->x && x<= (start_play_btn->btn_rect->x + 100)
            && y >= start_play_btn->btn_rect->y &&  y <= (start_play_btn->btn_rect->y+100)){
            if(start_play_btn->btn_cb_func)
                start_play_btn->btn_cb_func(this);
            return true;
    }
    
    return false;
}

bool MediaState::check_buttons(Uint32 x, Uint32 y){
    if(control_btns == nullptr)
        return false;
    for(auto it=control_btns->cbegin(); it != control_btns->cend(); ++it){
        SDL_Button *btn = *it;
        if(btn == nullptr)
            continue;
        SDL_Rect *btn_rect = btn->btn_rect;
        if (x >= btn_rect->x && x<= (btn_rect->x + 64)
            && y >= btn_rect->y &&  y <= (btn_rect->y+64)){
            printf("check_buttons, %d,%d, RECT(%d,%d) %s\n",x,y,btn_rect->x,btn_rect->y,btn->btn_file);
            if(btn->btn_cb_func)
                btn->btn_cb_func(this);
            return true;
        }
    }
   
    return false;
}

bool MediaState::sdl_init(){
    
    Uint32 flags = SDL_INIT_TIMER;
    //if(audio)
    flags = flags | SDL_INIT_AUDIO;
    //if(video)
    flags |= SDL_INIT_VIDEO;
    
    if(SDL_Init(flags)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return false;
    }
    printf("SDL is initialized sucessfully !");
    return true;
}
bool MediaState::sdl_loop_event(){
    
    SDL_Event event;
    while (!quit)
    {
        SDL_WaitEvent(&event);
        switch (event.type)
        {
            case FF_QUIT_EVENT:
            case SDL_KEYDOWN:
            case SDL_QUIT:
                quit = true;
                if(video)
                    video->quit = true;
                if(audio)
                    audio->quit = true;
                av_log(NULL,AV_LOG_DEBUG,"quit loop event \n");
                SDL_PauseAudio(1);
                goto end;
                break;
                
            case FF_REFRESH_EVENT:
                video_refresh_timer(this);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if(!started && event.button.button==SDL_BUTTON_LEFT &&
                   check_start_button(event.button.x,event.button.y))
                {
                     mouse_left_down = true;
                }else if (started && event.button.button==SDL_BUTTON_LEFT &&
                        check_buttons(event.button.x,event.button.y))
                    {
                        mouse_left_down = true;
                    }
                break;
            case SDL_MOUSEMOTION:
               // printf("mouse(x=%d,y=%d)\n",event.button.x,event.button.y);
                break;
            case SDL_MOUSEBUTTONUP:
                if (mouse_left_down)
                {
                    mouse_left_down = false;
                    check_buttons(event.button.x,event.button.y);
                }
            default:
                break;
        }
    }
end:
    av_log(NULL,AV_LOG_DEBUG,"end ...");
    SDL_Quit();
    return true;
}

bool MediaState::open_input()
{
    ffmpeg = new FFmpegTools;
    ffmpeg->open_input_file(filename);

    pFormatCtx = ffmpeg->get_input_fmt_ctx();
    int audio_index = ffmpeg->get_audio_stream_index();
    int video_index = ffmpeg->get_video_stream_index();
    
    av_dump_format(pFormatCtx, 0, filename, 0);
    av_log(NULL,AV_LOG_ERROR,"audio(%d)/video(%d) stream index\n",audio_index,video_index);
    
    if (audio_index < 0 || video_index < 0){
        av_log(NULL,AV_LOG_ERROR,"audio(%d)/video(%d) stream index is invalid\n",audio_index,video_index);
    }
    
    if(audio_index >= 0)
    {
        audio = new AudioState();
        audio->stream_index = audio_index;
        audio->stream = pFormatCtx->streams[audio->stream_index];
        audio->audio_ctx = ffmpeg->get_audio_decoder_ctx();
    }
    
    if(video_index >= 0)
    {
        video = new VideoState();
        video->stream_index = video_index;
        video->ifmt_ctx = pFormatCtx;
        
        video->stream = pFormatCtx->streams[video->stream_index];
        video->video_ctx = ffmpeg->get_video_decoder_ctx();
        video->frame_timer = static_cast<double>(av_gettime()) / 1000000.0;
        video->frame_last_delay = 40e-3;
    }
    
    if(video && filter_desc != nullptr)
        video->init_filters(filter_desc);
    
    return true;
}

int MediaState::sdl_display_audio_wave(){
    int rdft_bits;
    int height = audio_display_h;
    int i_start,x, y1, y, ys;
    int64_t time_diff;
    
    AudioState *s = audio;
    
    for (rdft_bits = 1; (1 << rdft_bits) < 2 * height; rdft_bits++)
        ;
    int nb_freq = 1 << (rdft_bits - 1);
    
    int channels = audio->audio_ctx->channels;
    int nb_display_channels = channels;
    
    int n = 2 * channels;
    int delay = s->audio_write_buf_size;
    delay /= n;
    
    /* to be more precise, we take into account the time spent since
     the last buffer computation */
    if (s->audio_callback_time) {
        time_diff = av_gettime_relative() - s->audio_callback_time;
        delay -= (time_diff * s->audio_ctx->sample_rate) / 1000000;
    }
    int data_used = rect.w;
    delay += 2 * data_used;
    if (delay < data_used)
        delay = data_used;
    
    auto compute_mod=[](int a, int b)
    {
        return a < 0 ? a%b + b : a%b;
    };
   
    i_start= x = compute_mod(s->sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
    
    int h = INT_MIN;
    for (int i = 0; i < 1000; i += channels) {
        int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
        int a = s->sample_array[idx];
        int b = s->sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
        int c = s->sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
        int d = s->sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
        int score = a - d;
        if (h < score && (b ^ c) < 0) {
            h = score;
            i_start = idx;
        }
    }
    
    s->last_i_start = i_start;
    
    auto fill_rectangle = [](SDL_Renderer *renderer,int x, int y, int w, int h){
        SDL_Rect rect;
        rect.x = x;
        rect.y = y;
        rect.w = w;
        rect.h = h;
        if (w && h)
            SDL_RenderFillRect(renderer, &rect);
    };
    
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    //printf("audio wave:h:%d\n",h);
    /* total height for one channel */
    h = height / nb_display_channels;
    /* graph height / 2 */
    int h2 = (h * 9) / 20;
    //printf("audio wave:h:%d,h2%d:\n",h,h2);
    for (int ch = 0; ch < nb_display_channels; ch++) {
        int i = i_start + ch;
        y1 = rect.h + ch * h + (h / 2); /* position of center line */
        //printf("audio wave:h:%d,y1:%d\n",h,y1);
        for (x = 0; x < rect.w; x++) {
            y = (s->sample_array[i] * h2) >> 15;
            if (y < 0) {
                y = -y;
                ys = y1 - y;
            } else {
                ys = y1;
            }
            //printf("audio wave,rect(%d,%d,%d,%d)\n",x,ys,1,y);
            fill_rectangle(renderer, 0 + x, ys, 1, y);
            i += channels;
            if (i >= SAMPLE_ARRAY_SIZE)
                i -= SAMPLE_ARRAY_SIZE;
        }
    }
    
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    
    for (int ch = 1; ch < nb_display_channels; ch++) {
        y = rect.h  + ch * h;
        fill_rectangle(renderer,0, y, rect.w, 1);
    }
    
    return 0;
}

int MediaState::sdl_display_image(){
    
    int w=800;
    int h=600;
    
    rect.x = 0;
    rect.y = 0;
    rect.w = w;
    rect.h = h;
    
    if(window == nullptr)
    {
        window = SDL_CreateWindow("Test",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  w, h+audio_display_h+BTN_DISPLAY_H,
                                  SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);
        
        if(!window) {
            printf("SDL: could not set video mode - exiting\n");
            return -1;
        }
    }
    
    if(renderer == nullptr){
        renderer = SDL_CreateRenderer(window, -1, 0);
        av_log(NULL,AV_LOG_DEBUG,"SDL video is initilized sucessfully \n" );
        init_control_buttons(w,h);
    }
    
    if(video_texture == nullptr){
        //SDL_Surface * image = IMG_Load(audio_background_filename);
        int width = 0;
        int height = 0;
        unsigned short depth = 0;
        uint8_t* data = nullptr;
        
        int ret = ffmpeg->read_jpeg_file(audio_background_filename, &data, width, height,depth);
        if(ret < 1){
            fprintf(stderr,"read jpeg failed \n");
            return -1;
        }
        
//        int depth, pitch;
//        Uint32 pixel_format;
//        if (req_format == STBI_rgb) {
//            depth = 24;
//            pitch = 3 * width;
//            pixel_format = SDL_PIXELFORMAT_RGB24;
//        } else {
//            depth = 32;
//            pitch = 4 * width;
//            pixel_format = SDL_PIXELFORMAT_RGBA32;
//        }
        
        int pitch = 3*width;
        
        SDL_Surface* image = SDL_CreateRGBSurfaceWithFormatFrom((void*)data, width, height, depth, pitch,
                                                     SDL_PIXELFORMAT_RGB24);
        
        if (image == NULL) {
            fprintf(stderr,"Creating surface failed: %s\n", SDL_GetError());
            return -1;
        }
        
        video_texture = SDL_CreateTextureFromSurface(renderer, image);
        if(video_texture == nullptr){
            fprintf(stderr,"Creating texture failed: %s\n", SDL_GetError());
            SDL_FreeSurface(image);
            return -1;
        }
        SDL_FreeSurface(image);
    }
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear( renderer );
    SDL_RenderCopy( renderer, video_texture, NULL, &(rect) );
    
    if(audio)
        sdl_display_audio_wave();
    sdl_display_buttons();
    
    SDL_RenderPresent( renderer );
    
    return 0;
}

int MediaState::sdl_display_first_frame(AVFrame* pFrame){
    
    int w = 0;
    int h = 0;
    
    int dstW = w;
    int dstH = h;
    
    if(pFrame){
        w = pFrame->width;
        h = pFrame->height;
        dstW = w;
        dstH = h;
        if(w <1 || h<1)
            return -1;
    }
    
    rect.x = 0;
    rect.y = 0;
    rect.w = w;
    rect.h = h;
    
    if(window == nullptr)
    {
        window = SDL_CreateWindow("Test",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  w, h+audio_display_h+BTN_DISPLAY_H,
                                  SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);
        
        if(!window) {
            printf("SDL: could not set video mode - exiting\n");
            return -1;
        }
    }
    
    if(renderer == nullptr){
        renderer = SDL_CreateRenderer(window, -1, 0);
        av_log(NULL,AV_LOG_DEBUG,"SDL video is initilized sucessfully \n" );
        
    }
    
    if(video_texture == nullptr){
        video_texture = SDL_CreateTexture(renderer,
                                          SDL_PIXELFORMAT_IYUV,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          w,
                                          h);
    }
    
    if(pFrame)
    {
        enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(pFrame->format);
        static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
        
        AVFrame* dst = nullptr;
        if(!dst)
        {
            dst = av_frame_alloc();
            av_image_alloc(dst->data,dst->linesize,dstW,dstH,dst_pix_fmts,1);
        }
        
        if(convert_ctx == nullptr){
            convert_ctx = sws_getContext(w, h,org_pix_fmts,
                                         dstW, dstH,dst_pix_fmts,
                                         SWS_BICUBIC, NULL, NULL, NULL);
        }
        
        if(!convert_ctx) {
            fprintf(stderr, "Cannot initialize the conversion context!\n");
            return -1;
        }
        sws_scale(convert_ctx, pFrame->data,
                  pFrame->linesize,
                  0, h,
                  dst->data, dst->linesize);
        //save_as_jpeg(dst->data[0], pFrame->width,pFrame->height);
        //printf("draw image rect(%d,%d,%d,%d)\n",rect.x,rect.y,rect.w,rect.h);
        SDL_UpdateTexture( video_texture, &(rect), dst->data[0], dst->linesize[0]);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear( renderer );
        SDL_RenderCopy( renderer, video_texture, &(rect), &(rect) );
    }
   
    const char* play_btn = "/Users/wangbinfeng/dev/opencvdemo/play-button.png";
    SDL_Surface *pSurface = IMG_Load(play_btn);
    if(pSurface != nullptr){
        printf("load surface %s \n",play_btn);
        
        Uint32 color_key=SDL_MapRGB(pSurface->format,255,255,255);
        SDL_SetColorKey(pSurface,SDL_TRUE,color_key);
        
        SDL_Texture* btn_texture = SDL_CreateTextureFromSurface(renderer, pSurface);
        
        if (btn_texture == NULL) {
            printf("Error image load: %s\n", SDL_GetError());
        }
        
        SDL_FreeSurface(pSurface);
        pSurface = NULL;
        
        start_play_btn = new SDL_Button;
        start_play_btn->btn_cb_func = media_play;
        start_play_btn->btn_file = strdup(play_btn);
        
        start_play_btn->btn_rect = new SDL_Rect;
        start_play_btn->btn_rect->x = (w - 100) /2;
        start_play_btn->btn_rect->y = (h - 100) /2;
        start_play_btn->btn_rect->w = 100;
        start_play_btn->btn_rect->h = 100;
        
        printf("check_buttons, %d,%d, RECT(%d,%d)\n",start_play_btn->btn_rect->x,
               start_play_btn->btn_rect->y,
               start_play_btn->btn_rect->x+100,
               start_play_btn->btn_rect->y+100);
        
        SDL_RenderCopy(renderer,btn_texture,NULL,start_play_btn->btn_rect);
        
    }else
        printf(" load surface %s ,w:%dx%d\n",play_btn,pSurface->clip_rect.w,pSurface->clip_rect.h);
     
    SDL_RenderPresent( renderer );
    
    return 0;
}


int MediaState::sdl_display_frame(AVFrame* pFrame,AVFrame* displayFrame){
    
    int w = 0;
    int h = 0;
    
    int dstW = w;
    int dstH = h;
    
    if(video && pFrame){
        w = pFrame->width;
        h = pFrame->height;
        dstW = w;
        dstH = h;
        if(w <1 || h<1)
            return -1;
    }
    if(!video && audio){
        w=800;
        h=0;
    }
    
    rect.x = 0;
    rect.y = 0;
    rect.w = w;
    rect.h = h;
   
    if(window == nullptr)
    {
        window = SDL_CreateWindow("Test",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  w, h+audio_display_h+BTN_DISPLAY_H,
                                  SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);
        
        if(!window) {
            printf("SDL: could not set video mode - exiting\n");
            return -1;
        }
    }
    
    if(renderer == nullptr){
        renderer = SDL_CreateRenderer(window, -1, 0);
        av_log(NULL,AV_LOG_DEBUG,"SDL video is initilized sucessfully \n" );
        init_control_buttons(w,h);
    }
    
    if(video && video_texture == nullptr){
        video_texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_IYUV,
                                SDL_TEXTUREACCESS_STREAMING,
                                w,
                                h);
    }
    
    if(video)
    {
        enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(pFrame->format);
        static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
        
        AVFrame* dst = displayFrame;
        if(!dst)
        {
            dst = av_frame_alloc();
            av_image_alloc(dst->data,dst->linesize,dstW,dstH,dst_pix_fmts,1);
        }
        
        if(convert_ctx == nullptr){
            convert_ctx = sws_getContext(w, h,org_pix_fmts,
                                         dstW, dstH,dst_pix_fmts,
                                         SWS_BICUBIC, NULL, NULL, NULL);
        }
        
        if(!convert_ctx) {
            fprintf(stderr, "Cannot initialize the conversion context!\n");
            return -1;
        }
        sws_scale(convert_ctx, pFrame->data,
                  pFrame->linesize,
                  0, h,
                  dst->data, dst->linesize);
        //save_as_jpeg(dst->data[0], pFrame->width,pFrame->height);
        //printf("draw image rect(%d,%d,%d,%d)\n",rect.x,rect.y,rect.w,rect.h);
        SDL_UpdateTexture( video_texture, &(rect), dst->data[0], dst->linesize[0]);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear( renderer );
        SDL_RenderCopy( renderer, video_texture, &(rect), &(rect) );
    }
    if(audio)
        sdl_display_audio_wave();
    sdl_display_buttons();
    SDL_RenderPresent( renderer );
    
    return 0;
}

//int decode_thread(void *data)
void* decode_thread(void *data)
{
	MediaState *media = (MediaState*)data;
	AVPacket *packet = av_packet_alloc();

	while (!media->quit)
	{
		int ret = av_read_frame(media->pFormatCtx, packet);
		if (ret < 0)
		{
            if (ret == AVERROR_EOF){
                printf("decode_thread:no frame is avaiable due to EOF\n");
                if(media->video)
                   media->video->eof = true;
                if(media->audio)
                    media->audio->eof = true;
                if(media->audio && media->audio->audioq.queue.empty()
                   && media->video && media->video->videoq->queue.empty())
                {
                    media->quit = true;
                }
				break;
            }
			if (media->pFormatCtx->pb->error == 0) // No error,wait for user input
			{
				//SDL_Delay(100);
                av_usleep(100*1000);
				continue;
			}
			else
				break;
		}

		if (media->audio != nullptr && packet->stream_index == media->audio->stream_index) // audio stream
		{
            printf("this is audio packat\n");
			media->audio->audioq.enQueue(packet);
		}else if (media->video != nullptr && packet->stream_index == media->video->stream_index) // video stream
		{
             printf("this is video packat\n");
			media->video->videoq->enQueue(packet);
		}		
		av_packet_unref(packet);
	}

	av_packet_free(&packet);
    printf("quit media decode thread \n");
	return 0;
}


uint32_t sdl_refresh_timer_cb(uint32_t interval, void *opaque)
{
    SDL_Event event;
    event.type = FF_REFRESH_EVENT;
    event.user.data1 = opaque;
    SDL_PushEvent(&event);
    return 0; // 0 means stop timer
}

void schedule_refresh(MediaState *media, int delay)
{
    if(media->quit)
        return ;
    
    SDL_AddTimer(delay, sdl_refresh_timer_cb, media);
    
}

void video_refresh_timer(void *userdata)
{
    MediaState *media = (MediaState*)userdata;
    VideoState *video = media->video;
    
    if(media->quit)
        return;
    
    if (video && video->stream_index >= 0)
    {
       
        if (video->paused || (video->videoq->queue.empty() && !media->quit && !video->eof))
            schedule_refresh(media, 1);
        else if(video->videoq->queue.empty() && (media->quit || video->eof))
            return ;
        else
        {
            video->frameq.deQueue(video->frame,true);
            
            if(media->audio != nullptr)
            {
                double current_pts = *(double*)video->frame->opaque;
                double delay = current_pts - video->frame_last_pts;
                if (delay <= 0 || delay >= 1.0)
                    delay = video->frame_last_delay;
                
                video->frame_last_delay = delay;
                video->frame_last_pts = current_pts;
                
                double ref_clock = media->audio->get_audio_clock();
                double diff = current_pts - ref_clock;// diff < 0 => video slow,diff > 0 => video quick
                double threshold = (delay > SYNC_THRESHOLD) ? delay : SYNC_THRESHOLD;
                
                if (fabs(diff) < NOSYNC_THRESHOLD)
                {
                    if (diff <= -threshold)
                        delay = 0;
                    else if (diff >= threshold)
                        delay *= 2;
                }
                
                video->frame_timer += delay;
                double actual_delay = video->frame_timer - static_cast<double>(av_gettime()) / 1000000.0;
                if (actual_delay <= 0.010)
                    actual_delay = 0.010;
                
                schedule_refresh(media, static_cast<int>(actual_delay * 1000 + 0.5));
                
            }else{
                schedule_refresh(media, 100);
            }
            if(video->frame){
                media->sdl_display_frame(video->frame,video->displayFrame);
            }
            
            av_frame_unref(video->frame);
        }
    }
    else
    {
        if(media->audio)
            media->sdl_display_image();
        schedule_refresh(media, 100);
    }
}

int media_backward(void* data){
    MediaState* media = (MediaState*)data;
    printf("press backward button\n");
    return 0;
}

int media_play(void* data){
    MediaState* media = (MediaState*)data;
    printf("press play button\n");
    media->play();
    media->paused = false;
    if(media->audio)
        media->audio->paused = true;
    if(media->video)
        media->video->paused = true;
    SDL_PauseAudio(0);
    return 0;
}
int media_pause(void* data){
    MediaState* media = (MediaState*)data;
    printf("press pause button\n");
    media->paused = true;
    if(media->audio)
    media->audio->paused = true;
    if(media->video)
    media->video->paused = true;
    SDL_PauseAudio(1);
    return 0;
}
int media_stop(void* data){
    MediaState* media = (MediaState*)data;
    printf("press stop button\n");
    media->quit = true;
    if(media->audio)
        media->audio->paused = true;
    if(media->video)
        media->video->paused = true;
    if(media->audio)
    {
        SDL_PauseAudio(1);
        SDL_AudioQuit();
        SDL_CloseAudio();
    }
    
    //int status = -1;
    //printf("wait video thread\n");
    //SDL_WaitThread(media->video->thread, &status);
    //printf("wait video thread, status=%d\n",status);
    //printf("wait media thread\n");
    //SDL_WaitThread(media->thread, &status);
    //printf("wait media thread, status=%d\n",status);
    media->stop();
    SDL_Quit();
    return 0;
}
int media_forward(void* data){
    MediaState* media = (MediaState*)data;
    printf("press forward button\n");
    return 0;
}

