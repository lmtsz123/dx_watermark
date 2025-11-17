#pragma once

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <wrl.h>
#include <vector>
#include <cstring> 

class MouseHandler {
public:
    MouseHandler();
    ~MouseHandler();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int screenWidth, int screenHeight);
    void UpdateMouse(const DXGI_OUTDUPL_FRAME_INFO& frameInfo, IDXGIOutputDuplication* duplication);
    void RenderMouse(ID3D11Texture2D* renderTarget);
    void Cleanup();

private:
    struct MouseInfo {
        int x, y;
        bool visible;
        DXGI_OUTDUPL_POINTER_SHAPE_TYPE shapeType;
        std::vector<BYTE> shapeBuffer;
        UINT shapeWidth, shapeHeight;
        UINT shapePitch;
        POINT hotSpot;
    };

    bool CreateMouseTexture();
    bool ProcessMonochromePointer();
    bool ProcessColorPointer();
    bool ProcessMaskedColorPointer();
    void UpdateMouseTexture();

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_mouseTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_mouseSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;

    // 渲染相关
    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;

    MouseInfo m_mouseInfo;
    int m_screenWidth, m_screenHeight;
    bool m_initialized;
    bool m_needsUpdate;
};