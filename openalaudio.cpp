//
//  openalaudio.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/11.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include "openalaudio.hpp"
using namespace OpenAL;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
extern "C"{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/log.h>
    #include <libavutil/frame.h>
    #include <libavutil/mathematics.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
    #include <libavutil/samplefmt.h>
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h>
    #include <mpg123.h>
}


#include <GL/glew.h>
#include <GLFW/glfw3.h>


OpenALEngine::OpenALEngine(const char* fn):filename(fn){
    audio_buff = NULL;
    codec_ctx = NULL;
    ifmt_ctx = NULL;
    device = NULL;
    context = NULL;
    
}
OpenALEngine::~OpenALEngine(){
    
    if(audio_buff != NULL)
        free(audio_buff);
    
    if(codec_ctx){
        avcodec_free_context(&codec_ctx);
        codec_ctx = NULL;
    }
    if(ifmt_ctx){
        avformat_close_input(&ifmt_ctx);
        ifmt_ctx = NULL;
    }
    
    close();
}
void OpenALEngine::list_audio_devices(const ALCchar *devices)
{
    const ALCchar *device = devices, *next = devices + 1;
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
}

#define TEST_ERROR(_msg)        \
error = alGetError();        \
if (error != AL_NO_ERROR) {    \
fprintf(stderr, _msg "\n");    \
return -1;        \
}

 inline ALenum OpenALEngine::to_al_format(short channels, short samples)
{
    bool stereo = (channels > 1);
    
    switch (samples) {
        case 16:
            if (stereo)
                return AL_FORMAT_STEREO16;
            else
                return AL_FORMAT_MONO16;
        case 8:
            if (stereo)
                return AL_FORMAT_STEREO8;
            else
                return AL_FORMAT_MONO8;
        default:
            return -1;
    }
}

int OpenALEngine::display_audio_wave(){
    
    auto fill_rectangle = [](int leftX,int leftY,int rightX,int rightY){
        //画封闭曲线
        glBegin(GL_LINE_LOOP);
        //左下角
        glVertex2d(leftX,leftY);
        //右下角
        glVertex2d(rightX,leftY);
        //右上角
        glVertex2d(rightX,rightY);
        //左上角
        glVertex2d(leftX,rightY);
        //结束画线
        glEnd();
    };
    
    int h = 600/2;

        for (int x = 0; x < 800; x++) {
            fill_rectangle(x, h, x+1, h+sample_array[x]);
        }
    
    return 0;
}

void OpenALEngine::update_sample_display(int16_t *samples, int samples_size){
    int size, len;
    
    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - sample_array_index;
        if (len > size)
            len = size;
        memcpy(sample_array + sample_array_index, samples, len * sizeof(short));
        samples += len;
        sample_array_index += len;
        if (sample_array_index >= SAMPLE_ARRAY_SIZE)
            sample_array_index = 0;
        size -= len;
    }
}
int OpenALEngine::audio_decode_frame(AVPacket *pkt)
{
    AVFrame *frame = av_frame_alloc();
    int data_size = 0;
    SwrContext *swr_ctx = NULL;
    
    int ret = avcodec_send_packet(codec_ctx, pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF){
        fprintf(stderr,"fail to avcodec_send_packet\n");
        return -1;
    }
    
    ret = avcodec_receive_frame(codec_ctx, frame);
    if (ret < 0 && ret != AVERROR_EOF){
        fprintf(stderr,"fail to avcodec_receive_Frame\n");
        return -1;
    }
    
    if (frame->channels > 0 && frame->channel_layout == 0)
        frame->channel_layout = av_get_default_channel_layout(frame->channels);
    else if (frame->channels == 0 && frame->channel_layout > 0)
        frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
    
    enum AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;
    unsigned long long dst_layout = av_get_default_channel_layout(frame->channels);
    
    fprintf(stdout,"dst_layout:%lld,sampe_rate:%d\n",dst_layout,frame->sample_rate);
    swr_ctx = swr_alloc_set_opts(NULL, dst_layout, dst_format, frame->sample_rate,
                                 frame->channel_layout,
                                 (enum AVSampleFormat)frame->format,
                                 frame->sample_rate, 0, NULL);
    
    if (!swr_ctx || swr_init(swr_ctx) < 0){
        fprintf(stderr,"fail to set opts for swr\n");
        return -1;
    }
    
    enum AVRounding r = AV_ROUND_INF;
    uint64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx,frame->sample_rate) + frame->nb_samples,
                                             frame->sample_rate,
                                             frame->sample_rate,
                                             AV_ROUND_INF);
    printf("dst_nb_samples:%llu\n",dst_nb_samples);
    int nb = swr_convert(swr_ctx, &audio_buff,
                         (int)(dst_nb_samples),
                         (const uint8_t**)frame->data, frame->nb_samples);
    printf("nb:%d\n",nb);
    data_size = frame->channels * nb * av_get_bytes_per_sample(dst_format);
    
    av_frame_free(&frame);
    swr_free(&swr_ctx);
    printf("data_size:%d\n",data_size);
    return data_size;
}

