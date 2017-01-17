#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "filter.h"
#include "safequeue.h"

struct InputFile;
struct OutputFile;

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
        int num;
        OverlayConfig overlayMap[MaxInput];
        int sequence[MaxInput];
        Layout():num(0){}
    };
    //int openOutput(char* filename);
    void addInputFile(char* filename, int sequence);
    void openInputFile(int sequence);
    void deleteInputFile(int sequence);
    void constructing();
    void reconfig();

    SafeQueue<std::shared_ptr<Frame>> VideoQueue;
private:
    std::vector<InputFile*> input;
    //may be a event with config info
    bool reconfigReq;
    Layout layout;
    AVFrame* canvas;
    AVRational outputFrameRate;
    //AVFrame* outputFrame;
    //OutputFile* output;
};



#endif // SCENE_H
