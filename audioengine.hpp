//
//  audioengine.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/11.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef audioengine_hpp
#define audioengine_hpp

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
}
#include <string>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>


#include <OpenAL/al.h>
#include <OpenAl/alc.h>


typedef struct _tFrame
{
    void* data;
    int size;
    int chs;
    int samplerate;
    uint64_t pts;
    uint64_t duration;
}TFRAME, *PTFRAME;

class AudioDecoder
{
public:
    AudioDecoder(int outSamplerate = 44100, int outChannels = 2);
    ~AudioDecoder();
    int Open();
    int OpenFile(const char* path);
    int GetChs() { return m_ch; };
    int GetSampleRate() { return m_sampleRate; };
    uint64_t GetWholeDuration() { return m_wholeDuration; };//单位微秒
    int StartDecode();
    int StopDecode();
    PTFRAME GetFrame();
    void SetSeekPosition(double playProcess);
    int add_frame(uint8_t* data, int size);
    
private:
    int DecodeThread();
    int InternalAudioSeek(uint64_t start_time);
    
    
private:
    int m_ch;
    int m_sampleRate;
    uint64_t m_wholeDuration;
    std::shared_ptr<AVFormatContext> m_pFormatCtx;
    AVCodecContext* m_pAudioCodecCtx;
    AVCodec* m_pAudioCodec;
    struct SwrContext* au_convert_ctx = nullptr;
    
    int m_nAudioIndex;
    const char* m_strPath;
    std::queue<PTFRAME> m_queueData;
    std::atomic_bool m_bDecoding;
    static const int MAX_BUFF_SIZE = 128;
    std::shared_ptr<std::mutex> m_pmtx;
    std::shared_ptr<std::condition_variable> m_pcond;
    std::shared_ptr<std::thread> m_pDecode;
    
    std::atomic_bool m_bStop;
    std::atomic_bool m_bSeeked;
    
    int m_outSampleRate;
    int m_outChs;
};


#define NUMBUFFERS              (4)
#define    SERVICE_UPDATE_PERIOD    (20)
#define SAMPLE_ARRAY_SIZE (8 * 65536)

class ALEngine
{
public:
    ALEngine();
    ~ALEngine();
    
    int Play();
    int PausePlay();
    int Pause();
    int Stop();
    int StartPlay(const char* path);
    
private:
    ALuint            m_source;
    std::unique_ptr<AudioDecoder> m_decoder;
    std::unique_ptr<std::thread> m_ptPlaying;
    
    std::atomic_bool m_bStop;
    
    ALuint m_buffers[NUMBUFFERS];
    ALuint m_bufferTemp;
    
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
private:
    int SoundPlayingThread();
    int SoundCallback(ALuint& bufferID);
    int InitEngine();
    int DestroyEngine();
    void display_wave();
    int OpenFile(const char* path);
    int Open();
    void update_sample_display(int16_t *samples, int samples_size);
    int display_audio_wave();
};

#endif /* audioengine_hpp */
