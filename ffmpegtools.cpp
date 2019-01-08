//
//  ffmpegtools.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/4.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include <setjmp.h>
#include "boost/algorithm/string.hpp"
#include "ffmpegtools.hpp"

extern "C"{
#include <libavutil/time.h>
#include "libavutil/avassert.h"
}

#include <OpenAL/al.h>
#include <OpenAl/alc.h>

FFmpegTools::FFmpegTools(){
    ifmt_ctx = nullptr;
    ofmt_ctx = nullptr;
    
    filter_ctx = nullptr;
    
    video_stream_index = -1;
    audio_stream_index = -1;
    
    stream_ctx = nullptr;
    convert_ctx = nullptr;
    videoq  = new PacketQueue();
    
    window           = nullptr;
    video_texture    = nullptr;
    renderer         = nullptr;
    
    frame_number = 0;
    first_frame_number = -1;
    
    codec_parser_ctx = nullptr;
}

FFmpegTools::~FFmpegTools(){
    if(ifmt_ctx)
    {
        for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
            avcodec_free_context(&stream_ctx[i].dec);
            if (ofmt_ctx && ofmt_ctx->nb_streams > i && ofmt_ctx->streams[i] && stream_ctx[i].enc)
            {
                avcodec_free_context(&stream_ctx[i].enc);
                avcodec_close(stream_ctx[i].enc);
            }
            if (filter_ctx && filter_ctx[i].filter_graph)
                avfilter_graph_free(&filter_ctx[i].filter_graph);
        }
    }
    if(filter_ctx){
        av_free(filter_ctx);
        filter_ctx = nullptr;
    }
    
    if(audio_stream_ctx){
        av_free(audio_stream_ctx);
        audio_stream_ctx = nullptr;
    }
    
    if(video_stream_ctx){
        av_free(video_stream_ctx);
        video_stream_ctx = nullptr;
    }
    
    if(convert_ctx){
        sws_freeContext(convert_ctx);
        convert_ctx = nullptr;
    }
    
    if(ifmt_ctx){
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = nullptr;
    }
    
    if(codec_parser_ctx)
        av_parser_close(codec_parser_ctx);
    
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
    
    avformat_network_deinit();
    printf("delete ffmpeg\n");
}


int FFmpegTools::open_input_file(const char *filename)
{
    int ret;
    unsigned int i;
    
    ifmt_ctx = NULL;
    AVDictionary *avdic=nullptr;
    
    if(boost::starts_with(filename,"rtsp://") || boost::starts_with(filename,"rtmp://")){
        
        av_dict_set(&avdic,"rtsp_transport","tcp",0);
        av_dict_set(&avdic, "buffer_size", "1024000", 0);
        av_dict_set(&avdic, "stimeout", "20000000", 0);
        av_dict_set(&avdic, "max_delay", "500000", 0);
        avformat_network_init();
    }
    
    if (avformat_open_input(&ifmt_ctx, filename, NULL, &avdic) != 0) {
        printf("can't open the file. \n");
        return false;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    
    stream_ctx = (OutputStream*)av_mallocz_array(ifmt_ctx->nb_streams, sizeof(*stream_ctx));
    if (!stream_ctx)
        return AVERROR(ENOMEM);
    
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u of codec_id %d\n", i,
                   stream->codecpar->codec_id);
            return AVERROR_DECODER_NOT_FOUND;
        }
        AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
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
                av_log(NULL,AV_LOG_INFO,"codec type is VIDEO\n");
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
                video_stream_index = i;
            }else{
                av_log(NULL,AV_LOG_INFO,"codec type is AUDIO\n");
                audio_stream_index = i;
            }
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
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
        stream_ctx[i].dec = codec_ctx;
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    return 0;
}


AVCodecContext* FFmpegTools::get_video_decoder_ctx() {
    if(video_stream_index >=0)
        return stream_ctx[video_stream_index].dec;
    else return nullptr;
}
AVCodecContext* FFmpegTools::get_audio_decoder_ctx() {
    if(audio_stream_index >=0 )
    return stream_ctx[audio_stream_index].dec;
    else return nullptr;
}


int FFmpegTools::init_filter(FilteringContext* fctx, AVCodecContext *dec_ctx,
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

int FFmpegTools::init_filters(void)
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
            filter_spec = "null"; /* passthrough (dummy) filter for video */
        else
            filter_spec = "anull"; /* passthrough (dummy) filter for audio */
        
        ret = init_filter(&filter_ctx[i], stream_ctx[i].dec,
                          stream_ctx[i].enc, filter_spec);
        if (ret)
            return ret;
    }
    return 0;
}


int FFmpegTools::handle_filters(AVFrame *filt_frame,
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
    }
    
    return 0;
}


int FFmpegTools::get_first_video_frame(AVFrame *pframe ) {
    int ret = -1;
    int audio_size = 0;
    enum AVMediaType type = AVMEDIA_TYPE_UNKNOWN;
    AVPacket *packet = av_packet_alloc();
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    AVFrame *frame = NULL;
    
    do{
        printf("first av_reading ....\n");
        ret = av_read_frame(ifmt_ctx, packet);
        
        if (ret < 0)
        {
            if (ret == AVERROR_EOF){
                printf("no frame is avaiable due to EOF\n");
                break;
            }
            if (ifmt_ctx->pb->error == 0) // No error,wait for user input
            {
                continue;
            }
            else{
                printf("something happens ...\n");
                break;
            }
        }
        
        int stream_index = packet->stream_index;
        type = ifmt_ctx->streams[packet->stream_index]->codecpar->codec_type;
        
        if(type == AVMEDIA_TYPE_VIDEO){
            frame = av_frame_alloc();
            if (!frame) {
                frame = nullptr;
                break;
            }
            
            av_packet_rescale_ts(packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 get_video_decoder_ctx()->time_base);
            
            ret = avcodec_decode_video2(get_video_decoder_ctx(),
                                        frame,
                                        &got_frame,
                                        packet);
            
            if (ret < 0) {
                av_frame_unref(frame);
                av_frame_free(&frame);
                fprintf(stderr, "Decoding failed\n");
                break;
            }
            
            if (got_frame) {
                frame->pts = frame->best_effort_timestamp;
                printf("got first video frame, frame %dx%d\n",frame->width,frame->height);
                if(first_frame_number < 0)
                    first_frame_number =
                av_frame_ref(pframe, frame);
                av_frame_unref(frame);
                break;
            } else {
                av_frame_free(&frame);
            }
        }else{
            
            av_packet_unref(packet);
        }
       
    }while(1);
    
    av_packet_unref(packet);
    av_packet_free(&packet);
    
    return 1;
}


int FFmpegTools::decode(AVCodecContext *avctx, AVFrame *frame, int *got_frame, AVPacket *pkt)
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
    
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    
    return 0;
}


int FFmpegTools::read_frames(){
    int ret = -1;
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
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
                frame = nullptr;
                break;
            }
            
            stream_index = packet.stream_index;
            type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
            
            AVCodecContext* avctx = get_video_decoder_ctx();
            if(type == AVMEDIA_TYPE_AUDIO)
                avctx = get_audio_decoder_ctx();
            
            //printf("Demuxer gave frame of stream_index %u\n", stream_index);
            if(type == AVMEDIA_TYPE_VIDEO)
            {
                av_packet_rescale_ts(&packet,
                                     ifmt_ctx->streams[stream_index]->time_base,
                                      get_video_decoder_ctx()->time_base);
                
                ret = decode(avctx, frame, &got_frame, &packet);
               
                if (ret < 0) {
                    av_frame_unref(frame);
                    av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                    break;
                }
                
                if (got_frame) {
                    frame->pts = frame->best_effort_timestamp;
                    printf("push to queue, send signal %dx%d\n",frame->width,frame->height);
                    sdl_display_frame(frame);
                    //save_as_jpeg(frame, frame->width, frame->height);
                    frameq.enQueue(frame);
                    av_frame_unref(frame);
                    //av_frame_free(&frame);
                    if (ret < 0)
                        goto end;
                    usleep(3000);
                } else {
                    av_frame_free(&frame);
                }
            }
            
            av_packet_unref(&packet);
        
    }
    
end:
    av_packet_unref(&packet);
    av_frame_free(&frame);

    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    return NULL;
}

