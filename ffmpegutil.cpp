//
//  ffmpegutil.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/10/18.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include "ffmpegutil.hpp"

//Uint32  FFmpegUtil::audio_len;
//Uint8  *FFmpegUtil::audio_pos;

static void fill_audio(void *udata,Uint8 *stream,int len){
    cout << "fill_audio ....." << endl;
    SDL_memset(stream, 0, len);
    
    FFmpegUtil* ffmpeg = (FFmpegUtil*)udata;
    
    Uint32  audio_len = ffmpeg->get_audio_len();
    Uint8  *audio_pos = ffmpeg->get_audio_pos();
    
    cout<<"fill_audio, audio_len:" << audio_len <<endl;
    /*
    if(audio_len==0){
        ffmpeg->decode_audio();
        audio_len = ffmpeg->get_audio_len();
        audio_pos = ffmpeg->get_audio_pos();
    }*/
    if(audio_len==0)
        return;
    len=(len>audio_len?audio_len:len);    /*  Mix  as  much  data  as  possible  */
    
    SDL_MixAudio(stream,audio_pos,len,SDL_MIX_MAXVOLUME);
    audio_pos += len;
    audio_len -= len;
    ffmpeg->set_audio_len(audio_len);
    ffmpeg->set_audio_pos(audio_pos);
}

FFmpegVideoFrameProcessor::~FFmpegVideoFrameProcessor(){}
FFmpegAudioFrameProcessor::~FFmpegAudioFrameProcessor(){}

template<typename T>
AVQueue<T>::~AVQueue(){}


int FFmpegUtil::init(){
    /*
    ifmt_ctx = avformat_alloc_context();
    
    if (avformat_open_input(&ifmt_ctx, filename.c_str(), NULL, NULL) != 0)
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    
    
    if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
        return -1;
    }
        
    
    videoIndex = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
            break;
        }
    }
    if (videoIndex == -1)
    {
        printf("Couldn't find a video stream.\n");
        return -1;
    }
    
    
    audioIndex = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audioIndex = i;
            break;
        }
    }
    if (audioIndex == -1)
    {
        printf("Couldn't find a audio stream.\n");
        return -1;
    }
    
    pAudioCodecCtx = avcodec_alloc_context3(NULL);
    if (pAudioCodecCtx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return -1;
    }
    
    avcodec_parameters_to_context(pAudioCodecCtx, ifmt_ctx->streams[audioIndex]->codecpar);
    
    pAudioCodec = avcodec_find_decoder(pAudioCodecCtx->codec_id);  //指向AVCodec的指针.查找解码器
    if (pAudioCodec == NULL)
    {
        printf("Audio Codec not found.\n");
        return -1;
    }
    
    if (avcodec_open2(pAudioCodecCtx, pAudioCodec, NULL) < 0)
    {
        printf("Could not open audio codec.\n");
        return -1;
    }
    
    
    pVideoCodecCtx = avcodec_alloc_context3(NULL);
    if (pVideoCodecCtx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return -1;
    }
    avcodec_parameters_to_context(pVideoCodecCtx, ifmt_ctx->streams[videoIndex]->codecpar);
    
    pVideoCodec = avcodec_find_decoder(pVideoCodecCtx->codec_id);
    if (pVideoCodec == NULL)
    {
        printf("Video Codec not found.\n");
        return -1;
    }
    
    if (avcodec_open2(pVideoCodecCtx, pVideoCodec, NULL) < 0)
    {
        printf("Could not open video codec.\n");
        return -1;
    }
    */
    
    //audio_queue = new PacketQueue2();
    //video_queue = new PacketQueue2();
    
    //initAV();
    
    
    return 0;
}

