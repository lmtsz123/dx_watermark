#include "VideoProcessor.h"
#include <iostream>

VideoProcessor::VideoProcessor()
    : inputFormatCtx_(nullptr)
    , outputFormatCtx_(nullptr)
    , decoder_(nullptr)
    , encoder_(nullptr)
    , decoderCtx_(nullptr)
    , encoderCtx_(nullptr)
    , swsCtx_(nullptr)
    , swsToRgbCtx_(nullptr)
    , swsToYuvCtx_(nullptr)
    , videoStream_(nullptr)
    , outVideoStream_(nullptr)
    , videoStreamIndex_(-1)
    , d3dProcessor_(nullptr)
    , width_(0)
    , height_(0)
    , pixelFormat_(AV_PIX_FMT_NONE)
    , watermarkTexture_(nullptr)
    , watermarkSRV_(nullptr)
    , videoTexture_(nullptr)
    , videoSRV_(nullptr)
{
}

VideoProcessor::~VideoProcessor()
{
    Cleanup();
}

bool VideoProcessor::GetVideoDimensions(const std::string& path, int& width, int& height)
{
    AVFormatContext* formatCtx = nullptr;
    
    // 打开输入文件
    if (avformat_open_input(&formatCtx, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "无法打开输入文件: " << path << std::endl;
        return false;
    }

    // 获取流信息
    if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
        std::cerr << "无法获取流信息" << std::endl;
        avformat_close_input(&formatCtx);
        return false;
    }

    // 查找视频流
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "未找到视频流" << std::endl;
        avformat_close_input(&formatCtx);
        return false;
    }

    // 获取视频尺寸
    AVCodecParameters* codecpar = formatCtx->streams[videoStreamIndex]->codecpar;
    width = codecpar->width;
    height = codecpar->height;

    // 清理
    avformat_close_input(&formatCtx);
    
    return true;
}

bool VideoProcessor::OpenInput(const std::string& path)
{
    // 打开输入文件
    if (avformat_open_input(&inputFormatCtx_, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "无法打开输入文件: " << path << std::endl;
        return false;
    }

    // 获取流信息
    if (avformat_find_stream_info(inputFormatCtx_, nullptr) < 0) {
        std::cerr << "无法获取流信息" << std::endl;
        return false;
    }

    // 查找视频流
    videoStreamIndex_ = -1;
    for (unsigned int i = 0; i < inputFormatCtx_->nb_streams; i++) {
        if (inputFormatCtx_->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex_ = i;
            break;
        }
    }

    if (videoStreamIndex_ == -1) {
        std::cerr << "未找到视频流" << std::endl;
        return false;
    }

    videoStream_ = inputFormatCtx_->streams[videoStreamIndex_];
    AVCodecParameters* codecpar = videoStream_->codecpar;

    // 查找解码器
    decoder_ = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder_) {
        std::cerr << "未找到解码器" << std::endl;
        return false;
    }

    // 创建解码器上下文
    decoderCtx_ = avcodec_alloc_context3(decoder_);
    if (!decoderCtx_) {
        std::cerr << "无法分配解码器上下文" << std::endl;
        return false;
    }

    if (avcodec_parameters_to_context(decoderCtx_, codecpar) < 0) {
        std::cerr << "无法复制解码器参数" << std::endl;
        return false;
    }

    // 打开解码器
    if (avcodec_open2(decoderCtx_, decoder_, nullptr) < 0) {
        std::cerr << "无法打开解码器" << std::endl;
        return false;
    }

    width_ = decoderCtx_->width;
    height_ = decoderCtx_->height;
    pixelFormat_ = decoderCtx_->pix_fmt;

    std::cout << "输入视频: " << width_ << "x" << height_ 
              << ", 格式: " << av_get_pix_fmt_name(pixelFormat_) << std::endl;

    return true;
}

