#include "MouseHandler.h"
#include <iostream>
#include <d3dcompiler.h>
#include <cstring>

using namespace Microsoft::WRL;

// 简单的顶点着色器
const char* g_vertexShaderSource = R"(
struct VS_INPUT {
    float2 pos : POSITION;
    float2 tex : TEXCOORD;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

cbuffer Constants : register(b0) {
    float2 mousePos;
    float2 screenSize;
    float2 mouseSize;
    float2 padding;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    
    // 计算鼠标在屏幕上的位置
    float2 finalPos = input.pos * mouseSize + mousePos;
    
    // 转换到NDC坐标
    finalPos = (finalPos / screenSize) * 2.0 - 1.0;
    finalPos.y = -finalPos.y;
    
    output.pos = float4(finalPos, 0.0, 1.0);
    output.tex = input.tex;
    
    return output;
}
)";

// 像素着色器
const char* g_pixelShaderSource = R"(
Texture2D mouseTexture : register(t0);
SamplerState mouseSampler : register(s0);

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD;
};

float4 main(PS_INPUT input) : SV_TARGET {
    return mouseTexture.Sample(mouseSampler, input.tex);
}
)";

MouseHandler::MouseHandler() 
    : m_screenWidth(0), m_screenHeight(0), m_initialized(false), m_needsUpdate(false) {
    memset(&m_mouseInfo, 0, sizeof(m_mouseInfo));
}

MouseHandler::~MouseHandler() {
    Cleanup();
}

bool MouseHandler::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, 
                             int screenWidth, int screenHeight) {
    m_device = device;
    m_context = context;
    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;

    HRESULT hr;

    // 编译顶点着色器
    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> errorBlob;
    hr = D3DCompile(g_vertexShaderSource, strlen(g_vertexShaderSource), nullptr,
                    nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cout << "Vertex shader compilation error: " 
                      << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                      nullptr, &m_vertexShader);
    if (FAILED(hr)) return false;

    // 编译像素着色器
    ComPtr<ID3DBlob> psBlob;
    hr = D3DCompile(g_pixelShaderSource, strlen(g_pixelShaderSource), nullptr,
                    nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            std::cout << "Pixel shader compilation error: " 
                      << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    hr = m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                     nullptr, &m_pixelShader);
    if (FAILED(hr)) return false;

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}
    };

    hr = m_device->CreateInputLayout(layout, ARRAYSIZE(layout),
                                     vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                     &m_inputLayout);
    if (FAILED(hr)) return false;

    // 创建顶点缓冲区
    struct Vertex {
        float x, y;
        float u, v;
    };

    Vertex vertices[] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 1.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f}
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    hr = m_device->CreateBuffer(&bufferDesc, &initData, &m_vertexBuffer);
    if (FAILED(hr)) return false;

    // 创建常量缓冲区
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = sizeof(float) * 8; // 8个float
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateBuffer(&cbDesc, nullptr, &m_constantBuffer);
    if (FAILED(hr)) return false;

    // 创建采样器
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;

    hr = m_device->CreateSamplerState(&samplerDesc, &m_samplerState);
    if (FAILED(hr)) return false;

    // 创建混合状态
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = m_device->CreateBlendState(&blendDesc, &m_blendState);
    if (FAILED(hr)) return false;

    m_initialized = true;
    return true;
}

void MouseHandler::UpdateMouse(const DXGI_OUTDUPL_FRAME_INFO& frameInfo, 
                              IDXGIOutputDuplication* duplication) {
    // 更新鼠标位置
    if (frameInfo.LastMouseUpdateTime.QuadPart > 0) {
        m_mouseInfo.x = frameInfo.PointerPosition.Position.x;
        m_mouseInfo.y = frameInfo.PointerPosition.Position.y;
        m_mouseInfo.visible = frameInfo.PointerPosition.Visible != 0;
    }

    // 更新鼠标形状
    if (frameInfo.PointerShapeBufferSize > 0) {
        m_mouseInfo.shapeBuffer.resize(frameInfo.PointerShapeBufferSize);
        
        DXGI_OUTDUPL_POINTER_SHAPE_INFO shapeInfo;
        UINT bufferSizeRequired;
        
        HRESULT hr = duplication->GetFramePointerShape(
            frameInfo.PointerShapeBufferSize,
            m_mouseInfo.shapeBuffer.data(),
            &bufferSizeRequired,
            &shapeInfo
        );

        if (SUCCEEDED(hr)) {
            m_mouseInfo.shapeType = static_cast<DXGI_OUTDUPL_POINTER_SHAPE_TYPE>(shapeInfo.Type);  // 修复类型转换
            m_mouseInfo.shapeWidth = shapeInfo.Width;
            m_mouseInfo.shapeHeight = shapeInfo.Height;
            m_mouseInfo.shapePitch = shapeInfo.Pitch;
            m_mouseInfo.hotSpot = shapeInfo.HotSpot;
            
            m_needsUpdate = true;
        }
    }
}