void FFmpegTools::save_as_jpeg(AVFrame* frame, int width, int height)
{
    
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    uint8_t *buffer;
    FILE *fp;
    static int iFrame = 0;
    
    if(!frame)
        return ;
    
    if(iFrame > 100 || iFrame < 5){
        iFrame++;
        return ;
    }
    buffer = frame->data[0];
    
    char fname[128] = { 0 };
    sprintf(fname, "/Users/wangbinfeng/dev/opencvdemo/frame_%d.jpg", iFrame++);
    
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

int FFmpegTools::read_jpeg_file (const char * filename,uint8_t **data,int &width, int& height,unsigned short& depth)
{
    /* This struct contains the JPEG decompression parameters and pointers to
     * working space (which is allocated as needed by the JPEG library).
     */
    struct jpeg_decompress_struct cinfo;
    /* We use our private extension JPEG error handler.
     * Note that this struct must live as long as the main JPEG parameter
     * struct, to avoid dangling-pointer problems.
     */
    
    struct my_error_mgr {
        struct jpeg_error_mgr pub;    /* "public" fields */
        jmp_buf setjmp_buffer;    /* for return to caller */
    };
    typedef struct my_error_mgr * my_error_ptr;
    struct my_error_mgr jerr;
    /* More stuff */
    FILE * infile;        /* source file */
    JSAMPARRAY buffer;        /* Output row buffer */
    int row_stride;        /* physical row width in output buffer */
    
    /* In this example we want to open the input file before doing anything else,
     * so that the setjmp() error recovery below can assume the file is open.
     * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
     * requires it in order to read binary files.
     */
    
    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return 0;
    }
    
    /* Step 1: allocate and initialize JPEG decompression object */
    
    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = [](j_common_ptr cinfo)
    {
        /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
        my_error_ptr myerr = (my_error_ptr) cinfo->err;
        
        /* Always display the message. */
        /* We could postpone this until after returning, if we chose. */
        (*cinfo->err->output_message) (cinfo);
        
        /* Return control to the setjmp point */
        longjmp(myerr->setjmp_buffer, 1);
    };
    
    /* Establish the setjmp return context for my_error_exit to use. */
    if (setjmp(jerr.setjmp_buffer)) {
        /* If we get here, the JPEG code has signaled an error.
         * We need to clean up the JPEG object, close the input file, and return.
         */
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return 0;
    }
    /* Now we can initialize the JPEG decompression object. */
    jpeg_create_decompress(&cinfo);
    
    /* Step 2: specify data source (eg, a file) */
    
    jpeg_stdio_src(&cinfo, infile);
    
    /* Step 3: read file parameters with jpeg_read_header() */
    
    (void) jpeg_read_header(&cinfo, TRUE);
    /* We can ignore the return value from jpeg_read_header since
     *   (a) suspension is not possible with the stdio data source, and
     *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
     * See libjpeg.txt for more info.
     */
    
    /* Step 4: set parameters for decompression */
    
    /* In this example, we don't need to change any of the defaults set by
     * jpeg_read_header(), so we do nothing here.
     */
    
    /* Step 5: Start decompressor */
    
    (void) jpeg_start_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
     * with the stdio data source.
     */
    width=cinfo.output_width;
    height=cinfo.output_height;
    depth=cinfo.output_components;
    
    *data = new uint8_t[width*height*depth];
    //uint8_t *dst_buffer = *data;
    memset(*data,0,sizeof(uint8_t)*width*height*depth);
    
    /* We may need to do some setup of our own at this point before reading
     * the data.  After jpeg_start_decompress() we have the correct scaled
     * output image dimensions available, as well as the output colormap
     * if we asked for color quantization.
     * In this example, we need to make an output work buffer of the right size.
     */
    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;
    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)
    ((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    
    /* Step 6: while (scan lines remain to be read) */
    /*           jpeg_read_scanlines(...); */
    
    /* Here we use the library's state variable cinfo.output_scanline as the
     * loop counter, so that we don't have to keep track ourselves.
     */
    
    uint8_t *p= *data;
    while (cinfo.output_scanline < cinfo.output_height) {
        /* jpeg_read_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could ask for
         * more than one scanline at a time if that's more convenient.
         */
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);
        /* Assume put_scanline_someplace wants a pointer and sample count. */
        //put_scanline_someplace(buffer[0], row_stride);
        memcpy(p,*buffer,width*depth);    //将buffer中的数据逐行给src_buff
        p+=width*depth;
    }
    
    /* Step 7: Finish decompression */
    
    (void) jpeg_finish_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
     * with the stdio data source.
     */
    
    /* Step 8: Release JPEG decompression object */
    
    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_decompress(&cinfo);
    
    /* After finish_decompress, we can close the input file.
     * Here we postpone it until after no more JPEG errors are possible,
     * so as to simplify the setjmp error logic above.  (Actually, I don't
     * think that jpeg_destroy can do an error exit, but why assume anything...)
     */
    fclose(infile);
    
    /* At this point you may want to check to see whether any corrupt-data
     * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
     */
    
    /* And we're done! */
    return 1;
}


bool FFmpegTools::sdl_init() {
    Uint32 flags = SDL_INIT_TIMER;
    flags |= SDL_INIT_AUDIO;
    flags |= SDL_INIT_VIDEO;
    
    if(SDL_Init(flags)) {
        printf( "Could not initialize SDL - %s\n", SDL_GetError());
        return false;
    }
    printf("SDL is initialized sucessfully !");
    return true;
}

bool FFmpegTools::sdl_loop_event(){
    
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
    return true;
}


int FFmpegTools::sdl_display_frame(AVFrame *frame){
    
    if(!frame)
        return -1;
    
    int w=frame->width;
    int h=frame->height;
    
    if(w <=0 || h<=0)
        return -1;
    
    SDL_Rect display_rect;
    
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
    
    
    if(window == nullptr)
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
    
    
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear( renderer );
    
    enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(frame->format);
    static enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_YUV420P;
        
    AVFrame* dst = av_frame_alloc();
    av_image_alloc(dst->data,dst->linesize,w,h,dst_pix_fmts,1);
        
    
    if(convert_ctx == nullptr){
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

    
    SDL_RenderCopy( renderer, video_texture, &(rect), &(display_rect) );
    SDL_RenderPresent( renderer );
    
    return 0;
}


double FFmpegTools::r2d(AVRational r) const
{
    return r.num == 0 || r.den == 0 ? 0. : (double)r.num / (double)r.den;
}

double FFmpegTools::get_duration_sec(int stream_index) const
{
    double sec = (double)ifmt_ctx->duration / (double)AV_TIME_BASE;
    double eps_zero = 0.000025;
    if (sec < eps_zero)
    {
        sec = (double)ifmt_ctx->streams[stream_index]->duration * r2d(ifmt_ctx->streams[stream_index]->time_base);
    }
    
    return sec;
}

int64_t FFmpegTools::get_total_frames(int stream_index) const
{
    int64_t nbf = ifmt_ctx->streams[stream_index]->nb_frames;
    
    if (nbf == 0)
    {
        nbf = (int64_t)floor(get_duration_sec(stream_index) * get_fps(stream_index) + 0.5);
    }
    return nbf;
}

int64_t FFmpegTools::get_bitrate() const
{
    return ifmt_ctx->bit_rate;
}

double FFmpegTools::get_fps(int stream_index) const
{
    double fps = r2d(av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[stream_index], NULL));
    return fps;
}

int64_t FFmpegTools::dts_to_frame_number(int64_t dts,int stream_index)
{
    double sec = dts_to_sec(dts,stream_index);
    return (int64_t)(get_fps(stream_index) * sec + 0.5);
}

double FFmpegTools::dts_to_sec(int64_t dts,int stream_index)
{
    return (double)(dts - ifmt_ctx->streams[stream_index]->start_time) *
    r2d(ifmt_ctx->streams[stream_index]->time_base);
}


