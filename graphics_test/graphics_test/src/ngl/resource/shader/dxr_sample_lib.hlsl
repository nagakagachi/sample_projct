
// register指定(バインディング) 記述の注意.
// DXRのRayTracing ShaderはStateObject生成時にShaderとRootSignatureのバインディングを完全にチェックするため
// register指定を省略するとチェックに引っかかりStateObject生成に失敗する.



RaytracingAccelerationStructure	rt_as : register(t0);
RWTexture2D<float4>				out_uav : register(u0);


struct Payload
{
	uint hit;
};


[shader("raygeneration")]
void rayGen()
{
	uint3 launch_index = DispatchRaysIndex();
	uint3 launch_dim = DispatchRaysDimensions();


	float2 screen_pos_f = float2(launch_index.xy);
	float2 screen_size_f = float2(launch_dim.xy);


	float aspect = screen_size_f.x / screen_size_f.y;
	float2 ray_dir_xy = ((screen_pos_f / screen_size_f) * 2.0 - 1.0) * float2(1.0, -1.0);

	float3 ray_dir = normalize(float3(ray_dir_xy.x * aspect, ray_dir_xy.y, 1.0));

	RayDesc ray;
	ray.Origin = float3(0.0, 0.0, -2.0);
	ray.Direction = ray_dir;
	ray.TMin = 0.0;
	ray.TMax = 1e9;

	
	Payload payload;
	const int ray_flag = 0;
	const int ray_index = 0;
	TraceRay(rt_as, ray_flag, 0xff, ray_index, 0, 0, ray, payload );

	float3 col = (0 != payload.hit) ? float3(1.0, 1.0, 1.0) : float3(0.0, 0.0, 1.0);

	out_uav[launch_index.xy] = float4(col, 0.0);
}

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