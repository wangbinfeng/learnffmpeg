//
//  audioengine.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/11.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include <GL/glew.h>
#include <GLFW/glfw3.h>


#include "audioengine.hpp"

#define CPP_TIME_BASE_Q (AVRational{1, AV_TIME_BASE})

AudioDecoder::AudioDecoder(int outSamplerate, int outChannels)
{
    m_pmtx = nullptr;
    m_pAudioCodec = nullptr;
    m_pFormatCtx = nullptr;
    m_pAudioCodecCtx = nullptr;
    static bool bFFMPEGInit = false;
    if (!bFFMPEGInit)
    {
       bFFMPEGInit = true;
    }
    m_outSampleRate = outSamplerate;
    m_outChs = outChannels;
    m_wholeDuration = -1;
    m_ch = -1;
    m_strPath = "";
    m_bStop = true;
    m_bSeeked = false;
}


AudioDecoder::~AudioDecoder()
{
    
}

int AudioDecoder::Open() {
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
    
    int ret = avcodec_open2(codec_ctx, dec, NULL);
    if (ret < 0) {
        fprintf(stderr, "Failed to open decoder \n");
        return ret;
    }
    return ret;
}

int AudioDecoder::OpenFile(const char* path)
{
    if (!m_bStop)
    {
        StopDecode();
    }
    m_strPath = path;
    AVFormatContext* pFormatCtx = nullptr;
    if (0 != avformat_open_input( &pFormatCtx, path, NULL, NULL))
    {
        return -1;
    }
    m_pFormatCtx = std::shared_ptr<AVFormatContext>(pFormatCtx, [](AVFormatContext* pf) { if (pf) avformat_close_input(&pf); });
    
    if (avformat_find_stream_info(m_pFormatCtx.get(), NULL) < 0)
    {
        return -1;
    }
    
    for (int i = 0; i < m_pFormatCtx->nb_streams; i++)
    {
        if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            m_nAudioIndex = i;
            break;
        }
    }
    
    if (m_nAudioIndex == -1)
    {
        return -1;
    }
    
    AVCodecContext* codec_ctx = avcodec_alloc_context3(NULL);
    if (codec_ctx == NULL)
    {
        printf("Could not allocate AVCodecContext\n");
        return -1;
    }
    avcodec_parameters_to_context(codec_ctx, m_pFormatCtx->streams[m_nAudioIndex]->codecpar);
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
    
    m_ch = codec_ctx->channels;
    m_wholeDuration = av_rescale_q(m_pFormatCtx->streams[m_nAudioIndex]->duration,
                                   m_pFormatCtx->streams[m_nAudioIndex]->time_base,
                                   CPP_TIME_BASE_Q);
    m_pAudioCodecCtx = codec_ctx;
    
    return 0;
}

int AudioDecoder::StartDecode()
{
    m_bStop = false;
    m_pmtx = std::make_shared<std::mutex>();
    m_pcond = std::make_shared<std::condition_variable>();
    m_pDecode = std::make_shared<std::thread>(&AudioDecoder::DecodeThread, this);
    
    return 0;
}

int AudioDecoder::StopDecode()
{
    m_bStop = true;
    m_pcond->notify_all();
    if(m_pDecode->joinable())
        m_pDecode->join();
    std::unique_lock<std::mutex> lck(*m_pmtx);
    
    int queueSize = m_queueData.size();
    
    for (int i = queueSize - 1; i >= 0; i--)
    {
        PTFRAME f = m_queueData.front();
        m_queueData.pop();
        if (f)
        {
            if (f->data)
                av_free(f->data);
            delete f;
        }
    }
    return 0;
}

PTFRAME AudioDecoder::GetFrame()
{
    PTFRAME frame = nullptr;
    std::unique_lock<std::mutex> lck(*m_pmtx);
    m_pcond->wait(lck,
                  [this]() {return m_bStop || m_queueData.size() > 0; });
    if (!m_bStop)
    {
        frame = m_queueData.front();
        m_queueData.pop();
    }
    lck.unlock();
    m_pcond->notify_one();
    return frame;
}

