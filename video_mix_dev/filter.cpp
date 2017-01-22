#include "filter.h"
#include "demux_decode.h"
#include <string.h>

void OverlayBox::config(OverlayConfig c, OverlayType t, void* opaque)
{
    int ret = 0;
    valid = false;
    char desc[512] = {0};
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    if(opaque == NULL)
        return;
    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVFilterInOut *overlayed_output;
    //AVRational time_base = input_fmt_ctx->streams[video_stream_index]->time_base;
    //free old graph
    avfilter_graph_free(&filter_graph);
    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
//    snprintf(args, sizeof(args),
//            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
//            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
//            time_base.num, time_base.den,
//            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx_main, buffersrc, "in",
                                       "video_size=1280x720:pix_fmt=0:time_base=1/25:pixel_aspect=1280/720", NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx_main;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;
    //make desc

    if(t == PICTURE_OVERLAY){
        char* path = (char*)opaque;
        snprintf(desc, sizeof(desc),
                 "movie=\'%s\',scale=%dx%d,setsar=1/1[top];"
                 "[in]split[main][tmp];"
                 "[tmp]crop=w=%d:h=%d:x=%d:y=%d,setsar=1/1[bottom];"
                 "[top][bottom]blend=all_opacity=%f[picture];"
                 "[main][picture]overlay=x=%d:y=%d",
                 path, c.overlay_w, c.overlay_h,
                 c.overlay_w, c.overlay_h, c.offset_x, c.offset_y,
                 c.opacity,
                 c.offset_x, c.offset_y
                 );
        printf("pic desc: %s\n",desc);
    } else if(t == TEXT_OVERLAY){
        char* text = (char*)opaque;
        snprintf(desc, sizeof(desc),
                 "drawtext=text=\'%s\':fontsize=40:x=%d:y=%d",
                 text, c.offset_x, c.offset_y
                 );
        printf("text desc: %s\n", desc);
    } else if(t == STREAM_OVERLAY){
        //AVCodecContext* ctx = (AVCodecContext*)opaque;
        FrameArgs* frameargs = (FrameArgs*)opaque;
        char args[128];
        snprintf(args, sizeof(args),
                 "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
                 frameargs->width, frameargs->height, frameargs->fmt, 1, 25, frameargs->width, frameargs->height
                 );
//        snprintf(args, sizeof(args),
//                 "video_size=1280x720:pix_fmt=0:time_base=1/25:pixel_aspect=1280/720");
        ret = avfilter_graph_create_filter(&buffersrc_ctx_overlayed, buffersrc, "overlayed",
                                       args, NULL, filter_graph);
        if(ret < 0){
            printf("buffersrc overlay failed\n");
            goto end;
        }
//        snprintf(desc, sizeof(desc),
//                 "[overlayed]scale=%dx%d,setsar=1/1[top];"
//                 "[in]split[main][tmp];"
//                 "[tmp]crop=w=%d:h=%d:x=%d:y=%d,setsar=1/1[bottom];"
//                 "[top][bottom]blend=all_opacity=%f[picture];"
//                 "[main][picture]overlay=x=%d:y=%d[out]",
//                 c.overlay_w, c.overlay_h,
//                 c.overlay_w, c.overlay_h, c.offset_x, c.offset_y,
//                 c.opacity,
//                 c.offset_x, c.offset_y
//                 );
//        snprintf(desc, sizeof(desc),
//                 "[overlayed]scale=%dx%d[top];"
//                 "[in][top]overlay=x=%d:y=%d[out]",
//                 c.overlay_w, c.overlay_h,
//                 c.offset_x, c.offset_y
//                 );
        snprintf(desc, sizeof(desc),
                 "[overlayed]scale=%dx%d,format=yuva420p,lutyuv=a=%d[top];"
                 "[in][top]overlay=x=%d:y=%d[out]",
                 c.overlay_w, c.overlay_h, (int)(c.opacity * 255),
                 c.offset_x, c.offset_y
                 );
        printf("stream desc: %s\n", desc);
        overlayed_output = avfilter_inout_alloc();
        overlayed_output->name       = av_strdup("overlayed");
        overlayed_output->filter_ctx = buffersrc_ctx_overlayed;
        overlayed_output->pad_idx    = 0;
        overlayed_output->next       = NULL;
        outputs->next = overlayed_output;
    } else{
        goto end;
    }
    if ((ret = avfilter_graph_parse_ptr(filter_graph, desc,
                                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;
    valid = true;
end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
}

bool OverlayBox::push_main_frame(AVFrame* frame)
{
    if (valid && av_buffersrc_add_frame_flags(buffersrc_ctx_main, frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
        av_log(NULL, AV_LOG_DEBUG, "push frame\n");
        return true;
    }
    return false;
}

bool OverlayBox::push_overlayed_frame(AVFrame* frame)
{
    if (valid && av_buffersrc_add_frame_flags(buffersrc_ctx_overlayed, frame, AV_BUFFERSRC_FLAG_KEEP_REF) >= 0) {
        av_log(NULL, AV_LOG_DEBUG, "push frame\n");
        return true;
    }
    return false;
}
int OverlayBox::pop_frame(AVFrame* frame)
{
    int ret;
    /* pull filtered frames from the filtergraph */
    //should be pop immediately
    while (1) {
        ret = av_buffersink_get_frame(buffersink_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
            printf("huheng %s!\n",ret == AVERROR(EAGAIN) ? "eagain":"error");
            return ret;
        }
        if (ret < 0){
            //printf("huheng %d\n",ret);
            return ret;
        }
        break;
    }
    //success
    return 1;
}
//int init_filters(FilterBox* box, char* args, const char *filters_descr)
//{
//    //char args[512];
//    int ret = 0;
//    AVFilter *buffersrc  = avfilter_get_by_name("buffer");
//    AVFilter *buffersink = avfilter_get_by_name("buffersink");
//    AVFilterInOut *outputs = avfilter_inout_alloc();
//    AVFilterInOut *inputs  = avfilter_inout_alloc();
//    memset(box, 0, sizeof(FilterBox));
//    //AVRational time_base = input_fmt_ctx->streams[video_stream_index]->time_base;
//    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };

//    box->filter_graph = avfilter_graph_alloc();
//    if (!outputs || !inputs || !box->filter_graph) {
//        ret = AVERROR(ENOMEM);
//        goto end;
//    }

//    /* buffer video source: the decoded frames from the decoder will be inserted here. */
////    snprintf(args, sizeof(args),
////            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
////            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
////            time_base.num, time_base.den,
////            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

//    ret = avfilter_graph_create_filter(&(box->buffersrc_ctx), buffersrc, "in",
//                                       args, NULL, box->filter_graph);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
//        goto end;
//    }

//    /* buffer video sink: to terminate the filter chain. */
//    ret = avfilter_graph_create_filter(&box->buffersink_ctx, buffersink, "out",
//                                       NULL, NULL, box->filter_graph);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
//        goto end;
//    }

//    ret = av_opt_set_int_list(box->buffersink_ctx, "pix_fmts", pix_fmts,
//                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
//        goto end;
//    }

//    /*
//     * Set the endpoints for the filter graph. The filter_graph will
//     * be linked to the graph described by filters_descr.
//     */

//    /*
//     * The buffer source output must be connected to the input pad of
//     * the first filter described by filters_descr; since the first
//     * filter input label is not specified, it is set to "in" by
//     * default.
//     */
//    outputs->name       = av_strdup("in");
//    outputs->filter_ctx = box->buffersrc_ctx;
//    outputs->pad_idx    = 0;
//    outputs->next       = NULL;

//    /*
//     * The buffer sink input must be connected to the output pad of
//     * the last filter described by filters_descr; since the last
//     * filter output label is not specified, it is set to "out" by
//     * default.
//     */
//    inputs->name       = av_strdup("out");
//    inputs->filter_ctx = box->buffersink_ctx;
//    inputs->pad_idx    = 0;
//    inputs->next       = NULL;

//    if ((ret = avfilter_graph_parse_ptr(box->filter_graph, filters_descr,
//                                    &inputs, &outputs, NULL)) < 0)
//        goto end;

//    if ((ret = avfilter_graph_config(box->filter_graph, NULL)) < 0)
//        goto end;
//    box->valid = true;
//end:
//    avfilter_inout_free(&inputs);
//    avfilter_inout_free(&outputs);

//    return ret;
//}
