#include "broadcastingstation.h"
#include <stdio.h>
#include "demux_decode.h"
#include "encode_mux.h"
#include <vector>
#include <algorithm>

BroadcastingStation::BroadcastingStation():
    reconfigReq(false),
    outputFrameNum(0),
    abortRequest(false),
    streaming(false)
{
    canvas = alloc_picture(AV_PIX_FMT_YUV420P, Width, Height);
    outputFrameRate = {1,25};
    outputSampleRate = {1,44100};
    fill_yuv_image(canvas, 0, Width, Height);
}

BroadcastingStation::~BroadcastingStation()
{
    for(auto& input : inputs)
        if(input.second)
            delete input.second;
    for(auto& output : outputs)
        if(output.second)
            delete output.second;
    av_frame_unref(canvas);
    av_frame_free(&canvas);
}

void BroadcastingStation::openInputFile(int id)
{
    if(inputs.find(id) == inputs.end()){
        printf("not found the inputfile, id: %d\n", id);
        return;
    }
    //FIXME: add init layout info
    if(inputs[id] != NULL){
        //may be add feature about media source distinguished
        open_input_file(inputs[id]);
        //start decode thread
        inputs[id]->decodeThread = new std::thread(decode_thread, inputs[id], this);
    }
}

void BroadcastingStation::addInputFile(char* filename, int id)
{
    InputFile* oldFile = NULL;
    if(inputs.find(id) != inputs.end()){
        oldFile = inputs[id];
    }
    InputFile* file = new InputFile(filename);
    //add new file to inputs
    inputMutex.lock();
    inputs[id] = file;
    inputMutex.unlock();
    //delete old file
    if(oldFile != NULL){
        if(oldFile->decodeThread != NULL){
            oldFile->abortRequest = true;
            oldFile->decodeThread->join();
        }
        delete oldFile;
    }
    return;
}

void BroadcastingStation::deleteInputFile(int id)
{
    if(inputs.find(id) == inputs.end()){
        printf("file id: %d was not found!\n", id);
        return;
    }
    InputFile* file = inputs[id];
    //delete the file
    inputMutex.lock();
    inputs.erase(id);
    inputMutex.unlock();

    //remove done, stop decode thread and delete the file
    if(file != NULL){
        if(file->decodeThread != NULL){
            file->abortRequest = true;
            file->decodeThread->join();
        }
        delete file;
    }
}

int BroadcastingStation::overlayPicture(AVFrame* main, AVFrame* top, AVFrame* outputFrame, InputFile* file)
{
    //main frame is YUV420P
    //top may be YUVA420P added alpha
    if(!file->layoutConf.box.valid ||
            top->format != file->fa.fmt ||
            top->width  != file->fa.width ||
            top->height != file->fa.height ||
            !(file->layoutConf.box.conf == file->layoutConf.overlayConf)){
        //reconfig filterbox
        FrameArgs frameargs = {outputFrameRate, top->format, top->width, top->height};
        file->layoutConf.box.config(file->layoutConf.overlayConf, OverlayBox::STREAM_OVERLAY, &frameargs);
        file->fa = frameargs;
    }
    if(!file->layoutConf.box.valid){
        //memory concat
        //concatPicture();
        printf("config overlaybox failed!\n");
        return 1;
    }
    file->layoutConf.box.push_main_frame(main);
    file->layoutConf.box.push_overlayed_frame(top);
    file->layoutConf.box.pop_frame(outputFrame);
    return 0;
}

int a1 =0;
int a2 = 0;
int a3 =0;

