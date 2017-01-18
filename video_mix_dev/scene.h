#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "filter.h"
#include "safequeue.h"
#include "util.h"
#include "demux_decode.h"
#include "encode_mux.h"
class Scene{
public:
    Scene();
    ~Scene();
    enum{
        Width = 1280,
        Height = 720,
        MaxInput = 16
    };
    struct Layout{
        OverlayBox filterBox[MaxInput];
        OverlayConfig overlayMap[MaxInput];
        int sequence[MaxInput];
        int num;
        Layout():num(0){}
    };
    //int openOutput(char* filename);
    void addInputFile(char* filename, int sequence);
    void openInputFile(int sequence);
    void deleteInputFile(int sequence);
    void constructing(SafeQueue<std::shared_ptr<Frame>, 10>& videoq);
    void reconfig();
    AVFrame* mixVideoStream();
    int overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, int index);
    SafeQueue<std::shared_ptr<Frame>, 100> VideoQueue;
    int abortRequest;
    //FIXME: should be private, for test
    Layout layout;
private:
    //std::vector<InputFile*> input;
    InputFile* input[MaxInput];
    //may be a event with config info
    bool reconfigReq;

    AVFrame* canvas;
    AVRational outputFrameRate;
    //AVFrame* outputFrame;
    //OutputFile* output;
};



#endif // SCENE_H
