#ifndef WATERMARK_RENDERER_H
#define WATERMARK_RENDERER_H

#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <string>
#include <vector>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

class WatermarkRenderer
{
public:
    WatermarkRenderer();
    ~WatermarkRenderer();

    bool Initialize();
    bool CreateTiledWatermark(int width, int height, 
                             const std::wstring& text,
                             std::vector<unsigned char>& outData);
    
    // 从PNG文件加载水印并缩放到指定尺寸
    bool LoadWatermarkFromPNG(const std::string& pngPath, 
                             int targetWidth, int targetHeight,
                             std::vector<unsigned char>& outData);

private:
    ComPtr<ID2D1Factory> d2dFactory_;
    ComPtr<IDWriteFactory> dwriteFactory_;
};

#endif