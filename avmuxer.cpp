/*
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat API example.
 *
 * Output a media file in any supported libavformat format. The default
 * codecs are used.
 * @example muxing.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <vector>

extern "C"{
#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/timestamp.h"
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
#include "SDL2/SDL.h"
};


#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/stitching.hpp>
#include <opencv2/opencv.hpp>

#include "PthreadQueue.hpp"

#define STREAM_DURATION   10.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

// a wrapper around a single output AVStream
typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    
    AVFrame *frame;
    AVFrame *tmp_frame;
    
    float t, tincr, tincr2;
    
    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
    
    int w, h;
    
    OutputStream(){
        st = NULL;
        enc = NULL;
        frame = NULL;
        sws_ctx = NULL;
        swr_ctx = NULL;
        next_pts = 0;
        samples_count = 0;
    }
    
} OutputStream;

class AVMuxer{
private:
    SDL_Window *window ;
    SDL_Renderer *renderer ;
    SDL_Texture *video_texture;
    
    OutputStream *video_st;
    OutputStream *audio_st;
    
    AVOutputFormat *fmt ;
    AVFormatContext *oc ;
    
    AVCodec *audio_codec;
    AVCodec *video_codec;
    
    std::vector<AVFrame*> image_frames;
    PthreadFrameQueue audio_frames_queue;
    cv::VideoCapture cap;
    
public:
    AVMuxer();
    ~AVMuxer();
    int start();
    
private:
    void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
    int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt);
    void add_stream(OutputStream *ost, AVFormatContext *oc,AVCodec **codec,enum AVCodecID codec_id);
    int add_audio_stream(OutputStream *ost, AVFormatContext *oc,AVCodec **codec,enum AVCodecID codec_id);
    AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout,int sample_rate, int nb_samples);
    void open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    AVFrame *get_audio_frame(OutputStream *ost);
    int write_audio_frame(AVFormatContext *oc, OutputStream *ost);
    AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
    void open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    AVFrame *get_video_frame_from_file(OutputStream *ost);
    int write_video_frame(AVFormatContext *oc, OutputStream *ost);
    void close_stream(AVFormatContext *oc, OutputStream *ost);
    int decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt);
    AVFrame* load_frame_from_image(const char* filename);
    AVFrame* load_frame_from_audio_file(const char* filename);
    int sdl_display_frame(AVFrame *frame);
    int sdl_loop_event();
    cv::Mat rotate(cv::Mat &inputImg, float angle);
    int cv_handle_frame(AVFrame *pFrame);
    AVFrame *get_video_frame_from_cap(OutputStream *ost);
};

AVMuxer::AVMuxer(){
    window = NULL;
    renderer = NULL;
    video_texture = NULL;
    video_st = NULL;
    audio_st = NULL;
    fmt  = NULL;
    oc = NULL ;
    audio_codec = NULL;
    video_codec = NULL;
}

AVMuxer::~AVMuxer(){
    
    /* Close each codec. */
   
    
    /* free the stream */
    avformat_free_context(oc);
    
    
    if(video_texture){
        SDL_DestroyTexture(video_texture);
        video_texture = NULL;
    }
    
    if (window){
        SDL_DestroyWindow(window);
        window = NULL;
    }
    
    if(renderer){
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    
    if(video_st)
        delete video_st;
    if(audio_st)
        delete audio_st;
    
    window = NULL;
    renderer = NULL;
    video_texture = NULL;
    video_st = NULL;
    audio_st = NULL;
}

cv::Mat AVMuxer::rotate(cv::Mat &inputImg, float angle)
{
    using namespace cv;
    Mat tempImg;
    
    float radian = (float) (angle /180.0 * CV_PI);
    
    //填充图像使其符合旋转要求
    int uniSize =(int) ( max(inputImg.cols, inputImg.rows)* 1.414 );
    int dx = (int) (uniSize - inputImg.cols)/2;
    int dy = (int) (uniSize - inputImg.rows)/2;
    
    copyMakeBorder(inputImg, tempImg, dy, dy, dx, dx, BORDER_CONSTANT);
    
    
    //旋轉中心
    Point2f center( (float)(tempImg.cols/2) , (float) (tempImg.rows/2));
    Mat affine_matrix = getRotationMatrix2D( center, angle, 1.0 );
    
    //旋轉
    warpAffine(tempImg, tempImg, affine_matrix, tempImg.size());
    
    
    //旋轉后的圖像大小
    float sinVal = fabs(sin(radian));
    float cosVal = fabs(cos(radian));
    
    
    Size targetSize( (int)(inputImg.cols * cosVal + inputImg.rows * sinVal),
                    (int)(inputImg.cols * sinVal + inputImg.rows * cosVal) );
    
    //剪掉四周边框
    int x = (tempImg.cols - targetSize.width) / 2;
    int y = (tempImg.rows - targetSize.height) / 2;
    
    Rect rect(x, y, targetSize.width, targetSize.height);
    
    return Mat(tempImg, rect);
    
}

