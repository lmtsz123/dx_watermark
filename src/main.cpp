#include "VideoProcessor.h"
#include "WatermarkRenderer.h"
#include <iostream>
#include <Windows.h>
#include <filesystem>

int main(int argc, char* argv[])
{
    // 设置控制台代码页为 UTF-8，解决中文乱码问题
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    
    // 初始化COM
    CoInitialize(nullptr);

    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <输入视频> [透明度]" << std::endl;
        std::cout << "示例: " << argv[0] << " input.mp4 0.3" << std::endl;
        std::cout << "输出文件将自动生成在输入文件的同一目录下" << std::endl;
        CoUninitialize();
        return 1;
    }

    std::string inputPath = argv[1];
    float alpha = (argc >= 3) ? std::stof(argv[2]) : 0.3f;
    
    // 生成输出路径：在输入文件的同一目录，文件名添加 _watermarked 后缀
    std::filesystem::path inputFilePath(inputPath);
    std::string stem = inputFilePath.stem().string();
    std::string extension = inputFilePath.extension().string();
    std::filesystem::path outputFilePath = inputFilePath.parent_path() / (stem + "_watermarked" + extension);
    std::string outputPath = outputFilePath.string();

    std::cout << "=== DirectX视频水印处理 ===" << std::endl;
    std::cout << "输入: " << inputPath << std::endl;
    std::cout << "输出: " << outputPath << std::endl;
    std::cout << "透明度: " << alpha << std::endl;

    // 获取视频尺寸
    std::cout << "\n正在读取视频信息..." << std::endl;
    int videoWidth = 0, videoHeight = 0;
    if (!VideoProcessor::GetVideoDimensions(inputPath, videoWidth, videoHeight)) {
        std::cerr << "无法获取视频尺寸" << std::endl;
        CoUninitialize();
        return 1;
    }
    
    std::cout << "视频尺寸: " << videoWidth << "x" << videoHeight << std::endl;

    // 加载水印
    std::cout << "\n正在加载水印..." << std::endl;
    WatermarkRenderer watermarkRenderer;
    if (!watermarkRenderer.Initialize()) {
        std::cerr << "初始化水印渲染器失败" << std::endl;
        CoUninitialize();
        return 1;
    }

    std::vector<unsigned char> watermarkData;
    
    // 从PNG文件加载水印（自适应视频尺寸）
    std::string watermarkPath = "watermark_1.png";
    if (!watermarkRenderer.LoadWatermarkFromPNG(watermarkPath, videoWidth, videoHeight, watermarkData)) {
        std::cerr << "加载水印失败，请确保 watermark_1.png 存在于程序目录" << std::endl;
        CoUninitialize();
        return 1;
    }

    // 处理视频
    std::cout << "\n开始处理视频..." << std::endl;
    VideoProcessor processor;
    
    if (!processor.ProcessVideo(inputPath, outputPath, 
                                watermarkData.data(), videoWidth, videoHeight, alpha)) {
        std::cerr << "视频处理失败" << std::endl;
        CoUninitialize();
        return 1;
    }

    std::cout << "\n处理完成！" << std::endl;
    std::cout << "输出文件: " << outputPath << std::endl;

    CoUninitialize();
    return 0;
}