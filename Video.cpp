
#include "Video.h"

static void* decode(void *arg)
{
    VideoState *video = (VideoState*)arg;
    
    AVFrame *frame = av_frame_alloc();
    AVFrame *filt_frame = av_frame_alloc();
    
    AVPacket packet;
    double pts = 0;
    
    while (!video->quit || !video->paused)
    {
        //printf("decode video pop %d \n",video->videoq->queue.size());
        video->videoq->deQueue(&packet, true);
        
        int ret = avcodec_send_packet(video->video_ctx, &packet);
        if(ret == AVERROR(EAGAIN) )
            printf("Fail to avcodec_send_packet due to EAGIN\n");
        else if(ret == AVERROR_EOF)
            printf("Fail to avcodec_send_packet due to EOF\n");
        else if(ret != 0)
            printf("result of avcodec_send_packet : %d \n",ret);
        
        if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            video->quit = true;
            video->eof = true;
            break;
        }
        
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            continue;
        
        ret = avcodec_receive_frame(video->video_ctx, frame);
        if(ret == AVERROR(EAGAIN) )
            printf("Fail to avcodec_receive_frame due to EAGIN\n");
        else if(ret == AVERROR_EOF)
            printf("Fail to avcodec_receive_frame due to EOF\n");
        else if(ret != 0)
            printf("result of avcodec_receive_frame : %d \n",ret);
        
        if(ret == AVERROR_EOF){
            video->quit = true;
            video->eof = true;
            break;
        }
        
        if (ret < 0 && ret != AVERROR_EOF)
            continue;
        
        
        if(!video->quit && video->handle_filters(frame, filt_frame) == 0 )
            frame = filt_frame;
        
        pts = frame->best_effort_timestamp;
        if(pts == AV_NOPTS_VALUE)
            pts = 0;
        
        pts *= av_q2d(video->stream->time_base);
        pts = video->synchronize(frame, pts);
        frame->opaque = &pts;
        
        if (video->frameq.nb_frames >= PthreadFrameQueue::capacity){
            av_usleep(500 * 2 * 1000);
            //SDL_Delay(500 * 2);
        }
        
        video->frameq.enQueue(frame);
        
        av_frame_unref(frame);
        av_frame_unref(filt_frame);
    }
    
    if(frame)
        av_frame_free(&frame);
    
    printf("quit decode thread ...\n");
    
    return 0;
}

VideoState::VideoState()
{
	video_ctx        = nullptr;
	stream_index     = -1;
	stream           = nullptr;

	frame            = nullptr;
	displayFrame     = nullptr;

	videoq           = new PthreadPacketQueue();

	frame_timer      = 0.0;
	frame_last_delay = 0.0;
	frame_last_pts   = 0.0;
	video_clock      = 0.0;
  
    buffersink_ctx = nullptr;
    buffersrc_ctx = nullptr;
    
    quit = false;
    eof = false;
    paused = false;
    
}

VideoState::~VideoState()
{
    frameq.flush();
    videoq->flush();
	delete videoq;
    
    if(frame)
        av_frame_free(&frame);
    
    if(displayFrame){
        av_free(displayFrame->data[0]);
        av_frame_free(&displayFrame);
    }
    
    if(ifmt_ctx)
        avformat_close_input(&ifmt_ctx);
    
//    if(thread){
//        int status = 0;
//        SDL_WaitThread(thread, &status);
//    }
    pthread_join(tid,NULL);
}

void VideoState::start()
{
    frame = av_frame_alloc();
    //thread = SDL_CreateThread(decode, "", this);
    pthread_create(&tid,NULL,decode,this);
}

double VideoState::synchronize(AVFrame *srcFrame, double pts)
{
	double frame_delay;

	if (pts != 0)
		video_clock = pts;
	else
		pts = video_clock; 

	frame_delay = av_q2d(stream->time_base);
	frame_delay += srcFrame->repeat_pict * (frame_delay * 0.5);

	video_clock += frame_delay;

	return pts;
}

int VideoState::init_filters(const char *filters_descr)
{
  char args[512];
  int ret = 0;

 const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
 const AVFilter *buffersink = avfilter_get_by_name("buffersink");
 AVFilterInOut *outputs = avfilter_inout_alloc();
 AVFilterInOut *inputs  = avfilter_inout_alloc();
 AVRational time_base = ifmt_ctx->streams[stream_index]->time_base;
 enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
 
 AVFilterGraph *filter_graph = avfilter_graph_alloc();
 if (!outputs || !inputs || !filter_graph) {
     ret = AVERROR(ENOMEM);
     goto end;
 }
 
 // buffer video source: the decoded frames from the decoder will be inserted here.
 snprintf(args, sizeof(args),
 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
 video_ctx->width, video_ctx->height,
 video_ctx->pix_fmt,
 time_base.num, time_base.den,
 video_ctx->sample_aspect_ratio.num,
 video_ctx->sample_aspect_ratio.den);
 
 ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
 args, NULL, filter_graph);
 if (ret < 0) {
     av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
     goto end;
 }
 
 //buffer video sink: to terminate the filter chain.
 ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                    NULL, NULL, filter_graph);
 if (ret < 0) {
     av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
     goto end;
 }
 
 ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                           AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
 if (ret < 0) {
     av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
     goto end;
 }
    
 outputs->name       = av_strdup("in");
 outputs->filter_ctx = buffersrc_ctx;
 outputs->pad_idx    = 0;
 outputs->next       = NULL;


 inputs->name       = av_strdup("out");
 inputs->filter_ctx = buffersink_ctx;
 inputs->pad_idx    = 0;
 inputs->next       = NULL;
 

 if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                     &inputs, &outputs, NULL)) < 0)
     goto end;
 
 if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
     goto end;
 
 end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
 
    return ret;
}

int VideoState::handle_filters(AVFrame *pFrame,
                               AVFrame *filt_frame) {
    
    if(!buffersrc_ctx || !buffersink_ctx)
        return -1;
    
    int ret = 0;
    /* push the decoded frame into the filtergraph */
    if (av_buffersrc_add_frame_flags(buffersrc_ctx, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }
    
    /* pull filtered frames from the filtergraph */
    while (1) {
        ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return ret;
        if (ret < 0)
            return ret;
        if(ret == 0)
            break;
    }
    //av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
    return 0;
}
