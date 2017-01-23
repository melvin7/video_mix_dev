#ifndef FILTER_H
#define FILTER_H

extern "C"{
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

typedef struct FrameArgs{
    AVRational rate;
    int fmt;
    int width;
    int height;
}FrameArgs;

typedef struct OverlayConfig{
    double opacity;
    int overlay_w;
    int overlay_h;
    int offset_x;
    int offset_y;
    //TODO: add a void* config data
    OverlayConfig():opacity(1.0),overlay_w(1280),overlay_h(720),offset_x(0),offset_y(0){}
    OverlayConfig(double o, int ow, int oh, int ox, int oy):
        opacity(o), overlay_w(ow), overlay_h(oh), offset_x(ox), offset_y(oy)
    {}
    OverlayConfig(OverlayConfig& c){
        opacity = c.opacity;
        overlay_w = c.overlay_w;
        overlay_h = c.overlay_h;
        offset_x = c.offset_x;
        offset_y = c.offset_y;
    }
    bool operator==(const OverlayConfig& c){
        return opacity == c.opacity &&
                overlay_h == c.overlay_h &&
                overlay_w == c.overlay_w &&
                offset_x == c.offset_x &&
                offset_y == c.offset_y;
    }
}OverlayConfig;

//a filter box has an input pad named "in" and an output pad named "out".
//the box can take overlay texts or pictures with opacity on an input stream;
//box reconfigure when parameters changed
class OverlayBox {
public:
    enum {
        MainHeight = 1280,
        MainWidth = 720
    };
    enum OverlayType{
        PICTURE_OVERLAY,
        TEXT_OVERLAY,
        STREAM_OVERLAY,
        NB_OVERLAY
    };


    OverlayBox():
        valid(false),
        filter_graph(NULL),
        buffersrc_ctx_main(NULL),
        buffersrc_ctx_overlayed(NULL),
        buffersink_ctx(NULL),
        overlay_type(NB_OVERLAY),
        desc(NULL){}
        
    ~OverlayBox(){
        avfilter_graph_free(&filter_graph);
    }
    void setOverlayConfig(OverlayConfig& c);
    void config(OverlayConfig c, OverlayType t, void* opaque);
    bool push_main_frame(AVFrame* );
    bool push_overlayed_frame(AVFrame*);
    int pop_frame(AVFrame*);
    bool valid;
    OverlayConfig conf;
private:
    AVFilterGraph* filter_graph;
    AVFilterContext* buffersrc_ctx_main;
    AVFilterContext* buffersrc_ctx_overlayed;
    AVFilterContext* buffersink_ctx;
    OverlayType overlay_type;
    char* desc;
};

#endif // FILTER_H
