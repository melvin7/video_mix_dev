// rtmp://live.mudu.tv/watch/fp6r8x rtmp://pub.mudu.tv/watch/hm1s6n

#include <thread>
#include <memory>

#include <stdio.h>

#include "demo.h"
#include "safequeue.h"
#include "filter.h"
#include <string.h>

#define STREAM_DURATION   10000.0
#define STREAM_FRAME_RATE 25 /* 25 images/s */
#define STREAM_PIX_FMT    AV_PIX_FMT_YUV420P /* default pix_fmt */

#define SCALE_FLAGS SWS_BICUBIC

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


static AVFrame *get_one_video_frame(OutputStream *ost)
{
    AVCodecContext *c = ost->enc;
    int got_frame = 0;
    AVFrame* dummy_frame;
    AVFrame* output_frame;
    /* check if we want to generate more frames */
    if (av_compare_ts(ost->next_pts, c->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here */
    if (av_frame_make_writable(ost->frame) < 0)
        exit(1);
    if (av_frame_make_writable(ost->tmp_frame) < 0)
        exit(1);
//    static int64_t lasttime = av_gettime_relative();
//    int64_t diff = av_gettime_relative() - lasttime;
//    while(diff < 40000){
//        av_usleep(10000);
//        diff = av_gettime_relative() - lasttime;
//    }
//    lasttime = av_gettime_relative();
    if(videoFrameQ.size() != 0){
        auto sharedFrame = std::make_shared<Frame>();
        videoFrameQ.pop(sharedFrame);
        av_frame_move_ref(ost->filter_frame, sharedFrame->frame);
        //swscale and send to
        if (!ost->sws_ctx) {
            ost->sws_ctx = sws_getContext(ost->filter_frame->width, ost->filter_frame->height,
                                          (enum AVPixelFormat)ost->filter_frame->format,
                                          c->width/2, c->height/2,
                                          c->pix_fmt,
                                          SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx) {
                fprintf(stderr,
                        "Could not initialize the conversion context\n");
                exit(1);
            }
        }
        sws_scale(ost->sws_ctx,
                  (const uint8_t * const *)ost->filter_frame->data, ost->filter_frame->linesize,
                  0, ost->filter_frame->height, ost->tmp_frame->data, ost->tmp_frame->linesize);
        concatyuv420P_test(ost->frame, ost->tmp_frame);
    }else{
        fill_yuv_image(ost->frame, ost->next_pts, c->width, c->height);
    }
    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

int write_one_video_frame(AVFormatContext *oc, OutputStream *ost)
{
    int ret;
    AVCodecContext *c;
    AVFrame *frame;
    int got_packet = 0;
    AVPacket pkt = { 0 };

    c = ost->enc;

    frame = get_one_video_frame(ost);
    //frame = get_video_frame(ost);
    //send to filter



    av_init_packet(&pkt);

    /* encode the image */
    ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
    if (ret < 0) {
        //fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
        exit(1);
    }

    if (got_packet) {
        ret = write_frame(oc, &c->time_base, ost, &pkt);
    } else {
        ret = 0;
    }

    if (ret < 0) {
        //fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }

    return (frame || got_packet) ? 0 : 1;
}
const int output_width = 640;
const int output_height = 320;
const int output_format = 0;

int main(int argc, char *argv[])
{
    if(argc < 3)
        printf("input parameters!\n");
    OutputFile of = {0};
    of.filename = argv[2];
    InputFile is;
    is.filename = std::string(argv[1]);
    av_register_all();
    avformat_network_init();
    if(open_output_file(&of) != 0)
        return -1;
    if(open_input_file(&is) != 0){
        is.valid = false;
    }
    char args[512];
    AVRational output_timebase = (AVRational){1, STREAM_FRAME_RATE};
    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            output_width, output_height, output_format,
            output_timebase.num, output_timebase.den,
            output_width, output_height);
    //init filter
    if(init_filter(of.video_st.box, args, "drawtext=text='MUDUTV':fontsize=30:x=(w-text_w)/2:y=(h-text_h)/2") < 0)


    std::thread t([&](){
        while(1){
            //init a decoder
            if(!is.valid){
                av_usleep(100000);
                is.valid = (open_input_file(&is) == 0 ? true:false);
                continue;
            }
            Decoder d(is.fmt_ctx,is.video_dec_ctx,is.video_stream_index);
            while(is.valid){
                //Decoder d(is.fmt_ctx, is.video_dec_ctx, is.video_stream_index);
                AVFrame* frame = av_frame_alloc();
                int got_frame = decoder_decode_frame(&d, frame);
                if(got_frame < 0){
                    is.valid = false;
                    continue;
                }
                if(got_frame){
                    auto qFrame = std::make_shared<Frame>();
                    av_frame_move_ref(qFrame->frame, frame);
                    videoFrameQ.push(qFrame);
                }
            }
        }
    });

    AVFormatContext* oc = of.fmt_ctx;
    AVDictionary * opt = NULL;

    int ret = avformat_write_header(oc, &opt);
    if (ret < 0) {
        //fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
        return 1;
    }
    int encode_video = 1;
    int encode_audio = 1;

    while (encode_video || encode_audio) {
        /* select the stream to encode */
        if (encode_video &&
            (!encode_audio || av_compare_ts(of.video_st.next_pts, of.video_st.enc->time_base,
                                            of.audio_st.next_pts, of.audio_st.enc->time_base) <= 0)) {
            encode_video = !write_one_video_frame(oc, &of.video_st);
        } else {
            encode_audio = !write_audio_frame(oc, &of.audio_st);
        }
    }
    t.join();
    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);
    int have_video =1;
    int have_audio =1;
    /* Close each codec. */
    if (have_video)
        close_stream(oc, &of.video_st);
    if (have_audio)
        close_stream(oc, &of.audio_st);

    if (!(of.fmt_ctx->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);
}