int AudioDecoder::add_frame(uint8_t* data, int size){
    if (m_strPath == nullptr)
        return -1;
    m_bDecoding = true;
   
    int64_t in_channel_layout = av_get_default_channel_layout(m_pAudioCodecCtx->channels);
    int64_t outLayout = AV_CH_LAYOUT_STEREO;
    
    if(au_convert_ctx == nullptr){
        au_convert_ctx = swr_alloc();
        au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
                                            outLayout, AV_SAMPLE_FMT_S16,
                                            m_outSampleRate, in_channel_layout,
                                            m_pAudioCodecCtx->sample_fmt, m_pAudioCodecCtx->sample_rate, 0,
                                            NULL);
        swr_init(au_convert_ctx);
    }
    
    PTFRAME frame = new TFRAME;
    frame->chs = m_outChs;
    frame->samplerate = m_outSampleRate;

    frame->data = data;
    frame->size = size;
    std::unique_lock<std::mutex> lck(*m_pmtx);
    m_pcond->wait(lck,
                  [this]() {return m_bStop || m_queueData.size() < MAX_BUFF_SIZE; });
    m_queueData.push(frame);
    lck.unlock();
    m_pcond->notify_one();
    
    return 0;
}

int AudioDecoder::DecodeThread()
{
    if (m_strPath == nullptr)
        return -1;
    m_bDecoding = true;
    AVPacket *packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    std::shared_ptr<AVFrame> pAudioFrame(av_frame_alloc(), [](AVFrame* pFrame) {av_frame_free(&pFrame);});
    int64_t in_channel_layout = av_get_default_channel_layout(m_pAudioCodecCtx->channels);
    struct SwrContext* au_convert_ctx = swr_alloc();
    int64_t outLayout;
    if (m_outChs == 1)
    {
        outLayout = AV_CH_LAYOUT_MONO;
    }
    else
    {
        outLayout = AV_CH_LAYOUT_STEREO;
    }
    
    au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
                                        outLayout, AV_SAMPLE_FMT_S16,
                                        m_outSampleRate, in_channel_layout,
                                        m_pAudioCodecCtx->sample_fmt, m_pAudioCodecCtx->sample_rate, 0,
                                        NULL);
    swr_init(au_convert_ctx);
    
    while (true)
    {
        int ret = av_read_frame(m_pFormatCtx.get(), packet);
        if (ret < 0)
            break;
        if (packet->stream_index == m_nAudioIndex)
        {
            int nAudioFinished = 0;
            int nRet = avcodec_decode_audio4(m_pAudioCodecCtx,
                                             pAudioFrame.get(),
                                             &nAudioFinished, packet);
            if (nRet > 0 && nAudioFinished != 0)
            {
                PTFRAME frame = new TFRAME;
                frame->chs = m_outChs;
                frame->samplerate = m_outSampleRate;
                frame->duration = av_rescale_q(packet->duration, m_pFormatCtx->streams[m_nAudioIndex]->time_base, CPP_TIME_BASE_Q);
                frame->pts = av_rescale_q(packet->pts, m_pFormatCtx->streams[m_nAudioIndex]->time_base, CPP_TIME_BASE_Q);

                //resample
                int outSizeCandidate = m_outSampleRate * 8 *
                double(frame->duration) / 1000000.0;
                uint8_t* convertData = (uint8_t*)av_malloc(sizeof(uint8_t) * outSizeCandidate);
                int out_samples = swr_convert(au_convert_ctx,
                                              &convertData, outSizeCandidate,
                                              (const uint8_t**)&pAudioFrame->data[0], pAudioFrame->nb_samples);
                int Audiobuffer_size = av_samples_get_buffer_size(NULL,
                                                                  m_outChs, out_samples,AV_SAMPLE_FMT_S16,1);
                frame->data = convertData;
                frame->size = Audiobuffer_size;
                std::unique_lock<std::mutex> lck(*m_pmtx);
                m_pcond->wait(lck,
                              [this]() {return m_bStop || m_queueData.size() < MAX_BUFF_SIZE; });
                if (m_bStop)
                {
                    av_packet_unref(packet);
                    break;
                }
                if (m_bSeeked)
                {
                    m_bSeeked = false;
                    av_packet_unref(packet);
                    continue;
                }
                m_queueData.push(frame);
                lck.unlock();
                m_pcond->notify_one();
            }
        }
        av_packet_unref(packet);
    }
    swr_free(&au_convert_ctx);
    av_packet_free(&packet);
    return 0;
}


