//
//  ffmpegutil.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/10/18.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef ffmpegutil_hpp
#define ffmpegutil_hpp

#include <string.h>
#include <iostream>
#include <functional>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <vector>
#include <cinttypes>


#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"
#include "opencv2/opencv.hpp"


extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libswresample/swresample.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "jpeglib.h"
#include "ao/ao.h"
#include "SDL2/SDL.h"
};

#include "opencvdemo.hpp"

using namespace cv;
using namespace std;


class FFmpegVideoFrameProcessor{
public:
    virtual void process(AVFrame* pFrame) = 0;
    virtual ~FFmpegVideoFrameProcessor() = 0;
};


class FFmpegAudioFrameProcessor{
public:
    virtual void process(AVPacket* packet) = 0;
    virtual ~FFmpegAudioFrameProcessor() = 0;
};

class LibAOAudioFrameProcessor : public FFmpegAudioFrameProcessor{
public:
    explicit LibAOAudioFrameProcessor(AVCodecContext* aCodecCtx):pAudioCodecCtx(aCodecCtx){
        initAO();
    }
    ~LibAOAudioFrameProcessor(){
        closeAudio();
    }
    void initAO(){
        
        ao_initialize();
        //int driver=ao_default_driver_id();
        int driver = ao_driver_id("macosx");
        cout << "AO driver id:" << driver << endl;
        if(driver < 0)
            return ;
       
        
        adevice=ao_open_live(driver,&sformat,NULL);
        if(adevice == NULL) {
            fprintf(stderr, "Error opening device.\n");
            return ;
        }
    
        
    }
   
    void closeAudio(){
        
        if(adevice)
        {
            ao_close(adevice);
            ao_shutdown();
        }
    }
    
    void process(AVPacket* packet){
        int ret = avcodec_send_packet(pAudioCodecCtx, packet);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            std::cout << "avcodec_send_packet: " << ret << std::endl;
            return ;
        }
        
        ret = avcodec_receive_frame(pAudioCodecCtx, pFrame);
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
            cout << "fail to avcodec_receive_frame, ret = " << ret << endl;
            return ;
        }
        if(adevice)
            ao_play(adevice, (char*)pFrame->extended_data[0],pFrame->linesize[0] );
    }
    
private:
    ao_sample_format sformat;
    ao_device *adevice;;
    AVCodecContext* pAudioCodecCtx;
    AVFrame* pFrame;
};

class SDLAudioProcessor : public FFmpegAudioFrameProcessor{
private:
    AVFrame         wanted_frame;
public:
    SDLAudioProcessor(AVCodecContext* aCodecCtx):audiofifo(nullptr){
        pAudioCodecCtx = aCodecCtx;
        audio_len = 0;
        audio_pos = nullptr;
        out_buffer = nullptr;
        au_convert_ctx = nullptr;
    }
    ~SDLAudioProcessor(){
        if(au_convert_ctx)
            swr_free(&au_convert_ctx);
        SDL_CloseAudio();//Close SDL
        SDL_Quit();
        if(out_buffer)
            av_free(out_buffer);
    }
    
    void process(AVPacket *packet){
        
        
        /*
        AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
        int out_sample_rate = pAudioCodecCtx->sample_rate;
        int out_framesize = 1024;
        int out_nb_samples = out_framesize;
        uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
        int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
        //Out Buffer Size
        int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
        
        uint8_t ** audio_data_buffer = NULL;
        av_samples_alloc_array_and_samples(&audio_data_buffer, NULL, out_channels, pFrame->nb_samples, out_sample_fmt, 1);
        
        int convert_size = swr_convert(au_convert_ctx, audio_data_buffer, pFrame->nb_samples,
                                       (const uint8_t**)pFrame->data, pFrame->nb_samples);
        
        ret = av_audio_fifo_realloc(audiofifo, av_audio_fifo_size(audiofifo) + convert_size);
        if (ret < 0){
            printf("av_audio_fifo_realloc error\n");
            return ;
        }
        if (av_audio_fifo_write(audiofifo, (void **)audio_data_buffer, convert_size) < convert_size){
            printf("av_audio_fifo_write error\n");
            return ;
        }
        while (av_audio_fifo_size(audiofifo) >= out_framesize){
            int frame_size = FFMIN(av_audio_fifo_size(audiofifo), out_framesize);
            cout << "frame_zise:"<<frame_size << endl;
            audio_frame->nb_samples = frame_size;
            audio_frame->channel_layout =out_channel_layout;
            audio_frame->format = out_sample_fmt;
            audio_frame->sample_rate = out_sample_rate;
            
            av_frame_get_buffer(audio_frame, 0);
            if (av_audio_fifo_read(audiofifo, (void **)audio_frame->data, frame_size) < frame_size){
                printf("av_audio_fifo_read error\n");
                return ;
            }
            if (wanted_spec.samples != frame_size){
                SDL_CloseAudio();
                out_nb_samples = frame_size;
                out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);
                
                wanted_spec.samples = out_nb_samples;
                SDL_OpenAudio(&wanted_spec, NULL);
                
            }
            cout << "audio len "<< audio_len  << endl;
            while (audio_len>0)//Wait until finish
                SDL_Delay(1);
            
            audio_len = out_buffer_size;
            audio_pos = *audio_frame->data;
            
        }
        */
    }
    
    
private:
    
