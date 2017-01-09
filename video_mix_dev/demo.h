#ifndef DEMO_H
#define DEMO_H

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}
#include<string>

/*wrap AVPacket*/
struct Packet
{
    AVPacket pkt;
    int serial;
    Packet():serial(0){
        av_init_packet(&pkt);
    }
    ~Packet(){
        av_packet_unref(&pkt);
    }
};

/*wrap AVFrame*/
struct Frame
{
    AVFrame* frame;
    int serial;
    Frame():serial(0){
        frame = av_frame_alloc();
    }
    ~Frame(){
        av_frame_unref(frame);
        av_frame_free(&frame);
    }
};

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    /* pts of the next frame that will be generated */
    int64_t next_pts;

    int samples_count;
    char* filename;
    AVFrame *frame;
    AVFrame *tmp_frame;
    AVFrame *filter_frame;
    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

typedef struct OutputFile
{
    char* filename;
    AVFormatContext *fmt_ctx;
    OutputStream* video_st;
    OutputStream* audio_st;
}OutputFile;


class InputFile{
public:
    InputFile():fmt_ctx(NULL), video_dec_ctx(NULL), video_stream_index(-1), valid(false){}
    //video state, like index and position in the pad
    //input filename
    std::string filename;
    AVFormatContext* fmt_ctx;
    AVCodecContext* video_dec_ctx;
    int video_stream_index;
    //input stream is valid, false: not init, eof or error
    bool valid;
    //push AVFrame to queue
    //SafeQueue<std::shared_ptr<Frame>, 100> videoFrameQ;
};


int write_audio_frame(AVFormatContext *oc, OutputStream *ost);
int open_input_file(InputFile* is);
int open_output_file(OutputFile* of);

#endif // DEMO_H