//for seek
int AudioDecoder::InternalAudioSeek(uint64_t start_time_in_us)
{
    if (start_time_in_us < 0)
    {
        return -1;
    }
    int64_t seek_pos = start_time_in_us;
    if (m_pFormatCtx->start_time != AV_NOPTS_VALUE)
        seek_pos += m_pFormatCtx->start_time;
    if (av_seek_frame(m_pFormatCtx.get(), -1, seek_pos, AVSEEK_FLAG_BACKWARD) < 0)
    {
        return -2;
    }
    avcodec_flush_buffers(m_pAudioCodecCtx);
    return 0;
}

//设置seek
void AudioDecoder::SetSeekPosition(double playProcess)
{
    uint64_t pos = (uint64_t)(playProcess * (double)m_wholeDuration);
    std::unique_lock<std::mutex> lck(*m_pmtx);
    m_bSeeked = true;
    InternalAudioSeek(pos);
    int queueSize = m_queueData.size();
    for (int i = queueSize - 1; i >= 0; i--)
    {
        PTFRAME f = m_queueData.front();
        m_queueData.pop();
        if (f)
        {
            if (f->data)
                av_free(f->data);
            delete f;
        }
    }
    m_pcond->notify_all();//如果是从wait里面拿的锁，就要手动去激活他们
    return;
}



ALEngine::ALEngine()
{
    InitEngine();
    m_bStop = true;
}


ALEngine::~ALEngine()
{
    DestroyEngine();
    
}

int ALEngine::InitEngine()
{
    ALCdevice* pDevice;
    ALCcontext* pContext;
    
    pDevice = alcOpenDevice(NULL);
    pContext = alcCreateContext(pDevice, NULL);
    alcMakeContextCurrent(pContext);
    
    if (alcGetError(pDevice) != ALC_NO_ERROR)
        return AL_FALSE;
    
    const ALCchar *device = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
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

    alGenSources((ALuint)1, &m_source);
    if (alGetError() != AL_NO_ERROR)
    {
        printf("Error generating audio source.");
        return -1;
    }
    ALfloat SourcePos[] = { 0.0, 0.0, 0.0 };
    ALfloat SourceVel[] = { 0.0, 0.0, 0.0 };
    ALfloat ListenerPos[] = { 0.0, 0, 0 };
    ALfloat ListenerVel[] = { 0.0, 0.0, 0.0 };
    // first 3 elements are "at", second 3 are "up"
    
    ALfloat ListenerOri[] = { 0.0, 0.0, -1.0,  0.0, 1.0, 0.0 };
    
    alSourcef(m_source, AL_PITCH, 1.0);
    if (alGetError() != AL_NO_ERROR)
    {
        printf("Error generating audio source.");
        return -1;
    }
    alSourcef(m_source, AL_GAIN, 1.0);
    if (alGetError() != AL_NO_ERROR)
    {
        printf("Error generating audio source.");
        return -1;
    }
    alSourcefv(m_source, AL_POSITION, SourcePos);
    alSourcefv(m_source, AL_VELOCITY, SourceVel);
    alSourcef(m_source, AL_REFERENCE_DISTANCE, 50.0f);
    alSourcei(m_source, AL_LOOPING, AL_FALSE);
    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    alListener3f(AL_POSITION, 0, 0, 0);
    
    
    alGenBuffers(NUMBUFFERS, m_buffers);
    
    return 0;
}

int ALEngine::DestroyEngine()
{
    ALCcontext* pCurContext;
    ALCdevice* pCurDevice;
    
    pCurContext = alcGetCurrentContext();
    pCurDevice = alcGetContextsDevice(pCurContext);
    
    alcMakeContextCurrent(NULL);
    alcDestroyContext(pCurContext);
    alcCloseDevice(pCurDevice);
    return 0;
}

