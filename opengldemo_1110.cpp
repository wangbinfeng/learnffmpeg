//
//  opengldemo.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/6.
//  Copyright © 2018年 Binfeng. All rights reserved.
//
#include <unistd.h>
#include <stdio.h>
#include <thread>
#include <pthread.h>
#include <setjmp.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

extern "C"{
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
};

#include "PacketQueue.h"

AVFormatContext *ifmt_ctx;
AVCodecContext *pVideoCodecCtx;
AVCodec *pVideoCodec;
std::queue<AVFrame*> frameq;
//std::mutex mutex;
pthread_mutex_t* mutex;
pthread_t tid;

static int open_input_file(const char *filename)
{
    pVideoCodecCtx = nullptr;
    pVideoCodec = nullptr;
    ifmt_ctx = nullptr;
    
    ifmt_ctx = avformat_alloc_context();
    
    if (avformat_open_input(&ifmt_ctx, filename, NULL, NULL) != 0)
    {
        printf("Couldn't open input stream.\n");
        return -1;
    }
    
    
    if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    
    
    int videoIndex = -1;
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
    av_dump_format(ifmt_ctx, 0, filename, 0);
   
    return 0;
}

static void* display_frames(void* arg){
 
    int ret = -1;
    const char *infileName = "/Users/wangbinfeng/dev/opencvdemo/tiantian.mov";
    
    AVPacket packet = { .data = NULL, .size = 0 };
    AVFrame *frame = NULL;
    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;
    int (*dec_func)(AVCodecContext *, AVFrame *, int *, const AVPacket *);
    SwsContext *convert_ctx = NULL;
    
    if ((ret = open_input_file(infileName)) >=0)
    {
        printf("open file sucessfully");
       
        ret = -1;
        while (1)
        {
            //printf( "read frames ...\n");
            if ((ret = av_read_frame(ifmt_ctx, &packet)) < 0)
                break;
            
            frame = av_frame_alloc();
            if (!frame) {
                frame = nullptr;
                break;
            }
            
            stream_index = packet.stream_index;
            type = ifmt_ctx->streams[packet.stream_index]->codecpar->codec_type;
            //printf("Demuxer gave frame of stream_index %u\n", stream_index);
            if(type == AVMEDIA_TYPE_VIDEO)
            {
                //printf("Going to reencode&filter the frame\n");
                
                av_packet_rescale_ts(&packet,
                                     ifmt_ctx->streams[stream_index]->time_base,
                                     pVideoCodecCtx->time_base);
                
                ret = avcodec_decode_video2(pVideoCodecCtx, frame,&got_frame, &packet);
                if (ret < 0) {
                    av_frame_free(&frame);
                    av_log(NULL, AV_LOG_ERROR, "Decoding failed\n");
                    break;
                }
                
                if (got_frame) {
                    frame->pts = frame->best_effort_timestamp;
                    int dstW = frame->width;
                    int dstH = frame->height;
                    printf("w:%d,h:%d\n",dstW,dstH);
                    
                    enum AVPixelFormat org_pix_fmts = static_cast<AVPixelFormat>(frame->format);
                    //enum AVPixelFormat dst_pix = AV_PIX_FMT_YUV420P;
                    enum AVPixelFormat dst_pix = AV_PIX_FMT_RGB24;
                    AVFrame* dst = av_frame_alloc();
                    av_image_alloc(dst->data,dst->linesize,dstW,dstH,dst_pix,1);
                    
                    
                    if(convert_ctx == nullptr){
                        
                        convert_ctx = sws_getContext(dstW, dstH,
                                                     org_pix_fmts,
                                                     dstW,
                                                     dstH,
                                                     dst_pix,
                                                     SWS_BICUBIC, NULL, NULL, NULL);
                    }
                    
                    if(convert_ctx == nullptr) {
                        fprintf(stderr, "Cannot initialize the conversion context!\n");
                        break;
                    }
                    //printf("sws_scale ....\n");
                    ret = sws_scale(convert_ctx, frame->data,
                                    frame->linesize,
                                    0, dstH,
                                    dst->data, dst->linesize);
                    if(ret < 0){
                        printf("fail to sws scale\n");
                        break;
                    }
                    dst->width = frame->width;
                    dst->height = frame->height;
                    
                    //mutex.lock();
                    pthread_mutex_lock(mutex);
                    printf("push to queue, send signal %dx%d\n",dst->width,dst->height);
                    frameq.push(dst);
                    pthread_mutex_unlock(mutex);
                    //av_frame_free(&dst);
                    av_frame_free(&frame);
                    if (ret < 0)
                        goto end;
                    usleep(300);
                } else {
                    av_frame_free(&frame);
                }
            }
            
            av_packet_unref(&packet);
        }
    }
end:
    av_packet_unref(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&pVideoCodecCtx);
    avformat_close_input(&ifmt_ctx);
    
    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    return NULL;
   
}



