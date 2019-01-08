
#include "Audio.h"

extern "C" {
#include <libswresample/swresample.h>
}


static void audio_callback(void* userdata, Uint8 *stream, int len);
static int audio_decode_frame(AudioState *audio_state, uint8_t *audio_buf, int buf_size);
static void update_sample_display(AudioState *audio_state, int16_t *audio_buf, int buf_size);

AudioState::AudioState()
	:BUFFER_SIZE(192000)
{
	audio_ctx = nullptr;
	stream_index = -1;
	stream = nullptr;
	audio_clock = 0;

	audio_buff = new uint8_t[BUFFER_SIZE];
	audio_buff_size = 0;
	audio_buff_index = 0;
    quit = false;
    eof = false;
    paused = false;
}

AudioState::AudioState(AVCodecContext *audioCtx, int index)
	:BUFFER_SIZE(192000)
{
	audio_ctx = audioCtx;
	stream_index = index;
    
	audio_buff = new uint8_t[BUFFER_SIZE];
	audio_buff_size = 0;
	audio_buff_index = 0;
}

AudioState::~AudioState()
{
    audioq.flush();
    if (audio_buff){
        memset(audio_buff, 0, audio_buff_size);
		delete []audio_buff;
        audio_buff = nullptr;
    }
}

bool AudioState::start()
{
    //Out Audio Param
    uint64_t out_channel_layout=AV_CH_LAYOUT_STEREO;
    //nb_samples: AAC-1024 MP3-1152
    int out_nb_samples=audio_ctx->frame_size;
    AVSampleFormat out_sample_fmt=AV_SAMPLE_FMT_S16;
    int out_sample_rate=audio_ctx->sample_rate;//44100;
    
    SDL_AudioSpec wanted_spec;
    wanted_spec.freq = audio_ctx->sample_rate;
    wanted_spec.format = AUDIO_S16SYS;
    wanted_spec.channels = audio_ctx->channels;
    wanted_spec.silence = 0;
    wanted_spec.samples = out_nb_samples;
    wanted_spec.callback = audio_callback;
    wanted_spec.userdata = this;
    
    if (SDL_OpenAudio(&wanted_spec, NULL)<0){
        printf("can't open audio.\n");
        return -1;
    }
    av_log(NULL,AV_LOG_INFO,"SDL audio is initilized sucessfully \n");
    int64_t in_channel_layout=av_get_default_channel_layout(audio_ctx->channels);
    
    struct SwrContext *au_convert_ctx = swr_alloc();
    au_convert_ctx=swr_alloc_set_opts(au_convert_ctx,
                                      out_channel_layout,
                                      out_sample_fmt,
                                      out_sample_rate,
                                      in_channel_layout,
                                      audio_ctx->sample_fmt ,
                                      audio_ctx->sample_rate,0,
                                      NULL);
    swr_init(au_convert_ctx);
    
    SDL_PauseAudio(0);
    
    return true;
}

double AudioState::get_audio_clock()
{
	int hw_buf_size = audio_buff_size - audio_buff_index;
	int bytes_per_sec = stream->codecpar->sample_rate * audio_ctx->channels * 2;
	double pts = audio_clock - static_cast<double>(hw_buf_size) / bytes_per_sec;
	return pts;
}

