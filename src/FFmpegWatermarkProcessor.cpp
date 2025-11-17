#include "FFmpegWatermarkProcessor.h"
#include <iostream>
#include <sstream>

FFmpegWatermarkProcessor::FFmpegWatermarkProcessor()
    : inputFormatCtx_(nullptr)
    , outputFormatCtx_(nullptr)
    , decoder_(nullptr)
    , encoder_(nullptr)
    , decoderCtx_(nullptr)
    , encoderCtx_(nullptr)
    , videoStream_(nullptr)
    , outVideoStream_(nullptr)
    , videoStreamIndex_(-1)
    , filterGraph_(nullptr)
    , bufferSrcCtx_(nullptr)
    , bufferSinkCtx_(nullptr)
    , width_(0)
    , height_(0)
    , pixelFormat_(AV_PIX_FMT_NONE)
{
}

FFmpegWatermarkProcessor::~FFmpegWatermarkProcessor()
{
    Cleanup();
}

bool FFmpegWatermarkProcessor::GetVideoDimensions(const std::string& path, int& width, int& height)
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

bool FFmpegWatermarkProcessor::OpenInput(const std::string& path)
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

bool FFmpegWatermarkProcessor::OpenOutput(const std::string& path)
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

bool FFmpegWatermarkProcessor::InitializeFilter(const std::string& watermarkPath, float alpha)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret;
    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut* outputs = avfilter_inout_alloc();
    AVFilterInOut* inputs = avfilter_inout_alloc();

    filterGraph_ = avfilter_graph_alloc();
    if (!outputs || !inputs || !filterGraph_) {
        std::cerr << "无法分配filter资源" << std::endl;
        return false;
    }

    // 创建buffer source
    std::ostringstream args;
    args << "video_size=" << width_ << "x" << height_
         << ":pix_fmt=" << static_cast<int>(pixelFormat_)
         << ":time_base=" << videoStream_->time_base.num << "/" << videoStream_->time_base.den
         << ":pixel_aspect=" << decoderCtx_->sample_aspect_ratio.num << "/" 
         << (decoderCtx_->sample_aspect_ratio.den ? decoderCtx_->sample_aspect_ratio.den : 1);

    std::cout << "Buffer source参数: " << args.str() << std::endl;

    ret = avfilter_graph_create_filter(&bufferSrcCtx_, buffersrc, "in",
                                       args.str().c_str(), nullptr, filterGraph_);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法创建buffer source: " << errbuf << std::endl;
        return false;
    }

    // 创建buffer sink
    ret = avfilter_graph_create_filter(&bufferSinkCtx_, buffersink, "out",
                                       nullptr, nullptr, filterGraph_);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法创建buffer sink: " << errbuf << std::endl;
        return false;
    }

    // 设置输出格式 - 使用av_opt_set_bin代替av_opt_set_int_list
    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    ret = av_opt_set_bin(bufferSinkCtx_, "pix_fmts", (uint8_t*)pix_fmts, 
                         sizeof(pix_fmts), AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法设置输出像素格式: " << errbuf << std::endl;
        return false;
    }

    // 设置输出和输入
    outputs->name = av_strdup("in");
    outputs->filter_ctx = bufferSrcCtx_;
    outputs->pad_idx = 0;
    outputs->next = nullptr;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = bufferSinkCtx_;
    inputs->pad_idx = 0;
    inputs->next = nullptr;

    // 构建filter描述字符串 - 转义路径中的特殊字符
    std::string escapedPath = watermarkPath;
    // 替换反斜杠为正斜杠（Windows路径）
    for (size_t i = 0; i < escapedPath.length(); i++) {
        if (escapedPath[i] == '\\') {
            escapedPath[i] = '/';
        }
    }
    
    // 使用overlay filter叠加水印
    std::ostringstream filterDesc;
    filterDesc << "movie='" << escapedPath << "'"
               << ",scale=" << width_ << ":" << height_
               << "[wm];[in][wm]overlay=0:0";

    std::cout << "Filter描述: " << filterDesc.str() << std::endl;

    // 解析filter图
    ret = avfilter_graph_parse_ptr(filterGraph_, filterDesc.str().c_str(),
                                    &inputs, &outputs, nullptr);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法解析filter图: " << errbuf << std::endl;
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return false;
    }

    // 配置filter图
    ret = avfilter_graph_config(filterGraph_, nullptr);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "无法配置filter图: " << errbuf << std::endl;
        avfilter_inout_free(&inputs);
        avfilter_inout_free(&outputs);
        return false;
    }

    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    std::cout << "Filter初始化成功" << std::endl;
    return true;
}