static int read_jpeg_file (const char * filename,uint8_t **data,int &width, int& height)
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
    typedef my_error_mgr* my_error_ptr;
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
    
    //typedef struct my_error_mgr * my_error_ptr;

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
    unsigned short depth=cinfo.output_components;
    
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



static void load_jpeg(const GLuint& texture_ID) {
    int    width=0,height=0;
    const char *filename = "/Users/wangbinfeng/dev/opencvdemo/frame_5.jpg";
    uint8_t* data = nullptr;
    int ret = -1;
    
    if( (ret = read_jpeg_file(filename,&data,width,height)) < 1)
        return ;
    
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    printf("%s, width=%d,height=%d\n",filename,width,height);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    
    delete []data;
    
}

static void save_as_jpeg(uint8_t *buffer, int width, int height)
{
    char fname[128] = { 0 };
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;
    //uint8_t *buffer;
    FILE *fp;
    static int iFrame = 0;
    if(iFrame > 100)
        return ;
    //buffer = pFrame->data[0];
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

static void myDisplay(){
    const GLfloat factor = 0.1f;
    
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.5,-0.5,0.0);
    glVertex3f(0.5,0.0,0.0);
    glVertex3f(0.0,0.5,0.0);
    glEnd();
    glFlush();
    
    GLfloat x;
    glClear(GL_COLOR_BUFFER_BIT);
    glBegin(GL_LINES);
    glVertex2f(-1.0f, 0.0f);
    glVertex2f(1.0f, 0.0f);         // 以上两个点可以画x轴
    glVertex2f(0.0f, -1.0f);
    glVertex2f(0.0f, 1.0f);         // 以上两个点可以画y轴
    glEnd();
    glBegin(GL_LINE_STRIP);
    for(x=-1.0f/factor; x<1.0f/factor; x+=0.01f)
    {
        glVertex2f(x*factor, sin(x)*factor);
    }
    glEnd();
    glFlush();
    
}

