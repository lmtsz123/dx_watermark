#include "WatermarkRenderer.h"
#include <iostream>
#include <algorithm>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

WatermarkRenderer::WatermarkRenderer()
{
}

WatermarkRenderer::~WatermarkRenderer()
{
}

bool WatermarkRenderer::Initialize()
{
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "创建D2D工厂失败" << std::endl;
        return false;
    }

    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                            __uuidof(IDWriteFactory),
                            reinterpret_cast<IUnknown**>(dwriteFactory_.GetAddressOf()));
    if (FAILED(hr)) {
        std::cerr << "创建DWrite工厂失败" << std::endl;
        return false;
    }

    return true;
}

bool WatermarkRenderer::CreateTiledWatermark(int width, int height,
                                            const std::wstring& text,
                                            std::vector<unsigned char>& outData)
{
    // 创建WIC位图
    ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicFactory.GetAddressOf()));
    if (FAILED(hr)) return false;

    ComPtr<IWICBitmap> wicBitmap;
    hr = wicFactory->CreateBitmap(width, height,
                                 GUID_WICPixelFormat32bppPBGRA,
                                 WICBitmapCacheOnDemand,
                                 wicBitmap.GetAddressOf());
    if (FAILED(hr)) return false;

    // 创建D2D渲染目标
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );

    ComPtr<ID2D1RenderTarget> renderTarget;
    hr = d2dFactory_->CreateWicBitmapRenderTarget(wicBitmap.Get(), rtProps, renderTarget.GetAddressOf());
    if (FAILED(hr)) return false;

    // 创建文本格式
    ComPtr<IDWriteTextFormat> textFormat;
    hr = dwriteFactory_->CreateTextFormat(
        L"Arial",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        35.0f,
        L"zh-cn",
        textFormat.GetAddressOf()
    );
    if (FAILED(hr)) return false;

    // 创建画刷
    ComPtr<ID2D1SolidColorBrush> brush;
    renderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White, 0.3f),
        brush.GetAddressOf()
    );

    // 开始绘制
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0)); // 透明背景

    // 平铺绘制文字
    int xStep = 280;
    int yStep = 120;
    
    for (int y = -50; y < height + 50; y += yStep) {
        for (int x = -100; x < width + 100; x += xStep) {
            D2D1_RECT_F textRect = D2D1::RectF(
                static_cast<float>(x),
                static_cast<float>(y),
                static_cast<float>(x + 500),
                static_cast<float>(y + 100)
            );
            
            renderTarget->DrawText(
                text.c_str(),
                static_cast<UINT32>(text.length()),
                textFormat.Get(),
                textRect,
                brush.Get()
            );
        }
    }

    hr = renderTarget->EndDraw();
    if (FAILED(hr)) return false;

    // 读取位图数据
    outData.resize(width * height * 4);
    
    WICRect rect = { 0, 0, width, height };
    UINT stride = width * 4;
    std::vector<BYTE> buffer(stride * height);
    
    hr = wicBitmap->CopyPixels(&rect, stride, buffer.size(), buffer.data());
    if (FAILED(hr)) return false;

    // 转换BGRA到RGBA（保留alpha通道）
    for (int i = 0; i < width * height; i++) {
        outData[i * 4 + 0] = buffer[i * 4 + 2]; // R
        outData[i * 4 + 1] = buffer[i * 4 + 1]; // G
        outData[i * 4 + 2] = buffer[i * 4 + 0]; // B
        outData[i * 4 + 3] = buffer[i * 4 + 3]; // A
    }

    return true;
}

