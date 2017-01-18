#ifndef ENCODE_MUX_H
#define ENCODE_MUX_H

extern "C" {
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
}

#include "util.h"
#include <stdlib.h>

struct OutputStream;
struct OutputFile;

int open_output_file(OutputFile* of);
int write_video_frame(AVFormatContext *oc, OutputStream *ost);
void fill_yuv_image(AVFrame *pict, int frame_index, int width, int height);
AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height);
int write_frame(AVFormatContext *fmt_ctx, const AVRational *time_base, OutputStream *os, AVPacket *pkt);
void close_stream(AVFormatContext *oc, OutputStream *ost);

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;
    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;
    //char* filename;
    AVFrame *frame;
    AVFrame *tmp_frame;
    AVFrame *filter_frame;
    AVFrame *output_frame;
    AVFrame *text_frame;
    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
    //FilterBox box;
} OutputStream;

typedef struct OutputFile
{
    char* filename;
    AVFormatContext *fmt_ctx;
    OutputStream video_st;
    OutputStream audio_st;
    OutputFile(char* name){
        filename = (char*)malloc(strlen(name) + 1);
        strcpy(filename, name);
        memset(&video_st, 0, sizeof(video_st));
        memset(&audio_st, 0, sizeof(audio_st));
    }
    ~OutputFile(){
        close_stream(fmt_ctx, &video_st);
        close_stream(fmt_ctx, &audio_st);
        avformat_close_input(&fmt_ctx);
        free(filename);
    }
}OutputFile;

#endif // ENCODE_MUX_H
