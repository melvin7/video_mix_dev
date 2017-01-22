// rtmp://live.mudu.tv/watch/fp6r8x rtmp://pub.mudu.tv/watch/hm1s6n

#include <thread>
#include <memory>

#include <stdio.h>

#include "demux_decode.h"
#include "encode_mux.h"
#include "safequeue.h"
#include "filter.h"
#include "broadcastingstation.h"
#include <string.h>

void concatyuv420P_test(AVFrame* dstFrame, AVFrame* overlayFrame)
{
    //for test
    if(dstFrame->width != 2 * overlayFrame->width || dstFrame->height != 2 * overlayFrame->height){
        printf("assert failed!\n");
        return;
    }
    int height = dstFrame->height;
    memset(dstFrame->data[0], 0x80, height * dstFrame->linesize[0]);
    memset(dstFrame->data[1], 0x80, height/2 * dstFrame->linesize[1]);
    memset(dstFrame->data[2], 0x80, height/2 * dstFrame->linesize[2]);
    for(int i = 0; i < height/2; ++i){
        memcpy(dstFrame->data[0] + i * dstFrame->linesize[0], overlayFrame->data[0] + i * overlayFrame->linesize[0], overlayFrame->linesize[0]);
    }
    for(int i = 0; i < height/4; ++i){
        memcpy(dstFrame->data[1] + i * dstFrame->linesize[1], overlayFrame->data[1] + i * overlayFrame->linesize[1], overlayFrame->linesize[1]);
        memcpy(dstFrame->data[2] + i * dstFrame->linesize[2], overlayFrame->data[2] + i * overlayFrame->linesize[2], overlayFrame->linesize[2]);
    }
}
int64_t g_start_time;
int main(int argc, char** argv)
{
    av_register_all();
    avfilter_register_all();
    avformat_network_init();
    g_start_time = av_gettime_relative();
    //1 create BroadcastingStation
    BroadcastingStation* s = new BroadcastingStation();

    //4.
    s->addOutputFile(argv[1], 0);
    s->startStreaming();
    std::thread reapThread(std::mem_fn(&BroadcastingStation::reapFrames),s);

    //s->addInputFile("rtmp://live.mudu.tv/watch/134w50", 0);
    s->addInputFile("rtmp://live.mudu.tv/watch/8ong6e", 0);
    s->layout.overlayMap[0].overlay_w = 320;
    s->layout.overlayMap[0].overlay_h = 240;
    s->openInputFile(0);
    //s->addInputFile("rtmp://live.mudu.tv/watch/134w50",1);
    s->addInputFile("a.mp4", 1);
    s->layout.overlayMap[1] = {1.0, 640, 360, 400, 400};
    s->openInputFile(1);
    //
    //s->addInputFile("/home/huheng/Videos/samplemedia/SampleVideo_1280x720_1mb.mp4", 2);
    s->addInputFile("a.mp4", 2);
    s->layout.overlayMap[2] = {1.0, 320, 180, 200, 200};
    s->openInputFile(2);

    std::thread outputThread(std::mem_fn(&BroadcastingStation::streamingOut), s);
    //manage the BroadcastingStation accordding to event
    int tt = 0;
    while(1){
        av_usleep(1000000);
        s->layout.overlayMap[0] = {0.5,320,240,0,(tt) % 500};
        tt += 100;
    }

    //thread join
    outputThread.join();
    reapThread.join();
    delete s;
    return 0;
}
