// Simple vertex shader for triangle
// Outputs: position and color

struct VS_INPUT {
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VS_OUTPUT {
    float4 position : SV_Position;
    float3 color : COLOR;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}
