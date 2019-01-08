//
//  openalaudio.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/11.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef openalaudio_hpp
#define openalaudio_hpp

#include <atomic>
#include <OpenAL/al.h>
#include <OpenAl/alc.h>

#include "PacketQueue.h"
extern "C"{
    #include <libavformat/avformat.h>
    #include "libavutil/time.h"
    #include <libswresample/swresample.h>
}
#define SAMPLE_ARRAY_SIZE (8 * 65536)

namespace OpenAL {
    
class OpenALEngine
{
    
private:
    ALuint source;
    ALCcontext *context;
    ALuint m_buffers[4];
   
    ALCdevice *device;
    
    AVFormatContext* ifmt_ctx ;
    AVCodecContext* codec_ctx ;
    
    ALint source_state;
    uint8_t *audio_buff;
    
    const char* filename;
    
    
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    int audio_write_buf_size;
    
public:
    OpenALEngine(const char* fn);
    ~OpenALEngine();
    void start_play();

private:
    void list_audio_devices(const ALCchar *devices);
    inline ALenum to_al_format(short channels, short samples);
    int audio_decode_frame(AVPacket *pkt);
    int get_one_audio_frame() ;
    int open_play_all();
    int open_play_mp3();
    int init();
    int close();
    
    void update_sample_display(int16_t *samples, int samples_size);
    int display_audio_wave();
};
}

#endif /* openalaudio_hpp */
