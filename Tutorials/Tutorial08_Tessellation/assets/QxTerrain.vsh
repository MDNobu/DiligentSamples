#include "QxStructures.fxh"

cbuffer VSConstants
{
    GlobalConstants g_Constants;
};

struct TerrainVSIn
{
    uint BlockID : SV_VERTEXID;
};

void TerrainVS(in TerrainVSIn VSIn,
    out TerrainVSOut VSOut)
{
    uint blockHorzOrder =
        VSIn.BlockID % g_Constants.NumHorzBlocks;
    uint blockVertOrder =
        VSIn.BlockID / g_Constants.NumHorzBlocks;
    
    float2 blockOffset = float2(
        float(blockHorzOrder) / g_Constants.fNumHorzBlocks,
        float(blockVertOrder) / g_Constants.fNumVertBlocks
    );

    VSOut.BlockOffset = BlockOffset;
}