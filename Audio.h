
#ifndef AUDIO_H
#define AUDIO_H

#include "PacketQueue.h"
extern "C"{
    #include <libavformat/avformat.h>
    #include "libavutil/time.h"
    #include <libswresample/swresample.h>
}

#define SAMPLE_ARRAY_SIZE (8 * 65536)

struct AudioState
{
	const uint32_t BUFFER_SIZE;

	PacketQueue audioq;

	double audio_clock; 
	AVStream *stream;

	uint8_t *audio_buff;
	uint32_t audio_buff_size;
	uint32_t audio_buff_index;
	
	int stream_index;
	AVCodecContext *audio_ctx;
    
    bool quit;
    bool eof;
    bool paused;
    
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    int audio_write_buf_size;
    int64_t audio_callback_time;
    
	AudioState();
	AudioState(AVCodecContext *audio_ctx, int audio_stream);
	~AudioState();
	bool start();
	double get_audio_clock();
};

#endif