int ALEngine::Open() {
    if (!m_bStop)
    {
        Stop();
    }
    m_bStop = false;
    
    m_ptPlaying.reset(new std::thread(&ALEngine::SoundPlayingThread, this));
    
    return 0;
}

int ALEngine::OpenFile(const char* path)
{
    if (!m_bStop)
    {
        Stop();
    }
    m_bStop = false;
    
    m_decoder = std::make_unique<AudioDecoder>();
    m_decoder->OpenFile(path);
    m_decoder->StartDecode();
   
    m_ptPlaying.reset(new std::thread(&ALEngine::SoundPlayingThread, this));
   
    return 0;
}

int ALEngine::Play()
{
    int state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_STOPPED || state == AL_INITIAL)
    {
        alSourcePlay(m_source);
    }
    
    return 0;
}

int ALEngine::PausePlay()
{
    int state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PAUSED)
    {
        alSourcePlay(m_source);
    }
    
    return 0;
}

int ALEngine::Pause()
{
    alSourcePause(m_source);
    return 0;
}

int ALEngine::Stop()
{
    if(m_bStop)
        return 0;
    m_bStop = true;
    alSourceStop(m_source);
    alSourcei(m_source, AL_BUFFER, 0);
    m_decoder->StopDecode();//要先把decoder stop,否则可能hang住 播放线程
    if(m_ptPlaying->joinable())
        m_ptPlaying->join();
    
    alDeleteBuffers(NUMBUFFERS, m_buffers);
    alDeleteSources(1, &m_source);
    
    
    return 0;
}


int ALEngine::SoundPlayingThread()
{
    //get frame
    for (int i = 0; i < NUMBUFFERS; i++)
    {
        SoundCallback(m_buffers[i]);
    }
    Play();
    
    while (true)
    {
        if (m_bStop)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(SERVICE_UPDATE_PERIOD));
        ALint processed = 0;
        alGetSourcei(m_source, AL_BUFFERS_PROCESSED, &processed);
        //printf("the processed is:%d\n", processed);
        while (processed > 0)
        {
            ALuint bufferID = 0;
            alSourceUnqueueBuffers(m_source, 1, &bufferID);
            SoundCallback(bufferID);
            processed--;
        }
        Play();
    }
    
    return 0;
}

int ALEngine::SoundCallback(ALuint& bufferID)
{
    PTFRAME frame = nullptr;
    if(m_decoder)
        frame = m_decoder->GetFrame();
    
    if (frame == nullptr)
        return -1;
    
    ALenum fmt;
    if (frame->chs == 1)
    {
        fmt = AL_FORMAT_MONO16;
    }
    else
    {
        fmt = AL_FORMAT_STEREO16;
    }
    alBufferData(bufferID, fmt, frame->data, frame->size, frame->samplerate);
    update_sample_display((int16_t*)frame->data, frame->size);
    alSourceQueueBuffers(m_source, 1, &bufferID);
    if (frame)
    {
        av_free(frame->data);
        delete frame;
    }
    return 0;
}