void AVMuxer::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

int AVMuxer::write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, AVStream *st, AVPacket *pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;
    
    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */

int AVMuxer::add_audio_stream(OutputStream *ost,
                               AVFormatContext *oc,
                               AVCodec **codec,
                               enum AVCodecID codec_id)
{
    int i;
    int ret;
    
    AVFormatContext *ifmt_ctx = NULL;
    AVCodecContext *c = NULL;
    AVCodec *acodec = NULL;
    const char* filename = "/Users/wangbinfeng/dev/opencvdemo/SampleAudio_0.7mb.mp3";
    
    if (avformat_open_input(&ifmt_ctx, filename, NULL, NULL) != 0) {
        printf("can't open the file. \n");
        return -1;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return -1;
    }
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        
        enum AVMediaType codec_type = ifmt_ctx->streams[i]->codecpar->codec_type;
        
        if (codec_type == AVMEDIA_TYPE_VIDEO){
            av_log(NULL,AV_LOG_INFO,"codec type is VIDEO\n");
        }else if(codec_type == AVMEDIA_TYPE_DATA){
            av_log(NULL,AV_LOG_INFO,"codec type is DATA\n");
        }else if(codec_type == AVMEDIA_TYPE_ATTACHMENT){
            av_log(NULL,AV_LOG_INFO,"codec type is ATTACHMENT\n");
        }else if(codec_type == AVMEDIA_TYPE_NB){
            av_log(NULL,AV_LOG_INFO,"codec type is NB\n");
        }else if(codec_type == AVMEDIA_TYPE_SUBTITLE){
            av_log(NULL,AV_LOG_INFO,"codec type is SUBTITLE\n");
        }else if(codec_type == AVMEDIA_TYPE_UNKNOWN){
            av_log(NULL,AV_LOG_INFO,"codec type is UNKNOWN\n");
        }else if (codec_type == AVMEDIA_TYPE_AUDIO){
        
            AVStream *stream = ifmt_ctx->streams[i];
            //(*codec) = avcodec_find_decoder(stream->codecpar->codec_id);
            acodec = avcodec_find_decoder(stream->codecpar->codec_id);
            if (codec == NULL) {
                av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u of codec_id %d\n", i,
                       stream->codecpar->codec_id);
                return -1;
            }
            
            c = avcodec_alloc_context3(acodec);
            if (!c) {
                av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
                return -1;
            }
        
            av_log(NULL,AV_LOG_INFO,"codec type is AUDIO\n");
            ret = avcodec_parameters_to_context(c,stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                       "for stream #%u\n", i);
                return -1;
            }
            
            ret = avcodec_open2(c, acodec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return -1;
            }
        }
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    
//    ost->st = avformat_new_stream(oc, NULL);
//    if (!ost->st) {
//        fprintf(stderr, "Could not allocate stream\n");
//        exit(1);
//    }
//
//    ost->st->id = oc->nb_streams-1;
//    ost->enc = c;
    
//    switch ((*codec)->type) {
//        case AVMEDIA_TYPE_AUDIO:
//            c->sample_fmt  = (*codec)->sample_fmts ?
//            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
//
//            if(c->bit_rate <= 0 )
//                c->bit_rate    = 64000;
//
//            if(c->sample_rate <= 0 )
//                c->sample_rate = 44100;
//
//            if ((*codec)->supported_samplerates) {
//                c->sample_rate = (*codec)->supported_samplerates[0];
//                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
//                    if ((*codec)->supported_samplerates[i] == 44100)
//                        c->sample_rate = 44100;
//                }
//            }
//            c->channels  = av_get_channel_layout_nb_channels(c->channel_layout);
//            c->channel_layout = AV_CH_LAYOUT_STEREO;
//            if ((*codec)->channel_layouts) {
//                c->channel_layout = (*codec)->channel_layouts[0];
//                for (i = 0; (*codec)->channel_layouts[i]; i++) {
//                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
//                        c->channel_layout = AV_CH_LAYOUT_STEREO;
//                }
//            }
//            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
//            ost->st->time_base = (AVRational){ 1, c->sample_rate };
//            break;
//
//        default:
//            break;
//    }
//
//    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
//        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
//
    
    AVPacket packet = { .data = NULL, .size = 0 };
    enum AVMediaType type;
    unsigned int stream_index;
    int got_frame = 0;
    AVFrame *frame = NULL;
    
    ret = -1;
    
    while (1)
    {
        printf( "read frames ...\n");
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0){
            av_log(NULL,AV_LOG_ERROR,"fail to read frame %d\n",ret);
            break;
        }
        
        frame = av_frame_alloc();
        if (!frame) {
            frame = NULL;
            break;
        }
        
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
        
        //printf("Demuxer gave frame of stream_index %u\n", stream_index);
        if(type == AVMEDIA_TYPE_AUDIO)
        {
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 c->time_base);
            
            ret = decode(c, frame, &got_frame, &packet);
            
            if (ret < 0) {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }
            
            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                printf("enqueu image frame: size %d:%d, format:%d \n",
                       frame->width,frame->height,frame->format);
                audio_frames_queue.enQueue(frame);
            } else {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
            }
        }
        av_packet_unref(&packet);
    }
    