    AVCodecContext* pAudioCodecCtx;
    AVAudioFifo* audiofifo;
    struct SwrContext *au_convert_ctx;
    
    int64_t in_channel_layout;
   
    AVSampleFormat out_sample_fmt ;
    AVFrame* audio_frame;
    
    SDL_AudioSpec wanted_spec;
    uint8_t ** audio_data_buffer = NULL;
    Uint32  audio_len;
    Uint8  *audio_pos;
    Uint8  *audio_chunk;
    const int MAX_AUDIO_FRAME_SIZE = 192000;
    uint8_t  *out_buffer;
    int out_buffer_size;
};





class FFmpegUtil{
private:
    AVCodecContext *pVideoCodecCtx;
    AVCodecContext *pAudioCodecCtx;
    AVCodec *pVideoCodec;
    AVCodec *pAudioCodec;
    
    
    std::string filename;
    std::string outputFilename;
    FILE *output;
    AVFormatContext *pOutputFormatCtx;
    AVCodecContext *pOutputVideoCodecCtx;
    AVCodec *pOutputVideoCodec;
    AVCodec *pOutputAudioCodec;
    int oW,oH;
    
    int videoIndex, audioIndex;
    
    vector<FFmpegVideoFrameProcessor*> videoprocessors;
    
    FFmpegVideoFrameProcessor* videoProcessor;
    FFmpegAudioFrameProcessor* audioProcessor;
    CvFrameProcessor *cvframeProcessor;

    
    struct SwrContext *au_convert_ctx;
    struct SwsContext *enc_convert_ctx;
    
    int64_t in_channel_layout;
    AVSampleFormat out_sample_fmt ;
    
    Uint32  audio_len;
    Uint8  *audio_pos;
    
    const int MAX_AUDIO_FRAME_SIZE = 192000;
    uint8_t  *out_buffer;
    int out_buffer_size;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Event sdlEvent;
    SDL_AudioSpec wanted_spec;
   
    bool enableExtractFrame;
    const char* savedPath;
    
    string filter;// = "scale=78:24,transpose=cclock";
    //AVFilterContext *buffersink_ctx;
    //AVFilterContext *buffersrc_ctx;
    //AVFilterContext *filter_yadif_ctx;
    //AVFilterGraph *filter_graph;
    int64_t last_pts = AV_NOPTS_VALUE;
    
    AVQueue<AVPacket> *audio_queue;
    AVQueue<AVPacket> *video_queue;
    
    double audio_clock;
    double video_clock;
    
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    
    typedef struct FilteringContext {
        AVFilterContext *buffersink_ctx;
        AVFilterContext *buffersrc_ctx;
        AVFilterGraph *filter_graph;
    } FilteringContext;
    
    FilteringContext *filter_ctx;
    
    typedef struct StreamContext {
        AVCodecContext *dec_ctx;
        AVCodecContext *enc_ctx;
    } StreamContext;
    
