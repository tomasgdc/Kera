struct VertexInput {
    float3 position : POSITION;
    float3 color    : COLOR;
};

struct VertexStageOutput {
    float4 sv_position : SV_Position;
    float3 fragColor   : COLOR;
};

[shader("vertex")]
VertexStageOutput vertexMain(VertexInput input) {
    VertexStageOutput output;
    output.sv_position = float4(input.position, 1.0);
    output.fragColor = input.color;
    return output;
}

[shader("fragment")]
float4 fragmentMain(float3 fragColor : COLOR) : SV_Target {
    return float4(fragColor, 1.0);
}
