
#ifndef PACKET_QUEUE_H
#define PACKET_QUEUE_H

#include <queue>
#include <thread>

extern "C"{

#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
#include "libavcodec/avcodec.h"
};

template<typename T>
class AVQueue{
public:
    
    int enQueue(const T *pkt) ;
    int deQueue(T* pkt,bool block) ;
    bool is_queue_empty();
     ~AVQueue();
};


struct PacketQueue
{
	std::queue<AVPacket*> queue;

	Uint32    nb_packets;
	Uint32    size;
	SDL_mutex *mutex;
	SDL_cond  *cond;

	PacketQueue();
    ~PacketQueue();
	bool enQueue(const AVPacket *packet);
	bool deQueue(AVPacket *packet, bool block);
    void flush();
};


struct FrameQueue
{
    static const int capacity = 30;
    
    std::queue<AVFrame*> queue;
    uint32_t nb_frames;
    
    SDL_mutex* mutex;
    SDL_cond * cond;
    
    FrameQueue();
    ~FrameQueue();
    bool enQueue(const AVFrame* frame);
    bool deQueue(AVFrame *frame,bool block);
    void flush();
};


#endif