//time relevant
bool BroadcastingStation::getPicture(InputFile* is, std::shared_ptr<Frame>& pic)
{
    if(!is || is->videoFrameQ.size() <= 0){
        return false;
    }
    //
    int64_t need_pts = outputFrameNum*960;
    //keep the last frame
    std::shared_ptr<Frame> sharedFrame;
    while(1){
        if(is->videoFrameQ.size() == 1){
            pic = is->videoFrameQ.front();
            a1++;
            break;
        }

        if(is->videoFrameQ.front()->frame->pts >= need_pts){
            if(!sharedFrame){
                pic = is->videoFrameQ.front();
                a2++;
            }else{
                int64_t delta1 = is->videoFrameQ.front()->frame->pts - need_pts;
                int64_t delta2 = need_pts - sharedFrame->frame->pts;
                pic = (delta1 <= delta2 ? is->videoFrameQ.front() : pic);
                a3++;
            }
            break;
        }else{
            is->videoFrameQ.pop(sharedFrame);
            pic = sharedFrame;
        }
    }
    printf("a1: %d, a2: %d, a3: %d\n", a1, a2, a3);
    return true;
}

AVFrame* BroadcastingStation::mixAudioStream()
{
    AVFrame* audioFrame = NULL;
    InputFile* is = NULL;
    int64_t need_pts = av_rescale_q(outputFrameNum, outputFrameRate, outputSampleRate);
    std::shared_ptr<Frame> sharedFrame;
    inputMutex.lock();
    for(auto it = inputs.begin(); it != inputs.end(); ++it){
        if(it->second){
            while(it->second->audioFrameQ.size() > 0){
                if(it->second->audioFrameQ.front()->frame->pts <= need_pts){
                    it->second->audioFrameQ.pop(sharedFrame);
                    if(it == inputs.begin())
                        outputAudioQ.push(sharedFrame);
                }else{
                    break;
                }
            }
        }
//        if(f.second && f.second->audioDecoder)
//            f.second->audioDecoder->frameQueue.clear();
    }
    inputMutex.unlock();
    return NULL;
    if(sharedFrame){
        audioFrame = av_frame_alloc();
        av_frame_move_ref(audioFrame, sharedFrame->frame);
    }
    return audioFrame;
}

AVFrame* BroadcastingStation::mixVideoStream()
{
    bool getflags = false;
    AVFrame* main = av_frame_clone(canvas);
    AVFrame* outputFrame = av_frame_alloc();

    //benchmark for video mixing
    int64_t time_start = av_gettime_relative();
    std::vector<InputFile*> inputSequence;
    inputMutex.lock();
    for(auto& f : inputs){
        inputSequence.push_back(f.second);
    }
    std::sort(inputSequence.begin(), inputSequence.end(),
         [](InputFile* a, InputFile*b){
                return a && b && a->layoutConf.orderNum < b->layoutConf.orderNum;
    });
    for(auto file : inputSequence) {
        //get a frame from queue
        //push and pop
        std::shared_ptr<Frame> sharedFrame;
        //get picture by pts and drop the picture expired, make video queue fresh
        if(!file || !getPicture(file, sharedFrame))
            continue;
        //overlayPicture should not failed, but we will take care of it
        getflags = true;
        //FIXME pts issue
        main->pts = sharedFrame->frame->pts = outputFrameNum;
        //av_frame_copy_props(main, sharedFrame->frame);
        overlayPicture(main, sharedFrame->frame, outputFrame, file);
        av_frame_unref(main);
        av_frame_move_ref(main, outputFrame);
    }
    inputMutex.unlock();
    av_frame_free(&outputFrame);
    int64_t time_end = av_gettime_relative();
    printf("huheng debug mix time: %lld\n", time_end - time_start);

    //if there was no overlay picure, sleeping to avoid pushing too more pure canvas
    if(!getflags){
        av_usleep(30000);
    }
    return main;
}

//void BroadcastingStation::reapAudioFrames()
//{

//}

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
//        int64_t current_time = av_gettime_relative();
//        int64_t output_time = (int64_t)outputFrameNum * 4000000 * outputFrameRate.num / outputFrameRate.den;
//        while(current_time - start_time < output_time){
//            av_usleep(5000);
//            current_time = av_gettime_relative();
//        }

        //every 1/framerate time output a AVFrame
        AVFrame* outputFrame = mixVideoStream();
        auto sharedFrame = std::make_shared<Frame>();
        av_frame_move_ref(sharedFrame->frame, outputFrame);
        av_frame_free(&outputFrame);
        sharedFrame->frame->pts = outputFrameNum++;
        outputVideoQ.push(sharedFrame);

        //reap audio frame
        //auto sharedAudioFrame = std::make_shared<Frame>();
        AVFrame* outputAudioFrame = mixAudioStream();
