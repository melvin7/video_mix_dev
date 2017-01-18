#include "scene.h"
#include <stdio.h>
#include "demux_decode.h"
#include "encode_mux.h"

Scene::Scene():reconfigReq(false)
{
    canvas = alloc_picture(AV_PIX_FMT_YUV420P, 1280, 720);
    outputFrameRate = {1,25};
    fill_yuv_image(canvas, 0, 1280, 720);
}

Scene::~Scene()
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

//int Scene::openOutput(char* filename)
//{
//    output = new OutputFile(filename);
//    return open_output_file(output);
//}

void Scene::addInputFile(char* filename, int sequence)
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
    layout.sequence[layout.num] = layout.num;
    layout.num++;
}

void Scene::openInputFile(int sequence)
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

void Scene::deleteInputFile(int sequence)
{
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
    layout.num--;
}

void Scene::reconfig()
{
    //update layout

    //then set reconfigReq, avoid mutex
    reconfigReq = true;
}

int Scene::overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, int index)
{
    if(!layout.filterBox[index].valid){
        layout.filterBox[index].config(layout.overlayMap[index],OverlayBox::STREAM_OVERLAY,input[index]->video_dec_ctx);
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

AVFrame* Scene::mixVideoStream()
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

//constructing thread output frames to a video queue
//todo: add pausing flag for switch scenes
void Scene::constructing(SafeQueue<std::shared_ptr<Frame>, 10>& videoq)
{
    while(!abortRequest){
        if(reconfigReq){
            //setFilterBox();
            reconfigReq = false;
        }
        //every 1/framerate time output a AVFrame
        AVFrame* outputFrame = mixVideoStream();
        auto sharedFrame = std::make_shared<Frame>();
        av_frame_move_ref(sharedFrame->frame, outputFrame);
        av_frame_free(&outputFrame);
        videoq.push(sharedFrame);
    }
}
