#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>

class MouseHandler;

class DXGICapture {
public:
    DXGICapture();
    ~DXGICapture();

    bool Initialize();
    bool CaptureFrame();
    void Cleanup();

    // 获取捕获的纹理
    ID3D11Texture2D* GetCapturedTexture() const { return m_capturedTexture.Get(); }
    
    // 获取桌面尺寸
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    bool InitializeD3D();
    bool InitializeDXGI();
    bool ProcessFrame(IDXGIResource* resource);
    void SaveTextureToFile(ID3D11Texture2D* texture, const std::wstring& filename);

    // D3D11相关
    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_capturedTexture;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_stagingTexture;

    // DXGI相关
    Microsoft::WRL::ComPtr<IDXGIOutputDuplication> m_duplication;
    Microsoft::WRL::ComPtr<IDXGIOutput1> m_output1;

    // 鼠标处理器
    std::unique_ptr<MouseHandler> m_mouseHandler;

    // 桌面信息
    int m_width;
    int m_height;
    DXGI_OUTPUT_DESC m_outputDesc;

    bool m_initialized;
};