//        if(outputAudioFrame){
//            av_frame_move_ref(sharedAudioFrame->frame, outputAudioFrame);
//            outputAudioQ.push(sharedAudioFrame);
//        }

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

void BroadcastingStation::setOverlayConfig(OverlayConfig& c, int id)
{
    inputMutex.lock();
    if(inputs[id]){
        inputs[id]->layoutConf.overlayConf = c;
    }
    inputMutex.unlock();
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
                if(pts < now - 1000000){
                    outputVideoQ.pop(sharedFrame);
                } else {
                    break;
                }
            }
            while(outputAudioQ.size() > 0){
                sharedFrame = outputAudioQ.front();
                int64_t pts = av_rescale(sharedFrame->frame->pts, 1000000 * outputFrameRate.num, outputFrameRate.den);
                if(pts < now - 1000000){
                    outputVideoQ.pop(sharedFrame);
                } else {
                    break;
                }
            }
            av_usleep(30000);
            continue;
        }
        //streaming encode and write
        //open output file
        //select a encoder
        AVCodecContext* video_encoder_ctx;
        AVCodecContext* audio_encoder_ctx;
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
            video_encoder_ctx = of.second->video_st.enc;
            audio_encoder_ctx = of.second->audio_st.enc;
            of.second->valid = true;
        }
        //printf("start streaming result: %d\n", flags);
        //write packet, use codec
        std::shared_ptr<Frame> sharedFrame;
        int ret = 0;
        while(streaming && outputs.size() != 0 && ret == 0){
            if(outputVideoQ.size() == 0 || outputAudioQ.size() == 0){
                av_usleep(10000);
                continue;
            }
            //for test
//            outputVideoQ.clear();
//            outputAudioQ.pop(sharedFrame);
//            int got_packet = 0;
//            AVPacket pkt = {0};
//            av_init_packet(&pkt);
//            ret = avcodec_encode_audio2(audio_encoder_ctx, &pkt, sharedFrame->frame, &got_packet);
//            if(ret < 0) {
//                fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
//                continue;
//            }
//            if(got_packet) {
//                for(auto& of : outputs){
//                    if(of.second && of.second->valid)
//                        ret = write_frame(of.second->fmt_ctx, &audio_encoder_ctx->time_base, &of.second->audio_st, &pkt);
//                }
//            }
//            continue;



            if(av_compare_ts(outputVideoQ.front()->frame->pts,
                             outputFrameRate,
                             outputAudioQ.front()->frame->pts,
                             outputSampleRate) <= 0){
                //consume video frame
                outputVideoQ.pop(sharedFrame);
                int got_packet = 0;
                AVPacket pkt = {0};
                av_init_packet(&pkt);
                ret = avcodec_encode_video2(video_encoder_ctx, &pkt, sharedFrame->frame, &got_packet);
                if(ret < 0) {
                    fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
                    continue;
                }
                if(got_packet) {
                    for(auto& of : outputs){
                        if(of.second && of.second->valid)
                            ret = write_frame(of.second->fmt_ctx, &video_encoder_ctx->time_base, &of.second->video_st, &pkt);
                    }
                }
            }else{
                outputAudioQ.pop(sharedFrame);
                int got_packet = 0;
                AVPacket pkt = {0};
                av_init_packet(&pkt);
                ret = avcodec_encode_audio2(audio_encoder_ctx, &pkt, sharedFrame->frame, &got_packet);
                if(ret < 0) {
                    fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
                    continue;
                }
                if(got_packet) {
                    for(auto& of : outputs){
                        if(of.second && of.second->valid)
                            ret = write_frame(of.second->fmt_ctx, &audio_encoder_ctx->time_base, &of.second->audio_st, &pkt);
                    }
                }
            }


        }
    }
}
