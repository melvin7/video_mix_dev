#ifndef DEMUX_DECODE_H
#define DEMUX_DECODE_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

#include "libavutil/time.h"
}
#include <string>
#include "filter.h"
#include "safequeue.h"
#include "util.h"

struct OutputStream;

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


struct LayoutConfig{
    OverlayConfig overlayConf;
    std::vector<OverlayBox> filterList; //filter list for input stream
    int orderNum;
};

class InputFile{
public:
    InputFile(char* fname):filename(fname), fmt_ctx(NULL), video_dec_ctx(NULL),
        video_stream_index(-1), valid(false), video_time_base({1,1000}),
        abortRequest(false),decodeThread(NULL){}
    ~InputFile(){
        avcodec_close(video_dec_ctx);
        avformat_close_input(&fmt_ctx);
    }
    //bool getPicture(std::shared_ptr<Frame>& sharedFrame, AVRational frameRate);
    //video state, like index and position in the pad
    //input filename
    std::string filename;
    AVFormatContext* fmt_ctx;
    //video config
    AVCodecContext* video_dec_ctx;
    int video_stream_index;
    SafeQueue<std::shared_ptr<Frame>, 100> videoFrameQ;
    LayoutConfig layoutConf;
    AVRational video_time_base;
    FrameArgs fa;

    //audio config
    SafeQueue<std::shared_ptr<Frame>, 100> audioFrameQ;

    //input stream is valid, false: not init, eof or error
    bool valid;
    bool abortRequest;   
    std::thread* decodeThread;
    //video overlay config
    int64_t start_time;
    int64_t start_pts;
    int start_frame_num;

};

/*
    open input file
*/
class BroadcastingStation;
int open_input_file(InputFile* is);
int decoder_decode_frame(Decoder *d, AVFrame *frame);
void decode_thread(InputFile* is, BroadcastingStation* bs);

#endif