bool VideoProcessor::OpenOutput(const std::string& path)
{
    // 创建输出格式上下文
    avformat_alloc_output_context2(&outputFormatCtx_, nullptr, nullptr, path.c_str());
    if (!outputFormatCtx_) {
        std::cerr << "无法创建输出上下文" << std::endl;
        return false;
    }

    // 查找编码器 (使用H.264)
    encoder_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder_) {
        std::cerr << "未找到H.264编码器" << std::endl;
        return false;
    }

    // 创建输出视频流
    outVideoStream_ = avformat_new_stream(outputFormatCtx_, nullptr);
    if (!outVideoStream_) {
        std::cerr << "无法创建输出流" << std::endl;
        return false;
    }

    // 创建编码器上下文
    encoderCtx_ = avcodec_alloc_context3(encoder_);
    if (!encoderCtx_) {
        std::cerr << "无法分配编码器上下文" << std::endl;
        return false;
    }

    // 设置编码参数
    encoderCtx_->width = width_;
    encoderCtx_->height = height_;
    encoderCtx_->time_base = videoStream_->time_base;
    encoderCtx_->framerate = av_guess_frame_rate(inputFormatCtx_, videoStream_, nullptr);
    encoderCtx_->pix_fmt = AV_PIX_FMT_YUV420P;
    encoderCtx_->bit_rate = 4000000; // 4 Mbps
    encoderCtx_->gop_size = 12;
    encoderCtx_->max_b_frames = 2;

    if (outputFormatCtx_->oformat->flags & AVFMT_GLOBALHEADER) {
        encoderCtx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 设置H.264编码参数
    AVDictionary* opts = nullptr;
    av_dict_set(&opts, "preset", "medium", 0);
    av_dict_set(&opts, "crf", "23", 0);

    // 打开编码器
    if (avcodec_open2(encoderCtx_, encoder_, &opts) < 0) {
        std::cerr << "无法打开编码器" << std::endl;
        av_dict_free(&opts);
        return false;
    }
    av_dict_free(&opts);

    // 复制编码器参数到流
    if (avcodec_parameters_from_context(outVideoStream_->codecpar, encoderCtx_) < 0) {
        std::cerr << "无法复制编码器参数" << std::endl;
        return false;
    }

    outVideoStream_->time_base = encoderCtx_->time_base;

    // 打开输出文件
    if (!(outputFormatCtx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFormatCtx_->pb, path.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "无法打开输出文件: " << path << std::endl;
            return false;
        }
    }

    // 写入文件头
    if (avformat_write_header(outputFormatCtx_, nullptr) < 0) {
        std::cerr << "写入文件头失败" << std::endl;
        return false;
    }

    std::cout << "输出视频: " << width_ << "x" << height_ << std::endl;

    return true;
}

AVFrame* VideoProcessor::ProcessFrame(AVFrame* frame,
                                      const unsigned char* watermarkData,
                                      int watermarkWidth,
                                      int watermarkHeight,
                                      float alpha)
{
    // 打印第一帧的颜色属性
    static bool firstFrame = true;
    if (firstFrame) {
        std::cout << "原始帧颜色属性: " << std::endl;
        std::cout << "  color_range: " << frame->color_range << std::endl;
        std::cout << "  color_primaries: " << frame->color_primaries << std::endl;
        std::cout << "  color_trc: " << frame->color_trc << std::endl;
        std::cout << "  colorspace: " << frame->colorspace << std::endl;
        firstFrame = false;
    }
    
    // 分配RGB缓冲区
    AVFrame* rgbFrame = av_frame_alloc();
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = width_;
    rgbFrame->height = height_;
    av_frame_get_buffer(rgbFrame, 0);

    // 转换为RGB（使用缓存的上下文）
    sws_scale(swsToRgbCtx_, frame->data, frame->linesize, 0, height_,
              rgbFrame->data, rgbFrame->linesize);

    // 由于FFmpeg的linesize可能有padding，需要复制到紧密排列的缓冲区
    std::vector<unsigned char> tightRgbData(width_ * height_ * 3);
    for (int y = 0; y < height_; y++) {
        memcpy(tightRgbData.data() + y * width_ * 3,
               rgbFrame->data[0] + y * rgbFrame->linesize[0],
               width_ * 3);
    }

    // 更新视频纹理数据（不重新创建纹理）
    if (!d3dProcessor_->UpdateTextureData(videoTexture_, tightRgbData.data(), width_, height_)) {
        std::cerr << "更新视频纹理失败" << std::endl;
        av_frame_free(&rgbFrame);
        return nullptr;
    }

    // GPU混合
    std::vector<unsigned char> blendedData(width_ * height_ * 3);
    if (!d3dProcessor_->BlendTextures(videoSRV_, watermarkSRV_, alpha, blendedData.data())) {
        std::cerr << "GPU混合失败" << std::endl;
        av_frame_free(&rgbFrame);
        return nullptr;
    }

    // 将混合后的RGB转回YUV420P（创建新的YUV帧）
    AVFrame* yuvFrame = av_frame_alloc();
    yuvFrame->format = AV_PIX_FMT_YUV420P;
    yuvFrame->width = width_;
    yuvFrame->height = height_;
    av_frame_get_buffer(yuvFrame, 0);
    
    // 复制原始帧的属性
    yuvFrame->pts = frame->pts;
    yuvFrame->pkt_dts = frame->pkt_dts;
    yuvFrame->color_range = frame->color_range;
    yuvFrame->color_primaries = frame->color_primaries;
    yuvFrame->color_trc = frame->color_trc;
    yuvFrame->colorspace = frame->colorspace;
    yuvFrame->pict_type = AV_PICTURE_TYPE_NONE;

    // 创建临时RGB帧用于转换
    AVFrame* tempRgbFrame = av_frame_alloc();
    tempRgbFrame->format = AV_PIX_FMT_RGB24;
    tempRgbFrame->width = width_;
    tempRgbFrame->height = height_;
    av_frame_get_buffer(tempRgbFrame, 0);

    // 复制混合后的数据
    memcpy(tempRgbFrame->data[0], blendedData.data(), width_ * height_ * 3);

    // 转换回YUV（写入新分配的yuvFrame）
    sws_scale(swsToYuvCtx_, tempRgbFrame->data, tempRgbFrame->linesize, 0, height_,
              yuvFrame->data, yuvFrame->linesize);

    av_frame_free(&tempRgbFrame);
    av_frame_free(&rgbFrame);

    return yuvFrame;
}