int OpenALEngine::get_one_audio_frame() {
    int ret = -1;
    int audio_size = 0;
    enum AVMediaType type = AVMEDIA_TYPE_UNKNOWN;
    AVPacket *packet = av_packet_alloc();
    
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
                av_usleep(100*1000);
                continue;
            }
            else
                break;
        }
        
        int stream_index = packet->stream_index;
        type = ifmt_ctx->streams[packet->stream_index]->codecpar->codec_type;
        
        if(type == AVMEDIA_TYPE_AUDIO){
            av_packet_rescale_ts(packet,
                                 ifmt_ctx->streams[stream_index]->time_base,
                                 codec_ctx->time_base);
            
            audio_size =  audio_decode_frame(packet);
            if(audio_size > 0)
                update_sample_display((int16_t *)audio_buff, audio_size);
            printf("freq:%d,channels:%d,buffer_size:%d\n",
                   codec_ctx->sample_rate,codec_ctx->channels,audio_size);
            
        }else{
            printf("it's not video\n");
        }
        printf("audio_size:%d\n",audio_size);
        av_packet_unref(packet);
        
    }while(audio_size == 0);
    
    av_packet_free(&packet);
    
    
    return audio_size;
}


int OpenALEngine::open_play_all() {
    int ret = -1;
   
    if (0 != avformat_open_input( &ifmt_ctx, filename, NULL, NULL))
    {
        return -1;
    }
    
    if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
    {
        return -1;
    }
    
    int audio_index = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_index = i;
            break;
        }
    }
    
    if (audio_index == -1)
    {
        return -1;
    }
    
    codec_ctx = avcodec_alloc_context3(NULL);
    if (codec_ctx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return -1;
    }
    avcodec_parameters_to_context(codec_ctx, ifmt_ctx->streams[audio_index]->codecpar);
    AVCodec* pcodec = avcodec_find_decoder(codec_ctx->codec_id);
    if (pcodec == NULL)
    {
        printf("Video Codec not found.\n");
        return -1;
    }
    
    if (avcodec_open2(codec_ctx, pcodec, NULL) < 0)
    {
        printf("Could not open video codec.\n");
        return -1;
    }
    
    av_dump_format(ifmt_ctx, 0, filename, 0);
    printf("freq:%d,channels:%d\n",codec_ctx->sample_rate,codec_ctx->channels);
    
    unsigned int i;
    double pts = 0;
    
    uint32_t audio_buff_size = 0;
    audio_buff = (uint8_t*)malloc(codec_ctx->sample_rate);
    if(audio_buff == NULL)
        return -1;
    
    ALenum fmt;
    if (codec_ctx->channels == 1)
    {
        fmt = AL_FORMAT_MONO16;
    }
    else
    {
        fmt = AL_FORMAT_STEREO16;
    }
    
    int processed = 0;
    int total_processed = 0;
    int audio_size = 0;
    
    audio_size = get_one_audio_frame();
    
    if(audio_size <= 0)
        return -1;
    
    printf("=================\n");
    
    for(int i= 0; i < 4; i++)
    {
        alBufferData(m_buffers[i], fmt, audio_buff, audio_size, codec_ctx->sample_rate);
        alSourceQueueBuffers(source, 1, &m_buffers[i]);
    }
    alSourcePlay(source);
    ALCenum error = alGetError();
    if(error != AL_NO_ERROR)
        printf("buffer playing\n");
    else
        printf("no error for buffer playing\n");
    
    while (1) {
        processed = 0;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
        
        total_processed += processed;
        //printf("total Processed %d,processed=%d\r", total_processed,processed);
        
        while(processed)
        {
            ALuint bufferId = 0;
            alSourceUnqueueBuffers(source, 1, &bufferId);
            audio_size = get_one_audio_frame();
            printf("processed:%d,audio_size:%d\n",processed,audio_size);
            if(audio_size > 0){
                alBufferData(bufferId, fmt, audio_buff, audio_size, codec_ctx->sample_rate);
                alSourceQueueBuffers(source, 1, &bufferId);
            }
            //display_audio_wave();
            processed--;
            
            printf("processed %d\n", processed);
        }
        
        alGetSourcei(source, AL_SOURCE_STATE, &source_state);
        if(source_state != AL_PLAYING)
        {
            //printf("source state is not PLAYING\n");
            alGetSourcei(source, AL_BUFFERS_QUEUED, &source_state);
            //printf("source state is %d\n",source_state);
            if(source_state)
            {
                printf("source buffers have data, play it\n");
                alSourcePlay(source);
            }else
                goto end;
        }
        
    }
