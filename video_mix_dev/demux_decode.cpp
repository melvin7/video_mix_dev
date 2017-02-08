#include "util.h"
#include "demux_decode.h"
#include "broadcastingstation.h"
extern "C" {
#include "libavutil/mathematics.h"
}

Decoder* init_decoder(AVFormatContext* fmt_ctx, enum AVMediaType type)
{
    AVCodec* dec;
    int ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return NULL;
    }
    int stream_index = ret;
    AVCodecContext* dec_ctx = fmt_ctx->streams[stream_index]->codec;
    av_opt_set_int(dec_ctx, "refcounted_frames", 1, 0);

    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open decoder\n");
        return NULL;
    }
    AVRational time_base = fmt_ctx->streams[stream_index]->time_base;
    return new Decoder(dec_ctx, stream_index, time_base);
}

int open_input_file(InputFile* is)
{
    //benchmark test
    int64_t start_time = av_gettime_relative();
    int ret;
    if(is == NULL)
        return -1;
    if(is->audioDecoder){
        delete is->audioDecoder;
        is->audioDecoder = NULL;
    }
    if(is->videoDecoder){
        delete is->videoDecoder;
        is->videoDecoder = NULL;
    }
    avformat_close_input(&is->fmt_ctx);
    is->fmt_ctx = NULL;
    is->start_time = -1;
    is->output_start_pts = -1;
    if ((ret = avformat_open_input(&is->fmt_ctx, is->filename.c_str(), NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file: %s\n", is->filename.c_str());
        return ret;
    }

    if ((ret = avformat_find_stream_info(is->fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    is->videoDecoder = init_decoder(is->fmt_ctx, AVMEDIA_TYPE_VIDEO);
    is->audioDecoder = init_decoder(is->fmt_ctx, AVMEDIA_TYPE_AUDIO);

    if(is->videoDecoder || is->audioDecoder)
        is->valid = true;
    int64_t end_time = av_gettime_relative();
    printf("huheng bench open input time: %lld", end_time - start_time);
    return 0;
}

int consume_packet(InputFile* is, AVPacket* pkt)
{
    int ret;
    if(!pkt || !is || !is->audioDecoder || !is->videoDecoder){
        return -1;
    }
    Decoder* d = NULL;
    SafeQueue<std::shared_ptr<Frame>, FrameQueueSize> *frameQueue = NULL;
    int64_t output_start_pts = 0;
    int dd = 0;
    if(pkt->stream_index == is->audioDecoder->stream_index){
        d = is->audioDecoder;
        frameQueue = &is->audioFrameQ;
    } else if(pkt->stream_index == is->videoDecoder->stream_index){
        d = is->videoDecoder;
        frameQueue = &is->videoFrameQ;

    }
    ret = avcodec_send_packet(d->avctx, pkt);
    if(ret < 0){
        printf("send packet failed: %d", ret);
        return ret;
    }
    AVFrame* frame = av_frame_alloc();
    while(1){
        ret = avcodec_receive_frame(d->avctx, frame);
        if(ret == 0){
            //got frame and send to frame queue
            auto sharedFrame = std::make_shared<Frame>();
            frame->pts = av_frame_get_best_effort_timestamp(frame);
            if(d->start_pts == -1){
                d->start_pts = frame->pts +dd;
            }

            av_frame_move_ref(sharedFrame->frame, frame);
            int64_t old_delta= sharedFrame->frame->pts - d->start_pts;
            int64_t new_delta = av_rescale_q(old_delta, d->time_base, d->dst_time_base);
            sharedFrame->frame->pts = new_delta + d->dst_start_pts;
            if(frameQueue)
                frameQueue->push(sharedFrame);
        }else{
            break;
        }
    }
    av_frame_free(&frame);
    return 0;
}

void decode_thread(InputFile* is, BroadcastingStation* bs){
    while(!is->abortRequest){
        //init a decoder
        if(!is->valid){
            av_usleep(1000000);
            is->valid = (open_input_file(is) == 0 ? true:false);
            continue;
        }
        bool flushed = false;
        while(!is->abortRequest && is->valid){
            if(flushed){
                //eof
                is->valid = false;
                continue;
            }
            AVPacket pkt;
            av_init_packet(&pkt);
            int ret = av_read_frame(is->fmt_ctx, &pkt);
            if (ret < 0 && !flushed){
                //send flush packet to audio and video decoder
                AVPacket flush_pkt;
                flush_pkt.data = NULL;
                flush_pkt.size = 0;
                flush_pkt.stream_index = is->videoDecoder->stream_index;
                consume_packet(is, &flush_pkt);
                flush_pkt.stream_index = is->audioDecoder->stream_index;
                consume_packet(is, &flush_pkt);
                flushed = true;
            } else if(ret == 0){
                if(pkt.stream_index == is->videoDecoder->stream_index){
                    if(is->videoDecoder->dst_start_pts == -1){
                        is->videoDecoder->dst_time_base = bs->outputFrameRate;
                        is->audioDecoder->dst_time_base = bs->outputSampleRate;
                        is->videoDecoder->dst_start_pts = bs->outputFrameNum;
                        is->audioDecoder->dst_start_pts = av_rescale_q(bs->outputFrameNum,
                                                                       bs->outputFrameRate,
                                                                       bs->outputSampleRate);
                    }
                    consume_packet(is, &pkt);
                }else if(pkt.stream_index == is->audioDecoder->stream_index){
                    if(is->audioDecoder->dst_start_pts == -1){
                        is->videoDecoder->dst_time_base = bs->outputFrameRate;
                        is->audioDecoder->dst_time_base = bs->outputSampleRate;
                        is->videoDecoder->dst_start_pts = bs->outputFrameNum;
                        is->audioDecoder->dst_start_pts = av_rescale_q(bs->outputFrameNum,
                                                                       bs->outputFrameRate,
                                                                       bs->outputSampleRate);
                    }
                    consume_packet(is, &pkt);
                }
            }
            av_packet_unref(&pkt);
        }
     }
}
