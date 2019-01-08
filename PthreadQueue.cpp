//
//  PthreadQueue.cpp
//  opencvdemo
//
//  Created by 王斌峰 on 2018/11/7.
//  Copyright © 2018年 Binfeng. All rights reserved.
//

#include "PthreadQueue.hpp"



PthreadPacketQueue::PthreadPacketQueue()
{
    nb_packets = 0;
    size       = 0;
    
    mutex      = new pthread_mutex_t;
    pthread_mutex_init(mutex,nullptr);
    cond       = new pthread_cond_t;
    pthread_cond_init(cond, nullptr);
}

PthreadPacketQueue::~PthreadPacketQueue(){
    if(mutex){
        pthread_mutex_destroy(mutex);
        delete mutex;
        mutex = nullptr;
    }
    if(cond){
        pthread_cond_destroy(cond);
        delete cond;
        cond = nullptr;
    }
}

bool PthreadPacketQueue::enQueue(const AVPacket *packet)
{
    AVPacket *pkt = av_packet_alloc();
    if (av_packet_ref(pkt, packet) < 0)
        return false;
    
    pthread_mutex_lock(mutex);
    queue.push(pkt);
    
    size += pkt->size;
    nb_packets++;
    pthread_cond_signal(cond);
    pthread_mutex_unlock(mutex);
    return true;
}

bool PthreadPacketQueue::deQueue(AVPacket *packet, bool block)
{
    bool ret = false;
    pthread_mutex_lock(mutex);
    
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
            pthread_cond_wait(cond,mutex);
        }
    }
    pthread_mutex_unlock(mutex);
    return ret;
}

void PthreadPacketQueue::flush(){
    pthread_mutex_lock(mutex);
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
    
    pthread_mutex_unlock(mutex);
}


PthreadFrameQueue::PthreadFrameQueue()
{
    nb_frames = 0;
    mutex      = new pthread_mutex_t;
    pthread_mutex_init(mutex,nullptr);
    cond       = new pthread_cond_t;
    pthread_cond_init(cond, nullptr);
}

PthreadFrameQueue::~PthreadFrameQueue(){
    if(mutex){
        pthread_mutex_destroy(mutex);
        delete mutex;
        mutex = nullptr;
    }
    if(cond){
        pthread_cond_destroy(cond);
        delete cond;
        cond = nullptr;
    }
}

bool PthreadFrameQueue::enQueue(const AVFrame* frame)
{
    fprintf(stdout,"begin,enqueue, frame queue size:%d\n",nb_frames);
    AVFrame* p = av_frame_alloc();
    
    int ret = av_frame_ref(p, frame);
    if (ret < 0)
        return false;
    if(p->opaque != NULL)
        p->opaque = (void *)new double(*(double*)p->opaque);
    pthread_mutex_lock(mutex);
    queue.push(p);
    nb_frames++;
    fprintf(stdout,"end enqueue, frame queue size:%d\n",nb_frames);
    pthread_cond_signal(cond);
    pthread_mutex_unlock(mutex);
    
    return true;
}

void PthreadFrameQueue::flush(){
    
    pthread_mutex_lock(mutex);
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
    
    pthread_mutex_unlock(mutex);
}
bool PthreadFrameQueue::deQueue(AVFrame *frame,bool block)
{
    bool ret = true;
    
    pthread_mutex_lock(mutex);
    while (true)
    {
        if (!queue.empty())
        {
            if (av_frame_ref(frame, queue.front()) < 0)
            {
                ret = false;
                break;
            }
            
            auto tmp = queue.front();
            queue.pop();
            av_frame_free(&tmp);
            nb_frames--;
            printf("dequeue, frame queue size:%d\n",nb_frames);
            ret = true;
            break;
        }
        else if(block)
        {
            printf("frame queue is empty, wait ....\n");
            pthread_cond_wait(cond, mutex);
        }else{
            return false;
        }
    }
    pthread_mutex_unlock(mutex);
    return ret;
}
