#include "ScreenRecorder.h"
#include "DXGICapture.h"
#include "D3DProcessor.h"
#include <iostream>
#include <thread>
#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

ScreenRecorder::ScreenRecorder()
    : capture_(nullptr)
    , d3dProcessor_(nullptr)
    , formatCtx_(nullptr)
    , codecCtx_(nullptr)
    , videoStream_(nullptr)
    , swsCtx_(nullptr)
    , width_(0)
    , height_(0)
    , fps_(30)
    , frameCount_(0)
    , watermarkData_(nullptr)
    , watermarkWidth_(0)
    , watermarkHeight_(0)
    , alpha_(0.3f)
    , videoTexture_(nullptr)
    , watermarkTexture_(nullptr)
    , videoSRV_(nullptr)
    , watermarkSRV_(nullptr)
{
}

ScreenRecorder::~ScreenRecorder()
{
    Cleanup();
}

bool ScreenRecorder::RecordScreen(const std::string& outputPath,
                                  int duration,
                                  int fps,
                                  const unsigned char* watermarkData,
                                  int watermarkWidth,
                                  int watermarkHeight,
                                  float alpha)
{
    fps_ = fps;
    watermarkData_ = watermarkData;
    watermarkWidth_ = watermarkWidth;
    watermarkHeight_ = watermarkHeight;
    alpha_ = alpha;

    std::cout << "初始化桌面捕获..." << std::endl;
    if (!InitializeCapture()) {
        std::cerr << "初始化桌面捕获失败" << std::endl;
        return false;
    }

    std::cout << "桌面尺寸: " << width_ << "x" << height_ << std::endl;

    std::cout << "初始化视频编码器..." << std::endl;
    if (!InitializeEncoder(outputPath, width_, height_, fps)) {
        std::cerr << "初始化编码器失败" << std::endl;
        return false;
    }

    int totalFrames = duration * fps;
    std::cout << "开始录制 " << duration << " 秒 (" << totalFrames << " 帧)..." << std::endl;

    auto startTime = std::chrono::steady_clock::now();
    auto frameDuration = std::chrono::milliseconds(1000 / fps);

    for (int i = 0; i < totalFrames; i++) {
        auto frameStart = std::chrono::steady_clock::now();

        if (!CaptureAndProcessFrame()) {
            std::cerr << "捕获帧失败: " << i << std::endl;
            continue;
        }

        if ((i + 1) % fps == 0) {
            std::cout << "已录制 " << (i + 1) / fps << " 秒..." << std::endl;
        }

        // 控制帧率
        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart);
        if (elapsed < frameDuration) {
            std::this_thread::sleep_for(frameDuration - elapsed);
        }
    }

    std::cout << "录制完成，正在写入文件..." << std::endl;

    // 刷新编码器
    avcodec_send_frame(codecCtx_, nullptr);
    AVPacket* pkt = av_packet_alloc();
    while (avcodec_receive_packet(codecCtx_, pkt) == 0) {
        av_packet_rescale_ts(pkt, codecCtx_->time_base, videoStream_->time_base);
        pkt->stream_index = videoStream_->index;
        av_interleaved_write_frame(formatCtx_, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    av_write_trailer(formatCtx_);

    std::cout << "录制成功！" << std::endl;
    return true;
}

bool ScreenRecorder::InitializeCapture()
{
    capture_ = new DXGICapture();
    if (!capture_->Initialize()) {
        return false;
    }

    width_ = capture_->GetWidth();
    height_ = capture_->GetHeight();

    // 初始化D3D处理器用于水印混合
    d3dProcessor_ = new D3DProcessor();
    if (!d3dProcessor_->Initialize(width_, height_)) {
        std::cerr << "初始化D3D处理器失败" << std::endl;
        return false;
    }

    // 创建水印纹理（假设水印尺寸与桌面相同）
    if (watermarkData_) {
        ID3D11Texture2D* watermarkTex = nullptr;
        ID3D11ShaderResourceView* watermarkSrv = nullptr;
        
        if (!d3dProcessor_->CreateTextureFromRGBA(watermarkData_, watermarkWidth_, watermarkHeight_, 
                                                  &watermarkTex, &watermarkSrv)) {
            std::cerr << "创建水印纹理失败" << std::endl;
            return false;
        }
        
        watermarkTexture_ = watermarkTex;
        watermarkSRV_ = watermarkSrv;
    }

    return true;
}

bool ScreenRecorder::InitializeEncoder(const std::string& outputPath, int width, int height, int fps)
{
    // 分配输出上下文
    avformat_alloc_output_context2(&formatCtx_, nullptr, nullptr, outputPath.c_str());
    if (!formatCtx_) {
        std::cerr << "无法创建输出上下文" << std::endl;
        return false;
    }

    // 查找编码器
    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        std::cerr << "找不到H264编码器" << std::endl;
        return false;
    }

    // 创建视频流
    videoStream_ = avformat_new_stream(formatCtx_, nullptr);
    if (!videoStream_) {
        std::cerr << "无法创建视频流" << std::endl;
        return false;
    }

    // 创建编码器上下文
    codecCtx_ = avcodec_alloc_context3(codec);
    if (!codecCtx_) {
        std::cerr << "无法创建编码器上下文" << std::endl;
        return false;
    }

    // 设置编码参数
    codecCtx_->codec_id = AV_CODEC_ID_H264;
    codecCtx_->codec_type = AVMEDIA_TYPE_VIDEO;
    codecCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codecCtx_->width = width;
    codecCtx_->height = height;
    codecCtx_->time_base = AVRational{1, fps};
    codecCtx_->framerate = AVRational{fps, 1};
    codecCtx_->gop_size = fps;
    codecCtx_->max_b_frames = 2;
    codecCtx_->bit_rate = 4000000;

    // H264编码选项
    av_opt_set(codecCtx_->priv_data, "preset", "fast", 0);
    av_opt_set(codecCtx_->priv_data, "tune", "zerolatency", 0);

    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        codecCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开编码器
    if (avcodec_open2(codecCtx_, codec, nullptr) < 0) {
        std::cerr << "无法打开编码器" << std::endl;
        return false;
    }

    // 复制编码器参数到流
    avcodec_parameters_from_context(videoStream_->codecpar, codecCtx_);
    videoStream_->time_base = codecCtx_->time_base;

    // 打开输出文件
    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&formatCtx_->pb, outputPath.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "无法打开输出文件" << std::endl;
            return false;
        }
    }

    // 写入文件头
    if (avformat_write_header(formatCtx_, nullptr) < 0) {
        std::cerr << "写入文件头失败" << std::endl;
        return false;
    }

    // 初始化颜色空间转换
    swsCtx_ = sws_getContext(
        width, height, AV_PIX_FMT_RGB24,
        width, height, AV_PIX_FMT_YUV420P,
        SWS_BICUBIC, nullptr, nullptr, nullptr
    );

    if (!swsCtx_) {
        std::cerr << "无法创建颜色空间转换器" << std::endl;
        return false;
    }

    frameCount_ = 0;
    return true;
}

