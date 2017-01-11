
#include "demo.h"

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


