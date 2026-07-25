#include "platform/psvita/shaders/ForwardLambertShaderSource.hpp"

namespace helengine::psvita::shaders {
    /// Returns the shared Cg source compiled into the Vita forward-Lambert vertex and fragment artifacts.
    const char* GetForwardLambertShaderSource() {
        static constexpr const char* Source = R"(struct VS_IN
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct PS_IN
{
    float4 pos : POSITION;
    float3 normal : TEXCOORD0;
};

PS_IN VS(
    VS_IN input,
    uniform float4x4 HelengineWorldViewProjection,
    uniform float4x4 HelengineNormalTransform)
{
    PS_IN output;
    output.pos = mul(float4(input.pos, 1.0f), HelengineWorldViewProjection);
    output.normal = normalize(mul(float4(input.normal, 0.0f), HelengineNormalTransform).xyz);
    return output;
}

float4 PS(
    PS_IN input,
    uniform float4 HelengineBaseColor,
    uniform float4 HelengineLightDirection,
    uniform float4 HelengineLightColor,
    uniform float4 HelengineAmbient) : COLOR
{
    float diffuse = max(dot(normalize(input.normal), -normalize(HelengineLightDirection.xyz)), 0.0f);
    return HelengineBaseColor * (HelengineAmbient + HelengineLightColor * diffuse);
}
)";
        return Source;
    }
}
