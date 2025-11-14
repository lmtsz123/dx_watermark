#include "D3DProcessor.h"
#include <iostream>
#include <fstream>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

D3DProcessor::D3DProcessor() : width_(0), height_(0)
{
}

D3DProcessor::~D3DProcessor()
{
    Cleanup();
}

bool D3DProcessor::Initialize(int width, int height)
{
    width_ = width;
    height_ = height;

    if (!CreateDevice()) return false;
    if (!CompileShaders()) return false;
    if (!CreateBuffers()) return false;
    if (!CreateSamplerState()) return false;

    return true;
}

bool D3DProcessor::CreateDevice()
{
    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &device_,
        &featureLevel,
        &context_
    );

    if (FAILED(hr)) {
        std::cerr << "创建D3D11设备失败" << std::endl;
        return false;
    }

    // 创建渲染目标纹理
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width_;
    texDesc.Height = height_;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;

    hr = device_->CreateTexture2D(&texDesc, nullptr, &renderTargetTexture_);
    if (FAILED(hr)) {
        std::cerr << "创建渲染目标纹理失败" << std::endl;
        return false;
    }

    // 创建渲染目标视图
    hr = device_->CreateRenderTargetView(renderTargetTexture_.Get(), nullptr, &renderTargetView_);
    if (FAILED(hr)) {
        std::cerr << "创建渲染目标视图失败" << std::endl;
        return false;
    }

    // 创建staging纹理（用于从GPU读取数据）
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width = width_;
    stagingDesc.Height = height_;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    hr = device_->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture_);
    if (FAILED(hr)) {
        std::cerr << "创建staging纹理失败" << std::endl;
        return false;
    }

    std::cout << "D3D11设备创建成功，特性级别: " << featureLevel << std::endl;
    return true;
}

bool D3DProcessor::CompileShaders()
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    // 编译顶点着色器
    HRESULT hr = D3DCompileFromFile(
        L"shaders/WatermarkVS.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "vs_5_0",
        flags,
        0,
        &vsBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "顶点着色器编译错误: " 
                     << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    // 编译像素着色器
    hr = D3DCompileFromFile(
        L"shaders/WatermarkPS.hlsl",
        nullptr,
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "main",
        "ps_5_0",
        flags,
        0,
        &psBlob,
        &errorBlob
    );

    if (FAILED(hr)) {
        if (errorBlob) {
            std::cerr << "像素着色器编译错误: " 
                     << (char*)errorBlob->GetBufferPointer() << std::endl;
        }
        return false;
    }

    // 创建着色器对象
    hr = device_->CreateVertexShader(vsBlob->GetBufferPointer(), 
                                     vsBlob->GetBufferSize(), 
                                     nullptr, 
                                     &vertexShader_);
    if (FAILED(hr)) return false;

    hr = device_->CreatePixelShader(psBlob->GetBufferPointer(), 
                                    psBlob->GetBufferSize(), 
                                    nullptr, 
                                    &pixelShader_);
    if (FAILED(hr)) return false;

    // 创建输入布局
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, 
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, 
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device_->CreateInputLayout(layout, ARRAYSIZE(layout),
                                    vsBlob->GetBufferPointer(),
                                    vsBlob->GetBufferSize(),
                                    &inputLayout_);
    if (FAILED(hr)) return false;

    std::cout << "着色器编译成功" << std::endl;
    return true;
}

