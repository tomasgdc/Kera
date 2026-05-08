struct VertexInput {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct InstanceInput {
    float4x4 modelMatrix : TRANSFORM;
};

struct Uniforms {
    float4x4 projection;
    float4x4 view;
};
ParameterBlock<Uniforms> globalParams;

struct VertexStageOutput {
    float4 sv_position : SV_Position;
    float3 fragColor   : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput vertex, InstanceInput instance) {
    VertexStageOutput output;

    float4 worldPos = mul(float4(vertex.position, 1.0), instance.modelMatrix);
    float4 viewPos  = mul(worldPos, globalParams.view);
    output.sv_position = mul(viewPos, globalParams.projection);
    
    output.fragColor = vertex.color;

    return output;
}

[shader("fragment")]
float4 fragmentMain(float3 fragColor : COLOR) : SV_Target {
    return float4(fragColor, 1.0);
}
