#ifndef FILTER_H
#define FILTER_H

extern "C"{
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

typedef struct FilterBox {
    AVFilterGraph* filter_graph;
    AVFilterContext* buffersrc_ctx;
    AVFilterContext* buffersink_ctx;
    char* desc;
}FilterBox;

#endif // FILTER_H
