#include "WatermarkRenderer.h"
#include <iostream>
#include <algorithm>
#include <locale>
#include <io.h>
#include <fcntl.h>
#include <iomanip>

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
    // 调试：输出文本内容
    _setmode(_fileno(stdout), _O_U8TEXT);
    std::wcout << L"[调试] 接收到的文本: \"" << text << L"\" (长度: " << text.length() << L")" << std::endl;
    for (size_t i = 0; i < text.length() && i < 10; i++) {
        std::wcout << L"  字符[" << i << L"]: U+" << std::hex << std::setw(4) << std::setfill(L'0') 
                   << static_cast<int>(text[i]) << std::dec << std::endl;
    }
    _setmode(_fileno(stdout), _O_TEXT);
    
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

    // 创建文本格式（使用支持中文的字体）
    ComPtr<IDWriteTextFormat> textFormat;
    hr = dwriteFactory_->CreateTextFormat(
        L"Microsoft YaHei",  // 微软雅黑，支持中文
        nullptr,
        DWRITE_FONT_WEIGHT_BOLD,  // 加粗，更清晰
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        48.0f,  // 增大字号，更明显
        L"zh-cn",
        textFormat.GetAddressOf()
    );
    if (FAILED(hr)) {
        std::cerr << "创建文本格式失败，HRESULT: 0x" << std::hex << hr << std::endl;
        return false;
    }

    // 创建画刷（白色，不透明）
    // 注意：这里的透明度应该是1.0，最终透明度由用户的alpha参数控制
    ComPtr<ID2D1SolidColorBrush> brush;
    renderTarget->CreateSolidColorBrush(
        D2D1::ColorF(D2D1::ColorF::White, 1.0f),
        brush.GetAddressOf()
    );

    // 开始绘制
    renderTarget->BeginDraw();
    renderTarget->Clear(D2D1::ColorF(0, 0, 0, 0)); // 透明背景

    // 计算文本尺寸
    ComPtr<IDWriteTextLayout> textLayout;
    hr = dwriteFactory_->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        textFormat.Get(),
        1000.0f,  // 最大宽度
        100.0f,   // 最大高度
        textLayout.GetAddressOf()
    );
    
    DWRITE_TEXT_METRICS textMetrics;
    textLayout->GetMetrics(&textMetrics);
    
    // 平铺参数（根据文本大小调整间距）
    int xStep = static_cast<int>(textMetrics.width + 150);  // 水平间距
    int yStep = static_cast<int>(textMetrics.height + 80);   // 垂直间距
    
    // 旋转角度：-45度（顺时针45度）
    float angle = -45.0f * 3.14159f / 180.0f;
    
    // 扩大绘制范围以覆盖旋转后的区域
    int extendedWidth = width * 2;
    int extendedHeight = height * 2;
    
    // 平铺绘制文字（45度倾斜）
    for (int y = -extendedHeight / 2; y < extendedHeight; y += yStep) {
        for (int x = -extendedWidth / 2; x < extendedWidth; x += xStep) {
            // 保存当前变换
            renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
            
            // 设置变换：先平移到文本位置，再旋转
            D2D1::Matrix3x2F transform = 
                D2D1::Matrix3x2F::Rotation(angle * 180.0f / 3.14159f, D2D1::Point2F(0, 0)) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(x), static_cast<float>(y));
            
            renderTarget->SetTransform(transform);
            
            // 绘制文本（从原点开始）
            D2D1_RECT_F textRect = D2D1::RectF(
                0,
                0,
                textMetrics.width + 50,
                textMetrics.height + 20
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
    
    // 恢复变换
    renderTarget->SetTransform(D2D1::Matrix3x2F::Identity());

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
    int nonTransparentPixels = 0;
    for (int i = 0; i < width * height; i++) {
        outData[i * 4 + 0] = buffer[i * 4 + 2]; // R
        outData[i * 4 + 1] = buffer[i * 4 + 1]; // G
        outData[i * 4 + 2] = buffer[i * 4 + 0]; // B
        outData[i * 4 + 3] = buffer[i * 4 + 3]; // A
        
        if (buffer[i * 4 + 3] > 0) {
            nonTransparentPixels++;
        }
    }
    
    std::cout << "[调试] 生成的水印非透明像素数: " << nonTransparentPixels 
              << " / " << (width * height) << std::endl;

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

    // 直接拉伸到目标尺寸（填满整个画面）
    UINT scaledWidth = targetWidth;
    UINT scaledHeight = targetHeight;
    
    std::cout << "水印拉伸到视频尺寸: " << scaledWidth << "x" << scaledHeight << std::endl;

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

    // 创建目标缓冲区（与视频尺寸相同，RGBA格式）
    outData.resize(targetWidth * targetHeight * 4);
    
    // 复制整个水印，转换BGRA到RGBA
    for (UINT y = 0; y < scaledHeight; y++) {
        for (UINT x = 0; x < scaledWidth; x++) {
            int srcIdx = (y * scaledWidth + x) * 4;
            int dstIdx = (y * targetWidth + x) * 4;
            
            BYTE b = scaledBuffer[srcIdx + 0];
            BYTE g = scaledBuffer[srcIdx + 1];
            BYTE r = scaledBuffer[srcIdx + 2];
            BYTE a = scaledBuffer[srcIdx + 3];
            
            // 转换BGRA到RGBA
            outData[dstIdx + 0] = r;
            outData[dstIdx + 1] = g;
            outData[dstIdx + 2] = b;
            outData[dstIdx + 3] = a;
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