int FFmpegUtil::initAV(){
    
    //Out Audio Param
    uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
    //nb_samples: AAC-1024 MP3-1152
    int out_nb_samples=pAudioCodecCtx->frame_size;
    AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=pAudioCodecCtx->sample_rate;//44100;
    int out_channels=av_get_channel_layout_nb_channels(out_channel_layout);
    //Out Buffer Size
    out_buffer_size=av_samples_get_buffer_size(NULL,out_channels ,out_nb_samples,out_sample_fmt, 1);
    
    out_buffer=(uint8_t *)av_malloc(MAX_AUDIO_FRAME_SIZE*2);
    //AVFrame* pFrame=av_frame_alloc();
    
    //Init
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return -1;
    }
    //SDL_AudioSpec
    wanted_spec.freq = out_sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = out_channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = fill_audio;
    wanted_spec.userdata = this;
    
    //typedef  void (FFmpegUtil::*_HANDLER)(void*,Uint8*,int);
    //typedef  void (*cbPtr)(void*,Uint8*,int);
    
    //_HANDLER   sct = &FFmpegUtil::fill_audio;
    //wanted_spec.callback = *(cbPtr*)&sct;
    //using namespace std::placeholders;
    
    
    if (SDL_OpenAudio(&wanted_spec, NULL)<0){
        printf("can't open audio.\n");
        return -1;
    }
    cout << "SDL audio is initilized sucessfully " << endl;
    //FIX:Some Codec's Context Information is missing
    in_channel_layout=av_get_default_channel_layout(pAudioCodecCtx->channels);
    //Swr
    
    au_convert_ctx = swr_alloc();
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,out_channel_layout, out_sample_fmt, out_sample_rate,
                                      in_channel_layout,pAudioCodecCtx->sample_fmt , pAudioCodecCtx->sample_rate,0, NULL);
    swr_init(au_convert_ctx);
    
    //Play
    SDL_PauseAudio(0);
    
    return 0;
}


