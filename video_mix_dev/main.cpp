// rtmp://live.mudu.tv/watch/fp6r8x rtmp://pub.mudu.tv/watch/hm1s6n

#include <thread>
#include <memory>

#include <string.h>
#include <stdio.h>

#include "demux_decode.h"
#include "encode_mux.h"
#include "safequeue.h"
#include "filter.h"
#include "broadcastingstation.h"

void concatyuv420P_test(AVFrame* dstFrame, AVFrame* overlayFrame, OverlayConfig& c)
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
    std::thread reapThread(std::mem_fn(&BroadcastingStation::reapFrames),s);

    //s->addInputFile("rtmp://live.mudu.tv/watch/134w50", 0);
    s->addInputFile("rtmp://live.mudu.tv/watch/8ong6e", 0);
    s->inputs[0]->layoutConf.overlayConf = {0.5, 480, 320, 0, 0};
    s->openInputFile(0);
    //s->addInputFile("rtmp://live.mudu.tv/watch/134w50",1);
    s->addInputFile("a.mp4", 1);
    s->inputs[1]->layoutConf.overlayConf = {0.5, 400, 400, 200, 200};
    s->openInputFile(1);
    //
    s->addInputFile("/home/huheng/Videos/samplemedia/SampleVideo_1280x720_1mb.mp4", 2);
    //s->addInputFile("a.mp4", 2);
    s->inputs[2]->layoutConf.overlayConf = {0.9, 600, 400, 600, 0};
    s->openInputFile(2);

    std::thread outputThread(std::mem_fn(&BroadcastingStation::streamingOut), s);
    s->startStreaming();

    //manage the BroadcastingStation accordding to event
    int tt = 0;
    while(1){
        av_usleep(1000000);
        //config
        //if(tt>1000)
          //  continue;
        OverlayConfig c = {(tt%1000)/1000.0, 100 + tt%500,100+tt%300,tt%1000,tt%700};
        s->setOverlayConfig(c, 1);
        OverlayConfig c2 = {0.9, 600,400, 600, tt%600};
        s->setOverlayConfig(c2, 2);
        tt += 100;
    }

    //thread join
    outputThread.join();
    reapThread.join();
    delete s;
    return 0;
}
