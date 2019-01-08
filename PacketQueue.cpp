
#include "PacketQueue.h"
#include <iostream>

extern bool quit;



PacketQueue::PacketQueue()
{
	nb_packets = 0;
	size       = 0;

	mutex      = SDL_CreateMutex();
	cond       = SDL_CreateCond();
}

PacketQueue::~PacketQueue(){
    if(mutex){
        SDL_DestroyMutex(mutex);
        mutex = nullptr;
    }
    if(cond){
        SDL_DestroyCond(cond);
        cond = nullptr;
    }
}

bool PacketQueue::enQueue(const AVPacket *packet)
{
	AVPacket *pkt = av_packet_alloc();
	if (av_packet_ref(pkt, packet) < 0)
		return false;

	SDL_LockMutex(mutex);
	queue.push(pkt);

	size += pkt->size;
	nb_packets++;

	SDL_CondSignal(cond);
	SDL_UnlockMutex(mutex);
    //fprintf(stdout, "packet queue, %d\n",nb_packets);
	return true;
}

bool PacketQueue::deQueue(AVPacket *packet, bool block)
{
	bool ret = false;

	SDL_LockMutex(mutex);
	while (true)
	{
        if (!queue.empty())
		{
			if (av_packet_ref(packet, queue.front()) < 0)
			{
				ret = false;
				break;
			}
			AVPacket* pkt = queue.front();

			queue.pop();
			av_packet_unref(pkt);
			nb_packets--;
			size -= packet->size;

			ret = true;
			break;
		}
		else if (!block)
		{
			ret = false;
			break;
		}
		else
		{
            printf("packet queue is empty, wait ....\n");
			SDL_CondWait(cond, mutex);
		}
	}
	SDL_UnlockMutex(mutex);
	return ret;
}

void PacketQueue::flush(){
    SDL_LockMutex(mutex);
    while (true)
    {
        if (!queue.empty())
        {
            AVPacket* pkt = queue.front();
            queue.pop();
            av_packet_unref(pkt);
            nb_packets--;
        }else
            break;
    }
    
    SDL_UnlockMutex(mutex);
}


FrameQueue::FrameQueue()
{
    nb_frames = 0;
    
    mutex     = SDL_CreateMutex();
    cond      = SDL_CreateCond();
}

FrameQueue::~FrameQueue(){
    SDL_DestroyMutex(mutex);
    SDL_DestroyCond(cond);
}

bool FrameQueue::enQueue(const AVFrame* frame)
{
    AVFrame* p = av_frame_alloc();
    fprintf(stdout,"begin, nb_frames:%d\n",nb_frames);
    int ret = av_frame_ref(p, frame);
    if (ret < 0){
        fprintf(stdout,"av_frame_ref, nb_frames:%d\n",nb_frames);
        return false;
    }
    
    //p->opaque = (void *)new double(*(double*)p->opaque);
    fprintf(stdout,"mid, nb_frames:%d\n",nb_frames);
    SDL_LockMutex(mutex);
    queue.push(p);
    
    nb_frames++;
    
    SDL_CondSignal(cond);
    fprintf(stdout,"end,nb_frames:%d\n",nb_frames);
    SDL_UnlockMutex(mutex);
    
    return true;
}

void FrameQueue::flush(){
    SDL_LockMutex(mutex);
    while (true)
    {
        if (!queue.empty())
        {
            auto tmp = queue.front();
            queue.pop();
            av_frame_free(&tmp);
            nb_frames--;
        }else
            break;
    }
    
    SDL_UnlockMutex(mutex);
}
bool FrameQueue::deQueue(AVFrame *frame,bool block)
{
    bool ret = true;
    
    SDL_LockMutex(mutex);
    while (true)
    {
        if (!queue.empty())
        {
            printf("frame queue is not empty, wait ....\n");
            if (av_frame_ref(frame, queue.front()) < 0)
            {
                printf("frame queue is av_frame_ref ..\n");
                ret = false;
                break;
            }
            
            auto tmp = queue.front();
            queue.pop();
            av_frame_free(&tmp);
            nb_frames--;
            ret = true;
            break;
        }else if (!block)
        {
            ret = false;
            break;
        }
        else
        {
            printf("frame queue is empty, wait ....\n");
            SDL_CondWait(cond, mutex);
        }
    }
    
    SDL_UnlockMutex(mutex);
    return ret;
}
