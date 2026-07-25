namespace helengine.psvita.builder;

/// <summary>
/// Lowers the first supported Forward Standard Shader profile into self-contained Cg source accepted by the Vita device compiler.
/// </summary>
public sealed class PsVitaForwardStandardSourceLowerer {
    /// <summary>
    /// Lowers one shared Standard Shader variant into the corresponding Vita Cg program pair.
    /// </summary>
    /// <param name="variant">Platform-independent Standard Shader variant selected by the shared shader catalog.</param>
    /// <returns>Vita Cg vertex and fragment source for the requested shared variant.</returns>
    public PsVitaForwardStandardSourcePair Lower(StandardShaderVariant variant) {
        if (variant == null) {
            throw new ArgumentNullException(nameof(variant));
        }

        if (string.Equals(variant.Name, BuiltInMaterialIds.StandardForwardVariantName, StringComparison.Ordinal)) {
            return new PsVitaForwardStandardSourcePair(LowerVertex(), LowerFragment());
        } else if (string.Equals(variant.Name, BuiltInMaterialIds.StandardForwardShadowedVariantName, StringComparison.Ordinal)) {
            return new PsVitaForwardStandardSourcePair(LowerShadowedVertex(), LowerShadowedFragment());
        } else if (string.Equals(variant.Name, BuiltInMaterialIds.StandardShadowDepthVariantName, StringComparison.Ordinal)) {
            return new PsVitaForwardStandardSourcePair(LowerShadowDepthVertex(), LowerShadowDepthFragment());
        }

        throw new InvalidOperationException($"PS Vita cannot lower unknown Standard Shader variant '{variant.Name}'.");
    }

    /// <summary>
    /// Produces the vertex stage for base-color and diffuse-texture Lambert rendering.
    /// </summary>
    /// <returns>Self-contained Vita Cg vertex source.</returns>
    public string LowerVertex() {
        return """
float4x4 HelengineWorldViewProjection;
float4x4 HelengineNormalTransform;

struct HelengineVertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct HelengineVertexOutput {
    float4 position : POSITION;
    float3 normal : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

HelengineVertexOutput VS(HelengineVertexInput input) {
    HelengineVertexOutput output;
    output.position = mul(float4(input.position, 1.0), HelengineWorldViewProjection);
    output.normal = mul(input.normal, (float3x3)HelengineNormalTransform);
    output.texCoord = input.texCoord;
    return output;
}
""";
    }

    /// <summary>
    /// Produces the fragment stage for base-color multiplied diffuse-texture Lambert rendering.
    /// </summary>
    /// <returns>Self-contained Vita Cg fragment source.</returns>
    public string LowerFragment() {
        return """
float4 HelengineBaseColor;
float4 HelengineLightDirection;
float4 HelengineLightColor;
float4 HelengineAmbient;
sampler2D HelengineDiffuseTexture;

struct HelengineFragmentInput {
    float3 normal : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
};

float4 PS(HelengineFragmentInput input) : COLOR {
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-HelengineLightDirection.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float3 lighting = HelengineAmbient.xyz + (HelengineLightColor.xyz * diffuse);
    float4 diffuseSample = tex2D(HelengineDiffuseTexture, input.texCoord);
    return float4(diffuseSample.rgb * HelengineBaseColor.rgb * lighting, diffuseSample.a * HelengineBaseColor.a);
}
""";
    }

    /// <summary>
    /// Produces the vertex stage for Standard Shader rendering that receives one directional shadow map.
    /// </summary>
    /// <returns>Self-contained Vita Cg vertex source with main-camera and light-space positions.</returns>
    public string LowerShadowedVertex() {
        return """
float4x4 HelengineWorldViewProjection;
float4x4 HelengineNormalTransform;
float4x4 HelengineLightViewProjection;

struct HelengineVertexInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct HelengineShadowedVertexOutput {
    float4 position : POSITION;
    float3 normal : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 shadowPosition : TEXCOORD2;
};

HelengineShadowedVertexOutput VS(HelengineVertexInput input) {
    HelengineShadowedVertexOutput output;
    output.position = mul(float4(input.position, 1.0), HelengineWorldViewProjection);
    output.normal = mul(input.normal, (float3x3)HelengineNormalTransform);
    output.texCoord = input.texCoord;
    output.shadowPosition = mul(float4(input.position, 1.0), HelengineLightViewProjection);
    return output;
}
""";
    }

