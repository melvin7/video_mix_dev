#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include "filter.h"

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
    void reapAVFrame();
    void constructing();
    void reconfig();

    SafeQueue<std::shared_ptr<Frame>> VideoQueue;
private:
    std::vector<InputFile*> input;
    std::vector<OverlayConfig> inputConfig;
    //may be a event with config info
    bool reconfigReq;
    Layout layout;
    AVFrame* canvas;
    //AVFrame* outputFrame;
    //OutputFile* output;
};



#endif // SCENE_H