void FFmpegTools::seek_video_frame(int64_t _frame_number)
{
    int64_t  picture_pts = AV_NOPTS_VALUE;
    
    _frame_number = std::min(_frame_number, get_total_frames(video_stream_index));
    int delta = 16;
    
    AVFrame* picture = av_frame_alloc();
    if(!picture)
        return;
    
    // if we have not grabbed a single frame before first seek, let's read the first frame
    // and get some valuable information during the process
    if( first_frame_number < 0 && get_total_frames(video_stream_index) > 1 )
        get_video_frame(picture);
    
    AVFormatContext *ic = ifmt_ctx;
    for(;;)
    {
        
        int64_t _frame_number_temp = std::max(_frame_number-delta, (int64_t)0);
        double sec = (double)_frame_number_temp / get_fps(video_stream_index);
        int64_t time_stamp = ic->streams[video_stream_index]->start_time;
        double  time_base  = r2d(ic->streams[video_stream_index]->time_base);
        time_stamp += (int64_t)(sec / time_base + 0.5);
        if (get_total_frames(video_stream_index) > 1)
            av_seek_frame(ic, video_stream_index, time_stamp, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(get_video_decoder_ctx());
        
        if( _frame_number > 0 )
        {
            get_video_frame(picture);
            
            if( _frame_number > 1 )
            {
                frame_number = dts_to_frame_number(picture_pts,video_stream_index) - first_frame_number;
                
                if( frame_number < 0 || frame_number > _frame_number-1 )
                {
                    if( _frame_number_temp == 0 || delta >= INT_MAX/4 )
                        break;
                    delta = delta < 16 ? delta*2 : delta*3/2;
                    continue;
                }
                while( frame_number < _frame_number-1 )
                {
                    if(!get_video_frame(picture))
                        break;
                }
                frame_number++;
                break;
            }
            else
            {
                frame_number = 1;
                break;
            }
        }
        else
        {
            frame_number = 0;
            break;
        }
    }
    av_frame_unref(picture);
    av_frame_free(&picture);
}

void FFmpegTools::seek_video_frame(double sec)
{
    seek_video_frame((int64_t)(sec * get_fps(video_stream_index) + 0.5));
}

bool FFmpegTools::get_video_frame(AVFrame* picture)
{
    bool valid = false;
    int got_picture;
    
    int count_errs = 0;
    const int max_number_of_attempts = 1 << 9;
    AVPacket packet = { .data = NULL, .size = 0 };
    
    AVFormatContext *ic = ifmt_ctx;
    int video_stream = video_stream_index;
    
    if( !ic  )  return false;
    
    if( ic->streams[video_stream]->nb_frames > 0 &&
       frame_number > ic->streams[video_stream]->nb_frames )
        return false;
    
    int64_t  picture_pts = AV_NOPTS_VALUE;
    
    picture = av_frame_alloc();
    if(!picture)
        return false;
    
    // get the next frame
    while (!valid)
    {
        av_packet_unref(&packet);
        
        int ret = av_read_frame(ic, &packet);
        if (ret == AVERROR(EAGAIN)) continue;
        
        /* else if (ret < 0) break; */
        
        if( packet.stream_index != video_stream )
        {
            av_packet_unref (&packet);
            count_errs++;
            if (count_errs > max_number_of_attempts)
                break;
            continue;
        }
        AVCodecContext *video_codec_ctx = get_video_decoder_ctx();
        // Decode video frame
        avcodec_decode_video2(video_codec_ctx, picture, &got_picture, &packet);
        
        // Did we get a video frame?
        if(got_picture)
        {
            //picture_pts = picture->best_effort_timestamp;
            if( picture_pts == AV_NOPTS_VALUE )
                picture_pts = picture->pts != AV_NOPTS_VALUE && picture->pts != 0 ? picture->pts : picture->pkt_dts;
            
            frame_number++;
            valid = true;
        }
        else
        {
            count_errs++;
            if (count_errs > max_number_of_attempts)
                break;
        }
    }
    
    if( valid && first_frame_number < 0 )
        first_frame_number = dts_to_frame_number(picture_pts,video_stream_index);
    
    // return if we have a new picture or not
    return valid;
}

AVFrame * FFmpegTools::alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height, bool alloc)
{
    AVFrame * picture = NULL;
    uint8_t * picture_buf = 0;
    int size;
    
    picture = av_frame_alloc();
    if (!picture)
        return NULL;
    
    picture->format = pix_fmt;
    picture->width = width;
    picture->height = height;
    
    size = av_image_get_buffer_size(pix_fmt, width, height, 1);
    if(alloc){
        picture_buf = (uint8_t *) malloc(size);
        if (!picture_buf)
        {
            av_free(picture);
            return NULL;
        }
        av_image_fill_arrays(picture->data, picture->linesize,
                             picture_buf, pix_fmt, width, height, 1);
    }
    else {
    }
    
    return picture;
}


AVFrame *FFmpegTools::alloc_video_frame(enum AVPixelFormat pix_fmt, int width, int height)
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

AVFrame* FFmpegTools::alloc_audio_frame(enum AVSampleFormat sample_fmt,
                                        uint64_t channel_layout,
                                        int sample_rate,
                                        int nb_samples)
{
    fprintf(stdout, "enter alloc audio frame, nb_samples:%d\n",nb_samples);
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
    fprintf(stdout, "alloc audio frame, nb_samples:%d\n",nb_samples);
    if (nb_samples) {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0) {
            fprintf(stderr, "Error allocating an audio buffer\n");
            exit(1);
        }
    }
    fprintf(stdout, " audio frame allocated, nb_samples:%d\n",nb_samples);
    
    return frame;
}

AVFrame *FFmpegTools::get_audio_frame(AVFormatContext *ic,OutputStream *ost)
{
    const double STREAM_DURATION =  10.0;
    
    AVFrame *frame = ost->frame;
    
    bool valid = false;
    int got_picture;
    
    int count_errs = 0;
    const int max_number_of_attempts = 1 << 9;
    AVPacket packet = { .data = NULL, .size = 0 };
   
    int audio_stream = ost->st->id;
    
    if( !ic  )  return NULL;
    
    if( ic->streams[audio_stream]->nb_frames > 0 &&
       frame_number > ic->streams[audio_stream]->nb_frames )
        return NULL;
    
    int64_t  picture_pts = AV_NOPTS_VALUE;
   
    // get the next frame
    while (!valid)
    {
        av_packet_unref(&packet);
        
        int ret = av_read_frame(ic, &packet);
        if (ret == AVERROR(EAGAIN)) continue;
        
        /* else if (ret < 0) break; */
        
        if( packet.stream_index != ost->st->id)
        {
            av_packet_unref (&packet);
            count_errs++;
            if (count_errs > max_number_of_attempts)
                break;
            continue;
        }
       
        // Decode audio frame
        avcodec_decode_audio4(ost->dec, frame, &got_picture, &packet);
        
        // Did we get a video frame?
        if(got_picture)
        {
            //picture_pts = picture->best_effort_timestamp;
            if( picture_pts == AV_NOPTS_VALUE )
                picture_pts = frame->pts != AV_NOPTS_VALUE && frame->pts != 0 ? frame->pts : frame->pkt_dts;
            
            frame_number++;
            valid = true;
        }
        else
        {
            count_errs++;
            if (count_errs > max_number_of_attempts)
                break;
        }
    }
    
    if( valid && first_frame_number < 0 )
        first_frame_number = dts_to_frame_number(picture_pts,video_stream_index);
    
    if(valid){
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
    }
    
    return frame;
}