    StreamContext *stream_ctx;
    
    
public:
    explicit FFmpegUtil(const string& fname):ifmt_ctx(nullptr),pVideoCodecCtx(nullptr),pVideoCodec(nullptr),
    pAudioCodecCtx(nullptr),pAudioCodec(nullptr),
    videoIndex(-1),audioIndex(-1),videoProcessor(nullptr),audioProcessor(nullptr),cvframeProcessor(nullptr),
    filter("")
    {
        filename = fname;
        
        outputFilename="";
        output=nullptr;
        pOutputVideoCodecCtx = nullptr;
        pOutputVideoCodec = nullptr;
        pOutputFormatCtx = nullptr;
        pOutputAudioCodec = nullptr;
        
        enableExtractFrame = false;
        savedPath = nullptr;
        sdlRenderer=nullptr;
        sdlTexture=nullptr;
        audio_len = 0;
        audio_pos = nullptr;
       
        audio_queue = nullptr;
        video_queue = nullptr;
        
        //filter_yadif_ctx = nullptr;
        //filter_graph = nullptr;
        //buffersink_ctx = nullptr;
        //buffersrc_ctx = nullptr;
        
        
        filter_ctx = nullptr;
        stream_ctx = nullptr;
        
        ofmt_ctx = nullptr;
        ifmt_ctx = nullptr;
        
        enc_convert_ctx = nullptr;
        filter = "null";
    }
    ~FFmpegUtil(){
        
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_free_context(&stream_ctx[i].dec_ctx);
            if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc_ctx)
            {
                avcodec_free_context(&stream_ctx[i].enc_ctx);
                avcodec_close(stream_ctx[i].enc_ctx);
            }
            if (filter_ctx && filter_ctx[i].filter_graph)
                avfilter_graph_free(&filter_ctx[i].filter_graph);
        }
        if(filter_ctx)
            av_free(filter_ctx);
        if(stream_ctx)
            av_free(stream_ctx);
        if(ifmt_ctx)
            avformat_close_input(&ifmt_ctx);
        
        if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ofmt_ctx->pb);
        if(ofmt_ctx)
            avformat_free_context(ofmt_ctx);
        
        /*
        if(pVideoCodecCtx){
            avcodec_free_context(&pVideoCodecCtx);
            avcodec_close(pVideoCodecCtx);
        }
        if(pAudioCodecCtx){
            avcodec_free_context(&pAudioCodecCtx);
            avcodec_close(pAudioCodecCtx);
        }
        if(ifmt_ctx)
            avformat_close_input(&ifmt_ctx);
        */
        
        if(audioProcessor)
            delete audioProcessor;
        if(videoProcessor)
            delete videoProcessor;
        
        if(cvframeProcessor)
            delete cvframeProcessor;
        
        //if(filter_graph)
        //    avfilter_graph_free(&filter_graph);
        
        if(audio_queue)
            delete audio_queue;
        if(video_queue)
            delete video_queue;
            
    }

public:
    int getDuration(){ return (int)ifmt_ctx->duration / 1000000;}
    long long getBitrate(){ return pVideoCodecCtx->bit_rate;}
    int getWidth(){return pVideoCodecCtx->width;}
    int getHeight() {return pVideoCodecCtx->height;}
    string getCodec() { return pVideoCodec->name;}
    int getAudioIndex(){return audioIndex;}
    int getVideoIndex(){return videoIndex;}
    AVCodecContext* getAudioCtx() { return pAudioCodecCtx;}
    
    Uint32  get_audio_len(){return audio_len;}
    void set_audio_len(Uint32 v) {audio_len = v;}
    
    Uint8  *get_audio_pos(){return audio_pos;}
    void set_audio_pos(Uint8 *v){audio_pos = v;}
    
    
    void setFFmpegVideoFrameProcessor(FFmpegVideoFrameProcessor* ptr){
        videoProcessor = ptr;
        videoprocessors.push_back(ptr);
    }
    void setFFmpegAudioFrameProcessor(FFmpegAudioFrameProcessor* ptr){audioProcessor = ptr;}
    void setCvFrameProcessor(CvFrameProcessor* ptr){
        cvframeProcessor = ptr;
    }
    
    void setOutputFile(const string fname){outputFilename = fname;}
    void setFilter(const std::string& _filter){filter = _filter;}
    void setExtractFrame(bool val){ enableExtractFrame = val;}
    void setSavedPath(const char* path){ savedPath = path;}
    void setOutputWidthHeight(int w,int h) {oW = w;oH = h;}
    
    void run();
    
    
private:
    int init();
    int initAV();
    int decode_audio(AVFrame* audio_frame);
    
    int handle_filters(AVFrame *filt_frame, AVFrame *pFrame,AVPacket* packet,int stream_index);
    //int init_filters(const char *filters_descr);
    int init_filters(void);
    int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                    AVCodecContext *enc_ctx, const char *filter_spec);
    
    void saveFrameAsJPEG(AVFrame* pFrame, int width, int height, int iFrame);
    int sdlDisplayFrame(SwsContext *convert_ctx,AVFrame *pFrame);
    int displayFrame(SwsContext *convert_ctx, AVFrame *pFrame) ;
    void dump();
    
    //static void fill_audio(void *udata,Uint8 *stream,int len);
    int simplest_rgb24_to_bmp(const char *rgb24path,int width,int height,const char *bmppath);
    void extractToJpegFile(AVFrame *filt_frame,  struct SwsContext *img_convert_ctx, AVFrame *pFrameRGB);
    int open_input_file(const char *filename);
    int open_output_file(const char *filename);
    
};

#endif /* ffmpegutil_hpp */