bool VideoProcessor::ProcessVideo(const std::string& inputPath,
                                  const std::string& outputPath,
                                  const unsigned char* watermarkData,
                                  int watermarkWidth,
                                  int watermarkHeight,
                                  float alpha)
{
    // 打开输入
    if (!OpenInput(inputPath)) {
        return false;
    }

    // 打开输出
    if (!OpenOutput(outputPath)) {
        return false;
    }

    // 创建D3D处理器
    d3dProcessor_ = new D3DProcessor();
    if (!d3dProcessor_->Initialize(width_, height_)) {
        std::cerr << "初始化D3D处理器失败" << std::endl;
        return false;
    }

    // 创建纹理（只创建一次，所有帧共享，每帧只更新数据）
    std::cout << "创建GPU纹理..." << std::endl;
    
    // 创建水印纹理
    if (!d3dProcessor_->CreateTextureFromRGBA(watermarkData, watermarkWidth, watermarkHeight,
                                              &watermarkTexture_, &watermarkSRV_)) {
        std::cerr << "创建水印纹理失败" << std::endl;
        return false;
    }
    
    // 创建视频纹理（空纹理，每帧更新数据）
    std::vector<unsigned char> emptyData(width_ * height_ * 3, 0);
    if (!d3dProcessor_->CreateTextureFromData(emptyData.data(), width_, height_,
                                              &videoTexture_, &videoSRV_)) {
        std::cerr << "创建视频纹理失败" << std::endl;
        return false;
    }
    
    std::cout << "GPU纹理创建成功" << std::endl;
    
    // 创建颜色空间转换上下文（只创建一次）
    std::cout << "创建颜色空间转换上下文..." << std::endl;
    
    // YUV -> RGB (原始视频是 full range，所以两边都用 full range)
    swsToRgbCtx_ = sws_getContext(
        width_, height_, pixelFormat_,
        width_, height_, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!swsToRgbCtx_) {
        std::cerr << "创建YUV->RGB转换上下文失败" << std::endl;
        return false;
    }
    
    // 使用 full range (1) 匹配原始视频
    int srcRange = 1, dstRange = 1, brightness = 0;
    int contrast = 1 << 16, saturation = 1 << 16;
    const int* inv_table = sws_getCoefficients(SWS_CS_ITU709);
    const int* table = sws_getCoefficients(SWS_CS_ITU709);
    sws_setColorspaceDetails(swsToRgbCtx_, inv_table, srcRange, table, dstRange,
                            brightness, contrast, saturation);
    
    // RGB -> YUV (输出也用 full range 保持一致)
    swsToYuvCtx_ = sws_getContext(
        width_, height_, AV_PIX_FMT_RGB24,
        width_, height_, AV_PIX_FMT_YUV420P,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );
    if (!swsToYuvCtx_) {
        std::cerr << "创建RGB->YUV转换上下文失败" << std::endl;
        return false;
    }
    
    // 两边都用 full range
    srcRange = 1; dstRange = 1;
    sws_setColorspaceDetails(swsToYuvCtx_, inv_table, srcRange, table, dstRange,
                            brightness, contrast, saturation);
    
    std::cout << "颜色空间转换上下文创建成功" << std::endl;

    // 处理视频帧
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    int64_t frameCount = 0;

    std::cout << "开始处理视频帧..." << std::endl;

    while (av_read_frame(inputFormatCtx_, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex_) {
            // 发送数据包到解码器
            if (avcodec_send_packet(decoderCtx_, packet) >= 0) {
                // 接收解码后的帧
                while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
                    // 处理帧（添加水印），返回新的YUV帧
                    AVFrame* processedFrame = ProcessFrame(frame, watermarkData, watermarkWidth, watermarkHeight, alpha);
                    if (!processedFrame) {
                        std::cerr << "处理帧失败" << std::endl;
                        continue;
                    }
                    
                    // 编码处理后的帧
                    if (avcodec_send_frame(encoderCtx_, processedFrame) >= 0) {
                        AVPacket* outPacket = av_packet_alloc();
                        while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
                            av_packet_rescale_ts(outPacket, encoderCtx_->time_base, 
                                                outVideoStream_->time_base);
                            outPacket->stream_index = outVideoStream_->index;
                            av_interleaved_write_frame(outputFormatCtx_, outPacket);
                            av_packet_unref(outPacket);
                        }
                        av_packet_free(&outPacket);
                    }

                    // 释放处理后的帧
                    av_frame_free(&processedFrame);

                    frameCount++;
                    if (frameCount % 30 == 0) {
                        std::cout << "已处理 " << frameCount << " 帧" << std::endl;
                    }
                }
            }
        }
        av_packet_unref(packet);
    }

    // 刷新解码器
    avcodec_send_packet(decoderCtx_, nullptr);
    while (avcodec_receive_frame(decoderCtx_, frame) >= 0) {
        AVFrame* processedFrame = ProcessFrame(frame, watermarkData, watermarkWidth, watermarkHeight, alpha);
        if (!processedFrame) continue;
        
        avcodec_send_frame(encoderCtx_, processedFrame);
        AVPacket* outPacket = av_packet_alloc();
        while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
            av_packet_rescale_ts(outPacket, encoderCtx_->time_base, outVideoStream_->time_base);
            outPacket->stream_index = outVideoStream_->index;
            av_interleaved_write_frame(outputFormatCtx_, outPacket);
            av_packet_unref(outPacket);
        }
        av_packet_free(&outPacket);
        
        // 释放处理后的帧
        av_frame_free(&processedFrame);
        
        frameCount++;
    }

    // 刷新编码器
    avcodec_send_frame(encoderCtx_, nullptr);
    AVPacket* outPacket = av_packet_alloc();
    while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
        av_packet_rescale_ts(outPacket, encoderCtx_->time_base, outVideoStream_->time_base);
        outPacket->stream_index = outVideoStream_->index;
        av_interleaved_write_frame(outputFormatCtx_, outPacket);
        av_packet_unref(outPacket);
    }
    av_packet_free(&outPacket);

    // 写入文件尾
    av_write_trailer(outputFormatCtx_);

    std::cout << "处理完成！总共 " << frameCount << " 帧" << std::endl;

    av_frame_free(&frame);
    av_packet_free(&packet);

    return true;
}

