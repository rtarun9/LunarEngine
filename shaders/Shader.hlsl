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

struct SceneBuffer
{
    row_major matrix viewProjectionMatrix;
};

struct TransformBuffer
{
    row_major matrix modelMatrix;
};

// [[vk::binding(x, y)]] : binding number x, set number x.
[[vk::binding(0, 0)]] ConstantBuffer<SceneBuffer> sceneBuffer: register(b0, space0);
[[vk::binding(0, 1)]] ConstantBuffer<TransformBuffer> transformBuffer : register(b0, space1);

VsOutput VsMain(VertexInput input)
{
    VsOutput output;
    output.position = mul(mul(float4(input.position, 1.0f), transformBuffer.modelMatrix), sceneBuffer.viewProjectionMatrix);
    output.color = input.color;

    return output;
}

float4 PsMain(VsOutput input) : SV_Target { return float4(input.color, 1.0f); }