    /// <summary>
    /// Produces the fragment stage for Standard Shader rendering that attenuates direct directional light with one hard shadow map comparison.
    /// </summary>
    /// <returns>Self-contained Vita Cg fragment source with diffuse and shadow sampler bindings.</returns>
    public string LowerShadowedFragment() {
        return """
float4 HelengineBaseColor;
float4 HelengineLightDirection;
float4 HelengineLightColor;
float4 HelengineAmbient;
float4 HelengineShadowBias;
sampler2D HelengineDiffuseTexture;
sampler2D HelengineShadowTexture;

struct HelengineShadowedFragmentInput {
    float3 normal : TEXCOORD0;
    float2 texCoord : TEXCOORD1;
    float4 shadowPosition : TEXCOORD2;
};

float4 PS(HelengineShadowedFragmentInput input) : COLOR {
    float3 normal = normalize(input.normal);
    float3 lightDirection = normalize(-HelengineLightDirection.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    float3 shadowCoordinate = input.shadowPosition.xyz / input.shadowPosition.w;
    float2 shadowTextureCoordinate = float2(
        (shadowCoordinate.x * 0.5) + 0.5,
        (1.0 - shadowCoordinate.y) * 0.5);
    float shadowMapContainsCoordinate = shadowCoordinate.x >= -1.0 && shadowCoordinate.x <= 1.0
        && shadowCoordinate.y >= -1.0 && shadowCoordinate.y <= 1.0
        && shadowCoordinate.z >= 0.0 && shadowCoordinate.z <= 1.0;
    float2 packedShadowDepth = tex2D(HelengineShadowTexture, shadowTextureCoordinate).rg;
    float shadowDepth = dot(packedShadowDepth, float2(1.0, 1.0 / 255.0));
    float directLightVisibility = shadowMapContainsCoordinate > 0.5
        ? (shadowCoordinate.z <= (shadowDepth + HelengineShadowBias.x) ? 1.0 : 0.0)
        : 1.0;
    float3 lighting = HelengineAmbient.xyz + (HelengineLightColor.xyz * diffuse * directLightVisibility);
    float4 diffuseSample = tex2D(HelengineDiffuseTexture, input.texCoord);
    return float4(diffuseSample.rgb * HelengineBaseColor.rgb * lighting, diffuseSample.a * HelengineBaseColor.a);
}
""";
    }

    /// <summary>
    /// Produces the vertex stage for the depth-only Standard Shader caster pass.
    /// </summary>
    /// <returns>Self-contained Vita Cg vertex source using only the light view-projection transform.</returns>
    public string LowerShadowDepthVertex() {
        return """
float4x4 HelengineLightViewProjection;

struct HelengineShadowDepthVertexInput {
    float3 position : POSITION;
};

struct HelengineShadowDepthVertexOutput {
    float4 position : POSITION;
    float depth : TEXCOORD0;
};

HelengineShadowDepthVertexOutput VS(HelengineShadowDepthVertexInput input) {
    HelengineShadowDepthVertexOutput output;
    output.position = mul(float4(input.position, 1.0), HelengineLightViewProjection);
    output.depth = output.position.z / max(output.position.w, 0.0001);
    return output;
}
""";
    }

    /// <summary>
    /// Produces the fragment stage for the depth-only Standard Shader caster pass.
    /// </summary>
    /// <returns>Self-contained Vita Cg fragment source with no material or lighting inputs.</returns>
    public string LowerShadowDepthFragment() {
        return """
struct HelengineShadowDepthFragmentInput {
    float depth : TEXCOORD0;
};

float4 PS(HelengineShadowDepthFragmentInput input) : COLOR {
    float2 packedDepth = frac(input.depth * float2(1.0, 255.0));
    packedDepth.x -= packedDepth.y * (1.0 / 255.0);
    return float4(packedDepth, 0.0, 1.0);
}
""";
    }
}
