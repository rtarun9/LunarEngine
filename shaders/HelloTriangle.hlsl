struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

cbuffer MVPBuffer : register(b0)
{
    row_major matrix model_matrix;
};

VertexOutput vs_main(VertexInput vertex)
{
    VertexOutput output;
    output.position = mul(float4(vertex.position, 1.0f), model_matrix);
    output.color = vertex.color;

    return output;
}

float4 ps_main(VertexOutput input) : SV_Target
{
    return float4(input.color, 1.0f);
}