end:
    av_packet_unref(&packet);
    
    if(ifmt_ctx){
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    
    if (ret < 0 && ret != AVERROR_EOF)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    if(ret == AVERROR_EOF){
        printf( "END of FILE : %s\n", av_err2str(ret));
        return 0;
    }else
        return ret;
    
}


void AVMuxer::add_stream(OutputStream *ost, AVFormatContext *oc,
                       AVCodec **codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext *c;
    int i;
    
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }
    
    ost->st = avformat_new_stream(oc, NULL);
    if (!ost->st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams-1;
    c = avcodec_alloc_context3(*codec);
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    ost->enc = c;
    
    switch ((*codec)->type) {
        case AVMEDIA_TYPE_AUDIO:
            c->sample_fmt  = (*codec)->sample_fmts ?
            (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
            c->bit_rate    = 64000;
            c->sample_rate = 44100;
            if ((*codec)->supported_samplerates) {
                c->sample_rate = (*codec)->supported_samplerates[0];
                for (i = 0; (*codec)->supported_samplerates[i]; i++) {
                    if ((*codec)->supported_samplerates[i] == 44100)
                        c->sample_rate = 44100;
                }
            }
            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            c->channel_layout = AV_CH_LAYOUT_STEREO;
            if ((*codec)->channel_layouts) {
                c->channel_layout = (*codec)->channel_layouts[0];
                for (i = 0; (*codec)->channel_layouts[i]; i++) {
                    if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                        c->channel_layout = AV_CH_LAYOUT_STEREO;
                }
            }
            c->channels        = av_get_channel_layout_nb_channels(c->channel_layout);
            ost->st->time_base = (AVRational){ 1, c->sample_rate };
            break;
            
        case AVMEDIA_TYPE_VIDEO:
            c->codec_id = codec_id;
            
            c->bit_rate = 400000;
            /* Resolution must be a multiple of two. */
            c->width    = ost->w;
            c->height   = ost->h;
            /* timebase: This is the fundamental unit of time (in seconds) in terms
             * of which frame timestamps are represented. For fixed-fps content,
             * timebase should be 1/framerate and timestamp increments should be
             * identical to 1. */
            ost->st->time_base = (AVRational){ 1, STREAM_FRAME_RATE };
            c->time_base       = ost->st->time_base;
            
            c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
            //c->pix_fmt       = STREAM_PIX_FMT;
            c->pix_fmt = AV_PIX_FMT_YUV420P;
            
            if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
                /* just for testing, we also add B-frames */
                c->max_b_frames = 2;
            }
            if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO) {
                /* Needed to avoid using macroblocks in which some coeffs overflow.
                 * This does not happen with normal video, it just happens here as
                 * the motion of the chroma plane does not match the luma plane. */
                c->mb_decision = 2;
            }
            break;
            
        default:
            break;
    }
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */


AVFrame* AVMuxer::alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                  uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame *frame = av_frame_alloc();
    int ret;
    
    if (!frame) {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }
    
    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;
    
    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }
    
    return frame;
}