void FFmpegUtil::extractToJpegFile(AVFrame *filt_frame,  struct SwsContext *img_convert_ctx, AVFrame *pFrameRGB) {
    if(enableExtractFrame){
        static int i;
        cout << "enableExtractFrame:filt_frame: "<<filt_frame->width << "x" << filt_frame->height << endl;
        if(filt_frame->width <=0 )
            return ;
        if(pFrameRGB == nullptr)
        {
            pFrameRGB = av_frame_alloc();
            av_image_alloc(pFrameRGB->data,pFrameRGB->linesize,filt_frame->width,filt_frame->height,AV_PIX_FMT_RGB24,1);
        }
        if(img_convert_ctx == nullptr)
            img_convert_ctx = sws_getContext(filt_frame->width, filt_frame->height, pVideoCodecCtx->pix_fmt,
                                             filt_frame->width, filt_frame->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
        sws_scale(img_convert_ctx, filt_frame->data,
                  filt_frame->linesize,
                  0, filt_frame->height,
                  pFrameRGB->data, pFrameRGB->linesize);
        
        if(++i < 20)
            saveFrameAsJPEG(pFrameRGB, filt_frame->width, filt_frame->height, i);
    }
}

void FFmpegUtil::run(){
    if(filename != "")
        open_input_file(filename.c_str());
    if(outputFilename != "")
        open_output_file(outputFilename.c_str());
    
    initAV();
    
    //int ret = init();
    //if(ret < 0 ) return ;
    
    dump();
    
    if(filter != ""){
        cout << "init flter "<< filter << endl;
        if(init_filters() < 0)
            return ;
    }
    int ret = -1;
    
    AVFrame* filt_frame = nullptr;
    AVFrame* pFrame = nullptr;
    SwsContext* convert_ctx = nullptr;
    
    AVPacket* packet = av_packet_alloc();
    if (!packet)
        return;
    av_init_packet(packet);

    struct SwsContext* img_convert_ctx = nullptr;
    AVFrame *pFrameRGB = nullptr;
    
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    
    int i=0;
    int got_frame;
    
    while (av_read_frame(ifmt_ctx,packet) >= 0)
    {
        unsigned int stream_index = packet->stream_index;
        enum AVMediaType type = ifmt_ctx->streams[packet->stream_index]->codecpar->codec_type;
        AVCodecContext *dec_ctx = stream_ctx[packet->stream_index].dec_ctx;
        
        av_packet_rescale_ts(packet,
                             ifmt_ctx->streams[stream_index]->time_base,
                             stream_ctx[stream_index].dec_ctx->time_base);
        
        
        pFrame = av_frame_alloc();
        if (!pFrame) {
            ret = AVERROR(ENOMEM);
            break;
        }
        
        if(type == AVMEDIA_TYPE_VIDEO){
            ret = avcodec_decode_video2(stream_ctx[stream_index].dec_ctx,
                       pFrame,
                       &got_frame,
                       packet);
        }else{
            ret = avcodec_decode_audio4(stream_ctx[stream_index].dec_ctx,
                                        pFrame,
                                        &got_frame,
                                        packet);
        }
        
        /*
         ret = avcodec_send_packet(dec_ctx, packet);
         if (ret < 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
             printf("avcodec_send_packet:%d",ret );
             break;
         }
         
         ret = avcodec_receive_frame(dec_ctx, pFrame);
         if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
             printf("avcodec_send_packet: %dd",ret );
         }
        
        if (ret < 0) {
            av_frame_free(&pFrame);
            av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
            break;
        }
        
        got_frame =1;*/
        if (got_frame) {
            pFrame->pts = pFrame->best_effort_timestamp;
            
            AVFrame* tempFrame = pFrame;
            if (filter_ctx && filter_ctx[stream_index].filter_graph) {
                if(filt_frame == nullptr)
                    filt_frame = av_frame_alloc();
                ret = handle_filters(filt_frame, pFrame,packet,packet->stream_index);
                tempFrame = filt_frame;
            }
            
            if (packet->stream_index == videoIndex) {
                sdlDisplayFrame(convert_ctx, tempFrame);
                extractToJpegFile(tempFrame, img_convert_ctx, pFrameRGB);
            }else if(packet->stream_index == audioIndex){
                //audioq.enQueue(packet);
                decode_audio(tempFrame);
            }
            
            if(stream_ctx[stream_index].enc_ctx){
                if (packet->stream_index == videoIndex) {
                    printf("Run -> Send video filterd frame %3" PRId64" %d:%d \n", tempFrame->pts,tempFrame->width,tempFrame->height);
                }else{
                    printf("Run -> Send audio filterd frame %3" PRId64" %d:%d \n", tempFrame->pts,tempFrame->width,tempFrame->height);
                    
                }
                //encode_write_frame(tempFrame, stream_index,NULL);
            }
            
            if (filter_ctx && filter_ctx[stream_index].filter_graph) {
                av_frame_unref(filt_frame);
            }
        }
        
        av_frame_free(&pFrame);
        av_packet_unref(packet);
        
        SDL_PollEvent(&sdlEvent);
        switch( sdlEvent.type ) {
            case SDL_QUIT:
            case SDL_KEYDOWN:
            case SDL_MOUSEBUTTONDOWN:
                SDL_Quit();
                goto end;
                break;
            default:
                break;
        }
    }
    
    if(ofmt_ctx)
        av_write_trailer(ofmt_ctx);
    
end:
    if(pFrame)
        av_frame_free(&pFrame);
        
    if(filt_frame)
        av_frame_free(&filt_frame);
        
    if(convert_ctx)
        sws_freeContext(convert_ctx);
        
    if(packet)
        av_packet_free(&packet);
        
    if(pFrameRGB)
        av_frame_free(&pFrameRGB);
        
    if(img_convert_ctx)
        sws_freeContext(img_convert_ctx);
        
    if(output)
        fclose(output);
    
    if(sdlTexture)
        SDL_DestroyTexture(sdlTexture);
    
    cout << "end of run ..." << endl;
}


int FFmpegUtil::decode_audio(AVFrame* audio_frame){
   
    swr_convert(au_convert_ctx,&out_buffer, MAX_AUDIO_FRAME_SIZE,
                   (const uint8_t **)audio_frame->data ,
                    audio_frame->nb_samples);
    printf(" pts:%lld\t packet size:%d\t audio_len:%d\n",audio_frame->pts,audio_frame->pkt_size,audio_len);
    
    while(audio_len>0)
            SDL_Delay(1);
        
    audio_len = out_buffer_size;
    audio_pos = (Uint8 *) out_buffer;

    return 0;
}

int FFmpegUtil::init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
                       AVCodecContext *enc_ctx, const char *filter_spec)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc = NULL;
    const AVFilter *buffersink = NULL;
    AVFilterContext *buffersrc_ctx = NULL;
    AVFilterContext *buffersink_ctx = NULL;
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterGraph *filter_graph = avfilter_graph_alloc();
    
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        buffersrc = avfilter_get_by_name("buffer");
        buffersink = avfilter_get_by_name("buffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                 dec_ctx->time_base.num, dec_ctx->time_base.den,
                 dec_ctx->sample_aspect_ratio.num,
                 dec_ctx->sample_aspect_ratio.den);
        
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
            goto end;
        }
        
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
            goto end;
        }
        
        if(enc_ctx)
        {
            ret = av_opt_set_bin(buffersink_ctx, "pix_fmts",
                             (uint8_t*)&enc_ctx->pix_fmt, sizeof(enc_ctx->pix_fmt),
                             AV_OPT_SEARCH_CHILDREN);
        }else{
            enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
            ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                      AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        }
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
            goto end;
        }
    } else if (dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if(enc_ctx == nullptr)
            goto end;
        
        buffersrc = avfilter_get_by_name("abuffer");
        buffersink = avfilter_get_by_name("abuffersink");
        if (!buffersrc || !buffersink) {
            av_log(NULL, AV_LOG_ERROR, "filtering source or sink element not found\n");
            ret = AVERROR_UNKNOWN;
            goto end;
        }
        
        if (!dec_ctx->channel_layout)
            dec_ctx->channel_layout =
            av_get_default_channel_layout(dec_ctx->channels);
        snprintf(args, sizeof(args),
                 "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%" PRIx64,
                 dec_ctx->time_base.num, dec_ctx->time_base.den, dec_ctx->sample_rate,
                 av_get_sample_fmt_name(dec_ctx->sample_fmt),
                 dec_ctx->channel_layout);
        ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                           args, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
            goto end;
        }
        
        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                           NULL, NULL, filter_graph);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
            goto end;
        }
        
        
        ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
                                 (uint8_t*)&enc_ctx->sample_fmt, sizeof(enc_ctx->sample_fmt),
                                 AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
                goto end;
        }
            
            ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
                                 (uint8_t*)&enc_ctx->channel_layout,
                                 sizeof(enc_ctx->channel_layout), AV_OPT_SEARCH_CHILDREN);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
                goto end;
            }
        
            ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
                                 (uint8_t*)&enc_ctx->sample_rate, sizeof(enc_ctx->sample_rate),
                                 AV_OPT_SEARCH_CHILDREN);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
                goto end;
            }
        
    } else {
        ret = AVERROR_UNKNOWN;
        goto end;
    }
    
    /* Endpoints for the filter graph. */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;
    
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    
    if (!outputs->name || !inputs->name) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    
    if ((ret = avfilter_graph_parse_ptr(filter_graph, filter_spec,
                                        &inputs, &outputs, NULL)) < 0)
        goto end;
    
    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    
    /* Fill FilteringContext */
    fctx->buffersrc_ctx = buffersrc_ctx;
    fctx->buffersink_ctx = buffersink_ctx;
    fctx->filter_graph = filter_graph;
    
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    
    return ret;
}

