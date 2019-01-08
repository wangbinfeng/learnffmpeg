//
//  ffmpegtools.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/4.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef ffmpegtools_hpp
#define ffmpegtools_hpp


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
#include "libavutil/timestamp.h"
#include "jpeglib.h"
};
#include <SDL2/SDL_image.h>


#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/stitching.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/videoio.hpp"

#include "framehandlers.hpp"

//#include "PthreadQueue.hpp"
#include "PacketQueue.h"

typedef struct OutputStream {
    
    AVFormatContext *ifmt_ctx;
    AVStream *st;
    AVCodecContext *enc;
    AVCodecContext *dec;
    AVCodec *codec;
    int stream_index;
    
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    
    AVFrame *frame;
    AVFrame *tmp_frame;
    
    float t, tincr, tincr2;
    int nb_samples;
    
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
        dec = NULL;
        ifmt_ctx = NULL;
        codec = NULL;
        stream_index = -1;
    }
    
} OutputStream;


class FFmpegTools{
public:
    FFmpegTools();
    ~FFmpegTools();
private:
    AVFormatContext *ifmt_ctx;
    AVFormatContext *ofmt_ctx;
    
    AVCodecParserContext *codec_parser_ctx;
    
    typedef struct FilteringContext {
        AVFilterContext *buffersink_ctx;
        AVFilterContext *buffersrc_ctx;
        AVFilterGraph *filter_graph;
    } FilteringContext;
    FilteringContext *filter_ctx;
    
    int video_stream_index;
    int audio_stream_index;
    
    OutputStream *stream_ctx = nullptr;
    OutputStream *video_stream_ctx = nullptr;
    OutputStream *audio_stream_ctx = nullptr;
    
    struct SwsContext *convert_ctx;
    
    FrameQueue frameq;
    PacketQueue* videoq;
    pthread_mutex_t* mutex;
    pthread_t tid;
    
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *video_texture;
    SDL_Rect rect;
    
    int64_t frame_number, first_frame_number;
    uint8_t* h264_header;
    int h264_header_len;
    bool first_frame ;
    
private:
    std::vector<FrameProcessor*> frame_processors;
public:
    void add_frame_processor(FrameProcessor* fp){ frame_processors.push_back(fp);}
    
    AVFormatContext* get_input_fmt_ctx() {return ifmt_ctx;}
    AVCodecContext* get_video_decoder_ctx(); 
    AVCodecContext* get_audio_decoder_ctx();
    struct SwsContext* get_convert_ctx(){return convert_ctx;}
    int get_video_stream_index(){return video_stream_index;}
    int get_audio_stream_index(){return audio_stream_index;}
    
    
    int open_input_file(const char *filename);
    int handle_filters(AVFrame *filt_frame, AVFrame *pFrame,AVPacket* packet,int stream_index);
    int init_filters(void);
    int init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                    AVCodecContext *enc_ctx, const char *filter_spec);
    
    int open_codec(const std::string& video_codec_name ,
                   const std::string& audio_codec_name ,
                   u_int8_t* sps = nullptr, unsigned spsSize = 0,
                   u_int8_t* pps = nullptr, unsigned ppsSize = 0
                   );
public:
    int  read_frames();
    int  decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt);
    int  get_first_video_frame(AVFrame* frame);
    //int  get_one_frame(AVFrame* frame,bool block);
    void save_as_jpeg(AVFrame* frame, int width, int height);
    int  read_jpeg_file (const char * filename,uint8_t **data,int &width, int& height,unsigned short &depth);
    
    bool sdl_init();
    bool sdl_loop_event();
    int  sdl_display_frame(AVFrame *frame);
    
    bool    get_video_frame(AVFrame* picture);
    void    seek_video_frame(int64_t frame_number);
    void    seek_video_frame(double sec);
    
    int64_t get_total_frames(int stream_index) const;
    double  get_duration_sec(int stream_index) const;
    double  get_fps(int stream_index) const;
    int64_t    get_bitrate() const;
    
    double  r2d(AVRational r) const;
    int64_t dts_to_frame_number(int64_t dts,int stream_index);
    double  dts_to_sec(int64_t dts,int stream_index);
    
    AVFrame *alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height);
    AVFrame *alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height, bool alloc);
    AVFrame *alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                            uint64_t channel_layout,
                                            int sample_rate, int nb_samples);
   
    int open_input_device(OutputStream** audio_ost,
                          OutputStream** video_ost,
                          const char* devicename);
    
    int cv_capture_camera_to_file(const char *filename,int w = 1280, int h = 720, int fps = 30);
    
    void add_video_stream_0(AVStream **st, AVFormatContext *oc,
                     AVCodecContext** c,AVCodec **codec,
                     enum AVCodecID codec_id, int fps, int w, int h
                          );
    void add_video_stream(AVFormatContext *oc,OutputStream **video_os,const char* filename,
                          AVCodecID codec_id,
                          int w, int h, double bitrate,
                          double fps, enum AVPixelFormat pixel_format);
    
    void add_video_stream(AVStream **st,AVFormatContext *oc,
                               AVCodecContext** c,
                               AVCodec **codec,
                               const char* filename,
                     AVCodecID codec_id,
                     int w, int h, double bitrate,
                     double fps, enum AVPixelFormat pixel_format);
    
    
    void add_audio_stream(AVFormatContext *oc,
                          OutputStream **audio_os,
                          enum AVCodecID codec_id,
                          int64_t bitrate = 64000,
                          int sample_rate = 44100 );
    
    int write_audio_frame(AVFormatContext *oc,OutputStream* os);
    int write_video_frame(AVFormatContext *oc, AVCodecContext *c,
                          AVFrame* frame,
                          AVRational *time_base,
                          int index);
    AVFrame *get_audio_frame(AVFormatContext *oc,OutputStream *ost);
    AVFrame *get_video_frame_from_cap(cv::Mat& image,AVCodecContext *c,
                                      int64_t &next_pts);
    
    cv::Mat* frame_to_cv_mat(AVFrame* frame);
    AVFrame* cv_mat_to_frame(const cv::Mat &src_mat,
                                          int w, int h,
                                          enum AVPixelFormat org_pix_fmts = AV_PIX_FMT_YUV420P);
    int cv_handle_frame(AVFrame *pFrame);
    
    AVFrame* load_frame_from_image(const char* filename);
    int create_video_from_images(const char *outfilename,const char* infiles,int w=0,int h=0, int fps = 30);
    AVFrame *convert_image_frame(AVCodecContext *c,
                                 struct SwsContext **sws_ctx,
                                 AVFrame* tmp_frame,int64_t& next_pts,int dstw,int dsth);
    
    void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt);
    
    int decode_video_rtp(uint8_t* buf, int buf_len);
    int decode_audio_rtp(uint8_t* buf, int buf_len);
public:
    void detect_qr_code(const char* fname);
};


#endif /* ffmpegtools_hpp */
