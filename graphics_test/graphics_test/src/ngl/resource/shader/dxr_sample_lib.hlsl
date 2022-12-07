
// register指定(バインディング) 記述の注意.
// DXRのRayTracing ShaderはStateObject生成時にShaderとRootSignatureのバインディングを完全にチェックするため
// register指定を省略するとチェックに引っかかりStateObject生成に失敗する.


#if 0
// Comment

	Global Root Sig のレイアウト. システム化を簡易にするために固定インデックスから固定数を固定用途. 完全に使っていなくても不正なアクセスさえしなければOK.
		0 RootDesc t65535(AS)
		1 DescTable b0 - b15
		2 DescTable t0 - b15
		3 DescTable s0 - b15
		4 DescTable u0 - b15


	検討中
	Local Root Sig のレイアウト.
	わかりやすくするために 100 以降のregister indexにする. GlobalとLocal間ではオーバラップはエラー. Local間は独立しているので同じindexを使っても構わない とおもう.
	ここからShaderTableのレコードのリソース部はDescriptor4つ分になるか
	またHeapサイズの制限からLocal側はSampler使用不可とする. Global側の共有Sampler枠でやりくりする予定.
	UAVもとりあえず無しで.
		0 DescTable b1000 - b1015
		1 DescTable t1000 - b1015	// tにはBLASの頂点バッファなども欲しいため48個くらいまで用意したほうがいいかも?



#endif


// nglのmatrix系ははrow-majorメモリレイアウトであるための指定.
#pragma pack_matrix( row_major )

// Global Srv.
// システム供給のASはGlobalRootで衝突しないと思われるレジスタで供給.
RaytracingAccelerationStructure	rt_as : register(t65535);



// Global Srv.
RWTexture2D<float4>				out_uav : register(u0);


cbuffer CbSceneView : register(b0)
{
	float3x4 cb_view_mtx;
	float3x4 cb_view_inv_mtx;
	float4x4 cb_proj_mtx;
	float4x4 cb_proj_inv_mtx;

	// 正規化デバイス座標(NDC)のZ値からView空間Z値を計算するための係数. PerspectiveProjectionMatrixの方式によってCPU側で計算される値を変えることでシェーダ側は同一コード化.
	//	view_z = cb_ndc_z_to_view_z_coef.x / ( ndc_z * cb_ndc_z_to_view_z_coef.y + cb_ndc_z_to_view_z_coef.z )
	//
	//		cb_ndc_z_to_view_z_coef = 
	//			Standard RH: (-far_z * near_z, near_z - far_z, far_z, 0.0)
	//			Standard LH: ( far_z * near_z, near_z - far_z, far_z, 0.0)
	//			Reverse RH: (-far_z * near_z, far_z - near_z, near_z, 0.0)
	//			Reverse LH: ( far_z * near_z, far_z - near_z, near_z, 0.0)
	//			Infinite Far Reverse RH: (-near_z, 1.0, 0.0, 0.0)
	//			Infinite Far Reverse RH: ( near_z, 1.0, 0.0, 0.0)
	float4	cb_ndc_z_to_view_z_coef;
};



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


	float2 ndc_xy = ((screen_pos_f / screen_size_f) * 2.0 - 1.0) * float2(1.0, -1.0);

#if 1
	// 逆行列を使わずにProj行列の要素からレイ方向計算.
	const float inv_tan_horizontal = cb_proj_mtx._m00; // m00 = 1/tan(fov_x*0.5)
	const float inv_tan_vertical = cb_proj_mtx._m11; // m11 = 1/tan(fov_y*0.5)
	const float3 ray_dir_view = float3(ndc_xy.x / inv_tan_horizontal, ndc_xy.y / inv_tan_vertical, 1.0);

	// 向きのみなので w=0
	float3 ray_dir_world = mul(cb_view_inv_mtx, float4(ray_dir_view.xyz, 0.0));
#else
	// 逆行列からレイ方向計算.
	float4 ray_dir_view = mul(cb_proj_inv_mtx, float4(ndc_xy, 1.0, 1.0));
	ray_dir_view.xyz /= ray_dir_view.w;

	// 向きのみなので w=0
	float3 ray_dir_world = mul(cb_view_inv_mtx, float4(ray_dir_view.xyz, 0.0));
#endif
	
	float3 ray_dir = normalize(ray_dir_world.xyz);

	RayDesc ray;
	ray.Origin = float3(cb_view_inv_mtx._m03, cb_view_inv_mtx._m13, cb_view_inv_mtx._m23);// View逆行列からRay始点取得.
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


// ------------------------------------------------------
// Hitgroup Local Root Sig.

	// Tir 頂点位置.
	Buffer<float3>	srv_prim_vertex_pos	: t1000;
	// Tri Index
	Buffer<uint>	srv_prim_index		: t1001;
// ------------------------------------------------------

[shader("closesthit")]
void closestHit(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float ray_t = RayTCurrent();
	uint instanceId = InstanceID();
	uint geomIndex = GeometryIndex();

#if 0
	// ジオメトリ情報収集テスト
	uint tri_index_head = PrimitiveIndex() * 3;
	const uint3 tri_index = { srv_prim_index[tri_index_head + 0], srv_prim_index[tri_index_head + 1], srv_prim_index[tri_index_head + 2] };

	// Retrieve vertices for the hit triangle.
	float3 tri_vetex_pos[3] = {
		srv_prim_vertex_pos[tri_index[0]],
		srv_prim_vertex_pos[tri_index[1]],
		srv_prim_vertex_pos[tri_index[2]] };
	// 頂点データに無いのでここで法線計算.
	float3 tri_n_ls = normalize(cross(tri_vetex_pos[1] - tri_vetex_pos[0], tri_vetex_pos[2] - tri_vetex_pos[0]));
	// builtin ObjectToWorld3x4() でWorld変換可能. Object空間のRayDirなどもBuildinがある.
	float3 tri_n_ws = mul(ObjectToWorld3x4(), tri_n_ls);
	
	// ワールド法線の可視化.
	payload.color = float4(abs(tri_n_ws), 0.0);
#else

	// デバッグ用に距離で色変化.
	float distance_color = frac(ray_t / 20.0);
	payload.color = float4(distance_color, distance_color, distance_color, 0.0);
#endif
}

// 2つ目のhitgroupテスト.
[shader("closesthit")]
void closestHit2(inout Payload payload, in BuiltInTriangleIntersectionAttributes attribs)
{
	float ray_t = RayTCurrent();
	uint instanceId = InstanceID();
	uint geomIndex = GeometryIndex();
	uint tri_index_head = PrimitiveIndex() * 3;


	// デバッグ用に重心座標の可視化テスト.
	float3 bary = float3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

	payload.color = float4(bary, 0.0);
}