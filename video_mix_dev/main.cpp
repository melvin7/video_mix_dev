#include <thread>
#include <memory>

#include <stdio.h>

#include "demo.h"
#include "safequeue.h"



int main(int argc, char *argv[])
{
    if(argc < 3)
        printf("input parameters!\n");
    OutputFile of = {0};
    of.filename = argv[2];
    InputFile is;
    is.filename = std::string(argv[1]);
    if(open_output_file(&of) != 0)
        return -1;
    if(open_input_file(&is) != 0){
        is.valid = false;
    }
    SafeQueue<std::shared_ptr<Frame>, 100> videoFrameQ;
    //a thread push AVFrame to videoFrameQ
    //std::thread t
    /* Write the stream header, if any. */

    AVFormatContext* oc = of.fmt_ctx;
    AVDictionary * opt;
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
            (!encode_audio || av_compare_ts(video_st.next_pts, video_st.enc->time_base,
                                            audio_st.next_pts, audio_st.enc->time_base) <= 0)) {
            encode_video = !write_video_frame(oc, &os.video_st);
        } else {
            encode_audio = !write_audio_frame(oc, &os.audio_st);
        }
    }

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);
}