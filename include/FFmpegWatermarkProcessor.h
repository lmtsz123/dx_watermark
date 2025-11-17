#ifndef FFMPEG_WATERMARK_PROCESSOR_H
#define FFMPEG_WATERMARK_PROCESSOR_H

#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

class FFmpegWatermarkProcessor
{
public:
    FFmpegWatermarkProcessor();
    ~FFmpegWatermarkProcessor();

    // 使用FFmpeg filter处理视频水印
    bool ProcessVideo(const std::string& inputPath,
                     const std::string& outputPath,
                     const std::string& watermarkPath,
                     float alpha = 0.3f);

    // 静态方法：获取视频尺寸
    static bool GetVideoDimensions(const std::string& path, int& width, int& height);

private:
    bool OpenInput(const std::string& path);
    bool OpenOutput(const std::string& path);
    bool InitializeFilter(const std::string& watermarkPath, float alpha);
    void Cleanup();

    // FFmpeg相关
    AVFormatContext* inputFormatCtx_;
    AVFormatContext* outputFormatCtx_;
    const AVCodec* decoder_;
    const AVCodec* encoder_;
    AVCodecContext* decoderCtx_;
    AVCodecContext* encoderCtx_;
    AVStream* videoStream_;
    AVStream* outVideoStream_;
    int videoStreamIndex_;

    // Filter相关
    AVFilterGraph* filterGraph_;
    AVFilterContext* bufferSrcCtx_;
    AVFilterContext* bufferSinkCtx_;

    // 视频参数
    int width_;
    int height_;
    AVPixelFormat pixelFormat_;
};

#endif