void AVMuxer::open_audio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;
    
    c = ost->enc;
    
    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }
    
    /* init signal generator */
    ost->t     = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
    
    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
        nb_samples = 10000;
    else
        nb_samples = c->frame_size;
    
    ost->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                       c->sample_rate, nb_samples);
    ost->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                       c->sample_rate, nb_samples);
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
    
    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
    }
    
    /* set options */
    av_opt_set_int       (ost->swr_ctx, "in_channel_count",   c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int       (ost->swr_ctx, "out_channel_count",  c->channels,       0);
    av_opt_set_int       (ost->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);
    
    /* initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        exit(1);
    }
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */

AVFrame *AVMuxer::get_audio_frame(OutputStream *ost)
{
    
    AVFrame *frame = ost->tmp_frame;
//
//    bool ret = audio_frames_queue.deQueue(frame,false);
//    if(!ret)
//        return NULL;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, ost->enc->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;

    for (j = 0; j <frame->nb_samples; j++) {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++)
            *q++ = v;
        ost->t     += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

    return frame;
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
int AVMuxer::write_audio_frame(AVFormatContext *oc, OutputStream *ost)
{
    AVCodecContext *c;
    AVPacket pkt = { 0 }; // data and size must be 0;
    AVFrame *frame;
    int ret = -1;
    int got_packet = 0;
    int dst_nb_samples;
    
    av_init_packet(&pkt);
    c = ost->enc;
    static int inx = 0;
    frame = get_audio_frame(ost);
    
    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                        c->sample_rate, c->sample_rate, AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);

        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(ost->frame);
        if (ret < 0)
            exit(1);

        /* convert to destination format */
        ret = swr_convert(ost->swr_ctx,
                          ost->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            exit(1);
        }
        frame = ost->frame;

        frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        ost->samples_count += dst_nb_samples;
    }
    
    fprintf(stdout, "encode audio frame ...\n" );
    
    ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
    
//    ret = avcodec_send_frame(c, frame);
//    if (ret < 0){
//        fprintf(stderr, "Error encoding audio frame due to avcodec_send_frame (%s)\n", av_err2str(ret));
//        got_packet = 0;
//    } else {
//        while (1) {
//            ret = avcodec_receive_packet(c, &pkt);
//            if (ret == AVERROR(EAGAIN))
//                break;
//            if (ret < 0)
//                break;
//            got_packet = 1;
//        }
//        if (ret < 0) {
//            fprintf(stderr, "Error encoding audio frame due to avcodec_receive_packet(%s)\n", av_err2str(ret));
//            got_packet = 0;
//        }
//    }
    
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        if (ret < 0) {
            fprintf(stderr, "Error while writing audio frame: %s\n",
                    av_err2str(ret));
        }
    }
    inx++;
    if(inx > 500)
        return 0;
    return (frame || got_packet) ? 0 : 1;
}

/**************************************************************/
/* video output */

AVFrame* AVMuxer::alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame *picture;
    int ret;
    
    picture = av_frame_alloc();
    if (!picture)
        return NULL;
    
    picture->format = pix_fmt;
    picture->width  = width;
    picture->height = height;
    
    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }
    
    return picture;
}