// 其余函数保持不变...
bool MouseHandler::CreateMouseTexture() {
    if (m_mouseInfo.shapeBuffer.empty()) {
        return false;
    }

    HRESULT hr;
    
    // 创建鼠标纹理
    D3D11_TEXTURE2D_DESC textureDesc = {};
    textureDesc.Width = m_mouseInfo.shapeWidth;
    textureDesc.Height = m_mouseInfo.shapeHeight;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DYNAMIC;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = m_device->CreateTexture2D(&textureDesc, nullptr, &m_mouseTexture);
    if (FAILED(hr)) return false;

    // 创建着色器资源视图
    hr = m_device->CreateShaderResourceView(m_mouseTexture.Get(), nullptr, &m_mouseSRV);
    if (FAILED(hr)) return false;

    return true;
}

bool MouseHandler::ProcessMonochromePointer() {
    // 处理单色鼠标指针（通常是黑白的）
    std::vector<BYTE> colorBuffer(m_mouseInfo.shapeWidth * m_mouseInfo.shapeHeight * 4);
    
    UINT andMaskSize = m_mouseInfo.shapeHeight * m_mouseInfo.shapePitch;
    BYTE* andMask = m_mouseInfo.shapeBuffer.data();
    BYTE* xorMask = andMask + andMaskSize;
    
    for (UINT row = 0; row < m_mouseInfo.shapeHeight; ++row) {
        for (UINT col = 0; col < m_mouseInfo.shapeWidth; ++col) {
            UINT pixelIndex = row * m_mouseInfo.shapeWidth + col;
            UINT byteIndex = row * m_mouseInfo.shapePitch + col / 8;
            UINT bitIndex = 7 - (col % 8);
            
            bool andBit = (andMask[byteIndex] >> bitIndex) & 1;
            bool xorBit = (xorMask[byteIndex] >> bitIndex) & 1;
            
            BYTE* pixel = &colorBuffer[pixelIndex * 4];
            
            if (andBit) {
                if (xorBit) {
                    // 白色
                    pixel[0] = 255; // B
                    pixel[1] = 255; // G
                    pixel[2] = 255; // R
                    pixel[3] = 255; // A
                } else {
                    // 透明
                    pixel[0] = 0;
                    pixel[1] = 0;
                    pixel[2] = 0;
                    pixel[3] = 0;
                }
            } else {
                if (xorBit) {
                    // 反色（通常显示为白色）
                    pixel[0] = 255;
                    pixel[1] = 255;
                    pixel[2] = 255;
                    pixel[3] = 255;
                } else {
                    // 黑色
                    pixel[0] = 0;
                    pixel[1] = 0;
                    pixel[2] = 0;
                    pixel[3] = 255;
                }
            }
        }
    }
    
    // 更新纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_mouseTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;
    
    for (UINT row = 0; row < m_mouseInfo.shapeHeight; ++row) {
        memcpy((BYTE*)mapped.pData + row * mapped.RowPitch,
               colorBuffer.data() + row * m_mouseInfo.shapeWidth * 4,
               m_mouseInfo.shapeWidth * 4);
    }
    
    m_context->Unmap(m_mouseTexture.Get(), 0);
    return true;
}

bool MouseHandler::ProcessColorPointer() {
    // 处理彩色鼠标指针
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_mouseTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;
    
    for (UINT row = 0; row < m_mouseInfo.shapeHeight; ++row) {
        memcpy((BYTE*)mapped.pData + row * mapped.RowPitch,
               m_mouseInfo.shapeBuffer.data() + row * m_mouseInfo.shapePitch,
               m_mouseInfo.shapeWidth * 4);
    }
    
    m_context->Unmap(m_mouseTexture.Get(), 0);
    return true;
}

