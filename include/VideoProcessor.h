#ifndef VIDEO_PROCESSOR_H
#define VIDEO_PROCESSOR_H

#include "D3DProcessor.h"
#include <string>
#include <d3d11.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class VideoProcessor
{
public:
    VideoProcessor();
    ~VideoProcessor();

    bool ProcessVideo(const std::string& inputPath,
                     const std::string& outputPath,
                     const unsigned char* watermarkData,
                     int watermarkWidth,
                     int watermarkHeight,
                     float alpha = 0.3f);

    // 静态方法：获取视频尺寸
    static bool GetVideoDimensions(const std::string& path, int& width, int& height);

private:
    bool OpenInput(const std::string& path);
    bool OpenOutput(const std::string& path);
    AVFrame* ProcessFrame(AVFrame* frame, const unsigned char* watermarkData,
                         int watermarkWidth, int watermarkHeight, float alpha);
    void Cleanup();

    // FFmpeg相关
    AVFormatContext* inputFormatCtx_;
    AVFormatContext* outputFormatCtx_;
    const AVCodec* decoder_;
    const AVCodec* encoder_;
    AVCodecContext* decoderCtx_;
    AVCodecContext* encoderCtx_;
    SwsContext* swsCtx_;
    SwsContext* swsToRgbCtx_;   // 缓存 YUV->RGB 转换上下文
    SwsContext* swsToYuvCtx_;   // 缓存 RGB->YUV 转换上下文
    AVStream* videoStream_;
    AVStream* outVideoStream_;
    int videoStreamIndex_;

    // DirectX处理器
    D3DProcessor* d3dProcessor_;

    // 视频参数
    int width_;
    int height_;
    AVPixelFormat pixelFormat_;
    
    // 缓存的纹理（避免每帧重新创建）
    ID3D11Texture2D* watermarkTexture_;
    ID3D11ShaderResourceView* watermarkSRV_;
    ID3D11Texture2D* videoTexture_;
    ID3D11ShaderResourceView* videoSRV_;
};

#endif