#include "DXGICapture.h"
#include "MouseHandler.h"
#include <iostream>
#include <comdef.h>
#include <wrl/client.h>

using namespace Microsoft::WRL;

DXGICapture::DXGICapture() 
    : m_width(0), m_height(0), m_initialized(false) {
    m_mouseHandler = std::make_unique<MouseHandler>();
}

DXGICapture::~DXGICapture() {
    Cleanup();
}

bool DXGICapture::Initialize() {
    if (m_initialized) {
        return true;
    }

    if (!InitializeD3D()) {
        std::wcerr << L"Failed to initialize D3D11" << std::endl;
        return false;
    }

    if (!InitializeDXGI()) {
        std::wcerr << L"Failed to initialize DXGI" << std::endl;
        return false;
    }

    // 初始化鼠标处理器
    if (!m_mouseHandler->Initialize(m_device.Get(), m_context.Get(), m_width, m_height)) {
        std::wcerr << L"Failed to initialize mouse handler" << std::endl;
        return false;
    }

    m_initialized = true;
    return true;
}

bool DXGICapture::InitializeD3D() {
    HRESULT hr;
    
    // 创建D3D11设备和上下文
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context
    );

    if (FAILED(hr)) {
        return false;
    }

    return true;
}

bool DXGICapture::InitializeDXGI() {
    HRESULT hr;

    // 获取DXGI设备
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) return false;

    // 获取适配器
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) return false;

    // 枚举输出设备
    ComPtr<IDXGIOutput> dxgiOutput;
    hr = dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    if (FAILED(hr)) return false;

    // 获取输出描述
    hr = dxgiOutput->GetDesc(&m_outputDesc);
    if (FAILED(hr)) return false;

    m_width = m_outputDesc.DesktopCoordinates.right - m_outputDesc.DesktopCoordinates.left;
    m_height = m_outputDesc.DesktopCoordinates.bottom - m_outputDesc.DesktopCoordinates.top;

    // 获取IDXGIOutput1接口
    hr = dxgiOutput.As(&m_output1);
    if (FAILED(hr)) return false;

    // 创建桌面复制
    hr = m_output1->DuplicateOutput(m_device.Get(), &m_duplication);
    if (FAILED(hr)) return false;

    // 创建暂存纹理用于CPU访问
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = m_width;
    stagingDesc.Height = m_height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = m_device->CreateTexture2D(&stagingDesc, nullptr, &m_stagingTexture);
    if (FAILED(hr)) return false;

    return true;
}

bool DXGICapture::CaptureFrame() {
    if (!m_initialized) {
        return false;
    }

    HRESULT hr;
    ComPtr<IDXGIResource> resource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    // 获取下一帧
    hr = m_duplication->AcquireNextFrame(100, &frameInfo, &resource);
    if (hr == DXGI_ERROR_WAIT_TIMEOUT) {
        return true; // 没有新帧，这是正常的
    }
    if (FAILED(hr)) {
        return false;
    }

    // 处理鼠标更新
    if (frameInfo.LastMouseUpdateTime.QuadPart > 0) {
        m_mouseHandler->UpdateMouse(frameInfo, m_duplication.Get());
    }

    // 处理桌面图像
    bool result = ProcessFrame(resource.Get());

    // 释放帧
    m_duplication->ReleaseFrame();

    return result;
}

bool DXGICapture::ProcessFrame(IDXGIResource* resource) {
    HRESULT hr;

    // 获取桌面纹理 - 修复QueryInterface调用
    ComPtr<ID3D11Texture2D> desktopTexture;
    hr = resource->QueryInterface(__uuidof(ID3D11Texture2D), 
                                  (void**)desktopTexture.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    // 复制到我们的纹理
    if (!m_capturedTexture) {
        D3D11_TEXTURE2D_DESC desc;
        desktopTexture->GetDesc(&desc);
        
        // 修改为可以作为渲染目标的格式
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.CPUAccessFlags = 0;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        hr = m_device->CreateTexture2D(&desc, nullptr, &m_capturedTexture);
        if (FAILED(hr)) {
            return false;
        }
    }

    // 复制桌面内容
    m_context->CopyResource(m_capturedTexture.Get(), desktopTexture.Get());

    // 渲染鼠标到纹理上
    m_mouseHandler->RenderMouse(m_capturedTexture.Get());

    return true;
}

void DXGICapture::SaveTextureToFile(ID3D11Texture2D* texture, const std::wstring& filename) {
    // 复制到暂存纹理
    m_context->CopyResource(m_stagingTexture.Get(), texture);

    // 映射纹理以读取数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        return;
    }

    // 这里可以添加保存为BMP、PNG等格式的代码
    // 为简化，此处省略具体的图像编码实现

    m_context->Unmap(m_stagingTexture.Get(), 0);
}

void DXGICapture::Cleanup() {
    m_mouseHandler.reset();
    
    if (m_duplication) {
        m_duplication.Reset();
    }
    
    m_output1.Reset();
    m_stagingTexture.Reset();
    m_capturedTexture.Reset();
    m_context.Reset();
    m_device.Reset();
    
    m_initialized = false;
}