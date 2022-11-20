
// register指定(バインディング) 記述の注意.
// DXRのRayTracing ShaderはStateObject生成時にShaderとRootSignatureのバインディングを完全にチェックするため
// register指定を省略するとチェックに引っかかりStateObject生成に失敗する.



// Global Srv.
// システム供給のASはGlobalRootで衝突しないと思われるレジスタで供給.
RaytracingAccelerationStructure	rt_as : register(t65535);



// Global Srv.
RWTexture2D<float4>				out_uav : register(u0);


struct Payload
{
	float4 color;
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
	ray.Origin = float3(0.0, 2.0, -1.0);
	ray.Direction = ray_dir;
	ray.TMin = 0.0;
	ray.TMax = 1e38;

	
	Payload payload;
	const int ray_flag = 0;
	// HItGroupIndex計算時に加算される値. ShadowやAO等のPass毎のシェーダを切り替える際のインデックスに使うなど.
	const int ray_contribution_to_hitgroup = 0;
	// BLAS中のSubGeometryIndexに乗算される値. 結果はHitGroupIndex計算時に加算される.
	// BLAS中のSubGeometryIndexがそれぞれ別のHitGroupを利用する場合は1等, BLAS中のすべてが同じHitGroupなら0を指定するなどが考えられる.
	// 1に設定する場合はShaderTable構築時にBLAS内Geom分考慮したEntry登録が必要.
	const int multiplier_for_subgeometry_index = 1;
	const int miss_shader_index = 0;
	TraceRay(rt_as, ray_flag, 0xff, ray_contribution_to_hitgroup, multiplier_for_subgeometry_index, miss_shader_index, ray, payload );

	float3 col = payload.color.xyz;

	out_uav[launch_index.xy] = float4(col, 0.0);
}


[shader("miss")]
void miss(inout Payload payload)
{
	payload.color = float4(0.0, 0.0, 0.0, 0.0);
}

// 2つ目のMissShaderテスト. TraceRayの引数でどのMissShaderを利用するか直接的に指定する.
[shader("miss")]
void miss2(inout Payload payload)
{
	payload.color = float4(0.0, 0.8, 0.8, 0.0);
}




[shader("closesthit")]
void closestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint instanceId = InstanceID();
	uint geomIndex = GeometryIndex();
	float ray_t = RayTCurrent();

	// デバッグ用に距離で色変化.
	float distance_color = frac(ray_t / 20.0);

	payload.color = float4(distance_color, distance_color, distance_color, 0.0);
}

// 2つ目のhitgroupテスト.
[shader("closesthit")]
void closestHit2(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	uint instanceId = InstanceID();
	uint geomIndex = GeometryIndex();
	float ray_t = RayTCurrent();

	// デバッグ用に重心座標の可視化テスト.
	float3 bary = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

	payload.color = float4(bary, 0.0);
}