Texture2D videoTexture : register(t0);
Texture2D watermarkTexture : register(t1);
SamplerState samplerState : register(s0);

cbuffer WatermarkParams : register(b0)
{
    float alpha;
    float padding[3];
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
};

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 videoColor = videoTexture.Sample(samplerState, input.Tex);
    float4 watermarkColor = watermarkTexture.Sample(samplerState, input.Tex);
    
    // 如果水印有内容（alpha > 0），进行混合
    if (watermarkColor.a > 0.01)
    {
        // 使用水印的alpha和用户指定的alpha进行混合
        float finalAlpha = watermarkColor.a * alpha;
        float3 blended = lerp(videoColor.rgb, watermarkColor.rgb, finalAlpha);
        return float4(blended, 1.0f);
    }
    
    // 没有水印的地方返回原视频
    return float4(videoColor.rgb, 1.0f);
}
