struct TerrainVSOut
{
    float2 BlockOffset : BLOCK_OFFSET;
};

struct TerrainHSOut
{
    float2 BlockOffset : BLOCK_OFFSET;
};

struct TerrainHSConstFuncOut
{
    float Edges[4] : SV_TESSFACTOR;
    float Inside[2] : SV_INSIDETESSFACTOR;
};
struct TerrainDSOut
{
    float4 Pos : SV_POSITION;
    float2 UV : TEX_COORD;
};

struct TerrainGSOut
{
    TerrainDSOut DSOut;
    float3 DistToEdges : DIST_TO_EDGES;
};

struct GlobalConstants
{
    uint NumHorzBlocks;
    uint NumVertBlocks;
    float fNumHorzBlocks;
    float fNumVertBlocks;

    float fBlockSize;
    float LengthScale;
    float HeightScale;
    float LineWidth;

    float TessDensity;
    int AdaptiveTessellation;
    float2 Dummy2;

    float4x4 WorldView;
    float4x4 WorldViewProj;
    float4 ViewportSize;
};
