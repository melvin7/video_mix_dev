#include "scene.h"
#include <stdio.h>
#include "demux_decode.h"
#include "encode_mux.h"

Scene::Scene():input(MaxInput, NULL),inputConfig(MaxInput,{1,Width,Height,0,0}),
    inputNum(0),
    reconfigReq(false)
{
    canvas = alloc_picture(AV_PIX_FMT_YUV420P, 1280, 720);
    fill_yuv_image(canvas, 0, 1280, 720);
}

Scene::~Scene()
{
    for(int i=1; i<input.size(); ++i)
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
    input[sequence]->decodeThread = new std::thread(decode_thread, *input[sequence]);
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

void Scene::overlayPicture(InputFile* is, AVFrame* main, AVFrame* outputFrame)
{
    is->getPicture(time);
}

void Scene::mixVideoStream()
{
    AVFrame* main = av_frame_clone(canvas);
    AVFrame* outputFrame = av_frame_alloc();
    int64_t time = av_gettime_relative();
    for(int i = 0; i < layout.num; ++i){
        //get a frame from queue
        //push and pop
        int index = layout.sequence[i];
        overlayPicture(Input[index], AVFrame* main, AVFrame* outputFrame);
        av_frame_unref(main);
        av_frame_move_ref(main, outputFrame);
    }

}

void Scene::constructing()
{
    while(!abortRequest){
        if(reconfigReq){
            //setFilterBox();
            reconfigReq = false;
        }
        reapAVFrame();

    }
}
