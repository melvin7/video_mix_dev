#ifndef BROADCASTINGSTATION_H
#define BROADCASTINGSTATION_H

#include <vector>
#include <unordered_map>

#include "safequeue.h"
#include "util.h"
#include "demux_decode.h"
#include "encode_mux.h"

class BroadcastingStation{
public:
    BroadcastingStation();
    ~BroadcastingStation();
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

    //interface for event request
    void addInputFile(char* filename, int sequence);
    void openInputFile(int sequence);
    void deleteInputFile(int sequence);
    void addOutputFile(char* filename, int index);
    void deleteOutputFile(int index);
    void startStreaming();
    void stopStreaming();

    //thread
    void reapFrames();
    void streamingOut();

    void reconfig();
    AVFrame* mixVideoStream();
    int overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, int index);
    bool getPicture(InputFile* is, std::shared_ptr<Frame>& pic);

    int abortRequest;
    //FIXME: should be private, for test
    Layout layout;
    //output
    AVRational outputFrameRate;
    std::unordered_map<int, OutputFile*> outputs;
    //int openOutput(char* filename);
    SafeQueue<std::shared_ptr<Frame>, 50> outputVideoQ;
    int outputFrameNum;
    int64_t start_time;
private:
    InputFile* input[MaxInput];
    //may be a event with config info
    bool reconfigReq;
    AVFrame* canvas;
    bool streaming;
};

#endif // BroadcastingStation_H
