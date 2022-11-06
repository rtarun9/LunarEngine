struct VsOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

VsOutput VsMain(uint vertexID : SV_VertexID)
{
    const float3 VERTEX_POSITIONS[3] = {
        float3(-0.5f, 0.5f, 0.0f),
        float3(0.0f, -0.5f, 0.0f),
        float3(-0.5f, 0.5f, 0.0f),
    };

    const float3 VERTEX_COLORS[3] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f),
    };

    VsOutput output;
    output.position = float4(VERTEX_POSITIONS[vertexID], 1.0f);
    output.color = VERTEX_COLORS[vertexID];

    return output;
}

float4 PsMain(VsOutput input) : SV_Target { return float4(input.color, 1.0f); }