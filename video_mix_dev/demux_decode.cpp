#include "util.h"
#include "demux_decode.h"
#include "broadcastingstation.h"
extern "C" {
#include "libavutil/mathematics.h"
}

int decoder_read_packet(Decoder *d, AVPacket* pkt){
    if(d->flushed){
        return d->flushed;
    }
    int ret = av_read_frame(d->fmt, pkt);
    if(ret < 0 && d->flushed == 0){
        pkt->data = NULL;
        pkt->size = 0;
        d->flushed = ret;
    }
    return 0;
}

/*add by huheng*/
int decoder_decode_frame(Decoder *d, AVFrame *frame) {
    int got_frame = 0;

    do {
        int ret = -1;

        if (d->abort_request)
            return -1;

        if (!d->packet_pending) {
            AVPacket pkt;
            ret = decoder_read_packet(d, &pkt);
            if(ret < 0)
                return -1;
            if(pkt.data != NULL && pkt.stream_index != d->video_stream_index)
                continue;
            av_packet_unref(&d->pkt);
            d->pkt_temp = d->pkt = pkt;
            d->packet_pending = 1;
        }

        switch (d->avctx->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                ret = avcodec_decode_video2(d->avctx, frame, &got_frame, &d->pkt_temp);
                break;
            default:
                break;
        }

        if (ret < 0) {
            d->packet_pending = 0;
        } else {
            d->pkt_temp.dts =
            d->pkt_temp.pts = AV_NOPTS_VALUE;
            if (d->pkt_temp.data) {
                d->pkt_temp.data += ret;
                d->pkt_temp.size -= ret;
                if (d->pkt_temp.size <= 0)
                    d->packet_pending = 0;
            } else {
                if (!got_frame) {
                    d->packet_pending = 0;
                   // d->finished = d->pkt_serial;
                }
            }
        }
    } while (!got_frame);

    return got_frame;
}

int open_input_file(InputFile* is)
{
    //benchmark test
    int64_t start_time = av_gettime_relative();
    int ret;
    AVCodec *dec;
    if(is == NULL)
        return -1;
    avcodec_close(is->video_dec_ctx);
    avformat_close_input(&is->fmt_ctx);
    is->fmt_ctx = NULL;
    is->start_time = -1;
    is->start_pts = -1;
    if ((ret = avformat_open_input(&is->fmt_ctx, is->filename.c_str(), NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(is->fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the video stream */
    ret = av_find_best_stream(is->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    is->video_stream_index = ret;
    is->video_dec_ctx = is->fmt_ctx->streams[is->video_stream_index]->codec;
    av_opt_set_int(is->video_dec_ctx, "refcounted_frames", 1, 0);

    /* init the video decoder */
    if ((ret = avcodec_open2(is->video_dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }
    is->video_time_base = is->fmt_ctx->streams[is->video_stream_index]->time_base;
    is->valid = true;
    int64_t end_time = av_gettime_relative();
    printf("huheng bench open input time: %lld", end_time - start_time);
    return 0;
}

void decode_thread(InputFile* is, BroadcastingStation* bs){
    while(!is->abortRequest){
        //init a decoder
        if(!is->valid){
            av_usleep(10000);
            is->valid = (open_input_file(is) == 0 ? true:false);
            continue;
        }
        Decoder d(is->fmt_ctx,is->video_dec_ctx,is->video_stream_index);
        while(!is->abortRequest && is->valid){
            //Decoder d(is->fmt_ctx, is->video_dec_ctx, is->video_stream_index);
            //AVFrame* frame = av_frame_alloc();
            auto sharedFrame = std::make_shared<Frame>();
            AVFrame* frame = sharedFrame->frame;
            int got_frame = decoder_decode_frame(&d, frame);
            if(got_frame < 0){
                is->valid = false;
                continue;
            }
            if(got_frame){
                auto qFrame = std::make_shared<Frame>();
                frame->pts = av_frame_get_best_effort_timestamp(frame);
                if(is->start_pts == -1){
                    is->start_time = av_gettime_relative();
                    is->start_pts = frame->pts;
                    is->start_frame_num = bs->outputFrameNum + 1;
                }
                av_frame_move_ref(qFrame->frame, frame);
                is->videoFrameQ.push(qFrame);
            }
        }
     }
}

//bool InputFile::getPicture(std::shared_ptr<Frame>& pic, AVRational frameRate)
//{
//    if(videoFrameQ.size() <= 0){
//        return false;
//    }
//    int64_t pts = start_pts + av_rescale_q(frame_num, frameRate, video_time_base);
//    //keep the last frame
//    while(1){
//        if(videoFrameQ.size() == 1){
//            pic = videoFrameQ.front();
//            break;
//        }
//        std::shared_ptr<Frame> sharedFrame;
//        if(videoFrameQ.front()->frame->pts > pts){
//            pic = videoFrameQ.front();
//            break;
//        }else{
//            videoFrameQ.pop(sharedFrame);
//        }
//    }
//    frame_num++;
//    return true;
//}
