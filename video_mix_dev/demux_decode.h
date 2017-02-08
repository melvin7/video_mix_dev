#ifndef DEMUX_DECODE_H
#define DEMUX_DECODE_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"

#include "libavutil/time.h"
}
#include <string>
#include <map>
#include "filter.h"
#include "safequeue.h"
#include "util.h"

#define FrameQueueSize 100

//a video decoder decode the video stream in fmt
typedef struct Decoder {
    AVCodecContext* avctx;
    int stream_index; //stream index in fmt
    int abort_request;

    int64_t start_pts;
    int64_t dst_start_pts; //in dst_time_base
    AVRational dst_time_base;

    AVRational time_base;
    //SafeQueue<std::shared_ptr<Frame>, FrameQueueSize> frameQueue;
    Decoder():avctx(NULL),stream_index(-1),
        start_pts(-1),
        abort_request(false),time_base({1,1000}){}
    Decoder(AVCodecContext* ctx, int index, AVRational tb){
        avctx = ctx;
        stream_index = index;
        time_base = tb;
        start_pts = -1;
        dst_start_pts = -1;
    }
    ~Decoder(){
        avcodec_close(avctx);
    }
}Decoder;



struct LayoutConfig{
    OverlayBox box;
    OverlayConfig overlayConf;
    int orderNum;
    //std::map<int, OverlayBox> filterList; //filter list for input stream
};

class InputFile{
public:
    InputFile(char* fname):filename(fname), fmt_ctx(NULL),
        videoDecoder(NULL), audioDecoder(NULL),
        video_start_pts(-1), audio_start_pts(-1),
        output_start_pts(-1),
        valid(false), video_time_base({1,1000}),
        abortRequest(false),decodeThread(NULL){}
    ~InputFile(){
        avformat_close_input(&fmt_ctx);
    }
    //input filename
    std::string filename;
    AVFormatContext* fmt_ctx;
    //video config
    Decoder* videoDecoder;
    Decoder* audioDecoder;
    SafeQueue<std::shared_ptr<Frame>, FrameQueueSize> videoFrameQ;
    SafeQueue<std::shared_ptr<Frame>, FrameQueueSize> audioFrameQ;
    LayoutConfig layoutConf;
    AVRational video_time_base;
    FrameArgs fa;

    //input stream is valid, false: not init, eof or error
    bool valid;
    bool abortRequest;   
    std::thread* decodeThread;
    //video overlay config
    int64_t start_time;
    int64_t video_start_pts; //the start pts of input stream
    int64_t audio_start_pts;
    int64_t output_start_pts;
    int start_frame_num;

};

/*
    open input file
*/
class BroadcastingStation;

int open_input_file(InputFile* is);
void decode_thread(InputFile* is, BroadcastingStation* bs);

#endif
