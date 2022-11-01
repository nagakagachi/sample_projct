
// register指定(バインディング) 記述の注意.
// DXRのRayTracing ShaderはStateObject生成時にShaderとRootSignatureのバインディングを完全にチェックするため
// register指定を省略するとチェックに引っかかりStateObject生成に失敗する.

RaytracingAccelerationStructure	rt_as : register(t0);
RWTexture2D<float4>				out_uav : register(u0);

[shader("raygeneration")]
void rayGen()
{
	uint3 ray_index = DispatchRaysIndex();
	
	float2 col = frac(ray_index.xy / 100.0);

	out_uav[ray_index.xy] = float4(col.x, col.y, 0.0, 0.0);
	//out_uav[ray_index.xy] = float4(1.0, 0.0, 0.0, 0.0);
}

struct Payload
{
	uint hit;
};

[shader("miss")]
void miss(inout Payload payload)
{
	payload.hit = 0;
}

[shader("closesthit")]
void closestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	payload.hit = 1;
}