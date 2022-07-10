#include "QxStructures.fxh"
cbuffer GSConstants
{
    Constants g_Constants;
};

[maxvertexcount(3)]
void main(triangle VSOutput In[3],
    inout TriangleStream<GSOutput> triStream)
{
    float2 v0 = g_Constants.ViewportSize.xy * In[0].Pos.xy / In[0].Pos.w;
    float2 v1 = g_Constants.ViewportSize.xy * In[1].Pos.xy / In[1].Pos.w;
    float2 v2 = g_Constants.ViewportSize.xy * In[2].Pos.xy / In[2].Pos.w;

    float2 edge0 = v2 - v1;
    float2 edge1 = v2 - v0;
    float2 edge2 = v1 - v0;
    // edge1和edge2围成的平行四边形的面积，这里利用的叉积的几何性质
    float area = abs(edge1.x * edge2.y - edge1.y * edge2.x);

    GSOutput Out;
    Out.VSOut = In[0];
    Out.DistToEdges = float3(area / length(edge0), 0., 0.);
    triStream.Append(Out);

    Out.VSOut = In[1];
    Out.DistToEdges = float3(0., area / length(edge1), 0.);
    triStream.Append(Out);

    Out.VSOut = In[2];
    Out.DistToEdges = float3(0., 0., area / length(edge2));
    triStream.Append(Out);

    // 下面这句是结束当前strip，根据需要调用
    //triStream.RestartStrip();
}