// macOS evidence capture. Run record-demo.sh against the M7.3 Release build.
#include "OpenGL.h"
#import <AVFoundation/AVFoundation.h>
#include "Pipeline.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

using namespace lab;
namespace {
// Repeatable WASD and fire inputs at 60 fixed ticks/sec.
Motion motion(unsigned tick) {
    unsigned phase=tick%360;
    if (phase<66) return {1,0,true};
    if (phase<102) return {0,-1,true};
    if (phase<217) return {-1,0,true};
    if (phase<282) return {0,1,true};
    if (phase<331) return {1,0,true};
    return {0,-1,true};
}
void require(bool success,AVAssetWriter* writer,const char* operation) {
    if (!success) {
        NSString* reason=writer.error.localizedDescription;
        throw std::runtime_error(std::string(operation)+(reason ? ": "+std::string(reason.UTF8String) : ""));
    }
}
void record(NSString* path) {
    tiny2d::Window window(800,450,"Tiny2D Lab / M7.3 recording");
    const auto size=window.framebufferSize();
    glfwSwapInterval(0);
    std::vector<unsigned char> pixels(size.width*size.height*4);
    Pipeline pipe;
    lab::Picture picture;
    Neon neon(71);
    Display display;
    Stats last;
    NSURL* output=[NSURL fileURLWithPath:path];
    NSError* error=nil;
    AVAssetWriter* writer=[[AVAssetWriter alloc] initWithURL:output fileType:AVFileTypeMPEG4 error:&error];
    if (!writer) throw std::runtime_error(error.localizedDescription.UTF8String);
    NSDictionary* settings=@{AVVideoCodecKey:AVVideoCodecTypeH264, AVVideoWidthKey:@(size.width),
        AVVideoHeightKey:@(size.height),AVVideoCompressionPropertiesKey:@{AVVideoAverageBitRateKey:@4000000}};
    AVAssetWriterInput* input=[[AVAssetWriterInput alloc] initWithMediaType:AVMediaTypeVideo outputSettings:settings];
    NSDictionary* attrs=@{(NSString*)kCVPixelBufferPixelFormatTypeKey:@(kCVPixelFormatType_32BGRA),
        (NSString*)kCVPixelBufferWidthKey:@(size.width),(NSString*)kCVPixelBufferHeightKey:@(size.height)};
    AVAssetWriterInputPixelBufferAdaptor* adaptor=[AVAssetWriterInputPixelBufferAdaptor
        assetWriterInputPixelBufferAdaptorWithAssetWriterInput:input sourcePixelBufferAttributes:attrs];
    require([writer canAddInput:input],writer,"H.264 writer input");
    [writer addInput:input];
    require([writer startWriting],writer,"start recording");
    [writer startSessionAtSourceTime:kCMTimeZero];
    constexpr int frames=540,fps=30;
    for (int frame=0;frame<frames;++frame) @autoreleasepool {
        window.pollEvents();
        const auto current=window.framebufferSize();
        if (window.shouldClose() || current.width!=size.width || current.height!=size.height)
            throw std::runtime_error("window closed or framebuffer changed during capture");
        for (int step=0;step<2;++step) neon.update(motion(neon.tick));
        display.lighting=frame>=60;
        display.view=frame>=240 && frame<360 ? View::Light : View::Final;
        pipe.resize(size,display.lighting);
        picture.prepare(neon,display,false,window.size(),size,last);
        if (frame>=360 && frame<450) picture.lightRadii();
        pipe.draw(picture,display,last);
        glReadBuffer(GL_BACK);
        glReadPixels(0,0,size.width,size.height,GL_BGRA,GL_UNSIGNED_BYTE,pixels.data());
        tiny2d::detail::checkGL("demo readback");
        window.swapBuffers();
        while (![input isReadyForMoreMediaData]) {
            require(writer.status==AVAssetWriterStatusWriting,writer,"encode frame");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        CVPixelBufferRef buffer=nullptr;
        require(CVPixelBufferPoolCreatePixelBuffer(nullptr,adaptor.pixelBufferPool,&buffer)==kCVReturnSuccess,
                writer,"acquire encoder buffer");
        require(CVPixelBufferLockBaseAddress(buffer,0)==kCVReturnSuccess,writer,"lock encoder buffer");
        auto* dest=static_cast<unsigned char*>(CVPixelBufferGetBaseAddress(buffer));
        const auto stride=CVPixelBufferGetBytesPerRow(buffer);
        for (int y=0;y<size.height;++y)
            std::memcpy(dest+y*stride,&pixels[(size.height-y-1)*size.width*4],size.width*4);
        CVPixelBufferUnlockBaseAddress(buffer,0);
        const bool appended=[adaptor appendPixelBuffer:buffer withPresentationTime:CMTimeMake(frame,fps)];
        CVPixelBufferRelease(buffer);
        require(appended,writer,"append rendered frame");
    }
    [input markAsFinished];
    [writer endSessionAtSourceTime:CMTimeMake(frames,fps)];
    auto done=dispatch_semaphore_create(0);
    [writer finishWritingWithCompletionHandler:^{dispatch_semaphore_signal(done);}];
    dispatch_semaphore_wait(done,DISPATCH_TIME_FOREVER);
    require(writer.status==AVAssetWriterStatusCompleted,writer,"finish video");
    std::cout<<"seed=71 ticks="<<neon.tick<<" frames="<<frames<<" fps="<<fps
             <<" framebuffer="<<size.width<<'x'<<size.height<<" duration=18s\n";
}
}
int main(int argc,char** argv) {
    @autoreleasepool {
        try {
            if (argc!=2) throw std::invalid_argument("expected a new MP4 output path");
            record([NSString stringWithUTF8String:argv[1]]);
        } catch (const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
    }
}
