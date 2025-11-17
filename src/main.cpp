#include "VideoProcessor.h"
#include "FFmpegWatermarkProcessor.h"
#include "WatermarkRenderer.h"
#include "ScreenRecorder.h"
#include "DXGICapture.h"
#include <iostream>
#include <Windows.h>
#include <filesystem>
#include <shellapi.h>

int main(int argc, char* argv[])
{
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // 使用GetCommandLineW获取Unicode命令行参数
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (wargv == nullptr) {
        std::cerr << "获取命令行参数失败" << std::endl;
        return 1;
    }
    
    // 初始化COM
    CoInitialize(nullptr);

    if (wargc < 2) {
        std::cout << "=== 视频水印处理工具 ===" << std::endl;
        std::cout << "\n模式1: 视频文件添加水印" << std::endl;
        std::cout << "用法: " << argv[0] << " <输入视频> [透明度] [方法] [文字水印]" << std::endl;
        std::cout << "参数说明:" << std::endl;
        std::cout << "  输入视频: 要处理的视频文件路径" << std::endl;
        std::cout << "  透明度: 水印透明度 (0.0-1.0)，默认0.3" << std::endl;
        std::cout << "  方法: 处理方法，可选值：" << std::endl;
        std::cout << "    dx     - 使用DirectX GPU加速 (默认)" << std::endl;
        std::cout << "    ffmpeg - 使用FFmpeg filter" << std::endl;
        std::cout << "  文字水印: 可选，如果提供则生成文字水印（45度倾斜平铺）" << std::endl;
        std::cout << "           如果不提供则使用watermark_1.png图片水印" << std::endl;
        std::cout << "\n示例:" << std::endl;
        std::cout << "  " << argv[0] << " input.mp4 0.3 dx" << std::endl;
        std::cout << "  " << argv[0] << " input.mp4 0.3 dx \"机密文件\"" << std::endl;
        std::cout << "  " << argv[0] << " input.mp4 0.3 ffmpeg" << std::endl;
        
        std::cout << "\n模式2: 录制桌面并添加水印" << std::endl;
        std::cout << "用法: " << argv[0] << " --record <输出文件> <时长(秒)> [帧率] [透明度] [文字水印]" << std::endl;
        std::cout << "参数说明:" << std::endl;
        std::cout << "  输出文件: 录制视频的保存路径" << std::endl;
        std::cout << "  时长: 录制时长（秒）" << std::endl;
        std::cout << "  帧率: 录制帧率，默认30" << std::endl;
        std::cout << "  透明度: 水印透明度 (0.0-1.0)，默认0.3" << std::endl;
        std::cout << "  文字水印: 可选，如果提供则生成文字水印" << std::endl;
        std::cout << "           如果不提供则使用watermark_1.png图片水印" << std::endl;
        std::cout << "\n示例:" << std::endl;
        std::cout << "  " << argv[0] << " --record output.mp4 10" << std::endl;
        std::cout << "  " << argv[0] << " --record output.mp4 30 30 0.5" << std::endl;
        std::cout << "  " << argv[0] << " --record output.mp4 30 30 0.5 \"机密录屏\"" << std::endl;
        
        std::cout << "\n输出文件将自动生成在指定位置" << std::endl;
        LocalFree(wargv);
        CoUninitialize();
        return 1;
    }

    // 从Unicode参数转换为所需格式
    // 将wstring转换为UTF-8 string
    auto WStringToUTF8 = [](const std::wstring& wstr) -> std::string {
        if (wstr.empty()) return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
        return result;
    };
    
    // 检查是否是录屏模式
    std::wstring firstArg = wargv[1];
    if (firstArg == L"--record" || firstArg == L"-r") {
        // 录屏模式
        if (wargc < 4) {
            std::cerr << "错误: 录屏模式需要指定输出文件和时长" << std::endl;
            std::cerr << "用法: " << argv[0] << " --record <输出文件> <时长(秒)> [帧率] [透明度] [文字水印]" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }
        
        std::string outputPath = WStringToUTF8(wargv[2]);
        int duration = std::stoi(wargv[3]);
        int fps = (wargc >= 5) ? std::stoi(wargv[4]) : 30;
        float alpha = (wargc >= 6) ? std::stof(wargv[5]) : 0.3f;
        std::wstring textWatermark = (wargc >= 7) ? wargv[6] : L"";
        
        std::cout << "=== 桌面录制模式 ===" << std::endl;
        std::cout << "输出: " << outputPath << std::endl;
        std::cout << "时长: " << duration << " 秒" << std::endl;
        std::cout << "帧率: " << fps << " fps" << std::endl;
        std::cout << "透明度: " << alpha << std::endl;
        
        // 初始化水印渲染器
        WatermarkRenderer watermarkRenderer;
        if (!watermarkRenderer.Initialize()) {
            std::cerr << "初始化水印渲染器失败" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }
        
        // 获取屏幕尺寸（临时创建DXGICapture来获取）
        DXGICapture tempCapture;
        if (!tempCapture.Initialize()) {
            std::cerr << "无法获取屏幕尺寸" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }
        int screenWidth = tempCapture.GetWidth();
        int screenHeight = tempCapture.GetHeight();
        tempCapture.Cleanup();
        
        std::cout << "屏幕尺寸: " << screenWidth << "x" << screenHeight << std::endl;
        
        // 生成水印
        std::vector<unsigned char> watermarkData;
        if (!textWatermark.empty()) {
            std::cout << "生成文字水印..." << std::endl;
            if (!watermarkRenderer.CreateTiledWatermark(screenWidth, screenHeight, 
                                                       textWatermark, watermarkData)) {
                std::cerr << "生成文字水印失败" << std::endl;
                LocalFree(wargv);
                CoUninitialize();
                return 1;
            }
            std::cout << "文字水印生成成功" << std::endl;
        } else {
            std::string watermarkPath = "watermark_1.png";
            std::cout << "从文件加载水印: " << watermarkPath << std::endl;
            if (!watermarkRenderer.LoadWatermarkFromPNG(watermarkPath, screenWidth, 
                                                       screenHeight, watermarkData)) {
                std::cerr << "加载水印失败，请确保 watermark_1.png 存在于程序目录" << std::endl;
                LocalFree(wargv);
                CoUninitialize();
                return 1;
            }
        }
        
        // 开始录制
        ScreenRecorder recorder;
        bool success = recorder.RecordScreen(outputPath, duration, fps, 
                                            watermarkData.data(), 
                                            screenWidth, screenHeight, alpha);
        
        if (!success) {
            std::cerr << "录制失败" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }
        
        std::cout << "\n录制完成！" << std::endl;
        std::cout << "输出文件: " << outputPath << std::endl;
        
        LocalFree(wargv);
        CoUninitialize();
        return 0;
    }
    
    // 视频文件处理模式
    std::string inputPath = WStringToUTF8(wargv[1]);
    float alpha = (wargc >= 3) ? std::stof(wargv[2]) : 0.3f;
    std::string method = (wargc >= 4) ? WStringToUTF8(wargv[3]) : "dx";
    std::wstring textWatermark = (wargc >= 5) ? wargv[4] : L"";
    
    // 转换为小写
    for (auto& c : method) {
        c = std::tolower(c);
    }
    
    // 验证方法参数
    if (method != "dx" && method != "ffmpeg") {
        std::cerr << "错误: 无效的处理方法 '" << method << "'" << std::endl;
        std::cerr << "请使用 'dx' 或 'ffmpeg'" << std::endl;
        LocalFree(wargv);
        CoUninitialize();
        return 1;
    }
    
    // 生成输出路径：在输入文件的同一目录，文件名添加 _watermarked 后缀
    std::filesystem::path inputFilePath(inputPath);
    std::string stem = inputFilePath.stem().string();
    std::string extension = inputFilePath.extension().string();
    std::filesystem::path outputFilePath = inputFilePath.parent_path() / (stem + "_watermarked" + extension);
    std::string outputPath = outputFilePath.string();

    std::cout << "=== 视频水印处理 ===" << std::endl;
    std::cout << "处理方法: " << (method == "dx" ? "DirectX GPU加速" : "FFmpeg Filter") << std::endl;
    std::cout << "输入: " << inputPath << std::endl;
    std::cout << "输出: " << outputPath << std::endl;
    std::cout << "透明度: " << alpha << std::endl;

    bool success = false;
    
    if (method == "ffmpeg") {
        // 使用FFmpeg方法
        std::cout << "\n使用FFmpeg Filter处理..." << std::endl;
        
        std::string watermarkPath = "watermark_1.png";
        FFmpegWatermarkProcessor processor;
        
        success = processor.ProcessVideo(inputPath, outputPath, watermarkPath, alpha);
        
    } else {
        // 使用DirectX方法
        std::cout << "\n使用DirectX GPU加速处理..." << std::endl;
        
        // 获取视频尺寸
        std::cout << "\n正在读取视频信息..." << std::endl;
        int videoWidth = 0, videoHeight = 0;
        if (!VideoProcessor::GetVideoDimensions(inputPath, videoWidth, videoHeight)) {
            std::cerr << "无法获取视频尺寸" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }
        
        std::cout << "视频尺寸: " << videoWidth << "x" << videoHeight << std::endl;

        // 加载水印
        std::cout << "\n正在加载水印..." << std::endl;
        WatermarkRenderer watermarkRenderer;
        if (!watermarkRenderer.Initialize()) {
            std::cerr << "初始化水印渲染器失败" << std::endl;
            LocalFree(wargv);
            CoUninitialize();
            return 1;
        }

        std::vector<unsigned char> watermarkData;
        
        // 根据是否提供文字水印选择加载方式
        if (!textWatermark.empty()) {
            // 生成文字水印（45度倾斜平铺）
            std::cout << "生成文字水印..." << std::endl;
            
            // textWatermark已经是wstring，直接使用
            if (!watermarkRenderer.CreateTiledWatermark(videoWidth, videoHeight, textWatermark, watermarkData)) {
                std::cerr << "生成文字水印失败" << std::endl;
                LocalFree(wargv);
                CoUninitialize();
                return 1;
            }
            std::cout << "文字水印生成成功（45度倾斜平铺）" << std::endl;
        } else {
            // 从PNG文件加载水印（自适应视频尺寸）
            std::string watermarkPath = "watermark_1.png";
            std::cout << "从文件加载水印: " << watermarkPath << std::endl;
            
            if (!watermarkRenderer.LoadWatermarkFromPNG(watermarkPath, videoWidth, videoHeight, watermarkData)) {
                std::cerr << "加载水印失败，请确保 watermark_1.png 存在于程序目录" << std::endl;
                LocalFree(wargv);
                CoUninitialize();
                return 1;
            }
        }

        // 处理视频
        std::cout << "\n开始处理视频..." << std::endl;
        VideoProcessor processor;
        
        success = processor.ProcessVideo(inputPath, outputPath, 
                                    watermarkData.data(), videoWidth, videoHeight, alpha);
    }

    if (!success) {
        std::cerr << "视频处理失败" << std::endl;
        LocalFree(wargv);
        CoUninitialize();
        return 1;
    }

    std::cout << "\n处理完成！" << std::endl;
    std::cout << "输出文件: " << outputPath << std::endl;

    LocalFree(wargv);
    CoUninitialize();
    return 0;
}