bool D3DProcessor::CreateBuffers()
{
    // 创建全屏四边形
    Vertex vertices[] = {
        { DirectX::XMFLOAT3(-1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f,  1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 0.0f) },
        { DirectX::XMFLOAT3( 1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(1.0f, 1.0f) },
        { DirectX::XMFLOAT3(-1.0f, -1.0f, 0.0f), DirectX::XMFLOAT2(0.0f, 1.0f) },
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_DEFAULT;
    vbDesc.ByteWidth = sizeof(vertices);
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices;

    HRESULT hr = device_->CreateBuffer(&vbDesc, &vbData, &vertexBuffer_);
    if (FAILED(hr)) return false;

    // 索引缓冲
    UINT indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibDesc = {};
    ibDesc.Usage = D3D11_USAGE_DEFAULT;
    ibDesc.ByteWidth = sizeof(indices);
    ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA ibData = {};
    ibData.pSysMem = indices;

    hr = device_->CreateBuffer(&ibDesc, &ibData, &indexBuffer_);
    if (FAILED(hr)) return false;

    // 常量缓冲
    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.ByteWidth = 16; // float alpha + padding
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device_->CreateBuffer(&cbDesc, nullptr, &constantBuffer_);
    if (FAILED(hr)) return false;

    return true;
}

bool D3DProcessor::CreateSamplerState()
{
    D3D11_SAMPLER_DESC sampDesc = {};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = device_->CreateSamplerState(&sampDesc, &samplerState_);
    return SUCCEEDED(hr);
}

bool D3DProcessor::CreateTextureFromData(const unsigned char* data, 
                                         int width, int height,
                                         ID3D11Texture2D** texture,
                                         ID3D11ShaderResourceView** srv)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // 转换RGB到RGBA
    std::vector<unsigned char> rgbaData(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        rgbaData[i * 4 + 0] = data[i * 3 + 0]; // R
        rgbaData[i * 4 + 1] = data[i * 3 + 1]; // G
        rgbaData[i * 4 + 2] = data[i * 3 + 2]; // B
        rgbaData[i * 4 + 3] = 255;              // A
    }

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = rgbaData.data();
    initData.SysMemPitch = width * 4;

    HRESULT hr = device_->CreateTexture2D(&texDesc, &initData, texture);
    if (FAILED(hr)) return false;

    // 创建着色器资源视图
    hr = device_->CreateShaderResourceView(*texture, nullptr, srv);
    return SUCCEEDED(hr);
}

bool D3DProcessor::CreateTextureFromRGBA(const unsigned char* rgbaData,
                                         int width, int height,
                                         ID3D11Texture2D** texture,
                                         ID3D11ShaderResourceView** srv)
{
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = rgbaData;
    initData.SysMemPitch = width * 4;

    HRESULT hr = device_->CreateTexture2D(&texDesc, &initData, texture);
    if (FAILED(hr)) return false;

    // 创建着色器资源视图
    hr = device_->CreateShaderResourceView(*texture, nullptr, srv);
    return SUCCEEDED(hr);
}

bool D3DProcessor::UpdateTextureData(ID3D11Texture2D* texture, const unsigned char* data, int width, int height)
{
    // 转换RGB到RGBA
    std::vector<unsigned char> rgbaData(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        rgbaData[i * 4 + 0] = data[i * 3 + 0]; // R
        rgbaData[i * 4 + 1] = data[i * 3 + 1]; // G
        rgbaData[i * 4 + 2] = data[i * 3 + 2]; // B
        rgbaData[i * 4 + 3] = 255;              // A
    }

    // 更新纹理数据
    context_->UpdateSubresource(texture, 0, nullptr, rgbaData.data(), width * 4, 0);
    return true;
}

bool D3DProcessor::BlendTextures(ID3D11ShaderResourceView* videoSRV,
                                 ID3D11ShaderResourceView* watermarkSRV,
                                 float alpha,
                                 unsigned char* outputData)
{
    HRESULT hr;

    // 清空渲染目标，确保每帧都是干净的状态
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);

    // 设置渲染目标
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);

    // 设置视口
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);

    // 更新常量缓冲
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = context_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr)) {
        float* cbData = reinterpret_cast<float*>(mappedResource.pData);
        cbData[0] = alpha;
        context_->Unmap(constantBuffer_.Get(), 0);
    }

    // 设置渲染状态
    context_->IASetInputLayout(inputLayout_.Get());
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    context_->IASetIndexBuffer(indexBuffer_.Get(), DXGI_FORMAT_R32_UINT, 0);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 设置着色器
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context_->PSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());

    // 设置纹理（直接使用传入的SRV，不再创建新的）
    ID3D11ShaderResourceView* srvs[] = { videoSRV, watermarkSRV };
    context_->PSSetShaderResources(0, 2, srvs);
    context_->PSSetSamplers(0, 1, samplerState_.GetAddressOf());

    // 绘制
    context_->DrawIndexed(6, 0, 0);

    // 解绑shader资源，避免资源冲突
    ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
    context_->PSSetShaderResources(0, 2, nullSRVs);

    // 确保GPU完成渲染
    context_->Flush();

    // 将渲染结果复制到staging纹理（使用缓存的staging纹理）
    context_->CopyResource(stagingTexture_.Get(), renderTargetTexture_.Get());

    // 映射并读取数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context_->Map(stagingTexture_.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (SUCCEEDED(hr)) {
        // 转换RGBA到RGB，注意使用RowPitch而不是width*4
        unsigned char* src = static_cast<unsigned char*>(mapped.pData);
        for (int y = 0; y < height_; y++) {
            for (int x = 0; x < width_; x++) {
                // 使用RowPitch来正确计算源索引
                int srcIdx = y * mapped.RowPitch + x * 4;
                int dstIdx = (y * width_ + x) * 3;
                outputData[dstIdx + 0] = src[srcIdx + 0];  // R
                outputData[dstIdx + 1] = src[srcIdx + 1];  // G
                outputData[dstIdx + 2] = src[srcIdx + 2];  // B
            }
        }
        context_->Unmap(stagingTexture_.Get(), 0);
    }

    return true;
}

void D3DProcessor::Cleanup()
{
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }
}