void audio_callback(void* userdata, Uint8 *stream, int len)
{
	AudioState *audio_state = (AudioState*)userdata;
    
    audio_state->audio_callback_time = av_gettime_relative();
	SDL_memset(stream, 0, len);

	int audio_size = 0;
	int len1 = 0;
	while (len > 0)
	{
		if (audio_state->audio_buff_index >= audio_state->audio_buff_size)
		{
			audio_size = audio_decode_frame(audio_state, audio_state->audio_buff, sizeof(audio_state->audio_buff));
			if (audio_size < 0)
			{
                if(audio_state->eof)
                {
                    SDL_AudioQuit();
                    return ;
                }
				audio_state->audio_buff_size = 0;
				memset(audio_state->audio_buff, 0, audio_state->audio_buff_size);
			}
            else{
                update_sample_display(audio_state, (int16_t *)audio_state->audio_buff, audio_size);
                audio_state->audio_buff_size = audio_size;
            }

			audio_state->audio_buff_index = 0;
		}
		len1 = audio_state->audio_buff_size - audio_state->audio_buff_index;
		if (len1 > len)
			len1 = len;

		SDL_MixAudio(stream, audio_state->audio_buff + audio_state->audio_buff_index, len, SDL_MIX_MAXVOLUME);

		len -= len1;
		stream += len1;
		audio_state->audio_buff_index += len1;
        audio_state->audio_write_buf_size = audio_state->audio_buff_size - audio_state->audio_buff_index;
	}
}

void update_sample_display(AudioState *audio_state, int16_t *samples, int samples_size){
    int size, len;
    
    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - audio_state->sample_array_index;
        if (len > size)
            len = size;
        memcpy(audio_state->sample_array + audio_state->sample_array_index, samples, len * sizeof(short));
        samples += len;
        audio_state->sample_array_index += len;
        if (audio_state->sample_array_index >= SAMPLE_ARRAY_SIZE)
            audio_state->sample_array_index = 0;
        size -= len;
    }
}

int audio_decode_frame(AudioState *audio_state, uint8_t *audio_buf, int buf_size)
{
	AVFrame *frame = av_frame_alloc();
	int data_size = 0;
	AVPacket pkt;
	SwrContext *swr_ctx = nullptr;
	//static double clock = 0;
	
    //printf("audio decode %d\n",audio_state->audioq.queue.size());
	if (audio_state->quit || audio_state->paused)
		return -1;
    if(audio_state->audioq.queue.empty() && audio_state->eof )
        return -1;
    
	if (!audio_state->audioq.deQueue(&pkt, true))
		return -1;

	if (pkt.pts != AV_NOPTS_VALUE)
	{
		audio_state->audio_clock = av_q2d(audio_state->stream->time_base) * pkt.pts;
	}
	int ret = avcodec_send_packet(audio_state->audio_ctx, &pkt);
    if(ret == AVERROR_EOF){
        audio_state->eof = true;
        audio_state->quit = true;
    }
	if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
		return -1;

	ret = avcodec_receive_frame(audio_state->audio_ctx, frame);
    if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
        audio_state->eof = true;
        audio_state->quit = true;
    }
	if (ret < 0 && ret != AVERROR_EOF)
		return -1;
   
    
	if (frame->channels > 0 && frame->channel_layout == 0)
		frame->channel_layout = av_get_default_channel_layout(frame->channels);
	else if (frame->channels == 0 && frame->channel_layout > 0)
		frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);

	AVSampleFormat dst_format = AV_SAMPLE_FMT_S16;
	Uint64 dst_layout = av_get_default_channel_layout(frame->channels);
	
    swr_ctx = swr_alloc_set_opts(nullptr, dst_layout, dst_format, frame->sample_rate,
		frame->channel_layout, (AVSampleFormat)frame->format, frame->sample_rate, 0, nullptr);
	if (!swr_ctx || swr_init(swr_ctx) < 0)
		return -1;

	uint64_t dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, frame->sample_rate) + frame->nb_samples,
                                             frame->sample_rate, frame->sample_rate,
                                             AVRounding(1));
	
	int nb = swr_convert(swr_ctx, &audio_buf, static_cast<int>(dst_nb_samples), (const uint8_t**)frame->data, frame->nb_samples);
	data_size = frame->channels * nb * av_get_bytes_per_sample(dst_format);

	audio_state->audio_clock += static_cast<double>(data_size) / (2 * audio_state->stream->codecpar->channels * audio_state->stream->codecpar->sample_rate);

	av_frame_free(&frame);
	swr_free(&swr_ctx);

	return data_size;
}

