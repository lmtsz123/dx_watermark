#pragma once

#include <string>
#include <vector>

class DXGICapture;
class D3DProcessor;

class ScreenRecorder {
public:
    ScreenRecorder();
    ~ScreenRecorder();

    // 录制桌面并叠加水印
    // duration: 录制时长（秒）
    // fps: 帧率
    // watermarkData: 水印数据（RGBA格式）
    // watermarkWidth/Height: 水印尺寸
    // alpha: 水印透明度
    bool RecordScreen(const std::string& outputPath, 
                     int duration, 
                     int fps,
                     const unsigned char* watermarkData,
                     int watermarkWidth,
                     int watermarkHeight,
                     float alpha);

private:
    bool InitializeCapture();
    bool InitializeEncoder(const std::string& outputPath, int width, int height, int fps);
    bool CaptureAndProcessFrame();
    void Cleanup();

    DXGICapture* capture_;
    D3DProcessor* d3dProcessor_;
    
    // FFmpeg编码相关
    struct AVFormatContext* formatCtx_;
    struct AVCodecContext* codecCtx_;
    struct AVStream* videoStream_;
    struct SwsContext* swsCtx_;
    
    int width_;
    int height_;
    int fps_;
    int64_t frameCount_;
    
    // 水印数据
    const unsigned char* watermarkData_;
    int watermarkWidth_;
    int watermarkHeight_;
    float alpha_;
    
    // DirectX纹理和资源
    void* videoTexture_;
    void* watermarkTexture_;
    void* videoSRV_;
    void* watermarkSRV_;
};

