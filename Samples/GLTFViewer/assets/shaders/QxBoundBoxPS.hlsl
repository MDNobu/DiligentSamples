
float4 BoundBoxPS(
    in float4 Pos : SV_Position
    ) : SV_Target
{
    return float4(0., 0.9, 0., 1.);
}