void AVMuxer::open_video(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    int ret;
    AVCodecContext *c = ost->enc;
    AVDictionary *opt = NULL;
    
    av_dict_copy(&opt, opt_arg, 0);
    
    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    
    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    
    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    //if (c->pix_fmt != AV_PIX_FMT_YUV420P)
    {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame) {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

int AVMuxer::cv_handle_frame(AVFrame *pFrame) {
    
    static int frame_index = 0;
    
    int w = pFrame->width;
    int h = pFrame->height;
    
    if(w < 1 || h < 1)
        return 0;
    
    enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(pFrame->format);
    enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_BGR24;
    
    struct SwsContext *convert_to_mat_ctx = sws_getContext(w, h,org_pix_fmts,
                                                           w, h,dst_pix_fmts,
                                                           SWS_BICUBIC, NULL, NULL, NULL);
    
    if(!convert_to_mat_ctx) {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
        return -1;
    }
    
    AVFrame* temp_frame = av_frame_alloc();
    av_image_alloc(temp_frame->data,temp_frame->linesize,w,h,dst_pix_fmts,1);
    
    sws_scale(convert_to_mat_ctx, pFrame->data,
              pFrame->linesize,
              0, h,
              temp_frame->data, temp_frame->linesize);
    
    cv::Mat src_mat = cv::Mat(h,w,CV_8UC3,temp_frame->data[0]);
    
    static int count = 0;
    static int angle = 30;
    cv::Mat dst_mat = rotate(src_mat,angle);
    if(count >10){
        angle += 30;
        if(angle > 360)
            angle = 30;
        count = 0;
    }else
        count ++;
    cv::resize(dst_mat, src_mat, cv::Size(w,h));
    printf("dst_frame:cols:%d,rows:%d, w:%d, h:%d\n",src_mat.cols,src_mat.rows,w, h);
    
    AVFrame* dst_frame = alloc_picture(dst_pix_fmts,src_mat.rows,src_mat.cols);
    dst_frame->data[0] = src_mat.data;
    dst_frame->linesize[0] = src_mat.step;
    
    dst_pix_fmts = static_cast<AVPixelFormat>(dst_frame->format);
    struct SwsContext *convert_from_mat_ctx = sws_getContext(w, h,dst_pix_fmts,
                                                             w, h,org_pix_fmts,
                                                             SWS_BICUBIC, NULL, NULL, NULL);
    
    if(!convert_from_mat_ctx) {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
        return -1;
    }
    fprintf(stdout,"change back to YUV420p...\n");
    sws_scale(convert_from_mat_ctx,
              dst_frame->data,
              dst_frame->linesize,
              0, h,
              pFrame->data, pFrame->linesize);
    
    av_frame_unref(temp_frame);
    av_freep(&temp_frame[0]);
    av_frame_unref(dst_frame);
    av_freep(&dst_frame[0]);
    
    return 0;
}

AVFrame *AVMuxer::get_video_frame_from_file(OutputStream *ost)
{
    AVCodecContext *c = ost->enc;
    
    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;
    
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);
    
    static int count = 0;
    static int frame_index = 0;
    if(frame_index >=20)
        frame_index = 0;
    
    ost->tmp_frame = image_frames[frame_index];
    
    if(count > 10){
        frame_index++;
        count = 0;
    }
    
    int w = ost->tmp_frame->width;
    int h = ost->tmp_frame->height;
    if (!ost->sws_ctx) {
        enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(ost->tmp_frame->format);
        ost->sws_ctx = sws_getContext(w, h,
                                      org_pix_fmts,
                                      w, h,
                                      AV_PIX_FMT_YUV420P,
                                      SWS_BICUBIC, NULL, NULL, NULL);
        if (!ost->sws_ctx) {
            fprintf(stderr,
                    "Could not initialize the conversion context\n");
            exit(1);
        }
    }
   
    
    sws_scale(ost->sws_ctx,
              (const uint8_t * const *) ost->tmp_frame->data,
              ost->tmp_frame->linesize, 0, h, ost->frame->data,
              ost->frame->linesize);
    
    cv_handle_frame(ost->frame);
    
    sdl_display_frame(ost->frame);
    ost->frame->pts = ost->next_pts++;
    
    return ost->frame;
}

AVFrame *AVMuxer::get_video_frame_from_cap(OutputStream *ost){
    AVCodecContext *c = ost->enc;
    fprintf(stdout, "av_compare_ts ...\n");
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0){
        fprintf(stderr,"compare TS ... \n");
        return NULL;
    }
    cv::Mat cap_frame,image;
    cap >> cap_frame;
    if( cap_frame.empty() ){
        fprintf(stderr, "can't get captured frame from device, captured frame is empty\n");
        exit(1);
    }
    fprintf(stdout,"get video from from capture ...\n");
    cap_frame.copyTo(image);
    
//    fprintf(stdout, "alloc_video_frame, w:%d,h:%d ...\n",c->width,c->height);
//    AVFrame *frame = alloc_picture(c->pix_fmt, c->width, c->height);
//    if (!frame) {
//        fprintf(stderr, "Could not allocate video frame\n");
//        exit(1);
//    }
    AVFrame *frame = ost->frame;
    fprintf(stdout, "check frame writable ...\n");
    int ret = 0;
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if ((ret = av_frame_make_writable(frame)) < 0){
        fprintf(stderr, "Could not make  video frame writable due to %s \n", av_err2str(ret));
        exit(1);
    }
    
    int w = frame->width;
    int h = frame->height;
    
    fprintf(stdout, "alloc_video_frame w:%d,h:%d ...\n",w,h);
    
    enum AVPixelFormat org_pix_fmts = AV_PIX_FMT_BGR24;
    enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
    
    fprintf(stdout, "alloc_video_frame for dst_frame, rows:%d,cols:%d ...\n",image.rows,image.cols);
    
    AVFrame* dst_frame = alloc_picture(dst_pix_fmts,image.rows,image.cols);
    dst_frame->data[0] = image.data;
    dst_frame->linesize[0] = image.step;
    
    struct SwsContext* sws_ctx = NULL;
    if (!sws_ctx) {
        fprintf(stdout, "allocate sws rows:%d,cols:%d ...\n",image.rows,image.cols);
        
        sws_ctx = sws_getContext(w, h,
                                 org_pix_fmts,
                                 w, h,
                                 dst_pix_fmts,
                                 SWS_BICUBIC, NULL, NULL, NULL);
        if (!sws_ctx) {
            fprintf(stderr,
                    "Could not initialize the conversion context\n");
            exit(1);
        }
    }
    fprintf(stdout, "sws_scale rows:%d,cols:%d ...\n",image.rows,image.cols);
    
    
    sws_scale(sws_ctx,
              (const uint8_t * const *) dst_frame->data,
              dst_frame->linesize, 0, h, frame->data,
              frame->linesize);
    
    cv_handle_frame(frame);
   
    
    fprintf(stdout, "free rows:%d,cols:%d ...\n",image.rows,image.cols);
    
    //av_frame_unref(dst_frame);
    av_frame_free(&dst_frame);
    if(sws_ctx)
        sws_freeContext(sws_ctx);
    ost->frame->pts = ost->next_pts++;
    return frame;
}

