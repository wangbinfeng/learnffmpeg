#include <iostream>
#include <cassert>
#include <glob.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include "jpeglib.h"
}

/**
 * Pixel formats and codecs
 */
static const AVPixelFormat sourcePixelFormat = AV_PIX_FMT_BGR24;
static const AVPixelFormat destPixelFormat = AV_PIX_FMT_YUV420P;
static const AVCodecID destCodec = AV_CODEC_ID_H264;

static void saveFrameAsJPEG(AVFrame* pFrame, int width, int height, int iFrame)
{
    char fname[128] = { 0 };
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    uint8_t *buffer;
    FILE *fp;
    
    buffer = pFrame->data[0];
    sprintf(fname, "/Users/wangbinfeng/dev/opencvdemo/frame%d.jpg",iFrame);
    
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

int main_ex(int argc, char** argv)
{
    const char *outfileName = "/Users/wangbinfeng/dev/opencvdemo/tiantian_out_480p.mp4";
    const char *infileName = "/Users/wangbinfeng/dev/opencvdemo/tiantian.mov";
    
    std::string input(infileName), output(outfileName);
    uint32_t framesToEncode = 100;
    //std::istringstream("100") >> framesToEncode;
    
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };
    
    /**
     * Create cv::Mat
     */
    cv::VideoCapture videoCapturer(0);
    if (videoCapturer.isOpened() == false) {
        std::cerr << "Cannot open video at '" << input << "'" << std::endl;
        exit(1);
    }
    
    /**
     * Get some information of the video and print them
     */
    double totalFrameCount = videoCapturer.get(cv::CAP_PROP_FRAME_COUNT);
    uint width = videoCapturer.get(cv::CAP_PROP_FRAME_WIDTH),
    height = videoCapturer.get(cv::CAP_PROP_FRAME_HEIGHT);
   
    
    std::cout << input << "[Width:" << width << ", Height=" << height
    << ", FPS: " << videoCapturer.get(cv::CAP_PROP_FPS)
    << ", FrameCount: " << totalFrameCount << "]" << std::endl;
    
    /**
     * Be sure we're not asking more frames than there is
     */
    if (framesToEncode > totalFrameCount && totalFrameCount > 0) {
        std::cerr << "You asked for " << framesToEncode << " but there are only " << totalFrameCount
        << " frames, will encode as many as there is" << std::endl;
        framesToEncode = totalFrameCount;
    }
    
    /**
     * Create an encoder and open it
     */
    //avcodec_register_all();
    
    AVCodec *h264encoder = avcodec_find_encoder(destCodec);
    AVCodecContext *h264encoderContext = avcodec_alloc_context3(h264encoder);
    
    h264encoderContext->pix_fmt = destPixelFormat;
    h264encoderContext->width = width;
    h264encoderContext->height = height;
    h264encoderContext->time_base = (AVRational){1,30};
    
    if (avcodec_open2(h264encoderContext, h264encoder, NULL) < 0) {
        std::cerr << "Cannot open codec, exiting.." << std::endl;
        exit(1);
    }
    
    /**
     * Create a stream
     */
    AVFormatContext *cv2avFormatContext = avformat_alloc_context();
    AVStream *h264outputstream = avformat_new_stream(cv2avFormatContext, h264encoder);
    
    /*
    int ret = avformat_write_header(cv2avFormatContext, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }*/
    
    int got_frame;
    
    FILE* videoOutFile = fopen(output.c_str(), "wb");
    if (videoOutFile == 0) {
        std::cerr << "Cannot open output video file at '" << output << "'" << std::endl;
        exit(1);
    }
    
    /**
     * Prepare the conversion context
     */
    
    SwsContext *bgr2yuvcontext = sws_getContext(width, height, sourcePixelFormat,
                                                width, height, destPixelFormat,
                                                SWS_BICUBIC, NULL, NULL, NULL);
    /**
     * Convert and encode frames
     */
    for (uint i=0; i < framesToEncode; i++) {
        std::cout << "i:" << i << std::endl;
        /**
         * Get frame from OpenCV
         */
        cv::Mat cvFrame;
        assert(videoCapturer.read(cvFrame) == true);
        imshow("video",cvFrame);
        cv::waitKey(30);
        
        /**
         * Allocate source frame, i.e. input to sws_scale()
         */
        
        std::cout <<"row:"<< cvFrame.rows << ",cols:" << cvFrame.cols << std::endl;
        
        AVFrame *sourceAvFrame = av_frame_alloc();
        if(sourceAvFrame == NULL)
        {
            std::cout << "can't allocate sourceAvFrame ..." << std::endl;
            return 0;
        }
        
        av_image_alloc(sourceAvFrame->data,
                       sourceAvFrame->linesize,
                       width, height, sourcePixelFormat, 1);
        
        AVFrame *destAvFrame = av_frame_alloc();
        
        /**
         * Allocate destination frame, i.e. output from sws_scale()
         */
        av_image_alloc(destAvFrame->data, destAvFrame->linesize, width, height, destPixelFormat, 1);
        
        
        /**
         * Copy image data into AVFrame from cv::Mat
         */
        for (uint32_t h = 0; h < height; h++)
            memcpy(&(sourceAvFrame->data[0][h*sourceAvFrame->linesize[0]]), &(cvFrame.data[h*cvFrame.step]), width*3);
        
        
        sws_scale(bgr2yuvcontext, sourceAvFrame->data, sourceAvFrame->linesize,
                  0, height,
                  destAvFrame->data,
                  destAvFrame->linesize);
       
        
        /**
         * Prepare an AVPacket and set buffer to NULL so that it'll be allocated by FFmpeg
         */
        AVPacket avEncodedPacket;
        av_init_packet(&avEncodedPacket);
        avEncodedPacket.data = NULL;
        avEncodedPacket.size = 0;
        
        destAvFrame->pts = i;
        avcodec_encode_video2(h264encoderContext, &avEncodedPacket, destAvFrame, &got_frame);
        
        if (got_frame) {
            saveFrameAsJPEG(destAvFrame, width, height, i);
            std::cerr << "Encoded a frame of size " << avEncodedPacket.size << ", writing it.." << std::endl;
            //av_write_frame(cv2avFormatContext, &avEncodedPacket);
            
            //if (fwrite(avEncodedPacket.data, 1, avEncodedPacket.size, videoOutFile) < (unsigned)avEncodedPacket.size)
            //    std::cerr << "Could not write all " << avEncodedPacket.size << " bytes, but will continue.." << std::endl;
            
            fflush(videoOutFile);
        }
        
        /**
         * Per-frame cleanup
         */
        av_packet_free_side_data(&avEncodedPacket);
        av_free_packet(&avEncodedPacket);
        
        av_freep(sourceAvFrame->data);
        av_frame_free(&sourceAvFrame);
        av_freep(destAvFrame->data);
        av_frame_free(&destAvFrame);
        
    }
    
    //av_write_trailer(cv2avFormatContext);
    
    fwrite(endcode, 1, sizeof(endcode), videoOutFile);
    fclose(videoOutFile);
    
    
    sws_freeContext(bgr2yuvcontext);
    /**
     * Final cleanup
     */
    avformat_free_context(cv2avFormatContext);
    avcodec_close(h264encoderContext);
    avcodec_free_context(&h264encoderContext);
    
    return 0;
}