bool FFmpegWatermarkProcessor::ProcessVideo(const std::string& inputPath,
                                            const std::string& outputPath,
                                            const std::string& watermarkPath,
                                            float alpha)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    int ret;
    
    // 打开输入
    if (!OpenInput(inputPath)) {
        return false;
    }

    // 打开输出
    if (!OpenOutput(outputPath)) {
        return false;
    }

    // 初始化filter
    if (!InitializeFilter(watermarkPath, alpha)) {
        return false;
    }

    // 处理视频帧
    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    AVFrame* filtFrame = av_frame_alloc();

    if (!packet || !frame || !filtFrame) {
        std::cerr << "无法分配帧/数据包内存" << std::endl;
        return false;
    }

    int64_t frameCount = 0;
    int64_t encodedFrames = 0;

    std::cout << "开始处理视频帧..." << std::endl;

    while (av_read_frame(inputFormatCtx_, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex_) {
            // 发送数据包到解码器
            ret = avcodec_send_packet(decoderCtx_, packet);
            if (ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "发送数据包到解码器失败: " << errbuf << std::endl;
                av_packet_unref(packet);
                continue;
            }
            
            // 接收解码后的帧
            while ((ret = avcodec_receive_frame(decoderCtx_, frame)) >= 0) {
                frameCount++;
                
                // 将帧推送到filter
                ret = av_buffersrc_add_frame_flags(bufferSrcCtx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
                if (ret < 0) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "推送帧到filter失败: " << errbuf << std::endl;
                    break;
                }

                // 从filter获取处理后的帧
                while ((ret = av_buffersink_get_frame(bufferSinkCtx_, filtFrame)) >= 0) {
                    // 编码处理后的帧
                    filtFrame->pict_type = AV_PICTURE_TYPE_NONE;
                    
                    ret = avcodec_send_frame(encoderCtx_, filtFrame);
                    if (ret < 0) {
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        std::cerr << "发送帧到编码器失败: " << errbuf << std::endl;
                        av_frame_unref(filtFrame);
                        continue;
                    }
                    
                    AVPacket* outPacket = av_packet_alloc();
                    while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
                        av_packet_rescale_ts(outPacket, encoderCtx_->time_base, 
                                            outVideoStream_->time_base);
                        outPacket->stream_index = outVideoStream_->index;
                        
                        ret = av_interleaved_write_frame(outputFormatCtx_, outPacket);
                        if (ret < 0) {
                            av_strerror(ret, errbuf, sizeof(errbuf));
                            std::cerr << "写入数据包失败: " << errbuf << std::endl;
                        } else {
                            encodedFrames++;
                        }
                        av_packet_unref(outPacket);
                    }
                    av_packet_free(&outPacket);

                    av_frame_unref(filtFrame);
                    
                    if (encodedFrames % 30 == 0 && encodedFrames > 0) {
                        std::cout << "已解码 " << frameCount << " 帧, 已编码 " << encodedFrames << " 帧" << std::endl;
                    }
                }
                
                if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF && ret < 0) {
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    std::cerr << "从filter获取帧失败: " << errbuf << std::endl;
                }
            }
            
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF && ret < 0) {
                av_strerror(ret, errbuf, sizeof(errbuf));
                std::cerr << "接收解码帧失败: " << errbuf << std::endl;
            }
        }
        av_packet_unref(packet);
    }
    
    std::cout << "读取完成，开始刷新解码器..." << std::endl;

    // 刷新解码器
    std::cout << "刷新解码器..." << std::endl;
    avcodec_send_packet(decoderCtx_, nullptr);
    while ((ret = avcodec_receive_frame(decoderCtx_, frame)) >= 0) {
        frameCount++;
        av_buffersrc_add_frame_flags(bufferSrcCtx_, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        
        while ((ret = av_buffersink_get_frame(bufferSinkCtx_, filtFrame)) >= 0) {
            filtFrame->pict_type = AV_PICTURE_TYPE_NONE;
            avcodec_send_frame(encoderCtx_, filtFrame);
            
            AVPacket* outPacket = av_packet_alloc();
            while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
                av_packet_rescale_ts(outPacket, encoderCtx_->time_base, outVideoStream_->time_base);
                outPacket->stream_index = outVideoStream_->index;
                av_interleaved_write_frame(outputFormatCtx_, outPacket);
                encodedFrames++;
                av_packet_unref(outPacket);
            }
            av_packet_free(&outPacket);
            
            av_frame_unref(filtFrame);
        }
    }

    // 刷新filter
    std::cout << "刷新filter..." << std::endl;
    av_buffersrc_add_frame_flags(bufferSrcCtx_, nullptr, 0);
    while ((ret = av_buffersink_get_frame(bufferSinkCtx_, filtFrame)) >= 0) {
        filtFrame->pict_type = AV_PICTURE_TYPE_NONE;
        avcodec_send_frame(encoderCtx_, filtFrame);
        
        AVPacket* outPacket = av_packet_alloc();
        while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
            av_packet_rescale_ts(outPacket, encoderCtx_->time_base, outVideoStream_->time_base);
            outPacket->stream_index = outVideoStream_->index;
            av_interleaved_write_frame(outputFormatCtx_, outPacket);
            encodedFrames++;
            av_packet_unref(outPacket);
        }
        av_packet_free(&outPacket);
        
        av_frame_unref(filtFrame);
    }

    // 刷新编码器
    std::cout << "刷新编码器..." << std::endl;
    avcodec_send_frame(encoderCtx_, nullptr);
    AVPacket* outPacket = av_packet_alloc();
    while (avcodec_receive_packet(encoderCtx_, outPacket) >= 0) {
        av_packet_rescale_ts(outPacket, encoderCtx_->time_base, outVideoStream_->time_base);
        outPacket->stream_index = outVideoStream_->index;
        ret = av_interleaved_write_frame(outputFormatCtx_, outPacket);
        if (ret >= 0) {
            encodedFrames++;
        }
        av_packet_unref(outPacket);
    }
    av_packet_free(&outPacket);

    // 写入文件尾
    std::cout << "写入文件尾..." << std::endl;
    ret = av_write_trailer(outputFormatCtx_);
    if (ret < 0) {
        av_strerror(ret, errbuf, sizeof(errbuf));
        std::cerr << "写入文件尾失败: " << errbuf << std::endl;
        av_frame_free(&filtFrame);
        av_frame_free(&frame);
        av_packet_free(&packet);
        return false;
    }

    std::cout << "处理完成！解码 " << frameCount << " 帧, 编码 " << encodedFrames << " 帧" << std::endl;

    av_frame_free(&filtFrame);
    av_frame_free(&frame);
    av_packet_free(&packet);

    return true;
}

void FFmpegWatermarkProcessor::Cleanup()
{
    if (filterGraph_) {
        avfilter_graph_free(&filterGraph_);
        filterGraph_ = nullptr;
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
}

