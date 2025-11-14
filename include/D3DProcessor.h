#ifndef D3DPROCESSOR_H
#define D3DPROCESSOR_H

#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

struct Vertex
{
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT2 Tex;
};

class D3DProcessor
{
public:
    D3DProcessor();
    ~D3DProcessor();

    bool Initialize(int width, int height);
    bool CreateTextureFromData(const unsigned char* data, int width, int height, 
                               ID3D11Texture2D** texture, 
                               ID3D11ShaderResourceView** srv);
    bool CreateTextureFromRGBA(const unsigned char* rgbaData, int width, int height,
                               ID3D11Texture2D** texture,
                               ID3D11ShaderResourceView** srv);
    bool UpdateTextureData(ID3D11Texture2D* texture, const unsigned char* data, int width, int height);
    bool BlendTextures(ID3D11ShaderResourceView* videoSRV,
                      ID3D11ShaderResourceView* watermarkSRV,
                      float alpha,
                      unsigned char* outputData);
    void Cleanup();

private:
    bool CreateDevice();
    bool CompileShaders();
    bool CreateBuffers();
    bool CreateSamplerState();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapChain_;
    
    ComPtr<ID3D11VertexShader> vertexShader_;
    ComPtr<ID3D11PixelShader> pixelShader_;
    ComPtr<ID3D11InputLayout> inputLayout_;
    
    ComPtr<ID3D11Buffer> vertexBuffer_;
    ComPtr<ID3D11Buffer> indexBuffer_;
    ComPtr<ID3D11Buffer> constantBuffer_;
    
    ComPtr<ID3D11SamplerState> samplerState_;
    ComPtr<ID3D11RenderTargetView> renderTargetView_;
    ComPtr<ID3D11Texture2D> renderTargetTexture_;
    ComPtr<ID3D11Texture2D> stagingTexture_;  // 缓存staging纹理，避免每帧创建
    
    int width_;
    int height_;
};

#endif