void FFmpegTools::add_audio_stream(AVFormatContext *oc,
                                   OutputStream **audio_os,
                                   enum AVCodecID codec_id,
                                        int64_t bit_rate,
                                        int sample_rate )
{
    fprintf(stdout,"add audio stream ...\n");
    int i;
    
    if(*audio_os == NULL){
        *audio_os = new OutputStream;
    }
    
    /* find the encoder */
    AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        return ;
    }
    fprintf(stderr, "find encoder for '%s'\n",avcodec_get_name(codec_id));
    
    (*audio_os)->st = avformat_new_stream(oc, NULL);
    if (!(*audio_os)->st) {
        fprintf(stderr, "Could not allocate stream\n");
        return ;
    }
    
    fprintf(stderr, "audio stream is allocated\n");
    
    AVStream *st = (*audio_os)->st;
    
    st->id = oc->nb_streams-1;
    fprintf(stderr, "audio stream index is %d\n",st->id);
    
    (*audio_os)->enc = avcodec_alloc_context3(codec);
    AVCodecContext *c = (*audio_os)->enc;
    if (!c) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    fprintf(stderr, "alloc an encoding context for audio \n");
    codec->type = AVMEDIA_TYPE_AUDIO;
    
    c->sample_fmt  = (codec)->sample_fmts ? (codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    c->bit_rate    = bit_rate;
    c->sample_rate = sample_rate;
    
    if ((codec)->supported_samplerates) {
        fprintf(stderr, "audio codec supported samplerates ... \n");
        c->sample_rate = (codec)->supported_samplerates[0];
        for (i = 0; (codec)->supported_samplerates[i]; i++) {
            if ((codec)->supported_samplerates[i] == 44100)
                c->sample_rate = 44100;
        }
    }else
        fprintf(stderr, "audio codec does not support samplerates ... \n");
    
    c->channels  = av_get_channel_layout_nb_channels(c->channel_layout);
    c->channel_layout = AV_CH_LAYOUT_STEREO;
    
    if (codec->channel_layouts) {
        c->channel_layout = codec->channel_layouts[0];
        for (i = 0; (codec)->channel_layouts[i]; i++) {
            if ((codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                c->channel_layout = AV_CH_LAYOUT_STEREO;
        }
    }
 
    st->time_base = (AVRational){ 1, c->sample_rate };
    c->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
   
    int ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }
     fprintf(stderr, "audio codec is opened ... \n");
    /* init signal generator */
    (*audio_os)->t     = 0;
    (*audio_os)->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    (*audio_os)->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;
    
    if ((c)->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
       (*audio_os)->nb_samples = 10000;
    else
        (*audio_os)->nb_samples = c->frame_size;
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(st->codecpar,c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }else{
        fprintf(stderr, "Copy the stream parameters\n");
    }
    
    fprintf(stderr, "allocate audio frame sample_fmt:%d,channel_layout:%d,sample_rate:%d, channels:%d, frame_size:%d\n",
            c->sample_fmt, c->channel_layout,
            c->sample_rate,c->channels, c->frame_size);
    
    /* create resampler context */
    
    (*audio_os)->swr_ctx = swr_alloc();

    if (!((*audio_os)->swr_ctx)) {
        fprintf(stderr, "Could not allocate resampler context\n");
        exit(1);
    }

    fprintf(stderr, "resampler context is allocated\n");

    struct SwrContext *swr_ctx = (*audio_os)->swr_ctx;
    /* set options */
    av_opt_set_int       ((*audio_os)->swr_ctx, "in_channel_count",   c->channels,       0);
    av_opt_set_int       ((*audio_os)->swr_ctx, "in_sample_rate",     c->sample_rate,    0);
    av_opt_set_sample_fmt((*audio_os)->swr_ctx, "in_sample_fmt",      AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int       ((*audio_os)->swr_ctx, "out_channel_count",  c->channels,       0);
    av_opt_set_int       ((*audio_os)->swr_ctx, "out_sample_rate",    c->sample_rate,    0);
    av_opt_set_sample_fmt((*audio_os)->swr_ctx, "out_sample_fmt",     c->sample_fmt,     0);

//    (*audio_os)->swr_ctx  = swr_alloc_set_opts(NULL,
//                                           c->channels,
//                                           c->sample_fmt,
//                                           c->sample_rate,
//                                           c->channels,
//                                           AV_SAMPLE_FMT_S16,
//                                           c->sample_rate,
//                                           0, NULL);
//    struct SwrContext *swr_ctx = (*audio_os)->swr_ctx;
    
    fprintf(stderr, "init resampler context\n");
    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        swr_free(&swr_ctx);
        exit(1);
    }
    
    
    (*audio_os)->frame     = alloc_audio_frame(c->sample_fmt, c->channel_layout,
                                               c->sample_rate, (*audio_os)->nb_samples);
    
    fprintf(stderr, "allocate audio temp frame ... \n");
    (*audio_os)->tmp_frame = alloc_audio_frame(AV_SAMPLE_FMT_S16, c->channel_layout,
                                               c->sample_rate, (*audio_os)->nb_samples);
    
    fprintf(stderr, "resampler context is initilized\n");
    
//cleanup:
//    avcodec_free_context(&avctx);
//    avio_closep(&(*output_format_context)->pb);
//    avformat_free_context(*output_format_context);
//    *output_format_context = NULL;
//    return error < 0 ? error : AVERROR_EXIT;
}

void FFmpegTools::log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
    
    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}


int FFmpegTools::write_audio_frame(AVFormatContext *oc,OutputStream* os)
{
    fprintf(stdout, "write audio frame ...\n");
    AVPacket pkt = { 0 }; // data and size must be 0;
   
    int ret = -1;
    int got_packet = 0;
    int dst_nb_samples;
    
    AVCodecContext *c = os->enc;
    av_init_packet(&pkt);
    AVFrame *frame = os->tmp_frame;
 
    if (frame) {
        /* convert samples from native format to destination codec format, using the resampler */
        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(os->swr_ctx,
                                                      c->sample_rate) + frame->nb_samples,
                                        c->sample_rate,
                                        c->sample_rate, AV_ROUND_UP);
        av_assert0(dst_nb_samples == frame->nb_samples);
        
        fprintf(stdout, "write audio frame dst_nb_samples=%d...\n",dst_nb_samples);
        
        /* when we pass a frame to the encoder, it may keep a reference to it
         * internally;
         * make sure we do not overwrite it here
         */
        ret = av_frame_make_writable(frame);
        if (ret < 0){
            fprintf(stderr, "Error to make frame writable due to %d (%s)\n",ret,av_err2str(ret));
            exit(1);
        }
        
        /* convert to destination format */
        ret = swr_convert(os->swr_ctx,
                          os->frame->data, dst_nb_samples,
                          (const uint8_t **)frame->data, frame->nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting due to to %d (%s)\n",ret,av_err2str(ret));
            exit(1);
        }
        frame = os->frame;
        frame->pts = av_rescale_q(os->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
        os->samples_count += dst_nb_samples;
        
        fprintf(stdout, "encode audio frame ...\n" );
        
        ret = avcodec_encode_audio2(c, &pkt, frame, &got_packet);
        
//        ret = avcodec_send_frame(c, frame);
//        if (ret < 0){
//            fprintf(stderr, "Error encoding audio frame due to avcodec_send_frame (%s)\n", av_err2str(ret));
//            got_packet = 0;
//        } else {
//            while (1) {
//                ret = avcodec_receive_packet(c, &pkt);
//                if (ret == AVERROR(EAGAIN))
//                    break;
//                if (ret < 0)
//                    break;
//                got_packet = 1;
//            }
//            if (ret < 0) {
//                fprintf(stderr, "Error encoding audio frame due to avcodec_receive_packet(%s)\n", av_err2str(ret));
//                got_packet = 0;
//            }
//        }
        
        fprintf(stdout, "got_packet %s \n",(got_packet ? "yes" : "no"));
        if (got_packet) {
            av_packet_rescale_ts(&pkt, c->time_base, os->st->time_base);
            pkt.stream_index = os->st->index;
            
            /* Write the compressed frame to the media file. */
            log_packet(oc, &pkt);
            fprintf(stdout, "write the compressed frame to media file\n");
            ret = av_interleaved_write_frame(oc, &pkt);
            
            if (ret < 0) {
                fprintf(stderr, "Error while writing audio frame due to error %d (%s)\n",ret,
                        av_err2str(ret));
            }
        }
        
    }
    
    return (frame || got_packet) ? 0 : 1;
}


int FFmpegTools::open_input_device(OutputStream** audio_ost,
                                         OutputStream** video_ost,
                                         const char* device_name) {
    AVDictionary* options = NULL;
    av_dict_set(&options,"list_devices","true",0);
    AVInputFormat *iformat = av_find_input_format("avfoundation");
    int ret = -1;
    
    //const char* devicename = ":0";
   
    if(iformat)
        ret = avformat_open_input(&ifmt_ctx,device_name,iformat,&options);
    else
        return AVERROR_EXIT;
    
    if(!ifmt_ctx){
        av_log(NULL, AV_LOG_ERROR, "Could not create audio input context due to %d:%s\n",ret,av_err2str(ret));
        return AVERROR_UNKNOWN;
    }
    
    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information:%s\n",av_err2str(ret));
        return ret;
    }
    
    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *stream = ifmt_ctx->streams[i];
        AVCodec *dec = avcodec_find_decoder(stream->codecpar->codec_id);
        
        if (!dec) {
            av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream #%u of codec_id %d\n", i,
                   stream->codecpar->codec_id);
            return AVERROR_DECODER_NOT_FOUND;
        }
        AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
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
            
            OutputStream *ost = NULL;
            
            if (codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO){
                av_log(NULL,AV_LOG_INFO,"codec type is VIDEO\n");
                codec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, stream, NULL);
                if(*video_ost)
                    *video_ost = new OutputStream;
                ost = *video_ost;
                video_stream_index = i;
            }else{
                av_log(NULL,AV_LOG_INFO,"codec type is AUDIO\n");
                if(*audio_ost)
                    *audio_ost = new OutputStream;
                ost = *audio_ost;
                audio_stream_index = i;
            }
            /* Open decoder */
            ret = avcodec_open2(codec_ctx, dec, NULL);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder for stream #%u\n", i);
                return ret;
            }
            
            ost->dec = codec_ctx;
            ost->codec = dec;
            ost->st = stream;
            
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
       
    }
    
    av_dump_format(ifmt_ctx, 0, device_name, 0);
    return 0;
}