int FFmpegUtil::init_filters(void)
{
    const char *filter_spec;
    unsigned int i;
    int ret;
    filter_ctx =(FilteringContext*) av_malloc_array(ifmt_ctx->nb_streams, sizeof(*filter_ctx));
    if (!filter_ctx)
        return AVERROR(ENOMEM);
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        filter_ctx[i].buffersrc_ctx  = NULL;
        filter_ctx[i].buffersink_ctx = NULL;
        filter_ctx[i].filter_graph   = NULL;
        if (!(ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO
              || ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO))
            continue;
        
        
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            filter_spec = filter.c_str();//"null"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        
        ret = init_filter(&filter_ctx[i], stream_ctx[i].dec_ctx,
                          stream_ctx[i].enc_ctx, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}


int FFmpegUtil::handle_filters(AVFrame *filt_frame,
                             AVFrame *pFrame,
                             AVPacket* packet,
                             int stream_index) {
    int ret = 0;
    /* push the decoded frame into the filtergraph */
    if (av_buffersrc_add_frame_flags(filter_ctx[stream_index].buffersrc_ctx, pFrame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
        return ret;
    }
        
        /* pull filtered frames from the filtergraph */
    while (1) {
        av_log(NULL, AV_LOG_INFO, "Pulling filtered frame from filters\n");
        ret = av_buffersink_get_frame(filter_ctx[stream_index].buffersink_ctx, filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return ret;
        
        if (ret < 0)
            return ret;
        /*
        if (pFrame->pts != AV_NOPTS_VALUE) {
            if (last_pts != AV_NOPTS_VALUE) {
                    int64_t delay = av_rescale_q(pFrame->pts - last_pts,
                                                 filter_ctx[stream_index].buffersink_ctx->inputs[0]->time_base, AV_TIME_BASE_Q);
                    if (delay > 0 && delay < 1000000)
                        usleep(delay);
            }
            last_pts = pFrame->pts;
        }*/
        cout <<"filt_frame:" <<filt_frame->height << "x" << filt_frame->width << ", format:"<< filt_frame->format<< endl;
    }
    
    return 0;
}


void FFmpegUtil::saveFrameAsJPEG(AVFrame* pFrame, int width, int height, int iFrame)
{
    char fname[128] = { 0 };
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    uint8_t *buffer;
    FILE *fp;
    
    buffer = pFrame->data[0];
    sprintf(fname, "%s/_%s%d.jpg", savedPath,"frame", iFrame);
    
    fp = fopen(fname, "wb");
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);
    
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 80, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    
    row_stride = width * 3;
    while (cinfo.next_scanline < height)
    {
        row_pointer[0] = &buffer[cinfo.next_scanline * row_stride];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    fclose(fp);
    jpeg_destroy_compress(&cinfo);
    return;
}
    
    
int FFmpegUtil::sdlDisplayFrame(SwsContext *convert_ctx,AVFrame *pFrame){
    
    int w = pFrame->width;
    int h = pFrame->height;
    cout << "SDL video display ...." << "width:"<< w << " height:"<<h << endl;
    
    int dstW = w;
    int dstH = h;
    
    if(w <1 || h<1)
        return -1;
    
    static SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = w;
    rect.h = h;
    
    enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(pFrame->format);
    static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
    
    if(sdlRenderer == nullptr)
    {
        SDL_Window *screen = SDL_CreateWindow("Test",
                                              SDL_WINDOWPOS_UNDEFINED,
                                              SDL_WINDOWPOS_UNDEFINED,
                                              w, h,
                                              SDL_WINDOW_RESIZABLE| SDL_WINDOW_OPENGL);
        if(!screen) {
            printf("SDL: could not set video mode - exiting\n");
            return -1;
        }
        
        sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
        cout << "SDL video is initilized sucessfully " << endl;
    }
    
    if(sdlTexture == nullptr)
        sdlTexture = SDL_CreateTexture(sdlRenderer,
                                   SDL_PIXELFORMAT_IYUV,
                                   SDL_TEXTUREACCESS_STREAMING,
                                   w,
                                   h);
    
    static AVFrame* dst = nullptr;
    if(!dst)
    {
        dst = av_frame_alloc();
        av_image_alloc(dst->data,dst->linesize,dstW,dstH,dst_pix_fmts,1);
        //av_image_fill_arrays(dst->data, dst->linesize, pFrame, AV_PIX_FMT_BGR24, w, h, 32);
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
    
    SDL_UpdateTexture( sdlTexture, &rect, dst->data[0], dst->linesize[0]);
    SDL_RenderClear( sdlRenderer );
    SDL_RenderCopy( sdlRenderer, sdlTexture, &rect, &rect );
    SDL_RenderPresent( sdlRenderer );
        //延时20ms
    SDL_Delay(20);
    
    return 0;
}


int FFmpegUtil::displayFrame(SwsContext *convert_ctx, AVFrame *pFrame) {
    std::cout << "display frame: " << pVideoCodecCtx->frame_number << ", pix_fmt:"<< pFrame->format <<  std::endl;
    
    int w = pFrame->width;
    int h = pFrame->height;
    
    cout << "width:"<< w << " height:"<<h << endl;
    
    if(w < 1 || h < 1)
        return 0;
    
    int dstW = w;
    int dstH = h;
    
    static enum AVPixelFormat org_pix_fmts = pVideoCodecCtx->pix_fmt;
    static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_BGR24;
    if(convert_ctx == nullptr){
        convert_ctx = sws_getContext(pVideoCodecCtx->width, pVideoCodecCtx->height,org_pix_fmts,
                                     dstW, dstH,dst_pix_fmts,
                                     SWS_BICUBIC, NULL, NULL, NULL);
    }
    
    if(!convert_ctx) {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
        return -1;
    }
    
    AVFrame* dst = nullptr;
    if(!dst)
    {
        dst = av_frame_alloc();
        av_image_alloc(dst->data,dst->linesize,dstW,dstH,dst_pix_fmts,1);
        //av_image_fill_arrays(dst->data, dst->linesize, pFrame, AV_PIX_FMT_BGR24, w, h, 32);
    }
    
    sws_scale(convert_ctx, pFrame->data,
              pFrame->linesize,
              0, pVideoCodecCtx->height,
              dst->data, dst->linesize);
    
    
    cv::Mat output;
    
    cv::Mat m = cv::Mat(dstH,dstW,CV_8UC3,dst->data[0]);
    if(cvframeProcessor != nullptr){
        cvframeProcessor->process(m, output);
    }else
    {
        output = m;
    }
    
    imshow("MyVideo", output);
    waitKey(10);
    
    if(!dst){
        av_freep(&dst->data[0]);
    }
    
    return 0;
}

void FFmpegUtil::dump(){
    
    puts("AVFormatContext信息：");
    printf("文件名：%s\n", ifmt_ctx->url);
    
    int iHour, iMinute, iSecond, iTotalSeconds;//HH:MM:SS
    
    iTotalSeconds = (int)ifmt_ctx->duration / 1000000;/*微秒*/
    iHour = iTotalSeconds / 3600;//小时
    iMinute = iTotalSeconds % 3600 / 60;//分钟
    iSecond = iTotalSeconds % 60;//秒
    
    printf("持续时间：%02d:%02d:%02d\n", iHour, iMinute, iSecond);
    std::cout << "平均混合码率：" << ifmt_ctx->bit_rate / 1000 <<  "kb/s" << std::endl;
    printf("视音频个数：%d\n", ifmt_ctx->nb_streams);
    puts("AVInputFormat信息:");
    printf("封装格式名称：%s\n", ifmt_ctx->iformat->name);
    printf("封装格式长名称：%s\n", ifmt_ctx->iformat->long_name);
    printf("封装格式扩展名：%s\n", ifmt_ctx->iformat->extensions);
    printf("封装格式ID：%d\n", ifmt_ctx->iformat->raw_codec_id);
    puts("AVStream信息:");
    printf("视频流标识符：%d\n", ifmt_ctx->streams[videoIndex]->index);
    printf("音频流标识符：%d\n", ifmt_ctx->streams[audioIndex]->index);
    puts("AVCodecContext信息:");
    printf("视频码率：%lld kb/s\n", pVideoCodecCtx->bit_rate / 1000);
    printf("视频大小：%d * %d\n", pVideoCodecCtx->width, pVideoCodecCtx->height);
    cout << "AVCodec信息:" << endl;
    printf("视频编码格式：%s\n", pVideoCodec->name);
    printf("视频编码详细格式：%s\n", pVideoCodec->long_name);
    printf("视频时长：%lld秒\n", ifmt_ctx->streams[videoIndex]->duration/1000);
    printf("音频时长：%lld秒\n", ifmt_ctx->streams[audioIndex]->duration/1000);
    printf("音频采样率：%d\n", ifmt_ctx->streams[audioIndex]->codecpar->sample_rate);
    printf("音频信道数目：%d\n", ifmt_ctx->streams[audioIndex]->codecpar->channels);
    
    AVDictionaryEntry *dict = NULL;
    cout<< "AVFormatContext元数据：" << endl;
    dict = av_dict_get(ifmt_ctx->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    while (dict)
    {
        printf("[%s] = %s\n", dict->key, dict->value);
        dict = av_dict_get(ifmt_ctx->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    }
    
    puts("AVStream视频元数据：");
    dict = NULL;
    dict = av_dict_get(ifmt_ctx->streams[videoIndex]->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    while (dict)
    {
        printf("[%s] = %s\n", dict->key, dict->value);
        dict = av_dict_get(ifmt_ctx->streams[videoIndex]->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    }
    
    puts("AVStream音频元数据：");
    dict = NULL;
    dict = av_dict_get(ifmt_ctx->streams[audioIndex]->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    while (dict)
    {
        printf("[%s] = %s\n", dict->key, dict->value);
        dict = av_dict_get(ifmt_ctx->streams[audioIndex]->metadata, "", dict, AV_DICT_IGNORE_SUFFIX);
    }
    
    
    cout<<"dump-->" << endl;
    av_dump_format(ifmt_ctx, -1, filename.c_str(), 0);
    printf("\n\n编译信息：\n%s\n\n", avcodec_configuration());
}


int FFmpegUtil::simplest_rgb24_to_bmp(const char *rgb24path,int width,int height,const char *bmppath){
    typedef struct
    {
        long imageSize;
        long blank;
        long startPosition;
    }BmpHead;
    
    typedef struct
    {
        long  Length;
        long  width;
        long  height;
        unsigned short  colorPlane;
        unsigned short  bitColor;
        long  zipFormat;
        long  realSize;
        long  xPels;
        long  yPels;
        long  colorUse;
        long  colorImportant;
    }InfoHead;
    
    int i=0,j=0;
    BmpHead m_BMPHeader={0};
    InfoHead  m_BMPInfoHeader={0};
    char bfType[2]={'B','M'};
    int header_size=sizeof(bfType)+sizeof(BmpHead)+sizeof(InfoHead);
    unsigned char *rgb24_buffer=NULL;
    FILE *fp_rgb24=NULL,*fp_bmp=NULL;
    
    if((fp_rgb24=fopen(rgb24path,"rb"))==NULL){
        printf("Error: Cannot open input RGB24 file.\n");
        return -1;
    }
    if((fp_bmp=fopen(bmppath,"wb"))==NULL){
        printf("Error: Cannot open output BMP file.\n");
        return -1;
    }
    
    rgb24_buffer=(unsigned char *)malloc(width*height*3);
    fread(rgb24_buffer,1,width*height*3,fp_rgb24);
    
    m_BMPHeader.imageSize=3*width*height+header_size;
    m_BMPHeader.startPosition=header_size;
    
    m_BMPInfoHeader.Length=sizeof(InfoHead);
    m_BMPInfoHeader.width=width;
    //BMP storage pixel data in opposite direction of Y-axis (from bottom to top).
    m_BMPInfoHeader.height=-height;
    m_BMPInfoHeader.colorPlane=1;
    m_BMPInfoHeader.bitColor=24;
    m_BMPInfoHeader.realSize=3*width*height;
    
    fwrite(bfType,1,sizeof(bfType),fp_bmp);
    fwrite(&m_BMPHeader,1,sizeof(m_BMPHeader),fp_bmp);
    fwrite(&m_BMPInfoHeader,1,sizeof(m_BMPInfoHeader),fp_bmp);
    
    //BMP save R1|G1|B1,R2|G2|B2 as B1|G1|R1,B2|G2|R2
    //It saves pixel data in Little Endian
    //So we change 'R' and 'B'
    for(j =0;j<height;j++){
        for(i=0;i<width;i++){
            char temp=rgb24_buffer[(j*width+i)*3+2];
            rgb24_buffer[(j*width+i)*3+2]=rgb24_buffer[(j*width+i)*3+0];
            rgb24_buffer[(j*width+i)*3+0]=temp;
        }
    }
    fwrite(rgb24_buffer,3*width*height,1,fp_bmp);
    fclose(fp_rgb24);
    fclose(fp_bmp);
    free(rgb24_buffer);
    printf("Finish generate %s!\n",bmppath);
    return 0;
    return 0;
}

int FFmpegUtil::open_input_file(const char *filename)
{
    int ret;
    unsigned int i;
    
    ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    stream_ctx = (StreamContext*)av_mallocz_array(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);
    stream_ctx->dec_ctx = nullptr;
    stream_ctx->enc_ctx = nullptr;
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        AVCodecContext *codec_ctx;
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }
        codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            av_log(NULL, AV_LOG_ERROR, "Failed to allocate the decoder context for stream #%u\n", i);
            return AVERROR(ENOMEM);
        }
        ret = avcodec_parameters_to_context(codec_ctx, stream->codecpar);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context "
                   "for stream #%u\n", i);
            return ret;
        }
        /* Reencode video & audio and remux subtitles etc. */
        if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || codec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
                pVideoCodecCtx = codec_ctx;
                videoIndex = i;
            }else{
                pAudioCodecCtx = codec_ctx;
                audioIndex = i;
            }
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
            
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
                pVideoCodec = dec;
            }else{
                pAudioCodec = dec;
            }
        }
        stream_ctx[i].dec_ctx = codec_ctx;
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}

int FFmpegUtil::open_output_file(const char *filename)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;
    
    ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
            return AVERROR_UNKNOWN;
        }
        
        in_stream = ifmt_ctx->streams[i];
        dec_ctx = stream_ctx[i].dec_ctx;
        
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO
            || dec_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
            /* in this example, we choose transcoding to same codec */
            encoder = avcodec_find_encoder(dec_ctx->codec_id);
            //encoder = avcodec_find_encoder(AV_CODEC_ID_FLV1);
            if (!encoder) {
                av_log(NULL, AV_LOG_FATAL, "Necessary encoder not found\n");
                return AVERROR_INVALIDDATA;
            }
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) {
                av_log(NULL, AV_LOG_FATAL, "Failed to allocate the encoder context\n");
                return AVERROR(ENOMEM);
            }
            
            /* In this example, we transcode to same properties (picture size,
             * sample rate etc.). These properties can be changed for output
             * streams easily using filters */
            if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
                enc_ctx->height = dec_ctx->height;
                enc_ctx->width = dec_ctx->width;
                enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
                /* take first format from list of supported formats */
                if (encoder->pix_fmts)
                    enc_ctx->pix_fmt = encoder->pix_fmts[0];
                else
                    enc_ctx->pix_fmt = dec_ctx->pix_fmt;
                /* video time_base can be set to whatever is handy and supported by encoder */
                enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
            } else {
                enc_ctx->sample_rate = dec_ctx->sample_rate;
                enc_ctx->channel_layout = dec_ctx->channel_layout;
                enc_ctx->channels = av_get_channel_layout_nb_channels(enc_ctx->channel_layout);
                /* take first format from list of supported formats */
                enc_ctx->sample_fmt = encoder->sample_fmts[0];
                enc_ctx->time_base = (AVRational){1, enc_ctx->sample_rate};
            }
            
            if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
                enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            
            /* Third parameter can be used to pass settings to encoder */
            ret = avcodec_open2(enc_ctx, encoder, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n", i);
                return ret;
            }
            
            out_stream->time_base = enc_ctx->time_base;
            stream_ctx[i].enc_ctx = enc_ctx;
        } else if (dec_ctx->codec_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_FATAL, "Elementary stream #%d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        } else {
            /* if this stream must be remuxed */
            ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Copying parameters for stream #%u failed\n", i);
                return ret;
            }
            out_stream->time_base = in_stream->time_base;
        }
        
    }
    av_dump_format(ofmt_ctx, 0, filename, 1);
    
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", filename);
            return ret;
        }
    }
    
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    
    return 0;
}


