struct VertexInput
{
    [[vk::location(0)]] float3 position : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float3 color : COLOR;
};

struct VsOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

struct TransformData
{
    row_major matrix modelMatrix;
    row_major matrix viewProjectionMatrix;
};

[[vk::push_constant]] ConstantBuffer<TransformData> transformBuffer : register(b0);

VsOutput VsMain(VertexInput input)
{
    VsOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), transformBuffer.modelMatrix), transformBuffer.viewProjectionMatrix);
    output.color = input.color;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target { return float4(input.color, 1.0f); }