void VideoProcessor::Cleanup()
{
    // 释放纹理
    if (videoSRV_) {
        videoSRV_->Release();
        videoSRV_ = nullptr;
    }
    
    if (videoTexture_) {
        videoTexture_->Release();
        videoTexture_ = nullptr;
    }
    
    if (watermarkSRV_) {
        watermarkSRV_->Release();
        watermarkSRV_ = nullptr;
    }
    
    if (watermarkTexture_) {
        watermarkTexture_->Release();
        watermarkTexture_ = nullptr;
    }
    
    if (d3dProcessor_) {
        delete d3dProcessor_;
        d3dProcessor_ = nullptr;
    }

    if (decoderCtx_) {
        avcodec_free_context(&decoderCtx_);
    }

    if (encoderCtx_) {
        avcodec_free_context(&encoderCtx_);
    }

    if (inputFormatCtx_) {
        avformat_close_input(&inputFormatCtx_);
    }

    if (outputFormatCtx_) {
        if (!(outputFormatCtx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputFormatCtx_->pb);
        }
        avformat_free_context(outputFormatCtx_);
    }

    if (swsCtx_) {
        sws_freeContext(swsCtx_);
    }
    
    if (swsToRgbCtx_) {
        sws_freeContext(swsToRgbCtx_);
        swsToRgbCtx_ = nullptr;
    }
    
    if (swsToYuvCtx_) {
        sws_freeContext(swsToYuvCtx_);
        swsToYuvCtx_ = nullptr;
    }
}