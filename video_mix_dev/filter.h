#ifndef FILTER_H
#define FILTER_H

extern "C"{
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

//a filter box has an input pad named "in" and an output pad named "out".
//the box can take overlay texts or pictures with opacity on an input stream;

typedef struct FilterBox {
    AVFilterGraph* filter_graph;
    AVFilterContext* buffersrc_ctx;
    AVFilterContext* buffersink_ctx;
    char* desc;
    bool valid;
}FilterBox;

int init_filters(FilterBox* box, char* args, const char *filters_descr);
typedef struct Rectangle{
    //double opacity;
    int w; //width
    int h; //height
    Rectangle():w(0),h(0){}
    Rectangle(int width, int height):w(width),h(height){}
}Rectangle;

class OverLayBox{
public:
    OverLayBox():opacity(1.0), main_x(0), main_y(0), offset_x(0), offset_y(0){}
private:
    double opacity;
    int main_x;
    int main_y;
    int overlay_x;
    int overlay_y;
    int offset_x;
    int offset_y;
    FilterBox filter_box;
};

#endif // FILTER_H