bool WatermarkRenderer::LoadWatermarkFromPNG(const std::string& pngPath,
                                             int targetWidth, int targetHeight,
                                             std::vector<unsigned char>& outData)
{
    // 创建WIC工厂
    ComPtr<IWICImagingFactory> wicFactory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(wicFactory.GetAddressOf()));
    if (FAILED(hr)) {
        std::cerr << "创建WIC工厂失败" << std::endl;
        return false;
    }

    // 转换路径为宽字符
    std::wstring wPngPath(pngPath.begin(), pngPath.end());

    // 加载PNG文件
    ComPtr<IWICBitmapDecoder> decoder;
    hr = wicFactory->CreateDecoderFromFilename(
        wPngPath.c_str(),
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cerr << "无法打开PNG文件: " << pngPath << std::endl;
        return false;
    }

    // 获取第一帧
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "无法读取PNG帧" << std::endl;
        return false;
    }

    // 获取原始尺寸
    UINT originalWidth, originalHeight;
    frame->GetSize(&originalWidth, &originalHeight);
    std::cout << "水印原始尺寸: " << originalWidth << "x" << originalHeight << std::endl;

    // 创建格式转换器（转换为32位BGRA）
    ComPtr<IWICFormatConverter> converter;
    hr = wicFactory->CreateFormatConverter(converter.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "创建格式转换器失败" << std::endl;
        return false;
    }

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom
    );
    if (FAILED(hr)) {
        std::cerr << "初始化格式转换器失败" << std::endl;
        return false;
    }

    // 创建缩放器
    ComPtr<IWICBitmapScaler> scaler;
    hr = wicFactory->CreateBitmapScaler(scaler.GetAddressOf());
    if (FAILED(hr)) {
        std::cerr << "创建缩放器失败" << std::endl;
        return false;
    }

    // 计算缩放比例（保持宽高比）
    double scaleX = static_cast<double>(targetWidth) / originalWidth;
    double scaleY = static_cast<double>(targetHeight) / originalHeight;
    double scale = (std::min)(scaleX, scaleY);
    
    UINT scaledWidth = static_cast<UINT>(originalWidth * scale);
    UINT scaledHeight = static_cast<UINT>(originalHeight * scale);
    
    std::cout << "水印缩放后尺寸: " << scaledWidth << "x" << scaledHeight << std::endl;

    hr = scaler->Initialize(
        converter.Get(),
        scaledWidth,
        scaledHeight,
        WICBitmapInterpolationModeHighQualityCubic
    );
    if (FAILED(hr)) {
        std::cerr << "初始化缩放器失败" << std::endl;
        return false;
    }

    // 读取缩放后的位图数据
    WICRect scaledRect = { 0, 0, static_cast<INT>(scaledWidth), static_cast<INT>(scaledHeight) };
    UINT scaledStride = scaledWidth * 4;
    std::vector<BYTE> scaledBuffer(scaledStride * scaledHeight);
    
    hr = scaler->CopyPixels(&scaledRect, scaledStride, scaledBuffer.size(), scaledBuffer.data());
    if (FAILED(hr)) {
        std::cerr << "复制缩放后的像素失败" << std::endl;
        return false;
    }

    // 创建目标缓冲区（与视频尺寸相同，RGBA格式，初始化为全透明）
    outData.resize(targetWidth * targetHeight * 4);
    std::fill(outData.begin(), outData.end(), 0);
    
    // 计算居中偏移
    int offsetX = (targetWidth - scaledWidth) / 2;
    int offsetY = (targetHeight - scaledHeight) / 2;
    
    // 将缩放后的水印复制到目标缓冲区（居中）
    for (UINT y = 0; y < scaledHeight; y++) {
        for (UINT x = 0; x < scaledWidth; x++) {
            int targetX = x + offsetX;
            int targetY = y + offsetY;
            
            if (targetX >= 0 && targetX < targetWidth && targetY >= 0 && targetY < targetHeight) {
                int srcIdx = (y * scaledWidth + x) * 4;
                int dstIdx = (targetY * targetWidth + targetX) * 4;
                
                // 转换BGRA到RGBA（保留alpha通道）
                outData[dstIdx + 0] = scaledBuffer[srcIdx + 2]; // R
                outData[dstIdx + 1] = scaledBuffer[srcIdx + 1]; // G
                outData[dstIdx + 2] = scaledBuffer[srcIdx + 0]; // B
                outData[dstIdx + 3] = scaledBuffer[srcIdx + 3]; // A (保留alpha)
            }
        }
    }

    // 检查水印数据是否有非透明像素
    int nonTransparentPixels = 0;
    for (size_t i = 3; i < outData.size(); i += 4) {
        if (outData[i] > 0) {
            nonTransparentPixels++;
        }
    }
    
    std::cout << "水印加载成功，非透明像素数: " << nonTransparentPixels 
              << " / " << (targetWidth * targetHeight) << std::endl;
    
    if (nonTransparentPixels == 0) {
        std::cerr << "警告：水印完全透明！" << std::endl;
    }
    
    return true;
}