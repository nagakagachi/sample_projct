

cbuffer CbSampleCs
{
	float cb_time;
};


RWTexture2D<float4> out_uav : register(u0);


float sdBox(float3 p, float3 b)
{
	float3 q = abs(p) - b;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdRoundBox(float3 p, float3 b, float r)
{
	float3 q = abs(p) - b;
	return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}
float sdTorus(float3 p, float2 t)
{
	float2 q = float2(length(p.xz) - t.x, p.y);
	return length(q) - t.y;
}


[numthreads(8,8,1)]
void main_cs(
	uint3 dtid	: SV_DispatchThreadID,
	uint3 gtid	: SV_GroupThreadID,
	uint3 gid	: SV_GroupID,
	uint gindex	: SV_GroupIndex
	)
{
	uint2 tex_size;
	out_uav.GetDimensions(tex_size.x, tex_size.y);

	float2 uv = float2(dtid.xy) / float2(tex_size.xy);

	// 適当なSDFテスト.
	float3 uvw = float3(uv, 0.5);
	float distance = 1.0e5;
	distance = min(distance, sdBox(uvw - float3(0.5, 0.5, 0.5), float3(0.1, 0.1, 0.1)));
	distance = min(distance, sdBox(uvw - float3(0.3, 0.2, 0.5), float3(0.1, 0.05, 0.1)));
	distance = min(distance, sdRoundBox(uvw - float3(0.7, 0.3, 0.5), float3(0.05, 0.1, 0.1), 0.05));

	distance = min(distance, sdTorus(uvw.xzy - float3(0.5 + sin(cb_time * 0.5)*0.5, 0.5, 0.7), float2(0.1, 0.1)));

	
	out_uav[dtid.xy] = abs(distance) * 5;
}