void ALEngine::update_sample_display(int16_t *samples, int samples_size){
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

int ALEngine::display_audio_wave(){
    int rdft_bits;
    int height = 600;
    int i_start,x, y1, y, ys;
    int64_t time_diff;
    
    for (rdft_bits = 1; (1 << rdft_bits) < 2 * height; rdft_bits++)
        ;
    int nb_freq = 1 << (rdft_bits - 1);
    
    int channels = 1;//m_decoder->GetChs();
    int nb_display_channels = channels;
    
    int n = 2 * channels;
//    int delay = audio_write_buf_size;
//    delay /= n;
//
//    /* to be more precise, we take into account the time spent since
//     the last buffer computation */
//    if (audio_callback_time) {
//        time_diff = av_gettime_relative() - audio_callback_time;
//        delay -= (time_diff * audio_ctx->sample_rate) / 1000000;
//    }
//
//    int data_used = rect.w;
//    delay += 2 * data_used;
//    if (delay < data_used)
//        delay = data_used;
    
    int delay = 0;
    auto compute_mod=[](int a, int b)
    {
        return a < 0 ? a%b + b : a%b;
    };
    
    i_start= x = compute_mod(sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
    
    int h = INT_MIN;
    for (int i = 0; i < 1000; i += channels) {
        int idx = (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
        int a = sample_array[idx];
        int b = sample_array[(idx + 4 * channels) % SAMPLE_ARRAY_SIZE];
        int c = sample_array[(idx + 5 * channels) % SAMPLE_ARRAY_SIZE];
        int d = sample_array[(idx + 9 * channels) % SAMPLE_ARRAY_SIZE];
        int score = a - d;
        if (h < score && (b ^ c) < 0) {
            h = score;
            i_start = idx;
        }
    }
    
    last_i_start = i_start;
    
    h = height / nb_display_channels;
    /* graph height / 2 */
    int h2 = (h * 9) / 20;
    printf("audio wave:h:%d,h2%d:\n",h,h2);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (int ch = 0; ch < nb_display_channels; ch++) {
        int i = i_start + ch;
        y1 = 600 + ch * h + (h / 2); /* position of center line */
        //printf("audio wave:h:%d,y1:%d\n",h,y1);
        for (x = 0; x < 800; x++) {
            y = (sample_array[i] * h2) >> 15;
            if (y < 0) {
                y = -y;
                ys = y1 - y;
            } else {
                ys = y1;
            }
            
            printf("audio wave:h:%d,y1:%d,y:%d\n",h,y1,y);
            
            GLfloat xx = -1.0f;
            
            if(x < 400)
                xx = -1.0f + x /400.0f;
            else
                xx = (800 - x)/400.0f;
            
            GLfloat yy = 2.0f -  y/600.0f;
            
            printf("wave, xx=%f,yy=%f\n",xx,yy);
            
            GLfloat x;
            //glColor3f(xx,0.0,0.0) ;
            glBegin(GL_LINES);
            //glVertex2f(-1.0f, 0.0f);
            //glVertex2f(1.0f, 0.0f);
            glVertex2f(xx, -xx);
            glVertex2f(xx, xx);
            glEnd();
            
            glFlush();
            
           
            i += channels;
            if (i >= SAMPLE_ARRAY_SIZE)
                i -= SAMPLE_ARRAY_SIZE;
        }
    }
    
//    for (int ch = 1; ch < nb_display_channels; ch++) {
//        y = 600  + ch * h;
//        //fill_rectangle(renderer,0, y, rect.w, 1);
//    }
    
    return 0;
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

int ALEngine::StartPlay(const char *filename ){
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
    window = glfwCreateWindow(800, 600, "Audio Player", NULL, NULL);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    auto pe = std::make_unique<ALEngine>();
    pe->OpenFile(filename);
    
    glfwSetKeyCallback(window, key_callback);
    //glfwSetCharCallback(window, character_callback);
    //glfwSetCharModsCallback(window, charmods_callback);
    //glfwSetCursorPosCallback(window, cursor_position_callback);
    //glfwSetCursorEnterCallback(window, cursor_enter_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    
    /* Make the window's context current */
    glfwMakeContextCurrent(window);
    
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
    
    //glRotatef(-90,0.0f,0.0f,1.0f);
    /* Loop until the user closes the window */
    while (!glfwWindowShouldClose(window))
    {
        /* Render here */
        glClear(GL_COLOR_BUFFER_BIT);
        display_audio_wave();
        //display_wave();
        
        /* Swap front and back buffers */
        glfwSwapBuffers(window);
        /* Poll for and process events */
        glfwPollEvents();
    }
    
    Stop();
    
    glDeleteTextures(1,&texture_ID);
    glDisable(GL_TEXTURE_2D);
    
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
    
}

void ALEngine::display_wave(){
    const GLfloat factor = 0.1f;
    /*
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
    */
}

int main_al()
{
    auto pe = std::make_unique<ALEngine>();
    const char *filename = "/Users/wangbinfeng/dev/opencvdemo/xiyouji.mp3";
    pe->StartPlay(filename);
    return 0;
}

