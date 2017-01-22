#ifndef BROADCASTINGSTATION_H
#define BROADCASTINGSTATION_H

#include <vector>
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
    enum EVENT_TYPE{
        EVENT_ADD_INPUTFILE,        //data type: inputfile* which was created
        EVENT_DELETE_INPUTFILE,     //data type: int inputfile id
        EVENT_CONSTRUCT_FILTER      //data type: layout*
    };
    //c style event
    struct Event{
        enum EVENT_TYPE type;
        void* data;
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

    //thread
    void reapFrames();
    void streamingOut();

    void reconfig();
    AVFrame* mixVideoStream();
    int overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, int index);
    bool getPicture(InputFile* is, std::shared_ptr<Frame>& pic);

    //FIXME: should be private, for test
    Layout layout;
    //output
    AVRational outputFrameRate;
    std::map<int, OutputFile*> outputs;
    //int openOutput(char* filename);
    SafeQueue<std::shared_ptr<Frame>, 50> outputVideoQ;
    int outputFrameNum;
    int64_t start_time;
private:
    //InputFile* input[MaxInput];
    std::map<int, InputFile*> inputs;

    Event configEvent;

    //may be a event with config info
    bool reconfigReq;
    AVFrame* canvas;
    bool streaming;
    int abortRequest;
};

#endif // BroadcastingStation_H
