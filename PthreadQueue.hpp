//
//  PthreadQueue.hpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/7.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#ifndef PthreadQueue_hpp
#define PthreadQueue_hpp


#include <queue>
#include <thread>

extern "C"{
#include "libavcodec/avcodec.h"
};

typedef uint32_t Uint32;

struct PthreadPacketQueue{
    std::queue<AVPacket*> queue;
    
    Uint32    nb_packets;
    Uint32    size;
    pthread_mutex_t *mutex;
    pthread_cond_t  *cond;
    
    PthreadPacketQueue();
    ~PthreadPacketQueue();
    bool enQueue(const AVPacket *packet);
    bool deQueue(AVPacket *packet, bool block);
    void flush();
};


struct PthreadFrameQueue
{
    static const int capacity = 30;
    
    std::queue<AVFrame*> queue;
    uint32_t nb_frames;
    
    pthread_mutex_t* mutex;
    pthread_cond_t * cond;
    
    PthreadFrameQueue();
    ~PthreadFrameQueue();
    bool enQueue(const AVFrame* frame);
    bool deQueue(AVFrame *frame,bool block);
    void flush();
};

#endif /* PthreadQueue_hpp */