static void error_callback(int error, const char* description)
{
    fprintf(stderr, "Error: %s\n", description);
}
static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const char* key_name = glfwGetKeyName(key, 0);
    fprintf(stdout,"key pressed :%s\n",(key_name == NULL ? "" : key_name));
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_RIGHT ){
        fprintf(stdout,"mouse button right %s \n",
                (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
    }else if (button == GLFW_MOUSE_BUTTON_LEFT){
        fprintf(stdout,"mouse button left %s \n",
                (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
    }
        
}


static void character_callback(GLFWwindow* window, unsigned int codepoint)
{
    fprintf(stdout,"character:%d\n",codepoint);
}

static void charmods_callback(GLFWwindow* window, unsigned int codepoint, int mods)
{
    fprintf(stdout,"character:%d,mods:%d\n",codepoint,mods);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    fprintf(stdout,"mouse:%f,%f\n",xpos,ypos);
}

static void cursor_enter_callback(GLFWwindow* window, int entered)
{
    if (entered)
    {
        fprintf(stdout, " The cursor entered the client area of the window \n");
    }
    else
    {
        fprintf(stdout, " The cursor left the client area of the window\n");
    }
}

static void start_decode(){
    mutex      = new pthread_mutex_t;
    pthread_mutex_init(mutex,nullptr);
   
    pthread_attr_t attr_t;
    pthread_attr_init(&attr_t);
    void *arg = NULL;
    pthread_create(&tid,&attr_t,display_frames,arg);
}
static void end_decode(){
    //t.join();'
    
    pthread_join(tid,NULL);
    pthread_mutex_destroy(mutex);
    delete mutex;
    mutex = nullptr;
}

static void display_frame(const GLuint& texture_ID) {
    int width=0,height=0;
    
    AVFrame* frame = nullptr;
    //mutex.lock();
    pthread_mutex_lock(mutex);
    //display_frames();
    //printf("queue size:%d\n",frameq.size());
    if(!frameq.empty())
    {
        frame = frameq.front();
        frameq.pop();
    }
    //mutex.unlock();
    if(frame)
    {
        //save_as_jpeg(frame->data[0],frame->width,frame->height);
        printf("frame:%dx%d\n",frame->width,frame->height);
        if(width < 1 && frame->width > 1)
            width = frame->width;
        if(height < 1 && frame->height > 1)
            height = frame->height;
        
        uint8_t* data = frame->data[0];
        
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0,
                     GL_RGB, GL_UNSIGNED_BYTE, data);
        glBindTexture(GL_TEXTURE_2D, texture_ID);
        /*
         glDrawPixels(width,
         height,
         GL_RGB, GL_UNSIGNED_BYTE, frame->data[0]);
         */
                    glBegin(GL_QUADS);
                    glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
                    glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 0.0f);
                    glTexCoord2f(1.0f, 1.0f); glVertex3f(1.0f, 1.0f, 0.0f);
                    glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, -1.0f, 0.0f);
                    glEnd();
        
//        glBegin(GL_QUADS);
//        glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, -1.0f, 0.0f);
//        glTexCoord2f(1.0f, 1.0f); glVertex3f(-1.0f, 1.0f, 0.0f);
//        glTexCoord2f(1.0f, 0.0f); glVertex3f(1.0f, 1.0f, 0.0f);
//        glTexCoord2f(0.0f, 0.0f); glVertex3f(1.0f, -1.0f, 0.0f);
//        glEnd();
        
        av_frame_free(&frame);
    }
    pthread_mutex_unlock(mutex);
}


int main(void)
{
    GLFWwindow* window;
    
    glfwSetErrorCallback(error_callback);
    
    /* Initialize the library */
    if (!glfwInit())
        return -1;
    
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(640, 480, "Hello World", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    
    glfwSetKeyCallback(window, key_callback);
    //glfwSetCharCallback(window, character_callback);
    //glfwSetCharModsCallback(window, charmods_callback);
    //glfwSetCursorPosCallback(window, cursor_position_callback);
    //glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    
    start_decode();
    
    glEnable(GL_TEXTURE_2D);
   
    //std::thread t(display_frames);
    GLuint texture_ID;
    glGenTextures(1, &texture_ID); // 分配一个纹理对象的编号
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    
    //load_jpeg(texture_ID);
    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    //gluPerspective(75, 1, 1, 21);
    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();
    //gluLookAt(1, 5, 5, 0, 0, 0, 0, 0, 1);

    glRotatef(-90,0.0f,0.0f,1.0f);
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
        //myDisplay();
        display_frame(texture_ID);
        
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(1.0f, -1.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(-1.0f, 1.0f);
        glEnd();
        
        
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        /* Poll for and process events */
        glfwPollEvents();
    }
    glDeleteTextures(1,&texture_ID);
    glDisable(GL_TEXTURE_2D);
    
    glfwDestroyWindow(window);
    glfwTerminate();
   
    end_decode();
    
    return 0;
}
