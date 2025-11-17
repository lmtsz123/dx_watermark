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
    
    // 始终进行混合，使用水印的alpha和用户指定的alpha
    // 如果水印某处是透明的(alpha=0)，则finalAlpha=0，结果就是原视频
    float finalAlpha = watermarkColor.a * alpha;
    float3 blended = lerp(videoColor.rgb, watermarkColor.rgb, finalAlpha);
    
    return float4(blended, 1.0f);
}
