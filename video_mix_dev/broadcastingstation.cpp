#include "broadcastingstation.h"
#include <stdio.h>
#include "demux_decode.h"
#include "encode_mux.h"

BroadcastingStation::BroadcastingStation():
    reconfigReq(false),
    outputFrameNum(0),
    abortRequest(false),
    streaming(false)
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
        //start decode thread
        input[sequence]->decodeThread = new std::thread(decode_thread, input[sequence], this);
    }
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

//time relevant
bool BroadcastingStation::getPicture(InputFile* is, std::shared_ptr<Frame>& pic)
{
    if(is->videoFrameQ.size() <= 0){
        return false;
    }
    int64_t need_pts = is->start_pts + av_rescale_q(outputFrameNum - is->start_frame_num, outputFrameRate, is->video_time_base);
    //keep the last frame
    while(1){
        if(is->videoFrameQ.size() == 1){
            pic = is->videoFrameQ.front();
            break;
        }
        std::shared_ptr<Frame> sharedFrame;
        if(is->videoFrameQ.front()->frame->pts >= need_pts){
            pic = is->videoFrameQ.front();
            break;
        }else{
            is->videoFrameQ.pop(sharedFrame);
        }
    }
    return true;
}

AVFrame* BroadcastingStation::mixVideoStream()
{
    bool getflags = false;
    AVFrame* main = av_frame_clone(canvas);
    AVFrame* outputFrame = av_frame_alloc();
    //may be add framerate control
    int64_t time_start = av_gettime_relative();
    for(int i = 0; i < layout.num; ++i) {
        //get a frame from queue
        //push and pop
        int index = layout.sequence[i];
        std::shared_ptr<Frame> sharedFrame;
        if(!input[index] || !getPicture(input[index], sharedFrame))
            continue;
        //overlayPicture should not failed, but we will take care of it
        getflags = true;
        //FIXME pts issue
        main->pts = sharedFrame->frame->pts = outputFrameNum;
        //av_frame_copy_props(main, sharedFrame->frame);
        overlayPicture(main, sharedFrame->frame, outputFrame, index);
        av_frame_unref(main);
        av_frame_move_ref(main, outputFrame);
    }
    av_frame_free(&outputFrame);
    int64_t time_end = av_gettime_relative();
    //benchmark
    printf("huheng debug mix time: %lld\n", time_end - time_start);
    //if there was no overlay pic, sleeping to avoid pushing too more canvas
    if(!getflags){
        av_usleep(20000);
    }
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
        sharedFrame->frame->pts = outputFrameNum++;
        outputVideoQ.push(sharedFrame);
        int64_t now = av_gettime_relative();
        //benchmark
        printf("huheng: output_time %lld\n", now - out_time);
        out_time = now;
    }
}

void BroadcastingStation::addOutputFile(char* filename, int index)
{
    if(outputs[index] != NULL){
        delete outputs[index];
        outputs[index] = NULL;
    }
    outputs[index] = new OutputFile(filename);
}

void BroadcastingStation::deleteOutputFile(int index){
    if(outputs.find(index) == outputs.end())
        return;
    outputs.erase(index);
}

void BroadcastingStation::startStreaming()
{
    streaming = true;
}

void BroadcastingStation::stopStreaming()
{
    streaming = false;
    for(auto& of : outputs){
        if(!of.second || of.second->valid)
            continue;
        of.second->close();
    }
}

//video and audio may be seperated
void BroadcastingStation::streamingOut()
{
    while(1){
        //consume an AVFrame
        if(!streaming){
            //throw AVFrames expired
            int64_t now = av_gettime_relative() - start_time;
            std::shared_ptr<Frame> sharedFrame;
            while(outputVideoQ.size() > 0){
                sharedFrame = outputVideoQ.front();
                int64_t pts = av_rescale(sharedFrame->frame->pts, 1000000 * outputFrameRate.num, outputFrameRate.den);
                if(pts < now - 2000000){
                    outputVideoQ.pop(sharedFrame);
                }
            }
            av_usleep(30000);
            continue;
        }
        //streaming encode and write
        //open output file
        //select a encoder
        AVCodecContext* encoder_ctx;
        for(auto& of : outputs){
            //TODO: read output config
            if(open_output_file(of.second) < 0){
                continue;
            }
            AVFormatContext* oc = of.second->fmt_ctx;
            AVDictionary * opt = NULL;   
            int ret = avformat_write_header(oc, &opt);
            if (ret < 0) {
                fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
                of.second->close();
                continue;
            }
            streaming = true;
            encoder_ctx = of.second->video_st.enc;
            of.second->valid = true;
        }
        //printf("start streaming result: %d\n", flags);
        //write packet, use codec
        std::shared_ptr<Frame> sharedFrame;
        int ret = 0;
        while(streaming && ret == 0){
            outputVideoQ.pop(sharedFrame);
            int got_packet = 0;
            AVPacket pkt = {0};
            av_init_packet(&pkt);
            ret = avcodec_encode_video2(encoder_ctx, &pkt, sharedFrame->frame, &got_packet);
            if(ret < 0) {
                fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
                continue;
            }
            if(got_packet) {
                for(auto& of : outputs){
                    if(of.second && of.second->valid)
                        ret = write_frame(of.second->fmt_ctx, &encoder_ctx->time_base, &of.second->video_st, &pkt);
                }
            } 
                
        }

    }
}
