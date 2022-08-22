
struct PSInput
{
    float4 Pos : SV_Position;
    float4 ClipPos : CLIP_POS;
};

void main(
    in uint VertexID : SV_VertexID,
    out PSInput PSIn
    )
{
    static const float2 PosXY[] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, +3.0),
        float2(+3.0, -1.0)
    };

    PSIn.Pos = float4(PosXY[VertexID], 1.0, 1.0);
    PSIn.ClipPos = PSIn.Pos;
}