int AVMuxer::write_video_frame(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };
    
    c = ost->enc;
    
    //frame = get_video_frame(ost);
    
    frame = get_video_frame_from_cap(ost);
    if(frame)
    fprintf(stdout, "ost->frame w:%d,h:%d ...\n",frame->width,frame->height);
    else return 1;
    av_init_packet(&pkt);
    
    /* encode the image */
    fprintf(stdout,"encode video frame ...\n");
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        exit(1);
    }
    
    if (got_packet) {
        fprintf(stdout,"encode video frame,got packet ...\n");
        if(pkt.pts == AV_NOPTS_VALUE)
            fprintf(stdout,"pts is NOPTS\n");
        fprintf(stdout,"encode video frame, write frame ...\n");
        ret = write_frame(oc, &c->time_base, ost->st, &pkt);
    } else {
        ret = 0;
    }
    
    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }
    
    return (frame || got_packet) ? 0 : 1;
}

void AVMuxer::close_stream(AVFormatContext *oc, OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

int AVMuxer::decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt)
{
    int ret;
    
    *got_frame = 0;
    
    if (pkt) {
        ret = avcodec_send_packet(avctx, pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0 && ret != AVERROR_EOF)
            return ret;
    }
    
    ret = avcodec_receive_frame(avctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN))
        return ret;
    if (ret >= 0)
        *got_frame = 1;
    
    return 0;
}