bool MouseHandler::ProcessMaskedColorPointer() {
    // 处理带掩码的彩色鼠标指针
    std::vector<BYTE> colorBuffer(m_mouseInfo.shapeWidth * m_mouseInfo.shapeHeight * 4);
    
    UINT maskSize = m_mouseInfo.shapeHeight * m_mouseInfo.shapePitch;
    BYTE* colorData = m_mouseInfo.shapeBuffer.data();
    BYTE* maskData = colorData + maskSize;
    
    for (UINT row = 0; row < m_mouseInfo.shapeHeight; ++row) {
        for (UINT col = 0; col < m_mouseInfo.shapeWidth; ++col) {
            UINT pixelIndex = row * m_mouseInfo.shapeWidth + col;
            UINT maskByteIndex = row * m_mouseInfo.shapePitch + col / 8;
            UINT maskBitIndex = 7 - (col % 8);
            
            bool masked = (maskData[maskByteIndex] >> maskBitIndex) & 1;
            
            BYTE* srcPixel = &colorData[pixelIndex * 4];
            BYTE* dstPixel = &colorBuffer[pixelIndex * 4];
            
            if (masked) {
                // 使用颜色数据
                dstPixel[0] = srcPixel[0]; // B
                dstPixel[1] = srcPixel[1]; // G
                dstPixel[2] = srcPixel[2]; // R
                dstPixel[3] = srcPixel[3]; // A
            } else {
                // 透明
                dstPixel[0] = 0;
                dstPixel[1] = 0;
                dstPixel[2] = 0;
                dstPixel[3] = 0;
            }
        }
    }
    
    // 更新纹理
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_mouseTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return false;
    
    for (UINT row = 0; row < m_mouseInfo.shapeHeight; ++row) {
        memcpy((BYTE*)mapped.pData + row * mapped.RowPitch,
               colorBuffer.data() + row * m_mouseInfo.shapeWidth * 4,
               m_mouseInfo.shapeWidth * 4);
    }
    
    m_context->Unmap(m_mouseTexture.Get(), 0);
    return true;
}

void MouseHandler::UpdateMouseTexture() {
    if (!m_needsUpdate || m_mouseInfo.shapeBuffer.empty()) {
        return;
    }
    
    // 重新创建纹理（如果尺寸变化了）
    if (!m_mouseTexture) {
        if (!CreateMouseTexture()) {
            return;
        }
    }
    
    // 根据鼠标指针类型处理数据
    bool success = false;
    switch (m_mouseInfo.shapeType) {
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MONOCHROME:
            success = ProcessMonochromePointer();
            break;
            
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_COLOR:
            success = ProcessColorPointer();
            break;
            
        case DXGI_OUTDUPL_POINTER_SHAPE_TYPE_MASKED_COLOR:
            success = ProcessMaskedColorPointer();
            break;
    }
    
    if (success) {
        m_needsUpdate = false;
    }
}

void MouseHandler::RenderMouse(ID3D11Texture2D* renderTarget) {
    if (!m_initialized || !m_mouseInfo.visible || !m_mouseTexture) {
        return;
    }
    
    UpdateMouseTexture();
    
    // 创建渲染目标视图
    if (!m_renderTargetView) {
        HRESULT hr = m_device->CreateRenderTargetView(renderTarget, nullptr, &m_renderTargetView);
        if (FAILED(hr)) return;
    }
    
    // 保存当前状态
    ComPtr<ID3D11RenderTargetView> oldRTV;
    ComPtr<ID3D11DepthStencilView> oldDSV;
    m_context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
    
    // 设置渲染目标
    m_context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);
    
    // 设置视口
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_screenWidth);
    viewport.Height = static_cast<float>(m_screenHeight);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);
    
    // 更新常量缓冲区
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        float* constants = static_cast<float*>(mapped.pData);
        constants[0] = static_cast<float>(m_mouseInfo.x - m_mouseInfo.hotSpot.x); // mousePos.x
        constants[1] = static_cast<float>(m_mouseInfo.y - m_mouseInfo.hotSpot.y); // mousePos.y
        constants[2] = static_cast<float>(m_screenWidth);  // screenSize.x
        constants[3] = static_cast<float>(m_screenHeight); // screenSize.y
        constants[4] = static_cast<float>(m_mouseInfo.shapeWidth);  // mouseSize.x
        constants[5] = static_cast<float>(m_mouseInfo.shapeHeight); // mouseSize.y
        constants[6] = 0.0f; // padding
        constants[7] = 0.0f; // padding
        
        m_context->Unmap(m_constantBuffer.Get(), 0);
    }
    
    // 设置渲染状态
    m_context->IASetInputLayout(m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    UINT stride = sizeof(float) * 4;
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    
    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, m_constantBuffer.GetAddressOf());
    
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
    m_context->PSSetShaderResources(0, 1, m_mouseSRV.GetAddressOf());
    m_context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());
    
    // 设置混合状态
    float blendFactor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    m_context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    
    // 绘制
    m_context->Draw(6, 0);
    
    // 恢复状态
    m_context->OMSetRenderTargets(1, oldRTV.GetAddressOf(), oldDSV.Get());
}

void MouseHandler::Cleanup() {
    m_renderTargetView.Reset();
    m_blendState.Reset();
    m_samplerState.Reset();
    m_constantBuffer.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_mouseSRV.Reset();
    m_mouseTexture.Reset();
    m_context.Reset();
    m_device.Reset();
    
    m_initialized = false;
}