#ifndef BROADCASTINGSTATION_H
#define BROADCASTINGSTATION_H

#include <map>

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

    //interface for event request
    //there is no mutex in these interfaces, so call them in same thread;
    void addInputFile(char* filename, int id);
    void openInputFile(int id);
    void deleteInputFile(int id);
    void addOutputFile(char* filename, int index);
    void deleteOutputFile(int index);
    void startStreaming();
    void stopStreaming();
    void overlayConfigRequest();
    void setOverlayConfig(OverlayConfig& c, int id);

    //thread
    void reapFrames();
    void streamingOut();

    void reconfig();
    AVFrame* mixVideoStream();
    AVFrame* mixAudioStream();
    int overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, InputFile* file);
    bool getPicture(InputFile* is, std::shared_ptr<Frame>& pic);

    //FIXME: should be private, for test
    //output
    AVRational outputFrameRate;
    AVRational outputSampleRate;
    std::map<int, OutputFile*> outputs;
    //int openOutput(char* filename);
    SafeQueue<std::shared_ptr<Frame>, 100> outputVideoQ;
    SafeQueue<std::shared_ptr<Frame>, 100> outputAudioQ;
    int outputFrameNum;
    int64_t start_time;
    //input
    std::map<int, InputFile*> inputs;
private:
    //InputFile* input[MaxInput];
    std::mutex inputMutex;

    //may be a event with config info
    bool reconfigReq;
    AVFrame* canvas;
    bool streaming;
    int abortRequest;
};

#endif // BroadcastingStation_H