bool ScreenRecorder::CaptureAndProcessFrame()
{
    // 捕获桌面帧（包含鼠标）
    if (!capture_->CaptureFrame()) {
        return false;
    }

    ID3D11Texture2D* capturedTexture = capture_->GetCapturedTexture();
    if (!capturedTexture) {
        return false;
    }

    // 从D3D11纹理读取数据到CPU内存
    D3D11_TEXTURE2D_DESC desc;
    capturedTexture->GetDesc(&desc);

    // 创建临时staging纹理用于读取
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    capturedTexture->GetDevice(device.GetAddressOf());

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(context.GetAddressOf());

    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "创建staging纹理失败" << std::endl;
        return false;
    }

    context->CopyResource(stagingTexture.Get(), capturedTexture);

    // 读取像素数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        std::cerr << "映射纹理失败" << std::endl;
        return false;
    }

    // 转换BGRA到RGBA（紧密排列）
    std::vector<unsigned char> rgbaData(width_ * height_ * 4);
    const unsigned char* src = static_cast<const unsigned char*>(mapped.pData);
    
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            int srcIdx = y * mapped.RowPitch + x * 4;
            int dstIdx = (y * width_ + x) * 4;
            
            rgbaData[dstIdx + 0] = src[srcIdx + 2]; // R
            rgbaData[dstIdx + 1] = src[srcIdx + 1]; // G
            rgbaData[dstIdx + 2] = src[srcIdx + 0]; // B
            rgbaData[dstIdx + 3] = src[srcIdx + 3]; // A
        }
    }

    context->Unmap(stagingTexture.Get(), 0);

    // 如果有水印，进行混合
    std::vector<unsigned char> finalRgbData;
    if (watermarkData_ && watermarkSRV_) {
        ID3D11Texture2D* videoTex = static_cast<ID3D11Texture2D*>(videoTexture_);
        ID3D11ShaderResourceView* videoSrv = static_cast<ID3D11ShaderResourceView*>(videoSRV_);
        ID3D11ShaderResourceView* watermarkSrv = static_cast<ID3D11ShaderResourceView*>(watermarkSRV_);
        
        // 创建或更新视频纹理
        if (!videoTexture_) {
            if (!d3dProcessor_->CreateTextureFromRGBA(rgbaData.data(), width_, height_, 
                                                     &videoTex, &videoSrv)) {
                std::cerr << "创建视频纹理失败" << std::endl;
                return false;
            }
            videoTexture_ = videoTex;
            videoSRV_ = videoSrv;
        } else {
            // UpdateTextureData期望RGB格式，但我们有RGBA，所以直接使用UpdateSubresource
            Microsoft::WRL::ComPtr<ID3D11Device> device;
            videoTex->GetDevice(device.GetAddressOf());
            
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> ctx;
            device->GetImmediateContext(ctx.GetAddressOf());
            
            ctx->UpdateSubresource(videoTex, 0, nullptr, rgbaData.data(), width_ * 4, 0);
        }

        // GPU混合（输出RGB格式）
        finalRgbData.resize(width_ * height_ * 3);
        if (!d3dProcessor_->BlendTextures(videoSrv, watermarkSrv, alpha_, 
                                         finalRgbData.data())) {
            std::cerr << "GPU混合失败" << std::endl;
            return false;
        }
    } else {
        // 转换RGBA到RGB
        finalRgbData.resize(width_ * height_ * 3);
        for (int i = 0; i < width_ * height_; i++) {
            finalRgbData[i * 3 + 0] = rgbaData[i * 4 + 0]; // R
            finalRgbData[i * 3 + 1] = rgbaData[i * 4 + 1]; // G
            finalRgbData[i * 3 + 2] = rgbaData[i * 4 + 2]; // B
        }
    }

    // 编码帧
    AVFrame* rgbFrame = av_frame_alloc();
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = width_;
    rgbFrame->height = height_;
    av_frame_get_buffer(rgbFrame, 0);

    // 逐行复制（考虑linesize）
    for (int y = 0; y < height_; y++) {
        memcpy(rgbFrame->data[0] + y * rgbFrame->linesize[0],
               finalRgbData.data() + y * width_ * 3,
               width_ * 3);
    }

    // 转换为YUV420P
    AVFrame* yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = width_;
    yuvFrame->height = height_;
    yuvFrame->pts = frameCount_++;
    av_frame_get_buffer(yuvFrame, 0);

    sws_scale(swsCtx_, rgbFrame->data, rgbFrame->linesize, 0, height_,
              yuvFrame->data, yuvFrame->linesize);

    av_frame_free(&rgbFrame);

    // 编码
    int ret = avcodec_send_frame(codecCtx_, yuvFrame);
    av_frame_free(&yuvFrame);

    if (ret < 0) {
        std::cerr << "发送帧到编码器失败" << std::endl;
        return false;
    }

    // 接收编码后的包
    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(codecCtx_, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            std::cerr << "接收编码包失败" << std::endl;
            av_packet_free(&pkt);
            return false;
        }

        av_packet_rescale_ts(pkt, codecCtx_->time_base, videoStream_->time_base);
        pkt->stream_index = videoStream_->index;
        
        ret = av_interleaved_write_frame(formatCtx_, pkt);
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);

    return true;
}

void ScreenRecorder::Cleanup()
{
    if (swsCtx_) {
        sws_freeContext(swsCtx_);
        swsCtx_ = nullptr;
    }

    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
    }

    if (formatCtx_) {
        if (!(formatCtx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&formatCtx_->pb);
        }
        avformat_free_context(formatCtx_);
        formatCtx_ = nullptr;
    }

    if (d3dProcessor_) {
        delete d3dProcessor_;
        d3dProcessor_ = nullptr;
    }

    if (capture_) {
        delete capture_;
        capture_ = nullptr;
    }
}

