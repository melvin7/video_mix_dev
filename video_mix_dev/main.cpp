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

#define STREAM_DURATION   10000.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

const int output_width = 1280;
const int output_height = 720;
const int output_format = 0;
AVRational output_timebase = (AVRational){1, STREAM_FRAME_RATE};

SafeQueue<std::shared_ptr<Frame>, 30> videoFrameQ;

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

//the global value should be members of broadcasting station

SafeQueue<std::shared_ptr<Frame>,10> stationOutputVideoQ;
static bool streaming = true;

int write_one_video_frame(BroadcastingStation* bs, AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };

    c = ost->enc;
    std::shared_ptr<Frame> sharedFrame;
    bs->outputVideoQ.pop(sharedFrame);
    //frame = get_video_frame(ost);
    //send to filter
    frame = sharedFrame->frame;
    frame->pts = ost->next_pts++;
    av_init_packet(&pkt);
    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        exit(1);
    }
    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost, &pkt);
    } else {
        ret = 0;
    }

    if (ret < 0) {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }

    return (frame || got_packet) ? 0 : 1;
}

//benchmark
int64_t g_start_time;
int g_frames_num = 1;

void output_thread(BroadcastingStation* bs, OutputFile& of)
{
    while(1){
        if(!streaming){
            av_usleep(1000000);
            return;
        }
        //get a video frame
        AVFormatContext* oc = of.fmt_ctx;
        AVDictionary * opt = NULL;
        //FIXME: open a outputfile
        int ret = avformat_write_header(oc, &opt);
        if (ret < 0) {
            fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
            av_usleep(1000000);
            continue;
        }
        int encode_video = 1;
        int encode_audio = 1;

        while (streaming) {
            /* select the stream to encode */
            if (encode_video &&
                (!encode_audio || av_compare_ts(of.video_st.next_pts, of.video_st.enc->time_base,
                                                of.audio_st.next_pts, of.audio_st.enc->time_base) <= 0)) {
                encode_video = !write_one_video_frame(bs, oc, &of.video_st);
                g_frames_num++;
            } else {
                encode_audio = !write_audio_frame(oc, &of.audio_st);
            }
            int64_t now = av_gettime_relative();
            printf("huheng consistency time: %lld, framesnum: %d \n", now - g_start_time, g_frames_num);
            printf("outputrate: %d\n", (now - g_start_time)/g_frames_num);
        }
        /* Write the trailer, if any. The trailer must be written before you
         * close the CodecContexts open when you wrote the header; otherwise
         * av_write_trailer() may try to use memory that was freed on
         * av_codec_close(). */
        av_write_trailer(oc);
        //FIXME: close outputfile
    }

}

int main(int argc, char** argv)
{
    av_register_all();
    avfilter_register_all();
    avformat_network_init();
    g_start_time = av_gettime_relative();
    OutputFile of(argv[1]);
    if(open_output_file(&of) != 0)
        return -1;
    //1 create BroadcastingStation
    BroadcastingStation* s = new BroadcastingStation();
    //2.start streaming thread

    //4.
    std::thread reapThread(std::mem_fn(&BroadcastingStation::reapFrames),s);
    std::thread outputThread(output_thread, s, std::ref(of));

    //manage the BroadcastingStation accordding to event
    s->addInputFile("rtmp://live.mudu.tv/watch/134w50", 0);
    s->layout.overlayMap[0].overlay_w = 320;
    s->layout.overlayMap[0].overlay_h = 240;
    s->openInputFile(0);
    //s->addInputFile("rtmp://live.mudu.tv/watch/134w50",1);
    s->addInputFile("a.mp4", 1);
    s->layout.overlayMap[1] = {1.0, 640, 360, 400, 400};
    s->openInputFile(1);
    //
    s->addInputFile("/home/huheng/Videos/samplemedia/SampleVideo_1280x720_1mb.mp4", 2);
    s->layout.overlayMap[2] = {1.0, 320, 180, 200, 200};
    s->openInputFile(2);

    //
    int tt = 0;
    while(1){
        av_usleep(300000);
        s->layout.overlayMap[0] = {0.5,320,240,0,(tt) % 500};
        tt += 100;
    }


    //thread join

    outputThread.join();
    reapThread.join();
    delete s;
    return 0;
}

