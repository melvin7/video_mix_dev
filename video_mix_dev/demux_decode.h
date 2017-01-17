#ifndef DEMUX_DECODE_H
#define DEMUX_DECODE_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

#include "libavutil/time.h"
}
#include<string>
#include "filter.h"
#include "safequeue.h"

//a video decoder decode the video stream in fmt
typedef struct Decoder {
    AVFormatContext* fmt;
    AVCodecContext* avctx;
    AVPacket pkt;
    AVPacket pkt_temp;
    int video_stream_index; //stream index in fmt
    int packet_pending;
    int flushed;
    int abort_request;
    Decoder(AVFormatContext* avfmt, AVCodecContext* avcodec, int index):
    fmt(avfmt), avctx(avcodec), video_stream_index(index),
      packet_pending(0), flushed(0), abort_request(0){
        av_init_packet(&pkt);
        av_init_packet(&pkt_temp);
    }
}Decoder;


class InputFile{
public:
    InputFile(char* fname):filename(fname), fmt_ctx(NULL), video_dec_ctx(NULL),
        video_stream_index(-1), valid(false), abortRequest(false),decodeThread(NULL){}
    ~InputFile(){
        avcodec_close(video_dec_ctx);
        avformat_close_input(&fmt_ctx);
    }
    bool getPicture(int64_t time);
    //video state, like index and position in the pad
    //input filename
    std::string filename;
    AVFormatContext* fmt_ctx;
    AVCodecContext* video_dec_ctx;
    int video_stream_index;
    //input stream is valid, false: not init, eof or error
    bool valid;
    bool abortRequest;
    //push AVFrame to queue
    std::thread* decodeThread;
    SafeQueue<std::shared_ptr<Frame>, 100> videoFrameQ;
    SafeQueue<std::shared_ptr<Frame>, 100> audioFrameQ;
    //video overlay config
    OverlayConfig layout;
    int64_t start_time;
    int64_t start_pts;
};

/*
    open input file
*/
int open_input_file(InputFile* is);
int decoder_decode_frame(Decoder *d, AVFrame *frame);


int write_audio_frame(AVFormatContext *oc, OutputStream *ost);



#endif
