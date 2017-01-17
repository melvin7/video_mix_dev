#ifndef MIX_UTIL_H
#define MIX_UTIL_H

extern "C"{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

/* avoid a temporary address return build error in c++ */
#undef av_err2str
#define av_err2str(errnum) \
    av_make_error_string((char*)__builtin_alloca(AV_ERROR_MAX_STRING_SIZE), AV_ERROR_MAX_STRING_SIZE, errnum)

#undef av_ts2str
#define av_ts2str(ts) \
    av_ts_make_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts)

#undef av_ts2timestr
#define av_ts2timestr(ts, tb) \
    av_ts_make_time_string((char*)__builtin_alloca(AV_TS_MAX_STRING_SIZE), ts, tb)


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



#endif // MIX_UTIL_H