int FFmpegTools::cv_capture_camera_to_file(const char *filename, int w, int h, int fps)
{
    int ret = -1;
    unsigned int i = 0;
    
    avdevice_register_all();
    
    AVFormatContext *ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
 
    double bitrate = MIN(fps*w*h, (double)INT_MAX/2);
    
    AVCodec *video_codec = NULL;
    AVCodecContext *c = NULL;
    AVStream *st = NULL;
    
    add_video_stream(&st,ofmt_ctx,&c,&video_codec,filename,
                                    AV_CODEC_ID_H264,
                                    w,h,bitrate,fps,
                                    AV_PIX_FMT_YUV420P);
    
    //add_video_stream_0(&st,ofmt_ctx,&c,&video_codec,AV_CODEC_ID_H264,fps,w,h);
   
    
    if(c == NULL || st == NULL || video_codec == NULL){
        if(c == NULL)
            fprintf(stderr,"codec ctx is NULL\n");
        if(st == NULL)
            fprintf(stderr,"av stream is NULL\n");
        if(video_codec == NULL)
            fprintf(stderr,"video codec is NULL\n");
        return -1;
    }
    fprintf(stdout,"video stream added \n");
    
    bool only_video = true;
    fprintf(stdout,"audio stream added \n");
   
    OutputStream *audio_output_os = NULL;
    
    OutputStream *audio_input_os = NULL;
    OutputStream *video_input_os = NULL;
   
    const int sample_rate = 44100;
    //ret = open_input_device(&audio_input_os,&video_input_os,":0");
    //if(ret  == 0)
    {
       add_audio_stream(ofmt_ctx, &audio_output_os, AV_CODEC_ID_AAC,bitrate,sample_rate);
       only_video =false;
    }
    
    fprintf(stdout,"open output file %s\n",filename);
    /* open the output file, if needed */
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        fprintf(stdout,"no file, open output file %s\n",filename);
        ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open '%s': %s\n", filename,
                    av_err2str(ret));
            return 1;
        }
    }
 
    fprintf(stdout, " init muxer, write output file header \n");
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    
    cv::VideoCapture cap;
    
    cap.open(0);
    if( !cap.isOpened() )
    {
        fprintf(stderr, "Could not initialize capturing...\n");
        return 0;
    }else{
        fprintf(stdout, "camera is opened and capturing is initialized capturing...\n");
    }
    
    int64_t next_pts = 0;
    i = 0;
    
    cv::namedWindow("video capture",1);
    cv::Mat cap_frame,image;
    
    
    const int sample_size = 1024;
    ALbyte buffer[2048];
    ALint sample = 0;
    
    const ALCchar *device = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER );
    const ALCchar *next = device + 1;
    size_t len = 0;
    
    fprintf(stdout, "Devices list:\n");
    fprintf(stdout, "----------\n");
    while (device && *device != '\0' && next && *next != '\0') {
        fprintf(stdout, "%s\n", device);
        len = strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
    fprintf(stdout, "----------\n");
    
    ALCdevice *ao_device = alcCaptureOpenDevice(NULL, sample_rate, AL_FORMAT_STEREO16, sample_size);
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr,"fail to open capture device\n");
        return 0;
    }
    alcCaptureStart(ao_device);
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr,"fail to start capture device\n");
        return 0;
    }
    
    int inx = 0;
    
    while(1){
        fprintf(stdout,"get video frame from cap ...\n");
        
        cap >> cap_frame;
        if( cap_frame.empty() ){
            fprintf(stderr, "can't get captured frame from device, captured frame is empty\n");
            break;
        }
        fprintf(stdout,"captured frame, cols:%d,rows:%d\n",cap_frame.cols,cap_frame.rows);
       
        if(cap_frame.cols != w || cap_frame.rows != h ){
            cv::resize(cap_frame,image,cv::Size(w,h));
        }else
            cap_frame.copyTo(image);
        cv::imshow("video capture",image);
        cv::waitKey(20);
        
        if(only_video){
            fprintf(stdout, "alloc_video_frame rows:%d,cols:%d ...\n",image.rows,image.cols);
            AVFrame* frame = get_video_frame_from_cap(image, c, next_pts);
            fprintf(stdout, "got frame ? %s \n", (frame ? "yes" : "no"));
            
            if(frame){
                fprintf(stdout,"got frame, width:%d,height:%d, pts:%d\n",frame->width,frame->height,next_pts);
                write_video_frame(ofmt_ctx, c, frame, &(st->time_base), st->id);
            }
            av_frame_free(&frame);
            frame = NULL;
        }else{
//            int64 now_tick = getTickCount();
//            alcGetIntegerv(ao_device, ALC_CAPTURE_SAMPLES, (ALCsizei)sizeof(ALint), &sample);
//            fprintf(stdout,"capture audio sampels %d...%d\n",sample,audio_output_os->enc->frame_size);
//            alcCaptureSamples(ao_device, (ALCvoid *)(audio_output_os->tmp_frame->data[0]), sample);
//          
//            int64 next_tick = getTickCount();
//            if((next_tick - now_tick) < 100){
//                char key = (char)cv::waitKey(next_tick - now_tick);
//                if(key == 'q' || key == 'Q' || key == 27)
//                    break;
//            }
            //AVFrame *frame = get_audio_frame(ifmt_ctx,audio_input_os);
            write_audio_frame(ofmt_ctx, audio_output_os);
        }
        inx ++;
        if(inx >= 200)
            break;
    }
    
    av_write_trailer(ofmt_ctx);
    
    if(c != NULL){
        avcodec_free_context(&c);
        avcodec_close(c);
    }
    fprintf(stdout,"stop capturing ...\n");
    alcCaptureStop(ao_device);
    fprintf(stdout,"close capture deviceq ...\n");
    alcCaptureCloseDevice(ao_device);
    
    cap.release();
    
    if (!(ofmt_ctx->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    
    return 0;
}

void FFmpegTools::add_video_stream_0(AVStream **st,
                                     AVFormatContext *oc,
                                     AVCodecContext** codec_ctx,
                                     AVCodec **codec,
                                     enum AVCodecID codec_id,
                                     int fps, int w, int h
                      )
{
    
    int i;
    
    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Could not find encoder for '%s'\n",
                avcodec_get_name(codec_id));
        exit(1);
    }
    
    *st = avformat_new_stream(oc, NULL);
    if (!st) {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    (*st)->id = oc->nb_streams-1;
    *codec_ctx = avcodec_alloc_context3(*codec);
    if (!codec_ctx) {
        fprintf(stderr, "Could not alloc an encoding context\n");
        exit(1);
    }
    
    AVCodecContext *c = *codec_ctx;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    c->bit_rate = 400000;
    /* Resolution must be a multiple of two. */
    c->width    = w;
    c->height   = h;
    /* timebase: This is the fundamental unit of time (in seconds) in terms
     * of which frame timestamps are represented. For fixed-fps content,
     * timebase should be 1/framerate and timestamp increments should be
     * identical to 1. */
    (*st)->time_base = (AVRational){ 1, fps };
    c->time_base       = (*st)->time_base;
    
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
    
    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    
    /* open the codec */
    int ret = avcodec_open2(c, *codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context((*st)->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

void FFmpegTools::add_video_stream(AVFormatContext *oc,OutputStream **video_os,
                                   const char* filename,
                      AVCodecID codec_id,
                      int w, int h, double bitrate,
                      double fps,enum AVPixelFormat pixel_format){
    
    if(*video_os == NULL)
        *video_os = new OutputStream;
    
    int frame_rate, frame_rate_base;
    
    (*video_os)->st = avformat_new_stream(oc, 0);
    
    if (!((*video_os)->st)) {
        fprintf(stderr,"Could not allocate stream\n");
        return ;
    }
    
    AVCodecID guess_codec_id = av_guess_codec(oc->oformat, NULL, filename, NULL, AVMEDIA_TYPE_VIDEO);
    if(codec_id != AV_CODEC_ID_NONE){
        guess_codec_id = codec_id;
    }
    fprintf(stdout, "guess codec_id:%s\n",avcodec_get_name(guess_codec_id));
    //if(codec_tag) c->codec_tag=codec_tag;
    
    AVCodec *codec = avcodec_find_encoder(guess_codec_id);
    if (!codec) {
        fprintf(stderr, "Necessary encoder not found\n");
        return ;
    }
    (*video_os)->codec = codec;
    (*video_os)->st->id = oc->nb_streams-1;
    fprintf(stdout,"video stream index is %d\n",(*video_os)->st->id);
    
    AVCodecContext *c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr,"Failed to allocate the encoder context\n");
        return ;
    }
    
    (*video_os)->enc = c;
    
    c->codec_id = guess_codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;
    
    (*video_os)->st->time_base = (AVRational){ 1, 25 };
    c->time_base       = (*video_os)->st->time_base;
    
    c->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    c->codec_id = guess_codec_id;
    
    /* put sample parameters */
    int64_t lbit_rate = (int64_t)bitrate;
    lbit_rate += (bitrate / 2);
    lbit_rate = std::min(lbit_rate, (int64_t)INT_MAX);
    c->bit_rate = lbit_rate;
    
    // took advice from
    // http://ffmpeg-users.933282.n4.nabble.com/warning-clipping-1-dct-coefficients-to-127-127-td934297.html
    c->qmin = 3;
    
    /* resolution must be a multiple of two */
    c->width = w;
    c->height = h;
    
    /* time base: this is the fundamental unit of time (in seconds) in terms
     of which frame timestamps are represented. for fixed-fps content,
     timebase should be 1/framerate and timestamp increments should be
     identically 1. */
    frame_rate=(int)(fps+0.5);
    frame_rate_base=1;
    while (fabs((double)frame_rate/frame_rate_base) - fps > 0.001){
        frame_rate_base*=10;
        frame_rate=(int)(fps*frame_rate_base + 0.5);
    }
    
    c->time_base.den = frame_rate;
    c->time_base.num = frame_rate_base;
    /* adjust time base for supported framerates */
    if(codec && codec->supported_framerates){
        const AVRational *p= codec->supported_framerates;
        AVRational req = {frame_rate, frame_rate_base};
        const AVRational *best=NULL;
        AVRational best_error= {INT_MAX, 1};
        for(; p->den!=0; p++){
            AVRational error= av_sub_q(req, *p);
            if(error.num <0) error.num *= -1;
            if(av_cmp_q(error, best_error) < 0){
                best_error= error;
                best= p;
            }
        }
        if (best == NULL)
            return ;
        c->time_base.den= best->num;
        c->time_base.num= best->den;
    }
    
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = pixel_format;
    
    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        c->max_b_frames = 2;
    }
    
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO
        || c->codec_id == AV_CODEC_ID_MSMPEG4V3){
        /* needed to avoid using macroblocks in which some coeffs overflow
         this doesn't happen with normal video, it just happens here as the
         motion of the chroma plane doesn't match the luma plane */
        /* avoid FFMPEG warning 'clipping 1 dct coefficients...' */
        c->mb_decision=2;
    }
    
    if (c->codec_id == AV_CODEC_ID_H264) {
        c->gop_size = -1;
        c->qmin = -1;
        c->bit_rate = 0;
        if (c->priv_data)
            av_opt_set(c->priv_data,"crf","23", 0);
    }
    
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    (*video_os)->st->avg_frame_rate = (AVRational){frame_rate, frame_rate_base};
    
    /* open the codec */
    int ret = avcodec_open2(c, codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context((*video_os)->st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

void FFmpegTools::add_video_stream(AVStream **st,
                                        AVFormatContext *oc,
                                        AVCodecContext** c,
                                        AVCodec **codec,
                                        const char* filename,
                                        AVCodecID codec_id,
                                        int w, int h,
                                        double bitrate,
                                        double fps,
                                        enum AVPixelFormat pixel_format
                                        )
{
    
    int frame_rate, frame_rate_base;
    
    *st = avformat_new_stream(oc, 0);
    
    if (!(*st)) {
        fprintf(stderr,"Could not allocate stream\n");
        return ;
    }
   
    AVCodecID guess_codec_id = av_guess_codec(oc->oformat, NULL, filename, NULL, AVMEDIA_TYPE_VIDEO);
    if(codec_id != AV_CODEC_ID_NONE){
        guess_codec_id = codec_id;
    }
    fprintf(stdout, "guess codec_id:%s\n",avcodec_get_name(guess_codec_id));
    //if(codec_tag) c->codec_tag=codec_tag;
   
    *codec = avcodec_find_encoder(guess_codec_id);
    if (!(*codec)) {
        fprintf(stderr, "Necessary encoder not found\n");
        return ;
    }
    
    (*st)->id = oc->nb_streams-1;
    fprintf(stdout,"video stream index is %d\n",(*st)->id);
    *c = avcodec_alloc_context3((*codec));
    if (!c) {
        fprintf(stderr,"Failed to allocate the encoder context\n");
        return ;
    }
    
    (*c)->codec_id = guess_codec_id;
    (*c)->codec_type = AVMEDIA_TYPE_VIDEO;
    
    (*st)->time_base = (AVRational){ 1, 25 };
    (*c)->time_base       = (*st)->time_base;
    
    (*c)->gop_size      = 12; /* emit one intra frame every twelve frames at most */
    (*c)->codec_id = guess_codec_id;
    
    /* put sample parameters */
    int64_t lbit_rate = (int64_t)bitrate;
    lbit_rate += (bitrate / 2);
    lbit_rate = std::min(lbit_rate, (int64_t)INT_MAX);
    (*c)->bit_rate = lbit_rate;
    
    // took advice from
    // http://ffmpeg-users.933282.n4.nabble.com/warning-clipping-1-dct-coefficients-to-127-127-td934297.html
    (*c)->qmin = 3;
    
    /* resolution must be a multiple of two */
    (*c)->width = w;
    (*c)->height = h;
    
    /* time base: this is the fundamental unit of time (in seconds) in terms
     of which frame timestamps are represented. for fixed-fps content,
     timebase should be 1/framerate and timestamp increments should be
     identically 1. */
    frame_rate=(int)(fps+0.5);
    frame_rate_base=1;
    while (fabs((double)frame_rate/frame_rate_base) - fps > 0.001){
        frame_rate_base*=10;
        frame_rate=(int)(fps*frame_rate_base + 0.5);
    }

    (*c)->time_base.den = frame_rate;
    (*c)->time_base.num = frame_rate_base;
    /* adjust time base for supported framerates */
    if(*codec && (*codec)->supported_framerates){
        const AVRational *p= (*codec)->supported_framerates;
        AVRational req = {frame_rate, frame_rate_base};
        const AVRational *best=NULL;
        AVRational best_error= {INT_MAX, 1};
        for(; p->den!=0; p++){
            AVRational error= av_sub_q(req, *p);
            if(error.num <0) error.num *= -1;
            if(av_cmp_q(error, best_error) < 0){
                best_error= error;
                best= p;
            }
        }
        if (best == NULL)
            return ;
        (*c)->time_base.den= best->num;
        (*c)->time_base.num= best->den;
    }

    
    (*c)->gop_size = 12; /* emit one intra frame every twelve frames at most */
    (*c)->pix_fmt = pixel_format;
    
    if ((*c)->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
        (*c)->max_b_frames = 2;
    }
    
    if ((*c)->codec_id == AV_CODEC_ID_MPEG1VIDEO
        || (*c)->codec_id == AV_CODEC_ID_MSMPEG4V3){
        /* needed to avoid using macroblocks in which some coeffs overflow
         this doesn't happen with normal video, it just happens here as the
         motion of the chroma plane doesn't match the luma plane */
        /* avoid FFMPEG warning 'clipping 1 dct coefficients...' */
        (*c)->mb_decision=2;
    }
    
    if ((*c)->codec_id == AV_CODEC_ID_H264) {
        (*c)->gop_size = -1;
        (*c)->qmin = -1;
        (*c)->bit_rate = 0;
        if ((*c)->priv_data)
            av_opt_set((*c)->priv_data,"crf","23", 0);
    }
    
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
        (*c)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    
    (*st)->avg_frame_rate = (AVRational){frame_rate, frame_rate_base};

    /* open the codec */
    int ret = avcodec_open2(*c, *codec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context((*st)->codecpar, *c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
}

AVFrame *FFmpegTools::get_video_frame_from_cap(cv::Mat& image,
                                               AVCodecContext *c,
                                               int64_t &next_pts){
    const double  STREAM_DURATION = 10.0;
    
    fprintf(stdout, "av_compare_ts ...\n");
    if (av_compare_ts(next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0){
        fprintf(stderr,"compare TS ... \n");
        return NULL;
    }
    fprintf(stdout, "alloc_video_frame ...\n");
    AVFrame *frame = alloc_video_frame(c->pix_fmt, c->width, c->height);
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
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
    
    AVFrame* dst_frame = alloc_video_frame(dst_pix_fmts,image.rows,image.cols);
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
    
    frame->pts = next_pts++;
    fprintf(stdout, "free rows:%d,cols:%d ...\n",image.rows,image.cols);
    
    //av_frame_unref(dst_frame);
    av_frame_free(&dst_frame);
    if(sws_ctx)
        sws_freeContext(sws_ctx);
    
    return frame;
}
int FFmpegTools::write_video_frame(AVFormatContext *oc,
                                   AVCodecContext *c,
                                   AVFrame* frame,
                                   AVRational *time_base,
                                   int index)
{
    int ret;
    int got_packet = 0;
    AVPacket pkt = { .data = NULL, .size = 0 };
    
    av_init_packet(&pkt);
    fprintf(stdout, "write video frame w:%d, h:%d, ctx->w:%d,h:%d\n", frame->width,frame->height,c->width,c->height);
    
    ret = avcodec_send_frame(c, frame);
    if (ret < 0){
        fprintf(stderr, "Error encoding video frame due to avcodec_send_frame %d (%s)\n", ret,av_err2str(ret));
        return ret;
    }
    
    
    while (1) {
        ret = avcodec_receive_packet(c, &pkt);
        
        if (ret == AVERROR(EAGAIN)){
            fprintf(stderr, "Error encoding video frame due to avcodec_receive_packet (%s)\n", av_err2str(ret));
            break;
        }
        
        if (ret < 0) {
            fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
            exit(1);
        }
        got_packet = 1;
        break;
    }
    
    //ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    
    if (got_packet) {
        fprintf(stderr, "got packet\n");
        //av_packet_rescale_ts(&pkt, c->time_base, *time_base);
        pkt.stream_index = index;
        
        if (pkt.pts != (int64_t)AV_NOPTS_VALUE)
            pkt.pts = av_rescale_q(pkt.pts, c->time_base, *time_base);
        if (pkt.dts != (int64_t)AV_NOPTS_VALUE)
            pkt.dts = av_rescale_q(pkt.dts, c->time_base, *time_base);
        if (pkt.duration)
            pkt.duration = av_rescale_q(pkt.duration, c->time_base, *time_base);
        
        fprintf(stderr, "av_interleaved_write_frame ...\n");
        //AVRational time_base = oc->streams[pkt.stream_index]->time_base;
        
        //        printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
        //               av_ts2str(pkt.pts), av_ts2timestr(pkt.pts, time_base),
        //               av_ts2str(pkt.dts), av_ts2timestr(pkt.dts, time_base),
        //               av_ts2str(pkt.duration), av_ts2timestr(pkt.duration, time_base),
        //               pkt.stream_index);
        ret = av_write_frame(oc, &pkt);
        //ret = av_interleaved_write_frame(oc, &pkt);
    } else {
        fprintf(stderr, "not got packet\n");
        ret = 0;
    }
    
    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        return ret;
    }
    
    return (frame || got_packet) ? 0 : 1;
}

cv::Mat* FFmpegTools::frame_to_cv_mat(AVFrame* frame){
    int w = frame->width;
    int h = frame->height;
    
    if(w < 1 || h < 1)
        return NULL;
    
    enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(frame->format);
    enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_BGR24;
    
    struct SwsContext *convert_to_mat_ctx = sws_getContext(w, h,org_pix_fmts,
                                                           w, h,dst_pix_fmts,
                                                           SWS_BICUBIC, NULL, NULL, NULL);
    
    if(!convert_to_mat_ctx) {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
        return NULL;
    }
    
    AVFrame* temp_frame = av_frame_alloc();
    av_image_alloc(temp_frame->data,temp_frame->linesize,w,h,dst_pix_fmts,1);
    
    sws_scale(convert_to_mat_ctx, frame->data,
              frame->linesize,
              0, h,
              temp_frame->data, temp_frame->linesize);
    
    cv::Mat* dst_mat = new cv::Mat(h,w,CV_8UC3,temp_frame->data[0]);
    
    av_frame_unref(temp_frame);
    av_frame_free(&temp_frame);
    
    return dst_mat;
}


AVFrame* FFmpegTools::cv_mat_to_frame(const cv::Mat &src_mat,
                                      int w, int h,
                                      enum AVPixelFormat org_pix_fmts){
    
    enum AVPixelFormat dst_pix_fmts = AV_PIX_FMT_BGR24;
    //enum AVPixelFormat org_pix_fmts = AV_PIX_FMT_YUV420P;
    
    AVFrame* dst_frame = alloc_video_frame(dst_pix_fmts,src_mat.rows,src_mat.cols);
    dst_frame->data[0] = src_mat.data;
    dst_frame->linesize[0] = src_mat.step;
    
    struct SwsContext *convert_from_mat_ctx = sws_getContext(w, h,dst_pix_fmts,
                                                             w, h,org_pix_fmts,
                                                             SWS_BICUBIC, NULL, NULL, NULL);
    
    if(!convert_from_mat_ctx) {
        fprintf(stderr, "Cannot initialize the conversion context!\n");
        return NULL;
    }
    fprintf(stdout,"change back to YUV420p...\n");
    
    AVFrame* frame = av_frame_alloc();
    
    sws_scale(convert_from_mat_ctx,
              dst_frame->data,
              dst_frame->linesize,
              0, h,
              frame->data, frame->linesize);
    
    av_frame_unref(dst_frame);
    av_frame_free(&dst_frame);
    
    return frame;
}

int FFmpegTools::cv_handle_frame(AVFrame *pFrame) {
    
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
    cv::Mat dst_mat;
    printf("dst_frame:cols:%d,rows:%d, w:%d, h:%d\n",src_mat.cols,src_mat.rows,w, h);
    
    for(auto it = frame_processors.cbegin(); it != frame_processors.cend(); ++it){
        (*it)->process(src_mat,dst_mat);
    }
    
    //if(dst_mat.cols !=w || dst_mat.rows != h)
    cv::resize(dst_mat, src_mat, cv::Size(w,h));
    
    AVFrame* dst_frame = alloc_video_frame(dst_pix_fmts,src_mat.rows,src_mat.cols);
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


AVFrame* FFmpegTools::load_frame_from_image(const char* filename)
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

AVFrame *FFmpegTools::convert_image_frame(AVCodecContext *c,
                                                struct SwsContext **sws_ctx,
                                                AVFrame* tmp_frame,
                                                int64_t& next_pts,int dstw,int dsth)
{
    /* check if we want to generate more frames */
    const double STREAM_DURATION = 10.0;
    if (av_compare_ts(next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;
    
    AVFrame *frame = alloc_video_frame(c->pix_fmt, c->width, c->height);
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(frame) < 0)
        exit(1);
    
    int w = tmp_frame->width;
    int h = tmp_frame->height;
    if (!(*sws_ctx)) {
        enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(tmp_frame->format);
        *sws_ctx = sws_getContext(w, h,
                                      org_pix_fmts,
                                      dstw, dsth,
                                      AV_PIX_FMT_YUV420P,
                                      SWS_BICUBIC, NULL, NULL, NULL);
        if (!(*sws_ctx)) {
            fprintf(stderr,
                    "Could not initialize the conversion context\n");
            exit(1);
        }
    }
    
    sws_scale(*sws_ctx,
              (const uint8_t * const *) tmp_frame->data,
              tmp_frame->linesize, 0, h, frame->data,
              frame->linesize);
    
    //cv_handle_frame(ost->frame);
   
    frame->pts = next_pts++;
    
    return frame;
}

int FFmpegTools::create_video_from_images(const char *outfilename,
                                          const char *infiles,int w, int h, int fps)
{
    AVStream *out_stream;
    AVStream *in_stream;
    AVCodecContext *dec_ctx, *enc_ctx;
    AVCodec *encoder;
    int ret;
    unsigned int i;
    
    std::vector<AVFrame*> image_frames;
    
    for(i = 0;i < 20 ;i++){
        char fname[128] = { 0 };
        sprintf(fname, infiles, i);
        AVFrame* img_frame = load_frame_from_image(fname);
        if(img_frame){
            image_frames.push_back(img_frame);
            if(w == 0)
            w = img_frame->width;
            if(h == 0)
            h = img_frame->height;
        }
    }
    fprintf(stdout,"video size is %d x %d \n",w,h);
    AVFormatContext *ofmt_ctx = NULL;
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, outfilename);
    if (!ofmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        return AVERROR_UNKNOWN;
    }
    
    double bitrate = MIN(fps*w*h, (double)INT_MAX/2);
    
    AVCodec *video_codec = NULL;
    AVCodecContext *c = NULL;
    AVStream *st = NULL;
    
    add_video_stream(&st,ofmt_ctx,&c,&video_codec,outfilename,
                     AV_CODEC_ID_H264,
                     w,h,bitrate,fps,AV_PIX_FMT_YUV420P);
    
    //add_video_stream_0(&st,ofmt_ctx,&c,&video_codec,AV_CODEC_ID_H264,fps,w,h);
    
    if(c == NULL || st == NULL || video_codec == NULL){
        if(c == NULL)
            fprintf(stderr,"codec ctx is NULL\n");
        if(st == NULL)
            fprintf(stderr,"av stream is NULL\n");
        if(video_codec == NULL)
            fprintf(stderr,"video codec is NULL\n");
        return -1;
    }
    
    fprintf(stdout,"video stream added \n");
    /* open the output file, if needed */
    fprintf(stdout,"open output file %s\n",outfilename);
    ret = avio_open(&ofmt_ctx->pb, outfilename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        fprintf(stderr, "Could not open '%s': %s\n", outfilename,
                av_err2str(ret));
        return 1;
    }
    
    /* copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(st->codecpar, c);
    if (ret < 0) {
        fprintf(stderr, "Could not copy the stream parameters\n");
        exit(1);
    }
    
    /* init muxer, write output file header */
    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    
    int64_t next_pts = 0;
    i = 0;
    struct SwsContext *sws_ctx = NULL;
    cv::namedWindow("video capture",1);
    cv::Mat cap_frame,image;
    int num=0;
    for(i=0;i < 100; i++){
        fprintf(stdout,"get video frame from cap ...\n");
        
        AVFrame* temp_frame = image_frames[num++];
        if(num >= 20)
            num = 0;
        
        AVFrame* frame = convert_image_frame(c, &sws_ctx, temp_frame, next_pts,w,h);
        if(frame){
            fprintf(stdout,"got frame, width:%d,height:%d, pts:%d\n",frame->width,frame->height,next_pts);
            write_video_frame(ofmt_ctx, c, frame, &(st->time_base), st->id);
        }
        av_frame_free(&frame);
    }
    
    av_write_trailer(ofmt_ctx);
    
    if(c != NULL){
        avcodec_free_context(&c);
        avcodec_close(c);
    }
    if(sws_ctx != NULL)
    sws_freeContext(sws_ctx);
    
    if (!(ofmt_ctx->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    
    return 0;
}

void FFmpegTools::detect_qr_code(const char* fname){
    
    
    AVFrame* img_frame = load_frame_from_image(fname);
    
}

int FFmpegTools::open_codec(const std::string& video_codec_name,
                            const std::string& audio_codec_name,
                            u_int8_t* sps , unsigned spsSize,
                            u_int8_t* pps, unsigned ppsSize
                            ) {
    int ret = 0;
    
    if (video_codec_name != ""){
        enum AVCodecID video_codec_id = AV_CODEC_ID_H264;
        AVCodec *dec = avcodec_find_decoder(video_codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find decoder codec_id %d\n",
                   video_codec_id);
            return AVERROR_DECODER_NOT_FOUND;
        }
        
        AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            fprintf(stderr,"Failed to allocate the decoder context \n");
            return AVERROR(ENOMEM);
        }
        
    //    codec_parser_ctx = av_parser_init(codec_id);
    //    if(!codec_parser_ctx){
    //        fprintf(stderr,"Failed to init codec parser\n");
    //        return -1;
    //    }
        
        if (dec->capabilities & AV_CODEC_CAP_TRUNCATED)
            codec_ctx->flags |= AV_CODEC_FLAG_TRUNCATED; /* we do not send complete frames */

        if (sps && pps) {
            int totalsize = 0;
            unsigned char* tmp = NULL;
            unsigned char nalu_header[4] = { 0x00, 0x00, 0x00, 0x01};
            
            totalsize = 4 + spsSize + ppsSize;
            
            tmp = (unsigned char*)realloc(tmp, totalsize);
            memcpy(tmp, nalu_header, 4);
            memcpy(tmp + 4, sps, spsSize);
            memcpy(tmp + 4 + spsSize, pps, ppsSize);
            
            codec_ctx->extradata_size = totalsize;
            codec_ctx->extradata = tmp;
            h264_header_len = totalsize;
            h264_header = tmp;
            codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        
        ret = avcodec_open2(codec_ctx, dec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Failed to open decoder \n");
            return ret;
        }
        
        video_stream_ctx = new OutputStream;
        video_stream_ctx->codec = dec;
        video_stream_ctx->dec = codec_ctx;
        video_stream_ctx->sws_ctx = nullptr;
        video_stream_ctx->swr_ctx = nullptr;
        
        first_frame = true;
        fprintf(stdout, "codec is opened \n");
    }
    
    if( audio_codec_name != "" && audio_codec_name == "MPEG4-GENERIC" ) {
        enum AVCodecID audio_codec_id = AV_CODEC_ID_AAC;
        
        AVCodec *dec = avcodec_find_decoder(audio_codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find decoder codec_id %d\n",
                    audio_codec_id);
            return AVERROR_DECODER_NOT_FOUND;
        }
        
        AVCodecContext *codec_ctx = avcodec_alloc_context3(dec);
        if (!codec_ctx) {
            fprintf(stderr,"Failed to allocate the decoder context \n");
            return AVERROR(ENOMEM);
        }
        
        ret = avcodec_open2(codec_ctx, dec, NULL);
        if (ret < 0) {
            fprintf(stderr, "Failed to open decoder \n");
            return ret;
        }
        
        audio_stream_ctx = new OutputStream;
        audio_stream_ctx->codec = dec;
        audio_stream_ctx->dec = codec_ctx;
        audio_stream_ctx->sws_ctx = nullptr;
        audio_stream_ctx->swr_ctx = nullptr;
        
        first_frame = true;
        fprintf(stdout, "codec is opened \n");
    }
    
    return ret;
    
}

int FFmpegTools::decode_audio_rtp(uint8_t* buf, int buf_len) {
    int got, len;
    AVPacket pkt = { .data = NULL, .size = 0 };
    
    if (buf == NULL || buf_len == 0) {
        return -1;
    }
    
    av_packet_from_data(&pkt, buf, buf_len);
    
    fprintf(stdout, "decode rtp data, size:%d,total_len:%d\n",buf_len, pkt.size);
    
    AVFrame* m_picture = av_frame_alloc();
    
    len = avcodec_decode_audio4(audio_stream_ctx->dec, m_picture, &got, &pkt);
    if (len < 0) {
        fprintf(stdout, "Error while decoding frame\n");
        av_frame_free(&m_picture);
        return -1;
    }
    
    fprintf(stdout, "Got picture %d \n", (got ? len : -100));
    
    if(got){
        
        
    }
    
    pkt.data = NULL;
    pkt.size = 0;
    
    return 0;
}

int FFmpegTools::decode_video_rtp(uint8_t* buf, int buf_len)
{
    int got, len;
    AVPacket pkt = { .data = NULL, .size = 0 };
    
    if(buf == NULL || buf_len == 0){
        return -1;
    }
    
    av_packet_from_data(&pkt,buf,buf_len);
    
    fprintf(stdout, "decode rtp data, size:%d,total_len:%d\n",buf_len,pkt.size);
    
    AVFrame* m_picture = av_frame_alloc();
    
    len = avcodec_decode_video2(video_stream_ctx->dec, m_picture, &got, &pkt);
    if(len < 0){
        fprintf(stdout, "Error while decoding frame\n");
        av_frame_free(&m_picture);
        return -1;
    }
    
    fprintf(stdout, "Got picture %d \n", (got ? len : -100));
    
    if(got){
        
        AVFrame* rgb_frame = alloc_video_frame(AV_PIX_FMT_RGB24, video_stream_ctx->dec->width,
                                               video_stream_ctx->dec->height,true);
        
        if(!video_stream_ctx->sws_ctx){
            video_stream_ctx->sws_ctx = sws_getContext(video_stream_ctx->dec->width,
                                                 video_stream_ctx->dec->height,
                                                 video_stream_ctx->dec->pix_fmt,
                                                 video_stream_ctx->dec->width,
                                                 video_stream_ctx->dec->height,
                                                 AV_PIX_FMT_RGB24,
                                                 SWS_BICUBIC, NULL, NULL, NULL);
        }
        
        m_picture->data[0] += m_picture->linesize[0]*(video_stream_ctx->dec->height-1);
        m_picture->linesize[0] *= -1;
        m_picture->data[1] += m_picture->linesize[1]*(video_stream_ctx->dec->height/2-1);
        m_picture->linesize[1] *= -1;
        m_picture->data[2] += m_picture->linesize[2]*(video_stream_ctx->dec->height/2-1);
        m_picture->linesize[2] *= -1;
        
        sws_scale(stream_ctx->sws_ctx,
                  (const uint8_t* const*)m_picture->data,
                  m_picture->linesize,
                  0,
                  video_stream_ctx->dec->height,
                  rgb_frame->data, rgb_frame->linesize);
        
        save_as_jpeg(rgb_frame, video_stream_ctx->dec->width, video_stream_ctx->dec->height);
        //sdl_display_frame(rgb_frame);
        
        av_frame_free(&rgb_frame);
    }
    
    pkt.data = NULL;
    pkt.size = 0;
    
    return 0;
}
