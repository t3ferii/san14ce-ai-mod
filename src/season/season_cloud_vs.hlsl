// AI_TacticsMod local152 cloud layer: one full-screen triangle (no vertex buffer, draw 3,0). TEXCOORD0 = the pixel's ndc,
// as the game's own full-screen vertex shader gives the season patch's SeasonRain.hlsl (compiled with CLOUD_ONLY for the cloud).
void main(uint id : SV_VertexID, out float4 pos : SV_Position, out float4 ndc : TEXCOORD0)
{
    float2 p = float2((float)((id << 1) & 2), (float)(id & 2)) * 2.0 - 1.0;
    pos = float4(p, 0.0, 1.0);
    ndc = float4(p, 0.0, 1.0);
}
