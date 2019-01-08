
#ifndef VIDEO_H
#define VIDEO_H

#include "PthreadQueue.hpp"

extern "C"
{
#include "libswscale/swscale.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavutil/dict.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavcodec/avcodec.h"
};

struct VideoState
{
	PthreadPacketQueue* videoq;

	int stream_index;           // index of video stream
	AVCodecContext *video_ctx;  // have already be opened by avcodec_open2
	AVStream *stream;           // video stream

	PthreadFrameQueue frameq;
	AVFrame *frame;
	AVFrame *displayFrame;

	double frame_timer;         // Sync fields
	double frame_last_pts;
	double frame_last_delay;
	double video_clock;

    bool quit;
    bool eof;
    bool paused;
    
	void start();

	double synchronize(AVFrame *srcFrame, double pts);
    AVFormatContext *ifmt_ctx;
    
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    
    int init_filters(const char *filters_descr);
    int handle_filters(AVFrame *pFrame,AVFrame *filt_frame) ;
    int sdl_display_frame();
    
    pthread_t tid;
    //SDL_Thread *thread;
    
	VideoState();
	~VideoState();
};

#endif
