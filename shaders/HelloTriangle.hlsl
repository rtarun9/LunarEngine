static const float3 VERTEX_POSITIONS[3] = 
{
    float3(-0.5f, -0.5f, 0.0f), 
    float3(0.0f, 0.5f, 0.0f),
    float3(0.5f, -0.5f, 0.0f)
};

static const float3 VERTEX_COLORS[3] = 
{
    float3(1.0f, 0.0f, 0.0f), 
    float3(0.0f, 1.0f, 0.0f), 
    float3(0.0f, 0.0f, 1.0f)
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

VertexOutput vs_main(uint vertex_id : SV_VertexID)
{
    VertexOutput output;
    output.position = float4(VERTEX_POSITIONS[vertex_id], 1.0f);
    output.color = VERTEX_COLORS[vertex_id];

    return output;
}

float4 ps_main(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0f);
}