AVFrame* AVMuxer::load_frame_from_image(const char* filename)
{
    AVFrame* frame = NULL;
    
    int ret;
    unsigned int i;
    
    AVFormatContext *ifmt_ctx = NULL;
    AVDictionary *avdic=NULL;
    AVCodecContext *codec_ctx = NULL;
    
    if (avformat_open_input(&ifmt_ctx, filename, NULL, &avdic) != 0) {
        printf("can't open the file. \n");
        return frame;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return frame;
    }
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u of codec_id %d\n", i,
                   stream->codecpar->codec_id);
            return frame;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return frame;
        }
        
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
            ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                       "for stream #%u\n", i);
                return frame;
            }
            /* Reencode video & audio and remux subtitles etc. */
            
            av_log(NULL,AV_LOG_INFO,"codec type is VIDEO\n");
            codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
            
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return frame;
            }
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO){
            av_log(NULL,AV_LOG_INFO,"codec type is AUDIO\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_DATA){
            av_log(NULL,AV_LOG_INFO,"codec type is DATA\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_ATTACHMENT){
            av_log(NULL,AV_LOG_INFO,"codec type is ATTACHMENT\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_NB){
            av_log(NULL,AV_LOG_INFO,"codec type is NB\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE){
            av_log(NULL,AV_LOG_INFO,"codec type is SUBTITLE\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN){
            av_log(NULL,AV_LOG_INFO,"codec type is UNKNOWN\n");
        }
        
        if(codec_ctx->codec_type != AVMEDIA_TYPE_VIDEO && codec_ctx){
            avcodec_free_context(&codec_ctx);
            avcodec_close(codec_ctx);
        }
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    
    AVPacket packet = { .data = NULL, .size = 0 };
    enum AVMediaType type;
    unsigned int stream_index;
    int got_frame = 0;
    
    ret = -1;
    
    while (1)
    {
        printf( "read frames ...\n");
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0){
            av_log(NULL,AV_LOG_ERROR,"fail to read frame %d\n",ret);
            break;
        }
        
        frame = av_frame_alloc();
        if (!frame) {
            frame = NULL;
            break;
        }
        
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
        
        //printf("Demuxer gave frame of stream_index %u\n", stream_index);
        if(type == AVMEDIA_TYPE_VIDEO)
        {
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 codec_ctx->time_base);
            
            ret = decode(codec_ctx, frame, &got_frame, &packet);
            
            if (ret < 0) {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }
            
            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                printf("image frame: size %d:%d, format:%d \n",frame->width,frame->height,frame->format);
                
                break;
            } else {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
            }
        }
        av_packet_unref(&packet);
    }
    
end:
    av_packet_unref(&packet);
    
    if(codec_ctx){
        avcodec_free_context(&codec_ctx);
        avcodec_close(codec_ctx);
    }
    
    if(ifmt_ctx){
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    
    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    return frame;
}

AVFrame* AVMuxer::load_frame_from_audio_file(const char* filename)
{
    AVFrame* frame = NULL;
    
    int ret;
    unsigned int i;
    
    AVFormatContext *ifmt_ctx = NULL;
    AVDictionary *avdic=NULL;
    AVCodecContext *codec_ctx = NULL;
    
    if (avformat_open_input(&ifmt_ctx, filename, NULL, &avdic) != 0) {
        printf("can't open the file. \n");
        return frame;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return frame;
    }
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u of codec_id %d\n", i,
                   stream->codecpar->codec_id);
            return frame;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return frame;
        }
        
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
            av_log(NULL,AV_LOG_INFO,"codec type is VIDEO\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO){
            av_log(NULL,AV_LOG_INFO,"codec type is AUDIO\n");
            ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                       "for stream #%u\n", i);
                return frame;
            }
            
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return frame;
            }
            
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_DATA){
            av_log(NULL,AV_LOG_INFO,"codec type is DATA\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_ATTACHMENT){
            av_log(NULL,AV_LOG_INFO,"codec type is ATTACHMENT\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_NB){
            av_log(NULL,AV_LOG_INFO,"codec type is NB\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_SUBTITLE){
            av_log(NULL,AV_LOG_INFO,"codec type is SUBTITLE\n");
        }else if(codec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN){
            av_log(NULL,AV_LOG_INFO,"codec type is UNKNOWN\n");
        }
        
        if(codec_ctx->codec_type != AVMEDIA_TYPE_AUDIO && codec_ctx){
            avcodec_free_context(&codec_ctx);
            avcodec_close(codec_ctx);
        }
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    
    AVPacket packet = { .data = NULL, .size = 0 };
    enum AVMediaType type;
    unsigned int stream_index;
    int got_frame = 0;
    
    ret = -1;
    
    while (1)
    {
        printf( "read frames ...\n");
        if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0){
            av_log(NULL,AV_LOG_ERROR,"fail to read frame %d\n",ret);
            break;
        }
        
        frame = av_frame_alloc();
        if (!frame) {
            frame = NULL;
            break;
        }
        
        stream_index = packet.stream_index;
        type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
        
        //printf("Demuxer gave frame of stream_index %u\n", stream_index);
        if(type == AVMEDIA_TYPE_VIDEO)
        {
            av_packet_rescale_ts(&packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 codec_ctx->time_base);
            
            ret = decode(codec_ctx, frame, &got_frame, &packet);
            
            if (ret < 0) {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
                av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                break;
            }
            
            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                printf("image frame: size %d:%d, format:%d \n",frame->width,frame->height,frame->format);
                audio_frames_queue.enQueue(frame);
                //break;
            } else {
                av_frame_unref(frame);
                av_frame_free(&frame);
                frame = NULL;
            }
        }
        av_packet_unref(&packet);
    }
    
end:
    av_packet_unref(&packet);
    
    if(codec_ctx){
        avcodec_free_context(&codec_ctx);
        avcodec_close(codec_ctx);
    }
    
    if(ifmt_ctx){
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    
    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    return frame;
}

int AVMuxer::sdl_display_frame(AVFrame *frame)
{
    
    if(!frame)
        return -1;
    
    int w=frame->width;
    int h=frame->height;
    
    if(w <=0 || h<=0)
        return -1;
    
    SDL_Rect display_rect,rect;
    
    rect.x = 0;
    rect.y = 0;
    rect.w = w;
    rect.h = h;
    
    
    display_rect.x = 0;
    display_rect.y = 0;
    display_rect.w = (w > 800 ? 800 : w);
    display_rect.h = (h > 600 ? 600 : h);
    display_rect.w = w;
    display_rect.h = h;
    
    if(window == NULL)
    {
        window = SDL_CreateWindow("Test",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  display_rect.w, display_rect.h,
                                  SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);
        
        if(!window) {
            printf("SDL: could not set video mode - exiting\n");
            return -1;
        }
    }
    
    if(renderer == NULL){
        renderer = SDL_CreateRenderer(window, -1, 0);
        av_log(NULL,AV_LOG_DEBUG,"SDL video is initilized sucessfully \n" );
    }
    
    if(video_texture == NULL){
        video_texture = SDL_CreateTexture(renderer,
                                          SDL_PIXELFORMAT_IYUV,
                                          SDL_TEXTUREACCESS_STREAMING,
                                          w,
                                          h);
    }
    
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear( renderer );
    
    {
        enum AVPixelFormat org_pix_fmts = (enum AVPixelFormat)(frame->format);
        static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
        
        AVFrame* dst = av_frame_alloc();
        av_image_alloc(dst->data,dst->linesize,w,h,dst_pix_fmts,1);
        
        struct SwsContext *convert_ctx = NULL;
        if(convert_ctx == NULL){
            convert_ctx = sws_getContext(w, h,org_pix_fmts,
                                         w, h,dst_pix_fmts,
                                         SWS_BICUBIC, NULL, NULL, NULL);
        }
        
        if(!convert_ctx) {
            fprintf(stderr, "Cannot initialize the conversion context!\n");
            return -1;
        }
        sws_scale(convert_ctx, frame->data,
                  frame->linesize,
                  0, h,
                  dst->data, dst->linesize);
        
        SDL_UpdateTexture( video_texture, &(rect), dst->data[0], dst->linesize[0]);
        av_frame_unref(dst);
        av_frame_free(&dst);
    }
    
    SDL_RenderCopy( renderer, video_texture, &(rect), &(display_rect) );
    SDL_RenderPresent( renderer );
    
    return 0;
}

int AVMuxer::sdl_loop_event(){
    
    SDL_Event event;
    while (1)
    {
        SDL_WaitEvent(&event);
        switch (event.type)
        {
            case SDL_KEYDOWN:
            case SDL_QUIT:
            case SDL_MOUSEBUTTONDOWN:
                SDL_Quit();
                goto end;
                break;
            default:
                break;
        }
    }
end:
    printf("end ...\n");
    return 0;
}
/**************************************************************/
/* media file output */

int AVMuxer::start()
{
    video_st = new OutputStream;
    audio_st = new OutputStream;
    
    const char *filename;
    
    int ret;
    int have_video = 0, have_audio = 0;
    int encode_video = 0, encode_audio = 0;
    AVDictionary *opt = NULL;
    int i;
    
    filename = "/Users/wangbinfeng/dev/opencvdemo/muxing_frame.mp4";
    int w = 1280,h=720;
//    
//    for(i = 0;i < 20 ;i++){
//        char fname[128] = { 0 };
//        sprintf(fname, "/Users/wangbinfeng/dev/opencvdemo/frame_%d.jpg", i);
//        AVFrame* img_frame = load_frame_from_image(fname);
//        if(img_frame){
//            image_frames.push_back(img_frame);
//            w = img_frame->width;
//            h = img_frame->height;
//        }
//    }
    const char* audio_file = "";
    //load_frame_from_audio_file("/Users/wangbinfeng/dev/opencvdemo/xiyouji.mp3");
    
    Uint32 flags = SDL_INIT_TIMER;
    flags = flags | SDL_INIT_AUDIO;
    flags |= SDL_INIT_VIDEO;
    
    if(SDL_Init(flags)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    printf("SDL is initialized sucessfully !");
    
    video_st->w = (w > 0 ? w : 320);
    video_st->h = (h > 0 ? h : 288);
    
    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc) {
        printf("Could not deduce output format from file extension: using MPEG.\n");
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;
    
    fmt = oc->oformat;
    
    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE) {
        add_stream(video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }
    if (fmt->audio_codec != AV_CODEC_ID_NONE) {
        //int ret = add_audio_stream(audio_st, oc, &audio_codec, fmt->audio_codec);
        add_stream(audio_st, oc, &audio_codec, fmt->audio_codec);
        have_audio = 1;
        encode_audio = 1;
    }
    
    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(oc, video_codec, video_st, opt);
    
    if (have_audio)
        open_audio(oc, audio_codec, audio_st, opt);
    
    av_dump_format(oc, 0, filename, 1);
    
    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }
    
    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file: %s\n",
                av_err2str(ret));
        return 1;
    }
    
    cap.open(0);

    if( !cap.isOpened() )
    {
        fprintf(stderr, "Could not initialize capturing...\n");
        return 0;
    }else{
        fprintf(stdout, "camera is opened and capturing is initialized capturing...\n");
    }
    
    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(video_st->next_pts, video_st->enc->time_base,
                                            audio_st->next_pts, audio_st->enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, video_st);
        } else {
            encode_audio = !write_audio_frame(oc, audio_st);
        }
    }
    printf("finish to encode ...\n");
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);
    if (have_video)
        close_stream(oc, video_st);
    if (have_audio)
        close_stream(oc, audio_st);
    
    if (!(fmt->flags & AVFMT_NOFILE))
    /* Close the output file. */
        avio_closep(&oc->pb);
    
    //sdl_loop_event();
    //SDL_Quit();
    
    return 0;
}

int main_muxer(int argc, char **argv)
{
    AVMuxer muxer;
    muxer.start();
    
    return 0;
}