cbuffer Shape : register(b0) {float4 shape;} // width, height, radius, opacity
Texture2D image : register(t0);
SamplerState linear_sampler : register(s0);
struct Vertex {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
Vertex vs_main(uint id:SV_VertexID) {
    Vertex v;v.uv=float2((id<<1)&2,id&2);
    v.position=float4(v.uv*float2(2,-2)+float2(-1,1),0,1);return v;
}
float4 ps_main(Vertex v):SV_TARGET {
    float2 p=abs((v.uv-.5)*shape.xy)-(shape.xy*.5-shape.z);
    float distance=length(max(p,0))+min(max(p.x,p.y),0)-shape.z;
    float coverage=saturate(.75-distance);
    float4 c=image.Sample(linear_sampler,v.uv);
    if(shape.z<-1.5) {
        float2 q=abs((v.uv-.5)*shape.xy)-(shape.xy*.5-10-22);
        float d=length(max(q,0))+min(max(q.x,q.y),0)-22;
        coverage=exp(-max(d,0)*max(d,0)/32);
    } else if(shape.z<0) {
        float tail_x=shape.z<-1.1?1-v.uv.x:v.uv.x;
        coverage=saturate((tail_x-abs(v.uv.y-.5)*2)*min(shape.x,shape.y)+.5);
        c=image.Sample(linear_sampler,float2(.01,.9));
    }
    // Captured backbuffer RGB is already composited. Its alpha is undefined
    // in the game; use our geometric mask instead of multiplying that alpha.
    c.a=coverage*shape.w;c.rgb*=c.a;return c;
}