end:
    
    if (ret < 0)
        printf( "Error occurred: %s\n", av_err2str(ret));
    
    
    
    return 0;
}


int OpenALEngine::open_play_mp3() {
    ALint source_state;
    
    mpg123_handle *mpg123 ;
    int error;
    
    if (MPG123_OK != (error = mpg123_init())) {
        printf("failed to init mpg123\n");
        return -1;
    }
    
    mpg123 = mpg123_new(mpg123_decoders()[0], &error);
    
    if (MPG123_OK != (error = mpg123_open(mpg123,filename))) {
        fprintf(stderr,"error in open mp3 file\n");
        return -1;
    }
    
    long frequence;
    int iencoding, channels;
    int encoding;
    
    mpg123_getformat(mpg123, &frequence, &channels,  &iencoding);
    ALenum fmt;
    if (channels == 1)
    {
        fmt = AL_FORMAT_MONO16;
    }
    else
    {
        fmt = AL_FORMAT_STEREO16;
    }
    
    int buffer_size = frequence - (frequence % 4);
    
    printf("freq:%ld,channels:%d,buffer_size:%d\n",frequence,channels,buffer_size);
    
    char *bufferData = (char*) malloc(buffer_size);
    
    size_t len = 0;
    //feed data to openal buffer
    for(int i= 0; i < 4; i++)
    {
        mpg123_read(mpg123, (unsigned char *)bufferData, buffer_size, &len);
        alBufferData(m_buffers[i], fmt, bufferData, (int)len, frequence);
        alSourceQueueBuffers(source, 1, &m_buffers[i]);
    }
    
    alSourcePlay(source);
    ALCenum alerror = alGetError();
    if(alerror != AL_NO_ERROR)
        printf("buffer playing\n");
    
    int processed = 0;
    int total_processed = 0;
    
    printf("playing buffer_size:%d\n",buffer_size);
    
    while(1)
    {
        usleep(1000*1000);
        processed = 0;
        alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed);
        
        total_processed += processed;
        printf("total Processed %d\r", total_processed);
        while(processed)
        {
            ALuint bufferId = 0;
            alSourceUnqueueBuffers(source, 1, &bufferId);
            
            mpg123_read(mpg123, (unsigned char *)*bufferData, buffer_size, &len);
            if(len)
            {
                alBufferData(bufferId, fmt, bufferData, len, frequence);
                alSourceQueueBuffers(source, 1, &bufferId);
            }
            processed--;
            
        }
        alGetSourcei(source, AL_SOURCE_STATE, &source_state);
        if(source_state != AL_PLAYING)
        {
            alGetSourcei(source, AL_BUFFERS_QUEUED, &source_state);
            
            if(source_state)
            {
                alSourcePlay(source);//buffers have data, play it
            }
            else
            {
                //there is no data any more
                break;
            }
        }
    }
    free(bufferData);
    mpg123_close(mpg123);
    return 0;
}

int OpenALEngine::init()
{
    ALboolean enumeration;
    
    const ALCchar *defaultDeviceName = NULL;
    int ret = -1;
 
    ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
    ALboolean loop = AL_FALSE;
    ALCenum error;
    
    enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
    if (enumeration == AL_FALSE)
        fprintf(stderr, "enumeration extension not available\n");
    
    list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));
    
    if (!defaultDeviceName)
        defaultDeviceName = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    
    device = alcOpenDevice(defaultDeviceName);
    if (!device) {
        fprintf(stderr, "unable to open default device\n");
        return -1;
    }
    
    fprintf(stdout, "Device: %s\n", alcGetString(device, ALC_DEVICE_SPECIFIER));
    
    alGetError();
    
    context = alcCreateContext(device, NULL);
    if (!alcMakeContextCurrent(context)) {
        fprintf(stderr, "failed to make default context\n");
        return -1;
    }
    TEST_ERROR("make default context");
    
    /* set orientation */
    alListener3f(AL_POSITION, 0, 0, 1.0f);
    TEST_ERROR("listener position");
    alListener3f(AL_VELOCITY, 0, 0, 0);
    TEST_ERROR("listener velocity");
    alListenerfv(AL_ORIENTATION, listenerOri);
    TEST_ERROR("listener orientation");
    
    alGenSources((ALuint)1, &source);
    TEST_ERROR("source generation");
    
    alSourcef(source, AL_PITCH, 1);
    TEST_ERROR("source pitch");
    alSourcef(source, AL_GAIN, 1);
    TEST_ERROR("source gain");
    alSource3f(source, AL_POSITION, 0, 0, 0);
    TEST_ERROR("source position");
    alSource3f(source, AL_VELOCITY, 0, 0, 0);
    TEST_ERROR("source velocity");
    alSourcei(source, AL_LOOPING, AL_FALSE);
    TEST_ERROR("source looping");
    
    alGenBuffers(4,m_buffers);
    TEST_ERROR("buffer generation");
    return 0;
}

