#include "broadcastingstation.h"
#include <stdio.h>
#include "demux_decode.h"
#include "encode_mux.h"

BroadcastingStation::BroadcastingStation():
    reconfigReq(false),
    outputFrameNum(0)
{
    canvas = alloc_picture(AV_PIX_FMT_YUV420P, 1280, 720);
    outputFrameRate = {1,25};
    fill_yuv_image(canvas, 0, 1280, 720);
}

BroadcastingStation::~BroadcastingStation()
{
    for(int i=1; i<layout.num; ++i)
        if(input[i] != NULL)
            delete input[i];
    av_frame_unref(canvas);
    av_frame_free(&canvas);
    //av_frame_unref(outputFrame);
    //av_frame_free(&outputFrame);
    //delete ouput;
}

//int BroadcastingStation::openOutput(char* filename)
//{
//    output = new OutputFile(filename);
//    return open_output_file(output);
//}

void BroadcastingStation::addInputFile(char* filename, int sequence)
{
    if(sequence >= 15 || sequence < 0){
        printf("file sequence is invalid!\n");
        return;
    }
    if(input[sequence] != NULL){
        printf("sequence: %d is already exit!\n", sequence);
        return;
    }
    input[sequence] = new InputFile(filename);
    layout.sequence[sequence] = sequence;
    input[sequence]->layoutConfig = layout.overlayMap[sequence];
    //num++ must be the last step as the other thread may read it
    layout.num++;
}

void BroadcastingStation::openInputFile(int sequence)
{
    if(sequence >= 15 || sequence < 0){
        printf("file sequence is invalid!\n");
        return;
    }
    if(input[sequence] != NULL){
        open_input_file(input[sequence]);
    }
    //start decode thread
    input[sequence]->decodeThread = new std::thread(decode_thread, input[sequence]);
}

void BroadcastingStation::deleteInputFile(int sequence)
{
    //FIXME: add mutex or send event
    //num-- should be the first step because the other thread use it
    layout.num--;
    if(sequence >= 15 || sequence < 0){
        printf("file sequence is invalid!\n");
        return;
    }
    if(input[sequence] != NULL){
        if(input[sequence]->decodeThread != NULL){
            input[sequence]->abortRequest = true;
            input[sequence]->decodeThread->join();
        }
        delete input[sequence];
        input[sequence] = NULL;
    }

}

void BroadcastingStation::reconfig()
{
    //update layout

    //then set reconfigReq, avoid mutex
    reconfigReq = true;
}

int BroadcastingStation::overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, int index)
{
    //main frame is YUV420P
    //top may be YUVA420P added alpha
    //FIXME reconfig logic
    if(!layout.filterBox[index].valid ||
            top->format != input[index]->fa.fmt ||
            top->width  != input[index]->fa.width ||
            top->height != input[index]->fa.height ||
            layout.overlayMap[index].offset_y != input[index]->layoutConfig.offset_y){
        //reconfig filterbox
        FrameArgs frameargs = {outputFrameRate, top->format, top->width, top->height};
        layout.filterBox[index].config(layout.overlayMap[index],OverlayBox::STREAM_OVERLAY, &frameargs);
        input[index]->fa = frameargs;
        input[index]->layoutConfig.offset_y = layout.overlayMap[index].offset_y;
    }
    if(!layout.filterBox[index].valid){
        //memory concat
        //concatPicture();
        printf("config overlaybox failed!\n");
        return 1;
    }
    layout.filterBox[index].push_main_frame(main);
    layout.filterBox[index].push_overlayed_frame(top);
    layout.filterBox[index].pop_frame(outputFrame);
    return 0;
}

AVFrame* BroadcastingStation::mixVideoStream()
{
    AVFrame* main = av_frame_clone(canvas);
    AVFrame* outputFrame = av_frame_alloc();
    //may be add framerate control
    int64_t time_start = av_gettime_relative();
    for(int i = 0; i < layout.num; ++i) {
        //get a frame from queue
        //push and pop
        int index = layout.sequence[i];
        std::shared_ptr<Frame> sharedFrame;
        if(!input[index] || !input[index]->getPicture(sharedFrame,outputFrameRate))
            continue;
        //overlayPicture should not failed, but we will take care of it
       main->pts = sharedFrame->frame->pts;
        //av_frame_copy_props(main, sharedFrame->frame);
        overlayPicture(main, sharedFrame->frame, outputFrame, index);
        av_frame_unref(main);
        av_frame_move_ref(main, outputFrame);
    }
    av_frame_free(&outputFrame);
    int64_t time_end = av_gettime_relative();
    printf("huheng debug mix time: %lld\n", time_end - time_start);
    return main;
}
//benchmark
int64_t out_time;

//constructing thread output frames to a video queue
//todo: add pausing flag for switch SCENE
void BroadcastingStation::reapFrames()
{
    //first frame was canvas, set frameNum and clock
    start_time = av_gettime_relative();
    auto firstFrame = std::make_shared<Frame>();
    av_frame_ref(firstFrame->frame, canvas);
    firstFrame->frame->pts = 0;
    outputFrameNum = 1;
    outputVideoQ.push(firstFrame);
    //every 1/framerate output a frame
    while(!abortRequest){
        if(reconfigReq){
            //setFilterBox();
            reconfigReq = false;
        }
        //
        int64_t current_time = av_gettime_relative();
        int64_t output_time = (int64_t)outputFrameNum * 1000000 * outputFrameRate.num / outputFrameRate.den;
        while(current_time - start_time < output_time){
            av_usleep(5000); 
            current_time = av_gettime_relative();
        }
        //every 1/framerate time output a AVFrame
        AVFrame* outputFrame = mixVideoStream();
        auto sharedFrame = std::make_shared<Frame>();
        av_frame_move_ref(sharedFrame->frame, outputFrame);
        av_frame_free(&outputFrame);
        outputVideoQ.push(sharedFrame);
        outputFrameNum++;
        int64_t now = av_gettime_relative();
        printf("huheng: output_time %lld\n", now - out_time);
        out_time = now;
    }
}