int OpenALEngine::close(){
    alSourceStop(source);
    alSourcei(source, AL_BUFFER, 0);
    
    alDeleteSources(1, &source);
    alDeleteBuffers(4, m_buffers);
    device = alcGetContextsDevice(context);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(device);
    
    return 0;
}



void OpenALEngine::start_play(){
    int ret = init();
    if(ret <0 )
        return;
    //open_play_all();
    
    int width = 800;
    int height = 600;
    GLFWwindow* window;
    
    glfwSetErrorCallback([](int error, const char* description)
    {
        fprintf(stderr, "Error: %s\n", description);
    });
    
    /* Initialize the library */
    if (!glfwInit())
        return ;
    
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    //glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(width, height, "Audio Player", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return ;
    }
    
    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        const char* key_name = glfwGetKeyName(key, 0);
        fprintf(stdout,"key pressed :%s\n",(key_name == NULL ? "" : key_name));
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
//        else if(key == 's' || key == 'S')
//            alSourceStop(source);
//        else if(key =='r' || key == 'R')
//            alSourcePlay(source);
    });
    //glfwSetCharCallback(window, character_callback);
    //glfwSetCharModsCallback(window, charmods_callback);
    //glfwSetCursorPosCallback(window, cursor_position_callback);
    //glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT ){
            fprintf(stdout,"mouse button right %s \n",
                    (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
        }else if (button == GLFW_MOUSE_BUTTON_LEFT){
            fprintf(stdout,"mouse button left %s \n",
                    (action == GLFW_PRESS ? "pressed" : (action == GLFW_RELEASE ? "release" : "" )));
        }
        
    });
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    
    glEnable(GL_TEXTURE_2D);
    
    GLuint texture_ID;
    glGenTextures(1, &texture_ID); // 分配一个纹理对象的编号
    glBindTexture(GL_TEXTURE_2D, texture_ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glViewport(0,0,width,height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    glOrtho(-width/2, width/2, -height/2, height/2, -400, 400);
    
    new std::thread(&OpenALEngine::open_play_all, this);
    glClearColor(1.0f,1.0f,1.0f,1.0f);
    glColor3f(0.5,0.5,0.5);
    auto fill_rectangle = [](int leftX,int leftY,int rightX,int rightY){
        //画封闭曲线
        glBegin(GL_LINE_LOOP);
        //左下角
        glVertex2d(leftX,leftY);
        //右下角
        glVertex2d(rightX,leftY);
        //右上角
        glVertex2d(rightX,rightY);
        //左上角
        glVertex2d(leftX,rightY);
        //结束画线
        glEnd();
    };
    
    //glRotatef(-90,0.0f,0.0f,1.0f);
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
        glColor3f(0.5,0.5,0.5);
        //display_audio_wave();
        //display_wave();
        fill_rectangle(-100,100,100,200);
        fill_rectangle(30,100,100,200);
        fill_rectangle(10,80,100,200);
        fill_rectangle(-20,10,100,50);
        glFlush();
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        /* Poll for and process events */
        glfwPollEvents();
    }
    
    glDeleteTextures(1,&texture_ID);
    glDisable(GL_TEXTURE_2D);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
}


int main_pp(int argc, char **argv)
{
    const char *filename = "/Users/wangbinfeng/dev/opencvdemo/xiyouji.mp3";
    //const char *filename = "/Users/wangbinfeng/dev/opencvdemo/tiantian_dance.mov";
    //const char *filename = "/Users/wangbinfeng/dev/opencvdemo/SampleVideo_320x240_5mb.3gp";
    OpenALEngine* al = new OpenALEngine(filename);
    al->start_play();
    delete al;
    return 0;
}
